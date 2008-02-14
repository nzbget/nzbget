#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(nzbget, 0.3.2-testing, hugbug@users.sourceforge.net)
AM_INIT_AUTOMAKE(nzbget, 0.3.2-testing)
AC_CONFIG_SRCDIR([nzbget.cpp])
AC_CONFIG_HEADERS([config.h])


dnl Architecture check
AC_CANONICAL_HOST
case "$host" in
  *86-*-linux*)
    LIBPREF1="/usr"
    CFLAGS1="${CFLAGS} -m486"
    CPPFLAGS1="${CPPFLAGS} -D_GNU_SOURCE"
    ;;
  *-linux*)
    LIBPREF1="/usr"
    CPPFLAGS1="${CPPFLAGS} -D_GNU_SOURCE"
    ;;
  *-freebsd*)
    LIBPREF1="/usr/local"
    ;;
  *-solaris*)
    LIBPREF1="/usr"
    ;;
esac


if test "$LIBPREF" = ""; then
	LIBPREF="$LIBPREF1"
fi
if test "$CFLAGS" = ""; then
	CFLAGS="$CFLAGS1"
fi
if test "$CPPFLAGS" = ""; then
	CPPFLAGS="$CPPFLAGS1"
fi


dnl
dnl Check for programs.
dnl
AC_PROG_CXX
AC_PROG_CC
AC_PROG_GCC_TRADITIONAL
AC_PROG_RANLIB
AC_PROG_MAKE_SET
AC_PATH_PROG(FALSE, false, /usr/bin/false)
AC_PATH_PROG(TRUE, true, /usr/bin/true)
AC_PATH_PROG(RM, rm, $FALSE)
AC_PATH_PROG(LN, ln, $FALSE)
AC_PATH_PROG(TAR, tar, $FALSE)
AC_PATH_PROG(AR, ar, $FALSE)
AC_PATH_PROG(MAKE, make, $FALSE)
AC_PATH_PROG(CXXCPP, cpp, $FALSE)
AC_PATH_PROG(MV, mv, $FALSE)
AC_PATH_PROG(MKDIR, mkdir, $FALSE)
AC_PATH_PROG(CP, cp, $FALSE)
AC_PROG_INSTALL


dnl
dnl Checks for header files.
dnl
dnl AC_CHECK_HEADERS(stdarg.h time.h stdlib.h stdio.h unistd.h errno.h string.h sys/stat.h sys/time.h)
dnl AC_CHECK_HEADERS(libgen.h pwd.h getopt.h dirent.h fcntl.h pthread.h semaphore.h)
dnl AC_CHECK_HEADERS(sys/socket.h sys/types.h netinet/in.h arpa/inet.h netdb.h)

dnl
dnl Check for libs
dnl
AC_SEARCH_LIBS([pthread_create], [pthread])
AC_SEARCH_LIBS([socket], [socket])
AC_SEARCH_LIBS([inet_addr], [nsl])
AC_SEARCH_LIBS([gethostbyname_r], [nsl])
AC_SEARCH_LIBS([hstrerror], [resolv])


dnl
dnl Getopt
dnl
AC_CHECK_FUNC(getopt_long,
	[AC_DEFINE([HAVE_GETOPT_LONG], 1, [Define to 1 if getopt_long is supported])],
	[AC_LIBOBJ(getopt) AC_LIBOBJ(getopt1)])


dnl
dnl check ctime_r
dnl
AC_MSG_CHECKING(for ctime_r)
AC_TRY_COMPILE(
	[#include <time.h>],
	[ time_t clock; char buf[26];	ctime_r(&clock, buf, 26); ],
	AC_MSG_RESULT([[yes, and it takes 3 arguments]])
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
	AC_MSG_ERROR("function ctime_r not found.")
fi


dnl
dnl check gethostbyname_r
dnl
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
	AC_MSG_RESULT([no])
	AC_MSG_ERROR("function gethostbyname_r not found.")
fi


dnl
dnl check for __FUNCTION__ or __func__ macro
dnl
AC_MSG_CHECKING(for __FUNCTION__ macro)
AC_LANG_PUSH(C++)
AC_TRY_COMPILE([#include <stdio.h>], [printf("%s\n", __FUNCTION__);],
	AC_MSG_RESULT([yes])
	AC_DEFINE([FUNCTION_MACRO_NAME],[__FUNCTION__],[Define to the name of macro which returns the name of funtion being compiled])
	HAVE_FUNCTION_MACRO=yes,
	AC_MSG_RESULT([no]))
AC_LANG_POP(C++)
if test "$HAVE_FUNCTION_MACRO" != "yes"; then
	AC_MSG_CHECKING(for __func__ macro)
	AC_LANG_PUSH(C++)
	AC_TRY_COMPILE([#include <stdio.h>], [printf("%s\n", __func__);],
		AC_MSG_RESULT([yes])
		AC_DEFINE([FUNCTION_MACRO_NAME],[__func__],[Define to the name of macro which returns the name of function being compiled])
		HAVE_FUNCTION_MACRO=yes,
		AC_MSG_RESULT([no]))
	AC_LANG_POP(C++)
fi
if test "$HAVE_FUNCTION_MACRO" != "yes"; then
	AC_DEFINE([FUNCTION_MACRO_NAME],[NULL],[Define to the name of macro which returns the name of function being compiled])
fi


dnl
dnl check for pragma pack
dnl
AC_MSG_CHECKING(for pragma pack)
AC_TRY_COMPILE([#pragma pack(1)
#pragma pack()],,
	AC_MSG_RESULT([yes])
	AC_DEFINE([HAVE_PRAGMA_PACK],1,[Define to 1 to use pragma pack directive in MessageBase.h]),
	AC_MSG_RESULT([no]))


dnl 
dnl checks for libxml2 includes and libraries.
dnl 
INCVAL="${LIBPREF}/include/libxml2"
LIBVAL="${LIBPREF}/lib"
AC_ARG_WITH(libxml2_includes,
      [  --with-libxml2-includes=DIR	libxml2 include directory],
      [INCVAL="$withval"])
CPPFLAGS="${CPPFLAGS} -I${INCVAL}"
CFLAGS="${CFLAGS} -I${INCVAL}"
AC_CHECK_HEADER(libxml/tree.h,,
	AC_MSG_ERROR("libxml2 header files were not found."))
AC_ARG_WITH(libxml2_libraries,
	[  --with-libxml2-libraries=DIR	libxml2 library directory],
	[LIBVAL="$withval"])
LDFLAGS="${LDFLAGS} -L${LIBVAL}"
AC_SEARCH_LIBS([xmlNewNode], [xml2], ,
	AC_MSG_ERROR("libxml2 library not found in $LIBVAL."))


dnl
dnl Use curses. Deafult: yes
dnl
AC_MSG_CHECKING(whether to use curses)
AC_ARG_ENABLE(curses,
	[  --disable-curses		do not use curses (removes dependency from curses-library and makes executable smaller)],
	[ USECURSES=$enableval ],
	[ USECURSES=yes] )
AC_MSG_RESULT($USECURSES)
if test "$USECURSES" = "yes"; then
	INCVAL="${LIBPREF}/include"
	LIBVAL="${LIBPREF}/lib"
	AC_ARG_WITH(libcurses_includes,
		[  --with-libcurses-includes=DIR	libcurses include directory],
		[INCVAL="$withval"])
	CPPFLAGS="${CPPFLAGS} -I${INCVAL}"
	CFLAGS="${CFLAGS} -I${INCVAL}"
	AC_ARG_WITH(libcurses_libraries,
		[  --with-libcurses-libraries=DIR	libcurses library directory],
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
		AC_MSG_ERROR([Couldn't find curses headers (ncurses.h or curses.h).])
	fi
	AC_SEARCH_LIBS([refresh], [ncurses curses],, 
		AC_ERROR([Couldn't find curses library]))
else
	AC_DEFINE([DISABLE_CURSES],1,[Define to 1 to not use curses])
fi


dnl
dnl Use lib2par for par-checking. Deafult: no
dnl
AC_MSG_CHECKING(whether to include code for par-checking)
AC_ARG_ENABLE(parcheck,
	[  --enable-parcheck		include code for par-checking],
	[ ENABLEPARCHECK=$enableval ],
	[ ENABLEPARCHECK=yes] )
AC_MSG_RESULT($ENABLEPARCHECK)
if test "$ENABLEPARCHECK" = "yes"; then

	dnl 
	dnl checks for libsigc++ includes and libraries (required for libpar2).
	dnl
	INCVAL="${LIBPREF}/include/sigc++-2.0"
	LIBVAL="${LIBPREF}/lib"
	AC_ARG_WITH(libsigc_includes,
		[  --with-libsigc-includes=DIR	libsigc++-2.0 include directory],
		[INCVAL="$withval"])
	
	AC_ARG_WITH(libsigc_libraries,
		[  --with-libsigc-libraries=DIR	libsigc++-2.0 library directory],
		[LIBVAL="$withval"])

	LDFLAGS="${LDFLAGS} -L${LIBVAL}"
	CPPFLAGS="${CPPFLAGS} -I${INCVAL} -I${LIBVAL}/sigc++-2.0/include"

	AC_LANG_PUSH(C++)
	AC_CHECK_HEADER(sigc++/type_traits.h,,
		AC_MSG_ERROR("libsigc++-2.0 header files were not found in $INCVAL."))
	AC_LANG_POP(C++)

	dnl
	dnl checks for libpar2 includes and libraries.
	dnl
	INCVAL="${LIBPREF}/include"
	LIBVAL="${LIBPREF}/lib"
	AC_ARG_WITH(libpar2_includes,
		[  --with-libpar2-includes=DIR	libpar2 include directory],
		[INCVAL="$withval"])

	CPPFLAGS="${CPPFLAGS} -I${INCVAL}"
	
	AC_LANG_PUSH(C++)
	AC_CHECK_HEADER(libpar2/libpar2.h,,
		AC_MSG_ERROR("libpar2 header files were not found in $INCVAL."))
	AC_LANG_POP(C++)

	AC_ARG_WITH(libpar2_libraries,
		[  --with-libpar2-libraries=DIR	libpar2 library directory],
		[LIBVAL="$withval"])

	LDFLAGS="${LDFLAGS} -L${LIBVAL}"

	dnl Q: How to check for c++-class in library?
		LIBS="${LIBS} -lpar2"
	dnl		AC_CHECK_LIB(par2, GenerateCRC32Table, , FOUND=no)
	dnl		if test "$FOUND" = "no"; then
	dnl			AC_MSG_ERROR("libpar2 library not found in $LIBVAL.")
	dnl		fi

else
	AC_DEFINE([DISABLE_PARCHECK],1,[Define to 1 to disable smart par-verification and restoration])
fi


dnl 
dnl Debugging. Default: no
dnl 
AC_MSG_CHECKING(whether to include all debugging code)
AC_ARG_ENABLE(debug,
	[  --enable-debug		enable debugging],
	[ ENABLEDEBUG=$enableval ],
	[ ENABLEDEBUG=no] )
if test "$ENABLEDEBUG" = "yes"; then
	AC_DEFINE([DEBUG],1,Define to 1 to include debug-code)
	if test "$CC" = "gcc"; then
		CXXFLAGS="-g -Wall"
	else
		CXXFLAGS=""
	fi
fi
AC_MSG_RESULT($ENABLEDEBUG)


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
