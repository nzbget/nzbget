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
#ifdef WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include "nzbget.h"
#include "DiskService.h"
#include "Options.h"
#include "StatMeter.h"
#include "Log.h"
#include "Util.h"

DiskService::DiskService()
{
	m_iInterval = 0;
	m_bWaitingReported = false;
	m_bWaitingRequiredDir = true;
}

void DiskService::ServiceWork()
{
	m_iInterval++;
	if (m_iInterval == 5)
	{
		if (!g_pOptions->GetPauseDownload() && 
			g_pOptions->GetDiskSpace() > 0 && !g_pStatMeter->GetStandBy())
		{
			// check free disk space every 1 second
			CheckDiskSpace();
		}
		m_iInterval = 0;
	}

	if (m_bWaitingRequiredDir)
	{
		CheckRequiredDir();
	}
}

void DiskService::CheckDiskSpace()
{
	long long lFreeSpace = Util::FreeDiskSize(g_pOptions->GetDestDir());
	if (lFreeSpace > -1 && lFreeSpace / 1024 / 1024 < g_pOptions->GetDiskSpace())
	{
		warn("Low disk space on %s. Pausing download", g_pOptions->GetDestDir());
		g_pOptions->SetPauseDownload(true);
	}

	if (!Util::EmptyStr(g_pOptions->GetInterDir()))
	{
		lFreeSpace = Util::FreeDiskSize(g_pOptions->GetInterDir());
		if (lFreeSpace > -1 && lFreeSpace / 1024 / 1024 < g_pOptions->GetDiskSpace())
		{
			warn("Low disk space on %s. Pausing download", g_pOptions->GetInterDir());
			g_pOptions->SetPauseDownload(true);
		}
	}
}

void DiskService::CheckRequiredDir()
{
	if (!Util::EmptyStr(g_pOptions->GetRequiredDir()))
	{
		bool bAllExist = true;
		bool bWasWaitingReported = m_bWaitingReported;
		// split RequiredDir into tokens
		Tokenizer tok(g_pOptions->GetRequiredDir(), ",;");
		while (const char* szDir = tok.Next())
		{
			if (!Util::FileExists(szDir) && !Util::DirectoryExists(szDir))
			{
				if (!bWasWaitingReported)
				{
					info("Waiting for required directory %s", szDir);
					m_bWaitingReported = true;
				}
				bAllExist = false;
			}
		}
		if (!bAllExist)
		{
			return;
		}
	}

	if (m_bWaitingReported)
	{
		info("All required directories available");
	}

	g_pOptions->SetTempPauseDownload(false);
	g_pOptions->SetTempPausePostprocess(false);
	m_bWaitingRequiredDir = false;
}
