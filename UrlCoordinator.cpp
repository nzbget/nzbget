/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2012 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 * $Revision: 1 $
 * $Date: 2012-04-12 12:00:00 +0200 (Do, 12 April 2012) $
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
#include <cstdio>
#include <sys/stat.h>
#ifndef WIN32
#include <unistd.h>
#include <sys/time.h>
#endif

#include "nzbget.h"
#include "UrlCoordinator.h"
#include "Options.h"
#include "WebDownloader.h"
#include "DiskState.h"
#include "Log.h"
#include "Util.h"
#include "NZBFile.h"
#include "QueueCoordinator.h"

extern Options* g_pOptions;
extern DiskState* g_pDiskState;
extern QueueCoordinator* g_pQueueCoordinator;

UrlDownloader::UrlDownloader() : WebDownloader()
{
	m_szCategory = NULL;
}

UrlDownloader::~UrlDownloader()
{
	if (m_szCategory)
	{
		free(m_szCategory);
	}
}

void UrlDownloader::ProcessHeader(const char* szLine)
{
	WebDownloader::ProcessHeader(szLine);

	if (!strncmp(szLine, "X-DNZB-Category: ", 17))
	{
		if (m_szCategory)
		{
			free(m_szCategory);
		}

		const char *szCat = szLine + 17;

		int iCatLen = strlen(szCat);

		// trim trailing CR/LF/spaces
		while (iCatLen > 0 && (szCat[iCatLen-1] == '\n' || szCat[iCatLen-1] == '\r' || szCat[iCatLen-1] == ' ')) iCatLen--; 

		m_szCategory = (char*)malloc(iCatLen + 1);
		strncpy(m_szCategory, szCat, iCatLen);
		m_szCategory[iCatLen] = '\0';

		debug("Category: %s", m_szCategory);
	}
}

UrlCoordinator::UrlCoordinator()
{
	debug("Creating UrlCoordinator");

	m_bHasMoreJobs = true;
}

UrlCoordinator::~UrlCoordinator()
{
	debug("Destroying UrlCoordinator");
	// Cleanup

	debug("Deleting UrlDownloaders");
	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
	{
		delete *it;
	}
	m_ActiveDownloads.clear();

	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();
	for (UrlQueue::iterator it = pDownloadQueue->GetUrlQueue()->begin(); it != pDownloadQueue->GetUrlQueue()->end(); it++)
	{
		delete *it;
	}
	pDownloadQueue->GetUrlQueue()->clear();
	g_pQueueCoordinator->UnlockQueue();

	debug("UrlCoordinator destroyed");
}

void UrlCoordinator::Run()
{
	debug("Entering UrlCoordinator-loop");

	int iResetCounter = 0;

	while (!IsStopped())
	{
		if (!(g_pOptions->GetPauseDownload() || g_pOptions->GetPauseDownload2()))
		{
			// start download for next URL
			DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

			if ((int)m_ActiveDownloads.size() < g_pOptions->GetUrlConnections())
			{
				UrlInfo* pUrlInfo;
				bool bHasMoreUrls = GetNextUrl(pDownloadQueue, pUrlInfo);
				bool bUrlDownloadsRunning = !m_ActiveDownloads.empty();
				m_bHasMoreJobs = bHasMoreUrls || bUrlDownloadsRunning;
				if (bHasMoreUrls && !IsStopped() && Thread::GetThreadCount() < g_pOptions->GetThreadLimit())
				{
					StartUrlDownload(pUrlInfo);
				}
			}
			g_pQueueCoordinator->UnlockQueue();
		}

		int iSleepInterval = 100;
		usleep(iSleepInterval * 1000);

		iResetCounter += iSleepInterval;
		if (iResetCounter >= 1000)
		{
			// this code should not be called too often, once per second is OK
			ResetHangingDownloads();
			iResetCounter = 0;
		}
	}

	// waiting for downloads
	debug("UrlCoordinator: waiting for Downloads to complete");
	bool completed = false;
	while (!completed)
	{
		g_pQueueCoordinator->LockQueue();
		completed = m_ActiveDownloads.size() == 0;
		g_pQueueCoordinator->UnlockQueue();
		usleep(100 * 1000);
		ResetHangingDownloads();
	}
	debug("UrlCoordinator: Downloads are completed");

	debug("Exiting UrlCoordinator-loop");
}

void UrlCoordinator::Stop()
{
	Thread::Stop();

	debug("Stopping UrlDownloads");
	g_pQueueCoordinator->LockQueue();
	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
	{
		(*it)->Stop();
	}
	g_pQueueCoordinator->UnlockQueue();
	debug("UrlDownloads are notified");
}

void UrlCoordinator::ResetHangingDownloads()
{
	const int TimeOut = g_pOptions->GetTerminateTimeout();
	if (TimeOut == 0)
	{
		return;
	}

	g_pQueueCoordinator->LockQueue();
	time_t tm = ::time(NULL);

	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end();)
	{
		UrlDownloader* pUrlDownloader = *it;
		if (tm - pUrlDownloader->GetLastUpdateTime() > TimeOut &&
		   pUrlDownloader->GetStatus() == UrlDownloader::adRunning)
		{
			UrlInfo* pUrlInfo = pUrlDownloader->GetUrlInfo();
			debug("Terminating hanging download %s", pUrlDownloader->GetInfoName());
			if (pUrlDownloader->Terminate())
			{
				error("Terminated hanging download %s", pUrlDownloader->GetInfoName());
				pUrlInfo->SetStatus(UrlInfo::aiUndefined);
			}
			else
			{
				error("Could not terminate hanging download %s", pUrlDownloader->GetInfoName());
			}
			m_ActiveDownloads.erase(it);
			// it's not safe to destroy pUrlDownloader, because the state of object is unknown
			delete pUrlDownloader;
			it = m_ActiveDownloads.begin();
			continue;
		}
		it++;
	}                                              

	g_pQueueCoordinator->UnlockQueue();
}

void UrlCoordinator::LogDebugInfo()
{
	debug("   UrlCoordinator");
	debug("   ----------------");

	g_pQueueCoordinator->LockQueue();
	debug("    Active Downloads: %i", m_ActiveDownloads.size());
	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
	{
		UrlDownloader* pUrlDownloader = *it;
		pUrlDownloader->LogDebugInfo();
	}
	g_pQueueCoordinator->UnlockQueue();
}

void UrlCoordinator::AddUrlToQueue(UrlInfo* pUrlInfo, bool AddFirst)
{
	debug("Adding NZB-URL to queue");

	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	pDownloadQueue->GetUrlQueue()->push_back(pUrlInfo);

	if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
	{
		g_pDiskState->SaveDownloadQueue(pDownloadQueue);
	}

	g_pQueueCoordinator->UnlockQueue();
}

/*
 * Returns next URL for download.
 */
bool UrlCoordinator::GetNextUrl(DownloadQueue* pDownloadQueue, UrlInfo* &pUrlInfo)
{
	bool bOK = false;

	for (UrlQueue::iterator at = pDownloadQueue->GetUrlQueue()->begin(); at != pDownloadQueue->GetUrlQueue()->end(); at++)
	{
		pUrlInfo = *at;
		if (pUrlInfo->GetStatus() == 0)
		{
			bOK = true;
			break;
		}
	}

	return bOK;
}

void UrlCoordinator::StartUrlDownload(UrlInfo* pUrlInfo)
{
	debug("Starting new UrlDownloader");

	UrlDownloader* pUrlDownloader = new UrlDownloader();
	pUrlDownloader->SetAutoDestroy(true);
	pUrlDownloader->Attach(this);
	pUrlDownloader->SetUrlInfo(pUrlInfo);
	pUrlDownloader->SetURL(pUrlInfo->GetURL());

	char tmp[1024];

	pUrlInfo->GetName(tmp, 1024);
	pUrlDownloader->SetInfoName(tmp);

	snprintf(tmp, 1024, "%surl-%i.tmp", g_pOptions->GetTempDir(), pUrlInfo->GetID());
	tmp[1024-1] = '\0';
	pUrlDownloader->SetOutputFilename(tmp);

	pUrlInfo->SetStatus(UrlInfo::aiRunning);

	m_ActiveDownloads.push_back(pUrlDownloader);
	pUrlDownloader->Start();
}

void UrlCoordinator::Update(Subject* Caller, void* Aspect)
{
	debug("Notification from UrlDownloader received");

	UrlDownloader* pUrlDownloader = (UrlDownloader*) Caller;
	if ((pUrlDownloader->GetStatus() == WebDownloader::adFinished) ||
		(pUrlDownloader->GetStatus() == WebDownloader::adFailed) ||
		(pUrlDownloader->GetStatus() == WebDownloader::adRetry))
	{
		UrlCompleted(pUrlDownloader);
	}
}

void UrlCoordinator::UrlCompleted(UrlDownloader* pUrlDownloader)
{
	debug("URL downloaded");

	UrlInfo* pUrlInfo = pUrlDownloader->GetUrlInfo();

	if (pUrlDownloader->GetStatus() == WebDownloader::adFinished)
	{
		pUrlInfo->SetStatus(UrlInfo::aiFinished);
	}
	else if (pUrlDownloader->GetStatus() == WebDownloader::adFailed)
	{
		pUrlInfo->SetStatus(UrlInfo::aiFailed);
	}
	else if (pUrlDownloader->GetStatus() == WebDownloader::adRetry)
	{
		pUrlInfo->SetStatus(UrlInfo::aiUndefined);
	}

	char filename[1024];
	if (pUrlDownloader->GetOriginalFilename())
	{
		strncpy(filename, pUrlDownloader->GetOriginalFilename(), 1024);
		filename[1024-1] = '\0';
	}
	else
	{
		strncpy(filename, Util::BaseFileName(pUrlInfo->GetURL()), 1024);
		filename[1024-1] = '\0';

		// TODO: decode URL escaping
	}

	Util::MakeValidFilename(filename, '_', false);

	debug("Filename: [%s]", filename);

	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	// delete Download from Queue
	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
	{
		UrlDownloader* pa = *it;
		if (pa == pUrlDownloader)
		{
			m_ActiveDownloads.erase(it);
			break;
		}
	}

	bool bDeleteObj = false;

	if (pUrlInfo->GetStatus() == UrlInfo::aiFinished || pUrlInfo->GetStatus() == UrlInfo::aiFailed)
	{
		// delete UrlInfo from Queue
		for (UrlQueue::iterator it = pDownloadQueue->GetUrlQueue()->begin(); it != pDownloadQueue->GetUrlQueue()->end(); it++)
		{
			UrlInfo* pa = *it;
			if (pa == pUrlInfo)
			{
				pDownloadQueue->GetUrlQueue()->erase(it);
				break;
			}
		}

		bDeleteObj = true;

		if (g_pOptions->GetKeepHistory() > 0 && pUrlInfo->GetStatus() == UrlInfo::aiFailed)
		{
			HistoryInfo* pHistoryInfo = new HistoryInfo(pUrlInfo);
			pHistoryInfo->SetTime(time(NULL));
			pDownloadQueue->GetHistoryList()->push_front(pHistoryInfo);
			bDeleteObj = false;
		}
			
		if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
		{
			g_pDiskState->SaveDownloadQueue(pDownloadQueue);
		}
	}

	g_pQueueCoordinator->UnlockQueue();

	if (pUrlInfo->GetStatus() == UrlInfo::aiFinished)
	{
		// add nzb-file to download queue
		AddToNZBQueue(pUrlInfo, pUrlDownloader->GetOutputFilename(), filename, pUrlDownloader->GetCategory());
	}

	if (bDeleteObj)
	{
		delete pUrlInfo;
	}
}

void UrlCoordinator::AddToNZBQueue(UrlInfo* pUrlInfo, const char* szTempFilename, const char* szOriginalFilename, const char* szOriginalCategory)
{
	info("Queue downloaded collection %s", szOriginalFilename);

	NZBFile* pNZBFile = NZBFile::CreateFromFile(szTempFilename, pUrlInfo->GetCategory());
	if (pNZBFile)
	{
		pNZBFile->GetNZBInfo()->SetName(NULL);
		pNZBFile->GetNZBInfo()->SetFilename(pUrlInfo->GetNZBFilename() && strlen(pUrlInfo->GetNZBFilename()) > 0 ? pUrlInfo->GetNZBFilename() : szOriginalFilename);

		if (strlen(pUrlInfo->GetCategory()) > 0)
		{
			pNZBFile->GetNZBInfo()->SetCategory(pUrlInfo->GetCategory());
		}
		else if (szOriginalCategory)
		{
			pNZBFile->GetNZBInfo()->SetCategory(szOriginalCategory);
		}

		pNZBFile->GetNZBInfo()->BuildDestDirName();

		for (NZBFile::FileInfos::iterator it = pNZBFile->GetFileInfos()->begin(); it != pNZBFile->GetFileInfos()->end(); it++)
		{
			FileInfo* pFileInfo = *it;
			pFileInfo->SetPriority(pUrlInfo->GetPriority());
		}

		g_pQueueCoordinator->AddNZBFileToQueue(pNZBFile, false);
		delete pNZBFile;
		info("Collection %s added to queue", szOriginalFilename);
	}
	else
	{
		error("Could not add downloaded collection %s to queue", szOriginalFilename);
	}
}