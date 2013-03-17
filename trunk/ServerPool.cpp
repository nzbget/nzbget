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
#include "config.h"
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
#include <algorithm>

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

	if (pNewsServer->GetMaxConnections() > 0)
	{
		m_Servers.push_back(pNewsServer);
	}
	else
	{
		delete pNewsServer;
	}
}

void ServerPool::NormalizeLevels()
{
	if (m_Servers.empty())
	{
		return;
	}

	std::sort(m_Servers.begin(), m_Servers.end(), CompareServers);

	NewsServer* pNewsServer = m_Servers.front();

	m_iMaxLevel = 0;
	int iCurLevel = pNewsServer->GetLevel();
	for (Servers::iterator it = m_Servers.begin(); it != m_Servers.end(); it++)
	{
		NewsServer* pNewsServer = *it;
		if (pNewsServer->GetLevel() != iCurLevel)
		{
			m_iMaxLevel++;
		}

		pNewsServer->SetLevel(m_iMaxLevel);
	}
}

bool ServerPool::CompareServers(NewsServer* pServer1, NewsServer* pServer2)
{
	return pServer1->GetLevel() < pServer2->GetLevel();
}

void ServerPool::InitConnections()
{
	debug("Initializing connections in ServerPool");

	NormalizeLevels();

	for (Servers::iterator it = m_Servers.begin(); it != m_Servers.end(); it++)
	{
		NewsServer* pNewsServer = *it;
		for (int i = 0; i < pNewsServer->GetMaxConnections(); i++)
		{
			PooledConnection* pConnection = new PooledConnection(pNewsServer);
			pConnection->SetTimeout(m_iTimeout);
			m_Connections.push_back(pConnection);
		}
		if ((int)m_Levels.size() <= pNewsServer->GetLevel())
		{
			m_Levels.push_back(0);
		}
		m_Levels[pNewsServer->GetLevel()] += pNewsServer->GetMaxConnections();
	}

	if (m_Levels.empty())
	{
		warn("No news servers defined, download is not possible");
	}
}

NNTPConnection* ServerPool::GetConnection(int iLevel, NewsServer* pWantServer, Servers* pIgnoreServers)
{
	PooledConnection* pConnection = NULL;

	m_mutexConnections.Lock();

	if (iLevel < (int)m_Levels.size() && m_Levels[iLevel] > 0)
	{
		for (Connections::iterator it = m_Connections.begin(); it != m_Connections.end(); it++)
		{
			PooledConnection* pCandidateConnection = *it;
			NewsServer* pCandidateServer = pCandidateConnection->GetNewsServer();
			if (!pCandidateConnection->GetInUse() && pCandidateServer->GetLevel() == iLevel && 
				(!pWantServer || pCandidateServer == pWantServer ||
				 (pWantServer->GetGroup() > 0 && pWantServer->GetGroup() == pCandidateServer->GetGroup())))
			{
				// free connection found, check if it's not from the server which should be ignored
				bool bUseConnection = true;
				if (pIgnoreServers && !pWantServer)
				{
					for (Servers::iterator it = pIgnoreServers->begin(); it != pIgnoreServers->end(); it++)
					{
						NewsServer* pIgnoreServer = *it;
						if (pIgnoreServer == pCandidateServer ||
							(pIgnoreServer->GetGroup() > 0 && pIgnoreServer->GetGroup() == pCandidateServer->GetGroup()))
						{
							bUseConnection = false;
							break;
						}
					}
				}

				if (bUseConnection)
				{
					pConnection = pCandidateConnection;
					pConnection->SetInUse(true);
					break;
				}
			}
		}

		if (pConnection)
		{
			m_Levels[iLevel]--;
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
		debug("      %s: Level=%i, InUse:%i", (*it)->GetNewsServer()->GetHost(), (*it)->GetNewsServer()->GetLevel(), (int)(*it)->GetInUse());
	}

	m_mutexConnections.Unlock();
}
