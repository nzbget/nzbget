/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2015-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "nzbget.h"
#include "DiskService.h"
#include "Options.h"
#include "StatMeter.h"
#include "Log.h"
#include "Util.h"
#include "FileSystem.h"

void DiskService::ServiceWork()
{
	m_interval++;
	if (m_interval == 5)
	{
		if (!g_Options->GetPauseDownload() &&
			g_Options->GetDiskSpace() > 0 && !g_StatMeter->GetStandBy())
		{
			// check free disk space every 1 second
			CheckDiskSpace();
		}
		m_interval = 0;
	}

	if (m_waitingRequiredDir)
	{
		CheckRequiredDir();
	}
}

void DiskService::CheckDiskSpace()
{
	int64 freeSpace = FileSystem::FreeDiskSize(g_Options->GetDestDir());
	if (freeSpace > -1 && freeSpace / 1024 / 1024 < g_Options->GetDiskSpace())
	{
		warn("Low disk space on %s. Pausing download", g_Options->GetDestDir());
		g_Options->SetPauseDownload(true);
	}

	if (!Util::EmptyStr(g_Options->GetInterDir()))
	{
		freeSpace = FileSystem::FreeDiskSize(g_Options->GetInterDir());
		if (freeSpace > -1 && freeSpace / 1024 / 1024 < g_Options->GetDiskSpace())
		{
			warn("Low disk space on %s. Pausing download", g_Options->GetInterDir());
			g_Options->SetPauseDownload(true);
		}
	}
}

void DiskService::CheckRequiredDir()
{
	if (!Util::EmptyStr(g_Options->GetRequiredDir()))
	{
		bool allExist = true;
		bool wasWaitingReported = m_waitingReported;
		// split RequiredDir into tokens
		Tokenizer tok(g_Options->GetRequiredDir(), ",;");
		while (const char* dir = tok.Next())
		{
			if (!FileSystem::FileExists(dir) && !FileSystem::DirectoryExists(dir))
			{
				if (!wasWaitingReported)
				{
					info("Waiting for required directory %s", dir);
					m_waitingReported = true;
				}
				allExist = false;
			}
		}
		if (!allExist)
		{
			return;
		}
	}

	if (m_waitingReported)
	{
		info("All required directories available");
	}

	g_Options->SetTempPauseDownload(false);
	g_Options->SetTempPausePostprocess(false);
	m_waitingRequiredDir = false;
}
