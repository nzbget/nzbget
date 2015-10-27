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
#include "HistoryCoordinator.h"
#include "Options.h"
#include "Log.h"
#include "QueueCoordinator.h"
#include "DiskState.h"
#include "Util.h"
#include "NZBFile.h"
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

	time_t minTime = time(NULL) - g_pOptions->GetKeepHistory() * 60*60*24;
	bool changed = false;
	int index = 0;

	// traversing in a reverse order to delete items in order they were added to history
	// (just to produce the log-messages in a more logical order)
	for (HistoryList::reverse_iterator it = downloadQueue->GetHistory()->rbegin(); it != downloadQueue->GetHistory()->rend(); )
	{
		HistoryInfo* historyInfo = *it;
		if (historyInfo->GetKind() != HistoryInfo::hkDup && historyInfo->GetTime() < minTime)
		{
			if (g_pOptions->GetDupeCheck() && historyInfo->GetKind() == HistoryInfo::hkNzb)
			{
				// replace history element
				HistoryHide(downloadQueue, historyInfo, index);
				index++;
			}
			else
			{
				char niceName[1024];
				historyInfo->GetName(niceName, 1024);

				downloadQueue->GetHistory()->erase(downloadQueue->GetHistory()->end() - 1 - index);
				
				if (historyInfo->GetKind() == HistoryInfo::hkNzb)
				{
					DeleteDiskFiles(historyInfo->GetNZBInfo());
				}
				info("Collection %s removed from history", niceName);

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

void HistoryCoordinator::DeleteDiskFiles(NZBInfo* nzbInfo)
{
	if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
	{
		// delete parked files
		g_pDiskState->DiscardFiles(nzbInfo);
	}
	nzbInfo->GetFileList()->Clear();

	// delete nzb-file
	if (!g_pOptions->GetNzbCleanupDisk())
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

		if (Util::FileExists(name1))
		{
			info("Deleting file %s", name1);
			remove(name1);
		}
	}

	free(filename);
}

void HistoryCoordinator::AddToHistory(DownloadQueue* downloadQueue, NZBInfo* nzbInfo)
{
	//remove old item for the same NZB
	for (HistoryList::iterator it = downloadQueue->GetHistory()->begin(); it != downloadQueue->GetHistory()->end(); it++)
	{
		HistoryInfo* historyInfo = *it;
		if (historyInfo->GetNZBInfo() == nzbInfo)
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

	if (nzbInfo->GetDeleteStatus() == NZBInfo::dsNone)
	{
		// park files and delete files marked for deletion
		int parkedFiles = 0;
		for (FileList::iterator it = nzbInfo->GetFileList()->begin(); it != nzbInfo->GetFileList()->end(); )
		{
			FileInfo* fileInfo = *it;
			if (!fileInfo->GetDeleted())
			{
				detail("Parking file %s", fileInfo->GetFilename());
				g_pQueueCoordinator->DiscardDiskFile(fileInfo);
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
	char niceName[1024];
	historyInfo->GetName(niceName, 1024);

	// replace history element
	DupInfo* dupInfo = new DupInfo();
	dupInfo->SetID(historyInfo->GetNZBInfo()->GetID());
	dupInfo->SetName(historyInfo->GetNZBInfo()->GetName());
	dupInfo->SetDupeKey(historyInfo->GetNZBInfo()->GetDupeKey());
	dupInfo->SetDupeScore(historyInfo->GetNZBInfo()->GetDupeScore());
	dupInfo->SetDupeMode(historyInfo->GetNZBInfo()->GetDupeMode());
	dupInfo->SetSize(historyInfo->GetNZBInfo()->GetSize());
	dupInfo->SetFullContentHash(historyInfo->GetNZBInfo()->GetFullContentHash());
	dupInfo->SetFilteredContentHash(historyInfo->GetNZBInfo()->GetFilteredContentHash());

	dupInfo->SetStatus(
		historyInfo->GetNZBInfo()->GetMarkStatus() == NZBInfo::ksGood ? DupInfo::dsGood :
		historyInfo->GetNZBInfo()->GetMarkStatus() == NZBInfo::ksBad ? DupInfo::dsBad :
		historyInfo->GetNZBInfo()->GetMarkStatus() == NZBInfo::ksSuccess ? DupInfo::dsSuccess :
		historyInfo->GetNZBInfo()->GetDeleteStatus() == NZBInfo::dsDupe ? DupInfo::dsDupe :
		historyInfo->GetNZBInfo()->GetDeleteStatus() == NZBInfo::dsManual ||
		historyInfo->GetNZBInfo()->GetDeleteStatus() == NZBInfo::dsGood ||
		historyInfo->GetNZBInfo()->GetDeleteStatus() == NZBInfo::dsCopy ? DupInfo::dsDeleted :
		historyInfo->GetNZBInfo()->IsDupeSuccess() ? DupInfo::dsSuccess :
		DupInfo::dsFailed);

	HistoryInfo* newHistoryInfo = new HistoryInfo(dupInfo);
	newHistoryInfo->SetTime(historyInfo->GetTime());
	(*downloadQueue->GetHistory())[downloadQueue->GetHistory()->size() - 1 - rindex] = newHistoryInfo;

	DeleteDiskFiles(historyInfo->GetNZBInfo());

	delete historyInfo;
	info("Collection %s removed from history", niceName);
}

void HistoryCoordinator::PrepareEdit(DownloadQueue* downloadQueue, IDList* idList, DownloadQueue::EEditAction action)
{
	// First pass: when marking multiple items - mark them bad without performing the mark-logic,
	// this will later (on second step) avoid moving other items to download queue, if they are marked bad too.
	if (action == DownloadQueue::eaHistoryMarkBad)
	{
		for (IDList::iterator itID = idList->begin(); itID != idList->end(); itID++)
		{
			int id = *itID;
			HistoryInfo* historyInfo = downloadQueue->GetHistory()->Find(id);
			if (historyInfo && historyInfo->GetKind() == HistoryInfo::hkNzb)
			{
				historyInfo->GetNZBInfo()->SetMarkStatus(NZBInfo::ksBad);
			}
		}
	}
}

bool HistoryCoordinator::EditList(DownloadQueue* downloadQueue, IDList* idList, DownloadQueue::EEditAction action, int offset, const char* text)
{
	bool ok = false;
	PrepareEdit(downloadQueue, idList, action);

	for (IDList::iterator itID = idList->begin(); itID != idList->end(); itID++)
	{
		int id = *itID;
		for (HistoryList::iterator itHistory = downloadQueue->GetHistory()->begin(); itHistory != downloadQueue->GetHistory()->end(); itHistory++)
		{
			HistoryInfo* historyInfo = *itHistory;
			if (historyInfo->GetID() == id)
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
						g_pDupeCoordinator->HistoryMark(downloadQueue, historyInfo, NZBInfo::ksBad);
						break;

					case DownloadQueue::eaHistoryMarkGood:
						g_pDupeCoordinator->HistoryMark(downloadQueue, historyInfo, NZBInfo::ksGood);
						break;

					case DownloadQueue::eaHistoryMarkSuccess:
						g_pDupeCoordinator->HistoryMark(downloadQueue, historyInfo, NZBInfo::ksSuccess);
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
	char niceName[1024];
	historyInfo->GetName(niceName, 1024);
	info("Deleting %s from history", niceName);

	if (historyInfo->GetKind() == HistoryInfo::hkNzb)
	{
		DeleteDiskFiles(historyInfo->GetNZBInfo());
	}

	if (historyInfo->GetKind() == HistoryInfo::hkNzb &&
		g_pOptions->GetDeleteCleanupDisk() &&
		(historyInfo->GetNZBInfo()->GetDeleteStatus() != NZBInfo::dsNone ||
		historyInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psFailure ||
		historyInfo->GetNZBInfo()->GetUnpackStatus() == NZBInfo::usFailure ||
		historyInfo->GetNZBInfo()->GetUnpackStatus() == NZBInfo::usPassword) &&
		Util::DirectoryExists(historyInfo->GetNZBInfo()->GetDestDir()))
	{
		info("Deleting %s", historyInfo->GetNZBInfo()->GetDestDir());
		char errBuf[256];
		if (!Util::DeleteDirectoryWithContent(historyInfo->GetNZBInfo()->GetDestDir(), errBuf, sizeof(errBuf)))
		{
			error("Could not delete directory %s: %s", historyInfo->GetNZBInfo()->GetDestDir(), errBuf);
		}
	}

	if (final || !g_pOptions->GetDupeCheck() || historyInfo->GetKind() == HistoryInfo::hkUrl)
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
	char niceName[1024];
	historyInfo->GetName(niceName, 1024);
	debug("Returning %s from history back to download queue", niceName);
	NZBInfo* nzbInfo = NULL;

	if (reprocess && historyInfo->GetKind() != HistoryInfo::hkNzb)
	{
		error("Could not restart postprocessing for %s: history item has wrong type", niceName);
		return;
	}

	if (historyInfo->GetKind() == HistoryInfo::hkNzb)
	{
		nzbInfo = historyInfo->GetNZBInfo();

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
			warn("Could not return %s back from history to download queue: history item does not have any files left for download", niceName);
			return;
		}

		downloadQueue->GetQueue()->push_front(nzbInfo);
		historyInfo->DiscardNZBInfo();

		// reset postprocessing status variables
		nzbInfo->SetParCleanup(false);
		if (!nzbInfo->GetUnpackCleanedUpDisk())
		{
			nzbInfo->SetUnpackStatus(NZBInfo::usNone);
			nzbInfo->SetCleanupStatus(NZBInfo::csNone);
			nzbInfo->SetRenameStatus(NZBInfo::rsNone);
			nzbInfo->SetPostTotalSec(nzbInfo->GetPostTotalSec() - nzbInfo->GetUnpackSec());
			nzbInfo->SetUnpackSec(0);

			if (ParParser::FindMainPars(nzbInfo->GetDestDir(), NULL))
			{
				nzbInfo->SetParStatus(NZBInfo::psNone);
				nzbInfo->SetPostTotalSec(nzbInfo->GetPostTotalSec() - nzbInfo->GetParSec());
				nzbInfo->SetParSec(0);
				nzbInfo->SetRepairSec(0);
				nzbInfo->SetParFull(false);
			}
		}
		nzbInfo->SetDeleteStatus(NZBInfo::dsNone);
		nzbInfo->SetDeletePaused(false);
		nzbInfo->SetMarkStatus(NZBInfo::ksNone);
		nzbInfo->GetScriptStatuses()->Clear();
		nzbInfo->SetParkedFileCount(0);
		if (nzbInfo->GetMoveStatus() == NZBInfo::msFailure)
		{
			nzbInfo->SetMoveStatus(NZBInfo::msNone);
		}
		nzbInfo->SetReprocess(reprocess);
	}

	if (historyInfo->GetKind() == HistoryInfo::hkUrl)
	{
		nzbInfo = historyInfo->GetNZBInfo();
		historyInfo->DiscardNZBInfo();
		nzbInfo->SetUrlStatus(NZBInfo::lsNone);
		nzbInfo->SetDeleteStatus(NZBInfo::dsNone);
		downloadQueue->GetQueue()->push_front(nzbInfo);
	}

	downloadQueue->GetHistory()->erase(itHistory);
	// the object "pHistoryInfo" is released few lines later, after the call to "NZBDownloaded"
	nzbInfo->PrintMessage(Message::mkInfo, "%s returned from history back to download queue", niceName);

	if (reprocess)
	{
		// start postprocessing
		debug("Restarting postprocessing for %s", niceName);
		g_pPrePostProcessor->NZBDownloaded(downloadQueue, nzbInfo);
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
		char niceName[1024];
		historyInfo->GetName(niceName, 1024);
		error("Could not return %s from history back to queue: history item has wrong type", niceName);
		return;
	}

	NZBInfo* nzbInfo = historyInfo->GetNZBInfo();
	bool paused = restorePauseState && nzbInfo->GetDeletePaused();

	if (!Util::FileExists(nzbInfo->GetQueuedFilename()))
	{
		error("Could not return %s from history back to queue: could not find source nzb-file %s",
			nzbInfo->GetName(), nzbInfo->GetQueuedFilename());
		return;
	}

	NZBFile* nzbFile = new NZBFile(nzbInfo->GetQueuedFilename(), "");
	if (!nzbFile->Parse())
	{
		error("Could not return %s from history back to queue: could not parse nzb-file",
			nzbInfo->GetName());
		delete nzbFile;
		return;
	}

	info("Returning %s from history back to queue", nzbInfo->GetName());

	for (FileList::iterator it = nzbFile->GetNZBInfo()->GetFileList()->begin(); it != nzbFile->GetNZBInfo()->GetFileList()->end(); it++)
	{
		FileInfo* fileInfo = *it;
		fileInfo->SetPaused(paused);
	}

	if (Util::DirectoryExists(nzbInfo->GetDestDir()))
	{
		detail("Deleting %s", nzbInfo->GetDestDir());
		char errBuf[256];
		if (!Util::DeleteDirectoryWithContent(nzbInfo->GetDestDir(), errBuf, sizeof(errBuf)))
		{
			error("Could not delete directory %s: %s", nzbInfo->GetDestDir(), errBuf);
		}
	}

	nzbInfo->BuildDestDirName();
	if (Util::DirectoryExists(nzbInfo->GetDestDir()))
	{
		detail("Deleting %s", nzbInfo->GetDestDir());
		char errBuf[256];
		if (!Util::DeleteDirectoryWithContent(nzbInfo->GetDestDir(), errBuf, sizeof(errBuf)))
		{
			error("Could not delete directory %s: %s", nzbInfo->GetDestDir(), errBuf);
		}
	}

	g_pDiskState->DiscardFiles(nzbInfo);

	// reset status fields (which are not reset by "HistoryReturn")
	nzbInfo->SetMoveStatus(NZBInfo::msNone);
	nzbInfo->SetUnpackCleanedUpDisk(false);
	nzbInfo->SetParStatus(NZBInfo::psNone);
	nzbInfo->SetRenameStatus(NZBInfo::rsNone);
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

	nzbInfo->CopyFileList(nzbFile->GetNZBInfo());

	g_pQueueCoordinator->CheckDupeFileInfos(nzbInfo);
	delete nzbFile;

	HistoryReturn(downloadQueue, itHistory, historyInfo, false);

	g_pPrePostProcessor->NZBAdded(downloadQueue, nzbInfo);
}

bool HistoryCoordinator::HistorySetParameter(HistoryInfo* historyInfo, const char* text)
{
	char niceName[1024];
	historyInfo->GetName(niceName, 1024);
	debug("Setting post-process-parameter '%s' for '%s'", text, niceName);

	if (!(historyInfo->GetKind() == HistoryInfo::hkNzb || historyInfo->GetKind() == HistoryInfo::hkUrl))
	{
		error("Could not set post-process-parameter for %s: history item has wrong type", niceName);
		return false;
	}

	char* str = strdup(text);

	char* value = strchr(str, '=');
	if (value)
	{
		*value = '\0';
		value++;
		historyInfo->GetNZBInfo()->GetParameters()->SetParameter(str, value);
	}
	else
	{
		error("Could not set post-process-parameter for %s: invalid argument: %s", historyInfo->GetNZBInfo()->GetName(), text);
	}

	free(str);

	return true;
}

bool HistoryCoordinator::HistorySetCategory(HistoryInfo* historyInfo, const char* text)
{
	char niceName[1024];
	historyInfo->GetName(niceName, 1024);
	debug("Setting category '%s' for '%s'", text, niceName);

	if (!(historyInfo->GetKind() == HistoryInfo::hkNzb || historyInfo->GetKind() == HistoryInfo::hkUrl))
	{
		error("Could not set category for %s: history item has wrong type", niceName);
		return false;
	}

	historyInfo->GetNZBInfo()->SetCategory(text);

	return true;
}

bool HistoryCoordinator::HistorySetName(HistoryInfo* historyInfo, const char* text)
{
	char niceName[1024];
	historyInfo->GetName(niceName, 1024);
	debug("Setting name '%s' for '%s'", text, niceName);

	if (Util::EmptyStr(text))
	{
		error("Could not rename %s. The new name cannot be empty", niceName);
		return false;
	}

	if (historyInfo->GetKind() == HistoryInfo::hkNzb || historyInfo->GetKind() == HistoryInfo::hkUrl)
	{
		historyInfo->GetNZBInfo()->SetName(text);
	}
	else if (historyInfo->GetKind() == HistoryInfo::hkDup)
	{
		historyInfo->GetDupInfo()->SetName(text);
	}

	return true;
}

void HistoryCoordinator::HistorySetDupeParam(HistoryInfo* historyInfo, DownloadQueue::EEditAction action, const char* text)
{
	char niceName[1024];
	historyInfo->GetName(niceName, 1024);
	debug("Setting dupe-parameter '%i'='%s' for '%s'", (int)action, text, niceName);

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
			error("Could not set duplicate mode for %s: incorrect mode (%s)", niceName, text);
			return;
		}
	}

	if (historyInfo->GetKind() == HistoryInfo::hkNzb || historyInfo->GetKind() == HistoryInfo::hkUrl)
	{
		switch (action) 
		{
			case DownloadQueue::eaHistorySetDupeKey:
				historyInfo->GetNZBInfo()->SetDupeKey(text);
				break;

			case DownloadQueue::eaHistorySetDupeScore:
				historyInfo->GetNZBInfo()->SetDupeScore(atoi(text));
				break;

			case DownloadQueue::eaHistorySetDupeMode:
				historyInfo->GetNZBInfo()->SetDupeMode(mode);
				break;

			case DownloadQueue::eaHistorySetDupeBackup:
				if (historyInfo->GetKind() == HistoryInfo::hkUrl)
				{
					error("Could not set duplicate parameter for %s: history item has wrong type", niceName);
					return;
				}
				else if (historyInfo->GetNZBInfo()->GetDeleteStatus() != NZBInfo::dsDupe &&
					historyInfo->GetNZBInfo()->GetDeleteStatus() != NZBInfo::dsManual)
				{
					error("Could not set duplicate parameter for %s: history item has wrong delete status", niceName);
					return;
				}
				historyInfo->GetNZBInfo()->SetDeleteStatus(!strcasecmp(text, "YES") ||
					!strcasecmp(text, "TRUE") || !strcasecmp(text, "1") ? NZBInfo::dsDupe : NZBInfo::dsManual);
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
				error("Could not set duplicate parameter for %s: history item has wrong type", niceName);
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
