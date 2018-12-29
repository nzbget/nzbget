/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2016-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef NNTPSERVER_H
#define NNTPSERVER_H

#include "Thread.h"
#include "Connection.h"
#include "Util.h"

class NntpCache
{
public:
	void Append(const char* key, const char* data, int len = 0);
	bool Find(const char* key, const char*& data, int& size);

private:
	class CacheItem
	{
	public:
		CacheItem(const char* key, const char* data, int size) :
			m_key(key), m_data(data), m_size(size) {}

		CString m_key;
		CString m_data;
		int m_size = 0;
	};

	typedef std::unordered_map<std::string, std::unique_ptr<CacheItem>> CacheMap;

	CacheMap m_items;
	Mutex m_lock;
};

class NntpServer : public Thread
{
public:
	NntpServer(int id, const char* host, int port, const char* secureCert,
		const char* secureKey, const char* dataDir, const char* cacheDir,
		int latency, int speed, NntpCache* cache) :
		m_id(id), m_host(host), m_port(port), m_secureCert(secureCert),
		m_secureKey(secureKey), m_dataDir(dataDir), m_cacheDir(cacheDir),
		m_latency(latency), m_speed(speed), m_cache(cache) {}
	virtual void Run();
	virtual void Stop();

private:
	int m_id;
	CString m_host;
	int m_port;
	CString m_secureCert;
	CString m_secureKey;
	std::unique_ptr<Connection> m_connection;
	CString m_dataDir;
	CString m_cacheDir;
	int m_latency;
	int m_speed;
	NntpCache* m_cache;
};

#endif
