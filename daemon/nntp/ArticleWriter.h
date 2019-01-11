/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2014-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef ARTICLEWRITER_H
#define ARTICLEWRITER_H

#include "NString.h"
#include "DownloadInfo.h"
#include "Decoder.h"
#include "FileSystem.h"

class CachedSegmentData : public SegmentData
{
public:
	CachedSegmentData() {}
	CachedSegmentData(char* data, int size) : m_data(data), m_size(size) {}
	CachedSegmentData(const CachedSegmentData&) = delete;
	CachedSegmentData(CachedSegmentData&& other) :
		m_data(other.m_data), m_size(other.m_size) { other.m_data = nullptr; other.m_size = 0; }
	CachedSegmentData& operator=(CachedSegmentData&& other);
	virtual ~CachedSegmentData();
	virtual char* GetData() { return m_data; }

private:
	char* m_data = nullptr;
	int m_size = 0;

	friend class ArticleCache;
};

class ArticleWriter
{
public:
	void SetInfoName(const char* infoName) { m_infoName = infoName; }
	void SetFileInfo(FileInfo* fileInfo) { m_fileInfo = fileInfo; }
	void SetArticleInfo(ArticleInfo* articleInfo) { m_articleInfo = articleInfo; }
	void Prepare();
	bool Start(Decoder::EFormat format, const char* filename, int64 fileSize, int64 articleOffset, int articleSize);
	bool Write(char* buffer, int len);
	void Finish(bool success);
	bool GetDuplicate() { return m_duplicate; }
	void CompleteFileParts();
	static bool MoveCompletedFiles(NzbInfo* nzbInfo, const char* oldDestDir);
	void FlushCache();

private:
	FileInfo* m_fileInfo;
	ArticleInfo* m_articleInfo;
	DiskFile m_outFile;
	CString m_tempFilename;
	CString m_outputFilename;
	const char* m_resultFilename = nullptr;
	Decoder::EFormat m_format = Decoder::efUnknown;
	CachedSegmentData m_articleData;
	int64 m_articleOffset;
	int m_articleSize;
	int m_articlePtr;
	bool m_duplicate = false;
	CString m_infoName;

	bool CreateOutputFile(int64 size);
	void BuildOutputFilename();
	void SetWriteBuffer(DiskFile& outFile, int recSize);
};

class ArticleCache : public Thread
{
public:
	class FlushGuard
	{
	public:
		FlushGuard(FlushGuard&& other) = default;
		~FlushGuard();
	private:
		Guard m_guard;
		FlushGuard(Mutex& mutex);
		friend class ArticleCache;
	};

	virtual void Run();
	virtual void Stop();
	CachedSegmentData Alloc(int size);
	bool Realloc(CachedSegmentData* segment, int newSize);
	void Free(CachedSegmentData* segment);
	FlushGuard GuardFlush() { return FlushGuard(m_flushMutex); }
	Guard GuardContent() { return Guard(m_contentMutex); }
	bool GetFlushing() { return m_flushing; }
	size_t GetAllocated() { return m_allocated; }
	bool FileBusy(FileInfo* fileInfo) { return fileInfo == m_fileInfo; }

private:
	size_t m_allocated = 0;
	bool m_flushing = false;
	Mutex m_allocMutex;
	Mutex m_flushMutex;
	Mutex m_contentMutex;
	FileInfo* m_fileInfo = nullptr;
	ConditionVar m_allocCond;

	bool CheckFlush(bool flushEverything);
};

extern ArticleCache* g_ArticleCache;

#endif
