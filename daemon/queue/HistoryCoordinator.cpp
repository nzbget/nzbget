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
#include "ParCoordinator.h"
#include "PrePostProcessor.h"
#include "DupeCoordinator.h"

extern QueueCoordinator* g_pQueueCoordinator;
extern PrePostProcessor* g_pPrePostProcessor;
extern DupeCoordinator* g_pDupeCoordinator;
extern Options* g_pOptions;
extern DiskState* g_pDiskState;

HistoryCoordinator::HistoryCoordinator()
{
	debug("Creating HistoryCoordinator");
}

HistoryCoordinator::~HistoryCoordinator()
{
	debug("Destroying HistoryCoordinator");
}

void HistoryCoordinator::Cleanup()
{
	debug("Cleaning up HistoryCoordinator");

	DownloadQueue* pDownloadQueue = DownloadQueue::Lock();

	for (HistoryList::iterator it = pDownloadQueue->GetHistory()->begin(); it != pDownloadQueue->GetHistory()->end(); it++)
	{
		delete *it;
	}
	pDownloadQueue->GetHistory()->clear();

	DownloadQueue::Unlock();
}

/**
 * Removes old entries from (recent) history
 */
void HistoryCoordinator::IntervalCheck()
{
	DownloadQueue* pDownloadQueue = DownloadQueue::Lock();

	time_t tMinTime = time(NULL) - g_pOptions->GetKeepHistory() * 60*60*24;
	bool bChanged = false;
	int index = 0;

	// traversing in a reverse order to delete items in order they were added to history
	// (just to produce the log-messages in a more logical order)
	for (HistoryList::reverse_iterator it = pDownloadQueue->GetHistory()->rbegin(); it != pDownloadQueue->GetHistory()->rend(); )
	{
		HistoryInfo* pHistoryInfo = *it;
		if (pHistoryInfo->GetKind() != HistoryInfo::hkDup && pHistoryInfo->GetTime() < tMinTime)
		{
			if (g_pOptions->GetDupeCheck() && pHistoryInfo->GetKind() == HistoryInfo::hkNzb)
			{
				// replace history element
				HistoryHide(pDownloadQueue, pHistoryInfo, index);
				index++;
			}
			else
			{
				char szNiceName[1024];
				pHistoryInfo->GetName(szNiceName, 1024);

				pDownloadQueue->GetHistory()->erase(pDownloadQueue->GetHistory()->end() - 1 - index);
				
				if (pHistoryInfo->GetKind() == HistoryInfo::hkNzb)
				{
					DeleteQueuedFile(pHistoryInfo->GetNZBInfo()->GetQueuedFilename());
				}
				info("Collection %s removed from history", szNiceName);

				delete pHistoryInfo;
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
		pDownloadQueue->Save();
	}

	DownloadQueue::Unlock();
}

void HistoryCoordinator::DeleteQueuedFile(const char* szQueuedFile)
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

void HistoryCoordinator::AddToHistory(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
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
}

void HistoryCoordinator::HistoryHide(DownloadQueue* pDownloadQueue, HistoryInfo* pHistoryInfo, int rindex)
{
	char szNiceName[1024];
	pHistoryInfo->GetName(szNiceName, 1024);

	// replace history element
	DupInfo* pDupInfo = new DupInfo();
	pDupInfo->SetID(pHistoryInfo->GetNZBInfo()->GetID());
	pDupInfo->SetName(pHistoryInfo->GetNZBInfo()->GetName());
	pDupInfo->SetDupeKey(pHistoryInfo->GetNZBInfo()->GetDupeKey());
	pDupInfo->SetDupeScore(pHistoryInfo->GetNZBInfo()->GetDupeScore());
	pDupInfo->SetDupeMode(pHistoryInfo->GetNZBInfo()->GetDupeMode());
	pDupInfo->SetSize(pHistoryInfo->GetNZBInfo()->GetSize());
	pDupInfo->SetFullContentHash(pHistoryInfo->GetNZBInfo()->GetFullContentHash());
	pDupInfo->SetFilteredContentHash(pHistoryInfo->GetNZBInfo()->GetFilteredContentHash());

	pDupInfo->SetStatus(
		pHistoryInfo->GetNZBInfo()->GetMarkStatus() == NZBInfo::ksGood ? DupInfo::dsGood :
		pHistoryInfo->GetNZBInfo()->GetMarkStatus() == NZBInfo::ksBad ? DupInfo::dsBad :
		pHistoryInfo->GetNZBInfo()->GetDeleteStatus() == NZBInfo::dsDupe ? DupInfo::dsDupe :
		pHistoryInfo->GetNZBInfo()->GetDeleteStatus() == NZBInfo::dsManual ? DupInfo::dsDeleted :
		pHistoryInfo->GetNZBInfo()->IsDupeSuccess() ? DupInfo::dsSuccess :
		DupInfo::dsFailed);

	HistoryInfo* pNewHistoryInfo = new HistoryInfo(pDupInfo);
	pNewHistoryInfo->SetTime(pHistoryInfo->GetTime());
	(*pDownloadQueue->GetHistory())[pDownloadQueue->GetHistory()->size() - 1 - rindex] = pNewHistoryInfo;

	DeleteQueuedFile(pHistoryInfo->GetNZBInfo()->GetQueuedFilename());

	delete pHistoryInfo;
	info("Collection %s removed from history", szNiceName);
}

bool HistoryCoordinator::EditList(DownloadQueue* pDownloadQueue, IDList* pIDList, DownloadQueue::EEditAction eAction, int iOffset, const char* szText)
{
	bool bOK = false;

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
					case DownloadQueue::eaHistoryDelete:
					case DownloadQueue::eaHistoryFinalDelete:
						HistoryDelete(pDownloadQueue, itHistory, pHistoryInfo, eAction == DownloadQueue::eaHistoryFinalDelete);
						break;

					case DownloadQueue::eaHistoryReturn:
					case DownloadQueue::eaHistoryProcess:
						HistoryReturn(pDownloadQueue, itHistory, pHistoryInfo, eAction == DownloadQueue::eaHistoryProcess);
						break;

					case DownloadQueue::eaHistoryRedownload:
						HistoryRedownload(pDownloadQueue, itHistory, pHistoryInfo, false);
						break;

 					case DownloadQueue::eaHistorySetParameter:
						HistorySetParameter(pHistoryInfo, szText);
						break;

					case DownloadQueue::eaHistorySetDupeKey:
					case DownloadQueue::eaHistorySetDupeScore:
					case DownloadQueue::eaHistorySetDupeMode:
					case DownloadQueue::eaHistorySetDupeBackup:
						HistorySetDupeParam(pHistoryInfo, eAction, szText);
						break;

					case DownloadQueue::eaHistoryMarkBad:
					case DownloadQueue::eaHistoryMarkGood:
						g_pDupeCoordinator->HistoryMark(pDownloadQueue, pHistoryInfo, eAction == DownloadQueue::eaHistoryMarkGood);
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
		pDownloadQueue->Save();
	}

	return bOK;
}

void HistoryCoordinator::HistoryDelete(DownloadQueue* pDownloadQueue, HistoryList::iterator itHistory,
	HistoryInfo* pHistoryInfo, bool bFinal)
{
	char szNiceName[1024];
	pHistoryInfo->GetName(szNiceName, 1024);
	info("Deleting %s from history", szNiceName);

	if (pHistoryInfo->GetKind() == HistoryInfo::hkNzb)
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

	if (pHistoryInfo->GetKind() == HistoryInfo::hkNzb &&
		g_pOptions->GetDeleteCleanupDisk() &&
		(pHistoryInfo->GetNZBInfo()->GetDeleteStatus() != NZBInfo::dsNone ||
		pHistoryInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psFailure ||
		pHistoryInfo->GetNZBInfo()->GetUnpackStatus() == NZBInfo::usFailure ||
		pHistoryInfo->GetNZBInfo()->GetUnpackStatus() == NZBInfo::usPassword) &&
		Util::DirectoryExists(pHistoryInfo->GetNZBInfo()->GetDestDir()))
	{
		info("Deleting %s", pHistoryInfo->GetNZBInfo()->GetDestDir());
		char szErrBuf[256];
		if (!Util::DeleteDirectoryWithContent(pHistoryInfo->GetNZBInfo()->GetDestDir(), szErrBuf, sizeof(szErrBuf)))
		{
			error("Could not delete directory %s: %s", pHistoryInfo->GetNZBInfo()->GetDestDir(), szErrBuf);
		}
	}

	if (bFinal || !g_pOptions->GetDupeCheck() || pHistoryInfo->GetKind() == HistoryInfo::hkUrl)
	{
		pDownloadQueue->GetHistory()->erase(itHistory);
		delete pHistoryInfo;
	}
	else
	{
		if (pHistoryInfo->GetKind() == HistoryInfo::hkNzb)
		{
			// replace history element
			int rindex = pDownloadQueue->GetHistory()->size() - 1 - (itHistory - pDownloadQueue->GetHistory()->begin());
			HistoryHide(pDownloadQueue, pHistoryInfo, rindex);
		}
	}
}

void HistoryCoordinator::HistoryReturn(DownloadQueue* pDownloadQueue, HistoryList::iterator itHistory, HistoryInfo* pHistoryInfo, bool bReprocess)
{
	char szNiceName[1024];
	pHistoryInfo->GetName(szNiceName, 1024);
	debug("Returning %s from history back to download queue", szNiceName);
	bool bUnparked = false;
	NZBInfo* pNZBInfo = NULL;

	if (bReprocess && pHistoryInfo->GetKind() != HistoryInfo::hkNzb)
	{
		error("Could not restart postprocessing for %s: history item has wrong type", szNiceName);
		return;
	}

	if (pHistoryInfo->GetKind() == HistoryInfo::hkNzb)
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
			pNZBInfo->SetPostTotalSec(pNZBInfo->GetPostTotalSec() - pNZBInfo->GetUnpackSec());
			pNZBInfo->SetUnpackSec(0);

			if (ParCoordinator::FindMainPars(pNZBInfo->GetDestDir(), NULL))
			{
				pNZBInfo->SetParStatus(NZBInfo::psNone);
				pNZBInfo->SetPostTotalSec(pNZBInfo->GetPostTotalSec() - pNZBInfo->GetParSec());
				pNZBInfo->SetParSec(0);
				pNZBInfo->SetRepairSec(0);
			}
		}
		pNZBInfo->SetDeleteStatus(NZBInfo::dsNone);
		pNZBInfo->SetDeletePaused(false);
		pNZBInfo->SetMarkStatus(NZBInfo::ksNone);
		pNZBInfo->GetScriptStatuses()->Clear();
		pNZBInfo->SetParkedFileCount(0);
		if (pNZBInfo->GetMoveStatus() == NZBInfo::msFailure)
		{
			pNZBInfo->SetMoveStatus(NZBInfo::msNone);
		}
	}

	if (pHistoryInfo->GetKind() == HistoryInfo::hkUrl)
	{
		NZBInfo* pNZBInfo = pHistoryInfo->GetNZBInfo();
		pHistoryInfo->DiscardNZBInfo();
		pNZBInfo->SetUrlStatus(NZBInfo::lsNone);
		pNZBInfo->SetDeleteStatus(NZBInfo::dsNone);
		pDownloadQueue->GetQueue()->push_front(pNZBInfo);
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
		g_pPrePostProcessor->NZBDownloaded(pDownloadQueue, pNZBInfo);
	}

	if (bUnparked || bReprocess)
	{
		delete pHistoryInfo;
	}
}

void HistoryCoordinator::HistoryRedownload(DownloadQueue* pDownloadQueue, HistoryList::iterator itHistory,
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
		char szErrBuf[256];
		if (!Util::DeleteDirectoryWithContent(pNZBInfo->GetDestDir(), szErrBuf, sizeof(szErrBuf)))
		{
			error("Could not delete directory %s: %s", pNZBInfo->GetDestDir(), szErrBuf);
		}
	}

	pNZBInfo->BuildDestDirName();
	if (Util::DirectoryExists(pNZBInfo->GetDestDir()))
	{
		detail("Deleting %s", pNZBInfo->GetDestDir());
		char szErrBuf[256];
		if (!Util::DeleteDirectoryWithContent(pNZBInfo->GetDestDir(), szErrBuf, sizeof(szErrBuf)))
		{
			error("Could not delete directory %s: %s", pNZBInfo->GetDestDir(), szErrBuf);
		}
	}

	// reset status fields (which are not reset by "HistoryReturn")
	pNZBInfo->SetMoveStatus(NZBInfo::msNone);
	pNZBInfo->SetUnpackCleanedUpDisk(false);
	pNZBInfo->SetParStatus(NZBInfo::psNone);
	pNZBInfo->SetRenameStatus(NZBInfo::rsNone);
	pNZBInfo->SetDownloadedSize(0);
	pNZBInfo->SetDownloadSec(0);
	pNZBInfo->ClearCompletedFiles();
	pNZBInfo->GetServerStats()->Clear();
	pNZBInfo->GetCurrentServerStats()->Clear();

	pNZBInfo->CopyFileList(pNZBFile->GetNZBInfo());

	g_pQueueCoordinator->CheckDupeFileInfos(pNZBInfo);
	delete pNZBFile;

	HistoryReturn(pDownloadQueue, itHistory, pHistoryInfo, false);

	if (!bPaused && g_pOptions->GetParCheck() != Options::pcForce)
	{
		pDownloadQueue->EditEntry(pNZBInfo->GetID(), 
			DownloadQueue::eaGroupPauseExtraPars, 0, NULL);
	}
}

void HistoryCoordinator::HistorySetParameter(HistoryInfo* pHistoryInfo, const char* szText)
{
	char szNiceName[1024];
	pHistoryInfo->GetName(szNiceName, 1024);
	debug("Setting post-process-parameter '%s' for '%s'", szText, szNiceName);

	if (!(pHistoryInfo->GetKind() == HistoryInfo::hkNzb || pHistoryInfo->GetKind() == HistoryInfo::hkUrl))
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

void HistoryCoordinator::HistorySetDupeParam(HistoryInfo* pHistoryInfo, DownloadQueue::EEditAction eAction, const char* szText)
{
	char szNiceName[1024];
	pHistoryInfo->GetName(szNiceName, 1024);
	debug("Setting dupe-parameter '%i'='%s' for '%s'", (int)eAction, szText, szNiceName);

	EDupeMode eMode = dmScore;
	if (eAction == DownloadQueue::eaHistorySetDupeMode)
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

	if (pHistoryInfo->GetKind() == HistoryInfo::hkNzb || pHistoryInfo->GetKind() == HistoryInfo::hkUrl)
	{
		switch (eAction) 
		{
			case DownloadQueue::eaHistorySetDupeKey:
				pHistoryInfo->GetNZBInfo()->SetDupeKey(szText);
				break;

			case DownloadQueue::eaHistorySetDupeScore:
				pHistoryInfo->GetNZBInfo()->SetDupeScore(atoi(szText));
				break;

			case DownloadQueue::eaHistorySetDupeMode:
				pHistoryInfo->GetNZBInfo()->SetDupeMode(eMode);
				break;

			case DownloadQueue::eaHistorySetDupeBackup:
				if (pHistoryInfo->GetKind() == HistoryInfo::hkUrl)
				{
					error("Could not set duplicate parameter for %s: history item has wrong type", szNiceName);
					return;
				}
				else if (pHistoryInfo->GetNZBInfo()->GetDeleteStatus() != NZBInfo::dsDupe &&
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
	else if (pHistoryInfo->GetKind() == HistoryInfo::hkDup)
	{
		switch (eAction) 
		{
			case DownloadQueue::eaHistorySetDupeKey:
				pHistoryInfo->GetDupInfo()->SetDupeKey(szText);
				break;

			case DownloadQueue::eaHistorySetDupeScore:
				pHistoryInfo->GetDupInfo()->SetDupeScore(atoi(szText));
				break;

			case DownloadQueue::eaHistorySetDupeMode:
				pHistoryInfo->GetDupInfo()->SetDupeMode(eMode);
				break;

			case DownloadQueue::eaHistorySetDupeBackup:
				error("Could not set duplicate parameter for %s: history item has wrong type", szNiceName);
				return;

			default:
				// suppress compiler warning
				break;
		}
	}
}

void HistoryCoordinator::Redownload(DownloadQueue* pDownloadQueue, HistoryInfo* pHistoryInfo)
{
	HistoryList::iterator it = std::find(pDownloadQueue->GetHistory()->begin(),
		pDownloadQueue->GetHistory()->end(), pHistoryInfo);
	HistoryRedownload(pDownloadQueue, it, pHistoryInfo, true);
}
