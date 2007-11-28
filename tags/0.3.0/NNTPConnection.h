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


#ifndef NNTPCONNECTION_H
#define NNTPCONNECTION_H

#include <stdio.h>
#include <vector>

#include "NewsServer.h"
#include "Connection.h"

class NNTPConnection : public Connection
{
private:
	std::vector <char*> 	m_UnavailableGroups;
	char* 					m_szActiveGroup;
	static const int		LineBufSize = 1024*10;
	char*					m_szLineBuf;

	virtual int 			DoConnect();
	virtual int 			DoDisconnect();

public:
							NNTPConnection(NewsServer* server);
							~NNTPConnection();
	NewsServer*				GetNewsServer() { return(NewsServer*)m_pNetAddress; }
	char* 					Request(char* req);
	int 					Authenticate();
	int 					AuthInfoUser(int iRecur = 0);
	int 					AuthInfoPass(int iRecur = 0);
	int 					JoinGroup(char* grp);
};


#endif

