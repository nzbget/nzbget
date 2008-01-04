/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2004  Sven Henkel <sidddy@users.sourceforge.net>
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
#include <config.h>
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
#ifndef WIN32
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

void Connection::Init()
{
	debug("Intiializing global connection data");

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
}

void Connection::Final()
{
	debug("Finalizing global connection data");

#ifdef WIN32
	WSACleanup();
#endif
}


Connection::Connection(NetAddress* pNetAddress)
{
	debug("Creating Connection");

	m_pNetAddress		= pNetAddress;
	m_eStatus			= csDisconnected;
	m_iSocket			= INVALID_SOCKET;
	m_iBufAvail			= 0;
	m_bCanceling		= false;
	m_iTimeout			= 60;
}

Connection::~Connection()
{
	debug("Destroying Connection");

	if (m_eStatus == csConnected)
	{
		Disconnect();
	}
}

int Connection::Connect()
{
	debug("Connecting");

	if (m_eStatus == csConnected)
		return 0;

	int iRes = DoConnect();

	if (iRes >= 0)
		m_eStatus = csConnected;
	else
		Connection::DoDisconnect();

	return iRes;
}

int Connection::Disconnect()
{
	debug("Disconnecting");

	if (m_eStatus == csDisconnected)
		return 0;

	int iRes = DoDisconnect();

	m_eStatus = csDisconnected;

	return iRes;
}

int Connection::Bind()
{
	debug("Binding");

	if (m_eStatus == csListening)
	{
		return 0;
	}

	int iRes = DoBind();

	m_eStatus = csListening;

	return iRes;
}

int Connection::WriteLine(char* line)
{
	//debug("Connection::write(char* line)");

	if (m_eStatus != csConnected)
	{
		return -1;
	}

	int iRes = DoWriteLine(line);

	if (iRes == EOF)
		Connection::DoDisconnect();

	return iRes;
}

int Connection::Send(char* pBuffer, int iSize)
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

	if (res == NULL)
		Connection::DoDisconnect();

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
		ReportError("Could not receive data on socket", NULL, 0);
	}

	return iReceived;
}

bool Connection::RecvAll(char * pBuffer, int iSize)
{
	debug("Receiving data (full buffer)");

	memset(pBuffer, 0, iSize);

	char* pBufPtr = (char*)pBuffer;
	int NeedBytes = iSize;
	// Read from the socket until nothing remains
	while (NeedBytes > 0)
	{
		int iReceived = recv(m_iSocket, pBufPtr, NeedBytes, 0);
		// Did the recv succeed?
		if (iReceived <= 0)
		{
			ReportError("Could not receive data on socket", NULL, 0);
			return false;
		}
		pBufPtr += iReceived;
		NeedBytes -= iReceived;
	}
	return true;
}

int Connection::DoConnect()
{
	debug("Do connecting");

	struct sockaddr_in	sSocketAddress;
	memset(&sSocketAddress, '\0', sizeof(sSocketAddress));
	sSocketAddress.sin_family = AF_INET;
	sSocketAddress.sin_port = htons(m_pNetAddress->GetPort());
	sSocketAddress.sin_addr.s_addr = ResolveHostAddr(m_pNetAddress->GetHost());
	if (sSocketAddress.sin_addr.s_addr == (unsigned int)-1)
	{
		return -1;
	}

	m_iSocket = socket(PF_INET, SOCK_STREAM, 0);

	if (m_iSocket == INVALID_SOCKET)
	{
		ReportError("Socket creation failed for %s!", m_pNetAddress->GetHost(), 0);
		return -1;
	}

	int res = connect(m_iSocket , (struct sockaddr *) & sSocketAddress, sizeof(sSocketAddress));

	if (res < 0)
	{
		ReportError("Connection to %s failed!", m_pNetAddress->GetHost(), 0);
		return -1;
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
		ReportError("setsockopt failed", NULL, 0);
	}

	return 0;
}

unsigned int Connection::ResolveHostAddr(const char* szHost)
{
	unsigned int uaddr = inet_addr(szHost);
	if (uaddr == (unsigned int)-1)
	{
		struct hostent* hinfo;
		bool err = false;
		int h_errnop = 0;
#ifdef WIN32
		hinfo = gethostbyname(szHost);
		err = hinfo == NULL;
		h_errnop = WSAGetLastError();
#else
		struct hostent hinfobuf;
		static const int strbuflen = 1024;
		char* strbuf = (char*)malloc(strbuflen);
#ifdef HAVE_GETHOSTBYNAME_R_6
		err = gethostbyname_r(szHost, &hinfobuf, strbuf, strbuflen, &hinfo, &h_errnop);
		err = err || (hinfo == NULL); // error on null hinfo (means 'no entry')
#else
		hinfo = gethostbyname_r(szHost, &hinfobuf, strbuf, strbuflen, &h_errnop);
		err = hinfo == NULL;
#endif			
#endif
		if (err)
		{
			ReportError("Could not resolve hostname %s", szHost, h_errnop);
#ifndef WIN32
			free(strbuf);
#endif
			return (unsigned int)-1;
		}

		memcpy(&uaddr, hinfo->h_addr_list[0], sizeof(uaddr));
#ifndef WIN32
		free(strbuf);
#endif
	}
	return uaddr;
}

int Connection::DoDisconnect()
{
	debug("Do disconnecting");

	if (m_iSocket > 0)
	{
		closesocket(m_iSocket);
		m_iSocket = INVALID_SOCKET;
	}

	m_eStatus = csDisconnected;
	return 0;
}

int Connection::DoWriteLine(char* szText)
{
	//debug("Connection::doWrite()");
	return send(m_iSocket, szText, strlen(szText), 0);
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
			iBufAvail = recv(m_iSocket, m_szReadBuf, ReadBufLen, 0);
			if (iBufAvail < 0)
			{
				ReportError("Could not receive data on socket", NULL, 0);
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
			len = p - szBufPtr + 1;
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
		
		if (p)
		{
			break;
		}
		iSize--;
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

	m_iSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (m_iSocket == INVALID_SOCKET)
	{
		ReportError("Socket creation failed for %s!", m_pNetAddress->GetHost(), 0);
		return -1;
	}

	struct sockaddr_in	sSocketAddress;
	memset(&sSocketAddress, '\0', sizeof(sSocketAddress));
	sSocketAddress.sin_family = AF_INET;
	sSocketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	sSocketAddress.sin_port = htons(m_pNetAddress->GetPort());
	int opt = 1;
	setsockopt(m_iSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

	if (bind(m_iSocket, (struct sockaddr *) &sSocketAddress, sizeof(sSocketAddress)) < 0)
	{
		ReportError("Binding socket failed for %s", m_pNetAddress->GetHost(), 0);
		return -1;
	}

	if (listen(m_iSocket, 10) < 0)
	{
		ReportError("Listen on socket failed for %s", m_pNetAddress->GetHost(), 0);
		return -1;
	}

	return 0;
}

SOCKET Connection::DoAccept()
{
	struct sockaddr_in ClientAddress;
	socklen_t SockLen;

	SockLen = sizeof(ClientAddress);

	SOCKET iSocket = accept(GetSocket(), (struct sockaddr *) & ClientAddress, &SockLen);

	if (iSocket == INVALID_SOCKET && !m_bCanceling)
	{
		ReportError("Could not accept connection", NULL, 0);
	}

	return iSocket;
}

void Connection::Cancel()
{
	debug("Cancelling connection");
	m_bCanceling = true;
	if (m_iSocket != INVALID_SOCKET)
	{
		int r = shutdown(m_iSocket, SHUT_RDWR);
		if (r == -1)
		{
			ReportError("Could not shutdown connection", NULL, 0);
		}
		m_eStatus = csCancelled;
	}
}

void Connection::ReportError(const char* szMsgPrefix, const char* szMsgArg, int ErrCode)
{
	if (ErrCode == 0)
	{
#ifdef WIN32
		ErrCode = WSAGetLastError();
#else
		ErrCode = errno;
#endif
	}

	char szErrPrefix[1024];
	snprintf(szErrPrefix, 1024, szMsgPrefix, szMsgArg);
	szErrPrefix[1024-1] = '\0';
#ifdef WIN32
	debug("%s: ErrNo %i", szErrPrefix, ErrCode);
#else
	const char* szErrMsg = hstrerror(ErrCode);
	debug("%s: ErrNo %i, %s", szErrPrefix, ErrCode, szErrMsg);
#endif
}
