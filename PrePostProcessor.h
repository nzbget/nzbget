/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2009 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "Scanner.h"

#ifndef DISABLE_PARCHECK
#include "ParChecker.h"
#endif

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
		eaHistoryProcess
	};

private:
	typedef std::deque<char*>		FileList;

	class QueueCoordinatorObserver: public Observer
	{
	public:
		PrePostProcessor* owner;
		virtual void	Update(Subject* Caller, void* Aspect) { owner->QueueCoordinatorUpdate(Caller, Aspect); }
	};

#ifndef DISABLE_PARCHECK
	class ParCheckerObserver: public Observer
	{
	public:
		PrePostProcessor* owner;
		virtual void	Update(Subject* Caller, void* Aspect) { owner->ParCheckerUpdate(Caller, Aspect); }
	};

	class PostParChecker: public ParChecker
	{
	private:
		PrePostProcessor*	m_Owner;
		PostInfo*			m_pPostInfo;
	protected:
		virtual bool	RequestMorePars(int iBlockNeeded, int* pBlockFound);
		virtual void	UpdateProgress();
	public:
		PostInfo*		GetPostInfo() { return m_pPostInfo; }
		void			SetPostInfo(PostInfo* pPostInfo) { m_pPostInfo = pPostInfo; }

		friend class PrePostProcessor;
	};

	struct BlockInfo
	{
		FileInfo*		m_pFileInfo;
		int				m_iBlockCount;
	};

	typedef std::list<BlockInfo*> 	Blocks;
#endif
	
private:
	QueueCoordinatorObserver	m_QueueCoordinatorObserver;
	bool				m_bHasMoreJobs;
	bool				m_bPostScript;
	bool				m_bSchedulerPauseChanged;
	bool				m_bSchedulerPause;
	bool				m_bPostPause;
	Scanner				m_Scanner;

	bool				IsNZBFileCompleted(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, 
							bool bIgnorePausedPars, bool bCheckPostQueue, bool bAllowOnlyOneDeleted);
	void				CheckPostQueue();
	void				JobCompleted(DownloadQueue* pDownloadQueue, PostInfo* pPostInfo);
	void				StartScriptJob(DownloadQueue* pDownloadQueue, PostInfo* pPostInfo);
	void				SaveQueue(DownloadQueue* pDownloadQueue);
	void				SanitisePostQueue(PostQueue* pPostQueue);
	void				CheckDiskSpace();
	void				ApplySchedulerState();
	bool				PauseDownload();
	bool				UnpauseDownload();
	void				NZBAdded(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	void				NZBDownloaded(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	void				NZBDeleted(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	void				NZBCompleted(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, bool bSaveQueue);
	bool				FindMainPars(const char* szPath, FileList* pFileList);
	bool				ParseParFilename(const char* szParFilename, int* iBaseNameLen, int* iBlocks);
	bool				SameParCollection(const char* szFilename1, const char* szFilename2);
	bool				CreatePostJobs(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, bool bParCheck, bool bPostScript, bool bAddTop);
	void				DeleteQueuedFile(const char* szQueuedFile);
	void				PausePars(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	NZBInfo*			MergeGroups(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	bool				PostQueueMove(IDList* pIDList, EEditAction eAction, int iOffset);
	bool				PostQueueDelete(IDList* pIDList);
	bool				HistoryDelete(IDList* pIDList);
	bool				HistoryReturn(IDList* pIDList, bool bReprocess);
	void				Cleanup();
	FileInfo*			GetQueueGroup(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	void				CheckHistory();

#ifndef DISABLE_PARCHECK
	PostParChecker		m_ParChecker;
	ParCheckerObserver	m_ParCheckerObserver;

	void				ParCheckerUpdate(Subject* Caller, void* Aspect);
	bool				AddPar(FileInfo* pFileInfo, bool bDeleted);
	bool				RequestMorePars(NZBInfo* pNZBInfo, const char* szParFilename, int iBlockNeeded, int* pBlockFound);
	void				FindPars(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, const char* szParFilename, 
							Blocks* pBlocks, bool bStrictParName, bool bExactParName, int* pBlockFound);
	void				UpdateParProgress();
	void				StartParJob(PostInfo* pPostInfo);
#endif
	
public:
						PrePostProcessor();
	virtual				~PrePostProcessor();
	virtual void		Run();
	virtual void		Stop();
	void				QueueCoordinatorUpdate(Subject* Caller, void* Aspect);
	bool				HasMoreJobs() { return m_bHasMoreJobs; }
	void				ScanNZBDir(bool bSyncMode);
	bool				QueueEditList(IDList* pIDList, EEditAction eAction, int iOffset);
};

#endif
