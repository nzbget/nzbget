/*
 *  This file if part of nzbget
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


#ifndef NZBGET_H
#define NZBGET_H

#ifdef WIN32

// WIN32

#define snprintf _snprintf
#define strdup _strdup
#define ctime_r(timep, buf, bufsize) ctime_s(buf, bufsize, timep)
#define int32_t __int32
#define mkdir(dir, flags) _mkdir(dir)
#define strcasecmp(a, b) _stricmp(a, b)
#define strncasecmp(a, b, c) _strnicmp(a, b, c)

#pragma warning(disable:4800)
#pragma warning(disable:4267)
#pragma warning(disable:4244)

#define	__S_ISTYPE(mode, mask)	(((mode) & _S_IFMT) == (mask))
#define	S_ISDIR(mode)	 __S_ISTYPE((mode), _S_IFDIR)
#define	S_ISREG(mode)	 __S_ISTYPE((mode), _S_IFREG)
#define	S_DIRMODE NULL

#define usleep(usec) Sleep((usec) / 1000)
#define gettimeofday(tm, ignore) _ftime(tm)
#define _timeval _timeb
#define socklen_t int
#define SHUT_RDWR 0x02
#define PATH_SEPARATOR '\\'
#define ALT_PATH_SEPARATOR '/'

#else

// POSIX

#define _timeval timeval
#define closesocket(sock) close(sock)
#define SOCKET int
#define INVALID_SOCKET (-1)
#define PATH_SEPARATOR '/'
#define ALT_PATH_SEPARATOR '\\'
#define MAX_PATH 1024
#define	S_DIRMODE (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)

#endif

#endif
