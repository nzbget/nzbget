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

void MoveController::StartJob(PostInfo* postInfo)
{
	MoveController* moveController = new MoveController();
	moveController->m_postInfo = postInfo;
	moveController->SetAutoDestroy(false);

	postInfo->SetPostThread(moveController);

	moveController->Start();
}

void MoveController::Run()
{
	// the locking is needed for accessing the members of NZBInfo
	DownloadQueue::Lock();

	char nzbName[1024];
	strncpy(nzbName, m_postInfo->GetNZBInfo()->GetName(), 1024);
	nzbName[1024-1] = '\0';

	char infoName[1024];
	snprintf(infoName, 1024, "move for %s", m_postInfo->GetNZBInfo()->GetName());
	infoName[1024-1] = '\0';
	SetInfoName(infoName);

	strncpy(m_interDir, m_postInfo->GetNZBInfo()->GetDestDir(), 1024);
	m_interDir[1024-1] = '\0';

	m_postInfo->GetNZBInfo()->BuildFinalDirName(m_destDir, 1024);
	m_destDir[1024-1] = '\0';

	DownloadQueue::Unlock();

	PrintMessage(Message::mkInfo, "Moving completed files for %s", nzbName);

	bool ok = MoveFiles();

	infoName[0] = 'M'; // uppercase

	if (ok)
	{
		PrintMessage(Message::mkInfo, "%s successful", infoName);
		// save new dest dir
		DownloadQueue::Lock();
		m_postInfo->GetNZBInfo()->SetDestDir(m_destDir);
		m_postInfo->GetNZBInfo()->SetMoveStatus(NZBInfo::msSuccess);
		DownloadQueue::Unlock();
	}
	else
	{
		PrintMessage(Message::mkError, "%s failed", infoName);
		m_postInfo->GetNZBInfo()->SetMoveStatus(NZBInfo::msFailure);
	}

	m_postInfo->SetStage(PostInfo::ptQueued);
	m_postInfo->SetWorking(false);
}

bool MoveController::MoveFiles()
{
	char errBuf[1024];
	if (!Util::ForceDirectories(m_destDir, errBuf, sizeof(errBuf)))
	{
		PrintMessage(Message::mkError, "Could not create directory %s: %s", m_destDir, errBuf);
		return false;
	}

	bool ok = true;
	DirBrowser dir(m_interDir);
	while (const char* filename = dir.Next())
	{
		if (strcmp(filename, ".") && strcmp(filename, ".."))
		{
			char srcFile[1024];
			snprintf(srcFile, 1024, "%s%c%s", m_interDir, PATH_SEPARATOR, filename);
			srcFile[1024-1] = '\0';

			char dstFile[1024];
			Util::MakeUniqueFilename(dstFile, 1024, m_destDir, filename);

			bool hiddenFile = filename[0] == '.';

			if (!hiddenFile)
			{
				PrintMessage(Message::mkInfo, "Moving file %s to %s", Util::BaseFileName(srcFile), m_destDir);
			}

			if (!Util::MoveFile(srcFile, dstFile) && !hiddenFile)
			{
				char errBuf[256];
				PrintMessage(Message::mkError, "Could not move file %s to %s: %s", srcFile, dstFile,
					Util::GetLastErrorMessage(errBuf, sizeof(errBuf)));
				ok = false;
			}
		}
	}

	if (ok && !Util::DeleteDirectoryWithContent(m_interDir, errBuf, sizeof(errBuf)))
	{
		PrintMessage(Message::mkWarning, "Could not delete intermediate directory %s: %s", m_interDir, errBuf);
	}

	return ok;
}

void MoveController::AddMessage(Message::EKind kind, const char* text)
{
	m_postInfo->GetNZBInfo()->AddMessage(kind, text);
}

void CleanupController::StartJob(PostInfo* postInfo)
{
	CleanupController* cleanupController = new CleanupController();
	cleanupController->m_postInfo = postInfo;
	cleanupController->SetAutoDestroy(false);

	postInfo->SetPostThread(cleanupController);

	cleanupController->Start();
}

void CleanupController::Run()
{
	// the locking is needed for accessing the members of NZBInfo
	DownloadQueue::Lock();

	char nzbName[1024];
	strncpy(nzbName, m_postInfo->GetNZBInfo()->GetName(), 1024);
	nzbName[1024-1] = '\0';

	char infoName[1024];
	snprintf(infoName, 1024, "cleanup for %s", m_postInfo->GetNZBInfo()->GetName());
	infoName[1024-1] = '\0';
	SetInfoName(infoName);

	strncpy(m_destDir, m_postInfo->GetNZBInfo()->GetDestDir(), 1024);
	m_destDir[1024-1] = '\0';

	bool interDir = strlen(g_pOptions->GetInterDir()) > 0 &&
		!strncmp(m_destDir, g_pOptions->GetInterDir(), strlen(g_pOptions->GetInterDir()));
	if (interDir)
	{
		m_postInfo->GetNZBInfo()->BuildFinalDirName(m_finalDir, 1024);
		m_finalDir[1024-1] = '\0';
	}
	else
	{
		m_finalDir[0] = '\0';
	}

	DownloadQueue::Unlock();

	PrintMessage(Message::mkInfo, "Cleaning up %s", nzbName);

	bool deleted = false;
	bool ok = Cleanup(m_destDir, &deleted);

	if (ok && m_finalDir[0] != '\0')
	{
		bool deleted2 = false;
		ok = Cleanup(m_finalDir, &deleted2);
		deleted = deleted || deleted2;
	}

	infoName[0] = 'C'; // uppercase

	if (ok && deleted)
	{
		PrintMessage(Message::mkInfo, "%s successful", infoName);
		m_postInfo->GetNZBInfo()->SetCleanupStatus(NZBInfo::csSuccess);
	}
	else if (ok)
	{
		PrintMessage(Message::mkInfo, "Nothing to cleanup for %s", nzbName);
		m_postInfo->GetNZBInfo()->SetCleanupStatus(NZBInfo::csSuccess);
	}
	else
	{
		PrintMessage(Message::mkError, "%s failed", infoName);
		m_postInfo->GetNZBInfo()->SetCleanupStatus(NZBInfo::csFailure);
	}

	m_postInfo->SetStage(PostInfo::ptQueued);
	m_postInfo->SetWorking(false);
}

bool CleanupController::Cleanup(const char* destDir, bool *deleted)
{
	*deleted = false;
	bool ok = true;

	DirBrowser dir(destDir);
	while (const char* filename = dir.Next())
	{
		char fullFilename[1024];
		snprintf(fullFilename, 1024, "%s%c%s", destDir, PATH_SEPARATOR, filename);
		fullFilename[1024-1] = '\0';

		bool isDir = Util::DirectoryExists(fullFilename);

		if (strcmp(filename, ".") && strcmp(filename, "..") && isDir)
		{
			ok &= Cleanup(fullFilename, deleted);
		}

		// check file extension
		bool deleteIt = Util::MatchFileExt(filename, g_pOptions->GetExtCleanupDisk(), ",;") && !isDir;

		if (deleteIt)
		{
			PrintMessage(Message::mkInfo, "Deleting file %s", filename);
			if (remove(fullFilename) != 0)
			{
				char errBuf[256];
				PrintMessage(Message::mkError, "Could not delete file %s: %s", fullFilename, Util::GetLastErrorMessage(errBuf, sizeof(errBuf)));
				ok = false;
			}

			*deleted = true;
		}
	}

	return ok;
}

void CleanupController::AddMessage(Message::EKind kind, const char* text)
{
	m_postInfo->GetNZBInfo()->AddMessage(kind, text);
}
