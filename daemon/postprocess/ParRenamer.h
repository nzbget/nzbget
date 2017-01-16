/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2013-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef PARRENAMER_H
#define PARRENAMER_H

#ifndef DISABLE_PARCHECK

#include "NString.h"
#include "Log.h"

class ParRenamer
{
public:
	void Execute();
	void SetDestDir(const char* destDir) { m_destDir = destDir; }
	const char* GetInfoName() { return m_infoName; }
	void SetInfoName(const char* infoName) { m_infoName = infoName; }
	int GetRenamedCount() { return m_renamedCount; }
	bool HasMissedFiles() { return m_hasMissedFiles; }
	bool HasDamagedParFiles() { return m_hasDamagedParFiles; }
	void SetDetectMissing(bool detectMissing) { m_detectMissing = detectMissing; }

protected:
	virtual void UpdateProgress() {}
	virtual bool IsStopped() { return false; };
	virtual void PrintMessage(Message::EKind kind, const char* format, ...) PRINTF_SYNTAX(3) {}
	virtual void RegisterParredFile(const char* filename) {}
	virtual void RegisterRenamedFile(const char* oldFilename, const char* newFileName) {}
	const char* GetProgressLabel() { return m_progressLabel; }
	int GetStageProgress() { return m_stageProgress; }

private:
	class FileHash
	{
	public:
		FileHash(const char* filename, const char* hash) :
			m_filename(filename), m_hash(hash) {}
		const char* GetFilename() { return m_filename; }
		const char* GetHash() { return m_hash; }
		bool GetFileExists() { return m_fileExists; }
		void SetFileExists(bool fileExists) { m_fileExists = fileExists; }
	private:
		CString m_filename;
		CString m_hash;
		bool m_fileExists = false;
	};

	class ParInfo
	{
	public:
		ParInfo(const char* filename, const char* setId) :
			m_filename(filename), m_setId(setId) {}
		const char* GetFilename() { return m_filename; }
		const char* GetSetId() { return m_setId; }
	private:
		CString m_filename;
		CString m_setId;
	};

	typedef std::deque<FileHash> FileHashList;
	typedef std::deque<ParInfo> ParInfoList;
	typedef std::deque<CString> NameList;

	CString m_infoName;
	CString m_destDir;
	CString m_progressLabel;
	int m_stageProgress = 0;
	NameList m_dirList;
	FileHashList m_fileHashList;
	ParInfoList m_parInfoList;
	NameList m_badParList;
	NameList m_loadedParList;
	int m_fileCount = 0;
	int m_curFile = 0;
	int m_renamedCount = 0;
	bool m_hasMissedFiles = false;
	bool m_detectMissing = false;
	bool m_hasDamagedParFiles = false;

	void BuildDirList(const char* destDir);
	void LoadMainParFiles(const char* destDir);
	void LoadExtraParFiles(const char* destDir);
	void LoadParFile(const char* parFilename);
	void CheckFiles(const char* destDir, bool checkPars);
	void CheckRegularFile(const char* destDir, const char* filename);
	void CheckParFile(const char* destDir, const char* filename);
	bool IsSplittedFragment(const char* filename, const char* correctName);
	void CheckMissing();
	void RenameParFiles(const char* destDir);
	void RenameParFile(const char* destDir, const char* filename, const char* setId);
	bool NeedRenameParFiles();
	void RenameFile(const char* srcFilename, const char* destFileName);
	void RenameBadParFiles();
};

#endif

#endif
