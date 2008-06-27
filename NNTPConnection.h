/*
 *  This file is part of nzbget
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


#ifndef NNTPCONNECTION_H
#define NNTPCONNECTION_H

#include "NewsServer.h"
#include "Connection.h"

class NNTPConnection : public Connection
{
private:
	char* 					m_szActiveGroup;
	char*					m_szLineBuf;
	bool					m_bAuthError;

	virtual int 			DoConnect();
	virtual int 			DoDisconnect();
	void					Clear();

public:
							NNTPConnection(NewsServer* server);
							~NNTPConnection();
	NewsServer*				GetNewsServer() { return(NewsServer*)m_pNetAddress; }
	const char* 			Request(const char* req);
	bool 					Authenticate();
	bool 					AuthInfoUser(int iRecur = 0);
	bool 					AuthInfoPass(int iRecur = 0);
	const char*				JoinGroup(const char* grp);
	bool					GetAuthError() { return m_bAuthError; }
};


#endif

