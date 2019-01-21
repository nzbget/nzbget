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
		scExtensions,
		scProcess,
		scPauseScan,
		scUnpauseScan,
		scActivateServer,
		scDeactivateServer,
		scFetchFeed
	};

	class Task
	{
	public:
		Task(int id, int hours, int minutes, int weekDaysBits, ECommand command,
			const char* param) :
			m_id(id), m_hours(hours), m_minutes(minutes),
			m_weekDaysBits(weekDaysBits), m_command(command), m_param(param) {}
		friend class Scheduler;
		static const int STARTUP_TASK = -1;
	private:
		int m_id;
		int m_hours;
		int m_minutes;
		int m_weekDaysBits;
		ECommand m_command;
		CString m_param;
		time_t m_lastExecuted = 0;
	};

	void AddTask(std::unique_ptr<Task> task);

protected:
	virtual int ServiceInterval() { return m_serviceInterval; }
	virtual void ServiceWork();

private:
	typedef std::vector<std::unique_ptr<Task>> TaskList;
	typedef std::vector<bool> ServerStatusList;

	TaskList m_taskList;
	Mutex m_taskListMutex;
	time_t m_lastCheck = 0;
	bool m_downloadRateChanged;
	bool m_executeProcess;
	bool m_pauseDownloadChanged;
	bool m_pausePostProcessChanged;
	bool m_pauseScanChanged;
	bool m_serverChanged;
	ServerStatusList m_serverStatusList;
	bool m_firstChecked = false;
	int m_serviceInterval = 1;

	void ExecuteTask(Task* task);
	void CheckTasks();
	void PrepareLog();
	void PrintLog();
	void EditServer(bool active, const char* serverList);
	void FetchFeed(const char* feedList);
	void CheckScheduledResume();
	void FirstCheck();
	void ScheduleNextWork();
};

#endif
