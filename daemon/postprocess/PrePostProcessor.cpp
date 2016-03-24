/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "Log.h"
#include "HistoryCoordinator.h"
#include "DupeCoordinator.h"
#include "PostScript.h"
#include "Util.h"
#include "FileSystem.h"
#include "Unpack.h"
#include "Cleanup.h"
#include "NzbFile.h"
#include "QueueScript.h"
#include "ParParser.h"

PrePostProcessor::PrePostProcessor()
{
	debug("Creating PrePostProcessor");

	m_downloadQueueObserver.m_owner = this;
	DownloadQueue::Guard()->Attach(&m_downloadQueueObserver);
}

void PrePostProcessor::Run()
{
	debug("Entering PrePostProcessor-loop");

	while (!DownloadQueue::IsLoaded())
	{
		usleep(20 * 1000);
	}

	if (g_Options->GetServerMode() && g_Options->GetSaveQueue() && g_Options->GetReloadQueue())
	{
		SanitisePostQueue();
	}

	while (!IsStopped())
	{
		if (!g_Options->GetTempPausePostprocess())
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
	GuardedDownloadQueue guard = DownloadQueue::Guard();

#ifndef DISABLE_PARCHECK
	m_parCoordinator.Stop();
#endif

	if (m_curJob && m_curJob->GetPostInfo() &&
		(m_curJob->GetPostInfo()->GetStage() == PostInfo::ptUnpacking ||
		 m_curJob->GetPostInfo()->GetStage() == PostInfo::ptExecutingScript) &&
		m_curJob->GetPostInfo()->GetPostThread())
	{
		Thread* postThread = m_curJob->GetPostInfo()->GetPostThread();
		m_curJob->GetPostInfo()->SetPostThread(nullptr);
		postThread->SetAutoDestroy(true);
		postThread->Stop();
	}
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
		NzbFound(queueAspect->downloadQueue, queueAspect->nzbInfo);
	}
	else if (queueAspect->action == DownloadQueue::eaNzbAdded)
	{
		NzbAdded(queueAspect->downloadQueue, queueAspect->nzbInfo);
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
		NzbDeleted(queueAspect->downloadQueue, queueAspect->nzbInfo);
	}
	else if ((queueAspect->action == DownloadQueue::eaFileCompleted ||
		queueAspect->action == DownloadQueue::eaFileDeleted))
	{
		if (queueAspect->action == DownloadQueue::eaFileCompleted && !queueAspect->nzbInfo->GetPostInfo())
		{
			g_QueueScriptCoordinator->EnqueueScript(queueAspect->nzbInfo, QueueScriptCoordinator::qeFileDownloaded);
		}

		if (
#ifndef DISABLE_PARCHECK
			!m_parCoordinator.AddPar(queueAspect->fileInfo, queueAspect->action == DownloadQueue::eaFileDeleted) &&
#endif
			IsNzbFileCompleted(queueAspect->nzbInfo, true, false) &&
			!queueAspect->nzbInfo->GetPostInfo() &&
			(!queueAspect->fileInfo->GetPaused() || IsNzbFileCompleted(queueAspect->nzbInfo, false, false)))
		{
			if ((queueAspect->action == DownloadQueue::eaFileCompleted ||
				(queueAspect->fileInfo->GetAutoDeleted() &&
				 IsNzbFileCompleted(queueAspect->nzbInfo, false, true))) &&
				 queueAspect->fileInfo->GetNzbInfo()->GetDeleteStatus() != NzbInfo::dsHealth)
			{
				queueAspect->nzbInfo->PrintMessage(Message::mkInfo,
					"Collection %s completely downloaded", queueAspect->nzbInfo->GetName());
				g_QueueScriptCoordinator->EnqueueScript(queueAspect->nzbInfo, QueueScriptCoordinator::qeNzbDownloaded);
				NzbDownloaded(queueAspect->downloadQueue, queueAspect->nzbInfo);
			}
			else if ((queueAspect->action == DownloadQueue::eaFileDeleted ||
				(queueAspect->action == DownloadQueue::eaFileCompleted &&
				 queueAspect->fileInfo->GetNzbInfo()->GetDeleteStatus() > NzbInfo::dsNone)) &&
				!queueAspect->nzbInfo->GetParCleanup() &&
				IsNzbFileCompleted(queueAspect->nzbInfo, false, true))
			{
				queueAspect->nzbInfo->PrintMessage(Message::mkInfo,
					"Collection %s deleted from queue", queueAspect->nzbInfo->GetName());
				NzbDeleted(queueAspect->downloadQueue, queueAspect->nzbInfo);
			}
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
		m_parCoordinator.PausePars(downloadQueue, nzbInfo);
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

	if (!nzbInfo->GetPostInfo() && g_Options->GetDecode())
	{
		nzbInfo->PrintMessage(Message::mkInfo, "Queueing %s for post-processing", nzbInfo->GetName());

		nzbInfo->EnterPostProcess();
		m_jobCount++;

		if (nzbInfo->GetParStatus() == NzbInfo::psNone &&
			g_Options->GetParCheck() != Options::pcAlways &&
			g_Options->GetParCheck() != Options::pcForce)
		{
			nzbInfo->SetParStatus(NzbInfo::psSkipped);
		}

		if (nzbInfo->GetRenameStatus() == NzbInfo::rsNone && !g_Options->GetParRename())
		{
			nzbInfo->SetRenameStatus(NzbInfo::rsSkipped);
		}

		downloadQueue->Save();
	}
	else
	{
		NzbCompleted(downloadQueue, nzbInfo, true);
	}
}

void PrePostProcessor::NzbDeleted(DownloadQueue* downloadQueue, NzbInfo* nzbInfo)
{
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
		 nzbInfo->GetDeleteStatus() == NzbInfo::dsScan))
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
	if ((g_Options->GetDeleteCleanupDisk() && nzbInfo->GetCleanupDisk()) ||
		nzbInfo->GetDeleteStatus() == NzbInfo::dsDupe)
	{
		// download was cancelled, deleting already downloaded files from disk
		for (CompletedFileList::reverse_iterator it = nzbInfo->GetCompletedFiles()->rbegin(); it != nzbInfo->GetCompletedFiles()->rend(); it++)
		{
			CompletedFile& completedFile = *it;
			BString<1024> fullFileName("%s%c%s", nzbInfo->GetDestDir(), (int)PATH_SEPARATOR, completedFile.GetFileName());
			if (FileSystem::FileExists(fullFileName))
			{
				detail("Deleting file %s", completedFile.GetFileName());
				FileSystem::DeleteFile(fullFileName);
			}
		}

		// delete .out.tmp-files and _brokenlog.txt
		DirBrowser dir(nzbInfo->GetDestDir());
		while (const char* filename = dir.Next())
		{
			int len = strlen(filename);
			if ((len > 8 && !strcmp(filename + len - 8, ".out.tmp")) || !strcmp(filename, "_brokenlog.txt"))
			{
				BString<1024> fullFilename("%s%c%s", nzbInfo->GetDestDir(), PATH_SEPARATOR, filename);
				detail("Deleting file %s", filename);
				FileSystem::DeleteFile(fullFilename);
			}
		}

		// delete old directory (if empty)
		if (FileSystem::DirEmpty(nzbInfo->GetDestDir()))
		{
			FileSystem::RemoveDirectory(nzbInfo->GetDestDir());
		}
	}
}

void PrePostProcessor::CheckPostQueue()
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	if (!m_curJob && m_jobCount > 0)
	{
		m_curJob = GetNextJob(downloadQueue);
	}

	if (m_curJob)
	{
		PostInfo* postInfo = m_curJob->GetPostInfo();
		if (!postInfo->GetWorking() && !IsNzbFileDownloading(m_curJob))
		{
#ifndef DISABLE_PARCHECK
			if (postInfo->GetRequestParCheck() &&
				(postInfo->GetNzbInfo()->GetParStatus() <= NzbInfo::psSkipped ||
				 (postInfo->GetForceRepair() && !postInfo->GetNzbInfo()->GetParFull())) &&
				g_Options->GetParCheck() != Options::pcManual)
			{
				postInfo->SetForceParFull(postInfo->GetNzbInfo()->GetParStatus() > NzbInfo::psSkipped);
				postInfo->GetNzbInfo()->SetParStatus(NzbInfo::psNone);
				postInfo->SetRequestParCheck(false);
				postInfo->SetStage(PostInfo::ptQueued);
				postInfo->GetNzbInfo()->GetScriptStatuses()->clear();
				DeletePostThread(postInfo);
			}
			else if (postInfo->GetRequestParCheck() && postInfo->GetNzbInfo()->GetParStatus() <= NzbInfo::psSkipped &&
				g_Options->GetParCheck() == Options::pcManual)
			{
				postInfo->SetRequestParCheck(false);
				postInfo->GetNzbInfo()->SetParStatus(NzbInfo::psManual);
				DeletePostThread(postInfo);

				if (!postInfo->GetNzbInfo()->GetFileList()->empty())
				{
					postInfo->GetNzbInfo()->PrintMessage(Message::mkInfo,
						"Downloading all remaining files for manual par-check for %s", postInfo->GetNzbInfo()->GetName());
					downloadQueue->EditEntry(postInfo->GetNzbInfo()->GetId(), DownloadQueue::eaGroupResume, 0, nullptr);
					postInfo->SetStage(PostInfo::ptFinished);
				}
				else
				{
					postInfo->GetNzbInfo()->PrintMessage(Message::mkInfo,
						"There are no par-files remain for download for %s", postInfo->GetNzbInfo()->GetName());
					postInfo->SetStage(PostInfo::ptQueued);
				}
			}

#endif
			if (postInfo->GetDeleted())
			{
				postInfo->SetStage(PostInfo::ptFinished);
			}

			if (postInfo->GetStage() == PostInfo::ptQueued &&
				(!g_Options->GetPausePostProcess() || postInfo->GetNzbInfo()->GetForcePriority()))
			{
				DeletePostThread(postInfo);
				StartJob(downloadQueue, postInfo);
			}
			else if (postInfo->GetStage() == PostInfo::ptFinished)
			{
				UpdatePauseState(false, nullptr);
				JobCompleted(downloadQueue, postInfo);
			}
			else if (!g_Options->GetPausePostProcess())
			{
				error("Internal error: invalid state in post-processor");
				// TODO: cancel (delete) current job
			}
		}
	}
}

NzbInfo* PrePostProcessor::GetNextJob(DownloadQueue* downloadQueue)
{
	NzbInfo* nzbInfo = nullptr;

	for (NzbInfo* nzbInfo1: downloadQueue->GetQueue())
	{
		if (nzbInfo1->GetPostInfo() && !g_QueueScriptCoordinator->HasJob(nzbInfo1->GetId(), nullptr) &&
			(!nzbInfo || nzbInfo1->GetPriority() > nzbInfo->GetPriority()) &&
			(!g_Options->GetPausePostProcess() || nzbInfo1->GetForcePriority()))
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
void PrePostProcessor::SanitisePostQueue()
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
	for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
	{
		PostInfo* postInfo = nzbInfo->GetPostInfo();
		if (postInfo)
		{
			m_jobCount++;
			if (postInfo->GetStage() == PostInfo::ptExecutingScript ||
				!FileSystem::DirectoryExists(nzbInfo->GetDestDir()))
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
	postInfo->SetPostThread(nullptr);
}

void PrePostProcessor::StartJob(DownloadQueue* downloadQueue, PostInfo* postInfo)
{
	if (!postInfo->GetStartTime())
	{
		postInfo->SetStartTime(Util::CurrentTime());
	}

#ifndef DISABLE_PARCHECK
	if (postInfo->GetNzbInfo()->GetRenameStatus() == NzbInfo::rsNone &&
		postInfo->GetNzbInfo()->GetDeleteStatus() == NzbInfo::dsNone)
	{
		UpdatePauseState(g_Options->GetParPauseQueue(), "par-rename");
		m_parCoordinator.StartParRenameJob(postInfo);
		return;
	}
	else if (postInfo->GetNzbInfo()->GetParStatus() == NzbInfo::psNone &&
		postInfo->GetNzbInfo()->GetDeleteStatus() == NzbInfo::dsNone)
	{
		if (ParParser::FindMainPars(postInfo->GetNzbInfo()->GetDestDir(), nullptr))
		{
			UpdatePauseState(g_Options->GetParPauseQueue(), "par-check");
			m_parCoordinator.StartParCheckJob(postInfo);
		}
		else
		{
			postInfo->GetNzbInfo()->PrintMessage(Message::mkInfo,
				"Nothing to par-check for %s", postInfo->GetNzbInfo()->GetName());
			postInfo->GetNzbInfo()->SetParStatus(NzbInfo::psSkipped);
			postInfo->SetWorking(false);
			postInfo->SetStage(PostInfo::ptQueued);
		}
		return;
	}
	else if (postInfo->GetNzbInfo()->GetParStatus() == NzbInfo::psSkipped &&
		((g_Options->GetParScan() != Options::psDupe &&
		  postInfo->GetNzbInfo()->CalcHealth() < postInfo->GetNzbInfo()->CalcCriticalHealth(false) &&
		  postInfo->GetNzbInfo()->CalcCriticalHealth(false) < 1000) ||
		  postInfo->GetNzbInfo()->CalcHealth() == 0) &&
		ParParser::FindMainPars(postInfo->GetNzbInfo()->GetDestDir(), nullptr))
	{
		if (postInfo->GetNzbInfo()->CalcHealth() == 0)
		{
			postInfo->GetNzbInfo()->PrintMessage(Message::mkWarning,
				"Skipping par-check for %s due to health 0%%", postInfo->GetNzbInfo()->GetName());
		}
		else
		{
			postInfo->GetNzbInfo()->PrintMessage(Message::mkWarning,
				"Skipping par-check for %s due to health %.1f%% below critical %.1f%%",
				postInfo->GetNzbInfo()->GetName(),
				postInfo->GetNzbInfo()->CalcHealth() / 10.0, postInfo->GetNzbInfo()->CalcCriticalHealth(false) / 10.0);
		}
		postInfo->GetNzbInfo()->SetParStatus(NzbInfo::psFailure);
		return;
	}
	else if (postInfo->GetNzbInfo()->GetParStatus() == NzbInfo::psSkipped &&
		postInfo->GetNzbInfo()->GetFailedSize() - postInfo->GetNzbInfo()->GetParFailedSize() > 0 &&
		ParParser::FindMainPars(postInfo->GetNzbInfo()->GetDestDir(), nullptr))
	{
		postInfo->GetNzbInfo()->PrintMessage(Message::mkInfo,
			"Collection %s with health %.1f%% needs par-check",
			postInfo->GetNzbInfo()->GetName(), postInfo->GetNzbInfo()->CalcHealth() / 10.0);
		postInfo->SetRequestParCheck(true);
		return;
	}
#endif

	NzbParameter* unpackParameter = postInfo->GetNzbInfo()->GetParameters()->Find("*Unpack:", false);
	bool unpackParam = !(unpackParameter && !strcasecmp(unpackParameter->GetValue(), "no"));
	bool unpack = unpackParam && postInfo->GetNzbInfo()->GetUnpackStatus() == NzbInfo::usNone &&
		postInfo->GetNzbInfo()->GetDeleteStatus() == NzbInfo::dsNone;

	bool parFailed = postInfo->GetNzbInfo()->GetParStatus() == NzbInfo::psFailure ||
		postInfo->GetNzbInfo()->GetParStatus() == NzbInfo::psRepairPossible ||
		postInfo->GetNzbInfo()->GetParStatus() == NzbInfo::psManual;

	bool cleanup = !unpack &&
		postInfo->GetNzbInfo()->GetCleanupStatus() == NzbInfo::csNone &&
		!Util::EmptyStr(g_Options->GetExtCleanupDisk()) &&
		((postInfo->GetNzbInfo()->GetParStatus() == NzbInfo::psSuccess &&
		  postInfo->GetNzbInfo()->GetUnpackStatus() != NzbInfo::usFailure &&
		  postInfo->GetNzbInfo()->GetUnpackStatus() != NzbInfo::usSpace &&
		  postInfo->GetNzbInfo()->GetUnpackStatus() != NzbInfo::usPassword) ||
		 (postInfo->GetNzbInfo()->GetUnpackStatus() == NzbInfo::usSuccess &&
		  postInfo->GetNzbInfo()->GetParStatus() != NzbInfo::psFailure) ||
		 ((postInfo->GetNzbInfo()->GetUnpackStatus() == NzbInfo::usNone ||
		   postInfo->GetNzbInfo()->GetUnpackStatus() == NzbInfo::usSkipped) &&
		  (postInfo->GetNzbInfo()->GetParStatus() == NzbInfo::psNone ||
		   postInfo->GetNzbInfo()->GetParStatus() == NzbInfo::psSkipped) &&
		  postInfo->GetNzbInfo()->CalcHealth() == 1000));

	bool moveInter = !unpack &&
		postInfo->GetNzbInfo()->GetMoveStatus() == NzbInfo::msNone &&
		postInfo->GetNzbInfo()->GetUnpackStatus() != NzbInfo::usFailure &&
		postInfo->GetNzbInfo()->GetUnpackStatus() != NzbInfo::usSpace &&
		postInfo->GetNzbInfo()->GetUnpackStatus() != NzbInfo::usPassword &&
		postInfo->GetNzbInfo()->GetParStatus() != NzbInfo::psFailure &&
		postInfo->GetNzbInfo()->GetParStatus() != NzbInfo::psManual &&
		postInfo->GetNzbInfo()->GetDeleteStatus() == NzbInfo::dsNone &&
		!Util::EmptyStr(g_Options->GetInterDir()) &&
		!strncmp(postInfo->GetNzbInfo()->GetDestDir(), g_Options->GetInterDir(), strlen(g_Options->GetInterDir())) &&
		postInfo->GetNzbInfo()->GetDestDir()[strlen(g_Options->GetInterDir())] == PATH_SEPARATOR;

	bool postScript = true;

	if (unpack && parFailed)
	{
		postInfo->GetNzbInfo()->PrintMessage(Message::mkWarning,
			"Skipping unpack for %s due to %s", postInfo->GetNzbInfo()->GetName(),
			postInfo->GetNzbInfo()->GetParStatus() == NzbInfo::psManual ? "required par-repair" : "par-failure");
		postInfo->GetNzbInfo()->SetUnpackStatus(NzbInfo::usSkipped);
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

	postInfo->SetStageTime(Util::CurrentTime());

	if (unpack)
	{
		UpdatePauseState(g_Options->GetUnpackPauseQueue(), "unpack");
		UnpackController::StartJob(postInfo);
	}
	else if (cleanup)
	{
		UpdatePauseState(g_Options->GetUnpackPauseQueue() || g_Options->GetScriptPauseQueue(), "cleanup");
		CleanupController::StartJob(postInfo);
	}
	else if (moveInter)
	{
		UpdatePauseState(g_Options->GetUnpackPauseQueue() || g_Options->GetScriptPauseQueue(), "move");
		MoveController::StartJob(postInfo);
	}
	else
	{
		UpdatePauseState(g_Options->GetScriptPauseQueue(), "post-process-script");
		PostScriptController::StartJob(postInfo);
	}
}

void PrePostProcessor::JobCompleted(DownloadQueue* downloadQueue, PostInfo* postInfo)
{
	NzbInfo* nzbInfo = postInfo->GetNzbInfo();

	if (postInfo->GetStartTime() > 0)
	{
		nzbInfo->SetPostTotalSec((int)(Util::CurrentTime() - postInfo->GetStartTime()));
		postInfo->SetStartTime(0);
	}

	DeletePostThread(postInfo);
	nzbInfo->LeavePostProcess();

	if (IsNzbFileCompleted(nzbInfo, true, false))
	{
		// Cleaning up queue if par-check was successful or unpack was successful or
		// health is 100% (if unpack and par-check were not performed)
		// or health is below critical health
		bool canCleanupQueue =
			((nzbInfo->GetParStatus() == NzbInfo::psSuccess ||
			  nzbInfo->GetParStatus() == NzbInfo::psRepairPossible) &&
			 nzbInfo->GetUnpackStatus() != NzbInfo::usFailure &&
			 nzbInfo->GetUnpackStatus() != NzbInfo::usSpace &&
			 nzbInfo->GetUnpackStatus() != NzbInfo::usPassword) ||
			(nzbInfo->GetUnpackStatus() == NzbInfo::usSuccess &&
			 nzbInfo->GetParStatus() != NzbInfo::psFailure) ||
			(nzbInfo->GetUnpackStatus() <= NzbInfo::usSkipped &&
			 nzbInfo->GetParStatus() != NzbInfo::psFailure &&
			 nzbInfo->GetFailedSize() - nzbInfo->GetParFailedSize() == 0) ||
			(nzbInfo->CalcHealth() < nzbInfo->CalcCriticalHealth(false) &&
			 nzbInfo->CalcCriticalHealth(false) < 1000);
		if (g_Options->GetParCleanupQueue() && canCleanupQueue && !nzbInfo->GetFileList()->empty())
		{
			nzbInfo->PrintMessage(Message::mkInfo, "Cleaning up download queue for %s", nzbInfo->GetName());
			nzbInfo->SetParCleanup(true);
			downloadQueue->EditEntry(nzbInfo->GetId(), DownloadQueue::eaGroupDelete, 0, nullptr);
		}

		if (nzbInfo->GetUnpackCleanedUpDisk())
		{
			nzbInfo->GetCompletedFiles()->clear();
		}

		NzbCompleted(downloadQueue, nzbInfo, false);
	}

	if (nzbInfo == m_curJob)
	{
		m_curJob = nullptr;
	}
	m_jobCount--;

	downloadQueue->Save();
}

bool PrePostProcessor::IsNzbFileCompleted(NzbInfo* nzbInfo, bool ignorePausedPars, bool allowOnlyOneDeleted)
{
	int deleted = 0;

	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
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

bool PrePostProcessor::IsNzbFileDownloading(NzbInfo* nzbInfo)
{
	if (nzbInfo->GetActiveDownloads())
	{
		return true;
	}

	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
		if (!fileInfo->GetPaused())
		{
			return true;
		}
	}

	return false;
}

void PrePostProcessor::UpdatePauseState(bool needPause, const char* reason)
{
	if (needPause && !g_Options->GetTempPauseDownload())
	{
		info("Pausing download before %s", reason);
	}
	else if (!needPause && g_Options->GetTempPauseDownload())
	{
		info("Unpausing download after %s", m_pauseReason);
	}
	g_Options->SetTempPauseDownload(needPause);
	m_pauseReason = reason;
}

bool PrePostProcessor::EditList(DownloadQueue* downloadQueue, IdList* idList, DownloadQueue::EEditAction action, int offset, const char* text)
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
						debug("Terminating %s for %s", (postInfo->GetStage() == PostInfo::ptUnpacking ? "unpack" : "post-process-script"), postInfo->GetNzbInfo()->GetName());
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
					postInfo->GetNzbInfo()->PrintMessage(Message::mkInfo,
						"Deleting queued post-job %s", postInfo->GetNzbInfo()->GetName());
					JobCompleted(downloadQueue, postInfo);
					ok = true;
				}
				break;
			}
		}
	}

	return ok;
}
