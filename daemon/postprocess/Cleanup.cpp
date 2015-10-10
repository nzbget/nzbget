/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2013-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "Cleanup.h"
#include "Log.h"
#include "Util.h"
#include "ParParser.h"
#include "Options.h"

void MoveController::StartJob(PostInfo* pPostInfo)
{
	MoveController* pMoveController = new MoveController();
	pMoveController->m_pPostInfo = pPostInfo;
	pMoveController->SetAutoDestroy(false);

	pPostInfo->SetPostThread(pMoveController);

	pMoveController->Start();
}

void MoveController::Run()
{
	// the locking is needed for accessing the members of NZBInfo
	DownloadQueue::Lock();

	char szNZBName[1024];
	strncpy(szNZBName, m_pPostInfo->GetNZBInfo()->GetName(), 1024);
	szNZBName[1024-1] = '\0';

	char szInfoName[1024];
	snprintf(szInfoName, 1024, "move for %s", m_pPostInfo->GetNZBInfo()->GetName());
	szInfoName[1024-1] = '\0';
	SetInfoName(szInfoName);

	strncpy(m_szInterDir, m_pPostInfo->GetNZBInfo()->GetDestDir(), 1024);
	m_szInterDir[1024-1] = '\0';

	m_pPostInfo->GetNZBInfo()->BuildFinalDirName(m_szDestDir, 1024);
	m_szDestDir[1024-1] = '\0';

	DownloadQueue::Unlock();

	PrintMessage(Message::mkInfo, "Moving completed files for %s", szNZBName);

	bool bOK = MoveFiles();

	szInfoName[0] = 'M'; // uppercase

	if (bOK)
	{
		PrintMessage(Message::mkInfo, "%s successful", szInfoName);
		// save new dest dir
		DownloadQueue::Lock();
		m_pPostInfo->GetNZBInfo()->SetDestDir(m_szDestDir);
		m_pPostInfo->GetNZBInfo()->SetMoveStatus(NZBInfo::msSuccess);
		DownloadQueue::Unlock();
	}
	else
	{
		PrintMessage(Message::mkError, "%s failed", szInfoName);
		m_pPostInfo->GetNZBInfo()->SetMoveStatus(NZBInfo::msFailure);
	}

	m_pPostInfo->SetStage(PostInfo::ptQueued);
	m_pPostInfo->SetWorking(false);
}

bool MoveController::MoveFiles()
{
	char szErrBuf[1024];
	if (!Util::ForceDirectories(m_szDestDir, szErrBuf, sizeof(szErrBuf)))
	{
		PrintMessage(Message::mkError, "Could not create directory %s: %s", m_szDestDir, szErrBuf);
		return false;
	}

	bool bOK = true;
	DirBrowser dir(m_szInterDir);
	while (const char* filename = dir.Next())
	{
		if (strcmp(filename, ".") && strcmp(filename, ".."))
		{
			char szSrcFile[1024];
			snprintf(szSrcFile, 1024, "%s%c%s", m_szInterDir, PATH_SEPARATOR, filename);
			szSrcFile[1024-1] = '\0';

			char szDstFile[1024];
			Util::MakeUniqueFilename(szDstFile, 1024, m_szDestDir, filename);

			bool bHiddenFile = filename[0] == '.';

			if (!bHiddenFile)
			{
				PrintMessage(Message::mkInfo, "Moving file %s to %s", Util::BaseFileName(szSrcFile), m_szDestDir);
			}

			if (!Util::MoveFile(szSrcFile, szDstFile) && !bHiddenFile)
			{
				char szErrBuf[256];
				PrintMessage(Message::mkError, "Could not move file %s to %s: %s", szSrcFile, szDstFile,
					Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
				bOK = false;
			}
		}
	}

	if (bOK && !Util::DeleteDirectoryWithContent(m_szInterDir, szErrBuf, sizeof(szErrBuf)))
	{
		PrintMessage(Message::mkWarning, "Could not delete intermediate directory %s: %s", m_szInterDir, szErrBuf);
	}

	return bOK;
}

void MoveController::AddMessage(Message::EKind eKind, const char* szText)
{
	m_pPostInfo->GetNZBInfo()->AddMessage(eKind, szText);
}

void CleanupController::StartJob(PostInfo* pPostInfo)
{
	CleanupController* pCleanupController = new CleanupController();
	pCleanupController->m_pPostInfo = pPostInfo;
	pCleanupController->SetAutoDestroy(false);

	pPostInfo->SetPostThread(pCleanupController);

	pCleanupController->Start();
}

void CleanupController::Run()
{
	// the locking is needed for accessing the members of NZBInfo
	DownloadQueue::Lock();

	char szNZBName[1024];
	strncpy(szNZBName, m_pPostInfo->GetNZBInfo()->GetName(), 1024);
	szNZBName[1024-1] = '\0';

	char szInfoName[1024];
	snprintf(szInfoName, 1024, "cleanup for %s", m_pPostInfo->GetNZBInfo()->GetName());
	szInfoName[1024-1] = '\0';
	SetInfoName(szInfoName);

	strncpy(m_szDestDir, m_pPostInfo->GetNZBInfo()->GetDestDir(), 1024);
	m_szDestDir[1024-1] = '\0';

	bool bInterDir = strlen(g_pOptions->GetInterDir()) > 0 &&
		!strncmp(m_szDestDir, g_pOptions->GetInterDir(), strlen(g_pOptions->GetInterDir()));
	if (bInterDir)
	{
		m_pPostInfo->GetNZBInfo()->BuildFinalDirName(m_szFinalDir, 1024);
		m_szFinalDir[1024-1] = '\0';
	}
	else
	{
		m_szFinalDir[0] = '\0';
	}

	DownloadQueue::Unlock();

	PrintMessage(Message::mkInfo, "Cleaning up %s", szNZBName);

	bool bDeleted = false;
	bool bOK = Cleanup(m_szDestDir, &bDeleted);

	if (bOK && m_szFinalDir[0] != '\0')
	{
		bool bDeleted2 = false;
		bOK = Cleanup(m_szFinalDir, &bDeleted2);
		bDeleted = bDeleted || bDeleted2;
	}

	szInfoName[0] = 'C'; // uppercase

	if (bOK && bDeleted)
	{
		PrintMessage(Message::mkInfo, "%s successful", szInfoName);
		m_pPostInfo->GetNZBInfo()->SetCleanupStatus(NZBInfo::csSuccess);
	}
	else if (bOK)
	{
		PrintMessage(Message::mkInfo, "Nothing to cleanup for %s", szNZBName);
		m_pPostInfo->GetNZBInfo()->SetCleanupStatus(NZBInfo::csSuccess);
	}
	else
	{
		PrintMessage(Message::mkError, "%s failed", szInfoName);
		m_pPostInfo->GetNZBInfo()->SetCleanupStatus(NZBInfo::csFailure);
	}

	m_pPostInfo->SetStage(PostInfo::ptQueued);
	m_pPostInfo->SetWorking(false);
}

bool CleanupController::Cleanup(const char* szDestDir, bool *bDeleted)
{
	*bDeleted = false;
	bool bOK = true;

	DirBrowser dir(szDestDir);
	while (const char* filename = dir.Next())
	{
		char szFullFilename[1024];
		snprintf(szFullFilename, 1024, "%s%c%s", szDestDir, PATH_SEPARATOR, filename);
		szFullFilename[1024-1] = '\0';

		bool bIsDir = Util::DirectoryExists(szFullFilename);

		if (strcmp(filename, ".") && strcmp(filename, "..") && bIsDir)
		{
			bOK &= Cleanup(szFullFilename, bDeleted);
		}

		// check file extension
		bool bDeleteIt = Util::MatchFileExt(filename, g_pOptions->GetExtCleanupDisk(), ",;") && !bIsDir;

		if (bDeleteIt)
		{
			PrintMessage(Message::mkInfo, "Deleting file %s", filename);
			if (remove(szFullFilename) != 0)
			{
				char szErrBuf[256];
				PrintMessage(Message::mkError, "Could not delete file %s: %s", szFullFilename, Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
				bOK = false;
			}

			*bDeleted = true;
		}
	}

	return bOK;
}

void CleanupController::AddMessage(Message::EKind eKind, const char* szText)
{
	m_pPostInfo->GetNZBInfo()->AddMessage(eKind, szText);
}
