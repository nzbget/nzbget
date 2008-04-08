/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007  Andrei Prygounkov <hugbug@users.sourceforge.net>
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

/* win32.h - Defines and standard includes for MS Windows / Visual C++ 2005  */

/* Define to 1 to not use curses */
#undef DISABLE_CURSES

/* Define to 1 to disable smart par-verification and restoration */
#undef DISABLE_PARCHECK

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

#define VERSION "0.4.0"

/* Suppress warnings */
#define _CRT_SECURE_NO_DEPRECATE

/* Suppress warnings */
#define _CRT_NONSTDC_NO_WARNINGS

#define _USE_32BIT_TIME_T

#ifdef _DEBUG
// detection of memory leaks
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#include <windows.h>
#include <winbase.h>

