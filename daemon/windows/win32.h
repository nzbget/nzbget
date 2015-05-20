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

/* win32.h - Defines and standard includes for MS Windows / Visual C++ 2005  */

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

/* Define to 1 if spinlocks are supported */
#define HAVE_SPINLOCK

#define VERSION "16.0-testing"

/* Suppress warnings */
#define _CRT_SECURE_NO_DEPRECATE

/* Suppress warnings */
#define _CRT_NONSTDC_NO_WARNINGS

#define _USE_32BIT_TIME_T

#if _WIN32_WINNT < 0x0501
#undef _WIN32_WINNT
#define _WIN32_WINNT   0x0501
#endif

#ifdef _DEBUG
// detection of memory leaks
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#ifndef SKIP_DEFAULT_WINDOWS_HEADERS
#include <windows.h>
#include <winbase.h>
#endif
