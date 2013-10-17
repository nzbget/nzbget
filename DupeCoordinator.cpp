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
#include <set>
#include <algorithm>

#include "nzbget.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"
#include "DiskState.h"
#include "NZBFile.h"
#include "QueueCoordinator.h"
#include "DupeCoordinator.h"

extern QueueCoordinator* g_pQueueCoordinator;
extern Options* g_pOptions;
extern DiskState* g_pDiskState;

bool DupeCoordinator::IsDupeSuccess(NZBInfo* pNZBInfo)
{
	bool bFailure =
		pNZBInfo->GetDeleteStatus() != NZBInfo::dsNone ||
		pNZBInfo->GetMarkStatus() == NZBInfo::ksBad ||
		pNZBInfo->GetParStatus() == NZBInfo::psFailure ||
		pNZBInfo->GetUnpackStatus() == NZBInfo::usFailure ||
		(pNZBInfo->GetParStatus() == NZBInfo::psSkipped &&
		 pNZBInfo->GetUnpackStatus() == NZBInfo::usSkipped &&
		 pNZBInfo->CalcHealth() < pNZBInfo->CalcCriticalHealth());
	return !bFailure;
}

bool DupeCoordinator::SameNameOrKey(const char* szName1, const char* szDupeKey1,
	const char* szName2, const char* szDupeKey2)
{
	bool bHasDupeKeys = !Util::EmptyStr(szDupeKey1) && !Util::EmptyStr(szDupeKey2);
	return (bHasDupeKeys && !strcmp(szDupeKey1, szDupeKey2)) ||
		(!bHasDupeKeys && !strcmp(szName1, szName2));
}

/**
  Check if the title was already downloaded or is already queued.
*/
void DupeCoordinator::NZBFound(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	debug("Checking duplicates for %s", pNZBInfo->GetName());

	// find duplicates in download queue having exactly same content
	GroupQueue groupQueue;
	pDownloadQueue->BuildGroups(&groupQueue);

	for (GroupQueue::iterator it = groupQueue.begin(); it != groupQueue.end(); it++)
	{
		GroupInfo* pGroupInfo = *it;
		NZBInfo* pGroupNZBInfo = pGroupInfo->GetNZBInfo();
		bool bSameContent = (pNZBInfo->GetFullContentHash() > 0 &&
			pNZBInfo->GetFullContentHash() == pGroupNZBInfo->GetFullContentHash()) ||
			(pNZBInfo->GetFilteredContentHash() > 0 &&
			 pNZBInfo->GetFilteredContentHash() == pGroupNZBInfo->GetFilteredContentHash());
		if (pGroupNZBInfo != pNZBInfo && bSameContent)
		{
			if (!strcmp(pNZBInfo->GetName(), pGroupNZBInfo->GetName()))
			{
				warn("Skipping duplicate %s, already queued", pNZBInfo->GetName());
			}
			else
			{
				warn("Skipping duplicate %s, already queued as %s",
					pNZBInfo->GetName(), pGroupNZBInfo->GetName());
			}
			pNZBInfo->SetDeleteStatus(NZBInfo::dsManual); // Flag saying QueueCoordinator to skip nzb-file
			DeleteQueuedFile(pNZBInfo->GetQueuedFilename());
			return;
		}
	}

	// find duplicates in post queue having exactly same content
	for (PostQueue::iterator it = pDownloadQueue->GetPostQueue()->begin(); it != pDownloadQueue->GetPostQueue()->end(); it++)
	{
		PostInfo* pPostInfo = *it;
		bool bSameContent = (pNZBInfo->GetFullContentHash() > 0 &&
			pNZBInfo->GetFullContentHash() == pPostInfo->GetNZBInfo()->GetFullContentHash()) ||
			(pNZBInfo->GetFilteredContentHash() > 0 &&
			 pNZBInfo->GetFilteredContentHash() == pPostInfo->GetNZBInfo()->GetFilteredContentHash());
		if (bSameContent)
		{
			if (!strcmp(pNZBInfo->GetName(), pPostInfo->GetNZBInfo()->GetName()))
			{
				warn("Skipping duplicate %s, already queued", pNZBInfo->GetName());
			}
			else
			{
				warn("Skipping duplicate %s, already queued as %s",
					pNZBInfo->GetName(), pPostInfo->GetNZBInfo()->GetName());
			}
			pNZBInfo->SetDeleteStatus(NZBInfo::dsManual); // Flag saying QueueCoordinator to skip nzb-file
			DeleteQueuedFile(pNZBInfo->GetQueuedFilename());
			return;
		}
	}

	// find duplicates in history

	bool bSkip = false;
	bool bGood = false;
	bool bSameContent = false;
	const char* szDupeName = NULL;

	// find duplicates in queue having exactly same content
	// also: nzb-files having duplicates marked as good are skipped
	// also (only in score mode): nzb-files having success-duplicates in dup-history but don't having duplicates in recent history are skipped
	for (HistoryList::iterator it = pDownloadQueue->GetHistoryList()->begin(); it != pDownloadQueue->GetHistoryList()->end(); it++)
	{
		HistoryInfo* pHistoryInfo = *it;

		if (pHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo &&
			((pNZBInfo->GetFullContentHash() > 0 &&
			pNZBInfo->GetFullContentHash() == pHistoryInfo->GetNZBInfo()->GetFullContentHash()) ||
			(pNZBInfo->GetFilteredContentHash() > 0 &&
			 pNZBInfo->GetFilteredContentHash() == pHistoryInfo->GetNZBInfo()->GetFilteredContentHash())))
		{
			bSkip = true;
			bSameContent = true;
			szDupeName = pHistoryInfo->GetNZBInfo()->GetName();
			break;
		}

		if (pHistoryInfo->GetKind() == HistoryInfo::hkDupInfo &&
			((pNZBInfo->GetFullContentHash() > 0 &&
			  pNZBInfo->GetFullContentHash() == pHistoryInfo->GetDupInfo()->GetFullContentHash()) ||
			 (pNZBInfo->GetFilteredContentHash() > 0 &&
			  pNZBInfo->GetFilteredContentHash() == pHistoryInfo->GetDupInfo()->GetFilteredContentHash())))
		{
			bSkip = true;
			bSameContent = true;
			szDupeName = pHistoryInfo->GetDupInfo()->GetName();
			break;
		}

		if (pHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo &&
			pHistoryInfo->GetNZBInfo()->GetDupeMode() != dmForce &&
			pHistoryInfo->GetNZBInfo()->GetMarkStatus() == NZBInfo::ksGood &&
			SameNameOrKey(pHistoryInfo->GetNZBInfo()->GetName(), pHistoryInfo->GetNZBInfo()->GetDupeKey(),
				pNZBInfo->GetName(), pNZBInfo->GetDupeKey()))
		{
			bSkip = true;
			bGood = true;
			szDupeName = pHistoryInfo->GetNZBInfo()->GetName();
			break;
		}

		if (pHistoryInfo->GetKind() == HistoryInfo::hkDupInfo &&
			pHistoryInfo->GetDupInfo()->GetDupeMode() != dmForce &&
			(pHistoryInfo->GetDupInfo()->GetStatus() == DupInfo::dsGood ||
			 (pNZBInfo->GetDupeMode() == dmScore &&
			  pHistoryInfo->GetDupInfo()->GetStatus() == DupInfo::dsSuccess &&
			  pNZBInfo->GetDupeScore() <= pHistoryInfo->GetDupInfo()->GetDupeScore())) &&
			SameNameOrKey(pHistoryInfo->GetDupInfo()->GetName(), pHistoryInfo->GetDupInfo()->GetDupeKey(),
				pNZBInfo->GetName(), pNZBInfo->GetDupeKey()))
		{
			bSkip = true;
			bGood = pHistoryInfo->GetDupInfo()->GetStatus() == DupInfo::dsGood;
			szDupeName = pHistoryInfo->GetDupInfo()->GetName();
			break;
		}
	}

	if (!bSameContent && !bGood && pNZBInfo->GetDupeMode() == dmScore)
	{
		// nzb-files having success-duplicates in recent history (with different content) are added to history for backup
		for (HistoryList::iterator it = pDownloadQueue->GetHistoryList()->begin(); it != pDownloadQueue->GetHistoryList()->end(); it++)
		{
			HistoryInfo* pHistoryInfo = *it;
			if (pHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo &&
				pHistoryInfo->GetNZBInfo()->GetDupeMode() != dmForce &&
				SameNameOrKey(pHistoryInfo->GetNZBInfo()->GetName(), pHistoryInfo->GetNZBInfo()->GetDupeKey(),
					pNZBInfo->GetName(), pNZBInfo->GetDupeKey()) &&
				pNZBInfo->GetDupeScore() <= pHistoryInfo->GetNZBInfo()->GetDupeScore() &&
				IsDupeSuccess(pHistoryInfo->GetNZBInfo()))
			{
				// Flag saying QueueCoordinator to skip nzb-file, we also use this flag later in "PrePostProcessor::NZBAdded"
				pNZBInfo->SetDeleteStatus(NZBInfo::dsDupe);
				info("Collection %s is duplicate to %s", pNZBInfo->GetName(), pHistoryInfo->GetNZBInfo()->GetName());
				return;
			}
		}
	}

	if (bSkip)
	{
		if (!strcmp(pNZBInfo->GetName(), szDupeName))
		{
			warn("Skipping duplicate %s, found in history with %s", pNZBInfo->GetName(),
				bSameContent ? "exactly same content" : bGood ? "good status" : "success status");
		}
		else
		{
			warn("Skipping duplicate %s, found in history %s with %s",
				pNZBInfo->GetName(), szDupeName,
				bSameContent ? "exactly same content" : bGood ? "good status" : "success status");
		}

		// Flag saying QueueCoordinator to skip nzb-file
		pNZBInfo->SetDeleteStatus(NZBInfo::dsManual);
		DeleteQueuedFile(pNZBInfo->GetQueuedFilename());
		return;
	}
}

/**
 - If download queue or post-queue contain a duplicate the existing item
   and the newly added item are marked as duplicates to each other.
   The newly added item is paused (if dupemode=score).
*/
void DupeCoordinator::NZBAdded(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	debug("Checking duplicates for %s", pNZBInfo->GetName());

	bool bHigherScore = true;
	NZBInfo* pDupeNZBInfo = NULL;

	// find all duplicates in post queue
	std::set<NZBInfo*> postDupes;

	for (PostQueue::iterator it = pDownloadQueue->GetPostQueue()->begin(); it != pDownloadQueue->GetPostQueue()->end(); it++)
	{
		PostInfo* pPostInfo = *it;
		if (pPostInfo->GetNZBInfo()->GetDupeMode() != dmForce &&
			SameNameOrKey(pPostInfo->GetNZBInfo()->GetName(), pPostInfo->GetNZBInfo()->GetDupeKey(),
				pNZBInfo->GetName(), pNZBInfo->GetDupeKey()))
		{
			postDupes.insert(pPostInfo->GetNZBInfo());
			if (!pDupeNZBInfo)
			{
				pDupeNZBInfo = pPostInfo->GetNZBInfo();
			}
			bHigherScore = bHigherScore && pPostInfo->GetNZBInfo()->GetDupeScore() < pNZBInfo->GetDupeScore();
		}
	}

	// find all duplicates in download queue
	GroupQueue groupQueue;
	pDownloadQueue->BuildGroups(&groupQueue);
	std::list<GroupInfo*> queueDupes;
	GroupInfo* pNewGroupInfo = NULL;

	for (GroupQueue::iterator it = groupQueue.begin(); it != groupQueue.end(); it++)
	{
		GroupInfo* pGroupInfo = *it;
		NZBInfo* pGroupNZBInfo = pGroupInfo->GetNZBInfo();
		if (pGroupNZBInfo != pNZBInfo &&
			pGroupNZBInfo->GetDupeMode() != dmForce &&
			SameNameOrKey(pGroupNZBInfo->GetName(), pGroupNZBInfo->GetDupeKey(),
				pNZBInfo->GetName(), pNZBInfo->GetDupeKey()))
		{
			queueDupes.push_back(pGroupInfo);
			if (!pDupeNZBInfo)
			{
				pDupeNZBInfo = pGroupNZBInfo;
			}
			bHigherScore = bHigherScore && pGroupNZBInfo->GetDupeScore() < pNZBInfo->GetDupeScore();
		}
		if (pGroupNZBInfo == pNZBInfo)
		{
			pNewGroupInfo = pGroupInfo;
		}
	}

	if (pDupeNZBInfo)
	{
		info("Collection %s is duplicate to %s", pNZBInfo->GetName(), pDupeNZBInfo->GetName());

		// pause all duplicates with lower DupeScore, which are not in post-processing (only for dupemode=score)
		for (std::list<GroupInfo*>::iterator it = queueDupes.begin(); it != queueDupes.end(); it++)
		{
			GroupInfo* pGroupInfo = *it;
			NZBInfo* pDupeNZB = pGroupInfo->GetNZBInfo();
			if (pDupeNZB->GetDupeMode() == dmScore &&
				pDupeNZB->GetDupeScore() < pNZBInfo->GetDupeScore() &&
				postDupes.find(pDupeNZB) == postDupes.end() &&
				pGroupInfo->GetPausedFileCount() < pGroupInfo->GetRemainingFileCount())
			{
				info("Pausing collection %s with lower duplicate score", pDupeNZB->GetName());
				g_pQueueCoordinator->GetQueueEditor()->LockedEditEntry(pDownloadQueue, pGroupInfo->GetLastID(), false, QueueEditor::eaGroupPause, 0, NULL);
			}
		}

		if (!bHigherScore && pNZBInfo->GetDupeMode() == dmScore)
		{
			g_pQueueCoordinator->GetQueueEditor()->LockedEditEntry(pDownloadQueue, pNewGroupInfo->GetLastID(), false, QueueEditor::eaGroupPause, 0, NULL);
		}
	}
}

/**
  - if download of an item completes successfully and there are
    (paused) duplicates to this item in the queue, they all are deleted
    from queue;
  - if download of an item fails and there are (paused) duplicates to
    this item in the queue iten with highest DupeScore is unpaused;
*/
void DupeCoordinator::NZBCompleted(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	debug("Processing duplicates for %s", pNZBInfo->GetName());

	if (pNZBInfo->GetDupeMode() == dmScore)
	{
		if (IsDupeSuccess(pNZBInfo))
		{
			RemoveDupes(pDownloadQueue, pNZBInfo);
		}
		else
		{
			UnpauseBestDupe(pDownloadQueue, pNZBInfo, pNZBInfo->GetName(), pNZBInfo->GetDupeKey());
		}
	}
}

void DupeCoordinator::RemoveDupes(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	IDList groupIDList;

	GroupQueue groupQueue;
	pDownloadQueue->BuildGroups(&groupQueue);

	// removing paused duplicates for which nothing was downloaded yet
	for (GroupQueue::iterator it = groupQueue.begin(); it != groupQueue.end(); it++)
	{
		GroupInfo* pGroupInfo = *it;
		NZBInfo* pGroupNZBInfo = pGroupInfo->GetNZBInfo();

		if (pGroupNZBInfo != pNZBInfo &&
			pGroupNZBInfo->GetDupeMode() == dmScore &&
			pGroupNZBInfo->GetFailedArticles() == 0 &&
			pGroupNZBInfo->GetSuccessArticles() == 0 &&
			pGroupInfo->GetPausedFileCount() == pGroupInfo->GetRemainingFileCount() &&
			SameNameOrKey(pGroupNZBInfo->GetName(), pGroupNZBInfo->GetDupeKey(),
				pNZBInfo->GetName(), pNZBInfo->GetDupeKey()))
		{
			groupIDList.push_back(pGroupInfo->GetLastID());
			pGroupNZBInfo->SetDeleteStatus(NZBInfo::dsDupe);
		}
	}

	if (!groupIDList.empty())
	{
		info("Removing duplicates for %s from queue", pNZBInfo->GetName());
		g_pQueueCoordinator->GetQueueEditor()->LockedEditList(pDownloadQueue, &groupIDList, false, QueueEditor::eaGroupDelete, 0, NULL);
	}
}

void DupeCoordinator::UnpauseBestDupe(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, const char* szNZBName, const char* szDupeKey)
{
	// check if duplicates exist in post-processing queue
	std::set<NZBInfo*> postDupes;
	bool bPostProcess = false;
	int iPostScore = 0;
	for (PostQueue::iterator it = pDownloadQueue->GetPostQueue()->begin(); it != pDownloadQueue->GetPostQueue()->end(); it++)
	{
		PostInfo* pPostInfo = *it;
		if (pPostInfo->GetNZBInfo() != pNZBInfo &&
			pPostInfo->GetNZBInfo()->GetDupeMode() != dmForce &&
			SameNameOrKey(pPostInfo->GetNZBInfo()->GetName(), pPostInfo->GetNZBInfo()->GetDupeKey(), szNZBName, szDupeKey))
		{
			postDupes.insert(pPostInfo->GetNZBInfo());
			if (!bPostProcess || pPostInfo->GetNZBInfo()->GetDupeScore() > iPostScore)
			{
				iPostScore = pPostInfo->GetNZBInfo()->GetDupeScore();
			}
			bPostProcess = true;
		}
	}

	// find item in queue having the highest score (even higher than all post-processing duplicates)
	GroupQueue groupQueue;
	pDownloadQueue->BuildGroups(&groupQueue);
	GroupInfo* pBestGroupInfo = NULL;
	NZBInfo* pBestNZBInfo = NULL;
	for (GroupQueue::iterator it = groupQueue.begin(); it != groupQueue.end(); it++)
	{
		GroupInfo* pGroupInfo = *it;
		NZBInfo* pGroupNZBInfo = pGroupInfo->GetNZBInfo();
		if (pGroupNZBInfo != pNZBInfo &&
			pGroupNZBInfo->GetDupeMode() != dmForce &&
			postDupes.find(pGroupNZBInfo) == postDupes.end() &&
			SameNameOrKey(pGroupNZBInfo->GetName(), pGroupNZBInfo->GetDupeKey(), szNZBName, szDupeKey) &&
			(!bPostProcess || pGroupNZBInfo->GetDupeScore() > iPostScore) &&
			(!pBestNZBInfo || pGroupNZBInfo->GetDupeScore() > pBestNZBInfo->GetDupeScore()))
		{
			pBestNZBInfo = pGroupNZBInfo;
			pBestGroupInfo = pGroupInfo;
		}
	}

	if (pBestNZBInfo)
	{
		info("Unpausing duplicate %s", pBestNZBInfo->GetName());
		g_pQueueCoordinator->GetQueueEditor()->LockedEditEntry(pDownloadQueue, pBestGroupInfo->GetLastID(), false, QueueEditor::eaGroupResume, 0, NULL);
		g_pQueueCoordinator->GetQueueEditor()->LockedEditEntry(pDownloadQueue, pBestGroupInfo->GetLastID(), false, QueueEditor::eaGroupPauseExtraPars, 0, NULL);
	}
}

void DupeCoordinator::HistoryMark(DownloadQueue* pDownloadQueue, HistoryInfo* pHistoryInfo, bool bGood)
{
	char szNZBName[1024];
	pHistoryInfo->GetName(szNZBName, 1024);

	info("Marking %s as %s", szNZBName, (bGood ? "good" : "bad"));

	if (pHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo)
	{
		pHistoryInfo->GetNZBInfo()->SetMarkStatus(bGood ? NZBInfo::ksGood : NZBInfo::ksBad);
	}
	else if (pHistoryInfo->GetKind() == HistoryInfo::hkDupInfo)
	{
		pHistoryInfo->GetDupInfo()->SetStatus(bGood ? DupInfo::dsGood : DupInfo::dsBad);
	}
	else
	{
		error("Could not mark %s as bad: history item has wrong type", szNZBName);
		return;
	}

	if (!g_pOptions->GetDupeCheck() ||
		(pHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo &&
		 pHistoryInfo->GetNZBInfo()->GetDupeMode() == dmForce) ||
		(pHistoryInfo->GetKind() == HistoryInfo::hkDupInfo &&
		 pHistoryInfo->GetDupInfo()->GetDupeMode() == dmForce))
	{
		return;
	}

	if (bGood)
	{
		// mark as good
		// moving all duplicates from history to dup-history
		HistoryCleanup(pDownloadQueue, pHistoryInfo);
		return;
	}

	// mark as bad continues

	const char* szDupeKey = pHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo ? pHistoryInfo->GetNZBInfo()->GetDupeKey() :
		pHistoryInfo->GetKind() == HistoryInfo::hkDupInfo ? pHistoryInfo->GetDupInfo()->GetDupeKey() :
		NULL;

	// check if history (recent or dup) has other success-duplicates or good-duplicates
	bool bSuccessDupe = false;
	int iHighScore = 0;
	for (HistoryList::iterator it = pDownloadQueue->GetHistoryList()->begin(); it != pDownloadQueue->GetHistoryList()->end(); it++)
	{
		HistoryInfo* pHistoryInfo = *it;
		bool bGoodDupe = false;

		if (pHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo &&
			pHistoryInfo->GetNZBInfo()->GetDupeMode() != dmForce &&
			(IsDupeSuccess(pHistoryInfo->GetNZBInfo()) ||
			 pHistoryInfo->GetNZBInfo()->GetMarkStatus() == NZBInfo::ksGood) &&
			SameNameOrKey(pHistoryInfo->GetNZBInfo()->GetName(), pHistoryInfo->GetNZBInfo()->GetDupeKey(), szNZBName, szDupeKey))
		{
			if (!bSuccessDupe || pHistoryInfo->GetNZBInfo()->GetDupeScore() > iHighScore)
			{
				iHighScore = pHistoryInfo->GetNZBInfo()->GetDupeScore();
			}
			bSuccessDupe = true;
			bGoodDupe = pHistoryInfo->GetNZBInfo()->GetMarkStatus() == NZBInfo::ksGood;
		}

		if (pHistoryInfo->GetKind() == HistoryInfo::hkDupInfo &&
			pHistoryInfo->GetDupInfo()->GetDupeMode() != dmForce &&
			(pHistoryInfo->GetDupInfo()->GetStatus() == DupInfo::dsSuccess ||
			 pHistoryInfo->GetDupInfo()->GetStatus() == DupInfo::dsGood) &&
			SameNameOrKey(pHistoryInfo->GetDupInfo()->GetName(), pHistoryInfo->GetDupInfo()->GetDupeKey(), szNZBName, szDupeKey))
		{
			if (!bSuccessDupe || pHistoryInfo->GetDupInfo()->GetDupeScore() > iHighScore)
			{
				iHighScore = pHistoryInfo->GetDupInfo()->GetDupeScore();
			}
			bSuccessDupe = true;
			bGoodDupe = pHistoryInfo->GetDupInfo()->GetStatus() == DupInfo::dsGood;
		}

		if (bGoodDupe)
		{
			// another duplicate with good-status exists - exit without moving other dupes to queue
			return;
		}
	}

	// move existing dupe-backups with higher scores than existing success-duplicates from history to download queue
	HistoryList dupeList;
	for (HistoryList::iterator it = pDownloadQueue->GetHistoryList()->begin(); it != pDownloadQueue->GetHistoryList()->end(); it++)
	{
		HistoryInfo* pHistoryInfo = *it;
		if (pHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo &&
			pHistoryInfo->GetNZBInfo()->GetDupeMode() != dmForce &&
			pHistoryInfo->GetNZBInfo()->GetDeleteStatus() == NZBInfo::dsDupe &&
			(!bSuccessDupe || pHistoryInfo->GetNZBInfo()->GetDupeScore() > iHighScore) &&
			SameNameOrKey(pHistoryInfo->GetNZBInfo()->GetName(), pHistoryInfo->GetNZBInfo()->GetDupeKey(), szNZBName, szDupeKey))
		{
			dupeList.push_back(pHistoryInfo);
		}
	}

	for (HistoryList::iterator it = dupeList.begin(); it != dupeList.end(); it++)
	{
		HistoryInfo* pHistoryInfo = *it;
		HistoryReturnDupe(pDownloadQueue, pHistoryInfo);
	}

	UnpauseBestDupe(pDownloadQueue, NULL, szNZBName, szDupeKey);
}

void DupeCoordinator::HistoryReturnDupe(DownloadQueue* pDownloadQueue, HistoryInfo* pHistoryInfo)
{
	NZBInfo* pNZBInfo = pHistoryInfo->GetNZBInfo();

	if (!Util::FileExists(pNZBInfo->GetQueuedFilename()))
	{
		error("Could not return duplicate %s from history back to queue: could not find source nzb-file %s",
			pNZBInfo->GetName(), pNZBInfo->GetQueuedFilename());
		return;
	}

	NZBFile* pNZBFile = NZBFile::Create(pNZBInfo->GetQueuedFilename(), "");
	if (pNZBFile == NULL)
	{
		error("Could not return duplicate %s from history back to queue: could not parse nzb-file",
			pNZBInfo->GetName());
		return;
	}

	info("Returning duplicate %s from history back to queue", pNZBInfo->GetName());

	for (NZBFile::FileInfos::iterator it = pNZBFile->GetFileInfos()->begin(); it != pNZBFile->GetFileInfos()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		pFileInfo->SetNZBInfo(pNZBInfo);
		pFileInfo->SetPaused(true);
	}

	g_pQueueCoordinator->AddFileInfosToFileQueue(pNZBFile, pDownloadQueue->GetParkedFiles(), false);

	delete pNZBFile;

	HistoryList::iterator it = std::find(pDownloadQueue->GetHistoryList()->begin(), pDownloadQueue->GetHistoryList()->end(), pHistoryInfo);
	HistoryReturn(pDownloadQueue, it, pHistoryInfo, false);
}

void DupeCoordinator::HistoryCleanup(DownloadQueue* pDownloadQueue, HistoryInfo* pMarkHistoryInfo)
{
	const char* szDupeKey = pMarkHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo ? pMarkHistoryInfo->GetNZBInfo()->GetDupeKey() :
		pMarkHistoryInfo->GetKind() == HistoryInfo::hkDupInfo ? pMarkHistoryInfo->GetDupInfo()->GetDupeKey() :
		NULL;
	const char* szNZBName = pMarkHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo ? pMarkHistoryInfo->GetNZBInfo()->GetName() :
		pMarkHistoryInfo->GetKind() == HistoryInfo::hkDupInfo ? pMarkHistoryInfo->GetDupInfo()->GetName() :
		NULL;
	bool bChanged = false;
	int index = 0;

	// traversing in a reverse order to delete items in order they were added to history
	// (just to produce the log-messages in a more logical order)
	for (HistoryList::reverse_iterator it = pDownloadQueue->GetHistoryList()->rbegin(); it != pDownloadQueue->GetHistoryList()->rend(); )
	{
		HistoryInfo* pHistoryInfo = *it;

		if (pHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo &&
			pHistoryInfo->GetNZBInfo()->GetDupeMode() != dmForce &&
			pHistoryInfo->GetNZBInfo()->GetDeleteStatus() == NZBInfo::dsDupe &&
			pHistoryInfo != pMarkHistoryInfo &&
			SameNameOrKey(pHistoryInfo->GetNZBInfo()->GetName(), pHistoryInfo->GetNZBInfo()->GetDupeKey(), szNZBName, szDupeKey))
		{
			HistoryTransformToDup(pDownloadQueue, pHistoryInfo, index);
			index++;
			it = pDownloadQueue->GetHistoryList()->rbegin() + index;
			bChanged = true;
		}
		else
		{
			it++;
			index++;
		}
	}

	if (bChanged && g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
	{
		g_pDiskState->SaveDownloadQueue(pDownloadQueue);
	}
}

void DupeCoordinator::HistoryTransformToDup(DownloadQueue* pDownloadQueue, HistoryInfo* pHistoryInfo, int rindex)
{
	char szNiceName[1024];
	pHistoryInfo->GetName(szNiceName, 1024);

	// replace history element
	DupInfo* pDupInfo = new DupInfo();
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
		IsDupeSuccess(pHistoryInfo->GetNZBInfo()) ? DupInfo::dsSuccess :
		DupInfo::dsFailed);

	HistoryInfo* pNewHistoryInfo = new HistoryInfo(pDupInfo);
	pNewHistoryInfo->SetTime(pHistoryInfo->GetTime());
	(*pDownloadQueue->GetHistoryList())[pDownloadQueue->GetHistoryList()->size() - 1 - rindex] = pNewHistoryInfo;

	DeleteQueuedFile(pHistoryInfo->GetNZBInfo()->GetQueuedFilename());

	delete pHistoryInfo;
	info("Collection %s removed from recent history", szNiceName);
}
