/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2005 Bo Cordes Petersen <placebodk@users.sourceforge.net>
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


#include "nzbget.h"
#include "QueueCoordinator.h"
#include "Options.h"
#include "ServerPool.h"
#include "ArticleDownloader.h"
#include "ArticleWriter.h"
#include "DiskState.h"
#include "Util.h"
#include "Decoder.h"
#include "StatMeter.h"

bool QueueCoordinator::CoordinatorDownloadQueue::EditEntry(
	int ID, EEditAction action, int offset, const char* text)
{
	return m_owner->m_queueEditor.EditEntry(&m_owner->m_downloadQueue, ID, action, offset, text);
}

bool QueueCoordinator::CoordinatorDownloadQueue::EditList(
	IdList* idList, NameList* nameList, EMatchMode matchMode, EEditAction action, int offset, const char* text)
{
	m_massEdit = true;
	bool ret = m_owner->m_queueEditor.EditList(&m_owner->m_downloadQueue, idList, nameList, matchMode, action, offset, text);
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

	if (g_Options->GetSaveQueue() && g_Options->GetServerMode())
	{
		g_DiskState->SaveDownloadQueue(this);
	}

	m_wantSave = false;
}

QueueCoordinator::QueueCoordinator()
{
	debug("Creating QueueCoordinator");

	m_hasMoreJobs = true;
	m_serverConfigGeneration = 0;

	g_Log->RegisterDebuggable(this);

	m_downloadQueue.m_owner = this;
	CoordinatorDownloadQueue::Init(&m_downloadQueue);
}

QueueCoordinator::~QueueCoordinator()
{
	debug("Destroying QueueCoordinator");
	// Cleanup

	g_Log->UnregisterDebuggable(this);

	debug("Deleting ArticleDownloaders");
	for (ActiveDownloads::iterator it = m_activeDownloads.begin(); it != m_activeDownloads.end(); it++)
	{
		delete *it;
	}
	m_activeDownloads.clear();

	CoordinatorDownloadQueue::Final();

	debug("QueueCoordinator destroyed");
}

void QueueCoordinator::Load()
{
	DownloadQueue* downloadQueue = DownloadQueue::Lock();

	bool statLoaded = true;
	bool perfectServerMatch = true;
	bool queueLoaded = false;

	if (g_Options->GetServerMode() && g_Options->GetSaveQueue())
	{
		statLoaded = g_StatMeter->Load(&perfectServerMatch);

		if (g_Options->GetReloadQueue() && g_DiskState->DownloadQueueExists())
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
		downloadQueue->Save();

		// re-save file states into diskstate to update server ids
		if (g_Options->GetServerMode() && g_Options->GetSaveQueue())
		{
			for (NzbList::iterator it = downloadQueue->GetQueue()->begin(); it != downloadQueue->GetQueue()->end(); it++)
			{
				NzbInfo* nzbInfo = *it;

				if (g_Options->GetContinuePartial())
				{
					for (FileList::iterator it2 = nzbInfo->GetFileList()->begin(); it2 != nzbInfo->GetFileList()->end(); it2++)
					{
						FileInfo* fileInfo = *it2;
						if (!fileInfo->GetArticles()->empty())
						{
							g_DiskState->SaveFileState(fileInfo, false);
						}
					}
				}

				for (CompletedFiles::iterator it2 = nzbInfo->GetCompletedFiles()->begin(); it2 != nzbInfo->GetCompletedFiles()->end(); it2++)
				{
					CompletedFile* completedFile = *it2;
					if (completedFile->GetStatus() != CompletedFile::cfSuccess && completedFile->GetId() > 0)
					{
						FileInfo* fileInfo = new FileInfo(completedFile->GetId());
						if (g_DiskState->LoadFileState(fileInfo, g_ServerPool->GetServers(), false))
						{
							g_DiskState->SaveFileState(fileInfo, true);
						}
						delete fileInfo;
					}
				}
			}
		}
	}

	CoordinatorDownloadQueue::Loaded();
	DownloadQueue::Unlock();
}

void QueueCoordinator::Run()
{
	debug("Entering QueueCoordinator-loop");

	Load();
	AdjustDownloadsLimit();
	bool wasStandBy = true;
	bool articeDownloadsRunning = false;
	int resetCounter = 0;
	g_StatMeter->IntervalCheck();

	while (!IsStopped())
	{
		bool downloadsChecked = false;
		bool downloadStarted = false;
		NntpConnection* connection = g_ServerPool->GetConnection(0, NULL, NULL);
		if (connection)
		{
			// start download for next article
			FileInfo* fileInfo;
			ArticleInfo* articleInfo;
			bool freeConnection = false;

			DownloadQueue* downloadQueue = DownloadQueue::Lock();
			bool hasMoreArticles = GetNextArticle(downloadQueue, fileInfo, articleInfo);
			articeDownloadsRunning = !m_activeDownloads.empty();
			downloadsChecked = true;
			m_hasMoreJobs = hasMoreArticles || articeDownloadsRunning;
			if (hasMoreArticles && !IsStopped() && (int)m_activeDownloads.size() < m_downloadsLimit &&
				(!g_Options->GetTempPauseDownload() || fileInfo->GetExtraPriority()))
			{
				StartArticleDownload(fileInfo, articleInfo, connection);
				articeDownloadsRunning = true;
				downloadStarted = true;
			}
			else
			{
				freeConnection = true;
			}
			DownloadQueue::Unlock();

			if (freeConnection)
			{
				g_ServerPool->FreeConnection(connection, false);
			}
		}

		if (!downloadsChecked)
		{
			DownloadQueue::Lock();
			articeDownloadsRunning = !m_activeDownloads.empty();
			DownloadQueue::Unlock();
		}

		bool standBy = !articeDownloadsRunning;
		if (standBy != wasStandBy)
		{
			g_StatMeter->EnterLeaveStandBy(standBy);
			wasStandBy = standBy;
			if (standBy)
			{
				SavePartialState();
			}
		}

		// sleep longer in StandBy
		int sleepInterval = downloadStarted ? 0 : standBy ? 100 : 5;
		usleep(sleepInterval * 1000);

		if (!standBy)
		{
			g_StatMeter->AddSpeedReading(0);
		}

		Util::SetStandByMode(standBy);

		resetCounter += sleepInterval;
		if (resetCounter >= 1000)
		{
			// this code should not be called too often, once per second is OK
			g_ServerPool->CloseUnusedConnections();
			ResetHangingDownloads();
			if (!standBy)
			{
				SavePartialState();
			}
			resetCounter = 0;
			g_StatMeter->IntervalCheck();
			AdjustDownloadsLimit();
		}
	}

	// waiting for downloads
	debug("QueueCoordinator: waiting for Downloads to complete");
	bool completed = false;
	while (!completed)
	{
		DownloadQueue::Lock();
		completed = m_activeDownloads.size() == 0;
		DownloadQueue::Unlock();
		usleep(100 * 1000);
		ResetHangingDownloads();
	}
	debug("QueueCoordinator: Downloads are completed");

	SavePartialState();

	debug("Exiting QueueCoordinator-loop");
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
	for (Servers::iterator it = g_ServerPool->GetServers()->begin(); it != g_ServerPool->GetServers()->end(); it++)
	{
		NewsServer* newsServer = *it;
		if ((newsServer->GetNormLevel() == 0 || newsServer->GetNormLevel() == 1) && newsServer->GetActive())
		{
			downloadsLimit += newsServer->GetMaxConnections();
		}
	}

	m_downloadsLimit = downloadsLimit;
}

void QueueCoordinator::AddNzbFileToQueue(NzbFile* nzbFile, NzbInfo* urlInfo, bool addFirst)
{
	debug("Adding NZBFile to queue");

	NzbInfo* nzbInfo = nzbFile->GetNzbInfo();

	DownloadQueue* downloadQueue = DownloadQueue::Lock();

	DownloadQueue::Aspect foundAspect = { DownloadQueue::eaNzbFound, downloadQueue, nzbInfo, NULL };
	downloadQueue->Notify(&foundAspect);

	NzbInfo::EDeleteStatus deleteStatus = nzbInfo->GetDeleteStatus();

	if (deleteStatus != NzbInfo::dsNone)
	{
		bool allPaused = !nzbInfo->GetFileList()->empty();
		for (FileList::iterator it = nzbInfo->GetFileList()->begin(); it != nzbInfo->GetFileList()->end(); it++)
		{
			FileInfo* fileInfo = *it;
			allPaused &= fileInfo->GetPaused();
			if (g_Options->GetSaveQueue() && g_Options->GetServerMode())
			{
				g_DiskState->DiscardFile(fileInfo, true, false, false);
			}
		}
		nzbInfo->SetDeletePaused(allPaused);
	}

	if (deleteStatus != NzbInfo::dsManual)
	{
		// NZBInfo will be added either to queue or to history as duplicate
		// and therefore can be detached from NZBFile.
		nzbFile->DetachNzbInfo();
	}

	if (deleteStatus == NzbInfo::dsNone)
	{
		if (g_Options->GetDupeCheck() && nzbInfo->GetDupeMode() != dmForce)
		{
			CheckDupeFileInfos(nzbInfo);
		}

		if (urlInfo)
		{
			// insert at the URL position
			for (NzbList::iterator it = downloadQueue->GetQueue()->begin(); it != downloadQueue->GetQueue()->end(); it++)
			{
				NzbInfo* posNzbInfo = *it;
				if (posNzbInfo == urlInfo)
				{
					downloadQueue->GetQueue()->insert(it, nzbInfo);
					break;
				}
			}
		}
		else if (addFirst)
		{
			downloadQueue->GetQueue()->push_front(nzbInfo);
		}
		else
		{
			downloadQueue->GetQueue()->push_back(nzbInfo);
		}
	}

	if (urlInfo)
	{
		nzbInfo->SetId(urlInfo->GetId());
		downloadQueue->GetQueue()->Remove(urlInfo);
		delete urlInfo;
	}

	if (deleteStatus == NzbInfo::dsNone)
	{
		nzbInfo->PrintMessage(Message::mkInfo, "Collection %s added to queue", nzbInfo->GetName());
	}

	if (deleteStatus != NzbInfo::dsManual)
	{
		DownloadQueue::Aspect addedAspect = { DownloadQueue::eaNzbAdded, downloadQueue, nzbInfo, NULL };
		downloadQueue->Notify(&addedAspect);
	}

	downloadQueue->Save();

	DownloadQueue::Unlock();
}

void QueueCoordinator::CheckDupeFileInfos(NzbInfo* nzbInfo)
{
	debug("CheckDupeFileInfos");

	if (!g_Options->GetDupeCheck() || nzbInfo->GetDupeMode() == dmForce)
	{
		return;
	}

	FileList dupeList(true);

	int index1 = 0;
	for (FileList::iterator it = nzbInfo->GetFileList()->begin(); it != nzbInfo->GetFileList()->end(); it++)
	{
		index1++;
		FileInfo* fileInfo = *it;

		bool dupe = false;
		int index2 = 0;
		for (FileList::iterator it2 =  nzbInfo->GetFileList()->begin(); it2 !=  nzbInfo->GetFileList()->end(); it2++)
		{
			index2++;
			FileInfo* fileInfo2 = *it2;
			if (fileInfo != fileInfo2 &&
				!strcmp(fileInfo->GetFilename(), fileInfo2->GetFilename()) &&
				(fileInfo->GetSize() < fileInfo2->GetSize() ||
				 (fileInfo->GetSize() == fileInfo2->GetSize() && index2 < index1)))
			{
				warn("File \"%s\" appears twice in collection, adding only the biggest file", fileInfo->GetFilename());
				dupe = true;
				break;
			}
		}
		if (dupe)
		{
			dupeList.push_back(fileInfo);
			continue;
		}
	}

	for (FileList::iterator it = dupeList.begin(); it != dupeList.end(); it++)
	{
		FileInfo* fileInfo = *it;
		StatFileInfo(fileInfo, false);
		nzbInfo->GetFileList()->Remove(fileInfo);
		if (g_Options->GetSaveQueue() && g_Options->GetServerMode())
		{
			g_DiskState->DiscardFile(fileInfo, true, false, false);
		}
	}
}

void QueueCoordinator::Stop()
{
	Thread::Stop();

	debug("Stopping ArticleDownloads");
	DownloadQueue::Lock();
	for (ActiveDownloads::iterator it = m_activeDownloads.begin(); it != m_activeDownloads.end(); it++)
	{
		(*it)->Stop();
	}
	DownloadQueue::Unlock();
	debug("ArticleDownloads are notified");
}

/*
 * Returns next article for download.
 */
bool QueueCoordinator::GetNextArticle(DownloadQueue* downloadQueue, FileInfo* &fileInfo, ArticleInfo* &articleInfo)
{
	// find an unpaused file with the highest priority, then take the next article from the file.
	// if the file doesn't have any articles left for download, we store that fact and search again,
	// ignoring all files which were previously marked as not having any articles.

	// special case: if the file has ExtraPriority-flag set, it has the highest priority and the
	// Paused-flag is ignored.

	//debug("QueueCoordinator::GetNextArticle()");

	bool ok = false;

	// pCheckedFiles stores
	bool* checkedFiles = NULL;
	time_t curDate = time(NULL);

	while (!ok)
	{
		fileInfo = NULL;
		int num = 0;
		int fileNum = 0;

		for (NzbList::iterator it = downloadQueue->GetQueue()->begin(); it != downloadQueue->GetQueue()->end(); it++)
		{
			NzbInfo* nzbInfo = *it;
			for (FileList::iterator it2 = nzbInfo->GetFileList()->begin(); it2 != nzbInfo->GetFileList()->end(); it2++)
			{
				FileInfo* fileInfo1 = *it2;
				if ((!checkedFiles || !checkedFiles[num]) &&
					!fileInfo1->GetPaused() && !fileInfo1->GetDeleted() &&
					(g_Options->GetPropagationDelay() == 0 ||
					 (int)fileInfo1->GetTime() < (int)curDate - g_Options->GetPropagationDelay()) &&
					(!g_Options->GetPauseDownload() || nzbInfo->GetForcePriority()) &&
					(!fileInfo ||
					 (fileInfo1->GetExtraPriority() == fileInfo->GetExtraPriority() &&
					  fileInfo1->GetNzbInfo()->GetPriority() > fileInfo->GetNzbInfo()->GetPriority()) ||
					 (fileInfo1->GetExtraPriority() > fileInfo->GetExtraPriority())))
				{
					fileInfo = fileInfo1;
					fileNum = num;
				}
				num++;
			}
		}

		if (!fileInfo)
		{
			// there are no more files for download
			break;
		}

		if (fileInfo->GetArticles()->empty() && g_Options->GetSaveQueue() && g_Options->GetServerMode())
		{
			g_DiskState->LoadArticles(fileInfo);
		}

		// check if the file has any articles left for download
		for (FileInfo::Articles::iterator at = fileInfo->GetArticles()->begin(); at != fileInfo->GetArticles()->end(); at++)
		{
			articleInfo = *at;
			if (articleInfo->GetStatus() == ArticleInfo::aiUndefined)
			{
				ok = true;
				break;
			}
		}

		if (!ok)
		{
			// the file doesn't have any articles left for download, we mark the file as such
			if (!checkedFiles)
			{
				int totalFileCount = 0;
				for (NzbList::iterator it = downloadQueue->GetQueue()->begin(); it != downloadQueue->GetQueue()->end(); it++)
				{
					NzbInfo* nzbInfo = *it;
					totalFileCount += nzbInfo->GetFileList()->size();
				}

				if (totalFileCount > 0)
				{
					int arrSize = sizeof(bool) * totalFileCount;
					checkedFiles = (bool*)malloc(arrSize);
					memset(checkedFiles, false, arrSize);
				}
			}
			if (checkedFiles)
			{
				checkedFiles[fileNum] = true;
			}
		}
	}

	free(checkedFiles);

	return ok;
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

	BString<1024> infoName("%s%c%s [%i/%i]", fileInfo->GetNzbInfo()->GetName(), (int)PATH_SEPARATOR, fileInfo->GetFilename(), articleInfo->GetPartNumber(), (int)fileInfo->GetArticles()->size());
	articleDownloader->SetInfoName(infoName);

	articleInfo->SetStatus(ArticleInfo::aiRunning);
	fileInfo->SetActiveDownloads(fileInfo->GetActiveDownloads() + 1);
	fileInfo->GetNzbInfo()->SetActiveDownloads(fileInfo->GetNzbInfo()->GetActiveDownloads() + 1);

	m_activeDownloads.push_back(articleDownloader);
	articleDownloader->Start();
}

void QueueCoordinator::Update(Subject* Caller, void* Aspect)
{
	debug("Notification from ArticleDownloader received");

	ArticleDownloader* articleDownloader = (ArticleDownloader*)Caller;
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
	NzbInfo* nzbInfo = fileInfo->GetNzbInfo();
	ArticleInfo* articleInfo = articleDownloader->GetArticleInfo();
	bool retry = false;
	bool fileCompleted = false;

	DownloadQueue* downloadQueue = DownloadQueue::Lock();

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
		fileInfo->SetFilename(articleDownloader->GetArticleFilename());
		fileInfo->SetFilenameConfirmed(true);
		if (g_Options->GetDupeCheck() &&
			nzbInfo->GetDupeMode() != dmForce &&
			!nzbInfo->GetManyDupeFiles() &&
			Util::FileExists(nzbInfo->GetDestDir(), fileInfo->GetFilename()))
		{
			warn("File \"%s\" seems to be duplicate, cancelling download and deleting file from queue", fileInfo->GetFilename());
			fileCompleted = false;
			fileInfo->SetAutoDeleted(true);
			DeleteQueueEntry(downloadQueue, fileInfo);
		}
	}

	nzbInfo->SetDownloadedSize(nzbInfo->GetDownloadedSize() + articleDownloader->GetDownloadedSize());

	bool deleteFileObj = false;

	if (fileCompleted && !fileInfo->GetDeleted())
	{
		// all jobs done
		DownloadQueue::Unlock();
		articleDownloader->CompleteFileParts();
		downloadQueue = DownloadQueue::Lock();
		deleteFileObj = true;
	}

	CheckHealth(downloadQueue, fileInfo);

	bool hasOtherDownloaders = false;
	for (ActiveDownloads::iterator it = m_activeDownloads.begin(); it != m_activeDownloads.end(); it++)
	{
		ArticleDownloader* downloader = *it;
		if (downloader != articleDownloader && downloader->GetFileInfo() == fileInfo)
		{
			hasOtherDownloaders = true;
			break;
		}
	}
	deleteFileObj |= fileInfo->GetDeleted() && !hasOtherDownloaders;

	// remove downloader from downloader list
	m_activeDownloads.erase(std::find(m_activeDownloads.begin(), m_activeDownloads.end(), articleDownloader));

	fileInfo->SetActiveDownloads(fileInfo->GetActiveDownloads() - 1);
	nzbInfo->SetActiveDownloads(nzbInfo->GetActiveDownloads() - 1);

	if (deleteFileObj)
	{
		DeleteFileInfo(downloadQueue, fileInfo, fileCompleted);
		downloadQueue->Save();
	}

	DownloadQueue::Unlock();
}

void QueueCoordinator::StatFileInfo(FileInfo* fileInfo, bool completed)
{
	NzbInfo* nzbInfo = fileInfo->GetNzbInfo();
	if (completed || nzbInfo->GetDeleting())
	{
		nzbInfo->SetSuccessSize(nzbInfo->GetSuccessSize() + fileInfo->GetSuccessSize());
		nzbInfo->SetFailedSize(nzbInfo->GetFailedSize() + fileInfo->GetFailedSize());
		nzbInfo->SetFailedArticles(nzbInfo->GetFailedArticles() + fileInfo->GetFailedArticles() + fileInfo->GetMissedArticles());
		nzbInfo->SetSuccessArticles(nzbInfo->GetSuccessArticles() + fileInfo->GetSuccessArticles());
		if (fileInfo->GetParFile())
		{
			nzbInfo->SetParSuccessSize(nzbInfo->GetParSuccessSize() + fileInfo->GetSuccessSize());
			nzbInfo->SetParFailedSize(nzbInfo->GetParFailedSize() + fileInfo->GetFailedSize());
		}
		nzbInfo->GetServerStats()->ListOp(fileInfo->GetServerStats(), ServerStatList::soAdd);
	}
	else if (!nzbInfo->GetDeleting() && !nzbInfo->GetParCleanup())
	{
		// file deleted but not the whole nzb and not par-cleanup
		nzbInfo->SetFileCount(nzbInfo->GetFileCount() - 1);
		nzbInfo->SetSize(nzbInfo->GetSize() - fileInfo->GetSize());
		nzbInfo->SetCurrentSuccessSize(nzbInfo->GetCurrentSuccessSize() - fileInfo->GetSuccessSize());
		nzbInfo->SetFailedSize(nzbInfo->GetFailedSize() - fileInfo->GetMissedSize());
		nzbInfo->SetCurrentFailedSize(nzbInfo->GetCurrentFailedSize() - fileInfo->GetFailedSize() - fileInfo->GetMissedSize());
		nzbInfo->SetTotalArticles(nzbInfo->GetTotalArticles() - fileInfo->GetTotalArticles());
		nzbInfo->SetCurrentSuccessArticles(nzbInfo->GetCurrentSuccessArticles() - fileInfo->GetSuccessArticles());
		nzbInfo->SetCurrentFailedArticles(nzbInfo->GetCurrentFailedArticles() - fileInfo->GetFailedArticles());
		nzbInfo->GetCurrentServerStats()->ListOp(fileInfo->GetServerStats(), ServerStatList::soSubtract);
		if (fileInfo->GetParFile())
		{
			nzbInfo->SetParSize(nzbInfo->GetParSize() - fileInfo->GetSize());
			nzbInfo->SetParCurrentSuccessSize(nzbInfo->GetParCurrentSuccessSize() - fileInfo->GetSuccessSize());
			nzbInfo->SetParFailedSize(nzbInfo->GetParFailedSize() - fileInfo->GetMissedSize());
			nzbInfo->SetParCurrentFailedSize(nzbInfo->GetParCurrentFailedSize() - fileInfo->GetFailedSize() - fileInfo->GetMissedSize());
		}
		nzbInfo->SetRemainingSize(nzbInfo->GetRemainingSize() - fileInfo->GetRemainingSize());
		if (fileInfo->GetPaused())
		{
			nzbInfo->SetPausedSize(nzbInfo->GetPausedSize() - fileInfo->GetRemainingSize());
		}
	}

	if (fileInfo->GetParFile())
	{
		nzbInfo->SetRemainingParCount(nzbInfo->GetRemainingParCount() - 1);
	}
	if (fileInfo->GetPaused())
	{
		nzbInfo->SetPausedFileCount(nzbInfo->GetPausedFileCount() - 1);
	}
}

void QueueCoordinator::DeleteFileInfo(DownloadQueue* downloadQueue, FileInfo* fileInfo, bool completed)
{
	while (g_ArticleCache->FileBusy(fileInfo))
	{
		usleep(5*1000);
	}

	bool fileDeleted = fileInfo->GetDeleted();
	fileInfo->SetDeleted(true);

	StatFileInfo(fileInfo, completed);

	if (g_Options->GetSaveQueue() && g_Options->GetServerMode() &&
		(!completed || (fileInfo->GetMissedArticles() == 0 && fileInfo->GetFailedArticles() == 0)))
	{
		g_DiskState->DiscardFile(fileInfo, true, true, false);
	}

	if (!completed)
	{
		DiscardDiskFile(fileInfo);
	}

	NzbInfo* nzbInfo = fileInfo->GetNzbInfo();

	DownloadQueue::Aspect aspect = { completed && !fileDeleted ?
		DownloadQueue::eaFileCompleted : DownloadQueue::eaFileDeleted,
		downloadQueue, nzbInfo, fileInfo };
	downloadQueue->Notify(&aspect);

	// nzb-file could be deleted from queue in "Notify", check if it is still in queue.
	if (std::find(downloadQueue->GetQueue()->begin(), downloadQueue->GetQueue()->end(), nzbInfo) !=
		downloadQueue->GetQueue()->end())
	{
		nzbInfo->GetFileList()->Remove(fileInfo);
		delete fileInfo;
	}
}

void QueueCoordinator::DiscardDiskFile(FileInfo* fileInfo)
{
	// deleting temporary files

	if (!g_Options->GetDirectWrite())
	{
		for (FileInfo::Articles::iterator it = fileInfo->GetArticles()->begin(); it != fileInfo->GetArticles()->end(); it++)
		{
			ArticleInfo* pa = *it;
			if (pa->GetResultFilename())
			{
				remove(pa->GetResultFilename());
			}
		}
	}

	if (g_Options->GetDirectWrite() && fileInfo->GetOutputFilename())
	{
		remove(fileInfo->GetOutputFilename());
	}
}

void QueueCoordinator::SavePartialState()
{
	if (!(g_Options->GetServerMode() && g_Options->GetSaveQueue() && g_Options->GetContinuePartial()))
	{
		return;
	}

	DownloadQueue* downloadQueue = DownloadQueue::Lock();

	for (NzbList::iterator it = downloadQueue->GetQueue()->begin(); it != downloadQueue->GetQueue()->end(); it++)
	{
		NzbInfo* nzbInfo = *it;
		for (FileList::iterator it2 = nzbInfo->GetFileList()->begin(); it2 != nzbInfo->GetFileList()->end(); it2++)
		{
			FileInfo* fileInfo = *it2;
			if (fileInfo->GetPartialChanged())
			{
				debug("Saving partial state for %s", fileInfo->GetFilename());
				g_DiskState->SaveFileState(fileInfo, false);
				fileInfo->SetPartialChanged(false);
			}
		}
	}

	DownloadQueue::Unlock();
}

void QueueCoordinator::CheckHealth(DownloadQueue* downloadQueue, FileInfo* fileInfo)
{
	if (g_Options->GetHealthCheck() == Options::hcNone ||
		fileInfo->GetNzbInfo()->GetHealthPaused() ||
		fileInfo->GetNzbInfo()->GetDeleteStatus() == NzbInfo::dsHealth ||
		fileInfo->GetNzbInfo()->CalcHealth() >= fileInfo->GetNzbInfo()->CalcCriticalHealth(true) ||
		(g_Options->GetParScan() == Options::psDupe && g_Options->GetHealthCheck() == Options::hcDelete &&
		 fileInfo->GetNzbInfo()->GetSuccessArticles() * 100 / fileInfo->GetNzbInfo()->GetTotalArticles() > 10))
	{
		return;
	}

	if (g_Options->GetHealthCheck() == Options::hcPause)
	{
		warn("Pausing %s due to health %.1f%% below critical %.1f%%", fileInfo->GetNzbInfo()->GetName(),
			fileInfo->GetNzbInfo()->CalcHealth() / 10.0, fileInfo->GetNzbInfo()->CalcCriticalHealth(true) / 10.0);
		fileInfo->GetNzbInfo()->SetHealthPaused(true);
		downloadQueue->EditEntry(fileInfo->GetNzbInfo()->GetId(), DownloadQueue::eaGroupPause, 0, NULL);
	}
	else if (g_Options->GetHealthCheck() == Options::hcDelete)
	{
		fileInfo->GetNzbInfo()->PrintMessage(Message::mkWarning,
			"Cancelling download and deleting %s due to health %.1f%% below critical %.1f%%",
			fileInfo->GetNzbInfo()->GetName(), fileInfo->GetNzbInfo()->CalcHealth() / 10.0,
			fileInfo->GetNzbInfo()->CalcCriticalHealth(true) / 10.0);
		fileInfo->GetNzbInfo()->SetDeleteStatus(NzbInfo::dsHealth);
		downloadQueue->EditEntry(fileInfo->GetNzbInfo()->GetId(), DownloadQueue::eaGroupDelete, 0, NULL);
	}
}

void QueueCoordinator::LogDebugInfo()
{
	DownloadQueue* downloadQueue = DownloadQueue::Lock();

	info("   ---------- Queue");
	int64 remaining, remainingForced;
	downloadQueue->CalcRemainingSize(&remaining, &remainingForced);
	info("     Remaining: %.1f MB, Forced: %.1f MB", remaining / 1024.0 / 1024.0, remainingForced / 1024.0 / 1024.0);
	info("     Download: %s, Post-process: %s, Scan: %s",
		 (g_Options->GetPauseDownload() ? "paused" : g_Options->GetTempPauseDownload() ? "temp-paused" : "active"),
		 (g_Options->GetPausePostProcess() ? "paused" : "active"),
		 (g_Options->GetPauseScan() ? "paused" : "active"));

	info("   ---------- QueueCoordinator");
	info("    Active Downloads: %i, Limit: %i", (int)m_activeDownloads.size(), m_downloadsLimit);
	for (ActiveDownloads::iterator it = m_activeDownloads.begin(); it != m_activeDownloads.end(); it++)
	{
		ArticleDownloader* articleDownloader = *it;
		articleDownloader->LogDebugInfo();
	}
	DownloadQueue::Unlock();
}

void QueueCoordinator::ResetHangingDownloads()
{
	if (g_Options->GetTerminateTimeout() == 0 && g_Options->GetArticleTimeout() == 0)
	{
		return;
	}

	DownloadQueue::Lock();
	time_t tm = ::time(NULL);

	for (ActiveDownloads::iterator it = m_activeDownloads.begin(); it != m_activeDownloads.end();)
	{
		ArticleDownloader* articleDownloader = *it;

		if (tm - articleDownloader->GetLastUpdateTime() > g_Options->GetArticleTimeout() + 1 &&
		   articleDownloader->GetStatus() == ArticleDownloader::adRunning)
		{
			error("Cancelling hanging download %s @ %s", articleDownloader->GetInfoName(),
				articleDownloader->GetConnectionName());
			articleDownloader->Stop();
		}

		if (tm - articleDownloader->GetLastUpdateTime() > g_Options->GetTerminateTimeout() &&
		   articleDownloader->GetStatus() == ArticleDownloader::adRunning)
		{
			ArticleInfo* articleInfo = articleDownloader->GetArticleInfo();
			debug("Terminating hanging download %s", articleDownloader->GetInfoName());
			if (articleDownloader->Terminate())
			{
				error("Terminated hanging download %s @ %s", articleDownloader->GetInfoName(),
					articleDownloader->GetConnectionName());
				articleInfo->SetStatus(ArticleInfo::aiUndefined);
			}
			else
			{
				error("Could not terminate hanging download %s @ %s", articleDownloader->GetInfoName(),
					  articleDownloader->GetConnectionName());
			}
			m_activeDownloads.erase(it);

			articleDownloader->GetFileInfo()->SetActiveDownloads(articleDownloader->GetFileInfo()->GetActiveDownloads() - 1);
			articleDownloader->GetFileInfo()->GetNzbInfo()->SetActiveDownloads(articleDownloader->GetFileInfo()->GetNzbInfo()->GetActiveDownloads() - 1);
			articleDownloader->GetFileInfo()->GetNzbInfo()->SetDownloadedSize(articleDownloader->GetFileInfo()->GetNzbInfo()->GetDownloadedSize() + articleDownloader->GetDownloadedSize());

			// it's not safe to destroy pArticleDownloader, because the state of object is unknown
			delete articleDownloader;
			it = m_activeDownloads.begin();
			continue;
		}
		it++;
	}

	DownloadQueue::Unlock();
}

/*
 * Returns True if Entry was deleted from Queue or False if it was scheduled for Deletion.
 * NOTE: "False" does not mean unsuccess; the entry is (or will be) deleted in any case.
 */
bool QueueCoordinator::DeleteQueueEntry(DownloadQueue* downloadQueue, FileInfo* fileInfo)
{
	fileInfo->SetDeleted(true);
	bool downloading = false;
	for (ActiveDownloads::iterator it = m_activeDownloads.begin(); it != m_activeDownloads.end(); it++)
	{
		ArticleDownloader* articleDownloader = *it;
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
	for (FileList::iterator it = srcNzbInfo->GetFileList()->begin(); it != srcNzbInfo->GetFileList()->end(); it++)
	{
		FileInfo* fileInfo = *it;
		fileInfo->SetNzbInfo(destNzbInfo);
		destNzbInfo->GetFileList()->push_back(fileInfo);
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
	for (CompletedFiles::iterator it = srcNzbInfo->GetCompletedFiles()->begin(); it != srcNzbInfo->GetCompletedFiles()->end(); it++)
	{
		CompletedFile* completedFile = *it;
		destNzbInfo->GetCompletedFiles()->push_back(completedFile);
	}
	srcNzbInfo->GetCompletedFiles()->clear();

	// concatenate QueuedFilenames using character '|' as separator
	CString queuedFilename;
	queuedFilename.Format("%s|%s", destNzbInfo->GetQueuedFilename(), srcNzbInfo->GetQueuedFilename());
	destNzbInfo->SetQueuedFilename(queuedFilename);

	downloadQueue->GetQueue()->Remove(srcNzbInfo);
	g_DiskState->DiscardFiles(srcNzbInfo);
	delete srcNzbInfo;

	return true;
}

/*
 * Creates new nzb-item out of existing files from other nzb-items.
 * If any of file-items is being downloaded the command fail.
 * For each file-item an event "eaFileDeleted" is fired.
 */
bool QueueCoordinator::SplitQueueEntries(DownloadQueue* downloadQueue, FileList* fileList, const char* name, NzbInfo** newNzbInfo)
{
	if (fileList->empty())
	{
		return false;
	}

	NzbInfo* srcNzbInfo = NULL;

	for (FileList::iterator it = fileList->begin(); it != fileList->end(); it++)
	{
		FileInfo* fileInfo = *it;
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

	NzbInfo* nzbInfo = new NzbInfo();
	downloadQueue->GetQueue()->push_back(nzbInfo);

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

	for (FileList::iterator it = fileList->begin(); it != fileList->end(); it++)
	{
		FileInfo* fileInfo = *it;

		DownloadQueue::Aspect aspect = { DownloadQueue::eaFileDeleted, downloadQueue, fileInfo->GetNzbInfo(), fileInfo };
		downloadQueue->Notify(&aspect);

		fileInfo->SetNzbInfo(nzbInfo);
		nzbInfo->GetFileList()->push_back(fileInfo);
		srcNzbInfo->GetFileList()->Remove(fileInfo);

		srcNzbInfo->SetFileCount(srcNzbInfo->GetFileCount() - 1);
		srcNzbInfo->SetSize(srcNzbInfo->GetSize() - fileInfo->GetSize());
		srcNzbInfo->SetRemainingSize(srcNzbInfo->GetRemainingSize() - fileInfo->GetRemainingSize());
		srcNzbInfo->SetCurrentSuccessSize(srcNzbInfo->GetCurrentSuccessSize() - fileInfo->GetSuccessSize());
		srcNzbInfo->SetCurrentFailedSize(srcNzbInfo->GetCurrentFailedSize() - fileInfo->GetFailedSize() - fileInfo->GetMissedSize());
		srcNzbInfo->SetTotalArticles(srcNzbInfo->GetTotalArticles() - fileInfo->GetTotalArticles());
		srcNzbInfo->SetCurrentSuccessArticles(srcNzbInfo->GetCurrentSuccessArticles() - fileInfo->GetSuccessArticles());
		srcNzbInfo->SetCurrentFailedArticles(srcNzbInfo->GetCurrentFailedArticles() - fileInfo->GetFailedArticles());
		srcNzbInfo->GetCurrentServerStats()->ListOp(fileInfo->GetServerStats(), ServerStatList::soSubtract);

		nzbInfo->SetFileCount(nzbInfo->GetFileCount() + 1);
		nzbInfo->SetSize(nzbInfo->GetSize() + fileInfo->GetSize());
		nzbInfo->SetRemainingSize(nzbInfo->GetRemainingSize() + fileInfo->GetRemainingSize());
		nzbInfo->SetCurrentSuccessSize(nzbInfo->GetCurrentSuccessSize() + fileInfo->GetSuccessSize());
		nzbInfo->SetCurrentFailedSize(nzbInfo->GetCurrentFailedSize() + fileInfo->GetFailedSize() + fileInfo->GetMissedSize());
		nzbInfo->SetTotalArticles(nzbInfo->GetTotalArticles() + fileInfo->GetTotalArticles());
		nzbInfo->SetCurrentSuccessArticles(nzbInfo->GetCurrentSuccessArticles() + fileInfo->GetSuccessArticles());
		nzbInfo->SetCurrentFailedArticles(nzbInfo->GetCurrentFailedArticles() + fileInfo->GetFailedArticles());
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
		downloadQueue->GetQueue()->Remove(srcNzbInfo);
		g_DiskState->DiscardFiles(srcNzbInfo);
		delete srcNzbInfo;
	}

	*newNzbInfo = nzbInfo;
	return true;
}
