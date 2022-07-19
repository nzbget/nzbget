/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2005 Bo Cordes Petersen <placebodk@users.sourceforge.net>
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
#include "QueueCoordinator.h"
#include "Options.h"
#include "WorkState.h"
#include "ServerPool.h"
#include "ArticleDownloader.h"
#include "ArticleWriter.h"
#include "DiskState.h"
#include "Util.h"
#include "FileSystem.h"
#include "Decoder.h"
#include "StatMeter.h"

bool QueueCoordinator::CoordinatorDownloadQueue::EditEntry(
	int ID, EEditAction action, const char* args)
{
	return m_owner->m_queueEditor.EditEntry(&m_owner->m_downloadQueue, ID, action, args);
}

bool QueueCoordinator::CoordinatorDownloadQueue::EditList(
	IdList* idList, NameList* nameList, EMatchMode matchMode, EEditAction action, const char* args)
{
	m_massEdit = true;
	bool ret = m_owner->m_queueEditor.EditList(&m_owner->m_downloadQueue, idList, nameList, matchMode, action, args);
	m_massEdit = false;
	if (m_wantSave)
	{
		Save();
	}
	return ret;
}

void QueueCoordinator::CoordinatorDownloadQueue::Save()
{
	if (m_massEdit)
	{
		m_wantSave = true;
		return;
	}

	if (g_Options->GetServerMode())
	{
		g_DiskState->SaveDownloadQueue(this, m_historyChanged);
		m_stateChanged = true;
	}

	for (NzbInfo* nzbInfo : GetQueue())
	{
		nzbInfo->SetChanged(false);
	}

	m_wantSave = false;
	m_historyChanged = false;

	// queue has changed, time to wake up if in standby
	m_owner->WakeUp();
}

void QueueCoordinator::CoordinatorDownloadQueue::SaveChanged()
{
	if (g_Options->GetServerMode())
	{
		g_DiskState->SaveDownloadProgress(this);
		m_stateChanged = true;
	}
}

QueueCoordinator::QueueCoordinator()
{
	debug("Creating QueueCoordinator");

	CoordinatorDownloadQueue::Init(&m_downloadQueue);
	g_WorkState->Attach(this);
}

QueueCoordinator::~QueueCoordinator()
{
	debug("Destroying QueueCoordinator");

	for (ArticleDownloader* articleDownloader : m_activeDownloads)
	{
		delete articleDownloader;
	}
	m_activeDownloads.clear();

	CoordinatorDownloadQueue::Final();

	debug("QueueCoordinator destroyed");
}

void QueueCoordinator::Load()
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	bool statLoaded = true;
	bool perfectServerMatch = true;
	bool queueLoaded = false;

	if (g_Options->GetServerMode())
	{
		statLoaded = g_StatMeter->Load(&perfectServerMatch);

		if (g_DiskState->DownloadQueueExists())
		{
			queueLoaded = g_DiskState->LoadDownloadQueue(downloadQueue, g_ServerPool->GetServers());
		}
		else
		{
			g_DiskState->DiscardDownloadQueue();
		}
	}

	if (queueLoaded && statLoaded)
	{
		g_DiskState->CleanupTempDir(downloadQueue);
	}

	if (queueLoaded && statLoaded && !perfectServerMatch)
	{
		debug("Changes in section <NEWS SERVERS> of config file detected, resaving queue");

		// re-save current server list into diskstate to update server ids
		g_StatMeter->Save();

		// re-save queue into diskstate to update server ids
		downloadQueue->HistoryChanged();
		downloadQueue->Save();

		// re-save file states into diskstate to update server ids
		if (g_Options->GetServerMode())
		{
			for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
			{
				if (g_Options->GetContinuePartial())
				{
					for (FileInfo* fileInfo : nzbInfo->GetFileList())
					{
						if (!fileInfo->GetArticles()->empty())
						{
							g_DiskState->SaveFileState(fileInfo, false);
						}
					}
				}

				for (CompletedFile& completedFile : nzbInfo->GetCompletedFiles())
				{
					if ((completedFile.GetStatus() == CompletedFile::cfPartial ||
						 completedFile.GetStatus() == CompletedFile::cfFailure) &&
						completedFile.GetId() > 0)
					{
						FileInfo fileInfo(completedFile.GetId());
						if (g_DiskState->LoadFileState(&fileInfo, g_ServerPool->GetServers(), true))
						{
							g_DiskState->SaveFileState(&fileInfo, true);
						}
					}
				}
			}
		}
	}

	CoordinatorDownloadQueue::Loaded();
}

void QueueCoordinator::Run()
{
	debug("Entering QueueCoordinator-loop");

	Load();
	AdjustDownloadsLimit();
	bool wasStandBy = true;
	bool articeDownloadsRunning = false;
	time_t lastReset = 0;
	g_StatMeter->IntervalCheck();
	int waitInterval = 100;

	while (!IsStopped())
	{
		bool downloadsChecked = false;
		bool downloadStarted = false;
		NntpConnection* connection = g_ServerPool->GetConnection(0, nullptr, nullptr);
		if (connection)
		{
			// start download for next article
			FileInfo* fileInfo;
			ArticleInfo* articleInfo;
			bool freeConnection = false;

			{
				GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
				bool hasMoreArticles = GetNextArticle(downloadQueue, fileInfo, articleInfo);
				articeDownloadsRunning = !m_activeDownloads.empty();
				downloadsChecked = true;
				m_hasMoreJobs = hasMoreArticles || articeDownloadsRunning;
				if (hasMoreArticles && !IsStopped() && (int)m_activeDownloads.size() < m_downloadsLimit &&
					(!g_WorkState->GetTempPauseDownload() || fileInfo->GetExtraPriority()))
				{
					StartArticleDownload(fileInfo, articleInfo, connection);
					articeDownloadsRunning = true;
					downloadStarted = true;
				}
				else
				{
					freeConnection = true;
				}
			}

			if (freeConnection)
			{
				g_ServerPool->FreeConnection(connection, false);
			}
		}

		if (!downloadsChecked)
		{
			GuardedDownloadQueue guard = DownloadQueue::Guard();
			articeDownloadsRunning = !m_activeDownloads.empty();
		}

		bool standBy = !articeDownloadsRunning;
		if (standBy != wasStandBy)
		{
			g_StatMeter->EnterLeaveStandBy(standBy);
			g_WorkState->SetDownloading(!standBy);
			wasStandBy = standBy;
			if (standBy)
			{
				SaveAllPartialState();
			}
		}

		// sleep longer in StandBy
		if (standBy)
		{
			Guard guard(m_waitMutex);
			// sleeping max. 2 seconds; can't sleep much longer because we can't rely on
			// notifications from 'WorkState' and we also have periodical work to do here
			waitInterval = std::min(waitInterval * 2, 2000);
			m_waitCond.WaitFor(m_waitMutex, waitInterval, [&]{ return m_hasMoreJobs || IsStopped(); });
		}
		else
		{
			int sleepInterval = downloadStarted ? 0 : 5;
			Util::Sleep(sleepInterval);
			g_StatMeter->AddSpeedReading(0);
			waitInterval = 100;
		}

		if (lastReset != Util::CurrentTime())
		{
			// this code should not be called too often, once per second is OK
			g_ServerPool->CloseUnusedConnections();
			ResetHangingDownloads();
			if (!standBy)
			{
				SaveAllPartialState();
			}
			g_StatMeter->IntervalCheck();
			g_Log->IntervalCheck();
			AdjustDownloadsLimit();
			Util::SetStandByMode(standBy);
			lastReset = Util::CurrentTime();
		}
	}

	WaitJobs();
	SaveAllPartialState();
	SaveQueueIfChanged();
	SaveAllFileState();

	debug("Exiting QueueCoordinator-loop");
}

void QueueCoordinator::WakeUp()
{
	debug("Waking up QueueCoordinator");
	// Resume Run()
	Guard guard(m_waitMutex);
	m_hasMoreJobs = true;
	m_waitCond.NotifyAll();
}

void QueueCoordinator::WaitJobs()
{
	// waiting for downloads
	debug("QueueCoordinator: waiting for Downloads to complete");

	while (true)
	{
		{
			GuardedDownloadQueue guard = DownloadQueue::Guard();
			if (m_activeDownloads.empty())
			{
				break;
			}
		}
		Util::Sleep(100);
		ResetHangingDownloads();
	}

	debug("QueueCoordinator: Downloads are completed");
}

/*
 * Compute maximum number of allowed download threads
**/
void QueueCoordinator::AdjustDownloadsLimit()
{
	if (m_serverConfigGeneration == g_ServerPool->GetGeneration())
	{
		return;
	}

	// two extra threads for completing files (when connections are not needed)
	int downloadsLimit = 2;

	// allow one thread per 0-level (main) and 1-level (backup) server connection
	for (NewsServer* newsServer : g_ServerPool->GetServers())
	{
		if ((newsServer->GetNormLevel() == 0 || newsServer->GetNormLevel() == 1) && newsServer->GetActive())
		{
			downloadsLimit += newsServer->GetMaxConnections();
		}
	}

	m_downloadsLimit = downloadsLimit;
}

NzbInfo* QueueCoordinator::AddNzbFileToQueue(std::unique_ptr<NzbInfo> nzbInfo, NzbInfo* urlInfo, bool addFirst)
{
	debug("Adding NZBFile to queue");

	NzbInfo* addedNzb = nzbInfo.get();

	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	DownloadQueue::Aspect foundAspect = { DownloadQueue::eaNzbFound, downloadQueue, nzbInfo.get(), nullptr };
	downloadQueue->Notify(&foundAspect);

	NzbInfo::EDeleteStatus deleteStatus = nzbInfo->GetDeleteStatus();

	if (deleteStatus != NzbInfo::dsNone)
	{
		bool allPaused = !nzbInfo->GetFileList()->empty();
		for (FileInfo* fileInfo: nzbInfo->GetFileList())
		{
			allPaused &= fileInfo->GetPaused();
			if (g_Options->GetServerMode())
			{
				g_DiskState->DiscardFile(fileInfo->GetId(), true, false, false);
			}
		}
		nzbInfo->SetDeletePaused(allPaused);
	}

	if (deleteStatus == NzbInfo::dsNone)
	{
		if (g_Options->GetDupeCheck() && nzbInfo->GetDupeMode() != dmForce)
		{
			CheckDupeFileInfos(nzbInfo.get());
		}

		if (urlInfo)
		{
			// insert at the URL position
			downloadQueue->GetQueue()->insert(downloadQueue->GetQueue()->Find(urlInfo), std::move(nzbInfo));
		}
		else
		{
			downloadQueue->GetQueue()->Add(std::move(nzbInfo), addFirst);
		}
	}
	else
	{
		// temporary adding to queue in order for listeners to see it
		downloadQueue->GetQueue()->Add(std::move(nzbInfo), true);
	}

	if (urlInfo)
	{
		addedNzb->SetId(urlInfo->GetId());
		downloadQueue->GetQueue()->Remove(urlInfo);
	}

	if (deleteStatus == NzbInfo::dsNone)
	{
		addedNzb->PrintMessage(Message::mkInfo, "Collection %s added to queue", addedNzb->GetName());
	}

	if (deleteStatus != NzbInfo::dsManual)
	{
		DownloadQueue::Aspect addedAspect = { DownloadQueue::eaNzbAdded, downloadQueue, addedNzb, nullptr };
		downloadQueue->Notify(&addedAspect);
	}

	if (deleteStatus != NzbInfo::dsNone)
	{
		// in a case if none of listeners did already delete the temporary object - we do it ourselves
		downloadQueue->GetQueue()->Remove(addedNzb);
		if (!downloadQueue->GetHistory()->Find(addedNzb->GetId()))
		{
			addedNzb = nullptr;
		}
	}

	downloadQueue->Save();

	return addedNzb;
}

void QueueCoordinator::CheckDupeFileInfos(NzbInfo* nzbInfo)
{
	debug("CheckDupeFileInfos");

	RawFileList dupeList;

	int index1 = 0;
	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
		index1++;
		bool dupe = false;
		int index2 = 0;
		for (FileInfo* fileInfo2 : nzbInfo->GetFileList())
		{
			index2++;
			if (fileInfo != fileInfo2 &&
				!strcmp(fileInfo->GetFilename(), fileInfo2->GetFilename()) &&
				(fileInfo->GetSize() < fileInfo2->GetSize() ||
				 (fileInfo->GetSize() == fileInfo2->GetSize() && index2 < index1)))
			{
				// If more than two files have same filename we don't filter them out since that
				// naming might be intentional and correct filenames must be read from article bodies.
				int dupeCount = (int)std::count_if(nzbInfo->GetFileList()->begin(), nzbInfo->GetFileList()->end(),
					[fileInfo2](std::unique_ptr<FileInfo>& fileInfo3)
					{
						return !strcmp(fileInfo3->GetFilename(), fileInfo2->GetFilename());
					});
				if (dupeCount == 2)
				{
					warn("File \"%s\" appears twice in collection, adding only the biggest file", fileInfo->GetFilename());
					dupe = true;
					break;
				}
			}
		}
		if (dupe)
		{
			dupeList.push_back(fileInfo);
			continue;
		}
	}

	for (FileInfo* fileInfo : dupeList)
	{
		nzbInfo->UpdateDeletedStats(fileInfo);
		nzbInfo->GetFileList()->Remove(fileInfo);
		if (g_Options->GetServerMode())
		{
			g_DiskState->DiscardFile(fileInfo->GetId(), true, false, false);
		}
	}
}

void QueueCoordinator::Stop()
{
	Thread::Stop();

	debug("Stopping ArticleDownloads");
	{
		GuardedDownloadQueue guard = DownloadQueue::Guard();
		for (ArticleDownloader* articleDownloader : m_activeDownloads)
		{
			articleDownloader->Stop();
		}
	}
	debug("ArticleDownloads are notified");

	// Resume Run() to exit it
	Guard guard(m_waitMutex);
	m_waitCond.NotifyAll();
}

/*
 * Returns next article for download.
 */
bool QueueCoordinator::GetNextArticle(DownloadQueue* downloadQueue, FileInfo* &fileInfo, ArticleInfo* &articleInfo)
{
	// find an unpaused file with the highest priority, then take the next article from the file.
	// if the file doesn't have any articles left for download, we store that fact and search again,
	// ignoring all files which were previously marked as not having any articles.

	// special case: if the file has ExtraPriority-flag set, it has the highest priority.

	//debug("QueueCoordinator::GetNextArticle()");

	bool ok = false;

	RawFileList checkedFiles;
	time_t curDate = Util::CurrentTime();

	while (!ok)
	{
		fileInfo = nullptr;

		for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
		{
			bool nzbHigherPriority = fileInfo &&
				((nzbInfo->HasExtraPriority() == fileInfo->GetNzbInfo()->HasExtraPriority() &&
					nzbInfo->GetPriority() > fileInfo->GetNzbInfo()->GetPriority()) ||
					(nzbInfo->HasExtraPriority() > fileInfo->GetNzbInfo()->HasExtraPriority()));

			bool nzbPaused = nzbInfo->GetFileList()->size() - nzbInfo->GetPausedFileCount() <= 0;

			if ((!fileInfo || nzbHigherPriority) && !nzbPaused &&
				(!(g_WorkState->GetPauseDownload() || g_WorkState->GetQuotaReached()) || nzbInfo->GetForcePriority()))
			{
				for (FileInfo* fileInfo1 : nzbInfo->GetFileList())
				{
					bool alreadyChecked = !checkedFiles.empty() &&
						std::find(checkedFiles.begin(), checkedFiles.end(), fileInfo1) != checkedFiles.end();

					bool propagationWait = g_Options->GetPropagationDelay() > 0 &&
						(int)fileInfo1->GetTime() + g_Options->GetPropagationDelay() >= (int)curDate;

					bool higherPriority = fileInfo &&
						((fileInfo1->GetExtraPriority() == fileInfo->GetExtraPriority() &&
							fileInfo1->GetNzbInfo()->GetPriority() > fileInfo->GetNzbInfo()->GetPriority()) ||
							(fileInfo1->GetExtraPriority() > fileInfo->GetExtraPriority()));

					if (!alreadyChecked && !propagationWait && !fileInfo1->GetPaused() && 
						!fileInfo1->GetDeleted() && (!fileInfo || higherPriority))
					{
						fileInfo = fileInfo1;
					}
				}
			}
		}

		if (!fileInfo)
		{
			// there are no more files for download
			break;
		}

		if (g_Options->GetDirectRename() &&
			fileInfo->GetNzbInfo()->GetDirectRenameStatus() <= NzbInfo::tsRunning &&
			!fileInfo->GetNzbInfo()->GetAllFirst() &&
			GetNextFirstArticle(fileInfo->GetNzbInfo(), fileInfo, articleInfo))
		{
			return true;
		}

		if (fileInfo->GetArticles()->empty() && g_Options->GetServerMode())
		{
			g_DiskState->LoadArticles(fileInfo);
			LoadPartialState(fileInfo);
		}

		// check if the file has any articles left for download
		for (ArticleInfo* article : fileInfo->GetArticles())
		{
			if (article->GetStatus() == ArticleInfo::aiUndefined)
			{
				articleInfo = article;
				return true;
			}
		}

		if (!ok)
		{
			// the file doesn't have any articles left for download
			checkedFiles.reserve(100);
			checkedFiles.push_back(fileInfo);
		}
	}

	return false;
}

bool QueueCoordinator::GetNextFirstArticle(NzbInfo* nzbInfo, FileInfo* &fileInfo, ArticleInfo* &articleInfo)
{
	// find a file not renamed yet
	for (FileInfo* fileInfo1 : nzbInfo->GetFileList())
	{
		if (!fileInfo1->GetFilenameConfirmed())
		{
			if (fileInfo1->GetArticles()->empty() && g_Options->GetServerMode())
			{
				g_DiskState->LoadArticles(fileInfo1);
				LoadPartialState(fileInfo1);
			}
			if (!fileInfo1->GetArticles()->empty())
			{
				ArticleInfo* article = fileInfo1->GetArticles()->at(0).get();
				if (article->GetStatus() == ArticleInfo::aiUndefined)
				{
					fileInfo = fileInfo1;
					articleInfo = article;
					nzbInfo->SetDirectRenameStatus(NzbInfo::tsRunning);
					return true;
				}
			}
		}
	}

	// no more files for renaming remained
	nzbInfo->SetAllFirst(true);

	return false;
}

void QueueCoordinator::StartArticleDownload(FileInfo* fileInfo, ArticleInfo* articleInfo, NntpConnection* connection)
{
	debug("Starting new ArticleDownloader");

	ArticleDownloader* articleDownloader = new ArticleDownloader();
	articleDownloader->SetAutoDestroy(true);
	articleDownloader->Attach(this);
	articleDownloader->SetFileInfo(fileInfo);
	articleDownloader->SetArticleInfo(articleInfo);
	articleDownloader->SetConnection(connection);

	if (articleInfo->GetPartNumber() == 1 && g_Options->GetDirectRename() && !g_Options->GetRawArticle())
	{
		articleDownloader->SetContentAnalyzer(m_directRenamer.MakeArticleContentAnalyzer());
	}

	BString<1024> infoName("%s%c%s [%i/%i]", fileInfo->GetNzbInfo()->GetName(), PATH_SEPARATOR, fileInfo->GetFilename(), articleInfo->GetPartNumber(), (int)fileInfo->GetArticles()->size());
	articleDownloader->SetInfoName(infoName);

	articleInfo->SetStatus(ArticleInfo::aiRunning);
	fileInfo->SetActiveDownloads(fileInfo->GetActiveDownloads() + 1);
	fileInfo->GetNzbInfo()->SetActiveDownloads(fileInfo->GetNzbInfo()->GetActiveDownloads() + 1);

	m_activeDownloads.push_back(articleDownloader);
	articleDownloader->Start();
}

void QueueCoordinator::Update(Subject* caller, void* aspect)
{
	if (caller == g_WorkState)
	{
		debug("Notification from WorkState received");
		WakeUp();
		return;
	}

	debug("Notification from ArticleDownloader received");

	ArticleDownloader* articleDownloader = (ArticleDownloader*)caller;
	if ((articleDownloader->GetStatus() == ArticleDownloader::adFinished) ||
		(articleDownloader->GetStatus() == ArticleDownloader::adFailed) ||
		(articleDownloader->GetStatus() == ArticleDownloader::adRetry))
	{
		ArticleCompleted(articleDownloader);
	}
}

void QueueCoordinator::ArticleCompleted(ArticleDownloader* articleDownloader)
{
	debug("Article downloaded");

	FileInfo* fileInfo = articleDownloader->GetFileInfo();
	bool completeFileParts = false;

	{
		NzbInfo* nzbInfo = fileInfo->GetNzbInfo();
		ArticleInfo* articleInfo = articleDownloader->GetArticleInfo();
		bool retry = false;
		bool fileCompleted = false;

		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

		if (articleDownloader->GetStatus() == ArticleDownloader::adFinished)
		{
			articleInfo->SetStatus(ArticleInfo::aiFinished);
			fileInfo->SetSuccessSize(fileInfo->GetSuccessSize() + articleInfo->GetSize());
			nzbInfo->SetCurrentSuccessSize(nzbInfo->GetCurrentSuccessSize() + articleInfo->GetSize());
			nzbInfo->SetParCurrentSuccessSize(nzbInfo->GetParCurrentSuccessSize() + (fileInfo->GetParFile() ? articleInfo->GetSize() : 0));
			fileInfo->SetSuccessArticles(fileInfo->GetSuccessArticles() + 1);
			nzbInfo->SetCurrentSuccessArticles(nzbInfo->GetCurrentSuccessArticles() + 1);
		}
		else if (articleDownloader->GetStatus() == ArticleDownloader::adFailed)
		{
			articleInfo->SetStatus(ArticleInfo::aiFailed);
			fileInfo->SetFailedSize(fileInfo->GetFailedSize() + articleInfo->GetSize());
			nzbInfo->SetCurrentFailedSize(nzbInfo->GetCurrentFailedSize() + articleInfo->GetSize());
			nzbInfo->SetParCurrentFailedSize(nzbInfo->GetParCurrentFailedSize() + (fileInfo->GetParFile() ? articleInfo->GetSize() : 0));
			fileInfo->SetFailedArticles(fileInfo->GetFailedArticles() + 1);
			nzbInfo->SetCurrentFailedArticles(nzbInfo->GetCurrentFailedArticles() + 1);
		}
		else if (articleDownloader->GetStatus() == ArticleDownloader::adRetry)
		{
			articleInfo->SetStatus(ArticleInfo::aiUndefined);
			retry = true;
			if (articleInfo->GetPartNumber() == 1)
			{
				nzbInfo->SetAllFirst(false);
			}
		}

		if (!retry)
		{
			fileInfo->SetRemainingSize(fileInfo->GetRemainingSize() - articleInfo->GetSize());
			nzbInfo->SetRemainingSize(nzbInfo->GetRemainingSize() - articleInfo->GetSize());
			if (fileInfo->GetPaused())
			{
				nzbInfo->SetPausedSize(nzbInfo->GetPausedSize() - articleInfo->GetSize());
			}
			fileInfo->SetCompletedArticles(fileInfo->GetCompletedArticles() + 1);
			fileCompleted = (int)fileInfo->GetArticles()->size() == fileInfo->GetCompletedArticles();
			fileInfo->GetServerStats()->ListOp(articleDownloader->GetServerStats(), ServerStatList::soAdd);
			nzbInfo->GetCurrentServerStats()->ListOp(articleDownloader->GetServerStats(), ServerStatList::soAdd);
			fileInfo->SetPartialChanged(true);
		}

		if (!fileInfo->GetFilenameConfirmed() &&
			articleDownloader->GetStatus() == ArticleDownloader::adFinished &&
			articleDownloader->GetArticleFilename())
		{
			// in "FileNaming=auto"-mode prefer filename from nzb-file to filename read from article
			// if the name from article seems to be obfuscated
			bool useFilenameFromArticle = g_Options->GetFileNaming() == Options::nfArticle ||
				(g_Options->GetFileNaming() == Options::nfAuto &&
				 !Util::IsObfuscated(articleDownloader->GetArticleFilename()) &&
				 !nzbInfo->GetManyDupeFiles());
			if (useFilenameFromArticle)
			{
				fileInfo->SetFilename(articleDownloader->GetArticleFilename());
				fileInfo->MakeValidFilename();
			}
			fileInfo->SetFilenameConfirmed(true);
			if (g_Options->GetDupeCheck() &&
				nzbInfo->GetDupeMode() != dmForce &&
				!nzbInfo->GetManyDupeFiles() &&
				FileSystem::FileExists(BString<1024>("%s%c%s", nzbInfo->GetDestDir(), PATH_SEPARATOR, fileInfo->GetFilename())))
			{
				warn("File \"%s\" seems to be duplicate, cancelling download and deleting file from queue", fileInfo->GetFilename());
				fileCompleted = false;
				fileInfo->SetDupeDeleted(true);
				DeleteQueueEntry(downloadQueue, fileInfo);
			}
		}

		if (articleDownloader->GetContentAnalyzer() && articleDownloader->GetStatus() == ArticleDownloader::adFinished)
		{
			m_directRenamer.ArticleDownloaded(downloadQueue, fileInfo, articleInfo, articleDownloader->GetContentAnalyzer());
		}

		nzbInfo->SetDownloadedSize(nzbInfo->GetDownloadedSize() + articleDownloader->GetDownloadedSize());

		CheckHealth(downloadQueue, fileInfo);

		if (nzbInfo->GetParking() && fileInfo->GetActiveDownloads() == 1 && !fileInfo->GetDupeDeleted())
		{
			fileCompleted = true;
		}

		completeFileParts = fileCompleted && (!fileInfo->GetDeleted() || nzbInfo->GetParking());

		if (!completeFileParts)
		{
			DeleteDownloader(downloadQueue, articleDownloader, false);
		}
	}

	if (completeFileParts)
	{
		// all jobs done
		articleDownloader->CompleteFileParts();
		fileInfo->SetPartialChanged(false);

		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		DeleteDownloader(downloadQueue, articleDownloader, true);
	}
}

void QueueCoordinator::DeleteDownloader(DownloadQueue* downloadQueue,
	ArticleDownloader* articleDownloader, bool fileCompleted)
{
	FileInfo* fileInfo = articleDownloader->GetFileInfo();
	NzbInfo* nzbInfo = fileInfo->GetNzbInfo();
	bool hasOtherDownloaders = fileInfo->GetActiveDownloads() > 1;
	bool deleteFileObj = fileCompleted || (fileInfo->GetDeleted() && !hasOtherDownloaders);

	// remove downloader from downloader list
	m_activeDownloads.erase(std::find(m_activeDownloads.begin(), m_activeDownloads.end(), articleDownloader));

	fileInfo->SetActiveDownloads(fileInfo->GetActiveDownloads() - 1);
	nzbInfo->SetActiveDownloads(nzbInfo->GetActiveDownloads() - 1);

	if (deleteFileObj)
	{
		DeleteFileInfo(downloadQueue, fileInfo, fileCompleted);
		nzbInfo->SetChanged(true);
		downloadQueue->SaveChanged();
	}
}

void QueueCoordinator::DeleteFileInfo(DownloadQueue* downloadQueue, FileInfo* fileInfo, bool completed)
{
	while (g_ArticleCache->FileBusy(fileInfo))
	{
		Util::Sleep(5);
	}

	NzbInfo* nzbInfo = fileInfo->GetNzbInfo();
	bool parking = fileInfo->GetNzbInfo()->GetParking();
	bool fileDeleted = fileInfo->GetDeleted();

	fileInfo->SetDeleted(true);

	if (completed || nzbInfo->GetDeleting())
	{
		nzbInfo->UpdateCompletedStats(fileInfo);
	}
	else
	{
		nzbInfo->UpdateDeletedStats(fileInfo);
	}

	CompletedFile::EStatus fileStatus =
		fileInfo->GetTotalArticles() == fileInfo->GetSuccessArticles() ? CompletedFile::cfSuccess :
		fileInfo->GetTotalArticles() == fileInfo->GetMissedArticles() + fileInfo->GetFailedArticles() ? CompletedFile::cfFailure :
		fileInfo->GetSuccessArticles() > 0 || fileInfo->GetFailedArticles() > 0 ? CompletedFile::cfPartial :
		CompletedFile::cfNone;

	if (g_Options->GetServerMode())
	{
		g_DiskState->DiscardFile(fileInfo->GetId(), fileStatus == CompletedFile::cfSuccess || (fileDeleted && !parking), true, false);
		if (fileStatus == CompletedFile::cfPartial && (completed || parking))
		{
			g_DiskState->SaveFileState(fileInfo, true);
		}
	}

	if (!completed)
	{
		DiscardTempFiles(fileInfo);
	}

	if (completed || parking)
	{
		fileInfo->GetNzbInfo()->GetCompletedFiles()->emplace_back(
			fileInfo->GetId(),
			completed && fileInfo->GetOutputFilename() ?
			FileSystem::BaseFileName(fileInfo->GetOutputFilename()) : fileInfo->GetFilename(),
			fileInfo->GetOrigname(), fileStatus,
			fileStatus == CompletedFile::cfSuccess ? fileInfo->GetCrc() : 0,
			fileInfo->GetParFile(), fileInfo->GetHash16k(), fileInfo->GetParSetId());
	}

	if (g_Options->GetDirectRename())
	{
		m_directRenamer.FileDownloaded(downloadQueue, fileInfo);
	}

	if (nzbInfo->GetDirectRenameStatus() == NzbInfo::tsRunning &&
		!nzbInfo->GetDeleting() && nzbInfo->IsDownloadCompleted(true))
	{
		DiscardDirectRename(downloadQueue, nzbInfo);
	}

	std::unique_ptr<FileInfo> srcFileInfo = nzbInfo->GetFileList()->Remove(fileInfo);

	DownloadQueue::Aspect aspect = { completed && !fileDeleted ?
		DownloadQueue::eaFileCompleted : DownloadQueue::eaFileDeleted,
		downloadQueue, nzbInfo, fileInfo };
	downloadQueue->Notify(&aspect);

	// now can destroy FileInfo
	srcFileInfo.reset();
}

void QueueCoordinator::DiscardTempFiles(FileInfo* fileInfo)
{
	if (!g_Options->GetDirectWrite() && !fileInfo->GetForceDirectWrite())
	{
		for (ArticleInfo* pa : fileInfo->GetArticles())
		{
			if (pa->GetResultFilename())
			{
				FileSystem::DeleteFile(pa->GetResultFilename());
			}
		}
	}

	if (g_Options->GetDirectWrite() && fileInfo->GetOutputFilename() && !fileInfo->GetForceDirectWrite())
	{
		FileSystem::DeleteFile(fileInfo->GetOutputFilename());
	}
}

void QueueCoordinator::SaveQueueIfChanged()
{
	if (!g_Options->GetServerMode())
	{
		return;
	}

	bool hasChanges = false;
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
	for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
	{
		hasChanges |= nzbInfo->GetChanged();
	}

	if (hasChanges)
	{
		downloadQueue->Save();
	}
}

void QueueCoordinator::SaveAllPartialState()
{
	if (!g_Options->GetServerMode() || !g_Options->GetContinuePartial())
	{
		return;
	}

	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
	for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
	{
		for (FileInfo* fileInfo : nzbInfo->GetFileList())
		{
			SavePartialState(fileInfo);
		}
	}

	downloadQueue->SaveChanged();
}

void QueueCoordinator::SavePartialState(FileInfo* fileInfo)
{
	if (fileInfo->GetPartialChanged())
	{
		debug("Saving partial state for %s", fileInfo->GetFilename());
		if (fileInfo->GetPartialState() == FileInfo::psCompleted)
		{
			g_DiskState->DiscardFile(fileInfo->GetId(), false, false, true);
		}
		g_DiskState->SaveFileState(fileInfo, false);
		fileInfo->SetPartialChanged(false);
		fileInfo->SetPartialState(FileInfo::psPartial);
	}
}

void QueueCoordinator::LoadPartialState(FileInfo* fileInfo)
{
	if (fileInfo->GetPartialState() == FileInfo::psPartial)
	{
		g_DiskState->LoadFileState(fileInfo, g_ServerPool->GetServers(), false);
	}
	else if (fileInfo->GetPartialState() == FileInfo::psCompleted)
	{
		g_DiskState->LoadFileState(fileInfo, g_ServerPool->GetServers(), true);

		BString<1024> outputFilename("%s%c%s", fileInfo->GetNzbInfo()->GetDestDir(), PATH_SEPARATOR, fileInfo->GetFilename());
		fileInfo->SetOutputFilename(outputFilename);

		fileInfo->SetOutputInitialized(true);
		fileInfo->SetForceDirectWrite(true);
		fileInfo->SetFilenameConfirmed(true);
	}
}

void QueueCoordinator::SaveAllFileState()
{
	if (g_Options->GetServerMode() && m_downloadQueue.m_stateChanged)
	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		g_DiskState->SaveAllFileInfos(downloadQueue);
	}
}

void QueueCoordinator::CheckHealth(DownloadQueue* downloadQueue, FileInfo* fileInfo)
{
	if (g_Options->GetHealthCheck() == Options::hcNone ||
		fileInfo->GetNzbInfo()->GetHealthPaused() ||
		fileInfo->GetNzbInfo()->GetDeleteStatus() == NzbInfo::dsHealth ||
		fileInfo->GetNzbInfo()->CalcHealth() >= fileInfo->GetNzbInfo()->CalcCriticalHealth(true) ||
		(g_Options->GetParScan() == Options::psDupe && g_Options->GetHealthCheck() == Options::hcPark &&
		 fileInfo->GetNzbInfo()->GetSuccessArticles() * 100 / fileInfo->GetNzbInfo()->GetTotalArticles() > 10))
	{
		return;
	}

	if (g_Options->GetHealthCheck() == Options::hcPause)
	{
		warn("Pausing %s due to health %.1f%% below critical %.1f%%", fileInfo->GetNzbInfo()->GetName(),
			fileInfo->GetNzbInfo()->CalcHealth() / 10.0, fileInfo->GetNzbInfo()->CalcCriticalHealth(true) / 10.0);
		fileInfo->GetNzbInfo()->SetHealthPaused(true);
		downloadQueue->EditEntry(fileInfo->GetNzbInfo()->GetId(), DownloadQueue::eaGroupPause, nullptr);
	}
	else if (g_Options->GetHealthCheck() == Options::hcDelete ||
		g_Options->GetHealthCheck() == Options::hcPark)
	{
		fileInfo->GetNzbInfo()->PrintMessage(Message::mkWarning,
			"Cancelling download and deleting %s due to health %.1f%% below critical %.1f%%",
			fileInfo->GetNzbInfo()->GetName(), fileInfo->GetNzbInfo()->CalcHealth() / 10.0,
			fileInfo->GetNzbInfo()->CalcCriticalHealth(true) / 10.0);
		fileInfo->GetNzbInfo()->SetDeleteStatus(NzbInfo::dsHealth);
		downloadQueue->EditEntry(fileInfo->GetNzbInfo()->GetId(),
			g_Options->GetHealthCheck() == Options::hcPark ? DownloadQueue::eaGroupParkDelete : DownloadQueue::eaGroupDelete,
			nullptr);
	}
}

void QueueCoordinator::LogDebugInfo()
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	info("   ---------- Queue");
	int64 remaining, remainingForced;
	downloadQueue->CalcRemainingSize(&remaining, &remainingForced);
	info("     Remaining: %.1f MB, Forced: %.1f MB", remaining / 1024.0 / 1024.0, remainingForced / 1024.0 / 1024.0);
	info("     Download: %s, Post-process: %s, Scan: %s",
		 (g_WorkState->GetPauseDownload() ? "paused" : g_WorkState->GetTempPauseDownload() ? "temp-paused" : "active"),
		 (g_WorkState->GetPausePostProcess() ? "paused" : "active"),
		 (g_WorkState->GetPauseScan() ? "paused" : "active"));

	info("   ---------- QueueCoordinator");
	info("    Active Downloads: %i, Limit: %i", (int)m_activeDownloads.size(), m_downloadsLimit);
	for (ArticleDownloader* articleDownloader : m_activeDownloads)
	{
		articleDownloader->LogDebugInfo();
	}
}

void QueueCoordinator::ResetHangingDownloads()
{
	if (g_Options->GetArticleTimeout() == 0)
	{
		return;
	}

	GuardedDownloadQueue guard = DownloadQueue::Guard();
	time_t tm = Util::CurrentTime();

	for (ArticleDownloader* articleDownloader : m_activeDownloads)
	{
		if (tm - articleDownloader->GetLastUpdateTime() > g_Options->GetArticleTimeout() + 1 &&
			articleDownloader->GetStatus() == ArticleDownloader::adRunning)
		{
			error("Cancelling hanging download %s @ %s", articleDownloader->GetInfoName(),
				articleDownloader->GetConnectionName());
			articleDownloader->Stop();
		}
	}
}

/*
 * Returns True if Entry was deleted from Queue or False if it was scheduled for Deletion.
 * NOTE: "False" does not mean unsuccess; the entry is (or will be) deleted in any case.
 */
bool QueueCoordinator::DeleteQueueEntry(DownloadQueue* downloadQueue, FileInfo* fileInfo)
{
	fileInfo->SetDeleted(true);
	bool downloading = false;
	for (ArticleDownloader* articleDownloader : m_activeDownloads)
	{
		if (articleDownloader->GetFileInfo() == fileInfo)
		{
			downloading = true;
			articleDownloader->Stop();
		}
	}

	if (!downloading)
	{
		DeleteFileInfo(downloadQueue, fileInfo, false);
	}
	return downloading;
}

bool QueueCoordinator::SetQueueEntryCategory(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, const char* category)
{
	if (nzbInfo->GetPostInfo())
	{
		error("Could not change category for %s. File in post-process-stage", nzbInfo->GetName());
		return false;
	}

	BString<1024> oldDestDir = nzbInfo->GetDestDir();

	nzbInfo->SetCategory(category);
	nzbInfo->BuildDestDirName();

	bool dirUnchanged = !strcmp(nzbInfo->GetDestDir(), oldDestDir);
	bool ok = dirUnchanged || ArticleWriter::MoveCompletedFiles(nzbInfo, oldDestDir);

	return ok;
}

bool QueueCoordinator::SetQueueEntryName(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, const char* name)
{
	if (nzbInfo->GetPostInfo())
	{
		error("Could not rename %s. File in post-process-stage", nzbInfo->GetName());
		return false;
	}

	if (Util::EmptyStr(name))
	{
		error("Could not rename %s. The new name cannot be empty", nzbInfo->GetName());
		return false;
	}

	nzbInfo->SetName(NzbInfo::MakeNiceNzbName(name, false));

	if (nzbInfo->GetKind() == NzbInfo::nkUrl)
	{
		nzbInfo->SetFilename(BString<1024>("%s.nzb", nzbInfo->GetName()));
		return true;
	}

	BString<1024> oldDestDir = nzbInfo->GetDestDir();

	nzbInfo->BuildDestDirName();

	bool dirUnchanged = !strcmp(nzbInfo->GetDestDir(), oldDestDir);
	bool ok = dirUnchanged || ArticleWriter::MoveCompletedFiles(nzbInfo, oldDestDir);

	return ok;
}

bool QueueCoordinator::MergeQueueEntries(DownloadQueue* downloadQueue, NzbInfo* destNzbInfo, NzbInfo* srcNzbInfo)
{
	if (destNzbInfo->GetPostInfo() || srcNzbInfo->GetPostInfo())
	{
		error("Could not merge %s and %s. File in post-process-stage", destNzbInfo->GetName(), srcNzbInfo->GetName());
		return false;
	}

	if (destNzbInfo->GetKind() == NzbInfo::nkUrl || srcNzbInfo->GetKind() == NzbInfo::nkUrl)
	{
		error("Could not merge %s and %s. URLs cannot be merged", destNzbInfo->GetName(), srcNzbInfo->GetName());
		return false;
	}

	// set new dest directory, new category and move downloaded files to new dest directory
	srcNzbInfo->SetFilename(srcNzbInfo->GetFilename());
	SetQueueEntryCategory(downloadQueue, srcNzbInfo, destNzbInfo->GetCategory());

	// reattach file items to new NZBInfo-object
	for (std::unique_ptr<FileInfo>& fileInfo : *srcNzbInfo->GetFileList())
	{
		fileInfo->SetNzbInfo(destNzbInfo);
		destNzbInfo->GetFileList()->Add(std::move(fileInfo));
	}
	srcNzbInfo->GetFileList()->clear();

	destNzbInfo->SetFileCount(destNzbInfo->GetFileCount() + srcNzbInfo->GetFileCount());
	destNzbInfo->SetActiveDownloads(destNzbInfo->GetActiveDownloads() + srcNzbInfo->GetActiveDownloads());
	destNzbInfo->SetFullContentHash(0);
	destNzbInfo->SetFilteredContentHash(0);

	destNzbInfo->SetSize(destNzbInfo->GetSize() + srcNzbInfo->GetSize());
	destNzbInfo->SetRemainingSize(destNzbInfo->GetRemainingSize() + srcNzbInfo->GetRemainingSize());
	destNzbInfo->SetPausedFileCount(destNzbInfo->GetPausedFileCount() + srcNzbInfo->GetPausedFileCount());
	destNzbInfo->SetPausedSize(destNzbInfo->GetPausedSize() + srcNzbInfo->GetPausedSize());

	destNzbInfo->SetSuccessSize(destNzbInfo->GetSuccessSize() + srcNzbInfo->GetSuccessSize());
	destNzbInfo->SetCurrentSuccessSize(destNzbInfo->GetCurrentSuccessSize() + srcNzbInfo->GetCurrentSuccessSize());
	destNzbInfo->SetFailedSize(destNzbInfo->GetFailedSize() + srcNzbInfo->GetFailedSize());
	destNzbInfo->SetCurrentFailedSize(destNzbInfo->GetCurrentFailedSize() + srcNzbInfo->GetCurrentFailedSize());

	destNzbInfo->SetParSize(destNzbInfo->GetParSize() + srcNzbInfo->GetParSize());
	destNzbInfo->SetParSuccessSize(destNzbInfo->GetParSuccessSize() + srcNzbInfo->GetParSuccessSize());
	destNzbInfo->SetParCurrentSuccessSize(destNzbInfo->GetParCurrentSuccessSize() + srcNzbInfo->GetParCurrentSuccessSize());
	destNzbInfo->SetParFailedSize(destNzbInfo->GetParFailedSize() + srcNzbInfo->GetParFailedSize());
	destNzbInfo->SetParCurrentFailedSize(destNzbInfo->GetParCurrentFailedSize() + srcNzbInfo->GetParCurrentFailedSize());
	destNzbInfo->SetRemainingParCount(destNzbInfo->GetRemainingParCount() + srcNzbInfo->GetRemainingParCount());

	destNzbInfo->SetTotalArticles(destNzbInfo->GetTotalArticles() + srcNzbInfo->GetTotalArticles());
	destNzbInfo->SetSuccessArticles(destNzbInfo->GetSuccessArticles() + srcNzbInfo->GetSuccessArticles());
	destNzbInfo->SetFailedArticles(destNzbInfo->GetFailedArticles() + srcNzbInfo->GetFailedArticles());
	destNzbInfo->SetCurrentSuccessArticles(destNzbInfo->GetCurrentSuccessArticles() + srcNzbInfo->GetCurrentSuccessArticles());
	destNzbInfo->SetCurrentFailedArticles(destNzbInfo->GetCurrentFailedArticles() + srcNzbInfo->GetCurrentFailedArticles());
	destNzbInfo->GetServerStats()->ListOp(srcNzbInfo->GetServerStats(), ServerStatList::soAdd);
	destNzbInfo->GetCurrentServerStats()->ListOp(srcNzbInfo->GetCurrentServerStats(), ServerStatList::soAdd);

	destNzbInfo->SetMinTime(srcNzbInfo->GetMinTime() < destNzbInfo->GetMinTime() ? srcNzbInfo->GetMinTime() : destNzbInfo->GetMinTime());
	destNzbInfo->SetMaxTime(srcNzbInfo->GetMaxTime() > destNzbInfo->GetMaxTime() ? srcNzbInfo->GetMaxTime() : destNzbInfo->GetMaxTime());

	destNzbInfo->SetDownloadedSize(destNzbInfo->GetDownloadedSize() + srcNzbInfo->GetDownloadedSize());
	destNzbInfo->SetDownloadSec(destNzbInfo->GetDownloadSec() + srcNzbInfo->GetDownloadSec());
	destNzbInfo->SetDownloadStartTime((destNzbInfo->GetDownloadStartTime() > 0 &&
		destNzbInfo->GetDownloadStartTime() < srcNzbInfo->GetDownloadStartTime()) || srcNzbInfo->GetDownloadStartTime() == 0 ?
		destNzbInfo->GetDownloadStartTime() : srcNzbInfo->GetDownloadStartTime());

	// reattach completed file items to new NZBInfo-object
	for (CompletedFile& completedFile : srcNzbInfo->GetCompletedFiles())
	{
		destNzbInfo->GetCompletedFiles()->push_back(std::move(completedFile));
	}
	srcNzbInfo->GetCompletedFiles()->clear();

	// concatenate QueuedFilenames using character '|' as separator
	CString queuedFilename;
	queuedFilename.Format("%s|%s", destNzbInfo->GetQueuedFilename(), srcNzbInfo->GetQueuedFilename());
	destNzbInfo->SetQueuedFilename(queuedFilename);

	g_DiskState->DiscardFiles(srcNzbInfo);
	downloadQueue->GetQueue()->Remove(srcNzbInfo);

	return true;
}

/*
 * Creates new nzb-item out of existing files from other nzb-items.
 * If any of file-items is being downloaded the command fail.
 * For each file-item an event "eaFileDeleted" is fired.
 */
bool QueueCoordinator::SplitQueueEntries(DownloadQueue* downloadQueue, RawFileList* fileList, const char* name, NzbInfo** newNzbInfo)
{
	if (fileList->empty())
	{
		return false;
	}

	NzbInfo* srcNzbInfo = nullptr;

	for (FileInfo* fileInfo : fileList)
	{
		if (fileInfo->GetActiveDownloads() > 0 || fileInfo->GetCompletedArticles() > 0)
		{
			error("Could not split %s. File is already (partially) downloaded", fileInfo->GetFilename());
			return false;
		}
		if (fileInfo->GetNzbInfo()->GetPostInfo())
		{
			error("Could not split %s. File in post-process-stage", fileInfo->GetFilename());
			return false;
		}
		if (!srcNzbInfo)
		{
			srcNzbInfo = fileInfo->GetNzbInfo();
		}
	}

	std::unique_ptr<NzbInfo> nzbInfo = std::make_unique<NzbInfo>();

	nzbInfo->SetFilename(srcNzbInfo->GetFilename());
	nzbInfo->SetName(name);
	nzbInfo->SetCategory(srcNzbInfo->GetCategory());
	nzbInfo->SetFullContentHash(0);
	nzbInfo->SetFilteredContentHash(0);
	nzbInfo->SetPriority(srcNzbInfo->GetPriority());
	nzbInfo->BuildDestDirName();
	nzbInfo->SetQueuedFilename(srcNzbInfo->GetQueuedFilename());
	nzbInfo->GetParameters()->CopyFrom(srcNzbInfo->GetParameters());

	srcNzbInfo->SetFullContentHash(0);
	srcNzbInfo->SetFilteredContentHash(0);

	for (FileInfo* fileInfo : fileList)
	{
		DownloadQueue::Aspect aspect = { DownloadQueue::eaFileDeleted, downloadQueue, fileInfo->GetNzbInfo(), fileInfo };
		downloadQueue->Notify(&aspect);

		nzbInfo->GetFileList()->Add(srcNzbInfo->GetFileList()->Remove(fileInfo));
		fileInfo->SetNzbInfo(nzbInfo.get());

		srcNzbInfo->SetFileCount(srcNzbInfo->GetFileCount() - 1);
		srcNzbInfo->SetSize(srcNzbInfo->GetSize() - fileInfo->GetSize());
		srcNzbInfo->SetRemainingSize(srcNzbInfo->GetRemainingSize() - fileInfo->GetRemainingSize());
		srcNzbInfo->SetCurrentSuccessSize(srcNzbInfo->GetCurrentSuccessSize() - fileInfo->GetSuccessSize());
		srcNzbInfo->SetCurrentFailedSize(srcNzbInfo->GetCurrentFailedSize() - fileInfo->GetFailedSize() - fileInfo->GetMissedSize());
		srcNzbInfo->SetTotalArticles(srcNzbInfo->GetTotalArticles() - fileInfo->GetTotalArticles());
		srcNzbInfo->SetFailedArticles(srcNzbInfo->GetFailedArticles() - fileInfo->GetMissedArticles());
		srcNzbInfo->SetCurrentSuccessArticles(srcNzbInfo->GetCurrentSuccessArticles() - fileInfo->GetSuccessArticles());
		srcNzbInfo->SetCurrentFailedArticles(srcNzbInfo->GetCurrentFailedArticles() - fileInfo->GetFailedArticles() - fileInfo->GetMissedArticles());
		srcNzbInfo->GetCurrentServerStats()->ListOp(fileInfo->GetServerStats(), ServerStatList::soSubtract);

		nzbInfo->SetFileCount(nzbInfo->GetFileCount() + 1);
		nzbInfo->SetSize(nzbInfo->GetSize() + fileInfo->GetSize());
		nzbInfo->SetRemainingSize(nzbInfo->GetRemainingSize() + fileInfo->GetRemainingSize());
		nzbInfo->SetCurrentSuccessSize(nzbInfo->GetCurrentSuccessSize() + fileInfo->GetSuccessSize());
		nzbInfo->SetCurrentFailedSize(nzbInfo->GetCurrentFailedSize() + fileInfo->GetFailedSize() + fileInfo->GetMissedSize());
		nzbInfo->SetTotalArticles(nzbInfo->GetTotalArticles() + fileInfo->GetTotalArticles());
		nzbInfo->SetFailedArticles(nzbInfo->GetFailedArticles() + fileInfo->GetMissedArticles());
		nzbInfo->SetCurrentSuccessArticles(nzbInfo->GetCurrentSuccessArticles() + fileInfo->GetSuccessArticles());
		nzbInfo->SetCurrentFailedArticles(nzbInfo->GetCurrentFailedArticles() + fileInfo->GetFailedArticles() + fileInfo->GetMissedArticles());
		nzbInfo->GetCurrentServerStats()->ListOp(fileInfo->GetServerStats(), ServerStatList::soAdd);

		if (fileInfo->GetParFile())
		{
			srcNzbInfo->SetParSize(srcNzbInfo->GetParSize() - fileInfo->GetSize());
			srcNzbInfo->SetParCurrentSuccessSize(srcNzbInfo->GetParCurrentSuccessSize() - fileInfo->GetSuccessSize());
			srcNzbInfo->SetParCurrentFailedSize(srcNzbInfo->GetParCurrentFailedSize() - fileInfo->GetFailedSize() - fileInfo->GetMissedSize());
			srcNzbInfo->SetRemainingParCount(srcNzbInfo->GetRemainingParCount() - 1);

			nzbInfo->SetParSize(nzbInfo->GetParSize() + fileInfo->GetSize());
			nzbInfo->SetParCurrentSuccessSize(nzbInfo->GetParCurrentSuccessSize() + fileInfo->GetSuccessSize());
			nzbInfo->SetParCurrentFailedSize(nzbInfo->GetParCurrentFailedSize() + fileInfo->GetFailedSize() + fileInfo->GetMissedSize());
			nzbInfo->SetRemainingParCount(nzbInfo->GetRemainingParCount() + 1);
		}

		if (fileInfo->GetPaused())
		{
			srcNzbInfo->SetPausedFileCount(srcNzbInfo->GetPausedFileCount() - 1);
			srcNzbInfo->SetPausedSize(srcNzbInfo->GetPausedSize() - fileInfo->GetRemainingSize());

			nzbInfo->SetPausedFileCount(srcNzbInfo->GetPausedFileCount() + 1);
			nzbInfo->SetPausedSize(nzbInfo->GetPausedSize() + fileInfo->GetRemainingSize());
		}
	}

	nzbInfo->UpdateMinMaxTime();
	if (srcNzbInfo->GetCompletedFiles()->empty())
	{
		srcNzbInfo->UpdateMinMaxTime();
	}

	if (srcNzbInfo->GetFileList()->empty())
	{
		g_DiskState->DiscardFiles(srcNzbInfo);
		downloadQueue->GetQueue()->Remove(srcNzbInfo);
	}

	*newNzbInfo = nzbInfo.get();
	downloadQueue->GetQueue()->Add(std::move(nzbInfo));

	return true;
}

void QueueCoordinator::DirectRenameCompleted(DownloadQueue* downloadQueue, NzbInfo* nzbInfo)
{
	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
		if (g_Options->GetServerMode() && !fileInfo->GetArticles()->empty())
		{
			// save new file name into disk state file
			g_DiskState->SaveFile(fileInfo);
		}
	}

	DiscardDirectRename(downloadQueue, nzbInfo);

	nzbInfo->SetDirectRenameStatus(NzbInfo::tsSuccess);

	if (g_Options->GetParCheck() != Options::pcForce)
	{
		downloadQueue->EditEntry(nzbInfo->GetId(), DownloadQueue::eaGroupResume, nullptr);
		downloadQueue->EditEntry(nzbInfo->GetId(), DownloadQueue::eaGroupPauseAllPars, nullptr);
	}

	if (g_Options->GetReorderFiles())
	{
		nzbInfo->PrintMessage(Message::mkInfo, "Reordering files for %s", nzbInfo->GetName());
		downloadQueue->EditEntry(nzbInfo->GetId(), DownloadQueue::eaGroupSortFiles, nullptr);
	}

	nzbInfo->SetChanged(true);
	downloadQueue->SaveChanged();

	DownloadQueue::Aspect namedAspect = { DownloadQueue::eaNzbNamed, downloadQueue, nzbInfo, nullptr };
	downloadQueue->Notify(&namedAspect);
}

void QueueCoordinator::DiscardDirectRename(DownloadQueue* downloadQueue, NzbInfo* nzbInfo)
{
	int64 discardedSize = 0;
	int discardedCount = 0;

	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
		if (fileInfo->GetParFile() && fileInfo->GetCompletedArticles() == 1 &&
			fileInfo->GetActiveDownloads() == 0)
		{
			bool locked = false;
			{
				Guard contentGuard = g_ArticleCache->GuardContent();
				locked = fileInfo->GetFlushLocked();
				if (!locked)
				{
					fileInfo->SetFlushLocked(true);
				}
			}

			if (!locked)
			{
				// discard downloaded articles from partially downloaded par-files
				discardedSize += fileInfo->GetSuccessSize();
				discardedCount++;

				DiscardDownloadedArticles(nzbInfo, fileInfo);
			}

			if (!locked)
			{
				Guard contentGuard = g_ArticleCache->GuardContent();
				fileInfo->SetFlushLocked(false);
			}
		}

		if (g_Options->GetServerMode() &&
			!fileInfo->GetArticles()->empty() && g_Options->GetContinuePartial() &&
			fileInfo->GetActiveDownloads() == 0 && fileInfo->GetCachedArticles() == 0)
		{
			// discard article infos to free up memory if possible
			debug("Discarding article infos for %s/%s", nzbInfo->GetName(), fileInfo->GetFilename());
			fileInfo->SetPartialChanged(true);
			SavePartialState(fileInfo);
			fileInfo->GetArticles()->clear();
		}
	}

	if (discardedSize > 0)
	{
		nzbInfo->PrintMessage(Message::mkDetail, "Discarded %s from %i files used for direct renaming",
			*Util::FormatSize(discardedSize), discardedCount);
	}
}

void QueueCoordinator::DiscardDownloadedArticles(NzbInfo* nzbInfo, FileInfo* fileInfo)
{
	nzbInfo->SetRemainingSize(nzbInfo->GetRemainingSize() + fileInfo->GetSuccessSize() + fileInfo->GetFailedSize());
	if (fileInfo->GetPaused())
	{
		nzbInfo->SetPausedSize(nzbInfo->GetPausedSize() + fileInfo->GetSuccessSize() + fileInfo->GetFailedSize());
	}
	nzbInfo->GetCurrentServerStats()->ListOp(fileInfo->GetServerStats(), ServerStatList::soSubtract);
	fileInfo->GetServerStats()->clear();

	nzbInfo->SetCurrentSuccessArticles(nzbInfo->GetCurrentSuccessArticles() - fileInfo->GetSuccessArticles());
	nzbInfo->SetCurrentSuccessSize(nzbInfo->GetCurrentSuccessSize() - fileInfo->GetSuccessSize());
	nzbInfo->SetParCurrentSuccessSize(nzbInfo->GetParCurrentSuccessSize() - fileInfo->GetSuccessSize());
	fileInfo->SetSuccessSize(0);
	fileInfo->SetSuccessArticles(0);

	nzbInfo->SetCurrentFailedArticles(nzbInfo->GetCurrentFailedArticles() - fileInfo->GetFailedArticles());
	nzbInfo->SetCurrentFailedSize(nzbInfo->GetCurrentFailedSize() - fileInfo->GetFailedSize());
	nzbInfo->SetParCurrentFailedSize(nzbInfo->GetParCurrentFailedSize() - fileInfo->GetFailedSize());
	fileInfo->SetFailedSize(0);
	fileInfo->SetFailedArticles(0);

	fileInfo->SetCompletedArticles(0);
	fileInfo->SetRemainingSize(fileInfo->GetSize() - fileInfo->GetMissedSize());

	// discard temporary files
	DiscardTempFiles(fileInfo);
	g_DiskState->DiscardFile(fileInfo->GetId(), false, true, false);

	fileInfo->SetOutputFilename(nullptr);
	fileInfo->SetOutputInitialized(false);
	fileInfo->SetCachedArticles(0);
	fileInfo->SetPartialChanged(false);
	fileInfo->SetPartialState(FileInfo::psNone);

	if (g_Options->GetServerMode())
	{
		// free up memory used by articles if possible
		fileInfo->GetArticles()->clear();
	}
	else
	{
		// reset article states if discarding isn't possible
		for (ArticleInfo* articleInfo : fileInfo->GetArticles())
		{
			articleInfo->SetStatus(ArticleInfo::aiUndefined);
			articleInfo->SetResultFilename(nullptr);
			articleInfo->DiscardSegment();
		}
	}
}
