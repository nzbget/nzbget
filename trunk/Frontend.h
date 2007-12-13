/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2004  Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007  Andrei Prygounkov <hugbug@users.sourceforge.net>
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


#ifndef FRONTEND_H
#define FRONTEND_H

#include "Thread.h"
#include "Log.h"
#include "DownloadInfo.h"
#include "MessageBase.h"

class Frontend : public Thread
{
public:
	enum EEditAction
	{
		eaPauseUnpause,
		eaDelete,
		eaMoveUp,
		eaMoveDown,
		eaMoveTop,
		eaMoveBottom
	};
	
private:
	Log::Messages		m_RemoteMessages;
	DownloadQueue		m_RemoteQueue;

	bool				RequestMessages();
	bool				RequestFileList();

protected:
	bool				m_bSummary;
	bool				m_bFileList;
	unsigned int				m_iNeededLogEntries;
	unsigned int				m_iNeededLogFirstID;

	// summary
	float				m_fCurrentDownloadSpeed;
	long long 			m_lRemainingSize;
	bool				m_bPause;
	float				m_fDownloadLimit;
	int					m_iThreadCount;

	bool				PrepareData();
	void				FreeData();
	Log::Messages*		LockMessages();
	void				UnlockMessages();
	DownloadQueue*		LockQueue();
	void				UnlockQueue();
	bool				IsRemoteMode();
	void				InitMessageBase(SNZBMessageBase* pMessageBase, int iRequest, int iSize);
	void				ServerPauseUnpause(bool bPause);
	bool				RequestPauseUnpause(bool bPause);
	void				ServerSetDownloadRate(float fRate);
	bool				RequestSetDownloadRate(float fRate);
	void				ServerDumpDebug();
	bool				RequestDumpDebug();
	bool 				ServerEditQueue(EEditAction eAction, int iEntry);
	bool 				RequestEditQueue(int iAction, int iOffset, int iID);

public:
						Frontend();
};

#endif
