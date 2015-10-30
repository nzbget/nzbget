/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2014-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef ARTICLEWRITER_H
#define ARTICLEWRITER_H

#include "DownloadInfo.h"
#include "Decoder.h"

class ArticleWriter
{
private:
	FileInfo*			m_fileInfo;
	ArticleInfo*		m_articleInfo;
	FILE*				m_outFile;
	char*				m_tempFilename;
	char*				m_outputFilename;
	const char*			m_resultFilename;
	Decoder::EFormat	m_format;
	char*				m_articleData;
	long long			m_articleOffset;
	int					m_articleSize;
	int					m_articlePtr;
	bool				m_flushing;
	bool				m_duplicate;
	char*				m_infoName;

	bool				PrepareFile(char* line);
	bool				CreateOutputFile(long long size);
	void				BuildOutputFilename();
	bool				IsFileCached();
	void				SetWriteBuffer(FILE* outFile, int recSize);

protected:
	virtual void		SetLastUpdateTimeNow() {}

public:
						ArticleWriter();
						~ArticleWriter();
	void				SetInfoName(const char* infoName);
	void				SetFileInfo(FileInfo* fileInfo) { m_fileInfo = fileInfo; }
	void				SetArticleInfo(ArticleInfo* articleInfo) { m_articleInfo = articleInfo; }
	void				Prepare();
	bool				Start(Decoder::EFormat format, const char* filename, long long fileSize, long long articleOffset, int articleSize);
	bool				Write(char* bufffer, int len);
	void				Finish(bool success);
	bool				GetDuplicate() { return m_duplicate; }
	void				CompleteFileParts();
	static bool			MoveCompletedFiles(NzbInfo* nzbInfo, const char* oldDestDir);
	void				FlushCache();
};

class ArticleCache : public Thread
{
private:
	size_t				m_allocated;
	bool				m_flushing;
	Mutex				m_allocMutex;
	Mutex				m_flushMutex;
	Mutex				m_contentMutex;
	FileInfo*			m_fileInfo;

	bool				CheckFlush(bool flushEverything);

public:
						ArticleCache();
	virtual void		Run();
	void*				Alloc(int size);
	void*				Realloc(void* buf, int oldSize, int newSize);
	void				Free(int size);
	void				LockFlush();
	void				UnlockFlush();
	void				LockContent() { m_contentMutex.Lock(); }
	void				UnlockContent() { m_contentMutex.Unlock(); }
	bool				GetFlushing() { return m_flushing; }
	size_t				GetAllocated() { return m_allocated; }
	bool				FileBusy(FileInfo* fileInfo) { return fileInfo == m_fileInfo; }
};

extern ArticleCache* g_pArticleCache;

#endif
