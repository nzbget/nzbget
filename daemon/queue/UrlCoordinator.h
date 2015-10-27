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


#ifndef URLCOORDINATOR_H
#define URLCOORDINATOR_H

#include <deque>
#include <list>
#include <time.h>

#include "Log.h"
#include "Thread.h"
#include "WebDownloader.h"
#include "DownloadInfo.h"
#include "Observer.h"

class UrlDownloader;

class UrlCoordinator : public Thread, public Observer, public Debuggable
{
private:
	typedef std::list<UrlDownloader*>	ActiveDownloads;

private:
	ActiveDownloads			m_activeDownloads;
	bool					m_hasMoreJobs;
	bool					m_force;

	NZBInfo*				GetNextUrl(DownloadQueue* downloadQueue);
	void					StartUrlDownload(NZBInfo* nzbInfo);
	void					UrlCompleted(UrlDownloader* urlDownloader);
	void					ResetHangingDownloads();

protected:
	virtual void			LogDebugInfo();

public:
							UrlCoordinator();                
	virtual					~UrlCoordinator();
	virtual void			Run();
	virtual void 			Stop();
	void					Update(Subject* caller, void* aspect);

	// Editing the queue
	void					AddUrlToQueue(NZBInfo* nzbInfo, bool addTop);
	bool					HasMoreJobs() { return m_hasMoreJobs; }
	bool					DeleteQueueEntry(DownloadQueue* downloadQueue, NZBInfo* nzbInfo, bool avoidHistory);
};

extern UrlCoordinator* g_pUrlCoordinator;

class UrlDownloader : public WebDownloader
{
private:
	NZBInfo*				m_nzbInfo;
	char*					m_category;

protected:
	virtual void			ProcessHeader(const char* line);

public:
							UrlDownloader();
							~UrlDownloader();
	void					SetNZBInfo(NZBInfo* nzbInfo) { m_nzbInfo = nzbInfo; }
	NZBInfo*				GetNZBInfo() { return m_nzbInfo; }
	const char*				GetCategory() { return m_category; }
};

#endif
