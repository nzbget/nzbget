/*
 *  This file is part of nzbget
 *
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


#ifndef DISKSTATE_H
#define DISKSTATE_H

#include "DownloadInfo.h"
#include "PostInfo.h"

class DiskState
{
private:
	bool				SaveFileInfo(FileInfo* pFileInfo, const char* szFilename);
	bool				LoadFileInfo(FileInfo* pFileInfo, const char* szFilename, bool bFileSummary, bool bArticles);
	int					ParseFormatVersion(const char* szFormatSignature);
	
public:
	bool				DownloadQueueExists();
	bool				PostQueueExists(bool bCompleted);
	bool				SaveDownloadQueue(DownloadQueue* pDownloadQueue);
	bool				LoadDownloadQueue(DownloadQueue* pDownloadQueue);
	bool				SaveFile(FileInfo* pFileInfo);
	bool				LoadArticles(FileInfo* pFileInfo);
	bool				SavePostQueue(PostQueue* pPostQueue, bool bCompleted);
	bool				LoadPostQueue(PostQueue* pPostQueue, bool bCompleted);
	bool				DiscardDownloadQueue();
	bool				DiscardPostQueue();
	bool				DiscardFile(DownloadQueue* pDownloadQueue, FileInfo* pFileInfo);
	void				CleanupTempDir(DownloadQueue* pDownloadQueue);
};

#endif
