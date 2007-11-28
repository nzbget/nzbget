/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2004  Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2005	Florian Penzkofer <f.penzkofer@sent.com>
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


#ifndef SERVERPOOL_H
#define SERVERPOOL_H

#include <list>

#include "Thread.h"
#include "NewsServer.h"
#include "NNTPConnection.h"

class ServerPool
{
private:
	typedef std::vector<NewsServer*>		Servers;
	typedef std::vector<Semaphore*>			Semaphores;

	Servers				m_Servers;
	Servers				m_FreeConnections;
	Semaphores			m_Semaphores;
	int					m_iMaxLevel;
	Mutex			 	m_mutexFree;
	int					m_iTimeout;

public:
	ServerPool();
	~ServerPool();
	void				SetTimeout(int iTimeout) { m_iTimeout = iTimeout; }
	void 				AddServer(NewsServer *s);
	void				InitConnections();
	int					GetMaxLevel() { return m_iMaxLevel; }
	NNTPConnection*		GetConnection(int level);
	bool				HasFreeConnection();
	void 				FreeConnection(NNTPConnection* con);

	void				LogDebugInfo();
};

#endif
