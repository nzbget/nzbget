/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef PREPOSTPROCESSOR_H
#define PREPOSTPROCESSOR_H

#include <deque>

#include "Thread.h"
#include "Observer.h"
#include "DownloadInfo.h"
#include "ParCoordinator.h"

class PrePostProcessor : public Thread
{
private:
	class DownloadQueueObserver: public Observer
	{
	public:
		PrePostProcessor* m_owner;
		virtual void	Update(Subject* Caller, void* Aspect) { m_owner->DownloadQueueUpdate(Caller, Aspect); }
	};

private:
	ParCoordinator		m_parCoordinator;
	DownloadQueueObserver	m_downloadQueueObserver;
	int					m_jobCount;
	NZBInfo*			m_curJob;
	const char*			m_pauseReason;

	bool				IsNZBFileCompleted(NZBInfo* nzbInfo, bool ignorePausedPars, bool allowOnlyOneDeleted);
	bool				IsNZBFileDownloading(NZBInfo* nzbInfo);
	void				CheckPostQueue();
	void				JobCompleted(DownloadQueue* downloadQueue, PostInfo* postInfo);
	void				StartJob(DownloadQueue* downloadQueue, PostInfo* postInfo);
	void				SaveQueue(DownloadQueue* downloadQueue);
	void				SanitisePostQueue(DownloadQueue* downloadQueue);
	void				UpdatePauseState(bool needPause, const char* reason);
	void				NZBFound(DownloadQueue* downloadQueue, NZBInfo* nzbInfo);
	void				NZBDeleted(DownloadQueue* downloadQueue, NZBInfo* nzbInfo);
	void				NZBCompleted(DownloadQueue* downloadQueue, NZBInfo* nzbInfo, bool saveQueue);
	bool				PostQueueDelete(DownloadQueue* downloadQueue, IDList* idList);
	void				DeletePostThread(PostInfo* postInfo);
	NZBInfo*			GetNextJob(DownloadQueue* downloadQueue);
	void				DownloadQueueUpdate(Subject* Caller, void* Aspect);
	void				DeleteCleanup(NZBInfo* nzbInfo);

public:
						PrePostProcessor();
	virtual				~PrePostProcessor();
	virtual void		Run();
	virtual void		Stop();
	bool				HasMoreJobs() { return m_jobCount > 0; }
	int					GetJobCount() { return m_jobCount; }
	bool				EditList(DownloadQueue* downloadQueue, IDList* idList, DownloadQueue::EEditAction action, int offset, const char* text);
	void				NZBAdded(DownloadQueue* downloadQueue, NZBInfo* nzbInfo);
	void				NZBDownloaded(DownloadQueue* downloadQueue, NZBInfo* nzbInfo);
};

extern PrePostProcessor* g_pPrePostProcessor;

#endif
