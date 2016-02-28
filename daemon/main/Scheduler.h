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

#include "NString.h"
#include "Thread.h"
#include "Service.h"

class Scheduler : public Service
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
		int				m_id;
		int				m_hours;
		int				m_minutes;
		int				m_weekDaysBits;
		ECommand		m_command;
		CString			m_param;
		time_t			m_lastExecuted = 0;

	public:
						Task(int id, int hours, int minutes, int weekDaysBits, ECommand command,
								const char* param) :
							m_id(id), m_hours(hours), m_minutes(minutes),
							m_weekDaysBits(weekDaysBits), m_command(command), m_param(param) {}
		friend class	Scheduler;
	};

private:

	typedef std::deque<Task*>		TaskList;
	typedef std::vector<bool>		ServerStatusList;

	TaskList			m_taskList;
	Mutex				m_taskListMutex;
	time_t				m_lastCheck = 0;
	bool				m_downloadRateChanged;
	bool				m_executeProcess;
	bool				m_pauseDownloadChanged;
	bool				m_pausePostProcessChanged;
	bool				m_pauseScanChanged;
	bool				m_serverChanged;
	ServerStatusList	m_serverStatusList;
	bool				m_firstChecked = false;

	void				ExecuteTask(Task* task);
	void				CheckTasks();
	void				PrepareLog();
	void				PrintLog();
	void				EditServer(bool active, const char* serverList);
	void				FetchFeed(const char* feedList);
	void				CheckScheduledResume();
	void				FirstCheck();

protected:
	virtual int			ServiceInterval() { return 1000; }
	virtual void		ServiceWork();

public:
						~Scheduler();
	void				AddTask(Task* task);
};

#endif
