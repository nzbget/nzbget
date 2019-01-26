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
#include "HistoryCoordinator.h"
#include "Options.h"
#include "Log.h"
#include "QueueCoordinator.h"
#include "DiskState.h"
#include "Util.h"
#include "FileSystem.h"
#include "NzbFile.h"
#include "DupeCoordinator.h"
#include "ParParser.h"
#include "PrePostProcessor.h"
#include "DupeCoordinator.h"
#include "ServerPool.h"

/**
 * Removes old entries from (recent) history
 */
void HistoryCoordinator::ServiceWork()
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	time_t minTime = Util::CurrentTime() - g_Options->GetKeepHistory() * 60*60*24;
	bool changed = false;
	int index = 0;

	// traversing in a reverse order to delete items in order they were added to history
	// (just to produce the log-messages in a more logical order)
	for (HistoryList::reverse_iterator it = downloadQueue->GetHistory()->rbegin(); it != downloadQueue->GetHistory()->rend(); )
	{
		HistoryInfo* historyInfo = (*it).get();
		if (historyInfo->GetKind() != HistoryInfo::hkDup && historyInfo->GetTime() < minTime)
		{
			if (g_Options->GetDupeCheck() && historyInfo->GetKind() == HistoryInfo::hkNzb)
			{
				// replace history element
				HistoryHide(downloadQueue, historyInfo, index);
				index++;
			}
			else
			{
				if (historyInfo->GetKind() == HistoryInfo::hkNzb)
				{
					DeleteDiskFiles(historyInfo->GetNzbInfo());
				}
				info("Collection %s removed from history", historyInfo->GetName());

				downloadQueue->GetHistory()->erase(downloadQueue->GetHistory()->end() - 1 - index);
			}

			it = downloadQueue->GetHistory()->rbegin() + index;
			changed = true;
		}
		else
		{
			it++;
			index++;
		}
	}

	if (changed)
	{
		downloadQueue->HistoryChanged();
		downloadQueue->Save();
	}
}

void HistoryCoordinator::DeleteDiskFiles(NzbInfo* nzbInfo)
{
	if (g_Options->GetServerMode())
	{
		// delete parked files
		g_DiskState->DiscardFiles(nzbInfo);
	}
	nzbInfo->GetFileList()->clear();

	// delete nzb-file
	if (!g_Options->GetNzbCleanupDisk())
	{
		return;
	}

	// QueuedFile may contain one filename or several filenames separated
	// with "|"-character (for merged groups)
	CString filename = nzbInfo->GetQueuedFilename();
	char* end = filename - 1;
	while (end)
	{
		char* name1 = end + 1;
		end = strchr(name1, '|');
		if (end) *end = '\0';

		if (FileSystem::FileExists(name1))
		{
			info("Deleting file %s", name1);
			FileSystem::DeleteFile(name1);
		}
	}
}

void HistoryCoordinator::AddToHistory(DownloadQueue* downloadQueue, NzbInfo* nzbInfo)
{
	std::unique_ptr<NzbInfo> oldNzbInfo = downloadQueue->GetQueue()->Remove(nzbInfo);
	std::unique_ptr<HistoryInfo> historyInfo = std::make_unique<HistoryInfo>(std::move(oldNzbInfo));
	historyInfo->SetTime(Util::CurrentTime());
	downloadQueue->GetHistory()->Add(std::move(historyInfo), true);
	downloadQueue->HistoryChanged();

	// park remaining files
	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
		nzbInfo->UpdateCompletedStats(fileInfo);
		nzbInfo->GetCompletedFiles()->emplace_back(fileInfo->GetId(), fileInfo->GetFilename(),
			fileInfo->GetOrigname(), CompletedFile::cfNone, 0, fileInfo->GetParFile(),
			fileInfo->GetHash16k(), fileInfo->GetParSetId());
	}

	// Cleaning up parked files if par-check was successful or unpack was successful or
	// health is 100% (if unpack and par-check were not performed) or if deleted
	bool cleanupParkedFiles =
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
		(nzbInfo->GetDeleteStatus() != NzbInfo::dsNone);

	// Do not cleanup when parking
	cleanupParkedFiles &= !nzbInfo->GetParking();

	// Parking not possible if files were already deleted
	cleanupParkedFiles |= nzbInfo->GetUnpackCleanedUpDisk();

	if (cleanupParkedFiles)
	{
		g_DiskState->DiscardFiles(nzbInfo, false);
		nzbInfo->GetCompletedFiles()->clear();
	}

	nzbInfo->SetParkedFileCount(0);
	for (CompletedFile& completedFile : nzbInfo->GetCompletedFiles())
	{
		if (completedFile.GetStatus() == CompletedFile::cfNone ||
			// consider last completed file with partial status not completely tried
			(completedFile.GetStatus() == CompletedFile::cfPartial &&
			 &completedFile == &*nzbInfo->GetCompletedFiles()->rbegin()))
		{
			nzbInfo->PrintMessage(Message::mkDetail, "Parking file %s", completedFile.GetFilename());
			nzbInfo->SetParkedFileCount(nzbInfo->GetParkedFileCount() + 1);
		}
	}

	nzbInfo->GetFileList()->clear();
	nzbInfo->SetRemainingParCount(0);
	nzbInfo->SetParking(false);

	if (nzbInfo->GetDirectRenameStatus() == NzbInfo::tsRunning)
	{
		nzbInfo->SetDirectRenameStatus(NzbInfo::tsFailure);
	}

	nzbInfo->SetDupeHint(NzbInfo::dhNone);

	nzbInfo->PrintMessage(Message::mkInfo, "Collection %s added to history", nzbInfo->GetName());
}

void HistoryCoordinator::HistoryHide(DownloadQueue* downloadQueue, HistoryInfo* historyInfo, int rindex)
{
	// replace history element
	std::unique_ptr<DupInfo> dupInfo = std::make_unique<DupInfo>();
	dupInfo->SetId(historyInfo->GetNzbInfo()->GetId());
	dupInfo->SetName(historyInfo->GetNzbInfo()->GetName());
	dupInfo->SetDupeKey(historyInfo->GetNzbInfo()->GetDupeKey());
	dupInfo->SetDupeScore(historyInfo->GetNzbInfo()->GetDupeScore());
	dupInfo->SetDupeMode(historyInfo->GetNzbInfo()->GetDupeMode());
	dupInfo->SetSize(historyInfo->GetNzbInfo()->GetSize());
	dupInfo->SetFullContentHash(historyInfo->GetNzbInfo()->GetFullContentHash());
	dupInfo->SetFilteredContentHash(historyInfo->GetNzbInfo()->GetFilteredContentHash());

	dupInfo->SetStatus(
		historyInfo->GetNzbInfo()->GetMarkStatus() == NzbInfo::ksGood ? DupInfo::dsGood :
		historyInfo->GetNzbInfo()->GetMarkStatus() == NzbInfo::ksBad ? DupInfo::dsBad :
		historyInfo->GetNzbInfo()->GetMarkStatus() == NzbInfo::ksSuccess ? DupInfo::dsSuccess :
		historyInfo->GetNzbInfo()->GetDeleteStatus() == NzbInfo::dsDupe ? DupInfo::dsDupe :
		historyInfo->GetNzbInfo()->GetDeleteStatus() == NzbInfo::dsManual ||
		historyInfo->GetNzbInfo()->GetDeleteStatus() == NzbInfo::dsGood ||
		historyInfo->GetNzbInfo()->GetDeleteStatus() == NzbInfo::dsCopy ? DupInfo::dsDeleted :
		historyInfo->GetNzbInfo()->IsDupeSuccess() ? DupInfo::dsSuccess :
		DupInfo::dsFailed);

	std::unique_ptr<HistoryInfo> newHistoryInfo = std::make_unique<HistoryInfo>(std::move(dupInfo));
	newHistoryInfo->SetTime(historyInfo->GetTime());

	DeleteDiskFiles(historyInfo->GetNzbInfo());

	info("Collection %s removed from history", historyInfo->GetName());

	(*downloadQueue->GetHistory())[downloadQueue->GetHistory()->size() - 1 - rindex] = std::move(newHistoryInfo);
}

void HistoryCoordinator::PrepareEdit(DownloadQueue* downloadQueue, IdList* idList, DownloadQueue::EEditAction action)
{
	// First pass: when marking multiple items - mark them bad without performing the mark-logic,
	// this will later (on second step) avoid moving other items to download queue, if they are marked bad too.
	if (action == DownloadQueue::eaHistoryMarkBad)
	{
		for (int id : *idList)
		{
			HistoryInfo* historyInfo = downloadQueue->GetHistory()->Find(id);
			if (historyInfo && historyInfo->GetKind() == HistoryInfo::hkNzb)
			{
				historyInfo->GetNzbInfo()->SetMarkStatus(NzbInfo::ksBad);
			}
		}
	}
}

bool HistoryCoordinator::EditList(DownloadQueue* downloadQueue, IdList* idList,
	DownloadQueue::EEditAction action, const char* args)
{
	bool ok = false;
	PrepareEdit(downloadQueue, idList, action);

	for (int id : *idList)
	{
		for (HistoryList::iterator itHistory = downloadQueue->GetHistory()->begin(); itHistory != downloadQueue->GetHistory()->end(); itHistory++)
		{
			HistoryInfo* historyInfo = (*itHistory).get();
			if (historyInfo->GetId() == id)
			{
				ok = true;

				switch (action)
				{
					case DownloadQueue::eaHistoryDelete:
					case DownloadQueue::eaHistoryFinalDelete:
						HistoryDelete(downloadQueue, itHistory, historyInfo, action == DownloadQueue::eaHistoryFinalDelete);
						break;

					case DownloadQueue::eaHistoryReturn:
						HistoryReturn(downloadQueue, itHistory, historyInfo);
						break;

					case DownloadQueue::eaHistoryProcess:
						HistoryProcess(downloadQueue, itHistory, historyInfo);
						break;

					case DownloadQueue::eaHistoryRedownload:
						HistoryRedownload(downloadQueue, itHistory, historyInfo, false);
						break;

					case DownloadQueue::eaHistoryRetryFailed:
						HistoryRetry(downloadQueue, itHistory, historyInfo, true, false);
						break;

					case DownloadQueue::eaHistorySetParameter:
						ok = HistorySetParameter(historyInfo, args);
						break;

 					case DownloadQueue::eaHistorySetCategory:
						ok = HistorySetCategory(historyInfo, args);
						break;

 					case DownloadQueue::eaHistorySetName:
						ok = HistorySetName(historyInfo, args);
						break;

					case DownloadQueue::eaHistorySetDupeKey:
					case DownloadQueue::eaHistorySetDupeScore:
					case DownloadQueue::eaHistorySetDupeMode:
					case DownloadQueue::eaHistorySetDupeBackup:
						HistorySetDupeParam(historyInfo, action, args);
						break;

					case DownloadQueue::eaHistoryMarkBad:
						g_DupeCoordinator->HistoryMark(downloadQueue, historyInfo, NzbInfo::ksBad);
						break;

					case DownloadQueue::eaHistoryMarkGood:
						g_DupeCoordinator->HistoryMark(downloadQueue, historyInfo, NzbInfo::ksGood);
						break;

					case DownloadQueue::eaHistoryMarkSuccess:
						g_DupeCoordinator->HistoryMark(downloadQueue, historyInfo, NzbInfo::ksSuccess);
						break;

					default:
						// nothing, just to avoid compiler warning
						break;
				}

				break;
			}
		}
	}

	if (ok)
	{
		downloadQueue->HistoryChanged();
		downloadQueue->Save();
	}

	return ok;
}

void HistoryCoordinator::HistoryDelete(DownloadQueue* downloadQueue, HistoryList::iterator itHistory,
	HistoryInfo* historyInfo, bool final)
{
	info("Deleting %s from history", historyInfo->GetName());

	if (historyInfo->GetKind() == HistoryInfo::hkNzb)
	{
		DeleteDiskFiles(historyInfo->GetNzbInfo());
	}

	if (historyInfo->GetKind() == HistoryInfo::hkNzb &&
		(historyInfo->GetNzbInfo()->GetDeleteStatus() != NzbInfo::dsNone ||
		historyInfo->GetNzbInfo()->GetParStatus() == NzbInfo::psFailure ||
		historyInfo->GetNzbInfo()->GetUnpackStatus() == NzbInfo::usFailure ||
		historyInfo->GetNzbInfo()->GetUnpackStatus() == NzbInfo::usPassword) &&
		FileSystem::DirectoryExists(historyInfo->GetNzbInfo()->GetDestDir()))
	{
		info("Deleting %s", historyInfo->GetNzbInfo()->GetDestDir());
		CString errmsg;
		if (!FileSystem::DeleteDirectoryWithContent(historyInfo->GetNzbInfo()->GetDestDir(), errmsg))
		{
			error("Could not delete directory %s: %s", historyInfo->GetNzbInfo()->GetDestDir(), *errmsg);
		}
	}

	if (final || !g_Options->GetDupeCheck() || historyInfo->GetKind() == HistoryInfo::hkUrl)
	{
		downloadQueue->GetHistory()->erase(itHistory);
	}
	else
	{
		if (historyInfo->GetKind() == HistoryInfo::hkNzb)
		{
			// replace history element
			int rindex = downloadQueue->GetHistory()->size() - 1 - (itHistory - downloadQueue->GetHistory()->begin());
			HistoryHide(downloadQueue, historyInfo, rindex);
		}
	}
}

void HistoryCoordinator::MoveToQueue(DownloadQueue* downloadQueue, HistoryList::iterator itHistory, HistoryInfo* historyInfo, bool reprocess)
{
	debug("Returning %s from history back to download queue", historyInfo->GetName());
	CString nicename = historyInfo->GetName();
	NzbInfo* nzbInfo = historyInfo->GetNzbInfo();

	// unpark files
	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
		nzbInfo->PrintMessage(Message::mkDetail, "Unparking file %s", fileInfo->GetFilename());
	}

	downloadQueue->GetQueue()->Add(std::unique_ptr<NzbInfo>(nzbInfo), true);
	historyInfo->DiscardNzbInfo();

	// reset postprocessing status variables
	if (!nzbInfo->GetUnpackCleanedUpDisk())
	{
		nzbInfo->SetUnpackStatus(NzbInfo::usNone);
		nzbInfo->SetDirectUnpackStatus(NzbInfo::nsNone);
		nzbInfo->SetCleanupStatus(NzbInfo::csNone);
		nzbInfo->SetParRenameStatus(NzbInfo::rsNone);
		nzbInfo->SetRarRenameStatus(NzbInfo::rsNone);
		nzbInfo->SetPostTotalSec(nzbInfo->GetPostTotalSec() - nzbInfo->GetUnpackSec());
		nzbInfo->SetUnpackSec(0);

		if (ParParser::FindMainPars(nzbInfo->GetDestDir(), nullptr))
		{
			nzbInfo->SetParStatus(NzbInfo::psNone);
			nzbInfo->SetPostTotalSec(nzbInfo->GetPostTotalSec() - nzbInfo->GetParSec());
			nzbInfo->SetParSec(0);
			nzbInfo->SetRepairSec(0);
			nzbInfo->SetParFull(false);
		}
	}
	nzbInfo->SetDeleteStatus(NzbInfo::dsNone);
	nzbInfo->SetDeletePaused(false);
	nzbInfo->SetMarkStatus(NzbInfo::ksNone);
	nzbInfo->GetScriptStatuses()->clear();
	nzbInfo->SetParkedFileCount(0);
	if (nzbInfo->GetMoveStatus() == NzbInfo::msFailure)
	{
		nzbInfo->SetMoveStatus(NzbInfo::msNone);
	}
	nzbInfo->SetReprocess(reprocess);
	nzbInfo->SetFinalDir("");

	downloadQueue->GetHistory()->erase(itHistory);
	// the object "pHistoryInfo" is released few lines later, after the call to "NZBDownloaded"
	nzbInfo->PrintMessage(Message::mkInfo, "%s returned from history back to download queue", *nicename);

	if (reprocess)
	{
		// start postprocessing
		debug("Restarting postprocessing for %s", *nicename);
		g_PrePostProcessor->NzbDownloaded(downloadQueue, nzbInfo);
		DownloadQueue::Aspect aspect = {DownloadQueue::eaNzbReturned, downloadQueue, nzbInfo, nullptr};
		downloadQueue->Notify(&aspect);
	}
}

void HistoryCoordinator::HistoryRedownload(DownloadQueue* downloadQueue, HistoryList::iterator itHistory,
	HistoryInfo* historyInfo, bool restorePauseState)
{
	if (historyInfo->GetKind() == HistoryInfo::hkUrl)
	{
		NzbInfo* nzbInfo = historyInfo->GetNzbInfo();
		historyInfo->DiscardNzbInfo();
		nzbInfo->SetUrlStatus(NzbInfo::lsNone);
		nzbInfo->SetDeleteStatus(NzbInfo::dsNone);
		nzbInfo->SetDupeHint(nzbInfo->GetDupeHint() == NzbInfo::dhNone ? NzbInfo::dhRedownloadManual : nzbInfo->GetDupeHint());
		downloadQueue->GetQueue()->Add(std::unique_ptr<NzbInfo>(nzbInfo), true);
		downloadQueue->GetHistory()->erase(itHistory);

		DownloadQueue::Aspect aspect = {DownloadQueue::eaUrlReturned, downloadQueue, nzbInfo, nullptr};
		downloadQueue->Notify(&aspect);

		return;
	}

	if (historyInfo->GetKind() != HistoryInfo::hkNzb)
	{
		error("Could not download again %s: history item has wrong type", historyInfo->GetName());
		return;
	}

	NzbInfo* nzbInfo = historyInfo->GetNzbInfo();
	bool paused = restorePauseState && nzbInfo->GetDeletePaused();

	if (!FileSystem::FileExists(nzbInfo->GetQueuedFilename()))
	{
		error("Could not download again %s: could not find source nzb-file %s",
			nzbInfo->GetName(), nzbInfo->GetQueuedFilename());
		return;
	}

	NzbFile nzbFile(nzbInfo->GetQueuedFilename(), "");
	if (!nzbFile.Parse())
	{
		error("Could not download again %s: could not parse nzb-file", nzbInfo->GetName());
		return;
	}

	info("Downloading again %s", nzbInfo->GetName());

	std::unique_ptr<NzbInfo> newNzbInfo = nzbFile.DetachNzbInfo();

	for (FileInfo* fileInfo : newNzbInfo->GetFileList())
	{
		fileInfo->SetPaused(paused);
	}

	if (FileSystem::DirectoryExists(nzbInfo->GetDestDir()))
	{
		detail("Deleting %s", nzbInfo->GetDestDir());
		CString errmsg;
		if (!FileSystem::DeleteDirectoryWithContent(nzbInfo->GetDestDir(), errmsg))
		{
			error("Could not delete directory %s: %s", nzbInfo->GetDestDir(), *errmsg);
		}
	}

	nzbInfo->BuildDestDirName();
	if (FileSystem::DirectoryExists(nzbInfo->GetDestDir()))
	{
		detail("Deleting %s", nzbInfo->GetDestDir());
		CString errmsg;
		if (!FileSystem::DeleteDirectoryWithContent(nzbInfo->GetDestDir(), errmsg))
		{
			error("Could not delete directory %s: %s", nzbInfo->GetDestDir(), *errmsg);
		}
	}

	g_DiskState->DiscardFiles(nzbInfo);

	// reset status fields (which are not reset by "MoveToQueue")
	nzbInfo->SetMoveStatus(NzbInfo::msNone);
	nzbInfo->SetUnpackCleanedUpDisk(false);
	nzbInfo->SetParStatus(NzbInfo::psNone);
	nzbInfo->SetParRenameStatus(NzbInfo::rsNone);
	nzbInfo->SetRarRenameStatus(NzbInfo::rsNone);
	nzbInfo->SetDirectRenameStatus(NzbInfo::tsNone);
	nzbInfo->SetDirectUnpackStatus(NzbInfo::nsNone);
	nzbInfo->SetDownloadedSize(0);
	nzbInfo->SetDownloadSec(0);
	nzbInfo->SetPostTotalSec(0);
	nzbInfo->SetParSec(0);
	nzbInfo->SetRepairSec(0);
	nzbInfo->SetUnpackSec(0);
	nzbInfo->SetExtraParBlocks(0);
	nzbInfo->SetAllFirst(false);
	nzbInfo->SetWaitingPar(false);
	nzbInfo->SetLoadingPar(false);
	nzbInfo->GetCompletedFiles()->clear();
	nzbInfo->GetServerStats()->clear();
	nzbInfo->GetCurrentServerStats()->clear();

	nzbInfo->MoveFileList(newNzbInfo.get());

	g_QueueCoordinator->CheckDupeFileInfos(nzbInfo);

	MoveToQueue(downloadQueue, itHistory, historyInfo, false);

	g_PrePostProcessor->NzbAdded(downloadQueue, nzbInfo);

	DownloadQueue::Aspect aspect = {DownloadQueue::eaNzbReturned, downloadQueue, nzbInfo, nullptr};
	downloadQueue->Notify(&aspect);
}

void HistoryCoordinator::HistoryReturn(DownloadQueue* downloadQueue, HistoryList::iterator itHistory,
	HistoryInfo* historyInfo)
{
	if (historyInfo->GetKind() == HistoryInfo::hkUrl)
	{
		HistoryRedownload(downloadQueue, itHistory, historyInfo, false);
	}
	else if (historyInfo->GetKind() == HistoryInfo::hkNzb && historyInfo->GetNzbInfo()->GetParkedFileCount() == 0)
	{
		warn("Could not download remaining files for %s: history item does not have any files left for download", historyInfo->GetName());
	}
	else if (historyInfo->GetKind() == HistoryInfo::hkNzb)
	{
		HistoryRetry(downloadQueue, itHistory, historyInfo, false, false);
	}
	else
	{
		error("Could not download remaining files for %s: history item has wrong type", historyInfo->GetName());
	}
}

void HistoryCoordinator::HistoryProcess(DownloadQueue* downloadQueue, HistoryList::iterator itHistory, HistoryInfo* historyInfo)
{
	if (historyInfo->GetKind() != HistoryInfo::hkNzb)
	{
		error("Could not post-process again %s: history item has wrong type", historyInfo->GetName());
		return;
	}

	HistoryRetry(downloadQueue, itHistory, historyInfo, false, true);
}

void HistoryCoordinator::HistoryRetry(DownloadQueue* downloadQueue, HistoryList::iterator itHistory,
	HistoryInfo* historyInfo, bool resetFailed, bool reprocess)
{
	if (historyInfo->GetKind() != HistoryInfo::hkNzb)
	{
		error("Could not %s for %s: history item has wrong type",
			(resetFailed ? "retry failed articles" : "download remaining files"),
			historyInfo->GetName());
		return;
	}

	NzbInfo* nzbInfo = historyInfo->GetNzbInfo();

	if (!FileSystem::DirectoryExists(nzbInfo->GetDestDir()))
	{
		error("Could not %s %s: destination directory %s doesn't exist",
			(resetFailed ? "retry failed articles for" : reprocess ? "post-process again" : "download remaining files for"),
			historyInfo->GetName(), nzbInfo->GetDestDir());
		return;
	}

	nzbInfo->PrintMessage(Message::mkInfo, "%s %s",
		(resetFailed ? "Retrying failed articles for" : reprocess ? "Post-processing again" : "Downloading remaining files for"),
		nzbInfo->GetName());

	// move failed completed files to (parked) file list
	for (CompletedFileList::iterator it = nzbInfo->GetCompletedFiles()->begin(); it != nzbInfo->GetCompletedFiles()->end(); )
	{
		CompletedFile& completedFile = *it;
		if (completedFile.GetStatus() != CompletedFile::cfSuccess &&
			(completedFile.GetStatus() != CompletedFile::cfFailure || resetFailed) &&
			completedFile.GetId() > 0)
		{
			std::unique_ptr<FileInfo> fileInfo = std::make_unique<FileInfo>();
			fileInfo->SetId(completedFile.GetId());
			if (g_DiskState->LoadFile(fileInfo.get(), true, true) &&
				(completedFile.GetStatus() == CompletedFile::cfNone ||
				 (completedFile.GetStatus() == CompletedFile::cfFailure && resetFailed) ||
				 (completedFile.GetStatus() == CompletedFile::cfPartial &&
				  g_DiskState->LoadFileState(fileInfo.get(), g_ServerPool->GetServers(), true) &&
				  (resetFailed || fileInfo->GetRemainingSize() > 0))))
			{
				fileInfo->SetFilename(completedFile.GetFilename());
				fileInfo->SetNzbInfo(nzbInfo);

				BString<1024> outputFilename("%s%c%s", nzbInfo->GetDestDir(), PATH_SEPARATOR, fileInfo->GetFilename());

				if (fileInfo->GetSuccessArticles() == 0 || FileSystem::FileSize(outputFilename) == 0)
				{
					FileSystem::DeleteFile(outputFilename);
				}

				if (fileInfo->GetSuccessArticles() > 0)
				{
					if (FileSystem::FileExists(outputFilename))
					{
						fileInfo->SetPartialState(FileInfo::psCompleted);
					}
					else if (!reprocess)
					{
						nzbInfo->PrintMessage(Message::mkWarning, "File %s could not be found on disk, downloading again", fileInfo->GetFilename());
						fileInfo->SetPartialState(FileInfo::psNone);
					}
				}

				ResetArticles(fileInfo.get(), completedFile.GetStatus() == CompletedFile::cfFailure, resetFailed);

				g_DiskState->DiscardFile(fileInfo->GetId(), false, true, fileInfo->GetPartialState() != FileInfo::psCompleted);
				if (fileInfo->GetPartialState() == FileInfo::psCompleted)
				{
					g_DiskState->SaveFileState(fileInfo.get(), true);
				}
				fileInfo->GetArticles()->clear();

				nzbInfo->GetFileList()->Add(std::move(fileInfo), false);

				it = nzbInfo->GetCompletedFiles()->erase(it);
				continue;
			}
		}
		it++;
	}

	nzbInfo->UpdateCurrentStats();

	MoveToQueue(downloadQueue, itHistory, historyInfo, reprocess);

	if (g_Options->GetParCheck() != Options::pcForce)
	{
		downloadQueue->EditEntry(nzbInfo->GetId(), DownloadQueue::eaGroupPauseExtraPars, nullptr);
	}
}

void HistoryCoordinator::ResetArticles(FileInfo* fileInfo, bool allFailed, bool resetFailed)
{
	NzbInfo* nzbInfo = fileInfo->GetNzbInfo();

	if (allFailed)
	{
		fileInfo->SetFailedSize(fileInfo->GetSize() - fileInfo->GetMissedSize());
		fileInfo->SetFailedArticles(fileInfo->GetTotalArticles() - fileInfo->GetMissedArticles());
		fileInfo->SetRemainingSize(0);
		fileInfo->SetCompletedArticles(fileInfo->GetFailedArticles());
	}

	nzbInfo->GetServerStats()->ListOp(fileInfo->GetServerStats(), ServerStatList::soSubtract);

	nzbInfo->SetFailedSize(nzbInfo->GetFailedSize() - fileInfo->GetFailedSize());
	nzbInfo->SetSuccessSize(nzbInfo->GetSuccessSize() - fileInfo->GetSuccessSize());
	nzbInfo->SetFailedArticles(nzbInfo->GetFailedArticles() - fileInfo->GetFailedArticles());
	nzbInfo->SetSuccessArticles(nzbInfo->GetSuccessArticles() - fileInfo->GetSuccessArticles());

	if (fileInfo->GetParFile())
	{
		nzbInfo->SetParFailedSize(nzbInfo->GetParFailedSize() - fileInfo->GetFailedSize());
		nzbInfo->SetParSuccessSize(nzbInfo->GetParSuccessSize() - fileInfo->GetSuccessSize());
	}

	for (ArticleInfo* pa : fileInfo->GetArticles())
	{
		if ((pa->GetStatus() == ArticleInfo::aiFailed && (resetFailed || fileInfo->GetPartialState() == FileInfo::psNone)) ||
			(pa->GetStatus() == ArticleInfo::aiUndefined && resetFailed && allFailed) ||
			(pa->GetStatus() == ArticleInfo::aiFinished && fileInfo->GetPartialState() == FileInfo::psNone))
		{
			fileInfo->SetCompletedArticles(fileInfo->GetCompletedArticles() - 1);
			fileInfo->SetRemainingSize(fileInfo->GetRemainingSize() + pa->GetSize());

			if (pa->GetStatus() == ArticleInfo::aiFailed ||
				pa->GetStatus() == ArticleInfo::aiUndefined)
			{
				fileInfo->SetFailedArticles(fileInfo->GetFailedArticles() - 1);
				fileInfo->SetFailedSize(fileInfo->GetFailedSize() - pa->GetSize());
			}
			else if (pa->GetStatus() == ArticleInfo::aiFinished)
			{
				fileInfo->SetSuccessArticles(fileInfo->GetSuccessArticles() - 1);
				fileInfo->SetSuccessSize(fileInfo->GetSuccessSize() - pa->GetSize());
			}

			pa->SetStatus(ArticleInfo::aiUndefined);
			pa->SetCrc(0);
			pa->SetSegmentOffset(0);
			pa->SetSegmentSize(0);
		}
	}
}

bool HistoryCoordinator::HistorySetParameter(HistoryInfo* historyInfo, const char* text)
{
	debug("Setting post-process-parameter '%s' for '%s'", text, historyInfo->GetName());

	if (!(historyInfo->GetKind() == HistoryInfo::hkNzb || historyInfo->GetKind() == HistoryInfo::hkUrl))
	{
		error("Could not set post-process-parameter for %s: history item has wrong type", historyInfo->GetName());
		return false;
	}

	CString str = text;
	char* value = strchr(str, '=');
	if (value)
	{
		*value = '\0';
		value++;
		historyInfo->GetNzbInfo()->GetParameters()->SetParameter(str, value);
	}
	else
	{
		error("Could not set post-process-parameter for %s: invalid argument: %s", historyInfo->GetNzbInfo()->GetName(), text);
	}

	return true;
}

bool HistoryCoordinator::HistorySetCategory(HistoryInfo* historyInfo, const char* text)
{
	debug("Setting category '%s' for '%s'", text, historyInfo->GetName());

	if (!(historyInfo->GetKind() == HistoryInfo::hkNzb || historyInfo->GetKind() == HistoryInfo::hkUrl))
	{
		error("Could not set category for %s: history item has wrong type", historyInfo->GetName());
		return false;
	}

	historyInfo->GetNzbInfo()->SetCategory(text);

	return true;
}

bool HistoryCoordinator::HistorySetName(HistoryInfo* historyInfo, const char* text)
{
	debug("Setting name '%s' for '%s'", text, historyInfo->GetName());

	if (Util::EmptyStr(text))
	{
		error("Could not rename %s. The new name cannot be empty", historyInfo->GetName());
		return false;
	}

	if (historyInfo->GetKind() == HistoryInfo::hkNzb || historyInfo->GetKind() == HistoryInfo::hkUrl)
	{
		historyInfo->GetNzbInfo()->SetName(text);
	}
	else if (historyInfo->GetKind() == HistoryInfo::hkDup)
	{
		historyInfo->GetDupInfo()->SetName(text);
	}

	return true;
}

void HistoryCoordinator::HistorySetDupeParam(HistoryInfo* historyInfo, DownloadQueue::EEditAction action, const char* text)
{
	debug("Setting dupe-parameter '%i'='%s' for '%s'", (int)action, text, historyInfo->GetName());

	EDupeMode mode = dmScore;
	if (action == DownloadQueue::eaHistorySetDupeMode)
	{
		if (!strcasecmp(text, "SCORE"))
		{
			mode = dmScore;
		}
		else if (!strcasecmp(text, "ALL"))
		{
			mode = dmAll;
		}
		else if (!strcasecmp(text, "FORCE"))
		{
			mode = dmForce;
		}
		else
		{
			error("Could not set duplicate mode for %s: incorrect mode (%s)", historyInfo->GetName(), text);
			return;
		}
	}

	if (historyInfo->GetKind() == HistoryInfo::hkNzb || historyInfo->GetKind() == HistoryInfo::hkUrl)
	{
		switch (action)
		{
			case DownloadQueue::eaHistorySetDupeKey:
				historyInfo->GetNzbInfo()->SetDupeKey(text);
				break;

			case DownloadQueue::eaHistorySetDupeScore:
				historyInfo->GetNzbInfo()->SetDupeScore(atoi(text));
				break;

			case DownloadQueue::eaHistorySetDupeMode:
				historyInfo->GetNzbInfo()->SetDupeMode(mode);
				break;

			case DownloadQueue::eaHistorySetDupeBackup:
				if (historyInfo->GetKind() == HistoryInfo::hkUrl)
				{
					error("Could not set duplicate parameter for %s: history item has wrong type", historyInfo->GetName());
					return;
				}
				else if (historyInfo->GetNzbInfo()->GetDeleteStatus() != NzbInfo::dsDupe &&
					historyInfo->GetNzbInfo()->GetDeleteStatus() != NzbInfo::dsManual)
				{
					error("Could not set duplicate parameter for %s: history item has wrong delete status", historyInfo->GetName());
					return;
				}
				historyInfo->GetNzbInfo()->SetDeleteStatus(!strcasecmp(text, "YES") ||
					!strcasecmp(text, "TRUE") || !strcasecmp(text, "1") ? NzbInfo::dsDupe : NzbInfo::dsManual);
				break;

			default:
				// suppress compiler warning
				break;
		}
	}
	else if (historyInfo->GetKind() == HistoryInfo::hkDup)
	{
		switch (action)
		{
			case DownloadQueue::eaHistorySetDupeKey:
				historyInfo->GetDupInfo()->SetDupeKey(text);
				break;

			case DownloadQueue::eaHistorySetDupeScore:
				historyInfo->GetDupInfo()->SetDupeScore(atoi(text));
				break;

			case DownloadQueue::eaHistorySetDupeMode:
				historyInfo->GetDupInfo()->SetDupeMode(mode);
				break;

			case DownloadQueue::eaHistorySetDupeBackup:
				error("Could not set duplicate parameter for %s: history item has wrong type", historyInfo->GetName());
				return;

			default:
				// suppress compiler warning
				break;
		}
	}
}

void HistoryCoordinator::Redownload(DownloadQueue* downloadQueue, HistoryInfo* historyInfo)
{
	HistoryList::iterator it = downloadQueue->GetHistory()->Find(historyInfo);
	HistoryRedownload(downloadQueue, it, historyInfo, true);
}
