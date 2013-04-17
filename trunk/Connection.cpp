/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include "nzbget.h"
#include "Connection.h"
#include "Log.h"

static const int CONNECTION_READBUFFER_SIZE = 1024;
#ifndef HAVE_GETADDRINFO
#ifndef HAVE_GETHOSTBYNAME_R
Mutex* Connection::m_pMutexGetHostByName = NULL;
#endif
#endif


void Connection::Init()
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
	TLSSocket::Init();
#endif

#ifndef HAVE_GETADDRINFO
#ifndef HAVE_GETHOSTBYNAME_R
	m_pMutexGetHostByName = new Mutex();
#endif
#endif
}

void Connection::Final()
{
	debug("Finalizing global connection data");

#ifdef WIN32
	WSACleanup();
#endif

#ifndef DISABLE_TLS
	TLSSocket::Final();
#endif

#ifndef HAVE_GETADDRINFO
#ifndef HAVE_GETHOSTBYNAME_R
	delete m_pMutexGetHostByName;
#endif
#endif
}

Connection::Connection(const char* szHost, int iPort, bool bTLS)
{
	debug("Creating Connection");

	m_szHost			= NULL;
	m_iPort				= iPort;
	m_bTLS				= bTLS;
	m_szCipher			= NULL;
	m_eStatus			= csDisconnected;
	m_iSocket			= INVALID_SOCKET;
	m_iBufAvail			= 0;
	m_iTimeout			= 60;
	m_bSuppressErrors	= true;
	m_szReadBuf			= (char*)malloc(CONNECTION_READBUFFER_SIZE + 1);
#ifndef DISABLE_TLS
	m_pTLSSocket		= NULL;
	m_bTLSError			= false;
#endif

	if (szHost)
	{
		m_szHost = strdup(szHost);
	}
}

Connection::Connection(SOCKET iSocket, bool bTLS)
{
	debug("Creating Connection");

	m_szHost			= NULL;
	m_iPort				= 0;
	m_bTLS				= bTLS;
	m_szCipher			= NULL;
	m_eStatus			= csConnected;
	m_iSocket			= iSocket;
	m_iBufAvail			= 0;
	m_iTimeout			= 60;
	m_bSuppressErrors	= true;
	m_szReadBuf			= (char*)malloc(CONNECTION_READBUFFER_SIZE + 1);
#ifndef DISABLE_TLS
	m_pTLSSocket		= NULL;
	m_bTLSError			= false;
#endif
}

Connection::~Connection()
{
	debug("Destroying Connection");

	Disconnect();

	if (m_szHost)
	{
		free(m_szHost);
	}
	if (m_szCipher)
	{
		free(m_szCipher);
	}

	free(m_szReadBuf);
#ifndef DISABLE_TLS
	if (m_pTLSSocket)
	{
		delete m_pTLSSocket;
	}
#endif
}

void Connection::SetSuppressErrors(bool bSuppressErrors)
{
	m_bSuppressErrors = bSuppressErrors;
#ifndef DISABLE_TLS
	if (m_pTLSSocket)
	{
		m_pTLSSocket->SetSuppressErrors(bSuppressErrors);
	}
#endif
}

void Connection::SetCipher(const char* szCipher)
{
	if (m_szCipher)
	{
		free(m_szCipher);
	}
	m_szCipher = szCipher ? strdup(szCipher) : NULL;
}

bool Connection::Connect()
{
	debug("Connecting");

	if (m_eStatus == csConnected)
	{
		return true;
	}

	bool bRes = DoConnect();

	if (bRes)
	{
		m_eStatus = csConnected;
	}
	else
	{
		DoDisconnect();
	}

	return bRes;
}

bool Connection::Disconnect()
{
	debug("Disconnecting");

	if (m_eStatus == csDisconnected)
	{
		return true;
	}

	bool bRes = DoDisconnect();

	m_eStatus = csDisconnected;
	m_iSocket = INVALID_SOCKET;
	m_iBufAvail = 0;

	return bRes;
}

bool Connection::Bind()
{
	debug("Binding");

	if (m_eStatus == csListening)
	{
		return true;
	}

#ifdef HAVE_GETADDRINFO
	struct addrinfo addr_hints, *addr_list, *addr;
	char iPortStr[sizeof(int) * 4 + 1]; // is enough to hold any converted int
	
	memset(&addr_hints, 0, sizeof(addr_hints));
	addr_hints.ai_family = AF_UNSPEC;    // Allow IPv4 or IPv6
	addr_hints.ai_socktype = SOCK_STREAM,
	addr_hints.ai_flags = AI_PASSIVE;    // For wildcard IP address
	
	sprintf(iPortStr, "%d", m_iPort);
	
	int res = getaddrinfo(m_szHost, iPortStr, &addr_hints, &addr_list);
	if (res != 0)
	{
		error("Could not resolve hostname %s", m_szHost);
		return false;
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
	
#else
	
	struct sockaddr_in	sSocketAddress;
	memset(&sSocketAddress, 0, sizeof(sSocketAddress));
	sSocketAddress.sin_family = AF_INET;
	if (!m_szHost || strlen(m_szHost) == 0)
	{
		sSocketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	else
	{
		sSocketAddress.sin_addr.s_addr = ResolveHostAddr(m_szHost);
		if (sSocketAddress.sin_addr.s_addr == (unsigned int)-1)
		{
			return false;
		}
	}
	sSocketAddress.sin_port = htons(m_iPort);
	
	m_iSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (m_iSocket == INVALID_SOCKET)
	{
		ReportError("Socket creation failed for %s", m_szHost, true, 0);
		return false;
	}
	
	int opt = 1;
	setsockopt(m_iSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
	
	int res = bind(m_iSocket, (struct sockaddr *) &sSocketAddress, sizeof(sSocketAddress));
	if (res == -1)
	{
		// Connection failed
		closesocket(m_iSocket);
		m_iSocket = INVALID_SOCKET;
	}
#endif
	
	if (m_iSocket == INVALID_SOCKET)
	{
		ReportError("Binding socket failed for %s", m_szHost, true, 0);
		return false;
	}
	
	if (listen(m_iSocket, 100) < 0)
	{
		ReportError("Listen on socket failed for %s", m_szHost, true, 0);
		return false;
	}
	
	m_eStatus = csListening;

	return true;
}

int Connection::WriteLine(const char* pBuffer)
{
	//debug("Connection::WriteLine");

	if (m_eStatus != csConnected)
	{
		return -1;
	}

	int iRes = send(m_iSocket, pBuffer, strlen(pBuffer), 0);

	return iRes;
}

bool Connection::Send(const char* pBuffer, int iSize)
{
	debug("Sending data");

	if (m_eStatus != csConnected)
	{
		return false;
	}

	int iBytesSent = 0;
	while (iBytesSent < iSize)
	{
		int iRes = send(m_iSocket, pBuffer + iBytesSent, iSize-iBytesSent, 0);
		if (iRes <= 0)
		{
			return false;
		}
		iBytesSent += iRes;
	}

	return true;
}

char* Connection::ReadLine(char* pBuffer, int iSize, int* pBytesRead)
{
	if (m_eStatus != csConnected)
	{
		return NULL;
	}

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

Connection* Connection::Accept()
{
	debug("Accepting connection");

	if (m_eStatus != csListening)
	{
		return NULL;
	}

	SOCKET iSocket = accept(m_iSocket, NULL, NULL);
	if (iSocket == INVALID_SOCKET && m_eStatus != csCancelled)
	{
		ReportError("Could not accept connection", NULL, true, 0);
	}
	if (iSocket == INVALID_SOCKET)
	{
		return NULL;
	}
	
	Connection* pCon = new Connection(iSocket, m_bTLS);

	return pCon;
}

int Connection::TryRecv(char* pBuffer, int iSize)
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

bool Connection::Recv(char * pBuffer, int iSize)
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

	m_iSocket = INVALID_SOCKET;
	
#ifdef HAVE_GETADDRINFO
	struct addrinfo addr_hints, *addr_list, *addr;
	char iPortStr[sizeof(int) * 4 + 1]; //is enough to hold any converted int

	memset(&addr_hints, 0, sizeof(addr_hints));
	addr_hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	addr_hints.ai_socktype = SOCK_STREAM,

	sprintf(iPortStr, "%d", m_iPort);

	int res = getaddrinfo(m_szHost, iPortStr, &addr_hints, &addr_list);
	if (res != 0)
	{
		ReportError("Could not resolve hostname %s", m_szHost, true, 0);
		return false;
	}

	for (addr = addr_list; addr != NULL; addr = addr->ai_next)
	{
		bool bLastAddr = !addr->ai_next;
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
			if (bLastAddr)
			{
				ReportError("Connection to %s failed", m_szHost, true, 0);
			}
			closesocket(m_iSocket);
			m_iSocket = INVALID_SOCKET;
		}
		else if (bLastAddr)
		{
			ReportError("Socket creation failed for %s", m_szHost, true, 0);
		}
	}

	freeaddrinfo(addr_list);

	if (m_iSocket == INVALID_SOCKET)
	{
		return false;
	} 

#else

	struct sockaddr_in	sSocketAddress;
	memset(&sSocketAddress, 0, sizeof(sSocketAddress));
	sSocketAddress.sin_family = AF_INET;
	sSocketAddress.sin_port = htons(m_iPort);
	sSocketAddress.sin_addr.s_addr = ResolveHostAddr(m_szHost);
	if (sSocketAddress.sin_addr.s_addr == (unsigned int)-1)
	{
		return false;
	}

	m_iSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (m_iSocket == INVALID_SOCKET)
	{
		ReportError("Socket creation failed for %s", m_szHost, true, 0);
		return false;
	}

	int res = connect(m_iSocket , (struct sockaddr *) & sSocketAddress, sizeof(sSocketAddress));
	if (res == -1)
	{
		ReportError("Connection to %s failed", m_szHost, true, 0);
		closesocket(m_iSocket);
		m_iSocket = INVALID_SOCKET;
		return false;
	}
#endif

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
		ReportError("Socket initialization failed for %s", m_szHost, true, 0);
	}

#ifndef DISABLE_TLS
	if (m_bTLS && !StartTLS(true, NULL, NULL))
	{
		return false;
	}
#endif

	return true;
}

bool Connection::DoDisconnect()
{
	debug("Do disconnecting");

	if (m_iSocket != INVALID_SOCKET)
	{
#ifndef DISABLE_TLS
		CloseTLS();
#endif
		closesocket(m_iSocket);
		m_iSocket = INVALID_SOCKET;
	}

	m_eStatus = csDisconnected;
	return true;
}

void Connection::ReadBuffer(char** pBuffer, int *iBufLen)
{
	*iBufLen = m_iBufAvail;
	*pBuffer = m_szBufPtr;
	m_iBufAvail = 0;
};

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

void Connection::ReportError(const char* szMsgPrefix, const char* szMsgArg, bool PrintErrCode, int herrno)
{
#ifndef DISABLE_TLS
	if (m_bTLSError)
	{
		// TLS-Error was already reported
		m_bTLSError = false;
		return;
	}
#endif

	char szErrPrefix[1024];
	snprintf(szErrPrefix, 1024, szMsgPrefix, szMsgArg);
	szErrPrefix[1024-1] = '\0';
	
	if (PrintErrCode)
	{
#ifdef WIN32
		int ErrCode = WSAGetLastError();
		char szErrMsg[1024];
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, ErrCode, 0, szErrMsg, 1024, NULL);
		szErrMsg[1024-1] = '\0';
#else
		const char *szErrMsg = NULL;
		int ErrCode = herrno;
		if (herrno == 0)
		{
			ErrCode = errno;
			szErrMsg = strerror(ErrCode);
		}
		else
		{
			szErrMsg = hstrerror(ErrCode);
		}
#endif
		if (m_bSuppressErrors)
		{
			debug("%s: ErrNo %i, %s", szErrPrefix, ErrCode, szErrMsg);
		}
		else
		{
			error("%s: ErrNo %i, %s", szErrPrefix, ErrCode, szErrMsg);
		}
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
bool Connection::StartTLS(bool bIsClient, const char* szCertFile, const char* szKeyFile)
{
	debug("Starting TLS");

	if (m_pTLSSocket)
	{
		delete m_pTLSSocket;
	}

	m_pTLSSocket = new TLSSocket(m_iSocket, bIsClient, szCertFile, szKeyFile, m_szCipher);
	m_pTLSSocket->SetSuppressErrors(m_bSuppressErrors);

	return m_pTLSSocket->Start();
}

void Connection::CloseTLS()
{
	if (m_pTLSSocket)
	{
		m_pTLSSocket->Close();
		delete m_pTLSSocket;
		m_pTLSSocket = NULL;
	}
}

int Connection::recv(SOCKET s, char* buf, int len, int flags)
{
	int iReceived = 0;
	
	if (m_pTLSSocket)
	{
		m_bTLSError = false;
		iReceived = m_pTLSSocket->Recv(buf, len);
		if (iReceived < 0)
		{
			m_bTLSError = true;
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
	int iSent = 0;

	if (m_pTLSSocket)
	{
		m_bTLSError = false;
		iSent = m_pTLSSocket->Send(buf, len);
		if (iSent < 0)
		{
			m_bTLSError = true;
			return -1;
		}
		return iSent;
	}
	else
	{
		iSent = ::send(s, buf, len, flags);
		return iSent;
	}
}
#endif

#ifndef HAVE_GETADDRINFO
unsigned int Connection::ResolveHostAddr(const char* szHost)
{
	unsigned int uaddr = inet_addr(szHost);
	if (uaddr == (unsigned int)-1)
	{
		struct hostent* hinfo;
		bool err = false;
		int h_errnop = 0;
#ifdef HAVE_GETHOSTBYNAME_R
		struct hostent hinfobuf;
		char strbuf[1024];
#ifdef HAVE_GETHOSTBYNAME_R_6
		err = gethostbyname_r(szHost, &hinfobuf, strbuf, sizeof(strbuf), &hinfo, &h_errnop);
		err = err || (hinfo == NULL); // error on null hinfo (means 'no entry')
#endif			
#ifdef HAVE_GETHOSTBYNAME_R_5
		hinfo = gethostbyname_r(szHost, &hinfobuf, strbuf, sizeof(strbuf), &h_errnop);
		err = hinfo == NULL;
#endif			
#ifdef HAVE_GETHOSTBYNAME_R_3
		//NOTE: gethostbyname_r with three parameters were not tested
		struct hostent_data hinfo_data;
		hinfo = gethostbyname_r((char*)szHost, (struct hostent*)hinfobuf, &hinfo_data);
		err = hinfo == NULL;
#endif			
#else
		m_pMutexGetHostByName->Lock();
		hinfo = gethostbyname(szHost);
		err = hinfo == NULL;
#endif
		if (err)
		{
#ifndef HAVE_GETHOSTBYNAME_R
			m_pMutexGetHostByName->Unlock();
#endif
			ReportError("Could not resolve hostname %s", szHost, true, h_errnop);
			return (unsigned int)-1;
		}

		memcpy(&uaddr, hinfo->h_addr_list[0], sizeof(uaddr));
		
#ifndef HAVE_GETHOSTBYNAME_R
		m_pMutexGetHostByName->Unlock();
#endif
	}
	return uaddr;
}
#endif

const char* Connection::GetRemoteAddr()
{
	struct sockaddr_in PeerName;
	int iPeerNameLength = sizeof(PeerName);
	if (getpeername(m_iSocket, (struct sockaddr*)&PeerName, (SOCKLEN_T*) &iPeerNameLength) >= 0)
	{
#ifdef WIN32
		 strncpy(m_szRemoteAddr, inet_ntoa(PeerName.sin_addr), sizeof(m_szRemoteAddr));
#else
		inet_ntop(AF_INET, &PeerName.sin_addr, m_szRemoteAddr, sizeof(m_szRemoteAddr));
#endif
	}
	m_szRemoteAddr[sizeof(m_szRemoteAddr)-1] = '\0';
	
	return m_szRemoteAddr;
}
