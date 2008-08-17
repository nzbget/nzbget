/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2008 Andrei Prygounkov <hugbug@users.sourceforge.net>
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
#ifndef WIN32
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

extern QueueCoordinator* g_pQueueCoordinator;
extern Options* g_pOptions;
extern DiskState* g_pDiskState;

static const int PARSTATUS_NOT_CHECKED = 0;
static const int PARSTATUS_FAILED = 1;
static const int PARSTATUS_REPAIRED = 2;
static const int PARSTATUS_REPAIR_POSSIBLE = 3;

#ifndef DISABLE_PARCHECK
bool PrePostProcessor::PostParChecker::RequestMorePars(int iBlockNeeded, int* pBlockFound)
{
	return m_Owner->RequestMorePars(GetNZBFilename(), GetParFilename(), iBlockNeeded, pBlockFound);
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

	m_QueueCoordinatorObserver.owner = this;
	g_pQueueCoordinator->Attach(&m_QueueCoordinatorObserver);

	m_PostQueue.clear();
	m_CompletedJobs.clear();

	const char* szScript = g_pOptions->GetPostProcess();
	m_bPostScript = szScript && strlen(szScript) > 0;

#ifndef DISABLE_PARCHECK
	m_ParCheckerObserver.owner = this;
	m_ParChecker.Attach(&m_ParCheckerObserver);
	m_ParChecker.m_Owner = this;
#endif
}

PrePostProcessor::~PrePostProcessor()
{
	debug("Destroying PrePostProcessor");
	
	for (PostQueue::iterator it = m_PostQueue.begin(); it != m_PostQueue.end(); it++)
	{
		delete *it;
	}

#ifndef DISABLE_PARCHECK
	for (PostQueue::iterator it = m_CompletedJobs.begin(); it != m_CompletedJobs.end(); it++)
	{
		delete *it;
	}
#endif
}

void PrePostProcessor::Run()
{
	debug("Entering PrePostProcessor-loop");

	if (g_pOptions->GetServerMode() && g_pOptions->GetSaveQueue())
	{
		if (g_pOptions->GetReloadQueue() && g_pOptions->GetReloadPostQueue())
		{
			m_mutexQueue.Lock();
			if (g_pDiskState->PostQueueExists(false))
			{
				g_pDiskState->LoadPostQueue(&m_PostQueue, false);
				SanitisePostQueue();
			}
			if (g_pDiskState->PostQueueExists(true))
			{
				g_pDiskState->LoadPostQueue(&m_CompletedJobs, true);
			}
			m_mutexQueue.Unlock();
		}
		else
		{
			g_pDiskState->DiscardPostQueue();
		}
	}

	int iNZBDirInterval = g_pOptions->GetNzbDirInterval() * 1000;
	int iDiskSpaceInterval = 1000;
	while (!IsStopped())
	{
		if (g_pOptions->GetNzbDir() && g_pOptions->GetNzbDirInterval() > 0 && 
			iNZBDirInterval >= g_pOptions->GetNzbDirInterval() * 1000)
		{
			// check nzbdir every g_pOptions->GetNzbDirInterval() seconds
			CheckIncomingNZBs(g_pOptions->GetNzbDir(), "");
			iNZBDirInterval = 0;
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

		usleep(200 * 1000);
	}

	debug("Exiting PrePostProcessor-loop");
}

void PrePostProcessor::Stop()
{
	Thread::Stop();
	m_mutexQueue.Lock();

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

	if (!m_PostQueue.empty())
	{
		PostInfo* pPostInfo = m_PostQueue.front();
		if (pPostInfo->GetStage() == PostInfo::ptExecutingScript && pPostInfo->GetScriptThread())
		{
			Thread* pScriptThread = pPostInfo->GetScriptThread();
			pPostInfo->SetScriptThread(NULL);
			pScriptThread->SetAutoDestroy(true);
			pScriptThread->Stop();
		}
	}

	m_mutexQueue.Unlock();
}

void PrePostProcessor::QueueCoordinatorUpdate(Subject * Caller, void * Aspect)
{
	if (IsStopped())
	{
		return;
	}

	QueueCoordinator::Aspect* pAspect = (QueueCoordinator::Aspect*)Aspect;
	if (pAspect->eAction == QueueCoordinator::eaNZBFileAdded &&
		g_pOptions->GetLoadPars() != Options::lpAll)
	{
		PausePars(pAspect->pDownloadQueue, pAspect->szNZBFilename);
	}
	else if ((pAspect->eAction == QueueCoordinator::eaFileCompleted ||
		pAspect->eAction == QueueCoordinator::eaFileDeleted))
	{
		if (
#ifndef DISABLE_PARCHECK
			!AddPar(pAspect->pFileInfo, pAspect->eAction == QueueCoordinator::eaFileDeleted) &&
#endif
			IsNZBFileCompleted(pAspect->pDownloadQueue, pAspect->pFileInfo->GetNZBInfo()->GetFilename(), false, true, false) &&
			(!pAspect->pFileInfo->GetPaused() || IsNZBFileCompleted(pAspect->pDownloadQueue, pAspect->pFileInfo->GetNZBInfo()->GetFilename(), false, false, false)))
		{
			char szNZBNiceName[1024];
			pAspect->pFileInfo->GetNZBInfo()->GetNiceNZBName(szNZBNiceName, 1024);
			if (pAspect->eAction == QueueCoordinator::eaFileCompleted)
			{
				info("Collection %s completely downloaded", szNZBNiceName);
			}
			else if (IsNZBFileCompleted(pAspect->pDownloadQueue, pAspect->pFileInfo->GetNZBInfo()->GetFilename(), false, false, false))
			{
				info("Collection %s deleted from queue", szNZBNiceName);
			}

			if (pAspect->eAction == QueueCoordinator::eaFileCompleted)
			{
				pAspect->pFileInfo->GetNZBInfo()->SetPostProcess(true);
#ifndef DISABLE_PARCHECK
				(g_pOptions->GetParCheck() && g_pOptions->GetDecode() && 
					CheckPars(pAspect->pDownloadQueue, pAspect->pFileInfo)) ||
#endif
				CheckScript(pAspect->pFileInfo);
			}
		}

		m_mutexQueue.Lock();
		if (IsNZBFileCompleted(pAspect->pDownloadQueue, pAspect->pFileInfo->GetNZBInfo()->GetFilename(), false, false, true))
		{
			if (ClearCompletedJobs(pAspect->pFileInfo->GetNZBInfo()->GetFilename()) &&
				g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
			{
				g_pDiskState->SavePostQueue(&m_CompletedJobs, true);
			}
		}
		m_mutexQueue.Unlock();
	}
}

/**
* Check if there are files in directory for incoming nzb-files
* and add them to download queue
*/
void PrePostProcessor::CheckIncomingNZBs(const char* szDirectory, const char* szCategory)
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
			int len = strlen(filename);

			// check subfolders of first level
			if (strlen(szCategory) == 0 && (buffer.st_mode & S_IFDIR) != 0 && strcmp(filename, ".") && strcmp(filename, ".."))
			{
				fullfilename[strlen(fullfilename) + 1] = '\0';
				fullfilename[strlen(fullfilename)] = PATH_SEPARATOR;
				CheckIncomingNZBs(fullfilename, filename);
			}
			else if (len > 4 && !strcasecmp(filename + len - 4, ".nzb"))
			{
				// file found, checking modification-time
				if (time(NULL) - buffer.st_mtime > g_pOptions->GetNzbDirFileAge() &&
					time(NULL) - buffer.st_ctime > g_pOptions->GetNzbDirFileAge())
				{
					// the file is at least g_pOptions->GetNzbDirFileAge() seconds old, we can process it
					AddFileToQueue(fullfilename, szCategory);
				}
			}
		}
	}
}

void PrePostProcessor::AddFileToQueue(const char* szFilename, const char* szCategory)
{
	const char* szBasename = Util::BaseFileName(szFilename);

	info("Collection %s found", szBasename);
	char bakname[1024];
	bool bAdded = g_pQueueCoordinator->AddFileToQueue(szFilename, szCategory);
	if (bAdded)
	{
		info("Collection %s added to queue", szBasename);
		snprintf(bakname, 1024, "%s.queued", szFilename);
		bakname[1024-1] = '\0';
	}
	else
	{
		error("Could not add collection %s to queue", szBasename);
		snprintf(bakname, 1024, "%s.error", szFilename);
		bakname[1024-1] = '\0';
	}

	char bakname2[1024];
	strcpy(bakname2, bakname);
	int i = 2;
	struct stat buffer;
	while (!stat(bakname2, &buffer))
	{
		snprintf(bakname2, 1024, "%s%i", bakname, i++);
		bakname2[1024-1] = '\0';
	}
	
	if (rename(szFilename, bakname2))
	{
		error("Could not rename file %s to %s! Errcode: %i", szFilename, bakname2, errno);
	}
	else if (bAdded)
	{
		// find just added item in queue and save bakname2 into NZBInfo.QueuedFileName
		DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();
		for (DownloadQueue::reverse_iterator it = pDownloadQueue->rbegin(); it != pDownloadQueue->rend(); it++)
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
	char szQueuedFilename[1024];
	szQueuedFilename[0] = '\0';

	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();
	m_mutexQueue.Lock();

	if (!m_PostQueue.empty())
	{
		PostInfo* pPostInfo = m_PostQueue.front();
		if (!pPostInfo->GetWorking())
		{
#ifndef DISABLE_PARCHECK
			if (pPostInfo->GetParCheck() && pPostInfo->GetParStatus() == PARSTATUS_NOT_CHECKED)
			{
				StartParJob(pPostInfo);
			}
			else
#endif
			if (pPostInfo->GetStage() == PostInfo::ptQueued)
			{
				StartScriptJob(pDownloadQueue, pPostInfo);
			}
			else if (pPostInfo->GetStage() == PostInfo::ptFinished)
			{
#ifndef DISABLE_PARCHECK
				if ((g_pOptions->GetParCleanupQueue() || g_pOptions->GetNzbCleanupDisk()) && 
					IsNZBFileCompleted(pDownloadQueue, pPostInfo->GetNZBFilename(), true, true, true) &&
					pPostInfo->GetParStatus() != PARSTATUS_NOT_CHECKED &&
					pPostInfo->GetParStatus() != PARSTATUS_FAILED &&
					!HasFailedParJobs(pPostInfo->GetNZBFilename()))
				{
					if (g_pOptions->GetParCleanupQueue())
					{
						iCleanupGroupID = GetParCleanupQueueGroup(pDownloadQueue, pPostInfo->GetNZBFilename());
						NZBInfo::MakeNiceNZBName(pPostInfo->GetNZBFilename(), szNZBNiceName, sizeof(szNZBNiceName));
					}
					if (g_pOptions->GetNzbCleanupDisk())
					{
						strncpy(szQueuedFilename, pPostInfo->GetQueuedFilename(), 1024);
						szQueuedFilename[1024-1] = '\0';
					}
				}
#endif
				JobCompleted(pDownloadQueue, pPostInfo);
			}
			else
			{
				error("Internal error: invalid state in post-processor");
			}
		}
	}
	
	m_mutexQueue.Unlock();
	g_pQueueCoordinator->UnlockQueue();

	if (iCleanupGroupID > 0)
	{
		info("Cleaning up download queue for %s", szNZBNiceName);
		g_pQueueCoordinator->GetQueueEditor()->EditEntry(iCleanupGroupID, false, QueueEditor::eaGroupDelete, 0, NULL);
	}

	if (szQueuedFilename[0] != '\0' && Util::FileExists(szQueuedFilename))
	{
		info("Deleting file %s", szQueuedFilename);
		remove(szQueuedFilename);
	}
}

/**
 * Mutex "m_mutexQueue" must be locked prior to call of this function.
 */
void PrePostProcessor::SavePostQueue()
{
	if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
	{
		g_pDiskState->SavePostQueue(&m_PostQueue, false);
	}
}

/**
 * Reset the state of items after reloading from disk and
 * delete items which could not be resumed.
 * Mutex "m_mutexQueue" must be locked prior to call of this function.
 */
void PrePostProcessor::SanitisePostQueue()
{
	for (PostQueue::iterator it = m_PostQueue.begin(); it != m_PostQueue.end(); it++)
	{
		PostInfo* pPostInfo = *it;
		if (pPostInfo->GetStage() == PostInfo::ptExecutingScript ||
			!Util::DirectoryExists(pPostInfo->GetDestDir()))
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

bool PrePostProcessor::PostProcessEnabled()
{
	const char* szScript = g_pOptions->GetPostProcess();
	return szScript && strlen(szScript) > 0;
}

/**
 * Mutex "m_mutexQueue" must be locked prior to call of this function.
 */
void PrePostProcessor::StartScriptJob(DownloadQueue* pDownloadQueue, PostInfo* pPostInfo)
{
	if (!PostProcessEnabled())
	{
		pPostInfo->SetStage(PostInfo::ptFinished);
		return;
	}

	pPostInfo->SetProgressLabel("Executing post-process-script");
	pPostInfo->SetWorking(true);
	pPostInfo->SetStage(PostInfo::ptExecutingScript);
	pPostInfo->SetFileProgress(0);
	pPostInfo->SetStageProgress(0);
	SavePostQueue();

	if (!pPostInfo->GetStartTime())
	{
		pPostInfo->SetStartTime(time(NULL));
	}
	pPostInfo->SetStageTime(time(NULL));

	bool bNZBFileCompleted = IsNZBFileCompleted(pDownloadQueue, pPostInfo->GetNZBFilename(), true, true, true);
#ifndef DISABLE_PARCHECK
	bool bHasFailedParJobs = HasFailedParJobs(pPostInfo->GetNZBFilename()) || pPostInfo->GetParFailed();
#else
	bool bHasFailedParJobs = false;
#endif

	ScriptController::StartScriptJob(pPostInfo, g_pOptions->GetPostProcess(), bNZBFileCompleted, bHasFailedParJobs);
}

/**
 * Mutex "m_mutexQueue" must be locked prior to call of this function.
 */
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

	for (PostQueue::iterator it = m_PostQueue.begin(); it != m_PostQueue.end(); it++)
	{
		if (pPostInfo == *it)
		{
			m_PostQueue.erase(it);
			break;
		}
	}

	m_CompletedJobs.push_back(pPostInfo);

	if (IsNZBFileCompleted(pDownloadQueue, pPostInfo->GetNZBFilename(), false, false, true))
	{
		ClearCompletedJobs(pPostInfo->GetNZBFilename());
	}

	if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
	{
		g_pDiskState->SavePostQueue(&m_PostQueue, false);
		g_pDiskState->SavePostQueue(&m_CompletedJobs, true);
	}

	m_bHasMoreJobs = !m_PostQueue.empty();
}

PostQueue* PrePostProcessor::LockPostQueue()
{
	m_mutexQueue.Lock();
	return &m_PostQueue;
}

void PrePostProcessor::UnlockPostQueue()
{
	m_mutexQueue.Unlock();
}

bool PrePostProcessor::CheckScript(FileInfo * pFileInfo)
{
	bool bJobAdded = false;

	if (m_bPostScript &&
		!JobExists(&m_PostQueue, pFileInfo->GetNZBInfo()->GetFilename()) && 
		(!JobExists(&m_CompletedJobs, pFileInfo->GetNZBInfo()->GetFilename()) || g_pOptions->GetAllowReProcess()))
	{
		m_mutexQueue.Lock();

		char szNZBNiceName[1024];
		pFileInfo->GetNZBInfo()->GetNiceNZBName(szNZBNiceName, 1024);

		info("Queueing %s for post-process-script", szNZBNiceName);
		PostInfo* pPostInfo = new PostInfo();
		pPostInfo->SetNZBFilename(pFileInfo->GetNZBInfo()->GetFilename());
		pPostInfo->SetDestDir(pFileInfo->GetNZBInfo()->GetDestDir());
		pPostInfo->SetParFilename("");
		pPostInfo->SetInfoName(szNZBNiceName);
		pPostInfo->SetCategory(pFileInfo->GetNZBInfo()->GetCategory());
		pPostInfo->SetQueuedFilename(pFileInfo->GetNZBInfo()->GetQueuedFilename());
		pPostInfo->SetParCheck(false);
		m_PostQueue.push_back(pPostInfo);
		SavePostQueue();
		m_bHasMoreJobs = true;
		bJobAdded = true;

		m_mutexQueue.Unlock();
	}

	return bJobAdded;
}

/**
 * Mutex "m_mutexQueue" must be locked prior to call of this function.
 */
bool PrePostProcessor::JobExists(PostQueue* pPostQueue, const char* szNZBFilename)
{
	for (PostQueue::iterator it = pPostQueue->begin(); it != pPostQueue->end(); it++)
	{
		PostInfo* pPostInfo = *it;
		if (!strcmp(pPostInfo->GetNZBFilename(), szNZBFilename))
		{
			return true;
		}
	}
	return false;
}

/**
 * Delete info about completed par-jobs for nzb-collection after the collection is completely downloaded.
 * Mutex "m_mutexQueue" must be locked prior to call of this function.
 */
bool PrePostProcessor::ClearCompletedJobs(const char* szNZBFilename)
{
	bool bListChanged = false;

	for (PostQueue::iterator it = m_CompletedJobs.begin(); it != m_CompletedJobs.end();)
	{
		PostInfo* pPostInfo = *it;
		if (!strcmp(szNZBFilename, pPostInfo->GetNZBFilename()))
		{
			debug("Deleting completed job %s", pPostInfo->GetInfoName());
			m_CompletedJobs.erase(it);
			delete pPostInfo;
			it = m_CompletedJobs.begin();
			bListChanged = true;
			continue;
		}
		it++;
	}

	return bListChanged;
}

//*********************************************************************************
// PAR-HANDLING

void PrePostProcessor::PausePars(DownloadQueue* pDownloadQueue, const char* szNZBFilename)
{
	debug("PrePostProcessor: Pausing pars");
	
	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (!strcmp(pFileInfo->GetNZBInfo()->GetFilename(), szNZBFilename))
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

/**
 * Mutex "m_mutexQueue" must be locked prior to call of this function, 
 * if the parameter "bCheckPostQueue" is set to "true".
 */
bool PrePostProcessor::IsNZBFileCompleted(DownloadQueue* pDownloadQueue, const char* szNZBFilename, 
	bool bIgnoreFirstInPostQueue, bool bIgnorePaused, bool bCheckPostQueue)
{
	bool bNZBFileCompleted = true;

	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if ((!bIgnorePaused || !pFileInfo->GetPaused()) && !pFileInfo->GetDeleted() &&
			!strcmp(pFileInfo->GetNZBInfo()->GetFilename(), szNZBFilename))
		{
			bNZBFileCompleted = false;
			break;
		}
	}

	if (bNZBFileCompleted && bCheckPostQueue)
	{
		for (PostQueue::iterator it = m_PostQueue.begin() + int(bIgnoreFirstInPostQueue); it != m_PostQueue.end(); it++)
		{
			PostInfo* pPostInfo = *it;
			if (!strcmp(pPostInfo->GetNZBFilename(), szNZBFilename))
			{
				bNZBFileCompleted = false;
				break;
			}
		}
	}

	return bNZBFileCompleted;
}

#ifndef DISABLE_PARCHECK

/**
 * Mutex "m_mutexQueue" must be locked prior to call of this function.
 */
void PrePostProcessor::StartParJob(PostInfo* pPostInfo)
{
	if (g_pOptions->GetParPauseQueue() && !g_pOptions->GetPause())
	{
		info("Pausing queue before par-check");
		g_pOptions->SetPause(true);
	}

	info("Checking pars for %s", pPostInfo->GetInfoName());
	m_ParChecker.SetNZBFilename(pPostInfo->GetNZBFilename());
	m_ParChecker.SetParFilename(pPostInfo->GetParFilename());
	m_ParChecker.SetInfoName(pPostInfo->GetInfoName());
	pPostInfo->SetWorking(true);
	m_ParChecker.Start();
}

bool PrePostProcessor::CheckPars(DownloadQueue * pDownloadQueue, FileInfo * pFileInfo)
{
	debug("Checking if pars exist");
	
	char szNZBNiceName[1024];
	pFileInfo->GetNZBInfo()->GetNiceNZBName(szNZBNiceName, 1024);

	bool bJobAdded = false;

	m_mutexQueue.Lock();

	FileList fileList;
	if (FindMainPars(pFileInfo->GetNZBInfo()->GetDestDir(), &fileList))
	{
		debug("Found pars");
		
		for (FileList::iterator it = fileList.begin(); it != fileList.end(); it++)
		{
			char* szParFilename = *it;
			debug("Found par: %s", szParFilename);

			char szFullFilename[1024];
			snprintf(szFullFilename, 1024, "%s%c%s", pFileInfo->GetNZBInfo()->GetDestDir(), (int)PATH_SEPARATOR, szParFilename);
			szFullFilename[1024-1] = '\0';

			if (!ParJobExists(&m_PostQueue, szFullFilename) && 
				(!ParJobExists(&m_CompletedJobs, szFullFilename) || g_pOptions->GetAllowReProcess()))
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
				pPostInfo->SetNZBFilename(pFileInfo->GetNZBInfo()->GetFilename());
				pPostInfo->SetDestDir(pFileInfo->GetNZBInfo()->GetDestDir());
				pPostInfo->SetParFilename(szFullFilename);
				pPostInfo->SetInfoName(szParInfoName);
				pPostInfo->SetCategory(pFileInfo->GetNZBInfo()->GetCategory());
				pPostInfo->SetQueuedFilename(pFileInfo->GetNZBInfo()->GetQueuedFilename());
				pPostInfo->SetParCheck(true);
				m_PostQueue.push_back(pPostInfo);
				SavePostQueue();
				m_bHasMoreJobs = true;
				bJobAdded = true;
			}

			free(szParFilename);
		}
	}

	m_mutexQueue.Unlock();

	debug("bJobAdded=%i", (int)bJobAdded);
	return bJobAdded;
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

bool PrePostProcessor::AddPar(FileInfo * pFileInfo, bool bDeleted)
{
	m_mutexQueue.Lock();
	bool bSameCollection = m_ParChecker.IsRunning() &&
		!strcmp(pFileInfo->GetNZBInfo()->GetFilename(), m_ParChecker.GetNZBFilename()) &&
		SameParCollection(pFileInfo->GetFilename(), Util::BaseFileName(m_ParChecker.GetParFilename()));
	if (bSameCollection && !bDeleted)
	{
		char szFullFilename[1024];
		snprintf(szFullFilename, 1024, "%s%c%s", pFileInfo->GetNZBInfo()->GetDestDir(), (int)PATH_SEPARATOR, pFileInfo->GetFilename());
		szFullFilename[1024-1] = '\0';
		m_ParChecker.AddParFile(szFullFilename);

		if (g_pOptions->GetParPauseQueue())
		{
			g_pOptions->SetPause(true);
		}
	}
	else
	{
		m_ParChecker.QueueChanged();
	}
	m_mutexQueue.Unlock();
	return bSameCollection;
}

bool PrePostProcessor::SameParCollection(const char* szFilename1, const char* szFilename2)
{
	int iBaseLen1 = 0, iBaseLen2 = 0;
	return ParseParFilename(szFilename1, &iBaseLen1, NULL) &&
		ParseParFilename(szFilename2, &iBaseLen2, NULL) &&
		iBaseLen1 == iBaseLen2 &&
		!strncasecmp(szFilename1, szFilename2, iBaseLen1);
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
						fprintf(file, "Repair failed for %s: %s\n", m_ParChecker.GetInfoName(), m_ParChecker.GetErrMsg() ? m_ParChecker.GetErrMsg() : "");
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

		m_mutexQueue.Lock();

		PostInfo* pPostInfo = m_PostQueue.front();
		pPostInfo->SetParFailed(m_ParChecker.GetStatus() == ParChecker::psFailed);
		pPostInfo->SetWorking(false);
		pPostInfo->SetStage(PostInfo::ptQueued);

		if (m_ParChecker.GetStatus() == ParChecker::psFailed)
		{
			pPostInfo->SetParStatus(PARSTATUS_FAILED);
		}
		else if (g_pOptions->GetParRepair() || m_ParChecker.GetRepairNotNeeded())
		{
			pPostInfo->SetParStatus(PARSTATUS_REPAIRED);
		}
		else
		{
			pPostInfo->SetParStatus(PARSTATUS_REPAIR_POSSIBLE);
		}

		SavePostQueue();

		m_mutexQueue.Unlock();

		if (g_pOptions->GetParPauseQueue() && !(g_pOptions->GetPostPauseQueue() && PostProcessEnabled()))
		{
			info("Unpausing queue after par-check");
			g_pOptions->SetPause(false);
		}
	}
}

/**
 * Check if the deletion of unneeded (paused) par-files from download queue after successful par-check is possible.
 * If the collection has paused non-par-files, none files will be deleted (even pars).
 * Returns the group-ID, which can be later used in call to GetQueueEditor()->EditEntry()
 */
int PrePostProcessor::GetParCleanupQueueGroup(DownloadQueue* pDownloadQueue, const char* szNZBFilename)
{
	debug("Preparing to cleaning up download queue from par-files");

	// check if nzb-file has only pars paused
	int ID = 0;
	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (!strcmp(pFileInfo->GetNZBInfo()->GetFilename(), szNZBFilename))
		{
			ID = pFileInfo->GetID();
			if (!pFileInfo->GetPaused() && !pFileInfo->GetDeleted() &&
				!ParseParFilename(pFileInfo->GetFilename(), NULL, NULL))
			{
				return 0;
			}
		}
	}
	return ID;
}

/**
 * Check if nzb-file has failures from other par-jobs
 * (if nzb-file has more than one collections)
 *
 * Mutex "m_mutexQueue" must be locked prior to call of this function.
 */
bool PrePostProcessor::HasFailedParJobs(const char* szNZBFilename)
{
	bool bHasFailedJobs = false;

	for (PostQueue::iterator it = m_CompletedJobs.begin(); it != m_CompletedJobs.end(); it++)
	{
		PostInfo* pPostInfo = *it;
		if (pPostInfo->GetParCheck() && pPostInfo->GetParFailed() &&
			!strcmp(pPostInfo->GetNZBFilename(), szNZBFilename))
		{
			bHasFailedJobs = true;
			break;
		}
	}

	return bHasFailedJobs;
}

/**
 * Mutex "m_mutexQueue" must be locked prior to call of this function.
 */
bool PrePostProcessor::ParJobExists(PostQueue* pPostQueue, const char* szParFilename)
{
	for (PostQueue::iterator it = pPostQueue->begin(); it != pPostQueue->end(); it++)
	{
		PostInfo* pPostInfo = *it;
		if (pPostInfo->GetParCheck() && !strcmp(pPostInfo->GetParFilename(), szParFilename))
		{
			return true;
		}
	}
	return false;
}

/**
* Unpause par2-files
* returns true, if the files with required number of blocks were unpaused,
* or false if there are no more files in queue for this collection or not enough blocks
*/
bool PrePostProcessor::RequestMorePars(const char* szNZBFilename, const char* szParFilename, int iBlockNeeded, int* pBlockFound)
{
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();
	
	Blocks blocks;
	blocks.clear();
	int iBlockFound = 0;

	FindPars(pDownloadQueue, szNZBFilename, szParFilename, &blocks, true, true, &iBlockFound);
	if (iBlockFound == 0)
	{
		FindPars(pDownloadQueue, szNZBFilename, szParFilename, &blocks, true, false, &iBlockFound);
	}
	if (iBlockFound == 0 && !g_pOptions->GetStrictParName())
	{
		FindPars(pDownloadQueue, szNZBFilename, szParFilename, &blocks, false, false, &iBlockFound);
	}

	if (iBlockFound >= iBlockNeeded)
	{
		char szNZBNiceName[1024];
		NZBInfo::MakeNiceNZBName(szNZBFilename, szNZBNiceName, 1024);

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
		g_pOptions->SetPause(false);
	}

	return bOK;
}

void PrePostProcessor::FindPars(DownloadQueue * pDownloadQueue, const char* szNZBFilename, const char* szParFilename,
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

	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		int iBlocks = 0;
		if (!strcmp(pFileInfo->GetNZBInfo()->GetFilename(), szNZBFilename) &&
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

void PrePostProcessor::UpdateParProgress()
{
	m_mutexQueue.Lock();

	PostInfo* pPostInfo = m_PostQueue.front();
	if (m_ParChecker.GetFileProgress() == 0)
	{
		pPostInfo->SetProgressLabel(m_ParChecker.GetProgressLabel());
	}
	pPostInfo->SetFileProgress(m_ParChecker.GetFileProgress());
	pPostInfo->SetStageProgress(m_ParChecker.GetStageProgress());
    PostInfo::EStage StageKind[] = { PostInfo::ptLoadingPars, PostInfo::ptVerifyingSources, PostInfo::ptRepairing, PostInfo::ptVerifyingRepaired };
	PostInfo::EStage eStage = StageKind[m_ParChecker.GetStage()];

	if (!pPostInfo->GetStartTime())
	{
		pPostInfo->SetStartTime(time(NULL));
	}

	if (pPostInfo->GetStage() != eStage)
	{
		pPostInfo->SetStage(eStage);
		pPostInfo->SetStageTime(time(NULL));
	}

	m_mutexQueue.Unlock();
}

#endif
