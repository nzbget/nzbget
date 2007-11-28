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

/*
 *
 * m_Semaphore Patch by Florian Penzkofer <f.penzkofer@sent.com>
 * The queue of mutexes that was used did not work for every
 * implementation of POSIX threads. Now a m_Semaphore is used.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <unistd.h>
#include <errno.h>
#endif

#include "nzbget.h"
#include "ServerPool.h"
#include "Log.h"

ServerPool::ServerPool()
{
	debug("Creating ServerPool");

	m_iMaxLevel = 0;
	m_iTimeout = 60;
	m_Servers.clear();
	m_FreeConnections.clear();
	m_Semaphores.clear();
}

ServerPool::~ ServerPool()
{
	debug("Destroying ServerPool");

	m_FreeConnections.clear();

	for (Semaphores::iterator it = m_Semaphores.begin(); it != m_Semaphores.end(); it++)
	{
		delete *it;
	}
	m_Semaphores.clear();

	for (Servers::iterator it = m_Servers.begin(); it != m_Servers.end(); it++)
	{
		delete *it;
	}
	m_Servers.clear();
}

void ServerPool::AddServer(NewsServer* pNewsServer)
{
	debug("Adding server to ServerPool");

	m_Servers.push_back(pNewsServer);
}

void ServerPool::InitConnections()
{
	debug("Initializing connections in ServerPool");

	m_iMaxLevel = 0;
	for (Servers::iterator it = m_Servers.begin(); it != m_Servers.end(); it++)
	{
		NewsServer* pNewsServer = *it;
		if (m_iMaxLevel < pNewsServer->GetLevel())
		{
			m_iMaxLevel = pNewsServer->GetLevel();
		}
		for (int i = 0; i < pNewsServer->GetMaxConnections(); i++)
		{
			m_FreeConnections.push_back(pNewsServer);
		}
	}

	for (int iLevel = 0; iLevel <= m_iMaxLevel; iLevel++)
	{
		int iMaxConnectionsForLevel = 0;
		for (Servers::iterator it = m_Servers.begin(); it != m_Servers.end(); it++)
		{
			NewsServer* pNewsServer = *it;
			if (iLevel == pNewsServer->GetLevel())
			{
				iMaxConnectionsForLevel += pNewsServer->GetMaxConnections();
			}
		}

		Semaphore* sem = new Semaphore(iMaxConnectionsForLevel);
		m_Semaphores.push_back(sem);
	}
}

NNTPConnection* ServerPool::GetConnection(int level)
{
	debug("Getting connection");

	debug("sem_wait...");
	// decrease m_Semaphore counter or block
	bool bWaitVal = m_Semaphores[level]->Wait();
	debug("sem_wait...OK");

	if (!bWaitVal)
	{
		debug("semaphore error: %i", errno);
		return NULL;
	}

	m_mutexFree.Lock();

	NNTPConnection* pConnection = NULL;
	for (Servers::iterator it = m_FreeConnections.begin(); it != m_FreeConnections.end(); it++)
	{
		NewsServer* server = *it;
		if (server->GetLevel() == level)
		{
			// free connection found, take it!
			pConnection = new NNTPConnection(server);
			pConnection->SetTimeout(m_iTimeout);
			m_FreeConnections.erase(it);
			break;
		}
	}

	m_mutexFree.Unlock();

	if (!pConnection)
	{
		error("ServerPool: serious error, no free connection found, but there should be one.");
	}

	return pConnection;
}

void ServerPool::FreeConnection(NNTPConnection* pConnection)
{
	debug("Freeing connection");

	// give back free connection
	m_mutexFree.Lock();
	m_FreeConnections.push_back(pConnection->GetNewsServer());
	m_Semaphores[pConnection->GetNewsServer()->GetLevel()]->Post();
	m_mutexFree.Unlock();

	delete pConnection;
}

bool ServerPool::HasFreeConnection()
{
	return !m_Semaphores[0]->IsLocked();
}

void ServerPool::LogDebugInfo()
{
	debug("   ServerPool");
	debug("   ----------------");

	debug("    Max-Level: %i", m_iMaxLevel);

	m_mutexFree.Lock();

	debug("    Free Connections: %i", m_FreeConnections.size());
	for (Servers::iterator it = m_FreeConnections.begin(); it != m_FreeConnections.end(); it++)
	{
		debug("      Free Connection: level=%i", (*it)->GetLevel());
	}
/*
	debug("    Semaphores: %i", m_Semaphores.size());
	for (int iLevel = 0; iLevel <= m_iMaxLevel; iLevel++)
	{
		sem_t* sem = m_Semaphores[iLevel];
		int iSemValue;
		sem_getvalue(sem, &iSemValue);
		debug("      Semaphore: level=%i, value=%i", iLevel, iSemValue);
	}
*/
	m_mutexFree.Unlock();
}
