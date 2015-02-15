/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef QUEUESCRIPT_H
#define QUEUESCRIPT_H

#include <list>

#include "Script.h"
#include "Thread.h"
#include "DownloadInfo.h"
#include "Options.h"

class NZBScriptController : public ScriptController
{
protected:
	void				PrepareEnvParameters(NZBParameterList* pParameters, const char* szStripPrefix);
	void				PrepareEnvScript(NZBParameterList* pParameters, const char* szScriptName);
	void				ExecuteScriptList(const char* szScriptList);
	virtual void		ExecuteScript(Options::Script* pScript) = 0;
};

class QueueScriptCoordinator
{
public:
	enum EEvent
	{
		qeFileDownloaded,	// lowest priority
		qeNzbAdded,
		qeNzbDownloaded		// highest priority
	};

private:
	class QueueItem
	{
	private:
		int					m_iNZBID;
		Options::Script*	m_pScript;
		EEvent				m_eEvent;
	public:
							QueueItem(int iNZBID, Options::Script* pScript, EEvent eEvent);
		int					GetNZBID() { return m_iNZBID; }
		Options::Script*	GetScript() { return m_pScript; }
		EEvent				GetEvent() { return m_eEvent; }
	};

	typedef std::list<QueueItem*> Queue;
	
	Queue				m_Queue;
	Mutex				m_mutexQueue;
	QueueItem*			m_pCurItem;
	bool				m_bHasQueueScripts;

	void				StartScript(NZBInfo* pNZBInfo, QueueItem* pQueueItem);

public:
						QueueScriptCoordinator();
						~QueueScriptCoordinator();
	void				InitOptions();
	void				EnqueueScript(NZBInfo* pNZBInfo, EEvent eEvent);
	void				CheckQueue();
	bool				HasJob(int iNZBID);
};

#endif
