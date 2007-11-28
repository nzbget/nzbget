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


#ifndef REMOTESERVER_H
#define REMOTESERVER_H

#include "Thread.h"
#include "NetAddress.h"
#include "Connection.h"
#include "MessageBase.h"

static int const REQUESTBUFFERSIZE = 8192;

class RemoteServer : public Thread
{
private:
	NetAddress*			m_pNetAddress;
	Connection*			m_pConnection;

public:
						RemoteServer();
						~RemoteServer();
	virtual void		Run();
	virtual void 		Stop();
};

class MessageCommand : public Thread
{
private:
	SOCKET				m_iSocket;
	char				m_RequestBuffer[REQUESTBUFFERSIZE];
	int					m_iExtraDataLength;

	void				ProcessRequest();
	void				RequestDownload();
	void				RequestList();
	void				RequestLog();
	void				RequestEditQueue();
	void				SendResponse(char* szAnswer);

public:
	void				SetSocket(SOCKET iSocket);
	virtual void		Run();
};

#endif
