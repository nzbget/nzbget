/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
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


#ifndef QUEUECOORDINATOR_H
#define QUEUECOORDINATOR_H

#include <deque>
#include <list>

#include "Log.h"
#include "Thread.h"
#include "NZBFile.h"
#include "ArticleDownloader.h"
#include "DownloadInfo.h"
#include "Observer.h"
#include "QueueEditor.h"
#include "NNTPConnection.h"
                                            
class QueueCoordinator : public Thread, public Observer, public Debuggable
{
public:
	typedef std::list<ArticleDownloader*>	ActiveDownloads;

private:
	class CoordinatorDownloadQueue : public DownloadQueue
	{
	private:
		QueueCoordinator*	m_pOwner;
		friend class QueueCoordinator;
	public:
		virtual bool		EditEntry(int ID, EEditAction eAction, int iOffset, const char* szText);
		virtual bool		EditList(IDList* pIDList, NameList* pNameList, EMatchMode eMatchMode, EEditAction eAction, int iOffset, const char* szText);
		virtual void		Save();
	};

private:
	CoordinatorDownloadQueue	m_DownloadQueue;
	ActiveDownloads				m_ActiveDownloads;
	QueueEditor					m_QueueEditor;
	bool						m_bHasMoreJobs;
	int							m_iDownloadsLimit;
	int							m_iServerConfigGeneration;

	bool					GetNextArticle(DownloadQueue* pDownloadQueue, FileInfo* &pFileInfo, ArticleInfo* &pArticleInfo);
	void					StartArticleDownload(FileInfo* pFileInfo, ArticleInfo* pArticleInfo, NNTPConnection* pConnection);
	void					ArticleCompleted(ArticleDownloader* pArticleDownloader);
	void					DeleteFileInfo(DownloadQueue* pDownloadQueue, FileInfo* pFileInfo, bool bCompleted);
	void					StatFileInfo(FileInfo* pFileInfo, bool bCompleted);
	void					CheckHealth(DownloadQueue* pDownloadQueue, FileInfo* pFileInfo);
	void					ResetHangingDownloads();
	void					AdjustDownloadsLimit();

protected:
	virtual void			LogDebugInfo();

public:
							QueueCoordinator();                
	virtual					~QueueCoordinator();
	virtual void			Run();
	virtual void 			Stop();
	void					Update(Subject* Caller, void* Aspect);

	// editing queue
	void					AddNZBFileToQueue(NZBFile* pNZBFile, NZBInfo* pUrlInfo, bool bAddFirst);
	void					CheckDupeFileInfos(NZBInfo* pNZBInfo);
	bool					HasMoreJobs() { return m_bHasMoreJobs; }
	void					DiscardDiskFile(FileInfo* pFileInfo);
	bool					DeleteQueueEntry(DownloadQueue* pDownloadQueue, FileInfo* pFileInfo);
	bool					SetQueueEntryCategory(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, const char* szCategory);
	bool					SetQueueEntryName(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, const char* szName);
	bool					MergeQueueEntries(DownloadQueue* pDownloadQueue, NZBInfo* pDestNZBInfo, NZBInfo* pSrcNZBInfo);
	bool					SplitQueueEntries(DownloadQueue* pDownloadQueue, FileList* pFileList, const char* szName, NZBInfo** pNewNZBInfo);
};

#endif
