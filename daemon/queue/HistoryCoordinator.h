/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
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


#ifndef HISTORYCOORDINATOR_H
#define HISTORYCOORDINATOR_H

#include "DownloadInfo.h"
#include "Service.h"

class HistoryCoordinator : public Service
{
public:
	void AddToHistory(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);
	bool EditList(DownloadQueue* downloadQueue, IdList* idList, DownloadQueue::EEditAction action, const char* args);
	void DeleteDiskFiles(NzbInfo* nzbInfo);
	void HistoryHide(DownloadQueue* downloadQueue, HistoryInfo* historyInfo, int rindex);
	void Redownload(DownloadQueue* downloadQueue, HistoryInfo* historyInfo);

protected:
	virtual int ServiceInterval() { return 60 * 60; }
	virtual void ServiceWork();

private:
	void HistoryDelete(DownloadQueue* downloadQueue, HistoryList::iterator itHistory, HistoryInfo* historyInfo, bool final);
	void HistoryReturn(DownloadQueue* downloadQueue, HistoryList::iterator itHistory, HistoryInfo* historyInfo);
	void HistoryProcess(DownloadQueue* downloadQueue, HistoryList::iterator itHistory, HistoryInfo* historyInfo);
	void HistoryRedownload(DownloadQueue* downloadQueue, HistoryList::iterator itHistory, HistoryInfo* historyInfo, bool restorePauseState);
	void HistoryRetry(DownloadQueue* downloadQueue, HistoryList::iterator itHistory, HistoryInfo* historyInfo, bool resetFailed, bool reprocess);
	bool HistorySetParameter(HistoryInfo* historyInfo, const char* text);
	void HistorySetDupeParam(HistoryInfo* historyInfo, DownloadQueue::EEditAction action, const char* text);
	bool HistorySetCategory(HistoryInfo* historyInfo, const char* text);
	bool HistorySetName(HistoryInfo* historyInfo, const char* text);
	void MoveToQueue(DownloadQueue* downloadQueue, HistoryList::iterator itHistory, HistoryInfo* historyInfo, bool reprocess);
	void PrepareEdit(DownloadQueue* downloadQueue, IdList* idList, DownloadQueue::EEditAction action);
	void ResetArticles(FileInfo* fileInfo, bool allFailed, bool resetFailed);
};

extern HistoryCoordinator* g_HistoryCoordinator;

#endif
