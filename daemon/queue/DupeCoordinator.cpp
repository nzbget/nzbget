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

bool DupeCoordinator::SameNameOrKey(const char* name1, const char* dupeKey1,
	const char* name2, const char* dupeKey2)
{
	bool hasDupeKeys = !Util::EmptyStr(dupeKey1) && !Util::EmptyStr(dupeKey2);
	return (hasDupeKeys && !strcmp(dupeKey1, dupeKey2)) ||
		(!hasDupeKeys && !strcmp(name1, name2));
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
void DupeCoordinator::NZBFound(DownloadQueue* downloadQueue, NZBInfo* nzbInfo)
{
	debug("Checking duplicates for %s", nzbInfo->GetName());

	// find duplicates in download queue with exactly same content
	for (NZBList::iterator it = downloadQueue->GetQueue()->begin(); it != downloadQueue->GetQueue()->end(); it++)
	{
		NZBInfo* queuedNzbInfo = *it;
		bool sameContent = (nzbInfo->GetFullContentHash() > 0 &&
			nzbInfo->GetFullContentHash() == queuedNzbInfo->GetFullContentHash()) ||
			(nzbInfo->GetFilteredContentHash() > 0 &&
			 nzbInfo->GetFilteredContentHash() == queuedNzbInfo->GetFilteredContentHash());

		// if there is a duplicate with exactly same content (via hash-check) 
		// in queue - the new item is skipped
		if (queuedNzbInfo != nzbInfo && sameContent && nzbInfo->GetKind() == NZBInfo::nkNzb)
		{
			char message[1024];
			if (!strcmp(nzbInfo->GetName(), queuedNzbInfo->GetName()))
			{
				snprintf(message, 1024, "Skipping duplicate %s, already queued", nzbInfo->GetName());
			}
			else
			{
				snprintf(message, 1024, "Skipping duplicate %s, already queued as %s",
					nzbInfo->GetName(), queuedNzbInfo->GetName());
			}
			message[1024-1] = '\0';

			if (nzbInfo->GetFeedID())
			{
				warn("%s", message);
				// Flag saying QueueCoordinator to skip nzb-file
				nzbInfo->SetDeleteStatus(NZBInfo::dsManual);
				g_pHistoryCoordinator->DeleteDiskFiles(nzbInfo);
			}
			else
			{
				nzbInfo->SetDeleteStatus(NZBInfo::dsCopy);
				nzbInfo->AddMessage(Message::mkWarning, message);
			}

			return;
		}
	}

	// if download has empty dupekey and empty dupescore - check if download queue
	// or history have an item with the same name and non empty dupekey or dupescore and
	// take these properties from this item
	if (Util::EmptyStr(nzbInfo->GetDupeKey()) && nzbInfo->GetDupeScore() == 0)
	{
		for (NZBList::iterator it = downloadQueue->GetQueue()->begin(); it != downloadQueue->GetQueue()->end(); it++)
		{
			NZBInfo* queuedNzbInfo = *it;
			if (!strcmp(queuedNzbInfo->GetName(), nzbInfo->GetName()) &&
				(!Util::EmptyStr(queuedNzbInfo->GetDupeKey()) || queuedNzbInfo->GetDupeScore() != 0))
			{
				nzbInfo->SetDupeKey(queuedNzbInfo->GetDupeKey());
				nzbInfo->SetDupeScore(queuedNzbInfo->GetDupeScore());
				info("Assigning dupekey %s and dupescore %i to %s from existing queue item with the same name",
					 nzbInfo->GetDupeKey(), nzbInfo->GetDupeScore(), nzbInfo->GetName());
				break;
			}
		}
	}
	if (Util::EmptyStr(nzbInfo->GetDupeKey()) && nzbInfo->GetDupeScore() == 0)
	{
		for (HistoryList::iterator it = downloadQueue->GetHistory()->begin(); it != downloadQueue->GetHistory()->end(); it++)
		{
			HistoryInfo* historyInfo = *it;
			if (historyInfo->GetKind() == HistoryInfo::hkNzb &&
				!strcmp(historyInfo->GetNZBInfo()->GetName(), nzbInfo->GetName()) &&
				(!Util::EmptyStr(historyInfo->GetNZBInfo()->GetDupeKey()) || historyInfo->GetNZBInfo()->GetDupeScore() != 0))
			{
				nzbInfo->SetDupeKey(historyInfo->GetNZBInfo()->GetDupeKey());
				nzbInfo->SetDupeScore(historyInfo->GetNZBInfo()->GetDupeScore());
				info("Assigning dupekey %s and dupescore %i to %s from existing history item with the same name",
					 nzbInfo->GetDupeKey(), nzbInfo->GetDupeScore(), nzbInfo->GetName());
				break;
			}
			if (historyInfo->GetKind() == HistoryInfo::hkDup &&
				!strcmp(historyInfo->GetDupInfo()->GetName(), nzbInfo->GetName()) &&
				(!Util::EmptyStr(historyInfo->GetDupInfo()->GetDupeKey()) || historyInfo->GetDupInfo()->GetDupeScore() != 0))
			{
				nzbInfo->SetDupeKey(historyInfo->GetDupInfo()->GetDupeKey());
				nzbInfo->SetDupeScore(historyInfo->GetDupInfo()->GetDupeScore());
				info("Assigning dupekey %s and dupescore %i to %s from existing history item with the same name",
					 nzbInfo->GetDupeKey(), nzbInfo->GetDupeScore(), nzbInfo->GetName());
				break;
			}
		}
	}

	// find duplicates in history

	bool skip = false;
	bool good = false;
	bool sameContent = false;
	const char* dupeName = NULL;

	// find duplicates in history having exactly same content
	// also: nzb-files having duplicates marked as good are skipped
	// also (only in score mode): nzb-files having success-duplicates in dup-history but not having duplicates in recent history are skipped
	for (HistoryList::iterator it = downloadQueue->GetHistory()->begin(); it != downloadQueue->GetHistory()->end(); it++)
	{
		HistoryInfo* historyInfo = *it;

		if (historyInfo->GetKind() == HistoryInfo::hkNzb &&
			((nzbInfo->GetFullContentHash() > 0 &&
			nzbInfo->GetFullContentHash() == historyInfo->GetNZBInfo()->GetFullContentHash()) ||
			(nzbInfo->GetFilteredContentHash() > 0 &&
			 nzbInfo->GetFilteredContentHash() == historyInfo->GetNZBInfo()->GetFilteredContentHash())))
		{
			skip = true;
			sameContent = true;
			dupeName = historyInfo->GetNZBInfo()->GetName();
			break;
		}

		if (historyInfo->GetKind() == HistoryInfo::hkDup &&
			((nzbInfo->GetFullContentHash() > 0 &&
			  nzbInfo->GetFullContentHash() == historyInfo->GetDupInfo()->GetFullContentHash()) ||
			 (nzbInfo->GetFilteredContentHash() > 0 &&
			  nzbInfo->GetFilteredContentHash() == historyInfo->GetDupInfo()->GetFilteredContentHash())))
		{
			skip = true;
			sameContent = true;
			dupeName = historyInfo->GetDupInfo()->GetName();
			break;
		}

		if (historyInfo->GetKind() == HistoryInfo::hkNzb &&
			historyInfo->GetNZBInfo()->GetDupeMode() != dmForce &&
			historyInfo->GetNZBInfo()->GetMarkStatus() == NZBInfo::ksGood &&
			SameNameOrKey(historyInfo->GetNZBInfo()->GetName(), historyInfo->GetNZBInfo()->GetDupeKey(),
				nzbInfo->GetName(), nzbInfo->GetDupeKey()))
		{
			skip = true;
			good = true;
			dupeName = historyInfo->GetNZBInfo()->GetName();
			break;
		}

		if (historyInfo->GetKind() == HistoryInfo::hkDup &&
			historyInfo->GetDupInfo()->GetDupeMode() != dmForce &&
			(historyInfo->GetDupInfo()->GetStatus() == DupInfo::dsGood ||
			 (nzbInfo->GetDupeMode() == dmScore &&
			  historyInfo->GetDupInfo()->GetStatus() == DupInfo::dsSuccess &&
			  nzbInfo->GetDupeScore() <= historyInfo->GetDupInfo()->GetDupeScore())) &&
			SameNameOrKey(historyInfo->GetDupInfo()->GetName(), historyInfo->GetDupInfo()->GetDupeKey(),
				nzbInfo->GetName(), nzbInfo->GetDupeKey()))
		{
			skip = true;
			good = historyInfo->GetDupInfo()->GetStatus() == DupInfo::dsGood;
			dupeName = historyInfo->GetDupInfo()->GetName();
			break;
		}
	}

	if (!sameContent && !good && nzbInfo->GetDupeMode() == dmScore)
	{
		// nzb-files having success-duplicates in recent history (with different content) are added to history for backup
		for (HistoryList::iterator it = downloadQueue->GetHistory()->begin(); it != downloadQueue->GetHistory()->end(); it++)
		{
			HistoryInfo* historyInfo = *it;
			if (historyInfo->GetKind() == HistoryInfo::hkNzb &&
				historyInfo->GetNZBInfo()->GetDupeMode() != dmForce &&
				SameNameOrKey(historyInfo->GetNZBInfo()->GetName(), historyInfo->GetNZBInfo()->GetDupeKey(),
					nzbInfo->GetName(), nzbInfo->GetDupeKey()) &&
				nzbInfo->GetDupeScore() <= historyInfo->GetNZBInfo()->GetDupeScore() &&
				historyInfo->GetNZBInfo()->IsDupeSuccess())
			{
				// Flag saying QueueCoordinator to skip nzb-file
				nzbInfo->SetDeleteStatus(NZBInfo::dsDupe);
				info("Collection %s is a duplicate to %s", nzbInfo->GetName(), historyInfo->GetNZBInfo()->GetName());
				return;
			}
		}
	}

	if (skip)
	{
		char message[1024];
		if (!strcmp(nzbInfo->GetName(), dupeName))
		{
			snprintf(message, 1024, "Skipping duplicate %s, found in history with %s", nzbInfo->GetName(),
				sameContent ? "exactly same content" : good ? "good status" : "success status");
		}
		else
		{
			snprintf(message, 1024, "Skipping duplicate %s, found in history %s with %s",
				nzbInfo->GetName(), dupeName,
				sameContent ? "exactly same content" : good ? "good status" : "success status");
		}
		message[1024-1] = '\0';

		if (nzbInfo->GetFeedID())
		{
			warn("%s", message);
			// Flag saying QueueCoordinator to skip nzb-file
			nzbInfo->SetDeleteStatus(NZBInfo::dsManual);
			g_pHistoryCoordinator->DeleteDiskFiles(nzbInfo);
		}
		else
		{
			nzbInfo->SetDeleteStatus(sameContent ? NZBInfo::dsCopy : NZBInfo::dsGood);
			nzbInfo->AddMessage(Message::mkWarning, message);
		}

		return;
	}

	// find duplicates in download queue and post-queue and handle both items according to their scores:
	// only one item remains in queue and another one is moved to history as dupe-backup
	if (nzbInfo->GetDupeMode() == dmScore)
	{
		// find duplicates in download queue
		int index = 0;
		for (NZBList::iterator it = downloadQueue->GetQueue()->begin(); it != downloadQueue->GetQueue()->end(); index++)
		{
			NZBInfo* queuedNzbInfo = *it++;
			if (queuedNzbInfo != nzbInfo &&
				queuedNzbInfo->GetKind() == NZBInfo::nkNzb &&
				queuedNzbInfo->GetDupeMode() != dmForce &&
				SameNameOrKey(queuedNzbInfo->GetName(), queuedNzbInfo->GetDupeKey(),
					nzbInfo->GetName(), nzbInfo->GetDupeKey()))
			{
				// if queue has a duplicate with the same or higher score - the new item
				// is moved to history as dupe-backup
				if (nzbInfo->GetDupeScore() <= queuedNzbInfo->GetDupeScore())
				{
					// Flag saying QueueCoordinator to skip nzb-file
					nzbInfo->SetDeleteStatus(NZBInfo::dsDupe);
					info("Collection %s is a duplicate to %s", nzbInfo->GetName(), queuedNzbInfo->GetName());
					return;
				}

				// if queue has a duplicate with lower score - the existing item is moved
				// to history as dupe-backup (unless it is in post-processing stage) and
				// the new item is added to queue (unless it is in post-processing stage)
				if (!queuedNzbInfo->GetPostInfo())
				{
					// the existing queue item is moved to history as dupe-backup
					info("Moving collection %s with lower duplicate score to history", queuedNzbInfo->GetName());
					queuedNzbInfo->SetDeleteStatus(NZBInfo::dsDupe);
					downloadQueue->EditEntry(queuedNzbInfo->GetID(),
						DownloadQueue::eaGroupDelete, 0, NULL);
					it = downloadQueue->GetQueue()->begin() + index;
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
void DupeCoordinator::NZBCompleted(DownloadQueue* downloadQueue, NZBInfo* nzbInfo)
{
	debug("Processing duplicates for %s", nzbInfo->GetName());

	if (nzbInfo->GetDupeMode() == dmScore && !nzbInfo->IsDupeSuccess())
	{
		ReturnBestDupe(downloadQueue, nzbInfo, nzbInfo->GetName(), nzbInfo->GetDupeKey());
	}
}

/**
 Returns the best duplicate from history to download queue.
*/
void DupeCoordinator::ReturnBestDupe(DownloadQueue* downloadQueue, NZBInfo* nzbInfo, const char* nzbName, const char* dupeKey)
{
	// check if history (recent or dup) has other success-duplicates or good-duplicates
	bool dupeFound = false;
	int historyScore = 0;
	for (HistoryList::iterator it = downloadQueue->GetHistory()->begin(); it != downloadQueue->GetHistory()->end(); it++)
	{
		HistoryInfo* historyInfo = *it;
		bool goodDupe = false;

		if (historyInfo->GetKind() == HistoryInfo::hkNzb &&
			historyInfo->GetNZBInfo()->GetDupeMode() != dmForce &&
			historyInfo->GetNZBInfo()->IsDupeSuccess() &&
			SameNameOrKey(historyInfo->GetNZBInfo()->GetName(), historyInfo->GetNZBInfo()->GetDupeKey(), nzbName, dupeKey))
		{
			if (!dupeFound || historyInfo->GetNZBInfo()->GetDupeScore() > historyScore)
			{
				historyScore = historyInfo->GetNZBInfo()->GetDupeScore();
			}
			dupeFound = true;
			goodDupe = historyInfo->GetNZBInfo()->GetMarkStatus() == NZBInfo::ksGood;
		}

		if (historyInfo->GetKind() == HistoryInfo::hkDup &&
			historyInfo->GetDupInfo()->GetDupeMode() != dmForce &&
			(historyInfo->GetDupInfo()->GetStatus() == DupInfo::dsSuccess ||
			 historyInfo->GetDupInfo()->GetStatus() == DupInfo::dsGood) &&
			SameNameOrKey(historyInfo->GetDupInfo()->GetName(), historyInfo->GetDupInfo()->GetDupeKey(), nzbName, dupeKey))
		{
			if (!dupeFound || historyInfo->GetDupInfo()->GetDupeScore() > historyScore)
			{
				historyScore = historyInfo->GetDupInfo()->GetDupeScore();
			}
			dupeFound = true;
			goodDupe = historyInfo->GetDupInfo()->GetStatus() == DupInfo::dsGood;
		}

		if (goodDupe)
		{
			// another duplicate with good-status exists - exit without moving other dupes to queue
			return;
		}
	}

	// check if duplicates exist in download queue
	bool queueDupe = false;
	int queueScore = 0;
	for (NZBList::iterator it = downloadQueue->GetQueue()->begin(); it != downloadQueue->GetQueue()->end(); it++)
	{
		NZBInfo* queuedNzbInfo = *it;
		if (queuedNzbInfo != nzbInfo &&
			queuedNzbInfo->GetKind() == NZBInfo::nkNzb &&
			queuedNzbInfo->GetDupeMode() != dmForce &&
			SameNameOrKey(queuedNzbInfo->GetName(), queuedNzbInfo->GetDupeKey(), nzbName, dupeKey) &&
			(!queueDupe || queuedNzbInfo->GetDupeScore() > queueScore))
		{
			queueScore = queuedNzbInfo->GetDupeScore();
			queueDupe = true;
		}
	}

	// find dupe-backup with highest score, whose score is also higher than other
	// success-duplicates and higher than already queued items
	HistoryInfo* historyDupe = NULL;
	for (HistoryList::iterator it = downloadQueue->GetHistory()->begin(); it != downloadQueue->GetHistory()->end(); it++)
	{
		HistoryInfo* historyInfo = *it;
		if (historyInfo->GetKind() == HistoryInfo::hkNzb &&
			historyInfo->GetNZBInfo()->GetDupeMode() != dmForce &&
			historyInfo->GetNZBInfo()->GetDeleteStatus() == NZBInfo::dsDupe &&
			historyInfo->GetNZBInfo()->CalcHealth() >= historyInfo->GetNZBInfo()->CalcCriticalHealth(true) &&
			historyInfo->GetNZBInfo()->GetMarkStatus() != NZBInfo::ksBad &&
			(!dupeFound || historyInfo->GetNZBInfo()->GetDupeScore() > historyScore) &&
			(!queueDupe || historyInfo->GetNZBInfo()->GetDupeScore() > queueScore) &&
			(!historyDupe || historyInfo->GetNZBInfo()->GetDupeScore() > historyDupe->GetNZBInfo()->GetDupeScore()) &&
			SameNameOrKey(historyInfo->GetNZBInfo()->GetName(), historyInfo->GetNZBInfo()->GetDupeKey(), nzbName, dupeKey))
		{
			historyDupe = historyInfo;
		}
	}

	// move that dupe-backup from history to download queue
	if (historyDupe)
	{
		info("Found duplicate %s for %s", historyDupe->GetNZBInfo()->GetName(), nzbName);
		g_pHistoryCoordinator->Redownload(downloadQueue, historyDupe);
	}
}

void DupeCoordinator::HistoryMark(DownloadQueue* downloadQueue, HistoryInfo* historyInfo, NZBInfo::EMarkStatus markStatus)
{
	char nzbName[1024];
	historyInfo->GetName(nzbName, 1024);

    const char* markStatusName[] = { "NONE", "bad", "good", "success" };

	info("Marking %s as %s", nzbName, markStatusName[markStatus]);

	if (historyInfo->GetKind() == HistoryInfo::hkNzb)
	{
		historyInfo->GetNZBInfo()->SetMarkStatus(markStatus);
	}
	else if (historyInfo->GetKind() == HistoryInfo::hkDup)
	{
		historyInfo->GetDupInfo()->SetStatus(
			markStatus == NZBInfo::ksGood ? DupInfo::dsGood :
			markStatus == NZBInfo::ksSuccess ? DupInfo::dsSuccess :
			DupInfo::dsBad);
	}
	else
	{
		error("Could not mark %s as bad: history item has wrong type", nzbName);
		return;
	}

	if (!g_pOptions->GetDupeCheck() ||
		(historyInfo->GetKind() == HistoryInfo::hkNzb &&
		 historyInfo->GetNZBInfo()->GetDupeMode() == dmForce) ||
		(historyInfo->GetKind() == HistoryInfo::hkDup &&
		 historyInfo->GetDupInfo()->GetDupeMode() == dmForce))
	{
		return;
	}

	if (markStatus == NZBInfo::ksGood)
	{
		// mark as good
		// moving all duplicates from history to dup-history
		HistoryCleanup(downloadQueue, historyInfo);
	}
	else if (markStatus == NZBInfo::ksBad)
	{
		// mark as bad
		const char* dupeKey = historyInfo->GetKind() == HistoryInfo::hkNzb ? historyInfo->GetNZBInfo()->GetDupeKey() :
			historyInfo->GetKind() == HistoryInfo::hkDup ? historyInfo->GetDupInfo()->GetDupeKey() :
			NULL;
		ReturnBestDupe(downloadQueue, NULL, nzbName, dupeKey);
	}
}

void DupeCoordinator::HistoryCleanup(DownloadQueue* downloadQueue, HistoryInfo* markHistoryInfo)
{
	const char* dupeKey = markHistoryInfo->GetKind() == HistoryInfo::hkNzb ? markHistoryInfo->GetNZBInfo()->GetDupeKey() :
		markHistoryInfo->GetKind() == HistoryInfo::hkDup ? markHistoryInfo->GetDupInfo()->GetDupeKey() :
		NULL;
	const char* nzbName = markHistoryInfo->GetKind() == HistoryInfo::hkNzb ? markHistoryInfo->GetNZBInfo()->GetName() :
		markHistoryInfo->GetKind() == HistoryInfo::hkDup ? markHistoryInfo->GetDupInfo()->GetName() :
		NULL;
	bool changed = false;
	int index = 0;

	// traversing in a reverse order to delete items in order they were added to history
	// (just to produce the log-messages in a more logical order)
	for (HistoryList::reverse_iterator it = downloadQueue->GetHistory()->rbegin(); it != downloadQueue->GetHistory()->rend(); )
	{
		HistoryInfo* historyInfo = *it;

		if (historyInfo->GetKind() == HistoryInfo::hkNzb &&
			historyInfo->GetNZBInfo()->GetDupeMode() != dmForce &&
			historyInfo->GetNZBInfo()->GetDeleteStatus() == NZBInfo::dsDupe &&
			historyInfo != markHistoryInfo &&
			SameNameOrKey(historyInfo->GetNZBInfo()->GetName(), historyInfo->GetNZBInfo()->GetDupeKey(), nzbName, dupeKey))
		{
			g_pHistoryCoordinator->HistoryHide(downloadQueue, historyInfo, index);
			index++;
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
}

DupeCoordinator::EDupeStatus DupeCoordinator::GetDupeStatus(DownloadQueue* downloadQueue,
	const char* name, const char* dupeKey)
{
	EDupeStatus statuses = dsNone;

	// find duplicates in download queue
	for (NZBList::iterator it = downloadQueue->GetQueue()->begin(); it != downloadQueue->GetQueue()->end(); it++)
	{
		NZBInfo* nzbInfo = *it;
		if (SameNameOrKey(name, dupeKey, nzbInfo->GetName(), nzbInfo->GetDupeKey()))
		{
			if (nzbInfo->GetSuccessArticles() + nzbInfo->GetFailedArticles() > 0)
			{
				statuses = (EDupeStatus)(statuses | dsDownloading);
			}
			else
			{
				statuses = (EDupeStatus)(statuses | dsQueued);
			}
		}
	}

	// find duplicates in history
	for (HistoryList::iterator it = downloadQueue->GetHistory()->begin(); it != downloadQueue->GetHistory()->end(); it++)
	{
		HistoryInfo* historyInfo = *it;

		if (historyInfo->GetKind() == HistoryInfo::hkNzb &&
			SameNameOrKey(name, dupeKey, historyInfo->GetNZBInfo()->GetName(), historyInfo->GetNZBInfo()->GetDupeKey()))
		{
			const char* textStatus = historyInfo->GetNZBInfo()->MakeTextStatus(true);
			if (!strncasecmp(textStatus, "SUCCESS", 7))
			{
				statuses = (EDupeStatus)(statuses | dsSuccess);
			}
			else if (!strncasecmp(textStatus, "FAILURE", 7))
			{
				statuses = (EDupeStatus)(statuses | dsFailure);
			}
			else if (!strncasecmp(textStatus, "WARNING", 7))
			{
				statuses = (EDupeStatus)(statuses | dsWarning);
			}
		}

		if (historyInfo->GetKind() == HistoryInfo::hkDup &&
			SameNameOrKey(name, dupeKey, historyInfo->GetDupInfo()->GetName(), historyInfo->GetDupInfo()->GetDupeKey()))
		{
			if (historyInfo->GetDupInfo()->GetStatus() == DupInfo::dsSuccess ||
				historyInfo->GetDupInfo()->GetStatus() == DupInfo::dsGood)
			{
				statuses = (EDupeStatus)(statuses | dsSuccess);
			}
			else if (historyInfo->GetDupInfo()->GetStatus() == DupInfo::dsFailed ||
					 historyInfo->GetDupInfo()->GetStatus() == DupInfo::dsBad)
			{
				statuses = (EDupeStatus)(statuses | dsFailure);
			}
		}
	}

	return statuses;
}

void DupeCoordinator::ListHistoryDupes(DownloadQueue* downloadQueue, NZBInfo* nzbInfo, NZBList* dupeList)
{
	if (nzbInfo->GetDupeMode() == dmForce)
	{
		return;
	}

	// find duplicates in history
	for (HistoryList::iterator it = downloadQueue->GetHistory()->begin(); it != downloadQueue->GetHistory()->end(); it++)
	{
		HistoryInfo* historyInfo = *it;

		if (historyInfo->GetKind() == HistoryInfo::hkNzb &&
			historyInfo->GetNZBInfo()->GetDupeMode() != dmForce &&
			SameNameOrKey(historyInfo->GetNZBInfo()->GetName(), historyInfo->GetNZBInfo()->GetDupeKey(),
				nzbInfo->GetName(), nzbInfo->GetDupeKey()))
		{
			dupeList->push_back(historyInfo->GetNZBInfo());
		}
	}
}
