/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
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


#ifndef FRONTEND_H
#define FRONTEND_H

#include "Thread.h"
#include "Log.h"
#include "DownloadInfo.h"
#include "MessageBase.h"
#include "QueueEditor.h"

class Frontend : public Thread
{
private:
	Log::Messages		m_RemoteMessages;

	bool				RequestMessages();
	bool				RequestFileList();

protected:
	bool				m_bSummary;
	bool				m_bFileList;
	unsigned int		m_iNeededLogEntries;
	unsigned int		m_iNeededLogFirstID;
	int					m_iUpdateInterval;

	// summary
	int					m_iCurrentDownloadSpeed;
	long long 			m_lRemainingSize;
	bool				m_bPauseDownload;
	int					m_iDownloadLimit;
	int					m_iThreadCount;
	int					m_iPostJobCount;
	int					m_iUpTimeSec;
	int					m_iDnTimeSec;
	long long			m_iAllBytes;
	bool				m_bStandBy;

	bool				PrepareData();
	void				FreeData();
	Log::Messages*		LockMessages();
	void				UnlockMessages();
	DownloadQueue*		LockQueue();
	void				UnlockQueue();
	bool				IsRemoteMode();
	void				InitMessageBase(SNZBRequestBase* pMessageBase, int iRequest, int iSize);
	void				ServerPauseUnpause(bool bPause);
	bool				RequestPauseUnpause(bool bPause);
	void				ServerSetDownloadRate(int iRate);
	bool				RequestSetDownloadRate(int iRate);
	bool 				ServerEditQueue(DownloadQueue::EEditAction eAction, int iOffset, int iEntry);
	bool 				RequestEditQueue(DownloadQueue::EEditAction eAction, int iOffset, int iID);

public:
						Frontend();
};

#endif
