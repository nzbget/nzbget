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
#include <ctype.h>
#include <fstream>
#ifdef WIN32
#include <direct.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

#include "nzbget.h"
#include "PrePostProcessor.h"
#include "Options.h"
#include "Log.h"
#include "QueueCoordinator.h"
#include "ScriptController.h"
#include "DiskState.h"
#include "Util.h"
#include "Scheduler.h"

extern QueueCoordinator* g_pQueueCoordinator;
extern Options* g_pOptions;
extern DiskState* g_pDiskState;
extern Scheduler* g_pScheduler;

static const int PARSTATUS_NOT_CHECKED = 0;
static const int PARSTATUS_FAILED = 1;
static const int PARSTATUS_REPAIRED = 2;
static const int PARSTATUS_REPAIR_POSSIBLE = 3;

#ifndef DISABLE_PARCHECK
bool PrePostProcessor::PostParChecker::RequestMorePars(int iBlockNeeded, int* pBlockFound)
{
	return m_Owner->RequestMorePars(m_pPostInfo->GetNZBInfo(), GetParFilename(), iBlockNeeded, pBlockFound);
}

void PrePostProcessor::PostParChecker::UpdateProgress()
{
	m_Owner->UpdateParProgress();
}
#endif

PrePostProcessor::PrePostProcessor()
{
	debug("Creating PrePostProcessor");

	m_bHasMoreJobs = false;
	m_bPostPause = false;
	m_bRequestedNZBDirScan = false;
	m_bPause = false;

	m_QueueCoordinatorObserver.owner = this;
	g_pQueueCoordinator->Attach(&m_QueueCoordinatorObserver);

	const char* szPostScript = g_pOptions->GetPostProcess();
	m_bPostScript = szPostScript && strlen(szPostScript) > 0;

	const char* szNZBScript = g_pOptions->GetNZBProcess();
	m_bNZBScript = szNZBScript && strlen(szNZBScript) > 0;

#ifndef DISABLE_PARCHECK
	m_ParCheckerObserver.owner = this;
	m_ParChecker.Attach(&m_ParCheckerObserver);
	m_ParChecker.m_Owner = this;
#endif
}

PrePostProcessor::~PrePostProcessor()
{
	debug("Destroying PrePostProcessor");
}

void PrePostProcessor::Cleanup()
{
	debug("Cleaning up PrePostProcessor");

	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	for (PostQueue::iterator it = pDownloadQueue->GetPostQueue()->begin(); it != pDownloadQueue->GetPostQueue()->end(); it++)
	{
		delete *it;
	}
	pDownloadQueue->GetPostQueue()->clear();

	for (PostQueue::iterator it = pDownloadQueue->GetCompletedPostList()->begin(); it != pDownloadQueue->GetCompletedPostList()->end(); it++)
	{
		delete *it;
	}
	pDownloadQueue->GetCompletedPostList()->clear();

	g_pQueueCoordinator->UnlockQueue();
}

void PrePostProcessor::Run()
{
	debug("Entering PrePostProcessor-loop");

	if (g_pOptions->GetServerMode() && g_pOptions->GetSaveQueue() &&
		g_pOptions->GetReloadQueue() && g_pOptions->GetReloadPostQueue())
	{
		DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();
		SanitisePostQueue(pDownloadQueue->GetPostQueue());
		g_pQueueCoordinator->UnlockQueue();
	}

	g_pScheduler->FirstCheck();
	ApplySchedulerState();

	int iNZBDirInterval = g_pOptions->GetNzbDirInterval() * 1000;
	int iDiskSpaceInterval = 1000;
	int iSchedulerInterval = 1000;
	bool bSecondScan = false;
	while (!IsStopped())
	{
		if (g_pOptions->GetNzbDir() && (m_bRequestedNZBDirScan || 
			(g_pOptions->GetNzbDirInterval() > 0 && iNZBDirInterval >= g_pOptions->GetNzbDirInterval() * 1000)))
		{
			// check nzbdir every g_pOptions->GetNzbDirInterval() seconds or if requested
			bool bCheckTimestamp = !m_bRequestedNZBDirScan;
			m_bRequestedNZBDirScan = false;
			CheckIncomingNZBs(g_pOptions->GetNzbDir(), "", bCheckTimestamp);
			iNZBDirInterval = 0;
			if (m_bNZBScript && (g_pOptions->GetNzbDirFileAge() < g_pOptions->GetNzbDirInterval()))
			{
				if (!bSecondScan)
				{
					// scheduling second scan of incoming directory in g_pOptions->GetNzbDirFileAge() seconds.
					// the second scan is needed because the files extracted by nzbprocess-script
					// might be skipped on the first scan; that might occur depending on file
					// names and their storage location in directory's entry list.
					iNZBDirInterval = (g_pOptions->GetNzbDirInterval() - g_pOptions->GetNzbDirFileAge() - 1) * 1000;
				}
				bSecondScan = !bSecondScan;
			}
		}
		iNZBDirInterval += 200;

		if (!g_pOptions->GetPause() && g_pOptions->GetDiskSpace() > 0 && 
			!g_pQueueCoordinator->GetStandBy() && iDiskSpaceInterval >= 1000)
		{
			// check free disk space every 1 second
			CheckDiskSpace();
			iDiskSpaceInterval = 0;
		}
		iDiskSpaceInterval += 200;

		// check post-queue every 200 msec
		CheckPostQueue();

		if (iSchedulerInterval >= 1000)
		{
			// check scheduler tasks every 1 second
			g_pScheduler->IntervalCheck();
			ApplySchedulerState();
			iSchedulerInterval = 0;
		}
		iSchedulerInterval += 200;

		usleep(200 * 1000);
	}

	Cleanup();

	debug("Exiting PrePostProcessor-loop");
}

void PrePostProcessor::Stop()
{
	Thread::Stop();
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

#ifndef DISABLE_PARCHECK
	if (m_ParChecker.IsRunning())
	{
		m_ParChecker.Stop();
		int iMSecWait = 5000;
		while (m_ParChecker.IsRunning() && iMSecWait > 0)
		{
			usleep(50 * 1000);
			iMSecWait -= 50;
		}
		if (m_ParChecker.IsRunning())
		{
			warn("Terminating par-check for %s", m_ParChecker.GetInfoName());
			m_ParChecker.Kill();
		}
	}
#endif

	if (!pDownloadQueue->GetPostQueue()->empty())
	{
		PostInfo* pPostInfo = pDownloadQueue->GetPostQueue()->front();
		if (pPostInfo->GetStage() == PostInfo::ptExecutingScript && pPostInfo->GetScriptThread())
		{
			Thread* pScriptThread = pPostInfo->GetScriptThread();
			pPostInfo->SetScriptThread(NULL);
			pScriptThread->SetAutoDestroy(true);
			pScriptThread->Stop();
		}
	}

	g_pQueueCoordinator->UnlockQueue();
}

void PrePostProcessor::QueueCoordinatorUpdate(Subject * Caller, void * Aspect)
{
	if (IsStopped())
	{
		return;
	}

	QueueCoordinator::Aspect* pAspect = (QueueCoordinator::Aspect*)Aspect;
	if (pAspect->eAction == QueueCoordinator::eaNZBFileAdded)
	{
		NZBAdded(pAspect->pDownloadQueue, pAspect->pNZBInfo);
	}
	else if ((pAspect->eAction == QueueCoordinator::eaFileCompleted ||
		pAspect->eAction == QueueCoordinator::eaFileDeleted))
	{
		if (
#ifndef DISABLE_PARCHECK
			!AddPar(pAspect->pFileInfo, pAspect->eAction == QueueCoordinator::eaFileDeleted) &&
#endif
			IsNZBFileCompleted(pAspect->pDownloadQueue, pAspect->pNZBInfo, false, true, false, false) &&
			(!pAspect->pFileInfo->GetPaused() || IsNZBFileCompleted(pAspect->pDownloadQueue, pAspect->pNZBInfo, false, false, false, false)))
		{
			char szNZBNiceName[1024];
			pAspect->pNZBInfo->GetNiceNZBName(szNZBNiceName, 1024);
			if (pAspect->eAction == QueueCoordinator::eaFileCompleted)
			{
				info("Collection %s completely downloaded", szNZBNiceName);
				NZBCompleted(pAspect->pDownloadQueue, pAspect->pNZBInfo, pAspect->pFileInfo);
			}
			else if (pAspect->pNZBInfo->GetDeleted() &&
				!pAspect->pNZBInfo->GetParCleanup() &&
				IsNZBFileCompleted(pAspect->pDownloadQueue, pAspect->pNZBInfo, false, false, false, true))
			{
				info("Collection %s deleted from queue", szNZBNiceName);
				NZBDeleted(pAspect->pDownloadQueue, pAspect->pNZBInfo, pAspect->pFileInfo);
			}
		}

		if (IsNZBFileCompleted(pAspect->pDownloadQueue, pAspect->pNZBInfo, false, false, true, false))
		{
			if (ClearCompletedJobs(pAspect->pDownloadQueue, pAspect->pNZBInfo))
			{
				SavePostQueue(pAspect->pDownloadQueue);
			}
		}
	}
}

void PrePostProcessor::NZBAdded(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	if (g_pOptions->GetMergeNzb())
	{
		pNZBInfo = MergeGroups(pDownloadQueue, pNZBInfo);
	}

	if (g_pOptions->GetLoadPars() != Options::lpAll)
	{
		PausePars(pDownloadQueue, pNZBInfo);
	}
}

void PrePostProcessor::NZBCompleted(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, FileInfo* pFileInfo)
{
	pNZBInfo->SetPostProcess(true);
	bool bParCheck = false;
#ifndef DISABLE_PARCHECK
	bParCheck = g_pOptions->GetParCheck() && g_pOptions->GetDecode();
#endif
	CreatePostJobs(pDownloadQueue, pNZBInfo, bParCheck, false);
}

void PrePostProcessor::NZBDeleted(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, FileInfo* pFileInfo)
{
	if (g_pOptions->GetDeleteCleanupDisk() && pNZBInfo->GetCleanupDisk())
	{
		// download was cancelled, deleting already downloaded files from disk
		for (NZBInfo::Files::reverse_iterator it = pNZBInfo->GetCompletedFiles()->rbegin(); it != pNZBInfo->GetCompletedFiles()->rend(); it++)
		{
			char* szFilename = *it;
			if (Util::FileExists(szFilename))
			{
				detail("Deleting file %s", szFilename);
				remove(szFilename);
				// delete old directory (if empty)
				if (g_pOptions->GetAppendNZBDir() && Util::DirEmpty(pNZBInfo->GetDestDir()))
				{
					rmdir(pNZBInfo->GetDestDir());
				}
			}
		}
		if (g_pOptions->GetNzbCleanupDisk())
		{
			DeleteQueuedFile(pNZBInfo->GetQueuedFilename());
		}
	}
}

void PrePostProcessor::DeleteQueuedFile(const char* szQueuedFile)
{
	// szQueuedFile may contain one filename or several filenames separated 
	// with "|"-character (for merged groups)
	char* szFilename = strdup(szQueuedFile);
	char* szEnd = szFilename - 1;
	
	while (szEnd)
	{
		char* szName1 = szEnd + 1;
		szEnd = strchr(szName1, '|');
		if (szEnd) *szEnd = '\0';

		if (Util::FileExists(szName1))
		{
			info("Deleting file %s", szName1);
			remove(szName1);
		}
	}

	free(szFilename);
}

NZBInfo* PrePostProcessor::MergeGroups(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	int iAddedGroupID = 0;

	// merge(1): find ID of any file in new nzb-file
	for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (pFileInfo->GetNZBInfo() == pNZBInfo)
		{
			iAddedGroupID = pFileInfo->GetID();
			break;
		}
	}

	// merge(2): check if queue has another nzb-files with the same filename
	if (iAddedGroupID > 0)
	{
		for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
		{
			FileInfo* pFileInfo = *it;
			if (pFileInfo->GetNZBInfo() != pNZBInfo &&
				!strcmp(pFileInfo->GetNZBInfo()->GetFilename(), pNZBInfo->GetFilename()))
			{
				// file found, do merging

				IDList cIDList;
				cIDList.push_back(pFileInfo->GetID());
				cIDList.push_back(iAddedGroupID);

				g_pQueueCoordinator->GetQueueEditor()->LockedEditList(pDownloadQueue, &cIDList, false, QueueEditor::eaGroupMerge, 0, NULL);

				return pFileInfo->GetNZBInfo();
			}
		}
	}

	return pNZBInfo;
}

void PrePostProcessor::ScanNZBDir()
{
	// ideally we should use mutex to access "m_bRequestedNZBDirScan",
	// but it's not critical here.
	m_bRequestedNZBDirScan = true;
}

/**
* Check if there are files in directory for incoming nzb-files
* and add them to download queue
*/
void PrePostProcessor::CheckIncomingNZBs(const char* szDirectory, const char* szCategory, bool bCheckTimestamp)
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

void PrePostProcessor::ProcessIncomingFile(const char* szDirectory, const char* szBaseFilename, const char* szFullFilename, const char* szCategory)
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

void PrePostProcessor::AddFileToQueue(const char* szFilename, const char* szCategory)
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

void PrePostProcessor::CheckDiskSpace()
{
	long long lFreeSpace = Util::FreeDiskSize(g_pOptions->GetDestDir());
	if (lFreeSpace > -1 && lFreeSpace / 1024 / 1024 < g_pOptions->GetDiskSpace())
	{
		warn("Low disk space. Pausing download");
		g_pOptions->SetPause(true);
	}
}

void PrePostProcessor::CheckPostQueue()
{
	int iCleanupGroupID = 0;
	char szNZBNiceName[1024];
	char* szQueuedFilename = NULL;

	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	if (!pDownloadQueue->GetPostQueue()->empty())
	{
		PostInfo* pPostInfo = pDownloadQueue->GetPostQueue()->front();
		if (!pPostInfo->GetWorking())
		{
#ifndef DISABLE_PARCHECK
			if (pPostInfo->GetRequestParCheck() == PostInfo::rpAll)
			{
				CreatePostJobs(pDownloadQueue, pPostInfo->GetNZBInfo(), true, true);
			}
			else if (pPostInfo->GetRequestParCheck() == PostInfo::rpCurrent && !pPostInfo->GetParCheck())
			{
				pPostInfo->SetParCheck(true);
				pPostInfo->SetRequestParCheck(PostInfo::rpNone);
				pPostInfo->SetStage(PostInfo::ptQueued);
				if (pPostInfo->GetScriptThread())
				{
					delete pPostInfo->GetScriptThread();
					pPostInfo->SetScriptThread(NULL);
				}
			}

			if (pPostInfo->GetParCheck() && pPostInfo->GetParStatus() == PARSTATUS_NOT_CHECKED && !m_bPause)
			{
				StartParJob(pPostInfo);
			}
			else
#endif
			if (pPostInfo->GetStage() == PostInfo::ptQueued && !m_bPause)
			{
				StartScriptJob(pDownloadQueue, pPostInfo);
			}
			else if (pPostInfo->GetStage() == PostInfo::ptFinished)
			{
#ifndef DISABLE_PARCHECK
				if ((g_pOptions->GetParCleanupQueue() || g_pOptions->GetNzbCleanupDisk()) && 
					IsNZBFileCompleted(pDownloadQueue, pPostInfo->GetNZBInfo(), true, true, true, false) &&
					((pPostInfo->GetParStatus() != PARSTATUS_NOT_CHECKED &&
					  pPostInfo->GetParStatus() != PARSTATUS_FAILED) ||
					 pPostInfo->GetRequestParCleanup()) &&
					 !HasFailedParJobs(pDownloadQueue, pPostInfo->GetNZBInfo(), true))
				{
					if (g_pOptions->GetParCleanupQueue())
					{
						FileInfo* pFileInfo = GetParCleanupQueueGroup(pDownloadQueue, pPostInfo->GetNZBInfo());
						if (pFileInfo)
						{
							pFileInfo->GetNZBInfo()->SetParCleanup(true);
							iCleanupGroupID = pFileInfo->GetID();
						}
						NZBInfo::MakeNiceNZBName(pPostInfo->GetNZBInfo()->GetFilename(), szNZBNiceName, sizeof(szNZBNiceName));
					}
					if (g_pOptions->GetNzbCleanupDisk())
					{
						szQueuedFilename = strdup(pPostInfo->GetNZBInfo()->GetQueuedFilename());
					}
				}
#endif
				if (m_bPostScript && g_pOptions->GetPostPauseQueue())
				{
					if (UnpauseDownload())
					{
						info("Unpausing queue after post-process-script");
					}
				}

				JobCompleted(pDownloadQueue, pPostInfo);
			}
			else if (!m_bPause)
			{
				error("Internal error: invalid state in post-processor");
			}
		}
	}
	
	g_pQueueCoordinator->UnlockQueue();

	if (iCleanupGroupID > 0)
	{
		info("Cleaning up download queue for %s", szNZBNiceName);
		g_pQueueCoordinator->GetQueueEditor()->EditEntry(iCleanupGroupID, false, QueueEditor::eaGroupDelete, 0, NULL);
	}

	if (szQueuedFilename)
	{
		DeleteQueuedFile(szQueuedFilename);
		free(szQueuedFilename);
	}
}

void PrePostProcessor::SavePostQueue(DownloadQueue* pDownloadQueue)
{
	if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
	{
		g_pDiskState->SaveDownloadQueue(pDownloadQueue);
	}
}

/**
 * Reset the state of items after reloading from disk and
 * delete items which could not be resumed.
 */
void PrePostProcessor::SanitisePostQueue(PostQueue* pPostQueue)
{
	for (PostQueue::iterator it = pPostQueue->begin(); it != pPostQueue->end(); it++)
	{
		PostInfo* pPostInfo = *it;
		if (pPostInfo->GetStage() == PostInfo::ptExecutingScript ||
			!Util::DirectoryExists(pPostInfo->GetNZBInfo()->GetDestDir()))
		{
			pPostInfo->SetStage(PostInfo::ptFinished);
			pPostInfo->SetParCheck(false);
		}
		else 
		{
			pPostInfo->SetStage(PostInfo::ptQueued);
		}
	}
}

void PrePostProcessor::StartScriptJob(DownloadQueue* pDownloadQueue, PostInfo* pPostInfo)
{
	if (!m_bPostScript)
	{
		pPostInfo->SetStage(PostInfo::ptFinished);
		return;
	}

	pPostInfo->SetProgressLabel("Executing post-process-script");
	pPostInfo->SetWorking(true);
	pPostInfo->SetStage(PostInfo::ptExecutingScript);
	pPostInfo->SetFileProgress(0);
	pPostInfo->SetStageProgress(0);
	SavePostQueue(pDownloadQueue);

	if (!pPostInfo->GetStartTime())
	{
		pPostInfo->SetStartTime(time(NULL));
	}
	pPostInfo->SetStageTime(time(NULL));

	bool bNZBFileCompleted = IsNZBFileCompleted(pDownloadQueue, pPostInfo->GetNZBInfo(), true, true, true, false);
#ifndef DISABLE_PARCHECK
	bool bHasFailedParJobs = HasFailedParJobs(pDownloadQueue, pPostInfo->GetNZBInfo(), false) || 
		pPostInfo->GetParStatus() == PARSTATUS_FAILED || 
		pPostInfo->GetParStatus() == PARSTATUS_REPAIR_POSSIBLE;
#else
	bool bHasFailedParJobs = false;
#endif

	if (g_pOptions->GetPostPauseQueue())
	{
		if (PauseDownload())
		{
			info("Pausing queue before post-process-script");
		}
	}

	PostScriptController::StartScriptJob(pPostInfo, g_pOptions->GetPostProcess(), bNZBFileCompleted, bHasFailedParJobs);
}

void PrePostProcessor::JobCompleted(DownloadQueue* pDownloadQueue, PostInfo* pPostInfo)
{
	pPostInfo->SetWorking(false);
	pPostInfo->SetProgressLabel("");
	pPostInfo->SetStage(PostInfo::ptFinished);

	if (pPostInfo->GetScriptThread())
	{
		delete pPostInfo->GetScriptThread();
		pPostInfo->SetScriptThread(NULL);
	}

	for (PostQueue::iterator it = pDownloadQueue->GetPostQueue()->begin(); it != pDownloadQueue->GetPostQueue()->end(); it++)
	{
		if (pPostInfo == *it)
		{
			pDownloadQueue->GetPostQueue()->erase(it);
			break;
		}
	}

	pDownloadQueue->GetCompletedPostList()->push_back(pPostInfo);

	if (IsNZBFileCompleted(pDownloadQueue, pPostInfo->GetNZBInfo(), false, false, true, false))
	{
		ClearCompletedJobs(pDownloadQueue, pPostInfo->GetNZBInfo());
	}

	SavePostQueue(pDownloadQueue);

	m_bHasMoreJobs = !pDownloadQueue->GetPostQueue()->empty();
}

bool PrePostProcessor::JobExists(PostQueue* pPostQueue, NZBInfo* pNZBInfo, 
	const char* szParFilename, bool bParCheck)
{
	for (PostQueue::iterator it = pPostQueue->begin(); it != pPostQueue->end(); it++)
	{
		PostInfo* pPostInfo = *it;
		if (pPostInfo->GetNZBInfo() == pNZBInfo &&
			(!bParCheck || (pPostInfo->GetParCheck() && 
			 !strcmp(pPostInfo->GetParFilename(), szParFilename))))
		{
			return true;
		}
	}
	return false;
}

/**
 * Delete info about completed par-jobs for nzb-collection after the collection is completely downloaded.
 */
bool PrePostProcessor::ClearCompletedJobs(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	bool bListChanged = false;

	for (PostQueue::iterator it = pDownloadQueue->GetCompletedPostList()->begin(); it != pDownloadQueue->GetCompletedPostList()->end();)
	{
		PostInfo* pPostInfo = *it;
		if (pPostInfo->GetNZBInfo() == pNZBInfo)
		{
			debug("Deleting completed job %s", pPostInfo->GetInfoName());
			pDownloadQueue->GetCompletedPostList()->erase(it);
			delete pPostInfo;
			it = pDownloadQueue->GetCompletedPostList()->begin();
			bListChanged = true;
			continue;
		}
		it++;
	}

	return bListChanged;
}

//*********************************************************************************
// PAR-HANDLING

void PrePostProcessor::PausePars(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	debug("PrePostProcessor: Pausing pars");
	
	for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (pFileInfo->GetNZBInfo() == pNZBInfo)
		{
			g_pQueueCoordinator->GetQueueEditor()->LockedEditEntry(pDownloadQueue, pFileInfo->GetID(), false, 
				(g_pOptions->GetLoadPars() == Options::lpOne ||
					(g_pOptions->GetLoadPars() == Options::lpNone && g_pOptions->GetParCheck()))
				? QueueEditor::eaGroupPauseExtraPars : QueueEditor::eaGroupPauseAllPars,
				0, NULL);
			break;
		}
	}
}

bool PrePostProcessor::IsNZBFileCompleted(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo,
	bool bIgnoreFirstInPostQueue, bool bIgnorePaused, bool bCheckPostQueue, bool bAllowOnlyOneDeleted)
{
	bool bNZBFileCompleted = true;
	int iDeleted = 0;

	for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (pFileInfo->GetNZBInfo() == pNZBInfo)
		{
			if (pFileInfo->GetDeleted())
			{
				iDeleted++;
			}
			if (((!bIgnorePaused || !pFileInfo->GetPaused()) && !pFileInfo->GetDeleted()) ||
				(bAllowOnlyOneDeleted && iDeleted > 1))
			{
				bNZBFileCompleted = false;
				break;
			}
		}
	}

	if (bNZBFileCompleted && bCheckPostQueue)
	{
		for (PostQueue::iterator it = pDownloadQueue->GetPostQueue()->begin() + int(bIgnoreFirstInPostQueue); it != pDownloadQueue->GetPostQueue()->end(); it++)
		{
			PostInfo* pPostInfo = *it;
			if (pPostInfo->GetNZBInfo() == pNZBInfo)
			{
				bNZBFileCompleted = false;
				break;
			}
		}
	}

	return bNZBFileCompleted;
}

bool PrePostProcessor::CreatePostJobs(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo,
	bool bParCheck, bool bAddTop)
{
	debug("Queueing post-process-jobs");

	if (!bParCheck && !m_bPostScript)
	{
		// par-check is disabled and postprocess is also disabled
		// no jobs can be created
		return false;
	}

	char szNZBNiceName[1024];
	pNZBInfo->GetNiceNZBName(szNZBNiceName, 1024);

	PostQueue cPostQueue;

	FileList fileList;
	if (FindMainPars(pNZBInfo->GetDestDir(), &fileList))
	{
		debug("Found pars");
		
		for (FileList::iterator it = fileList.begin(); it != fileList.end(); it++)
		{
			char* szParFilename = *it;
			debug("Found par: %s", szParFilename);

			char szFullParFilename[1024];
			snprintf(szFullParFilename, 1024, "%s%c%s", pNZBInfo->GetDestDir(), (int)PATH_SEPARATOR, szParFilename);
			szFullParFilename[1024-1] = '\0';

			bool bJobExists = JobExists(pDownloadQueue->GetPostQueue(), pNZBInfo, szFullParFilename, bParCheck) ||
				JobExists(pDownloadQueue->GetCompletedPostList(), pNZBInfo, szFullParFilename, bParCheck);
			if (!bJobExists || (m_bPostScript && g_pOptions->GetAllowReProcess()))
			{
				char szInfoName[1024];
				int iBaseLen = 0;
				ParseParFilename(szParFilename, &iBaseLen, NULL);
				int maxlen = iBaseLen < 1024 ? iBaseLen : 1024 - 1;
				strncpy(szInfoName, szParFilename, maxlen);
				szInfoName[maxlen] = '\0';
				
				char szParInfoName[1024];
				snprintf(szParInfoName, 1024, "%s%c%s", szNZBNiceName, (int)PATH_SEPARATOR, szInfoName);
				szParInfoName[1024-1] = '\0';
				
				info("Queueing %s%c%s for par-check", szNZBNiceName, (int)PATH_SEPARATOR, szInfoName);
				PostInfo* pPostInfo = new PostInfo();
				pPostInfo->SetNZBInfo(pNZBInfo);
				pPostInfo->SetParFilename(szFullParFilename);
				pPostInfo->SetInfoName(szParInfoName);
				pPostInfo->SetParCheck(bParCheck && !bJobExists);
				if (bAddTop)
				{
					cPostQueue.push_front(pPostInfo);
				}
				else
				{
					cPostQueue.push_back(pPostInfo);
				}
			}

			free(szParFilename);
		}
	}

	if (cPostQueue.empty() && m_bPostScript && 
		(!(JobExists(pDownloadQueue->GetPostQueue(), pNZBInfo, NULL, false) || 
		   JobExists(pDownloadQueue->GetCompletedPostList(), pNZBInfo, NULL, false)) ||
		g_pOptions->GetAllowReProcess()))
	{
		info("Queueing %s for post-process-script", szNZBNiceName);
		PostInfo* pPostInfo = new PostInfo();
		pPostInfo->SetNZBInfo(pNZBInfo);
		pPostInfo->SetParFilename("");
		pPostInfo->SetInfoName(szNZBNiceName);
		pPostInfo->SetParCheck(false);
		cPostQueue.push_back(pPostInfo);
	}

	for (PostQueue::iterator it = cPostQueue.begin(); it != cPostQueue.end(); it++)
	{
		if (bAddTop)
		{
			pDownloadQueue->GetPostQueue()->push_front(*it);
		}
		else
		{
			pDownloadQueue->GetPostQueue()->push_back(*it);
		}
	}

	SavePostQueue(pDownloadQueue);
	m_bHasMoreJobs = true;

	return true;
}

bool PrePostProcessor::FindMainPars(const char * szPath, FileList * pFileList)
{
	pFileList->clear();
	DirBrowser dir(szPath);
	while (const char* filename = dir.Next())
	{
		int iBaseLen = 0;
		if (ParseParFilename(filename, &iBaseLen, NULL))
		{
			// check if the base file already added to list
			bool exists = false;
			for (FileList::iterator it = pFileList->begin(); it != pFileList->end(); it++)
			{
				const char* filename2 = *it;
				exists = SameParCollection(filename, filename2);
				if (exists)
				{
					break;
				}
			}
			if (!exists)
			{
				pFileList->push_back(strdup(filename));
			}
		}
	}
	return !pFileList->empty();
}

bool PrePostProcessor::SameParCollection(const char* szFilename1, const char* szFilename2)
{
	int iBaseLen1 = 0, iBaseLen2 = 0;
	return ParseParFilename(szFilename1, &iBaseLen1, NULL) &&
		ParseParFilename(szFilename2, &iBaseLen2, NULL) &&
		iBaseLen1 == iBaseLen2 &&
		!strncasecmp(szFilename1, szFilename2, iBaseLen1);
}

bool PrePostProcessor::ParseParFilename(const char * szParFilename, int* iBaseNameLen, int* iBlocks)
{
	char szFilename[1024];
	strncpy(szFilename, szParFilename, 1024);
	szFilename[1024-1] = '\0';
	for (char* p = szFilename; *p; p++) *p = tolower(*p); // convert string to lowercase

	int iLen = strlen(szFilename);
	if (iLen < 6)
	{
		return false;
	}

	// find last occurence of ".par2" and trim filename after it
	char* szEnd = szFilename;
	while (char* p = strstr(szEnd, ".par2")) szEnd = p + 5;
	*szEnd = '\0';
	iLen = strlen(szFilename);
	
	if (strcasecmp(szFilename + iLen - 5, ".par2"))
	{
		return false;
	}
	*(szFilename + iLen - 5) = '\0';

	int blockcnt = 0;
	char* p = strrchr(szFilename, '.');
	if (p && !strncasecmp(p, ".vol", 4))
	{
		char* b = strchr(p, '+');
		if (!b)
		{
			b = strchr(p, '-');
		}
		if (b)
		{
			blockcnt = atoi(b+1);
			*p = '\0';
		}
	}

	if (iBaseNameLen)
	{
		*iBaseNameLen = strlen(szFilename);
	}
	if (iBlocks)
	{
		*iBlocks = blockcnt;
	}
	
	return true;
}

#ifndef DISABLE_PARCHECK

/**
 * DownloadQueue must be locked prior to call of this function.
 */
void PrePostProcessor::StartParJob(PostInfo* pPostInfo)
{
	if (g_pOptions->GetParPauseQueue())
	{
		if (PauseDownload())
		{
			info("Pausing queue before par-check");
		}
	}

	info("Checking pars for %s", pPostInfo->GetInfoName());
	m_ParChecker.SetPostInfo(pPostInfo);
	m_ParChecker.SetParFilename(pPostInfo->GetParFilename());
	m_ParChecker.SetInfoName(pPostInfo->GetInfoName());
	pPostInfo->SetWorking(true);
	m_ParChecker.Start();
}

/**
 * DownloadQueue must be locked prior to call of this function.
 */
bool PrePostProcessor::AddPar(FileInfo * pFileInfo, bool bDeleted)
{
	bool bSameCollection = m_ParChecker.IsRunning() &&
		pFileInfo->GetNZBInfo() == m_ParChecker.GetPostInfo()->GetNZBInfo() &&
		SameParCollection(pFileInfo->GetFilename(), Util::BaseFileName(m_ParChecker.GetParFilename()));
	if (bSameCollection && !bDeleted)
	{
		char szFullFilename[1024];
		snprintf(szFullFilename, 1024, "%s%c%s", pFileInfo->GetNZBInfo()->GetDestDir(), (int)PATH_SEPARATOR, pFileInfo->GetFilename());
		szFullFilename[1024-1] = '\0';
		m_ParChecker.AddParFile(szFullFilename);

		if (g_pOptions->GetParPauseQueue())
		{
			PauseDownload();
		}
	}
	else
	{
		m_ParChecker.QueueChanged();
	}
	return bSameCollection;
}

void PrePostProcessor::ParCheckerUpdate(Subject* Caller, void* Aspect)
{
	if (m_ParChecker.GetStatus() == ParChecker::psFinished ||
		m_ParChecker.GetStatus() == ParChecker::psFailed)
	{
		char szPath[1024];
		strncpy(szPath, m_ParChecker.GetParFilename(), 1024);
		szPath[1024-1] = '\0';
		if (char* p = strrchr(szPath, PATH_SEPARATOR)) *p = '\0';

		if (g_pOptions->GetCreateBrokenLog())
		{
			char szBrokenLogName[1024];
			snprintf(szBrokenLogName, 1024, "%s%c_brokenlog.txt", szPath, (int)PATH_SEPARATOR);
			szBrokenLogName[1024-1] = '\0';
			
			if (!m_ParChecker.GetRepairNotNeeded() || Util::FileExists(szBrokenLogName))
			{
				FILE* file = fopen(szBrokenLogName, "a");
				if (file)
				{
					if (m_ParChecker.GetStatus() == ParChecker::psFailed)
					{
						if (m_ParChecker.GetCancelled())
						{
							fprintf(file, "Repair cancelled for %s\n", m_ParChecker.GetInfoName());
						}
						else
						{
							fprintf(file, "Repair failed for %s: %s\n", m_ParChecker.GetInfoName(), m_ParChecker.GetErrMsg() ? m_ParChecker.GetErrMsg() : "");
						}
					}
					else if (m_ParChecker.GetRepairNotNeeded())
					{
						fprintf(file, "Repair not needed for %s\n", m_ParChecker.GetInfoName());
					}
					else
					{
						if (g_pOptions->GetParRepair())
						{
							fprintf(file, "Successfully repaired %s\n", m_ParChecker.GetInfoName());
						}
						else
						{
							fprintf(file, "Repair possible for %s\n", m_ParChecker.GetInfoName());
						}
					}
					fclose(file);
				}
				else
				{
					error("Could not open file %s", szBrokenLogName);
				}
			}
		}

		DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

		PostInfo* pPostInfo = m_ParChecker.GetPostInfo();
		pPostInfo->SetWorking(false);
		if (pPostInfo->GetDeleted())
		{
			pPostInfo->SetStage(PostInfo::ptFinished);
		}
		else
		{
			pPostInfo->SetStage(PostInfo::ptQueued);
		}

		if (m_ParChecker.GetStatus() == ParChecker::psFailed && !m_ParChecker.GetCancelled())
		{
			pPostInfo->SetParStatus(PARSTATUS_FAILED);
		}
		else if (m_ParChecker.GetStatus() == ParChecker::psFinished &&
			(g_pOptions->GetParRepair() || m_ParChecker.GetRepairNotNeeded()))
		{
			pPostInfo->SetParStatus(PARSTATUS_REPAIRED);
		}
		else
		{
			pPostInfo->SetParStatus(PARSTATUS_REPAIR_POSSIBLE);
		}

		SavePostQueue(pDownloadQueue);

		g_pQueueCoordinator->UnlockQueue();

		if (g_pOptions->GetParPauseQueue() && !(g_pOptions->GetPostPauseQueue() && m_bPostScript))
		{
			if (UnpauseDownload())
			{
				info("Unpausing queue after par-check");
			}
		}
	}
}

/**
 * Check if the deletion of unneeded (paused) par-files from download queue after successful par-check is possible.
 * If the collection has paused non-par-files, none files will be deleted (even pars).
 * Returns fileinfo, which ID can be later used in call to GetQueueEditor()->EditEntry()
 */
FileInfo* PrePostProcessor::GetParCleanupQueueGroup(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	debug("Preparing to cleaning up download queue from par-files");

	// check if nzb-file has only pars paused
	FileInfo* pRetFileInfo = NULL;
	for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (pFileInfo->GetNZBInfo() == pNZBInfo)
		{
			if (!pFileInfo->GetPaused() && !pFileInfo->GetDeleted() &&
				!ParseParFilename(pFileInfo->GetFilename(), NULL, NULL))
			{
				return NULL;
			}
			pRetFileInfo = pFileInfo;
		}
	}
	return pRetFileInfo;
}

/**
 * Check if nzb-file has failures from other par-jobs
 * (if nzb-file has more than one collections)
 */
bool PrePostProcessor::HasFailedParJobs(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, bool bIgnoreRepairPossible)
{
	bool bHasFailedJobs = false;

	for (PostQueue::iterator it = pDownloadQueue->GetCompletedPostList()->begin(); it != pDownloadQueue->GetCompletedPostList()->end(); it++)
	{
		PostInfo* pPostInfo = *it;
		if (pPostInfo->GetParCheck() && 
			(pPostInfo->GetParStatus() == PARSTATUS_FAILED || 
			 (!bIgnoreRepairPossible && pPostInfo->GetParStatus() == PARSTATUS_REPAIR_POSSIBLE)) &&
			pPostInfo->GetNZBInfo() == pNZBInfo)
		{
			bHasFailedJobs = true;
			break;
		}
	}

	return bHasFailedJobs;
}

/**
* Unpause par2-files
* returns true, if the files with required number of blocks were unpaused,
* or false if there are no more files in queue for this collection or not enough blocks
*/
bool PrePostProcessor::RequestMorePars(NZBInfo* pNZBInfo, const char* szParFilename, int iBlockNeeded, int* pBlockFound)
{
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();
	
	Blocks blocks;
	blocks.clear();
	int iBlockFound = 0;

	FindPars(pDownloadQueue, pNZBInfo, szParFilename, &blocks, true, true, &iBlockFound);
	if (iBlockFound == 0)
	{
		FindPars(pDownloadQueue, pNZBInfo, szParFilename, &blocks, true, false, &iBlockFound);
	}
	if (iBlockFound == 0 && !g_pOptions->GetStrictParName())
	{
		FindPars(pDownloadQueue, pNZBInfo, szParFilename, &blocks, false, false, &iBlockFound);
	}

	if (iBlockFound >= iBlockNeeded)
	{
		char szNZBNiceName[1024];
		pNZBInfo->GetNiceNZBName(szNZBNiceName, 1024);

		// 1. first unpause all files with par-blocks less or equal iBlockNeeded
		// starting from the file with max block count.
		// if par-collection was built exponentially and all par-files present,
		// this step selects par-files with exact number of blocks we need.
		while (iBlockNeeded > 0)
		{               
			BlockInfo* pBestBlockInfo = NULL;
			for (Blocks::iterator it = blocks.begin(); it != blocks.end(); it++)
			{
				BlockInfo* pBlockInfo = *it;
				if (pBlockInfo->m_iBlockCount <= iBlockNeeded &&
				   (!pBestBlockInfo || pBestBlockInfo->m_iBlockCount < pBlockInfo->m_iBlockCount))
				{
					pBestBlockInfo = pBlockInfo;
				}
			}
			if (pBestBlockInfo)
			{
				if (pBestBlockInfo->m_pFileInfo->GetPaused())
				{
					info("Unpausing %s%c%s for par-recovery", szNZBNiceName, (int)PATH_SEPARATOR, pBestBlockInfo->m_pFileInfo->GetFilename());
					pBestBlockInfo->m_pFileInfo->SetPaused(false);
				}
				iBlockNeeded -= pBestBlockInfo->m_iBlockCount;
				blocks.remove(pBestBlockInfo);
				delete pBestBlockInfo;
			}
			else
			{
				break;
			}
		}
			
		// 2. then unpause other files
		// this step only needed if the par-collection was built not exponentially 
		// or not all par-files present (or some of them were corrupted)
		// this step is not optimal, but we hope, that the first step will work good 
		// in most cases and we will not need the second step often
		while (iBlockNeeded > 0)
		{
			BlockInfo* pBlockInfo = blocks.front();
			if (pBlockInfo->m_pFileInfo->GetPaused())
			{
				info("Unpausing %s%c%s for par-recovery", szNZBNiceName, (int)PATH_SEPARATOR, pBlockInfo->m_pFileInfo->GetFilename());
				pBlockInfo->m_pFileInfo->SetPaused(false);
			}
			iBlockNeeded -= pBlockInfo->m_iBlockCount;
		}
	}

	g_pQueueCoordinator->UnlockQueue();

	if (pBlockFound)
	{
		*pBlockFound = iBlockFound;
	}

	for (Blocks::iterator it = blocks.begin(); it != blocks.end(); it++)
	{
		delete *it;
	}
	blocks.clear();

	bool bOK = iBlockNeeded <= 0;

	if (bOK && g_pOptions->GetParPauseQueue())
	{
		UnpauseDownload();
	}

	return bOK;
}

void PrePostProcessor::FindPars(DownloadQueue * pDownloadQueue, NZBInfo* pNZBInfo, const char* szParFilename,
	Blocks * pBlocks, bool bStrictParName, bool bExactParName, int* pBlockFound)
{
    *pBlockFound = 0;
	
	// extract base name from m_szParFilename (trim .par2-extension and possible .vol-part)
	char* szBaseParFilename = Util::BaseFileName(szParFilename);
	char szMainBaseFilename[1024];
	int iMainBaseLen = 0;
	if (!ParseParFilename(szBaseParFilename, &iMainBaseLen, NULL))
	{
		// should not happen
        error("Internal error: could not parse filename %s", szBaseParFilename);
		return;
	}
	int maxlen = iMainBaseLen < 1024 ? iMainBaseLen : 1024 - 1;
	strncpy(szMainBaseFilename, szBaseParFilename, maxlen);
	szMainBaseFilename[maxlen] = '\0';
	for (char* p = szMainBaseFilename; *p; p++) *p = tolower(*p); // convert string to lowercase

	for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		int iBlocks = 0;
		if (pFileInfo->GetNZBInfo() == pNZBInfo &&
			ParseParFilename(pFileInfo->GetFilename(), NULL, &iBlocks) &&
			iBlocks > 0)
		{
			bool bUseFile = true;

			if (bExactParName)
			{
				bUseFile = SameParCollection(pFileInfo->GetFilename(), Util::BaseFileName(szParFilename));
			}
			else if (bStrictParName)
			{
				// the pFileInfo->GetFilename() may be not confirmed and may contain
				// additional texts if Subject could not be parsed correctly

				char szLoFileName[1024];
				strncpy(szLoFileName, pFileInfo->GetFilename(), 1024);
				szLoFileName[1024-1] = '\0';
				for (char* p = szLoFileName; *p; p++) *p = tolower(*p); // convert string to lowercase
				
				char szCandidateFileName[1024];
				snprintf(szCandidateFileName, 1024, "%s.par2", szMainBaseFilename);
				szCandidateFileName[1024-1] = '\0';
				if (!strstr(szLoFileName, szCandidateFileName))
				{
					snprintf(szCandidateFileName, 1024, "%s.vol", szMainBaseFilename);
					szCandidateFileName[1024-1] = '\0';
					bUseFile = strstr(szLoFileName, szCandidateFileName);
				}
			}

			// if it is a par2-file with blocks and it was from the same NZB-request
			// and it belongs to the same file collection (same base name),
			// then OK, we can use it
			if (bUseFile)
			{
				BlockInfo* pBlockInfo = new BlockInfo();
				pBlockInfo->m_pFileInfo = pFileInfo;
				pBlockInfo->m_iBlockCount = iBlocks;
				pBlocks->push_back(pBlockInfo);
				*pBlockFound += iBlocks;
			}
		}
	}
}

void PrePostProcessor::UpdateParProgress()
{
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	PostInfo* pPostInfo = pDownloadQueue->GetPostQueue()->front();
	if (m_ParChecker.GetFileProgress() == 0)
	{
		pPostInfo->SetProgressLabel(m_ParChecker.GetProgressLabel());
	}
	pPostInfo->SetFileProgress(m_ParChecker.GetFileProgress());
	pPostInfo->SetStageProgress(m_ParChecker.GetStageProgress());
    PostInfo::EStage StageKind[] = { PostInfo::ptLoadingPars, PostInfo::ptVerifyingSources, PostInfo::ptRepairing, PostInfo::ptVerifyingRepaired };
	PostInfo::EStage eStage = StageKind[m_ParChecker.GetStage()];
	time_t tCurrent = time(NULL);

	if (!pPostInfo->GetStartTime())
	{
		pPostInfo->SetStartTime(tCurrent);
	}

	if (pPostInfo->GetStage() != eStage)
	{
		pPostInfo->SetStage(eStage);
		pPostInfo->SetStageTime(tCurrent);
	}

	bool bParCancel = false;
#ifdef HAVE_PAR2_CANCEL
	if (!m_ParChecker.GetCancelled())
	{
		if ((g_pOptions->GetParTimeLimit() > 0) &&
			m_ParChecker.GetStage() == ParChecker::ptRepairing &&
			((g_pOptions->GetParTimeLimit() > 5 && tCurrent - pPostInfo->GetStageTime() > 5 * 60) ||
			(g_pOptions->GetParTimeLimit() <= 5 && tCurrent - pPostInfo->GetStageTime() > 1 * 60)))
		{
			// first five (or one) minutes elapsed, now can check the estimated time
			int iEstimatedRepairTime = (int)((tCurrent - pPostInfo->GetStartTime()) * 1000 / 
				(pPostInfo->GetStageProgress() > 0 ? pPostInfo->GetStageProgress() : 1));
			if (iEstimatedRepairTime > g_pOptions->GetParTimeLimit() * 60)
			{
				debug("Estimated repair time %i seconds", iEstimatedRepairTime);
				warn("Cancelling par-repair for %s, estimated repair time (%i minutes) exceeds allowed repair time", m_ParChecker.GetInfoName(), iEstimatedRepairTime / 60);
				bParCancel = true;
			}
		}
	}
#endif

	if (bParCancel)
	{
		m_ParChecker.Cancel();
	}

	g_pQueueCoordinator->UnlockQueue();
}

#endif

void PrePostProcessor::ApplySchedulerState()
{
	if (g_pScheduler->GetDownloadRateChanged())
	{
		info("Scheduler: set download rate to %i KB/s", g_pScheduler->GetDownloadRate());
		g_pOptions->SetDownloadRate((float)g_pScheduler->GetDownloadRate());
	}

	if (g_pScheduler->GetPauseChanged())
	{
		info("Scheduler: %s download queue", g_pScheduler->GetPause() ? "pause" : "unpause");
		m_bSchedulerPauseChanged = true;
		m_bSchedulerPause = g_pScheduler->GetPause();
		if (!m_bPostPause)
		{
			g_pOptions->SetPause(m_bSchedulerPause);
		}
	}
}

bool PrePostProcessor::PauseDownload()
{
	debug("PrePostProcessor::PauseDownload()");

	if (m_bPostPause && g_pOptions->GetPause())
	{
		return false;
	}

	m_bPostPause = !g_pOptions->GetPause();
	m_bSchedulerPauseChanged = false;
	g_pOptions->SetPause(true);
	return m_bPostPause;
}

bool PrePostProcessor::UnpauseDownload()
{
	debug("PrePostProcessor::UnpauseDownload()");

	bool bPause = true;
	if (m_bPostPause)
	{
		m_bPostPause = false;
		bPause = m_bSchedulerPauseChanged && m_bSchedulerPause;
		g_pOptions->SetPause(bPause);
	}
	return !bPause;
}

bool PrePostProcessor::QueueEditList(IDList* pIDList, EEditAction eAction, int iOffset)
{
	debug("Edit-command for post-processor-queue received");
	if (eAction == eaPostDelete)
	{
		return QueueDelete(pIDList);
	}
	else
	{
		return QueueMove(pIDList, eAction, iOffset);
	}
}

bool PrePostProcessor::QueueDelete(IDList* pIDList)
{
	bool bOK = false;

	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	for (IDList::iterator itID = pIDList->begin(); itID != pIDList->end(); itID++)
	{
		int iID = *itID;
		for (PostQueue::iterator itPost = pDownloadQueue->GetPostQueue()->begin(); itPost != pDownloadQueue->GetPostQueue()->end(); itPost++)
		{
			PostInfo* pPostInfo = *itPost;
			if (pPostInfo->GetID() == iID)
			{
				if (pPostInfo->GetWorking())
				{
					info("Deleting active post-job %s", pPostInfo->GetInfoName());
					pPostInfo->SetDeleted(true);
#ifndef DISABLE_PARCHECK
					if (PostInfo::ptLoadingPars <= pPostInfo->GetStage() && pPostInfo->GetStage() <= PostInfo::ptVerifyingRepaired)
					{
#ifdef HAVE_PAR2_CANCEL
						if (!m_ParChecker.GetCancelled())
						{
							debug("Cancelling par-repair for %s", m_ParChecker.GetInfoName());
							m_ParChecker.Cancel();
							bOK = true;
						}
#else
						warn("Cannot cancel par-repair for %s, used version of libpar2 does not support cancelling", m_ParChecker.GetInfoName());
#endif
					}
					else
#endif
					if (pPostInfo->GetScriptThread())
					{
						debug("Terminating post-process-script for %s", pPostInfo->GetInfoName());
						pPostInfo->GetScriptThread()->Stop();
						bOK = true;
					}
					else
					{
						error("Internal error in PrePostProcessor::QueueDelete");
					}
				}
				else
				{
					info("Deleting queued post-job %s", pPostInfo->GetInfoName());
					JobCompleted(pDownloadQueue, pPostInfo);
					bOK = true;
				}
				break;
			}
		}
	}

	g_pQueueCoordinator->UnlockQueue();

	return bOK;
}

bool PrePostProcessor::QueueMove(IDList* pIDList, EEditAction eAction, int iOffset)
{
	if (pIDList->size() != 1)
	{
		//NOTE: Only one post-job can be moved at once
		return false;
	}

	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	bool bOK = false;

	int iID = pIDList->front();
	unsigned int iIndex = 0;
	PostInfo* pPostInfo = NULL;

	for (PostQueue::iterator it = pDownloadQueue->GetPostQueue()->begin(); it != pDownloadQueue->GetPostQueue()->end(); it++)
	{
		PostInfo* pPostInfo1 = *it;
		if (pPostInfo1->GetID() == iID)
		{
			pPostInfo = pPostInfo1;
			break;
		}
		iIndex++;
	}

	if (pPostInfo)
	{
		// NOTE: only items which are not currently being processed can be moved

		unsigned int iNewIndex = 0;
		switch (eAction)
		{
			case eaPostMoveTop:
				iNewIndex = 1;
				break;

			case eaPostMoveBottom:
				iNewIndex = pDownloadQueue->GetPostQueue()->size() - 1;
				break;

			case eaPostMoveOffset:
				iNewIndex = iIndex + iOffset;
				break;
				
			default: ; // suppress compiler warning
		}

		if (iNewIndex < 1)
		{
			iNewIndex = 1;
		}
		else if (iNewIndex > pDownloadQueue->GetPostQueue()->size() - 1)
		{
			iNewIndex = pDownloadQueue->GetPostQueue()->size() - 1;
		}

		if (0 < iNewIndex && iNewIndex < pDownloadQueue->GetPostQueue()->size() && iNewIndex != iIndex)
		{
			pDownloadQueue->GetPostQueue()->erase(pDownloadQueue->GetPostQueue()->begin() + iIndex);
			pDownloadQueue->GetPostQueue()->insert(pDownloadQueue->GetPostQueue()->begin() + iNewIndex, pPostInfo);
			SavePostQueue(pDownloadQueue);
			bOK = true;
		}
	}

	g_pQueueCoordinator->UnlockQueue();

	return bOK;
}
