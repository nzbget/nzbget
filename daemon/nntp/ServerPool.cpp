/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

ServerPool::PooledConnection::PooledConnection(NewsServer* server) : NntpConnection(server)
{
	m_inUse = false;
	m_freeTime = 0;
}

ServerPool::ServerPool()
{
	debug("Creating ServerPool");

	m_maxNormLevel = 0;
	m_timeout = 60;
	m_generation = 0;
	m_retryInterval = 0;

	g_pLog->RegisterDebuggable(this);
}

ServerPool::~ ServerPool()
{
	debug("Destroying ServerPool");

	g_pLog->UnregisterDebuggable(this);

	m_levels.clear();

	for (Servers::iterator it = m_servers.begin(); it != m_servers.end(); it++)
	{
		delete *it;
	}
	m_servers.clear();
	m_sortedServers.clear();

	for (Connections::iterator it = m_connections.begin(); it != m_connections.end(); it++)
	{
		delete *it;
	}
	m_connections.clear();
}

void ServerPool::AddServer(NewsServer* newsServer)
{
	debug("Adding server to ServerPool");

	m_servers.push_back(newsServer);
	m_sortedServers.push_back(newsServer);
}

/*
 * Calculate normalized levels for all servers.
 * Normalized Level means: starting from 0 with step 1.
 * The servers of minimum Level must be always used even if they are not active;
 * this is to prevent backup servers to act as main servers.
**/
void ServerPool::NormalizeLevels()
{
	if (m_servers.empty())
	{
		return;
	}

	std::sort(m_sortedServers.begin(), m_sortedServers.end(), CompareServers);

	// find minimum level
	int minLevel = m_sortedServers.front()->GetLevel();
	for (Servers::iterator it = m_sortedServers.begin(); it != m_sortedServers.end(); it++)
	{
		NewsServer* newsServer = *it;
		if (newsServer->GetLevel() < minLevel)
		{
			minLevel = newsServer->GetLevel();
		}
	}

	m_maxNormLevel = 0;
	int lastLevel = minLevel;
	for (Servers::iterator it = m_sortedServers.begin(); it != m_sortedServers.end(); it++)
	{
		NewsServer* newsServer = *it;
		if ((newsServer->GetActive() && newsServer->GetMaxConnections() > 0) ||
			(newsServer->GetLevel() == minLevel))
		{
			if (newsServer->GetLevel() != lastLevel)
			{
				m_maxNormLevel++;
			}
			newsServer->SetNormLevel(m_maxNormLevel);
			lastLevel = newsServer->GetLevel();
		}
		else
		{
			newsServer->SetNormLevel(-1);
		}
	}
}

bool ServerPool::CompareServers(NewsServer* server1, NewsServer* server2)
{
	return server1->GetLevel() < server2->GetLevel();
}

void ServerPool::InitConnections()
{
	debug("Initializing connections in ServerPool");

	m_connectionsMutex.Lock();

	NormalizeLevels();
	m_levels.clear();

	for (Servers::iterator it = m_sortedServers.begin(); it != m_sortedServers.end(); it++)
	{
		NewsServer* newsServer = *it;
		newsServer->SetBlockTime(0);
		int normLevel = newsServer->GetNormLevel();
		if (newsServer->GetNormLevel() > -1)
		{
			if ((int)m_levels.size() <= normLevel)
			{
				m_levels.push_back(0);
			}

			if (newsServer->GetActive())
			{
				int connections = 0;
				
				for (Connections::iterator it = m_connections.begin(); it != m_connections.end(); it++)
				{
					PooledConnection* connection = *it;
					if (connection->GetNewsServer() == newsServer)
					{
						connections++;
					}
				}
				
				for (int i = connections; i < newsServer->GetMaxConnections(); i++)
				{
					PooledConnection* connection = new PooledConnection(newsServer);
					connection->SetTimeout(m_timeout);
					m_connections.push_back(connection);
					connections++;
				}

				m_levels[normLevel] += connections;
			}
		}
	}

	m_generation++;

	m_connectionsMutex.Unlock();
}

NntpConnection* ServerPool::GetConnection(int level, NewsServer* wantServer, Servers* ignoreServers)
{
	PooledConnection* connection = NULL;
	m_connectionsMutex.Lock();

	time_t curTime = time(NULL);

	if (level < (int)m_levels.size() && m_levels[level] > 0)
	{
		Connections candidates;
		candidates.reserve(m_connections.size());

		for (Connections::iterator it = m_connections.begin(); it != m_connections.end(); it++)
		{
			PooledConnection* candidateConnection = *it;
			NewsServer* candidateServer = candidateConnection->GetNewsServer();
			if (!candidateConnection->GetInUse() && candidateServer->GetActive() &&
				candidateServer->GetNormLevel() == level && 
				(!wantServer || candidateServer == wantServer ||
				 (wantServer->GetGroup() > 0 && wantServer->GetGroup() == candidateServer->GetGroup())) &&
				(candidateConnection->GetStatus() == Connection::csConnected ||
				 !candidateServer->GetBlockTime() ||
				 candidateServer->GetBlockTime() + m_retryInterval <= curTime ||
				 candidateServer->GetBlockTime() > curTime))
			{
				// free connection found, check if it's not from the server which should be ignored
				bool useConnection = true;
				if (ignoreServers && !wantServer)
				{
					for (Servers::iterator it = ignoreServers->begin(); it != ignoreServers->end(); it++)
					{
						NewsServer* ignoreServer = *it;
						if (ignoreServer == candidateServer ||
							(ignoreServer->GetGroup() > 0 && ignoreServer->GetGroup() == candidateServer->GetGroup() &&
							 ignoreServer->GetNormLevel() == candidateServer->GetNormLevel()))
						{
							useConnection = false;
							break;
						}
					}
				}

				candidateServer->SetBlockTime(0);

				if (useConnection)
				{
					candidates.push_back(candidateConnection);
				}
			}
		}

		if (!candidates.empty())
		{
			// Peeking a random free connection. This is better than taking the first
			// available connection because provides better distribution across news servers,
			// especially when one of servers becomes unavailable or doesn't have requested articles.
			int randomIndex = rand() % candidates.size();
			connection = candidates[randomIndex];
			connection->SetInUse(true);
		}

		if (connection)
		{
			m_levels[level]--;
		}
	}

	m_connectionsMutex.Unlock();

	return connection;
}

void ServerPool::FreeConnection(NntpConnection* connection, bool used)
{
	if (used)
	{
		debug("Freeing used connection");
	}

	m_connectionsMutex.Lock();

	((PooledConnection*)connection)->SetInUse(false);
	if (used)
	{
		((PooledConnection*)connection)->SetFreeTimeNow();
	}

	if (connection->GetNewsServer()->GetNormLevel() > -1 && connection->GetNewsServer()->GetActive())
	{
		m_levels[connection->GetNewsServer()->GetNormLevel()]++;
	}

	m_connectionsMutex.Unlock();
}

void ServerPool::BlockServer(NewsServer* newsServer)
{
	m_connectionsMutex.Lock();
	time_t curTime = time(NULL);
	bool newBlock = newsServer->GetBlockTime() != curTime;
	newsServer->SetBlockTime(curTime);
	m_connectionsMutex.Unlock();

	if (newBlock && m_retryInterval > 0)
	{
		warn("Blocking %s (%s) for %i sec", newsServer->GetName(), newsServer->GetHost(), m_retryInterval);
	}
}

void ServerPool::CloseUnusedConnections()
{
	m_connectionsMutex.Lock();

	time_t curtime = ::time(NULL);

	// close and free all connections of servers which were disabled since the last check
	int i = 0;
	for (Connections::iterator it = m_connections.begin(); it != m_connections.end(); )
	{
		PooledConnection* connection = *it;
		bool deleted = false;

		if (!connection->GetInUse() &&
			(connection->GetNewsServer()->GetNormLevel() == -1 ||
			 !connection->GetNewsServer()->GetActive()))
		{
			debug("Closing (and deleting) unused connection to server%i", connection->GetNewsServer()->GetId());
			if (connection->GetStatus() == Connection::csConnected)
			{
				connection->Disconnect();
			}
			delete connection;
			m_connections.erase(it);
			it = m_connections.begin() + i;
			deleted = true;
		}

		if (!deleted)
		{
			it++;
			i++;
		}
	}

	// close all opened connections on levels not having any in-use connections
	for (int level = 0; level <= m_maxNormLevel; level++)
	{
		// check if we have in-use connections on the level
		bool hasInUseConnections = false;
		int inactiveTime = 0;
		for (Connections::iterator it = m_connections.begin(); it != m_connections.end(); it++)
		{
			PooledConnection* connection = *it;
			if (connection->GetNewsServer()->GetNormLevel() == level)
			{
				if (connection->GetInUse())
				{
					hasInUseConnections = true;
					break;
				}
				else
				{
					int tdiff = (int)(curtime - connection->GetFreeTime());
					if (tdiff > inactiveTime)
					{
						inactiveTime = tdiff;
					}
				}
			}
		}

		// if there are no in-use connections on the level and the hold time out has
		// expired - close all connections of the level.
		if (!hasInUseConnections && inactiveTime > CONNECTION_HOLD_SECODNS)
		{
			for (Connections::iterator it = m_connections.begin(); it != m_connections.end(); it++)
			{
				PooledConnection* connection = *it;
				if (connection->GetNewsServer()->GetNormLevel() == level &&
					connection->GetStatus() == Connection::csConnected)
				{
					debug("Closing (and keeping) unused connection to server%i", connection->GetNewsServer()->GetId());
					connection->Disconnect();
				}
			}
		}
	}

	m_connectionsMutex.Unlock();
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

	info("    Max-Level: %i", m_maxNormLevel);

	m_connectionsMutex.Lock();

	time_t curTime = time(NULL);

	info("    Servers: %i", m_servers.size());
	for (Servers::iterator it = m_servers.begin(); it != m_servers.end(); it++)
	{
		NewsServer*  newsServer = *it;
		info("      %i) %s (%s): Level=%i, NormLevel=%i, BlockSec=%i", newsServer->GetId(), newsServer->GetName(),
			newsServer->GetHost(), newsServer->GetLevel(), newsServer->GetNormLevel(),
			newsServer->GetBlockTime() && newsServer->GetBlockTime() + m_retryInterval > curTime ?
				newsServer->GetBlockTime() + m_retryInterval - curTime : 0);
	}

	info("    Levels: %i", m_levels.size());
	int index = 0;
	for (Levels::iterator it = m_levels.begin(); it != m_levels.end(); it++, index++)
	{
		int  size = *it;
		info("      %i: Free connections=%i", index, size);
	}

	info("    Connections: %i", m_connections.size());
	for (Connections::iterator it = m_connections.begin(); it != m_connections.end(); it++)
	{
		PooledConnection*  connection = *it;
		info("      %i) %s (%s): Level=%i, NormLevel=%i, InUse:%i", connection->GetNewsServer()->GetId(),
			connection->GetNewsServer()->GetName(), connection->GetNewsServer()->GetHost(),
			connection->GetNewsServer()->GetLevel(), connection->GetNewsServer()->GetNormLevel(),
			(int)connection->GetInUse());
	}

	m_connectionsMutex.Unlock();
}
