/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2009 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
}

ServerPool::~ ServerPool()
{
	debug("Destroying ServerPool");

	m_Levels.clear();

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

		m_Levels.push_back(iMaxConnectionsForLevel);
	}
}

NNTPConnection* ServerPool::GetConnection(int iLevel)
{
	PooledConnection* pConnection = NULL;

	m_mutexConnections.Lock();

	if (m_Levels[iLevel] > 0)
	{
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

		m_Levels[iLevel]--;

		if (!pConnection)
		{
			error("ServerPool: internal error, no free connection found, but there should be one");
		}
	}

	m_mutexConnections.Unlock();

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
	m_Levels[pConnection->GetNewsServer()->GetLevel()]++;

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
			int tdiff = (int)(curtime - pConnection->GetFreeTime());
			if (tdiff > CONNECTION_HOLD_SECODNS)
			{
				debug("Closing unused connection to %s", pConnection->GetHost());
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

	m_mutexConnections.Unlock();
}
