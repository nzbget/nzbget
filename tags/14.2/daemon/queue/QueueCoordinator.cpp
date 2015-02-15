/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2005 Bo Cordes Petersen <placebodk@users.sourceforge.net>
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
#include <sys/stat.h>
#ifndef WIN32
#include <unistd.h>
#include <sys/time.h>
#endif
#include <algorithm>

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

extern Options* g_pOptions;
extern ServerPool* g_pServerPool;
extern DiskState* g_pDiskState;
extern StatMeter* g_pStatMeter;
extern ArticleCache* g_pArticleCache;

bool QueueCoordinator::CoordinatorDownloadQueue::EditEntry(
	int ID, EEditAction eAction, int iOffset, const char* szText)
{
	return m_pOwner->m_QueueEditor.EditEntry(&m_pOwner->m_DownloadQueue, ID, eAction, iOffset, szText);
}

bool QueueCoordinator::CoordinatorDownloadQueue::EditList(
	IDList* pIDList, NameList* pNameList, EMatchMode eMatchMode, EEditAction eAction, int iOffset, const char* szText)
{
	return m_pOwner->m_QueueEditor.EditList(&m_pOwner->m_DownloadQueue, pIDList, pNameList, eMatchMode, eAction, iOffset, szText);
}

void QueueCoordinator::CoordinatorDownloadQueue::Save()
{
	if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
	{
		g_pDiskState->SaveDownloadQueue(this);
	}
}

QueueCoordinator::QueueCoordinator()
{
	debug("Creating QueueCoordinator");

	m_bHasMoreJobs = true;
	m_iServerConfigGeneration = 0;

	g_pLog->RegisterDebuggable(this);

	m_DownloadQueue.m_pOwner = this;
	CoordinatorDownloadQueue::Init(&m_DownloadQueue);
}

QueueCoordinator::~QueueCoordinator()
{
	debug("Destroying QueueCoordinator");
	// Cleanup

	g_pLog->UnregisterDebuggable(this);

	debug("Deleting ArticleDownloaders");
	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
	{
		delete *it;
	}
	m_ActiveDownloads.clear();

	CoordinatorDownloadQueue::Final();

	debug("QueueCoordinator destroyed");
}

void QueueCoordinator::Load()
{
	DownloadQueue* pDownloadQueue = DownloadQueue::Lock();

	bool bStatLoaded = true;
	bool bPerfectServerMatch = true;
	bool bQueueLoaded = false;

	if (g_pOptions->GetServerMode() && g_pOptions->GetSaveQueue())
	{
		bStatLoaded = g_pStatMeter->Load(&bPerfectServerMatch);

		if (g_pOptions->GetReloadQueue() && g_pDiskState->DownloadQueueExists())
		{
			bQueueLoaded = g_pDiskState->LoadDownloadQueue(pDownloadQueue, g_pServerPool->GetServers());
		}
		else
		{
			g_pDiskState->DiscardDownloadQueue();
		}
	}

	if (bQueueLoaded && bStatLoaded)
	{
		g_pDiskState->CleanupTempDir(pDownloadQueue);
	}

	if (bQueueLoaded && bStatLoaded && !bPerfectServerMatch)
	{
		debug("Changes in section <NEWS SERVERS> of config file detected, resaving queue");

		// re-save current server list into diskstate to update server ids
		g_pStatMeter->Save();

		// re-save queue into diskstate to update server ids
		pDownloadQueue->Save();

		// re-save file states into diskstate to update server ids
		if (g_pOptions->GetServerMode() && g_pOptions->GetSaveQueue())
		{
			for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
			{
				NZBInfo* pNZBInfo = *it;

				if (g_pOptions->GetContinuePartial())
				{
					for (FileList::iterator it2 = pNZBInfo->GetFileList()->begin(); it2 != pNZBInfo->GetFileList()->end(); it2++)
					{
						FileInfo* pFileInfo = *it2;
						if (!pFileInfo->GetArticles()->empty())
						{
							g_pDiskState->SaveFileState(pFileInfo, false);
						}
					}
				}

				for (CompletedFiles::iterator it2 = pNZBInfo->GetCompletedFiles()->begin(); it2 != pNZBInfo->GetCompletedFiles()->end(); it2++)
				{
					CompletedFile* pCompletedFile = *it2;
					if (pCompletedFile->GetStatus() != CompletedFile::cfSuccess && pCompletedFile->GetID() > 0)
					{
						FileInfo* pFileInfo = new FileInfo(pCompletedFile->GetID());
						if (g_pDiskState->LoadFileState(pFileInfo, g_pServerPool->GetServers(), false))
						{
							g_pDiskState->SaveFileState(pFileInfo, true);
						}
						delete pFileInfo;
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
	bool bWasStandBy = true;
	bool bArticeDownloadsRunning = false;
	int iResetCounter = 0;
	g_pStatMeter->IntervalCheck();

	while (!IsStopped())
	{
		bool bDownloadsChecked = false;
		bool bDownloadStarted = false;
		NNTPConnection* pConnection = g_pServerPool->GetConnection(0, NULL, NULL);
		if (pConnection)
		{
			// start download for next article
			FileInfo* pFileInfo;
			ArticleInfo* pArticleInfo;
			bool bFreeConnection = false;
			
			DownloadQueue* pDownloadQueue = DownloadQueue::Lock();
			bool bHasMoreArticles = GetNextArticle(pDownloadQueue, pFileInfo, pArticleInfo);
			bArticeDownloadsRunning = !m_ActiveDownloads.empty();
			bDownloadsChecked = true;
			m_bHasMoreJobs = bHasMoreArticles || bArticeDownloadsRunning;
			if (bHasMoreArticles && !IsStopped() && (int)m_ActiveDownloads.size() < m_iDownloadsLimit &&
				(!g_pOptions->GetTempPauseDownload() || pFileInfo->GetExtraPriority()))
			{
				StartArticleDownload(pFileInfo, pArticleInfo, pConnection);
				bArticeDownloadsRunning = true;
				bDownloadStarted = true;
			}
			else
			{
				bFreeConnection = true;
			}
			DownloadQueue::Unlock();
			
			if (bFreeConnection)
			{
				g_pServerPool->FreeConnection(pConnection, false);
			}
		}

		if (!bDownloadsChecked)
		{
			DownloadQueue::Lock();
			bArticeDownloadsRunning = !m_ActiveDownloads.empty();
			DownloadQueue::Unlock();
		}

		bool bStandBy = !bArticeDownloadsRunning;
		if (bStandBy != bWasStandBy)
		{
			g_pStatMeter->EnterLeaveStandBy(bStandBy);
			bWasStandBy = bStandBy;
			if (bStandBy)
			{
				SavePartialState();
			}
		}

		// sleep longer in StandBy
		int iSleepInterval = bDownloadStarted ? 0 : bStandBy ? 100 : 5;
		usleep(iSleepInterval * 1000);

		if (!bStandBy)
		{
			g_pStatMeter->AddSpeedReading(0);
		}

		Util::SetStandByMode(bStandBy);

		iResetCounter += iSleepInterval;
		if (iResetCounter >= 1000)
		{
			// this code should not be called too often, once per second is OK
			g_pServerPool->CloseUnusedConnections();
			ResetHangingDownloads();
			if (!bStandBy)
			{
				SavePartialState();
			}
			iResetCounter = 0;
			g_pStatMeter->IntervalCheck();
			AdjustDownloadsLimit();
		}
	}

	// waiting for downloads
	debug("QueueCoordinator: waiting for Downloads to complete");
	bool completed = false;
	while (!completed)
	{
		DownloadQueue::Lock();
		completed = m_ActiveDownloads.size() == 0;
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
	if (m_iServerConfigGeneration == g_pServerPool->GetGeneration())
	{
		return;
	}

	// two extra threads for completing files (when connections are not needed)
	int iDownloadsLimit = 2;

	// allow one thread per 0-level (main) and 1-level (backup) server connection
	for (Servers::iterator it = g_pServerPool->GetServers()->begin(); it != g_pServerPool->GetServers()->end(); it++)
	{
		NewsServer* pNewsServer = *it;
		if ((pNewsServer->GetNormLevel() == 0 || pNewsServer->GetNormLevel() == 1) && pNewsServer->GetActive())
		{
			iDownloadsLimit += pNewsServer->GetMaxConnections();
		}
	}

	m_iDownloadsLimit = iDownloadsLimit;
}

void QueueCoordinator::AddNZBFileToQueue(NZBFile* pNZBFile, NZBInfo* pUrlInfo, bool bAddFirst)
{
	debug("Adding NZBFile to queue");

	NZBInfo* pNZBInfo = pNZBFile->GetNZBInfo();

	DownloadQueue* pDownloadQueue = DownloadQueue::Lock();

	DownloadQueue::Aspect foundAspect = { DownloadQueue::eaNzbFound, pDownloadQueue, pNZBInfo, NULL };
	pDownloadQueue->Notify(&foundAspect);

	NZBInfo::EDeleteStatus eDeleteStatus = pNZBInfo->GetDeleteStatus();

	if (eDeleteStatus != NZBInfo::dsNone)
	{
		bool bAllPaused = !pNZBInfo->GetFileList()->empty();
		for (FileList::iterator it = pNZBInfo->GetFileList()->begin(); it != pNZBInfo->GetFileList()->end(); it++)
		{
			FileInfo* pFileInfo = *it;
			bAllPaused &= pFileInfo->GetPaused();
			if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
			{
				g_pDiskState->DiscardFile(pFileInfo, true, false, false);
			}
		}
		pNZBInfo->SetDeletePaused(bAllPaused);
	}

	if (eDeleteStatus != NZBInfo::dsManual)
	{
		// NZBInfo will be added either to queue or to history as duplicate
		// and therefore can be detached from NZBFile.
		pNZBFile->DetachNZBInfo();
	}

	if (eDeleteStatus == NZBInfo::dsNone)
	{
		if (g_pOptions->GetDupeCheck() && pNZBInfo->GetDupeMode() != dmForce)
		{
			CheckDupeFileInfos(pNZBInfo);
		}

		if (pUrlInfo)
		{
			// insert at the URL position
			for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
			{
				NZBInfo* pPosNzbInfo = *it;
				if (pPosNzbInfo == pUrlInfo)
				{
					pDownloadQueue->GetQueue()->insert(it, pNZBInfo);
					break;
				}
			}
		}
		else if (bAddFirst)
		{
			pDownloadQueue->GetQueue()->push_front(pNZBInfo);
		}
		else
		{
			pDownloadQueue->GetQueue()->push_back(pNZBInfo);
		}
	}

	if (pUrlInfo)
	{
		pNZBInfo->SetID(pUrlInfo->GetID());
		pDownloadQueue->GetQueue()->Remove(pUrlInfo);
		delete pUrlInfo;
	}

	if (eDeleteStatus == NZBInfo::dsNone)
	{
		info("Collection %s added to queue", pNZBInfo->GetName());
	}

	if (eDeleteStatus != NZBInfo::dsManual)
	{
		DownloadQueue::Aspect addedAspect = { DownloadQueue::eaNzbAdded, pDownloadQueue, pNZBInfo, NULL };
		pDownloadQueue->Notify(&addedAspect);
	}
		
	pDownloadQueue->Save();

	DownloadQueue::Unlock();
}

void QueueCoordinator::CheckDupeFileInfos(NZBInfo* pNZBInfo)
{
	debug("CheckDupeFileInfos");

	if (!g_pOptions->GetDupeCheck() || pNZBInfo->GetDupeMode() == dmForce)
	{
		return;
	}

	FileList dupeList(true);

	int index1 = 0;
	for (FileList::iterator it = pNZBInfo->GetFileList()->begin(); it != pNZBInfo->GetFileList()->end(); it++)
	{
		index1++;
		FileInfo* pFileInfo = *it;

		bool dupe = false;
		int index2 = 0;
		for (FileList::iterator it2 =  pNZBInfo->GetFileList()->begin(); it2 !=  pNZBInfo->GetFileList()->end(); it2++)
		{
			index2++;
			FileInfo* pFileInfo2 = *it2;
			if (pFileInfo != pFileInfo2 &&
				!strcmp(pFileInfo->GetFilename(), pFileInfo2->GetFilename()) &&
				(pFileInfo->GetSize() < pFileInfo2->GetSize() || 
				 (pFileInfo->GetSize() == pFileInfo2->GetSize() && index2 < index1)))
			{
				warn("File \"%s\" appears twice in collection, adding only the biggest file", pFileInfo->GetFilename());
				dupe = true;
				break;
			}
		}
		if (dupe)
		{
			dupeList.push_back(pFileInfo);
			continue;
		}
	}

	for (FileList::iterator it = dupeList.begin(); it != dupeList.end(); it++)
	{
		FileInfo* pFileInfo = *it;
		StatFileInfo(pFileInfo, false);
		pNZBInfo->GetFileList()->Remove(pFileInfo);
		if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
		{
			g_pDiskState->DiscardFile(pFileInfo, true, false, false);
		}
	}
}

void QueueCoordinator::Stop()
{
	Thread::Stop();

	debug("Stopping ArticleDownloads");
	DownloadQueue::Lock();
	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
	{
		(*it)->Stop();
	}
	DownloadQueue::Unlock();
	debug("ArticleDownloads are notified");
}

/*
 * Returns next article for download.
 */
bool QueueCoordinator::GetNextArticle(DownloadQueue* pDownloadQueue, FileInfo* &pFileInfo, ArticleInfo* &pArticleInfo)
{
	// find an unpaused file with the highest priority, then take the next article from the file.
	// if the file doesn't have any articles left for download, we store that fact and search again,
	// ignoring all files which were previously marked as not having any articles.

	// special case: if the file has ExtraPriority-flag set, it has the highest priority and the
	// Paused-flag is ignored.

	//debug("QueueCoordinator::GetNextArticle()");

	bool bOK = false;

	// pCheckedFiles stores
	bool* pCheckedFiles = NULL;
	time_t tCurDate = time(NULL);

	while (!bOK) 
	{
		pFileInfo = NULL;
		int iNum = 0;
		int iFileNum = 0;

		for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
		{
			NZBInfo* pNZBInfo = *it;
			for (FileList::iterator it2 = pNZBInfo->GetFileList()->begin(); it2 != pNZBInfo->GetFileList()->end(); it2++)
			{
				FileInfo* pFileInfo1 = *it2;
				if ((!pCheckedFiles || !pCheckedFiles[iNum]) && 
					!pFileInfo1->GetPaused() && !pFileInfo1->GetDeleted() &&
					(g_pOptions->GetPropagationDelay() == 0 ||
					 (int)pFileInfo1->GetTime() < (int)tCurDate - g_pOptions->GetPropagationDelay()) &&
					(!g_pOptions->GetPauseDownload() || pNZBInfo->GetForcePriority()) &&
					(!pFileInfo ||
					 (pFileInfo1->GetExtraPriority() == pFileInfo->GetExtraPriority() &&
					  pFileInfo1->GetNZBInfo()->GetPriority() > pFileInfo->GetNZBInfo()->GetPriority()) ||
					 (pFileInfo1->GetExtraPriority() > pFileInfo->GetExtraPriority())))
				{
					pFileInfo = pFileInfo1;
					iFileNum = iNum;
				}
				iNum++;
			}
		}

		if (!pFileInfo)
		{
			// there are no more files for download
			break;
		}

		if (pFileInfo->GetArticles()->empty() && g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
		{
			g_pDiskState->LoadArticles(pFileInfo);
		}

		// check if the file has any articles left for download
		for (FileInfo::Articles::iterator at = pFileInfo->GetArticles()->begin(); at != pFileInfo->GetArticles()->end(); at++)
		{
			pArticleInfo = *at;
			if (pArticleInfo->GetStatus() == ArticleInfo::aiUndefined)
			{
				bOK = true;
				break;
			}
		}

		if (!bOK)
		{
			// the file doesn't have any articles left for download, we mark the file as such
			if (!pCheckedFiles)
			{
				int iTotalFileCount = 0;
				for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
				{
					NZBInfo* pNZBInfo = *it;
					iTotalFileCount += pNZBInfo->GetFileList()->size();
				}

				if (iTotalFileCount > 0)
				{
					int iArrSize = sizeof(bool) * iTotalFileCount;
					pCheckedFiles = (bool*)malloc(iArrSize);
					memset(pCheckedFiles, false, iArrSize);
				}
			}
			if (pCheckedFiles)
			{
				pCheckedFiles[iFileNum] = true;
			}
		}
	}

	free(pCheckedFiles);

	return bOK;
}

void QueueCoordinator::StartArticleDownload(FileInfo* pFileInfo, ArticleInfo* pArticleInfo, NNTPConnection* pConnection)
{
	debug("Starting new ArticleDownloader");

	ArticleDownloader* pArticleDownloader = new ArticleDownloader();
	pArticleDownloader->SetAutoDestroy(true);
	pArticleDownloader->Attach(this);
	pArticleDownloader->SetFileInfo(pFileInfo);
	pArticleDownloader->SetArticleInfo(pArticleInfo);
	pArticleDownloader->SetConnection(pConnection);

	char szInfoName[1024];
	snprintf(szInfoName, 1024, "%s%c%s [%i/%i]", pFileInfo->GetNZBInfo()->GetName(), (int)PATH_SEPARATOR, pFileInfo->GetFilename(), pArticleInfo->GetPartNumber(), (int)pFileInfo->GetArticles()->size());
	szInfoName[1024-1] = '\0';
	pArticleDownloader->SetInfoName(szInfoName);

	pArticleInfo->SetStatus(ArticleInfo::aiRunning);
	pFileInfo->SetActiveDownloads(pFileInfo->GetActiveDownloads() + 1);
	pFileInfo->GetNZBInfo()->SetActiveDownloads(pFileInfo->GetNZBInfo()->GetActiveDownloads() + 1);

	m_ActiveDownloads.push_back(pArticleDownloader);
	pArticleDownloader->Start();
}

void QueueCoordinator::Update(Subject* Caller, void* Aspect)
{
	debug("Notification from ArticleDownloader received");

	ArticleDownloader* pArticleDownloader = (ArticleDownloader*)Caller;
	if ((pArticleDownloader->GetStatus() == ArticleDownloader::adFinished) ||
		(pArticleDownloader->GetStatus() == ArticleDownloader::adFailed) ||
		(pArticleDownloader->GetStatus() == ArticleDownloader::adRetry))
	{
		ArticleCompleted(pArticleDownloader);
	}
}

void QueueCoordinator::ArticleCompleted(ArticleDownloader* pArticleDownloader)
{
	debug("Article downloaded");

	FileInfo* pFileInfo = pArticleDownloader->GetFileInfo();
	NZBInfo* pNZBInfo = pFileInfo->GetNZBInfo();
	ArticleInfo* pArticleInfo = pArticleDownloader->GetArticleInfo();
	bool bRetry = false;
	bool fileCompleted = false;

	DownloadQueue* pDownloadQueue = DownloadQueue::Lock();

	if (pArticleDownloader->GetStatus() == ArticleDownloader::adFinished)
	{
		pArticleInfo->SetStatus(ArticleInfo::aiFinished);
		pFileInfo->SetSuccessSize(pFileInfo->GetSuccessSize() + pArticleInfo->GetSize());
		pNZBInfo->SetCurrentSuccessSize(pNZBInfo->GetCurrentSuccessSize() + pArticleInfo->GetSize());
		pNZBInfo->SetParCurrentSuccessSize(pNZBInfo->GetParCurrentSuccessSize() + (pFileInfo->GetParFile() ? pArticleInfo->GetSize() : 0));
		pFileInfo->SetSuccessArticles(pFileInfo->GetSuccessArticles() + 1);
		pNZBInfo->SetCurrentSuccessArticles(pNZBInfo->GetCurrentSuccessArticles() + 1);
	}
	else if (pArticleDownloader->GetStatus() == ArticleDownloader::adFailed)
	{
		pArticleInfo->SetStatus(ArticleInfo::aiFailed);
		pFileInfo->SetFailedSize(pFileInfo->GetFailedSize() + pArticleInfo->GetSize());
		pNZBInfo->SetCurrentFailedSize(pNZBInfo->GetCurrentFailedSize() + pArticleInfo->GetSize());
		pNZBInfo->SetParCurrentFailedSize(pNZBInfo->GetParCurrentFailedSize() + (pFileInfo->GetParFile() ? pArticleInfo->GetSize() : 0));
		pFileInfo->SetFailedArticles(pFileInfo->GetFailedArticles() + 1);
		pNZBInfo->SetCurrentFailedArticles(pNZBInfo->GetCurrentFailedArticles() + 1);
	}
	else if (pArticleDownloader->GetStatus() == ArticleDownloader::adRetry)
	{
		pArticleInfo->SetStatus(ArticleInfo::aiUndefined);
		bRetry = true;
	}

	if (!bRetry)
	{
		pFileInfo->SetRemainingSize(pFileInfo->GetRemainingSize() - pArticleInfo->GetSize());
		pNZBInfo->SetRemainingSize(pNZBInfo->GetRemainingSize() - pArticleInfo->GetSize());
		if (pFileInfo->GetPaused())
		{
			pNZBInfo->SetPausedSize(pNZBInfo->GetPausedSize() - pArticleInfo->GetSize());
		}
		pFileInfo->SetCompletedArticles(pFileInfo->GetCompletedArticles() + 1);
		fileCompleted = (int)pFileInfo->GetArticles()->size() == pFileInfo->GetCompletedArticles();
		pFileInfo->GetServerStats()->ListOp(pArticleDownloader->GetServerStats(), ServerStatList::soAdd);
		pNZBInfo->GetCurrentServerStats()->ListOp(pArticleDownloader->GetServerStats(), ServerStatList::soAdd);
		pFileInfo->SetPartialChanged(true);
	}

	if (!pFileInfo->GetFilenameConfirmed() &&
		pArticleDownloader->GetStatus() == ArticleDownloader::adFinished &&
		pArticleDownloader->GetArticleFilename())
	{
		pFileInfo->SetFilename(pArticleDownloader->GetArticleFilename());
		pFileInfo->SetFilenameConfirmed(true);
		if (g_pOptions->GetDupeCheck() &&
			pNZBInfo->GetDupeMode() != dmForce &&
			!pNZBInfo->GetManyDupeFiles() &&
			Util::FileExists(pNZBInfo->GetDestDir(), pFileInfo->GetFilename()))
		{
			warn("File \"%s\" seems to be duplicate, cancelling download and deleting file from queue", pFileInfo->GetFilename());
			fileCompleted = false;
			pFileInfo->SetAutoDeleted(true);
			DeleteQueueEntry(pDownloadQueue, pFileInfo);
		}
	}

	pNZBInfo->SetDownloadedSize(pNZBInfo->GetDownloadedSize() + pArticleDownloader->GetDownloadedSize());

	bool deleteFileObj = false;

	if (fileCompleted && !pFileInfo->GetDeleted())
	{
		// all jobs done
		DownloadQueue::Unlock();
		pArticleDownloader->CompleteFileParts();
		pDownloadQueue = DownloadQueue::Lock();
		deleteFileObj = true;
	}

	CheckHealth(pDownloadQueue, pFileInfo);

	bool hasOtherDownloaders = false;
	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
	{
		ArticleDownloader* pDownloader = *it;
		if (pDownloader != pArticleDownloader && pDownloader->GetFileInfo() == pFileInfo)
		{
			hasOtherDownloaders = true;
			break;
		}
	}
	deleteFileObj |= pFileInfo->GetDeleted() && !hasOtherDownloaders;

	// remove downloader from downloader list
	m_ActiveDownloads.erase(std::find(m_ActiveDownloads.begin(), m_ActiveDownloads.end(), pArticleDownloader));

	pFileInfo->SetActiveDownloads(pFileInfo->GetActiveDownloads() - 1);
	pNZBInfo->SetActiveDownloads(pNZBInfo->GetActiveDownloads() - 1);

	if (deleteFileObj)
	{
		DeleteFileInfo(pDownloadQueue, pFileInfo, fileCompleted);
		pDownloadQueue->Save();
	}

	DownloadQueue::Unlock();
}

void QueueCoordinator::StatFileInfo(FileInfo* pFileInfo, bool bCompleted)
{
	NZBInfo* pNZBInfo = pFileInfo->GetNZBInfo();
	if (bCompleted || pNZBInfo->GetDeleting())
	{
		pNZBInfo->SetSuccessSize(pNZBInfo->GetSuccessSize() + pFileInfo->GetSuccessSize());
		pNZBInfo->SetFailedSize(pNZBInfo->GetFailedSize() + pFileInfo->GetFailedSize());
		pNZBInfo->SetFailedArticles(pNZBInfo->GetFailedArticles() + pFileInfo->GetFailedArticles() + pFileInfo->GetMissedArticles());
		pNZBInfo->SetSuccessArticles(pNZBInfo->GetSuccessArticles() + pFileInfo->GetSuccessArticles());
		if (pFileInfo->GetParFile())
		{
			pNZBInfo->SetParSuccessSize(pNZBInfo->GetParSuccessSize() + pFileInfo->GetSuccessSize());
			pNZBInfo->SetParFailedSize(pNZBInfo->GetParFailedSize() + pFileInfo->GetFailedSize());
		}
		pNZBInfo->GetServerStats()->ListOp(pFileInfo->GetServerStats(), ServerStatList::soAdd);
	}
	else if (!pNZBInfo->GetDeleting() && !pNZBInfo->GetParCleanup())
	{
		// file deleted but not the whole nzb and not par-cleanup
		pNZBInfo->SetFileCount(pNZBInfo->GetFileCount() - 1);
		pNZBInfo->SetSize(pNZBInfo->GetSize() - pFileInfo->GetSize());
		pNZBInfo->SetCurrentSuccessSize(pNZBInfo->GetCurrentSuccessSize() - pFileInfo->GetSuccessSize());
		pNZBInfo->SetFailedSize(pNZBInfo->GetFailedSize() - pFileInfo->GetMissedSize());
		pNZBInfo->SetCurrentFailedSize(pNZBInfo->GetCurrentFailedSize() - pFileInfo->GetFailedSize() - pFileInfo->GetMissedSize());
		pNZBInfo->SetTotalArticles(pNZBInfo->GetTotalArticles() - pFileInfo->GetTotalArticles());
		pNZBInfo->SetCurrentSuccessArticles(pNZBInfo->GetCurrentSuccessArticles() - pFileInfo->GetSuccessArticles());
		pNZBInfo->SetCurrentFailedArticles(pNZBInfo->GetCurrentFailedArticles() - pFileInfo->GetFailedArticles());
		pNZBInfo->GetCurrentServerStats()->ListOp(pFileInfo->GetServerStats(), ServerStatList::soSubtract);
		if (pFileInfo->GetParFile())
		{
			pNZBInfo->SetParSize(pNZBInfo->GetParSize() - pFileInfo->GetSize());
			pNZBInfo->SetParCurrentSuccessSize(pNZBInfo->GetParCurrentSuccessSize() - pFileInfo->GetSuccessSize());
			pNZBInfo->SetParFailedSize(pNZBInfo->GetParFailedSize() - pFileInfo->GetMissedSize());
			pNZBInfo->SetParCurrentFailedSize(pNZBInfo->GetParCurrentFailedSize() - pFileInfo->GetFailedSize() - pFileInfo->GetMissedSize());
		}
		pNZBInfo->SetRemainingSize(pNZBInfo->GetRemainingSize() - pFileInfo->GetRemainingSize());
		if (pFileInfo->GetPaused())
		{
			pNZBInfo->SetPausedSize(pNZBInfo->GetPausedSize() - pFileInfo->GetRemainingSize());
		}
	}

	if (pFileInfo->GetParFile())
	{
		pNZBInfo->SetRemainingParCount(pNZBInfo->GetRemainingParCount() - 1);
	}
	if (pFileInfo->GetPaused())
	{
		pNZBInfo->SetPausedFileCount(pNZBInfo->GetPausedFileCount() - 1);
	}
}

void QueueCoordinator::DeleteFileInfo(DownloadQueue* pDownloadQueue, FileInfo* pFileInfo, bool bCompleted)
{
	while (g_pArticleCache->FileBusy(pFileInfo))
	{
		usleep(5*1000);
	}

	bool fileDeleted = pFileInfo->GetDeleted();
	pFileInfo->SetDeleted(true);

	StatFileInfo(pFileInfo, bCompleted);

	if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode() &&
		(!bCompleted || (pFileInfo->GetMissedArticles() == 0 && pFileInfo->GetFailedArticles() == 0)))
	{
		g_pDiskState->DiscardFile(pFileInfo, true, true, false);
	}

	if (!bCompleted)
	{
		DiscardDiskFile(pFileInfo);
	}

	NZBInfo* pNZBInfo = pFileInfo->GetNZBInfo();

	DownloadQueue::Aspect aspect = { bCompleted && !fileDeleted ? 
		DownloadQueue::eaFileCompleted : DownloadQueue::eaFileDeleted,
		pDownloadQueue, pNZBInfo, pFileInfo };
	pDownloadQueue->Notify(&aspect);

	// nzb-file could be deleted from queue in "Notify", check if it is still in queue.
	if (std::find(pDownloadQueue->GetQueue()->begin(), pDownloadQueue->GetQueue()->end(), pNZBInfo) !=
		pDownloadQueue->GetQueue()->end())
	{
		pNZBInfo->GetFileList()->Remove(pFileInfo);
		delete pFileInfo;
	}
}

void QueueCoordinator::DiscardDiskFile(FileInfo* pFileInfo)
{
	// deleting temporary files

	if (!g_pOptions->GetDirectWrite())
	{
		for (FileInfo::Articles::iterator it = pFileInfo->GetArticles()->begin(); it != pFileInfo->GetArticles()->end(); it++)
		{
			ArticleInfo* pa = *it;
			if (pa->GetResultFilename())
			{
				remove(pa->GetResultFilename());
			}
		}
	}

	if (g_pOptions->GetDirectWrite() && pFileInfo->GetOutputFilename())
	{
		remove(pFileInfo->GetOutputFilename());
	}
}

void QueueCoordinator::SavePartialState()
{
	if (!(g_pOptions->GetServerMode() && g_pOptions->GetSaveQueue() && g_pOptions->GetContinuePartial()))
	{
		return;
	}

	DownloadQueue* pDownloadQueue = DownloadQueue::Lock();

	for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
	{
		NZBInfo* pNZBInfo = *it;
		for (FileList::iterator it2 = pNZBInfo->GetFileList()->begin(); it2 != pNZBInfo->GetFileList()->end(); it2++)
		{
			FileInfo* pFileInfo = *it2;
			if (pFileInfo->GetPartialChanged())
			{
				debug("Saving partial state for %s", pFileInfo->GetFilename());
				g_pDiskState->SaveFileState(pFileInfo, false);
				pFileInfo->SetPartialChanged(false);
			}
		}
	}

	DownloadQueue::Unlock();
}

void QueueCoordinator::CheckHealth(DownloadQueue* pDownloadQueue, FileInfo* pFileInfo)
{
	if (g_pOptions->GetHealthCheck() == Options::hcNone ||
		pFileInfo->GetNZBInfo()->GetHealthPaused() ||
		pFileInfo->GetNZBInfo()->GetDeleteStatus() == NZBInfo::dsHealth ||
		pFileInfo->GetNZBInfo()->CalcHealth() >= pFileInfo->GetNZBInfo()->CalcCriticalHealth(true))
	{
		return;
	}

	if (g_pOptions->GetHealthCheck() == Options::hcPause)
	{
		warn("Pausing %s due to health %.1f%% below critical %.1f%%", pFileInfo->GetNZBInfo()->GetName(),
			pFileInfo->GetNZBInfo()->CalcHealth() / 10.0, pFileInfo->GetNZBInfo()->CalcCriticalHealth(true) / 10.0);
		pFileInfo->GetNZBInfo()->SetHealthPaused(true);
		pDownloadQueue->EditEntry(pFileInfo->GetNZBInfo()->GetID(), DownloadQueue::eaGroupPause, 0, NULL);
	}
	else if (g_pOptions->GetHealthCheck() == Options::hcDelete)
	{
		warn("Cancelling download and deleting %s due to health %.1f%% below critical %.1f%%", pFileInfo->GetNZBInfo()->GetName(),
			pFileInfo->GetNZBInfo()->CalcHealth() / 10.0, pFileInfo->GetNZBInfo()->CalcCriticalHealth(true) / 10.0);
		pFileInfo->GetNZBInfo()->SetDeleteStatus(NZBInfo::dsHealth);
		pDownloadQueue->EditEntry(pFileInfo->GetNZBInfo()->GetID(), DownloadQueue::eaGroupDelete, 0, NULL);
	}
}

void QueueCoordinator::LogDebugInfo()
{
	DownloadQueue* pDownloadQueue = DownloadQueue::Lock();

	info("   ---------- Queue");
	long long lRemaining, lRemainingForced;
	pDownloadQueue->CalcRemainingSize(&lRemaining, &lRemainingForced);
	info("     Remaining: %.1f MB, Forced: %.1f MB", lRemaining / 1024.0 / 1024.0, lRemainingForced / 1024.0 / 1024.0);
	info("     Download: %s, Post-process: %s, Scan: %s",
		 (g_pOptions->GetPauseDownload() ? "paused" : g_pOptions->GetTempPauseDownload() ? "temp-paused" : "active"),
		 (g_pOptions->GetPausePostProcess() ? "paused" : "active"),
		 (g_pOptions->GetPauseScan() ? "paused" : "active"));

	info("   ---------- QueueCoordinator");
	info("    Active Downloads: %i, Limit: %i", m_ActiveDownloads.size(), m_iDownloadsLimit);
	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
	{
		ArticleDownloader* pArticleDownloader = *it;
		pArticleDownloader->LogDebugInfo();
	}
	DownloadQueue::Unlock();
}

void QueueCoordinator::ResetHangingDownloads()
{
	if (g_pOptions->GetTerminateTimeout() == 0 && g_pOptions->GetArticleTimeout() == 0)
	{
		return;
	}

	DownloadQueue::Lock();
	time_t tm = ::time(NULL);

	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end();)
	{
		ArticleDownloader* pArticleDownloader = *it;
																		   
		if (tm - pArticleDownloader->GetLastUpdateTime() > g_pOptions->GetArticleTimeout() + 1 &&
		   pArticleDownloader->GetStatus() == ArticleDownloader::adRunning)
		{
			error("Cancelling hanging download %s @ %s", pArticleDownloader->GetInfoName(),
				pArticleDownloader->GetConnectionName());
			pArticleDownloader->Stop();
		}
		
		if (tm - pArticleDownloader->GetLastUpdateTime() > g_pOptions->GetTerminateTimeout() &&
		   pArticleDownloader->GetStatus() == ArticleDownloader::adRunning)
		{
			ArticleInfo* pArticleInfo = pArticleDownloader->GetArticleInfo();
			debug("Terminating hanging download %s", pArticleDownloader->GetInfoName());
			if (pArticleDownloader->Terminate())
			{
				error("Terminated hanging download %s @ %s", pArticleDownloader->GetInfoName(),
					pArticleDownloader->GetConnectionName());
				pArticleInfo->SetStatus(ArticleInfo::aiUndefined);
			}
			else
			{
				error("Could not terminate hanging download %s @ %s", pArticleDownloader->GetInfoName(),
					  pArticleDownloader->GetConnectionName());
			}
			m_ActiveDownloads.erase(it);

			pArticleDownloader->GetFileInfo()->SetActiveDownloads(pArticleDownloader->GetFileInfo()->GetActiveDownloads() - 1);
			pArticleDownloader->GetFileInfo()->GetNZBInfo()->SetActiveDownloads(pArticleDownloader->GetFileInfo()->GetNZBInfo()->GetActiveDownloads() - 1);
			pArticleDownloader->GetFileInfo()->GetNZBInfo()->SetDownloadedSize(pArticleDownloader->GetFileInfo()->GetNZBInfo()->GetDownloadedSize() + pArticleDownloader->GetDownloadedSize());

			// it's not safe to destroy pArticleDownloader, because the state of object is unknown
			delete pArticleDownloader;
			it = m_ActiveDownloads.begin();
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
bool QueueCoordinator::DeleteQueueEntry(DownloadQueue* pDownloadQueue, FileInfo* pFileInfo)
{
	pFileInfo->SetDeleted(true);
	bool bDownloading = false;
	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
	{
		ArticleDownloader* pArticleDownloader = *it;
		if (pArticleDownloader->GetFileInfo() == pFileInfo)
		{
			bDownloading = true;
			pArticleDownloader->Stop();
		}
	}

	if (!bDownloading)
	{
		DeleteFileInfo(pDownloadQueue, pFileInfo, false);
	}
	return bDownloading;
}

bool QueueCoordinator::SetQueueEntryCategory(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, const char* szCategory)
{
	if (pNZBInfo->GetPostInfo())
	{
		error("Could not change category for %s. File in post-process-stage", pNZBInfo->GetName());
		return false;
	}

	char szOldDestDir[1024];
	strncpy(szOldDestDir, pNZBInfo->GetDestDir(), 1024);
	szOldDestDir[1024-1] = '\0';

	pNZBInfo->SetCategory(szCategory);
	pNZBInfo->BuildDestDirName();

	bool bDirUnchanged = !strcmp(pNZBInfo->GetDestDir(), szOldDestDir);
	bool bOK = bDirUnchanged || ArticleWriter::MoveCompletedFiles(pNZBInfo, szOldDestDir);

	return bOK;
}

bool QueueCoordinator::SetQueueEntryName(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, const char* szName)
{
	if (pNZBInfo->GetPostInfo())
	{
		error("Could not rename %s. File in post-process-stage", pNZBInfo->GetName());
		return false;
	}

	if (Util::EmptyStr(szName))
	{
		error("Could not rename %s. The new name cannot be empty", pNZBInfo->GetName());
		return false;
	}

	char szNZBNicename[1024];
	NZBInfo::MakeNiceNZBName(szName, szNZBNicename, sizeof(szNZBNicename), false);
	pNZBInfo->SetName(szNZBNicename);

	if (pNZBInfo->GetKind() == NZBInfo::nkUrl)
	{
		char szFilename[1024];
		snprintf(szFilename, 1024, "%s.nzb", szNZBNicename);
		szFilename[1024-1] = '\0';
		pNZBInfo->SetFilename(szFilename);
		return true;
	}

	char szOldDestDir[1024];
	strncpy(szOldDestDir, pNZBInfo->GetDestDir(), 1024);
	szOldDestDir[1024-1] = '\0';

	pNZBInfo->BuildDestDirName();

	bool bDirUnchanged = !strcmp(pNZBInfo->GetDestDir(), szOldDestDir);
	bool bOK = bDirUnchanged || ArticleWriter::MoveCompletedFiles(pNZBInfo, szOldDestDir);

	return bOK;
}

bool QueueCoordinator::MergeQueueEntries(DownloadQueue* pDownloadQueue, NZBInfo* pDestNZBInfo, NZBInfo* pSrcNZBInfo)
{
	if (pDestNZBInfo->GetPostInfo() || pSrcNZBInfo->GetPostInfo())
	{
		error("Could not merge %s and %s. File in post-process-stage", pDestNZBInfo->GetName(), pSrcNZBInfo->GetName());
		return false;
	}

	if (pDestNZBInfo->GetKind() == NZBInfo::nkUrl || pSrcNZBInfo->GetKind() == NZBInfo::nkUrl)
	{
		error("Could not merge %s and %s. URLs cannot be merged", pDestNZBInfo->GetName(), pSrcNZBInfo->GetName());
		return false;
	}

	// set new dest directory, new category and move downloaded files to new dest directory
	pSrcNZBInfo->SetFilename(pSrcNZBInfo->GetFilename());
	SetQueueEntryCategory(pDownloadQueue, pSrcNZBInfo, pDestNZBInfo->GetCategory());

	// reattach file items to new NZBInfo-object
	for (FileList::iterator it = pSrcNZBInfo->GetFileList()->begin(); it != pSrcNZBInfo->GetFileList()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		pFileInfo->SetNZBInfo(pDestNZBInfo);
		pDestNZBInfo->GetFileList()->push_back(pFileInfo);
	}

	pSrcNZBInfo->GetFileList()->clear();

	pDestNZBInfo->SetFileCount(pDestNZBInfo->GetFileCount() + pSrcNZBInfo->GetFileCount());
	pDestNZBInfo->SetActiveDownloads(pDestNZBInfo->GetActiveDownloads() + pSrcNZBInfo->GetActiveDownloads());
	pDestNZBInfo->SetFullContentHash(0);
	pDestNZBInfo->SetFilteredContentHash(0);

	pDestNZBInfo->SetSize(pDestNZBInfo->GetSize() + pSrcNZBInfo->GetSize());
	pDestNZBInfo->SetRemainingSize(pDestNZBInfo->GetRemainingSize() + pSrcNZBInfo->GetRemainingSize());
	pDestNZBInfo->SetPausedFileCount(pDestNZBInfo->GetPausedFileCount() + pSrcNZBInfo->GetPausedFileCount());
	pDestNZBInfo->SetPausedSize(pDestNZBInfo->GetPausedSize() + pSrcNZBInfo->GetPausedSize());

	pDestNZBInfo->SetSuccessSize(pDestNZBInfo->GetSuccessSize() + pSrcNZBInfo->GetSuccessSize());
	pDestNZBInfo->SetCurrentSuccessSize(pDestNZBInfo->GetCurrentSuccessSize() + pSrcNZBInfo->GetCurrentSuccessSize());
	pDestNZBInfo->SetFailedSize(pDestNZBInfo->GetFailedSize() + pSrcNZBInfo->GetFailedSize());
	pDestNZBInfo->SetCurrentFailedSize(pDestNZBInfo->GetCurrentFailedSize() + pSrcNZBInfo->GetCurrentFailedSize());

	pDestNZBInfo->SetParSize(pDestNZBInfo->GetParSize() + pSrcNZBInfo->GetParSize());
	pDestNZBInfo->SetParSuccessSize(pDestNZBInfo->GetParSuccessSize() + pSrcNZBInfo->GetParSuccessSize());
	pDestNZBInfo->SetParCurrentSuccessSize(pDestNZBInfo->GetParCurrentSuccessSize() + pSrcNZBInfo->GetParCurrentSuccessSize());
	pDestNZBInfo->SetParFailedSize(pDestNZBInfo->GetParFailedSize() + pSrcNZBInfo->GetParFailedSize());
	pDestNZBInfo->SetParCurrentFailedSize(pDestNZBInfo->GetParCurrentFailedSize() + pSrcNZBInfo->GetParCurrentFailedSize());
	pDestNZBInfo->SetRemainingParCount(pDestNZBInfo->GetRemainingParCount() + pSrcNZBInfo->GetRemainingParCount());

	pDestNZBInfo->SetTotalArticles(pDestNZBInfo->GetTotalArticles() + pSrcNZBInfo->GetTotalArticles());
	pDestNZBInfo->SetSuccessArticles(pDestNZBInfo->GetSuccessArticles() + pSrcNZBInfo->GetSuccessArticles());
	pDestNZBInfo->SetFailedArticles(pDestNZBInfo->GetFailedArticles() + pSrcNZBInfo->GetFailedArticles());
	pDestNZBInfo->SetCurrentSuccessArticles(pDestNZBInfo->GetCurrentSuccessArticles() + pSrcNZBInfo->GetCurrentSuccessArticles());
	pDestNZBInfo->SetCurrentFailedArticles(pDestNZBInfo->GetCurrentFailedArticles() + pSrcNZBInfo->GetCurrentFailedArticles());
	pDestNZBInfo->GetServerStats()->ListOp(pSrcNZBInfo->GetServerStats(), ServerStatList::soAdd);
	pDestNZBInfo->GetCurrentServerStats()->ListOp(pSrcNZBInfo->GetCurrentServerStats(), ServerStatList::soAdd);

	pDestNZBInfo->SetMinTime(pSrcNZBInfo->GetMinTime() < pDestNZBInfo->GetMinTime() ? pSrcNZBInfo->GetMinTime() : pDestNZBInfo->GetMinTime());
	pDestNZBInfo->SetMaxTime(pSrcNZBInfo->GetMaxTime() > pDestNZBInfo->GetMaxTime() ? pSrcNZBInfo->GetMaxTime() : pDestNZBInfo->GetMaxTime());

	pDestNZBInfo->SetDownloadedSize(pDestNZBInfo->GetDownloadedSize() + pSrcNZBInfo->GetDownloadedSize());
	pDestNZBInfo->SetDownloadSec(pDestNZBInfo->GetDownloadSec() + pSrcNZBInfo->GetDownloadSec());
	pDestNZBInfo->SetDownloadStartTime((pDestNZBInfo->GetDownloadStartTime() > 0 &&
		pDestNZBInfo->GetDownloadStartTime() < pSrcNZBInfo->GetDownloadStartTime()) || pSrcNZBInfo->GetDownloadStartTime() == 0 ?
		pDestNZBInfo->GetDownloadStartTime() : pSrcNZBInfo->GetDownloadStartTime());

	// reattach completed file items to new NZBInfo-object
	for (CompletedFiles::iterator it = pSrcNZBInfo->GetCompletedFiles()->begin(); it != pSrcNZBInfo->GetCompletedFiles()->end(); it++)
    {
		CompletedFile* pCompletedFile = *it;
		pDestNZBInfo->GetCompletedFiles()->push_back(pCompletedFile);
	}
	pSrcNZBInfo->GetCompletedFiles()->clear();

	// concatenate QueuedFilenames using character '|' as separator
	int iLen = strlen(pDestNZBInfo->GetQueuedFilename()) + strlen(pSrcNZBInfo->GetQueuedFilename()) + 1;
	char* szQueuedFilename = (char*)malloc(iLen);
	snprintf(szQueuedFilename, iLen, "%s|%s", pDestNZBInfo->GetQueuedFilename(), pSrcNZBInfo->GetQueuedFilename());
	szQueuedFilename[iLen - 1] = '\0';
	pDestNZBInfo->SetQueuedFilename(szQueuedFilename);
	free(szQueuedFilename);

	pDownloadQueue->GetQueue()->Remove(pSrcNZBInfo);
	delete pSrcNZBInfo;

	return true;
}

/*
 * Creates new nzb-item out of existing files from other nzb-items.
 * If any of file-items is being downloaded the command fail.
 * For each file-item an event "eaFileDeleted" is fired.
 */
bool QueueCoordinator::SplitQueueEntries(DownloadQueue* pDownloadQueue, FileList* pFileList, const char* szName, NZBInfo** pNewNZBInfo)
{
	if (pFileList->empty())
	{
		return false;
	}

	NZBInfo* pSrcNZBInfo = NULL;

	for (FileList::iterator it = pFileList->begin(); it != pFileList->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (pFileInfo->GetActiveDownloads() > 0 || pFileInfo->GetCompletedArticles() > 0)
		{
			error("Could not split %s. File is already (partially) downloaded", pFileInfo->GetFilename());
			return false;
		}
		if (pFileInfo->GetNZBInfo()->GetPostInfo())
		{
			error("Could not split %s. File in post-process-stage", pFileInfo->GetFilename());
			return false;
		}
		if (!pSrcNZBInfo)
		{
			pSrcNZBInfo = pFileInfo->GetNZBInfo();
		}
	}

	NZBInfo* pNZBInfo = new NZBInfo();
	pDownloadQueue->GetQueue()->push_back(pNZBInfo);

	pNZBInfo->SetFilename(pSrcNZBInfo->GetFilename());
	pNZBInfo->SetName(szName);
	pNZBInfo->SetCategory(pSrcNZBInfo->GetCategory());
	pNZBInfo->SetFullContentHash(0);
	pNZBInfo->SetFilteredContentHash(0);
	pNZBInfo->SetPriority(pSrcNZBInfo->GetPriority());
	pNZBInfo->BuildDestDirName();
	pNZBInfo->SetQueuedFilename(pSrcNZBInfo->GetQueuedFilename());
	pNZBInfo->GetParameters()->CopyFrom(pSrcNZBInfo->GetParameters());

	pSrcNZBInfo->SetFullContentHash(0);
	pSrcNZBInfo->SetFilteredContentHash(0);

	for (FileList::iterator it = pFileList->begin(); it != pFileList->end(); it++)
	{
		FileInfo* pFileInfo = *it;

		DownloadQueue::Aspect aspect = { DownloadQueue::eaFileDeleted, pDownloadQueue, pFileInfo->GetNZBInfo(), pFileInfo };
		pDownloadQueue->Notify(&aspect);

		pFileInfo->SetNZBInfo(pNZBInfo);
		pNZBInfo->GetFileList()->push_back(pFileInfo);
		pSrcNZBInfo->GetFileList()->Remove(pFileInfo);

		pSrcNZBInfo->SetFileCount(pSrcNZBInfo->GetFileCount() - 1);
		pSrcNZBInfo->SetSize(pSrcNZBInfo->GetSize() - pFileInfo->GetSize());
		pSrcNZBInfo->SetRemainingSize(pSrcNZBInfo->GetRemainingSize() - pFileInfo->GetRemainingSize());
		pSrcNZBInfo->SetCurrentSuccessSize(pSrcNZBInfo->GetCurrentSuccessSize() - pFileInfo->GetSuccessSize());
		pSrcNZBInfo->SetCurrentFailedSize(pSrcNZBInfo->GetCurrentFailedSize() - pFileInfo->GetFailedSize() - pFileInfo->GetMissedSize());
		pSrcNZBInfo->SetTotalArticles(pSrcNZBInfo->GetTotalArticles() - pFileInfo->GetTotalArticles());
		pSrcNZBInfo->SetCurrentSuccessArticles(pSrcNZBInfo->GetCurrentSuccessArticles() - pFileInfo->GetSuccessArticles());
		pSrcNZBInfo->SetCurrentFailedArticles(pSrcNZBInfo->GetCurrentFailedArticles() - pFileInfo->GetFailedArticles());
		pSrcNZBInfo->GetCurrentServerStats()->ListOp(pFileInfo->GetServerStats(), ServerStatList::soSubtract);

		pNZBInfo->SetFileCount(pNZBInfo->GetFileCount() + 1);
		pNZBInfo->SetSize(pNZBInfo->GetSize() + pFileInfo->GetSize());
		pNZBInfo->SetRemainingSize(pNZBInfo->GetRemainingSize() + pFileInfo->GetRemainingSize());
		pNZBInfo->SetCurrentSuccessSize(pNZBInfo->GetCurrentSuccessSize() + pFileInfo->GetSuccessSize());
		pNZBInfo->SetCurrentFailedSize(pNZBInfo->GetCurrentFailedSize() + pFileInfo->GetFailedSize() + pFileInfo->GetMissedSize());
		pNZBInfo->SetTotalArticles(pNZBInfo->GetTotalArticles() + pFileInfo->GetTotalArticles());
		pNZBInfo->SetCurrentSuccessArticles(pNZBInfo->GetCurrentSuccessArticles() + pFileInfo->GetSuccessArticles());
		pNZBInfo->SetCurrentFailedArticles(pNZBInfo->GetCurrentFailedArticles() + pFileInfo->GetFailedArticles());
		pNZBInfo->GetCurrentServerStats()->ListOp(pFileInfo->GetServerStats(), ServerStatList::soAdd);

		if (pFileInfo->GetParFile())
		{
			pSrcNZBInfo->SetParSize(pSrcNZBInfo->GetParSize() - pFileInfo->GetSize());
			pSrcNZBInfo->SetParCurrentSuccessSize(pSrcNZBInfo->GetParCurrentSuccessSize() - pFileInfo->GetSuccessSize());
			pSrcNZBInfo->SetParCurrentFailedSize(pSrcNZBInfo->GetParCurrentFailedSize() - pFileInfo->GetFailedSize() - pFileInfo->GetMissedSize());
			pSrcNZBInfo->SetRemainingParCount(pSrcNZBInfo->GetRemainingParCount() - 1);

			pNZBInfo->SetParSize(pNZBInfo->GetParSize() + pFileInfo->GetSize());
			pNZBInfo->SetParCurrentSuccessSize(pNZBInfo->GetParCurrentSuccessSize() + pFileInfo->GetSuccessSize());
			pNZBInfo->SetParCurrentFailedSize(pNZBInfo->GetParCurrentFailedSize() + pFileInfo->GetFailedSize() + pFileInfo->GetMissedSize());
			pNZBInfo->SetRemainingParCount(pNZBInfo->GetRemainingParCount() + 1);
		}

		if (pFileInfo->GetPaused())
		{
			pSrcNZBInfo->SetPausedFileCount(pSrcNZBInfo->GetPausedFileCount() - 1);
			pSrcNZBInfo->SetPausedSize(pSrcNZBInfo->GetPausedSize() - pFileInfo->GetRemainingSize());

			pNZBInfo->SetPausedFileCount(pSrcNZBInfo->GetPausedFileCount() + 1);
			pNZBInfo->SetPausedSize(pNZBInfo->GetPausedSize() + pFileInfo->GetRemainingSize());
		}
	}

	pNZBInfo->UpdateMinMaxTime();
	if (pSrcNZBInfo->GetCompletedFiles()->empty())
	{
		pSrcNZBInfo->UpdateMinMaxTime();
	}

	if (pSrcNZBInfo->GetFileList()->empty())
	{
		pDownloadQueue->GetQueue()->Remove(pSrcNZBInfo);
		delete pSrcNZBInfo;
	}

	*pNewNZBInfo = pNZBInfo;
	return true;
}
