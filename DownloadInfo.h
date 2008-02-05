/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004  Sven Henkel <sidddy@users.sourceforge.net>
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


#ifndef DOWNLOADINFO_H
#define DOWNLOADINFO_H

#include <vector>
#include <deque>

#include "Thread.h"

class NZBInfo
{
private:
	int					m_iRefCount;
	char* 				m_szFilename;
	char* 				m_szDestDir;
	int		 			m_iFileCount;
	long long 			m_lSize;

public:
						NZBInfo();
						~NZBInfo();
	void				AddReference();
	void				Release();
	const char*			GetFilename() { return m_szFilename; }
	void				SetFilename(const char* szFilename);
	void				GetNiceNZBName(char* szBuffer, int iSize);
	static void			MakeNiceNZBName(const char* szNZBFilename, char* szBuffer, int iSize);
	const char*			GetDestDir() { return m_szDestDir; }
	void				SetDestDir(const char* szDestDir);
	long long 			GetSize() { return m_lSize; }
	void 				SetSize(long long s) { m_lSize = s; }
	int					GetFileCount() { return m_iFileCount; }
	void 				SetFileCount(int s) { m_iFileCount = s; }
};

class ArticleInfo
{
public:
	enum EStatus
	{
		aiUndefined,
		aiRunning,
		aiFinished,
		aiFailed
	};
	
private:
	int					m_iPartNumber;
	char*				m_szMessageID;
	int					m_iSize;
	EStatus				m_eStatus;
	char*				m_szResultFilename;

public:
						ArticleInfo();
						~ArticleInfo();
	void 				SetPartNumber(int s) { m_iPartNumber = s; }
	int 				GetPartNumber() { return m_iPartNumber; }
	const char* 		GetMessageID() { return m_szMessageID; }
	void 				SetMessageID(const char* szMessageID);
	void 				SetSize(int s) { m_iSize = s; }
	int 				GetSize() { return m_iSize; }
	EStatus				GetStatus() { return m_eStatus; }
	void				SetStatus(EStatus Status) { m_eStatus = Status; }
	const char*			GetResultFilename() { return m_szResultFilename; }
	void 				SetResultFilename(const char* v);
};

class FileInfo
{
public:
	typedef std::vector<ArticleInfo*>	Articles;
	typedef std::vector<char*>			Groups;

private:
	int					m_iID;
	NZBInfo*			m_pNZBInfo;
	Articles			m_Articles;
	Groups				m_Groups;
	char* 				m_szSubject;
	char*				m_szFilename;
	long long 			m_lSize;
	long long 			m_lRemainingSize;
	bool				m_bPaused;
	bool				m_bDeleted;
	bool				m_bFilenameConfirmed;
	int					m_iCompleted;
	bool				m_bOutputInitialized;
	Mutex				m_mutexOutputFile;

	static int			m_iIDGen;

public:
						FileInfo();
						~FileInfo();
	int					GetID() { return m_iID; }
	void				SetID(int s);
	NZBInfo*			GetNZBInfo() { return m_pNZBInfo; }
	void				SetNZBInfo(NZBInfo* pNZBInfo);
	Articles* 			GetArticles() { return &m_Articles; }
	Groups* 			GetGroups() { return &m_Groups; }
	const char*			GetSubject() { return m_szSubject; }
	void 				SetSubject(const char* szSubject);
	const char*			GetFilename() { return m_szFilename; }
	void 				SetFilename(const char* szFilename);
	void				MakeValidFilename();
	bool				GetFilenameConfirmed() { return m_bFilenameConfirmed; }
	void				SetFilenameConfirmed(bool bFilenameConfirmed) { m_bFilenameConfirmed = bFilenameConfirmed; }
	void 				SetSize(long long s) { m_lSize = s; m_lRemainingSize = s; }
	long long 			GetSize() { return m_lSize; }
	long long 			GetRemainingSize() { return m_lRemainingSize; }
	void 				SetRemainingSize(long long s) { m_lRemainingSize = s; }
	bool				GetPaused() { return m_bPaused; }
	void				SetPaused(bool Paused) { m_bPaused = Paused; }
	bool				GetDeleted() { return m_bDeleted; }
	void				SetDeleted(bool Deleted) { m_bDeleted = Deleted; }
	int					GetCompleted() { return m_iCompleted; }
	void				SetCompleted(int s) { m_iCompleted = s; }
	void				ClearArticles();
	void				LockOutputFile();
	void				UnlockOutputFile();
	bool				GetOutputInitialized() { return m_bOutputInitialized; }
	void				SetOutputInitialized(bool bOutputInitialized) { m_bOutputInitialized = bOutputInitialized; }
	bool				IsDupe(const char* szFilename);
};
                              
typedef std::deque<FileInfo*> DownloadQueue;

class GroupInfo;
typedef std::deque<GroupInfo*> GroupQueue;

class GroupInfo
{
private:
	NZBInfo*			m_pNZBInfo;
	int					m_iFirstID;
	int					m_iLastID;
	int		 			m_iRemainingFileCount;
	long long 			m_lRemainingSize;
	long long 			m_lPausedSize;
	int					m_iRemainingParCount;

public:
						GroupInfo();
						~GroupInfo();
	NZBInfo*			GetNZBInfo() { return m_pNZBInfo; }
	int					GetFirstID() { return m_iFirstID; }
	int					GetLastID() { return m_iLastID; }
	long long 			GetRemainingSize() { return m_lRemainingSize; }
	long long 			GetPausedSize() { return m_lPausedSize; }
	int					GetRemainingFileCount() { return m_iRemainingFileCount; }
	int					GetRemainingParCount() { return m_iRemainingParCount; }

	static void			BuildGroups(DownloadQueue* pDownloadQueue, GroupQueue* pGroupQueue);
};

#endif
