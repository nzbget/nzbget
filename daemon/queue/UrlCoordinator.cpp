/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2012-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "UrlCoordinator.h"
#include "Options.h"
#include "WorkState.h"
#include "WebDownloader.h"
#include "Util.h"
#include "FileSystem.h"
#include "NzbFile.h"
#include "Scanner.h"
#include "DiskState.h"
#include "QueueScript.h"

void UrlDownloader::ProcessHeader(const char* line)
{
	WebDownloader::ProcessHeader(line);

	if (!strncmp(line, "X-DNZB-Category:", 16))
	{
		m_category = Util::Trim(CString(line + 16));

		debug("Category: %s", *m_category);
	}
	else if (!strncmp(line, "X-DNZB-", 7))
	{
		CString modLine = line;
		char* value = strchr(modLine, ':');
		if (value)
		{
			*value = '\0';
			value++;
			while (*value == ' ') value++;
			Util::Trim(value);

			debug("X-DNZB: %s", *modLine);
			debug("Value: %s", value);

			BString<100> paramName("*DNZB:%s", modLine + 7);
			CString paramValue = WebUtil::Latin1ToUtf8(value);
			m_nzbInfo->GetParameters()->SetParameter(paramName, paramValue);
		}
	}
}

UrlCoordinator::UrlCoordinator()
{
	m_downloadQueueObserver.m_owner = this;
	DownloadQueue::Guard()->Attach(&m_downloadQueueObserver);
}

UrlCoordinator::~UrlCoordinator()
{
	debug("Destroying UrlCoordinator");

	for (UrlDownloader* urlDownloader : m_activeDownloads)
	{
		delete urlDownloader;
	}
	m_activeDownloads.clear();

	debug("UrlCoordinator destroyed");
}

void UrlCoordinator::Run()
{
	debug("Entering UrlCoordinator-loop");

	time_t lastReset = 0;

	while (!DownloadQueue::IsLoaded())
	{
		Util::Sleep(20);
	}

	while (!IsStopped())
	{
		bool downloadStarted = false;

		{
			NzbInfo* nzbInfo = nullptr;
			GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
			if ((int)m_activeDownloads.size() < g_Options->GetUrlConnections())
			{
				nzbInfo = GetNextUrl(downloadQueue);
				if (nzbInfo && (!g_WorkState->GetPauseDownload() || g_Options->GetUrlForce()))
				{
					StartUrlDownload(nzbInfo);
					downloadStarted = true;
				}
			}
			m_hasMoreJobs = !m_activeDownloads.empty() || nzbInfo;
		}

		if (lastReset != Util::CurrentTime())
		{
			// this code should not be called too often, once per second is OK
			ResetHangingDownloads();
			lastReset = Util::CurrentTime();
		}

		if (!m_hasMoreJobs && !IsStopped())
		{
			Guard guard(m_waitMutex);
			m_waitCond.Wait(m_waitMutex, [&] { return m_hasMoreJobs || IsStopped(); });
		}
		else
		{
			int sleepInterval = downloadStarted ? 0 : 100;
			Util::Sleep(sleepInterval);
		}
	}

	WaitJobs();

	debug("Exiting UrlCoordinator-loop");
}

void UrlCoordinator::WaitJobs()
{
	// waiting for downloads
	debug("UrlCoordinator: waiting for Downloads to complete");

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

	debug("UrlCoordinator: Downloads are completed");
}

void UrlCoordinator::Stop()
{
	Thread::Stop();

	debug("Stopping UrlDownloads");
	{
		GuardedDownloadQueue guard = DownloadQueue::Guard();
		for (UrlDownloader* urlDownloader : m_activeDownloads)
		{
			urlDownloader->Stop();
		}
	}
	debug("UrlDownloads are notified");

	// Resume Run() to exit it
	Guard guard(m_waitMutex);
	m_waitCond.NotifyAll();
}

void UrlCoordinator::DownloadQueueUpdate(Subject* caller, void* aspect)
{
	debug("Notification from download queue received");

	DownloadQueue::Aspect* queueAspect = (DownloadQueue::Aspect*)aspect;
	if (queueAspect->action == DownloadQueue::eaUrlAdded ||
		queueAspect->action == DownloadQueue::eaUrlReturned)
	{
		// Resume Run()
		Guard guard(m_waitMutex);
		m_hasMoreJobs = true;
		m_waitCond.NotifyAll();
	}
}

void UrlCoordinator::ResetHangingDownloads()
{
	if (g_Options->GetUrlTimeout() == 0)
	{
		return;
	}

	GuardedDownloadQueue guard = DownloadQueue::Guard();
	time_t tm = Util::CurrentTime();

	for (UrlDownloader* urlDownloader: m_activeDownloads)
	{
		if (tm - urlDownloader->GetLastUpdateTime() > g_Options->GetUrlTimeout() + 10 &&
			urlDownloader->GetStatus() == UrlDownloader::adRunning)
		{
			error("Cancelling hanging url download %s", urlDownloader->GetInfoName());
			urlDownloader->Stop();
		}
	}
}

void UrlCoordinator::LogDebugInfo()
{
	info("   ---------- UrlCoordinator");

	GuardedDownloadQueue guard = DownloadQueue::Guard();
	info("    Active Downloads: %i", (int)m_activeDownloads.size());
	for (UrlDownloader* urlDownloader : m_activeDownloads)
	{
		urlDownloader->LogDebugInfo();
	}
}

/*
 * Returns next URL for download.
 */
NzbInfo* UrlCoordinator::GetNextUrl(DownloadQueue* downloadQueue)
{
	NzbInfo* nzbInfo = nullptr;

	for (NzbInfo* nzbInfo1 : downloadQueue->GetQueue())
	{
		if (nzbInfo1->GetKind() == NzbInfo::nkUrl &&
			nzbInfo1->GetUrlStatus() == NzbInfo::lsNone &&
			nzbInfo1->GetDeleteStatus() == NzbInfo::dsNone &&
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
	urlDownloader->SetInfoName(nzbInfo->MakeNiceUrlName(nzbInfo->GetUrl(), nzbInfo->GetFilename()));
	urlDownloader->SetOutputFilename(BString<1024>("%s%curl-%i.tmp",
		g_Options->GetTempDir(), PATH_SEPARATOR, nzbInfo->GetId()));

	nzbInfo->SetActiveDownloads(1);
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

	const char* origname;
	if (urlDownloader->GetOriginalFilename())
	{
		origname = urlDownloader->GetOriginalFilename();
	}
	else
	{
		origname = FileSystem::BaseFileName(nzbInfo->GetUrl());

		// TODO: decode URL escaping
	}

	CString filename = FileSystem::MakeValidFilename(origname);

	debug("Filename: [%s]", *filename);

	bool retry;

	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

		// remove downloader from downloader list
		m_activeDownloads.erase(std::find(m_activeDownloads.begin(), m_activeDownloads.end(), urlDownloader));

		retry = urlDownloader->GetStatus() == WebDownloader::adRetry && !nzbInfo->GetDeleting();

		if (nzbInfo->GetDeleting())
		{
			nzbInfo->SetDeleteStatus(nzbInfo->GetDeleteStatus() == NzbInfo::dsNone ? NzbInfo::dsManual : nzbInfo->GetDeleteStatus());
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
			DownloadQueue::Aspect aspect = {DownloadQueue::eaUrlCompleted, downloadQueue, nzbInfo, nullptr};
			downloadQueue->Notify(&aspect);
		}
	}

	if (retry)
	{
		nzbInfo->SetActiveDownloads(0);
		return;
	}

	if (nzbInfo->GetUrlStatus() == NzbInfo::lsFinished)
	{
		// add nzb-file to download queue
		Scanner::EAddStatus addStatus = g_Scanner->AddExternalFile(
			!Util::EmptyStr(nzbInfo->GetFilename()) ? nzbInfo->GetFilename() : *filename,
			!Util::EmptyStr(nzbInfo->GetCategory()) ? nzbInfo->GetCategory() : urlDownloader->GetCategory(),
			nzbInfo->GetPriority(), nzbInfo->GetDupeKey(), nzbInfo->GetDupeScore(), nzbInfo->GetDupeMode(),
			nzbInfo->GetParameters(), false, nzbInfo->GetAddUrlPaused(), nzbInfo,
			urlDownloader->GetOutputFilename(), nullptr, 0, nullptr);

		if (addStatus == Scanner::asSuccess)
		{
			// if scanner has successfully added nzb-file to queue, our nzbInfo is
			// already removed from queue and destroyed
			return;
		}

		nzbInfo->SetUrlStatus(addStatus == Scanner::asFailed ? NzbInfo::lsScanFailed : NzbInfo::lsScanSkipped);
	}

	// the rest of function is only for failed URLs or for failed scans

	g_QueueScriptCoordinator->EnqueueScript(nzbInfo, QueueScriptCoordinator::qeUrlCompleted);

	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

		nzbInfo->SetActiveDownloads(0);

		DownloadQueue::Aspect aspect = {DownloadQueue::eaUrlFailed, downloadQueue, nzbInfo, nullptr};
		downloadQueue->Notify(&aspect);
	}
}

bool UrlCoordinator::DeleteQueueEntry(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, bool avoidHistory)
{
	if (nzbInfo->GetActiveDownloads() > 0)
	{
		info("Deleting active URL %s", nzbInfo->GetName());
		nzbInfo->SetDeleting(true);
		nzbInfo->SetAvoidHistory(avoidHistory);

		for (UrlDownloader* urlDownloader : m_activeDownloads)
		{
			if (urlDownloader->GetNzbInfo() == nzbInfo)
			{
				urlDownloader->Stop();
				return true;
			}
		}

		return false;
	}

	info("Deleting URL %s", nzbInfo->GetName());

	nzbInfo->SetDeleteStatus(nzbInfo->GetDeleteStatus() == NzbInfo::dsNone ? NzbInfo::dsManual : nzbInfo->GetDeleteStatus());
	nzbInfo->SetUrlStatus(NzbInfo::lsNone);

	DownloadQueue::Aspect deletedAspect = {DownloadQueue::eaUrlDeleted, downloadQueue, nzbInfo, nullptr};
	downloadQueue->Notify(&deletedAspect);

	return true;
}

void UrlCoordinator::AddUrlToQueue(std::unique_ptr<NzbInfo> nzbInfo, bool addFirst)
{
	debug("Adding URL to queue");
										
	NzbInfo* addedNzb = nzbInfo.get();

	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	DownloadQueue::Aspect foundAspect = {DownloadQueue::eaUrlFound, downloadQueue, addedNzb, nullptr};
	downloadQueue->Notify(&foundAspect);

	if (addedNzb->GetDeleteStatus() != NzbInfo::dsManual)
	{
		downloadQueue->GetQueue()->Add(std::move(nzbInfo), addFirst);

		DownloadQueue::Aspect addedAspect = {DownloadQueue::eaUrlAdded, downloadQueue, addedNzb, nullptr};
		downloadQueue->Notify(&addedAspect);
	}

	downloadQueue->Save();
}

