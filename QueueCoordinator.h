/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
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


#ifndef QUEUECOORDINATOR_H
#define QUEUECOORDINATOR_H

#include <deque>
#include <list>
#include <time.h>

#include "Thread.h"
#include "NZBFile.h"
#include "ArticleDownloader.h"
#include "DownloadInfo.h"
#include "Observer.h"
#include "QueueEditor.h"
#include "NNTPConnection.h"
                                            
class QueueCoordinator : public Thread, public Observer, public Subject, public DownloadSpeedMeter, public DownloadQueueHolder
{
public:
	typedef std::list<ArticleDownloader*>	ActiveDownloads;

	enum EAspectAction
	{
		eaNZBFileFound,
		eaNZBFileAdded,
		eaFileCompleted,
		eaFileDeleted
	};

	struct Aspect
	{
		EAspectAction eAction;
		DownloadQueue* pDownloadQueue;
		NZBInfo* pNZBInfo;
		FileInfo* pFileInfo;
	};

private:
	DownloadQueue			m_DownloadQueue;
	ActiveDownloads			m_ActiveDownloads;
	QueueEditor				m_QueueEditor;
	Mutex			 		m_mutexDownloadQueue;
	bool					m_bHasMoreJobs;
	int						m_iDownloadsLimit;
	int						m_iServerConfigGeneration;

	// statistics
	static const int		SPEEDMETER_SLOTS = 30;    
	static const int		SPEEDMETER_SLOTSIZE = 1;  //Split elapsed time into this number of secs.
    int						m_iSpeedBytes[SPEEDMETER_SLOTS];
    int                     m_iSpeedTotalBytes;
    int 					m_iSpeedTime[SPEEDMETER_SLOTS];
    int                     m_iSpeedStartTime; 
	time_t					m_tSpeedCorrection;
#ifdef HAVE_SPINLOCK
	SpinLock				m_spinlockSpeed;
#else
	Mutex					m_mutexSpeed;
#endif

    int						m_iSpeedBytesIndex;
	long long				m_iAllBytes;
	time_t					m_tStartServer;
	time_t					m_tLastCheck;
	time_t					m_tStartDownload;
	time_t					m_tPausedFrom;
	bool					m_bStandBy;
	Mutex					m_mutexStat;

	bool					GetNextArticle(FileInfo* &pFileInfo, ArticleInfo* &pArticleInfo);
	void					StartArticleDownload(FileInfo* pFileInfo, ArticleInfo* pArticleInfo, NNTPConnection* pConnection);
	void					ArticleCompleted(ArticleDownloader* pArticleDownloader);
	void					DeleteFileInfo(FileInfo* pFileInfo, bool bCompleted);
	void					StatFileInfo(FileInfo* pFileInfo, bool bCompleted);
	void					CheckHealth(FileInfo* pFileInfo);
	void					ResetHangingDownloads();
	void					ResetSpeedStat();
	void					EnterLeaveStandBy(bool bEnter);
	void					AdjustStartTime();
	void					AdjustDownloadsLimit();

public:
							QueueCoordinator();                
	virtual					~QueueCoordinator();
	virtual void			Run();
	virtual void 			Stop();
	void					Update(Subject* Caller, void* Aspect);

	// statistics
	long long 				CalcRemainingSize();
	virtual int				CalcCurrentDownloadSpeed();
	virtual void			AddSpeedReading(int iBytes);
	void					CalcStat(int* iUpTimeSec, int* iDnTimeSec, long long* iAllBytes, bool* bStandBy);

	// Editing the queue
	DownloadQueue*			LockQueue();
	void					UnlockQueue() ;
	void					AddNZBFileToQueue(NZBFile* pNZBFile, bool bAddFirst);
	void					CheckDupeFileInfos(NZBInfo* pNZBInfo);
	bool					HasMoreJobs() { return m_bHasMoreJobs; }
	bool					GetStandBy() { return m_bStandBy; }
	bool					DeleteQueueEntry(FileInfo* pFileInfo);
	bool					SetQueueEntryNZBCategory(NZBInfo* pNZBInfo, const char* szCategory);
	bool					SetQueueEntryNZBName(NZBInfo* pNZBInfo, const char* szName);
	bool					MergeQueueEntries(NZBInfo* pDestNZBInfo, NZBInfo* pSrcNZBInfo);
	bool					SplitQueueEntries(FileList* pFileList, const char* szName, NZBInfo** pNewNZBInfo);
	void					DiscardDiskFile(FileInfo* pFileInfo);
	QueueEditor*			GetQueueEditor() { return &m_QueueEditor; }

	void					LogDebugInfo();
};

#endif
