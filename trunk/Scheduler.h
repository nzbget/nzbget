/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2008-2009 Andrei Prygounkov <hugbug@users.sourceforge.net>
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


#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <list>

#include "Thread.h"

class Scheduler
{
public:
	enum ECommand
	{
		scPauseDownload,
		scUnpauseDownload,
		scDownloadRate,
		scProcess,
		scPauseScan,
		scUnpauseScan
	};

	class Task
	{
	private:
		int				m_iHours;
		int				m_iMinutes;
		int				m_iWeekDaysBits;
		ECommand		m_eCommand;
		int				m_iDownloadRate;
		char*			m_szProcess;
		time_t			m_tLastExecuted;

	public:
						Task(int iHours, int iMinutes, int iWeekDaysBits, ECommand eCommand, 
							int iDownloadRate, const char* szProcess);
						~Task();
		friend class	Scheduler;
	};

private:

	typedef std::list<Task*>		TaskList;

	TaskList			m_TaskList;
	Mutex				m_mutexTaskList;
	time_t				m_tLastCheck;
	bool				m_bDetectClockChanges;
	bool				m_bDownloadRateChanged;
	bool				m_bExecuteProcess;
	int					m_iDownloadRate;
	bool				m_bPauseDownloadChanged;
	bool				m_bPauseDownload;
	bool				m_bPauseScanChanged;
	bool				m_bPauseScan;
	void				ExecuteTask(Task* pTask);
	void				CheckTasks();
	static bool			CompareTasks(Scheduler::Task* pTask1, Scheduler::Task* pTask2);

public:
						Scheduler();
						~Scheduler();
	void				AddTask(Task* pTask);
	void				FirstCheck();
	void				IntervalCheck();
	bool				GetDownloadRateChanged() { return m_bDownloadRateChanged; }
	int					GetDownloadRate() { return m_iDownloadRate; }
	bool				GetPauseDownloadChanged() { return m_bPauseDownloadChanged; }
	bool				GetPauseDownload() { return m_bPauseDownload; }
	bool				GetPauseScanChanged() { return m_bPauseScanChanged; }
	bool				GetPauseScan() { return m_bPauseScan; }
};

#endif
