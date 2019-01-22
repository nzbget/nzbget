/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2007-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "nzbget.h"
#include "PrePostProcessor.h"
#include "Options.h"
#include "WorkState.h"
#include "Log.h"
#include "HistoryCoordinator.h"
#include "DupeCoordinator.h"
#include "PostScript.h"
#include "Util.h"
#include "FileSystem.h"
#include "Unpack.h"
#include "Cleanup.h"
#include "Rename.h"
#include "Repair.h"
#include "NzbFile.h"
#include "QueueScript.h"
#include "ParParser.h"
#include "DirectUnpack.h"

PrePostProcessor::PrePostProcessor()
{
	debug("Creating PrePostProcessor");

	DownloadQueue::Guard()->Attach(this);
}

void PrePostProcessor::Run()
{
	debug("Entering PrePostProcessor-loop");

	while (!DownloadQueue::IsLoaded())
	{
		Util::Sleep(20);
	}

	if (g_Options->GetServerMode())
	{
		SanitisePostQueue();
	}

	while (!IsStopped())
	{
		if (g_WorkState->GetTempPausePostprocess())
		{
			// Postprocess is paused: just wait and loop
			Util::Sleep(200);
			continue;
		}

		if (m_queuedJobs)
		{
			// check post-queue every 200 msec
			CheckPostQueue();
			Util::Sleep(200);
		}
		else
		{
			// Wait until we get the stop signal or more jobs in the queue
			Guard guard(m_waitMutex);
			m_waitCond.Wait(m_waitMutex, [&]{ return m_queuedJobs || IsStopped(); });
		}
	}

	WaitJobs();

	debug("Exiting PrePostProcessor-loop");
}

void PrePostProcessor::WaitJobs()
{
	debug("PrePostProcessor: waiting for jobs to complete");

	// wait 5 seconds until all post-processing jobs gracefully finish
	time_t waitStart = Util::CurrentTime();
	while (Util::CurrentTime() < waitStart + 5)
	{
		{
			GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
			if (m_activeJobs.empty())
			{
				break;
			}
		}
		CheckPostQueue();
		Util::Sleep(200);
	}

	// kill remaining post-processing jobs; not safe but we can't wait any longer
	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		for (NzbInfo* postJob : m_activeJobs)
		{
			if (postJob->GetPostInfo() && postJob->GetPostInfo()->GetPostThread())
			{
				Thread* thread = postJob->GetPostInfo()->GetPostThread();
				postJob->GetPostInfo()->SetPostThread(nullptr);
				warn("Terminating active post-process job for %s", postJob->GetName());
				thread->Kill();
				delete thread;
			}
		}
	}

	// wait 5 seconds until direct unpack threads gracefully finish
	waitStart = Util::CurrentTime();
	while (Util::CurrentTime() < waitStart + 5)
	{
		{
			GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
			if (std::find_if(downloadQueue->GetQueue()->begin(),
				downloadQueue->GetQueue()->end(),
				[](const std::unique_ptr<NzbInfo>& nzbInfo)
				{
					return nzbInfo->GetUnpackThread() != nullptr;
				}) == downloadQueue->GetQueue()->end())
			{
				break;
			}
		}
		Util::Sleep(200);
	}

	// disconnect remaining direct unpack jobs
	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
		{
			nzbInfo->SetUnpackThread(nullptr);
		}
	}

	debug("PrePostProcessor: Jobs are completed");
}

void PrePostProcessor::Stop()
{
	Thread::Stop();

	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

		for (NzbInfo* postJob : m_activeJobs)
		{
			if (postJob->GetPostInfo() && postJob->GetPostInfo()->GetPostThread())
			{
				postJob->GetPostInfo()->GetPostThread()->Stop();
			}
		}

		for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
		{
			if (nzbInfo->GetUnpackThread())
			{
				((DirectUnpack*)nzbInfo->GetUnpackThread())->Stop(downloadQueue, nzbInfo);
			}
		}
	}

	// Resume Run() to exit it
	Guard guard(m_waitMutex);
	m_waitCond.NotifyAll();
}

/**
 * Reset the state of items after reloading from disk and
 * delete items which could not be resumed.
 * Also count the number of post-jobs.
 */
void PrePostProcessor::SanitisePostQueue()
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
	for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
	{
		PostInfo* postInfo = nzbInfo->GetPostInfo();
		if (postInfo)
		{
			m_queuedJobs++;
			if (postInfo->GetStage() == PostInfo::ptExecutingScript ||
				!FileSystem::DirectoryExists(nzbInfo->GetDestDir()))
			{
				postInfo->SetStage(PostInfo::ptFinished);
			}
			else
			{
				postInfo->SetStage(PostInfo::ptQueued);
			}
			postInfo->SetWorking(false);
		}
	}
}

void PrePostProcessor::DownloadQueueUpdate(void* aspect)
{
	if (IsStopped())
	{
		return;
	}

	DownloadQueue::Aspect* queueAspect = (DownloadQueue::Aspect*)aspect;
	if (queueAspect->action == DownloadQueue::eaNzbFound ||
		queueAspect->action == DownloadQueue::eaUrlFound)
	{
		NzbFound(queueAspect->downloadQueue, queueAspect->nzbInfo);
	}
	else if (queueAspect->action == DownloadQueue::eaNzbAdded ||
		queueAspect->action == DownloadQueue::eaUrlAdded)
	{
		NzbAdded(queueAspect->downloadQueue, queueAspect->nzbInfo);
	}
	else if (queueAspect->action == DownloadQueue::eaNzbNamed)
	{
		g_QueueScriptCoordinator->EnqueueScript(queueAspect->nzbInfo, QueueScriptCoordinator::qeNzbNamed);
	}
	else if (queueAspect->action == DownloadQueue::eaNzbDeleted &&
		queueAspect->nzbInfo->GetDeleting() &&
		!queueAspect->nzbInfo->GetPostInfo() &&
		queueAspect->nzbInfo->GetFileList()->empty())
	{
		// the deleting of nzbs is usually handled via eaFileDeleted-event, but when deleting nzb without
		// any files left the eaFileDeleted-event is not fired and we need to process eaNzbDeleted-event instead
		queueAspect->nzbInfo->PrintMessage(Message::mkInfo,
			"Collection %s deleted from queue", queueAspect->nzbInfo->GetName());
		NzbDeleted(queueAspect->downloadQueue, queueAspect->nzbInfo);
	}
	else if (queueAspect->action == DownloadQueue::eaUrlDeleted)
	{
		NzbDeleted(queueAspect->downloadQueue, queueAspect->nzbInfo);
	}
	else if (queueAspect->action == DownloadQueue::eaUrlFailed)
	{
		NzbCompleted(queueAspect->downloadQueue, queueAspect->nzbInfo, true);
	}
	else if ((queueAspect->action == DownloadQueue::eaFileCompleted ||
		queueAspect->action == DownloadQueue::eaFileDeleted))
	{
		if (queueAspect->action == DownloadQueue::eaFileCompleted)
		{
			FileDownloaded(queueAspect->downloadQueue, queueAspect->nzbInfo, queueAspect->fileInfo);
		}

#ifndef DISABLE_PARCHECK
		for (NzbInfo* postJob : m_activeJobs)
		{
			if (postJob && queueAspect->fileInfo->GetNzbInfo() == postJob &&
				postJob->GetPostInfo() && postJob->GetPostInfo()->GetPostThread() &&
				postJob->GetPostInfo()->GetStage() >= PostInfo::ptLoadingPars &&
				postJob->GetPostInfo()->GetStage() <= PostInfo::ptVerifyingRepaired &&
				((RepairController*)postJob->GetPostInfo()->GetPostThread())->AddPar(
					queueAspect->fileInfo, queueAspect->action == DownloadQueue::eaFileDeleted))
			{
				return;
			}
		}
#endif

		if ((queueAspect->action == DownloadQueue::eaFileCompleted ||
			 queueAspect->fileInfo->GetDupeDeleted()) &&
			queueAspect->fileInfo->GetNzbInfo()->GetDeleteStatus() != NzbInfo::dsHealth &&
			!queueAspect->nzbInfo->GetPostInfo() &&
			queueAspect->nzbInfo->IsDownloadCompleted(true))
		{
			queueAspect->nzbInfo->PrintMessage(Message::mkInfo,
				"Collection %s completely downloaded", queueAspect->nzbInfo->GetName());
			g_QueueScriptCoordinator->EnqueueScript(queueAspect->nzbInfo, QueueScriptCoordinator::qeNzbDownloaded);
			NzbDownloaded(queueAspect->downloadQueue, queueAspect->nzbInfo);
		}
		else if ((queueAspect->action == DownloadQueue::eaFileDeleted ||
			(queueAspect->action == DownloadQueue::eaFileCompleted &&
			 queueAspect->fileInfo->GetNzbInfo()->GetDeleteStatus() > NzbInfo::dsNone)) &&
			!queueAspect->nzbInfo->GetPostInfo() &&
			queueAspect->nzbInfo->IsDownloadCompleted(false))
		{
			queueAspect->nzbInfo->PrintMessage(Message::mkInfo,
				"Collection %s deleted from queue", queueAspect->nzbInfo->GetName());
			NzbDeleted(queueAspect->downloadQueue, queueAspect->nzbInfo);
		}
	}
}

void PrePostProcessor::NzbFound(DownloadQueue* downloadQueue, NzbInfo* nzbInfo)
{
	if (g_Options->GetDupeCheck() && nzbInfo->GetDupeMode() != dmForce)
	{
		g_DupeCoordinator->NzbFound(downloadQueue, nzbInfo);
	}
}

void PrePostProcessor::NzbAdded(DownloadQueue* downloadQueue, NzbInfo* nzbInfo)
{
	if (g_Options->GetParCheck() != Options::pcForce)
	{
		downloadQueue->EditEntry(nzbInfo->GetId(), DownloadQueue::eaGroupPauseExtraPars, nullptr);
	}

	if (g_Options->GetReorderFiles() && nzbInfo->GetDeleteStatus() == NzbInfo::dsNone)
	{
		nzbInfo->PrintMessage(Message::mkInfo, "Reordering files for %s", nzbInfo->GetName());
		downloadQueue->EditEntry(nzbInfo->GetId(), DownloadQueue::eaGroupSortFiles, nullptr);
	}

	if (nzbInfo->GetDeleteStatus() == NzbInfo::dsDupe ||
		nzbInfo->GetDeleteStatus() == NzbInfo::dsCopy ||
		nzbInfo->GetDeleteStatus() == NzbInfo::dsGood ||
		nzbInfo->GetDeleteStatus() == NzbInfo::dsScan)
	{
		NzbCompleted(downloadQueue, nzbInfo, false);
	}
	else
	{
		g_QueueScriptCoordinator->EnqueueScript(nzbInfo, QueueScriptCoordinator::qeNzbAdded);
	}
}

void PrePostProcessor::NzbDownloaded(DownloadQueue* downloadQueue, NzbInfo* nzbInfo)
{
	if (nzbInfo->GetDeleteStatus() == NzbInfo::dsHealth ||
		nzbInfo->GetDeleteStatus() == NzbInfo::dsBad)
	{
		g_QueueScriptCoordinator->EnqueueScript(nzbInfo, QueueScriptCoordinator::qeNzbDeleted);
	}

	if (!nzbInfo->GetPostInfo() && !g_Options->GetRawArticle() && !g_Options->GetSkipWrite())
	{
		nzbInfo->PrintMessage(Message::mkInfo, "Queueing %s for post-processing", nzbInfo->GetName());

		nzbInfo->EnterPostProcess();

		if (nzbInfo->GetParStatus() == NzbInfo::psNone &&
			g_Options->GetParCheck() != Options::pcAlways &&
			g_Options->GetParCheck() != Options::pcForce)
		{
			nzbInfo->SetParStatus(NzbInfo::psSkipped);
		}

		if (nzbInfo->GetUnpackThread())
		{
			nzbInfo->GetPostInfo()->SetWorking(true);
			m_activeJobs.push_back(nzbInfo);
			((DirectUnpack*)nzbInfo->GetUnpackThread())->NzbDownloaded(downloadQueue, nzbInfo);
		}

		nzbInfo->SetChanged(true);
		downloadQueue->SaveChanged();

		// We have more jobs in the queue, notify Run()
		Guard guard(m_waitMutex);
		m_queuedJobs++;
		m_waitCond.NotifyAll();
	}
	else
	{
		if (nzbInfo->GetUnpackThread())
		{
			((DirectUnpack*)nzbInfo->GetUnpackThread())->NzbDownloaded(downloadQueue, nzbInfo);
		}

		NzbCompleted(downloadQueue, nzbInfo, true);
	}
}

void PrePostProcessor::NzbDeleted(DownloadQueue* downloadQueue, NzbInfo* nzbInfo)
{
	if (nzbInfo->GetUnpackThread())
	{
		((DirectUnpack*)nzbInfo->GetUnpackThread())->NzbDeleted(downloadQueue, nzbInfo);
	}

	if (nzbInfo->GetDeleteStatus() == NzbInfo::dsNone)
	{
		nzbInfo->SetDeleteStatus(NzbInfo::dsManual);
	}
	nzbInfo->SetDeleting(false);

	DeleteCleanup(nzbInfo);

	if (nzbInfo->GetDeleteStatus() == NzbInfo::dsHealth ||
		nzbInfo->GetDeleteStatus() == NzbInfo::dsBad)
	{
		NzbDownloaded(downloadQueue, nzbInfo);
	}
	else
	{
		NzbCompleted(downloadQueue, nzbInfo, true);
	}
}

void PrePostProcessor::NzbCompleted(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, bool saveQueue)
{
	bool downloadDupe = nzbInfo->GetDupeHint() == NzbInfo::dhRedownloadAuto;
	bool addToHistory = g_Options->GetKeepHistory() > 0 && !nzbInfo->GetAvoidHistory();
	if (addToHistory)
	{
		g_HistoryCoordinator->AddToHistory(downloadQueue, nzbInfo);
	}
	nzbInfo->SetAvoidHistory(false);

	bool needSave = addToHistory;

	if (g_Options->GetDupeCheck() && nzbInfo->GetDupeMode() != dmForce &&
		(nzbInfo->GetDeleteStatus() == NzbInfo::dsNone ||
		 nzbInfo->GetDeleteStatus() == NzbInfo::dsHealth ||
		 nzbInfo->GetDeleteStatus() == NzbInfo::dsBad ||
		 nzbInfo->GetDeleteStatus() == NzbInfo::dsScan ||
		 (nzbInfo->GetDeleteStatus() == NzbInfo::dsCopy && downloadDupe)))
	{
		g_DupeCoordinator->NzbCompleted(downloadQueue, nzbInfo);
		needSave = true;
	}

	if (nzbInfo->GetDeleteStatus() > NzbInfo::dsNone &&
		nzbInfo->GetDeleteStatus() != NzbInfo::dsHealth &&
		nzbInfo->GetDeleteStatus() != NzbInfo::dsBad)
		// nzbs deleted by health check or marked as bad are processed as downloaded with failure status
	{
		g_QueueScriptCoordinator->EnqueueScript(nzbInfo, QueueScriptCoordinator::qeNzbDeleted);
	}

	if (!addToHistory)
	{
		g_HistoryCoordinator->DeleteDiskFiles(nzbInfo);
		downloadQueue->GetQueue()->Remove(nzbInfo);
	}

	if (saveQueue && needSave)
	{
		downloadQueue->Save();
	}
}

void PrePostProcessor::DeleteCleanup(NzbInfo* nzbInfo)
{
	if (nzbInfo->GetCleanupDisk() ||
		nzbInfo->GetDeleteStatus() == NzbInfo::dsDupe)
	{
		// download was cancelled, deleting already downloaded files from disk
		for (CompletedFile& completedFile: nzbInfo->GetCompletedFiles())
		{
			BString<1024> fullFileName("%s%c%s", nzbInfo->GetDestDir(), PATH_SEPARATOR, completedFile.GetFilename());
			if (FileSystem::FileExists(fullFileName))
			{
				detail("Deleting file %s", completedFile.GetFilename());
				FileSystem::DeleteFile(fullFileName);
			}
		}

		// delete .out.tmp-files
		DirBrowser dir(nzbInfo->GetDestDir());
		while (const char* filename = dir.Next())
		{
			int len = strlen(filename);
			if (len > 8 && !strcmp(filename + len - 8, ".out.tmp"))
			{
				BString<1024> fullFilename("%s%c%s", nzbInfo->GetDestDir(), PATH_SEPARATOR, filename);
				detail("Deleting file %s", filename);
				FileSystem::DeleteFile(fullFilename);
			}
		}

		// delete old directory (if empty)
		FileSystem::DeleteDirectory(nzbInfo->GetDestDir());
	}
}

void PrePostProcessor::CheckRequestPar(DownloadQueue* downloadQueue)
{
#ifndef DISABLE_PARCHECK
	for (NzbInfo* postJob : m_activeJobs)
	{
		PostInfo* postInfo = postJob->GetPostInfo();

		if (postInfo->GetRequestParCheck() &&
			(postInfo->GetNzbInfo()->GetParStatus() <= NzbInfo::psSkipped ||
			(postInfo->GetForceRepair() && !postInfo->GetNzbInfo()->GetParFull())) &&
			g_Options->GetParCheck() != Options::pcManual)
		{
			postInfo->SetForceParFull(postInfo->GetNzbInfo()->GetParStatus() > NzbInfo::psSkipped);
			postInfo->GetNzbInfo()->SetParStatus(NzbInfo::psNone);
			postInfo->SetRequestParCheck(false);
			postInfo->GetNzbInfo()->GetScriptStatuses()->clear();
			postInfo->SetWorking(false);
		}
		else if (postInfo->GetRequestParCheck() &&
			postInfo->GetNzbInfo()->GetParStatus() <= NzbInfo::psSkipped &&
			g_Options->GetParCheck() == Options::pcManual)
		{
			postInfo->SetRequestParCheck(false);
			postInfo->GetNzbInfo()->SetParStatus(NzbInfo::psManual);

			if (!postInfo->GetNzbInfo()->GetFileList()->empty())
			{
				postInfo->GetNzbInfo()->PrintMessage(Message::mkInfo,
					"Downloading all remaining files for manual par-check for %s", postInfo->GetNzbInfo()->GetName());
				downloadQueue->EditEntry(postInfo->GetNzbInfo()->GetId(), DownloadQueue::eaGroupResume, nullptr);
				postInfo->SetStage(PostInfo::ptFinished);
			}
			else
			{
				postInfo->GetNzbInfo()->PrintMessage(Message::mkInfo,
					"There are no par-files remain for download for %s", postInfo->GetNzbInfo()->GetName());
			}
			postInfo->SetWorking(false);
		}
	}
#endif
}

void PrePostProcessor::CleanupJobs(DownloadQueue* downloadQueue)
{
	m_activeJobs.erase(std::remove_if(m_activeJobs.begin(), m_activeJobs.end(),
		[processor = this, downloadQueue](NzbInfo* postJob)
		{
			PostInfo* postInfo = postJob->GetPostInfo();
			if (!postInfo->GetWorking() &&
				!(postInfo->GetPostThread() && postInfo->GetPostThread()->IsRunning()))
			{
				delete postInfo->GetPostThread();
				postInfo->SetPostThread(nullptr);

				postInfo->SetStageTime(0);
				postInfo->SetStageProgress(0);
				postInfo->SetFileProgress(0);
				postInfo->SetProgressLabel("");

				if (postInfo->GetStartTime() > 0)
				{
					postJob->SetPostTotalSec(postJob->GetPostTotalSec() +
						(int)(Util::CurrentTime() - postInfo->GetStartTime()));
					postInfo->SetStartTime(0);
				}

				if (postInfo->GetStage() == PostInfo::ptFinished || postInfo->GetDeleted())
				{
					processor->JobCompleted(downloadQueue, postInfo);
				}
				else
				{
					postInfo->SetStage(PostInfo::ptQueued);
				}
				return true;
			}
			return false;
		}),
		m_activeJobs.end());
}

bool PrePostProcessor::CanRunMoreJobs(bool* allowPar)
{
	int totalJobs = (int)m_activeJobs.size();
	int parJobs = 0;
	int otherJobs = 0;
	bool repairJobs = false;

	for (NzbInfo* postJob : m_activeJobs)
	{
		bool parJob = postJob->GetPostInfo()->GetStage() >= PostInfo::ptLoadingPars &&
			postJob->GetPostInfo()->GetStage() <= PostInfo::ptVerifyingRepaired;
		repairJobs |= postJob->GetPostInfo()->GetStage() == PostInfo::ptRepairing;
		parJobs += parJob ? 1 : 0;
		otherJobs += parJob ? 0 : 1;
	}

	switch (g_Options->GetPostStrategy())
	{
		case Options::ppSequential:
			*allowPar = true;
			return totalJobs == 0;

		case Options::ppBalanced:
			*allowPar = parJobs == 0;
			return otherJobs == 0 && (parJobs == 0 || repairJobs);

		case Options::ppAggressive:
			*allowPar = parJobs < 1;
			return totalJobs < 3;

		case Options::ppRocket:
			*allowPar = parJobs < 2;
			return totalJobs < 6;
	}

	return false;
}

NzbInfo* PrePostProcessor::PickNextJob(DownloadQueue* downloadQueue, bool allowPar)
{
	NzbInfo* nzbInfo = nullptr;

	for (NzbInfo* nzbInfo1: downloadQueue->GetQueue())
	{
		if (nzbInfo1->GetPostInfo() && !nzbInfo1->GetPostInfo()->GetWorking() &&
			!g_QueueScriptCoordinator->HasJob(nzbInfo1->GetId(), nullptr) &&
			nzbInfo1->GetDirectUnpackStatus() != NzbInfo::nsRunning &&
			(!nzbInfo || nzbInfo1->GetPriority() > nzbInfo->GetPriority()) &&
			(!g_WorkState->GetPausePostProcess() || nzbInfo1->GetForcePriority()) &&
			(allowPar || !nzbInfo1->GetPostInfo()->GetNeedParCheck()) &&
			(std::find(m_activeJobs.begin(), m_activeJobs.end(), nzbInfo1) == m_activeJobs.end()) &&
			nzbInfo1->IsDownloadCompleted(true))
		{
			nzbInfo = nzbInfo1;
		}
	}

	return nzbInfo;
}

void PrePostProcessor::CheckPostQueue()
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	size_t countBefore = m_activeJobs.size();
	CheckRequestPar(downloadQueue);
	CleanupJobs(downloadQueue);
	bool changed = m_activeJobs.size() != countBefore;

	bool allowPar;
	while (CanRunMoreJobs(&allowPar) && !IsStopped())
	{
		NzbInfo* postJob = PickNextJob(downloadQueue, allowPar);
		if (!postJob)
		{
			break;
		}

		m_activeJobs.push_back(postJob);

		PostInfo* postInfo = postJob->GetPostInfo();
		if (postInfo->GetStage() == PostInfo::ptQueued &&
			(!g_WorkState->GetPausePostProcess() || postInfo->GetNzbInfo()->GetForcePriority()))
		{
			StartJob(downloadQueue, postInfo, allowPar);
			CheckRequestPar(downloadQueue);
			CleanupJobs(downloadQueue);
			changed = true;
		}
	}

	if (changed)
	{
		downloadQueue->Save();
		UpdatePauseState();
	}

	Util::SetStandByMode(m_activeJobs.empty());
}

void PrePostProcessor::StartJob(DownloadQueue* downloadQueue, PostInfo* postInfo, bool allowPar)
{
	NzbInfo* nzbInfo = postInfo->GetNzbInfo();

	if (!postInfo->GetStartTime())
	{
		postInfo->SetStartTime(Util::CurrentTime());
	}
	postInfo->SetStageTime(Util::CurrentTime());
	postInfo->SetStageProgress(0);
	postInfo->SetFileProgress(0);
	postInfo->SetProgressLabel("");

	if (nzbInfo->GetParRenameStatus() == NzbInfo::rsNone &&
		nzbInfo->GetDeleteStatus() == NzbInfo::dsNone &&
		g_Options->GetParRename())
	{
		EnterStage(downloadQueue, postInfo, PostInfo::ptParRenaming);
		RenameController::StartJob(postInfo, RenameController::jkPar);
		return;
	}

#ifndef DISABLE_PARCHECK
	if (nzbInfo->GetParStatus() == NzbInfo::psNone &&
		nzbInfo->GetDeleteStatus() == NzbInfo::dsNone)
	{
		if (ParParser::FindMainPars(nzbInfo->GetDestDir(), nullptr))
		{
			if (!allowPar)
			{
				postInfo->SetNeedParCheck(true);
				return;
			}

			EnterStage(downloadQueue, postInfo, PostInfo::ptLoadingPars);
			postInfo->SetNeedParCheck(false);
			RepairController::StartJob(postInfo);
		}
		else
		{
			nzbInfo->PrintMessage(Message::mkInfo,
				"Nothing to par-check for %s", nzbInfo->GetName());
			nzbInfo->SetParStatus(NzbInfo::psSkipped);
		}
		return;
	}
	
	if (nzbInfo->GetParStatus() == NzbInfo::psSkipped &&
		((g_Options->GetParScan() != Options::psDupe &&
		  nzbInfo->CalcHealth() < nzbInfo->CalcCriticalHealth(false) &&
		  nzbInfo->CalcCriticalHealth(false) < 1000) ||
		  nzbInfo->CalcHealth() == 0) &&
		ParParser::FindMainPars(nzbInfo->GetDestDir(), nullptr))
	{
		if (nzbInfo->CalcHealth() == 0)
		{
			nzbInfo->PrintMessage(Message::mkWarning,
				"Skipping par-check for %s due to health 0%%", nzbInfo->GetName());
		}
		else
		{
			nzbInfo->PrintMessage(Message::mkWarning,
				"Skipping par-check for %s due to health %.1f%% below critical %.1f%%",
				nzbInfo->GetName(),
				nzbInfo->CalcHealth() / 10.0, nzbInfo->CalcCriticalHealth(false) / 10.0);
		}
		nzbInfo->SetParStatus(NzbInfo::psFailure);
		return;
	}
	
	if (nzbInfo->GetParStatus() == NzbInfo::psSkipped &&
		nzbInfo->GetFailedSize() - nzbInfo->GetParFailedSize() > 0 &&
		ParParser::FindMainPars(nzbInfo->GetDestDir(), nullptr))
	{
		nzbInfo->PrintMessage(Message::mkInfo,
			"Collection %s with health %.1f%% needs par-check",
			nzbInfo->GetName(), nzbInfo->CalcHealth() / 10.0);
		postInfo->SetRequestParCheck(true);
		return;
	}
#endif

	NzbParameter* unpackParameter = nzbInfo->GetParameters()->Find("*Unpack:");
	bool wantUnpack = !(unpackParameter && !strcasecmp(unpackParameter->GetValue(), "no"));
	bool unpack = wantUnpack && nzbInfo->GetUnpackStatus() == NzbInfo::usNone &&
		nzbInfo->GetDeleteStatus() == NzbInfo::dsNone;

	if (nzbInfo->GetRarRenameStatus() == NzbInfo::rsNone &&
		unpack && g_Options->GetRarRename())
	{
		EnterStage(downloadQueue, postInfo, PostInfo::ptRarRenaming);
		RenameController::StartJob(postInfo, RenameController::jkRar);
		return;
	}

#ifndef DISABLE_PARCHECK
	if (nzbInfo->GetParStatus() == NzbInfo::psSkipped &&
		nzbInfo->GetDeleteStatus() == NzbInfo::dsNone &&
		g_Options->GetParCheck() == Options::pcAuto &&
		!UnpackController::HasCompletedArchiveFiles(nzbInfo) &&
		ParParser::FindMainPars(nzbInfo->GetDestDir(), nullptr))
	{
		nzbInfo->PrintMessage(Message::mkInfo,
			"Requesting par-check for collection %s without archive files",
			nzbInfo->GetName());
		postInfo->SetRequestParCheck(true);
		return;
	}
#endif

	bool parFailed = nzbInfo->GetParStatus() == NzbInfo::psFailure ||
		nzbInfo->GetParStatus() == NzbInfo::psRepairPossible ||
		nzbInfo->GetParStatus() == NzbInfo::psManual;

	bool cleanup = !unpack && wantUnpack &&
		nzbInfo->GetCleanupStatus() == NzbInfo::csNone &&
		!Util::EmptyStr(g_Options->GetExtCleanupDisk()) &&
		((nzbInfo->GetParStatus() == NzbInfo::psSuccess &&
		  nzbInfo->GetUnpackStatus() != NzbInfo::usFailure &&
		  nzbInfo->GetUnpackStatus() != NzbInfo::usSpace &&
		  nzbInfo->GetUnpackStatus() != NzbInfo::usPassword) ||
		 (nzbInfo->GetUnpackStatus() == NzbInfo::usSuccess &&
		  nzbInfo->GetParStatus() != NzbInfo::psFailure) ||
		 ((nzbInfo->GetUnpackStatus() == NzbInfo::usNone ||
		   nzbInfo->GetUnpackStatus() == NzbInfo::usSkipped) &&
		  (nzbInfo->GetParStatus() == NzbInfo::psNone ||
		   nzbInfo->GetParStatus() == NzbInfo::psSkipped) &&
		  nzbInfo->CalcHealth() == 1000));

	bool moveInter = !unpack &&
		nzbInfo->GetMoveStatus() == NzbInfo::msNone &&
		nzbInfo->GetUnpackStatus() != NzbInfo::usFailure &&
		nzbInfo->GetUnpackStatus() != NzbInfo::usSpace &&
		nzbInfo->GetUnpackStatus() != NzbInfo::usPassword &&
		nzbInfo->GetParStatus() != NzbInfo::psFailure &&
		nzbInfo->GetParStatus() != NzbInfo::psManual &&
		nzbInfo->GetDeleteStatus() == NzbInfo::dsNone &&
		!(((nzbInfo->GetUnpackStatus() == NzbInfo::usNone ||
		   nzbInfo->GetUnpackStatus() == NzbInfo::usSkipped) &&
		  (nzbInfo->GetParStatus() == NzbInfo::psNone ||
		   nzbInfo->GetParStatus() == NzbInfo::psSkipped) &&
		  nzbInfo->CalcHealth() < 1000)) &&
		!Util::EmptyStr(g_Options->GetInterDir()) &&
		!strncmp(nzbInfo->GetDestDir(), g_Options->GetInterDir(), strlen(g_Options->GetInterDir())) &&
		nzbInfo->GetDestDir()[strlen(g_Options->GetInterDir())] == PATH_SEPARATOR;

	if (unpack && parFailed)
	{
		nzbInfo->PrintMessage(Message::mkWarning,
			"Skipping unpack for %s due to %s", nzbInfo->GetName(),
			nzbInfo->GetParStatus() == NzbInfo::psManual ? "required par-repair" : "par-failure");
		nzbInfo->SetUnpackStatus(NzbInfo::usSkipped);
		unpack = false;
	}

	if (unpack)
	{
		EnterStage(downloadQueue, postInfo, PostInfo::ptUnpacking);
		UnpackController::StartJob(postInfo);
	}
	else if (cleanup)
	{
		EnterStage(downloadQueue, postInfo, PostInfo::ptCleaningUp);
		CleanupController::StartJob(postInfo);
	}
	else if (moveInter)
	{
		EnterStage(downloadQueue, postInfo, PostInfo::ptMoving);
		MoveController::StartJob(postInfo);
	}
	else
	{
		EnterStage(downloadQueue, postInfo, PostInfo::ptExecutingScript);
		PostScriptController::StartJob(postInfo);
	}
}

void PrePostProcessor::EnterStage(DownloadQueue* downloadQueue, PostInfo* postInfo, PostInfo::EStage stage)
{
	postInfo->SetWorking(true);
	postInfo->SetStage(stage);
}

void PrePostProcessor::JobCompleted(DownloadQueue* downloadQueue, PostInfo* postInfo)
{
	NzbInfo* nzbInfo = postInfo->GetNzbInfo();

	nzbInfo->LeavePostProcess();

	if (nzbInfo->IsDownloadCompleted(true))
	{
		NzbCompleted(downloadQueue, nzbInfo, false);
	}

	Guard guard(m_waitMutex);
	m_queuedJobs--;
}

void PrePostProcessor::UpdatePauseState()
{
	bool needPause = false;
	for (NzbInfo* postJob : m_activeJobs)
	{
		switch (postJob->GetPostInfo()->GetStage())
		{
			case PostInfo::ptLoadingPars:
			case PostInfo::ptVerifyingSources:
			case PostInfo::ptRepairing:
			case PostInfo::ptVerifyingRepaired:
			case PostInfo::ptParRenaming:
				needPause |= g_Options->GetParPauseQueue();
				break;

			case PostInfo::ptRarRenaming:
			case PostInfo::ptUnpacking:
			case PostInfo::ptCleaningUp:
			case PostInfo::ptMoving:
				needPause |= g_Options->GetUnpackPauseQueue();
				break;

			case PostInfo::ptExecutingScript:
				needPause |= g_Options->GetScriptPauseQueue();
				break;

			case PostInfo::ptQueued:
			case PostInfo::ptFinished:
				break;
		}
	}

	if (needPause && !g_WorkState->GetTempPauseDownload())
	{
		info("Pausing download before post-processing");
	}
	else if (!needPause && g_WorkState->GetTempPauseDownload())
	{
		info("Unpausing download after post-processing");
	}

	g_WorkState->SetTempPauseDownload(needPause);
}

bool PrePostProcessor::EditList(DownloadQueue* downloadQueue, IdList* idList,
	DownloadQueue::EEditAction action, const char* args)
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

bool PrePostProcessor::PostQueueDelete(DownloadQueue* downloadQueue, IdList* idList)
{
	bool ok = false;

	for (int id : *idList)
	{
		for (NzbInfo* nzbInfo: downloadQueue->GetQueue())
		{
			PostInfo* postInfo = nzbInfo->GetPostInfo();
			if (postInfo && nzbInfo->GetId() == id)
			{
				if (postInfo->GetWorking())
				{
					postInfo->GetNzbInfo()->PrintMessage(Message::mkInfo,
						"Deleting active post-job %s", postInfo->GetNzbInfo()->GetName());
					postInfo->SetDeleted(true);
					if (postInfo->GetPostThread())
					{
						debug("Terminating post-process thread for %s", postInfo->GetNzbInfo()->GetName());
						postInfo->GetPostThread()->Stop();
						ok = true;
					}
					else if (postInfo->GetNzbInfo()->GetUnpackThread())
					{
						((DirectUnpack*)postInfo->GetNzbInfo()->GetUnpackThread())->NzbDeleted(downloadQueue, postInfo->GetNzbInfo());
						ok = true;
					}
					else
					{
						error("Internal error in PrePostProcessor::QueueDelete");
					}
				}
				else
				{
					postInfo->GetNzbInfo()->PrintMessage(Message::mkInfo,
						"Deleting queued post-job %s", postInfo->GetNzbInfo()->GetName());

					JobCompleted(downloadQueue, postInfo);

					m_activeJobs.erase(std::remove_if(m_activeJobs.begin(), m_activeJobs.end(),
						[postInfo](NzbInfo* postJob)
						{
							return postInfo == postJob->GetPostInfo();
						}),
						m_activeJobs.end());

					ok = true;
				}
				break;
			}
		}
	}

	if (ok)
	{
		downloadQueue->Save();
	}

	return ok;
}

void PrePostProcessor::FileDownloaded(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, FileInfo* fileInfo)
{
	if (!nzbInfo->GetPostInfo())
	{
		g_QueueScriptCoordinator->EnqueueScript(nzbInfo, QueueScriptCoordinator::qeFileDownloaded);
	}

	if (g_Options->GetDirectUnpack() && !g_Options->GetRawArticle() && !g_Options->GetSkipWrite())
	{
		bool allowPar;
		if (nzbInfo->GetDirectUnpackStatus() == NzbInfo::nsNone &&
			nzbInfo->GetDirectRenameStatus() != NzbInfo::tsRunning &&
			DirectUnpack::IsArchiveFilename(fileInfo->GetFilename()) &&
			CanRunMoreJobs(&allowPar))
		{
			NzbParameter* unpackParameter = nzbInfo->GetParameters()->Find("*Unpack:");
			bool wantUnpack = !(unpackParameter && !strcasecmp(unpackParameter->GetValue(), "no"));
			if (wantUnpack && nzbInfo->GetFailedArticles() == 0)
			{
				DirectUnpack::StartJob(nzbInfo);
			}
		}
		else if (nzbInfo->GetDirectUnpackStatus() == NzbInfo::nsRunning)
		{
			((DirectUnpack*)nzbInfo->GetUnpackThread())->FileDownloaded(downloadQueue, fileInfo);
		}
	}
}
