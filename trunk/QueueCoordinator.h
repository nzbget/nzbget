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

#include "Thread.h"
#include "NZBFile.h"
#include "ArticleDownloader.h"
#include "DownloadInfo.h"
#include "Observer.h"
#include "DiskState.h"
#include "QueueEditor.h"
                                            
class QueueCoordinator : public Thread, public Observer, public Subject, public DownloadSpeedMeter
{
public:
	typedef std::list<ArticleDownloader*>	ActiveDownloads;
	typedef enum EAspectAction
	{
		eaNZBFileAdded,
		eaFileCompleted,
		eaFileDeleted
	};
	typedef struct Aspect
	{
		EAspectAction eAction;
		FileInfo* pFileInfo;
		DownloadQueue* pDownloadQueue;
		const char* szNZBFilename;
	};

private:
	DownloadQueue			m_DownloadQueue;
	ActiveDownloads			m_ActiveDownloads;
	DiskState				m_DiskState;
	QueueEditor				m_QueueEditor;
	Mutex			 		m_mutexDownloadQueue;
	bool					m_bHasMoreJobs;

	bool					GetNextArticle(FileInfo* &pFileInfo, ArticleInfo* &pArticleInfo);
	void					StartArticleDownload(FileInfo* pFileInfo, ArticleInfo* pArticleInfo);
	void					BuildArticleFilename(ArticleDownloader* pArticleDownloader, FileInfo* pFileInfo, ArticleInfo* pArticleInfo);
	bool					IsDupe(FileInfo* pFileInfo);
	void					ArticleCompleted(ArticleDownloader* pArticleDownloader);
	void					DeleteFileInfo(FileInfo* pFileInfo);
	void					ResetHangingDownloads();

public:
							QueueCoordinator();                
	virtual					~QueueCoordinator();
	virtual void			Run();
	virtual void 			Stop();
	long long 				CalcRemainingSize();                      
	virtual float			CalcCurrentDownloadSpeed();
	void					Update(Subject* Caller, void* Aspect);

	// Editing the queue
	DownloadQueue*			LockQueue();
	void					UnlockQueue() ;
	void					AddNZBFileToQueue(NZBFile* pNZBQueue, bool bAddFirst);
	bool					AddFileToQueue(const char* szFileName);
	bool					HasMoreJobs() { return m_bHasMoreJobs; }
	bool					DeleteQueueEntry(FileInfo* pFileInfo);
	QueueEditor*			GetQueueEditor() { return &m_QueueEditor; }
	
	void					LogDebugInfo();
};

#endif
