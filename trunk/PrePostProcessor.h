/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
public:
	// NOTE: changes to this enum must be synced with "eRemoteEditAction" in unit "MessageBase.h"
	enum EEditAction
	{
		eaPostDelete = 51
	};

private:
	class QueueCoordinatorObserver: public Observer
	{
	public:
		PrePostProcessor* m_pOwner;
		virtual void	Update(Subject* Caller, void* Aspect) { m_pOwner->QueueCoordinatorUpdate(Caller, Aspect); }
	};

private:
	ParCoordinator		m_ParCoordinator;
	QueueCoordinatorObserver	m_QueueCoordinatorObserver;
	int					m_iJobCount;
	NZBInfo*			m_pCurJob;
	const char*			m_szPauseReason;

	bool				IsNZBFileCompleted(NZBInfo* pNZBInfo, bool bIgnorePausedPars, bool bAllowOnlyOneDeleted);
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
	bool				PostQueueDelete(IDList* pIDList);
	void				Cleanup();
	void				DeletePostThread(PostInfo* pPostInfo);
	NZBInfo*			GetNextJob(DownloadQueue* pDownloadQueue);

public:
						PrePostProcessor();
	virtual				~PrePostProcessor();
	virtual void		Run();
	virtual void		Stop();
	void				QueueCoordinatorUpdate(Subject* Caller, void* Aspect);
	bool				HasMoreJobs() { return m_iJobCount > 0; }
	int					GetJobCount() { return m_iJobCount; }
	bool				EditList(IDList* pIDList, EEditAction eAction, int iOffset, const char* szText);
	void				NZBDownloaded(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
};

#endif
