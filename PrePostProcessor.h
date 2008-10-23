/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007  Andrei Prygounkov <hugbug@users.sourceforge.net>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include "PostInfo.h"

#ifndef DISABLE_PARCHECK
#include "ParChecker.h"
#endif

class PrePostProcessor : public Thread
{
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
		PrePostProcessor* m_Owner;
	protected:
		virtual bool	RequestMorePars(int iBlockNeeded, int* pBlockFound);
		virtual void	UpdateProgress();

		friend class PrePostProcessor;
	};

	struct BlockInfo
	{
		FileInfo*		m_pFileInfo;
		int				m_iBlockCount;
	};

	typedef std::deque<BlockInfo*> 	Blocks;
#endif
	
private:
	QueueCoordinatorObserver	m_QueueCoordinatorObserver;
	bool				m_bHasMoreJobs;
	bool				m_bPostScript;
	bool				m_bNZBScript;
	bool				m_bSchedulerPauseChanged;
	bool				m_bSchedulerPause;
	bool				m_bPostPause;

	void				PausePars(DownloadQueue* pDownloadQueue, const char* szNZBFilename);
	void				CheckIncomingNZBs(const char* szDirectory, const char* szCategory);
	bool				IsNZBFileCompleted(DownloadQueue* pDownloadQueue, const char* szNZBFilename, 
							bool bIgnoreFirstInPostQueue, bool bIgnorePaused, bool bCheckPostQueue, bool bAllowOnlyOneDeleted);
	bool				CheckScript(FileInfo* pFileInfo);
	bool				JobExists(PostQueue* pPostQueue, const char* szNZBFilename, const char* szParFilename, bool bParCheck);
	bool				ClearCompletedJobs(const char* szNZBFilename);
	void				CheckPostQueue();
	void				JobCompleted(DownloadQueue* pDownloadQueue, PostInfo* pPostInfo);
	void				StartScriptJob(DownloadQueue* pDownloadQueue, PostInfo* pPostInfo);
	void				SavePostQueue();
	void				SanitisePostQueue();
	void				CheckDiskSpace();
	void				AddFileToQueue(const char* szFilename, const char* szCategory);
	void				ProcessIncomingFile(const char* szDirectory, const char* szBaseFilename, const char* szFullFilename, const char* szCategory);
	void				ApplySchedulerState();
	bool				PauseDownload();
	bool				UnpauseDownload();
	void				CollectionCompleted(DownloadQueue* pDownloadQueue, FileInfo* pFileInfo);
	void				CollectionDeleted(DownloadQueue* pDownloadQueue, FileInfo* pFileInfo);
	bool				FindMainPars(const char* szPath, FileList* pFileList);
	bool				ParseParFilename(const char* szParFilename, int* iBaseNameLen, int* iBlocks);
	bool				SameParCollection(const char* szFilename1, const char* szFilename2);
	bool				CreatePostJobs(DownloadQueue* pDownloadQueue, const char* szDestDir, const char* szNZBFilename, 
							const char* szCategory, const char* szQueuedFilename, bool bParCheck, bool bLockQueue, bool bAddTop);
	void				DeleteQueuedFile(const char* szQueuedFile);

	Mutex			 	m_mutexQueue;
	PostQueue			m_PostQueue;
	PostQueue			m_CompletedJobs;

#ifndef DISABLE_PARCHECK
	PostParChecker		m_ParChecker;
	ParCheckerObserver	m_ParCheckerObserver;

	void				ParCheckerUpdate(Subject* Caller, void* Aspect);
	bool				AddPar(FileInfo* pFileInfo, bool bDeleted);
	FileInfo*			GetParCleanupQueueGroup(DownloadQueue* pDownloadQueue, const char* szNZBFilename);
	bool				HasFailedParJobs(const char* szNZBFilename);
	bool				RequestMorePars(const char* szNZBFilename, const char* szParFilename, int iBlockNeeded, int* pBlockFound);
	void				FindPars(DownloadQueue* pDownloadQueue, const char* szNZBFilename, const char* szParFilename, 
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
	PostQueue*			LockPostQueue();
	void				UnlockPostQueue();
};

#endif
