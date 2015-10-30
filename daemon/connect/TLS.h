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

class TlsSocket
{
private:
	bool				m_isClient;
	char*				m_certFile;
	char*				m_keyFile;
	char*				m_cipher;
	SOCKET				m_socket;
	bool				m_suppressErrors;
	int					m_retCode;
	bool				m_initialized;
	bool				m_connected;

	// using "void*" to prevent the including of GnuTLS/OpenSSL header files into TLS.h
	void*				m_context;
	void*				m_session;

	void				ReportError(const char* errMsg);

protected:
	virtual void		PrintError(const char* errMsg);

public:
						TlsSocket(SOCKET socket, bool isClient, const char* certFile, const char* keyFile, const char* cipher);
	virtual				~TlsSocket();
	static void			Init();
	static void			Final();
	bool				Start();
	void				Close();
	int					Send(const char* buffer, int size);
	int					Recv(char* buffer, int size);
	void				SetSuppressErrors(bool suppressErrors) { m_suppressErrors = suppressErrors; }
};

#endif
#endif
