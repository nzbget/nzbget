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
#include "HistoryCoordinator.h"
#include "DupeCoordinator.h"
#include "PostScript.h"
#include "Util.h"
#include "Scheduler.h"
#include "Scanner.h"
#include "Unpack.h"
#include "NZBFile.h"
#include "StatMeter.h"

extern HistoryCoordinator* g_pHistoryCoordinator;
extern DupeCoordinator* g_pDupeCoordinator;
extern Options* g_pOptions;
extern Scheduler* g_pScheduler;
extern Scanner* g_pScanner;
extern StatMeter* g_pStatMeter;

PrePostProcessor::PrePostProcessor()
{
	debug("Creating PrePostProcessor");

	m_iJobCount = 0;
	m_pCurJob = NULL;
	m_szPauseReason = NULL;

	m_DownloadQueueObserver.m_pOwner = this;
	DownloadQueue* pDownloadQueue = DownloadQueue::Lock();
	pDownloadQueue->Attach(&m_DownloadQueueObserver);
	DownloadQueue::Unlock();
}

PrePostProcessor::~PrePostProcessor()
{
	debug("Destroying PrePostProcessor");
}

void PrePostProcessor::Run()
{
	debug("Entering PrePostProcessor-loop");

	while (!DownloadQueue::IsLoaded())
	{
		usleep(20 * 1000);
	}

	if (g_pOptions->GetServerMode() && g_pOptions->GetSaveQueue() && g_pOptions->GetReloadQueue())
	{
		DownloadQueue* pDownloadQueue = DownloadQueue::Lock();
		SanitisePostQueue(pDownloadQueue);
		DownloadQueue::Unlock();
	}

	g_pScheduler->FirstCheck();

	int iDiskSpaceInterval = 1000;
	int iSchedulerInterval = 1000;
	int iHistoryInterval = 600000;
	const int iStepMSec = 200;

	while (!IsStopped())
	{
		// check incoming nzb directory
		g_pScanner->Check();

		if (!g_pOptions->GetPauseDownload() && 
			g_pOptions->GetDiskSpace() > 0 && !g_pStatMeter->GetStandBy() && 
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
			iSchedulerInterval = 0;
		}
		iSchedulerInterval += iStepMSec;

		if (iHistoryInterval >= 600000)
		{
			// check history (remove old entries) every 10 minutes
			g_pHistoryCoordinator->IntervalCheck();
			iHistoryInterval = 0;
		}
		iHistoryInterval += iStepMSec;

		Util::SetStandByMode(!m_pCurJob);

		usleep(iStepMSec * 1000);
	}

	g_pHistoryCoordinator->Cleanup();

	debug("Exiting PrePostProcessor-loop");
}

void PrePostProcessor::Stop()
{
	Thread::Stop();
	DownloadQueue::Lock();

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

	DownloadQueue::Unlock();
}

void PrePostProcessor::DownloadQueueUpdate(Subject* Caller, void* Aspect)
{
	if (IsStopped())
	{
		return;
	}

	DownloadQueue::Aspect* pQueueAspect = (DownloadQueue::Aspect*)Aspect;
	if (pQueueAspect->eAction == DownloadQueue::eaNzbFound)
	{
		NZBFound(pQueueAspect->pDownloadQueue, pQueueAspect->pNZBInfo);
	}
	else if (pQueueAspect->eAction == DownloadQueue::eaNzbAdded)
	{
		NZBAdded(pQueueAspect->pDownloadQueue, pQueueAspect->pNZBInfo);
	}
	else if ((pQueueAspect->eAction == DownloadQueue::eaFileCompleted ||
		pQueueAspect->eAction == DownloadQueue::eaFileDeleted))
	{
		if (
#ifndef DISABLE_PARCHECK
			!m_ParCoordinator.AddPar(pQueueAspect->pFileInfo, pQueueAspect->eAction == DownloadQueue::eaFileDeleted) &&
#endif
			IsNZBFileCompleted(pQueueAspect->pNZBInfo, true, false) &&
			!pQueueAspect->pNZBInfo->GetPostInfo() &&
			(!pQueueAspect->pFileInfo->GetPaused() || IsNZBFileCompleted(pQueueAspect->pNZBInfo, false, false)))
		{
			if ((pQueueAspect->eAction == DownloadQueue::eaFileCompleted ||
				(pQueueAspect->pFileInfo->GetAutoDeleted() &&
				 IsNZBFileCompleted(pQueueAspect->pNZBInfo, false, true))) &&
				 pQueueAspect->pFileInfo->GetNZBInfo()->GetDeleteStatus() != NZBInfo::dsHealth)
			{
				info("Collection %s completely downloaded", pQueueAspect->pNZBInfo->GetName());
				NZBDownloaded(pQueueAspect->pDownloadQueue, pQueueAspect->pNZBInfo);
			}
			else if ((pQueueAspect->eAction == DownloadQueue::eaFileDeleted ||
				(pQueueAspect->eAction == DownloadQueue::eaFileCompleted &&
				 pQueueAspect->pFileInfo->GetNZBInfo()->GetDeleteStatus() > NZBInfo::dsNone)) &&
				!pQueueAspect->pNZBInfo->GetParCleanup() &&
				IsNZBFileCompleted(pQueueAspect->pNZBInfo, false, true))
			{
				info("Collection %s deleted from queue", pQueueAspect->pNZBInfo->GetName());
				NZBDeleted(pQueueAspect->pDownloadQueue, pQueueAspect->pNZBInfo);
			}
		}
	}
}

void PrePostProcessor::NZBFound(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	if (g_pOptions->GetDupeCheck() && pNZBInfo->GetDupeMode() != dmForce)
	{
		g_pDupeCoordinator->NZBFound(pDownloadQueue, pNZBInfo);
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
	else if (!Util::EmptyStr(g_pOptions->GetQueueScript()))
	{
		QueueScriptController::StartScripts(pNZBInfo, QueueScriptController::qeNzbAdded, false);
	}
}

void PrePostProcessor::NZBDownloaded(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	if (!pNZBInfo->GetPostInfo() && g_pOptions->GetDecode())
	{
		info("Queueing %s for post-processing", pNZBInfo->GetName());

		pNZBInfo->EnterPostProcess();
		m_iJobCount++;

		if (pNZBInfo->GetParStatus() == NZBInfo::psNone &&
			g_pOptions->GetParCheck() != Options::pcAlways &&
			g_pOptions->GetParCheck() != Options::pcForce)
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

		pDownloadQueue->Save();
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
		for (CompletedFiles::reverse_iterator it = pNZBInfo->GetCompletedFiles()->rbegin(); it != pNZBInfo->GetCompletedFiles()->rend(); it++)
		{
			CompletedFile* pCompletedFile = *it;

			char szFullFileName[1024];
			snprintf(szFullFileName, 1024, "%s%c%s", pNZBInfo->GetDestDir(), (int)PATH_SEPARATOR, pCompletedFile->GetFileName());
			szFullFileName[1024-1] = '\0';

			if (Util::FileExists(szFullFileName))
			{
				detail("Deleting file %s", pCompletedFile->GetFileName());
				remove(szFullFileName);
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
	bool bAddToHistory = g_pOptions->GetKeepHistory() > 0 && !pNZBInfo->GetAvoidHistory();
	if (bAddToHistory)
	{
		g_pHistoryCoordinator->AddToHistory(pDownloadQueue, pNZBInfo);
	}
	pNZBInfo->SetAvoidHistory(false);

	bool bNeedSave = bAddToHistory;

	if (g_pOptions->GetDupeCheck() && pNZBInfo->GetDupeMode() != dmForce &&
		(pNZBInfo->GetDeleteStatus() == NZBInfo::dsNone ||
		 pNZBInfo->GetDeleteStatus() == NZBInfo::dsHealth))
	{
		g_pDupeCoordinator->NZBCompleted(pDownloadQueue, pNZBInfo);
		bNeedSave = true;
	}

	if (!bAddToHistory)
	{
		g_pHistoryCoordinator->DeleteDiskFiles(pNZBInfo);
		pDownloadQueue->GetQueue()->Remove(pNZBInfo);
		delete pNZBInfo;
	}

	if (bSaveQueue && bNeedSave)
	{
		pDownloadQueue->Save();
	}
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
	DownloadQueue* pDownloadQueue = DownloadQueue::Lock();

	if (!m_pCurJob && m_iJobCount > 0)
	{
		m_pCurJob = GetNextJob(pDownloadQueue);
		if (!m_pCurJob && !g_pOptions->GetPausePostProcess())
		{
			error("Internal error: no jobs found in queue");
			m_iJobCount = 0;
		}
	}

	if (m_pCurJob)
	{
		PostInfo* pPostInfo = m_pCurJob->GetPostInfo();
		if (!pPostInfo->GetWorking() && !IsNZBFileDownloading(m_pCurJob))
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
					pDownloadQueue->EditEntry(pPostInfo->GetNZBInfo()->GetID(), DownloadQueue::eaGroupResume, 0, NULL);
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

			if (pPostInfo->GetStage() == PostInfo::ptQueued &&
				(!g_pOptions->GetPausePostProcess() || pPostInfo->GetNZBInfo()->GetForcePriority()))
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
	
	DownloadQueue::Unlock();
}

NZBInfo* PrePostProcessor::GetNextJob(DownloadQueue* pDownloadQueue)
{
	NZBInfo* pNZBInfo = NULL;

	for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
	{
		NZBInfo* pNZBInfo1 = *it;
		if (pNZBInfo1->GetPostInfo() && (!pNZBInfo || pNZBInfo1->GetPriority() > pNZBInfo->GetPriority()) &&
			(!g_pOptions->GetPausePostProcess() || pNZBInfo1->GetForcePriority()))
		{
			pNZBInfo = pNZBInfo1;
		}
	}

	return pNZBInfo;
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
	if (!pPostInfo->GetStartTime())
	{
		pPostInfo->SetStartTime(time(NULL));
	}

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

	pDownloadQueue->Save();

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

	if (pPostInfo->GetStartTime() > 0)
	{
		pNZBInfo->SetPostTotalSec((int)(time(NULL) - pPostInfo->GetStartTime()));
		pPostInfo->SetStartTime(0);
	}

	DeletePostThread(pPostInfo);
	pNZBInfo->LeavePostProcess();

	if (IsNZBFileCompleted(pNZBInfo, true, false))
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
				pNZBInfo->SetParCleanup(true);
				pDownloadQueue->EditEntry(pNZBInfo->GetID(), DownloadQueue::eaGroupDelete, 0, NULL);
			}
		}

		if (pNZBInfo->GetUnpackCleanedUpDisk())
		{
			pNZBInfo->ClearCompletedFiles();
		}

		NZBCompleted(pDownloadQueue, pNZBInfo, false);
	}

	if (pNZBInfo == m_pCurJob)
	{
		m_pCurJob = NULL;
	}
	m_iJobCount--;

	pDownloadQueue->Save();
}

bool PrePostProcessor::IsNZBFileCompleted(NZBInfo* pNZBInfo, bool bIgnorePausedPars, bool bAllowOnlyOneDeleted)
{
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
			return false;
		}
	}

	return true;
}

bool PrePostProcessor::IsNZBFileDownloading(NZBInfo* pNZBInfo)
{
	if (pNZBInfo->GetActiveDownloads())
	{
		return true;
	}

	for (FileList::iterator it = pNZBInfo->GetFileList()->begin(); it != pNZBInfo->GetFileList()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (!pFileInfo->GetPaused())
		{
			return true;
		}
	}

	return false;
}

void PrePostProcessor::UpdatePauseState(bool bNeedPause, const char* szReason)
{
	if (bNeedPause && !g_pOptions->GetTempPauseDownload())
	{
		info("Pausing download before %s", szReason);
	}
	else if (!bNeedPause && g_pOptions->GetTempPauseDownload())
	{
		info("Unpausing download after %s", m_szPauseReason);
	}
	g_pOptions->SetTempPauseDownload(bNeedPause);
	m_szPauseReason = szReason;
}

bool PrePostProcessor::EditList(DownloadQueue* pDownloadQueue, IDList* pIDList, DownloadQueue::EEditAction eAction, int iOffset, const char* szText)
{
	debug("Edit-command for post-processor received");
	switch (eAction)
	{
		case DownloadQueue::eaPostDelete:
			return PostQueueDelete(pDownloadQueue, pIDList);

		default:
			return false;
	}
}

bool PrePostProcessor::PostQueueDelete(DownloadQueue* pDownloadQueue, IDList* pIDList)
{
	bool bOK = false;

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

	return bOK;
}
