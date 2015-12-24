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
 * $Revision: 951 $
 * $Date$
 *
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

HistoryCoordinator::HistoryCoordinator()
{
	debug("Creating HistoryCoordinator");
}

HistoryCoordinator::~HistoryCoordinator()
{
	debug("Destroying HistoryCoordinator");
}

/**
 * Removes old entries from (recent) history
 */
void HistoryCoordinator::ServiceWork()
{
	DownloadQueue* downloadQueue = DownloadQueue::Lock();

	time_t minTime = time(NULL) - g_Options->GetKeepHistory() * 60*60*24;
	bool changed = false;
	int index = 0;

	// traversing in a reverse order to delete items in order they were added to history
	// (just to produce the log-messages in a more logical order)
	for (HistoryList::reverse_iterator it = downloadQueue->GetHistory()->rbegin(); it != downloadQueue->GetHistory()->rend(); )
	{
		HistoryInfo* historyInfo = *it;
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
				downloadQueue->GetHistory()->erase(downloadQueue->GetHistory()->end() - 1 - index);

				if (historyInfo->GetKind() == HistoryInfo::hkNzb)
				{
					DeleteDiskFiles(historyInfo->GetNzbInfo());
				}
				info("Collection %s removed from history", historyInfo->GetName());

				delete historyInfo;
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
		downloadQueue->Save();
	}

	DownloadQueue::Unlock();
}

void HistoryCoordinator::DeleteDiskFiles(NzbInfo* nzbInfo)
{
	if (g_Options->GetSaveQueue() && g_Options->GetServerMode())
	{
		// delete parked files
		g_DiskState->DiscardFiles(nzbInfo);
	}
	nzbInfo->GetFileList()->Clear();

	// delete nzb-file
	if (!g_Options->GetNzbCleanupDisk())
	{
		return;
	}

	// QueuedFile may contain one filename or several filenames separated
	// with "|"-character (for merged groups)
	char* filename = strdup(nzbInfo->GetQueuedFilename());
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

	free(filename);
}

void HistoryCoordinator::AddToHistory(DownloadQueue* downloadQueue, NzbInfo* nzbInfo)
{
	//remove old item for the same NZB
	for (HistoryList::iterator it = downloadQueue->GetHistory()->begin(); it != downloadQueue->GetHistory()->end(); it++)
	{
		HistoryInfo* historyInfo = *it;
		if (historyInfo->GetNzbInfo() == nzbInfo)
		{
			delete historyInfo;
			downloadQueue->GetHistory()->erase(it);
			break;
		}
	}

	HistoryInfo* historyInfo = new HistoryInfo(nzbInfo);
	historyInfo->SetTime(time(NULL));
	downloadQueue->GetHistory()->push_front(historyInfo);
	downloadQueue->GetQueue()->Remove(nzbInfo);

	if (nzbInfo->GetDeleteStatus() == NzbInfo::dsNone)
	{
		// park files and delete files marked for deletion
		int parkedFiles = 0;
		for (FileList::iterator it = nzbInfo->GetFileList()->begin(); it != nzbInfo->GetFileList()->end(); )
		{
			FileInfo* fileInfo = *it;
			if (!fileInfo->GetDeleted())
			{
				detail("Parking file %s", fileInfo->GetFilename());
				g_QueueCoordinator->DiscardDiskFile(fileInfo);
				parkedFiles++;
				it++;
			}
			else
			{
				// since we removed pNZBInfo from queue we need to take care of removing file infos marked for deletion
				nzbInfo->GetFileList()->erase(it);
				delete fileInfo;
				it = nzbInfo->GetFileList()->begin() + parkedFiles;
			}
		}
		nzbInfo->SetParkedFileCount(parkedFiles);
	}
	else
	{
		nzbInfo->GetFileList()->Clear();
	}

	nzbInfo->PrintMessage(Message::mkInfo, "Collection %s added to history", nzbInfo->GetName());
}

void HistoryCoordinator::HistoryHide(DownloadQueue* downloadQueue, HistoryInfo* historyInfo, int rindex)
{
	// replace history element
	DupInfo* dupInfo = new DupInfo();
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

	HistoryInfo* newHistoryInfo = new HistoryInfo(dupInfo);
	newHistoryInfo->SetTime(historyInfo->GetTime());
	(*downloadQueue->GetHistory())[downloadQueue->GetHistory()->size() - 1 - rindex] = newHistoryInfo;

	DeleteDiskFiles(historyInfo->GetNzbInfo());

	info("Collection %s removed from history", historyInfo->GetName());
	delete historyInfo;
}

void HistoryCoordinator::PrepareEdit(DownloadQueue* downloadQueue, IdList* idList, DownloadQueue::EEditAction action)
{
	// First pass: when marking multiple items - mark them bad without performing the mark-logic,
	// this will later (on second step) avoid moving other items to download queue, if they are marked bad too.
	if (action == DownloadQueue::eaHistoryMarkBad)
	{
		for (IdList::iterator itId = idList->begin(); itId != idList->end(); itId++)
		{
			int id = *itId;
			HistoryInfo* historyInfo = downloadQueue->GetHistory()->Find(id);
			if (historyInfo && historyInfo->GetKind() == HistoryInfo::hkNzb)
			{
				historyInfo->GetNzbInfo()->SetMarkStatus(NzbInfo::ksBad);
			}
		}
	}
}

bool HistoryCoordinator::EditList(DownloadQueue* downloadQueue, IdList* idList, DownloadQueue::EEditAction action, int offset, const char* text)
{
	bool ok = false;
	PrepareEdit(downloadQueue, idList, action);

	for (IdList::iterator itId = idList->begin(); itId != idList->end(); itId++)
	{
		int id = *itId;
		for (HistoryList::iterator itHistory = downloadQueue->GetHistory()->begin(); itHistory != downloadQueue->GetHistory()->end(); itHistory++)
		{
			HistoryInfo* historyInfo = *itHistory;
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
					case DownloadQueue::eaHistoryProcess:
						HistoryReturn(downloadQueue, itHistory, historyInfo, action == DownloadQueue::eaHistoryProcess);
						break;

					case DownloadQueue::eaHistoryRedownload:
						HistoryRedownload(downloadQueue, itHistory, historyInfo, false);
						break;

 					case DownloadQueue::eaHistorySetParameter:
						ok = HistorySetParameter(historyInfo, text);
						break;

 					case DownloadQueue::eaHistorySetCategory:
						ok = HistorySetCategory(historyInfo, text);
						break;

 					case DownloadQueue::eaHistorySetName:
						ok = HistorySetName(historyInfo, text);
						break;

					case DownloadQueue::eaHistorySetDupeKey:
					case DownloadQueue::eaHistorySetDupeScore:
					case DownloadQueue::eaHistorySetDupeMode:
					case DownloadQueue::eaHistorySetDupeBackup:
						HistorySetDupeParam(historyInfo, action, text);
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
		g_Options->GetDeleteCleanupDisk() &&
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
		delete historyInfo;
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

void HistoryCoordinator::HistoryReturn(DownloadQueue* downloadQueue, HistoryList::iterator itHistory, HistoryInfo* historyInfo, bool reprocess)
{
	debug("Returning %s from history back to download queue", historyInfo->GetName());
	NzbInfo* nzbInfo = NULL;
	CString nicename = historyInfo->GetName();

	if (reprocess && historyInfo->GetKind() != HistoryInfo::hkNzb)
	{
		error("Could not restart postprocessing for %s: history item has wrong type", historyInfo->GetName());
		return;
	}

	if (historyInfo->GetKind() == HistoryInfo::hkNzb)
	{
		nzbInfo = historyInfo->GetNzbInfo();

		// unpark files
		bool unparked = false;
		for (FileList::iterator it = nzbInfo->GetFileList()->begin(); it != nzbInfo->GetFileList()->end(); it++)
		{
			FileInfo* fileInfo = *it;
			detail("Unpark file %s", fileInfo->GetFilename());
			unparked = true;
		}

		if (!(unparked || reprocess))
		{
			warn("Could not return %s back from history to download queue: history item does not have any files left for download", historyInfo->GetName());
			return;
		}

		downloadQueue->GetQueue()->push_front(nzbInfo);
		historyInfo->DiscardNzbInfo();

		// reset postprocessing status variables
		nzbInfo->SetParCleanup(false);
		if (!nzbInfo->GetUnpackCleanedUpDisk())
		{
			nzbInfo->SetUnpackStatus(NzbInfo::usNone);
			nzbInfo->SetCleanupStatus(NzbInfo::csNone);
			nzbInfo->SetRenameStatus(NzbInfo::rsNone);
			nzbInfo->SetPostTotalSec(nzbInfo->GetPostTotalSec() - nzbInfo->GetUnpackSec());
			nzbInfo->SetUnpackSec(0);

			if (ParParser::FindMainPars(nzbInfo->GetDestDir(), NULL))
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
		nzbInfo->GetScriptStatuses()->Clear();
		nzbInfo->SetParkedFileCount(0);
		if (nzbInfo->GetMoveStatus() == NzbInfo::msFailure)
		{
			nzbInfo->SetMoveStatus(NzbInfo::msNone);
		}
		nzbInfo->SetReprocess(reprocess);
	}

	if (historyInfo->GetKind() == HistoryInfo::hkUrl)
	{
		nzbInfo = historyInfo->GetNzbInfo();
		historyInfo->DiscardNzbInfo();
		nzbInfo->SetUrlStatus(NzbInfo::lsNone);
		nzbInfo->SetDeleteStatus(NzbInfo::dsNone);
		downloadQueue->GetQueue()->push_front(nzbInfo);
	}

	downloadQueue->GetHistory()->erase(itHistory);
	// the object "pHistoryInfo" is released few lines later, after the call to "NZBDownloaded"
	nzbInfo->PrintMessage(Message::mkInfo, "%s returned from history back to download queue", *nicename);

	if (reprocess)
	{
		// start postprocessing
		debug("Restarting postprocessing for %s", *nicename);
		g_PrePostProcessor->NzbDownloaded(downloadQueue, nzbInfo);
	}

	delete historyInfo;
}

void HistoryCoordinator::HistoryRedownload(DownloadQueue* downloadQueue, HistoryList::iterator itHistory,
	HistoryInfo* historyInfo, bool restorePauseState)
{
	if (historyInfo->GetKind() == HistoryInfo::hkUrl)
	{
		HistoryReturn(downloadQueue, itHistory, historyInfo, false);
		return;
	}

	if (historyInfo->GetKind() != HistoryInfo::hkNzb)
	{
		error("Could not return %s from history back to queue: history item has wrong type", historyInfo->GetName());
		return;
	}

	NzbInfo* nzbInfo = historyInfo->GetNzbInfo();
	bool paused = restorePauseState && nzbInfo->GetDeletePaused();

	if (!FileSystem::FileExists(nzbInfo->GetQueuedFilename()))
	{
		error("Could not return %s from history back to queue: could not find source nzb-file %s",
			nzbInfo->GetName(), nzbInfo->GetQueuedFilename());
		return;
	}

	NzbFile* nzbFile = new NzbFile(nzbInfo->GetQueuedFilename(), "");
	if (!nzbFile->Parse())
	{
		error("Could not return %s from history back to queue: could not parse nzb-file",
			nzbInfo->GetName());
		delete nzbFile;
		return;
	}

	info("Returning %s from history back to queue", nzbInfo->GetName());

	for (FileList::iterator it = nzbFile->GetNzbInfo()->GetFileList()->begin(); it != nzbFile->GetNzbInfo()->GetFileList()->end(); it++)
	{
		FileInfo* fileInfo = *it;
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

	// reset status fields (which are not reset by "HistoryReturn")
	nzbInfo->SetMoveStatus(NzbInfo::msNone);
	nzbInfo->SetUnpackCleanedUpDisk(false);
	nzbInfo->SetParStatus(NzbInfo::psNone);
	nzbInfo->SetRenameStatus(NzbInfo::rsNone);
	nzbInfo->SetDownloadedSize(0);
	nzbInfo->SetDownloadSec(0);
	nzbInfo->SetPostTotalSec(0);
	nzbInfo->SetParSec(0);
	nzbInfo->SetRepairSec(0);
	nzbInfo->SetUnpackSec(0);
	nzbInfo->SetExtraParBlocks(0);
	nzbInfo->ClearCompletedFiles();
	nzbInfo->GetServerStats()->Clear();
	nzbInfo->GetCurrentServerStats()->Clear();

	nzbInfo->CopyFileList(nzbFile->GetNzbInfo());

	g_QueueCoordinator->CheckDupeFileInfos(nzbInfo);
	delete nzbFile;

	HistoryReturn(downloadQueue, itHistory, historyInfo, false);

	g_PrePostProcessor->NzbAdded(downloadQueue, nzbInfo);
}

bool HistoryCoordinator::HistorySetParameter(HistoryInfo* historyInfo, const char* text)
{
	debug("Setting post-process-parameter '%s' for '%s'", text, historyInfo->GetName());

	if (!(historyInfo->GetKind() == HistoryInfo::hkNzb || historyInfo->GetKind() == HistoryInfo::hkUrl))
	{
		error("Could not set post-process-parameter for %s: history item has wrong type", historyInfo->GetName());
		return false;
	}

	char* str = strdup(text);

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

	free(str);

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
	HistoryList::iterator it = std::find(downloadQueue->GetHistory()->begin(),
		downloadQueue->GetHistory()->end(), historyInfo);
	HistoryRedownload(downloadQueue, it, historyInfo, true);
}
