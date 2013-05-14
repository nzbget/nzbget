/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2008-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
# include "config.h"
#endif

#ifdef WIN32
#define SKIP_DEFAULT_WINDOWS_HEADERS
#include "win32.h"
#endif

#ifndef DISABLE_TLS

#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <strings.h>
#endif
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <list>

#ifdef WIN32
#include "nzbget.h"
#endif

#ifdef HAVE_LIBGNUTLS
#include <gnutls/gnutls.h>
#if GNUTLS_VERSION_NUMBER <= 0x020b00
#define NEED_GCRYPT_LOCKING
#endif
#ifdef NEED_GCRYPT_LOCKING
#include <gcrypt.h>
#endif /* NEED_GCRYPT_LOCKING */
#endif /* HAVE_LIBGNUTLS */
#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif /* HAVE_OPENSSL */

#ifndef WIN32
#include "nzbget.h"
#endif

#include "TLS.h"
#include "Thread.h"
#include "Log.h"


#ifdef HAVE_LIBGNUTLS
#ifdef NEED_GCRYPT_LOCKING

/**
 * Mutexes for gcryptlib
 */

typedef std::list<Mutex*> Mutexes;
Mutexes* g_pGCryptLibMutexes;

static int gcry_mutex_init(void **priv)
{
	Mutex* pMutex = new Mutex();
	g_pGCryptLibMutexes->push_back(pMutex);
	*priv = pMutex;
	return 0;
}

static int gcry_mutex_destroy(void **lock)
{
	Mutex* pMutex = ((Mutex*)*lock);
	g_pGCryptLibMutexes->remove(pMutex);
	delete pMutex;
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

Mutex* *g_pOpenSSLMutexes;

static void openssl_locking(int mode, int n, const char *file, int line)
{
	Mutex* mutex = g_pOpenSSLMutexes[n];
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
static unsigned long openssl_thread_id(void)
{
#ifdef WIN32
	return (unsigned long)GetCurrentThreadId();
#else
	return (unsigned long)pthread_self();
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


void TLSSocket::Init()
{
	debug("Initializing TLS library");

#ifdef HAVE_LIBGNUTLS
#ifdef NEED_GCRYPT_LOCKING
	g_pGCryptLibMutexes = new Mutexes();
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
	int iMaxMutexes = CRYPTO_num_locks();
	g_pOpenSSLMutexes = (Mutex**)malloc(sizeof(Mutex*)*iMaxMutexes);
	for (int i=0; i < iMaxMutexes; i++)
	{
		g_pOpenSSLMutexes[i] = new Mutex();
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

void TLSSocket::Final()
{
	debug("Finalizing TLS library");

#ifdef HAVE_LIBGNUTLS
	gnutls_global_deinit();

#ifdef NEED_GCRYPT_LOCKING
	// fixing memory leak in gcryptlib
	for (Mutexes::iterator it = g_pGCryptLibMutexes->begin(); it != g_pGCryptLibMutexes->end(); it++)
	{
		delete *it;
	}
	delete g_pGCryptLibMutexes;
#endif /* NEED_GCRYPT_LOCKING */
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
	int iMaxMutexes = CRYPTO_num_locks();
	for (int i=0; i < iMaxMutexes; i++)
	{
		delete g_pOpenSSLMutexes[i];
	}
	free(g_pOpenSSLMutexes);
#endif /* HAVE_OPENSSL */
}

TLSSocket::TLSSocket(SOCKET iSocket, bool bIsClient, const char* szCertFile, const char* szKeyFile, const char* szCipher)
{
	m_iSocket = iSocket;
	m_bIsClient = bIsClient;
	m_szCertFile = szCertFile ? strdup(szCertFile) : NULL;
	m_szKeyFile = szKeyFile ? strdup(szKeyFile) : NULL;
	m_szCipher = szCipher && strlen(szCipher) > 0 ? strdup(szCipher) : NULL;
	m_pContext = NULL;
	m_pSession = NULL;
	m_bSuppressErrors = false;
	m_bInitialized = false;
	m_bConnected = false;
}

TLSSocket::~TLSSocket()
{
	if (m_szCertFile)
	{
		free(m_szCertFile);
	}
	if (m_szKeyFile)
	{
		free(m_szKeyFile);
	}
	if (m_szCipher)
	{
		free(m_szCipher);
	}
	Close();
}

void TLSSocket::ReportError(const char* szErrMsg)
{
#ifdef HAVE_LIBGNUTLS
	const char* errstr = gnutls_strerror(m_iRetCode);
	if (m_bSuppressErrors)
	{
		debug("%s: %s", szErrMsg, errstr);
	}
	else
	{
		error("%s: %s", szErrMsg, errstr);
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
		
		if (m_bSuppressErrors)
		{
			debug("%s: %s", szErrMsg, errstr);
		}
		else if (errcode != 0)
		{
			error("%s: %s", szErrMsg, errstr);
		}
		else
		{
			error("%s", szErrMsg);
		}
	} while (errcode);
#endif /* HAVE_OPENSSL */
}

bool TLSSocket::Start()
{
#ifdef HAVE_LIBGNUTLS
	gnutls_certificate_credentials_t cred;
	m_iRetCode = gnutls_certificate_allocate_credentials(&cred);
	if (m_iRetCode != 0)
	{
		ReportError("Could not create TLS context");
		return false;
	}

	m_pContext = cred;

	if (m_szCertFile && m_szKeyFile)
	{
		m_iRetCode = gnutls_certificate_set_x509_key_file((gnutls_certificate_credentials_t)m_pContext,
			m_szCertFile, m_szKeyFile, GNUTLS_X509_FMT_PEM);
		if (m_iRetCode != 0)
		{
			ReportError("Could not load certificate or key file");
			Close();
			return false;
		}
	}

	gnutls_session_t sess;
	m_iRetCode = gnutls_init(&sess, m_bIsClient ? GNUTLS_CLIENT : GNUTLS_SERVER);
	if (m_iRetCode != 0)
	{
		ReportError("Could not create TLS session");
		Close();
		return false;
	}

	m_pSession = sess;

	m_bInitialized = true;

	const char* szPriority = m_szCipher ? m_szCipher : "NORMAL";

	m_iRetCode = gnutls_priority_set_direct((gnutls_session_t)m_pSession, szPriority, NULL);
	if (m_iRetCode != 0)
	{
		ReportError("Could not select cipher for TLS session");
		Close();
		return false;
	}

	m_iRetCode = gnutls_credentials_set((gnutls_session_t)m_pSession, GNUTLS_CRD_CERTIFICATE, 
		(gnutls_certificate_credentials_t*)m_pContext);
	if (m_iRetCode != 0)
	{
		ReportError("Could not initialize TLS session");
		Close();
		return false;
	}

	gnutls_transport_set_ptr((gnutls_session_t)m_pSession, (gnutls_transport_ptr_t)(size_t)m_iSocket);

	m_iRetCode = gnutls_handshake((gnutls_session_t)m_pSession);
	if (m_iRetCode != 0)
	{
		ReportError("TLS handshake failed");
		Close();
		return false;
	}

	m_bConnected = true;
	return true;
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
	m_pContext = SSL_CTX_new(SSLv23_method());

	if (!m_pContext)
	{
		ReportError("Could not create TLS context");
		return false;
	}

	if (m_szCertFile && m_szKeyFile)
	{
		if (SSL_CTX_use_certificate_file((SSL_CTX*)m_pContext, m_szCertFile, SSL_FILETYPE_PEM) != 1)
		{
			ReportError("Could not load certificate file");
			Close();
			return false;
		}
		if (SSL_CTX_use_PrivateKey_file((SSL_CTX*)m_pContext, m_szKeyFile, SSL_FILETYPE_PEM) != 1)
		{
			ReportError("Could not load key file");
			Close();
			return false;
		}
	}
	
	m_pSession = SSL_new((SSL_CTX*)m_pContext);
	if (!m_pSession)
	{
		ReportError("Could not create TLS session");
		Close();
		return false;
	}

	if (m_szCipher && !SSL_set_cipher_list((SSL*)m_pSession, m_szCipher))
	{
		ReportError("Could not select cipher for TLS");
		Close();
		return false;
	}

	if (!SSL_set_fd((SSL*)m_pSession, m_iSocket))
	{
		ReportError("Could not set the file descriptor for TLS");
		Close();
		return false;
	}

	int error_code = m_bIsClient ? SSL_connect((SSL*)m_pSession) : SSL_accept((SSL*)m_pSession);
	if (error_code < 1)
	{
		ReportError("TLS handshake failed");
		Close();
		return false;
	}

	m_bConnected = true;
	return true;
#endif /* HAVE_OPENSSL */
}

void TLSSocket::Close()
{
	if (m_pSession)
	{
#ifdef HAVE_LIBGNUTLS
		if (m_bConnected)
		{
			gnutls_bye((gnutls_session_t)m_pSession, GNUTLS_SHUT_WR);
		}
		if (m_bInitialized)
		{
			gnutls_deinit((gnutls_session_t)m_pSession);
		}
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
		if (m_bConnected)
		{
			SSL_shutdown((SSL*)m_pSession);
		}
		SSL_free((SSL*)m_pSession);
#endif /* HAVE_OPENSSL */

		m_pSession = NULL;
	}

	if (m_pContext)
	{
#ifdef HAVE_LIBGNUTLS
		gnutls_certificate_free_credentials((gnutls_certificate_credentials_t)m_pContext);
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
		SSL_CTX_free((SSL_CTX*)m_pContext);
#endif /* HAVE_OPENSSL */

		m_pContext = NULL;
	}
}

int TLSSocket::Send(const char* pBuffer, int iSize)
{
	int ret;

#ifdef HAVE_LIBGNUTLS
	ret = gnutls_record_send((gnutls_session_t)m_pSession, pBuffer, iSize);
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
	ret = SSL_write((SSL*)m_pSession, pBuffer, iSize);
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

int TLSSocket::Recv(char* pBuffer, int iSize)
{
	int ret;

#ifdef HAVE_LIBGNUTLS
	ret = gnutls_record_recv((gnutls_session_t)m_pSession, pBuffer, iSize);
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
	ret = SSL_read((SSL*)m_pSession, pBuffer, iSize);
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
