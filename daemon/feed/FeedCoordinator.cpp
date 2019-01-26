/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2013-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "FeedCoordinator.h"
#include "Options.h"
#include "WorkState.h"
#include "WebDownloader.h"
#include "Util.h"
#include "FileSystem.h"
#include "FeedFile.h"
#include "FeedFilter.h"
#include "FeedScript.h"
#include "DiskState.h"
#include "DupeCoordinator.h"
#include "UrlCoordinator.h"

std::unique_ptr<RegEx>& FeedCoordinator::FilterHelper::GetRegEx(int id)
{
	m_regExes.resize(id);
	return m_regExes[id - 1];
}

void FeedCoordinator::FilterHelper::CalcDupeStatus(const char* title, const char* dupeKey, char* statusBuf, int bufLen)
{
	const char* dupeStatusName[] = { "", "QUEUED", "DOWNLOADING", "3", "SUCCESS", "5", "6", "7", "WARNING",
		"9", "10", "11", "12", "13", "14", "15", "FAILURE" };

	DupeCoordinator::EDupeStatus dupeStatus = g_DupeCoordinator->GetDupeStatus(DownloadQueue::Guard(), title, dupeKey);

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

	m_downloadQueueObserver.m_owner = this;
	DownloadQueue::Guard()->Attach(&m_downloadQueueObserver);

	m_workStateObserver.m_owner = this;
	g_WorkState->Attach(&m_workStateObserver);
}

FeedCoordinator::~FeedCoordinator()
{
	debug("Destroying FeedCoordinator");

	for (FeedDownloader* feedDownloader : m_activeDownloads)
	{
		delete feedDownloader;
	}
	m_activeDownloads.clear();
}

void FeedCoordinator::Run()
{
	debug("Entering FeedCoordinator-loop");

	while (!DownloadQueue::IsLoaded())
	{
		Util::Sleep(20);
	}

	if (g_Options->GetServerMode())
	{
		Guard guard(m_downloadsMutex);
		g_DiskState->LoadFeeds(&m_feeds, &m_feedHistory);
	}

	time_t lastCleanup = 0;
	while (!IsStopped())
	{
		// this code should not be called too often, once per second is OK
		if (!g_WorkState->GetPauseDownload() || m_force || g_Options->GetUrlForce())
		{
			Guard guard(m_downloadsMutex);

			time_t current = Util::CurrentTime();
			if ((int)m_activeDownloads.size() < g_Options->GetUrlConnections())
			{
				m_force = false;
				// check feed list and update feeds
				for (FeedInfo* feedInfo : &m_feeds)
				{
					if (((feedInfo->GetInterval() > 0 &&
						 (feedInfo->GetNextUpdate() == 0 ||
						  current >= feedInfo->GetNextUpdate() ||
						  current < feedInfo->GetNextUpdate() - feedInfo->GetInterval() * 60)) ||
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
		}

		CheckSaveFeeds();
		ResetHangingDownloads();

		if (std::abs(Util::CurrentTime() - lastCleanup) >= 60)
		{
			// clean up feed history once a minute
			CleanupHistory();
			CleanupCache();
			CheckSaveFeeds();
			lastCleanup = Util::CurrentTime();
		}

		Guard guard(m_downloadsMutex);
		if (m_force)
		{
			// don't sleep too long if there active feeds scheduled for redownload
			m_waitCond.WaitFor(m_downloadsMutex, 1000, [&]{ return IsStopped(); });
		}
		else
		{
			// no active jobs, we can sleep longer:
			//  - if option "UrlForce" is active or if the feed list is empty we need to wake up
			//    only when a new feed preview is requested. We could wait indefinitely for that
			//    but we need to do some job every now and then and therefore we sleep only 60 seconds.
			//  - if option "UrlForce" is disabled we need also to wake up when state "DownloadPaused"
			//    is changed. We detect this via notification from 'WorkState'. However such
			//    notifications are not 100% reliable due to possible race conditions. Therefore
			//    we sleep for max. 5 seconds.
			int waitInterval = g_Options->GetUrlForce() || m_feeds.empty() ? 60000 : 5000;
			m_waitCond.WaitFor(m_downloadsMutex, waitInterval, [&]{ return m_force || IsStopped(); });
		}
	}

	// waiting for downloads
	debug("FeedCoordinator: waiting for Downloads to complete");
	bool completed = false;
	while (!completed)
	{
		{
			Guard guard(m_downloadsMutex);
			completed = m_activeDownloads.size() == 0;
		}
		CheckSaveFeeds();
		Util::Sleep(100);
		ResetHangingDownloads();
	}
	debug("FeedCoordinator: Downloads are completed");

	debug("Exiting FeedCoordinator-loop");
}

void FeedCoordinator::Stop()
{
	Thread::Stop();

	debug("Stopping UrlDownloads");
	Guard guard(m_downloadsMutex);
	for (FeedDownloader* feedDownloader : m_activeDownloads)
	{
		feedDownloader->Stop();
	}
	debug("UrlDownloads are notified");

	// Resume Run() to exit it
	m_waitCond.NotifyAll();
}

void FeedCoordinator::WorkStateUpdate(Subject* caller, void* aspect)
{
	m_force = true;
	m_waitCond.NotifyAll();
}

void FeedCoordinator::ResetHangingDownloads()
{
	if (g_Options->GetUrlTimeout() == 0)
	{
		return;
	}

	Guard guard(m_downloadsMutex);
	time_t tm = Util::CurrentTime();

	for (FeedDownloader* feedDownloader: m_activeDownloads)
	{
		if (tm - feedDownloader->GetLastUpdateTime() > g_Options->GetUrlTimeout() + 10 &&
			feedDownloader->GetStatus() == FeedDownloader::adRunning)
		{
			error("Cancelling hanging feed download %s", feedDownloader->GetInfoName());
			feedDownloader->Stop();
		}
	}
}

void FeedCoordinator::LogDebugInfo()
{
	info("   ---------- FeedCoordinator");

	Guard guard(m_downloadsMutex);
	info("    Active Downloads: %i", (int)m_activeDownloads.size());
	for (FeedDownloader* feedDownloader : m_activeDownloads)
	{
		feedDownloader->LogDebugInfo();
	}
}

void FeedCoordinator::StartFeedDownload(FeedInfo* feedInfo, bool force)
{
	debug("Starting new FeedDownloader for %s", feedInfo->GetName());

	FeedDownloader* feedDownloader = new FeedDownloader();
	feedDownloader->SetAutoDestroy(true);
	feedDownloader->Attach(this);
	feedDownloader->SetFeedInfo(feedInfo);
	feedDownloader->SetUrl(feedInfo->GetUrl());
	feedDownloader->SetInfoName(feedInfo->GetName());
	feedDownloader->SetForce(force || g_Options->GetUrlForce());

	BString<1024> outFilename;
	if (feedInfo->GetId() > 0)
	{
		outFilename.Format("%s%cfeed-%i.tmp", g_Options->GetTempDir(), PATH_SEPARATOR, feedInfo->GetId());
	}
	else
	{
		outFilename.Format("%s%cfeed-%i-%i.tmp", g_Options->GetTempDir(), PATH_SEPARATOR, (int)Util::CurrentTime(), rand());
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
	{
		Guard guard(m_downloadsMutex);
		m_activeDownloads.erase(std::find(m_activeDownloads.begin(), m_activeDownloads.end(), feedDownloader));
	}

	SchedulerNextUpdate(feedInfo, statusOK);

	if (statusOK)
	{
		if (!feedInfo->GetPreview())
		{
			bool scriptSuccess = true;
			FeedScriptController::ExecuteScripts(
				!Util::EmptyStr(feedInfo->GetExtensions()) ? feedInfo->GetExtensions(): g_Options->GetExtensions(),
				feedInfo->GetOutputFilename(), feedInfo->GetId(), &scriptSuccess);
			if (!scriptSuccess)
			{
				feedInfo->SetStatus(FeedInfo::fsFailed);
				return;
			}

			std::unique_ptr<FeedFile> feedFile = parseFeed(feedInfo);

			std::vector<std::unique_ptr<NzbInfo>> addedNzbs;

			{
				Guard guard(m_downloadsMutex);
				if (feedFile)
				{
					std::unique_ptr<FeedItemList> feedItems = feedFile->DetachFeedItems();
					addedNzbs = ProcessFeed(feedInfo, feedItems.get());
					feedFile.reset();
				}
				feedInfo->SetLastUpdate(Util::CurrentTime());
				feedInfo->SetForce(false);
				m_save = true;
			}

			for (std::unique_ptr<NzbInfo>& nzbInfo : addedNzbs)
			{
				g_UrlCoordinator->AddUrlToQueue(std::move(nzbInfo), false);
			}
		}
		feedInfo->SetStatus(FeedInfo::fsFinished);
	}
	else
	{
		feedInfo->SetStatus(FeedInfo::fsFailed);
	}
}

void FeedCoordinator::SchedulerNextUpdate(FeedInfo* feedInfo, bool success)
{
	time_t current = Util::CurrentTime();
	int interval;

	if (success)
	{
		interval = feedInfo->GetInterval() * 60;
		feedInfo->SetLastInterval(0);
	}
	else
	{
		// On failure schedule next update sooner:
		// starting with 1 minute and increasing, but not greater than FeedX.Interval
		interval = feedInfo->GetLastInterval() * 2;
		interval = std::max(interval, 60);
		interval = std::min(interval, feedInfo->GetInterval() * 60);
		feedInfo->SetLastInterval(interval);
	}

	detail("Scheduling update for feed %s in %i minute(s)", feedInfo->GetName(), interval / 60);
	feedInfo->SetNextUpdate(current + interval);
}

void FeedCoordinator::FilterFeed(FeedInfo* feedInfo, FeedItemList* feedItems)
{
	debug("Filtering feed %s", feedInfo->GetName());

	FilterHelper filterHelper;
	std::unique_ptr<FeedFilter> feedFilter;

	if (!Util::EmptyStr(feedInfo->GetFilter()))
	{
		feedFilter = std::make_unique<FeedFilter>(feedInfo->GetFilter());
	}

	for (FeedItemInfo& feedItemInfo : feedItems)
	{
		feedItemInfo.SetMatchStatus(FeedItemInfo::msAccepted);
		feedItemInfo.SetMatchRule(0);
		feedItemInfo.SetPauseNzb(feedInfo->GetPauseNzb());
		feedItemInfo.SetPriority(feedInfo->GetPriority());
		feedItemInfo.SetAddCategory(feedInfo->GetCategory());
		feedItemInfo.SetDupeScore(0);
		feedItemInfo.SetDupeMode(dmScore);
		feedItemInfo.SetFeedFilterHelper(&filterHelper);
		feedItemInfo.BuildDupeKey(nullptr, nullptr, nullptr, nullptr);
		if (feedFilter)
		{
			feedFilter->Match(feedItemInfo);
		}
	}
}

std::vector<std::unique_ptr<NzbInfo>> FeedCoordinator::ProcessFeed(FeedInfo* feedInfo, FeedItemList* feedItems)
{
	debug("Process feed %s", feedInfo->GetName());

	FilterFeed(feedInfo, feedItems);

	std::vector<std::unique_ptr<NzbInfo>> addedNzbs;
	bool firstFetch = feedInfo->GetLastUpdate() == 0;
	int added = 0;

	for (FeedItemInfo& feedItemInfo : feedItems)
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
				addedNzbs.push_back(CreateNzbInfo(feedInfo, feedItemInfo));
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

	return addedNzbs;
}

std::unique_ptr<NzbInfo> FeedCoordinator::CreateNzbInfo(FeedInfo* feedInfo, FeedItemInfo& feedItemInfo)
{
	debug("Download %s from %s", feedItemInfo.GetUrl(), feedInfo->GetName());

	std::unique_ptr<NzbInfo> nzbInfo = std::make_unique<NzbInfo>();
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
		nzbInfo->SetFilename(FileSystem::MakeValidFilename(nzbName2));
	}

	nzbInfo->SetCategory(feedItemInfo.GetAddCategory());
	nzbInfo->SetPriority(feedItemInfo.GetPriority());
	nzbInfo->SetAddUrlPaused(feedItemInfo.GetPauseNzb());
	nzbInfo->SetDupeKey(feedItemInfo.GetDupeKey());
	nzbInfo->SetDupeScore(feedItemInfo.GetDupeScore());
	nzbInfo->SetDupeMode(feedItemInfo.GetDupeMode());
	nzbInfo->SetSize(feedItemInfo.GetSize());
	nzbInfo->SetMinTime(feedItemInfo.GetTime());
	nzbInfo->SetMaxTime(feedItemInfo.GetTime());

	return nzbInfo;
}

std::shared_ptr<FeedItemList> FeedCoordinator::ViewFeed(int id)
{
	if (id < 1 || id > (int)m_feeds.size())
	{
		return nullptr;
	}

	std::unique_ptr<FeedInfo>& feedInfo = m_feeds[id - 1];

	return PreviewFeed(feedInfo->GetId(), feedInfo->GetName(), feedInfo->GetUrl(), feedInfo->GetFilter(),
		feedInfo->GetBacklog(), feedInfo->GetPauseNzb(), feedInfo->GetCategory(),
		feedInfo->GetPriority(), feedInfo->GetInterval(), feedInfo->GetExtensions(), 0, nullptr);
}

std::shared_ptr<FeedItemList> FeedCoordinator::PreviewFeed(int id,
	const char* name, const char* url, const char* filter, bool backlog, bool pauseNzb,
	const char* category, int priority, int interval, const char* feedScript,
	int cacheTimeSec, const char* cacheId)
{
	debug("Preview feed %s", name);

	std::unique_ptr<FeedInfo> feedInfo = std::make_unique<FeedInfo>(id, name, url, backlog, interval,
		filter, pauseNzb, category, priority, feedScript);
	feedInfo->SetPreview(true);

	std::shared_ptr<FeedItemList> feedItems;
	bool hasCache = false;
	if (cacheTimeSec > 0 && *cacheId != '\0')
	{
		Guard guard(m_downloadsMutex);
		for (FeedCacheItem& feedCacheItem : m_feedCache)
		{
			if (!strcmp(feedCacheItem.GetCacheId(), cacheId))
			{
				feedCacheItem.SetLastUsage(Util::CurrentTime());
				feedItems = feedCacheItem.GetFeedItems();
				hasCache = true;
				break;
			}
		}
	}

	if (!hasCache)
	{
		bool firstFetch = true;

		{
			Guard guard(m_downloadsMutex);

			for (FeedInfo* feedInfo2 : &m_feeds)
			{
				if (!strcmp(feedInfo2->GetUrl(), feedInfo->GetUrl()) &&
					!strcmp(feedInfo2->GetFilter(), feedInfo->GetFilter()) &&
					feedInfo2->GetLastUpdate() > 0)
				{
					firstFetch = false;
					break;
				}
			}

			StartFeedDownload(feedInfo.get(), true);

			m_force = true;
			m_waitCond.NotifyAll();
		}

		// wait until the download in a separate thread completes
		while (feedInfo->GetStatus() == FeedInfo::fsRunning)
		{
			Util::Sleep(100);
		}

		// now can process the feed

		if (feedInfo->GetStatus() != FeedInfo::fsFinished)
		{
			return nullptr;
		}

		FeedScriptController::ExecuteScripts(
			!Util::EmptyStr(feedInfo->GetExtensions()) ? feedInfo->GetExtensions(): g_Options->GetExtensions(),
			feedInfo->GetOutputFilename(), feedInfo->GetId(), nullptr);

		std::unique_ptr<FeedFile> feedFile = parseFeed(feedInfo.get());
		if (!feedFile)
		{
			return nullptr;
		}

		feedItems = feedFile->DetachFeedItems();
		feedFile.reset();

		for (FeedItemInfo& feedItemInfo : feedItems.get())
		{
			feedItemInfo.SetStatus(firstFetch && feedInfo->GetBacklog() ? FeedItemInfo::isBacklog : FeedItemInfo::isNew);
			FeedHistoryInfo* feedHistoryInfo = m_feedHistory.Find(feedItemInfo.GetUrl());
			if (feedHistoryInfo)
			{
				feedItemInfo.SetStatus((FeedItemInfo::EStatus)feedHistoryInfo->GetStatus());
			}
		}
	}

	FilterFeed(feedInfo.get(), feedItems.get());
	feedInfo.reset();

	if (cacheTimeSec > 0 && *cacheId != '\0' && !hasCache)
	{
		Guard guard(m_downloadsMutex);
		m_feedCache.emplace_back(url, cacheTimeSec, cacheId, Util::CurrentTime(), feedItems);
	}

	return feedItems;
}

void FeedCoordinator::FetchFeed(int id)
{
	debug("FetchFeeds");

	Guard guard(m_downloadsMutex);
	for (FeedInfo* feedInfo : &m_feeds)
	{
		if (feedInfo->GetId() == id || id == 0)
		{
			feedInfo->SetFetch(true);
			m_force = true;
		}
	}

	m_waitCond.NotifyAll();
}

std::unique_ptr<FeedFile> FeedCoordinator::parseFeed(FeedInfo* feedInfo)
{
	std::unique_ptr<FeedFile> feedFile = std::make_unique<FeedFile>(feedInfo->GetOutputFilename(), feedInfo->GetName());
	if (feedFile->Parse())
	{
		FileSystem::DeleteFile(feedInfo->GetOutputFilename());
	}
	else
	{
		error("Feed file %s kept for troubleshooting (will be deleted on next successful feed fetch)", feedInfo->GetOutputFilename());
		feedFile.reset();
	}
	return feedFile;
}

void FeedCoordinator::DownloadQueueUpdate(Subject* caller, void* aspect)
{
	debug("Notification from URL-Coordinator received");

	DownloadQueue::Aspect* queueAspect = (DownloadQueue::Aspect*)aspect;
	if (queueAspect->action == DownloadQueue::eaUrlCompleted)
	{
		Guard guard(m_downloadsMutex);
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
	}
}

bool FeedCoordinator::HasActiveDownloads()
{
	Guard guard(m_downloadsMutex);
	return !m_activeDownloads.empty();
}

void FeedCoordinator::CheckSaveFeeds()
{
	Guard guard(m_downloadsMutex);
	if (m_save)
	{
		debug("CheckSaveFeeds: save");
		if (g_Options->GetServerMode())
		{
			g_DiskState->SaveFeeds(&m_feeds, &m_feedHistory);
		}
		m_save = false;
	}
}

void FeedCoordinator::CleanupHistory()
{
	debug("CleanupHistory");

	Guard guard(m_downloadsMutex);

	time_t oldestUpdate = Util::CurrentTime();

	for (FeedInfo* feedInfo : &m_feeds)
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
}

void FeedCoordinator::CleanupCache()
{
	debug("CleanupCache");

	Guard guard(m_downloadsMutex);

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
}
