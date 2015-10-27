/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "Unpack.h"
#include "Cleanup.h"
#include "NZBFile.h"
#include "QueueScript.h"
#include "ParParser.h"

PrePostProcessor::PrePostProcessor()
{
	debug("Creating PrePostProcessor");

	m_jobCount = 0;
	m_curJob = NULL;
	m_pauseReason = NULL;

	m_downloadQueueObserver.m_owner = this;
	DownloadQueue* downloadQueue = DownloadQueue::Lock();
	downloadQueue->Attach(&m_downloadQueueObserver);
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
		DownloadQueue* downloadQueue = DownloadQueue::Lock();
		SanitisePostQueue(downloadQueue);
		DownloadQueue::Unlock();
	}

	while (!IsStopped())
	{
		if (!g_pOptions->GetTempPausePostprocess())
		{
			// check post-queue every 200 msec
			CheckPostQueue();
		}

		Util::SetStandByMode(!m_curJob);

		usleep(200 * 1000);
	}

	debug("Exiting PrePostProcessor-loop");
}

void PrePostProcessor::Stop()
{
	Thread::Stop();
	DownloadQueue::Lock();

#ifndef DISABLE_PARCHECK
	m_parCoordinator.Stop();
#endif

	if (m_curJob && m_curJob->GetPostInfo() &&
		(m_curJob->GetPostInfo()->GetStage() == PostInfo::ptUnpacking ||
		 m_curJob->GetPostInfo()->GetStage() == PostInfo::ptExecutingScript) && 
		m_curJob->GetPostInfo()->GetPostThread())
	{
		Thread* postThread = m_curJob->GetPostInfo()->GetPostThread();
		m_curJob->GetPostInfo()->SetPostThread(NULL);
		postThread->SetAutoDestroy(true);
		postThread->Stop();
	}

	DownloadQueue::Unlock();
}

void PrePostProcessor::DownloadQueueUpdate(Subject* Caller, void* Aspect)
{
	if (IsStopped())
	{
		return;
	}

	DownloadQueue::Aspect* queueAspect = (DownloadQueue::Aspect*)Aspect;
	if (queueAspect->action == DownloadQueue::eaNzbFound)
	{
		NZBFound(queueAspect->downloadQueue, queueAspect->nzbInfo);
	}
	else if (queueAspect->action == DownloadQueue::eaNzbAdded)
	{
		NZBAdded(queueAspect->downloadQueue, queueAspect->nzbInfo);
	}
	else if (queueAspect->action == DownloadQueue::eaNzbDeleted &&
		queueAspect->nzbInfo->GetDeleting() &&
		!queueAspect->nzbInfo->GetPostInfo() &&
		!queueAspect->nzbInfo->GetParCleanup() &&
		queueAspect->nzbInfo->GetFileList()->empty())
	{
		// the deleting of nzbs is usually handled via eaFileDeleted-event, but when deleting nzb without
		// any files left the eaFileDeleted-event is not fired and we need to process eaNzbDeleted-event instead
		queueAspect->nzbInfo->PrintMessage(Message::mkInfo,
			"Collection %s deleted from queue", queueAspect->nzbInfo->GetName());
		NZBDeleted(queueAspect->downloadQueue, queueAspect->nzbInfo);
	}
	else if ((queueAspect->action == DownloadQueue::eaFileCompleted ||
		queueAspect->action == DownloadQueue::eaFileDeleted))
	{
		if (queueAspect->action == DownloadQueue::eaFileCompleted && !queueAspect->nzbInfo->GetPostInfo())
		{
			g_pQueueScriptCoordinator->EnqueueScript(queueAspect->nzbInfo, QueueScriptCoordinator::qeFileDownloaded);
		}

		if (
#ifndef DISABLE_PARCHECK
			!m_parCoordinator.AddPar(queueAspect->fileInfo, queueAspect->action == DownloadQueue::eaFileDeleted) &&
#endif
			IsNZBFileCompleted(queueAspect->nzbInfo, true, false) &&
			!queueAspect->nzbInfo->GetPostInfo() &&
			(!queueAspect->fileInfo->GetPaused() || IsNZBFileCompleted(queueAspect->nzbInfo, false, false)))
		{
			if ((queueAspect->action == DownloadQueue::eaFileCompleted ||
				(queueAspect->fileInfo->GetAutoDeleted() &&
				 IsNZBFileCompleted(queueAspect->nzbInfo, false, true))) &&
				 queueAspect->fileInfo->GetNZBInfo()->GetDeleteStatus() != NZBInfo::dsHealth)
			{
				queueAspect->nzbInfo->PrintMessage(Message::mkInfo,
					"Collection %s completely downloaded", queueAspect->nzbInfo->GetName());
				g_pQueueScriptCoordinator->EnqueueScript(queueAspect->nzbInfo, QueueScriptCoordinator::qeNzbDownloaded);
				NZBDownloaded(queueAspect->downloadQueue, queueAspect->nzbInfo);
			}
			else if ((queueAspect->action == DownloadQueue::eaFileDeleted ||
				(queueAspect->action == DownloadQueue::eaFileCompleted &&
				 queueAspect->fileInfo->GetNZBInfo()->GetDeleteStatus() > NZBInfo::dsNone)) &&
				!queueAspect->nzbInfo->GetParCleanup() &&
				IsNZBFileCompleted(queueAspect->nzbInfo, false, true))
			{
				queueAspect->nzbInfo->PrintMessage(Message::mkInfo,
					"Collection %s deleted from queue", queueAspect->nzbInfo->GetName());
				NZBDeleted(queueAspect->downloadQueue, queueAspect->nzbInfo);
			}
		}
	}
}

void PrePostProcessor::NZBFound(DownloadQueue* downloadQueue, NZBInfo* nzbInfo)
{
	if (g_pOptions->GetDupeCheck() && nzbInfo->GetDupeMode() != dmForce)
	{
		g_pDupeCoordinator->NZBFound(downloadQueue, nzbInfo);
	}
}

void PrePostProcessor::NZBAdded(DownloadQueue* downloadQueue, NZBInfo* nzbInfo)
{
	if (g_pOptions->GetParCheck() != Options::pcForce)
	{
		m_parCoordinator.PausePars(downloadQueue, nzbInfo);
	}

	if (nzbInfo->GetDeleteStatus() == NZBInfo::dsDupe ||
		nzbInfo->GetDeleteStatus() == NZBInfo::dsCopy ||
		nzbInfo->GetDeleteStatus() == NZBInfo::dsGood ||
		nzbInfo->GetDeleteStatus() == NZBInfo::dsScan)
	{
		NZBCompleted(downloadQueue, nzbInfo, false);
	}
	else
	{
		g_pQueueScriptCoordinator->EnqueueScript(nzbInfo, QueueScriptCoordinator::qeNzbAdded);
	}
}

void PrePostProcessor::NZBDownloaded(DownloadQueue* downloadQueue, NZBInfo* nzbInfo)
{
	if (nzbInfo->GetDeleteStatus() == NZBInfo::dsHealth ||
		nzbInfo->GetDeleteStatus() == NZBInfo::dsBad)
	{
		g_pQueueScriptCoordinator->EnqueueScript(nzbInfo, QueueScriptCoordinator::qeNzbDeleted);
	}

	if (!nzbInfo->GetPostInfo() && g_pOptions->GetDecode())
	{
		nzbInfo->PrintMessage(Message::mkInfo, "Queueing %s for post-processing", nzbInfo->GetName());

		nzbInfo->EnterPostProcess();
		m_jobCount++;

		if (nzbInfo->GetParStatus() == NZBInfo::psNone &&
			g_pOptions->GetParCheck() != Options::pcAlways &&
			g_pOptions->GetParCheck() != Options::pcForce)
		{
			nzbInfo->SetParStatus(NZBInfo::psSkipped);
		}

		if (nzbInfo->GetRenameStatus() == NZBInfo::rsNone && !g_pOptions->GetParRename())
		{
			nzbInfo->SetRenameStatus(NZBInfo::rsSkipped);
		}

		downloadQueue->Save();
	}
	else
	{
		NZBCompleted(downloadQueue, nzbInfo, true);
	}
}

void PrePostProcessor::NZBDeleted(DownloadQueue* downloadQueue, NZBInfo* nzbInfo)
{
	if (nzbInfo->GetDeleteStatus() == NZBInfo::dsNone)
	{
		nzbInfo->SetDeleteStatus(NZBInfo::dsManual);
	}
	nzbInfo->SetDeleting(false);

	DeleteCleanup(nzbInfo);

	if (nzbInfo->GetDeleteStatus() == NZBInfo::dsHealth ||
		nzbInfo->GetDeleteStatus() == NZBInfo::dsBad)
	{
		NZBDownloaded(downloadQueue, nzbInfo);
	}
	else
	{
		NZBCompleted(downloadQueue, nzbInfo, true);
	}
}

void PrePostProcessor::NZBCompleted(DownloadQueue* downloadQueue, NZBInfo* nzbInfo, bool saveQueue)
{
	bool addToHistory = g_pOptions->GetKeepHistory() > 0 && !nzbInfo->GetAvoidHistory();
	if (addToHistory)
	{
		g_pHistoryCoordinator->AddToHistory(downloadQueue, nzbInfo);
	}
	nzbInfo->SetAvoidHistory(false);

	bool needSave = addToHistory;

	if (g_pOptions->GetDupeCheck() && nzbInfo->GetDupeMode() != dmForce &&
		(nzbInfo->GetDeleteStatus() == NZBInfo::dsNone ||
		 nzbInfo->GetDeleteStatus() == NZBInfo::dsHealth ||
		 nzbInfo->GetDeleteStatus() == NZBInfo::dsBad ||
		 nzbInfo->GetDeleteStatus() == NZBInfo::dsScan))
	{
		g_pDupeCoordinator->NZBCompleted(downloadQueue, nzbInfo);
		needSave = true;
	}

	if (nzbInfo->GetDeleteStatus() > NZBInfo::dsNone &&
		nzbInfo->GetDeleteStatus() != NZBInfo::dsHealth &&
		nzbInfo->GetDeleteStatus() != NZBInfo::dsBad)
		// nzbs deleted by health check or marked as bad are processed as downloaded with failure status
	{
		g_pQueueScriptCoordinator->EnqueueScript(nzbInfo, QueueScriptCoordinator::qeNzbDeleted);
	}

	if (!addToHistory)
	{
		g_pHistoryCoordinator->DeleteDiskFiles(nzbInfo);
		downloadQueue->GetQueue()->Remove(nzbInfo);
		delete nzbInfo;
	}

	if (saveQueue && needSave)
	{
		downloadQueue->Save();
	}
}

void PrePostProcessor::DeleteCleanup(NZBInfo* nzbInfo)
{
	if ((g_pOptions->GetDeleteCleanupDisk() && nzbInfo->GetCleanupDisk()) ||
		nzbInfo->GetDeleteStatus() == NZBInfo::dsDupe)
	{
		// download was cancelled, deleting already downloaded files from disk
		for (CompletedFiles::reverse_iterator it = nzbInfo->GetCompletedFiles()->rbegin(); it != nzbInfo->GetCompletedFiles()->rend(); it++)
		{
			CompletedFile* completedFile = *it;

			char fullFileName[1024];
			snprintf(fullFileName, 1024, "%s%c%s", nzbInfo->GetDestDir(), (int)PATH_SEPARATOR, completedFile->GetFileName());
			fullFileName[1024-1] = '\0';

			if (Util::FileExists(fullFileName))
			{
				detail("Deleting file %s", completedFile->GetFileName());
				remove(fullFileName);
			}
		}

		// delete .out.tmp-files and _brokenlog.txt
		DirBrowser dir(nzbInfo->GetDestDir());
		while (const char* filename = dir.Next())
		{
			int len = strlen(filename);
			if ((len > 8 && !strcmp(filename + len - 8, ".out.tmp")) || !strcmp(filename, "_brokenlog.txt"))
			{
				char fullFilename[1024];
				snprintf(fullFilename, 1024, "%s%c%s", nzbInfo->GetDestDir(), PATH_SEPARATOR, filename);
				fullFilename[1024-1] = '\0';

				detail("Deleting file %s", filename);
				remove(fullFilename);
			}
		}
	
		// delete old directory (if empty)
		if (Util::DirEmpty(nzbInfo->GetDestDir()))
		{
			rmdir(nzbInfo->GetDestDir());
		}
	}
}

void PrePostProcessor::CheckPostQueue()
{
	DownloadQueue* downloadQueue = DownloadQueue::Lock();

	if (!m_curJob && m_jobCount > 0)
	{
		m_curJob = GetNextJob(downloadQueue);
	}

	if (m_curJob)
	{
		PostInfo* postInfo = m_curJob->GetPostInfo();
		if (!postInfo->GetWorking() && !IsNZBFileDownloading(m_curJob))
		{
#ifndef DISABLE_PARCHECK
			if (postInfo->GetRequestParCheck() &&
				(postInfo->GetNZBInfo()->GetParStatus() <= NZBInfo::psSkipped ||
				 (postInfo->GetForceRepair() && !postInfo->GetNZBInfo()->GetParFull())) &&
				g_pOptions->GetParCheck() != Options::pcManual)
			{
				postInfo->SetForceParFull(postInfo->GetNZBInfo()->GetParStatus() > NZBInfo::psSkipped);
				postInfo->GetNZBInfo()->SetParStatus(NZBInfo::psNone);
				postInfo->SetRequestParCheck(false);
				postInfo->SetStage(PostInfo::ptQueued);
				postInfo->GetNZBInfo()->GetScriptStatuses()->Clear();
				DeletePostThread(postInfo);
			}
			else if (postInfo->GetRequestParCheck() && postInfo->GetNZBInfo()->GetParStatus() <= NZBInfo::psSkipped &&
				g_pOptions->GetParCheck() == Options::pcManual)
			{
				postInfo->SetRequestParCheck(false);
				postInfo->GetNZBInfo()->SetParStatus(NZBInfo::psManual);
				DeletePostThread(postInfo);

				if (!postInfo->GetNZBInfo()->GetFileList()->empty())
				{
					postInfo->GetNZBInfo()->PrintMessage(Message::mkInfo,
						"Downloading all remaining files for manual par-check for %s", postInfo->GetNZBInfo()->GetName());
					downloadQueue->EditEntry(postInfo->GetNZBInfo()->GetID(), DownloadQueue::eaGroupResume, 0, NULL);
					postInfo->SetStage(PostInfo::ptFinished);
				}
				else
				{
					postInfo->GetNZBInfo()->PrintMessage(Message::mkInfo,
						"There are no par-files remain for download for %s", postInfo->GetNZBInfo()->GetName());
					postInfo->SetStage(PostInfo::ptQueued);
				}
			}
			
#endif
			if (postInfo->GetDeleted())
			{
				postInfo->SetStage(PostInfo::ptFinished);
			}

			if (postInfo->GetStage() == PostInfo::ptQueued &&
				(!g_pOptions->GetPausePostProcess() || postInfo->GetNZBInfo()->GetForcePriority()))
			{
				DeletePostThread(postInfo);
				StartJob(downloadQueue, postInfo);
			}
			else if (postInfo->GetStage() == PostInfo::ptFinished)
			{
				UpdatePauseState(false, NULL);
				JobCompleted(downloadQueue, postInfo);
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

NZBInfo* PrePostProcessor::GetNextJob(DownloadQueue* downloadQueue)
{
	NZBInfo* nzbInfo = NULL;

	for (NZBList::iterator it = downloadQueue->GetQueue()->begin(); it != downloadQueue->GetQueue()->end(); it++)
	{
		NZBInfo* nzbInfo1 = *it;
		if (nzbInfo1->GetPostInfo() && !g_pQueueScriptCoordinator->HasJob(nzbInfo1->GetID(), NULL) &&
			(!nzbInfo || nzbInfo1->GetPriority() > nzbInfo->GetPriority()) &&
			(!g_pOptions->GetPausePostProcess() || nzbInfo1->GetForcePriority()))
		{
			nzbInfo = nzbInfo1;
		}
	}

	return nzbInfo;
}

/**
 * Reset the state of items after reloading from disk and
 * delete items which could not be resumed.
 * Also count the number of post-jobs.
 */
void PrePostProcessor::SanitisePostQueue(DownloadQueue* downloadQueue)
{
	for (NZBList::iterator it = downloadQueue->GetQueue()->begin(); it != downloadQueue->GetQueue()->end(); it++)
	{
		NZBInfo* nzbInfo = *it;
		PostInfo* postInfo = nzbInfo->GetPostInfo();
		if (postInfo)
		{
			m_jobCount++;
			if (postInfo->GetStage() == PostInfo::ptExecutingScript ||
				!Util::DirectoryExists(nzbInfo->GetDestDir()))
			{
				postInfo->SetStage(PostInfo::ptFinished);
			}
			else
			{
				postInfo->SetStage(PostInfo::ptQueued);
			}
		}
	}
}

void PrePostProcessor::DeletePostThread(PostInfo* postInfo)
{
	delete postInfo->GetPostThread();
	postInfo->SetPostThread(NULL);
}

void PrePostProcessor::StartJob(DownloadQueue* downloadQueue, PostInfo* postInfo)
{
	if (!postInfo->GetStartTime())
	{
		postInfo->SetStartTime(time(NULL));
	}

#ifndef DISABLE_PARCHECK
	if (postInfo->GetNZBInfo()->GetRenameStatus() == NZBInfo::rsNone &&
		postInfo->GetNZBInfo()->GetDeleteStatus() == NZBInfo::dsNone)
	{
		UpdatePauseState(g_pOptions->GetParPauseQueue(), "par-rename");
		m_parCoordinator.StartParRenameJob(postInfo);
		return;
	}
	else if (postInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psNone &&
		postInfo->GetNZBInfo()->GetDeleteStatus() == NZBInfo::dsNone)
	{
		if (ParParser::FindMainPars(postInfo->GetNZBInfo()->GetDestDir(), NULL))
		{
			UpdatePauseState(g_pOptions->GetParPauseQueue(), "par-check");
			m_parCoordinator.StartParCheckJob(postInfo);
		}
		else
		{
			postInfo->GetNZBInfo()->PrintMessage(Message::mkInfo,
				"Nothing to par-check for %s", postInfo->GetNZBInfo()->GetName());
			postInfo->GetNZBInfo()->SetParStatus(NZBInfo::psSkipped);
			postInfo->SetWorking(false);
			postInfo->SetStage(PostInfo::ptQueued);
		}
		return;
	}
	else if (postInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psSkipped &&
		((g_pOptions->GetParScan() != Options::psDupe &&
		  postInfo->GetNZBInfo()->CalcHealth() < postInfo->GetNZBInfo()->CalcCriticalHealth(false) &&
		  postInfo->GetNZBInfo()->CalcCriticalHealth(false) < 1000) ||
		  postInfo->GetNZBInfo()->CalcHealth() == 0) &&
		ParParser::FindMainPars(postInfo->GetNZBInfo()->GetDestDir(), NULL))
	{
		postInfo->GetNZBInfo()->PrintMessage(Message::mkWarning,
			postInfo->GetNZBInfo()->CalcHealth() == 0 ?
				"Skipping par-check for %s due to health 0%%" :
				"Skipping par-check for %s due to health %.1f%% below critical %.1f%%",
			postInfo->GetNZBInfo()->GetName(),
			postInfo->GetNZBInfo()->CalcHealth() / 10.0, postInfo->GetNZBInfo()->CalcCriticalHealth(false) / 10.0);
		postInfo->GetNZBInfo()->SetParStatus(NZBInfo::psFailure);
		return;
	}
	else if (postInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psSkipped &&
		postInfo->GetNZBInfo()->GetFailedSize() - postInfo->GetNZBInfo()->GetParFailedSize() > 0 &&
		ParParser::FindMainPars(postInfo->GetNZBInfo()->GetDestDir(), NULL))
	{
		postInfo->GetNZBInfo()->PrintMessage(Message::mkInfo,
			"Collection %s with health %.1f%% needs par-check",
			postInfo->GetNZBInfo()->GetName(), postInfo->GetNZBInfo()->CalcHealth() / 10.0);
		postInfo->SetRequestParCheck(true);
		return;
	}
#endif

	NZBParameter* unpackParameter = postInfo->GetNZBInfo()->GetParameters()->Find("*Unpack:", false);
	bool unpackParam = !(unpackParameter && !strcasecmp(unpackParameter->GetValue(), "no"));
	bool unpack = unpackParam && postInfo->GetNZBInfo()->GetUnpackStatus() == NZBInfo::usNone &&
		postInfo->GetNZBInfo()->GetDeleteStatus() == NZBInfo::dsNone;

	bool parFailed = postInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psFailure ||
		postInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psRepairPossible ||
		postInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psManual;

	bool cleanup = !unpack &&
		postInfo->GetNZBInfo()->GetCleanupStatus() == NZBInfo::csNone &&
		!Util::EmptyStr(g_pOptions->GetExtCleanupDisk()) &&
		((postInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psSuccess &&
		  postInfo->GetNZBInfo()->GetUnpackStatus() != NZBInfo::usFailure &&
		  postInfo->GetNZBInfo()->GetUnpackStatus() != NZBInfo::usSpace &&
		  postInfo->GetNZBInfo()->GetUnpackStatus() != NZBInfo::usPassword) ||
		 (postInfo->GetNZBInfo()->GetUnpackStatus() == NZBInfo::usSuccess &&
		  postInfo->GetNZBInfo()->GetParStatus() != NZBInfo::psFailure) ||
		 ((postInfo->GetNZBInfo()->GetUnpackStatus() == NZBInfo::usNone || 
		   postInfo->GetNZBInfo()->GetUnpackStatus() == NZBInfo::usSkipped) &&
		  (postInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psNone ||
		   postInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psSkipped) &&
		  postInfo->GetNZBInfo()->CalcHealth() == 1000));

	bool moveInter = !unpack &&
		postInfo->GetNZBInfo()->GetMoveStatus() == NZBInfo::msNone &&
		postInfo->GetNZBInfo()->GetUnpackStatus() != NZBInfo::usFailure &&
		postInfo->GetNZBInfo()->GetUnpackStatus() != NZBInfo::usSpace &&
		postInfo->GetNZBInfo()->GetUnpackStatus() != NZBInfo::usPassword &&
		postInfo->GetNZBInfo()->GetParStatus() != NZBInfo::psFailure &&
		postInfo->GetNZBInfo()->GetParStatus() != NZBInfo::psManual &&
		postInfo->GetNZBInfo()->GetDeleteStatus() == NZBInfo::dsNone &&
		!Util::EmptyStr(g_pOptions->GetInterDir()) &&
		!strncmp(postInfo->GetNZBInfo()->GetDestDir(), g_pOptions->GetInterDir(), strlen(g_pOptions->GetInterDir()));

	bool postScript = true;

	if (unpack && parFailed)
	{
		postInfo->GetNZBInfo()->PrintMessage(Message::mkWarning,
			"Skipping unpack for %s due to %s", postInfo->GetNZBInfo()->GetName(),
			postInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psManual ? "required par-repair" : "par-failure");
		postInfo->GetNZBInfo()->SetUnpackStatus(NZBInfo::usSkipped);
		unpack = false;
	}

	if (!unpack && !moveInter && !postScript)
	{
		postInfo->SetStage(PostInfo::ptFinished);
		return;
	}

	postInfo->SetProgressLabel(unpack ? "Unpacking" : moveInter ? "Moving" : "Executing post-process-script");
	postInfo->SetWorking(true);
	postInfo->SetStage(unpack ? PostInfo::ptUnpacking : moveInter ? PostInfo::ptMoving : PostInfo::ptExecutingScript);
	postInfo->SetFileProgress(0);
	postInfo->SetStageProgress(0);

	downloadQueue->Save();

	postInfo->SetStageTime(time(NULL));

	if (unpack)
	{
		UpdatePauseState(g_pOptions->GetUnpackPauseQueue(), "unpack");
		UnpackController::StartJob(postInfo);
	}
	else if (cleanup)
	{
		UpdatePauseState(g_pOptions->GetUnpackPauseQueue() || g_pOptions->GetScriptPauseQueue(), "cleanup");
		CleanupController::StartJob(postInfo);
	}
	else if (moveInter)
	{
		UpdatePauseState(g_pOptions->GetUnpackPauseQueue() || g_pOptions->GetScriptPauseQueue(), "move");
		MoveController::StartJob(postInfo);
	}
	else
	{
		UpdatePauseState(g_pOptions->GetScriptPauseQueue(), "post-process-script");
		PostScriptController::StartJob(postInfo);
	}
}

void PrePostProcessor::JobCompleted(DownloadQueue* downloadQueue, PostInfo* postInfo)
{
	NZBInfo* nzbInfo = postInfo->GetNZBInfo();

	if (postInfo->GetStartTime() > 0)
	{
		nzbInfo->SetPostTotalSec((int)(time(NULL) - postInfo->GetStartTime()));
		postInfo->SetStartTime(0);
	}

	DeletePostThread(postInfo);
	nzbInfo->LeavePostProcess();

	if (IsNZBFileCompleted(nzbInfo, true, false))
	{
		// Cleaning up queue if par-check was successful or unpack was successful or
		// health is 100% (if unpack and par-check were not performed)
		// or health is below critical health
		bool canCleanupQueue =
			((nzbInfo->GetParStatus() == NZBInfo::psSuccess ||
			  nzbInfo->GetParStatus() == NZBInfo::psRepairPossible) &&
			 nzbInfo->GetUnpackStatus() != NZBInfo::usFailure &&
			 nzbInfo->GetUnpackStatus() != NZBInfo::usSpace &&
			 nzbInfo->GetUnpackStatus() != NZBInfo::usPassword) ||
			(nzbInfo->GetUnpackStatus() == NZBInfo::usSuccess &&
			 nzbInfo->GetParStatus() != NZBInfo::psFailure) ||
			(nzbInfo->GetUnpackStatus() <= NZBInfo::usSkipped &&
			 nzbInfo->GetParStatus() != NZBInfo::psFailure &&
			 nzbInfo->GetFailedSize() - nzbInfo->GetParFailedSize() == 0) ||
			(nzbInfo->CalcHealth() < nzbInfo->CalcCriticalHealth(false) &&
			 nzbInfo->CalcCriticalHealth(false) < 1000);
		if (g_pOptions->GetParCleanupQueue() && canCleanupQueue && !nzbInfo->GetFileList()->empty())
		{
			nzbInfo->PrintMessage(Message::mkInfo, "Cleaning up download queue for %s", nzbInfo->GetName());
			nzbInfo->SetParCleanup(true);
			downloadQueue->EditEntry(nzbInfo->GetID(), DownloadQueue::eaGroupDelete, 0, NULL);
		}

		if (nzbInfo->GetUnpackCleanedUpDisk())
		{
			nzbInfo->ClearCompletedFiles();
		}

		NZBCompleted(downloadQueue, nzbInfo, false);
	}

	if (nzbInfo == m_curJob)
	{
		m_curJob = NULL;
	}
	m_jobCount--;

	downloadQueue->Save();
}

bool PrePostProcessor::IsNZBFileCompleted(NZBInfo* nzbInfo, bool ignorePausedPars, bool allowOnlyOneDeleted)
{
	int deleted = 0;

	for (FileList::iterator it = nzbInfo->GetFileList()->begin(); it != nzbInfo->GetFileList()->end(); it++)
	{
		FileInfo* fileInfo = *it;
		if (fileInfo->GetDeleted())
		{
			deleted++;
		}
		if (((!fileInfo->GetPaused() || !ignorePausedPars || !fileInfo->GetParFile()) &&
			!fileInfo->GetDeleted()) ||
			(allowOnlyOneDeleted && deleted > 1))
		{
			return false;
		}
	}

	return true;
}

bool PrePostProcessor::IsNZBFileDownloading(NZBInfo* nzbInfo)
{
	if (nzbInfo->GetActiveDownloads())
	{
		return true;
	}

	for (FileList::iterator it = nzbInfo->GetFileList()->begin(); it != nzbInfo->GetFileList()->end(); it++)
	{
		FileInfo* fileInfo = *it;
		if (!fileInfo->GetPaused())
		{
			return true;
		}
	}

	return false;
}

void PrePostProcessor::UpdatePauseState(bool needPause, const char* reason)
{
	if (needPause && !g_pOptions->GetTempPauseDownload())
	{
		info("Pausing download before %s", reason);
	}
	else if (!needPause && g_pOptions->GetTempPauseDownload())
	{
		info("Unpausing download after %s", m_pauseReason);
	}
	g_pOptions->SetTempPauseDownload(needPause);
	m_pauseReason = reason;
}

bool PrePostProcessor::EditList(DownloadQueue* downloadQueue, IDList* idList, DownloadQueue::EEditAction action, int offset, const char* text)
{
	debug("Edit-command for post-processor received");
	switch (action)
	{
		case DownloadQueue::eaPostDelete:
			return PostQueueDelete(downloadQueue, idList);

		default:
			return false;
	}
}

bool PrePostProcessor::PostQueueDelete(DownloadQueue* downloadQueue, IDList* idList)
{
	bool ok = false;

	for (IDList::iterator itID = idList->begin(); itID != idList->end(); itID++)
	{
		int id = *itID;

		for (NZBList::iterator it = downloadQueue->GetQueue()->begin(); it != downloadQueue->GetQueue()->end(); it++)
		{
			NZBInfo* nzbInfo = *it;
			PostInfo* postInfo = nzbInfo->GetPostInfo();
			if (postInfo && nzbInfo->GetID() == id)
			{
				if (postInfo->GetWorking())
				{
					postInfo->GetNZBInfo()->PrintMessage(Message::mkInfo,
						"Deleting active post-job %s", postInfo->GetNZBInfo()->GetName());
					postInfo->SetDeleted(true);
#ifndef DISABLE_PARCHECK
					if (PostInfo::ptLoadingPars <= postInfo->GetStage() && postInfo->GetStage() <= PostInfo::ptRenaming)
					{
						if (m_parCoordinator.Cancel())
						{
							ok = true;
						}
					}
					else
#endif
					if (postInfo->GetPostThread())
					{
						debug("Terminating %s for %s", (postInfo->GetStage() == PostInfo::ptUnpacking ? "unpack" : "post-process-script"), postInfo->GetNZBInfo()->GetName());
						postInfo->GetPostThread()->Stop();
						ok = true;
					}
					else
					{
						error("Internal error in PrePostProcessor::QueueDelete");
					}
				}
				else
				{
					postInfo->GetNZBInfo()->PrintMessage(Message::mkInfo,
						"Deleting queued post-job %s", postInfo->GetNZBInfo()->GetName());
					JobCompleted(downloadQueue, postInfo);
					ok = true;
				}
				break;
			}
		}
	}

	return ok;
}
