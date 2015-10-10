/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <errno.h>

#include "nzbget.h"
#include "DupeMatcher.h"
#include "Log.h"
#include "Util.h"
#include "Options.h"
#include "Script.h"
#include "Thread.h"

class RarLister : public Thread, public ScriptController
{
private:
	DupeMatcher*		m_pOwner;
	long long			m_lMaxSize;
	bool				m_bCompressed;
	bool				m_bLastSizeMax;
	long long			m_lExpectedSize;
	char*				m_szFilenameBuf;
	int					m_iFilenameBufLen;
	char				m_szLastFilename[1024];

protected:
	virtual void		AddMessage(Message::EKind eKind, const char* szText);

public:
	virtual void		Run();
	static bool			FindLargestFile(DupeMatcher* pOwner, const char* szDirectory,
							char* szFilenameBuf, int iFilenameBufLen, long long lExpectedSize,
							int iTimeoutSec, long long* pMaxSize, bool* pCompressed);
};

bool RarLister::FindLargestFile(DupeMatcher* pOwner, const char* szDirectory,
	char* szFilenameBuf, int iFilenameBufLen, long long lExpectedSize,
	int iTimeoutSec, long long* pMaxSize, bool* pCompressed)
{
	RarLister unrar;
	unrar.m_pOwner = pOwner;
	unrar.m_lExpectedSize = lExpectedSize;
	unrar.m_lMaxSize = -1;
	unrar.m_bCompressed = false;
	unrar.m_bLastSizeMax = false;
	unrar.m_szFilenameBuf = szFilenameBuf;
	unrar.m_iFilenameBufLen = iFilenameBufLen;

	char** pCmdArgs = NULL;
	if (!Util::SplitCommandLine(g_pOptions->GetUnrarCmd(), &pCmdArgs))
	{
		return false;
	}
	const char* szUnrarPath = *pCmdArgs;
	unrar.SetScript(szUnrarPath);

	const char* szArgs[4];
	szArgs[0] = szUnrarPath;
	szArgs[1] = "lt";
	szArgs[2] = "*.rar";
	szArgs[3] = NULL;
	unrar.SetArgs(szArgs, false);
	unrar.SetWorkingDir(szDirectory);

	time_t curTime = time(NULL);

	unrar.Start();

	// wait up to iTimeoutSec for unrar output
	while (unrar.IsRunning() &&
		curTime + iTimeoutSec > time(NULL) &&
		curTime >= time(NULL))					// in a case clock was changed
	{
		usleep(200 * 1000);
	}

	if (unrar.IsRunning())
	{
		unrar.Terminate();
	}

	// wait until terminated or killed
	while (unrar.IsRunning())
	{
		usleep(200 * 1000);
	}

	for (char** szArgPtr = pCmdArgs; *szArgPtr; szArgPtr++)
	{
		free(*szArgPtr);
	}
	free(pCmdArgs);

	*pMaxSize = unrar.m_lMaxSize;
	*pCompressed = unrar.m_bCompressed;

	return true;
}

void RarLister::Run()
{
	Execute();
}

void RarLister::AddMessage(Message::EKind eKind, const char* szText)
{
	if (!strncasecmp(szText, "Archive: ", 9))
	{
		m_pOwner->PrintMessage(Message::mkDetail, "Reading file %s", szText + 9);
	}
	else if (!strncasecmp(szText, "        Name: ", 14))
	{
		strncpy(m_szLastFilename, szText + 14, sizeof(m_szLastFilename));
		m_szLastFilename[sizeof(m_szLastFilename)-1] = '\0';
	}
	else if (!strncasecmp(szText, "        Size: ", 14))
	{
		m_bLastSizeMax = false;
		long long lSize = atoll(szText + 14);
		if (lSize > m_lMaxSize)
		{
			m_lMaxSize = lSize;
			m_bLastSizeMax = true;
			strncpy(m_szFilenameBuf, m_szLastFilename, m_iFilenameBufLen);
			m_szFilenameBuf[m_iFilenameBufLen-1] = '\0';
		}
		return;
	}

	if (m_bLastSizeMax && !strncasecmp(szText, " Compression: ", 14))
	{
		m_bCompressed = !strstr(szText, " -m0");
		if (m_lMaxSize > m_lExpectedSize ||
			DupeMatcher::SizeDiffOK(m_lMaxSize, m_lExpectedSize, 20))
		{
			// alread found the largest file, aborting unrar
			Terminate();
		}
	}
}


DupeMatcher::DupeMatcher(const char* szDestDir, long long lExpectedSize)
{
	m_szDestDir = strdup(szDestDir);
	m_lExpectedSize = lExpectedSize;
	m_lMaxSize = -1;
	m_bCompressed = false;
}

DupeMatcher::~DupeMatcher()
{
	free(m_szDestDir);
}

bool DupeMatcher::SizeDiffOK(long long lSize1, long long lSize2, int iMaxDiffPercent)
{
	if (lSize1 == 0 || lSize2 == 0)
	{
		return false;
	}

	long long lDiff = lSize1 - lSize2;
	lDiff = lDiff > 0 ? lDiff : -lDiff;
	long long lMax = lSize1 > lSize2 ? lSize1 : lSize2;
	int lDiffPercent = (int)(lDiff * 100 / lMax);
	return lDiffPercent < iMaxDiffPercent;
}

bool DupeMatcher::Prepare()
{
	char szFilename[1024];
	FindLargestFile(m_szDestDir, szFilename, sizeof(szFilename), &m_lMaxSize, &m_bCompressed);
	bool bSizeOK = SizeDiffOK(m_lMaxSize, m_lExpectedSize, 20);
	PrintMessage(Message::mkDetail, "Found main file %s with size %lli bytes%s",
		szFilename, m_lMaxSize, bSizeOK ? "" : ", size mismatch");
	return bSizeOK;
}

bool DupeMatcher::MatchDupeContent(const char* szDupeDir)
{
	long long lDupeMaxSize = 0;
	bool lDupeCompressed = false;
	char szFilename[1024];
	FindLargestFile(szDupeDir, szFilename, sizeof(szFilename), &lDupeMaxSize, &lDupeCompressed);
	bool bOK = lDupeMaxSize == m_lMaxSize && lDupeCompressed == m_bCompressed;
	PrintMessage(Message::mkDetail, "Found main file %s with size %lli bytes%s",
		szFilename, m_lMaxSize, bOK ? "" : ", size mismatch");
	return bOK;
}

void DupeMatcher::FindLargestFile(const char* szDirectory, char* szFilenameBuf, int iBufLen,
	long long* pMaxSize, bool* pCompressed)
{
	*pMaxSize = 0;
	*pCompressed = false;

	DirBrowser dir(szDirectory);
	while (const char* filename = dir.Next())
	{
		if (strcmp(filename, ".") && strcmp(filename, ".."))
		{
			char szFullFilename[1024];
			snprintf(szFullFilename, 1024, "%s%c%s", szDirectory, PATH_SEPARATOR, filename);
			szFullFilename[1024-1] = '\0';

			long long lFileSize = Util::FileSize(szFullFilename);
			if (lFileSize > *pMaxSize)
			{
				*pMaxSize = lFileSize;
				strncpy(szFilenameBuf, filename, iBufLen);
				szFilenameBuf[iBufLen-1] = '\0';
			}

			if (Util::MatchFileExt(filename, ".rar", ","))
			{
				RarLister::FindLargestFile(this, szDirectory, szFilenameBuf, iBufLen,
					m_lMaxSize, 60, pMaxSize, pCompressed);
				return;
			}
		}
	}
}
