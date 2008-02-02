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


#ifndef CONNECTION_H
#define CONNECTION_H

#include "NetAddress.h"

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

	unsigned int		ResolveHostAddr(const char* szHost);
	void				ReportError(const char* szMsgPrefix, const char* szMsgArg, int ErrCode);
	virtual int 		DoConnect();
	virtual int			DoDisconnect();
	int					DoBind();
	int					DoWriteLine(char* text);
	char*				DoReadLine(char* pBuffer, int iSize, int* pBytesRead);
	SOCKET				DoAccept();

public:
						Connection(NetAddress* pNetAddress);
						Connection(SOCKET iSocket, bool bAutoClose);
	virtual 			~Connection();
	static void			Init();
	static void			Final();
	int 				Connect();
	int					Disconnect();
	int					Bind();
	int					Send(char* pBuffer, int iSize);
	int					Recv(char* pBuffer, int iSize);
	bool				RecvAll(char* pBuffer, int iSize);
	char*				ReadLine(char* pBuffer, int iSize, int* pBytesRead);
	int					WriteLine(char* text);
	SOCKET				Accept();
	void				Cancel();
	NetAddress*			GetServer() { return m_pNetAddress; }
	SOCKET				GetSocket() { return m_iSocket; }
	void				SetTimeout(int iTimeout) { m_iTimeout = iTimeout; }
	EStatus				GetStatus() { return m_eStatus; }
	void				SetSuppressErrors(bool bSuppressErrors) { m_bSuppressErrors = bSuppressErrors; }
	bool				GetSuppressErrors() { return m_bSuppressErrors; }
};

#endif
