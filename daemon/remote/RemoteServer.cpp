/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2005 Bo Cordes Petersen <placebodk@sourceforge.net>
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


#include "nzbget.h"
#include "RemoteServer.h"
#include "BinRpc.h"
#include "WebServer.h"
#include "Log.h"
#include "Options.h"
#include "FileSystem.h"

//*****************************************************************
// RemoteServer

void RemoteServer::Run()
{
	debug("Entering RemoteServer-loop");

#ifndef DISABLE_TLS
	if (m_tls)
	{
		if (strlen(g_Options->GetSecureCert()) == 0 || !FileSystem::FileExists(g_Options->GetSecureCert()))
		{
			error("Could not initialize TLS, secure certificate is not configured or the cert-file was not found. Check option <SecureCert>");
			return;
		}

		if (strlen(g_Options->GetSecureKey()) == 0 || !FileSystem::FileExists(g_Options->GetSecureKey()))
		{
			error("Could not initialize TLS, secure key is not configured or the key-file was not found. Check option <SecureKey>");
			return;
		}
	}
#endif

	while (!IsStopped())
	{
		bool bind = true;

		if (!m_connection)
		{
			m_connection = std::make_unique<Connection>(g_Options->GetControlIp(),
				m_tls ? g_Options->GetSecurePort() : g_Options->GetControlPort(),
				m_tls);
			m_connection->SetTimeout(g_Options->GetRemoteTimeout());
			m_connection->SetSuppressErrors(false);
			bind = m_connection->Bind();
		}

		// Accept connections and store the new Connection
		std::unique_ptr<Connection> acceptedConnection;
		if (bind)
		{
			acceptedConnection = m_connection->Accept();
		}
		if (!bind || !acceptedConnection)
		{
			// Remote server could not bind or accept connection, waiting 1/2 sec and try again
			if (IsStopped())
			{
				break;
			}
			m_connection.reset();
			Util::Sleep(500);
			continue;
		}

		if (!IsStopped())
		{
			RequestProcessor* commandThread = new RequestProcessor();
			commandThread->SetAutoDestroy(true);
			commandThread->SetConnection(std::move(acceptedConnection));
#ifndef DISABLE_TLS
			commandThread->SetTls(m_tls);
#endif
			Guard guard(m_processorsMutex);
			m_activeProcessors.push_back(commandThread);
			commandThread->Attach(this);
			commandThread->Start();
		}
	}

	if (m_connection)
	{
		m_connection->Disconnect();
	}

	// waiting for request processors
	debug("RemoteServer: waiting for request processor to complete");
	bool completed = false;
	while (!completed)
	{
		{
			Guard guard(m_processorsMutex);
			completed = m_activeProcessors.size() == 0;
		}
		Util::Sleep(100);
	}
	debug("RemoteServer: request processor are completed");

	debug("Exiting RemoteServer-loop");
}

void RemoteServer::Stop()
{
	Thread::Stop();
	if (m_connection)
	{
		m_connection->SetSuppressErrors(true);
		m_connection->SetForceClose(true);
		m_connection->Cancel();

		debug("Stopping RequestProcessors");
		Guard guard(m_processorsMutex);
		for (RequestProcessor* requestProcessor : m_activeProcessors)
		{
			requestProcessor->Stop();
		}
		debug("RequestProcessors are notified");
	}
	debug("RemoteServer stop end");
}

void RemoteServer::ForceStop()
{
	debug("Killing RequestProcessors");
	Guard guard(m_processorsMutex);
	for (RequestProcessor* requestProcessor : m_activeProcessors)
	{
		requestProcessor->Kill();
	}
	m_activeProcessors.clear();
	debug("RequestProcessors are killed");
}

void RemoteServer::Update(Subject* caller, void* aspect)
{
	debug("Notification from RequestProcessor received");

	RequestProcessor* requestProcessor = (RequestProcessor*)caller;
	Guard guard(m_processorsMutex);
	m_activeProcessors.erase(std::find(m_activeProcessors.begin(), m_activeProcessors.end(), requestProcessor));
}

//*****************************************************************
// RequestProcessor

RequestProcessor::~RequestProcessor()
{
	m_connection->Disconnect();
}

void RequestProcessor::Run()
{
	Execute();
	Notify(nullptr);
}

void RequestProcessor::Stop()
{
	Thread::Stop();
#ifdef WIN32
	m_connection->SetForceClose(true);
#endif
	m_connection->Cancel();
}

void RequestProcessor::Execute()
{
	bool ok = false;
	m_connection->SetSuppressErrors(true);

#ifndef DISABLE_TLS
	if (m_tls && !m_connection->StartTls(false, g_Options->GetSecureCert(), g_Options->GetSecureKey()))
	{
		debug("Could not establish secure connection to web-client: Start TLS failed");
		return;
	}
#endif

	// Read the first 4 bytes to determine request type
	uint32 signature = 0;
	if (!m_connection->Recv((char*)&signature, 4))
	{
		debug("Could not read request signature");
		return;
	}

	if ((int)ntohl(signature) == (int)NZBMESSAGE_SIGNATURE)
	{
		// binary request received
		ok = true;
		BinRpcProcessor processor;
		processor.SetConnection(m_connection.get());
		processor.Execute();
	}
	else if (!strncmp((char*)&signature, "POST", 4) ||
		!strncmp((char*)&signature, "GET ", 4) ||
		!strncmp((char*)&signature, "OPTI", 4))
	{
		// HTTP request received
		ok = true;
		while (ServWebRequest((char*)&signature))
		{
			if (!m_connection->Recv((char*)&signature, 4))
			{
				debug("Could not read request signature");
				break;
			}
		}

		m_connection->SetGracefull(true);
		m_connection->Disconnect();
	}

	if (!ok)
	{
		warn("Non-nzbget request received on port %i from %s", m_tls ? g_Options->GetSecurePort() : g_Options->GetControlPort(), m_connection->GetRemoteAddr());
	}
}

bool RequestProcessor::ServWebRequest(const char* signature)
{
	// HTTP request received
	char buffer[1024];
	if (!m_connection->ReadLine(buffer, sizeof(buffer), nullptr))
	{
		return false;
	}

	WebProcessor::EHttpMethod httpMethod = WebProcessor::hmGet;
	char* url = buffer;
	if (!strncmp(signature, "POST", 4))
	{
		httpMethod = WebProcessor::hmPost;
		url++;
	}
	else if (!strncmp(signature, "OPTI", 4) && strlen(url) > 4)
	{
		httpMethod = WebProcessor::hmOptions;
		url += 4;
	}
	else if (!(!strncmp(signature, "GET ", 4)))
	{
		return false;
	}

	if (char* p = strchr(url, ' '))
	{
		*p = '\0';
	}

	debug("url: %s", url);

	WebProcessor processor;
	processor.SetConnection(m_connection.get());
	processor.SetUrl(url);
	processor.SetHttpMethod(httpMethod);
	processor.Execute();

	return processor.GetKeepAlive();
}
