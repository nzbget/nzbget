/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2008-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <list>
#include <vector>
#include <time.h>

#include "Thread.h"

class Scheduler
{
public:
	enum ECommand
	{
		scPauseDownload,
		scUnpauseDownload,
		scPausePostProcess,
		scUnpausePostProcess,
		scDownloadRate,
		scScript,
		scProcess,
		scPauseScan,
		scUnpauseScan,
		scActivateServer,
		scDeactivateServer,
		scFetchFeed
	};

	class Task
	{
	private:
		int				m_iID;
		int				m_iHours;
		int				m_iMinutes;
		int				m_iWeekDaysBits;
		ECommand		m_eCommand;
		char*			m_szParam;
		time_t			m_tLastExecuted;

	public:
						Task(int iID, int iHours, int iMinutes, int iWeekDaysBits, ECommand eCommand, 
							const char* szParam);
						~Task();
		friend class	Scheduler;
	};

private:

	typedef std::list<Task*>		TaskList;
	typedef std::vector<bool>		ServerStatusList;

	TaskList			m_TaskList;
	Mutex				m_mutexTaskList;
	time_t				m_tLastCheck;
	bool				m_bDownloadRateChanged;
	bool				m_bExecuteProcess;
	bool				m_bPauseDownloadChanged;
	bool				m_bPausePostProcessChanged;
	bool				m_bPauseScanChanged;
	bool				m_bServerChanged;
	ServerStatusList	m_ServerStatusList;
	void				ExecuteTask(Task* pTask);
	void				CheckTasks();
	static bool			CompareTasks(Scheduler::Task* pTask1, Scheduler::Task* pTask2);
	void				PrepareLog();
	void				PrintLog();
	void				EditServer(bool bActive, const char* szServerList);
	void				FetchFeed(const char* szFeedList);
	void				CheckScheduledResume();

public:
						Scheduler();
						~Scheduler();
	void				AddTask(Task* pTask);
	void				FirstCheck();
	void				IntervalCheck();
};

extern Scheduler* g_pScheduler;

#endif
