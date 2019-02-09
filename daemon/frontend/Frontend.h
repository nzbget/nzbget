/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef FRONTEND_H
#define FRONTEND_H

#include "Thread.h"
#include "Log.h"
#include "DownloadInfo.h"
#include "MessageBase.h"
#include "QueueEditor.h"
#include "Observer.h"

class Frontend : public Thread
{
public:
	Frontend();

protected:
	bool m_summary = false;
	bool m_fileList = false;
	uint32 m_neededLogEntries = 0;
	uint32 m_neededLogFirstId = 0;
	int m_updateInterval;

	// summary
	int m_currentDownloadSpeed = 0;
	int64 m_remainingSize = 0;
	bool m_pauseDownload = false;
	int m_downloadLimit = 0;
	int m_threadCount = 0;
	int m_postJobCount = 0;
	int m_upTimeSec = 0;
	int m_dnTimeSec = 0;
	int64 m_allBytes = 0;
	bool m_standBy = false;
	Mutex m_waitMutex;
	ConditionVar m_waitCond;

	virtual void Stop();
	bool PrepareData();
	void FreeData();
	GuardedMessageList GuardMessages();
	bool IsRemoteMode();
	void InitMessageBase(SNzbRequestBase* messageBase, int request, int size);
	void ServerPauseUnpause(bool pause);
	bool RequestPauseUnpause(bool pause);
	void ServerSetDownloadRate(int rate);
	bool RequestSetDownloadRate(int rate);
	bool ServerEditQueue(DownloadQueue::EEditAction action, int offset, int entry);
	bool RequestEditQueue(DownloadQueue::EEditAction action, int offset, int id);
	void Wait(int milliseconds);

private:
	class WorkStateObserver : public Observer
	{
	public:
		Frontend* m_owner;
		virtual void Update(Subject* caller, void* aspect) { m_owner->WorkStateUpdate(caller, aspect); }
	};

	MessageList m_remoteMessages;
	WorkStateObserver m_workStateObserver;

	bool RequestMessages();
	bool RequestFileList();
	void WorkStateUpdate(Subject* caller, void* aspect);
};

#endif
