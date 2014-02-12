/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include <set>
#include <algorithm>

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

void PrePostProcessor::PostDupeCoordinator::HistoryRedownload(DownloadQueue* pDownloadQueue, HistoryInfo* pHistoryInfo)
{
	HistoryList::iterator it = std::find(pDownloadQueue->GetHistory()->begin(),
		pDownloadQueue->GetHistory()->end(), pHistoryInfo);
	m_pOwner->HistoryRedownload(pDownloadQueue, it, pHistoryInfo, true);
}

PrePostProcessor::PrePostProcessor()
{
	debug("Creating PrePostProcessor");

	m_bPostPause = false;
	m_iJobCount = 0;
	m_pCurJob = NULL;

	m_QueueCoordinatorObserver.m_pOwner = this;
	g_pQueueCoordinator->Attach(&m_QueueCoordinatorObserver);
#ifndef DISABLE_PARCHECK
	m_ParCoordinator.m_pOwner = this;
#endif
	m_DupeCoordinator.m_pOwner = this;
}

PrePostProcessor::~PrePostProcessor()
{
	debug("Destroying PrePostProcessor");
}

void PrePostProcessor::Cleanup()
{
	debug("Cleaning up PrePostProcessor");

	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	for (HistoryList::iterator it = pDownloadQueue->GetHistory()->begin(); it != pDownloadQueue->GetHistory()->end(); it++)
	{
		delete *it;
	}
	pDownloadQueue->GetHistory()->clear();

	g_pQueueCoordinator->UnlockQueue();
}

void PrePostProcessor::Run()
{
	debug("Entering PrePostProcessor-loop");

	while (!g_pQueueCoordinator->IsInitialized() && !IsStopped())
	{
		usleep(5 * 1000);
	}

	if (g_pOptions->GetServerMode() && g_pOptions->GetSaveQueue() &&
		g_pOptions->GetReloadQueue() && g_pOptions->GetReloadPostQueue())
	{
		DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();
		SanitisePostQueue(pDownloadQueue);
		g_pQueueCoordinator->UnlockQueue();
	}

	g_pScheduler->FirstCheck();
	ApplySchedulerState();

	int iDiskSpaceInterval = 1000;
	int iSchedulerInterval = 1000;
	int iHistoryInterval = 600000;
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

		if (iHistoryInterval >= 600000)
		{
			// check history (remove old entries) every 10 minutes
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
	g_pQueueCoordinator->LockQueue();

#ifndef DISABLE_PARCHECK
	m_ParCoordinator.Stop();
#endif

	if (m_pCurJob && m_pCurJob->GetPostInfo() &&
		(m_pCurJob->GetPostInfo()->GetStage() == PostInfo::ptUnpacking ||
		 m_pCurJob->GetPostInfo()->GetStage() == PostInfo::ptExecutingScript) && 
		m_pCurJob->GetPostInfo()->GetPostThread())
	{
		Thread* pPostThread = m_pCurJob->GetPostInfo()->GetPostThread();
		m_pCurJob->GetPostInfo()->SetPostThread(NULL);
		pPostThread->SetAutoDestroy(true);
		pPostThread->Stop();
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
	if (pAspect->eAction == QueueCoordinator::eaNZBFileFound)
	{
		NZBFound(pAspect->pDownloadQueue, pAspect->pNZBInfo);
	}
	else if (pAspect->eAction == QueueCoordinator::eaNZBFileAdded)
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
			if ((pAspect->eAction == QueueCoordinator::eaFileCompleted ||
				(pAspect->pFileInfo->GetAutoDeleted() &&
				 IsNZBFileCompleted(pAspect->pDownloadQueue, pAspect->pNZBInfo, false, true))) &&
				 pAspect->pFileInfo->GetNZBInfo()->GetDeleteStatus() != NZBInfo::dsHealth)
			{
				info("Collection %s completely downloaded", pAspect->pNZBInfo->GetName());
				NZBDownloaded(pAspect->pDownloadQueue, pAspect->pNZBInfo);
			}
			else if ((pAspect->eAction == QueueCoordinator::eaFileDeleted ||
				(pAspect->eAction == QueueCoordinator::eaFileCompleted &&
				 pAspect->pFileInfo->GetNZBInfo()->GetDeleteStatus() > NZBInfo::dsNone)) &&
				!pAspect->pNZBInfo->GetParCleanup() && !pAspect->pNZBInfo->GetPostInfo() &&
				IsNZBFileCompleted(pAspect->pDownloadQueue, pAspect->pNZBInfo, false, true))
			{
				info("Collection %s deleted from queue", pAspect->pNZBInfo->GetName());
				NZBDeleted(pAspect->pDownloadQueue, pAspect->pNZBInfo);
			}
		}
	}
}

void PrePostProcessor::NZBFound(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	if (g_pOptions->GetDupeCheck() && pNZBInfo->GetDupeMode() != dmForce)
	{
		m_DupeCoordinator.NZBFound(pDownloadQueue, pNZBInfo);
	}
}

void PrePostProcessor::NZBAdded(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	if (g_pOptions->GetParCheck() != Options::pcForce)
	{
		m_ParCoordinator.PausePars(pDownloadQueue, pNZBInfo);
	}

	if (g_pOptions->GetDupeCheck() && pNZBInfo->GetDupeMode() != dmForce &&
		pNZBInfo->GetDeleteStatus() == NZBInfo::dsDupe)
	{
		NZBCompleted(pDownloadQueue, pNZBInfo, false);
	}
	else if (!Util::EmptyStr(g_pOptions->GetNZBAddedProcess()))
	{
		NZBAddedScriptController::StartScript(pDownloadQueue, pNZBInfo, g_pOptions->GetNZBAddedProcess());
	}
}

void PrePostProcessor::NZBDownloaded(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	if (!pNZBInfo->GetPostInfo() && g_pOptions->GetDecode())
	{
		info("Queueing %s for post-processing", pNZBInfo->GetName());

		pNZBInfo->EnterPostProcess();
		m_iJobCount++;

		if (pNZBInfo->GetParStatus() == NZBInfo::psNone && g_pOptions->GetParCheck() != Options::pcForce)
		{
			pNZBInfo->SetParStatus(NZBInfo::psSkipped);
		}

		if (pNZBInfo->GetRenameStatus() == NZBInfo::rsNone && !g_pOptions->GetParRename())
		{
			pNZBInfo->SetRenameStatus(NZBInfo::rsSkipped);
		}

		if (pNZBInfo->GetDeleteStatus() != NZBInfo::dsNone)
		{
			pNZBInfo->SetParStatus(NZBInfo::psFailure);
			pNZBInfo->SetUnpackStatus(NZBInfo::usFailure);
			pNZBInfo->SetRenameStatus(NZBInfo::rsFailure);
			pNZBInfo->SetMoveStatus(NZBInfo::msFailure);
		}

		SaveQueue(pDownloadQueue);
	}
	else
	{
		NZBCompleted(pDownloadQueue, pNZBInfo, true);
	}
}

void PrePostProcessor::NZBDeleted(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	if (pNZBInfo->GetDeleteStatus() == NZBInfo::dsNone)
	{
		pNZBInfo->SetDeleteStatus(NZBInfo::dsManual);
	}
	pNZBInfo->SetDeleting(false);

	if ((g_pOptions->GetDeleteCleanupDisk() && pNZBInfo->GetCleanupDisk()) ||
		pNZBInfo->GetDeleteStatus() == NZBInfo::dsDupe)
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
	}

	if (pNZBInfo->GetDeleteStatus() == NZBInfo::dsHealth)
	{
		NZBDownloaded(pDownloadQueue, pNZBInfo);
	}
	else
	{
		NZBCompleted(pDownloadQueue, pNZBInfo, true);
	}
}

void PrePostProcessor::NZBCompleted(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, bool bSaveQueue)
{
	bool bNeedSave = false;
	bool bAddToHistory = g_pOptions->GetKeepHistory() > 0 && !pNZBInfo->GetAvoidHistory();

	if (bAddToHistory)
	{
		//remove old item for the same NZB
		for (HistoryList::iterator it = pDownloadQueue->GetHistory()->begin(); it != pDownloadQueue->GetHistory()->end(); it++)
		{
			HistoryInfo* pHistoryInfo = *it;
			if (pHistoryInfo->GetNZBInfo() == pNZBInfo)
			{
				delete pHistoryInfo;
				pDownloadQueue->GetHistory()->erase(it);
				break;
			}
		}

		HistoryInfo* pHistoryInfo = new HistoryInfo(pNZBInfo);
		pHistoryInfo->SetTime(time(NULL));
		pDownloadQueue->GetHistory()->push_front(pHistoryInfo);
		pDownloadQueue->GetQueue()->Remove(pNZBInfo);

		if (pNZBInfo->GetDeleteStatus() == NZBInfo::dsNone)
		{
			// park files and delete files marked for deletion
			int iParkedFiles = 0;
			for (FileList::iterator it = pNZBInfo->GetFileList()->begin(); it != pNZBInfo->GetFileList()->end(); )
			{
				FileInfo* pFileInfo = *it;
				if (!pFileInfo->GetDeleted())
				{
					detail("Parking file %s", pFileInfo->GetFilename());
					g_pQueueCoordinator->DiscardDiskFile(pFileInfo);
					iParkedFiles++;
					it++;
				}
				else
				{
					// since we removed pNZBInfo from queue we need to take care of removing file infos marked for deletion
					pNZBInfo->GetFileList()->erase(it);
					delete pFileInfo;
					it = pNZBInfo->GetFileList()->begin() + iParkedFiles;
				}
			}
			pNZBInfo->SetParkedFileCount(iParkedFiles);
		}
		else
		{
			pNZBInfo->GetFileList()->Clear();
		}

		info("Collection %s added to history", pNZBInfo->GetName());
		bNeedSave = true;
	}

	pNZBInfo->SetAvoidHistory(false);

	if (g_pOptions->GetDupeCheck() && pNZBInfo->GetDupeMode() != dmForce &&
		(pNZBInfo->GetDeleteStatus() == NZBInfo::dsNone ||
		 pNZBInfo->GetDeleteStatus() == NZBInfo::dsHealth))
	{
		m_DupeCoordinator.NZBCompleted(pDownloadQueue, pNZBInfo);
		bNeedSave = true;
	}

	if (!bAddToHistory)
	{
		pDownloadQueue->GetQueue()->Remove(pNZBInfo);
		delete pNZBInfo;
	}

	if (bSaveQueue && bNeedSave)
	{
		SaveQueue(pDownloadQueue);
	}
}

/**
 * Removes old entries from (recent) history
 */
void PrePostProcessor::CheckHistory()
{
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	time_t tMinTime = time(NULL) - g_pOptions->GetKeepHistory() * 60*60*24;
	bool bChanged = false;
	int index = 0;

	// traversing in a reverse order to delete items in order they were added to history
	// (just to produce the log-messages in a more logical order)
	for (HistoryList::reverse_iterator it = pDownloadQueue->GetHistory()->rbegin(); it != pDownloadQueue->GetHistory()->rend(); )
	{
		HistoryInfo* pHistoryInfo = *it;
		if (pHistoryInfo->GetKind() != HistoryInfo::hkDupInfo && pHistoryInfo->GetTime() < tMinTime)
		{
			if (g_pOptions->GetDupeCheck() && pHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo)
			{
				// replace history element
				m_DupeCoordinator.HistoryTransformToDup(pDownloadQueue, pHistoryInfo, index);
				index++;
			}
			else
			{
				char szNiceName[1024];
				pHistoryInfo->GetName(szNiceName, 1024);

				pDownloadQueue->GetHistory()->erase(pDownloadQueue->GetHistory()->end() - 1 - index);
				delete pHistoryInfo;
				
				if (pHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo)
				{
					DeleteQueuedFile(pHistoryInfo->GetNZBInfo()->GetQueuedFilename());
				}
				info("Collection %s removed from history", szNiceName);
			}

			it = pDownloadQueue->GetHistory()->rbegin() + index;
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
	if (!g_pOptions->GetNzbCleanupDisk())
	{
		return;
	}

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

void PrePostProcessor::CheckDiskSpace()
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

void PrePostProcessor::CheckPostQueue()
{
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	if (!m_pCurJob && m_iJobCount > 0 && !g_pOptions->GetPausePostProcess())
	{
		m_pCurJob = GetNextJob(pDownloadQueue);
		if (!m_pCurJob)
		{
			error("Internal error: no jobs found in queue");
			m_iJobCount = 0;
		}
	}

	if (m_pCurJob)
	{
		PostInfo* pPostInfo = m_pCurJob->GetPostInfo();
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

				if (!pPostInfo->GetNZBInfo()->GetFileList()->empty())
				{
					info("Downloading all remaining files for manual par-check for %s", pPostInfo->GetNZBInfo()->GetName());
					g_pQueueCoordinator->GetQueueEditor()->LockedEditEntry(pDownloadQueue,
						pPostInfo->GetNZBInfo()->GetID(), QueueEditor::eaGroupResume, 0, NULL);
					pPostInfo->SetStage(PostInfo::ptFinished);
				}
				else
				{
					info("There are no par-files remain for download for %s", pPostInfo->GetNZBInfo()->GetName());
					pPostInfo->SetStage(PostInfo::ptQueued);
				}
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
				// TODO: cancel (delete) current job
			}
		}
	}
	
	g_pQueueCoordinator->UnlockQueue();
}

NZBInfo* PrePostProcessor::GetNextJob(DownloadQueue* pDownloadQueue)
{
	NZBInfo* pNZBInfo = NULL;

	for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
	{
		NZBInfo* pNZBInfo1 = *it;
		if (pNZBInfo1->GetPostInfo() && (!pNZBInfo || pNZBInfo1->GetPriority() > pNZBInfo->GetPriority()))
		{
			pNZBInfo = pNZBInfo1;
		}
	}

	return pNZBInfo;
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
 * Also count the number of post-jobs.
 */
void PrePostProcessor::SanitisePostQueue(DownloadQueue* pDownloadQueue)
{
	for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
	{
		NZBInfo* pNZBInfo = *it;
		PostInfo* pPostInfo = pNZBInfo->GetPostInfo();
		if (pPostInfo)
		{
			m_iJobCount++;
			if (pPostInfo->GetStage() == PostInfo::ptExecutingScript ||
				!Util::DirectoryExists(pNZBInfo->GetDestDir()))
			{
				pPostInfo->SetStage(PostInfo::ptFinished);
			}
			else
			{
				pPostInfo->SetStage(PostInfo::ptQueued);
			}
		}
	}
}

void PrePostProcessor::DeletePostThread(PostInfo* pPostInfo)
{
	delete pPostInfo->GetPostThread();
	pPostInfo->SetPostThread(NULL);
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
			info("Nothing to par-check for %s", pPostInfo->GetNZBInfo()->GetName());
			pPostInfo->GetNZBInfo()->SetParStatus(NZBInfo::psSkipped);
			pPostInfo->SetWorking(false);
			pPostInfo->SetStage(PostInfo::ptQueued);
		}
		return;
	}
	else if (pPostInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psSkipped &&
		pPostInfo->GetNZBInfo()->CalcHealth() < pPostInfo->GetNZBInfo()->CalcCriticalHealth(false) &&
		pPostInfo->GetNZBInfo()->CalcCriticalHealth(false) < 1000 &&
		m_ParCoordinator.FindMainPars(pPostInfo->GetNZBInfo()->GetDestDir(), NULL))
	{
		warn("Skipping par-check for %s due to health %.1f%% below critical %.1f%%", pPostInfo->GetNZBInfo()->GetName(),
			pPostInfo->GetNZBInfo()->CalcHealth() / 10.0, pPostInfo->GetNZBInfo()->CalcCriticalHealth(false) / 10.0);
		pPostInfo->GetNZBInfo()->SetParStatus(NZBInfo::psFailure);
		return;
	}
	else if (pPostInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psSkipped &&
		pPostInfo->GetNZBInfo()->GetFailedSize() - pPostInfo->GetNZBInfo()->GetParFailedSize() > 0 &&
		m_ParCoordinator.FindMainPars(pPostInfo->GetNZBInfo()->GetDestDir(), NULL))
	{
		info("Collection %s with health %.1f%% needs par-check",
			pPostInfo->GetNZBInfo()->GetName(), pPostInfo->GetNZBInfo()->CalcHealth() / 10.0);
		pPostInfo->SetRequestParCheck(true);
		return;
	}
#endif

	NZBParameter* pUnpackParameter = pPostInfo->GetNZBInfo()->GetParameters()->Find("*Unpack:", false);
	bool bUnpackParam = !(pUnpackParameter && !strcasecmp(pUnpackParameter->GetValue(), "no"));
	bool bUnpack = bUnpackParam && (pPostInfo->GetNZBInfo()->GetUnpackStatus() == NZBInfo::usNone);

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
		pPostInfo->GetNZBInfo()->GetUnpackStatus() != NZBInfo::usSpace &&
		pPostInfo->GetNZBInfo()->GetUnpackStatus() != NZBInfo::usPassword &&
		pPostInfo->GetNZBInfo()->GetParStatus() != NZBInfo::psFailure &&
		pPostInfo->GetNZBInfo()->GetParStatus() != NZBInfo::psManual &&
		strlen(g_pOptions->GetInterDir()) > 0 &&
		!strncmp(pPostInfo->GetNZBInfo()->GetDestDir(), g_pOptions->GetInterDir(), strlen(g_pOptions->GetInterDir()));

	bool bPostScript = true;

	if (bUnpack && bParFailed)
	{
		warn("Skipping unpack for %s due to %s", pPostInfo->GetNZBInfo()->GetName(),
			pPostInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psManual ? "required par-repair" : "par-failure");
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
	NZBInfo* pNZBInfo = pPostInfo->GetNZBInfo();
	DeletePostThread(pPostInfo);
	pNZBInfo->LeavePostProcess();

	if (IsNZBFileCompleted(pDownloadQueue, pNZBInfo, true, false))
	{
		// Cleaning up queue if par-check was successful or unpack was successful or
		// script was successful (if unpack was not performed)
		bool bCanCleanupQueue = pNZBInfo->GetParStatus() == NZBInfo::psSuccess ||
			 pNZBInfo->GetParStatus() == NZBInfo::psRepairPossible ||
			 pNZBInfo->GetUnpackStatus() == NZBInfo::usSuccess ||
			 (pNZBInfo->GetUnpackStatus() == NZBInfo::usNone &&
			  pNZBInfo->GetScriptStatuses()->CalcTotalStatus() == ScriptStatus::srSuccess);
		if (g_pOptions->GetParCleanupQueue() && bCanCleanupQueue)
		{
			if (!pNZBInfo->GetFileList()->empty())
			{
				info("Cleaning up download queue for %s", pNZBInfo->GetName());
				pNZBInfo->ClearCompletedFiles();
				pNZBInfo->SetParCleanup(true);
				g_pQueueCoordinator->GetQueueEditor()->LockedEditEntry(pDownloadQueue, pNZBInfo->GetID(),
					QueueEditor::eaGroupDelete, 0, NULL);
			}
		}

		NZBCompleted(pDownloadQueue, pNZBInfo, false);
	}

	if (pNZBInfo == m_pCurJob)
	{
		m_pCurJob = NULL;
	}
	m_iJobCount--;

	SaveQueue(pDownloadQueue);
}

bool PrePostProcessor::IsNZBFileCompleted(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo,
	bool bIgnorePausedPars, bool bAllowOnlyOneDeleted)
{
	bool bNZBFileCompleted = true;
	int iDeleted = 0;

	for (FileList::iterator it = pNZBInfo->GetFileList()->begin(); it != pNZBInfo->GetFileList()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (pFileInfo->GetDeleted())
		{
			iDeleted++;
		}
		if (((!pFileInfo->GetPaused() || !bIgnorePausedPars || !pFileInfo->GetParFile()) &&
			!pFileInfo->GetDeleted()) ||
			(bAllowOnlyOneDeleted && iDeleted > 1))
		{
			bNZBFileCompleted = false;
			break;
		}
	}

	return bNZBFileCompleted;
}

void PrePostProcessor::ApplySchedulerState()
{
	if (g_pScheduler->GetPauseDownloadChanged())
	{
		info("Scheduler: %s download queue", g_pScheduler->GetPauseDownload() ? "pausing" : "unpausing");
		m_bSchedulerPauseChanged = true;
		m_bSchedulerPause = g_pScheduler->GetPauseDownload();
		if (!m_bPostPause)
		{
			g_pOptions->SetPauseDownload(m_bSchedulerPause);
		}
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
		case eaPostDelete:
			return PostQueueDelete(pIDList);

		case eaHistoryDelete:
		case eaHistoryFinalDelete:
		case eaHistoryReturn:
		case eaHistoryProcess:
		case eaHistoryRedownload:
		case eaHistorySetParameter:
		case eaHistorySetDupeKey:
		case eaHistorySetDupeScore:
		case eaHistorySetDupeMode:
		case eaHistorySetDupeBackup:
		case eaHistoryMarkBad:
		case eaHistoryMarkGood:
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

		for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
		{
			NZBInfo* pNZBInfo = *it;
			PostInfo* pPostInfo = pNZBInfo->GetPostInfo();
			if (pPostInfo && pNZBInfo->GetID() == iID)
			{
				if (pPostInfo->GetWorking())
				{
					info("Deleting active post-job %s", pPostInfo->GetNZBInfo()->GetName());
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
						debug("Terminating %s for %s", (pPostInfo->GetStage() == PostInfo::ptUnpacking ? "unpack" : "post-process-script"), pPostInfo->GetNZBInfo()->GetName());
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
					info("Deleting queued post-job %s", pPostInfo->GetNZBInfo()->GetName());
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

bool PrePostProcessor::HistoryEdit(IDList* pIDList, EEditAction eAction, int iOffset, const char* szText)
{
	bool bOK = false;

	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	for (IDList::iterator itID = pIDList->begin(); itID != pIDList->end(); itID++)
	{
		int iID = *itID;
		for (HistoryList::iterator itHistory = pDownloadQueue->GetHistory()->begin(); itHistory != pDownloadQueue->GetHistory()->end(); itHistory++)
		{
			HistoryInfo* pHistoryInfo = *itHistory;
			if (pHistoryInfo->GetID() == iID)
			{
				switch (eAction)
				{
					case eaHistoryDelete:
					case eaHistoryFinalDelete:
						HistoryDelete(pDownloadQueue, itHistory, pHistoryInfo, eAction == eaHistoryFinalDelete);
						break;

					case eaHistoryReturn:
					case eaHistoryProcess:
						HistoryReturn(pDownloadQueue, itHistory, pHistoryInfo, eAction == eaHistoryProcess);
						break;

					case eaHistoryRedownload:
						HistoryRedownload(pDownloadQueue, itHistory, pHistoryInfo, false);
						break;

 					case eaHistorySetParameter:
						HistorySetParameter(pHistoryInfo, szText);
						break;

					case eaHistorySetDupeKey:
					case eaHistorySetDupeScore:
					case eaHistorySetDupeMode:
					case eaHistorySetDupeBackup:
						HistorySetDupeParam(pHistoryInfo, eAction, szText);
						break;

					case eaHistoryMarkBad:
					case eaHistoryMarkGood:
						m_DupeCoordinator.HistoryMark(pDownloadQueue, pHistoryInfo, eAction == eaHistoryMarkGood);
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

void PrePostProcessor::HistoryDelete(DownloadQueue* pDownloadQueue, HistoryList::iterator itHistory,
	HistoryInfo* pHistoryInfo, bool bFinal)
{
	char szNiceName[1024];
	pHistoryInfo->GetName(szNiceName, 1024);
	info("Deleting %s from history", szNiceName);

	if (pHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo)
	{
		NZBInfo* pNZBInfo = pHistoryInfo->GetNZBInfo();

		// delete parked files
		if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
		{
			for (FileList::iterator it = pNZBInfo->GetFileList()->begin(); it != pNZBInfo->GetFileList()->end(); it++)
			{
				FileInfo* pFileInfo = *it;
				g_pDiskState->DiscardFile(pFileInfo);
			}
		}
		pNZBInfo->GetFileList()->Clear();

		DeleteQueuedFile(pNZBInfo->GetQueuedFilename());
	}

	if (pHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo &&
		g_pOptions->GetDeleteCleanupDisk() &&
		(pHistoryInfo->GetNZBInfo()->GetDeleteStatus() != NZBInfo::dsNone ||
		pHistoryInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psFailure ||
		pHistoryInfo->GetNZBInfo()->GetUnpackStatus() == NZBInfo::usFailure ||
		pHistoryInfo->GetNZBInfo()->GetUnpackStatus() == NZBInfo::usPassword) &&
		Util::DirectoryExists(pHistoryInfo->GetNZBInfo()->GetDestDir()))
	{
		info("Deleting %s", pHistoryInfo->GetNZBInfo()->GetDestDir());
		Util::DeleteDirectoryWithContent(pHistoryInfo->GetNZBInfo()->GetDestDir());
	}

	if (bFinal || !g_pOptions->GetDupeCheck() || pHistoryInfo->GetKind() == HistoryInfo::hkUrlInfo)
	{
		pDownloadQueue->GetHistory()->erase(itHistory);
		delete pHistoryInfo;
	}
	else
	{
		if (pHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo)
		{
			// replace history element
			int rindex = pDownloadQueue->GetHistory()->size() - 1 - (itHistory - pDownloadQueue->GetHistory()->begin());
			m_DupeCoordinator.HistoryTransformToDup(pDownloadQueue, pHistoryInfo, rindex);
		}
	}
}

void PrePostProcessor::HistoryReturn(DownloadQueue* pDownloadQueue, HistoryList::iterator itHistory, HistoryInfo* pHistoryInfo, bool bReprocess)
{
	char szNiceName[1024];
	pHistoryInfo->GetName(szNiceName, 1024);
	debug("Returning %s from history back to download queue", szNiceName);
	bool bUnparked = false;
	NZBInfo* pNZBInfo = NULL;

	if (bReprocess && pHistoryInfo->GetKind() != HistoryInfo::hkNZBInfo)
	{
		error("Could not restart postprocessing for %s: history item has wrong type", szNiceName);
		return;
	}

	if (pHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo)
	{
		pNZBInfo = pHistoryInfo->GetNZBInfo();

		// unpark files
		for (FileList::iterator it = pNZBInfo->GetFileList()->begin(); it != pNZBInfo->GetFileList()->end(); it++)
		{
			FileInfo* pFileInfo = *it;
			detail("Unpark file %s", pFileInfo->GetFilename());
			bUnparked = true;
		}

		pDownloadQueue->GetQueue()->push_front(pNZBInfo);
		pHistoryInfo->DiscardNZBInfo();

		// reset postprocessing status variables
		pNZBInfo->SetParCleanup(false);
		if (!pNZBInfo->GetUnpackCleanedUpDisk())
		{
			pNZBInfo->SetUnpackStatus(NZBInfo::usNone);
			pNZBInfo->SetCleanupStatus(NZBInfo::csNone);
			pNZBInfo->SetRenameStatus(NZBInfo::rsNone);

			if (m_ParCoordinator.FindMainPars(pNZBInfo->GetDestDir(), NULL))
			{
				pNZBInfo->SetParStatus(NZBInfo::psNone);
			}
		}
		pNZBInfo->SetDeleteStatus(NZBInfo::dsNone);
		pNZBInfo->SetDeletePaused(false);
		pNZBInfo->SetMarkStatus(NZBInfo::ksNone);
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
		pDownloadQueue->GetHistory()->erase(itHistory);
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
		NZBDownloaded(pDownloadQueue, pNZBInfo);
	}

	if (bUnparked || bReprocess)
	{
		delete pHistoryInfo;
	}
}

void PrePostProcessor::HistoryRedownload(DownloadQueue* pDownloadQueue, HistoryList::iterator itHistory,
	HistoryInfo* pHistoryInfo, bool bRestorePauseState)
{
	NZBInfo* pNZBInfo = pHistoryInfo->GetNZBInfo();
	bool bPaused = bRestorePauseState && pNZBInfo->GetDeletePaused();

	if (!Util::FileExists(pNZBInfo->GetQueuedFilename()))
	{
		error("Could not return collection %s from history back to queue: could not find source nzb-file %s",
			pNZBInfo->GetName(), pNZBInfo->GetQueuedFilename());
		return;
	}

	NZBFile* pNZBFile = NZBFile::Create(pNZBInfo->GetQueuedFilename(), "");
	if (pNZBFile == NULL)
	{
		error("Could not return collection %s from history back to queue: could not parse nzb-file",
			pNZBInfo->GetName());
		return;
	}

	info("Returning collection %s from history back to queue", pNZBInfo->GetName());

	for (FileList::iterator it = pNZBFile->GetNZBInfo()->GetFileList()->begin(); it != pNZBFile->GetNZBInfo()->GetFileList()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		pFileInfo->SetPaused(bPaused);
	}

	if (Util::DirectoryExists(pNZBInfo->GetDestDir()))
	{
		detail("Deleting %s", pNZBInfo->GetDestDir());
		Util::DeleteDirectoryWithContent(pNZBInfo->GetDestDir());
	}

	pNZBInfo->BuildDestDirName();
	if (Util::DirectoryExists(pNZBInfo->GetDestDir()))
	{
		detail("Deleting %s", pNZBInfo->GetDestDir());
		Util::DeleteDirectoryWithContent(pNZBInfo->GetDestDir());
	}

	// reset status fields (which are not reset by "HistoryReturn")
	pNZBInfo->SetMoveStatus(NZBInfo::msNone);
	pNZBInfo->SetUnpackCleanedUpDisk(false);
	pNZBInfo->SetParStatus(NZBInfo::psNone);
	pNZBInfo->SetRenameStatus(NZBInfo::rsNone);
	pNZBInfo->ClearCompletedFiles();
	pNZBInfo->GetServerStats()->Clear();

	pNZBInfo->CopyFileList(pNZBFile->GetNZBInfo());

	g_pQueueCoordinator->CheckDupeFileInfos(pNZBInfo);
	delete pNZBFile;

	HistoryReturn(pDownloadQueue, itHistory, pHistoryInfo, false);

	if (!bPaused && g_pOptions->GetParCheck() != Options::pcForce)
	{
		g_pQueueCoordinator->GetQueueEditor()->LockedEditEntry(pDownloadQueue, pNZBInfo->GetID(), 
			QueueEditor::eaGroupPauseExtraPars, 0, NULL);
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
		pHistoryInfo->GetNZBInfo()->GetParameters()->SetParameter(szStr, szValue);
	}
	else
	{
		error("Could not set post-process-parameter for %s: invalid argument: %s", pHistoryInfo->GetNZBInfo()->GetName(), szText);
	}

	free(szStr);
}

void PrePostProcessor::HistorySetDupeParam(HistoryInfo* pHistoryInfo, EEditAction eAction, const char* szText)
{
	char szNiceName[1024];
	pHistoryInfo->GetName(szNiceName, 1024);
	debug("Setting dupe-parameter '%i'='%s' for '%s'", (int)eAction, szText, szNiceName);

	if (!(pHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo || 
		pHistoryInfo->GetKind() == HistoryInfo::hkDupInfo))
	{
		error("Could not set duplicate parameter for %s: history item has wrong type", szNiceName);
		return;
	}

	EDupeMode eMode = dmScore;
	if (eAction == eaHistorySetDupeMode)
	{
		if (!strcasecmp(szText, "SCORE"))
		{
			eMode = dmScore;
		}
		else if (!strcasecmp(szText, "ALL"))
		{
			eMode = dmAll;
		}
		else if (!strcasecmp(szText, "FORCE"))
		{
			eMode = dmForce;
		}
		else
		{
			error("Could not set duplicate mode for %s: incorrect mode (%s)", szNiceName, szText);
			return;
		}
	}

	if (pHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo)
	{
		switch (eAction) 
		{
			case eaHistorySetDupeKey:
				pHistoryInfo->GetNZBInfo()->SetDupeKey(szText);
				break;

			case eaHistorySetDupeScore:
				pHistoryInfo->GetNZBInfo()->SetDupeScore(atoi(szText));
				break;

			case eaHistorySetDupeMode:
				pHistoryInfo->GetNZBInfo()->SetDupeMode(eMode);
				break;

			case eaHistorySetDupeBackup:
				if (pHistoryInfo->GetNZBInfo()->GetDeleteStatus() != NZBInfo::dsDupe &&
					pHistoryInfo->GetNZBInfo()->GetDeleteStatus() != NZBInfo::dsManual)
				{
					error("Could not set duplicate parameter for %s: history item has wrong delete status", szNiceName);
					return;
				}
				pHistoryInfo->GetNZBInfo()->SetDeleteStatus(!strcasecmp(szText, "YES") ||
					!strcasecmp(szText, "TRUE") || !strcasecmp(szText, "1") ? NZBInfo::dsDupe : NZBInfo::dsManual);
				break;

			default:
				// suppress compiler warning
				break;
		}
	}
	else if (pHistoryInfo->GetKind() == HistoryInfo::hkDupInfo)
	{
		switch (eAction) 
		{
			case eaHistorySetDupeKey:
				pHistoryInfo->GetDupInfo()->SetDupeKey(szText);
				break;

			case eaHistorySetDupeScore:
				pHistoryInfo->GetDupInfo()->SetDupeScore(atoi(szText));
				break;

			case eaHistorySetDupeMode:
				pHistoryInfo->GetDupInfo()->SetDupeMode(eMode);
				break;

			case eaHistorySetDupeBackup:
				error("Could not set duplicate parameter for %s: history item has wrong type", szNiceName);
				return;

			default:
				// suppress compiler warning
				break;
		}
	}
}
