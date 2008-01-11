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

static const int CONNECTION_HOLD_SECODNS = 5;

ServerPool::PooledConnection::PooledConnection(NewsServer* server) : NNTPConnection(server)
{
	m_bInUse = false;
	m_tFreeTime = 0;
}

ServerPool::ServerPool()
{
	debug("Creating ServerPool");

	m_iMaxLevel = 0;
	m_iTimeout = 60;
	m_Servers.clear();
	m_Connections.clear();
	m_Semaphores.clear();
}

ServerPool::~ ServerPool()
{
	debug("Destroying ServerPool");

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

	for (Connections::iterator it = m_Connections.begin(); it != m_Connections.end(); it++)
	{
		delete *it;
	}
	m_Connections.clear();
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
			PooledConnection* pConnection = new PooledConnection(pNewsServer);
			pConnection->SetTimeout(m_iTimeout);
			m_Connections.push_back(pConnection);
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

NNTPConnection* ServerPool::GetConnection(int iLevel, bool bWait)
{
	bool bWaitVal = false;
	if (bWait)
	{
		debug("Getting connection (wait)");
		bWaitVal = m_Semaphores[iLevel]->Wait();
	}
	else
	{
		bWaitVal = m_Semaphores[iLevel]->TryWait();
	}

	if (!bWaitVal)
	{
		// signal received or wait timeout
		return NULL;
	}

	m_mutexConnections.Lock();

	PooledConnection* pConnection = NULL;
	for (Connections::iterator it = m_Connections.begin(); it != m_Connections.end(); it++)
	{
		PooledConnection* pConnection1 = *it;
		if (!pConnection1->GetInUse() && pConnection1->GetNewsServer()->GetLevel() == iLevel)
		{
			// free connection found, take it!
			pConnection = pConnection1;
			pConnection->SetInUse(true);
			break;
		}
	}

	m_mutexConnections.Unlock();

	if (!pConnection)
	{
		error("ServerPool: serious error, no free connection found, but there should be one.");
	}

	return pConnection;
}

void ServerPool::FreeConnection(NNTPConnection* pConnection, bool bUsed)
{
	if (bUsed)
	{
		debug("Freeing used connection");
	}

	m_mutexConnections.Lock();

	((PooledConnection*)pConnection)->SetInUse(false);
	if (bUsed)
	{
		((PooledConnection*)pConnection)->SetFreeTimeNow();
	}
	m_Semaphores[pConnection->GetNewsServer()->GetLevel()]->Post();

	m_mutexConnections.Unlock();
}

void ServerPool::CloseUnusedConnections()
{
	m_mutexConnections.Lock();

	time_t curtime = ::time(NULL);

	for (Connections::iterator it = m_Connections.begin(); it != m_Connections.end(); it++)
	{
		PooledConnection* pConnection = *it;
		if (!pConnection->GetInUse() && pConnection->GetStatus() == Connection::csConnected)
		{
			int tdiff = curtime - pConnection->GetFreeTime();
			if (tdiff > CONNECTION_HOLD_SECODNS)
			{
				debug("Closing unused connection to %s", pConnection->GetNewsServer()->GetHost());
				pConnection->Disconnect();
			}
		}
	}

	m_mutexConnections.Unlock();
}

void ServerPool::LogDebugInfo()
{
	debug("   ServerPool");
	debug("   ----------------");

	debug("    Max-Level: %i", m_iMaxLevel);

	m_mutexConnections.Lock();

	debug("    Connections: %i", m_Connections.size());
	for (Connections::iterator it = m_Connections.begin(); it != m_Connections.end(); it++)
	{
		debug("      Connection: Level=%i, InUse:%i", (*it)->GetNewsServer()->GetLevel(), (int)(*it)->GetInUse());
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
	m_mutexConnections.Unlock();
}
