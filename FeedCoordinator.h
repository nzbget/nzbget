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
 * $Revision: 0 $
 * $Date: 2013-06-24 00:00:00 +0200 (Mo, 24 Jun 2013) $
 *
 */


#ifndef FEEDCOORDINATOR_H
#define FEEDCOORDINATOR_H

#include <deque>
#include <list>
#include <time.h>

#include "Thread.h"
#include "WebDownloader.h"
#include "DownloadInfo.h"
#include "Observer.h"


class FeedDownloader;

class FeedCoordinator : public Thread, public Observer, public Subject
{
public:
	typedef std::list<FeedDownloader*>	ActiveDownloads;

private:
	class UrlCoordinatorObserver: public Observer
	{
	public:
		FeedCoordinator*	m_pOwner;
		virtual void		Update(Subject* pCaller, void* pAspect) { m_pOwner->UrlCoordinatorUpdate(pCaller, pAspect); }
	};

private:
	Feeds					m_Feeds;
	ActiveDownloads			m_ActiveDownloads;
	FeedHistory				m_FeedHistory;
	Mutex					m_mutexDownloads;
	UrlCoordinatorObserver	m_UrlCoordinatorObserver;
	bool					m_bForce;
	bool					m_bSave;

	void					StartFeedDownload(FeedInfo* pFeedInfo, bool bForce);
	void					FeedCompleted(FeedDownloader* pFeedDownloader);
	void					ProcessFeed(FeedInfo* pFeedInfo, FeedItemInfos* pFeedItemInfos);
	void					DownloadItem(FeedInfo* pFeedInfo, FeedItemInfo* pFeedItemInfo);
	void					ResetHangingDownloads();
	void					UrlCoordinatorUpdate(Subject* pCaller, void* pAspect);
	void					CleanupHistory();
	void					CheckSaveFeeds();

public:
							FeedCoordinator();                
	virtual					~FeedCoordinator();
	virtual void			Run();
	virtual void 			Stop();
	void					Update(Subject* pCaller, void* pAspect);
	void					AddFeed(FeedInfo* pFeedInfo);
	bool					PreviewFeed(const char* szName, const char* szUrl, const char* szFilter, FeedItemInfos* pFeedItemInfos);
	bool					ViewFeed(int iID, FeedItemInfos* pFeedItemInfos);
	void					FetchAllFeeds();
	bool					HasActiveDownloads();

	void					LogDebugInfo();
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
