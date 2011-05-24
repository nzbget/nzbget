/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2009 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef CONNECTION_H
#define CONNECTION_H

#include "NetAddress.h"
#ifndef HAVE_GETADDRINFO
#ifndef HAVE_GETHOSTBYNAME_R
#include "Thread.h"
#endif
#endif

class Connection
{
public:
	enum EStatus
	{
		csConnected,
		csDisconnected,
		csListening,
		csCancelled
	};
	
protected:
	NetAddress* 		m_pNetAddress;
	SOCKET				m_iSocket;
	char*				m_szReadBuf;
	int					m_iBufAvail;
	char*				m_szBufPtr;
	EStatus				m_eStatus;
	int					m_iTimeout;
	bool				m_bSuppressErrors;
	bool				m_bAutoClose;
#ifndef DISABLE_TLS
	void*				m_pTLS;
	static bool			bTLSLibInitialized;
	bool				m_bTLSError;
#endif
#ifndef HAVE_GETADDRINFO
#ifndef HAVE_GETHOSTBYNAME_R
	static Mutex*		m_pMutexGetHostByName;
#endif
#endif

	void				ReportError(const char* szMsgPrefix, const char* szMsgArg, bool PrintErrCode, int herrno);
	virtual bool 		DoConnect();
	virtual bool		DoDisconnect();
	int					DoBind();
	int					DoWriteLine(const char* pBuffer);
	char*				DoReadLine(char* pBuffer, int iSize, int* pBytesRead);
	SOCKET				DoAccept();
#ifndef HAVE_GETADDRINFO
	unsigned int		ResolveHostAddr(const char* szHost);
#endif
#ifndef DISABLE_TLS
	bool				CheckTLSResult(int iResultCode, char* szErrStr, const char* szErrMsgPrefix);
	int					recv(SOCKET s, char* buf, int len, int flags);
	int					send(SOCKET s, const char* buf, int len, int flags);
	void				CloseTLS();
#endif

public:
						Connection(NetAddress* pNetAddress);
						Connection(SOCKET iSocket, bool bAutoClose);
	virtual 			~Connection();
	static void			Init(bool bTLS);
	static void			Final();
	bool 				Connect();
	bool				Disconnect();
	int					Bind();
	int					Send(const char* pBuffer, int iSize);
	int					Recv(char* pBuffer, int iSize);
	bool				RecvAll(char* pBuffer, int iSize);
	char*				ReadLine(char* pBuffer, int iSize, int* pBytesRead);
	int					WriteLine(const char* pBuffer);
	SOCKET				Accept();
	void				Cancel();
	NetAddress*			GetServer() { return m_pNetAddress; }
	SOCKET				GetSocket() { return m_iSocket; }
	void				SetTimeout(int iTimeout) { m_iTimeout = iTimeout; }
	EStatus				GetStatus() { return m_eStatus; }
	void				SetSuppressErrors(bool bSuppressErrors) { m_bSuppressErrors = bSuppressErrors; }
	bool				GetSuppressErrors() { return m_bSuppressErrors; }
#ifndef DISABLE_TLS
	bool				StartTLS();
#endif
};

#endif
