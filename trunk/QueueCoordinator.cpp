/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2005  Bo Cordes Petersen <placebodk@users.sourceforge.net>
 *  Copyright (C) 2007  Andrei Prygounkov <hugbug@users.sourceforge.net>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Revision$
 * $Date$
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <unistd.h>
#include <sys/time.h>
#endif

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
	m_DownloadQueue.clear();
	m_ActiveDownloads.clear();

	YDecoder::Init();
}

QueueCoordinator::~QueueCoordinator()
{
	debug("Destroying QueueCoordinator");
	// Cleanup

	debug("Deleting DownloadQueue");
	for (DownloadQueue::iterator it = m_DownloadQueue.begin(); it != m_DownloadQueue.end(); it++)
	{
		delete *it;
	}
	m_DownloadQueue.clear();

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

	if (g_pOptions->GetServerMode() && g_pOptions->GetSaveQueue() && g_pDiskState->Exists())
	{
		if (g_pOptions->GetReloadQueue())
		{
			g_pDiskState->Load(&m_DownloadQueue);
		}
		else
		{
			g_pDiskState->Discard();
		}
	}

	g_pDiskState->CleanupTempDir(&m_DownloadQueue);

	m_mutexDownloadQueue.Unlock();

	while (!IsStopped())
	{
		while (g_pOptions->GetPause() && !IsStopped())
		{
			// Sleep for a while
			usleep(500 * 1000);
		}

		if (g_pServerPool->HasFreeConnection() && Thread::GetThreadCount() < g_pOptions->GetThreadLimit())
		{
			// start download for next article
			FileInfo* pFileInfo;
			ArticleInfo* pArticleInfo;

			m_mutexDownloadQueue.Lock();
			bool bHasMoreArticles = GetNextArticle(pFileInfo, pArticleInfo);
			m_bHasMoreJobs = bHasMoreArticles || !m_ActiveDownloads.empty();
			if (bHasMoreArticles && !IsStopped())
			{
				StartArticleDownload(pFileInfo, pArticleInfo);
			}
			m_mutexDownloadQueue.Unlock();

			if (!IsStopped())
			{
				// two possibilities:
				// 1) hasMoreArticles==false: there are no jobs, waiting for a while
				// 2) hasMoreArticles==true: the pause prevents starting of many threads, before the download-thread locks the connection
				usleep(100 * 1000);
			}
		}
		else
		{
			// there are no free connection available, waiting for a while
			usleep(100 * 1000);
		}

		ResetHangingDownloads();
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

void QueueCoordinator::AddNZBFileToQueue(NZBFile* pNZBFile, bool bAddFirst)
{
	debug("Adding NZBFile to queue");

	m_mutexDownloadQueue.Lock();

	DownloadQueue tmpDownloadQueue;

	for (NZBFile::FileInfos::iterator it = pNZBFile->GetFileInfos()->begin(); it != pNZBFile->GetFileInfos()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (g_pOptions->GetDupeCheck() && IsDupe(pFileInfo))
		{
			warn("File \"%s\" seems to be duplicate, skipping", pFileInfo->GetFilename());
		}
		else
		{
			if (bAddFirst)
			{
				tmpDownloadQueue.push_front(pFileInfo);
			}
			else
			{
				m_DownloadQueue.push_back(pFileInfo);
			}
		}
	}

	if (bAddFirst)
	{
		for (DownloadQueue::iterator it = tmpDownloadQueue.begin(); it != tmpDownloadQueue.end(); it++)
		{
			m_DownloadQueue.push_front(*it);
		}
	}

	pNZBFile->DetachFileInfos();

	Aspect aspect = { eaNZBFileAdded, NULL, &m_DownloadQueue, pNZBFile->GetFileName() };
	Notify(&aspect);
	
	if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
	{
		g_pDiskState->Save(&m_DownloadQueue);
	}

	m_mutexDownloadQueue.Unlock();
}

bool QueueCoordinator::AddFileToQueue(const char* szFileName)
{
	// Parse the buffer and make it into a NZBFile
	NZBFile* pNZBFile = NZBFile::CreateFromFile(szFileName);

	// Did file parse correctly?
	if (!pNZBFile)
	{
		return false;
	}

	// Add NZBFile to Queue
	AddNZBFileToQueue(pNZBFile, false);

	delete pNZBFile;

	return true;
}

float QueueCoordinator::CalcCurrentDownloadSpeed()
{
	float fSpeedAllDownloads = 0;

	m_mutexDownloadQueue.Lock();

	struct _timeval curtime; 
	gettimeofday(&curtime, 0);

	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
	{
		ArticleDownloader* pArticleDownloader = *it;

		float fSpeed = 0.0f;
		struct _timeval* arttime = pArticleDownloader->GetStartTime();

#ifdef WIN32
		if (arttime->time != 0)
#else
		if (arttime->tv_sec != 0)
#endif
		{
#ifdef WIN32
			float tdiff = (float)((curtime.time - arttime->time) + (curtime.millitm - arttime->millitm) / 1000.0);
#else
			float tdiff = (float)((curtime.tv_sec - arttime->tv_sec) + (curtime.tv_usec - arttime->tv_usec) / 1000000.0);
#endif
			if (tdiff > 0)
			{
				fSpeed = (pArticleDownloader->GetBytes() / tdiff / 1024);
			}
		}

		fSpeedAllDownloads += fSpeed;
	}

	m_mutexDownloadQueue.Unlock();

	return fSpeedAllDownloads;
}

long long QueueCoordinator::CalcRemainingSize()
{
	long long lRemainingSize = 0;

	m_mutexDownloadQueue.Lock();
	for (DownloadQueue::iterator it = m_DownloadQueue.begin(); it != m_DownloadQueue.end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (!pFileInfo->GetPaused() && !pFileInfo->GetDeleted())
		{
			lRemainingSize += pFileInfo->GetRemainingSize();
		}
	}
	m_mutexDownloadQueue.Unlock();

	return lRemainingSize;
}

/*
 * NOTE: DownloadQueue must be locked prior to call of this function
 * Returns True if Entry was deleted from Queue or False it was scheduled for Deletion.
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
		DeleteFileInfo(pFileInfo);
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

bool QueueCoordinator::GetNextArticle(FileInfo* &pFileInfo, ArticleInfo* &pArticleInfo)
{
	//debug("QueueCoordinator::GetNextArticle()");

	for (DownloadQueue::iterator it = m_DownloadQueue.begin(); it != m_DownloadQueue.end(); it++)
	{
		pFileInfo = *it;
		if (!pFileInfo->GetPaused() && !pFileInfo->GetDeleted())
		{
			if (pFileInfo->GetArticles()->empty() && g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
			{
				g_pDiskState->LoadArticles(pFileInfo);
			}
			for (FileInfo::Articles::iterator at = pFileInfo->GetArticles()->begin(); at != pFileInfo->GetArticles()->end(); at++)
			{
				pArticleInfo = *at;
				if (pArticleInfo->GetStatus() == 0)
				{
					return true;
				}
			}
		}
	}

	return false;
}

void QueueCoordinator::StartArticleDownload(FileInfo* pFileInfo, ArticleInfo* pArticleInfo)
{
	debug("Starting new ArticleDownloader");

	ArticleDownloader* pArticleDownloader = new ArticleDownloader();
	pArticleDownloader->SetAutoDestroy(true);
	pArticleDownloader->Attach(this);
	pArticleDownloader->SetFileInfo(pFileInfo);
	pArticleDownloader->SetArticleInfo(pArticleInfo);
	BuildArticleFilename(pArticleDownloader, pFileInfo, pArticleInfo);

	pArticleInfo->SetStatus(ArticleInfo::aiRunning);

	m_ActiveDownloads.push_back(pArticleDownloader);
	pArticleDownloader->Start();
	pArticleDownloader->WaitInit();
}

void QueueCoordinator::BuildArticleFilename(ArticleDownloader* pArticleDownloader, FileInfo* pFileInfo, ArticleInfo* pArticleInfo)
{
	char name[1024];
	
	snprintf(name, 1024, "%s%i.%03i", g_pOptions->GetTempDir(), pFileInfo->GetID(), pArticleInfo->GetPartNumber());
	name[1024-1] = '\0';
	pArticleInfo->SetResultFilename(name);

	char tmpname[1024];
	snprintf(tmpname, 1024, "%s.tmp", name);
	tmpname[1024-1] = '\0';
	pArticleDownloader->SetTempFilename(tmpname);

	char szNZBNiceName[1024];
	pFileInfo->GetNiceNZBName(szNZBNiceName, 1024);
	
	snprintf(name, 1024, "%s%c%s [%i/%i]", szNZBNiceName, (int)PATH_SEPARATOR, pFileInfo->GetFilename(), pArticleInfo->GetPartNumber(), pFileInfo->GetArticles()->size());
	name[1024-1] = '\0';
	pArticleDownloader->SetInfoName(name);

	if (g_pOptions->GetDirectWrite())
	{
		snprintf(name, 1024, "%s%i.out", g_pOptions->GetTempDir(), pFileInfo->GetID());
		name[1024-1] = '\0';
		pArticleDownloader->SetOutputFilename(name);
	}
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
	        (pArticleDownloader->GetStatus() == ArticleDownloader::adFailed))
	{
		ArticleCompleted(pArticleDownloader);
	}
}

void QueueCoordinator::ArticleCompleted(ArticleDownloader* pArticleDownloader)
{
	debug("Article downloaded");

	FileInfo* pFileInfo = pArticleDownloader->GetFileInfo();
	ArticleInfo* pArticleInfo = pArticleDownloader->GetArticleInfo();

	m_mutexDownloadQueue.Lock();

	if (pArticleDownloader->GetStatus() == ArticleDownloader::adFinished)
	{
		pArticleInfo->SetStatus(ArticleInfo::aiFinished);
	}
	else if (pArticleDownloader->GetStatus() == ArticleDownloader::adFailed)
	{
		pArticleInfo->SetStatus(ArticleInfo::aiFailed);
	}

	pFileInfo->SetRemainingSize(pFileInfo->GetRemainingSize() - pArticleInfo->GetSize());
	pFileInfo->SetCompleted(pFileInfo->GetCompleted() + 1);
	bool fileCompleted = (int)pFileInfo->GetArticles()->size() == pFileInfo->GetCompleted();

	if (!pFileInfo->GetFilenameConfirmed() &&
	        pArticleDownloader->GetStatus() == ArticleDownloader::adFinished &&
	        pArticleDownloader->GetArticleFilename())
	{
		pFileInfo->SetFilename(pArticleDownloader->GetArticleFilename());
		pFileInfo->SetFilenameConfirmed(true);
	}

	bool deleteFileObj = false;

	if (pFileInfo->GetDeleted())
	{
		int cnt = 0;
		for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
		{
			if ((*it)->GetFileInfo() == pFileInfo)
			{
				cnt++;
			}
		}
		if (cnt == 1)
		{
			// this was the last Download for a file deleted from queue
			deleteFileObj = true;
		}
	}

	if (fileCompleted && !IsStopped() && !pFileInfo->GetDeleted())
	{
		// all jobs done
		m_mutexDownloadQueue.Unlock();
		pArticleDownloader->CompleteFileParts();
		m_mutexDownloadQueue.Lock();
		deleteFileObj = true;
	}

	// delete Download from Queue
	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
	{
		ArticleDownloader* pa = *it;
		if (pa == pArticleDownloader)
		{
			m_ActiveDownloads.erase(it);
			break;
		}
	}
	if (deleteFileObj)
	{
		// delete File from Queue
		pFileInfo->SetDeleted(true);

		Aspect aspect = { fileCompleted ? eaFileCompleted : eaFileDeleted, pFileInfo, &m_DownloadQueue, NULL };
		Notify(&aspect);
		
		DeleteFileInfo(pFileInfo);
	}

	m_mutexDownloadQueue.Unlock();
}

void QueueCoordinator::DeleteFileInfo(FileInfo* pFileInfo)
{
	for (DownloadQueue::iterator it = m_DownloadQueue.begin(); it != m_DownloadQueue.end(); it++)
	{
		FileInfo* pa = *it;
		if (pa == pFileInfo)
		{
			m_DownloadQueue.erase(it);
			break;
		}
	}

	if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
	{
		g_pDiskState->DiscardFile(&m_DownloadQueue, pFileInfo);
	}

	delete pFileInfo;
}

bool QueueCoordinator::IsDupe(FileInfo* pFileInfo)
{
	debug("Checking if the file is already queued");

	if (pFileInfo->IsDupe())
	{
		return true;
	}

	for (DownloadQueue::iterator it = m_DownloadQueue.begin(); it != m_DownloadQueue.end(); it++)
	{
		FileInfo* pQueueEntry = *it;
		if (!strcmp(pFileInfo->GetDestDir(), pQueueEntry->GetDestDir()) &&
		        !strcmp(pFileInfo->GetFilename(), pQueueEntry->GetFilename()))
		{
			return true;
		}
	}

	return false;
}

void QueueCoordinator::LogDebugInfo()
{
	debug("--------------------------------------------");
	debug("Dumping debug info to log");
	debug("--------------------------------------------");

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
	const int TimeOut = g_pOptions->GetTerminateTimeout();
	if (TimeOut == 0)
	{
		return;
	}

	m_mutexDownloadQueue.Lock();
	time_t tm = ::time(NULL);

	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end();)
	{
		ArticleDownloader* pArticleDownloader = *it;
		if (tm - pArticleDownloader->GetLastUpdateTime() > TimeOut &&
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
				error("Could not terminate hanging download %s", BaseFileName(pArticleInfo->GetResultFilename()));
			}
			m_ActiveDownloads.erase(it);
			// it's not safe to destroy pArticleDownloader, because the state of object is unknown
			delete pArticleDownloader;
			it = m_ActiveDownloads.begin();
			continue;
		}
		it++;
	}                                              

	m_mutexDownloadQueue.Unlock();
}
