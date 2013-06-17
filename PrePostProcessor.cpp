/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "PrePostProcessor.h"
#include "Options.h"
#include "Log.h"
#include "QueueCoordinator.h"
#include "ScriptController.h"
#include "DiskState.h"
#include "Util.h"
#include "Scheduler.h"
#include "Scanner.h"
#include "Unpack.h"
#include "NZBFile.h"

extern QueueCoordinator* g_pQueueCoordinator;
extern Options* g_pOptions;
extern DiskState* g_pDiskState;
extern Scheduler* g_pScheduler;
extern Scanner* g_pScanner;

PrePostProcessor::PrePostProcessor()
{
	debug("Creating PrePostProcessor");

	m_bHasMoreJobs = false;
	m_bPostPause = false;

	m_QueueCoordinatorObserver.m_pOwner = this;
	g_pQueueCoordinator->Attach(&m_QueueCoordinatorObserver);

#ifndef DISABLE_PARCHECK
	m_ParCoordinator.m_pOwner = this;
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

	for (FileQueue::iterator it = pDownloadQueue->GetParkedFiles()->begin(); it != pDownloadQueue->GetParkedFiles()->end(); it++)
	{
		delete *it;
	}
	pDownloadQueue->GetParkedFiles()->clear();

	for (HistoryList::iterator it = pDownloadQueue->GetHistoryList()->begin(); it != pDownloadQueue->GetHistoryList()->end(); it++)
	{
		delete *it;
	}
	pDownloadQueue->GetHistoryList()->clear();

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

	int iDiskSpaceInterval = 1000;
	int iSchedulerInterval = 1000;
	int iHistoryInterval = 60000;
	const int iStepMSec = 200;

	while (!IsStopped())
	{
		// check incoming nzb directory
		g_pScanner->Check();

		if (!(g_pOptions->GetPauseDownload() || g_pOptions->GetPauseDownload2()) && 
			g_pOptions->GetDiskSpace() > 0 && !g_pQueueCoordinator->GetStandBy() && 
			iDiskSpaceInterval >= 1000)
		{
			// check free disk space every 1 second
			CheckDiskSpace();
			iDiskSpaceInterval = 0;
		}
		iDiskSpaceInterval += iStepMSec;

		// check post-queue every 200 msec
		CheckPostQueue();

		if (iSchedulerInterval >= 1000)
		{
			// check scheduler tasks every 1 second
			g_pScheduler->IntervalCheck();
			ApplySchedulerState();
			iSchedulerInterval = 0;
			CheckScheduledResume();
		}
		iSchedulerInterval += iStepMSec;

		if (iHistoryInterval >= 60000)
		{
			// check history (remove old entries) every 1 minute
			CheckHistory();
			iHistoryInterval = 0;
		}
		iHistoryInterval += iStepMSec;

		usleep(iStepMSec * 1000);
	}

	Cleanup();

	debug("Exiting PrePostProcessor-loop");
}

void PrePostProcessor::Stop()
{
	Thread::Stop();
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

#ifndef DISABLE_PARCHECK
	m_ParCoordinator.Stop();
#endif

	if (!pDownloadQueue->GetPostQueue()->empty())
	{
		PostInfo* pPostInfo = pDownloadQueue->GetPostQueue()->front();
		if ((pPostInfo->GetStage() == PostInfo::ptUnpacking ||
			 pPostInfo->GetStage() == PostInfo::ptExecutingScript) && 
			pPostInfo->GetPostThread())
		{
			Thread* pPostThread = pPostInfo->GetPostThread();
			pPostInfo->SetPostThread(NULL);
			pPostThread->SetAutoDestroy(true);
			pPostThread->Stop();
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
			!m_ParCoordinator.AddPar(pAspect->pFileInfo, pAspect->eAction == QueueCoordinator::eaFileDeleted) &&
#endif
			IsNZBFileCompleted(pAspect->pDownloadQueue, pAspect->pNZBInfo, true, false) &&
			(!pAspect->pFileInfo->GetPaused() || IsNZBFileCompleted(pAspect->pDownloadQueue, pAspect->pNZBInfo, false, false)))
		{
			if (pAspect->eAction == QueueCoordinator::eaFileCompleted ||
				(pAspect->pFileInfo->GetAutoDeleted() &&
				 IsNZBFileCompleted(pAspect->pDownloadQueue, pAspect->pNZBInfo, false, true)))
			{
				info("Collection %s completely downloaded", pAspect->pNZBInfo->GetName());
				NZBDownloaded(pAspect->pDownloadQueue, pAspect->pNZBInfo);
			}
			else if (pAspect->eAction == QueueCoordinator::eaFileDeleted &&
				!pAspect->pNZBInfo->GetParCleanup() &&
				IsNZBFileCompleted(pAspect->pDownloadQueue, pAspect->pNZBInfo, false, true))
			{
				info("Collection %s deleted from queue", pAspect->pNZBInfo->GetName());
				NZBDeleted(pAspect->pDownloadQueue, pAspect->pNZBInfo);
			}
		}
	}
}

void PrePostProcessor::NZBAdded(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	PostScriptController::InitParamsForNewNZB(pNZBInfo);

	if (g_pOptions->GetMergeNzb())
	{
		pNZBInfo = MergeGroups(pDownloadQueue, pNZBInfo);
	}

	if (g_pOptions->GetParCheck() != Options::pcForce)
	{
		m_ParCoordinator.PausePars(pDownloadQueue, pNZBInfo);
	}

	if (strlen(g_pOptions->GetNZBAddedProcess()) > 0)
	{
		NZBAddedScriptController::StartScript(pDownloadQueue, pNZBInfo, g_pOptions->GetNZBAddedProcess());
	}
}

void PrePostProcessor::NZBDownloaded(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	if (!pNZBInfo->GetPostProcess() && g_pOptions->GetDecode())
	{
		info("Queueing %s for post-processing", pNZBInfo->GetName());

		PostInfo* pPostInfo = new PostInfo();
		pPostInfo->SetNZBInfo(pNZBInfo);
		pPostInfo->SetInfoName(pNZBInfo->GetName());

		if (pNZBInfo->GetParStatus() == NZBInfo::psNone && g_pOptions->GetParCheck() != Options::pcForce)
		{
			pNZBInfo->SetParStatus(NZBInfo::psSkipped);
		}

		if (pNZBInfo->GetRenameStatus() == NZBInfo::rsNone)
		{
			pNZBInfo->SetRenameStatus(NZBInfo::rsSkipped);
		}

		pNZBInfo->SetPostProcess(true);
		pDownloadQueue->GetPostQueue()->push_back(pPostInfo);
		SaveQueue(pDownloadQueue);
		m_bHasMoreJobs = true;
	}
	else
	{
		NZBCompleted(pDownloadQueue, pNZBInfo, true);
	}
}

void PrePostProcessor::NZBDeleted(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	if (g_pOptions->GetDeleteCleanupDisk() && pNZBInfo->GetCleanupDisk())
	{
		// download was cancelled, deleting already downloaded files from disk
		for (NZBInfo::Files::reverse_iterator it = pNZBInfo->GetCompletedFiles()->rbegin(); it != pNZBInfo->GetCompletedFiles()->rend(); it++)
		{
			char* szFilename = *it;
			if (Util::FileExists(szFilename))
			{
				detail("Deleting file %s", Util::BaseFileName(szFilename));
				remove(szFilename);
			}
		}

		// delete .out.tmp-files and _brokenlog.txt
		DirBrowser dir(pNZBInfo->GetDestDir());
		while (const char* szFilename = dir.Next())
		{
			int iLen = strlen(szFilename);
			if ((iLen > 8 && !strcmp(szFilename + iLen - 8, ".out.tmp")) || !strcmp(szFilename, "_brokenlog.txt"))
			{
				char szFullFilename[1024];
				snprintf(szFullFilename, 1024, "%s%c%s", pNZBInfo->GetDestDir(), PATH_SEPARATOR, szFilename);
				szFullFilename[1024-1] = '\0';

				detail("Deleting file %s", szFilename);
				remove(szFullFilename);
			}
		}
	
		// delete old directory (if empty)
		if (Util::DirEmpty(pNZBInfo->GetDestDir()))
		{
			rmdir(pNZBInfo->GetDestDir());
		}

		if (g_pOptions->GetNzbCleanupDisk())
		{
			DeleteQueuedFile(pNZBInfo->GetQueuedFilename());
		}
	}

	NZBCompleted(pDownloadQueue, pNZBInfo, true);
}

void PrePostProcessor::NZBCompleted(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, bool bSaveQueue)
{
	if (g_pOptions->GetKeepHistory() > 0)
	{
		//remove old item for the same NZB
		for (HistoryList::iterator it = pDownloadQueue->GetHistoryList()->begin(); it != pDownloadQueue->GetHistoryList()->end(); it++)
		{
			HistoryInfo* pHistoryInfo = *it;
			if (pHistoryInfo->GetNZBInfo() == pNZBInfo)
			{
				delete pHistoryInfo;
				pDownloadQueue->GetHistoryList()->erase(it);
				break;
			}
		}

		HistoryInfo* pHistoryInfo = new HistoryInfo(pNZBInfo);
		pHistoryInfo->SetTime(time(NULL));
		pDownloadQueue->GetHistoryList()->push_front(pHistoryInfo);

		// park files
		int iParkedFiles = 0;
		int index = 0;
		for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); )
		{
			FileInfo* pFileInfo = *it;
			if (pFileInfo->GetNZBInfo() == pNZBInfo && !pFileInfo->GetDeleted())
			{
				detail("Park file %s", pFileInfo->GetFilename());
				g_pQueueCoordinator->DiscardDiskFile(pFileInfo);
				pDownloadQueue->GetFileQueue()->erase(it);
				pDownloadQueue->GetParkedFiles()->push_back(pFileInfo);
				it = pDownloadQueue->GetFileQueue()->begin() + index;
				iParkedFiles++;
			}
			else
			{
				it++;
				index++;
			}
		}
		pNZBInfo->SetParkedFileCount(iParkedFiles);

		if (bSaveQueue)
		{
			SaveQueue(pDownloadQueue);
		}

		info("Collection %s added to history", pNZBInfo->GetName());
	}
}

/**
 * Removes old entries from history
 */
void PrePostProcessor::CheckHistory()
{
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	time_t tMinTime = time(NULL) - g_pOptions->GetKeepHistory() * 60000;
	bool bChanged = false;
	int index = 0;

	// traversing in a reverse order to delete items in order they were added to history
	// (just to produce the log-messages in a more logical order)
	for (HistoryList::reverse_iterator it = pDownloadQueue->GetHistoryList()->rbegin(); it != pDownloadQueue->GetHistoryList()->rend(); )
	{
		HistoryInfo* pHistoryInfo = *it;
		if (pHistoryInfo->GetTime() < tMinTime)
		{
			char szNiceName[1024];
			pHistoryInfo->GetName(szNiceName, 1024);
			pDownloadQueue->GetHistoryList()->erase(pDownloadQueue->GetHistoryList()->end() - 1 - index);
			delete pHistoryInfo;
			info("Collection %s removed from history", szNiceName);
			it = pDownloadQueue->GetHistoryList()->rbegin() + index;
			bChanged = true;
		}
		else
		{
			it++;
			index++;
		}
	}

	if (bChanged)
	{
		SaveQueue(pDownloadQueue);
	}

	g_pQueueCoordinator->UnlockQueue();
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

	// merge(2): check if queue has other nzb-files with the same name
	if (iAddedGroupID > 0)
	{
		for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
		{
			FileInfo* pFileInfo = *it;
			if (pFileInfo->GetNZBInfo() != pNZBInfo &&
				!strcmp(pFileInfo->GetNZBInfo()->GetName(), pNZBInfo->GetName()))
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

void PrePostProcessor::CheckDiskSpace()
{
	long long lFreeSpace = Util::FreeDiskSize(g_pOptions->GetDestDir());
	if (lFreeSpace > -1 && lFreeSpace / 1024 / 1024 < g_pOptions->GetDiskSpace())
	{
		warn("Low disk space. Pausing download");
		g_pOptions->SetPauseDownload(true);
	}
}

void PrePostProcessor::CheckPostQueue()
{
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	if (!pDownloadQueue->GetPostQueue()->empty())
	{
		PostInfo* pPostInfo = pDownloadQueue->GetPostQueue()->front();
		if (!pPostInfo->GetWorking())
		{
#ifndef DISABLE_PARCHECK
			if (pPostInfo->GetRequestParCheck() && pPostInfo->GetNZBInfo()->GetParStatus() <= NZBInfo::psSkipped &&
				g_pOptions->GetParCheck() != Options::pcManual)
			{
				pPostInfo->GetNZBInfo()->SetParStatus(NZBInfo::psNone);
				pPostInfo->SetRequestParCheck(false);
				pPostInfo->SetStage(PostInfo::ptQueued);
				pPostInfo->GetNZBInfo()->GetScriptStatuses()->Clear();
				DeletePostThread(pPostInfo);
			}
			else if (pPostInfo->GetRequestParCheck() && pPostInfo->GetNZBInfo()->GetParStatus() <= NZBInfo::psSkipped &&
				g_pOptions->GetParCheck() == Options::pcManual)
			{
				pPostInfo->SetRequestParCheck(false);
				pPostInfo->GetNZBInfo()->SetParStatus(NZBInfo::psManual);
				DeletePostThread(pPostInfo);

				FileInfo* pFileInfo = GetQueueGroup(pDownloadQueue, pPostInfo->GetNZBInfo());
				if (pFileInfo)
				{
					info("Downloading all remaining files for manual par-check for %s", pPostInfo->GetNZBInfo()->GetName());
					g_pQueueCoordinator->GetQueueEditor()->LockedEditEntry(pDownloadQueue, pFileInfo->GetID(), false, QueueEditor::eaGroupResume, 0, NULL);
					pPostInfo->SetStage(PostInfo::ptFinished);
					pPostInfo->GetNZBInfo()->SetPostProcess(false);
				}
				else
				{
					info("There are no par-files remain for download for %s", pPostInfo->GetNZBInfo()->GetName());
					pPostInfo->SetStage(PostInfo::ptQueued);
				}
			}
			else if (pPostInfo->GetRequestParRename())
			{
				pPostInfo->GetNZBInfo()->SetRenameStatus(NZBInfo::rsNone);
				pPostInfo->SetRequestParRename(false);
				pPostInfo->SetStage(PostInfo::ptQueued);
				DeletePostThread(pPostInfo);
			}

#endif
			if (pPostInfo->GetDeleted())
			{
				pPostInfo->SetStage(PostInfo::ptFinished);
			}

			if (pPostInfo->GetStage() == PostInfo::ptQueued && !g_pOptions->GetPausePostProcess())
			{
				DeletePostThread(pPostInfo);
				StartJob(pDownloadQueue, pPostInfo);
			}
			else if (pPostInfo->GetStage() == PostInfo::ptFinished)
			{
				UpdatePauseState(false, NULL);
				JobCompleted(pDownloadQueue, pPostInfo);
			}
			else if (!g_pOptions->GetPausePostProcess())
			{
				error("Internal error: invalid state in post-processor");
			}
		}
	}
	
	g_pQueueCoordinator->UnlockQueue();
}

void PrePostProcessor::SaveQueue(DownloadQueue* pDownloadQueue)
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
		}
		else 
		{
			pPostInfo->SetStage(PostInfo::ptQueued);
		}
	}
}

void PrePostProcessor::DeletePostThread(PostInfo* pPostInfo)
{
	if (pPostInfo->GetPostThread())
	{
		delete pPostInfo->GetPostThread();
		pPostInfo->SetPostThread(NULL);
	}
}

void PrePostProcessor::StartJob(DownloadQueue* pDownloadQueue, PostInfo* pPostInfo)
{
#ifndef DISABLE_PARCHECK
	if (pPostInfo->GetNZBInfo()->GetRenameStatus() == NZBInfo::rsNone)
	{
		UpdatePauseState(g_pOptions->GetParPauseQueue(), "par-rename");
		m_ParCoordinator.StartParRenameJob(pPostInfo);
		return;
	}
	else if (pPostInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psNone)
	{
		if (m_ParCoordinator.FindMainPars(pPostInfo->GetNZBInfo()->GetDestDir(), NULL))
		{
			UpdatePauseState(g_pOptions->GetParPauseQueue(), "par-check");
			m_ParCoordinator.StartParCheckJob(pPostInfo);
		}
		else
		{
			info("Nothing to par-check for %s", pPostInfo->GetInfoName());
			pPostInfo->GetNZBInfo()->SetParStatus(NZBInfo::psSkipped);
			pPostInfo->SetWorking(false);
			pPostInfo->SetStage(PostInfo::ptQueued);
		}
		return;
	}
#endif

	bool bUnpack = g_pOptions->GetUnpack() && (pPostInfo->GetNZBInfo()->GetUnpackStatus() == NZBInfo::usNone);

	bool bParFailed = pPostInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psFailure ||
		pPostInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psRepairPossible ||
		pPostInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psManual;

	bool bCleanup = !bUnpack &&
		pPostInfo->GetNZBInfo()->GetCleanupStatus() == NZBInfo::csNone &&
		(pPostInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psSuccess ||
		 (pPostInfo->GetNZBInfo()->GetUnpackStatus() == NZBInfo::usSuccess &&
		  pPostInfo->GetNZBInfo()->GetParStatus() != NZBInfo::psFailure)) &&
		strlen(g_pOptions->GetExtCleanupDisk()) > 0;

	bool bMoveInter = !bUnpack &&
		pPostInfo->GetNZBInfo()->GetMoveStatus() == NZBInfo::msNone &&
		pPostInfo->GetNZBInfo()->GetUnpackStatus() != NZBInfo::usFailure &&
		pPostInfo->GetNZBInfo()->GetParStatus() != NZBInfo::psFailure &&
		pPostInfo->GetNZBInfo()->GetParStatus() != NZBInfo::psManual &&
		strlen(g_pOptions->GetInterDir()) > 0 &&
		!strncmp(pPostInfo->GetNZBInfo()->GetDestDir(), g_pOptions->GetInterDir(), strlen(g_pOptions->GetInterDir()));

	// TODO: check if download has pp-scripts defined
	bool bPostScript = true;

	if (bUnpack && bParFailed)
	{
		warn("Skipping unpack due to %s for %s",
			pPostInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psManual ? "required par-repair" : "par-failure",
			pPostInfo->GetInfoName());
		pPostInfo->GetNZBInfo()->SetUnpackStatus(NZBInfo::usSkipped);
		bUnpack = false;
	}

	if (!bUnpack && !bMoveInter && !bPostScript)
	{
		pPostInfo->SetStage(PostInfo::ptFinished);
		return;
	}

	pPostInfo->SetProgressLabel(bUnpack ? "Unpacking" : bMoveInter ? "Moving" : "Executing post-process-script");
	pPostInfo->SetWorking(true);
	pPostInfo->SetStage(bUnpack ? PostInfo::ptUnpacking : bMoveInter ? PostInfo::ptMoving : PostInfo::ptExecutingScript);
	pPostInfo->SetFileProgress(0);
	pPostInfo->SetStageProgress(0);
	SaveQueue(pDownloadQueue);

	if (!pPostInfo->GetStartTime())
	{
		pPostInfo->SetStartTime(time(NULL));
	}
	pPostInfo->SetStageTime(time(NULL));

	if (bUnpack)
	{
		UpdatePauseState(g_pOptions->GetUnpackPauseQueue(), "unpack");
		UnpackController::StartJob(pPostInfo);
	}
	else if (bCleanup)
	{
		UpdatePauseState(g_pOptions->GetUnpackPauseQueue() || g_pOptions->GetScriptPauseQueue(), "cleanup");
		CleanupController::StartJob(pPostInfo);
	}
	else if (bMoveInter)
	{
		UpdatePauseState(g_pOptions->GetUnpackPauseQueue() || g_pOptions->GetScriptPauseQueue(), "move");
		MoveController::StartJob(pPostInfo);
	}
	else
	{
		UpdatePauseState(g_pOptions->GetScriptPauseQueue(), "post-process-script");
		PostScriptController::StartJob(pPostInfo);
	}
}

void PrePostProcessor::JobCompleted(DownloadQueue* pDownloadQueue, PostInfo* pPostInfo)
{
	pPostInfo->SetWorking(false);
	pPostInfo->SetProgressLabel("");
	pPostInfo->SetStage(PostInfo::ptFinished);

	DeletePostThread(pPostInfo);

	if (IsNZBFileCompleted(pDownloadQueue, pPostInfo->GetNZBInfo(), true, false))
	{
		// Cleaning up queue if par-check was successful or unpack was successful or
		// script was successful (if unpack was not performed)
		bool bCanCleanupQueue = pPostInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psSuccess ||
			 pPostInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psRepairPossible ||
			 pPostInfo->GetNZBInfo()->GetUnpackStatus() == NZBInfo::usSuccess ||
			 (pPostInfo->GetNZBInfo()->GetUnpackStatus() == NZBInfo::usNone &&
			  pPostInfo->GetNZBInfo()->GetScriptStatuses()->CalcTotalStatus() == ScriptStatus::srSuccess);
		if ((g_pOptions->GetParCleanupQueue() || g_pOptions->GetNzbCleanupDisk()) && bCanCleanupQueue)
		{
			if (g_pOptions->GetParCleanupQueue())
			{
				FileInfo* pFileInfo = GetQueueGroup(pDownloadQueue, pPostInfo->GetNZBInfo());
				if (pFileInfo)
				{
					info("Cleaning up download queue for %s", pPostInfo->GetNZBInfo()->GetName());
					pFileInfo->GetNZBInfo()->ClearCompletedFiles();
					pFileInfo->GetNZBInfo()->SetParCleanup(true);
					g_pQueueCoordinator->GetQueueEditor()->LockedEditEntry(pDownloadQueue, pFileInfo->GetID(), false, QueueEditor::eaGroupDelete, 0, NULL);
				}
			}
			if (g_pOptions->GetNzbCleanupDisk())
			{
				DeleteQueuedFile(pPostInfo->GetNZBInfo()->GetQueuedFilename());
			}
		}

		NZBCompleted(pDownloadQueue, pPostInfo->GetNZBInfo(), false);
	}

	for (PostQueue::iterator it = pDownloadQueue->GetPostQueue()->begin(); it != pDownloadQueue->GetPostQueue()->end(); it++)
	{
		if (pPostInfo == *it)
		{
			pDownloadQueue->GetPostQueue()->erase(it);
			break;
		}
	}

	delete pPostInfo;

	SaveQueue(pDownloadQueue);

	m_bHasMoreJobs = !pDownloadQueue->GetPostQueue()->empty();
}

bool PrePostProcessor::IsNZBFileCompleted(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo,
	bool bIgnorePausedPars, bool bAllowOnlyOneDeleted)
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
			if (((!pFileInfo->GetPaused() || !bIgnorePausedPars ||
				!(m_ParCoordinator.ParseParFilename(pFileInfo->GetFilename(), NULL, NULL))) && 
				!pFileInfo->GetDeleted()) ||
				(bAllowOnlyOneDeleted && iDeleted > 1))
			{
				bNZBFileCompleted = false;
				break;
			}
		}
	}

	return bNZBFileCompleted;
}

/**
 * Returns the first FileInfo belonging to given NZBInfo.
 */
FileInfo* PrePostProcessor::GetQueueGroup(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (pFileInfo->GetNZBInfo() == pNZBInfo)
		{
			return pFileInfo;
		}
	}
	return NULL;
}

void PrePostProcessor::ApplySchedulerState()
{
	if (g_pScheduler->GetDownloadRateChanged())
	{
		info("Scheduler: set download rate to %i KB/s", g_pScheduler->GetDownloadRate() / 1024);
		g_pOptions->SetDownloadRate(g_pScheduler->GetDownloadRate());
	}

	if (g_pScheduler->GetPauseDownloadChanged())
	{
		info("Scheduler: %s download queue", g_pScheduler->GetPauseDownload() ? "pause" : "unpause");
		m_bSchedulerPauseChanged = true;
		m_bSchedulerPause = g_pScheduler->GetPauseDownload();
		if (!m_bPostPause)
		{
			g_pOptions->SetPauseDownload(m_bSchedulerPause);
		}
	}

	if (g_pScheduler->GetPauseScanChanged())
	{
		info("Scheduler: %s scan", g_pScheduler->GetPauseScan() ? "pause" : "unpause");
		g_pOptions->SetPauseScan(g_pScheduler->GetPauseScan());
	}
}

void PrePostProcessor::UpdatePauseState(bool bNeedPause, const char* szReason)
{
	if (bNeedPause)
	{
		if (PauseDownload())
		{
			info("Pausing queue before %s", szReason);
		}
	}
	else if (m_bPostPause)
	{
		if (UnpauseDownload())
		{
			info("Unpausing queue after %s", m_szPauseReason);
		}
	}

	m_szPauseReason = szReason;
}

bool PrePostProcessor::PauseDownload()
{
	debug("PrePostProcessor::PauseDownload()");

	if (m_bPostPause && g_pOptions->GetPauseDownload())
	{
		return false;
	}

	m_bPostPause = !g_pOptions->GetPauseDownload();
	m_bSchedulerPauseChanged = false;
	g_pOptions->SetPauseDownload(true);
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
		g_pOptions->SetPauseDownload(bPause);
	}
	return !bPause;
}

void PrePostProcessor::CheckScheduledResume()
{
	time_t tResumeTime = g_pOptions->GetResumeTime();
	time_t tCurrentTime = time(NULL);
	if (tResumeTime > 0 && tCurrentTime >= tResumeTime)
	{
		info("Autoresume");
		g_pOptions->SetResumeTime(0);
		g_pOptions->SetPauseDownload2(false);
		g_pOptions->SetPausePostProcess(false);
		g_pOptions->SetPauseScan(false);
	}
}

bool PrePostProcessor::QueueEditList(IDList* pIDList, EEditAction eAction, int iOffset, const char* szText)
{
	debug("Edit-command for post-processor received");
	switch (eAction)
	{
		case eaPostMoveOffset:
		case eaPostMoveTop:
		case eaPostMoveBottom:
			return PostQueueMove(pIDList, eAction, iOffset);

		case eaPostDelete:
			return PostQueueDelete(pIDList);

		case eaHistoryDelete:
		case eaHistoryReturn:
		case eaHistoryProcess:
		case eaHistorySetParameter:
			return HistoryEdit(pIDList, eAction, iOffset, szText);

		default:
			return false;
	}
}

bool PrePostProcessor::PostQueueDelete(IDList* pIDList)
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
					if (PostInfo::ptLoadingPars <= pPostInfo->GetStage() && pPostInfo->GetStage() <= PostInfo::ptRenaming)
					{
						if (m_ParCoordinator.Cancel())
						{
							bOK = true;
						}
					}
					else
#endif
					if (pPostInfo->GetPostThread())
					{
						debug("Terminating %s for %s", (pPostInfo->GetStage() == PostInfo::ptUnpacking ? "unpack" : "post-process-script"), pPostInfo->GetInfoName());
						pPostInfo->GetPostThread()->Stop();
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

bool PrePostProcessor::PostQueueMove(IDList* pIDList, EEditAction eAction, int iOffset)
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
			SaveQueue(pDownloadQueue);
			bOK = true;
		}
	}

	g_pQueueCoordinator->UnlockQueue();

	return bOK;
}

bool PrePostProcessor::HistoryEdit(IDList* pIDList, EEditAction eAction, int iOffset, const char* szText)
{
	bool bOK = false;

	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	for (IDList::iterator itID = pIDList->begin(); itID != pIDList->end(); itID++)
	{
		int iID = *itID;
		for (HistoryList::iterator itHistory = pDownloadQueue->GetHistoryList()->begin(); itHistory != pDownloadQueue->GetHistoryList()->end(); itHistory++)
		{
			HistoryInfo* pHistoryInfo = *itHistory;
			if (pHistoryInfo->GetID() == iID)
			{
				switch (eAction)
				{
					case eaHistoryDelete:
						HistoryDelete(pDownloadQueue, itHistory, pHistoryInfo);
						break;

					case eaHistoryReturn:
					case eaHistoryProcess:
						HistoryReturn(pDownloadQueue, itHistory, pHistoryInfo, eAction == eaHistoryProcess);
						break;

					case eaHistorySetParameter:
						HistorySetParameter(pHistoryInfo, szText);
						break;
						
					default:
						// nothing, just to avoid compiler warning
						break;
				}

				bOK = true;
				break;
			}
		}
	}

	if (bOK)
	{
		SaveQueue(pDownloadQueue);
	}

	g_pQueueCoordinator->UnlockQueue();

	return bOK;
}

void PrePostProcessor::HistoryDelete(DownloadQueue* pDownloadQueue, HistoryList::iterator itHistory, HistoryInfo* pHistoryInfo)
{
	char szNiceName[1024];
	pHistoryInfo->GetName(szNiceName, 1024);
	info("Deleting %s from history", szNiceName);

	if (pHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo)
	{
		NZBInfo* pNZBInfo = pHistoryInfo->GetNZBInfo();

		// delete parked files
		int index = 0;
		for (FileQueue::iterator it = pDownloadQueue->GetParkedFiles()->begin(); it != pDownloadQueue->GetParkedFiles()->end(); )
		{
			FileInfo* pFileInfo = *it;
			if (pFileInfo->GetNZBInfo() == pNZBInfo)
			{
				pDownloadQueue->GetParkedFiles()->erase(it);
				if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
				{
					g_pDiskState->DiscardFile(pFileInfo);
				}
				delete pFileInfo;
				it = pDownloadQueue->GetParkedFiles()->begin() + index;
			}
			else
			{
				it++;
				index++;
			}
		}
	}

	pDownloadQueue->GetHistoryList()->erase(itHistory);
	delete pHistoryInfo;
}

void PrePostProcessor::HistoryReturn(DownloadQueue* pDownloadQueue, HistoryList::iterator itHistory, HistoryInfo* pHistoryInfo, bool bReprocess)
{
	char szNiceName[1024];
	pHistoryInfo->GetName(szNiceName, 1024);
	debug("Returning %s from history back to download queue", szNiceName);
	bool bUnparked = false;

	if (bReprocess && pHistoryInfo->GetKind() != HistoryInfo::hkNZBInfo)
	{
		error("Could not restart postprocessing for %s: history item has wrong type", szNiceName);
		return;
	}

	if (pHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo)
	{
		NZBInfo* pNZBInfo = pHistoryInfo->GetNZBInfo();

		// unpark files
		int index = 0;
		for (FileQueue::reverse_iterator it = pDownloadQueue->GetParkedFiles()->rbegin(); it != pDownloadQueue->GetParkedFiles()->rend(); )
		{
			FileInfo* pFileInfo = *it;
			if (pFileInfo->GetNZBInfo() == pNZBInfo)
			{
				detail("Unpark file %s", pFileInfo->GetFilename());
				pDownloadQueue->GetParkedFiles()->erase(pDownloadQueue->GetParkedFiles()->end() - 1 - index);
				pDownloadQueue->GetFileQueue()->push_front(pFileInfo);
				bUnparked = true;
				it = pDownloadQueue->GetParkedFiles()->rbegin() + index;
			}
			else
			{
				it++;
				index++;
			}
		}

		// reset postprocessing status variables
		pNZBInfo->SetPostProcess(false);
		pNZBInfo->SetParCleanup(false);
		if (!pNZBInfo->GetUnpackCleanedUpDisk())
		{
			pNZBInfo->SetParStatus(NZBInfo::psNone);
			pNZBInfo->SetRenameStatus(NZBInfo::rsNone);
			pNZBInfo->SetUnpackStatus(NZBInfo::usNone);
			pNZBInfo->SetCleanupStatus(NZBInfo::csNone);
		}
		pNZBInfo->GetScriptStatuses()->Clear();
		pNZBInfo->SetParkedFileCount(0);
	}

	if (pHistoryInfo->GetKind() == HistoryInfo::hkUrlInfo)
	{
		UrlInfo* pUrlInfo = pHistoryInfo->GetUrlInfo();
		pHistoryInfo->DiscardUrlInfo();
		pUrlInfo->SetStatus(UrlInfo::aiUndefined);
		pDownloadQueue->GetUrlQueue()->push_back(pUrlInfo);
		bUnparked = true;
	}

	if (bUnparked || bReprocess)
	{
		pDownloadQueue->GetHistoryList()->erase(itHistory);
		// the object "pHistoryInfo" is released few lines later, after the call to "NZBDownloaded"
		info("%s returned from history back to download queue", szNiceName);
	}
	else
	{
		warn("Could not return %s back from history to download queue: history item does not have any files left for download", szNiceName);
	}

	if (bReprocess)
	{
		// start postprocessing
		debug("Restarting postprocessing for %s", szNiceName);
		NZBDownloaded(pDownloadQueue, pHistoryInfo->GetNZBInfo());
	}

	if (bUnparked || bReprocess)
	{
		delete pHistoryInfo;
	}
}

void PrePostProcessor::HistorySetParameter(HistoryInfo* pHistoryInfo, const char* szText)
{
	char szNiceName[1024];
	pHistoryInfo->GetName(szNiceName, 1024);
	debug("Setting post-process-parameter '%s' for '%s'", szText, szNiceName);

	if (pHistoryInfo->GetKind() != HistoryInfo::hkNZBInfo)
	{
		error("Could not set post-process-parameter for %s: history item has wrong type", szNiceName);
		return;
	}

	char* szStr = strdup(szText);

	char* szValue = strchr(szStr, '=');
	if (szValue)
	{
		*szValue = '\0';
		szValue++;
		pHistoryInfo->GetNZBInfo()->SetParameter(szStr, szValue);
	}
	else
	{
		error("Could not set post-process-parameter for %s: invalid argument: %s", pHistoryInfo->GetNZBInfo()->GetName(), szText);
	}

	free(szStr);
}
