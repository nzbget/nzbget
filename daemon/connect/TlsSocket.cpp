/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2008-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#include "nzbget.h"

#ifndef DISABLE_TLS

#include "TlsSocket.h"
#include "Thread.h"
#include "Log.h"
#include "Util.h"
#include "FileSystem.h"

CString TlsSocket::m_certStore;

#ifdef HAVE_LIBGNUTLS
#ifdef NEED_GCRYPT_LOCKING

/**
 * Mutexes for gcryptlib
 */

std::vector<std::unique_ptr<Mutex>> g_GCryptLibMutexes;

static int gcry_mutex_init(void **priv)
{
	g_GCryptLibMutexes.emplace_back(std::make_unique<Mutex>());
	*priv = g_GCryptLibMutexes.back().get();
	return 0;
}

static int gcry_mutex_destroy(void **lock)
{
	Mutex* mutex = ((Mutex*)*lock);
	g_GCryptLibMutexes.erase(std::find_if(g_GCryptLibMutexes.begin(), g_GCryptLibMutexes.end(),
		[mutex](std::unique_ptr<Mutex>& itMutex)
		{
			return itMutex.get() == mutex;
		}));
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
{	GCRY_THREAD_OPTION_USER, nullptr,
	gcry_mutex_init, gcry_mutex_destroy,
	gcry_mutex_lock, gcry_mutex_unlock,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
};

#endif /* NEED_GCRYPT_LOCKING */
#endif /* HAVE_LIBGNUTLS */


#ifdef HAVE_OPENSSL

#ifndef CRYPTO_set_locking_callback
#define NEED_CRYPTO_LOCKING
#endif

#ifdef NEED_CRYPTO_LOCKING

/**
 * Mutexes for OpenSSL
 */

std::vector<std::unique_ptr<Mutex>> g_OpenSSLMutexes;

static void openssl_locking(int mode, int n, const char* file, int line)
{
	Mutex* mutex = g_OpenSSLMutexes[n].get();
	if (mode & CRYPTO_LOCK)
	{
		mutex->Lock();
	}
	else
	{
		mutex->Unlock();
	}
}

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

#endif /* NEED_CRYPTO_LOCKING */
#endif /* HAVE_OPENSSL */


void TlsSocket::Init()
{
	debug("Initializing TLS library");

#ifdef HAVE_LIBGNUTLS
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

#ifdef NEED_CRYPTO_LOCKING
	for (int i = 0, num = CRYPTO_num_locks(); i < num; i++)
	{
		g_OpenSSLMutexes.emplace_back(std::make_unique<Mutex>());
	}

	CRYPTO_set_locking_callback(openssl_locking);
	CRYPTO_set_dynlock_create_callback(openssl_dynlock_create);
	CRYPTO_set_dynlock_destroy_callback(openssl_dynlock_destroy);
	CRYPTO_set_dynlock_lock_callback(openssl_dynlock_lock);
#endif /* NEED_CRYPTO_LOCKING */

	SSL_load_error_strings();
	SSL_library_init();
	OpenSSL_add_all_algorithms();

#endif /* HAVE_OPENSSL */
}

void TlsSocket::Final()
{
#ifdef HAVE_LIBGNUTLS
	gnutls_global_deinit();
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
#ifndef LIBRESSL_VERSION_NUMBER
#if OPENSSL_VERSION_NUMBER < 0x30000000L
	FIPS_mode_set(0);
#else
	EVP_default_properties_enable_fips(NULL, 0);
#endif
#endif
#ifdef NEED_CRYPTO_LOCKING
	CRYPTO_set_locking_callback(nullptr);
	CRYPTO_set_id_callback(nullptr);
#endif
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	ERR_remove_state(0);
#endif
#if	OPENSSL_VERSION_NUMBER >= 0x10002000L && ! defined (LIBRESSL_VERSION_NUMBER)
	SSL_COMP_free_compression_methods();
#endif
	//ENGINE_cleanup();
	CONF_modules_free();
	CONF_modules_unload(1);
#ifndef OPENSSL_NO_COMP
	COMP_zlib_cleanup();
#endif
	ERR_free_strings();
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
#endif /* HAVE_OPENSSL */
}

TlsSocket::~TlsSocket()
{
	Close();

#ifdef HAVE_OPENSSL
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	ERR_remove_state(0);
#endif
#endif
}

void TlsSocket::ReportError(const char* errMsg, bool suppressable)
{
#ifdef HAVE_LIBGNUTLS
	const char* errstr = gnutls_strerror(m_retCode);
	if (suppressable && m_suppressErrors)
	{
		debug("%s: %s", errMsg, errstr);
	}
	else
	{
		PrintError(BString<1024>("%s: %s", errMsg, errstr));
	}
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
	int errcode = ERR_get_error();
	do
	{
		char errstr[1024];
		ERR_error_string_n(errcode, errstr, sizeof(errstr));
		errstr[1024-1] = '\0';

		if (suppressable && m_suppressErrors)
		{
			debug("%s: %s", errMsg, errstr);
		}
		else if (errcode != 0)
		{
			PrintError(BString<1024>("%s: %s", errMsg, errstr));
		}
		else
		{
			PrintError(errMsg);
		}

		errcode = ERR_get_error();
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
		ReportError("Could not create TLS context", false);
		return false;
	}

	m_context = cred;

	if (m_certFile && m_keyFile)
	{
		m_retCode = gnutls_certificate_set_x509_key_file((gnutls_certificate_credentials_t)m_context,
			m_certFile, m_keyFile, GNUTLS_X509_FMT_PEM);
		if (m_retCode != 0)
		{
			ReportError("Could not load certificate or key file", false);
			Close();
			return false;
		}
	}

	gnutls_session_t sess;
	m_retCode = gnutls_init(&sess, m_isClient ? GNUTLS_CLIENT : GNUTLS_SERVER);
	if (m_retCode != 0)
	{
		ReportError("Could not create TLS session", false);
		Close();
		return false;
	}

	m_session = sess;

	m_initialized = true;

	const char* priority = !m_cipher.Empty() ? m_cipher.Str() :
		(m_certFile && m_keyFile ? "NORMAL:!VERS-SSL3.0" : "NORMAL");

	m_retCode = gnutls_priority_set_direct((gnutls_session_t)m_session, priority, nullptr);
	if (m_retCode != 0)
	{
		ReportError("Could not select cipher for TLS", false);
		Close();
		return false;
	}

	if (m_host)
	{
		m_retCode = gnutls_server_name_set((gnutls_session_t)m_session, GNUTLS_NAME_DNS, m_host, m_host.Length());
		if (m_retCode != 0)
		{
			ReportError("Could not set hostname for TLS");
			Close();
			return false;
		}
	}

	m_retCode = gnutls_credentials_set((gnutls_session_t)m_session, GNUTLS_CRD_CERTIFICATE,
		(gnutls_certificate_credentials_t*)m_context);
	if (m_retCode != 0)
	{
		ReportError("Could not initialize TLS session", false);
		Close();
		return false;
	}

	gnutls_transport_set_ptr((gnutls_session_t)m_session, (gnutls_transport_ptr_t)(size_t)m_socket);

	m_retCode = gnutls_handshake((gnutls_session_t)m_session);
	if (m_retCode != 0)
	{
		ReportError(BString<1024>("TLS handshake failed for %s", *m_host));
		Close();
		return false;
	}

	if (m_isClient && !m_certStore.Empty() && !ValidateCert())
	{
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
		ReportError("Could not create TLS context", false);
		return false;
	}

	if (m_certFile && m_keyFile)
	{
		if (SSL_CTX_use_certificate_chain_file((SSL_CTX*)m_context, m_certFile) != 1)
		{
			ReportError("Could not load certificate file", false);
			Close();
			return false;
		}
		if (SSL_CTX_use_PrivateKey_file((SSL_CTX*)m_context, m_keyFile, SSL_FILETYPE_PEM) != 1)
		{
			ReportError("Could not load key file", false);
			Close();
			return false;
		}
		if (!SSL_CTX_set_options((SSL_CTX*)m_context, SSL_OP_NO_SSLv3))
		{
			ReportError("Could not select minimum protocol version for TLS", false);
			Close();
			return false;
		}

		// For ECC certificates
		EVP_PKEY* ecdh = EVP_EC_gen( "prime256v1" );
            /*  EC_KEY_new_by_curve_name(NID_X9_62_prime256v1); */
		if (!ecdh)
		{
			ReportError("Could not generate ecdh parameters for TLS", false);
			Close();
			return false;
		}
		if (!SSL_CTX_set_tmp_ecdh((SSL_CTX*)m_context, ecdh))
		{
			ReportError("Could not set ecdh parameters for TLS", false);
			EVP_PKEY_free(ecdh);
			Close();
			return false;
		}
		EVP_PKEY_free(ecdh);
	}

	if (m_isClient && !m_certStore.Empty())
	{
		// Enable certificate validation
		if (SSL_CTX_load_verify_locations((SSL_CTX*)m_context, m_certStore, nullptr) != 1)
		{
			ReportError("Could not set certificate store location", false);
			Close();
			return false;
		}

		SSL_CTX_set_verify((SSL_CTX*)m_context, SSL_VERIFY_PEER, nullptr);
	}

	m_session = SSL_new((SSL_CTX*)m_context);
	if (!m_session)
	{
		ReportError("Could not create TLS session", false);
		Close();
		return false;
	}

	if (!m_cipher.Empty() && !SSL_set_cipher_list((SSL*)m_session, m_cipher))
	{
		ReportError("Could not select cipher for TLS", false);
		Close();
		return false;
	}

	if (m_isClient && m_host && !SSL_set_tlsext_host_name((SSL*)m_session, m_host))
	{
		ReportError("Could not set host name for TLS");
		Close();
		return false;
	}

	if (!SSL_set_fd((SSL*)m_session, (int)m_socket))
	{
		ReportError("Could not set the file descriptor for TLS");
		Close();
		return false;
	}

	int error_code = m_isClient ? SSL_connect((SSL*)m_session) : SSL_accept((SSL*)m_session);
	if (error_code < 1)
	{
		long verifyRes = SSL_get_verify_result((SSL*)m_session);
		if (verifyRes != X509_V_OK)
		{
			PrintError(BString<1024>("TLS certificate verification failed for %s: %s."
				" For more info visit http://nzbget.net/certificate-verification",
				*m_host, X509_verify_cert_error_string(verifyRes)));
		}
		else
		{
			ReportError(BString<1024>("TLS handshake failed for %s", *m_host));
		}
		Close();
		return false;
	}

	if (m_isClient && !m_certStore.Empty() && !ValidateCert())
	{
		Close();
		return false;
	}

	m_connected = true;
	return true;
#endif /* HAVE_OPENSSL */
}

bool TlsSocket::ValidateCert()
{
#ifdef HAVE_LIBGNUTLS
#if	GNUTLS_VERSION_NUMBER >= 0x030104
#if	GNUTLS_VERSION_NUMBER >= 0x030306
	if (FileSystem::DirectoryExists(m_certStore))
	{
		if (gnutls_certificate_set_x509_trust_dir((gnutls_certificate_credentials_t)m_context, m_certStore, GNUTLS_X509_FMT_PEM) < 0)
		{
			ReportError("Could not set certificate store location");
			return false;
		}
	}
	else
#endif
	{
		if (gnutls_certificate_set_x509_trust_file((gnutls_certificate_credentials_t)m_context, m_certStore, GNUTLS_X509_FMT_PEM) < 0)
		{
			ReportError("Could not set certificate store location");
			return false;
		}
	}

	unsigned int status = 0;
	if (gnutls_certificate_verify_peers3((gnutls_session_t)m_session, m_host, &status) != 0 ||
		gnutls_certificate_type_get((gnutls_session_t)m_session) != GNUTLS_CRT_X509)
	{
		ReportError("Could not verify TLS certificate");
		return false;
	}

	if (status != 0)
	{
		if (status & GNUTLS_CERT_UNEXPECTED_OWNER)
		{
			// Extracting hostname from the certificate
			unsigned int cert_list_size = 0;
			const gnutls_datum_t* cert_list = gnutls_certificate_get_peers((gnutls_session_t)m_session, &cert_list_size);
			if (cert_list_size > 0)
			{
				gnutls_x509_crt_t cert;
				gnutls_x509_crt_init(&cert);
				gnutls_x509_crt_import(cert, &cert_list[0], GNUTLS_X509_FMT_DER);
				char dn[256];
				size_t size = sizeof(dn);
				if (gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_X520_COMMON_NAME, 0, 0, dn, &size) == 0)
				{
					PrintError(BString<1024>("TLS certificate verification failed for %s: certificate hostname mismatch (%s)."
						" For more info visit http://nzbget.net/certificate-verification", *m_host, dn));
					gnutls_x509_crt_deinit(cert);
					return false;
				}
				gnutls_x509_crt_deinit(cert);
			}
		}

		gnutls_datum_t msgdata;
		if (gnutls_certificate_verification_status_print(status, GNUTLS_CRT_X509, &msgdata, 0) == 0)
		{
			PrintError(BString<1024>("TLS certificate verification failed for %s: %s."
				" For more info visit http://nzbget.net/certificate-verification", *m_host, msgdata.data));
			gnutls_free(&msgdata);
		}
		else
		{
			ReportError(BString<1024>("TLS certificate verification failed for %s."
				" For more info visit http://nzbget.net/certificate-verification", *m_host));
		}
		return false;
	}
#endif
	return true;
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
	// verify a server certificate was presented during the negotiation
	X509* cert = SSL_get_peer_certificate((SSL*)m_session);
	if (!cert)
	{
		PrintError(BString<1024>("TLS certificate verification failed for %s: no certificate provided by server."
			" For more info visit http://nzbget.net/certificate-verification", *m_host));
		return false;
	}

#ifdef HAVE_X509_CHECK_HOST
	// hostname verification
	if (!m_host.Empty() && X509_check_host(cert, m_host, m_host.Length(), 0, nullptr) != 1)
	{
		const unsigned char* certHost = nullptr;
        // Find the position of the CN field in the Subject field of the certificate
        int common_name_loc = X509_NAME_get_index_by_NID(X509_get_subject_name(cert), NID_commonName, -1);
        if (common_name_loc >= 0)
		{
			// Extract the CN field
			X509_NAME_ENTRY* common_name_entry = X509_NAME_get_entry(X509_get_subject_name(cert), common_name_loc);
			if (common_name_entry != nullptr)
			{
				// Convert the CN field to a C string
				ASN1_STRING* common_name_asn1 = X509_NAME_ENTRY_get_data(common_name_entry);
				if (common_name_asn1 != nullptr)
				{
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
					certHost = ASN1_STRING_get0_data(common_name_asn1);
#else
					certHost = ASN1_STRING_data(common_name_asn1);
#endif
				}
			}
        }

		PrintError(BString<1024>("TLS certificate verification failed for %s: certificate hostname mismatch (%s)."
			" For more info visit http://nzbget.net/certificate-verification", *m_host, certHost));
		X509_free(cert);
		return false;
	}
#endif

	X509_free(cert);
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

		m_session = nullptr;
	}

	if (m_context)
	{
#ifdef HAVE_LIBGNUTLS
		gnutls_certificate_free_credentials((gnutls_certificate_credentials_t)m_context);
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
		SSL_CTX_free((SSL_CTX*)m_context);
#endif /* HAVE_OPENSSL */

		m_context = nullptr;
	}
}

int TlsSocket::Send(const char* buffer, int size)
{
#ifdef HAVE_LIBGNUTLS
	m_retCode = gnutls_record_send((gnutls_session_t)m_session, buffer, size);
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
	m_retCode = SSL_write((SSL*)m_session, buffer, size);
#endif /* HAVE_OPENSSL */

	if (m_retCode < 0)
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

	return m_retCode;
}

int TlsSocket::Recv(char* buffer, int size)
{
#ifdef HAVE_LIBGNUTLS
	m_retCode = gnutls_record_recv((gnutls_session_t)m_session, buffer, size);
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
	m_retCode = SSL_read((SSL*)m_session, buffer, size);
#endif /* HAVE_OPENSSL */

	if (m_retCode < 0)
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

	return m_retCode;
}

#endif
