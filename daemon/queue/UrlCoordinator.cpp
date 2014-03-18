/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2012-2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "Log.h"
#include "Util.h"
#include "NZBFile.h"
#include "Scanner.h"

extern Options* g_pOptions;
extern Scanner* g_pScanner;

UrlDownloader::UrlDownloader() : WebDownloader()
{
	m_szCategory = NULL;
}

UrlDownloader::~UrlDownloader()
{
	free(m_szCategory);
}

void UrlDownloader::ProcessHeader(const char* szLine)
{
	WebDownloader::ProcessHeader(szLine);

	if (!strncmp(szLine, "X-DNZB-Category:", 16))
	{
		free(m_szCategory);
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

			char* szVal = WebUtil::Latin1ToUtf8(szValue);
			m_pNZBInfo->GetParameters()->SetParameter(szParamName, szVal);
			free(szVal);
		}
		free(szModLine);
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

	debug("UrlCoordinator destroyed");
}

void UrlCoordinator::Run()
{
	debug("Entering UrlCoordinator-loop");

	int iResetCounter = 0;

	while (!IsStopped())
	{
		if (!g_pOptions->GetPauseDownload() || g_pOptions->GetUrlForce())
		{
			// start download for next URL
			DownloadQueue* pDownloadQueue = DownloadQueue::Lock();

			if ((int)m_ActiveDownloads.size() < g_pOptions->GetUrlConnections())
			{
				NZBInfo* pNZBInfo = GetNextUrl(pDownloadQueue);
				bool bHasMoreUrls = pNZBInfo != NULL;
				bool bUrlDownloadsRunning = !m_ActiveDownloads.empty();
				m_bHasMoreJobs = bHasMoreUrls || bUrlDownloadsRunning;
				if (bHasMoreUrls && !IsStopped())
				{
					StartUrlDownload(pNZBInfo);
				}
			}
			DownloadQueue::Unlock();
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
		DownloadQueue::Lock();
		completed = m_ActiveDownloads.size() == 0;
		DownloadQueue::Unlock();
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
	DownloadQueue::Lock();
	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
	{
		(*it)->Stop();
	}
	DownloadQueue::Unlock();
	debug("UrlDownloads are notified");
}

void UrlCoordinator::ResetHangingDownloads()
{
	const int TimeOut = g_pOptions->GetTerminateTimeout();
	if (TimeOut == 0)
	{
		return;
	}

	DownloadQueue::Lock();
	time_t tm = ::time(NULL);

	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end();)
	{
		UrlDownloader* pUrlDownloader = *it;
		if (tm - pUrlDownloader->GetLastUpdateTime() > TimeOut &&
		   pUrlDownloader->GetStatus() == UrlDownloader::adRunning)
		{
			NZBInfo* pNZBInfo = pUrlDownloader->GetNZBInfo();
			debug("Terminating hanging download %s", pUrlDownloader->GetInfoName());
			if (pUrlDownloader->Terminate())
			{
				error("Terminated hanging download %s", pUrlDownloader->GetInfoName());
				pNZBInfo->SetUrlStatus(NZBInfo::lsNone);
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

	DownloadQueue::Unlock();
}

void UrlCoordinator::LogDebugInfo()
{
	debug("   UrlCoordinator");
	debug("   ----------------");

	DownloadQueue::Lock();
	debug("    Active Downloads: %i", m_ActiveDownloads.size());
	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
	{
		UrlDownloader* pUrlDownloader = *it;
		pUrlDownloader->LogDebugInfo();
	}
	DownloadQueue::Unlock();
}

void UrlCoordinator::AddUrlToQueue(NZBInfo* pNZBInfo, bool bAddTop)
{
	debug("Adding NZB-URL to queue");

	DownloadQueue* pDownloadQueue = DownloadQueue::Lock();
	if (bAddTop)
	{
		pDownloadQueue->GetQueue()->push_front(pNZBInfo);
	}
	else
	{
		pDownloadQueue->GetQueue()->push_back(pNZBInfo);
	}
	pDownloadQueue->Save();
	DownloadQueue::Unlock();
}

/*
 * Returns next URL for download.
 */
NZBInfo* UrlCoordinator::GetNextUrl(DownloadQueue* pDownloadQueue)
{
	bool bPauseDownload = g_pOptions->GetPauseDownload();

	NZBInfo* pNZBInfo = NULL;

	for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
	{
		NZBInfo* pNZBInfo1 = *it;
		if (pNZBInfo1->GetKind() == NZBInfo::nkUrl &&
			pNZBInfo1->GetUrlStatus() == NZBInfo::lsNone &&
			pNZBInfo1->GetDeleteStatus() == NZBInfo::dsNone &&
			(!bPauseDownload || g_pOptions->GetUrlForce()) &&
			(!pNZBInfo || pNZBInfo1->GetPriority() > pNZBInfo->GetPriority()))
		{
			pNZBInfo = pNZBInfo1;
		}
	}

	return pNZBInfo;
}

void UrlCoordinator::StartUrlDownload(NZBInfo* pNZBInfo)
{
	debug("Starting new UrlDownloader");

	UrlDownloader* pUrlDownloader = new UrlDownloader();
	pUrlDownloader->SetAutoDestroy(true);
	pUrlDownloader->Attach(this);
	pUrlDownloader->SetNZBInfo(pNZBInfo);
	pUrlDownloader->SetURL(pNZBInfo->GetURL());
	pUrlDownloader->SetForce(g_pOptions->GetUrlForce());
	pNZBInfo->SetActiveDownloads(1);

	char tmp[1024];

	pNZBInfo->MakeNiceUrlName(pNZBInfo->GetURL(), pNZBInfo->GetFilename(), tmp, 1024);
	pUrlDownloader->SetInfoName(tmp);

	snprintf(tmp, 1024, "%surl-%i.tmp", g_pOptions->GetTempDir(), pNZBInfo->GetID());
	tmp[1024-1] = '\0';
	pUrlDownloader->SetOutputFilename(tmp);

	pNZBInfo->SetUrlStatus(NZBInfo::lsRunning);

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

	NZBInfo* pNZBInfo = pUrlDownloader->GetNZBInfo();

	if (pNZBInfo->GetDeleting())
	{
		pNZBInfo->SetUrlStatus(NZBInfo::lsNone);
		pNZBInfo->SetDeleteStatus(NZBInfo::dsManual);
		pNZBInfo->SetDeleting(false);
	}
	else if (pUrlDownloader->GetStatus() == WebDownloader::adFinished)
	{
		pNZBInfo->SetUrlStatus(NZBInfo::lsFinished);
	}
	else if (pUrlDownloader->GetStatus() == WebDownloader::adFailed)
	{
		pNZBInfo->SetUrlStatus(NZBInfo::lsFailed);
	}
	else if (pUrlDownloader->GetStatus() == WebDownloader::adRetry)
	{
		pNZBInfo->SetUrlStatus(NZBInfo::lsNone);
	}

	char filename[1024];
	if (pUrlDownloader->GetOriginalFilename())
	{
		strncpy(filename, pUrlDownloader->GetOriginalFilename(), 1024);
		filename[1024-1] = '\0';
	}
	else
	{
		strncpy(filename, Util::BaseFileName(pNZBInfo->GetURL()), 1024);
		filename[1024-1] = '\0';

		// TODO: decode URL escaping
	}

	Util::MakeValidFilename(filename, '_', false);

	debug("Filename: [%s]", filename);

	// delete Download from active jobs
	DownloadQueue* pDownloadQueue = DownloadQueue::Lock();
	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
	{
		UrlDownloader* pa = *it;
		if (pa == pUrlDownloader)
		{
			m_ActiveDownloads.erase(it);
			break;
		}
	}
	pNZBInfo->SetActiveDownloads(0);

	DownloadQueue::Aspect aspect = { DownloadQueue::eaUrlCompleted, pDownloadQueue, pNZBInfo, NULL };
	pDownloadQueue->Notify(&aspect);

	DownloadQueue::Unlock();

	if (pNZBInfo->GetUrlStatus() == NZBInfo::lsFinished)
	{
		// add nzb-file to download queue
		Scanner::EAddStatus eAddStatus = g_pScanner->AddExternalFile(
			pNZBInfo->GetFilename() && strlen(pNZBInfo->GetFilename()) > 0 ? pNZBInfo->GetFilename() : filename,
			strlen(pNZBInfo->GetCategory()) > 0 ? pNZBInfo->GetCategory() : pUrlDownloader->GetCategory(),
			pNZBInfo->GetPriority(), pNZBInfo->GetDupeKey(), pNZBInfo->GetDupeScore(), pNZBInfo->GetDupeMode(),
			pNZBInfo->GetParameters(), false, pNZBInfo->GetAddUrlPaused(), pNZBInfo,
			pUrlDownloader->GetOutputFilename(), NULL, 0);

		if (eAddStatus == Scanner::asSuccess)
		{
			// if scanner has successfully added nzb-file to queue, our pNZBInfo is already destroyed
			return;
		}

		pNZBInfo->SetUrlStatus(eAddStatus == Scanner::asFailed ? NZBInfo::lsScanFailed : NZBInfo::lsScanSkipped);
	}

	// delete Download from Url Queue
	if (pNZBInfo->GetUrlStatus() != NZBInfo::lsRetry)
	{
		DownloadQueue* pDownloadQueue = DownloadQueue::Lock();
		pDownloadQueue->GetQueue()->Remove(pNZBInfo);
		bool bDeleteObj = true;

		if (g_pOptions->GetKeepHistory() > 0 &&
			pNZBInfo->GetUrlStatus() != NZBInfo::lsFinished &&
			!pNZBInfo->GetAvoidHistory())
		{
			HistoryInfo* pHistoryInfo = new HistoryInfo(pNZBInfo);
			pHistoryInfo->SetTime(time(NULL));
			pDownloadQueue->GetHistory()->push_front(pHistoryInfo);
			bDeleteObj = false;
		}
			
		pDownloadQueue->Save();

		DownloadQueue::Unlock();

		if (bDeleteObj)
		{
			delete pNZBInfo;
		}
	}
}

bool UrlCoordinator::DeleteQueueEntry(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, bool bAvoidHistory)
{
	if (pNZBInfo->GetActiveDownloads() > 0)
	{
		info("Deleting active URL %s", pNZBInfo->GetName());
		pNZBInfo->SetDeleting(true);
		pNZBInfo->SetAvoidHistory(bAvoidHistory);

		for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
		{
			UrlDownloader* pUrlDownloader = *it;
			if (pUrlDownloader->GetNZBInfo() == pNZBInfo)
			{
				pUrlDownloader->Stop();
				return true;
			}
		}                                              
	}

	info("Deleting URL %s", pNZBInfo->GetName());

	pNZBInfo->SetDeleteStatus(NZBInfo::dsManual);
	pNZBInfo->SetUrlStatus(NZBInfo::lsNone);

	pDownloadQueue->GetQueue()->Remove(pNZBInfo);
	if (g_pOptions->GetKeepHistory() > 0 && !bAvoidHistory)
	{
		HistoryInfo* pHistoryInfo = new HistoryInfo(pNZBInfo);
		pHistoryInfo->SetTime(time(NULL));
		pDownloadQueue->GetHistory()->push_front(pHistoryInfo);
	}
	else
	{
		delete pNZBInfo;
	}

	return true;
}
