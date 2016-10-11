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
	bool HasMoreJobs() { return m_jobCount > 0; }
	int GetJobCount() { return m_jobCount; }
	bool EditList(DownloadQueue* downloadQueue, IdList* idList, DownloadQueue::EEditAction action,
		int offset, const char* text);
	void NzbAdded(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);
	void NzbDownloaded(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);

protected:
	virtual void Update(Subject* caller, void* aspect) { DownloadQueueUpdate(aspect); }

private:
	int m_jobCount = 0;
	RawNzbList m_activeJobs;
	const char* m_pauseReason = nullptr;

	bool IsNzbFileCompleted(NzbInfo* nzbInfo, bool ignorePausedPars);
	void CheckPostQueue();
	void CleanupJobs();
	NzbInfo* PickNextJob(DownloadQueue* downloadQueue, bool allowPar);
	void CheckRequestPar(DownloadQueue* downloadQueue, PostInfo* postInfo);
	void StartJob(DownloadQueue* downloadQueue, PostInfo* postInfo, bool allowPar);
	void SanitisePostQueue();
	void UpdatePauseState(bool needPause, const char* reason);
	void NzbFound(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);
	void NzbDeleted(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);
	void NzbCompleted(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, bool saveQueue);
	void JobCompleted(DownloadQueue* downloadQueue, PostInfo* postInfo);
	bool PostQueueDelete(DownloadQueue* downloadQueue, IdList* idList);
	void DeletePostThread(PostInfo* postInfo);
	void DownloadQueueUpdate(void* aspect);
	void DeleteCleanup(NzbInfo* nzbInfo);
};

extern PrePostProcessor* g_PrePostProcessor;

#endif
