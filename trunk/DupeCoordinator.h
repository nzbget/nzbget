/*
 *  This file is part of nzbget
 *
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


#ifndef DUPECOORDINATOR_H
#define DUPECOORDINATOR_H

#include <deque>

#include "DownloadInfo.h"

class DupeCoordinator
{
private:
	bool				IsDupeSuccess(NZBInfo* pNZBInfo);
	void				UnpauseBestDupe(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, const char* szNZBName, const char* szDupeKey);
	void				RemoveDupes(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	void				HistoryReturnDupe(DownloadQueue* pDownloadQueue, HistoryInfo* pHistoryInfo);
	void				HistoryCleanup(DownloadQueue* pDownloadQueue, HistoryInfo* pMarkHistoryInfo);
	bool				SameNameOrKey(const char* szName1, const char* szDupeKey1, const char* szName2, const char* szDupeKey2);

protected:
	virtual void		HistoryReturn(DownloadQueue* pDownloadQueue, HistoryList::iterator itHistory, HistoryInfo* pHistoryInfo, bool bReprocess) = 0;
	virtual void		DeleteQueuedFile(const char* szQueuedFile) = 0;

public:
	void				NZBCompleted(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	void				NZBFound(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	void				NZBAdded(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	void				HistoryMark(DownloadQueue* pDownloadQueue, HistoryInfo* pHistoryInfo, bool bGood);
	void				HistoryTransformToDup(DownloadQueue* pDownloadQueue, HistoryInfo* pHistoryInfo, int rindex);
};

#endif
