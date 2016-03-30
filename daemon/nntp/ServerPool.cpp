/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "nzbget.h"
#include "ServerPool.h"
#include "Util.h"

static const int CONNECTION_HOLD_SECODNS = 5;

void ServerPool::PooledConnection::SetFreeTimeNow()
{
	m_freeTime = Util::CurrentTime();
}


void ServerPool::AddServer(std::unique_ptr<NewsServer> newsServer)
{
	debug("Adding server to ServerPool");

	m_sortedServers.push_back(newsServer.get());
	m_servers.push_back(std::move(newsServer));
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

	std::sort(m_sortedServers.begin(), m_sortedServers.end(),
		[](NewsServer* server1, NewsServer* server2)
		{
			return server1->GetLevel() < server2->GetLevel();
		});

	// find minimum level
	int minLevel = m_sortedServers.front()->GetLevel();
	for (NewsServer* newsServer : m_sortedServers)
	{
		if (newsServer->GetLevel() < minLevel)
		{
			minLevel = newsServer->GetLevel();
		}
	}

	m_maxNormLevel = 0;
	int lastLevel = minLevel;
	for (NewsServer* newsServer : m_sortedServers)
	{
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

void ServerPool::InitConnections()
{
	debug("Initializing connections in ServerPool");

	Guard guard(m_connectionsMutex);

	NormalizeLevels();
	m_levels.clear();

	for (NewsServer* newsServer : m_sortedServers)
	{
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

				for (PooledConnection* connection : &m_connections)
				{
					if (connection->GetNewsServer() == newsServer)
					{
						connections++;
					}
				}

				for (int i = connections; i < newsServer->GetMaxConnections(); i++)
				{
					std::unique_ptr<PooledConnection> connection = std::make_unique<PooledConnection>(newsServer);
					connection->SetTimeout(m_timeout);
					m_connections.push_back(std::move(connection));
					connections++;
				}

				m_levels[normLevel] += connections;
			}
		}
	}

	m_generation++;
}

/* Returns connection from any server on a given level or nullptr if there is no free connection at the moment.
 * If all servers are blocked and all are optional a connection from the next level is returned instead.
 */
NntpConnection* ServerPool::GetConnection(int level, NewsServer* wantServer, RawServerList* ignoreServers)
{
	Guard guard(m_connectionsMutex);

	for (; level < (int)m_levels.size() && m_levels[level] > 0; level++)
	{
		NntpConnection* connection = LockedGetConnection(level, wantServer, ignoreServers);
		if (connection)
		{
			return connection;
		}

		for (NewsServer* newsServer : m_sortedServers)
		{
			if (newsServer->GetNormLevel() == level && newsServer->GetActive() &&
				!(newsServer->GetOptional() && IsServerBlocked(newsServer)))
			{
				return nullptr;
			}
		}
	}

	return nullptr;
}

NntpConnection* ServerPool::LockedGetConnection(int level, NewsServer* wantServer, RawServerList* ignoreServers)
{
	if (level >= (int)m_levels.size() || m_levels[level] == 0)
	{
		return nullptr;
	}

	PooledConnection* connection = nullptr;
	std::vector<PooledConnection*> candidates;
	candidates.reserve(m_connections.size());

	for (PooledConnection* candidateConnection : &m_connections)
	{
		NewsServer* candidateServer = candidateConnection->GetNewsServer();
		if (!candidateConnection->GetInUse() && candidateServer->GetActive() &&
			candidateServer->GetNormLevel() == level &&
			(!wantServer || candidateServer == wantServer ||
			 (wantServer->GetGroup() > 0 && wantServer->GetGroup() == candidateServer->GetGroup())) &&
			(candidateConnection->GetStatus() == Connection::csConnected ||
			 !IsServerBlocked(candidateServer)))
		{
			// free connection found, check if it's not from the server which should be ignored
			bool useConnection = true;
			if (ignoreServers && !wantServer)
			{
				for (NewsServer* ignoreServer : ignoreServers)
				{
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

	return connection;
}

void ServerPool::FreeConnection(NntpConnection* connection, bool used)
{
	if (used)
	{
		debug("Freeing used connection");
	}

	Guard guard(m_connectionsMutex);

	((PooledConnection*)connection)->SetInUse(false);
	if (used)
	{
		((PooledConnection*)connection)->SetFreeTimeNow();
	}

	if (connection->GetNewsServer()->GetNormLevel() > -1 && connection->GetNewsServer()->GetActive())
	{
		m_levels[connection->GetNewsServer()->GetNormLevel()]++;
	}
}

void ServerPool::BlockServer(NewsServer* newsServer)
{
	bool newBlock = false;
	{
		Guard guard(m_connectionsMutex);
		time_t curTime = Util::CurrentTime();
		newBlock = newsServer->GetBlockTime() != curTime;
		newsServer->SetBlockTime(curTime);
	}

	if (newBlock && m_retryInterval > 0)
	{
		warn("Blocking %s (%s) for %i sec", newsServer->GetName(), newsServer->GetHost(), m_retryInterval);
	}
}

bool ServerPool::IsServerBlocked(NewsServer* newsServer)
{
	if (!newsServer->GetBlockTime())
	{
		return false;
	}

	time_t curTime = Util::CurrentTime();
	bool blocked = newsServer->GetBlockTime() <= curTime &&
		curTime < newsServer->GetBlockTime() + m_retryInterval;
	return blocked;
}

void ServerPool::CloseUnusedConnections()
{
	Guard guard(m_connectionsMutex);

	time_t curtime = Util::CurrentTime();

	// close and free all connections of servers which were disabled since the last check
	m_connections.erase(std::remove_if(m_connections.begin(), m_connections.end(),
		[](std::unique_ptr<PooledConnection>& connection)
		{
			if (!connection->GetInUse() &&
				(connection->GetNewsServer()->GetNormLevel() == -1 ||
				 !connection->GetNewsServer()->GetActive()))
			{
				debug("Closing (and deleting) unused connection to server%i", connection->GetNewsServer()->GetId());
				if (connection->GetStatus() == Connection::csConnected)
				{
					connection->Disconnect();
				}
				return true;
			}
			return false;
		}),
		m_connections.end());

	// close all opened connections on levels not having any in-use connections
	for (int level = 0; level <= m_maxNormLevel; level++)
	{
		// check if we have in-use connections on the level
		bool hasInUseConnections = false;
		int inactiveTime = 0;
		for (PooledConnection* connection : &m_connections)
		{
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
			for (PooledConnection* connection : &m_connections)
			{
				if (connection->GetNewsServer()->GetNormLevel() == level &&
					connection->GetStatus() == Connection::csConnected)
				{
					debug("Closing (and keeping) unused connection to server%i", connection->GetNewsServer()->GetId());
					connection->Disconnect();
				}
			}
		}
	}
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

	Guard guard(m_connectionsMutex);

	time_t curTime = Util::CurrentTime();

	info("    Servers: %i", (int)m_servers.size());
	for (NewsServer* newsServer : &m_servers)
	{
		info("      %i) %s (%s): Level=%i, NormLevel=%i, BlockSec=%i", newsServer->GetId(), newsServer->GetName(),
			newsServer->GetHost(), newsServer->GetLevel(), newsServer->GetNormLevel(),
			newsServer->GetBlockTime() && newsServer->GetBlockTime() + m_retryInterval > curTime ?
				(int)(newsServer->GetBlockTime() + m_retryInterval - curTime) : 0);
	}

	info("    Levels: %i", (int)m_levels.size());
	int index = 0;
	for (int size : m_levels)
	{
		info("      %i: Free connections=%i", index, size);
		index++;
	}

	info("    Connections: %i", (int)m_connections.size());
	for (PooledConnection* connection : &m_connections)
	{
		info("      %i) %s (%s): Level=%i, NormLevel=%i, InUse:%i", connection->GetNewsServer()->GetId(),
			connection->GetNewsServer()->GetName(), connection->GetNewsServer()->GetHost(),
			connection->GetNewsServer()->GetLevel(), connection->GetNewsServer()->GetNormLevel(),
			(int)connection->GetInUse());
	}
}
