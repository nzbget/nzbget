/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2008-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TLSSOCKET_H
#define TLSSOCKET_H

#ifndef DISABLE_TLS

#include "NString.h"

class TlsSocket
{
public:
	TlsSocket(SOCKET socket, bool isClient, const char* host,
		const char* certFile, const char* keyFile, const char* cipher) :
		m_socket(socket), m_isClient(isClient), m_host(host),
		m_certFile(certFile), m_keyFile(keyFile), m_cipher(cipher) {}
	virtual ~TlsSocket();
	static void Init();
	static void InitOptions(const char* certStore) { m_certStore = certStore; }
	static void Final();
	bool Start();
	void Close();
	int Send(const char* buffer, int size);
	int Recv(char* buffer, int size);
	void SetSuppressErrors(bool suppressErrors) { m_suppressErrors = suppressErrors; }

protected:
	virtual void PrintError(const char* errMsg);

private:
	SOCKET m_socket;
	bool m_isClient;
	CString m_host;
	CString m_certFile;
	CString m_keyFile;
	CString m_cipher;
	bool m_suppressErrors = false;
	bool m_initialized = false;
	bool m_connected = false;
	int m_retCode;
	static CString m_certStore;

	// using "void*" to prevent the including of GnuTLS/OpenSSL header files into TlsSocket.h
	void* m_context = nullptr;
	void* m_session = nullptr;

	void ReportError(const char* errMsg, bool suppressable = true);
	bool ValidateCert();
};

#endif
#endif
