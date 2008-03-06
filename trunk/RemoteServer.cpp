/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2005  Bo Cordes Petersen <placebodk@sourceforge.net>
 *  Copyright (C) 2007  Andrei Prygounkov <hugbug@users.sourceforge.net>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include "nzbget.h"
#include "RemoteServer.h"
#include "BinRpc.h"
#include "XmlRpc.h"
#include "Log.h"
#include "Options.h"

extern Options* g_pOptions;

//*****************************************************************
// RemoteServer

RemoteServer::RemoteServer()
{
	debug("Creating RemoteServer");

	m_pNetAddress = new NetAddress(g_pOptions->GetServerIP(), g_pOptions->GetServerPort());
	m_pConnection = NULL;
}

RemoteServer::~RemoteServer()
{
	debug("Destroying RemoteServer");

	if (m_pConnection)
	{
		delete m_pConnection;
	}
	delete m_pNetAddress;
}

void RemoteServer::Run()
{
	debug("Entering RemoteServer-loop");

	while (!IsStopped())
	{
		bool bBind = true;

		if (!m_pConnection)
		{
			m_pConnection = new Connection(m_pNetAddress);
			m_pConnection->SetTimeout(g_pOptions->GetConnectionTimeout());
			m_pConnection->SetSuppressErrors(false);
			bBind = m_pConnection->Bind() == 0;
		}

		// Accept connections and store the "new" socket value
		SOCKET iSocket = INVALID_SOCKET;
		if (bBind)
		{
			iSocket = m_pConnection->Accept();
		}
		if (!bBind || iSocket == INVALID_SOCKET)
		{
			// Remote server could not bind or accept connection, waiting 1/2 sec and try again
			if (IsStopped())
			{
				break; 
			}
			usleep(500 * 1000);
			delete m_pConnection;
			m_pConnection = NULL;
			continue;
		}

		RequestProcessor* commandThread = new RequestProcessor();
		commandThread->SetAutoDestroy(true);
		commandThread->SetSocket(iSocket);
		commandThread->Start();
	}
	if (m_pConnection)
	{
		m_pConnection->Disconnect();
	}

	debug("Exiting RemoteServer-loop");
}

void RemoteServer::Stop()
{
	Thread::Stop();
	if (m_pConnection)
	{
		m_pConnection->SetSuppressErrors(true);
		m_pConnection->Cancel();
#ifdef WIN32
		m_pConnection->Disconnect();
#endif
	}
}

//*****************************************************************
// RequestProcessor

void RequestProcessor::Run()
{
	// Read the first 4 bytes to determine request type
	bool bOK = false;
	int iSignature = 0;
	int iBytesReceived = recv(m_iSocket, (char*)&iSignature, sizeof(iSignature), 0);
	if (iBytesReceived < 0)
	{
		return;
	}

	// Info - connection received
#ifdef WIN32
	char* ip = NULL;
#else
	char ip[20];
#endif
	struct sockaddr_in PeerName;
	int iPeerNameLength = sizeof(PeerName);
	if (getpeername(m_iSocket, (struct sockaddr*)&PeerName, (socklen_t*) &iPeerNameLength) >= 0)
	{
#ifdef WIN32
		ip = inet_ntoa(PeerName.sin_addr);
#else
		inet_ntop(AF_INET, &PeerName.sin_addr, ip, sizeof(ip));
#endif
	}

	if ((int)ntohl(iSignature) == (int)NZBMESSAGE_SIGNATURE)
	{
		// binary request received
		bOK = true;
		BinRpcProcessor processor;
		processor.SetSocket(m_iSocket);
		processor.SetSignature(iSignature);
		processor.SetClientIP(ip);
		processor.Execute();
	}
	else if (!strncmp((char*)&iSignature, "POST", 4) || !strncmp((char*)&iSignature, "GET ", 4))
	{
		// XML-RPC or JSON-RPC request received
		Connection con(m_iSocket, false);
		char szBuffer[1024];
		if (con.ReadLine(szBuffer, sizeof(szBuffer), NULL))
		{
			XmlRpcProcessor::EHttpMethod eHttpMethod = XmlRpcProcessor::hmGet;
			char* szUrl = szBuffer;
			if (!strncmp((char*)&iSignature, "POST", 4))
			{
				eHttpMethod = XmlRpcProcessor::hmPost;
				szUrl++;
			}
			if (char* p = strchr(szUrl, ' '))
			{
				*p = '\0';
			}

			XmlRpcProcessor::ERpcProtocol eProtocol = XmlRpcProcessor::rpUndefined;
			if (!strcmp(szUrl, "/xmlrpc") || !strncmp(szUrl, "/xmlrpc/", 8))
			{
				eProtocol = XmlRpcProcessor::rpXmlRpc;
			}
			else if (!strcmp(szUrl, "/jsonrpc") || !strncmp(szUrl, "/jsonrpc/", 9))
			{
				eProtocol = XmlRpcProcessor::rpJsonRpc;
			}

			if (eProtocol != XmlRpcProcessor::rpUndefined)
			{
				XmlRpcProcessor processor;
				processor.SetConnection(&con);
				processor.SetClientIP(ip);
				processor.SetProtocol(eProtocol);
				processor.SetHttpMethod(eHttpMethod);
				processor.SetUrl(szUrl);
				processor.Execute();
				bOK = true;
			}
		}
	}

	if (!bOK)
	{
		warn("Non-nzbget request received on port %i from %s", g_pOptions->GetServerPort(), ip);
	}

	closesocket(m_iSocket);
}
