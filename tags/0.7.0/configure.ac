#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(nzbget, 0.7.0, hugbug@users.sourceforge.net)
AM_INIT_AUTOMAKE(nzbget, 0.7.0)
AC_CONFIG_SRCDIR([nzbget.cpp])
AC_CONFIG_HEADERS([config.h])



dnl
dnl Architecture check.
dnl
AC_CANONICAL_HOST


dnl
dnl Set default library path, if not specified in environment variable "LIBPREF".
dnl
if test "$LIBPREF" = ""; then
	case "$host" in
		*-linux*)
			LIBPREF="/usr"
			;;
		*-freebsd*)
			LIBPREF="/usr/local"
			;;
		*-solaris*)
			LIBPREF="/usr"
			;;
	esac
fi


dnl
dnl Check for programs.
dnl
AC_PROG_CXX
AC_PATH_PROG(TAR, tar, $FALSE)
AC_PATH_PROG(MAKE, make, $FALSE)
AC_PROG_INSTALL


dnl
dnl Do all tests with c++ compiler.
dnl
AC_LANG(C++)


dnl
dnl Checks for header files.
dnl
AC_CHECK_HEADERS(sys/prctl.h)


dnl
dnl Check for libs
dnl
AC_SEARCH_LIBS([pthread_create], [pthread])
AC_SEARCH_LIBS([socket], [socket])
AC_SEARCH_LIBS([inet_addr], [nsl])
AC_SEARCH_LIBS([hstrerror], [resolv])


dnl
dnl Getopt
dnl
AC_CHECK_FUNC(getopt_long,
	[AC_DEFINE([HAVE_GETOPT_LONG], 1, [Define to 1 if getopt_long is supported])],)


dnl
dnl stat64
dnl
AC_CHECK_FUNC(stat64,
	[AC_DEFINE([HAVE_STAT64], 1, [Define to 1 if stat64 is supported])],)


dnl
dnl check ctime_r
dnl
AC_MSG_CHECKING(for ctime_r)
AC_TRY_COMPILE(
	[#include <time.h>],
	[ time_t clock; char buf[26];	ctime_r(&clock, buf, 26); ],
	AC_MSG_RESULT([[yes, and it takes 3 arguments]])
	FOUND="yes"
	AC_DEFINE([HAVE_CTIME_R_3], 1, [Define to 1 if ctime_r takes 3 arguments]),
	FOUND="no")
if test "$FOUND" = "no"; then
AC_TRY_COMPILE(
	[#include <time.h>],
	[ time_t clock; char buf[26];	ctime_r(&clock, buf); ],
	AC_MSG_RESULT([[yes, and it takes 2 arguments]])
	FOUND="yes"
	AC_DEFINE([HAVE_CTIME_R_2], 1, [Define to 1 if ctime_r takes 2 arguments]),
	FOUND="no")
fi
if test "$FOUND" = "no"; then
	AC_MSG_RESULT([no])
	AC_MSG_ERROR("function ctime_r not found")
fi


dnl
dnl check getaddrinfo
dnl
AC_CHECK_FUNC(getaddrinfo,
	FOUND="yes"
	[AC_DEFINE([HAVE_GETADDRINFO], 1, [Define to 1 if getaddrinfo is supported])]
	AC_SEARCH_LIBS([getaddrinfo], [nsl]),
	FOUND="no")


dnl
dnl check gethostbyname_r, if getaddrinfo is not available
dnl
if test "$FOUND" = "no"; then
	AC_MSG_CHECKING(for gethostbyname_r)
	
	AC_TRY_COMPILE(
		[#include <netdb.h>],
		[	char* szHost; struct hostent hinfobuf; char* strbuf; int h_errnop;
			struct hostent* hinfo = gethostbyname_r(szHost, &hinfobuf, strbuf, 1024, &h_errnop); ],
		AC_MSG_RESULT([[yes, and it takes 5 arguments]])
		FOUND="yes"                      
		AC_DEFINE([HAVE_GETHOSTBYNAME_R_5], 1, [Define to 1 if gethostbyname_r takes 5 arguments]),
		FOUND="no")
	
	if test "$FOUND" = "no"; then
	AC_TRY_COMPILE(
		[#include <netdb.h>],
		[	char* szHost; struct hostent* hinfo; struct hostent hinfobuf; char* strbuf; int h_errnop;
			int err = gethostbyname_r(szHost, &hinfobuf, strbuf, 1024, &hinfo, &h_errnop); ],
		AC_MSG_RESULT([[yes, and it takes 6 arguments]])
		FOUND="yes"
		AC_DEFINE([HAVE_GETHOSTBYNAME_R_6], 1, [Define to 1 if gethostbyname_r takes 6 arguments]),
		FOUND="no")
	fi
	
	if test "$FOUND" = "no"; then
	AC_TRY_COMPILE(
		[#include <netdb.h>],
		[	char* szHost; struct hostent hinfo; struct hostent_data hinfobuf; 
			int err = gethostbyname_r(szHost, &hinfo, &hinfobuf); ],
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


dnl
dnl Determine what socket length (socklen_t) data type is
dnl
AC_MSG_CHECKING([for type of socket length (socklen_t)])
AC_TRY_COMPILE([
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>],[
(void)getsockopt (1, 1, 1, NULL, (socklen_t*)NULL)],[
  AC_MSG_RESULT(socklen_t)
  SOCKLEN_T=socklen_t],[
  AC_TRY_COMPILE([
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>],[
(void)getsockopt (1, 1, 1, NULL, (size_t*)NULL)],[
    AC_MSG_RESULT(size_t)
    SOCKLEN_T=size_t],[
    AC_TRY_COMPILE([
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>],[
(void)getsockopt (1, 1, 1, NULL, (int*)NULL)],[
      AC_MSG_RESULT(int)
      SOCKLEN_T=int],[
      AC_MSG_WARN(could not determine)
      SOCKLEN_T=int])])])
AC_DEFINE_UNQUOTED(SOCKLEN_T, $SOCKLEN_T, [Determine what socket length (socklen_t) data type is])


dnl 
dnl checks for libxml2 includes and libraries.
dnl
AC_ARG_WITH(libxml2_includes,
	[AS_HELP_STRING([--with-libxml2-includes=DIR], [libxml2 include directory])],
	[CPPFLAGS="${CPPFLAGS} -I${withval}"]
	[CFLAGS="${CFLAGS} -I${withval}"]
	[INCVAL="yes"],
	[INCVAL="no"])
AC_ARG_WITH(libxml2_libraries,
	[AS_HELP_STRING([--with-libxml2-libraries=DIR], [libxml2 library directory])],
	[LDFLAGS="${LDFLAGS} -L${withval}"]
	[LIBVAL="yes"],
	[LIBVAL="no"])
if test "$INCVAL" = "no" -o "$LIBVAL" = "no"; then
	PKG_CHECK_MODULES(libxml2, libxml-2.0,
		[LDFLAGS="${LDFLAGS} $libxml2_LIBS"]
		[CPPFLAGS="${CPPFLAGS} $libxml2_CFLAGS"]
		[CFLAGS="${CFLAGS} $libxml2_CFLAGS"])
fi
AC_CHECK_HEADER(libxml/tree.h,,
	AC_MSG_ERROR("libxml2 header files not found"))
AC_SEARCH_LIBS([xmlNewNode], [xml2], ,
	AC_MSG_ERROR("libxml2 library not found"))


dnl
dnl Use curses. Deafult: yes
dnl
AC_MSG_CHECKING(whether to use curses)
AC_ARG_ENABLE(curses,
	[AS_HELP_STRING([--disable-curses], [do not use curses (removes dependency from curses-library)])],
	[USECURSES=$enableval],
	[USECURSES=yes] )
AC_MSG_RESULT($USECURSES)
if test "$USECURSES" = "yes"; then
	INCVAL="${LIBPREF}/include"
	LIBVAL="${LIBPREF}/lib"
	AC_ARG_WITH(libcurses_includes,
		[AS_HELP_STRING([--with-libcurses-includes=DIR], [libcurses include directory])],
		[INCVAL="$withval"])
	CPPFLAGS="${CPPFLAGS} -I${INCVAL}"
	CFLAGS="${CFLAGS} -I${INCVAL}"
	AC_ARG_WITH(libcurses_libraries,
		[AS_HELP_STRING([--with-libcurses-libraries=DIR], [libcurses library directory])],
		[LIBVAL="$withval"])
	LDFLAGS="${LDFLAGS} -L${LIBVAL}"
	
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
		AC_MSG_ERROR([Couldn't find curses headers (ncurses.h or curses.h)])
	fi
	AC_SEARCH_LIBS([refresh], [ncurses curses],, 
		AC_ERROR([Couldn't find curses library]))
else
	AC_DEFINE([DISABLE_CURSES],1,[Define to 1 to not use curses])
fi


dnl
dnl Use libpar2 for par-checking. Deafult: no
dnl
AC_MSG_CHECKING(whether to include code for par-checking)
AC_ARG_ENABLE(parcheck,
	[AS_HELP_STRING([--disable-parcheck], [do not include par-check/-repair-support (removes dependency from libpar2- and libsigc-libraries)])],
	[ ENABLEPARCHECK=$enableval ],
	[ ENABLEPARCHECK=yes] )
AC_MSG_RESULT($ENABLEPARCHECK)
if test "$ENABLEPARCHECK" = "yes"; then

	dnl 
	dnl checks for libsigc++ includes and libraries (required for libpar2).
	dnl

	AC_ARG_WITH(libsigc_includes,
		[AS_HELP_STRING([--with-libsigc-includes=DIR], [libsigc++-2.0 include directory])],
		[CPPFLAGS="${CPPFLAGS} -I${withval}"]
		[INCVAL="yes"],
		[INCVAL="no"])
	AC_ARG_WITH(libsigc_libraries,
		[AS_HELP_STRING([--with-libsigc-libraries=DIR], [libsigc++-2.0 library directory])],
		[LDFLAGS="${LDFLAGS} -L${withval}"]
		[CPPFLAGS="${CPPFLAGS} -I${withval}/sigc++-2.0/include"]
		[LIBVAL="yes"],
		[LIBVAL="no"])
	if test "$INCVAL" = "no" -o "$LIBVAL" = "no"; then
		PKG_CHECK_MODULES(libsigc, sigc++-2.0,
			[LDFLAGS="${LDFLAGS} $libsigc_LIBS"]
			[CPPFLAGS="${CPPFLAGS} $libsigc_CFLAGS"])
	fi
	
	AC_CHECK_HEADER(sigc++/type_traits.h,,
		AC_MSG_ERROR("libsigc++-2.0 header files not found"))

	dnl
	dnl checks for libpar2 includes and libraries.
	dnl
	INCVAL="${LIBPREF}/include"
	LIBVAL="${LIBPREF}/lib"
	AC_ARG_WITH(libpar2_includes,
		[AS_HELP_STRING([--with-libpar2-includes=DIR], [libpar2 include directory])],
		[INCVAL="$withval"])

	CPPFLAGS="${CPPFLAGS} -I${INCVAL}"
	
	AC_CHECK_HEADER(libpar2/libpar2.h,,
		AC_MSG_ERROR("libpar2 header files not found"))

	AC_ARG_WITH(libpar2_libraries,
		[AS_HELP_STRING([--with-libpar2-libraries=DIR], [libpar2 library directory])],
		[LIBVAL="$withval"])

	LDFLAGS="${LDFLAGS} -L${LIBVAL}"

	AC_SEARCH_LIBS([_ZN12Par2RepairerC1Ev], [par2], ,
		AC_MSG_ERROR("libpar2 library not found"))

	dnl
	dnl check if libpar2 library is linkable
	dnl
	AC_MSG_CHECKING(for libpar2 linking)
	AC_TRY_LINK(
		[#include <libpar2/par2cmdline.h>]
		[#include <libpar2/par2repairer.h>]
		[ class Repairer : public Par2Repairer { }; ],
		[ Repairer* p = new Repairer(); ],
		AC_MSG_RESULT([[yes]]),
		AC_MSG_RESULT([[no]])
		AC_MSG_ERROR("libpar2 library not found"))

	dnl
	dnl check if libpar2 has support for cancelling
	dnl
	AC_MSG_CHECKING(whether libpar2 supports cancelling)
	AC_TRY_COMPILE(
		[#include <libpar2/par2cmdline.h>]
		[#include <libpar2/par2repairer.h>]
		[ class Repairer : public Par2Repairer { void test() { cancelled = true; } }; ],
		[],
		AC_MSG_RESULT([[yes]])
		AC_DEFINE([HAVE_PAR2_CANCEL], 1, [Define to 1 if libpar2 supports cancelling (needs a special patch)]),
		AC_MSG_RESULT([[no]]))
else
	AC_DEFINE([DISABLE_PARCHECK],1,[Define to 1 to disable smart par-verification and restoration])
fi


dnl
dnl Use TLS/SSL. Deafult: yes
dnl
AC_MSG_CHECKING(whether to use TLS/SSL)
AC_ARG_ENABLE(tls,
	[AS_HELP_STRING([--disable-tls], [do not use TLS/SSL (removes dependency from TLS/SSL-libraries)])],
	[ USETLS=$enableval ],
	[ USETLS=yes] )
AC_MSG_RESULT($USETLS)
if test "$USETLS" = "yes"; then
	AC_ARG_WITH(tlslib,
		[AS_HELP_STRING([--with-tlslib=(GnuTLS, OpenSSL)], [TLS/SSL library to use])],
		[TLSLIB="$withval"])
	if test "$TLSLIB" != "GnuTLS" -a "$TLSLIB" != "OpenSSL" -a "$TLSLIB" != ""; then
		AC_MSG_ERROR([Invalid argument for option --with-tlslib])
	fi
	
	if test "$TLSLIB" = "GnuTLS" -o "$TLSLIB" = ""; then
		INCVAL="${LIBPREF}/include"
		LIBVAL="${LIBPREF}/lib"
		AC_ARG_WITH(libgnutls_includes,
			[AS_HELP_STRING([--with-libgnutls-includes=DIR], [GnuTLS include directory])],
			[INCVAL="$withval"])
		CPPFLAGS="${CPPFLAGS} -I${INCVAL}"
		CFLAGS="${CFLAGS} -I${INCVAL}"
		AC_ARG_WITH(libgnutls_libraries,
			[AS_HELP_STRING([--with-libgnutls-libraries=DIR], [GnuTLS library directory])],
			[LIBVAL="$withval"])
		LDFLAGS="${LDFLAGS} -L${LIBVAL}"
	
		AC_CHECK_HEADER(gnutls/gnutls.h,
			FOUND=yes
			TLSHEADERS=yes,
			FOUND=no)
		if test "$FOUND" = "no" -a "$TLSLIB" = "GnuTLS"; then
			AC_MSG_ERROR([Couldn't find GnuTLS headers (gnutls.h)])
		fi
		if test "$FOUND" = "yes"; then
			AC_SEARCH_LIBS([gnutls_global_init], [gnutls],
				AC_SEARCH_LIBS([gcry_control], [gnutls gcrypt],
					FOUND=yes,
					FOUND=no),
				FOUND=no)
			if test "$FOUND" = "no" -a "$TLSLIB" = "GnuTLS"; then
				AC_MSG_ERROR([Couldn't find GnuTLS library])
			fi
			if test "$FOUND" = "yes"; then
				TLSLIB="GnuTLS"
				AC_DEFINE([HAVE_LIBGNUTLS],1,[Define to 1 to use GnuTLS library for TLS/SSL-support.])
			fi
		fi
	fi
	
	if test "$TLSLIB" = "OpenSSL" -o "$TLSLIB" = ""; then
		INCVAL="${LIBPREF}/include"
		LIBVAL="${LIBPREF}/lib"
		AC_ARG_WITH(openssl_includes,
			[AS_HELP_STRING([--with-openssl-includes=DIR], [OpenSSL include directory])],
			[INCVAL="$withval"])
		CPPFLAGS="${CPPFLAGS} -I${INCVAL}"
		CFLAGS="${CFLAGS} -I${INCVAL}"
		AC_ARG_WITH(openssl_libraries,
			[AS_HELP_STRING([--with-openssl-libraries=DIR], [OpenSSL library directory])],
			[LIBVAL="$withval"])
		LDFLAGS="${LDFLAGS} -L${LIBVAL}"
	
		AC_CHECK_HEADER(openssl/ssl.h,
			FOUND=yes
			TLSHEADERS=yes,
			FOUND=no)
		if test "$FOUND" = "no" -a "$TLSLIB" = "OpenSSL"; then
			AC_MSG_ERROR([Couldn't find OpenSSL headers (ssl.h)])
		fi
		if test "$FOUND" = "yes"; then
			AC_SEARCH_LIBS([SSL_library_init], [ssl],
				FOUND=yes,
				FOUND=no)
			if test "$FOUND" = "no" -a "$TLSLIB" = "OpenSSL"; then
				AC_MSG_ERROR([Couldn't find OpenSSL library])
			fi
			if test "$FOUND" = "yes"; then
				TLSLIB="OpenSSL"
				AC_DEFINE([HAVE_OPENSSL],1,[Define to 1 to use OpenSSL library for TLS/SSL-support.])
			fi
		fi
	fi
	
	if test "$TLSLIB" = ""; then
		if test "$TLSHEADERS" = ""; then
			AC_MSG_ERROR([Couldn't find neither GnuTLS nor OpenSSL headers (gnutls.h or ssl.h)])
		else
			AC_MSG_ERROR([Couldn't find neither GnuTLS nor OpenSSL library])
		fi
	fi
else
	AC_DEFINE([DISABLE_TLS],1,[Define to 1 to not use TLS/SSL])
fi


dnl 
dnl Debugging. Default: no
dnl 
AC_MSG_CHECKING(whether to include all debugging code)
AC_ARG_ENABLE(debug,
	[AS_HELP_STRING([--enable-debug], [enable debugging])],
	[ ENABLEDEBUG=$enableval ],
	[ ENABLEDEBUG=no] )
AC_MSG_RESULT($ENABLEDEBUG)


if test "$ENABLEDEBUG" = "yes"; then

dnl
dnl Begin of debugging code
dnl

AC_DEFINE([DEBUG],1,Define to 1 to include debug-code)


dnl
dnl Set debug flags for gcc (if gcc is used)
dnl
if test "$CC" = "gcc"; then
	CXXFLAGS="-g -Wall"
else
	CXXFLAGS=""
fi


dnl
dnl check for __FUNCTION__ or __func__ macro
dnl
AC_MSG_CHECKING(for macro returning current function name)
AC_TRY_COMPILE(
	[#include <stdio.h>], [printf("%s\n", __FUNCTION__);],
	AC_MSG_RESULT(__FUNCTION__)
	FUNCTION_MACRO_NAME=__FUNCTION__,
AC_TRY_COMPILE([#include <stdio.h>], [printf("%s\n", __func__);],
	AC_MSG_RESULT(__func__)
	FUNCTION_MACRO_NAME=__func__,
	AC_MSG_RESULT(none)))
if test "$FUNCTION_MACRO_NAME" != ""; then
	AC_DEFINE_UNQUOTED(FUNCTION_MACRO_NAME, $FUNCTION_MACRO_NAME, [Define to the name of macro which returns the name of function being compiled])
fi


dnl 
dnl variadic macros
dnl 
AC_MSG_CHECKING(for variadic macros)
AC_COMPILE_IFELSE([
	#define macro(...)   macrofunc(__VA_ARGS__)
	int macrofunc(int a, int b) { return a + b; }
	int test() { return macro(1, 2); }
	],
	AC_MSG_RESULT([yes])
	AC_DEFINE([HAVE_VARIADIC_MACROS], 1, Define to 1 if variadic macros are supported),
	AC_MSG_RESULT([no]))


dnl
dnl Backtracing on segmentation faults
dnl
AC_MSG_CHECKING(for backtrace)
AC_TRY_COMPILE(
	[#include <execinfo.h>]
	[#include <stdio.h>]
	[#include <stdlib.h>],
	[ void *array[100];	size_t size; char **strings; ]
	[ size = backtrace(array, 100); ]
	[ strings = backtrace_symbols(array, size); ],
	FOUND=yes
	AC_MSG_RESULT([[yes]])
	AC_DEFINE([HAVE_BACKTRACE], 1, [Define to 1 to create stacktrace on segmentation faults]),
	FOUND=no
	AC_MSG_RESULT([[no]]))


dnl
dnl "rdynamic" linker flag
dnl
AC_MSG_CHECKING(for rdynamic linker flag)
	old_LDFLAGS="$LDFLAGS"
	LDFLAGS="$LDFLAGS -rdynamic"
	AC_TRY_LINK([], [],
		AC_MSG_RESULT([[yes]]),
		AC_MSG_RESULT([[no]])
		[LDFLAGS="$old_LDFLAGS"])
		
dnl
dnl End of debugging code
dnl
fi


dnl Substitute flags.
AC_SUBST(CFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(CXXFLAGS)
AC_SUBST(TAR)
AC_SUBST(AR)
AC_SUBST(ADDSRCS)


AC_CONFIG_FILES([Makefile])
AC_OUTPUT
