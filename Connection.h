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
	static const int	ReadBufLen = 1024;
	char				m_szReadBuf[ReadBufLen + 1];
	int					m_iBufAvail;
	char*				m_szBufPtr;
	EStatus				m_eStatus;
	bool				m_bCanceling;
	int					m_iTimeout;
	unsigned int		ResolveHostAddr(const char* szHost);
	void				ReportError(const char* szMsgPrefix, const char* szMsgArg, int ErrCode);

public:
						Connection(NetAddress* pNetAddress);
	virtual 			~Connection();
	static void			Init();
	static void			Final();
	int 				Connect();
	int					Disconnect();
	int					Bind();
	int					Send(char* pBuffer, int iSize);
	int					Recv(char* pBuffer, int iSize);
	bool				RecvAll(char* pBuffer, int iSize);
	char*				ReadLine(char* pBuffer, int iSize);
	int					WriteLine(char* text);
	SOCKET				Accept();
	void				Cancel();
	NetAddress*			GetServer() { return m_pNetAddress; }
	SOCKET				GetSocket() { return m_iSocket; }
	void				SetTimeout(int iTimeout) { m_iTimeout = iTimeout; }

protected:
	virtual int 		DoConnect();
	virtual int			DoDisconnect();
	int					DoBind();
	int					DoWriteLine(char* text);
	char*				DoReadLine(char* pBuffer, int iSize);
	SOCKET				DoAccept();
};

#endif
