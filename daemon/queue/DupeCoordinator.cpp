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
#include "Options.h"
#include "Log.h"
#include "Util.h"
#include "NzbFile.h"
#include "HistoryCoordinator.h"
#include "DupeCoordinator.h"
#include "QueueScript.h"

bool DupeCoordinator::SameNameOrKey(const char* name1, const char* dupeKey1,
	const char* name2, const char* dupeKey2)
{
	bool hasDupeKeys = !Util::EmptyStr(dupeKey1) && !Util::EmptyStr(dupeKey2);
	return (hasDupeKeys && !strcasecmp(dupeKey1, dupeKey2)) ||
		(!hasDupeKeys && !strcasecmp(name1, name2));
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
void DupeCoordinator::NzbFound(DownloadQueue* downloadQueue, NzbInfo* nzbInfo)
{
	debug("Checking duplicates for %s", nzbInfo->GetName());

	// find duplicates in download queue with exactly same content
	for (NzbInfo* queuedNzbInfo : downloadQueue->GetQueue())
	{
		bool sameContent = (nzbInfo->GetFullContentHash() > 0 &&
			nzbInfo->GetFullContentHash() == queuedNzbInfo->GetFullContentHash()) ||
			(nzbInfo->GetFilteredContentHash() > 0 &&
			 nzbInfo->GetFilteredContentHash() == queuedNzbInfo->GetFilteredContentHash());

		// if there is a duplicate with exactly same content (via hash-check)
		// in queue - the new item is skipped
		if (queuedNzbInfo != nzbInfo && sameContent && nzbInfo->GetKind() == NzbInfo::nkNzb)
		{
			BString<1024> message;
			if (!strcmp(nzbInfo->GetName(), queuedNzbInfo->GetName()))
			{
				message.Format("Skipping duplicate %s, already queued", nzbInfo->GetName());
			}
			else
			{
				message.Format("Skipping duplicate %s, already queued as %s",
					nzbInfo->GetName(), queuedNzbInfo->GetName());
			}

			if (nzbInfo->GetFeedId())
			{
				warn("%s", *message);
				// Flag saying QueueCoordinator to skip nzb-file
				nzbInfo->SetDeleteStatus(NzbInfo::dsManual);
				g_HistoryCoordinator->DeleteDiskFiles(nzbInfo);
			}
			else
			{
				nzbInfo->SetDeleteStatus(NzbInfo::dsCopy);
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
		for (NzbInfo* queuedNzbInfo : downloadQueue->GetQueue())
		{
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
		for (HistoryInfo* historyInfo : downloadQueue->GetHistory())
		{
			if (historyInfo->GetKind() == HistoryInfo::hkNzb &&
				!strcmp(historyInfo->GetNzbInfo()->GetName(), nzbInfo->GetName()) &&
				(!Util::EmptyStr(historyInfo->GetNzbInfo()->GetDupeKey()) || historyInfo->GetNzbInfo()->GetDupeScore() != 0))
			{
				nzbInfo->SetDupeKey(historyInfo->GetNzbInfo()->GetDupeKey());
				nzbInfo->SetDupeScore(historyInfo->GetNzbInfo()->GetDupeScore());
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
	const char* dupeName = nullptr;

	// find duplicates in history having exactly same content
	// also: nzb-files having duplicates marked as good are skipped
	// also (only in score mode): nzb-files having success-duplicates in dup-history but not having duplicates in recent history are skipped
	for (HistoryInfo* historyInfo : downloadQueue->GetHistory())
	{
		if (historyInfo->GetKind() == HistoryInfo::hkNzb &&
			((nzbInfo->GetFullContentHash() > 0 &&
			nzbInfo->GetFullContentHash() == historyInfo->GetNzbInfo()->GetFullContentHash()) ||
			(nzbInfo->GetFilteredContentHash() > 0 &&
			 nzbInfo->GetFilteredContentHash() == historyInfo->GetNzbInfo()->GetFilteredContentHash())))
		{
			skip = true;
			sameContent = true;
			dupeName = historyInfo->GetNzbInfo()->GetName();
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
			historyInfo->GetNzbInfo()->GetDupeMode() != dmForce &&
			historyInfo->GetNzbInfo()->GetMarkStatus() == NzbInfo::ksGood &&
			SameNameOrKey(historyInfo->GetNzbInfo()->GetName(), historyInfo->GetNzbInfo()->GetDupeKey(),
				nzbInfo->GetName(), nzbInfo->GetDupeKey()))
		{
			skip = true;
			good = true;
			dupeName = historyInfo->GetNzbInfo()->GetName();
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

	if (!sameContent && nzbInfo->GetDupeHint() != NzbInfo::dhNone)
	{
		// dupe check when "download again" URLs: checking same content only
		return;
	}

	if (!sameContent && !good && nzbInfo->GetDupeMode() == dmScore)
	{
		// nzb-files having success-duplicates in recent history (with different content) are added to history for backup
		for (HistoryInfo* historyInfo : downloadQueue->GetHistory())
		{
			if ((historyInfo->GetKind() == HistoryInfo::hkNzb ||
				 historyInfo->GetKind() == HistoryInfo::hkUrl) &&
				historyInfo->GetNzbInfo()->GetDupeMode() != dmForce &&
				SameNameOrKey(historyInfo->GetNzbInfo()->GetName(), historyInfo->GetNzbInfo()->GetDupeKey(),
					nzbInfo->GetName(), nzbInfo->GetDupeKey()) &&
				nzbInfo->GetDupeScore() <= historyInfo->GetNzbInfo()->GetDupeScore() &&
				historyInfo->GetNzbInfo()->IsDupeSuccess())
			{
				// Flag saying QueueCoordinator to skip nzb-file
				nzbInfo->SetDeleteStatus(NzbInfo::dsDupe);
				info("Collection %s is a duplicate to %s", nzbInfo->GetName(), historyInfo->GetNzbInfo()->GetName());
				return;
			}
		}
	}

	if (skip)
	{
		BString<1024> message;
		if (!strcmp(nzbInfo->GetName(), dupeName))
		{
			message.Format("Skipping duplicate %s, found in history with %s", nzbInfo->GetName(),
				sameContent ? "exactly same content" : good ? "good status" : "success status");
		}
		else
		{
			message.Format("Skipping duplicate %s, found in history %s with %s",
				nzbInfo->GetName(), dupeName,
				sameContent ? "exactly same content" : good ? "good status" : "success status");
		}

		if (nzbInfo->GetFeedId() && nzbInfo->GetDupeHint() == NzbInfo::dhNone)
		{
			warn("%s", *message);
			// Flag saying QueueCoordinator to skip nzb-file
			nzbInfo->SetDeleteStatus(NzbInfo::dsManual);
			g_HistoryCoordinator->DeleteDiskFiles(nzbInfo);
		}
		else
		{
			nzbInfo->SetDeleteStatus(sameContent ? NzbInfo::dsCopy : NzbInfo::dsGood);
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
		for (NzbList::iterator it = downloadQueue->GetQueue()->begin(); it != downloadQueue->GetQueue()->end(); index++)
		{
			NzbInfo* queuedNzbInfo = (*it++).get();
			if (queuedNzbInfo != nzbInfo &&
				queuedNzbInfo->GetDeleteStatus() == NzbInfo::dsNone &&
				(queuedNzbInfo->GetKind() == NzbInfo::nkNzb ||
				 (queuedNzbInfo->GetKind() == NzbInfo::nkUrl && nzbInfo->GetKind() == NzbInfo::nkUrl)) &&
				queuedNzbInfo->GetDupeMode() != dmForce &&
				SameNameOrKey(queuedNzbInfo->GetName(), queuedNzbInfo->GetDupeKey(),
					nzbInfo->GetName(), nzbInfo->GetDupeKey()))
			{
				// if queue has a duplicate with the same or higher score - the new item
				// is moved to history as dupe-backup
				if (nzbInfo->GetDupeScore() <= queuedNzbInfo->GetDupeScore())
				{
					// Flag saying QueueCoordinator to skip nzb-file
					nzbInfo->SetDeleteStatus(NzbInfo::dsDupe);
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
					queuedNzbInfo->SetDeleteStatus(NzbInfo::dsDupe);
					int oldSize = downloadQueue->GetQueue()->size();
					downloadQueue->EditEntry(queuedNzbInfo->GetId(),
						DownloadQueue::eaGroupDelete, nullptr);
					int newSize = downloadQueue->GetQueue()->size();
					index += oldSize == newSize ? 1 : 0;
					it = downloadQueue->GetQueue()->begin() + index;
					index--;
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
void DupeCoordinator::NzbCompleted(DownloadQueue* downloadQueue, NzbInfo* nzbInfo)
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
void DupeCoordinator::ReturnBestDupe(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, const char* nzbName, const char* dupeKey)
{
	// check if history (recent or dup) has other success-duplicates or good-duplicates
	bool dupeFound = false;
	int historyScore = 0;
	for (HistoryInfo* historyInfo : downloadQueue->GetHistory())
	{
		bool goodDupe = false;

		if (historyInfo->GetKind() == HistoryInfo::hkNzb &&
			historyInfo->GetNzbInfo()->GetDupeMode() != dmForce &&
			historyInfo->GetNzbInfo()->IsDupeSuccess() &&
			SameNameOrKey(historyInfo->GetNzbInfo()->GetName(), historyInfo->GetNzbInfo()->GetDupeKey(), nzbName, dupeKey))
		{
			if (!dupeFound || historyInfo->GetNzbInfo()->GetDupeScore() > historyScore)
			{
				historyScore = historyInfo->GetNzbInfo()->GetDupeScore();
			}
			dupeFound = true;
			goodDupe = historyInfo->GetNzbInfo()->GetMarkStatus() == NzbInfo::ksGood;
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
	for (NzbInfo* queuedNzbInfo : downloadQueue->GetQueue())
	{
		if (queuedNzbInfo != nzbInfo &&
			queuedNzbInfo->GetKind() == NzbInfo::nkNzb &&
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
	HistoryInfo* historyDupe = nullptr;
	for (HistoryInfo* historyInfo : downloadQueue->GetHistory())
	{
		if ((historyInfo->GetKind() == HistoryInfo::hkNzb ||
			 historyInfo->GetKind() == HistoryInfo::hkUrl) &&
			historyInfo->GetNzbInfo()->GetDupeMode() != dmForce &&
			historyInfo->GetNzbInfo()->GetDeleteStatus() == NzbInfo::dsDupe &&
			historyInfo->GetNzbInfo()->CalcHealth() >= historyInfo->GetNzbInfo()->CalcCriticalHealth(true) &&
			historyInfo->GetNzbInfo()->GetMarkStatus() != NzbInfo::ksBad &&
			(!dupeFound || historyInfo->GetNzbInfo()->GetDupeScore() > historyScore) &&
			(!queueDupe || historyInfo->GetNzbInfo()->GetDupeScore() > queueScore) &&
			(!historyDupe || historyInfo->GetNzbInfo()->GetDupeScore() > historyDupe->GetNzbInfo()->GetDupeScore()) &&
			SameNameOrKey(historyInfo->GetNzbInfo()->GetName(), historyInfo->GetNzbInfo()->GetDupeKey(), nzbName, dupeKey))
		{
			historyDupe = historyInfo;
		}
	}

	// move that dupe-backup from history to download queue
	if (historyDupe)
	{
		info("Found duplicate %s for %s", historyDupe->GetNzbInfo()->GetName(), nzbName);
		historyDupe->GetNzbInfo()->SetDupeHint(NzbInfo::dhRedownloadAuto);
		g_HistoryCoordinator->Redownload(downloadQueue, historyDupe);
	}
}

void DupeCoordinator::HistoryMark(DownloadQueue* downloadQueue, HistoryInfo* historyInfo, NzbInfo::EMarkStatus markStatus)
{
	const char* markStatusName[] = { "NONE", "bad", "good", "success" };

	info("Marking %s as %s", historyInfo->GetName(), markStatusName[markStatus]);

	if (historyInfo->GetKind() == HistoryInfo::hkNzb)
	{
		historyInfo->GetNzbInfo()->SetMarkStatus(markStatus);
		g_QueueScriptCoordinator->EnqueueScript(historyInfo->GetNzbInfo(), QueueScriptCoordinator::qeNzbMarked);
	}
	else if (historyInfo->GetKind() == HistoryInfo::hkDup)
	{
		historyInfo->GetDupInfo()->SetStatus(
			markStatus == NzbInfo::ksGood ? DupInfo::dsGood :
			markStatus == NzbInfo::ksSuccess ? DupInfo::dsSuccess :
			DupInfo::dsBad);
	}
	else
	{
		error("Could not mark %s as bad: history item has wrong type", historyInfo->GetName());
		return;
	}

	if (!g_Options->GetDupeCheck() ||
		(historyInfo->GetKind() == HistoryInfo::hkNzb &&
		 historyInfo->GetNzbInfo()->GetDupeMode() == dmForce) ||
		(historyInfo->GetKind() == HistoryInfo::hkDup &&
		 historyInfo->GetDupInfo()->GetDupeMode() == dmForce))
	{
		return;
	}

	if (markStatus == NzbInfo::ksGood)
	{
		// mark as good
		// moving all duplicates from history to dup-history
		HistoryCleanup(downloadQueue, historyInfo);
	}
	else if (markStatus == NzbInfo::ksBad)
	{
		// mark as bad
		const char* dupeKey = historyInfo->GetKind() == HistoryInfo::hkNzb ? historyInfo->GetNzbInfo()->GetDupeKey() :
			historyInfo->GetKind() == HistoryInfo::hkDup ? historyInfo->GetDupInfo()->GetDupeKey() :
			nullptr;
		ReturnBestDupe(downloadQueue, nullptr, historyInfo->GetName(), dupeKey);
	}
}

void DupeCoordinator::HistoryCleanup(DownloadQueue* downloadQueue, HistoryInfo* markHistoryInfo)
{
	const char* dupeKey = markHistoryInfo->GetKind() == HistoryInfo::hkNzb ? markHistoryInfo->GetNzbInfo()->GetDupeKey() :
		markHistoryInfo->GetKind() == HistoryInfo::hkDup ? markHistoryInfo->GetDupInfo()->GetDupeKey() :
		nullptr;
	const char* nzbName = markHistoryInfo->GetKind() == HistoryInfo::hkNzb ? markHistoryInfo->GetNzbInfo()->GetName() :
		markHistoryInfo->GetKind() == HistoryInfo::hkDup ? markHistoryInfo->GetDupInfo()->GetName() :
		nullptr;
	bool changed = false;
	int index = 0;

	// traversing in a reverse order to delete items in order they were added to history
	// (just to produce the log-messages in a more logical order)
	for (HistoryList::reverse_iterator it = downloadQueue->GetHistory()->rbegin(); it != downloadQueue->GetHistory()->rend(); )
	{
		HistoryInfo* historyInfo = (*it).get();

		if ((historyInfo->GetKind() == HistoryInfo::hkNzb ||
			 historyInfo->GetKind() == HistoryInfo::hkUrl) &&
			historyInfo->GetNzbInfo()->GetDupeMode() != dmForce &&
			historyInfo->GetNzbInfo()->GetDeleteStatus() == NzbInfo::dsDupe &&
			historyInfo != markHistoryInfo &&
			SameNameOrKey(historyInfo->GetNzbInfo()->GetName(), historyInfo->GetNzbInfo()->GetDupeKey(), nzbName, dupeKey))
		{
			g_HistoryCoordinator->HistoryHide(downloadQueue, historyInfo, index);
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
		downloadQueue->HistoryChanged();
		downloadQueue->Save();
	}
}

DupeCoordinator::EDupeStatus DupeCoordinator::GetDupeStatus(DownloadQueue* downloadQueue,
	const char* name, const char* dupeKey)
{
	EDupeStatus statuses = dsNone;

	// find duplicates in download queue
	for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
	{
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
	for (HistoryInfo* historyInfo : downloadQueue->GetHistory())
	{
		if (historyInfo->GetKind() == HistoryInfo::hkNzb &&
			SameNameOrKey(name, dupeKey, historyInfo->GetNzbInfo()->GetName(), historyInfo->GetNzbInfo()->GetDupeKey()))
		{
			const char* textStatus = historyInfo->GetNzbInfo()->MakeTextStatus(true);
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

RawNzbList DupeCoordinator::ListHistoryDupes(DownloadQueue* downloadQueue, NzbInfo* nzbInfo)
{
	RawNzbList dupeList;

	if (nzbInfo->GetDupeMode() == dmForce)
	{
		return dupeList;
	}

	// find duplicates in history
	for (HistoryInfo* historyInfo : downloadQueue->GetHistory())
	{
		if (historyInfo->GetKind() == HistoryInfo::hkNzb &&
			historyInfo->GetNzbInfo()->GetDupeMode() != dmForce &&
			SameNameOrKey(historyInfo->GetNzbInfo()->GetName(), historyInfo->GetNzbInfo()->GetDupeKey(),
				nzbInfo->GetName(), nzbInfo->GetDupeKey()))
		{
			dupeList.push_back(historyInfo->GetNzbInfo());
		}
	}

	return dupeList;
}
