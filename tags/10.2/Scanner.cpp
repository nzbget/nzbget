/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2011 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include <fstream>
#ifdef WIN32
#include <direct.h>
#else
#include <unistd.h>
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

Scanner::FileData::FileData(const char* szFilename)
{
	m_szFilename = strdup(szFilename);
	m_iSize = 0;
	m_tLastChange = 0;
}

Scanner::FileData::~FileData()
{
	free(m_szFilename);
}

Scanner::Scanner()
{
	debug("Creating Scanner");

	m_bRequestedNZBDirScan = false;
	m_bScanning = false;
	m_iNZBDirInterval = g_pOptions->GetNzbDirInterval() * 1000;
	m_iPass = 0;
	m_iStepMSec = 0;

	const char* szNZBScript = g_pOptions->GetNZBProcess();
	m_bNZBScript = szNZBScript && strlen(szNZBScript) > 0;
}

Scanner::~Scanner()
{
	debug("Destroying Scanner");

	for (FileList::iterator it = m_FileList.begin(); it != m_FileList.end(); it++)
    {
        delete *it;
	}
	m_FileList.clear();
}

void Scanner::Check()
{
	if (g_pOptions->GetNzbDir() && (m_bRequestedNZBDirScan || 
		(!g_pOptions->GetPauseScan() && g_pOptions->GetNzbDirInterval() > 0 && 
		 m_iNZBDirInterval >= g_pOptions->GetNzbDirInterval() * 1000)))
	{
		// check nzbdir every g_pOptions->GetNzbDirInterval() seconds or if requested
		bool bCheckStat = !m_bRequestedNZBDirScan;
		m_bRequestedNZBDirScan = false;
		m_bScanning = true;
		CheckIncomingNZBs(g_pOptions->GetNzbDir(), "", bCheckStat);
		if (!bCheckStat && m_bNZBScript)
		{
			// if immediate scan requesten, we need second scan to process files extracted by NzbProcess-script
			CheckIncomingNZBs(g_pOptions->GetNzbDir(), "", bCheckStat);
		}
		m_bScanning = false;
		m_iNZBDirInterval = 0;

		// if NzbDirFileAge is less than NzbDirInterval (that can happen if NzbDirInterval
		// is set for rare scans like once per hour) we make 4 scans:
		//   - one additional scan is neccessary to check sizes of detected files;
		//   - another scan is required to check files which were extracted by NzbProcess-script;
		//   - third scan is needed to check sizes of extracted files.
		if (g_pOptions->GetNzbDirFileAge() < g_pOptions->GetNzbDirInterval())
		{
			int iMaxPass = m_bNZBScript ? 3 : 1;
			if (m_iPass < iMaxPass)
			{
				// scheduling another scan of incoming directory in NzbDirFileAge seconds.
				m_iNZBDirInterval = (g_pOptions->GetNzbDirInterval() - g_pOptions->GetNzbDirFileAge()) * 1000;
				m_iPass++;
			}
			else
			{
				m_iPass = 0;
			}
		}

		DropOldFiles();
	}
	m_iNZBDirInterval += m_iStepMSec;
}

/**
* Check if there are files in directory for incoming nzb-files
* and add them to download queue
*/
void Scanner::CheckIncomingNZBs(const char* szDirectory, const char* szCategory, bool bCheckStat)
{
	DirBrowser dir(szDirectory);
	while (const char* filename = dir.Next())
	{
		struct stat buffer;
		char fullfilename[1023 + 1]; // one char reserved for the trailing slash (if needed)
		snprintf(fullfilename, 1023, "%s%s", szDirectory, filename);
		fullfilename[1023 - 1] = '\0';
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
					szSubCategory[1024 - 1] = '\0';
					szUseCategory = szSubCategory;
				}
				CheckIncomingNZBs(fullfilename, szUseCategory, bCheckStat);
			}
			else if ((buffer.st_mode & S_IFDIR) == 0 && CanProcessFile(fullfilename, bCheckStat))
			{
				ProcessIncomingFile(szDirectory, filename, fullfilename, szCategory);
			}
		}
	}
}

/**
 * Only files which were not changed during last g_pOptions->GetNzbDirFileAge() seconds
 * can be processed. That prevents the processing of files, which are currently being
 * copied into nzb-directory (eg. being downloaded in web-browser).
 */
bool Scanner::CanProcessFile(const char* szFullFilename, bool bCheckStat)
{
	const char* szExtension = strrchr(szFullFilename, '.');
	if (!szExtension ||
		!strcasecmp(szExtension, ".queued") ||
		!strcasecmp(szExtension, ".error") ||
		!strcasecmp(szExtension, ".processed"))
	{
		return false;
	}

	if (!bCheckStat)
	{
		return true;
	}

	long long lSize = Util::FileSize(szFullFilename);
	time_t tCurrent = time(NULL);
	bool bCanProcess = false;
	bool bInList = false;

	for (FileList::iterator it = m_FileList.begin(); it != m_FileList.end(); it++)
    {
        FileData* pFileData = *it;
		if (!strcmp(pFileData->GetFilename(), szFullFilename))
		{
			bInList = true;
			if (pFileData->GetSize() == lSize &&
				tCurrent - pFileData->GetLastChange() >= g_pOptions->GetNzbDirFileAge())
			{
				bCanProcess = true;
				delete pFileData;
				m_FileList.erase(it);
			}
			else
			{
				pFileData->SetSize(lSize);
				if (pFileData->GetSize() != lSize)
				{
					pFileData->SetLastChange(tCurrent);
				}
			}
			break;
		}
	}

	if (!bInList)
	{
		FileData* pFileData = new FileData(szFullFilename);
		pFileData->SetSize(lSize);
		pFileData->SetLastChange(tCurrent);
		m_FileList.push_back(pFileData);
	}

	return bCanProcess;
}

/**
 * Remove old files from the list of monitored files.
 * Normally these files are deleted from the list when they are processed.
 * However if a file was detected by function "CanProcessFile" once but wasn't 
 * processed later (for example if the user deleted it), it will stay in the list,
 * until we remove it here.
 */
void Scanner::DropOldFiles()
{
	time_t tCurrent = time(NULL);

	int i = 0;
	for (FileList::iterator it = m_FileList.begin(); it != m_FileList.end(); )
    {
        FileData* pFileData = *it;
		if ((tCurrent - pFileData->GetLastChange() >= 
			(g_pOptions->GetNzbDirInterval() + g_pOptions->GetNzbDirFileAge()) * 2) ||
			// can occur if the system clock was adjusted
			tCurrent < pFileData->GetLastChange())
		{
			debug("Removing file %s from scan file list", pFileData->GetFilename());

			delete pFileData;
			m_FileList.erase(it);
			it = m_FileList.begin() + i;
		}
		else
		{
			it++;
			i++;
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

	char* szNZBCategory = strdup(szCategory);
	NZBParameterList* pParameterList = new NZBParameterList;
	int iPriority = 0;

	bool bExists = true;

	if (m_bNZBScript && strcasecmp(szExtension, ".nzb_processed"))
	{
		NZBScriptController::ExecuteScript(g_pOptions->GetNZBProcess(), szFullFilename, szDirectory, 
			&szNZBCategory, &iPriority, pParameterList); 
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
		if (bRenameOK)
		{
			AddFileToQueue(szRenamedName, szNZBCategory, iPriority, pParameterList);
		}
		else
		{
			error("Could not rename file %s to %s! Errcode: %i", szFullFilename, szRenamedName, errno);
		}
	}
	else if (bExists && !strcasecmp(szExtension, ".nzb"))
	{
		AddFileToQueue(szFullFilename, szNZBCategory, iPriority, pParameterList);
	}

	for (NZBParameterList::iterator it = pParameterList->begin(); it != pParameterList->end(); it++)
	{
		delete *it;
	}
	pParameterList->clear();
	delete pParameterList;

	free(szNZBCategory);
}

void Scanner::AddFileToQueue(const char* szFilename, const char* szCategory, int iPriority, NZBParameterList* pParameterList)
{
	const char* szBasename = Util::BaseFileName(szFilename);

	info("Collection %s found", szBasename);

	NZBFile* pNZBFile = NZBFile::CreateFromFile(szFilename, szCategory);
	if (!pNZBFile)
	{
		error("Could not add collection %s to queue", szBasename);
	}

	char bakname2[1024];
	bool bRenameOK = Util::RenameBak(szFilename, pNZBFile ? "queued" : "error", false, bakname2, 1024);
	if (!bRenameOK)
	{
		error("Could not rename file %s to %s! Errcode: %i", szFilename, bakname2, errno);
	}

	if (pNZBFile && bRenameOK)
	{
		pNZBFile->GetNZBInfo()->SetQueuedFilename(bakname2);

		for (NZBParameterList::iterator it = pParameterList->begin(); it != pParameterList->end(); it++)
		{
			NZBParameter* pParameter = *it;
			pNZBFile->GetNZBInfo()->SetParameter(pParameter->GetName(), pParameter->GetValue());
		}

		for (NZBFile::FileInfos::iterator it = pNZBFile->GetFileInfos()->begin(); it != pNZBFile->GetFileInfos()->end(); it++)
		{
			FileInfo* pFileInfo = *it;
			pFileInfo->SetPriority(iPriority);
		}

		g_pQueueCoordinator->AddNZBFileToQueue(pNZBFile, false);
		info("Collection %s added to queue", szBasename);
	}

	if (pNZBFile)
	{
		delete pNZBFile;
	}
}

void Scanner::ScanNZBDir(bool bSyncMode)
{
	// ideally we should use mutex to access "m_bRequestedNZBDirScan",
	// but it's not critical here.
	m_bScanning = true;
	m_bRequestedNZBDirScan = true;

	while (bSyncMode && (m_bScanning || m_bRequestedNZBDirScan))
	{
		usleep(100 * 1000);
	}
}
