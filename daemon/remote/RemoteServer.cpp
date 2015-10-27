/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2005 Bo Cordes Petersen <placebodk@sourceforge.net>
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
#include <stdio.h>
#ifndef WIN32
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include "nzbget.h"
#include "RemoteServer.h"
#include "BinRpc.h"
#include "WebServer.h"
#include "Log.h"
#include "Options.h"
#include "Util.h"

//*****************************************************************
// RemoteServer

RemoteServer::RemoteServer(bool tLS)
{
	debug("Creating RemoteServer");

	m_tLS = tLS;
	m_connection = NULL;
}

RemoteServer::~RemoteServer()
{
	debug("Destroying RemoteServer");

	delete m_connection;
}

void RemoteServer::Run()
{
	debug("Entering RemoteServer-loop");

#ifndef DISABLE_TLS
	if (m_tLS)
	{
		if (strlen(g_pOptions->GetSecureCert()) == 0 || !Util::FileExists(g_pOptions->GetSecureCert()))
		{
			error("Could not initialize TLS, secure certificate is not configured or the cert-file was not found. Check option <SecureCert>");
			return;
		}

		if (strlen(g_pOptions->GetSecureKey()) == 0 || !Util::FileExists(g_pOptions->GetSecureKey()))
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
			m_connection = new Connection(g_pOptions->GetControlIP(), 
				m_tLS ? g_pOptions->GetSecurePort() : g_pOptions->GetControlPort(),
				m_tLS);
			m_connection->SetTimeout(g_pOptions->GetUrlTimeout());
			m_connection->SetSuppressErrors(false);
			bind = m_connection->Bind();
		}

		// Accept connections and store the new Connection
		Connection* acceptedConnection = NULL;
		if (bind)
		{
			acceptedConnection = m_connection->Accept();
		}
		if (!bind || acceptedConnection == NULL)
		{
			// Remote server could not bind or accept connection, waiting 1/2 sec and try again
			if (IsStopped())
			{
				break; 
			}
			usleep(500 * 1000);
			delete m_connection;
			m_connection = NULL;
			continue;
		}

		RequestProcessor* commandThread = new RequestProcessor();
		commandThread->SetAutoDestroy(true);
		commandThread->SetConnection(acceptedConnection);
#ifndef DISABLE_TLS
		commandThread->SetTLS(m_tLS);
#endif
		commandThread->Start();
	}

	if (m_connection)
	{
		m_connection->Disconnect();
	}

	debug("Exiting RemoteServer-loop");
}

void RemoteServer::Stop()
{
	Thread::Stop();
	if (m_connection)
	{
		m_connection->SetSuppressErrors(true);
		m_connection->Cancel();
#ifdef WIN32
		m_connection->Disconnect();
#endif
	}
}

//*****************************************************************
// RequestProcessor

RequestProcessor::~RequestProcessor()
{
	m_connection->Disconnect();
	delete m_connection;
}

void RequestProcessor::Run()
{
	bool ok = false;

	m_connection->SetSuppressErrors(true);

#ifndef DISABLE_TLS
	if (m_tLS && !m_connection->StartTLS(false, g_pOptions->GetSecureCert(), g_pOptions->GetSecureKey()))
	{
		debug("Could not establish secure connection to web-client: Start TLS failed");
		return;
	}
#endif

	// Read the first 4 bytes to determine request type
	int signature = 0;
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
		processor.SetConnection(m_connection);
		processor.Execute();
	}
	else if (!strncmp((char*)&signature, "POST", 4) || 
		!strncmp((char*)&signature, "GET ", 4) ||
		!strncmp((char*)&signature, "OPTI", 4))
	{
		// HTTP request received
		char buffer[1024];
		if (m_connection->ReadLine(buffer, sizeof(buffer), NULL))
		{
			WebProcessor::EHttpMethod httpMethod = WebProcessor::hmGet;
			char* url = buffer;
			if (!strncmp((char*)&signature, "POST", 4))
			{
				httpMethod = WebProcessor::hmPost;
				url++;
			}
			if (!strncmp((char*)&signature, "OPTI", 4) && strlen(url) > 4)
			{
				httpMethod = WebProcessor::hmOptions;
				url += 4;
			}
			if (char* p = strchr(url, ' '))
			{
				*p = '\0';
			}

			debug("url: %s", url);

			WebProcessor processor;
			processor.SetConnection(m_connection);
			processor.SetUrl(url);
			processor.SetHttpMethod(httpMethod);
			processor.Execute();

			m_connection->SetGracefull(true);
			m_connection->Disconnect();

			ok = true;
		}
	}

	if (!ok)
	{
		warn("Non-nzbget request received on port %i from %s", m_tLS ? g_pOptions->GetSecurePort() : g_pOptions->GetControlPort(), m_connection->GetRemoteAddr());
	}
}
