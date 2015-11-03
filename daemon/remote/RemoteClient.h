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
	class MatchedNzbInfo: public NzbInfo
	{
	public:
		bool		m_match;
	};

	class MatchedFileInfo: public FileInfo
	{
	public:
		bool		m_match;
	};

	Connection* 	m_connection;
	bool			m_verbose;

	bool			InitConnection();
	void			InitMessageBase(SNzbRequestBase* messageBase, int request, int size);
	bool			ReceiveBoolResponse();
	void			printf(const char* msg, ...);
	void			perror(const char* msg);

public:
					RemoteClient();
					~RemoteClient();
	void			SetVerbose(bool verbose) { m_verbose = verbose; };
	bool 			RequestServerDownload(const char* nzbFilename, const char* nzbContent, const char* category,
						bool addFirst, bool addPaused, int priority,
						const char* dupeKey, int dupeMode, int dupeScore);
	bool			RequestServerList(bool files, bool groups, const char* pattern);
	bool			RequestServerPauseUnpause(bool pause, ERemotePauseUnpauseAction action);
	bool			RequestServerSetDownloadRate(int rate);
	bool			RequestServerDumpDebug();
	bool 			RequestServerEditQueue(DownloadQueue::EEditAction action, int offset, const char* text,
						int* idList, int idCount, NameList* nameList, ERemoteMatchMode matchMode);
	bool			RequestServerLog(int lines);
	bool			RequestServerShutdown();
	bool			RequestServerReload();
	bool			RequestServerVersion();
	bool			RequestPostQueue();
	bool 			RequestWriteLog(int kind, const char* text);
	bool			RequestScan(bool syncMode);
	bool			RequestHistory(bool withHidden);
	void			BuildFileList(SNzbListResponse* listResponse, const char* trailingData, DownloadQueue* downloadQueue);
};

#endif
