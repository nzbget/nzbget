#
#  This file is part of nzbget. See <http://nzbget.net>.
#
# Copyright (C) 2008-2021 Andrey Prygunkov <hugbug@users.sourceforge.net>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.71])
AC_INIT( nzbget, m4_esyscmd_s( git describe --tags --match "v*.*" --dirty ) )
AC_USE_SYSTEM_EXTENSIONS
AC_CONFIG_AUX_DIR(posix)
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_SRCDIR([daemon/main/nzbget.cpp])
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE

m4_include([posix/ax_cxx_compile_stdcxx.m4])

#
# Check for programs.
#
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PATH_PROG(TAR, tar, $FALSE)
AC_PATH_PROG(MAKE, make, $FALSE)

#
# Do all tests with c++ compiler.
#
AC_LANG(C++)

#
# Determine compiler switches to support C++14 standard.
#
AC_MSG_CHECKING(whether to test compiler features)
AC_ARG_ENABLE(cpp-check,
	[AS_HELP_STRING([--disable-cpp-check],[disable check for C++14 compiler features])],
	[ ENABLECPPCHECK=$enableval ],
	[ ENABLECPPCHECK=yes] )
AC_MSG_RESULT($ENABLECPPCHECK)
if test "$ENABLECPPCHECK" = "yes"; then
	AX_CXX_COMPILE_STDCXX(14,,[optional])
	if test "$HAVE_CXX14" != "1"; then
		AC_MSG_ERROR("A compiler with support for C++14 language features is required. For details visit http://nzbget.net/cpp14")
	fi
fi

#
# Checks for header files.
#
AC_CHECK_HEADERS(sys/prctl.h regex.h endian.h getopt.h)
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h netdb.h netinet/in.h stdint.h sys/capability.h sys/file.h sys/socket.h sys/statvfs.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_UID_T
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT64_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

#
# Check for libs
#
AC_SEARCH_LIBS([pthread_create], [pthread])
AC_SEARCH_LIBS([socket], [socket])
AC_SEARCH_LIBS([inet_addr], [nsl])
AC_SEARCH_LIBS([hstrerror], [resolv])
AC_SEARCH_LIBS([cap_get_proc],[cap])

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_REALLOC
AC_FUNC_STRERROR_R
AC_FUNC_STRNLEN
AC_CHECK_FUNCS([bzero dup2 fdatasync floor getcwd gethostbyname gettimeofday inet_ntoa memchr memmove memset mkdir pow realpath regcomp rmdir select socket strcasecmp strchr strdup strerror strncasecmp strpbrk strrchr strspn strstr strtol strtoul])

#
# Android NDK restrictions
#
AC_CHECK_FUNC(lockf,
	[AC_CHECK_DECL(lockf,
		[AC_DEFINE([HAVE_LOCKF], 1, [Define to 1 if lockf is supported])],,
		[#include <unistd.h>])])
AC_CHECK_FUNC(pthread_cancel,
	[AC_CHECK_DECL(pthread_cancel,
		[AC_DEFINE([HAVE_PTHREAD_CANCEL], 1, [Define to 1 if pthread_cancel is supported])],,
		[#include <pthread.h>])])

#
# Getopt
#
AC_CHECK_FUNC(getopt_long,
	[AC_DEFINE([HAVE_GETOPT_LONG], 1, [Define to 1 if getopt_long is supported])],)


#
# fsync
#
AC_CHECK_FUNC(fdatasync,
	[AC_DEFINE([HAVE_FDATASYNC], 1, [Define to 1 if fdatasync is supported])],)
AC_CHECK_DECL(F_FULLFSYNC,
	[AC_DEFINE([HAVE_FULLFSYNC], 1, [Define to 1 if F_FULLFSYNC is supported])],,[#include <fcntl.h>])

#
# use 64-Bits for file sizes
#
AC_SYS_LARGEFILE


#
# check ctime_r
#
AC_MSG_CHECKING(for ctime_r)

#          AC_COMPILE_IFELSE(
#            [AC_LANG_PROGRAM([[includes]],
#               [[function-body]])],
#            [action-if-true],
#            [action-if-false])

AC_COMPILE_IFELSE( [AC_LANG_PROGRAM([[#include <time.h>]],
                                    [[ time_t clock; char buf[26];	ctime_r(&clock, buf, 26); ]]
                                    )],
	AC_MSG_RESULT([[yes, and it takes 3 arguments]])
	FOUND="yes"
	AC_DEFINE([HAVE_CTIME_R_3], 1, [Define to 1 if ctime_r takes 3 arguments]),
	FOUND="no")
if test "$FOUND" = "no"; then
AC_COMPILE_IFELSE( [AC_LANG_PROGRAM([[#include <time.h>]],
                                	  [[ time_t clock; char buf[26];	ctime_r(&clock, buf); ]])],
	AC_MSG_RESULT([[yes, and it takes 2 arguments]])
	FOUND="yes"
	AC_DEFINE([HAVE_CTIME_R_2], 1, [Define to 1 if ctime_r takes 2 arguments]),
	FOUND="no")
fi
if test "$FOUND" = "no"; then
	AC_MSG_RESULT([no])
	AC_MSG_ERROR("function ctime_r not found")
fi


#
# check getaddrinfo
#
AC_CHECK_FUNC(getaddrinfo,
	FOUND="yes"
	[AC_DEFINE([HAVE_GETADDRINFO], 1, [Define to 1 if getaddrinfo is supported])]
	AC_SEARCH_LIBS([getaddrinfo], [nsl]),
	FOUND="no")


#
# check gethostbyname_r, if getaddrinfo is not available
#
if test "$FOUND" = "no"; then
	AC_MSG_CHECKING(for gethostbyname_r)
	
	AC_COMPILE_IFELSE( [AC_LANG_PROGRAM([[#include <netdb.h>]],
          [[	char* szHost; struct hostent hinfobuf; char* strbuf; int h_errnop;
		      struct hostent* hinfo = gethostbyname_r(szHost, &hinfobuf, strbuf, 1024, &h_errnop); ]])],
		AC_MSG_RESULT([[yes, and it takes 5 arguments]])
		FOUND="yes"
		AC_DEFINE([HAVE_GETHOSTBYNAME_R_5], 1, [Define to 1 if gethostbyname_r takes 5 arguments]),
		FOUND="no")
	
	if test "$FOUND" = "no"; then
	AC_COMPILE_IFELSE( [AC_LANG_PROGRAM([[#include <netdb.h>]],
    		  [[	char* szHost; struct hostent* hinfo; struct hostent hinfobuf; char* strbuf; int h_errnop;
			        int err = gethostbyname_r(szHost, &hinfobuf, strbuf, 1024, &hinfo, &h_errnop); ]])],
		AC_MSG_RESULT([[yes, and it takes 6 arguments]])
		FOUND="yes"
		AC_DEFINE([HAVE_GETHOSTBYNAME_R_6], 1, [Define to 1 if gethostbyname_r takes 6 arguments]),
		FOUND="no")
	fi
	
	if test "$FOUND" = "no"; then
	AC_COMPILE_IFELSE( [AC_LANG_PROGRAM([[#include <netdb.h>]],
		      [[	char* szHost; struct hostent hinfo; struct hostent_data hinfobuf;
			        int err = gethostbyname_r(szHost, &hinfo, &hinfobuf); ]])],
		AC_MSG_RESULT([[yes, and it takes 3 arguments]])
		FOUND="yes"
		AC_DEFINE([HAVE_GETHOSTBYNAME_R_3], 1, [Define to 1 if gethostbyname_r takes 3 arguments]),
		AC_MSG_RESULT([[no]])
		FOUND="no")
	fi
	
	if test "$FOUND" = "yes"; then
		AC_DEFINE([HAVE_GETHOSTBYNAME_R], 1, [Define to 1 if gethostbyname_r is supported])
		AC_SEARCH_LIBS([gethostbyname_r], [nsl])
	fi
fi


#
# Determine what socket length (socklen_t) data type is
#
AC_MSG_CHECKING([for type of socket length (socklen_t)])
AC_COMPILE_IFELSE(
  [ AC_LANG_PROGRAM( [[
                        #include <stddef.h>
                        #include <sys/types.h>
                        #include <sys/socket.h>
                     ]],
                     [[ (void)getsockopt (1, 1, 1, NULL, (socklen_t*)NULL)]]
  )],
  [ AC_MSG_RESULT(socklen_t)
    SOCKLEN_T=socklen_t ],
  [ AC_COMPILE_IFELSE(
    [ AC_LANG_PROGRAM( [[
                          #include <stddef.h>
                          #include <sys/types.h>
                          #include <sys/socket.h>
                       ]],
                       [[ (void)getsockopt (1, 1, 1, NULL, (size_t*)NULL) ]]
    )],
    [	AC_MSG_RESULT(size_t)
	    SOCKLEN_T=size_t ],
	  [ AC_COMPILE_IFELSE(
        [ AC_LANG_PROGRAM( [[
                              #include <stddef.h>
                              #include <sys/types.h>
                              #include <sys/socket.h>
                           ]],
                           [[ (void)getsockopt (1, 1, 1, NULL, (int*)NULL) ]]
        )],
        [ AC_MSG_RESULT(int)
          SOCKLEN_T=int ],
        [ AC_MSG_ERROR( [could not determine] )
          SOCKLEN_T=int ]
      )]
    )]
)
AC_DEFINE_UNQUOTED(SOCKLEN_T, $SOCKLEN_T, [Determine what socket length (socklen_t) data type is])


#
# check cpu cores via sysconf
#
AC_MSG_CHECKING(for cpu cores via sysconf)
AC_COMPILE_IFELSE( [AC_LANG_PROGRAM([[ #include <unistd.h> ]],
	                                  [[ int a = _SC_NPROCESSORS_ONLN; ]] )],
	FOUND="yes"
	AC_MSG_RESULT([[yes]])
	AC_DEFINE([HAVE_SC_NPROCESSORS_ONLN], 1, [Define to 1 if _SC_NPROCESSORS_ONLN is present in unistd.h]),

	FOUND="no"
)


#
# checks for libxml2 includes and libraries.
#
AC_MSG_CHECKING(whether to use libxml2)
AC_ARG_ENABLE(libxml2,
	[AS_HELP_STRING([--disable-libxml2], [do not use libxml2 (removes dependency from libxml2-library, only for development purposes)])],
	[USELIBXML2=$enableval],
	[USELIBXML2=yes] )
AC_MSG_RESULT($USELIBXML2)
if test "$USELIBXML2" = "yes"; then
	AC_ARG_WITH(libxml2_includes,
		[AS_HELP_STRING([--with-libxml2-includes=DIR], [libxml2 include directory])],
		[CPPFLAGS="${CPPFLAGS} -I${withval}"]
		[INCVAL="yes"],
		[INCVAL="no"])
	AC_ARG_WITH(libxml2_libraries,
		[AS_HELP_STRING([--with-libxml2-libraries=DIR], [libxml2 library directory])],
		[LDFLAGS="${LDFLAGS} -L${withval}"]
		[LIBVAL="yes"],
		[LIBVAL="no"])
	if test "$INCVAL" = "no" -o "$LIBVAL" = "no"; then
		PKG_CHECK_MODULES(libxml2, libxml-2.0,
			[LIBS="${LIBS} $libxml2_LIBS"]
			[CPPFLAGS="${CPPFLAGS} $libxml2_CFLAGS"],
			AC_MSG_ERROR("libxml2 library not found"))
	fi
	AC_CHECK_HEADER(libxml/tree.h, ,
		AC_MSG_ERROR("libxml2 header files not found"))
	AC_SEARCH_LIBS([xmlNewNode], [xml2], ,
		AC_MSG_ERROR("libxml2 library not found"))
else
	AC_DEFINE([DISABLE_LIBXML2],1,[Define to 1 to not use libxml2, only for development purposes])
fi


#
# Use curses. default: yes
#
AC_MSG_CHECKING(whether to use curses)
AC_ARG_ENABLE(curses,
	[AS_HELP_STRING([--disable-curses], [do not use curses (removes dependency from curses-library)])],
	[USECURSES=$enableval],
	[USECURSES=yes] )
AC_MSG_RESULT($USECURSES)
if test "$USECURSES" = "yes"; then
	AC_ARG_WITH(libcurses_includes,
		[AS_HELP_STRING([--with-libcurses-includes=DIR], [libcurses include directory])],
		[CPPFLAGS="${CPPFLAGS} -I${withval}"]
		[INCVAL="yes"],
		[INCVAL="no"])
	AC_ARG_WITH(libcurses_libraries,
		[AS_HELP_STRING([--with-libcurses-libraries=DIR], [libcurses library directory])],
		[LDFLAGS="${LDFLAGS} -L${withval}"]
		[LIBVAL="yes"],
		[LIBVAL="no"])
	if test "$INCVAL" = "no" -o "$LIBVAL" = "no"; then
		PKG_CHECK_MODULES(ncurses, ncurses,
			[LIBS="${LIBS} $ncurses_LIBS"]
			[CPPFLAGS="${CPPFLAGS} $ncurses_CFLAGS"],
			AC_MSG_ERROR("ncurses library not found"))
	fi

	AC_CHECK_HEADER(ncurses.h,
		FOUND=yes
		AC_DEFINE([HAVE_NCURSES_H],1,[Define to 1 if you have the <ncurses.h> header file.]),
		FOUND=no)
	if test "$FOUND" = "no"; then
		AC_CHECK_HEADER(ncurses/ncurses.h,
			FOUND=yes
			AC_DEFINE([HAVE_NCURSES_NCURSES_H],1,[Define to 1 if you have the <ncurses/ncurses.h> header file.]),
			FOUND=no)
	fi
	if test "$FOUND" = "no"; then
		AC_CHECK_HEADER(curses.h,
			FOUND=yes
			AC_DEFINE([HAVE_CURSES_H],1,[Define to 1 if you have the <curses.h> header file.]),
			FOUND=no)
	fi
	if test "$FOUND" = "no"; then
		AC_MSG_ERROR([Unable to find curses headers (ncurses.h or curses.h)])
	fi
	AC_SEARCH_LIBS([refresh], [ncurses curses],, 
		AC_MSG_ERROR([Unable to find curses library]))
	AC_SEARCH_LIBS([nodelay], [ncurses curses tinfo],, 
		AC_MSG_ERROR([Unable to find curses library]))
else
	AC_DEFINE([DISABLE_CURSES],1,[Define to 1 to not use curses])
fi


#
# Use par-checking. default: yes.
#
AC_MSG_CHECKING(whether to include code for par-checking)
AC_ARG_ENABLE(parcheck,
	[AS_HELP_STRING([--disable-parcheck], [do not include par-check/-repair-support])],
	[ ENABLEPARCHECK=$enableval ],
	[ ENABLEPARCHECK=yes] )
AC_MSG_RESULT($ENABLEPARCHECK)
if test "$ENABLEPARCHECK" = "yes"; then
	dnl PAR2 checks. 
	dnl 
	dnl Checks for typedefs, structures, and compiler characteristics.
	AC_TYPE_SIZE_T
	AC_FUNC_FSEEKO
	dnl Checks for library functions.
	AC_CHECK_FUNCS([stricmp])
	AC_CHECK_FUNCS([getopt])
	AM_CONDITIONAL(WITH_PAR2, true)
else
	AC_DEFINE([DISABLE_PARCHECK],1,[Define to 1 to disable par-verification and repair])
	AM_CONDITIONAL(WITH_PAR2, false)
fi


#
# Use TLS/SSL. default: yes
#
AC_MSG_CHECKING(whether to use TLS/SSL)
AC_ARG_ENABLE(tls,
	[AS_HELP_STRING([--disable-tls], [do not use TLS/SSL (removes dependency from TLS/SSL-libraries)])],
	[ USETLS=$enableval ],
	[ USETLS=yes] )
AC_MSG_RESULT($USETLS)
if test "$USETLS" = "yes"; then
	AC_ARG_WITH(tlslib,
		[AS_HELP_STRING([--with-tlslib=(OpenSSL, GnuTLS)], [TLS/SSL library to use])],
		[TLSLIB="$withval"])
	if test "$TLSLIB" != "GnuTLS" -a "$TLSLIB" != "OpenSSL" -a "$TLSLIB" != ""; then
		AC_MSG_ERROR([Invalid argument for option --with-tlslib])
	fi

	if test "$TLSLIB" = "OpenSSL" -o "$TLSLIB" = ""; then
		AC_ARG_WITH(openssl_includes,
			[AS_HELP_STRING([--with-openssl-includes=DIR], [OpenSSL include directory])],
			[CPPFLAGS="${CPPFLAGS} -I${withval}"]
			[INCVAL="yes"],
			[INCVAL="no"])
		AC_ARG_WITH(openssl_libraries,
			[AS_HELP_STRING([--with-openssl-libraries=DIR], [OpenSSL library directory])],
			[LDFLAGS="${LDFLAGS} -L${withval}"]
			[LIBVAL="yes"],
			[LIBVAL="no"])
		if test "$INCVAL" = "no" -o "$LIBVAL" = "no"; then
			PKG_CHECK_MODULES([openssl], [openssl],
				[LIBS="${LIBS} $openssl_LIBS"]
				[CPPFLAGS="${CPPFLAGS} $openssl_CFLAGS"])
		fi

		AC_CHECK_HEADER(openssl/ssl.h,
			FOUND=yes
			TLSHEADERS=yes,
			FOUND=no)
		if test "$FOUND" = "no" -a "$TLSLIB" = "OpenSSL"; then
			AC_MSG_ERROR([Unable to find OpenSSL headers (ssl.h)])
		fi
		if test "$FOUND" = "yes"; then
			AC_SEARCH_LIBS([ASN1_OBJECT_free], [crypto],
				AC_SEARCH_LIBS([SSL_CTX_new], [ssl],
					FOUND=yes,
					FOUND=no),
				FOUND=no)
			if test "$FOUND" = "no" -a "$TLSLIB" = "OpenSSL"; then
				AC_MSG_ERROR([Unable to find OpenSSL library])
			fi
			if test "$FOUND" = "yes"; then
				TLSLIB="OpenSSL"
				AC_DEFINE([HAVE_OPENSSL],1,[Define to 1 to use OpenSSL library for TLS/SSL-support and decryption.])
				AC_SEARCH_LIBS([X509_check_host], [crypto],
					AC_DEFINE([HAVE_X509_CHECK_HOST],1,[Define to 1 if OpenSSL supports function "X509_check_host".]))
			fi
		fi
	fi

	if test "$TLSLIB" = "GnuTLS" -o "$TLSLIB" = ""; then
		AC_ARG_WITH(libgnutls_includes,
			[AS_HELP_STRING([--with-libgnutls-includes=DIR], [GnuTLS include directory])],
			[CPPFLAGS="${CPPFLAGS} -I${withval}"]
			[INCVAL="yes"],
			[INCVAL="no"])
		AC_ARG_WITH(libgnutls_libraries,
			[AS_HELP_STRING([--with-libgnutls-libraries=DIR], [GnuTLS library directory])],
			[LDFLAGS="${LDFLAGS} -L${withval}"]
			[LIBVAL="yes"],
			[LIBVAL="no"])
		if test "$INCVAL" = "no" -o "$LIBVAL" = "no"; then
			PKG_CHECK_MODULES([gnutls], [gnutls],
				[LIBS="${LIBS} $gnutls_LIBS"]
				[CPPFLAGS="${CPPFLAGS} $gnutls_CFLAGS"])
		fi

		AC_CHECK_HEADER(gnutls/gnutls.h,
			FOUND=yes
			TLSHEADERS=yes,
			FOUND=no)
		if test "$FOUND" = "no" -a "$TLSLIB" = "GnuTLS"; then
			AC_MSG_ERROR([Unable to find GnuTLS headers (gnutls.h)])
		fi
		if test "$FOUND" = "yes"; then
			AC_SEARCH_LIBS([gnutls_global_init], [gnutls],
				FOUND=yes,
				FOUND=no)
			if test "$FOUND" = "yes"; then
				dnl gcrypt is optional
				AC_MSG_CHECKING([whether gcrypt is needed])
				AC_COMPILE_IFELSE( [AC_LANG_PROGRAM( [[
				                                        #include <gnutls/gnutls.h>
				                                     ]],
				                                     [[
				                                        #if GNUTLS_VERSION_NUMBER <= 0x020b00
					                                      [compile error]
					                                      #endif
                                      					int a;
                                      			 ]]
                                          )],
					[ AC_MSG_RESULT([no])
					GCRYPT=no ],

					[ AC_MSG_RESULT([yes])
					GCRYPT=yes ]
        )
				if test "$GCRYPT" = "yes"; then
					AC_CHECK_HEADER([gcrypt.h],
						AC_SEARCH_LIBS([gcry_control], [gnutls gcrypt],
							FOUND=yes,
							FOUND=no),
						FOUND=yes)
				fi
			fi
			if test "$FOUND" = "no" -a "$TLSLIB" = "GnuTLS"; then
				AC_MSG_ERROR([Unable to find GnuTLS library])
			fi
			if test "$FOUND" = "yes"; then
				TLSLIB="GnuTLS"
				AC_DEFINE([HAVE_LIBGNUTLS],1,[Define to 1 to use GnuTLS library for TLS/SSL-support.])
			fi
		fi

		if test "$TLSLIB" = "GnuTLS"; then
			AC_ARG_WITH(libnettle_includes,
				[AS_HELP_STRING([--with-libnettle-includes=DIR], [Nettle include directory])],
				[CPPFLAGS="${CPPFLAGS} -I${withval}"]
				[INCVAL="yes"],
				[INCVAL="no"])
			AC_ARG_WITH(libnettle_libraries,
				[AS_HELP_STRING([--with-libnettle-libraries=DIR], [Nettle library directory])],
				[LDFLAGS="${LDFLAGS} -L${withval}"]
				[LIBVAL="yes"],
				[LIBVAL="no"])
			if test "$INCVAL" = "no" -o "$LIBVAL" = "no"; then
				PKG_CHECK_MODULES([nettle], [nettle],
					[LIBS="${LIBS} $nettle_LIBS"]
					[CPPFLAGS="${CPPFLAGS} $nettle_CFLAGS"])
			fi
			AC_CHECK_HEADER(nettle/sha.h,
				FOUND=yes,
				FOUND=no)
			if test "$FOUND" = "no"; then
				AC_MSG_ERROR([Unable to find Nettle headers (sha.h)])
			fi
			AC_SEARCH_LIBS([nettle_pbkdf2_hmac_sha256], [nettle],
				FOUND=yes,
				FOUND=no)
			if test "$FOUND" = "no"; then
				AC_MSG_ERROR([Unable to find Nettle library, required when using GnuTLS])
			fi
			if test "$FOUND" = "yes"; then
				AC_DEFINE([HAVE_NETTLE],1,[Define to 1 to use Nettle library for decryption.])
			fi
		fi
	fi

	if test "$TLSLIB" = ""; then
		if test "$TLSHEADERS" = ""; then
			AC_MSG_ERROR([Unable to find either OpenSSL or GnuTLS headers (ssl.h or gnutls.h)])
		else
			AC_MSG_ERROR([Unable to find either OpenSSL or GnuTLS libraries])
		fi
	fi
else
	AC_DEFINE([DISABLE_TLS],1,[Define to 1 to not use TLS/SSL])
fi


#
# checks for zlib includes and libraries.
#
AC_MSG_CHECKING(whether to use gzip)
AC_ARG_ENABLE(gzip,
	[AS_HELP_STRING([--disable-gzip], [disable gzip-compression/decompression (removes dependency from zlib-library)])],
	[USEZLIB=$enableval],
	[USEZLIB=yes] )
AC_MSG_RESULT($USEZLIB)
if test "$USEZLIB" = "yes"; then
	AC_ARG_WITH(zlib_includes,
		[AS_HELP_STRING([--with-zlib-includes=DIR], [zlib include directory])],
		[CPPFLAGS="${CPPFLAGS} -I${withval}"]
		[INCVAL="yes"],
		[INCVAL="no"])
	AC_ARG_WITH(zlib_libraries,
		[AS_HELP_STRING([--with-zlib-libraries=DIR], [zlib library directory])],
		[LDFLAGS="${LDFLAGS} -L${withval}"]
		[LIBVAL="yes"],
		[LIBVAL="no"])
	if test "$INCVAL" = "no" -o "$LIBVAL" = "no"; then
		PKG_CHECK_MODULES([zlib], [zlib],
			[LIBS="${LIBS} $zlib_LIBS"]
			[CPPFLAGS="${CPPFLAGS} $zlib_CFLAGS"])
	fi
	
	AC_CHECK_HEADER(zlib.h,,
		AC_MSG_ERROR("zlib header files not found"))
	AC_SEARCH_LIBS([deflateBound], [z], ,
		AC_MSG_ERROR("zlib library not found"))
else
	AC_DEFINE([DISABLE_GZIP],1,[Define to 1 to disable gzip-support])
fi


#
# Determine if CPU supports SIMD instructions
#
AC_MSG_CHECKING(whether to use SIMD-optimized routines)
USE_SIMD=no
case $host_cpu in
	i?86|x86_64)
		SSE2_CXXFLAGS="-msse2"
		SSSE3_CXXFLAGS="-mssse3"
		PCLMUL_CXXFLAGS="-msse4.1 -mpclmul"
		USE_SIMD=yes
		;;
	arm*)
		NEON_CXXFLAGS="-mfpu=neon"
		ACLECRC_CXXFLAGS="-march=armv8-a+crc -fpermissive"
		USE_SIMD=yes
		;;
	aarch64)
		ACLECRC_CXXFLAGS="-march=armv8-a+crc -fpermissive"
		USE_SIMD=yes
		;;
esac
AC_MSG_RESULT($USE_SIMD)
AC_SUBST([SSE2_CXXFLAGS])
AC_SUBST([SSSE3_CXXFLAGS])
AC_SUBST([PCLMUL_CXXFLAGS])
AC_SUBST([NEON_CXXFLAGS])
AC_SUBST([ACLECRC_CXXFLAGS])


#
# Some Linux systems require an empty signal handler for SIGCHLD
# in order for exit codes to be correctly delivered to parent process.
# Some 32-Bit BSD systems however may not function properly if the handler is installed.
# The default behavior is to install the handler.
#
AC_MSG_CHECKING(whether to use an empty SIGCHLD handler)
AC_ARG_ENABLE(sigchld-handler,
	[AS_HELP_STRING([--disable-sigchld-handler], [do not use sigchld-handler (the disabling may be neccessary on 32-Bit BSD)])],
	[SIGCHLDHANDLER=$enableval],
	[SIGCHLDHANDLER=yes])
AC_MSG_RESULT($SIGCHLDHANDLER)
if test "$SIGCHLDHANDLER" = "yes"; then
	AC_DEFINE([SIGCHLD_HANDLER], 1, [Define to 1 to install an empty signal handler for SIGCHLD])
fi


#
# Debugging. Default: no
#
AC_MSG_CHECKING(whether to include all debugging code)
AC_ARG_ENABLE(debug,
	[AS_HELP_STRING([--enable-debug], [enable debugging])],
	[ ENABLEDEBUG=$enableval ],
	[ ENABLEDEBUG=no] )
AC_MSG_RESULT($ENABLEDEBUG)


if test "$ENABLEDEBUG" = "yes"; then

#
# Begin of debugging code
#

AC_DEFINE([DEBUG],1,Define to 1 to include debug-code)


#
# check for __FUNCTION__ or __func__ macro
#
AC_MSG_CHECKING(for macro returning current function name)
AC_COMPILE_IFELSE( [AC_LANG_PROGRAM( [[
                                        #include <stdio.h>
                                     ]],
                                     [[ printf("%s\n", __FUNCTION__); ]] )],
	[ AC_MSG_RESULT(__FUNCTION__)
	  FUNCTION_MACRO_NAME=__FUNCTION__ ],
  [ AC_COMPILE_IFELSE( [AC_LANG_PROGRAM( [[
                                            #include <stdio.h>
                                         ]],
                                         [[ printf("%s\n", __func__); ]]
                                       )],
      [ AC_MSG_RESULT(__func__)
      FUNCTION_MACRO_NAME=__func__ ],
      [ AC_MSG_RESULT(none) ]
  )]
)
if test "$FUNCTION_MACRO_NAME" != ""; then
	AC_DEFINE_UNQUOTED(FUNCTION_MACRO_NAME, $FUNCTION_MACRO_NAME, [Define to the name of macro which returns the name of function being compiled])
fi


#
# variadic macros
#
AC_MSG_CHECKING(for variadic macros)
AC_COMPILE_IFELSE(
  [ AC_LANG_PROGRAM( [[
                        #define macro(...)   macrofunc(__VA_ARGS__)
                     ]],
                     [[
                        int macrofunc(int a, int b) { return a + b; }
                        int a=macro(1, 2);
                     ]]
                )],

  [ AC_MSG_RESULT([yes])
    AC_DEFINE([HAVE_VARIADIC_MACROS], 1, Define to 1 if variadic macros are supported) ],

  [ AC_MSG_RESULT([no]) ]
)


#
# Backtracing on segmentation faults
#
AC_MSG_CHECKING(for backtrace)
AC_COMPILE_IFELSE(
  [ AC_LANG_PROGRAM( [[
                        #include <execinfo.h>
                        #include <stdio.h>
                        #include <stdlib.h>
                     ]],
                     [[
                        void *array[100];	size_t size; char **strings;
                        size = backtrace(array, 100);
                        strings = backtrace_symbols(array, size);
                     ]]
  )],

	[ FOUND=yes
  	AC_MSG_RESULT([[yes]])
  	AC_DEFINE([HAVE_BACKTRACE], 1, [Define to 1 to create stacktrace on segmentation faults]) ],

	[ FOUND=no
	  AC_MSG_RESULT([[no]]) ]
)

#
# "rdynamic" linker flag
#
AC_MSG_CHECKING(for rdynamic linker flag)
	old_LDFLAGS="$LDFLAGS"
	LDFLAGS="$LDFLAGS -rdynamic"
	AC_LINK_IFELSE(
              [AC_LANG_PROGRAM( [[ ]], [[ ]] )],
              AC_MSG_RESULT([[yes]]),
              [ AC_MSG_RESULT([[no]])
                LDFLAGS="$old_LDFLAGS" ] )

#
# End of debugging code
#
else
AC_DEFINE([NDEBUG],1,Define to 1 to exclude debug-code)
fi


#
# Enable test suite. Default: no.
#
AC_MSG_CHECKING(whether to enable unit and integration tests)
AC_ARG_ENABLE(tests,
	[AS_HELP_STRING([--enable-tests], [enable unit and integration tests])],
	[ ENABLETESTS=$enableval ],
	[ ENABLETESTS=no] )
AC_MSG_RESULT($ENABLETESTS)
if test "$ENABLETESTS" = "yes"; then
	AC_DEFINE([ENABLE_TESTS],1,[Define to 1 to enable unit and integration tests])
	AM_CONDITIONAL(WITH_TESTS, true)
else
	AM_CONDITIONAL(WITH_TESTS, false)
fi

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
