/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2005  Bo Cordes Petersen <placebodk@users.sourceforge.net>
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


#ifndef REMOTECLIENT_H
#define REMOTECLIENT_H

#include "Options.h"
#include "MessageBase.h"
#include "Connection.h"

class RemoteClient
{
private:
	Connection* 	m_pConnection;
	NetAddress*		m_pNetAddress;
	bool			m_bVerbose;

	bool			InitConnection();
	void			InitMessageBase(SNZBMessageBase* pMessageBase, int iRequest, int iSize);
	void			ReceiveCommandResult();
	void			printf(char* msg, ...);
	void			perror(char* msg);

public:
	RemoteClient();
	~RemoteClient();
	void			SetVerbose(bool bVerbose) { m_bVerbose = bVerbose; };
	bool 			RequestServerDownload(const char* szName, bool bAddFirst);
	bool			RequestServerList();
	bool			RequestServerPauseUnpause(bool bPause);
	bool			RequestServerSetDownloadRate(float fRate);
	bool			RequestServerDumpDebug();
	bool 			RequestServerEditQueue(int iAction, int iOffset, int* pIDList, int iIDCount, bool bSmartOrder);
	bool			RequestServerLog(int iLines);
	bool			RequestServerShutdown();
};

#endif
