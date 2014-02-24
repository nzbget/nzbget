/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2005 Bo Cordes Petersen <placebodk@users.sourceforge.net>
 *  Copyright (C) 2007-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef REMOTESERVER_H
#define REMOTESERVER_H

#include "Thread.h"
#include "Connection.h"

class RemoteServer : public Thread
{
private:
	bool				m_bTLS;
	Connection*			m_pConnection;

public:
						RemoteServer(bool bTLS);
						~RemoteServer();
	virtual void		Run();
	virtual void 		Stop();
};

class RequestProcessor : public Thread
{
private:
	bool				m_bTLS;
	Connection*			m_pConnection;

public:
						~RequestProcessor();
	virtual void		Run();
	void				SetTLS(bool bTLS) { m_bTLS = bTLS; }
	void				SetConnection(Connection* pConnection) { m_pConnection = pConnection; }
};

#endif
