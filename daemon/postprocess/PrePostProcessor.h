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
#include "ParCoordinator.h"

class PrePostProcessor : public Thread
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

private:
	class DownloadQueueObserver: public Observer
	{
	public:
		PrePostProcessor* m_owner;
		virtual void Update(Subject* Caller, void* Aspect) { m_owner->DownloadQueueUpdate(Caller, Aspect); }
	};

	ParCoordinator m_parCoordinator;
	DownloadQueueObserver m_downloadQueueObserver;
	int m_jobCount = 0;
	NzbInfo* m_curJob = nullptr;
	const char* m_pauseReason = nullptr;

	bool IsNzbFileCompleted(NzbInfo* nzbInfo, bool ignorePausedPars, bool allowOnlyOneDeleted);
	bool IsNzbFileDownloading(NzbInfo* nzbInfo);
	void CheckPostQueue();
	void JobCompleted(DownloadQueue* downloadQueue, PostInfo* postInfo);
	void StartJob(DownloadQueue* downloadQueue, PostInfo* postInfo);
	void SanitisePostQueue();
	void UpdatePauseState(bool needPause, const char* reason);
	void NzbFound(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);
	void NzbDeleted(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);
	void NzbCompleted(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, bool saveQueue);
	bool PostQueueDelete(DownloadQueue* downloadQueue, IdList* idList);
	void DeletePostThread(PostInfo* postInfo);
	NzbInfo* GetNextJob(DownloadQueue* downloadQueue);
	void DownloadQueueUpdate(Subject* Caller, void* Aspect);
	void DeleteCleanup(NzbInfo* nzbInfo);
};

extern PrePostProcessor* g_PrePostProcessor;

#endif
