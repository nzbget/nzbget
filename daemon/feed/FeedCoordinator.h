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

#include <deque>
#include <list>
#include <time.h>

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
		FeedCoordinator*	m_pOwner;
		virtual void		Update(Subject* pCaller, void* pAspect) { m_pOwner->DownloadQueueUpdate(pCaller, pAspect); }
	};

	class FeedCacheItem
	{
	private:
		char*				m_szUrl;
		int					m_iCacheTimeSec;
		char*				m_szCacheId;
		time_t				m_tLastUsage;
		FeedItemInfos*		m_pFeedItemInfos;

	public:
							FeedCacheItem(const char* szUrl, int iCacheTimeSec,const char* szCacheId,
								time_t tLastUsage, FeedItemInfos* pFeedItemInfos);
							~FeedCacheItem();
		const char*			GetUrl() { return m_szUrl; }
		int					GetCacheTimeSec() { return m_iCacheTimeSec; }
		const char*			GetCacheId() { return m_szCacheId; }
		time_t				GetLastUsage() { return m_tLastUsage; }
		void				SetLastUsage(time_t tLastUsage) { m_tLastUsage = tLastUsage; }
		FeedItemInfos*		GetFeedItemInfos() { return m_pFeedItemInfos; }
	};

	class FilterHelper : public FeedFilterHelper
	{
	private:
		RegEx*				m_pSeasonEpisodeRegEx;
	public:
							FilterHelper();
							~FilterHelper();
		virtual RegEx**		GetSeasonEpisodeRegEx() { return &m_pSeasonEpisodeRegEx; };
		virtual void		CalcDupeStatus(const char* szTitle, const char* szDupeKey, char* szStatusBuf, int iBufLen);
	};

	typedef std::deque<FeedCacheItem*>	FeedCache;
	typedef std::list<FeedDownloader*>	ActiveDownloads;

private:
	Feeds					m_Feeds;
	ActiveDownloads			m_ActiveDownloads;
	FeedHistory				m_FeedHistory;
	Mutex					m_mutexDownloads;
	DownloadQueueObserver	m_DownloadQueueObserver;
	bool					m_bForce;
	bool					m_bSave;
	FeedCache				m_FeedCache;
	FilterHelper			m_FilterHelper;

	void					StartFeedDownload(FeedInfo* pFeedInfo, bool bForce);
	void					FeedCompleted(FeedDownloader* pFeedDownloader);
	void					FilterFeed(FeedInfo* pFeedInfo, FeedItemInfos* pFeedItemInfos);
	void					ProcessFeed(FeedInfo* pFeedInfo, FeedItemInfos* pFeedItemInfos, NZBList* pAddedNZBs);
	NZBInfo*				CreateNZBInfo(FeedInfo* pFeedInfo, FeedItemInfo* pFeedItemInfo);
	void					ResetHangingDownloads();
	void					DownloadQueueUpdate(Subject* pCaller, void* pAspect);
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
	void					Update(Subject* pCaller, void* pAspect);
	void					AddFeed(FeedInfo* pFeedInfo);
	bool					PreviewFeed(const char* szName, const char* szUrl, const char* szFilter,
								bool bPauseNzb, const char* szCategory, int iPriority,
								int iCacheTimeSec, const char* szCacheId, FeedItemInfos** ppFeedItemInfos);
	bool					ViewFeed(int iID, FeedItemInfos** ppFeedItemInfos);
	void					FetchFeed(int iID);
	bool					HasActiveDownloads();
	Feeds*					GetFeeds() { return &m_Feeds; }
};

class FeedDownloader : public WebDownloader
{
private:
	FeedInfo*				m_pFeedInfo;

public:
	void					SetFeedInfo(FeedInfo* pFeedInfo) { m_pFeedInfo = pFeedInfo; }
	FeedInfo*				GetFeedInfo() { return m_pFeedInfo; }
};

#endif
