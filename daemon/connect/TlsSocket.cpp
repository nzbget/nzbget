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


#include "nzbget.h"

#ifndef DISABLE_TLS

#include "TlsSocket.h"
#include "Thread.h"
#include "Log.h"


#ifdef HAVE_LIBGNUTLS
#ifdef NEED_GCRYPT_LOCKING

/**
 * Mutexes for gcryptlib
 */

typedef std::list<Mutex*> Mutexes;
Mutexes* g_GCryptLibMutexes;

static int gcry_mutex_init(void **priv)
{
	Mutex* mutex = new Mutex();
	g_GCryptLibMutexes->push_back(mutex);
	*priv = mutex;
	return 0;
}

static int gcry_mutex_destroy(void **lock)
{
	Mutex* mutex = ((Mutex*)*lock);
	g_GCryptLibMutexes->remove(mutex);
	delete mutex;
	return 0;
}

static int gcry_mutex_lock(void **lock)
{
	((Mutex*)*lock)->Lock();
	return 0;
}

static int gcry_mutex_unlock(void **lock)
{
	((Mutex*)*lock)->Unlock();
	return 0;
}

static struct gcry_thread_cbs gcry_threads_Mutex =
{	GCRY_THREAD_OPTION_USER, NULL,
	gcry_mutex_init, gcry_mutex_destroy,
	gcry_mutex_lock, gcry_mutex_unlock,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

#endif /* NEED_GCRYPT_LOCKING */
#endif /* HAVE_LIBGNUTLS */


#ifdef HAVE_OPENSSL

/**
 * Mutexes for OpenSSL
 */

Mutex* *g_OpenSSLMutexes;

static void openssl_locking(int mode, int n, const char *file, int line)
{
	Mutex* mutex = g_OpenSSLMutexes[n];
	if (mode & CRYPTO_LOCK)
	{
		mutex->Lock();
	}
	else
	{
		mutex->Unlock();
	}
}

/*
static uint32 openssl_thread_id(void)
{
#ifdef WIN32
	return (uint32)GetCurrentThreadId();
#else
	return (uint32)pthread_self();
#endif
}
*/

static struct CRYPTO_dynlock_value* openssl_dynlock_create(const char *file, int line)
{
	return (CRYPTO_dynlock_value*)new Mutex();
}

static void openssl_dynlock_destroy(struct CRYPTO_dynlock_value *l, const char *file, int line)
{
	Mutex* mutex = (Mutex*)l;
	delete mutex;
}

static void openssl_dynlock_lock(int mode, struct CRYPTO_dynlock_value *l, const char *file, int line)
{
	Mutex* mutex = (Mutex*)l;
	if (mode & CRYPTO_LOCK)
	{
		mutex->Lock();
	}
	else
	{
		mutex->Unlock();
	}
}

#endif /* HAVE_OPENSSL */


void TlsSocket::Init()
{
	debug("Initializing TLS library");

#ifdef HAVE_LIBGNUTLS
#ifdef NEED_GCRYPT_LOCKING
	g_GCryptLibMutexes = new Mutexes();
#endif /* NEED_GCRYPT_LOCKING */

	int error_code;

#ifdef NEED_GCRYPT_LOCKING
	error_code = gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_Mutex);
	if (error_code != 0)
	{
		error("Could not initialize libcrypt");
		return;
	}
#endif /* NEED_GCRYPT_LOCKING */

	error_code = gnutls_global_init();
	if (error_code != 0)
	{
		error("Could not initialize libgnutls");
		return;
	}

#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
	int maxMutexes = CRYPTO_num_locks();
	g_OpenSSLMutexes = (Mutex**)malloc(sizeof(Mutex*)*maxMutexes);
	for (int i=0; i < maxMutexes; i++)
	{
		g_OpenSSLMutexes[i] = new Mutex();
	}

	SSL_load_error_strings();
	SSL_library_init();
	OpenSSL_add_all_algorithms();

	CRYPTO_set_locking_callback(openssl_locking);
	//CRYPTO_set_id_callback(openssl_thread_id);
	CRYPTO_set_dynlock_create_callback(openssl_dynlock_create);
	CRYPTO_set_dynlock_destroy_callback(openssl_dynlock_destroy);
	CRYPTO_set_dynlock_lock_callback(openssl_dynlock_lock);

#endif /* HAVE_OPENSSL */
}

void TlsSocket::Final()
{
	debug("Finalizing TLS library");

#ifdef HAVE_LIBGNUTLS
	gnutls_global_deinit();

#ifdef NEED_GCRYPT_LOCKING
	// fixing memory leak in gcryptlib
	for (Mutexes::iterator it = g_GCryptLibMutexes->begin(); it != g_GCryptLibMutexes->end(); it++)
	{
		delete *it;
	}
	delete g_GCryptLibMutexes;
#endif /* NEED_GCRYPT_LOCKING */
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
	int maxMutexes = CRYPTO_num_locks();
	for (int i=0; i < maxMutexes; i++)
	{
		delete g_OpenSSLMutexes[i];
	}
	free(g_OpenSSLMutexes);
#endif /* HAVE_OPENSSL */
}

TlsSocket::TlsSocket(SOCKET socket, bool isClient, const char* certFile, const char* keyFile, const char* cipher)
{
	m_socket = socket;
	m_isClient = isClient;
	m_certFile = certFile ? strdup(certFile) : NULL;
	m_keyFile = keyFile ? strdup(keyFile) : NULL;
	m_cipher = cipher && strlen(cipher) > 0 ? strdup(cipher) : NULL;
	m_context = NULL;
	m_session = NULL;
	m_suppressErrors = false;
	m_initialized = false;
	m_connected = false;
}

TlsSocket::~TlsSocket()
{
	free(m_certFile);
	free(m_keyFile);
	free(m_cipher);
	Close();
}

void TlsSocket::ReportError(const char* errMsg)
{
	char message[1024];

#ifdef HAVE_LIBGNUTLS
	const char* errstr = gnutls_strerror(m_retCode);
	if (m_suppressErrors)
	{
		debug("%s: %s", errMsg, errstr);
	}
	else
	{
		snprintf(message, sizeof(message), "%s: %s", errMsg, errstr);
		message[sizeof(message) - 1] = '\0';
		PrintError(message);
	}
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
	int errcode;
	do
	{
		errcode = ERR_get_error();

		char errstr[1024];
		ERR_error_string_n(errcode, errstr, sizeof(errstr));
		errstr[1024-1] = '\0';

		if (m_suppressErrors)
		{
			debug("%s: %s", errMsg, errstr);
		}
		else if (errcode != 0)
		{
			snprintf(message, sizeof(message), "%s: %s", errMsg, errstr);
			message[sizeof(message) - 1] = '\0';
			PrintError(message);
		}
		else
		{
			PrintError(errMsg);
		}
	} while (errcode);
#endif /* HAVE_OPENSSL */
}

void TlsSocket::PrintError(const char* errMsg)
{
	error("%s", errMsg);
}

bool TlsSocket::Start()
{
#ifdef HAVE_LIBGNUTLS
	gnutls_certificate_credentials_t cred;
	m_retCode = gnutls_certificate_allocate_credentials(&cred);
	if (m_retCode != 0)
	{
		ReportError("Could not create TLS context");
		return false;
	}

	m_context = cred;

	if (m_certFile && m_keyFile)
	{
		m_retCode = gnutls_certificate_set_x509_key_file((gnutls_certificate_credentials_t)m_context,
			m_certFile, m_keyFile, GNUTLS_X509_FMT_PEM);
		if (m_retCode != 0)
		{
			ReportError("Could not load certificate or key file");
			Close();
			return false;
		}
	}

	gnutls_session_t sess;
	m_retCode = gnutls_init(&sess, m_isClient ? GNUTLS_CLIENT : GNUTLS_SERVER);
	if (m_retCode != 0)
	{
		ReportError("Could not create TLS session");
		Close();
		return false;
	}

	m_session = sess;

	m_initialized = true;

	const char* priority = m_cipher ? m_cipher : "NORMAL";

	m_retCode = gnutls_priority_set_direct((gnutls_session_t)m_session, priority, NULL);
	if (m_retCode != 0)
	{
		ReportError("Could not select cipher for TLS session");
		Close();
		return false;
	}

	m_retCode = gnutls_credentials_set((gnutls_session_t)m_session, GNUTLS_CRD_CERTIFICATE,
		(gnutls_certificate_credentials_t*)m_context);
	if (m_retCode != 0)
	{
		ReportError("Could not initialize TLS session");
		Close();
		return false;
	}

	gnutls_transport_set_ptr((gnutls_session_t)m_session, (gnutls_transport_ptr_t)(size_t)m_socket);

	m_retCode = gnutls_handshake((gnutls_session_t)m_session);
	if (m_retCode != 0)
	{
		ReportError("TLS handshake failed");
		Close();
		return false;
	}

	m_connected = true;
	return true;
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
	m_context = SSL_CTX_new(SSLv23_method());

	if (!m_context)
	{
		ReportError("Could not create TLS context");
		return false;
	}

	if (m_certFile && m_keyFile)
	{
		if (SSL_CTX_use_certificate_chain_file((SSL_CTX*)m_context, m_certFile) != 1)
		{
			ReportError("Could not load certificate file");
			Close();
			return false;
		}
		if (SSL_CTX_use_PrivateKey_file((SSL_CTX*)m_context, m_keyFile, SSL_FILETYPE_PEM) != 1)
		{
			ReportError("Could not load key file");
			Close();
			return false;
		}
	}

	m_session = SSL_new((SSL_CTX*)m_context);
	if (!m_session)
	{
		ReportError("Could not create TLS session");
		Close();
		return false;
	}

	if (m_cipher && !SSL_set_cipher_list((SSL*)m_session, m_cipher))
	{
		ReportError("Could not select cipher for TLS");
		Close();
		return false;
	}

	if (!SSL_set_fd((SSL*)m_session, m_socket))
	{
		ReportError("Could not set the file descriptor for TLS");
		Close();
		return false;
	}

	int error_code = m_isClient ? SSL_connect((SSL*)m_session) : SSL_accept((SSL*)m_session);
	if (error_code < 1)
	{
		ReportError("TLS handshake failed");
		Close();
		return false;
	}

	m_connected = true;
	return true;
#endif /* HAVE_OPENSSL */
}

void TlsSocket::Close()
{
	if (m_session)
	{
#ifdef HAVE_LIBGNUTLS
		if (m_connected)
		{
			gnutls_bye((gnutls_session_t)m_session, GNUTLS_SHUT_WR);
		}
		if (m_initialized)
		{
			gnutls_deinit((gnutls_session_t)m_session);
		}
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
		if (m_connected)
		{
			SSL_shutdown((SSL*)m_session);
		}
		SSL_free((SSL*)m_session);
#endif /* HAVE_OPENSSL */

		m_session = NULL;
	}

	if (m_context)
	{
#ifdef HAVE_LIBGNUTLS
		gnutls_certificate_free_credentials((gnutls_certificate_credentials_t)m_context);
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
		SSL_CTX_free((SSL_CTX*)m_context);
#endif /* HAVE_OPENSSL */

		m_context = NULL;
	}
}

int TlsSocket::Send(const char* buffer, int size)
{
	int ret;

#ifdef HAVE_LIBGNUTLS
	ret = gnutls_record_send((gnutls_session_t)m_session, buffer, size);
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
	ret = SSL_write((SSL*)m_session, buffer, size);
#endif /* HAVE_OPENSSL */

	if (ret < 0)
	{
#ifdef HAVE_OPENSSL
		if (ERR_peek_error() == 0)
		{
			ReportError("Could not write to TLS-Socket: Connection closed by remote host");
		}
		else
#endif /* HAVE_OPENSSL */
		ReportError("Could not write to TLS-Socket");
		return -1;
	}

	return ret;
}

int TlsSocket::Recv(char* buffer, int size)
{
	int ret;

#ifdef HAVE_LIBGNUTLS
	ret = gnutls_record_recv((gnutls_session_t)m_session, buffer, size);
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
	ret = SSL_read((SSL*)m_session, buffer, size);
#endif /* HAVE_OPENSSL */

	if (ret < 0)
	{
#ifdef HAVE_OPENSSL
		if (ERR_peek_error() == 0)
		{
			ReportError("Could not read from TLS-Socket: Connection closed by remote host");
		}
		else
#endif /* HAVE_OPENSSL */
		{
			ReportError("Could not read from TLS-Socket");
		}
		return -1;
	}

	return ret;
}

#endif
