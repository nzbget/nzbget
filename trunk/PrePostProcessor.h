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
#include "DupeCoordinator.h"

class PrePostProcessor : public Thread
{
public:
	enum EEditAction
	{
		eaPostMoveOffset = 51,			// move post to m_iOffset relative to the current position in post-queue
		eaPostMoveTop,
		eaPostMoveBottom,
		eaPostDelete,
		eaHistoryDelete,
		eaHistoryReturn,
		eaHistoryProcess,
		eaHistorySetParameter,
		eaHistorySetDupeKey,
		eaHistorySetDupeScore,
		eaHistorySetDupeMode,
		eaHistoryMarkBad,
		eaHistoryMarkGood
	};

private:
	class QueueCoordinatorObserver: public Observer
	{
	public:
		PrePostProcessor* m_pOwner;
		virtual void	Update(Subject* Caller, void* Aspect) { m_pOwner->QueueCoordinatorUpdate(Caller, Aspect); }
	};

	class PostParCoordinator: public ParCoordinator
	{
	private:
		PrePostProcessor*	m_pOwner;
	protected:
		virtual bool		PauseDownload() { return m_pOwner->PauseDownload(); }
		virtual bool		UnpauseDownload() { return m_pOwner->UnpauseDownload(); }
		friend class PrePostProcessor;
	};

	class PostDupeCoordinator: public DupeCoordinator
	{
	private:
		PrePostProcessor*	m_pOwner;
	protected:
		virtual void		HistoryReturn(DownloadQueue* pDownloadQueue, HistoryList::iterator itHistory,
			HistoryInfo* pHistoryInfo, bool bReprocess)	{ m_pOwner->HistoryReturn(pDownloadQueue, itHistory, pHistoryInfo, bReprocess); }
		virtual void		DeleteQueuedFile(const char* szQueuedFile) {  m_pOwner->DeleteQueuedFile(szQueuedFile); }
		friend class PrePostProcessor;
	};

private:
	PostParCoordinator	m_ParCoordinator;
	PostDupeCoordinator	m_DupeCoordinator;
	QueueCoordinatorObserver	m_QueueCoordinatorObserver;
	bool				m_bHasMoreJobs;
	bool				m_bSchedulerPauseChanged;
	bool				m_bSchedulerPause;
	bool				m_bPostPause;
	const char*			m_szPauseReason;

	bool				IsNZBFileCompleted(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, 
							bool bIgnorePausedPars, bool bAllowOnlyOneDeleted);
	void				CheckPostQueue();
	void				JobCompleted(DownloadQueue* pDownloadQueue, PostInfo* pPostInfo);
	void				StartJob(DownloadQueue* pDownloadQueue, PostInfo* pPostInfo);
	void				SaveQueue(DownloadQueue* pDownloadQueue);
	void				SanitisePostQueue(PostQueue* pPostQueue);
	void				CheckDiskSpace();
	void				ApplySchedulerState();
	void				CheckScheduledResume();
	void				UpdatePauseState(bool bNeedPause, const char* szReason);
	bool				PauseDownload();
	bool				UnpauseDownload();
	void				NZBFound(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	void				NZBAdded(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	void				NZBDownloaded(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	void				NZBDeleted(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	void				NZBCompleted(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, bool bSaveQueue);
	void				DeleteQueuedFile(const char* szQueuedFile);
	int					FindGroupID(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	bool				PostQueueMove(IDList* pIDList, EEditAction eAction, int iOffset);
	bool				PostQueueDelete(IDList* pIDList);
	bool				HistoryEdit(IDList* pIDList, EEditAction eAction, int iOffset, const char* szText);
	void				HistoryDelete(DownloadQueue* pDownloadQueue, HistoryList::iterator itHistory, HistoryInfo* pHistoryInfo);
	void				HistoryReturn(DownloadQueue* pDownloadQueue, HistoryList::iterator itHistory, HistoryInfo* pHistoryInfo, bool bReprocess);
	void				HistorySetParameter(HistoryInfo* pHistoryInfo, const char* szText);
	void				HistorySetDupeParam(HistoryInfo* pHistoryInfo, EEditAction eAction, const char* szText);
	void				HistoryTransformToDup(DownloadQueue* pDownloadQueue, HistoryInfo* pHistoryInfo, int rindex);
	void				CheckHistory();
	void				Cleanup();
	FileInfo*			GetQueueGroup(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	void				DeletePostThread(PostInfo* pPostInfo);

public:
						PrePostProcessor();
	virtual				~PrePostProcessor();
	virtual void		Run();
	virtual void		Stop();
	void				QueueCoordinatorUpdate(Subject* Caller, void* Aspect);
	bool				HasMoreJobs() { return m_bHasMoreJobs; }
	bool				QueueEditList(IDList* pIDList, EEditAction eAction, int iOffset, const char* szText);
};

#endif
