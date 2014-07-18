/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
	FileInfo*			m_pFileInfo;
	ArticleInfo*		m_pArticleInfo;
	FILE*				m_pOutFile;
	char*				m_szTempFilename;
	char*				m_szOutputFilename;
	const char*			m_szResultFilename;
	Decoder::EFormat	m_eFormat;
	char*				m_pArticleData;
	int					m_iArticleOffset;
	int					m_iArticleSize;
	int					m_iArticlePtr;
	bool				m_bFlushing;
	bool				m_bDuplicate;
	char*				m_szInfoName;
	bool				m_bNeedFilename;

	bool				PrepareFile(char* szLine);
	bool				CreateOutputFile(int iSize);
	void				BuildOutputFilename();
	bool				IsFileCached();

protected:
	virtual void		SetLastUpdateTimeNow() {}

public:
						ArticleWriter();
						~ArticleWriter();
	void				SetInfoName(const char* szInfoName);
	void				SetFileInfo(FileInfo* pFileInfo) { m_pFileInfo = pFileInfo; }
	void				SetArticleInfo(ArticleInfo* pArticleInfo) { m_pArticleInfo = pArticleInfo; }
	bool				Start(Decoder::EFormat eFormat, const char* szFilename, int iFileSize, int iArticleOffset, int iArticleSize);
	bool				Write(char* szBufffer, int iLen);
	void				Finish(bool bSuccess);
	bool				GetDuplicate() { return m_bDuplicate; }
	void				CompleteFileParts();
	static bool			MoveCompletedFiles(NZBInfo* pNZBInfo, const char* szOldDestDir);
	void				FlushCache();
};

class ArticleCache : public Thread
{
private:
	size_t				m_iAllocated;
	bool				m_bFlushing;
	Mutex				m_mutexAlloc;
	Mutex				m_mutexFlush;
	Mutex				m_mutexContent;
	FileInfo*			m_pFileInfo;

	bool				CheckFlush(bool bFlushEverything);

public:
						ArticleCache();
	virtual void		Run();
	void*				Alloc(int iSize);
	void*				Realloc(void* buf, int iOldSize, int iNewSize);
	void				Free(int iSize);
	void				LockFlush();
	void				UnlockFlush();
	void				LockContent() { m_mutexContent.Lock(); }
	void				UnlockContent() { m_mutexContent.Unlock(); }
	bool				GetFlushing() { return m_bFlushing; }
	size_t				GetAllocated() { return m_iAllocated; }
	bool				FileBusy(FileInfo* pFileInfo) { return pFileInfo == m_pFileInfo; }
};

#endif
