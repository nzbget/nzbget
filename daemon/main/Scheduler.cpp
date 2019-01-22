/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2008-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "nzbget.h"
#include "Scheduler.h"
#include "Options.h"
#include "WorkState.h"
#include "Log.h"
#include "NewsServer.h"
#include "ServerPool.h"
#include "FeedInfo.h"
#include "FeedCoordinator.h"
#include "SchedulerScript.h"

void Scheduler::AddTask(std::unique_ptr<Task> task)
{
	Guard guard(m_taskListMutex);
	m_taskList.push_back(std::move(task));
}

void Scheduler::FirstCheck()
{
	{
		Guard guard(m_taskListMutex);

		std::sort(m_taskList.begin(), m_taskList.end(),
			[](const std::unique_ptr<Task>& task1, const std::unique_ptr<Task>& task2)
		{
			return (task1->m_hours < task2->m_hours) ||
				((task1->m_hours == task2->m_hours) && (task1->m_minutes < task2->m_minutes));
		});
	}

	// check all tasks for the last week
	CheckTasks();
}

void Scheduler::ScheduleNextWork()
{
	// Ideally we should calculate wait time until next scheduler task or until resume time.
	// The first isn't trivial and the second requires watching/reaction on changed scheduled resume time.
	// We do it simpler instead: check once per minute, when seconds are changing from 59 to 00.

	time_t curTime = Util::CurrentTime();
	tm sched;
	gmtime_r(&curTime, &sched);
	sched.tm_min++;
	sched.tm_sec = 0;
	time_t nextMinute = Util::Timegm(&sched);

	m_serviceInterval = nextMinute - curTime;
}

void Scheduler::ServiceWork()
{
	debug("Scheduler service work");

	if (!DownloadQueue::IsLoaded())
	{
		return;
	}

	debug("Scheduler service work: doing work");

	if (!m_firstChecked)
	{
		FirstCheck();
		m_firstChecked = true;
		return;
	}

	m_executeProcess = true;
	CheckTasks();
	CheckScheduledResume();
	ScheduleNextWork();
}

void Scheduler::CheckTasks()
{
	PrepareLog();

	{
		Guard guard(m_taskListMutex);

		time_t current = Util::CurrentTime();

		if (!m_taskList.empty())
		{
			// Detect large step changes of system time
			time_t diff = current - m_lastCheck;
			if (diff > 60 * 90 || diff < 0)
			{
				debug("Reset scheduled tasks (detected clock change greater than 90 minutes or negative)");

				// check all tasks for the last week
				m_lastCheck = current - 60 * 60 * 24 * 7;
				m_executeProcess = false;

				for (Task* task : &m_taskList)
				{
					if (task->m_hours != Task::STARTUP_TASK)
					{
						task->m_lastExecuted = 0;
					}
				}
			}

			time_t localCurrent = current + g_WorkState->GetLocalTimeOffset();
			time_t localLastCheck = m_lastCheck + g_WorkState->GetLocalTimeOffset();

			tm tmCurrent;
			gmtime_r(&localCurrent, &tmCurrent);
			tm tmLastCheck;
			gmtime_r(&localLastCheck, &tmLastCheck);

			tm tmLoop;
			memcpy(&tmLoop, &tmLastCheck, sizeof(tmLastCheck));
			tmLoop.tm_hour = tmCurrent.tm_hour;
			tmLoop.tm_min = tmCurrent.tm_min;
			tmLoop.tm_sec = tmCurrent.tm_sec;
			time_t loop = Util::Timegm(&tmLoop);

			while (loop <= localCurrent)
			{
				for (Task* task : &m_taskList)
				{
					if (task->m_lastExecuted != loop)
					{
						tm tmAppoint;
						memcpy(&tmAppoint, &tmLoop, sizeof(tmLoop));
						tmAppoint.tm_hour = task->m_hours;
						tmAppoint.tm_min = task->m_minutes;
						tmAppoint.tm_sec = 0;

						time_t appoint = Util::Timegm(&tmAppoint);

						int weekDay = tmAppoint.tm_wday;
						if (weekDay == 0)
						{
							weekDay = 7;
						}

						bool weekDayOK = task->m_weekDaysBits == 0 || (task->m_weekDaysBits & (1 << (weekDay - 1)));
						bool doTask = (task->m_hours >= 0 && weekDayOK && localLastCheck < appoint && appoint <= localCurrent) ||
							(task->m_hours == Task::STARTUP_TASK && task->m_lastExecuted == 0);

						if (doTask)
						{
							ExecuteTask(task);
							task->m_lastExecuted = loop;
						}
					}
				}
				loop += 60 * 60 * 24; // inc day
				gmtime_r(&loop, &tmLoop);
			}
		}

		m_lastCheck = current;
	}

	PrintLog();
}

void Scheduler::ExecuteTask(Task* task)
{
#ifdef DEBUG
	const char* commandName[] = { "Pause", "Unpause", "Pause Post-processing", "Unpause Post-processing",
		"Set download rate", "Execute process", "Execute script",
		"Pause Scan", "Unpause Scan", "Enable Server", "Disable Server", "Fetch Feed" };
	debug("Executing scheduled command: %s", commandName[task->m_command]);
#endif

	bool executeProcess = m_executeProcess || task->m_hours == Task::STARTUP_TASK;

	switch (task->m_command)
	{
		case scDownloadRate:
			if (!task->m_param.Empty())
			{
				g_WorkState->SetSpeedLimit(atoi(task->m_param) * 1024);
				m_downloadRateChanged = true;
			}
			break;

		case scPauseDownload:
		case scUnpauseDownload:
			g_WorkState->SetPauseDownload(task->m_command == scPauseDownload);
			m_pauseDownloadChanged = true;
			break;

		case scPausePostProcess:
		case scUnpausePostProcess:
			g_WorkState->SetPausePostProcess(task->m_command == scPausePostProcess);
			m_pausePostProcessChanged = true;
			break;

		case scPauseScan:
		case scUnpauseScan:
			g_WorkState->SetPauseScan(task->m_command == scPauseScan);
			m_pauseScanChanged = true;
			break;

		case scExtensions:
		case scProcess:
			if (executeProcess)
			{
				SchedulerScriptController::StartScript(task->m_param, task->m_command == scProcess, task->m_id);
			}
			break;

		case scActivateServer:
		case scDeactivateServer:
			EditServer(task->m_command == scActivateServer, task->m_param);
			break;

		case scFetchFeed:
			if (executeProcess)
			{
				FetchFeed(task->m_param);
				break;
			}
	}
}

void Scheduler::PrepareLog()
{
	m_downloadRateChanged = false;
	m_pauseDownloadChanged = false;
	m_pausePostProcessChanged = false;
	m_pauseScanChanged = false;
	m_serverChanged = false;
}

void Scheduler::PrintLog()
{
	if (m_downloadRateChanged)
	{
		info("Scheduler: setting download rate to %i KB/s", g_WorkState->GetSpeedLimit() / 1024);
	}
	if (m_pauseDownloadChanged)
	{
		info("Scheduler: %s download", g_WorkState->GetPauseDownload() ? "pausing" : "unpausing");
	}
	if (m_pausePostProcessChanged)
	{
		info("Scheduler: %s post-processing", g_WorkState->GetPausePostProcess() ? "pausing" : "unpausing");
	}
	if (m_pauseScanChanged)
	{
		info("Scheduler: %s scan", g_WorkState->GetPauseScan() ? "pausing" : "unpausing");
	}
	if (m_serverChanged)
	{
		int index = 0;
		for (NewsServer* server : g_ServerPool->GetServers())
		{
			if (server->GetActive() != m_serverStatusList[index])
			{
				info("Scheduler: %s %s", server->GetActive() ? "activating" : "deactivating", server->GetName());
			}
			index++;
		}
		g_ServerPool->Changed();
	}
}

void Scheduler::EditServer(bool active, const char* serverList)
{
	Tokenizer tok(serverList, ",;");
	while (const char* serverRef = tok.Next())
	{
		int id = atoi(serverRef);
		for (NewsServer* server : g_ServerPool->GetServers())
		{
			if ((id > 0 && server->GetId() == id) ||
				!strcasecmp(server->GetName(), serverRef))
			{
				if (!m_serverChanged)
				{
					// store old server status for logging
					m_serverStatusList.clear();
					m_serverStatusList.reserve(g_ServerPool->GetServers()->size());
					for (NewsServer* server2 : g_ServerPool->GetServers())
					{
						m_serverStatusList.push_back(server2->GetActive());
					}
				}
				m_serverChanged = true;
				server->SetActive(active);
				break;
			}
		}
	}
}

void Scheduler::FetchFeed(const char* feedList)
{
	Tokenizer tok(feedList, ",;");
	while (const char* feedRef = tok.Next())
	{
		int id = atoi(feedRef);
		for (FeedInfo* feed : g_FeedCoordinator->GetFeeds())
		{
			if (feed->GetId() == id ||
				!strcasecmp(feed->GetName(), feedRef) ||
				!strcasecmp("0", feedRef))
			{
				g_FeedCoordinator->FetchFeed(!strcasecmp("0", feedRef) ? 0 : feed->GetId());
				break;
			}
		}
	}
}

void Scheduler::CheckScheduledResume()
{
	time_t resumeTime = g_WorkState->GetResumeTime();
	time_t currentTime = Util::CurrentTime();
	if (resumeTime > 0 && currentTime >= resumeTime)
	{
		info("Autoresume");
		g_WorkState->SetResumeTime(0);
		g_WorkState->SetPauseDownload(false);
		g_WorkState->SetPausePostProcess(false);
		g_WorkState->SetPauseScan(false);
	}
}
