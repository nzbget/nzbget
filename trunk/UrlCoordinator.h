/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2012-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include "Thread.h"
#include "WebDownloader.h"
#include "DownloadInfo.h"
#include "Observer.h"

class UrlDownloader;

class UrlCoordinator : public Thread, public Observer, public Subject
{
public:
	typedef std::list<UrlDownloader*>	ActiveDownloads;
	enum EAspectAction
	{
		eaUrlAdded,
		eaUrlCompleted
	};
	struct Aspect
	{
		EAspectAction eAction;
		UrlInfo* pUrlInfo;
	};

private:
	ActiveDownloads			m_ActiveDownloads;
	bool					m_bHasMoreJobs;
	bool					m_bForce;

	bool					GetNextUrl(DownloadQueue* pDownloadQueue, UrlInfo* &pUrlInfo);
	void					StartUrlDownload(UrlInfo* pUrlInfo);
	void					UrlCompleted(UrlDownloader* pUrlDownloader);
	void					ResetHangingDownloads();
	void					AddToNZBQueue(UrlInfo* pUrlInfo, const char* szTempFilename, const char* szOriginalFilename, const char* szOriginalCategory);

public:
							UrlCoordinator();                
	virtual					~UrlCoordinator();
	virtual void			Run();
	virtual void 			Stop();
	void					Update(Subject* pCaller, void* pAspect);

	// Editing the queue
	void					AddUrlToQueue(UrlInfo* pUrlInfo, bool AddFirst);
	bool					HasMoreJobs() { return m_bHasMoreJobs; }

	void					LogDebugInfo();
};

class UrlDownloader : public WebDownloader
{
private:
	UrlInfo*				m_pUrlInfo;
	char*					m_szCategory;

protected:
	virtual void			ProcessHeader(const char* szLine);

public:
							UrlDownloader();
							~UrlDownloader();
	void					SetUrlInfo(UrlInfo* pUrlInfo) { m_pUrlInfo = pUrlInfo; }
	UrlInfo*				GetUrlInfo() { return m_pUrlInfo; }
	const char*				GetCategory() { return m_szCategory; }
};

#endif
