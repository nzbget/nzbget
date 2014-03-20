/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

static const int CONNECTION_HOLD_SECODNS = 5;

ServerPool::PooledConnection::PooledConnection(NewsServer* server) : NNTPConnection(server)
{
	m_bInUse = false;
	m_tFreeTime = 0;
}

ServerPool::ServerPool()
{
	debug("Creating ServerPool");

	m_iMaxNormLevel = 0;
	m_iTimeout = 60;
	m_iGeneration = 0;

	g_pLog->RegisterDebuggable(this);
}

ServerPool::~ ServerPool()
{
	debug("Destroying ServerPool");

	g_pLog->UnregisterDebuggable(this);

	m_Levels.clear();

	for (Servers::iterator it = m_Servers.begin(); it != m_Servers.end(); it++)
	{
		delete *it;
	}
	m_Servers.clear();
	m_SortedServers.clear();

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
	m_SortedServers.push_back(pNewsServer);
}

/*
 * Calculate normalized levels for all servers.
 * Normalized Level means: starting from 0 with step 1.
 * The servers of minimum Level must be always used even if they are not active;
 * this is to prevent backup servers to act as main servers.
**/
void ServerPool::NormalizeLevels()
{
	if (m_Servers.empty())
	{
		return;
	}

	std::sort(m_SortedServers.begin(), m_SortedServers.end(), CompareServers);

	// find minimum level
	int iMinLevel = m_SortedServers.front()->GetLevel();
	for (Servers::iterator it = m_SortedServers.begin(); it != m_SortedServers.end(); it++)
	{
		NewsServer* pNewsServer = *it;
		if (pNewsServer->GetLevel() < iMinLevel)
		{
			iMinLevel = pNewsServer->GetLevel();
		}
	}

	m_iMaxNormLevel = 0;
	int iLastLevel = iMinLevel;
	for (Servers::iterator it = m_SortedServers.begin(); it != m_SortedServers.end(); it++)
	{
		NewsServer* pNewsServer = *it;
		if ((pNewsServer->GetActive() && pNewsServer->GetMaxConnections() > 0) ||
			(pNewsServer->GetLevel() == iMinLevel))
		{
			if (pNewsServer->GetLevel() != iLastLevel)
			{
				m_iMaxNormLevel++;
			}
			pNewsServer->SetNormLevel(m_iMaxNormLevel);
			iLastLevel = pNewsServer->GetLevel();
		}
		else
		{
			pNewsServer->SetNormLevel(-1);
		}
	}
}

bool ServerPool::CompareServers(NewsServer* pServer1, NewsServer* pServer2)
{
	return pServer1->GetLevel() < pServer2->GetLevel();
}

void ServerPool::InitConnections()
{
	debug("Initializing connections in ServerPool");

	m_mutexConnections.Lock();

	NormalizeLevels();
	m_Levels.clear();

	for (Servers::iterator it = m_SortedServers.begin(); it != m_SortedServers.end(); it++)
	{
		NewsServer* pNewsServer = *it;
		int iNormLevel = pNewsServer->GetNormLevel();
		if (pNewsServer->GetNormLevel() > -1)
		{
			if ((int)m_Levels.size() <= iNormLevel)
			{
				m_Levels.push_back(0);
			}

			if (pNewsServer->GetActive())
			{
				int iConnections = 0;
				
				for (Connections::iterator it = m_Connections.begin(); it != m_Connections.end(); it++)
				{
					PooledConnection* pConnection = *it;
					if (pConnection->GetNewsServer() == pNewsServer)
					{
						iConnections++;
					}
				}
				
				for (int i = iConnections; i < pNewsServer->GetMaxConnections(); i++)
				{
					PooledConnection* pConnection = new PooledConnection(pNewsServer);
					pConnection->SetTimeout(m_iTimeout);
					m_Connections.push_back(pConnection);
					iConnections++;
				}

				m_Levels[iNormLevel] += iConnections;
			}
		}
	}

	m_iGeneration++;

	m_mutexConnections.Unlock();
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
			if (!pCandidateConnection->GetInUse() && pCandidateServer->GetActive() &&
				pCandidateServer->GetNormLevel() == iLevel && 
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
							(pIgnoreServer->GetGroup() > 0 && pIgnoreServer->GetGroup() == pCandidateServer->GetGroup() &&
							 pIgnoreServer->GetNormLevel() == pCandidateServer->GetNormLevel()))
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

	if (pConnection->GetNewsServer()->GetNormLevel() > -1 && pConnection->GetNewsServer()->GetActive())
	{
		m_Levels[pConnection->GetNewsServer()->GetNormLevel()]++;
	}

	m_mutexConnections.Unlock();
}

void ServerPool::CloseUnusedConnections()
{
	m_mutexConnections.Lock();

	time_t curtime = ::time(NULL);

	int i = 0;
	for (Connections::iterator it = m_Connections.begin(); it != m_Connections.end(); )
	{
		PooledConnection* pConnection = *it;
		bool bDeleted = false;

		if (!pConnection->GetInUse() &&
			(pConnection->GetNewsServer()->GetNormLevel() == -1 ||
			 !pConnection->GetNewsServer()->GetActive()))
		{
			debug("Closing (and deleting) unused connection to server%i", pConnection->GetNewsServer()->GetID());
			if (pConnection->GetStatus() == Connection::csConnected)
			{
				pConnection->Disconnect();
			}
			delete pConnection;
			m_Connections.erase(it);
			it = m_Connections.begin() + i;
			bDeleted = true;
		}

		if (!bDeleted && !pConnection->GetInUse() && pConnection->GetStatus() == Connection::csConnected)
		{
			int tdiff = (int)(curtime - pConnection->GetFreeTime());
			if (tdiff > CONNECTION_HOLD_SECODNS)
			{
				debug("Closing (and keeping) unused connection to server%i", pConnection->GetNewsServer()->GetID());
				pConnection->Disconnect();
			}
		}

		if (!bDeleted)
		{
			it++;
			i++;
		}
	}

	m_mutexConnections.Unlock();
}

void ServerPool::Changed()
{
	debug("Server config has been changed");

	InitConnections();
	CloseUnusedConnections();
}

void ServerPool::LogDebugInfo()
{
	info("   ---------- ServerPool");

	info("    Max-Level: %i", m_iMaxNormLevel);

	m_mutexConnections.Lock();

	info("    Servers: %i", m_Servers.size());
	for (Servers::iterator it = m_Servers.begin(); it != m_Servers.end(); it++)
	{
		NewsServer*  pNewsServer = *it;
		info("      %i) %s (%s): Level=%i, NormLevel=%i", pNewsServer->GetID(), pNewsServer->GetName(),
			pNewsServer->GetHost(), pNewsServer->GetLevel(), pNewsServer->GetNormLevel());
	}

	info("    Levels: %i", m_Levels.size());
	int index = 0;
	for (Levels::iterator it = m_Levels.begin(); it != m_Levels.end(); it++, index++)
	{
		int  iSize = *it;
		info("      %i: Size=%i", index, iSize);
	}

	info("    Connections: %i", m_Connections.size());
	for (Connections::iterator it = m_Connections.begin(); it != m_Connections.end(); it++)
	{
		PooledConnection*  pConnection = *it;
		info("      %i) %s (%s): Level=%i, NormLevel=%i, InUse:%i", pConnection->GetNewsServer()->GetID(),
			pConnection->GetNewsServer()->GetName(), pConnection->GetNewsServer()->GetHost(),
			pConnection->GetNewsServer()->GetLevel(), pConnection->GetNewsServer()->GetNormLevel(),
			(int)pConnection->GetInUse());
	}

	m_mutexConnections.Unlock();
}
