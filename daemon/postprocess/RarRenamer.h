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

	struct RarBlock
	{
		uint32 crc;
		uint8 type;
		uint16 flags;
		uint64 addsize;
		uint64 trailsize;
	};

	struct RarFile
	{
		CString m_filename;
		uint32 m_time = 0;
		uint32 m_attr = 0;
		int64 m_size = 0;
		bool m_splitBefore = false;
		bool m_splitAfter = false;
	};

	typedef std::deque<RarFile> FileList;

	struct RarVolume
	{
		CString m_filename;
		int m_version = 0;
		uint32 m_volumeNo = 0;
		bool m_newNaming = false;
		bool m_hasNextVolume = false;
		bool m_multiVolume = false;
		FileList m_files;

		RarVolume(int version, const char* filename) : m_version(version), m_filename(filename) {}
	};

	typedef std::deque<RarVolume> RarVolumeList;

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
	RarVolumeList m_volumes;

	void BuildDirList(const char* destDir);
	void CheckFiles(const char* destDir);
	void CheckRegularFile(const char* destDir, const char* filename);
	void RenameFile(const char* srcFilename, const char* destFileName);
	int DetectRarVersion(DiskFile& file);
	void LogDebugInfo();
	bool Seek(DiskFile& file, RarBlock* block, int64 relpos);
	bool Read(DiskFile& file, RarBlock* block, void* buffer, int64 size);
	bool Read16(DiskFile& file, RarBlock* block, uint16* result);
	bool Read32(DiskFile& file, RarBlock* block, uint32* result);
	bool ReadV(DiskFile& file, RarBlock* block, uint64* result);
	bool ReadRar3Volume(DiskFile& file, RarVolume& volume);
	RarBlock ReadRar3Block(DiskFile& file);
	bool ReadRar3File(DiskFile& file, RarVolume& volume, RarBlock& block, RarFile& innerFile);
};

#endif
