/*
 *  This file is part of nzbget
 *
 *  Based on "tls.c" from project "mpop" by Martin Lambers
 *  Original source code available on http://sourceforge.net/projects/mpop/
 *
 *  Copyright (C) 2000, 2003, 2004, 2005, 2006, 2007
 *  Martin Lambers <marlam@marlam.de>
 *
 *  Copyright (C) 2008 Andrei Prygounkov <hugbug@users.sourceforge.net>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef DISABLE_TLS

#define SKIP_DEFAULT_WINDOWS_HEADERS
#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <cstdio>
#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <strings.h>
#endif
#include <ctype.h>
#include <stdarg.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <list>

#include "nzbget.h"

#ifdef HAVE_LIBGNUTLS
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gcrypt.h>
#endif /* HAVE_LIBGNUTLS */
#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#endif /* HAVE_OPENSSL */

#ifdef HAVE_LIBIDN
# include <idna.h>
#endif

#include "TLS.h"
#include "Thread.h"

/**
 * Substitutes for xasprintf, xmalloc, xstrdup and _() to remove dependencies from gnulib
 */
#define xmalloc (char*)malloc
#define xstrdup strdup
#define _(a) (a)

char* xasprintf(const char* msg, ...)
{
	char* szResult = (char*)malloc(2048); // should be enough for all messages

	va_list ap;
	va_start(ap, msg);
	vsnprintf(szResult, 2048, msg, ap);
	szResult[2048-1] = '\0';
	va_end(ap);

	return szResult;
}
/* End Substitutes */


#ifdef HAVE_LIBGNUTLS

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
{ GCRY_THREAD_OPTION_USER, NULL,
  gcry_mutex_init, gcry_mutex_destroy,
  gcry_mutex_lock, gcry_mutex_unlock,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

#endif /* HAVE_LIBGNUTLS */

/*
 * tls_clear()
 *
 * see tls.h
 */

void tls_clear(tls_t *tls)
{
    tls->have_trust_file = 0;
    tls->is_active = 0;
}


/*
 * seed_prng()
 *
 * Seeds the OpenSSL random number generator.
 * Used error codes: TLS_ESEED
 */

#ifdef HAVE_OPENSSL
int seed_prng(char **errstr)
{
    char randfile[512];
    time_t t;
    int prn;
    int system_prn_max = 1024;
    
    /* Most systems have /dev/random or other sources of random numbers that
     * OpenSSL can use to seed itself.
     * The only system I know of where we must seed the PRNG is DOS.
     */
    if (!RAND_status())
    {
	if (!RAND_file_name(randfile, 512))
	{
	    *errstr = xasprintf(_("no environment variables RANDFILE or HOME, "
			"or filename of rand file too long"));
	    return TLS_ESEED;
	}
	if (RAND_load_file(randfile, -1) < 1)
	{
	    *errstr = xasprintf(_("%s: input error"), randfile);
	    return TLS_ESEED;
	}
	/* Seed in time. I can't think of other "random" things on DOS
	 * systems. */
	if ((t = time(NULL)) < 0)
	{
	    *errstr = xasprintf(_("cannot get system time: %s"), 
		    strerror(errno));
	    return TLS_ESEED;
	}
	RAND_seed((unsigned char *)&t, sizeof(time_t));
	/* If the RANDFILE + time is not enough, we fall back to the insecure
	 * and stupid method of seeding OpenSSLs PRNG with the systems PRNG. */
	if (!RAND_status())
	{
	    srand((unsigned int)(t % UINT_MAX));
	    while (!RAND_status() && system_prn_max > 0)
	    {
		prn = rand();
		RAND_seed(&prn, sizeof(int));
		system_prn_max--;
	    }
	}
	/* Are we happy now? */
	if (!RAND_status())
	{
	    *errstr = xasprintf(_("random file + time + pseudo randomness is "
			"not enough, giving up"));
	    return TLS_ESEED;
	}
	/* Save a rand file for later usage. We ignore errors here as we can't
	 * do anything about them. */
	(void)RAND_write_file(randfile);
    }
    return TLS_EOK;
}
#endif /* HAVE_OPENSSL */


/*
 * tls_lib_init()
 *
 * see tls.h
 */

int tls_lib_init(char **errstr)
{
#ifdef HAVE_LIBGNUTLS
	int error_code;

	g_pGCryptLibMutexes = new Mutexes();
    if ((error_code = gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_Mutex)) != 0)
    {
		*errstr = xasprintf("%s", "Could not initialize libcrypt");
		return TLS_ELIBFAILED;
    }

    if ((error_code = gnutls_global_init()) != 0)
    {
		*errstr = xasprintf("%s", gnutls_strerror(error_code));
		return TLS_ELIBFAILED;
    }

    return TLS_EOK;
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
    int e;
    
    SSL_load_error_strings();
    SSL_library_init();
    if ((e = seed_prng(errstr)) != TLS_EOK)
    {
	return e;
    }

    return TLS_EOK;
#endif /* HAVE_OPENSSL */
}


/*
 * tls_is_active()
 *
 * see tls.h
 */

int tls_is_active(tls_t *tls)
{
    return tls->is_active;
}


/*
 * tls_cert_info_new()
 */

tls_cert_info_t *tls_cert_info_new(void)
{
    tls_cert_info_t *tci;
    int i;
    
    tci = (tls_cert_info_t*)xmalloc(sizeof(tls_cert_info_t));
    for (i = 0; i < 6; i++)
    {
	tci->owner_info[i] = NULL;
	tci->issuer_info[i] = NULL;
    }

    return tci;
}


/*
 * tls_cert_info_free()
 */

void tls_cert_info_free(tls_cert_info_t *tci)
{
    int i;

    if (tci)
    {
	for (i = 0; i < 6; i++)
	{
	    free(tci->owner_info[i]);
	    free(tci->issuer_info[i]);
	}
	free(tci);
    }
}


/* 
 * asn1time_to_time_t() [OpenSSL only]
 * 
 * Convert a ASN1 time string ([YY]YYMMDDhhmm[ss](Z)) into a time_t.
 * The flag 'is_utc' indicates whether the string is in UTC or GENERALIZED
 * format. GENERALIZED means a 4 digit year.
 * In case of invalid strings or over-/underflows, 1 is returned, and the value
 * of 't' is undefined. On success, 0 is returned.
 *
 * This code uses many ideas from GnuTLS code (lib/x509/common.c).
 * The transformation of struct tm to time_t is based on code from Russ Allbery
 * (rra@stanford.edu), who wrote a mktime_utc function and placed it under 
 * public domain.
 */

#ifdef HAVE_OPENSSL
int is_leap(int year)
{
    return (((year) % 4) == 0 && (((year) % 100) != 0 || ((year) % 400) == 0));
}

int asn1time_to_time_t(const char *asn1time, int is_utc, time_t *t)
{
    size_t len;
    int i;
    size_t j;
    const char *p;
    char xx[3];
    char xxxx[5];
    const int monthdays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    struct tm tm;
    
    len = strlen(asn1time);
    if ((is_utc && len < 10) || (!is_utc && len < 12))
    {
	goto error_exit;
    }
    for (j = 0; j < len - 1; j++)
    {
	if (!isdigit((unsigned char)asn1time[j]))
	{
	    goto error_exit;
	}
    }
    
    xx[2] = '\0';
    xxxx[4] = '\0';
    p = asn1time;
    if (is_utc)
    {
	strncpy(xx, p, 2);
	tm.tm_year = atoi(xx);
	tm.tm_year += (tm.tm_year > 49) ? 1900 : 2000;
	p += 2;
    }
    else
    {
	strncpy(xxxx, p, 4);
	tm.tm_year = atoi(xxxx);
	p += 4;
    }
    strncpy(xx, p, 2);
    tm.tm_mon = atoi(xx) - 1;
    p += 2;
    strncpy(xx, p, 2);
    tm.tm_mday = atoi(xx);
    p += 2;
    strncpy(xx, p, 2);
    tm.tm_hour = atoi(xx);
    p += 2;
    strncpy(xx, p, 2);
    tm.tm_min = atoi(xx);
    p += 2;
    if (isdigit((unsigned char)(*p)))
    {
	strncpy(xx, p, 2);
	tm.tm_sec = atoi(xx);
    }
    else
    {
	tm.tm_sec = 0;
    }
    
    /* basic check for 32 bit time_t overflows. */
    if (sizeof(time_t) <= 4 && tm.tm_year >= 2038)
    {
	goto error_exit;
    }
    if (tm.tm_year < 1970 || tm.tm_mon < 0 || tm.tm_mon > 11)
    {
	goto error_exit;
    }
    *t = 0;
    for (i = 1970; i < tm.tm_year; i++)
    {
	*t += 365 + (is_leap(i) ? 1 : 0);
    }
    for (i = 0; i < tm.tm_mon; i++)
    {
	*t += monthdays[i];
    }
    if (tm.tm_mon > 1 && is_leap(tm.tm_year))
    {
	*t += 1;
    }
    *t = 24 * (*t + tm.tm_mday - 1) + tm.tm_hour;
    *t = 60 * (*t) + tm.tm_min;
    *t = 60 * (*t) + tm.tm_sec;
    
    return 0;
    
error_exit:
    return 1;
}
#endif /* HAVE_OPENSSL */


/*
 * tls_cert_info_get()
 *
 * see tls.h
 */

int tls_cert_info_get(tls_t *tls, tls_cert_info_t *tci, char **errstr)
{
#ifdef HAVE_LIBGNUTLS
    const gnutls_datum_t *cert_list;
    unsigned int cert_list_size;
    gnutls_x509_crt_t cert;
    size_t size;
    const char *oid[6] = { GNUTLS_OID_X520_COMMON_NAME,
	GNUTLS_OID_X520_ORGANIZATION_NAME,
	GNUTLS_OID_X520_ORGANIZATIONAL_UNIT_NAME,
	GNUTLS_OID_X520_LOCALITY_NAME,
	GNUTLS_OID_X520_STATE_OR_PROVINCE_NAME,
	GNUTLS_OID_X520_COUNTRY_NAME };
    int i;	
    int e;
    char *p;
    const char *errmsg;
    
    errmsg = _("cannot get TLS certificate info");
    if (!(cert_list = 
		gnutls_certificate_get_peers(tls->session, &cert_list_size))
	    || cert_list_size == 0)
    {
	*errstr = xasprintf(_("%s: no certificate was found"), errmsg);
	return TLS_ECERT;
    }
    if (gnutls_x509_crt_init(&cert) != 0)
    {
	*errstr = xasprintf(_("%s: cannot initialize certificate structure"),
		errmsg);
	return TLS_ECERT;
    }
    if (gnutls_x509_crt_import(cert, &cert_list[0], GNUTLS_X509_FMT_DER) != 0)
    {
	*errstr = xasprintf(_("%s: error parsing certificate"), errmsg);
	gnutls_x509_crt_deinit(cert);
	return TLS_ECERT;
    }

    /* certificate information */
    size = 20;
    if (gnutls_x509_crt_get_fingerprint(cert, GNUTLS_DIG_SHA, 
		tci->sha1_fingerprint, &size) != 0)
    {
	*errstr = xasprintf(_("%s: error getting SHA1 fingerprint"), errmsg);
	gnutls_x509_crt_deinit(cert);
	return TLS_ECERT;
    }
    size = 16;
    if (gnutls_x509_crt_get_fingerprint(cert, GNUTLS_DIG_MD5, 
		tci->md5_fingerprint, &size) != 0)
    {
	*errstr = xasprintf(_("%s: error getting MD5 fingerprint"), errmsg);
	gnutls_x509_crt_deinit(cert);
	return TLS_ECERT;
    }
    if ((tci->activation_time = gnutls_x509_crt_get_activation_time(cert)) < 0)
    {
	*errstr = xasprintf(_("%s: cannot get activation time"), errmsg);
	gnutls_x509_crt_deinit(cert);
	return TLS_ECERT;
    }
    if ((tci->expiration_time = gnutls_x509_crt_get_expiration_time(cert)) < 0)
    {
	*errstr = xasprintf(_("%s: cannot get expiration time"), errmsg);
	gnutls_x509_crt_deinit(cert);
	return TLS_ECERT;
    }

    /* owner information */
    for (i = 0; i < 6; i++)
    {
	size = 0;
	e = gnutls_x509_crt_get_dn_by_oid(cert, oid[i], 0, 0, NULL, &size);
	if (e == GNUTLS_E_SHORT_MEMORY_BUFFER)
	{
	    p = xmalloc(size);
	    e = gnutls_x509_crt_get_dn_by_oid(cert, oid[i], 0, 0, p, &size);
	    if (e == 0)
	    {
		tci->owner_info[i] = p;
	    }
	    else
	    {
		free(p);
	    }
	}
    }

    /* issuer information */
    for (i = 0; i < 6; i++)
    {
	size = 0;
	e = gnutls_x509_crt_get_issuer_dn_by_oid(
		cert, oid[i], 0, 0, NULL, &size);
	if (e == GNUTLS_E_SHORT_MEMORY_BUFFER)
	{
	    p = xmalloc(size);
	    e = gnutls_x509_crt_get_issuer_dn_by_oid(
		    cert, oid[i], 0, 0, p, &size);
	    if (e == 0)
	    {
		tci->issuer_info[i] = p;
	    }
	    else
	    {
		free(p);
	    }
	}
    }
    
    gnutls_x509_crt_deinit(cert);
    return TLS_EOK;
#endif /* HAVE_LIBGNUTLS */
    
#ifdef HAVE_OPENSSL
    X509 *x509cert;
    X509_NAME *x509_subject;
    X509_NAME *x509_issuer;
    ASN1_TIME *asn1time;
    int nid[6] = { NID_commonName,
	NID_organizationName, 
	NID_organizationalUnitName,
	NID_localityName,
	NID_stateOrProvinceName,
	NID_countryName };
    int size;
    unsigned int usize;
    char *p;
    int i;
    const char *errmsg;
    
    errmsg = _("cannot get TLS certificate info");
    if (!(x509cert = SSL_get_peer_certificate(tls->ssl)))
    {
	*errstr = xasprintf(_("%s: no certificate was found"), errmsg);
	return TLS_ECERT;
    }
    if (!(x509_subject = X509_get_subject_name(x509cert)))
    {
	*errstr = xasprintf(_("%s: cannot get certificate subject"), errmsg);
	X509_free(x509cert);
	return TLS_ECERT;
    }
    if (!(x509_issuer = X509_get_issuer_name(x509cert)))
    {
	*errstr = xasprintf(_("%s: cannot get certificate issuer"), errmsg);
	X509_free(x509cert);
	return TLS_ECERT;
    }

    /* certificate information */
    usize = 20;
    if (!X509_digest(x509cert, EVP_sha1(), tci->sha1_fingerprint, &usize))
    {
	*errstr = xasprintf(_("%s: error getting SHA1 fingerprint"), errmsg);
	return TLS_ECERT;
    }
    usize = 16;
    if (!X509_digest(x509cert, EVP_md5(), tci->md5_fingerprint, &usize))
    {
	*errstr = xasprintf(_("%s: error getting MD5 fingerprint"), errmsg);
	return TLS_ECERT;
    }
    asn1time = X509_get_notBefore(x509cert);
    if (asn1time_to_time_t((char *)asn1time->data, 
		(asn1time->type != V_ASN1_GENERALIZEDTIME), 
		&(tci->activation_time)) != 0)
    {
	*errstr = xasprintf(_("%s: cannot get activation time"), errmsg);
	X509_free(x509cert);
	tls_cert_info_free(tci);
	return TLS_ECERT;
    }
    asn1time = X509_get_notAfter(x509cert);
    if (asn1time_to_time_t((char *)asn1time->data, 
		(asn1time->type != V_ASN1_GENERALIZEDTIME), 
		&(tci->expiration_time)) != 0)
    {
	*errstr = xasprintf(_("%s: cannot get expiration time"), errmsg);
	X509_free(x509cert);
	tls_cert_info_free(tci);
	return TLS_ECERT;
    }

    /* owner information */
    for (i = 0; i < 6; i++)
    {
	size = X509_NAME_get_text_by_NID(x509_subject, nid[i], NULL, 0);
	size++;
	p = xmalloc((size_t)size);
	if (X509_NAME_get_text_by_NID(x509_subject, nid[i], p, size) != -1)
	{
	    tci->owner_info[i] = p;
	}
	else
	{
	    free(p);
	}
    }
    
    /* issuer information */
    for (i = 0; i < 6; i++)
    {
	size = X509_NAME_get_text_by_NID(x509_issuer, nid[i], NULL, 0);
	size++;
	p = xmalloc((size_t)size);
	if (X509_NAME_get_text_by_NID(x509_issuer, nid[i], p, size) != -1)
	{
	    tci->issuer_info[i] = p;
	}
	else
	{
	    free(p);
	}
    }
    
    X509_free(x509cert);
    return TLS_EOK;
#endif /* HAVE_OPENSSL */
}


/*
 * [OpenSSL only] hostname_match()
 * 
 * Compares the hostname with the name in the certificate. The certificate name
 * may include a wildcard as the leftmost domain component (its first two 
 * characters are "*." in this case).
 *
 * Returns 1 if the names match, 0 otherwise.
 *
 * This is the same form of matching that gnutls_x509_crt_check_hostname() from
 * GnuTLS 1.2.0 uses.
 * It conforms to RFC2595 (Using TLS with IMAP, POP3 and ACAP), section 2.4.
 * RFC2818 (HTTP over TLS), section 3.1 says that `f*.com matches foo.com'. This
 * function does not allow that.
 * RFC3207 (SMTP Service Extension for Secure SMTP over Transport Layer 
 * Security), section 4.1 says nothing more than `A SMTP client would probably
 * only want to authenticate an SMTP server whose server certificate has a
 * domain name that is the domain name that the client thought it was connecting
 * to'.
 */

#ifdef HAVE_OPENSSL
int hostname_match(const char *hostname, const char *certname)
{
    const char *cmp1, *cmp2;
    
    if (strncmp(certname, "*.", 2) == 0)
    {
	cmp1 = certname + 2;
	cmp2 = strchr(hostname, '.');
	if (!cmp2)
	{
	    return 0;
	}
	else
	{
	    cmp2++;
	}
    }
    else
    {
	cmp1 = certname;
	cmp2 = hostname;
    }

    if (*cmp1 == '\0' || *cmp2 == '\0')
    {
	return 0;
    }

    if (strcasecmp(cmp1, cmp2) != 0)
    {
	return 0;
    }

    return 1;
}
#endif /* HAVE_OPENSSL */


/* 
 * tls_check_cert()
 *
 * If the 'verify' flag is set, perform a real verification of the peer's 
 * certificate. If this succeeds, the connection can be considered secure.
 * If the 'verify' flag is not set, perform only a few sanity checks of the
 * peer's certificate. You cannot trust the connection when this succeeds.
 * Used error codes: TLS_ECERT
 */

int tls_check_cert(tls_t *tls, const char *hostname, int verify, char **errstr)
{
#ifdef HAVE_LIBGNUTLS
    int error_code;
    const char *error_msg;
    unsigned int status;
    const gnutls_datum_t *cert_list;
    unsigned int cert_list_size;
    unsigned int i;
    gnutls_x509_crt_t cert;
    time_t t1, t2;
#ifdef HAVE_LIBIDN
    char *hostname_ascii;
#endif
    
    if (verify)
    {
	error_msg = _("TLS certificate verification failed");
    }
    else
    {
	error_msg = _("TLS certificate check failed");
    }
    
    /* If 'verify' is true, this function uses the trusted CAs in the 
     * credentials structure. So you must have installed one or more CA 
     * certificates. */
    gnutls_certificate_set_verify_flags(tls->cred, 
	    GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT);
    if ((error_code = gnutls_certificate_verify_peers2(tls->session, 
		    &status)) != 0)
    {
	*errstr = xasprintf("%s: %s", error_msg, gnutls_strerror(error_code));
	return TLS_ECERT;
    }
    if (verify)
    {
	if (status & GNUTLS_CERT_SIGNER_NOT_FOUND)
	{
	    *errstr = xasprintf(
		    _("%s: the certificate hasn't got a known issuer"),
		    error_msg);
	    return TLS_ECERT;
	}
	if (status & GNUTLS_CERT_INVALID)
	{
	    *errstr = xasprintf(_("%s: the certificate is not trusted"),
		    error_msg);
	    return TLS_ECERT;
	}
    }
    if (status & GNUTLS_CERT_REVOKED)
    {
	*errstr = xasprintf(_("%s: the certificate has been revoked"),
		error_msg);
	return TLS_ECERT;
    }
    if (gnutls_certificate_type_get(tls->session) != GNUTLS_CRT_X509)
    {
	*errstr = xasprintf(_("%s: the certificate type is not X509"),
		error_msg);
	return TLS_ECERT;
    }
    if (!(cert_list = gnutls_certificate_get_peers(
		    tls->session, &cert_list_size)))
    {
	*errstr = xasprintf(_("%s: no certificate was found"), error_msg);
	return TLS_ECERT;
    }
    /* Needed to check times: */
    if ((t1 = time(NULL)) < 0)
    {
	*errstr = xasprintf("%s: cannot get system time: %s", error_msg, 
		strerror(errno));
	return TLS_ECERT;
    }
    /* Check the certificate chain. All certificates in the chain must have
     * valid activation/expiration times. The first certificate in the chain is
     * the host's certificate; it must match the hostname. */
    for (i = 0; i < cert_list_size; i++)
    {
	if (gnutls_x509_crt_init(&cert) < 0)
	{
	    *errstr = xasprintf(
		    _("%s: cannot initialize certificate structure"), 
		    error_msg);
	    return TLS_ECERT;
	}
	if (gnutls_x509_crt_import(cert, &cert_list[i], GNUTLS_X509_FMT_DER) 
		< 0)
	{
	    *errstr = xasprintf(_("%s: error parsing certificate %u of %u"), 
		    error_msg, i + 1, cert_list_size);
	    return TLS_ECERT;
	}
	/* Check hostname */
	if (i == 0)
	{
#ifdef HAVE_LIBIDN
	    if (idna_to_ascii_lz(hostname, &hostname_ascii, 0) == IDNA_SUCCESS)
	    {
    		if (!gnutls_x509_crt_check_hostname(cert, hostname_ascii)) 
    		{
    		    *errstr = xasprintf(_("%s: the certificate owner does not "
				"match hostname %s"), error_msg, hostname);
		    free(hostname_ascii);
	    	    return TLS_ECERT; 
		}
    		free(hostname_ascii);
	    }
	    else
#endif
	    if (!gnutls_x509_crt_check_hostname(cert, hostname)) 
	    {
	    	*errstr = xasprintf(_("%s: the certificate owner does not "
    			    "match hostname %s"), error_msg, hostname);
		return TLS_ECERT; 
	    }
	}
	/* Check certificate times */
	if ((t2 = gnutls_x509_crt_get_activation_time(cert)) < 0)
	{
	    *errstr = xasprintf(_("%s: cannot get activation time for "
			"certificate %u of %u"), 
		    error_msg, i + 1, cert_list_size);
	    return TLS_ECERT; 
	}
	if (t2 > t1)
	{
	    *errstr = xasprintf(
		    _("%s: certificate %u of %u is not yet activated"),
		    error_msg, i + 1, cert_list_size);
	    return TLS_ECERT;
	}
	if ((t2 = gnutls_x509_crt_get_expiration_time(cert)) < 0)
	{
	    *errstr = xasprintf(_("%s: cannot get expiration time for "
			"certificate %u of %u"), 
		    error_msg, i + 1, cert_list_size);
	    return TLS_ECERT;
	}
	if (t2 < t1)
	{
	    *errstr = xasprintf(_("%s: certificate %u of %u has expired"), 
		    error_msg, i + 1, cert_list_size);
	    return TLS_ECERT;
	}
	gnutls_x509_crt_deinit(cert);
    }

    return TLS_EOK;
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
    X509 *x509cert;
    long status;
    const char *error_msg;
    int i;
    /* hostname in ASCII format: */
    char *hostname_ascii;
    /* needed to get the common name: */
    X509_NAME *x509_subject;
    char *buf;
    int bufsize;
    /* needed to get the DNS subjectAltNames: */
    STACK *subj_alt_names;
    int subj_alt_names_count;
    GENERAL_NAME *subj_alt_name;
    /* did we find a name matching hostname? */
    int match_found;
    

    if (verify)
    {
	error_msg = _("TLS certificate verification failed");
    }
    else
    {
	error_msg = _("TLS certificate check failed");
    }

    /* Get certificate */
    if (!(x509cert = SSL_get_peer_certificate(tls->ssl)))
    {
	*errstr = xasprintf(_("%s: no certificate was sent"), error_msg);
	return TLS_ECERT;
    }
    
    /* Get result of OpenSSL's default verify function */
    if ((status = SSL_get_verify_result(tls->ssl)) != X509_V_OK)
    {
	if (verify || (status != X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY
		    && status != X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT
		    && status != X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN))
	{
	    *errstr = xasprintf("%s: %s", error_msg, 
		    X509_verify_cert_error_string(status));
	    X509_free(x509cert);
	    return TLS_ECERT;
	}
    }

    /* Check if 'hostname' matches the one of the subjectAltName extensions of
     * type DNS or the Common Name (CN). */
    
#ifdef HAVE_LIBIDN
    if (idna_to_ascii_lz(hostname, &hostname_ascii, 0) != IDNA_SUCCESS)
    {
	hostname_ascii = xstrdup(hostname);
    }
#else
    hostname_ascii = xstrdup(hostname);
#endif
    
    /* Try the DNS subjectAltNames. */
    match_found = 0;
    if ((subj_alt_names =
		(STACK*)X509_get_ext_d2i(x509cert, NID_subject_alt_name, NULL, NULL)))
    {
	subj_alt_names_count = sk_GENERAL_NAME_num(subj_alt_names);
	for (i = 0; i < subj_alt_names_count; i++) 
	{
	    subj_alt_name = sk_GENERAL_NAME_value(subj_alt_names, i);
	    if (subj_alt_name->type == GEN_DNS) 
	    {
		if ((match_found = hostname_match(hostname_ascii, 
				(char *)(subj_alt_name->d.ia5->data))))
		{
		    break;
		}
	    }
	}
    }
    if (!match_found)
    {
	/* Try the common name */    
	if (!(x509_subject = X509_get_subject_name(x509cert)))
	{
	    *errstr = xasprintf(_("%s: cannot get certificate subject"),
		    error_msg);
	    X509_free(x509cert);
	    return TLS_ECERT;
	}
	bufsize = X509_NAME_get_text_by_NID(x509_subject, NID_commonName, 
		NULL, 0);
	bufsize++;
	buf = xmalloc((size_t)bufsize);
	if (X509_NAME_get_text_by_NID(x509_subject, NID_commonName, 
		    buf, bufsize) == -1)
	{
	    *errstr = xasprintf(_("%s: cannot get certificate common name"),
		    error_msg);
	    X509_free(x509cert);
	    free(buf);
	    return TLS_ECERT;
	}
	match_found = hostname_match(hostname_ascii, buf);
	free(buf);
    }
    X509_free(x509cert);
    free(hostname_ascii);

    if (!match_found)
    {
	*errstr = xasprintf(
		_("%s: the certificate owner does not match hostname %s"), 
		error_msg, hostname);
	return TLS_ECERT;
    }
    
    return TLS_EOK;
#endif /* HAVE_OPENSSL */
}


/*
 * tls_init()
 *
 * see tls.h
 */

int tls_init(tls_t *tls, const char *key_file, const char *cert_file, 
	const char *trust_file, int force_sslv3, char **errstr)
{
#ifdef HAVE_LIBGNUTLS
    int error_code;
    
    if ((error_code = gnutls_init(&tls->session, GNUTLS_CLIENT)) != 0)
    {
	*errstr = xasprintf(_("cannot initialize TLS Session: %s"), 
		gnutls_strerror(error_code));
	return TLS_ELIBFAILED;
    }
    if ((error_code = gnutls_set_default_priority(tls->session)) != 0)
    {
	*errstr = xasprintf(_("cannot set priorities on TLS Session: %s"),
		gnutls_strerror(error_code));
	gnutls_deinit(tls->session);
	return TLS_ELIBFAILED;
    }
    if (force_sslv3)
    {
	const int force_sslv3_proto_prio[2] = { GNUTLS_SSL3, 0 };
	if ((error_code = gnutls_protocol_set_priority(tls->session, 
			force_sslv3_proto_prio)) != 0)
	{
	    *errstr = xasprintf(_("cannot force SSLv3: %s"),
	    	    gnutls_strerror(error_code));
	    gnutls_deinit(tls->session);
	    return TLS_ELIBFAILED;
	}
    }
    if ((error_code = gnutls_certificate_allocate_credentials(&tls->cred)) < 0)
    {
	*errstr = xasprintf(
		_("cannot allocate certificate for TLS Session: %s"),
		gnutls_strerror(error_code));
	gnutls_deinit(tls->session);
	return TLS_ELIBFAILED;
    }
    if (key_file && cert_file)
    {
	if ((error_code = gnutls_certificate_set_x509_key_file(tls->cred, 
		       	cert_file, key_file, GNUTLS_X509_FMT_PEM)) < 0)
	{
	    *errstr = xasprintf(_("cannot set X509 key file %s and/or "
			"X509 cert file %s for TLS Session: %s"),
	    	    key_file, cert_file, gnutls_strerror(error_code));
	    gnutls_deinit(tls->session);
	    gnutls_certificate_free_credentials(tls->cred);
	    return TLS_EFILE;
	}
    }
    if (trust_file)
    {
	if ((error_code = gnutls_certificate_set_x509_trust_file(
			tls->cred, trust_file, GNUTLS_X509_FMT_PEM)) <= 0)
	{
	    *errstr = xasprintf(
		    _("cannot set X509 trust file %s for TLS Session: %s"),
	    	    trust_file, gnutls_strerror(error_code));
	    gnutls_deinit(tls->session);
	    gnutls_certificate_free_credentials(tls->cred);
	    return TLS_EFILE;
	}
	tls->have_trust_file = 1;
    }
    if ((error_code = gnutls_credentials_set(tls->session, 
		    GNUTLS_CRD_CERTIFICATE, tls->cred)) < 0)
    {
	*errstr = xasprintf(_("cannot set credentials for TLS Session: %s"),
		gnutls_strerror(error_code));
	gnutls_deinit(tls->session);
	gnutls_certificate_free_credentials(tls->cred);
	return TLS_ELIBFAILED;
    }
    return TLS_EOK;
    
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
    
    SSL_METHOD *ssl_method = NULL;
    
    ssl_method = force_sslv3 ? SSLv3_client_method() : SSLv23_client_method();
    if (!ssl_method)
    {
	*errstr = xasprintf(_("cannot set TLS method"));
	return TLS_ELIBFAILED;
    }
    if (!(tls->ssl_ctx = SSL_CTX_new(ssl_method)))
    {
	*errstr = xasprintf(_("cannot create TLS context: %s"),
		ERR_error_string(ERR_get_error(), NULL));
	return TLS_ELIBFAILED;
    }
    /* SSLv2 has known flaws. Disable it. */
    (void)SSL_CTX_set_options(tls->ssl_ctx, SSL_OP_NO_SSLv2);
    if (key_file && cert_file)
    {
	if (SSL_CTX_use_PrivateKey_file(
		    tls->ssl_ctx, key_file, SSL_FILETYPE_PEM) != 1)
	{
	    *errstr = xasprintf(_("cannot load key file %s: %s"), 
		    key_file, ERR_error_string(ERR_get_error(), NULL));
	    SSL_CTX_free(tls->ssl_ctx);
	    tls->ssl_ctx = NULL;
	    return TLS_EFILE;
	}
	if (SSL_CTX_use_certificate_chain_file(tls->ssl_ctx, cert_file) != 1)
	{
	    *errstr = xasprintf(_("cannot load certificate file %s: %s"), 
		    cert_file, ERR_error_string(ERR_get_error(), NULL));
	    SSL_CTX_free(tls->ssl_ctx);
	    tls->ssl_ctx = NULL;
	    return TLS_EFILE;
	}
    }
    if (trust_file)
    {
	if (SSL_CTX_load_verify_locations(tls->ssl_ctx, trust_file, NULL) != 1)
	{
	    *errstr = xasprintf(_("cannot load trust file %s: %s"), 
		    trust_file, ERR_error_string(ERR_get_error(), NULL));
	    SSL_CTX_free(tls->ssl_ctx);
	    tls->ssl_ctx = NULL;
	    return TLS_EFILE;
	}
	tls->have_trust_file = 1;
    }
    if (!(tls->ssl = SSL_new(tls->ssl_ctx)))
    {
	*errstr = xasprintf(_("cannot create a TLS structure: %s"), 
		ERR_error_string(ERR_get_error(), NULL));
	SSL_CTX_free(tls->ssl_ctx);
	tls->ssl_ctx = NULL;
	return TLS_ELIBFAILED;
    }
    return TLS_EOK;

#endif /* HAVE_OPENSSL */
}


/*
 * openssl_io_error()
 *
 * Used only internally by the OpenSSL code.
 * 
 * Construct an error line according to 'error_code' (which originates from an
 * SSL_read(), SSL_write() or SSL_connect() operation) and 'error_code2' (which
 * originates from an SSL_get_error() call with 'error_code' as its argument).
 * The line will read: "error_string: error_reason". 'error_string' is given by
 * the calling function, this function finds out 'error_reason'.
 * The resulting string will be returned in an allocated string.
 * OpenSSL error strings are max 120 characters long according to
 * ERR_error_string(3). 
 */

#ifdef HAVE_OPENSSL
char *openssl_io_error(int error_code, int error_code2, 
	const char *error_string)
{
    unsigned long error_code3;
    const char *error_reason;
    
    switch (error_code2)
    {
	case SSL_ERROR_SYSCALL:
	    error_code3 = ERR_get_error();
    	    if (error_code3 == 0)
	    {
		if (error_code == 0)
		{
		    error_reason = _("a protocol violating EOF occured");
		}
		else if (error_code == -1)
		{
		    error_reason = strerror(errno);
		}
		else
		{
		    error_reason = _("unknown error");
		}
	    }
	    else
	    {
		error_reason = ERR_error_string(error_code3, NULL);
	    }
	    break;
	    
	case SSL_ERROR_ZERO_RETURN:
	    error_reason = _("the connection was closed unexpectedly");
	    break;

	case SSL_ERROR_SSL:
	    error_reason = ERR_error_string(ERR_get_error(), NULL);
	    break;
	    
	case SSL_ERROR_WANT_READ:
	case SSL_ERROR_WANT_WRITE:
    	    error_reason = _("the operation timed out");
	    break;

	default:
	    /* probably SSL_ERROR_NONE */
    	    error_reason = _("unknown error");
	    break;
    }
    return xasprintf("%s: %s", error_string, error_reason);
}
#endif /* HAVE_OPENSSL */


/*
 * tls_start()
 *
 * see tls.h
 */

int tls_start(tls_t *tls, int fd, const char *hostname, int no_certcheck, 
	tls_cert_info_t *tci, char **errstr)
{
#ifdef HAVE_LIBGNUTLS
    int error_code;
    
    gnutls_transport_set_ptr(tls->session, (gnutls_transport_ptr_t)(size_t)fd);
    if ((error_code = gnutls_handshake(tls->session)) < 0)
    {
	if (error_code == GNUTLS_E_INTERRUPTED)
	{
	    *errstr = xasprintf(_("operation aborted"));
	}
	else if (error_code == GNUTLS_E_AGAIN)
	{
	    /* This error message makes more sense than what
	     * gnutls_strerror() would return. */
	    *errstr = xasprintf(_("TLS handshake failed: %s"), 
		    _("the operation timed out"));
	}
	else
	{
	    *errstr = xasprintf(_("TLS handshake failed: %s"), 
	    	    gnutls_strerror(error_code));
	}
	gnutls_deinit(tls->session);
	gnutls_certificate_free_credentials(tls->cred);
	return TLS_EHANDSHAKE;
    }
    if (tci)
    {
	if ((error_code = tls_cert_info_get(tls, tci, errstr)) != TLS_EOK)
	{
	    gnutls_deinit(tls->session);
	    gnutls_certificate_free_credentials(tls->cred);
	    return error_code;
	}
    }
    if (!no_certcheck)
    {
	if ((error_code = tls_check_cert(tls, hostname, 
			tls->have_trust_file, errstr)) != TLS_EOK)
	{
	    gnutls_deinit(tls->session);
	    gnutls_certificate_free_credentials(tls->cred);
	    return error_code;
	}
    }    
    tls->is_active = 1;
    return TLS_EOK;
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
    int error_code;
    
    if (!SSL_set_fd(tls->ssl, fd))
    {
	*errstr = xasprintf(_("cannot set the file descriptor for TLS: %s"), 
		ERR_error_string(ERR_get_error(), NULL));
	SSL_free(tls->ssl);
	SSL_CTX_free(tls->ssl_ctx);
	return TLS_ELIBFAILED;
    }
    if ((error_code = SSL_connect(tls->ssl)) < 1)
    {
	if (errno == EINTR 
		&& (SSL_get_error(tls->ssl, error_code) == SSL_ERROR_WANT_READ
		    || SSL_get_error(tls->ssl, error_code) 
		    == SSL_ERROR_WANT_WRITE))
	{
	    *errstr = xasprintf(_("operation aborted"));
	}
	else
	{
	    *errstr = openssl_io_error(error_code, 
		    SSL_get_error(tls->ssl, error_code), 
		    _("TLS handshake failed"));
	}
	SSL_free(tls->ssl);
	SSL_CTX_free(tls->ssl_ctx);
	return TLS_EIO;
    }
    if (tci)
    {
	if ((error_code = tls_cert_info_get(tls, tci, errstr)) != TLS_EOK)
	{
	    SSL_free(tls->ssl);
	    SSL_CTX_free(tls->ssl_ctx);
	    return error_code;
	}
    }
    if (!no_certcheck)
    {
	if ((error_code = tls_check_cert(tls, hostname, tls->have_trust_file,
			errstr)) != TLS_EOK)
	{
	    SSL_free(tls->ssl);
	    SSL_CTX_free(tls->ssl_ctx);
	    return error_code;
	}
    }
    tls->is_active = 1;
    return TLS_EOK;
#endif /* HAVE_OPENSSL */
}


/*
 * tls_getbuf()
 *
 * see tls.h
 */

int tls_getbuf(tls_t *tls, char* s, size_t len, size_t* readlen, char **errstr)
{
#ifdef HAVE_LIBGNUTLS
	*readlen = 0;
    ssize_t ret;
    
    ret = gnutls_record_recv(tls->session, s, len);
    if (ret >= 0)
    {
		*readlen = ret;
		return TLS_EOK;
    }
    else
    {
		if (ret == GNUTLS_E_INTERRUPTED)
		{
			*errstr = xasprintf(_("operation aborted"));
		}
		else if (ret == GNUTLS_E_AGAIN)
		{
			/* This error message makes more sense than what
			 * gnutls_strerror() would return. */
			*errstr = xasprintf(_("cannot read from TLS connection: %s"), 
	    			_("the operation timed out"));
		}
		else
		{
			*errstr = xasprintf(_("cannot read from TLS connection: %s"), 
	    			gnutls_strerror((int)ret));
		}
		return TLS_EIO;
    }
    
#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
	*readlen = 0;
    
    int error_code;
    int error_code2;

	int ret = SSL_read(tls->ssl, s, len);
    
    if ((error_code = ret) < 1)
    {
		if ((error_code2 = SSL_get_error(tls->ssl, error_code)) 
			== SSL_ERROR_NONE)
		{
			return TLS_EOK;
		}
		else
		{
			if (errno == EINTR &&
	    			(SSL_get_error(tls->ssl, error_code) == SSL_ERROR_WANT_READ
				 || SSL_get_error(tls->ssl, error_code) 
				 == SSL_ERROR_WANT_WRITE))
			{
				*errstr = xasprintf(_("operation aborted"));
			}
			else
   			{
    			*errstr = openssl_io_error(error_code, error_code2, 
    				_("cannot read from TLS connection"));
   			}
			return TLS_EIO;
		}
    }
    else
    {
		*readlen = ret;
		return TLS_EOK;
    }
    
#endif /* HAVE_OPENSSL */
}


/*
 * tls_putbuf()
 *
 * see tls.h
 */

int tls_putbuf(tls_t *tls, const char *s, size_t len, char **errstr)
{
#ifdef HAVE_LIBGNUTLS
    
    ssize_t ret;

    if (len < 1)
    {
	/* nothing to be done */
	return TLS_EOK;
    }
    
    if ((ret = gnutls_record_send(tls->session, s, len)) < 0)
    {
	if (ret == GNUTLS_E_INTERRUPTED)
	{
	    *errstr = xasprintf(_("operation aborted"));
	}
	else if (ret == GNUTLS_E_AGAIN)
	{
	    /* This error message makes more sense than what
	     * gnutls_strerror() would return. */
	    *errstr = xasprintf(_("cannot write to TLS connection: %s"), 
		    _("the operation timed out"));
	}
	else
	{
	    *errstr = xasprintf(_("cannot write to TLS connection: %s"), 
	    	    gnutls_strerror((int)ret));
	}
	return TLS_EIO;
    }
    else if ((size_t)ret == len)
    {
	return TLS_EOK;
    }
    else /* 0 <= error_code < len */
    {
	*errstr = xasprintf(_("cannot write to TLS connection: %s"), 
		_("unknown error"));
	return TLS_EIO;
    }    

#endif /* HAVE_LIBGNUTLS */

#ifdef HAVE_OPENSSL
    
    int error_code;

    if (len < 1)
    {
	/* nothing to be done */
	return TLS_EOK;
    }
    
    if ((error_code = SSL_write(tls->ssl, s, (int)len)) != (int)len)
    {
	if (errno == EINTR 
		&& ((SSL_get_error(tls->ssl, error_code) == SSL_ERROR_WANT_READ
		       	|| SSL_get_error(tls->ssl, error_code) 
			== SSL_ERROR_WANT_WRITE)))
	{
	    *errstr = xasprintf(_("operation aborted"));	    
	}
	else
	{
	    *errstr = openssl_io_error(error_code,
	    	    SSL_get_error(tls->ssl, error_code),
	    	    _("cannot write to TLS connection"));
	}
	return TLS_EIO;
    }

    return TLS_EOK;

#endif /* HAVE_OPENSSL */
}


/*
 * tls_close()
 *
 * see tls.h
 */

void tls_close(tls_t *tls)
{
    if (tls->is_active)
    {
#ifdef HAVE_LIBGNUTLS
	gnutls_bye(tls->session, GNUTLS_SHUT_WR);
	gnutls_deinit(tls->session);
	gnutls_certificate_free_credentials(tls->cred);
#endif /* HAVE_LIBGNUTLS */
#ifdef HAVE_OPENSSL
 	SSL_shutdown(tls->ssl);
	SSL_free(tls->ssl);
	SSL_CTX_free(tls->ssl_ctx);
#endif /* HAVE_OPENSSL */
    }
    tls_clear(tls);
}


/*
 * tls_lib_deinit()
 *
 * see tls.h
 */

void tls_lib_deinit(void)
{
#ifdef HAVE_LIBGNUTLS
	gnutls_global_deinit();

	// fixing memory leak in gcryptlib
	for (Mutexes::iterator it = g_pGCryptLibMutexes->begin(); it != g_pGCryptLibMutexes->end(); it++)
	{
		delete *it;
	}
	delete g_pGCryptLibMutexes;
#endif /* HAVE_LIBGNUTLS */
}

#endif
