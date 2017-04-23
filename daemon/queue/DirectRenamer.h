/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef DIRECTRENAMER_H
#define DIRECTRENAMER_H

#include "ArticleDownloader.h"

class DirectRenamer
{
public:
	std::unique_ptr<ArticleContentAnalyzer> MakeArticleContentAnalyzer();
	void ArticleDownloaded(DownloadQueue* downloadQueue, FileInfo* fileInfo,
		ArticleInfo* articleInfo, ArticleContentAnalyzer* articleContentAnalyzer);
	void FileDownloaded(DownloadQueue* downloadQueue, FileInfo* fileInfo);

protected:
	virtual void RenameCompleted(DownloadQueue* downloadQueue, NzbInfo* nzbInfo) = 0;

private:
	void CheckState(NzbInfo* nzbInfo);
	void UnpausePars(NzbInfo* nzbInfo);
	void RenameFiles(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, RenameInfo::FileHashList* parHashes);
	void RenameFile(NzbInfo* nzbInfo, const char* oldName, const char* newName);
	bool NeedRenamePars(NzbInfo* nzbInfo);

	friend class DirectParLoader;
};

#endif
