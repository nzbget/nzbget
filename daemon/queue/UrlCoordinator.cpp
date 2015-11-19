/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2012-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "UrlCoordinator.h"
#include "Options.h"
#include "WebDownloader.h"
#include "Util.h"
#include "NzbFile.h"
#include "Scanner.h"
#include "DiskState.h"
#include "QueueScript.h"

UrlDownloader::UrlDownloader() : WebDownloader()
{
	m_category = NULL;
}

UrlDownloader::~UrlDownloader()
{
	free(m_category);
}

void UrlDownloader::ProcessHeader(const char* line)
{
	WebDownloader::ProcessHeader(line);

	if (!strncmp(line, "X-DNZB-Category:", 16))
	{
		free(m_category);
		char* category = strdup(line + 16);
		m_category = strdup(Util::Trim(category));
		free(category);

		debug("Category: %s", m_category);
	}
	else if (!strncmp(line, "X-DNZB-", 7))
	{
		char* modLine = strdup(line);
		char* value = strchr(modLine, ':');
		if (value)
		{
			*value = '\0';
			value++;
			while (*value == ' ') value++;
			Util::Trim(value);

			debug("X-DNZB: %s", modLine);
			debug("Value: %s", value);

			char paramName[100];
			snprintf(paramName, 100, "*DNZB:%s", modLine + 7);
			paramName[100-1] = '\0';

			char* val = WebUtil::Latin1ToUtf8(value);
			m_nzbInfo->GetParameters()->SetParameter(paramName, val);
			free(val);
		}
		free(modLine);
	}
}

UrlCoordinator::UrlCoordinator()
{
	debug("Creating UrlCoordinator");

	m_hasMoreJobs = true;

	g_Log->RegisterDebuggable(this);
}

UrlCoordinator::~UrlCoordinator()
{
	debug("Destroying UrlCoordinator");
	// Cleanup

	g_Log->UnregisterDebuggable(this);

	debug("Deleting UrlDownloaders");
	for (ActiveDownloads::iterator it = m_activeDownloads.begin(); it != m_activeDownloads.end(); it++)
	{
		delete *it;
	}
	m_activeDownloads.clear();

	debug("UrlCoordinator destroyed");
}

void UrlCoordinator::Run()
{
	debug("Entering UrlCoordinator-loop");

	while (!DownloadQueue::IsLoaded())
	{
		usleep(20 * 1000);
	}

	int resetCounter = 0;

	while (!IsStopped())
	{
		bool downloadStarted = false;
		if (!g_Options->GetPauseDownload() || g_Options->GetUrlForce())
		{
			// start download for next URL
			DownloadQueue* downloadQueue = DownloadQueue::Lock();
			if ((int)m_activeDownloads.size() < g_Options->GetUrlConnections())
			{
				NzbInfo* nzbInfo = GetNextUrl(downloadQueue);
				bool hasMoreUrls = nzbInfo != NULL;
				bool urlDownloadsRunning = !m_activeDownloads.empty();
				m_hasMoreJobs = hasMoreUrls || urlDownloadsRunning;
				if (hasMoreUrls && !IsStopped())
				{
					StartUrlDownload(nzbInfo);
					downloadStarted = true;
				}
			}
			DownloadQueue::Unlock();
		}

		int sleepInterval = downloadStarted ? 0 : 100;
		usleep(sleepInterval * 1000);

		resetCounter += sleepInterval;
		if (resetCounter >= 1000)
		{
			// this code should not be called too often, once per second is OK
			ResetHangingDownloads();
			resetCounter = 0;
		}
	}

	// waiting for downloads
	debug("UrlCoordinator: waiting for Downloads to complete");
	bool completed = false;
	while (!completed)
	{
		DownloadQueue::Lock();
		completed = m_activeDownloads.size() == 0;
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
	for (ActiveDownloads::iterator it = m_activeDownloads.begin(); it != m_activeDownloads.end(); it++)
	{
		(*it)->Stop();
	}
	DownloadQueue::Unlock();
	debug("UrlDownloads are notified");
}

void UrlCoordinator::ResetHangingDownloads()
{
	const int TimeOut = g_Options->GetTerminateTimeout();
	if (TimeOut == 0)
	{
		return;
	}

	DownloadQueue::Lock();
	time_t tm = time(NULL);

	for (ActiveDownloads::iterator it = m_activeDownloads.begin(); it != m_activeDownloads.end();)
	{
		UrlDownloader* urlDownloader = *it;
		if (tm - urlDownloader->GetLastUpdateTime() > TimeOut &&
		   urlDownloader->GetStatus() == UrlDownloader::adRunning)
		{
			NzbInfo* nzbInfo = urlDownloader->GetNzbInfo();
			debug("Terminating hanging download %s", urlDownloader->GetInfoName());
			if (urlDownloader->Terminate())
			{
				error("Terminated hanging download %s", urlDownloader->GetInfoName());
				nzbInfo->SetUrlStatus(NzbInfo::lsNone);
			}
			else
			{
				error("Could not terminate hanging download %s", urlDownloader->GetInfoName());
			}
			m_activeDownloads.erase(it);
			// it's not safe to destroy pUrlDownloader, because the state of object is unknown
			delete urlDownloader;
			it = m_activeDownloads.begin();
			continue;
		}
		it++;
	}

	DownloadQueue::Unlock();
}

void UrlCoordinator::LogDebugInfo()
{
	info("   ---------- UrlCoordinator");

	DownloadQueue::Lock();
	info("    Active Downloads: %i", m_activeDownloads.size());
	for (ActiveDownloads::iterator it = m_activeDownloads.begin(); it != m_activeDownloads.end(); it++)
	{
		UrlDownloader* urlDownloader = *it;
		urlDownloader->LogDebugInfo();
	}
	DownloadQueue::Unlock();
}

void UrlCoordinator::AddUrlToQueue(NzbInfo* nzbInfo, bool addTop)
{
	debug("Adding NZB-URL to queue");

	DownloadQueue* downloadQueue = DownloadQueue::Lock();
	if (addTop)
	{
		downloadQueue->GetQueue()->push_front(nzbInfo);
	}
	else
	{
		downloadQueue->GetQueue()->push_back(nzbInfo);
	}
	downloadQueue->Save();
	DownloadQueue::Unlock();
}

/*
 * Returns next URL for download.
 */
NzbInfo* UrlCoordinator::GetNextUrl(DownloadQueue* downloadQueue)
{
	bool pauseDownload = g_Options->GetPauseDownload();

	NzbInfo* nzbInfo = NULL;

	for (NzbList::iterator it = downloadQueue->GetQueue()->begin(); it != downloadQueue->GetQueue()->end(); it++)
	{
		NzbInfo* nzbInfo1 = *it;
		if (nzbInfo1->GetKind() == NzbInfo::nkUrl &&
			nzbInfo1->GetUrlStatus() == NzbInfo::lsNone &&
			nzbInfo1->GetDeleteStatus() == NzbInfo::dsNone &&
			(!pauseDownload || g_Options->GetUrlForce()) &&
			(!nzbInfo || nzbInfo1->GetPriority() > nzbInfo->GetPriority()))
		{
			nzbInfo = nzbInfo1;
		}
	}

	return nzbInfo;
}

void UrlCoordinator::StartUrlDownload(NzbInfo* nzbInfo)
{
	debug("Starting new UrlDownloader");

	UrlDownloader* urlDownloader = new UrlDownloader();
	urlDownloader->SetAutoDestroy(true);
	urlDownloader->Attach(this);
	urlDownloader->SetNzbInfo(nzbInfo);
	urlDownloader->SetUrl(nzbInfo->GetUrl());
	urlDownloader->SetForce(g_Options->GetUrlForce());
	nzbInfo->SetActiveDownloads(1);

	char tmp[1024];

	nzbInfo->MakeNiceUrlName(nzbInfo->GetUrl(), nzbInfo->GetFilename(), tmp, 1024);
	urlDownloader->SetInfoName(tmp);

	snprintf(tmp, 1024, "%surl-%i.tmp", g_Options->GetTempDir(), nzbInfo->GetId());
	tmp[1024-1] = '\0';
	urlDownloader->SetOutputFilename(tmp);

	nzbInfo->SetUrlStatus(NzbInfo::lsRunning);

	m_activeDownloads.push_back(urlDownloader);
	urlDownloader->Start();
}

void UrlCoordinator::Update(Subject* caller, void* aspect)
{
	debug("Notification from UrlDownloader received");

	UrlDownloader* urlDownloader = (UrlDownloader*) caller;
	if ((urlDownloader->GetStatus() == WebDownloader::adFinished) ||
		(urlDownloader->GetStatus() == WebDownloader::adFailed) ||
		(urlDownloader->GetStatus() == WebDownloader::adRetry))
	{
		UrlCompleted(urlDownloader);
	}
}

void UrlCoordinator::UrlCompleted(UrlDownloader* urlDownloader)
{
	debug("URL downloaded");

	NzbInfo* nzbInfo = urlDownloader->GetNzbInfo();

	char filename[1024];
	if (urlDownloader->GetOriginalFilename())
	{
		strncpy(filename, urlDownloader->GetOriginalFilename(), 1024);
		filename[1024-1] = '\0';
	}
	else
	{
		strncpy(filename, Util::BaseFileName(nzbInfo->GetUrl()), 1024);
		filename[1024-1] = '\0';

		// TODO: decode URL escaping
	}

	Util::MakeValidFilename(filename, '_', false);

	debug("Filename: [%s]", filename);

	DownloadQueue* downloadQueue = DownloadQueue::Lock();

	// delete Download from active jobs
	for (ActiveDownloads::iterator it = m_activeDownloads.begin(); it != m_activeDownloads.end(); it++)
	{
		UrlDownloader* pa = *it;
		if (pa == urlDownloader)
		{
			m_activeDownloads.erase(it);
			break;
		}
	}
	nzbInfo->SetActiveDownloads(0);

	bool retry = urlDownloader->GetStatus() == WebDownloader::adRetry && !nzbInfo->GetDeleting();

	if (nzbInfo->GetDeleting())
	{
		nzbInfo->SetDeleteStatus(NzbInfo::dsManual);
		nzbInfo->SetUrlStatus(NzbInfo::lsNone);
		nzbInfo->SetDeleting(false);
	}
	else if (urlDownloader->GetStatus() == WebDownloader::adFinished)
	{
		nzbInfo->SetUrlStatus(NzbInfo::lsFinished);
	}
	else if (urlDownloader->GetStatus() == WebDownloader::adFailed)
	{
		nzbInfo->SetUrlStatus(NzbInfo::lsFailed);
	}
	else if (urlDownloader->GetStatus() == WebDownloader::adRetry)
	{
		nzbInfo->SetUrlStatus(NzbInfo::lsNone);
	}

	if (!retry)
	{
		DownloadQueue::Aspect aspect = { DownloadQueue::eaUrlCompleted, downloadQueue, nzbInfo, NULL };
		downloadQueue->Notify(&aspect);
	}

	DownloadQueue::Unlock();

	if (retry)
	{
		return;
	}

	if (nzbInfo->GetUrlStatus() == NzbInfo::lsFinished)
	{
		// add nzb-file to download queue
		Scanner::EAddStatus addStatus = g_Scanner->AddExternalFile(
			!Util::EmptyStr(nzbInfo->GetFilename()) ? nzbInfo->GetFilename() : filename,
			!Util::EmptyStr(nzbInfo->GetCategory()) ? nzbInfo->GetCategory() : urlDownloader->GetCategory(),
			nzbInfo->GetPriority(), nzbInfo->GetDupeKey(), nzbInfo->GetDupeScore(), nzbInfo->GetDupeMode(),
			nzbInfo->GetParameters(), false, nzbInfo->GetAddUrlPaused(), nzbInfo,
			urlDownloader->GetOutputFilename(), NULL, 0, NULL);

		if (addStatus == Scanner::asSuccess)
		{
			// if scanner has successfully added nzb-file to queue, our pNZBInfo is
			// already removed from queue and destroyed
			return;
		}

		nzbInfo->SetUrlStatus(addStatus == Scanner::asFailed ? NzbInfo::lsScanFailed : NzbInfo::lsScanSkipped);
	}

	// the rest of function is only for failed URLs or for failed scans

	g_QueueScriptCoordinator->EnqueueScript(nzbInfo, QueueScriptCoordinator::qeUrlCompleted);

	downloadQueue = DownloadQueue::Lock();

	// delete URL from queue
	downloadQueue->GetQueue()->Remove(nzbInfo);
	bool deleteObj = true;

	// add failed URL to history
	if (g_Options->GetKeepHistory() > 0 &&
		nzbInfo->GetUrlStatus() != NzbInfo::lsFinished &&
		!nzbInfo->GetAvoidHistory())
	{
		HistoryInfo* historyInfo = new HistoryInfo(nzbInfo);
		historyInfo->SetTime(time(NULL));
		downloadQueue->GetHistory()->push_front(historyInfo);
		deleteObj = false;
	}

	downloadQueue->Save();

	DownloadQueue::Unlock();

	if (deleteObj)
	{
		g_DiskState->DiscardFiles(nzbInfo);
		delete nzbInfo;
	}
}

bool UrlCoordinator::DeleteQueueEntry(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, bool avoidHistory)
{
	if (nzbInfo->GetActiveDownloads() > 0)
	{
		info("Deleting active URL %s", nzbInfo->GetName());
		nzbInfo->SetDeleting(true);
		nzbInfo->SetAvoidHistory(avoidHistory);

		for (ActiveDownloads::iterator it = m_activeDownloads.begin(); it != m_activeDownloads.end(); it++)
		{
			UrlDownloader* urlDownloader = *it;
			if (urlDownloader->GetNzbInfo() == nzbInfo)
			{
				urlDownloader->Stop();
				return true;
			}
		}
	}

	info("Deleting URL %s", nzbInfo->GetName());

	nzbInfo->SetDeleteStatus(NzbInfo::dsManual);
	nzbInfo->SetUrlStatus(NzbInfo::lsNone);

	downloadQueue->GetQueue()->Remove(nzbInfo);
	if (g_Options->GetKeepHistory() > 0 && !avoidHistory)
	{
		HistoryInfo* historyInfo = new HistoryInfo(nzbInfo);
		historyInfo->SetTime(time(NULL));
		downloadQueue->GetHistory()->push_front(historyInfo);
	}
	else
	{
		g_DiskState->DiscardFiles(nzbInfo);
		delete nzbInfo;
	}

	return true;
}
