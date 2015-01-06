/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2005 Bo Cordes Petersen <placebodk@users.sourceforge.net>
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


#ifndef REMOTECLIENT_H
#define REMOTECLIENT_H

#include "Options.h"
#include "MessageBase.h"
#include "Connection.h"
#include "DownloadInfo.h"

class RemoteClient
{
private:
	class MatchedNZBInfo: public NZBInfo
	{
	public:
		bool		m_bMatch;
	};

	class MatchedFileInfo: public FileInfo
	{
	public:
		bool		m_bMatch;
	};

	Connection* 	m_pConnection;
	bool			m_bVerbose;

	bool			InitConnection();
	void			InitMessageBase(SNZBRequestBase* pMessageBase, int iRequest, int iSize);
	bool			ReceiveBoolResponse();
	void			printf(const char* msg, ...);
	void			perror(const char* msg);

public:
					RemoteClient();
					~RemoteClient();
	void			SetVerbose(bool bVerbose) { m_bVerbose = bVerbose; };
	bool 			RequestServerDownload(const char* szFilename, const char* szCategory, bool bAddFirst, bool bAddPaused, int iPriority);
	bool			RequestServerList(bool bFiles, bool bGroups, const char* szPattern);
	bool			RequestServerPauseUnpause(bool bPause, eRemotePauseUnpauseAction iAction);
	bool			RequestServerSetDownloadRate(int iRate);
	bool			RequestServerDumpDebug();
	bool 			RequestServerEditQueue(DownloadQueue::EEditAction eAction, int iOffset, const char* szText,
						int* pIDList, int iIDCount, NameList* pNameList, eRemoteMatchMode iMatchMode);
	bool			RequestServerLog(int iLines);
	bool			RequestServerShutdown();
	bool			RequestServerReload();
	bool			RequestServerVersion();
	bool			RequestPostQueue();
	bool 			RequestWriteLog(int iKind, const char* szText);
	bool			RequestScan(bool bSyncMode);
	bool			RequestHistory(bool bWithHidden);
	bool 			RequestServerDownloadUrl(const char* szURL, const char* szNZBFilename, const char* szCategory, bool bAddFirst, bool bAddPaused, int iPriority);
	void			BuildFileList(SNZBListResponse* pListResponse, const char* pTrailingData, DownloadQueue* pDownloadQueue);
};

#endif
