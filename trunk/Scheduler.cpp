/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2008-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * $Revision$
 * $Date$
 *
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include "win32.h"
#else
#include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "nzbget.h"
#include "Scheduler.h"
#include "ScriptController.h"
#include "Options.h"
#include "Log.h"
#include "NewsServer.h"
#include "ServerPool.h"
#include "FeedInfo.h"
#include "FeedCoordinator.h"

extern Options* g_pOptions;
extern ServerPool* g_pServerPool;
extern FeedCoordinator* g_pFeedCoordinator;

Scheduler::Task::Task(int iHours, int iMinutes, int iWeekDaysBits, ECommand eCommand, const char* szParam)
{
	m_iHours = iHours;
	m_iMinutes = iMinutes;
	m_iWeekDaysBits = iWeekDaysBits;
	m_eCommand = eCommand;
	m_szParam = szParam ? strdup(szParam) : NULL;
	m_tLastExecuted = 0;
}

Scheduler::Task::~Task()
{
	if (m_szParam)
	{
		free(m_szParam);
	}
}


Scheduler::Scheduler()
{
	debug("Creating Scheduler");

	m_tLastCheck = 0;
	m_TaskList.clear();
}

Scheduler::~Scheduler()
{
	debug("Destroying Scheduler");
	
	for (TaskList::iterator it = m_TaskList.begin(); it != m_TaskList.end(); it++)
	{
		delete *it;
	}
}

void Scheduler::AddTask(Task* pTask)
{
	m_mutexTaskList.Lock();
	m_TaskList.push_back(pTask);
	m_mutexTaskList.Unlock();
}

bool Scheduler::CompareTasks(Scheduler::Task* pTask1, Scheduler::Task* pTask2)
{
	return (pTask1->m_iHours < pTask2->m_iHours) || 
		((pTask1->m_iHours == pTask2->m_iHours) && (pTask1->m_iMinutes < pTask2->m_iMinutes));
}

void Scheduler::FirstCheck()
{
	m_mutexTaskList.Lock();
	m_TaskList.sort(CompareTasks);
	m_mutexTaskList.Unlock();

	// check all tasks for the last week
	time_t tCurrent = time(NULL);
	m_tLastCheck = tCurrent - 60*60*24*7;
	m_bDetectClockChanges = false;
	m_bExecuteProcess = false;
	CheckTasks();
}

void Scheduler::IntervalCheck()
{
	m_bDetectClockChanges = true;
	m_bExecuteProcess = true;
	CheckTasks();
}

void Scheduler::CheckTasks()
{
	PrepareLog();

	m_mutexTaskList.Lock();

	time_t tCurrent = time(NULL);
	struct tm tmCurrent;
	localtime_r(&tCurrent, &tmCurrent);

	struct tm tmLastCheck;

	if (m_bDetectClockChanges)
	{
		// Detect large step changes of system time 
		time_t tDiff = tCurrent - m_tLastCheck;
		if (tDiff > 60*90 || tDiff < -60*90)
		{
			debug("Reset scheduled tasks (detected clock adjustment greater than 90 minutes)");
			m_bExecuteProcess = false;
			m_tLastCheck = tCurrent;

			for (TaskList::iterator it = m_TaskList.begin(); it != m_TaskList.end(); it++)
			{
				Task* pTask = *it;
				pTask->m_tLastExecuted = 0;
			}
		}
	}

	localtime_r(&m_tLastCheck, &tmLastCheck);

	struct tm tmLoop;
	memcpy(&tmLoop, &tmLastCheck, sizeof(tmLastCheck));
	tmLoop.tm_hour = tmCurrent.tm_hour;
	tmLoop.tm_min = tmCurrent.tm_min;
	tmLoop.tm_sec = tmCurrent.tm_sec;
	time_t tLoop = mktime(&tmLoop);

	while (tLoop <= tCurrent)
	{
		for (TaskList::iterator it = m_TaskList.begin(); it != m_TaskList.end(); it++)
		{
			Task* pTask = *it;
			if (pTask->m_tLastExecuted != tLoop)
			{
				struct tm tmAppoint;
				memcpy(&tmAppoint, &tmLoop, sizeof(tmLoop));
				tmAppoint.tm_hour = pTask->m_iHours;
				tmAppoint.tm_min = pTask->m_iMinutes;
				tmAppoint.tm_sec = 0;

				time_t tAppoint = mktime(&tmAppoint);
				int iWeekDay = tmAppoint.tm_wday;
				if (iWeekDay == 0)
				{
					iWeekDay = 7;
				}

				bool bWeekDayOK = pTask->m_iWeekDaysBits == 0 || (pTask->m_iWeekDaysBits & (1 << (iWeekDay - 1)));
				bool bDoTask = bWeekDayOK && m_tLastCheck < tAppoint && tAppoint <= tCurrent;

				//debug("TEMP: 1) m_tLastCheck=%i, tCurrent=%i, tLoop=%i, tAppoint=%i, bWeekDayOK=%i, bDoTask=%i", m_tLastCheck, tCurrent, tLoop, tAppoint, (int)bWeekDayOK, (int)bDoTask);

				if (bDoTask)
				{
					ExecuteTask(pTask);
					pTask->m_tLastExecuted = tLoop;
				}
			}
		}
		tLoop += 60*60*24; // inc day
		localtime_r(&tLoop, &tmLoop);
	}

	m_tLastCheck = tCurrent;

	m_mutexTaskList.Unlock();

	PrintLog();
}

void Scheduler::ExecuteTask(Task* pTask)
{
	const char* szCommandName[] = { "Pause", "Unpause", "Set download rate", "Execute program", "Pause Scan", "Unpause Scan",
		"Enable Server", "Disable Server", "Fetch Feed" };
	debug("Executing scheduled command: %s", szCommandName[pTask->m_eCommand]);

	switch (pTask->m_eCommand)
	{
		case scDownloadRate:
			if (!Util::EmptyStr(pTask->m_szParam))
			{
				g_pOptions->SetDownloadRate(atoi(pTask->m_szParam) * 1024);
				m_bDownloadRateChanged = true;
			}
			break;

		case scPauseDownload:
		case scUnpauseDownload:
			m_bPauseDownload = pTask->m_eCommand == scPauseDownload;
			m_bPauseDownloadChanged = true;
			break;

		case scProcess:
			if (m_bExecuteProcess)
			{
				SchedulerScriptController::StartScript(pTask->m_szParam);
			}
			break;

		case scPauseScan:
		case scUnpauseScan:
			g_pOptions->SetPauseScan(pTask->m_eCommand == scPauseScan);
			m_bPauseScanChanged = true;
			break;

		case scActivateServer:
		case scDeactivateServer:
			EditServer(pTask->m_eCommand == scActivateServer, pTask->m_szParam);
			break;

		case scFetchFeed:
			if (m_bExecuteProcess)
			{
				FetchFeed(pTask->m_szParam);
				break;
			}
	}
}

void Scheduler::PrepareLog()
{
	m_bDownloadRateChanged = false;
	m_bPauseDownloadChanged = false;
	m_bPauseScanChanged = false;
	m_bServerChanged = false;
}

void Scheduler::PrintLog()
{
	if (m_bDownloadRateChanged)
	{
		info("Scheduler: setting download rate to %i KB/s", g_pOptions->GetDownloadRate() / 1024);
	}
	if (m_bPauseScanChanged)
	{
		info("Scheduler: %s scan", g_pOptions->GetPauseScan() ? "pausing" : "unpausing");
	}
	if (m_bServerChanged)
	{
		int index = 0;
		for (Servers::iterator it = g_pServerPool->GetServers()->begin(); it != g_pServerPool->GetServers()->end(); it++, index++)
		{
			NewsServer* pServer = *it;
			if (pServer->GetActive() != m_ServerStatusList[index])
			{
				info("Scheduler: %s %s", pServer->GetActive() ? "activating" : "deactivating", pServer->GetName());
			}
		}
		g_pServerPool->Changed();
	}
}

void Scheduler::EditServer(bool bActive, const char* szServerList)
{
	char* szServerList2 = strdup(szServerList);
	char* saveptr;
	char* szServer = strtok_r(szServerList2, ",;", &saveptr);
	while (szServer)
	{
		szServer = Util::Trim(szServer);
		if (!Util::EmptyStr(szServer))
		{
			int iID = atoi(szServer);
			for (Servers::iterator it = g_pServerPool->GetServers()->begin(); it != g_pServerPool->GetServers()->end(); it++)
			{
				NewsServer* pServer = *it;
				if ((iID > 0 && pServer->GetID() == iID) ||
					!strcasecmp(pServer->GetName(), szServer))
				{
					if (!m_bServerChanged)
					{
						// store old server status for logging
						m_ServerStatusList.clear();
						m_ServerStatusList.reserve(g_pServerPool->GetServers()->size());
						for (Servers::iterator it2 = g_pServerPool->GetServers()->begin(); it2 != g_pServerPool->GetServers()->end(); it2++)
						{
							NewsServer* pServer2 = *it2;
							m_ServerStatusList.push_back(pServer2->GetActive());
						}
					}
					m_bServerChanged = true;
					pServer->SetActive(bActive);
					break;
				}
			}
		}
		szServer = strtok_r(NULL, ",;", &saveptr);
	}
	free(szServerList2);
}

void Scheduler::FetchFeed(const char* szFeedList)
{
	char* szFeedList2 = strdup(szFeedList);
	char* saveptr;
	char* szFeed = strtok_r(szFeedList2, ",;", &saveptr);
	while (szFeed)
	{
		szFeed = Util::Trim(szFeed);
		if (!Util::EmptyStr(szFeed))
		{
			int iID = atoi(szFeed);
			for (Feeds::iterator it = g_pFeedCoordinator->GetFeeds()->begin(); it != g_pFeedCoordinator->GetFeeds()->end(); it++)
			{
				FeedInfo* pFeed = *it;
				if (pFeed->GetID() == iID ||
					!strcasecmp(pFeed->GetName(), szFeed) ||
					!strcasecmp("0", szFeed))
				{
					g_pFeedCoordinator->FetchFeed(pFeed->GetID());
					break;
				}
			}
		}
		szFeed = strtok_r(NULL, ",;", &saveptr);
	}
	free(szFeedList2);
}
