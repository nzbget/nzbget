/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2004  Sven Henkel <sidddy@users.sourceforge.net>
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifndef WIN32
#include <unistd.h>
#endif

#include "nzbget.h"
#include "DownloadInfo.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"

extern Options* g_pOptions;

int FileInfo::m_iIDGen = 0;

ArticleInfo::ArticleInfo()
{
	//debug("Creating ArticleInfo");
	m_szMessageID		= NULL;
	m_iSize 			= 0;
	m_eStatus			= aiUndefined;
	m_szResultFilename	= NULL;
}

ArticleInfo::~ ArticleInfo()
{
	//debug("Destroying ArticleInfo");

	if (m_szMessageID)
	{
		free(m_szMessageID);
	}
	if (m_szResultFilename)
	{
		free(m_szResultFilename);
	}
}

void ArticleInfo::SetMessageID(const char * szMessageID)
{
	m_szMessageID = strdup(szMessageID);
}

void ArticleInfo::SetResultFilename(const char * v)
{
	m_szResultFilename = strdup(v);
}


FileInfo::FileInfo()
{
	debug("Creating FileInfo");

	m_Articles.clear();
	m_Groups.clear();
	m_szSubject = NULL;
	m_szFilename = NULL;
	m_bFilenameConfirmed = false;
	m_szDestDir = NULL;
	m_szNZBFilename = NULL;
	m_lSize = 0;
	m_lRemainingSize = 0;
	m_bPaused = false;
	m_bDeleted = false;
	m_iCompleted = 0;
	m_bOutputInitialized = false;
	m_iIDGen++;
	m_iID = m_iIDGen;
}

FileInfo::~ FileInfo()
{
	debug("Destroying FileInfo");

	if (m_szSubject)
	{
		free(m_szSubject);
	}
	if (m_szFilename)
	{
		free(m_szFilename);
	}
	if (m_szDestDir)
	{
		free(m_szDestDir);
	}
	if (m_szNZBFilename)
	{
		free(m_szNZBFilename);
	}

	for (Groups::iterator it = m_Groups.begin(); it != m_Groups.end() ;it++)
	{
		free(*it);
	}
	m_Groups.clear();

	ClearArticles();
}

void FileInfo::ClearArticles()
{
	for (Articles::iterator it = m_Articles.begin(); it != m_Articles.end() ;it++)
	{
		delete *it;
	}
	m_Articles.clear();
}

void FileInfo::SetID(int s)
{
	m_iID = s;
	if (m_iIDGen < m_iID)
	{
		m_iIDGen = m_iID;
	}
}

void FileInfo::SetSubject(const char* szSubject)
{
	m_szSubject = strdup(szSubject);
}

void FileInfo::SetDestDir(const char* szDestDir)
{
	m_szDestDir = strdup(szDestDir);
}

void FileInfo::SetNZBFilename(const char * szNZBFilename)
{
	m_szNZBFilename = strdup(szNZBFilename);
}

void FileInfo::GetNiceNZBName(char* szBuffer, int iSize)
{
	MakeNiceNZBName(m_szNZBFilename, szBuffer, iSize);
}

void FileInfo::MakeNiceNZBName(const char * szNZBFilename, char * szBuffer, int iSize)
{
	char postname[1024];
	const char* szBaseName = BaseFileName(szNZBFilename);

	// if .nzb file has a certain structure, try to strip out certain elements
	if (sscanf(szBaseName, "msgid_%*d_%1023s", postname) == 1)
	{
		// OK, using stripped name
	}
	else
	{
		// using complete filename
		strncpy(postname, szBaseName, 1024);
		postname[1024-1] = '\0';
	}
	// wipe out ".nzb"
	if (char* p = strrchr(postname, '.')) *p = '\0';
	
	strncpy(szBuffer, postname, iSize);
	szBuffer[iSize-1] = '\0';
}

void FileInfo::ParseSubject()
{
	char* fnstart = strstr(m_szSubject, "\"");
	char* fnend = NULL;
	if (fnstart)
	{
		fnstart++;
		fnend = strstr(fnstart, "\"");
	}
	if (fnend)
	{
		char fn[1024];
		strncpy(fn, fnstart, fnend - fnstart);
		fn[fnend - fnstart] = '\0';
		m_szFilename = strdup(fn);
	}
	else
	{
		debug("Could not extract Filename from Subject: %s. Using Subject as Filename", m_szSubject);
		m_szFilename = strdup(m_szSubject);
	}

	//replace bad chars in filename
	char* p = m_szFilename;
	while (*p)
	{
		if (strchr("\\/:*?\"><'\n\r\t", *p))
		{
			*p = '_';
		}
		p++;
	}
}

void FileInfo::SetFilename(const char* szFilename)
{
	if (m_szFilename)
	{
		free(m_szFilename);
	}
	m_szFilename = strdup(szFilename);
}

void FileInfo::BuildDestDirName(const char* szNZBFilename)
{
	char szBuffer[1024];

	if (g_pOptions->GetAppendNZBDir())
	{
		char szNiceNZBName[1024];
		MakeNiceNZBName(szNZBFilename, szNiceNZBName, 1024);
		snprintf(szBuffer, 1024, "%s%s", g_pOptions->GetDestDir(), szNiceNZBName);
		szBuffer[1024-1] = '\0';
	}
	else
	{
		strncpy(szBuffer, g_pOptions->GetDestDir(), 1024);
		szBuffer[1024-1] = '\0'; // trim the last slash, always returned by GetDestDir()
	}

	m_szDestDir = strdup(szBuffer);
}

bool FileInfo::IsDupe()
{
	debug("Checking if the file was already downloaded or queued");

	struct stat buffer;
	char fileName[1024];
	bool exists = false;

	snprintf(fileName, 1024, "%s%c%s", GetDestDir(), (int)PATH_SEPARATOR, GetFilename());
	fileName[1024-1] = '\0';
	exists = !stat(fileName, &buffer);
	if (!exists)
	{
		snprintf(fileName, 1024, "%s%c%s_broken", GetDestDir(), (int)PATH_SEPARATOR, GetFilename());
		fileName[1024-1] = '\0';
		exists = !stat(fileName, &buffer);
	}

	return exists;
}

void FileInfo::LockOutputFile()
{
	m_mutexOutputFile.Lock();
}

void FileInfo::UnlockOutputFile()
{
	m_mutexOutputFile.Unlock();
}
