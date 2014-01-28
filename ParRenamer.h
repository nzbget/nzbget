/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2013-2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef PARRENAMER_H
#define PARRENAMER_H

#ifndef DISABLE_PARCHECK

#include <deque>

#include "Thread.h"
#include "Log.h"

class ParRenamer : public Thread
{
public:
	enum EStatus
	{
		psFailed,
		psSuccess
	};
	
	class FileHash
	{
	private:
		char*			m_szFilename;
		char*			m_szHash;

	public:
						FileHash(const char* szFilename, const char* szHash);
						~FileHash();
		const char*		GetFilename() { return m_szFilename; }
		const char*		GetHash() { return m_szHash; }
	};

	typedef std::deque<FileHash*>		FileHashList;

	typedef std::deque<char*>			DirList;
	
private:
	char*				m_szInfoName;
	char*				m_szDestDir;
	EStatus				m_eStatus;
	char*				m_szProgressLabel;
	int					m_iStageProgress;
	bool				m_bCancelled;
	DirList				m_DirList;
	FileHashList		m_FileHashList;
	int					m_iFileCount;
	int					m_iCurFile;
	int					m_iRenamedCount;

	void				Cleanup();
	void				ClearHashList();
	void				BuildDirList(const char* szDestDir);
	void				CheckDir(const char* szDestDir);
	void				LoadParFiles(const char* szDestDir);
	void				LoadParFile(const char* szParFilename);
	void				CheckFiles(const char* szDestDir, bool bRenamePars);
	void				CheckRegularFile(const char* szDestDir, const char* szFilename);
	void				CheckParFile(const char* szDestDir, const char* szFilename);

protected:
	virtual void		UpdateProgress() {}
	virtual void		Completed() {}
	virtual void		PrintMessage(Message::EKind eKind, const char* szFormat, ...) {}
	const char*			GetProgressLabel() { return m_szProgressLabel; }
	int					GetStageProgress() { return m_iStageProgress; }

public:
						ParRenamer();
	virtual				~ParRenamer();
	virtual void		Run();
	void				SetDestDir(const char* szDestDir);
	const char*			GetInfoName() { return m_szInfoName; }
	void				SetInfoName(const char* szInfoName);
	void				SetStatus(EStatus eStatus);
	EStatus				GetStatus() { return m_eStatus; }
	void				Cancel();
	bool				GetCancelled() { return m_bCancelled; }
};

#endif

#endif
