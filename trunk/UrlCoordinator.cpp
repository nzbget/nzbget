/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2012-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include "nzbget.h"
#include "UrlCoordinator.h"
#include "Options.h"
#include "WebDownloader.h"
#include "DiskState.h"
#include "Log.h"
#include "Util.h"
#include "NZBFile.h"
#include "QueueCoordinator.h"
#include "Scanner.h"

extern Options* g_pOptions;
extern DiskState* g_pDiskState;
extern QueueCoordinator* g_pQueueCoordinator;
extern Scanner* g_pScanner;


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

	if (!strncmp(szLine, "X-DNZB-Category:", 16))
	{
		if (m_szCategory)
		{
			free(m_szCategory);
		}

		char* szCategory = strdup(szLine + 16);
		m_szCategory = strdup(Util::Trim(szCategory));
		free(szCategory);

		debug("Category: %s", m_szCategory);
	}
	else if (!strncmp(szLine, "X-DNZB-", 7))
	{
		char* szModLine = strdup(szLine);
		char* szValue = strchr(szModLine, ':');
		if (szValue)
		{
			*szValue = NULL;
			szValue++;
			while (*szValue == ' ') szValue++;
			Util::Trim(szValue);
			
			debug("X-DNZB: %s", szModLine);
			debug("Value: %s", szValue);

			char szParamName[100];
			snprintf(szParamName, 100, "*DNZB:%s", szModLine + 7);
			szParamName[100-1] = '\0';

			m_ppParameters.SetParameter(szParamName, szValue);
		}
		free(szModLine);
	}
}

UrlCoordinator::UrlCoordinator()
{
	debug("Creating UrlCoordinator");

	m_bHasMoreJobs = true;
	m_bForce = false;
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
		if (!(g_pOptions->GetPauseDownload() || g_pOptions->GetPauseDownload2()) || m_bForce)
		{
			// start download for next URL
			DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

			if ((int)m_ActiveDownloads.size() < g_pOptions->GetUrlConnections())
			{
				UrlInfo* pUrlInfo;
				bool bHasMoreUrls = GetNextUrl(pDownloadQueue, pUrlInfo);
				bool bUrlDownloadsRunning = !m_ActiveDownloads.empty();
				m_bHasMoreJobs = bHasMoreUrls || bUrlDownloadsRunning;
				if (bHasMoreUrls && !IsStopped())
				{
					StartUrlDownload(pUrlInfo);
				}
				if (!bHasMoreUrls)
				{
					m_bForce = false;
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

	if (pUrlInfo->GetForce())
	{
		m_bForce = true;
	}

	g_pQueueCoordinator->UnlockQueue();
}

/*
 * Returns next URL for download.
 */
bool UrlCoordinator::GetNextUrl(DownloadQueue* pDownloadQueue, UrlInfo* &pUrlInfo)
{
	bool bPauseDownload = g_pOptions->GetPauseDownload() || g_pOptions->GetPauseDownload2();

	for (UrlQueue::iterator at = pDownloadQueue->GetUrlQueue()->begin(); at != pDownloadQueue->GetUrlQueue()->end(); at++)
	{
		pUrlInfo = *at;
		if (pUrlInfo->GetStatus() == 0 && (!bPauseDownload || pUrlInfo->GetForce()))
		{
			return true;
			break;
		}
	}

	return false;
}

void UrlCoordinator::StartUrlDownload(UrlInfo* pUrlInfo)
{
	debug("Starting new UrlDownloader");

	UrlDownloader* pUrlDownloader = new UrlDownloader();
	pUrlDownloader->SetAutoDestroy(true);
	pUrlDownloader->Attach(this);
	pUrlDownloader->SetUrlInfo(pUrlInfo);
	pUrlDownloader->SetURL(pUrlInfo->GetURL());
	pUrlDownloader->SetForce(pUrlInfo->GetForce());

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

void UrlCoordinator::Update(Subject* pCaller, void* pAspect)
{
	debug("Notification from UrlDownloader received");

	UrlDownloader* pUrlDownloader = (UrlDownloader*) pCaller;
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

	// delete Download from active jobs
	g_pQueueCoordinator->LockQueue();
	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
	{
		UrlDownloader* pa = *it;
		if (pa == pUrlDownloader)
		{
			m_ActiveDownloads.erase(it);
			break;
		}
	}
	g_pQueueCoordinator->UnlockQueue();

	Aspect aspect = { eaUrlCompleted, pUrlInfo };
	Notify(&aspect);

	if (pUrlInfo->GetStatus() == UrlInfo::aiFinished)
	{
		// add nzb-file to download queue
		Scanner::EAddStatus eAddStatus = g_pScanner->AddExternalFile(
			pUrlInfo->GetNZBFilename() && strlen(pUrlInfo->GetNZBFilename()) > 0 ? pUrlInfo->GetNZBFilename() : filename,
			strlen(pUrlInfo->GetCategory()) > 0 ? pUrlInfo->GetCategory() : pUrlDownloader->GetCategory(),
			pUrlInfo->GetPriority(), pUrlInfo->GetDupeKey(), pUrlInfo->GetDupeScore(), pUrlInfo->GetDupeMode(),
			pUrlDownloader->GetParameters(), pUrlInfo->GetAddTop(), pUrlInfo->GetAddPaused(),
			pUrlDownloader->GetOutputFilename(), NULL, 0);

		if (eAddStatus != Scanner::asSuccess)
		{
			pUrlInfo->SetStatus(eAddStatus == Scanner::asFailed ? UrlInfo::aiScanFailed : UrlInfo::aiScanSkipped);
		}
	}

	// delete Download from Url Queue
	if (pUrlInfo->GetStatus() != UrlInfo::aiRetry)
	{
		DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

		for (UrlQueue::iterator it = pDownloadQueue->GetUrlQueue()->begin(); it != pDownloadQueue->GetUrlQueue()->end(); it++)
		{
			UrlInfo* pa = *it;
			if (pa == pUrlInfo)
			{
				pDownloadQueue->GetUrlQueue()->erase(it);
				break;
			}
		}

		bool bDeleteObj = true;

		if (g_pOptions->GetKeepHistory() > 0 && pUrlInfo->GetStatus() != UrlInfo::aiFinished)
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

		g_pQueueCoordinator->UnlockQueue();

		if (bDeleteObj)
		{
			delete pUrlInfo;
		}
	}
}
