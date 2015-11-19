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
		char*			m_filename;
		char*			m_hash;
		bool			m_fileExists;

	public:
						FileHash(const char* filename, const char* hash);
						~FileHash();
		const char*		GetFilename() { return m_filename; }
		const char*		GetHash() { return m_hash; }
		bool			GetFileExists() { return m_fileExists; }
		void			SetFileExists(bool fileExists) { m_fileExists = fileExists; }
	};

	typedef std::deque<FileHash*>		FileHashList;
	typedef std::deque<char*>			DirList;

private:
	char*				m_infoName;
	char*				m_destDir;
	EStatus				m_status;
	char*				m_progressLabel;
	int					m_stageProgress;
	bool				m_cancelled;
	DirList				m_dirList;
	FileHashList		m_fileHashList;
	int					m_fileCount;
	int					m_curFile;
	int					m_renamedCount;
	bool				m_hasMissedFiles;
	bool				m_detectMissing;

	void				Cleanup();
	void				ClearHashList();
	void				BuildDirList(const char* destDir);
	void				CheckDir(const char* destDir);
	void				LoadParFiles(const char* destDir);
	void				LoadParFile(const char* parFilename);
	void				CheckFiles(const char* destDir, bool renamePars);
	void				CheckRegularFile(const char* destDir, const char* filename);
	void				CheckParFile(const char* destDir, const char* filename);
	bool				IsSplittedFragment(const char* filename, const char* correctName);
	void				CheckMissing();
	void				RenameFile(const char* srcFilename, const char* destFileName);

protected:
	virtual void		UpdateProgress() {}
	virtual void		Completed() {}
	virtual void		PrintMessage(Message::EKind kind, const char* format, ...) {}
	virtual void		RegisterParredFile(const char* filename) {}
	virtual void		RegisterRenamedFile(const char* oldFilename, const char* newFileName) {}
	const char*			GetProgressLabel() { return m_progressLabel; }
	int					GetStageProgress() { return m_stageProgress; }

public:
						ParRenamer();
	virtual				~ParRenamer();
	virtual void		Run();
	void				SetDestDir(const char* destDir);
	const char*			GetInfoName() { return m_infoName; }
	void				SetInfoName(const char* infoName);
	void				SetStatus(EStatus status);
	EStatus				GetStatus() { return m_status; }
	void				Cancel();
	bool				GetCancelled() { return m_cancelled; }
	bool				HasMissedFiles() { return m_hasMissedFiles; }
	void				SetDetectMissing(bool detectMissing) { m_detectMissing = detectMissing; }
};

#endif

#endif
