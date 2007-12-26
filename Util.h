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


#ifndef UTIL_H
#define UTIL_H

#ifdef WIN32
#include <stdio.h>
#include <io.h>
#else
#include <dirent.h>
#endif

#ifdef WIN32
extern int optind, opterr;
extern char *optarg;
int getopt(int argc, char *argv[], char *optstring);
#endif

class DirBrowser
{
private:
#ifdef WIN32
	struct _finddata_t	m_FindData;
	intptr_t			m_hFile;
	bool				m_bFirst;
#else
	DIR*				m_pDir;
	struct dirent*		m_pFindData;
#endif

public:
						DirBrowser(const char* szPath);
						~DirBrowser();
	const char*			Next();
};

char* BaseFileName(const char* filename);
void NormalizePathSeparators(char* szPath);
bool ForceDirectories(const char* szPath);
bool LoadFileIntoBuffer(const char* szFileName, char** pBuffer, int* pBufferLength);

long long JoinInt64(unsigned int Hi, unsigned int Lo);
void SplitInt64(long long Int64, unsigned int* Hi, unsigned int* Lo);

#endif
