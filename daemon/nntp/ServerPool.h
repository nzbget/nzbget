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


#ifndef SERVERPOOL_H
#define SERVERPOOL_H

#include <vector>
#include <time.h>

#include "Log.h"
#include "Thread.h"
#include "NewsServer.h"
#include "NNTPConnection.h"

class ServerPool : public Debuggable
{
private:
	class PooledConnection : public NNTPConnection
	{
	private:
		bool			m_inUse;
		time_t			m_freeTime;
	public:
						PooledConnection(NewsServer* server);
		bool			GetInUse() { return m_inUse; }
		void			SetInUse(bool inUse) { m_inUse = inUse; }
		time_t			GetFreeTime() { return m_freeTime; }
		void			SetFreeTimeNow() { m_freeTime = ::time(NULL); }
	};

	typedef std::vector<int>				Levels;
	typedef std::vector<PooledConnection*>	Connections;

	Servers				m_servers;
	Servers				m_sortedServers;
	Connections			m_connections;
	Levels				m_levels;
	int					m_maxNormLevel;
	Mutex			 	m_connectionsMutex;
	int					m_timeout;
	int					m_retryInterval;
	int					m_generation;

	void				NormalizeLevels();
	static bool			CompareServers(NewsServer* server1, NewsServer* server2);

protected:
	virtual void		LogDebugInfo();

public:
						ServerPool();
						~ServerPool();
	void				SetTimeout(int timeout) { m_timeout = timeout; }
	void				SetRetryInterval(int retryInterval) { m_retryInterval = retryInterval; }
	void 				AddServer(NewsServer* newsServer);
	void				InitConnections();
	int					GetMaxNormLevel() { return m_maxNormLevel; }
	Servers*			GetServers() { return &m_servers; } // Only for read access (no lockings)
	NNTPConnection*		GetConnection(int level, NewsServer* wantServer, Servers* ignoreServers);
	void 				FreeConnection(NNTPConnection* connection, bool used);
	void				CloseUnusedConnections();
	void				Changed();
	int					GetGeneration() { return m_generation; }
	void				BlockServer(NewsServer* newsServer);
};

extern ServerPool* g_pServerPool;

#endif
