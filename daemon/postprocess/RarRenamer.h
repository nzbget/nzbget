/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef RARRENAMER_H
#define RARRENAMER_H

#include "NString.h"
#include "Thread.h"
#include "Log.h"
#include "FileSystem.h"

class RarRenamer
{
public:
	enum EStatus
	{
		rsFailed,
		rsSuccess
	};

	void Execute();
	void SetDestDir(const char* destDir) { m_destDir = destDir; }
	const char* GetInfoName() { return m_infoName; }
	void SetInfoName(const char* infoName) { m_infoName = infoName; }
	EStatus GetStatus() { return m_status; }
	void Cancel();
	bool GetCancelled() { return m_cancelled; }

protected:
	virtual void UpdateProgress() {}
	virtual void Completed() {}
	virtual void PrintMessage(Message::EKind kind, const char* format, ...) PRINTF_SYNTAX(3) {}
	virtual void RegisterRenamedFile(const char* oldFilename, const char* newFileName) {}
	const char* GetProgressLabel() { return m_progressLabel; }
	int GetStageProgress() { return m_stageProgress; }

private:
	typedef std::deque<CString> DirList;

	CString m_infoName;
	CString m_destDir;
	EStatus m_status = rsFailed;
	CString m_progressLabel;
	int m_stageProgress = 0;
	bool m_cancelled = false;
	DirList m_dirList;
	int m_fileCount = 0;
	int m_curFile = 0;
	int m_renamedCount = 0;

	void BuildDirList(const char* destDir);
	void CheckFiles(const char* destDir);
	void CheckRegularFile(const char* destDir, const char* filename);
	void RenameFile(const char* srcFilename, const char* destFileName);
};

#endif
