/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2013-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "FeedCoordinator.h"
#include "Options.h"
#include "WebDownloader.h"
#include "Util.h"
#include "FileSystem.h"
#include "FeedFile.h"
#include "FeedFilter.h"
#include "FeedScript.h"
#include "DiskState.h"
#include "DupeCoordinator.h"

FeedCoordinator::FeedCacheItem::FeedCacheItem(const char* url, int cacheTimeSec,const char* cacheId,
	time_t lastUsage, FeedItemInfos* feedItemInfos)
{
	m_url = url;
	m_cacheTimeSec = cacheTimeSec;
	m_cacheId = cacheId;
	m_lastUsage = lastUsage;
	m_feedItemInfos = feedItemInfos;
	m_feedItemInfos->Retain();
}

FeedCoordinator::FeedCacheItem::~FeedCacheItem()
{
	m_feedItemInfos->Release();
}

FeedCoordinator::FilterHelper::FilterHelper()
{
	m_seasonEpisodeRegEx = nullptr;
}

FeedCoordinator::FilterHelper::~FilterHelper()
{
	delete m_seasonEpisodeRegEx;
}

void FeedCoordinator::FilterHelper::CalcDupeStatus(const char* title, const char* dupeKey, char* statusBuf, int bufLen)
{
	const char* dupeStatusName[] = { "", "QUEUED", "DOWNLOADING", "3", "SUCCESS", "5", "6", "7", "WARNING",
		"9", "10", "11", "12", "13", "14", "15", "FAILURE" };

	DownloadQueue* downloadQueue = DownloadQueue::Lock();
	DupeCoordinator::EDupeStatus dupeStatus = g_DupeCoordinator->GetDupeStatus(downloadQueue, title, dupeKey);
	DownloadQueue::Unlock();

	BString<1024> statuses;
	for (int i = 1; i <= (int)DupeCoordinator::dsFailure; i = i << 1)
	{
		if (dupeStatus & i)
		{
			if (!statuses.Empty())
			{
				statuses.Append(",");
			}
			statuses.Append(dupeStatusName[i]);
		}
	}

	strncpy(statusBuf, statuses, bufLen);
}

FeedCoordinator::FeedCoordinator()
{
	debug("Creating FeedCoordinator");
	m_force = false;
	m_save = false;

	g_Log->RegisterDebuggable(this);

	m_downloadQueueObserver.m_owner = this;
	DownloadQueue* downloadQueue = DownloadQueue::Lock();
	downloadQueue->Attach(&m_downloadQueueObserver);
	DownloadQueue::Unlock();
}

FeedCoordinator::~FeedCoordinator()
{
	debug("Destroying FeedCoordinator");
	// Cleanup

	g_Log->UnregisterDebuggable(this);

	debug("Deleting FeedDownloaders");
	for (FeedDownloader* feedDownloader : m_activeDownloads)
	{
		delete feedDownloader;
	}
	m_activeDownloads.clear();

	debug("Deleting Feeds");
	for (FeedInfo* feedInfo : m_feeds)
	{
		delete feedInfo;
	}
	m_feeds.clear();

	debug("Deleting FeedCache");
	m_feedCache.clear();

	debug("FeedCoordinator destroyed");
}

void FeedCoordinator::AddFeed(FeedInfo* feedInfo)
{
	m_feeds.push_back(feedInfo);
}

void FeedCoordinator::Run()
{
	debug("Entering FeedCoordinator-loop");

	while (!DownloadQueue::IsLoaded())
	{
		usleep(20 * 1000);
	}

	if (g_Options->GetServerMode() && g_Options->GetSaveQueue() && g_Options->GetReloadQueue())
	{
		m_downloadsMutex.Lock();
		g_DiskState->LoadFeeds(&m_feeds, &m_feedHistory);
		m_downloadsMutex.Unlock();
	}

	int sleepInterval = 100;
	int updateCounter = 0;
	int cleanupCounter = 60000;

	while (!IsStopped())
	{
		usleep(sleepInterval * 1000);

		updateCounter += sleepInterval;
		if (updateCounter >= 1000)
		{
			// this code should not be called too often, once per second is OK

			if (!g_Options->GetPauseDownload() || m_force || g_Options->GetUrlForce())
			{
				m_downloadsMutex.Lock();
				time_t current = Util::CurrentTime();
				if ((int)m_activeDownloads.size() < g_Options->GetUrlConnections())
				{
					m_force = false;
					// check feed list and update feeds
					for (FeedInfo* feedInfo : m_feeds)
					{
						if (((feedInfo->GetInterval() > 0 &&
							(current - feedInfo->GetLastUpdate() >= feedInfo->GetInterval() * 60 ||
							 current < feedInfo->GetLastUpdate())) ||
							feedInfo->GetFetch()) &&
							feedInfo->GetStatus() != FeedInfo::fsRunning)
						{
							StartFeedDownload(feedInfo, feedInfo->GetFetch());
						}
						else if (feedInfo->GetFetch())
						{
							m_force = true;
						}
					}
				}
				m_downloadsMutex.Unlock();
			}

			CheckSaveFeeds();
			ResetHangingDownloads();
			updateCounter = 0;
		}

		cleanupCounter += sleepInterval;
		if (cleanupCounter >= 60000)
		{
			// clean up feed history once a minute
			CleanupHistory();
			CleanupCache();
			CheckSaveFeeds();
			cleanupCounter = 0;
		}
	}

	// waiting for downloads
	debug("FeedCoordinator: waiting for Downloads to complete");
	bool completed = false;
	while (!completed)
	{
		m_downloadsMutex.Lock();
		completed = m_activeDownloads.size() == 0;
		m_downloadsMutex.Unlock();
		CheckSaveFeeds();
		usleep(100 * 1000);
		ResetHangingDownloads();
	}
	debug("FeedCoordinator: Downloads are completed");

	debug("Exiting FeedCoordinator-loop");
}

void FeedCoordinator::Stop()
{
	Thread::Stop();

	debug("Stopping UrlDownloads");
	m_downloadsMutex.Lock();
	for (FeedDownloader* feedDownloader : m_activeDownloads)
	{
		feedDownloader->Stop();
	}
	m_downloadsMutex.Unlock();
	debug("UrlDownloads are notified");
}

void FeedCoordinator::ResetHangingDownloads()
{
	const int timeout = g_Options->GetTerminateTimeout();
	if (timeout == 0)
	{
		return;
	}

	m_downloadsMutex.Lock();
	time_t tm = Util::CurrentTime();

	m_activeDownloads.erase(std::remove_if(m_activeDownloads.begin(), m_activeDownloads.end(),
		[timeout, tm](FeedDownloader* feedDownloader)
		{
			if (tm - feedDownloader->GetLastUpdateTime() > timeout &&
				feedDownloader->GetStatus() == FeedDownloader::adRunning)
			{
				debug("Terminating hanging download %s", feedDownloader->GetInfoName());
				if (feedDownloader->Terminate())
				{
					error("Terminated hanging download %s", feedDownloader->GetInfoName());
					feedDownloader->GetFeedInfo()->SetStatus(FeedInfo::fsUndefined);
				}
				else
				{
					error("Could not terminate hanging download %s", feedDownloader->GetInfoName());
				}

				// it's not safe to destroy feedDownloader, because the state of object is unknown
				delete feedDownloader;

				return true;
			}
			return false;
		}),
		m_activeDownloads.end());

	m_downloadsMutex.Unlock();
}

void FeedCoordinator::LogDebugInfo()
{
	info("   ---------- FeedCoordinator");

	m_downloadsMutex.Lock();
	info("    Active Downloads: %i", (int)m_activeDownloads.size());
	for (FeedDownloader* feedDownloader : m_activeDownloads)
	{
		feedDownloader->LogDebugInfo();
	}
	m_downloadsMutex.Unlock();
}

void FeedCoordinator::StartFeedDownload(FeedInfo* feedInfo, bool force)
{
	debug("Starting new FeedDownloader for %s", feedInfo->GetName());

	FeedDownloader* feedDownloader = new FeedDownloader();
	feedDownloader->SetAutoDestroy(true);
	feedDownloader->Attach(this);
	feedDownloader->SetFeedInfo(feedInfo);
	feedDownloader->SetUrl(feedInfo->GetUrl());
	if (strlen(feedInfo->GetName()) > 0)
	{
		feedDownloader->SetInfoName(feedInfo->GetName());
	}
	else
	{
		feedDownloader->SetInfoName(NzbInfo::MakeNiceUrlName(feedInfo->GetUrl(), ""));
	}
	feedDownloader->SetForce(force || g_Options->GetUrlForce());

	BString<1024> outFilename;
	if (feedInfo->GetId() > 0)
	{
		outFilename.Format("%sfeed-%i.tmp", g_Options->GetTempDir(), feedInfo->GetId());
	}
	else
	{
		outFilename.Format("%sfeed-%i-%i.tmp", g_Options->GetTempDir(), (int)Util::CurrentTime(), rand());
	}
	feedDownloader->SetOutputFilename(outFilename);

	feedInfo->SetStatus(FeedInfo::fsRunning);
	feedInfo->SetForce(force);
	feedInfo->SetFetch(false);

	m_activeDownloads.push_back(feedDownloader);
	feedDownloader->Start();
}

void FeedCoordinator::Update(Subject* caller, void* aspect)
{
	debug("Notification from FeedDownloader received");

	FeedDownloader* feedDownloader = (FeedDownloader*) caller;
	if ((feedDownloader->GetStatus() == WebDownloader::adFinished) ||
		(feedDownloader->GetStatus() == WebDownloader::adFailed) ||
		(feedDownloader->GetStatus() == WebDownloader::adRetry))
	{
		FeedCompleted(feedDownloader);
	}
}

void FeedCoordinator::FeedCompleted(FeedDownloader* feedDownloader)
{
	debug("Feed downloaded");

	FeedInfo* feedInfo = feedDownloader->GetFeedInfo();
	bool statusOK = feedDownloader->GetStatus() == WebDownloader::adFinished;
	if (statusOK)
	{
		feedInfo->SetOutputFilename(feedDownloader->GetOutputFilename());
	}

	// remove downloader from downloader list
	m_downloadsMutex.Lock();
	m_activeDownloads.erase(std::find(m_activeDownloads.begin(), m_activeDownloads.end(), feedDownloader));
	m_downloadsMutex.Unlock();

	if (statusOK)
	{
		if (!feedInfo->GetPreview())
		{
			FeedScriptController::ExecuteScripts(
				!Util::EmptyStr(feedInfo->GetFeedScript()) ? feedInfo->GetFeedScript(): g_Options->GetFeedScript(),
				feedInfo->GetOutputFilename(), feedInfo->GetId());
			FeedFile* feedFile = FeedFile::Create(feedInfo->GetOutputFilename());
			FileSystem::DeleteFile(feedInfo->GetOutputFilename());

			NzbList addedNzbs;

			m_downloadsMutex.Lock();
			if (feedFile)
			{
				ProcessFeed(feedInfo, feedFile->GetFeedItemInfos(), &addedNzbs);
				delete feedFile;
			}
			feedInfo->SetLastUpdate(Util::CurrentTime());
			feedInfo->SetForce(false);
			m_save = true;
			m_downloadsMutex.Unlock();

			DownloadQueue* downloadQueue = DownloadQueue::Lock();
			for (NzbInfo* nzbInfo : addedNzbs)
			{
				downloadQueue->GetQueue()->Add(nzbInfo, false);
			}
			downloadQueue->Save();
			DownloadQueue::Unlock();
		}
		feedInfo->SetStatus(FeedInfo::fsFinished);
	}
	else
	{
		feedInfo->SetStatus(FeedInfo::fsFailed);
	}
}

void FeedCoordinator::FilterFeed(FeedInfo* feedInfo, FeedItemInfos* feedItemInfos)
{
	debug("Filtering feed %s", feedInfo->GetName());

	FeedFilter* feedFilter = nullptr;
	if (feedInfo->GetFilter() && strlen(feedInfo->GetFilter()) > 0)
	{
		feedFilter = new FeedFilter(feedInfo->GetFilter());
	}

	for (FeedItemInfo& feedItemInfo : feedItemInfos)
	{
		feedItemInfo.SetMatchStatus(FeedItemInfo::msAccepted);
		feedItemInfo.SetMatchRule(0);
		feedItemInfo.SetPauseNzb(feedInfo->GetPauseNzb());
		feedItemInfo.SetPriority(feedInfo->GetPriority());
		feedItemInfo.SetAddCategory(feedInfo->GetCategory());
		feedItemInfo.SetDupeScore(0);
		feedItemInfo.SetDupeMode(dmScore);
		feedItemInfo.SetFeedFilterHelper(&m_filterHelper);
		feedItemInfo.BuildDupeKey(nullptr, nullptr, nullptr, nullptr);
		if (feedFilter)
		{
			feedFilter->Match(feedItemInfo);
		}
	}

	delete feedFilter;
}

void FeedCoordinator::ProcessFeed(FeedInfo* feedInfo, FeedItemInfos* feedItemInfos, NzbList* addedNzbs)
{
	debug("Process feed %s", feedInfo->GetName());

	FilterFeed(feedInfo, feedItemInfos);

	bool firstFetch = feedInfo->GetLastUpdate() == 0;
	int added = 0;

	for (FeedItemInfo& feedItemInfo : feedItemInfos)
	{
		if (feedItemInfo.GetMatchStatus() == FeedItemInfo::msAccepted)
		{
			FeedHistoryInfo* feedHistoryInfo = m_feedHistory.Find(feedItemInfo.GetUrl());
			FeedHistoryInfo::EStatus status = FeedHistoryInfo::hsUnknown;
			if (firstFetch && feedInfo->GetBacklog())
			{
				status = FeedHistoryInfo::hsBacklog;
			}
			else if (!feedHistoryInfo)
			{
				NzbInfo* nzbInfo = CreateNzbInfo(feedInfo, feedItemInfo);
				addedNzbs->Add(nzbInfo, false);
				status = FeedHistoryInfo::hsFetched;
				added++;
			}

			if (feedHistoryInfo)
			{
				feedHistoryInfo->SetLastSeen(Util::CurrentTime());
			}
			else
			{
				m_feedHistory.emplace_back(feedItemInfo.GetUrl(), status, Util::CurrentTime());
			}
		}
	}

	if (added)
	{
		info("%s has %i new item(s)", feedInfo->GetName(), added);
	}
	else
	{
		detail("%s has no new items", feedInfo->GetName());
	}
}

NzbInfo* FeedCoordinator::CreateNzbInfo(FeedInfo* feedInfo, FeedItemInfo& feedItemInfo)
{
	debug("Download %s from %s", feedItemInfo.GetUrl(), feedInfo->GetName());

	NzbInfo* nzbInfo = new NzbInfo();
	nzbInfo->SetKind(NzbInfo::nkUrl);
	nzbInfo->SetFeedId(feedInfo->GetId());
	nzbInfo->SetUrl(feedItemInfo.GetUrl());

	// add .nzb-extension if not present
	BString<1024> nzbName = feedItemInfo.GetFilename();
	char* ext = strrchr(nzbName, '.');
	if (ext && !strcasecmp(ext, ".nzb"))
	{
		*ext = '\0';
	}
	if (!nzbName.Empty())
	{
		BString<1024> nzbName2("%s.nzb", *nzbName);
		FileSystem::MakeValidFilename(nzbName2, '_', false);
		nzbInfo->SetFilename(nzbName2);
	}

	nzbInfo->SetCategory(feedItemInfo.GetAddCategory());
	nzbInfo->SetPriority(feedItemInfo.GetPriority());
	nzbInfo->SetAddUrlPaused(feedItemInfo.GetPauseNzb());
	nzbInfo->SetDupeKey(feedItemInfo.GetDupeKey());
	nzbInfo->SetDupeScore(feedItemInfo.GetDupeScore());
	nzbInfo->SetDupeMode(feedItemInfo.GetDupeMode());

	return nzbInfo;
}

bool FeedCoordinator::ViewFeed(int id, FeedItemInfos** ppFeedItemInfos)
{
	if (id < 1 || id > (int)m_feeds.size())
	{
		return false;
	}

	FeedInfo* feedInfo = m_feeds.at(id - 1);

	return PreviewFeed(feedInfo->GetId(), feedInfo->GetName(), feedInfo->GetUrl(), feedInfo->GetFilter(),
		feedInfo->GetBacklog(), feedInfo->GetPauseNzb(), feedInfo->GetCategory(),
		feedInfo->GetPriority(), feedInfo->GetInterval(), feedInfo->GetFeedScript(), 0, nullptr, ppFeedItemInfos);
}

bool FeedCoordinator::PreviewFeed(int id, const char* name, const char* url, const char* filter,
	bool backlog, bool pauseNzb, const char* category, int priority, int interval, const char* feedScript,
	int cacheTimeSec, const char* cacheId, FeedItemInfos** ppFeedItemInfos)
{
	debug("Preview feed %s", name);

	FeedInfo* feedInfo = new FeedInfo(id, name, url, backlog, interval,
		filter, pauseNzb, category, priority, feedScript);
	feedInfo->SetPreview(true);

	FeedItemInfos* feedItemInfos = nullptr;
	bool hasCache = false;
	if (cacheTimeSec > 0 && *cacheId != '\0')
	{
		m_downloadsMutex.Lock();
		for (FeedCacheItem& feedCacheItem : m_feedCache)
		{
			if (!strcmp(feedCacheItem.GetCacheId(), cacheId))
			{
				feedCacheItem.SetLastUsage(Util::CurrentTime());
				feedItemInfos = feedCacheItem.GetFeedItemInfos();
				feedItemInfos->Retain();
				hasCache = true;
				break;
			}
		}
		m_downloadsMutex.Unlock();
	}

	if (!hasCache)
	{
		m_downloadsMutex.Lock();

		bool firstFetch = true;
		for (FeedInfo* feedInfo2 : m_feeds)
		{
			if (!strcmp(feedInfo2->GetUrl(), feedInfo->GetUrl()) &&
				!strcmp(feedInfo2->GetFilter(), feedInfo->GetFilter()) &&
				feedInfo2->GetLastUpdate() > 0)
			{
				firstFetch = false;
				break;
			}
		}

		StartFeedDownload(feedInfo, true);
		m_downloadsMutex.Unlock();

		// wait until the download in a separate thread completes
		while (feedInfo->GetStatus() == FeedInfo::fsRunning)
		{
			usleep(100 * 1000);
		}

		// now can process the feed

		FeedFile* feedFile = nullptr;

		if (feedInfo->GetStatus() == FeedInfo::fsFinished)
		{
			FeedScriptController::ExecuteScripts(
				!Util::EmptyStr(feedInfo->GetFeedScript()) ? feedInfo->GetFeedScript(): g_Options->GetFeedScript(),
				feedInfo->GetOutputFilename(), feedInfo->GetId());
			feedFile = FeedFile::Create(feedInfo->GetOutputFilename());
		}

		FileSystem::DeleteFile(feedInfo->GetOutputFilename());

		if (!feedFile)
		{
			delete feedInfo;
			return false;
		}

		feedItemInfos = feedFile->GetFeedItemInfos();
		feedItemInfos->Retain();
		delete feedFile;

		for (FeedItemInfo& feedItemInfo : feedItemInfos)
		{
			feedItemInfo.SetStatus(firstFetch && feedInfo->GetBacklog() ? FeedItemInfo::isBacklog : FeedItemInfo::isNew);
			FeedHistoryInfo* feedHistoryInfo = m_feedHistory.Find(feedItemInfo.GetUrl());
			if (feedHistoryInfo)
			{
				feedItemInfo.SetStatus((FeedItemInfo::EStatus)feedHistoryInfo->GetStatus());
			}
		}
	}

	FilterFeed(feedInfo, feedItemInfos);
	delete feedInfo;

	if (cacheTimeSec > 0 && *cacheId != '\0' && !hasCache)
	{
		m_downloadsMutex.Lock();
		m_feedCache.emplace_back(url, cacheTimeSec, cacheId, Util::CurrentTime(), feedItemInfos);
		m_downloadsMutex.Unlock();
	}

	*ppFeedItemInfos = feedItemInfos;

	return true;
}

void FeedCoordinator::FetchFeed(int id)
{
	debug("FetchFeeds");

	m_downloadsMutex.Lock();
	for (FeedInfo* feedInfo : m_feeds)
	{
		if (feedInfo->GetId() == id || id == 0)
		{
			feedInfo->SetFetch(true);
			m_force = true;
		}
	}
	m_downloadsMutex.Unlock();
}

void FeedCoordinator::DownloadQueueUpdate(Subject* caller, void* aspect)
{
	debug("Notification from URL-Coordinator received");

	DownloadQueue::Aspect* queueAspect = (DownloadQueue::Aspect*)aspect;
	if (queueAspect->action == DownloadQueue::eaUrlCompleted)
	{
		m_downloadsMutex.Lock();
		FeedHistoryInfo* feedHistoryInfo = m_feedHistory.Find(queueAspect->nzbInfo->GetUrl());
		if (feedHistoryInfo)
		{
			feedHistoryInfo->SetStatus(FeedHistoryInfo::hsFetched);
		}
		else
		{
			m_feedHistory.emplace_back(queueAspect->nzbInfo->GetUrl(), FeedHistoryInfo::hsFetched, Util::CurrentTime());
		}
		m_save = true;
		m_downloadsMutex.Unlock();
	}
}

bool FeedCoordinator::HasActiveDownloads()
{
	m_downloadsMutex.Lock();
	bool active = !m_activeDownloads.empty();
	m_downloadsMutex.Unlock();
	return active;
}

void FeedCoordinator::CheckSaveFeeds()
{
	debug("CheckSaveFeeds");
	m_downloadsMutex.Lock();
	if (m_save)
	{
		if (g_Options->GetSaveQueue() && g_Options->GetServerMode())
		{
			g_DiskState->SaveFeeds(&m_feeds, &m_feedHistory);
		}
		m_save = false;
	}
	m_downloadsMutex.Unlock();
}

void FeedCoordinator::CleanupHistory()
{
	debug("CleanupHistory");

	m_downloadsMutex.Lock();

	time_t oldestUpdate = Util::CurrentTime();

	for (FeedInfo* feedInfo : m_feeds)
	{
		if (feedInfo->GetLastUpdate() < oldestUpdate)
		{
			oldestUpdate = feedInfo->GetLastUpdate();
		}
	}

	time_t borderDate = oldestUpdate - g_Options->GetFeedHistory() * 60*60*24;

	m_feedHistory.erase(std::remove_if(m_feedHistory.begin(), m_feedHistory.end(),
		[borderDate, this](FeedHistoryInfo& feedHistoryInfo)
		{
			if (feedHistoryInfo.GetLastSeen() < borderDate)
			{
				detail("Deleting %s from feed history", feedHistoryInfo.GetUrl());
				m_save = true;
				return true;
			}
			return false;
		}),
		m_feedHistory.end());

	m_downloadsMutex.Unlock();
}

void FeedCoordinator::CleanupCache()
{
	debug("CleanupCache");

	m_downloadsMutex.Lock();

	time_t curTime = Util::CurrentTime();

	m_feedCache.remove_if(
		[curTime](FeedCacheItem& feedCacheItem)
		{
			if (feedCacheItem.GetLastUsage() + feedCacheItem.GetCacheTimeSec() < curTime ||
				feedCacheItem.GetLastUsage() > curTime)
			{
				debug("Deleting %s from feed cache", feedCacheItem.GetUrl());
				return true;
			}
			return false;
		});

	m_downloadsMutex.Unlock();
}
