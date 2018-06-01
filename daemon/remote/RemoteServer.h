/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2005 Bo Cordes Petersen <placebodk@users.sourceforge.net>
 *  Copyright (C) 2007-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef REMOTESERVER_H
#define REMOTESERVER_H

#include "Thread.h"
#include "Connection.h"
#include "Observer.h"

class RequestProcessor;

class RemoteServer : public Thread, public Observer
{
public:
	RemoteServer(bool tls) : m_tls(tls) {}
	virtual void Run();
	virtual void Stop();
	void ForceStop();
	void Update(Subject* caller, void* aspect);

private:
	typedef std::deque<RequestProcessor*> RequestProcessors;

	bool m_tls;
	std::unique_ptr<Connection> m_connection;
	RequestProcessors m_activeProcessors;
	Mutex m_processorsMutex;
};

class RequestProcessor : public Thread, public Subject
{
public:
	~RequestProcessor();
	virtual void Run();
	virtual void Stop();
	void SetTls(bool tls) { m_tls = tls; }
	void SetConnection(std::unique_ptr<Connection>&& connection) { m_connection = std::move(connection); }

private:
	bool m_tls;
	std::unique_ptr<Connection> m_connection;

	bool ServWebRequest(const char* signature);
	void Execute();
};

#endif
