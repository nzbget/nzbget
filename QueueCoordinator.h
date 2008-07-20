/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2004  Sven Henkel <sidddy@users.sourceforge.net>
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


#ifndef QUEUECOORDINATOR_H
#define QUEUECOORDINATOR_H

#include <deque>
#include <list>
#include <time.h>
#ifdef WIN32
#include <sys/timeb.h>
#endif

#include "Thread.h"
#include "NZBFile.h"
#include "ArticleDownloader.h"
#include "DownloadInfo.h"
#include "Observer.h"
#include "QueueEditor.h"
#include "NNTPConnection.h"
                                            
class QueueCoordinator : public Thread, public Observer, public Subject, public DownloadSpeedMeter, public NZBInfoLocker
{
public:
	typedef std::list<ArticleDownloader*>	ActiveDownloads;
	enum EAspectAction
	{
		eaNZBFileAdded,
		eaFileCompleted,
		eaFileDeleted
	};
	struct Aspect
	{
		EAspectAction eAction;
		FileInfo* pFileInfo;
		DownloadQueue* pDownloadQueue;
		const char* szNZBFilename;
	};

private:
	DownloadQueue			m_DownloadQueue;
	ActiveDownloads			m_ActiveDownloads;
	QueueEditor				m_QueueEditor;
	Mutex			 		m_mutexDownloadQueue;
	bool					m_bHasMoreJobs;

	// statistics
	static const int		SPEEDMETER_SLOTS = 30;    
	static const int		SPEEDMETER_SLOTSIZE = 1;  //Split elapsed time into this number of secs.
    int						m_iSpeedBytes[SPEEDMETER_SLOTS];
    int                     m_iSpeedTotalBytes;
    int 					m_iSpeedTime[SPEEDMETER_SLOTS];
    int                     m_iSpeedStartTime; 

    int						m_iSpeedBytesIndex;
	long long				m_iAllBytes;
	time_t					m_tStartServer;
	time_t					m_tStartDownload;
	time_t					m_tPausedFrom;
	bool					m_bStandBy;
	Mutex					m_mutexStat;

	bool					GetNextArticle(FileInfo* &pFileInfo, ArticleInfo* &pArticleInfo);
	void					StartArticleDownload(FileInfo* pFileInfo, ArticleInfo* pArticleInfo, NNTPConnection* pConnection);
	void					BuildArticleFilename(ArticleDownloader* pArticleDownloader, FileInfo* pFileInfo, ArticleInfo* pArticleInfo);
	bool					IsDupe(FileInfo* pFileInfo);
	void					ArticleCompleted(ArticleDownloader* pArticleDownloader);
	void					DeleteFileInfo(FileInfo* pFileInfo, bool bCompleted);
	void					ResetHangingDownloads();
	void					ResetSpeedStat();
	void					EnterLeaveStandBy(bool bEnter);

public:
							QueueCoordinator();                
	virtual					~QueueCoordinator();
	virtual void			Run();
	virtual void 			Stop();
	void					Update(Subject* Caller, void* Aspect);

	// statistics
	long long 				CalcRemainingSize();
	virtual float			CalcCurrentDownloadSpeed();
	virtual void			AddSpeedReading(int iBytes);
	void					CalcStat(int* iUpTimeSec, int* iDnTimeSec, long long* iAllBytes, bool* bStandBy);

	// Editing the queue
	DownloadQueue*			LockQueue();
	void					UnlockQueue() ;
	void					AddNZBFileToQueue(NZBFile* pNZBQueue, bool bAddFirst);
	bool					AddFileToQueue(const char* szFileName, const char* szCategory);
	bool					HasMoreJobs() { return m_bHasMoreJobs; }
	bool					GetStandBy() { return m_bStandBy; }
	bool					DeleteQueueEntry(FileInfo* pFileInfo);
	bool					SetQueueEntryNZBCategory(NZBInfo* pNZBInfo, const char* szCategory);
	QueueEditor*			GetQueueEditor() { return &m_QueueEditor; }

	virtual void			LockNZBInfo(NZBInfo* pNZBInfo) { LockQueue(); }
	virtual void			UnlockNZBInfo(NZBInfo* pNZBInfo) { UnlockQueue(); }

	void					LogDebugInfo();
};

#endif
