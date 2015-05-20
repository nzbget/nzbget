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
		bool			m_bInUse;
		time_t			m_tFreeTime;
	public:
						PooledConnection(NewsServer* server);
		bool			GetInUse() { return m_bInUse; }
		void			SetInUse(bool bInUse) { m_bInUse = bInUse; }
		time_t			GetFreeTime() { return m_tFreeTime; }
		void			SetFreeTimeNow() { m_tFreeTime = ::time(NULL); }
	};

	typedef std::vector<int>				Levels;
	typedef std::vector<PooledConnection*>	Connections;

	Servers				m_Servers;
	Servers				m_SortedServers;
	Connections			m_Connections;
	Levels				m_Levels;
	int					m_iMaxNormLevel;
	Mutex			 	m_mutexConnections;
	int					m_iTimeout;
	int					m_iRetryInterval;
	int					m_iGeneration;

	void				NormalizeLevels();
	static bool			CompareServers(NewsServer* pServer1, NewsServer* pServer2);

protected:
	virtual void		LogDebugInfo();

public:
						ServerPool();
						~ServerPool();
	void				SetTimeout(int iTimeout) { m_iTimeout = iTimeout; }
	void				SetRetryInterval(int iRetryInterval) { m_iRetryInterval = iRetryInterval; }
	void 				AddServer(NewsServer* pNewsServer);
	void				InitConnections();
	int					GetMaxNormLevel() { return m_iMaxNormLevel; }
	Servers*			GetServers() { return &m_Servers; } // Only for read access (no lockings)
	NNTPConnection*		GetConnection(int iLevel, NewsServer* pWantServer, Servers* pIgnoreServers);
	void 				FreeConnection(NNTPConnection* pConnection, bool bUsed);
	void				CloseUnusedConnections();
	void				Changed();
	int					GetGeneration() { return m_iGeneration; }
	void				BlockServer(NewsServer* pNewsServer);
};

#endif
