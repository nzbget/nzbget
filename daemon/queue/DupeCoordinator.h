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
 * $Revision$
 * $Date$
 *
 */


#ifndef DUPECOORDINATOR_H
#define DUPECOORDINATOR_H

#include "DownloadInfo.h"

class DupeCoordinator
{
public:
	enum EDupeStatus
	{
		dsNone = 0,
		dsQueued = 1,
		dsDownloading = 2,
		dsSuccess = 4,
		dsWarning = 8,
		dsFailure = 16
	};

private:
	void				ReturnBestDupe(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, const char* nzbName, const char* dupeKey);
	void				HistoryCleanup(DownloadQueue* downloadQueue, HistoryInfo* markHistoryInfo);
	bool				SameNameOrKey(const char* name1, const char* dupeKey1, const char* name2, const char* dupeKey2);

public:
	void				NzbCompleted(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);
	void				NzbFound(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);
	void				HistoryMark(DownloadQueue* downloadQueue, HistoryInfo* historyInfo, NzbInfo::EMarkStatus markStatus);
	EDupeStatus			GetDupeStatus(DownloadQueue* downloadQueue, const char* name, const char* dupeKey);
	void				ListHistoryDupes(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, NzbList* dupeList);
};

extern DupeCoordinator* g_pDupeCoordinator;

#endif
