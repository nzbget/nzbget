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


#ifndef FEEDCOORDINATOR_H
#define FEEDCOORDINATOR_H

#include "NString.h"
#include "Log.h"
#include "Thread.h"
#include "WebDownloader.h"
#include "DownloadInfo.h"
#include "FeedInfo.h"
#include "Observer.h"
#include "Util.h"


class FeedDownloader;

class FeedCoordinator : public Thread, public Observer, public Subject, public Debuggable
{
private:
	class DownloadQueueObserver: public Observer
	{
	public:
		FeedCoordinator*	m_owner;
		virtual void		Update(Subject* caller, void* aspect) { m_owner->DownloadQueueUpdate(caller, aspect); }
	};

	class FeedCacheItem
	{
	private:
		CString				m_url;
		int					m_cacheTimeSec;
		CString				m_cacheId;
		time_t				m_lastUsage;
		FeedItemInfos*		m_feedItemInfos;

	public:
							FeedCacheItem(const char* url, int cacheTimeSec,const char* cacheId,
								time_t lastUsage, FeedItemInfos* feedItemInfos);
							~FeedCacheItem();
		const char*			GetUrl() { return m_url; }
		int					GetCacheTimeSec() { return m_cacheTimeSec; }
		const char*			GetCacheId() { return m_cacheId; }
		time_t				GetLastUsage() { return m_lastUsage; }
		void				SetLastUsage(time_t lastUsage) { m_lastUsage = lastUsage; }
		FeedItemInfos*		GetFeedItemInfos() { return m_feedItemInfos; }
	};

	class FilterHelper : public FeedFilterHelper
	{
	private:
		RegEx*				m_seasonEpisodeRegEx;
	public:
							FilterHelper();
							~FilterHelper();
		virtual RegEx**		GetSeasonEpisodeRegEx() { return &m_seasonEpisodeRegEx; };
		virtual void		CalcDupeStatus(const char* title, const char* dupeKey, char* statusBuf, int bufLen);
	};

	typedef std::deque<FeedCacheItem*>	FeedCache;
	typedef std::list<FeedDownloader*>	ActiveDownloads;

private:
	Feeds					m_feeds;
	ActiveDownloads			m_activeDownloads;
	FeedHistory				m_feedHistory;
	Mutex					m_downloadsMutex;
	DownloadQueueObserver	m_downloadQueueObserver;
	bool					m_force;
	bool					m_save;
	FeedCache				m_feedCache;
	FilterHelper			m_filterHelper;

	void					StartFeedDownload(FeedInfo* feedInfo, bool force);
	void					FeedCompleted(FeedDownloader* feedDownloader);
	void					FilterFeed(FeedInfo* feedInfo, FeedItemInfos* feedItemInfos);
	void					ProcessFeed(FeedInfo* feedInfo, FeedItemInfos* feedItemInfos, NzbList* addedNzbs);
	NzbInfo*				CreateNzbInfo(FeedInfo* feedInfo, FeedItemInfo& feedItemInfo);
	void					ResetHangingDownloads();
	void					DownloadQueueUpdate(Subject* caller, void* aspect);
	void					CleanupHistory();
	void					CleanupCache();
	void					CheckSaveFeeds();

protected:
	virtual void			LogDebugInfo();

public:
							FeedCoordinator();
	virtual					~FeedCoordinator();
	virtual void			Run();
	virtual void 			Stop();
	void					Update(Subject* caller, void* aspect);
	void					AddFeed(FeedInfo* feedInfo);
	bool					PreviewFeed(int id, const char* name, const char* url, const char* filter, bool backlog,
								bool pauseNzb, const char* category, int priority, int interval, const char* feedScript,
								int cacheTimeSec, const char* cacheId, FeedItemInfos** ppFeedItemInfos);
	bool					ViewFeed(int id, FeedItemInfos** ppFeedItemInfos);
	void					FetchFeed(int id);
	bool					HasActiveDownloads();
	Feeds*					GetFeeds() { return &m_feeds; }
};

extern FeedCoordinator* g_FeedCoordinator;

class FeedDownloader : public WebDownloader
{
private:
	FeedInfo*				m_feedInfo;

public:
	void					SetFeedInfo(FeedInfo* feedInfo) { m_feedInfo = feedInfo; }
	FeedInfo*				GetFeedInfo() { return m_feedInfo; }
};

#endif
