/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
		PrePostProcessor* m_pOwner;
		virtual void	Update(Subject* Caller, void* Aspect) { m_pOwner->DownloadQueueUpdate(Caller, Aspect); }
	};

private:
	ParCoordinator		m_ParCoordinator;
	DownloadQueueObserver	m_DownloadQueueObserver;
	int					m_iJobCount;
	NZBInfo*			m_pCurJob;
	const char*			m_szPauseReason;

	bool				IsNZBFileCompleted(NZBInfo* pNZBInfo, bool bIgnorePausedPars, bool bAllowOnlyOneDeleted);
	bool				IsNZBFileDownloading(NZBInfo* pNZBInfo);
	void				CheckPostQueue();
	void				JobCompleted(DownloadQueue* pDownloadQueue, PostInfo* pPostInfo);
	void				StartJob(DownloadQueue* pDownloadQueue, PostInfo* pPostInfo);
	void				SaveQueue(DownloadQueue* pDownloadQueue);
	void				SanitisePostQueue(DownloadQueue* pDownloadQueue);
	void				CheckDiskSpace();
	void				UpdatePauseState(bool bNeedPause, const char* szReason);
	void				NZBFound(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	void				NZBAdded(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	void				NZBDeleted(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	void				NZBCompleted(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, bool bSaveQueue);
	bool				PostQueueDelete(DownloadQueue* pDownloadQueue, IDList* pIDList);
	void				DeletePostThread(PostInfo* pPostInfo);
	NZBInfo*			GetNextJob(DownloadQueue* pDownloadQueue);
	void				DownloadQueueUpdate(Subject* Caller, void* Aspect);
	void				DeleteCleanup(NZBInfo* pNZBInfo);

public:
						PrePostProcessor();
	virtual				~PrePostProcessor();
	virtual void		Run();
	virtual void		Stop();
	bool				HasMoreJobs() { return m_iJobCount > 0; }
	int					GetJobCount() { return m_iJobCount; }
	bool				EditList(DownloadQueue* pDownloadQueue, IDList* pIDList, DownloadQueue::EEditAction eAction, int iOffset, const char* szText);
	void				NZBDownloaded(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
};

#endif
