/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef NZBGET_H
#define NZBGET_H

/***************** DEFINES FOR WINDOWS *****************/
#ifdef WIN32

/* Define to 1 to not use curses */
//#define DISABLE_CURSES

/* Define to 1 to disable smart par-verification and restoration */
//#define DISABLE_PARCHECK

/* Define to 1 to disable TLS/SSL-support. */
//#define DISABLE_TLS

#ifndef DISABLE_TLS
/* Define to 1 to use OpenSSL library for TLS/SSL-support */
#define HAVE_OPENSSL
/* Define to 1 to use GnuTLS library for TLS/SSL-support */
//#define HAVE_LIBGNUTLS
#endif

/* Define to the name of macro which returns the name of function being
compiled */
#define FUNCTION_MACRO_NAME __FUNCTION__

/* Define to 1 if ctime_r takes 2 arguments */
#undef HAVE_CTIME_R_2

/* Define to 1 if ctime_r takes 3 arguments */
#define HAVE_CTIME_R_3

/* Define to 1 if getopt_long is supported */
#undef HAVE_GETOPT_LONG

/* Define to 1 if variadic macros are supported */
#define HAVE_VARIADIC_MACROS

/* Define to 1 if libpar2 supports cancelling (needs a special patch) */
#define HAVE_PAR2_CANCEL

/* Define to 1 if function GetAddrInfo is supported */
#define HAVE_GETADDRINFO

/* Determine what socket length (socklen_t) data type is */
#define SOCKLEN_T socklen_t

/* Define to 1 if you have the <regex.h> header file. */
#define HAVE_REGEX_H 1

/* Suppress warnings */
#define _CRT_SECURE_NO_DEPRECATE

/* Suppress warnings */
#define _CRT_NONSTDC_NO_WARNINGS

#define _USE_32BIT_TIME_T

#if _WIN32_WINNT < 0x0501
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

#ifdef _DEBUG
// detection of memory leaks
#define _CRTDBG_MAP_ALLOC
#endif

#pragma warning(disable:4800) // 'type' : forcing value to bool 'true' or 'false' (performance warning)
#pragma warning(disable:4267) // 'var' : conversion from 'size_t' to 'type', possible loss of data

#endif


/***************** GLOBAL INCLUDES *****************/

#ifdef WIN32

// WINDOWS INCLUDES

// Using "WIN32_LEAN_AND_MEAN" to disable including on many unneeded headers
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <winsvc.h>
#include <direct.h>
#include <shlobj.h>
#include <dbghelp.h>
#include <mmsystem.h>
#include <io.h>
#include <process.h>
#include <WinIoCtl.h>
#include <wincon.h>
#include <shellapi.h>
#include <winreg.h>

#include <comutil.h>
#import <msxml.tlb> named_guids
using namespace MSXML;

#if _MSC_VER >= 1600
#include <stdint.h>
#define HAVE_STDINT_H
#endif

#ifdef _DEBUG
#include <crtdbg.h>
#endif

#else

// POSIX INCLUDES

#include "config.h"

#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <pwd.h>
#include <dirent.h>

#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlerror.h>
#include <libxml/entities.h>

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#endif

#endif /* POSIX INCLUDES */

// COMMON INCLUDES

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>

#include <string>
#include <vector>
#include <deque>
#include <list>
#include <set>
#include <map>
#include <iterator>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <memory>

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
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/pem.h>
#endif /* HAVE_OPENSSL */

#ifdef HAVE_REGEX_H
#include <regex.h>
#endif

#ifndef DISABLE_GZIP
#include <zlib.h>
#endif

#ifndef DISABLE_PARCHECK
#include <assert.h>
#include <iomanip>
#include <cassert>
#ifdef HAVE_MEMORY_H
# include <memory.h>
#endif
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#endif /* NOT DISABLE_PARCHECK */


/***************** GLOBAL FUNCTION AND CONST OVERRIDES *****************/

#ifdef WIN32

// WINDOWS

#define snprintf _snprintf
#ifndef strdup
#define strdup _strdup
#endif
#define fdopen _fdopen
#define ctime_r(timep, buf, bufsize) ctime_s(buf, bufsize, timep)
#define gmtime_r(time, tm) gmtime_s(tm, time)
#define strtok_r(str, delim, saveptr) strtok_s(str, delim, saveptr)
#define strerror_r(errnum, buffer, size) strerror_s(buffer, size, errnum)
#define mkdir(dir, flags) _mkdir(dir)
#define rmdir _rmdir
#define strcasecmp(a, b) _stricmp(a, b)
#define strncasecmp(a, b, c) _strnicmp(a, b, c)
#define ssize_t SSIZE_T
#define __S_ISTYPE(mode, mask) (((mode) & _S_IFMT) == (mask))
#define S_ISDIR(mode) __S_ISTYPE((mode), _S_IFDIR)
#define S_ISREG(mode) __S_ISTYPE((mode), _S_IFREG)
#define S_DIRMODE nullptr
#define usleep(usec) Sleep((usec) / 1000)
#define socklen_t int
#define SHUT_WR 0x01
#define SHUT_RDWR 0x02
#define PATH_SEPARATOR '\\'
#define ALT_PATH_SEPARATOR '/'
#define LINE_ENDING "\r\n"
#define pid_t int
#define atoll _atoi64
#define fseek _fseeki64
#define ftell _ftelli64
// va_copy is available in vc2013 and onwards
#if _MSC_VER < 1800
#define va_copy(d,s) ((d) = (s))
#endif
#ifndef FSCTL_SET_SPARSE
#define FSCTL_SET_SPARSE 590020
#endif
#define FOPEN_RB "rbN"
#define FOPEN_RBP "rb+N"
#define FOPEN_WB "wbN"
#define FOPEN_AB "abN"

#ifdef DEBUG
// redefine "exit" to avoid printing memory leaks report when terminated because of wrong command line switches
#define exit(code) ExitProcess(code)
#endif

#else

// POSIX

#define closesocket(sock) close(sock)
#define SOCKET int
#define INVALID_SOCKET (-1)
#define PATH_SEPARATOR '/'
#define ALT_PATH_SEPARATOR '\\'
#define MAX_PATH 1024
#define S_DIRMODE (S_IRWXU | S_IRWXG | S_IRWXO)
#define LINE_ENDING "\n"
#define FOPEN_RB "rb"
#define FOPEN_RBP "rb+"
#define FOPEN_WB "wb"
#define FOPEN_AB "ab"
#define CHILD_WATCHDOG 1

#endif /* POSIX */

// COMMON DEFINES FOR ALL PLATFORMS
#ifndef SHUT_RDWR
#define SHUT_RDWR 2
#endif

#ifdef HAVE_STDINT_H
typedef uint8_t uint8;
typedef uint32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;
#else
typedef unsigned char uint8;
typedef signed int int32;
typedef unsigned int uint32;
typedef signed long long int64;
typedef unsigned long long uint64;
#endif

typedef unsigned char uchar;

#ifdef __GNUC__
#define PRINTF_SYNTAX(strindex) __attribute__ ((format (printf, strindex, strindex+1)))
#else
#define PRINTF_SYNTAX(strindex)
#endif

// for-range loops on pointers to std-containers (avoiding dereferencing)
template <typename T> typename std::deque<T>::iterator begin(std::deque<T>* c) { return c->begin(); }
template <typename T> typename std::deque<T>::iterator end(std::deque<T>* c) { return c->end(); }
template <typename T> typename std::vector<T>::iterator begin(std::vector<T>* c) { return c->begin(); }
template <typename T> typename std::vector<T>::iterator end(std::vector<T>* c) { return c->end(); }
template <typename T> typename std::list<T>::iterator begin(std::list<T>* c) { return c->begin(); }
template <typename T> typename std::list<T>::iterator end(std::list<T>* c) { return c->end(); }

#endif /* NZBGET_H */
