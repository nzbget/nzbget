/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2008 Andrei Prygounkov <hugbug@users.sourceforge.net>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Revision$
 * $Date$
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "nzbget.h"
#include "Scheduler.h"
#include "ScriptController.h"
#include "Options.h"
#include "Log.h"

extern Options* g_pOptions;

Scheduler::Task::Task(int iHours, int iMinutes, int iWeekDaysBits, ECommand eCommand, 
	int iDownloadRate, const char* szProcess)
{
	m_iHours = iHours;
	m_iMinutes = iMinutes;
	m_iWeekDaysBits = iWeekDaysBits;
	m_eCommand = eCommand;
	m_iDownloadRate = iDownloadRate;
	m_szProcess = NULL;
	if (szProcess)
	{
		m_szProcess = strdup(szProcess);
	}
	m_tLastExecuted = 0;
}

Scheduler::Task::~Task()
{
	if (m_szProcess)
	{
		free(m_szProcess);
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
	m_bDownloadRateChanged = false;
	m_bPauseChanged = false;
	CheckTasks();
}

void Scheduler::IntervalCheck()
{
	m_bDetectClockChanges = true;
	m_bExecuteProcess = true;
	m_bDownloadRateChanged = false;
	m_bPauseChanged = false;
	CheckTasks();
}

void Scheduler::CheckTasks()
{
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
}

void Scheduler::ExecuteTask(Task* pTask)
{
	if (pTask->m_eCommand == scDownloadRate)
	{
		debug("Executing scheduled command: Set download rate to %i", pTask->m_iDownloadRate);
	}
	else
	{
		const char* szCommandName[] = { "Pause", "Unpause", "Set download rate", "Execute program" };
		debug("Executing scheduled command: %s", szCommandName[pTask->m_eCommand]);
	}

	switch (pTask->m_eCommand)
	{
		case scDownloadRate:
			m_iDownloadRate = pTask->m_iDownloadRate;
			m_bDownloadRateChanged = true;
			break;

		case scPause:
		case scUnpause:
			m_bPause = pTask->m_eCommand == scPause;
			m_bPauseChanged = true;
			break;

		case scProcess:
			if (m_bExecuteProcess)
			{
				SchedulerScriptController::StartScript(pTask->m_szProcess);
			}
			break;
	}
}
