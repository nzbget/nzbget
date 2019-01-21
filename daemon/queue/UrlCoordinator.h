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


#ifndef URLCOORDINATOR_H
#define URLCOORDINATOR_H

#include "NString.h"
#include "Log.h"
#include "Thread.h"
#include "WebDownloader.h"
#include "DownloadInfo.h"
#include "Observer.h"

class UrlDownloader;

class UrlCoordinator : public Thread, public Observer, public Debuggable
{
public:
	UrlCoordinator();
	virtual ~UrlCoordinator();
	virtual void Run();
	virtual void Stop();
	void Update(Subject* caller, void* aspect);

	// Editing the queue
	void AddUrlToQueue(std::unique_ptr<NzbInfo> nzbInfo, bool addFirst);
	bool HasMoreJobs() { return m_hasMoreJobs; }
	bool DeleteQueueEntry(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, bool avoidHistory);

protected:
	virtual void LogDebugInfo();

private:
	typedef std::list<UrlDownloader*> ActiveDownloads;

	class DownloadQueueObserver: public Observer
	{
	public:
		UrlCoordinator* m_owner;
		virtual void Update(Subject* caller, void* aspect) { m_owner->DownloadQueueUpdate(caller, aspect); }
	};

	ActiveDownloads m_activeDownloads;
	bool m_hasMoreJobs = true;
	bool m_force;
	Mutex m_waitMutex;
	ConditionVar m_waitCond;
	DownloadQueueObserver m_downloadQueueObserver;

	NzbInfo* GetNextUrl(DownloadQueue* downloadQueue);
	void StartUrlDownload(NzbInfo* nzbInfo);
	void UrlCompleted(UrlDownloader* urlDownloader);
	void ResetHangingDownloads();
	void WaitJobs();
	void DownloadQueueUpdate(Subject* caller, void* aspect);
};

extern UrlCoordinator* g_UrlCoordinator;

class UrlDownloader : public WebDownloader
{
public:
	void SetNzbInfo(NzbInfo* nzbInfo) { m_nzbInfo = nzbInfo; }
	NzbInfo* GetNzbInfo() { return m_nzbInfo; }
	const char* GetCategory() { return m_category; }

protected:
	virtual void ProcessHeader(const char* line);

private:
	NzbInfo* m_nzbInfo;
	CString m_category;
};

#endif
