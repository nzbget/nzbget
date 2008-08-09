/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2008 Andrei Prygounkov <hugbug@users.sourceforge.net>
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
#include <config.h>
#endif

#ifdef WIN32
// SKIP_DEFAULT_WINDOWS_HEADERS prevents the including of <windows.h>, which includes "winsock.h",
// but we need "winsock2.h" here (they conflicts with each other)
#define SKIP_DEFAULT_WINDOWS_HEADERS
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <cstdio>
#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include "nzbget.h"
#include "Connection.h"
#include "Log.h"
#include "TLS.h"

static const int CONNECTION_READBUFFER_SIZE = 1024;
#ifndef DISABLE_TLS
bool Connection::bTLSLibInitialized = false;
#endif

void Connection::Init(bool bTLS)
{
	debug("Initializing global connection data");

#ifdef WIN32
	WSADATA wsaData;
	int err = WSAStartup(MAKEWORD(2, 0), &wsaData);
	if (err != 0) 
	{
		error("Could not initialize socket library");
		return;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE( wsaData.wVersion ) != 0) 
	{
		error("Could not initialize socket library");
		WSACleanup();
		return; 
	}
#endif
#ifndef DISABLE_TLS
	if (bTLS)
	{
		debug("Initializing TLS library");
		char* szErrStr;
		int iRes = tls_lib_init(&szErrStr);
		bTLSLibInitialized = iRes == TLS_EOK;
		if (!bTLSLibInitialized)
		{
			error("Could not initialize TLS library: %s", szErrStr ? szErrStr : "unknown error");
			if (szErrStr)
			{
				free(szErrStr);
			}
		}
	}
#endif
}

void Connection::Final()
{
	debug("Finalizing global connection data");

#ifdef WIN32
	WSACleanup();
#endif

#ifndef DISABLE_TLS
	if (bTLSLibInitialized)
	{
		debug("Finalizing TLS library");
		tls_lib_deinit();
	}
#endif
}

Connection::Connection(NetAddress* pNetAddress)
{
	debug("Creating Connection");

	m_pNetAddress		= pNetAddress;
	m_eStatus			= csDisconnected;
	m_iSocket			= INVALID_SOCKET;
	m_iBufAvail			= 0;
	m_iTimeout			= 60;
	m_bSuppressErrors	= true;
	m_szReadBuf			= (char*)malloc(CONNECTION_READBUFFER_SIZE + 1);
	m_bAutoClose		= true;
#ifndef DISABLE_TLS
	m_pTLS				= NULL;
#endif
}

Connection::Connection(SOCKET iSocket, bool bAutoClose)
{
	debug("Creating Connection");

	m_pNetAddress		= NULL;
	m_eStatus			= csConnected;
	m_iSocket			= iSocket;
	m_iBufAvail			= 0;
	m_iTimeout			= 60;
	m_bSuppressErrors	= true;
	m_szReadBuf			= (char*)malloc(CONNECTION_READBUFFER_SIZE + 1);
	m_bAutoClose		= bAutoClose;
#ifndef DISABLE_TLS
	m_pTLS				= NULL;
#endif
}

Connection::~Connection()
{
	debug("Destroying Connection");

	if (m_eStatus == csConnected && m_bAutoClose)
	{
		Disconnect();
	}
	free(m_szReadBuf);
#ifndef DISABLE_TLS
	if (m_pTLS)
	{
		free(m_pTLS);
	}
#endif
}

bool Connection::Connect()
{
	debug("Connecting");

	if (m_eStatus == csConnected)
		return true;

	bool bRes = DoConnect();

	if (bRes)
		m_eStatus = csConnected;
	else
		Connection::DoDisconnect();

	return bRes;
}

bool Connection::Disconnect()
{
	debug("Disconnecting");

	if (m_eStatus == csDisconnected)
		return true;

	bool bRes = DoDisconnect();

	m_eStatus = csDisconnected;
	m_iSocket = INVALID_SOCKET;
	m_iBufAvail = 0;

	return bRes;
}

int Connection::Bind()
{
	debug("Binding");

	if (m_eStatus == csListening)
	{
		return 0;
	}

	int iRes = DoBind();

	if (iRes == 0)
	{
		m_eStatus = csListening;
	}

	return iRes;
}

int Connection::WriteLine(const char* pBuffer)
{
	//debug("Connection::write(char* line)");

	if (m_eStatus != csConnected)
	{
		return -1;
	}

	int iRes = DoWriteLine(pBuffer);

	return iRes;
}

int Connection::Send(const char* pBuffer, int iSize)
{
	debug("Sending data");

	if (m_eStatus != csConnected)
	{
		return -1;
	}

	int iRes = send(m_iSocket, pBuffer, iSize, 0);

	return iRes;
}

char* Connection::ReadLine(char* pBuffer, int iSize, int* pBytesRead)
{
	if (m_eStatus != csConnected)
	{
		return NULL;
	}

	char* res = DoReadLine(pBuffer, iSize, pBytesRead);

	return res;
}

SOCKET Connection::Accept()
{
	debug("Accepting connection");

	if (m_eStatus != csListening)
	{
		return INVALID_SOCKET;
	}

	SOCKET iRes = DoAccept();

	return iRes;
}

int Connection::Recv(char* pBuffer, int iSize)
{
	debug("Receiving data");

	memset(pBuffer, 0, iSize);

	int iReceived = recv(m_iSocket, pBuffer, iSize, 0);

	if (iReceived < 0)
	{
		ReportError("Could not receive data on socket", NULL, true, 0);
	}

	return iReceived;
}

bool Connection::RecvAll(char * pBuffer, int iSize)
{
	debug("Receiving data (full buffer)");

	memset(pBuffer, 0, iSize);

	char* pBufPtr = (char*)pBuffer;
	int NeedBytes = iSize;

	if (m_iBufAvail > 0)
	{
		int len = iSize > m_iBufAvail ? m_iBufAvail : iSize;
		memcpy(pBufPtr, m_szBufPtr, len);
		pBufPtr += len;
		m_szBufPtr += len;
		m_iBufAvail -= len;
		NeedBytes -= len;
	}

	// Read from the socket until nothing remains
	while (NeedBytes > 0)
	{
		int iReceived = recv(m_iSocket, pBufPtr, NeedBytes, 0);
		// Did the recv succeed?
		if (iReceived <= 0)
		{
			ReportError("Could not receive data on socket", NULL, true, 0);
			return false;
		}
		pBufPtr += iReceived;
		NeedBytes -= iReceived;
	}
	return true;
}

bool Connection::DoConnect()
{
	debug("Do connecting");

	struct addrinfo addr_hints, *addr_list, *addr;
	char iPortStr[sizeof(int) * 4 + 1]; //is enough to hold any converted int

	memset(&addr_hints, 0, sizeof(addr_hints));
	addr_hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	addr_hints.ai_socktype = SOCK_STREAM,

	sprintf(iPortStr, "%d", m_pNetAddress->GetPort());

	int res = getaddrinfo(m_pNetAddress->GetHost(), iPortStr, &addr_hints, &addr_list);
	if (res != 0)
	{
		ReportError("Could not resolve hostname %s", m_pNetAddress->GetHost(), true, 0);
		return false;
	}

	m_iSocket = INVALID_SOCKET;
	for (addr = addr_list; addr != NULL; addr = addr->ai_next)
	{
		m_iSocket = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (m_iSocket != INVALID_SOCKET)
		{
			res = connect(m_iSocket , addr->ai_addr, addr->ai_addrlen);
			if (res != -1) 
			{
				// Connection established
				break;
			}
			// Connection failed
			closesocket(m_iSocket);
			m_iSocket = INVALID_SOCKET;
		}
	}

	freeaddrinfo(addr_list);

	if (m_iSocket == INVALID_SOCKET)
	{
		ReportError("Connection to %s failed!", m_pNetAddress->GetHost(), true, 0);
		return false;
	} 

#ifdef WIN32
	int MSecVal = m_iTimeout * 1000;
	int err = setsockopt(m_iSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&MSecVal, sizeof(MSecVal));
#else
	struct timeval TimeVal;
	TimeVal.tv_sec = m_iTimeout;
	TimeVal.tv_usec = 0;
	int err = setsockopt(m_iSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&TimeVal, sizeof(TimeVal));
#endif
	if (err != 0)
	{
		ReportError("setsockopt failed", NULL, true, 0);
	}

	return true;
}

bool Connection::DoDisconnect()
{
	debug("Do disconnecting");

	if (m_iSocket > 0)
	{
		closesocket(m_iSocket);
		m_iSocket = INVALID_SOCKET;
#ifndef DISABLE_TLS
		if (m_pTLS)
		{
			CloseTLS();
		}
#endif
	}

	m_eStatus = csDisconnected;
	return true;
}

int Connection::DoWriteLine(const char* pBuffer)
{
	//debug("Connection::doWrite()");
	return send(m_iSocket, pBuffer, strlen(pBuffer), 0);
}

char* Connection::DoReadLine(char* pBuffer, int iSize, int* pBytesRead)
{
	//debug( "Connection::DoReadLine()" );
	char* pBufPtr = pBuffer;
	iSize--; // for trailing '0'
	int iBytesRead = 0;
	int iBufAvail = m_iBufAvail; // local variable is faster
	char* szBufPtr = m_szBufPtr; // local variable is faster
	while (iSize)
	{
		if (!iBufAvail)
		{
			iBufAvail = recv(m_iSocket, m_szReadBuf, CONNECTION_READBUFFER_SIZE, 0);
			if (iBufAvail < 0)
			{
				ReportError("Could not receive data on socket", NULL, true, 0);
				break;
			}
			else if (iBufAvail == 0)
			{
				break;
			}
			szBufPtr = m_szReadBuf;
			m_szReadBuf[iBufAvail] = '\0';
		}

		int len = 0;
		char* p = (char*)memchr(szBufPtr, '\n', iBufAvail);
		if (p)
		{
			len = (int)(p - szBufPtr + 1);
		}
		else
		{
			len = iBufAvail;
		}

		if (len > iSize)
		{
			len = iSize;
		}
		
		memcpy(pBufPtr, szBufPtr, len);
		pBufPtr += len;
		szBufPtr += len;
		iBufAvail -= len;
		iBytesRead += len;
		iSize -= len;
		
		if (p)
		{
			break;
		}
	}
	*pBufPtr = '\0';

	m_iBufAvail = iBufAvail > 0 ? iBufAvail : 0; // copy back to member
	m_szBufPtr = szBufPtr; // copy back to member
	
	if (pBytesRead)
	{
		*pBytesRead = iBytesRead;
	}
	
	if (pBufPtr == pBuffer)
	{
		return NULL;
	}
	return pBuffer;
}

int Connection::DoBind()
{
	debug("Do binding");

	struct addrinfo addr_hints, *addr_list, *addr;
	char iPortStr[sizeof(int) * 4 + 1]; // is enough to hold any converted int

	memset(&addr_hints, 0, sizeof(addr_hints));
	addr_hints.ai_family = AF_UNSPEC;    // Allow IPv4 or IPv6
	addr_hints.ai_socktype = SOCK_STREAM,
	addr_hints.ai_flags = AI_PASSIVE;    // For wildcard IP address

	sprintf(iPortStr, "%d", m_pNetAddress->GetPort());

	int res = getaddrinfo(m_pNetAddress->GetHost(), iPortStr, &addr_hints, &addr_list);
	if (res != 0)
	{
		error( "Could not resolve hostname %s", m_pNetAddress->GetHost() );
		return -1;
	}

	m_iSocket = INVALID_SOCKET;
	for (addr = addr_list; addr != NULL; addr = addr->ai_next)
	{
		m_iSocket = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (m_iSocket != INVALID_SOCKET)
		{
			int opt = 1;
			setsockopt(m_iSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
			res = bind(m_iSocket, addr->ai_addr, addr->ai_addrlen);
			if (res != -1)
			{
				// Connection established
				break;
			}
			// Connection failed
			closesocket(m_iSocket);
			m_iSocket = INVALID_SOCKET;
		}
	}

	freeaddrinfo(addr_list);

	if (m_iSocket == INVALID_SOCKET)
	{
		ReportError("Binding socket failed for %s", m_pNetAddress->GetHost(), true, 0);
		return -1;
	}

	if (listen(m_iSocket, 10) < 0)
	{
		ReportError("Listen on socket failed for %s", m_pNetAddress->GetHost(), true, 0);
		return -1;
	}

	return 0;
}

SOCKET Connection::DoAccept()
{
	SOCKET iSocket = accept(GetSocket(), NULL, NULL);

	if (iSocket == INVALID_SOCKET && m_eStatus != csCancelled)
	{
		ReportError("Could not accept connection", NULL, true, 0);
	}

	return iSocket;
}

void Connection::Cancel()
{
	debug("Cancelling connection");
	if (m_iSocket != INVALID_SOCKET)
	{
		m_eStatus = csCancelled;
		int r = shutdown(m_iSocket, SHUT_RDWR);
		if (r == -1)
		{
			ReportError("Could not shutdown connection", NULL, true, 0);
		}
	}
}

void Connection::ReportError(const char* szMsgPrefix, const char* szMsgArg, bool PrintErrCode, int ErrCode)
{
	char szErrPrefix[1024];
	snprintf(szErrPrefix, 1024, szMsgPrefix, szMsgArg);
	szErrPrefix[1024-1] = '\0';
	
	if (PrintErrCode)
	{
		if (ErrCode == 0)
		{
#ifdef WIN32
			ErrCode = WSAGetLastError();
#else
			ErrCode = errno;
#endif
		}
#ifdef WIN32
		if (m_bSuppressErrors)
		{
			debug("%s: ErrNo %i", szErrPrefix, ErrCode);
		}
		else
		{
			error("%s: ErrNo %i", szErrPrefix, ErrCode);
		}
#else
		const char* szErrMsg = hstrerror(ErrCode);
		if (m_bSuppressErrors)
		{
			debug("%s: ErrNo %i, %s", szErrPrefix, ErrCode, szErrMsg);
		}
		else
		{
			error("%s: ErrNo %i, %s", szErrPrefix, ErrCode, szErrMsg);
		}
#endif
	}
	else
	{
		if (m_bSuppressErrors)
		{
			debug(szErrPrefix);
		}
		else
		{
			error(szErrPrefix);
		}
	}
}

#ifndef DISABLE_TLS
bool Connection::CheckTLSResult(int iResultCode, char* szErrStr, const char* szErrMsgPrefix)
{
	bool bOK = iResultCode == TLS_EOK;
	if (!bOK)
	{
		ReportError(szErrMsgPrefix, szErrStr ? szErrStr : "unknown error", false, 0);
		if (szErrStr)
		{
			free(szErrStr);
		}
	}
	return bOK;
}

bool Connection::StartTLS()
{
	debug("Starting TLS");

	if (m_pTLS)
	{
		free(m_pTLS);
	}

	m_pTLS = malloc(sizeof(tls_t));
	tls_t* pTLS = (tls_t*)m_pTLS;
	memset(pTLS, 0, sizeof(tls_t));
	tls_clear(pTLS);

	char* szErrStr;
	int iRes;
		
	iRes = tls_init(pTLS, NULL, NULL, NULL, 0, &szErrStr);
	if (!CheckTLSResult(iRes, szErrStr, "Could not initialize TLS-object: %s"))
	{
		return false;
	}

	debug("tls_start...");
	iRes = tls_start(pTLS, (int)m_iSocket, NULL, 1, NULL, &szErrStr);
	debug("tls_start...%i", iRes);
	if (!CheckTLSResult(iRes, szErrStr, "Could not establish secure connection: %s"))
	{
		return false;
	}

	return true;
}

void Connection::CloseTLS()
{
	tls_close((tls_t*)m_pTLS);
	free(m_pTLS);
	m_pTLS = NULL;
}

int Connection::recv(SOCKET s, char* buf, int len, int flags)
{
	size_t iReceived = 0;
	
	if (m_pTLS)
	{
		char* szErrStr;
		int iRes = tls_getbuf((tls_t*)m_pTLS, buf, len, &iReceived, &szErrStr);
		if (!CheckTLSResult(iRes, szErrStr, "Could not read from TLS-socket: %s"))
		{
			return -1;
		}
	}
	else
	{
		iReceived = ::recv(s, buf, len, flags);
	}
	return iReceived;
}

int Connection::send(SOCKET s, const char* buf, int len, int flags)
{
	if (m_pTLS)
	{
		char* szErrStr;
		int iRes = tls_putbuf((tls_t*)m_pTLS, buf, len, &szErrStr);
		if (!CheckTLSResult(iRes, szErrStr, "Could not send to TLS-socket: %s"))
		{
			return -1;
		}
		return 0;
	}
	else
	{
		int iRet = ::send(s, buf, len, flags);
		return iRet;
	}
}
#endif
