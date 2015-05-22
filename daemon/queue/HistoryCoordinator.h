/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 * $Revision: 951 $
 * $Date$
 *
 */


#ifndef HISTORYCOORDINATOR_H
#define HISTORYCOORDINATOR_H

#include "DownloadInfo.h"

class HistoryCoordinator
{
private:
	void				HistoryDelete(DownloadQueue* pDownloadQueue, HistoryList::iterator itHistory, HistoryInfo* pHistoryInfo, bool bFinal);
	void				HistoryReturn(DownloadQueue* pDownloadQueue, HistoryList::iterator itHistory, HistoryInfo* pHistoryInfo, bool bReprocess);
	void				HistoryRedownload(DownloadQueue* pDownloadQueue, HistoryList::iterator itHistory, HistoryInfo* pHistoryInfo, bool bRestorePauseState);
	bool				HistorySetParameter(HistoryInfo* pHistoryInfo, const char* szText);
	void				HistorySetDupeParam(HistoryInfo* pHistoryInfo, DownloadQueue::EEditAction eAction, const char* szText);
	bool				HistorySetCategory(HistoryInfo* pHistoryInfo, const char* szText);
	bool				HistorySetName(HistoryInfo* pHistoryInfo, const char* szText);
	void				HistoryTransformToDup(DownloadQueue* pDownloadQueue, HistoryInfo* pHistoryInfo, int rindex);
	void				SaveQueue(DownloadQueue* pDownloadQueue);

public:
						HistoryCoordinator();
	virtual				~HistoryCoordinator();
	void				AddToHistory(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	bool				EditList(DownloadQueue* pDownloadQueue, IDList* pIDList, DownloadQueue::EEditAction eAction, int iOffset, const char* szText);
	void				DeleteDiskFiles(NZBInfo* pNZBInfo);
	void				HistoryHide(DownloadQueue* pDownloadQueue, HistoryInfo* pHistoryInfo, int rindex);
	void				Redownload(DownloadQueue* pDownloadQueue, HistoryInfo* pHistoryInfo);
	void				IntervalCheck();
	void				Cleanup();
};

extern HistoryCoordinator* g_pHistoryCoordinator;

#endif
