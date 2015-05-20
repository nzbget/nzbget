/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2008-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#ifndef TLS_H
#define TLS_H

#ifndef DISABLE_TLS

class TLSSocket
{
private:
	bool				m_bIsClient;
	char*				m_szCertFile;
	char*				m_szKeyFile;
	char*				m_szCipher;
	SOCKET				m_iSocket;
	bool				m_bSuppressErrors;
	int					m_iRetCode;
	bool				m_bInitialized;
	bool				m_bConnected;

	// using "void*" to prevent the including of GnuTLS/OpenSSL header files into TLS.h
	void*				m_pContext;
	void*				m_pSession;

	void				ReportError(const char* szErrMsg);

protected:
	virtual void		PrintError(const char* szErrMsg);

public:
						TLSSocket(SOCKET iSocket, bool bIsClient, const char* szCertFile, const char* szKeyFile, const char* szCipher);
	virtual				~TLSSocket();
	static void			Init();
	static void			Final();
	bool				Start();
	void				Close();
	int					Send(const char* pBuffer, int iSize);
	int					Recv(char* pBuffer, int iSize);
	void				SetSuppressErrors(bool bSuppressErrors) { m_bSuppressErrors = bSuppressErrors; }
};

#endif
#endif
