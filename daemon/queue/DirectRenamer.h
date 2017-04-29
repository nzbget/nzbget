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
	class FileHash
	{
	public:
		FileHash(const char* filename, const char* hash) :
			m_filename(filename), m_hash(hash) {}
		const char* GetFilename() { return m_filename; }
		const char* GetHash() { return m_hash; }

	private:
		CString m_filename;
		CString m_hash;
	};

	typedef std::deque<FileHash> FileHashList;

	class ParFile
	{
	public:
		ParFile(int id, const char* filename, const char* setId, bool completed) :
			m_id(id), m_filename(filename), m_setId(setId), m_completed(completed) {}
		int GetId() { return m_id; }
		const char* GetFilename() { return m_filename; }
		const char* GetSetId() { return m_setId; }
		bool GetCompleted() { return m_completed; }

	private:
		int m_id;
		CString m_filename;
		CString m_setId;
		bool m_completed = false;
	};

	typedef std::deque<ParFile> ParFileList;

	std::unique_ptr<ArticleContentAnalyzer> MakeArticleContentAnalyzer();
	void ArticleDownloaded(DownloadQueue* downloadQueue, FileInfo* fileInfo,
		ArticleInfo* articleInfo, ArticleContentAnalyzer* articleContentAnalyzer);
	void FileDownloaded(DownloadQueue* downloadQueue, FileInfo* fileInfo);

protected:
	virtual void RenameCompleted(DownloadQueue* downloadQueue, NzbInfo* nzbInfo) = 0;

private:
	void CheckState(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);
	void UnpausePars(NzbInfo* nzbInfo);
	void RenameFiles(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, FileHashList* parHashes);
	bool RenameCompletedFile(NzbInfo* nzbInfo, const char* oldName, const char* newName);
	bool NeedRenamePars(NzbInfo* nzbInfo);
	void CollectPars(NzbInfo* nzbInfo, ParFileList* parFiles);
	CString BuildNewRegularName(const char* oldName, FileHashList* parHashes, const char* hash16k);
	CString BuildNewParName(const char* oldName, const char* destDir, const char* setId, int& vol);

	friend class DirectParLoader;
};

#endif
