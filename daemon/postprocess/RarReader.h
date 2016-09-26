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


#ifndef RARREADER_H
#define RARREADER_H

#include "NString.h"
#include "Log.h"
#include "FileSystem.h"

class RarFile
{
public:
	const char* GetFilename() { return m_filename; }
	uint32 GetTime() { return m_time; }
	uint32 GetAttr() { return m_attr; }
	int64 GetSize() { return m_size; }
	bool GetSplitBefore() { return m_splitBefore; }
	bool GetSplitAfter() { return m_splitAfter; }
private:
	CString m_filename;
	uint32 m_time = 0;
	uint32 m_attr = 0;
	int64 m_size = 0;
	bool m_splitBefore = false;
	bool m_splitAfter = false;
	friend class RarVolume;
};

class RarVolume
{
public:
	typedef std::deque<RarFile> FileList;

	RarVolume(const char* filename) : m_filename(filename) {}
	bool Read();

	const char* GetFilename() { return m_filename; }
	int GetVersion() { return m_version; }
	uint32 GetVolumeNo() { return m_volumeNo; }
	bool GetNewNaming() { return m_newNaming; }
	bool GetHasNextVolume() { return m_hasNextVolume; }
	bool GetMultiVolume() { return m_multiVolume; }
	FileList* GetFiles() { return &m_files; }

private:
	struct RarBlock
	{
		uint32 crc;
		uint8 type;
		uint16 flags;
		uint64 addsize;
		uint64 trailsize;
	};

	CString m_filename;
	int m_version = 0;
	uint32 m_volumeNo = 0;
	bool m_newNaming = false;
	bool m_hasNextVolume = false;
	bool m_multiVolume = false;
	FileList m_files;

	int DetectRarVersion(DiskFile& file);
	void LogDebugInfo();
	bool Seek(DiskFile& file, RarBlock* block, int64 relpos);
	bool Read(DiskFile& file, RarBlock* block, void* buffer, int64 size);
	bool Read16(DiskFile& file, RarBlock* block, uint16* result);
	bool Read32(DiskFile& file, RarBlock* block, uint32* result);
	bool ReadV(DiskFile& file, RarBlock* block, uint64* result);
	bool ReadRar3Volume(DiskFile& file);
	bool ReadRar5Volume(DiskFile& file);
	RarBlock ReadRar3Block(DiskFile& file);
	RarBlock ReadRar5Block(DiskFile& file);
	bool ReadRar3File(DiskFile& file, RarBlock& block, RarFile& innerFile);
	bool ReadRar5File(DiskFile& file, RarBlock& block, RarFile& innerFile);
};

#endif
