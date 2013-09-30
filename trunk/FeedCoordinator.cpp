/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "FeedCoordinator.h"
#include "Options.h"
#include "WebDownloader.h"
#include "Log.h"
#include "Util.h"
#include "FeedFile.h"
#include "FeedFilter.h"
#include "UrlCoordinator.h"
#include "DiskState.h"

extern Options* g_pOptions;
extern UrlCoordinator* g_pUrlCoordinator;
extern DiskState* g_pDiskState;


FeedCoordinator::FeedCacheItem::FeedCacheItem(const char* szUrl, int iCacheTimeSec,const char* szCacheId,
	time_t tLastUsage, FeedItemInfos* pFeedItemInfos)
{
	m_szUrl = strdup(szUrl);
	m_iCacheTimeSec = iCacheTimeSec;
	m_szCacheId = strdup(szCacheId);
	m_tLastUsage = tLastUsage;
	m_pFeedItemInfos = pFeedItemInfos;
	m_pFeedItemInfos->Retain();
}

FeedCoordinator::FeedCacheItem::~FeedCacheItem()
{
	if (m_szUrl)
	{
		free(m_szUrl);
	}
	if (m_szCacheId)
	{
		free(m_szCacheId);
	}

	m_pFeedItemInfos->Release();
}

FeedCoordinator::FeedCoordinator()
{
	debug("Creating FeedCoordinator");
	m_bForce = false;
	m_bSave = false;

	m_UrlCoordinatorObserver.m_pOwner = this;
	g_pUrlCoordinator->Attach(&m_UrlCoordinatorObserver);
}

FeedCoordinator::~FeedCoordinator()
{
	debug("Destroying FeedCoordinator");
	// Cleanup

	debug("Deleting FeedDownloaders");
	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
	{
		delete *it;
	}
	m_ActiveDownloads.clear();

	debug("Deleting Feeds");
	for (Feeds::iterator it = m_Feeds.begin(); it != m_Feeds.end(); it++)
	{
		delete *it;
	}
	m_Feeds.clear();
	
	debug("Deleting FeedCache");
	for (FeedCache::iterator it = m_FeedCache.begin(); it != m_FeedCache.end(); it++)
	{
		delete *it;
	}
	m_FeedCache.clear();

	debug("FeedCoordinator destroyed");
}

void FeedCoordinator::AddFeed(FeedInfo* pFeedInfo)
{
	m_Feeds.push_back(pFeedInfo);
}

void FeedCoordinator::Run()
{
	debug("Entering FeedCoordinator-loop");

	m_mutexDownloads.Lock();
	if (g_pOptions->GetServerMode() && g_pOptions->GetSaveQueue() && g_pOptions->GetReloadQueue())
	{
		g_pDiskState->LoadFeeds(&m_Feeds, &m_FeedHistory);
	}
	m_mutexDownloads.Unlock();

	int iSleepInterval = 100;
	int iUpdateCounter = 0;
	int iCleanupCounter = 60000;

	while (!IsStopped())
	{
		usleep(iSleepInterval * 1000);

		iUpdateCounter += iSleepInterval;
		if (iUpdateCounter >= 1000)
		{
			// this code should not be called too often, once per second is OK

			if (!(g_pOptions->GetPauseDownload() || g_pOptions->GetPauseDownload2()) || m_bForce)
			{
				m_mutexDownloads.Lock();
				time_t tCurrent = time(NULL);
				if ((int)m_ActiveDownloads.size() < g_pOptions->GetUrlConnections())
				{
					m_bForce = false;
					// check feed list and update feeds
					for (Feeds::iterator it = m_Feeds.begin(); it != m_Feeds.end(); it++)
					{
						FeedInfo* pFeedInfo = *it;
						if (((pFeedInfo->GetInterval() > 0 &&
							(tCurrent - pFeedInfo->GetLastUpdate() >= pFeedInfo->GetInterval() * 60 ||
							 tCurrent < pFeedInfo->GetLastUpdate())) ||
							pFeedInfo->GetFetch()) &&
							pFeedInfo->GetStatus() != FeedInfo::fsRunning)
						{
							StartFeedDownload(pFeedInfo, pFeedInfo->GetFetch());
						}
						else if (pFeedInfo->GetFetch())
						{
							m_bForce = true;
						}
					}
				}
				m_mutexDownloads.Unlock();
			}

			CheckSaveFeeds();
			ResetHangingDownloads();
			iUpdateCounter = 0;
		}

		iCleanupCounter += iSleepInterval;
		if (iCleanupCounter >= 60000)
		{
			// clean up feed history once a minute
			CleanupHistory();
			CleanupCache();
			CheckSaveFeeds();
			iCleanupCounter = 0;
		}
	}

	// waiting for downloads
	debug("FeedCoordinator: waiting for Downloads to complete");
	bool completed = false;
	while (!completed)
	{
		m_mutexDownloads.Lock();
		completed = m_ActiveDownloads.size() == 0;
		m_mutexDownloads.Unlock();
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
	m_mutexDownloads.Lock();
	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
	{
		(*it)->Stop();
	}
	m_mutexDownloads.Unlock();
	debug("UrlDownloads are notified");
}

void FeedCoordinator::ResetHangingDownloads()
{
	const int TimeOut = g_pOptions->GetTerminateTimeout();
	if (TimeOut == 0)
	{
		return;
	}

	m_mutexDownloads.Lock();
	time_t tm = ::time(NULL);

	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end();)
	{
		FeedDownloader* pFeedDownloader = *it;
		if (tm - pFeedDownloader->GetLastUpdateTime() > TimeOut &&
		   pFeedDownloader->GetStatus() == FeedDownloader::adRunning)
		{
			debug("Terminating hanging download %s", pFeedDownloader->GetInfoName());
			if (pFeedDownloader->Terminate())
			{
				error("Terminated hanging download %s", pFeedDownloader->GetInfoName());
				pFeedDownloader->GetFeedInfo()->SetStatus(FeedInfo::fsUndefined);
			}
			else
			{
				error("Could not terminate hanging download %s", pFeedDownloader->GetInfoName());
			}
			m_ActiveDownloads.erase(it);
			// it's not safe to destroy pFeedDownloader, because the state of object is unknown
			delete pFeedDownloader;
			it = m_ActiveDownloads.begin();
			continue;
		}
		it++;
	}                                              

	m_mutexDownloads.Unlock();
}

void FeedCoordinator::LogDebugInfo()
{
	debug("   FeedCoordinator");
	debug("   ----------------");

	m_mutexDownloads.Lock();
	debug("    Active Downloads: %i", m_ActiveDownloads.size());
	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
	{
		FeedDownloader* pFeedDownloader = *it;
		pFeedDownloader->LogDebugInfo();
	}
	m_mutexDownloads.Unlock();
}

void FeedCoordinator::StartFeedDownload(FeedInfo* pFeedInfo, bool bForce)
{
	debug("Starting new FeedDownloader for %", pFeedInfo->GetName());

	FeedDownloader* pFeedDownloader = new FeedDownloader();
	pFeedDownloader->SetAutoDestroy(true);
	pFeedDownloader->Attach(this);
	pFeedDownloader->SetFeedInfo(pFeedInfo);
	pFeedDownloader->SetURL(pFeedInfo->GetUrl());
	if (strlen(pFeedInfo->GetName()) > 0)
	{
		pFeedDownloader->SetInfoName(pFeedInfo->GetName());
	}
	else
	{
		char szUrlName[1024];
		UrlInfo::MakeNiceName(pFeedInfo->GetUrl(), "", szUrlName, sizeof(szUrlName));
		pFeedDownloader->SetInfoName(szUrlName);
	}
	pFeedDownloader->SetForce(bForce);

	char tmp[1024];

	if (pFeedInfo->GetID() > 0)
	{
		snprintf(tmp, 1024, "%sfeed-%i.tmp", g_pOptions->GetTempDir(), pFeedInfo->GetID());
	}
	else
	{
		snprintf(tmp, 1024, "%sfeed-%i-%i.tmp", g_pOptions->GetTempDir(), (int)time(NULL), rand());
	}

	tmp[1024-1] = '\0';
	pFeedDownloader->SetOutputFilename(tmp);

	pFeedInfo->SetStatus(FeedInfo::fsRunning);
	pFeedInfo->SetForce(bForce);
	pFeedInfo->SetFetch(false);

	m_ActiveDownloads.push_back(pFeedDownloader);
	pFeedDownloader->Start();
}

void FeedCoordinator::Update(Subject* pCaller, void* pAspect)
{
	debug("Notification from FeedDownloader received");

	FeedDownloader* pFeedDownloader = (FeedDownloader*) pCaller;
	if ((pFeedDownloader->GetStatus() == WebDownloader::adFinished) ||
		(pFeedDownloader->GetStatus() == WebDownloader::adFailed) ||
		(pFeedDownloader->GetStatus() == WebDownloader::adRetry))
	{
		FeedCompleted(pFeedDownloader);
	}
}

void FeedCoordinator::FeedCompleted(FeedDownloader* pFeedDownloader)
{
	debug("Feed downloaded");

	FeedInfo* pFeedInfo = pFeedDownloader->GetFeedInfo();
	bool bStatusOK = pFeedDownloader->GetStatus() == WebDownloader::adFinished;
	if (bStatusOK)
	{
		pFeedInfo->SetOutputFilename(pFeedDownloader->GetOutputFilename());
	}

	// delete Download from Queue
	m_mutexDownloads.Lock();
	for (ActiveDownloads::iterator it = m_ActiveDownloads.begin(); it != m_ActiveDownloads.end(); it++)
	{
		FeedDownloader* pa = *it;
		if (pa == pFeedDownloader)
		{
			m_ActiveDownloads.erase(it);
			break;
		}
	}
	m_mutexDownloads.Unlock();

	if (bStatusOK)
	{
		if (!pFeedInfo->GetPreview())
		{
			FeedFile* pFeedFile = FeedFile::Create(pFeedInfo->GetOutputFilename());
			remove(pFeedInfo->GetOutputFilename());

			m_mutexDownloads.Lock();

			if (pFeedFile)
			{
				ProcessFeed(pFeedInfo, pFeedFile->GetFeedItemInfos());
				delete pFeedFile;
			}

			pFeedInfo->SetLastUpdate(time(NULL));
			pFeedInfo->SetForce(false);

			m_bSave = true;
			m_mutexDownloads.Unlock();
		}
		pFeedInfo->SetStatus(FeedInfo::fsFinished);
	}
	else
	{
		pFeedInfo->SetStatus(FeedInfo::fsFailed);
	}
}

void FeedCoordinator::FilterFeed(FeedInfo* pFeedInfo, FeedItemInfos* pFeedItemInfos)
{
	debug("Filtering feed %s", pFeedInfo->GetName());

	FeedFilter* pFeedFilter = NULL;
	if (pFeedInfo->GetFilter() && strlen(pFeedInfo->GetFilter()) > 0)
	{
		pFeedFilter = new FeedFilter(pFeedInfo->GetFilter());
	}

	for (FeedItemInfos::iterator it = pFeedItemInfos->begin(); it != pFeedItemInfos->end(); it++)
    {
        FeedItemInfo* pFeedItemInfo = *it;
		pFeedItemInfo->SetMatchStatus(FeedItemInfo::msAccepted);
		pFeedItemInfo->SetMatchRule(0);
		pFeedItemInfo->SetPauseNzb(pFeedInfo->GetPauseNzb());
		pFeedItemInfo->SetPriority(pFeedInfo->GetPriority());
		pFeedItemInfo->SetAddCategory(pFeedInfo->GetCategory());
		pFeedItemInfo->SetDupeScore(0);
		pFeedItemInfo->SetDupeMode(dmScore);
		pFeedItemInfo->BuildDupeKey(NULL, NULL);
		if (pFeedFilter)
		{
			pFeedFilter->Match(pFeedItemInfo);
		}
    }

	delete pFeedFilter;
}

void FeedCoordinator::ProcessFeed(FeedInfo* pFeedInfo, FeedItemInfos* pFeedItemInfos)
{
	debug("Process feed %s", pFeedInfo->GetName());

	FilterFeed(pFeedInfo, pFeedItemInfos);

	bool bFirstFetch = pFeedInfo->GetLastUpdate() == 0;
	int iAdded = 0;

    for (FeedItemInfos::iterator it = pFeedItemInfos->begin(); it != pFeedItemInfos->end(); it++)
    {
        FeedItemInfo* pFeedItemInfo = *it;
		if (pFeedItemInfo->GetMatchStatus() == FeedItemInfo::msAccepted)
		{
			FeedHistoryInfo* pFeedHistoryInfo = m_FeedHistory.Find(pFeedItemInfo->GetUrl());
			FeedHistoryInfo::EStatus eStatus = FeedHistoryInfo::hsUnknown;
			if (bFirstFetch)
			{
				eStatus = FeedHistoryInfo::hsBacklog;
			}
			else if (!pFeedHistoryInfo)
			{
				DownloadItem(pFeedInfo, pFeedItemInfo);
				eStatus = FeedHistoryInfo::hsFetched;
				iAdded++;
			}

			if (pFeedHistoryInfo)
			{
				pFeedHistoryInfo->SetLastSeen(time(NULL));
			}
			else
			{
				m_FeedHistory.Add(pFeedItemInfo->GetUrl(), eStatus, time(NULL));
			}
		}
    }

	if (iAdded)
	{
		info("%s has %i new item(s)", pFeedInfo->GetName(), iAdded);
	}
	else
	{
		detail("%s has no new items", pFeedInfo->GetName());
	}
}

void FeedCoordinator::DownloadItem(FeedInfo* pFeedInfo, FeedItemInfo* pFeedItemInfo)
{
	debug("Download %s from %s", pFeedItemInfo->GetUrl(), pFeedInfo->GetName());

	UrlInfo* pUrlInfo = new UrlInfo();
	pUrlInfo->SetURL(pFeedItemInfo->GetUrl());

	// add .nzb-extension if not present
	char szNZBName[1024];
	strncpy(szNZBName, pFeedItemInfo->GetFilename(), 1024);
	szNZBName[1024-1] = '\0';
	char* ext = strrchr(szNZBName, '.');
	if (ext && !strcasecmp(ext, ".nzb"))
	{
		*ext = '\0';
	}
	char szNZBName2[1024];
	snprintf(szNZBName2, 1024, "%s.nzb", szNZBName);
	Util::MakeValidFilename(szNZBName2, '_', false);
	if (strlen(szNZBName) > 0)
	{
		pUrlInfo->SetNZBFilename(szNZBName2);
	}

	pUrlInfo->SetCategory(pFeedItemInfo->GetAddCategory());
	pUrlInfo->SetPriority(pFeedItemInfo->GetPriority());
	pUrlInfo->SetAddPaused(pFeedItemInfo->GetPauseNzb());
	pUrlInfo->SetDupeKey(pFeedItemInfo->GetDupeKey());
	pUrlInfo->SetDupeScore(pFeedItemInfo->GetDupeScore());
	pUrlInfo->SetDupeMode(pFeedItemInfo->GetDupeMode());
	pUrlInfo->SetForce(pFeedInfo->GetForce());
	g_pUrlCoordinator->AddUrlToQueue(pUrlInfo, false);
}

bool FeedCoordinator::ViewFeed(int iID, FeedItemInfos** ppFeedItemInfos)
{
	if (iID < 1 || iID > (int)m_Feeds.size())
	{
		return false;
	}

	FeedInfo* pFeedInfo = m_Feeds.at(iID - 1);

	return PreviewFeed(pFeedInfo->GetName(), pFeedInfo->GetUrl(), pFeedInfo->GetFilter(), 
		pFeedInfo->GetPauseNzb(), pFeedInfo->GetCategory(), pFeedInfo->GetPriority(),
		0, NULL, ppFeedItemInfos);
}

bool FeedCoordinator::PreviewFeed(const char* szName, const char* szUrl, const char* szFilter,
	bool bPauseNzb, const char* szCategory, int iPriority,
	int iCacheTimeSec, const char* szCacheId, FeedItemInfos** ppFeedItemInfos)
{
	debug("Preview feed %s", szName);

	FeedInfo* pFeedInfo = new FeedInfo(0, szName, szUrl, 0, szFilter, bPauseNzb, szCategory, iPriority);
	pFeedInfo->SetPreview(true);
	
	FeedItemInfos* pFeedItemInfos = NULL;
	bool bHasCache = false;
	if (iCacheTimeSec > 0 && *szCacheId != '\0')
	{
		m_mutexDownloads.Lock();
		for (FeedCache::iterator it = m_FeedCache.begin(); it != m_FeedCache.end(); it++)
		{
			FeedCacheItem* pFeedCacheItem = *it;
			if (!strcmp(pFeedCacheItem->GetCacheId(), szCacheId))
			{
				pFeedCacheItem->SetLastUsage(time(NULL));
				pFeedItemInfos = pFeedCacheItem->GetFeedItemInfos();
				pFeedItemInfos->Retain();
				bHasCache = true;
				break;
			}
		}
		m_mutexDownloads.Unlock();
	}

	if (!bHasCache)
	{
		m_mutexDownloads.Lock();

		bool bFirstFetch = true;
		for (Feeds::iterator it = m_Feeds.begin(); it != m_Feeds.end(); it++)
		{
			FeedInfo* pFeedInfo2 = *it;
			if (!strcmp(pFeedInfo2->GetUrl(), pFeedInfo->GetUrl()) &&
				!strcmp(pFeedInfo2->GetFilter(), pFeedInfo->GetFilter()) &&
				pFeedInfo2->GetLastUpdate() > 0)
			{
				bFirstFetch = false;
				break;
			}
		}

		StartFeedDownload(pFeedInfo, true);
		m_mutexDownloads.Unlock();

		// wait until the download in a separate thread completes
		while (pFeedInfo->GetStatus() == FeedInfo::fsRunning)
		{
			usleep(100 * 1000);
		}

		// now can process the feed

		FeedFile* pFeedFile = NULL;

		if (pFeedInfo->GetStatus() == FeedInfo::fsFinished)
		{
			pFeedFile = FeedFile::Create(pFeedInfo->GetOutputFilename());
		}

		remove(pFeedInfo->GetOutputFilename());

		if (!pFeedFile)
		{
			delete pFeedInfo;
			return false;
		}

		pFeedItemInfos = pFeedFile->GetFeedItemInfos();
		pFeedItemInfos->Retain();
		delete pFeedFile;

		for (FeedItemInfos::iterator it = pFeedItemInfos->begin(); it != pFeedItemInfos->end(); it++)
		{
			FeedItemInfo* pFeedItemInfo = *it;
			pFeedItemInfo->SetStatus(bFirstFetch ? FeedItemInfo::isBacklog : FeedItemInfo::isNew);
			FeedHistoryInfo* pFeedHistoryInfo = m_FeedHistory.Find(pFeedItemInfo->GetUrl());
			if (pFeedHistoryInfo)
			{
				pFeedItemInfo->SetStatus((FeedItemInfo::EStatus)pFeedHistoryInfo->GetStatus());
			}
		}
	}

	FilterFeed(pFeedInfo, pFeedItemInfos);
	delete pFeedInfo;

	if (iCacheTimeSec > 0 && *szCacheId != '\0' && !bHasCache)
	{
		FeedCacheItem* pFeedCacheItem = new FeedCacheItem(szUrl, iCacheTimeSec, szCacheId, time(NULL), pFeedItemInfos);
		m_mutexDownloads.Lock();
		m_FeedCache.push_back(pFeedCacheItem);
		m_mutexDownloads.Unlock();
	}
	
	*ppFeedItemInfos = pFeedItemInfos;

	return true;
}

void FeedCoordinator::FetchAllFeeds()
{
	debug("FetchAllFeeds");

	m_mutexDownloads.Lock();
	for (Feeds::iterator it = m_Feeds.begin(); it != m_Feeds.end(); it++)
	{
		FeedInfo* pFeedInfo = *it;
		pFeedInfo->SetFetch(true);
		m_bForce = true;
	}
	m_mutexDownloads.Unlock();
}

void FeedCoordinator::UrlCoordinatorUpdate(Subject* pCaller, void* pAspect)
{
	debug("Notification from URL-Coordinator received");

	UrlCoordinator::Aspect* pUrlAspect = (UrlCoordinator::Aspect*)pAspect;
	if (pUrlAspect->eAction == UrlCoordinator::eaUrlCompleted)
	{
		m_mutexDownloads.Lock();
		FeedHistoryInfo* pFeedHistoryInfo = m_FeedHistory.Find(pUrlAspect->pUrlInfo->GetURL());
		if (pFeedHistoryInfo)
		{
			pFeedHistoryInfo->SetStatus(FeedHistoryInfo::hsFetched);
		}
		else
		{
			m_FeedHistory.Add(pUrlAspect->pUrlInfo->GetURL(), FeedHistoryInfo::hsFetched, time(NULL));
		}
		m_bSave = true;
		m_mutexDownloads.Unlock();
	}
}

bool FeedCoordinator::HasActiveDownloads()
{
	m_mutexDownloads.Lock();
	bool bActive = !m_ActiveDownloads.empty();
	m_mutexDownloads.Unlock();
	return bActive;
}

void FeedCoordinator::CheckSaveFeeds()
{
	debug("CheckSaveFeeds");
	m_mutexDownloads.Lock();
	if (m_bSave)
	{
		if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
		{
			g_pDiskState->SaveFeeds(&m_Feeds, &m_FeedHistory);
		}
		m_bSave = false;
	}
	m_mutexDownloads.Unlock();
}

void FeedCoordinator::CleanupHistory()
{
	debug("CleanupHistory");

	m_mutexDownloads.Lock();

	time_t tOldestUpdate = time(NULL);

	for (Feeds::iterator it = m_Feeds.begin(); it != m_Feeds.end(); it++)
	{
		FeedInfo* pFeedInfo = *it;
		if (pFeedInfo->GetLastUpdate() < tOldestUpdate)
		{
			tOldestUpdate = pFeedInfo->GetLastUpdate();
		}
	}

	time_t tBorderDate = tOldestUpdate - g_pOptions->GetFeedHistory();
	int i = 0;
	for (FeedHistory::iterator it = m_FeedHistory.begin(); it != m_FeedHistory.end(); )
	{
		FeedHistoryInfo* pFeedHistoryInfo = *it;
		if (pFeedHistoryInfo->GetLastSeen() < tBorderDate)
		{
			detail("Deleting %s from feed history", pFeedHistoryInfo->GetUrl());
			delete pFeedHistoryInfo;
			m_FeedHistory.erase(it);
			it = m_FeedHistory.begin() + i;
			m_bSave = true;
		}
		else
		{
			it++;
			i++;
		}
	}

	m_mutexDownloads.Unlock();
}

void FeedCoordinator::CleanupCache()
{
	debug("CleanupCache");
	
	m_mutexDownloads.Lock();
	
	time_t tCurTime = time(NULL);
	int i = 0;
	for (FeedCache::iterator it = m_FeedCache.begin(); it != m_FeedCache.end(); )
	{
		FeedCacheItem* pFeedCacheItem = *it;
		if (pFeedCacheItem->GetLastUsage() + pFeedCacheItem->GetCacheTimeSec() < tCurTime ||
			pFeedCacheItem->GetLastUsage() > tCurTime)
		{
			debug("Deleting %s from feed cache", pFeedCacheItem->GetUrl());
			delete pFeedCacheItem;
			m_FeedCache.erase(it);
			it = m_FeedCache.begin() + i;
		}
		else
		{
			it++;
			i++;
		}
	}
	
	m_mutexDownloads.Unlock();
}
