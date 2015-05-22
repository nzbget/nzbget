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
#include "Options.h"
#include "Log.h"
#include "Util.h"
#include "NZBFile.h"
#include "HistoryCoordinator.h"
#include "DupeCoordinator.h"

bool DupeCoordinator::SameNameOrKey(const char* szName1, const char* szDupeKey1,
	const char* szName2, const char* szDupeKey2)
{
	bool bHasDupeKeys = !Util::EmptyStr(szDupeKey1) && !Util::EmptyStr(szDupeKey2);
	return (bHasDupeKeys && !strcmp(szDupeKey1, szDupeKey2)) ||
		(!bHasDupeKeys && !strcmp(szName1, szName2));
}

/**
  Check if the title was already downloaded or is already queued:
  - if there is a duplicate with exactly same content (via hash-check) 
    in queue or in history - the new item is skipped;
  - if there is a duplicate marked as good in history - the new item is skipped;
  - if there is a duplicate with success-status in dup-history but
    there are no duplicates in recent history - the new item is skipped;
  - if queue has a duplicate with the same or higher score - the new item
    is moved to history as dupe-backup;
  - if queue has a duplicate with lower score - the existing item is moved
    to history as dupe-backup (unless it is in post-processing stage) and
	the new item is added to queue;
  - if queue doesn't have duplicates - the new item is added to queue.
*/
void DupeCoordinator::NZBFound(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	debug("Checking duplicates for %s", pNZBInfo->GetName());

	// find duplicates in download queue with exactly same content
	for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
	{
		NZBInfo* pQueuedNZBInfo = *it;
		bool bSameContent = (pNZBInfo->GetFullContentHash() > 0 &&
			pNZBInfo->GetFullContentHash() == pQueuedNZBInfo->GetFullContentHash()) ||
			(pNZBInfo->GetFilteredContentHash() > 0 &&
			 pNZBInfo->GetFilteredContentHash() == pQueuedNZBInfo->GetFilteredContentHash());

		// if there is a duplicate with exactly same content (via hash-check) 
		// in queue - the new item is skipped
		if (pQueuedNZBInfo != pNZBInfo && bSameContent && pNZBInfo->GetKind() == NZBInfo::nkNzb)
		{
			if (!strcmp(pNZBInfo->GetName(), pQueuedNZBInfo->GetName()))
			{
				warn("Skipping duplicate %s, already queued", pNZBInfo->GetName());
			}
			else
			{
				warn("Skipping duplicate %s, already queued as %s",
					pNZBInfo->GetName(), pQueuedNZBInfo->GetName());
			}
			// Flag saying QueueCoordinator to skip nzb-file
			pNZBInfo->SetDeleteStatus(NZBInfo::dsManual);
			g_pHistoryCoordinator->DeleteDiskFiles(pNZBInfo);
			return;
		}
	}

	// if download has empty dupekey and empty dupescore - check if download queue
	// or history have an item with the same name and non empty dupekey or dupescore and
	// take these properties from this item
	if (Util::EmptyStr(pNZBInfo->GetDupeKey()) && pNZBInfo->GetDupeScore() == 0)
	{
		for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
		{
			NZBInfo* pQueuedNZBInfo = *it;
			if (!strcmp(pQueuedNZBInfo->GetName(), pNZBInfo->GetName()) &&
				(!Util::EmptyStr(pQueuedNZBInfo->GetDupeKey()) || pQueuedNZBInfo->GetDupeScore() != 0))
			{
				pNZBInfo->SetDupeKey(pQueuedNZBInfo->GetDupeKey());
				pNZBInfo->SetDupeScore(pQueuedNZBInfo->GetDupeScore());
				info("Assigning dupekey %s and dupescore %i to %s from existing queue item with the same name",
					 pNZBInfo->GetDupeKey(), pNZBInfo->GetDupeScore(), pNZBInfo->GetName());
				break;
			}
		}
	}
	if (Util::EmptyStr(pNZBInfo->GetDupeKey()) && pNZBInfo->GetDupeScore() == 0)
	{
		for (HistoryList::iterator it = pDownloadQueue->GetHistory()->begin(); it != pDownloadQueue->GetHistory()->end(); it++)
		{
			HistoryInfo* pHistoryInfo = *it;
			if (pHistoryInfo->GetKind() == HistoryInfo::hkNzb &&
				!strcmp(pHistoryInfo->GetNZBInfo()->GetName(), pNZBInfo->GetName()) &&
				(!Util::EmptyStr(pHistoryInfo->GetNZBInfo()->GetDupeKey()) || pHistoryInfo->GetNZBInfo()->GetDupeScore() != 0))
			{
				pNZBInfo->SetDupeKey(pHistoryInfo->GetNZBInfo()->GetDupeKey());
				pNZBInfo->SetDupeScore(pHistoryInfo->GetNZBInfo()->GetDupeScore());
				info("Assigning dupekey %s and dupescore %i to %s from existing history item with the same name",
					 pNZBInfo->GetDupeKey(), pNZBInfo->GetDupeScore(), pNZBInfo->GetName());
				break;
			}
			if (pHistoryInfo->GetKind() == HistoryInfo::hkDup &&
				!strcmp(pHistoryInfo->GetDupInfo()->GetName(), pNZBInfo->GetName()) &&
				(!Util::EmptyStr(pHistoryInfo->GetDupInfo()->GetDupeKey()) || pHistoryInfo->GetDupInfo()->GetDupeScore() != 0))
			{
				pNZBInfo->SetDupeKey(pHistoryInfo->GetDupInfo()->GetDupeKey());
				pNZBInfo->SetDupeScore(pHistoryInfo->GetDupInfo()->GetDupeScore());
				info("Assigning dupekey %s and dupescore %i to %s from existing history item with the same name",
					 pNZBInfo->GetDupeKey(), pNZBInfo->GetDupeScore(), pNZBInfo->GetName());
				break;
			}
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
	for (HistoryList::iterator it = pDownloadQueue->GetHistory()->begin(); it != pDownloadQueue->GetHistory()->end(); it++)
	{
		HistoryInfo* pHistoryInfo = *it;

		if (pHistoryInfo->GetKind() == HistoryInfo::hkNzb &&
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

		if (pHistoryInfo->GetKind() == HistoryInfo::hkDup &&
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

		if (pHistoryInfo->GetKind() == HistoryInfo::hkNzb &&
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

		if (pHistoryInfo->GetKind() == HistoryInfo::hkDup &&
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
		for (HistoryList::iterator it = pDownloadQueue->GetHistory()->begin(); it != pDownloadQueue->GetHistory()->end(); it++)
		{
			HistoryInfo* pHistoryInfo = *it;
			if (pHistoryInfo->GetKind() == HistoryInfo::hkNzb &&
				pHistoryInfo->GetNZBInfo()->GetDupeMode() != dmForce &&
				SameNameOrKey(pHistoryInfo->GetNZBInfo()->GetName(), pHistoryInfo->GetNZBInfo()->GetDupeKey(),
					pNZBInfo->GetName(), pNZBInfo->GetDupeKey()) &&
				pNZBInfo->GetDupeScore() <= pHistoryInfo->GetNZBInfo()->GetDupeScore() &&
				pHistoryInfo->GetNZBInfo()->IsDupeSuccess())
			{
				// Flag saying QueueCoordinator to skip nzb-file
				pNZBInfo->SetDeleteStatus(NZBInfo::dsDupe);
				info("Collection %s is a duplicate to %s", pNZBInfo->GetName(), pHistoryInfo->GetNZBInfo()->GetName());
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
		g_pHistoryCoordinator->DeleteDiskFiles(pNZBInfo);
		return;
	}

	// find duplicates in download queue and post-queue and handle both items according to their scores:
	// only one item remains in queue and another one is moved to history as dupe-backup
	if (pNZBInfo->GetDupeMode() == dmScore)
	{
		// find duplicates in download queue
		int index = 0;
		for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); index++)
		{
			NZBInfo* pQueuedNZBInfo = *it++;
			if (pQueuedNZBInfo != pNZBInfo &&
				pQueuedNZBInfo->GetKind() == NZBInfo::nkNzb &&
				pQueuedNZBInfo->GetDupeMode() != dmForce &&
				SameNameOrKey(pQueuedNZBInfo->GetName(), pQueuedNZBInfo->GetDupeKey(),
					pNZBInfo->GetName(), pNZBInfo->GetDupeKey()))
			{
				// if queue has a duplicate with the same or higher score - the new item
				// is moved to history as dupe-backup
				if (pNZBInfo->GetDupeScore() <= pQueuedNZBInfo->GetDupeScore())
				{
					// Flag saying QueueCoordinator to skip nzb-file
					pNZBInfo->SetDeleteStatus(NZBInfo::dsDupe);
					info("Collection %s is a duplicate to %s", pNZBInfo->GetName(), pQueuedNZBInfo->GetName());
					return;
				}

				// if queue has a duplicate with lower score - the existing item is moved
				// to history as dupe-backup (unless it is in post-processing stage) and
				// the new item is added to queue (unless it is in post-processing stage)
				if (!pQueuedNZBInfo->GetPostInfo())
				{
					// the existing queue item is moved to history as dupe-backup
					info("Moving collection %s with lower duplicate score to history", pQueuedNZBInfo->GetName());
					pQueuedNZBInfo->SetDeleteStatus(NZBInfo::dsDupe);
					pDownloadQueue->EditEntry(pQueuedNZBInfo->GetID(),
						DownloadQueue::eaGroupDelete, 0, NULL);
					it = pDownloadQueue->GetQueue()->begin() + index;
				}
			}
		}
	}
}

/**
  - if download of an item fails and there are duplicates in history -
    return the best duplicate from history to queue for download;
  - if download of an item completes successfully - nothing extra needs to be done;
*/
void DupeCoordinator::NZBCompleted(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	debug("Processing duplicates for %s", pNZBInfo->GetName());

	if (pNZBInfo->GetDupeMode() == dmScore && !pNZBInfo->IsDupeSuccess())
	{
		ReturnBestDupe(pDownloadQueue, pNZBInfo, pNZBInfo->GetName(), pNZBInfo->GetDupeKey());
	}
}

/**
 Returns the best duplicate from history to download queue.
*/
void DupeCoordinator::ReturnBestDupe(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, const char* szNZBName, const char* szDupeKey)
{
	// check if history (recent or dup) has other success-duplicates or good-duplicates
	bool bHistoryDupe = false;
	int iHistoryScore = 0;
	for (HistoryList::iterator it = pDownloadQueue->GetHistory()->begin(); it != pDownloadQueue->GetHistory()->end(); it++)
	{
		HistoryInfo* pHistoryInfo = *it;
		bool bGoodDupe = false;

		if (pHistoryInfo->GetKind() == HistoryInfo::hkNzb &&
			pHistoryInfo->GetNZBInfo()->GetDupeMode() != dmForce &&
			pHistoryInfo->GetNZBInfo()->IsDupeSuccess() &&
			SameNameOrKey(pHistoryInfo->GetNZBInfo()->GetName(), pHistoryInfo->GetNZBInfo()->GetDupeKey(), szNZBName, szDupeKey))
		{
			if (!bHistoryDupe || pHistoryInfo->GetNZBInfo()->GetDupeScore() > iHistoryScore)
			{
				iHistoryScore = pHistoryInfo->GetNZBInfo()->GetDupeScore();
			}
			bHistoryDupe = true;
			bGoodDupe = pHistoryInfo->GetNZBInfo()->GetMarkStatus() == NZBInfo::ksGood;
		}

		if (pHistoryInfo->GetKind() == HistoryInfo::hkDup &&
			pHistoryInfo->GetDupInfo()->GetDupeMode() != dmForce &&
			(pHistoryInfo->GetDupInfo()->GetStatus() == DupInfo::dsSuccess ||
			 pHistoryInfo->GetDupInfo()->GetStatus() == DupInfo::dsGood) &&
			SameNameOrKey(pHistoryInfo->GetDupInfo()->GetName(), pHistoryInfo->GetDupInfo()->GetDupeKey(), szNZBName, szDupeKey))
		{
			if (!bHistoryDupe || pHistoryInfo->GetDupInfo()->GetDupeScore() > iHistoryScore)
			{
				iHistoryScore = pHistoryInfo->GetDupInfo()->GetDupeScore();
			}
			bHistoryDupe = true;
			bGoodDupe = pHistoryInfo->GetDupInfo()->GetStatus() == DupInfo::dsGood;
		}

		if (bGoodDupe)
		{
			// another duplicate with good-status exists - exit without moving other dupes to queue
			return;
		}
	}

	// check if duplicates exist in download queue
	bool bQueueDupe = false;
	int iQueueScore = 0;
	for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
	{
		NZBInfo* pQueuedNZBInfo = *it;
		if (pQueuedNZBInfo != pNZBInfo &&
			pQueuedNZBInfo->GetKind() == NZBInfo::nkNzb &&
			pQueuedNZBInfo->GetDupeMode() != dmForce &&
			SameNameOrKey(pQueuedNZBInfo->GetName(), pQueuedNZBInfo->GetDupeKey(), szNZBName, szDupeKey) &&
			(!bQueueDupe || pQueuedNZBInfo->GetDupeScore() > iQueueScore))
		{
			iQueueScore = pQueuedNZBInfo->GetDupeScore();
			bQueueDupe = true;
		}
	}

	// find dupe-backup with highest score, whose score is also higher than other
	// success-duplicates and higher than already queued items
	HistoryInfo* pHistoryDupe = NULL;
	for (HistoryList::iterator it = pDownloadQueue->GetHistory()->begin(); it != pDownloadQueue->GetHistory()->end(); it++)
	{
		HistoryInfo* pHistoryInfo = *it;
		if (pHistoryInfo->GetKind() == HistoryInfo::hkNzb &&
			pHistoryInfo->GetNZBInfo()->GetDupeMode() != dmForce &&
			pHistoryInfo->GetNZBInfo()->GetDeleteStatus() == NZBInfo::dsDupe &&
			pHistoryInfo->GetNZBInfo()->CalcHealth() >= pHistoryInfo->GetNZBInfo()->CalcCriticalHealth(true) &&
			pHistoryInfo->GetNZBInfo()->GetMarkStatus() != NZBInfo::ksBad &&
			(!bHistoryDupe || pHistoryInfo->GetNZBInfo()->GetDupeScore() > iHistoryScore) &&
			(!bQueueDupe || pHistoryInfo->GetNZBInfo()->GetDupeScore() > iQueueScore) &&
			(!pHistoryDupe || pHistoryInfo->GetNZBInfo()->GetDupeScore() > pHistoryDupe->GetNZBInfo()->GetDupeScore()) &&
			SameNameOrKey(pHistoryInfo->GetNZBInfo()->GetName(), pHistoryInfo->GetNZBInfo()->GetDupeKey(), szNZBName, szDupeKey))
		{
			pHistoryDupe = pHistoryInfo;
		}
	}

	// move that dupe-backup from history to download queue
	if (pHistoryDupe)
	{
		info("Found duplicate %s for %s", pHistoryDupe->GetNZBInfo()->GetName(), szNZBName);
		g_pHistoryCoordinator->Redownload(pDownloadQueue, pHistoryDupe);
	}
}

void DupeCoordinator::HistoryMark(DownloadQueue* pDownloadQueue, HistoryInfo* pHistoryInfo, NZBInfo::EMarkStatus eMarkStatus)
{
	char szNZBName[1024];
	pHistoryInfo->GetName(szNZBName, 1024);

    const char* szMarkStatusName[] = { "NONE", "bad", "good", "success" };

	info("Marking %s as %s", szNZBName, szMarkStatusName[eMarkStatus]);

	if (pHistoryInfo->GetKind() == HistoryInfo::hkNzb)
	{
		pHistoryInfo->GetNZBInfo()->SetMarkStatus(eMarkStatus);
	}
	else if (pHistoryInfo->GetKind() == HistoryInfo::hkDup)
	{
		pHistoryInfo->GetDupInfo()->SetStatus(
			eMarkStatus == NZBInfo::ksGood ? DupInfo::dsGood :
			eMarkStatus == NZBInfo::ksSuccess ? DupInfo::dsSuccess :
			DupInfo::dsBad);
	}
	else
	{
		error("Could not mark %s as bad: history item has wrong type", szNZBName);
		return;
	}

	if (!g_pOptions->GetDupeCheck() ||
		(pHistoryInfo->GetKind() == HistoryInfo::hkNzb &&
		 pHistoryInfo->GetNZBInfo()->GetDupeMode() == dmForce) ||
		(pHistoryInfo->GetKind() == HistoryInfo::hkDup &&
		 pHistoryInfo->GetDupInfo()->GetDupeMode() == dmForce))
	{
		return;
	}

	if (eMarkStatus == NZBInfo::ksGood)
	{
		// mark as good
		// moving all duplicates from history to dup-history
		HistoryCleanup(pDownloadQueue, pHistoryInfo);
	}
	else if (eMarkStatus == NZBInfo::ksBad)
	{
		// mark as bad
		const char* szDupeKey = pHistoryInfo->GetKind() == HistoryInfo::hkNzb ? pHistoryInfo->GetNZBInfo()->GetDupeKey() :
			pHistoryInfo->GetKind() == HistoryInfo::hkDup ? pHistoryInfo->GetDupInfo()->GetDupeKey() :
			NULL;
		ReturnBestDupe(pDownloadQueue, NULL, szNZBName, szDupeKey);
	}
}

void DupeCoordinator::HistoryCleanup(DownloadQueue* pDownloadQueue, HistoryInfo* pMarkHistoryInfo)
{
	const char* szDupeKey = pMarkHistoryInfo->GetKind() == HistoryInfo::hkNzb ? pMarkHistoryInfo->GetNZBInfo()->GetDupeKey() :
		pMarkHistoryInfo->GetKind() == HistoryInfo::hkDup ? pMarkHistoryInfo->GetDupInfo()->GetDupeKey() :
		NULL;
	const char* szNZBName = pMarkHistoryInfo->GetKind() == HistoryInfo::hkNzb ? pMarkHistoryInfo->GetNZBInfo()->GetName() :
		pMarkHistoryInfo->GetKind() == HistoryInfo::hkDup ? pMarkHistoryInfo->GetDupInfo()->GetName() :
		NULL;
	bool bChanged = false;
	int index = 0;

	// traversing in a reverse order to delete items in order they were added to history
	// (just to produce the log-messages in a more logical order)
	for (HistoryList::reverse_iterator it = pDownloadQueue->GetHistory()->rbegin(); it != pDownloadQueue->GetHistory()->rend(); )
	{
		HistoryInfo* pHistoryInfo = *it;

		if (pHistoryInfo->GetKind() == HistoryInfo::hkNzb &&
			pHistoryInfo->GetNZBInfo()->GetDupeMode() != dmForce &&
			pHistoryInfo->GetNZBInfo()->GetDeleteStatus() == NZBInfo::dsDupe &&
			pHistoryInfo != pMarkHistoryInfo &&
			SameNameOrKey(pHistoryInfo->GetNZBInfo()->GetName(), pHistoryInfo->GetNZBInfo()->GetDupeKey(), szNZBName, szDupeKey))
		{
			g_pHistoryCoordinator->HistoryHide(pDownloadQueue, pHistoryInfo, index);
			index++;
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
}

DupeCoordinator::EDupeStatus DupeCoordinator::GetDupeStatus(DownloadQueue* pDownloadQueue,
	const char* szName, const char* szDupeKey)
{
	EDupeStatus eStatuses = dsNone;

	// find duplicates in download queue
	for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
	{
		NZBInfo* pNZBInfo = *it;
		if (SameNameOrKey(szName, szDupeKey, pNZBInfo->GetName(), pNZBInfo->GetDupeKey()))
		{
			if (pNZBInfo->GetSuccessArticles() + pNZBInfo->GetFailedArticles() > 0)
			{
				eStatuses = (EDupeStatus)(eStatuses | dsDownloading);
			}
			else
			{
				eStatuses = (EDupeStatus)(eStatuses | dsQueued);
			}
		}
	}

	// find duplicates in history
	for (HistoryList::iterator it = pDownloadQueue->GetHistory()->begin(); it != pDownloadQueue->GetHistory()->end(); it++)
	{
		HistoryInfo* pHistoryInfo = *it;

		if (pHistoryInfo->GetKind() == HistoryInfo::hkNzb &&
			SameNameOrKey(szName, szDupeKey, pHistoryInfo->GetNZBInfo()->GetName(), pHistoryInfo->GetNZBInfo()->GetDupeKey()))
		{
			const char* szTextStatus = pHistoryInfo->GetNZBInfo()->MakeTextStatus(true);
			if (!strncasecmp(szTextStatus, "SUCCESS", 7))
			{
				eStatuses = (EDupeStatus)(eStatuses | dsSuccess);
			}
			else if (!strncasecmp(szTextStatus, "FAILURE", 7))
			{
				eStatuses = (EDupeStatus)(eStatuses | dsFailure);
			}
			else if (!strncasecmp(szTextStatus, "WARNING", 7))
			{
				eStatuses = (EDupeStatus)(eStatuses | dsWarning);
			}
		}

		if (pHistoryInfo->GetKind() == HistoryInfo::hkDup &&
			SameNameOrKey(szName, szDupeKey, pHistoryInfo->GetDupInfo()->GetName(), pHistoryInfo->GetDupInfo()->GetDupeKey()))
		{
			if (pHistoryInfo->GetDupInfo()->GetStatus() == DupInfo::dsSuccess ||
				pHistoryInfo->GetDupInfo()->GetStatus() == DupInfo::dsGood)
			{
				eStatuses = (EDupeStatus)(eStatuses | dsSuccess);
			}
			else if (pHistoryInfo->GetDupInfo()->GetStatus() == DupInfo::dsFailed ||
					 pHistoryInfo->GetDupInfo()->GetStatus() == DupInfo::dsBad)
			{
				eStatuses = (EDupeStatus)(eStatuses | dsFailure);
			}
		}
	}

	return eStatuses;
}
