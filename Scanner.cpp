/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2009 Andrei Prygounkov <hugbug@users.sourceforge.net>
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
#include <fstream>
#ifdef WIN32
#include <direct.h>
#endif
#include <sys/stat.h>
#include <errno.h>

#include "nzbget.h"
#include "Scanner.h"
#include "Options.h"
#include "Log.h"
#include "QueueCoordinator.h"
#include "ScriptController.h"
#include "DiskState.h"
#include "Util.h"

extern QueueCoordinator* g_pQueueCoordinator;
extern Options* g_pOptions;
extern DiskState* g_pDiskState;

Scanner::Scanner()
{
	debug("Creating Scanner");

	m_bRequestedNZBDirScan = false;
	m_iNZBDirInterval = g_pOptions->GetNzbDirInterval() * 1000;
	m_bSecondScan = false;
	m_iStepMSec = 0;

	const char* szNZBScript = g_pOptions->GetNZBProcess();
	m_bNZBScript = szNZBScript && strlen(szNZBScript) > 0;
}

void Scanner::Check()
{
	if (g_pOptions->GetNzbDir() && (m_bRequestedNZBDirScan || 
		(!g_pOptions->GetPauseScan() && g_pOptions->GetNzbDirInterval() > 0 && 
		 m_iNZBDirInterval >= g_pOptions->GetNzbDirInterval() * 1000)))
	{
		// check nzbdir every g_pOptions->GetNzbDirInterval() seconds or if requested
		bool bCheckTimestamp = !m_bRequestedNZBDirScan;
		m_bRequestedNZBDirScan = false;
		CheckIncomingNZBs(g_pOptions->GetNzbDir(), "", bCheckTimestamp);
		m_iNZBDirInterval = 0;
		if (m_bNZBScript && (g_pOptions->GetNzbDirFileAge() < g_pOptions->GetNzbDirInterval()))
		{
			if (!m_bSecondScan)
			{
				// scheduling second scan of incoming directory in g_pOptions->GetNzbDirFileAge() seconds.
				// the second scan is needed because the files extracted by nzbprocess-script
				// might be skipped on the first scan; that might occur depending on file
				// names and their storage location in directory's entry list.
				m_iNZBDirInterval = (g_pOptions->GetNzbDirInterval() - g_pOptions->GetNzbDirFileAge() - 1) * 1000;
			}
			m_bSecondScan = !m_bSecondScan;
		}
	}
	m_iNZBDirInterval += m_iStepMSec;
}

/**
* Check if there are files in directory for incoming nzb-files
* and add them to download queue
*/
void Scanner::CheckIncomingNZBs(const char* szDirectory, const char* szCategory, bool bCheckTimestamp)
{
	DirBrowser dir(szDirectory);
	while (const char* filename = dir.Next())
	{
		struct stat buffer;
		char fullfilename[1023 + 1]; // one char reserved for the trailing slash (if needed)
		snprintf(fullfilename, 1023, "%s%s", szDirectory, filename);
		fullfilename[1023-1] = '\0';
		if (!stat(fullfilename, &buffer))
		{
			// check subfolders
			if ((buffer.st_mode & S_IFDIR) != 0 && strcmp(filename, ".") && strcmp(filename, ".."))
			{
				fullfilename[strlen(fullfilename) + 1] = '\0';
				fullfilename[strlen(fullfilename)] = PATH_SEPARATOR;
				const char* szUseCategory = filename;
				char szSubCategory[1024];
				if (strlen(szCategory) > 0)
				{
					snprintf(szSubCategory, 1023, "%s%c%s", szCategory, PATH_SEPARATOR, filename);
					szSubCategory[1024-1] = '\0';
					szUseCategory = szSubCategory;
				}
				CheckIncomingNZBs(fullfilename, szUseCategory, bCheckTimestamp);
			}
			else if ((buffer.st_mode & S_IFDIR) == 0 &&
				(!bCheckTimestamp ||
				// file found, checking modification-time
				(time(NULL) - buffer.st_mtime > g_pOptions->GetNzbDirFileAge() &&
				time(NULL) - buffer.st_ctime > g_pOptions->GetNzbDirFileAge())))
			{
				// the file is at least g_pOptions->GetNzbDirFileAge() seconds old, we can process it
				ProcessIncomingFile(szDirectory, filename, fullfilename, szCategory);
			}
		}
	}
}

void Scanner::ProcessIncomingFile(const char* szDirectory, const char* szBaseFilename, const char* szFullFilename, const char* szCategory)
{
	const char* szExtension = strrchr(szBaseFilename, '.');
	if (!szExtension)
	{
		return;
	}

	bool bExists = true;

	if (m_bNZBScript && 
		strcasecmp(szExtension, ".queued") && 
		strcasecmp(szExtension, ".error") &&
		strcasecmp(szExtension, ".processed") &&
		strcasecmp(szExtension, ".nzb_processed"))
	{
		NZBScriptController::ExecuteScript(g_pOptions->GetNZBProcess(), szFullFilename, szDirectory); 
		bExists = Util::FileExists(szFullFilename);
		if (bExists && strcasecmp(szExtension, ".nzb"))
		{
			char bakname2[1024];
			bool bRenameOK = Util::RenameBak(szFullFilename, "processed", false, bakname2, 1024);
			if (!bRenameOK)
			{
				error("Could not rename file %s to %s! Errcode: %i", szFullFilename, bakname2, errno);
			}
		}
	}

	if (!strcasecmp(szExtension, ".nzb_processed"))
	{
		char szRenamedName[1024];
		bool bRenameOK = Util::RenameBak(szFullFilename, "nzb", true, szRenamedName, 1024);
		if (!bRenameOK)
		{
			error("Could not rename file %s to %s! Errcode: %i", szFullFilename, szRenamedName, errno);
			return;
		}
		AddFileToQueue(szRenamedName, szCategory);
	}
	else if (bExists && !strcasecmp(szExtension, ".nzb"))
	{
		AddFileToQueue(szFullFilename, szCategory);
	}
}

void Scanner::AddFileToQueue(const char* szFilename, const char* szCategory)
{
	const char* szBasename = Util::BaseFileName(szFilename);

	info("Collection %s found", szBasename);

	bool bAdded = g_pQueueCoordinator->AddFileToQueue(szFilename, szCategory);
	if (bAdded)
	{
		info("Collection %s added to queue", szBasename);
	}
	else
	{
		error("Could not add collection %s to queue", szBasename);
	}

	char bakname2[1024];
	bool bRenameOK = Util::RenameBak(szFilename, bAdded ? "queued" : "error", false, bakname2, 1024);
	if (!bRenameOK)
	{
		error("Could not rename file %s to %s! Errcode: %i", szFilename, bakname2, errno);
	}

	if (bAdded && bRenameOK)
	{
		// find just added item in queue and save bakname2 into NZBInfo.QueuedFileName
		DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();
		for (FileQueue::reverse_iterator it = pDownloadQueue->GetFileQueue()->rbegin(); it != pDownloadQueue->GetFileQueue()->rend(); it++)
		{
			FileInfo* pFileInfo = *it;
			if (!strcmp(pFileInfo->GetNZBInfo()->GetFilename(), szFilename) && 
				strlen(pFileInfo->GetNZBInfo()->GetQueuedFilename()) == 0)
			{
				pFileInfo->GetNZBInfo()->SetQueuedFilename(bakname2);
				if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
				{
					g_pDiskState->SaveDownloadQueue(pDownloadQueue);
				}
				break;
			}
		}
		g_pQueueCoordinator->UnlockQueue();
	}
}

void Scanner::ScanNZBDir()
{
	// ideally we should use mutex to access "m_bRequestedNZBDirScan",
	// but it's not critical here.
	m_bRequestedNZBDirScan = true;
}
