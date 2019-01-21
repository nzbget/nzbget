/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef PREPOSTPROCESSOR_H
#define PREPOSTPROCESSOR_H

#include "Thread.h"
#include "Observer.h"
#include "DownloadInfo.h"

class PrePostProcessor : public Thread, public Observer
{
public:
	PrePostProcessor();
	virtual void Run();
	virtual void Stop();
	bool HasMoreJobs() { return m_queuedJobs > 0; }
	int GetJobCount() { return m_queuedJobs; }
	bool EditList(DownloadQueue* downloadQueue, IdList* idList, DownloadQueue::EEditAction action,
		const char* args);
	void NzbAdded(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);
	void NzbDownloaded(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);

protected:
	virtual void Update(Subject* caller, void* aspect) { DownloadQueueUpdate(aspect); }

private:
	int m_queuedJobs = 0;
	RawNzbList m_activeJobs;
	Mutex m_waitMutex;
	ConditionVar m_waitCond;

	void CheckPostQueue();
	void CheckRequestPar(DownloadQueue* downloadQueue);
	void CleanupJobs(DownloadQueue* downloadQueue);
	bool CanRunMoreJobs(bool* allowPar);
	NzbInfo* PickNextJob(DownloadQueue* downloadQueue, bool allowPar);
	void StartJob(DownloadQueue* downloadQueue, PostInfo* postInfo, bool allowPar);
	void EnterStage(DownloadQueue* downloadQueue, PostInfo* postInfo, PostInfo::EStage stage);
	void SanitisePostQueue();
	void UpdatePauseState();
	void NzbFound(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);
	void NzbDeleted(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);
	void NzbCompleted(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, bool saveQueue);
	void JobCompleted(DownloadQueue* downloadQueue, PostInfo* postInfo);
	bool PostQueueDelete(DownloadQueue* downloadQueue, IdList* idList);
	void DownloadQueueUpdate(void* aspect);
	void DeleteCleanup(NzbInfo* nzbInfo);
	void WaitJobs();
	void FileDownloaded(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, FileInfo* fileInfo);
};

extern PrePostProcessor* g_PrePostProcessor;

#endif
