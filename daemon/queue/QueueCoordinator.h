/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef QUEUECOORDINATOR_H
#define QUEUECOORDINATOR_H

#include "Log.h"
#include "Thread.h"
#include "NzbFile.h"
#include "ArticleDownloader.h"
#include "DownloadInfo.h"
#include "Observer.h"
#include "QueueEditor.h"
#include "NntpConnection.h"
#include "DirectRenamer.h"

class QueueCoordinator : public Thread, public Observer, public Debuggable
{
public:
	typedef std::list<ArticleDownloader*> ActiveDownloads;

	QueueCoordinator();
	virtual ~QueueCoordinator();
	virtual void Run();
	virtual void Stop();
	void Update(Subject* caller, void* aspect);

	// editing queue
	NzbInfo* AddNzbFileToQueue(std::unique_ptr<NzbInfo> nzbInfo, NzbInfo* urlInfo, bool addFirst);
	void CheckDupeFileInfos(NzbInfo* nzbInfo);
	bool HasMoreJobs() { return m_hasMoreJobs; }
	void DiscardTempFiles(FileInfo* fileInfo);
	bool DeleteQueueEntry(DownloadQueue* downloadQueue, FileInfo* fileInfo);
	bool SetQueueEntryCategory(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, const char* category);
	bool SetQueueEntryName(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, const char* name);
	bool MergeQueueEntries(DownloadQueue* downloadQueue, NzbInfo* destNzbInfo, NzbInfo* srcNzbInfo);
	bool SplitQueueEntries(DownloadQueue* downloadQueue, RawFileList* fileList, const char* name, NzbInfo** newNzbInfo);

protected:
	virtual void LogDebugInfo();

private:
	class CoordinatorDownloadQueue : public DownloadQueue
	{
	public:
		CoordinatorDownloadQueue(QueueCoordinator* owner) : m_owner(owner) {}
		virtual bool EditEntry(int ID, EEditAction action, const char* args);
		virtual bool EditList(IdList* idList, NameList* nameList, EMatchMode matchMode,
			EEditAction action, const char* args);
		virtual void HistoryChanged() { m_historyChanged = true; }
		virtual void Save();
		virtual void SaveChanged();
	private:
		QueueCoordinator* m_owner;
		bool m_massEdit = false;
		bool m_wantSave = false;
		bool m_historyChanged = false;
		bool m_stateChanged = false;
		friend class QueueCoordinator;
	};

	class CoordinatorDirectRenamer : public DirectRenamer
	{
	public:
		CoordinatorDirectRenamer(QueueCoordinator* owner) : m_owner(owner) {}
	protected:
		virtual void RenameCompleted(DownloadQueue* downloadQueue, NzbInfo* nzbInfo)
			{ m_owner->DirectRenameCompleted(downloadQueue, nzbInfo); }
	private:
		QueueCoordinator* m_owner;
	};

	CoordinatorDownloadQueue m_downloadQueue{this};
	ActiveDownloads m_activeDownloads;
	QueueEditor m_queueEditor;
	CoordinatorDirectRenamer m_directRenamer{this};
	bool m_hasMoreJobs = true;
	int m_downloadsLimit;
	int m_serverConfigGeneration = 0;
	Mutex m_waitMutex;
	ConditionVar m_waitCond;

	bool GetNextArticle(DownloadQueue* downloadQueue, FileInfo* &fileInfo, ArticleInfo* &articleInfo);
	bool GetNextFirstArticle(NzbInfo* nzbInfo, FileInfo* &fileInfo, ArticleInfo* &articleInfo);
	void StartArticleDownload(FileInfo* fileInfo, ArticleInfo* articleInfo, NntpConnection* connection);
	void ArticleCompleted(ArticleDownloader* articleDownloader);
	void DeleteDownloader(DownloadQueue* downloadQueue, ArticleDownloader* articleDownloader, bool fileCompleted);
	void DeleteFileInfo(DownloadQueue* downloadQueue, FileInfo* fileInfo, bool completed);
	void DirectRenameCompleted(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);
	void DiscardDirectRename(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);
	void DiscardDownloadedArticles(NzbInfo* nzbInfo, FileInfo* fileInfo);
	void CheckHealth(DownloadQueue* downloadQueue, FileInfo* fileInfo);
	void ResetHangingDownloads();
	void AdjustDownloadsLimit();
	void Load();
	void SaveQueueIfChanged();
	void SaveAllPartialState();
	void SavePartialState(FileInfo* fileInfo);
	void LoadPartialState(FileInfo* fileInfo);
	void SaveAllFileState();
	void WaitJobs();
	void WakeUp();
};

extern QueueCoordinator* g_QueueCoordinator;

#endif
