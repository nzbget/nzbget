/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2005 Bo Cordes Petersen <placebodk@users.sourceforge.net>
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
#include "DiskState.h"
#include "Log.h"
#include "Util.h"
#include "Decoder.h"

extern Options* g_pOptions;
extern ServerPool* g_pServerPool;
extern DiskState* g_pDiskState;

QueueCoordinator::QueueCoordinator()
{
	debug("Creating QueueCoordinator");

	m_bHasMoreJobs = true;
	ResetSpeedStat();

	m_iAllBytes = 0;
	m_tStartServer = 0;
	m_tStartDownload = 0;
	m_tPausedFrom = 0;
	m_bStandBy = true;
	m_iServerConfigGeneration = 0;

	YDecoder::Init();
}

QueueCoordinator::~QueueCoordinator()
{
	debug("Destroying QueueCoordinator");
	// Cleanup

	debug("Deleting ArticleDownloaders");
	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
	{
		delete *it;
	}
	m_ActiveDownloads.clear();

	YDecoder::Final();

	debug("QueueCoordinator destroyed");
}

void QueueCoordinator::Run()
{
	debug("Entering QueueCoordinator-loop");

	m_mutexDownloadQueue.Lock();

	if (g_pOptions->GetServerMode())
	{
		g_pDiskState->LoadStats(g_pServerPool->GetServers());
		// currently there are no any stats but we need to save current server list into diskstate
		g_pDiskState->SaveStats(g_pServerPool->GetServers());
	}

	if (g_pOptions->GetServerMode() && g_pOptions->GetSaveQueue() && g_pDiskState->DownloadQueueExists())
	{
		if (g_pOptions->GetReloadQueue())
		{
			g_pDiskState->LoadDownloadQueue(&m_DownloadQueue);
		}
		else
		{
			g_pDiskState->DiscardDownloadQueue();
		}
	}

	g_pDiskState->CleanupTempDir(&m_DownloadQueue);

	m_mutexDownloadQueue.Unlock();

	AdjustDownloadsLimit();
	m_tStartServer = time(NULL);
	m_tLastCheck = m_tStartServer;
	bool bWasStandBy = true;
	bool bArticeDownloadsRunning = false;
	int iResetCounter = 0;

	while (!IsStopped())
	{
		bool bDownloadsChecked = false;
		if (!(g_pOptions->GetPauseDownload() || g_pOptions->GetPauseDownload2()))
		{
			NNTPConnection* pConnection = g_pServerPool->GetConnection(0, NULL, NULL);
			if (pConnection)
			{
				// start download for next article
				FileInfo* pFileInfo;
				ArticleInfo* pArticleInfo;
				bool bFreeConnection = false;
				
				m_mutexDownloadQueue.Lock();
				bool bHasMoreArticles = GetNextArticle(pFileInfo, pArticleInfo);
				bArticeDownloadsRunning = !m_ActiveDownloads.empty();
				bDownloadsChecked = true;
				m_bHasMoreJobs = bHasMoreArticles || bArticeDownloadsRunning;
				if (bHasMoreArticles && !IsStopped() && (int)m_ActiveDownloads.size() < m_iDownloadsLimit)
				{
					StartArticleDownload(pFileInfo, pArticleInfo, pConnection);
					bArticeDownloadsRunning = true;
				}
				else
				{
					bFreeConnection = true;
				}
				m_mutexDownloadQueue.Unlock();
				
				if (bFreeConnection)
				{
					g_pServerPool->FreeConnection(pConnection, false);
				}
			}
		}

		if (!bDownloadsChecked)
		{
			m_mutexDownloadQueue.Lock();
			bArticeDownloadsRunning = !m_ActiveDownloads.empty();
			m_mutexDownloadQueue.Unlock();
		}

		bool bStandBy = !bArticeDownloadsRunning;
		if (bStandBy ^ bWasStandBy)
		{
			EnterLeaveStandBy(bStandBy);
			bWasStandBy = bStandBy;
		}

		// sleep longer in StandBy
		int iSleepInterval = bStandBy ? 100 : 5;
		usleep(iSleepInterval * 1000);

		if (!bStandBy)
		{
			AddSpeedReading(0);
		}

		iResetCounter += iSleepInterval;
		if (iResetCounter >= 1000)
		{
			// this code should not be called too often, once per second is OK
			g_pServerPool->CloseUnusedConnections();
			ResetHangingDownloads();
			iResetCounter = 0;
			AdjustStartTime();
			AdjustDownloadsLimit();
		}
	}

	// waiting for downloads
	debug("QueueCoordinator: waiting for Downloads to complete");
	bool completed = false;
	while (!completed)
	{
		m_mutexDownloadQueue.Lock();
		completed = m_ActiveDownloads.size() == 0;
		m_mutexDownloadQueue.Unlock();
		usleep(100 * 1000);
		ResetHangingDownloads();
	}
	debug("QueueCoordinator: Downloads are completed");

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

void QueueCoordinator::AddNZBFileToQueue(NZBFile* pNZBFile, bool bAddFirst)
{
	debug("Adding NZBFile to queue");

	NZBInfo* pNZBInfo = pNZBFile->GetNZBInfo();

	m_mutexDownloadQueue.Lock();

	Aspect foundAspect = { eaNZBFileFound, &m_DownloadQueue, pNZBInfo, NULL };
	Notify(&foundAspect);

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
				g_pDiskState->DiscardFile(pFileInfo);
			}
		}
		pNZBInfo->SetDeletePaused(bAllPaused);
	}

	if (eDeleteStatus == NZBInfo::dsManual)
	{
		m_mutexDownloadQueue.Unlock(); // UNLOCK
		return;
	}

	// at this point NZBInfo will be added either to queue or to history as duplicate
	// and therefore can be detached from NZBFile.
	pNZBFile->DetachNZBInfo();

	if (eDeleteStatus == NZBInfo::dsNone)
	{
		if (g_pOptions->GetDupeCheck() && pNZBInfo->GetDupeMode() != dmForce)
		{
			CheckDupeFileInfos(pNZBInfo);
		}
		if (bAddFirst)
		{
			m_DownloadQueue.GetQueue()->push_front(pNZBInfo);
		}
		else
		{
			m_DownloadQueue.GetQueue()->push_back(pNZBInfo);
		}
	}

	char szNZBName[1024];
	strncpy(szNZBName, pNZBInfo->GetName(), sizeof(szNZBName)-1);

	Aspect aspect = { eaNZBFileAdded, &m_DownloadQueue, pNZBInfo, NULL };
	Notify(&aspect);
	
	if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
	{
		g_pDiskState->SaveDownloadQueue(&m_DownloadQueue);
	}

	m_mutexDownloadQueue.Unlock();

	if (eDeleteStatus == NZBInfo::dsNone)
	{
		info("Collection %s added to queue", szNZBName);
	}
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
			g_pDiskState->DiscardFile(pFileInfo);
		}
	}
}

/*
 * NOTE: see note to "AddSpeedReading"
 */
int QueueCoordinator::CalcCurrentDownloadSpeed()
{
	if (m_bStandBy)
	{
		return 0;
	}

	int iTimeDiff = (int)time(NULL) - m_iSpeedStartTime * SPEEDMETER_SLOTSIZE;
	if (iTimeDiff == 0)
	{
		return 0;
	}

	return m_iSpeedTotalBytes / iTimeDiff;
}

void QueueCoordinator::AddSpeedReading(int iBytes)
{
	time_t tCurTime = time(NULL);
	int iNowSlot = (int)tCurTime / SPEEDMETER_SLOTSIZE;

	if (g_pOptions->GetAccurateRate())
	{
#ifdef HAVE_SPINLOCK
		m_spinlockSpeed.Lock();
#else
		m_mutexSpeed.Lock();
#endif
	}

	while (iNowSlot > m_iSpeedTime[m_iSpeedBytesIndex])
	{
		//record bytes in next slot
		m_iSpeedBytesIndex++;
		if (m_iSpeedBytesIndex >= SPEEDMETER_SLOTS)
		{
			m_iSpeedBytesIndex = 0;
		}
		//Adjust counters with outgoing information.
		m_iSpeedTotalBytes -= m_iSpeedBytes[m_iSpeedBytesIndex];

		//Note we should really use the start time of the next slot
		//but its easier to just use the outgoing slot time. This
		//will result in a small error.
		m_iSpeedStartTime = m_iSpeedTime[m_iSpeedBytesIndex];

		//Now reset.
		m_iSpeedBytes[m_iSpeedBytesIndex] = 0;
		m_iSpeedTime[m_iSpeedBytesIndex] = iNowSlot;
	}

	// Once per second recalculate summary field "m_iSpeedTotalBytes" to recover from possible synchronisation errors
	if (tCurTime > m_tSpeedCorrection)
	{
		int iSpeedTotalBytes = 0;
		for (int i = 0; i < SPEEDMETER_SLOTS; i++)
		{
			iSpeedTotalBytes += m_iSpeedBytes[i];
		}
		m_iSpeedTotalBytes = iSpeedTotalBytes;
		m_tSpeedCorrection = tCurTime;
	}

	if (m_iSpeedTotalBytes == 0)
	{
		m_iSpeedStartTime = iNowSlot;
	}
	m_iSpeedBytes[m_iSpeedBytesIndex] += iBytes;
	m_iSpeedTotalBytes += iBytes;
	m_iAllBytes += iBytes;

	if (g_pOptions->GetAccurateRate())
	{
#ifdef HAVE_SPINLOCK
		m_spinlockSpeed.Unlock();
#else
		m_mutexSpeed.Unlock();
#endif
	}
}

void QueueCoordinator::ResetSpeedStat()
{
	time_t tCurTime = time(NULL);
	m_iSpeedStartTime = (int)tCurTime / SPEEDMETER_SLOTSIZE;
	for (int i = 0; i < SPEEDMETER_SLOTS; i++)
	{
		m_iSpeedBytes[i] = 0;
		m_iSpeedTime[i] = m_iSpeedStartTime;
	}
	m_iSpeedBytesIndex = 0;
	m_iSpeedTotalBytes = 0;
	m_tSpeedCorrection = tCurTime;
}

long long QueueCoordinator::CalcRemainingSize()
{
	long long lRemainingSize = 0;

	m_mutexDownloadQueue.Lock();
	for (NZBList::iterator it = m_DownloadQueue.GetQueue()->begin(); it != m_DownloadQueue.GetQueue()->end(); it++)
	{
		NZBInfo* pNZBInfo = *it;
		for (FileList::iterator it2 = pNZBInfo->GetFileList()->begin(); it2 != pNZBInfo->GetFileList()->end(); it2++)
		{
			FileInfo* pFileInfo = *it2;
			if (!pFileInfo->GetPaused() && !pFileInfo->GetDeleted())
			{
				lRemainingSize += pFileInfo->GetRemainingSize();
			}
		}
	}
	m_mutexDownloadQueue.Unlock();

	return lRemainingSize;
}

/*
 * NOTE: DownloadQueue must be locked prior to call of this function
 * Returns True if Entry was deleted from Queue or False if it was scheduled for Deletion.
 * NOTE: "False" does not mean unsuccess; the entry is (or will be) deleted in any case.
 */
bool QueueCoordinator::DeleteQueueEntry(FileInfo* pFileInfo)
{
	pFileInfo->SetDeleted(true);
	bool hasDownloads = false;
	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
	{
		ArticleDownloader* pArticleDownloader = *it;
		if (pArticleDownloader->GetFileInfo() == pFileInfo)
		{
			hasDownloads = true;
			pArticleDownloader->Stop();
		}
	}
	if (!hasDownloads)
	{
		DeleteFileInfo(pFileInfo, false);
	}
	return hasDownloads;
}

void QueueCoordinator::Stop()
{
	Thread::Stop();

	debug("Stopping ArticleDownloads");
	m_mutexDownloadQueue.Lock();
	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
	{
		(*it)->Stop();
	}
	m_mutexDownloadQueue.Unlock();
	debug("ArticleDownloads are notified");
}

/*
 * Returns next article for download.
 */
bool QueueCoordinator::GetNextArticle(FileInfo* &pFileInfo, ArticleInfo* &pArticleInfo)
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

	while (!bOK) 
	{
		//debug("QueueCoordinator::GetNextArticle() - in loop");

		pFileInfo = NULL;
		int iNum = 0;
		int iFileNum = 0;

		for (NZBList::iterator it = m_DownloadQueue.GetQueue()->begin(); it != m_DownloadQueue.GetQueue()->end(); it++)
		{
			NZBInfo* pNZBInfo = *it;
			for (FileList::iterator it2 = pNZBInfo->GetFileList()->begin(); it2 != pNZBInfo->GetFileList()->end(); it2++)
			{
				FileInfo* pFileInfo1 = *it2;
				if ((!pCheckedFiles || !pCheckedFiles[iNum]) && 
					!pFileInfo1->GetPaused() && !pFileInfo1->GetDeleted() &&
					(!pFileInfo ||
					 (pFileInfo1->GetExtraPriority() == pFileInfo->GetExtraPriority() &&
					  pFileInfo1->GetPriority() > pFileInfo->GetPriority()) ||
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
			if (pArticleInfo->GetStatus() == 0)
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
				for (NZBList::iterator it = m_DownloadQueue.GetQueue()->begin(); it != m_DownloadQueue.GetQueue()->end(); it++)
				{
					NZBInfo* pNZBInfo = *it;
					iTotalFileCount += pNZBInfo->GetFileList()->size();
				}

				int iArrSize = sizeof(bool) * iTotalFileCount;
				pCheckedFiles = (bool*)malloc(iArrSize);
				memset(pCheckedFiles, false, iArrSize);
			}
			pCheckedFiles[iFileNum] = true;
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

DownloadQueue* QueueCoordinator::LockQueue()
{
	m_mutexDownloadQueue.Lock();
	return &m_DownloadQueue;
}

void QueueCoordinator::UnlockQueue()
{
	m_mutexDownloadQueue.Unlock();
}

void QueueCoordinator::Update(Subject* Caller, void* Aspect)
{
	debug("Notification from ArticleDownloader received");

	ArticleDownloader* pArticleDownloader = (ArticleDownloader*) Caller;
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

	m_mutexDownloadQueue.Lock();

	if (pArticleDownloader->GetStatus() == ArticleDownloader::adFinished)
	{
		pArticleInfo->SetStatus(ArticleInfo::aiFinished);
		pFileInfo->SetSuccessSize(pFileInfo->GetSuccessSize() + pArticleInfo->GetSize());
		pNZBInfo->SetCurrentSuccessSize(pNZBInfo->GetCurrentSuccessSize() + pArticleInfo->GetSize());
		pNZBInfo->SetParCurrentSuccessSize(pNZBInfo->GetParCurrentSuccessSize() + (pFileInfo->GetParFile() ? pArticleInfo->GetSize() : 0));
		pFileInfo->SetSuccessArticles(pFileInfo->GetSuccessArticles() + 1);
	}
	else if (pArticleDownloader->GetStatus() == ArticleDownloader::adFailed)
	{
		pArticleInfo->SetStatus(ArticleInfo::aiFailed);
		pFileInfo->SetFailedSize(pFileInfo->GetFailedSize() + pArticleInfo->GetSize());
		pNZBInfo->SetCurrentFailedSize(pNZBInfo->GetCurrentFailedSize() + pArticleInfo->GetSize());
		pNZBInfo->SetParCurrentFailedSize(pNZBInfo->GetParCurrentFailedSize() + (pFileInfo->GetParFile() ? pArticleInfo->GetSize() : 0));
		pFileInfo->SetFailedArticles(pFileInfo->GetFailedArticles() + 1);
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
		pNZBInfo->GetServerStats()->Add(pArticleDownloader->GetServerStats());
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
			DeleteQueueEntry(pFileInfo);
		}
	}

	bool deleteFileObj = false;

	if (fileCompleted && !IsStopped() && !pFileInfo->GetDeleted())
	{
		// all jobs done
		m_mutexDownloadQueue.Unlock();
		pArticleDownloader->CompleteFileParts();
		m_mutexDownloadQueue.Lock();
		deleteFileObj = true;
	}

	CheckHealth(pFileInfo);

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
		DeleteFileInfo(pFileInfo, fileCompleted);
		if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
		{
			g_pDiskState->SaveDownloadQueue(&m_DownloadQueue);
		}
	}

	m_mutexDownloadQueue.Unlock();
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

	pNZBInfo->CalcFileStats();
}

void QueueCoordinator::DeleteFileInfo(FileInfo* pFileInfo, bool bCompleted)
{
	bool fileDeleted = pFileInfo->GetDeleted();
	pFileInfo->SetDeleted(true);

	StatFileInfo(pFileInfo, bCompleted);

	if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
	{
		g_pDiskState->DiscardFile(pFileInfo);
	}

	if (!bCompleted)
	{
		DiscardDiskFile(pFileInfo);
	}

	NZBInfo* pNZBInfo = pFileInfo->GetNZBInfo();

	Aspect aspect = { bCompleted && !fileDeleted ? eaFileCompleted : eaFileDeleted, &m_DownloadQueue, pNZBInfo, pFileInfo };
	Notify(&aspect);

	// nzb-file could be deleted from queue in "Notify", check if it is still in queue.
	if (std::find(m_DownloadQueue.GetQueue()->begin(), m_DownloadQueue.GetQueue()->end(), pNZBInfo) !=
		m_DownloadQueue.GetQueue()->end())
	{
		pNZBInfo->GetFileList()->Remove(pFileInfo);
		delete pFileInfo;
	}
}

void QueueCoordinator::DiscardDiskFile(FileInfo* pFileInfo)
{
	// deleting temporary files

	if (!g_pOptions->GetDirectWrite() || g_pOptions->GetContinuePartial())
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

void QueueCoordinator::CheckHealth(FileInfo* pFileInfo)
{
	if (g_pOptions->GetHealthCheck() == Options::hcNone ||
		pFileInfo->GetNZBInfo()->GetHealthPaused() ||
		pFileInfo->GetNZBInfo()->GetDeleteStatus() == NZBInfo::dsHealth ||
		pFileInfo->GetNZBInfo()->CalcHealth() >= pFileInfo->GetNZBInfo()->CalcCriticalHealth())
	{
		return;
	}

	if (g_pOptions->GetHealthCheck() == Options::hcPause)
	{
		warn("Pausing %s due to health %.1f%% below critical %.1f%%", pFileInfo->GetNZBInfo()->GetName(),
			pFileInfo->GetNZBInfo()->CalcHealth() / 10.0, pFileInfo->GetNZBInfo()->CalcCriticalHealth() / 10.0);
		pFileInfo->GetNZBInfo()->SetHealthPaused(true);
		m_QueueEditor.LockedEditEntry(&m_DownloadQueue, pFileInfo->GetID(), QueueEditor::eaGroupPause, 0, NULL);
	}
	else if (g_pOptions->GetHealthCheck() == Options::hcDelete)
	{
		warn("Cancelling download and deleting %s due to health %.1f%% below critical %.1f%%", pFileInfo->GetNZBInfo()->GetName(),
			pFileInfo->GetNZBInfo()->CalcHealth() / 10.0, pFileInfo->GetNZBInfo()->CalcCriticalHealth() / 10.0);
		pFileInfo->GetNZBInfo()->SetDeleteStatus(NZBInfo::dsHealth);
		m_QueueEditor.LockedEditEntry(&m_DownloadQueue, pFileInfo->GetID(), QueueEditor::eaGroupDelete, 0, NULL);
	}
}

void QueueCoordinator::LogDebugInfo()
{
	debug("--------------------------------------------");
	debug("Dumping debug debug to log");
	debug("--------------------------------------------");

	debug("   SpeedMeter");
	debug("   ----------");
	float fSpeed = (float)(CalcCurrentDownloadSpeed() / 1024.0);
	int iTimeDiff = (int)time(NULL) - m_iSpeedStartTime * SPEEDMETER_SLOTSIZE;
	debug("      Speed: %f", fSpeed);
	debug("      SpeedStartTime: %i", m_iSpeedStartTime);
	debug("      SpeedTotalBytes: %i", m_iSpeedTotalBytes);
	debug("      SpeedBytesIndex: %i", m_iSpeedBytesIndex);
	debug("      AllBytes: %i", m_iAllBytes);
	debug("      Time: %i", (int)time(NULL));
	debug("      TimeDiff: %i", iTimeDiff);
	for (int i=0; i < SPEEDMETER_SLOTS; i++)
	{
		debug("      Bytes[%i]: %i, Time[%i]: %i", i, m_iSpeedBytes[i], i, m_iSpeedTime[i]);
	}

	debug("   QueueCoordinator");
	debug("   ----------------");

	m_mutexDownloadQueue.Lock();
	debug("    Active Downloads: %i", m_ActiveDownloads.size());
	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
	{
		ArticleDownloader* pArticleDownloader = *it;
		pArticleDownloader->LogDebugInfo();
	}
	m_mutexDownloadQueue.Unlock();

	debug("");

	g_pServerPool->LogDebugInfo();
}

void QueueCoordinator::ResetHangingDownloads()
{
	if (g_pOptions->GetTerminateTimeout() == 0 && g_pOptions->GetConnectionTimeout() == 0)
	{
		return;
	}

	m_mutexDownloadQueue.Lock();
	time_t tm = ::time(NULL);

	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end();)
	{
		ArticleDownloader* pArticleDownloader = *it;

		if (tm - pArticleDownloader->GetLastUpdateTime() > g_pOptions->GetConnectionTimeout() + 1 &&
		   pArticleDownloader->GetStatus() == ArticleDownloader::adRunning)
		{
			error("Cancelling hanging download %s", pArticleDownloader->GetInfoName());
			pArticleDownloader->Stop();
		}
		
		if (tm - pArticleDownloader->GetLastUpdateTime() > g_pOptions->GetTerminateTimeout() &&
		   pArticleDownloader->GetStatus() == ArticleDownloader::adRunning)
		{
			ArticleInfo* pArticleInfo = pArticleDownloader->GetArticleInfo();
			debug("Terminating hanging download %s", pArticleDownloader->GetInfoName());
			if (pArticleDownloader->Terminate())
			{
				error("Terminated hanging download %s", pArticleDownloader->GetInfoName());
				pArticleInfo->SetStatus(ArticleInfo::aiUndefined);
			}
			else
			{
				error("Could not terminate hanging download %s", Util::BaseFileName(pArticleInfo->GetResultFilename()));
			}
			m_ActiveDownloads.erase(it);
			pArticleDownloader->GetFileInfo()->SetActiveDownloads(pArticleDownloader->GetFileInfo()->GetActiveDownloads() - 1);
			pArticleDownloader->GetFileInfo()->GetNZBInfo()->SetActiveDownloads(pArticleDownloader->GetFileInfo()->GetNZBInfo()->GetActiveDownloads() - 1);
			// it's not safe to destroy pArticleDownloader, because the state of object is unknown
			delete pArticleDownloader;
			it = m_ActiveDownloads.begin();
			continue;
		}
		it++;
	}                                              

	m_mutexDownloadQueue.Unlock();
}

void QueueCoordinator::EnterLeaveStandBy(bool bEnter)
{
	m_mutexStat.Lock();
	m_bStandBy = bEnter;
	if (bEnter)
	{
		m_tPausedFrom = time(NULL);
	}
	else
	{
		if (m_tStartDownload == 0)
		{
			m_tStartDownload = time(NULL);
		}
		else
		{
			m_tStartDownload += time(NULL) - m_tPausedFrom;
		}
		m_tPausedFrom = 0;
		ResetSpeedStat();
	}
	m_mutexStat.Unlock();
}

void QueueCoordinator::CalcStat(int* iUpTimeSec, int* iDnTimeSec, long long* iAllBytes, bool* bStandBy)
{
	m_mutexStat.Lock();
	if (m_tStartServer > 0)
	{
		*iUpTimeSec = (int)(time(NULL) - m_tStartServer);
	}
	else
	{
		*iUpTimeSec = 0;
	}
	*bStandBy = m_bStandBy;
	if (m_bStandBy)
	{
		*iDnTimeSec = (int)(m_tPausedFrom - m_tStartDownload);
	}
	else
	{
		*iDnTimeSec = (int)(time(NULL) - m_tStartDownload);
	}
	*iAllBytes = m_iAllBytes;
	m_mutexStat.Unlock();
}

/*
 * Detects large step changes of system time and adjust statistics.
 */
void QueueCoordinator::AdjustStartTime()
{
	time_t m_tCurTime = time(NULL);
	time_t tDiff = m_tCurTime - m_tLastCheck;
	if (tDiff > 60 || tDiff < 0)
	{
		m_tStartServer += tDiff + 1; // "1" because the method is called once per second
		if (m_tStartDownload != 0 && !m_bStandBy)
		{
			m_tStartDownload += tDiff + 1;
		}
	}		
	m_tLastCheck = m_tCurTime;
}

bool QueueCoordinator::SetQueueEntryNZBCategory(NZBInfo* pNZBInfo, const char* szCategory)
{
	if (pNZBInfo->GetPostProcess())
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
	bool bOK = bDirUnchanged || ArticleDownloader::MoveCompletedFiles(pNZBInfo, szOldDestDir);

	return bOK;
}

bool QueueCoordinator::SetQueueEntryNZBName(NZBInfo* pNZBInfo, const char* szName)
{
	if (pNZBInfo->GetPostProcess())
	{
		error("Could not rename %s. File in post-process-stage", pNZBInfo->GetName());
		return false;
	}

	if (strlen(szName) == 0)
	{
		error("Could not rename %s. The new name cannot be empty", pNZBInfo->GetName());
		return false;
	}

	char szOldDestDir[1024];
	strncpy(szOldDestDir, pNZBInfo->GetDestDir(), 1024);
	szOldDestDir[1024-1] = '\0';

	char szNZBNicename[1024];
	NZBInfo::MakeNiceNZBName(szName, szNZBNicename, sizeof(szNZBNicename), false);
	pNZBInfo->SetName(szNZBNicename);

	pNZBInfo->BuildDestDirName();

	bool bDirUnchanged = !strcmp(pNZBInfo->GetDestDir(), szOldDestDir);
	bool bOK = bDirUnchanged || ArticleDownloader::MoveCompletedFiles(pNZBInfo, szOldDestDir);

	return bOK;
}

/*
 * NOTE: DownloadQueue must be locked prior to call of this function
 */
bool QueueCoordinator::MergeQueueEntries(NZBInfo* pDestNZBInfo, NZBInfo* pSrcNZBInfo)
{
	if (pDestNZBInfo->GetPostProcess() || pSrcNZBInfo->GetPostProcess())
	{
		error("Could not merge %s and %s. File in post-process-stage", pDestNZBInfo->GetName(), pSrcNZBInfo->GetName());
		return false;
	}

	// set new dest directory, new category and move downloaded files to new dest directory
	pSrcNZBInfo->SetFilename(pSrcNZBInfo->GetFilename());
	SetQueueEntryNZBCategory(pSrcNZBInfo, pDestNZBInfo->GetCategory());

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

	pDestNZBInfo->CalcFileStats();

	// reattach completed file items to new NZBInfo-object
	for (NZBInfo::Files::iterator it = pSrcNZBInfo->GetCompletedFiles()->begin(); it != pSrcNZBInfo->GetCompletedFiles()->end(); it++)
    {
		char* szFileName = *it;
		pDestNZBInfo->GetCompletedFiles()->push_back(szFileName);
	}
	pSrcNZBInfo->GetCompletedFiles()->clear();

	// concatenate QueuedFilenames using character '|' as separator
	int iLen = strlen(pDestNZBInfo->GetQueuedFilename()) + strlen(pSrcNZBInfo->GetQueuedFilename()) + 1;
	char* szQueuedFilename = (char*)malloc(iLen);
	snprintf(szQueuedFilename, iLen, "%s|%s", pDestNZBInfo->GetQueuedFilename(), pSrcNZBInfo->GetQueuedFilename());
	szQueuedFilename[iLen - 1] = '\0';
	pDestNZBInfo->SetQueuedFilename(szQueuedFilename);
	free(szQueuedFilename);

	m_DownloadQueue.GetQueue()->Remove(pSrcNZBInfo);
	delete pSrcNZBInfo;

	return true;
}

/*
 * Creates new nzb-item out of existing files from other nzb-items.
 * If any of file-items is being downloaded the command fail.
 * For each file-item an event "eaFileDeleted" is fired.
 *
 * NOTE: DownloadQueue must be locked prior to call of this function
 */
bool QueueCoordinator::SplitQueueEntries(FileList* pFileList, const char* szName, NZBInfo** pNewNZBInfo)
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
		if (pFileInfo->GetNZBInfo()->GetPostProcess())
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
	m_DownloadQueue.GetQueue()->push_back(pNZBInfo);

	pNZBInfo->SetFilename(pSrcNZBInfo->GetFilename());
	pNZBInfo->SetName(szName);
	pNZBInfo->SetCategory(pSrcNZBInfo->GetCategory());
	pNZBInfo->SetFullContentHash(0);
	pNZBInfo->SetFilteredContentHash(0);
	pNZBInfo->BuildDestDirName();
	pNZBInfo->SetQueuedFilename(pSrcNZBInfo->GetQueuedFilename());
	pNZBInfo->GetParameters()->CopyFrom(pSrcNZBInfo->GetParameters());

	pSrcNZBInfo->SetFullContentHash(0);
	pSrcNZBInfo->SetFilteredContentHash(0);

	for (FileList::iterator it = pFileList->begin(); it != pFileList->end(); it++)
	{
		FileInfo* pFileInfo = *it;

		Aspect aspect = { eaFileDeleted, &m_DownloadQueue, pFileInfo->GetNZBInfo(), pFileInfo };
		Notify(&aspect);

		pFileInfo->SetNZBInfo(pNZBInfo);
		pNZBInfo->GetFileList()->push_back(pFileInfo);
		pSrcNZBInfo->GetFileList()->Remove(pFileInfo);

		pSrcNZBInfo->SetFileCount(pSrcNZBInfo->GetFileCount() - 1);
		pSrcNZBInfo->SetSize(pSrcNZBInfo->GetSize() - pFileInfo->GetSize());
		pSrcNZBInfo->SetRemainingSize(pSrcNZBInfo->GetRemainingSize() - pFileInfo->GetRemainingSize());

		pSrcNZBInfo->SetSuccessSize(pSrcNZBInfo->GetSuccessSize() - pFileInfo->GetSuccessSize());
		pSrcNZBInfo->SetCurrentSuccessSize(pSrcNZBInfo->GetCurrentSuccessSize() - pFileInfo->GetSuccessSize());
		pSrcNZBInfo->SetFailedSize(pSrcNZBInfo->GetFailedSize() - pFileInfo->GetFailedSize() - pFileInfo->GetMissedSize());
		pSrcNZBInfo->SetCurrentFailedSize(pSrcNZBInfo->GetCurrentFailedSize() - pFileInfo->GetFailedSize() - pFileInfo->GetMissedSize());

		pNZBInfo->SetFileCount(pNZBInfo->GetFileCount() + 1);
		pNZBInfo->SetSize(pNZBInfo->GetSize() + pFileInfo->GetSize());
		pNZBInfo->SetRemainingSize(pNZBInfo->GetRemainingSize() + pFileInfo->GetRemainingSize());
		pNZBInfo->SetSuccessSize(pNZBInfo->GetSuccessSize() + pFileInfo->GetSuccessSize());
		pNZBInfo->SetCurrentSuccessSize(pNZBInfo->GetCurrentSuccessSize() + pFileInfo->GetSuccessSize());
		pNZBInfo->SetFailedSize(pNZBInfo->GetFailedSize() + pFileInfo->GetFailedSize() + pFileInfo->GetMissedSize());
		pNZBInfo->SetCurrentFailedSize(pNZBInfo->GetCurrentFailedSize() + pFileInfo->GetFailedSize() + pFileInfo->GetMissedSize());

		if (pFileInfo->GetParFile())
		{
			pSrcNZBInfo->SetParSize(pSrcNZBInfo->GetParSize() - pFileInfo->GetSize());
			pSrcNZBInfo->SetParSuccessSize(pSrcNZBInfo->GetParSuccessSize() - pFileInfo->GetSuccessSize());
			pSrcNZBInfo->SetParCurrentSuccessSize(pSrcNZBInfo->GetParCurrentSuccessSize() - pFileInfo->GetSuccessSize());
			pSrcNZBInfo->SetParFailedSize(pSrcNZBInfo->GetParFailedSize() - pFileInfo->GetFailedSize() - pFileInfo->GetMissedSize());
			pSrcNZBInfo->SetParCurrentFailedSize(pSrcNZBInfo->GetParCurrentFailedSize() - pFileInfo->GetFailedSize() - pFileInfo->GetMissedSize());
			pSrcNZBInfo->SetRemainingParCount(pSrcNZBInfo->GetRemainingParCount() - 1);

			pNZBInfo->SetParSize(pNZBInfo->GetParSize() + pFileInfo->GetSize());
			pNZBInfo->SetParSuccessSize(pNZBInfo->GetParSuccessSize() + pFileInfo->GetSuccessSize());
			pNZBInfo->SetParCurrentSuccessSize(pNZBInfo->GetParCurrentSuccessSize() + pFileInfo->GetSuccessSize());
			pNZBInfo->SetParFailedSize(pNZBInfo->GetParFailedSize() + pFileInfo->GetFailedSize() + pFileInfo->GetMissedSize());
			pNZBInfo->SetParCurrentFailedSize(pNZBInfo->GetParCurrentFailedSize() + pFileInfo->GetFailedSize() + pFileInfo->GetMissedSize());
			pNZBInfo->SetRemainingParCount(pNZBInfo->GetRemainingParCount() + 1);
		}

		if (pFileInfo->GetPaused())
		{
			pSrcNZBInfo->SetPausedFileCount(pSrcNZBInfo->GetPausedFileCount() - 1);
			pSrcNZBInfo->SetPausedSize(pSrcNZBInfo->GetPausedSize() - pFileInfo->GetRemainingSize());

			pNZBInfo->SetPausedFileCount(pSrcNZBInfo->GetPausedFileCount() + 1);
			pNZBInfo->SetPausedSize(pSrcNZBInfo->GetPausedSize() + pFileInfo->GetRemainingSize());
		}
	}

	pNZBInfo->CalcFileStats();
	pSrcNZBInfo->CalcFileStats();

	if (pSrcNZBInfo->GetFileList()->empty())
	{
		m_DownloadQueue.GetQueue()->Remove(pSrcNZBInfo);
		delete pSrcNZBInfo;
	}

	*pNewNZBInfo = pNZBInfo;
	return true;
}
