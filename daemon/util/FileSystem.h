/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2007-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "NString.h"

class FileSystem
{
public:
	static CString GetLastErrorMessage();
	static char* BaseFileName(const char* filename);
	static bool SameFilename(const char* filename1, const char* filename2);
	static void NormalizePathSeparators(char* path);
	static bool LoadFileIntoBuffer(const char* filename, CharBuffer& buffer, bool addTrailingNull);
	static bool SaveBufferIntoFile(const char* filename, const char* buffer, int bufLen);
	static bool AllocateFile(const char* filename, int64 size, bool sparse, CString& errmsg);
	static bool TruncateFile(const char* filename, int size);
	static CString MakeValidFilename(const char* filename, bool allowSlashes = false);
	static bool ReservedChar(char ch);
	static CString MakeUniqueFilename(const char* destDir, const char* basename);
	static bool MoveFile(const char* srcFilename, const char* dstFilename);
	static bool CopyFile(const char* srcFilename, const char* dstFilename);
	static bool DeleteFile(const char* filename);
	static bool FileExists(const char* filename);
	static bool DirectoryExists(const char* dirFilename);
	static bool CreateDirectory(const char* dirFilename);

	/* Delete empty directory */
	static bool RemoveDirectory(const char* dirFilename);

	/* Delete directory which is empty or contains only hidden files or directories */
	static bool DeleteDirectory(const char* dirFilename);

	static bool DeleteDirectoryWithContent(const char* dirFilename, CString& errmsg);
	static bool ForceDirectories(const char* path, CString& errmsg);
	static CString GetCurrentDirectory();
	static bool SetCurrentDirectory(const char* dirFilename);
	static int64 FileSize(const char* filename);
	static int64 FreeDiskSize(const char* path);
	static bool DirEmpty(const char* dirFilename);
	static bool RenameBak(const char* filename, const char* bakPart, bool removeOldExtension, CString& newName);
#ifndef WIN32
	static CString ExpandHomePath(const char* filename);
	static void FixExecPermission(const char* filename);
#endif
	static CString ExpandFileName(const char* filename);
	static CString GetExeFileName(const char* argv0);

	/* Flush disk buffers for file with given descriptor */
	static bool FlushFileBuffers(int fileDescriptor, CString& errmsg);

	/* Flush disk buffers for file metadata (after file renaming) */
	static bool FlushDirBuffers(const char* filename, CString& errmsg);

	static CString MakeExtendedPath(const char* path, bool force);

#ifdef WIN32
	static WString UtfPathToWidePath(const char* utfpath);
	static CString WidePathToUtfPath(const wchar_t* wpath);
	static CString MakeCanonicalPath(const char* filename);
	static bool NeedLongPath(const char* path);
#endif
};

class DirBrowser
{
public:
	DirBrowser(const char* path, bool snapshot = true);
	~DirBrowser();
	const char* Next();

private:
#ifdef WIN32
	WIN32_FIND_DATAW m_findData;
	HANDLE m_file = INVALID_HANDLE_VALUE;
	bool m_first;
	CString m_filename;
#else
	DIR* m_dir = nullptr;
	struct dirent* m_findData;
#endif

	bool m_snapshot;
	typedef std::deque<CString> FileList;
	FileList m_snapshotFiles;
	FileList::iterator m_snapshotIter;

	const char* InternNext();
};

class DiskFile
{
public:
	enum EOpenMode
	{
		omRead, // file must exist
		omReadWrite, // file must exist
		omWrite, // create new or overwrite existing
		omAppend // create new or append to existing
	};

	enum ESeekOrigin
	{
		soSet,
		soCur,
		soEnd
	};

	DiskFile() = default;
	DiskFile(const DiskFile&) = delete;
	~DiskFile();
	bool Open(const char* filename, EOpenMode mode);
	bool Close();
	bool Active() { return m_file != nullptr; }
	int64 Read(void* buffer, int64 size);
	int64 Write(const void* buffer, int64 size);
	int64 Position();
	bool Seek(int64 position, ESeekOrigin origin = soSet);
	bool Eof();
	bool Error();
	int64 Print(const char* format, ...) PRINTF_SYNTAX(2);
	char* ReadLine(char* buffer, int64 size);
	bool SetWriteBuffer(int size);
	bool Flush();
	bool Sync(CString& errmsg);

private:
	FILE* m_file = nullptr;
};

#endif
