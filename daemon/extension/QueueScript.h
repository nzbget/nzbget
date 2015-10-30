/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include "DownloadInfo.h"
#include "ScriptConfig.h"

class QueueScriptCoordinator
{
public:
	enum EEvent
	{
		qeFileDownloaded,	// lowest priority
		qeUrlCompleted,
		qeNzbAdded,
		qeNzbDownloaded,
		qeNzbDeleted		// highest priority
	};

private:
	class QueueItem
	{
	private:
		int					m_nzbId;
		ScriptConfig::Script*	m_script;
		EEvent				m_event;
	public:
							QueueItem(int nzbId, ScriptConfig::Script* script, EEvent event);
		int					GetNzbId() { return m_nzbId; }
		ScriptConfig::Script*	GetScript() { return m_script; }
		EEvent				GetEvent() { return m_event; }
	};

	typedef std::list<QueueItem*> Queue;
	
	Queue				m_queue;
	Mutex				m_queueMutex;
	QueueItem*			m_curItem;
	bool				m_hasQueueScripts;
	bool				m_stopped;

	void				StartScript(NzbInfo* nzbInfo, QueueItem* queueItem);
	NzbInfo*			FindNzbInfo(DownloadQueue* downloadQueue, int nzbId);

public:
						QueueScriptCoordinator();
						~QueueScriptCoordinator();
	void				Stop() { m_stopped = true; }
	void				InitOptions();
	void				EnqueueScript(NzbInfo* nzbInfo, EEvent event);
	void				CheckQueue();
	bool				HasJob(int nzbId, bool* active);
	int					GetQueueSize();
};

extern QueueScriptCoordinator* g_pQueueScriptCoordinator;

#endif
