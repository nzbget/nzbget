/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#include "nzbget.h"
#include "FileSystem.h"

CString FileSystem::GetLastErrorMessage()
{
	BString<1024> msg;
	strerror_r(errno, msg, msg.Capacity());
	return *msg;
}

void FileSystem::NormalizePathSeparators(char* path)
{
	for (char* p = path; *p; p++)
	{
		if (*p == ALT_PATH_SEPARATOR)
		{
			*p = PATH_SEPARATOR;
		}
	}
}

bool FileSystem::ForceDirectories(const char* path, CString& errmsg)
{
	errmsg.Clear();
	BString<1024> normPath = path;
	NormalizePathSeparators(normPath);
	int len = strlen(normPath);
	if ((len > 0) && normPath[len-1] == PATH_SEPARATOR
#ifdef WIN32
		&& len > 3
#endif
		)
	{
		normPath[len-1] = '\0';
	}

	struct stat buffer;
	bool ok = !stat(normPath, &buffer);
	if (!ok && errno != ENOENT)
	{
		errmsg.Format("could not read information for directory %s: errno %i, %s",
			*normPath, errno, *GetLastErrorMessage());
		return false;
	}

	if (ok && !S_ISDIR(buffer.st_mode))
	{
		errmsg.Format("path %s is not a directory", *normPath);
		return false;
	}

	if (!ok
#ifdef WIN32
		&& strlen(normPath) > 2
#endif
		)
	{
		BString<1024> parentPath = *normPath;
		char* p = (char*)strrchr(parentPath, PATH_SEPARATOR);
		if (p)
		{
#ifdef WIN32
			if (p - parentPath == 2 && parentPath[1] == ':' && strlen(parentPath) > 2)
			{
				parentPath[3] = '\0';
			}
			else
#endif
			{
				*p = '\0';
			}
			if (strlen(parentPath) != strlen(path) && !ForceDirectories(parentPath, errmsg))
			{
				return false;
			}
		}

		if (mkdir(normPath, S_DIRMODE) != 0 && errno != EEXIST)
		{
			errmsg.Format("could not create directory %s: %s", *normPath, *GetLastErrorMessage());
			return false;
		}

		if (stat(normPath, &buffer) != 0)
		{
			errmsg.Format("could not read information for directory %s: %s",
				*normPath, *GetLastErrorMessage());
			return false;
		}

		if (!S_ISDIR(buffer.st_mode))
		{
			errmsg.Format("path %s is not a directory", *normPath);
			return false;
		}
	}

	return true;
}

CString FileSystem::GetCurrentDirectory()
{
	CString result;
	result.Reserve(1024);
#ifdef WIN32
	::GetCurrentDirectory(1024, result);
#else
	getcwd(result, 1024);
#endif
	return result;
}

bool FileSystem::SetCurrentDirectory(const char* dirFilename)
{
#ifdef WIN32
	return ::SetCurrentDirectory(dirFilename);
#else
	return chdir(dirFilename) == 0;
#endif
}

bool FileSystem::DirEmpty(const char* dirFilename)
{
	DirBrowser dir(dirFilename);
	return dir.Next() == NULL;
}

bool FileSystem::LoadFileIntoBuffer(const char* fileName, char** buffer, int* bufferLength)
{
	FILE* file = fopen(fileName, FOPEN_RB);
	if (!file)
	{
		return false;
	}

	// obtain file size.
	fseek(file , 0 , SEEK_END);
	int size  = (int)ftell(file);
	rewind(file);

	// allocate memory to contain the whole file.
	*buffer = (char*) malloc(size + 1);
	if (!*buffer)
	{
		return false;
	}

	// copy the file into the buffer.
	fread(*buffer, 1, size, file);

	fclose(file);

	(*buffer)[size] = 0;

	*bufferLength = size + 1;

	return true;
}

bool FileSystem::SaveBufferIntoFile(const char* fileName, const char* buffer, int bufLen)
{
	FILE* file = fopen(fileName, FOPEN_WB);
	if (!file)
	{
		return false;
	}

	int writtenBytes = fwrite(buffer, 1, bufLen, file);
	fclose(file);

	return writtenBytes == bufLen;
}

bool FileSystem::CreateSparseFile(const char* filename, int64 size, CString& errmsg)
{
	errmsg.Clear();
	bool ok = false;
#ifdef WIN32
	HANDLE hFile = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_NEW, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		errmsg = GetLastErrorMessage();
		return false;
	}
	// first try to create sparse file (supported only on NTFS partitions),
	// it may fail but that's OK.
	DWORD dwBytesReturned;
	DeviceIoControl(hFile, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &dwBytesReturned, NULL);

	LARGE_INTEGER size64;
	size64.QuadPart = size;
	SetFilePointerEx(hFile, size64, NULL, FILE_END);
	SetEndOfFile(hFile);
	CloseHandle(hFile);
	ok = true;
#else
	// create file
	FILE* file = fopen(filename, FOPEN_AB);
	if (!file)
	{
		errmsg = GetLastErrorMessage();
		return false;
	}
	fclose(file);

	// there are no reliable function to expand file on POSIX, so we must try different approaches,
	// starting with the fastest one and hoping it will work
	// 1) set file size using function "truncate" (this is fast, if it works)
	truncate(filename, size);
	// check if it worked
	ok = FileSize(filename) == size;
	if (!ok)
	{
		// 2) truncate did not work, expanding the file by writing to it (that's slow)
		truncate(filename, 0);
		file = fopen(filename, FOPEN_AB);
		if (!file)
		{
			errmsg = GetLastErrorMessage();
			return false;
		}
		char c = '0';
		fwrite(&c, 1, size, file);
		fclose(file);
		ok = FileSize(filename) == size;
	}
#endif
	return ok;
}

bool FileSystem::TruncateFile(const char* filename, int size)
{
	bool ok = false;
#ifdef WIN32
	FILE *file = fopen(filename, FOPEN_RBP);
	fseek(file, size, SEEK_SET);
	ok = SetEndOfFile((HANDLE)_get_osfhandle(_fileno(file))) != 0;
	fclose(file);
#else
	ok = truncate(filename, size) == 0;
#endif
	return ok;
}

char* FileSystem::BaseFileName(const char* filename)
{
	char* p = (char*)strrchr(filename, PATH_SEPARATOR);
	char* p1 = (char*)strrchr(filename, ALT_PATH_SEPARATOR);
	if (p1)
	{
		if ((p && p < p1) || !p)
		{
			p = p1;
		}
	}
	if (p)
	{
		return p + 1;
	}
	else
	{
		return (char*)filename;
	}
}

//replace bad chars in filename
void FileSystem::MakeValidFilename(char* filename, char cReplaceChar, bool allowSlashes)
{
	const char* replaceChars = allowSlashes ? ":*?\"><\n\r\t" : "\\/:*?\"><\n\r\t";
	char* p = filename;
	while (*p)
	{
		if (strchr(replaceChars, *p))
		{
			*p = cReplaceChar;
		}
		if (allowSlashes && *p == ALT_PATH_SEPARATOR)
		{
			*p = PATH_SEPARATOR;
		}
		p++;
	}

	// remove trailing dots and spaces. they are not allowed in directory names on windows,
	// but we remove them on posix also, in a case the directory is accessed from windows via samba.
	for (int len = strlen(filename); len > 0 && (filename[len - 1] == '.' || filename[len - 1] == ' '); len--)
	{
		filename[len - 1] = '\0';
	}
}

// returns TRUE if the name was changed by adding duplicate-suffix
CString FileSystem::MakeUniqueFilename(const char* destDir, const char* basename)
{
	CString result;
	result.Format("%s%c%s", destDir, (int)PATH_SEPARATOR, basename);

	int dupeNumber = 0;
	while (FileExists(result))
	{
		dupeNumber++;

		const char* extension = strrchr(basename, '.');
		if (extension && extension != basename)
		{
			BString<1024> filenameWithoutExt = basename;
			int end = extension - basename;
			filenameWithoutExt[end < 1024 ? end : 1024-1] = '\0';

			if (!strcasecmp(extension, ".par2"))
			{
				char* volExtension = strrchr(filenameWithoutExt, '.');
				if (volExtension && volExtension != filenameWithoutExt &&
					!strncasecmp(volExtension, ".vol", 4))
				{
					*volExtension = '\0';
					extension = basename + (volExtension - filenameWithoutExt);
				}
			}

			result.Format("%s%c%s.duplicate%d%s", destDir, (int)PATH_SEPARATOR,
				*filenameWithoutExt, dupeNumber, extension);
		}
		else
		{
			result.Format("%s%c%s.duplicate%d", destDir, (int)PATH_SEPARATOR,
				basename, dupeNumber);
		}
	}

	return result;
}

bool FileSystem::MoveFile(const char* srcFilename, const char* dstFilename)
{
	bool ok = rename(srcFilename, dstFilename) == 0;

#ifndef WIN32
	if (!ok && errno == EXDEV)
	{
		ok = CopyFile(srcFilename, dstFilename) && DeleteFile(srcFilename);
	}
#endif

	return ok;
}

bool FileSystem::CopyFile(const char* srcFilename, const char* dstFilename)
{
	FILE* infile = fopen(srcFilename, FOPEN_RB);
	if (!infile)
	{
		return false;
	}

	FILE* outfile = fopen(dstFilename, FOPEN_WB);
	if (!outfile)
	{
		fclose(infile);
		return false;
	}

	static const int BUFFER_SIZE = 1024 * 50;
	char* buffer = (char*)malloc(BUFFER_SIZE);

	int cnt = BUFFER_SIZE;
	while (cnt == BUFFER_SIZE)
	{
		cnt = (int)fread(buffer, 1, BUFFER_SIZE, infile);
		fwrite(buffer, 1, cnt, outfile);
	}

	fclose(infile);
	fclose(outfile);
	free(buffer);

	return true;
}

bool FileSystem::DeleteFile(const char* filename)
{
	return remove(filename) == 0;
}

bool FileSystem::FileExists(const char* filename)
{
#ifdef WIN32
	// we use a native windows call because c-lib function "stat" fails on windows if file date is invalid
	WIN32_FIND_DATA findData;
	HANDLE handle = FindFirstFile(filename, &findData);
	if (handle != INVALID_HANDLE_VALUE)
	{
		bool exists = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
		FindClose(handle);
		return exists;
	}
	return false;
#else
	struct stat buffer;
	bool exists = !stat(filename, &buffer) && S_ISREG(buffer.st_mode);
	return exists;
#endif
}

bool FileSystem::FileExists(const char* path, const char* filenameWithoutPath)
{
	BString<1024> fullFilename("%s%c%s", path, (int)PATH_SEPARATOR, filenameWithoutPath);
	bool exists = FileExists(fullFilename);
	return exists;
}

bool FileSystem::DirectoryExists(const char* dirFilename)
{
#ifdef WIN32
	// we use a native windows call because c-lib function "stat" fails on windows if file date is invalid
	WIN32_FIND_DATA findData;
	HANDLE handle = FindFirstFile(dirFilename, &findData);
	if (handle != INVALID_HANDLE_VALUE)
	{
		bool exists = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		FindClose(handle);
		return exists;
	}
	return false;
#else
	struct stat buffer;
	bool exists = !stat(dirFilename, &buffer) && S_ISDIR(buffer.st_mode);
	return exists;
#endif
}

bool FileSystem::CreateDirectory(const char* dirFilename)
{
	mkdir(dirFilename, S_DIRMODE);
	return DirectoryExists(dirFilename);
}

bool FileSystem::RemoveDirectory(const char* dirFilename)
{
	return rmdir(dirFilename) == 0;
}

bool FileSystem::DeleteDirectoryWithContent(const char* dirFilename, CString& errmsg)
{
	errmsg.Clear();

	bool del = false;
	bool ok = true;

	DirBrowser dir(dirFilename);
	while (const char* filename = dir.Next())
	{
		BString<1024> fullFilename("%s%c%s", dirFilename, PATH_SEPARATOR, filename);

		if (FileSystem::DirectoryExists(fullFilename))
		{
			del = DeleteDirectoryWithContent(fullFilename, errmsg);
		}
		else
		{
			del = remove(fullFilename) == 0;
		}
		ok &= del;
		if (!del && errmsg.Empty())
		{
			errmsg.Format("could not delete %s: %s", *fullFilename, *GetLastErrorMessage());
		}
	}

	del = RemoveDirectory(dirFilename);
	ok &= del;
	if (!del && errmsg.Empty())
	{
		errmsg = GetLastErrorMessage();
	}
	return ok;
}

int64 FileSystem::FileSize(const char* filename)
{
#ifdef WIN32
	struct _stat32i64 buffer;
	_stat32i64(filename, &buffer);
#else
	struct stat buffer;
	stat(filename, &buffer);
#endif
	return buffer.st_size;
}

int64 FileSystem::FreeDiskSize(const char* path)
{
#ifdef WIN32
	ULARGE_INTEGER free, dummy;
	if (GetDiskFreeSpaceEx(path, &free, &dummy, &dummy))
	{
		return free.QuadPart;
	}
#else
	struct statvfs diskdata;
	if (!statvfs(path, &diskdata))
	{
		return (int64)diskdata.f_frsize * (int64)diskdata.f_bavail;
	}
#endif
	return -1;
}

bool FileSystem::RenameBak(const char* filename, const char* bakPart, bool removeOldExtension, CString& newName)
{
	BString<1024> changedFilename;

	if (removeOldExtension)
	{
		changedFilename = filename;
		char* extension = strrchr(changedFilename, '.');
		if (extension)
		{
			*extension = '\0';
		}
	}

	newName.Format("%s.%s", removeOldExtension ? *changedFilename : filename, bakPart);

	int i = 2;
	struct stat buffer;
	while (!stat(newName, &buffer))
	{
		newName.Format("%s.%i.%s", removeOldExtension ? *changedFilename : filename, i++, bakPart);
	}

	bool ok = MoveFile(filename, newName);
	return ok;
}

#ifndef WIN32
CString FileSystem::ExpandHomePath(const char* filename)
{
	CString result;

	if (filename && (filename[0] == '~') && (filename[1] == '/'))
	{
		// expand home-dir

		char* home = getenv("HOME");
		if (!home)
		{
			struct passwd *pw = getpwuid(getuid());
			if (pw)
			{
				home = pw->pw_dir;
			}
		}

		if (!home)
		{
			return filename;
		}

		if (home[strlen(home)-1] == '/')
		{
			result.Format("%s%s", home, filename + 2);
		}
		else
		{
			result.Format("%s/%s", home, filename + 2);
		}
	}
	else
	{
		result.Append(filename ? filename : "");
	}

	return result;
}
#endif

CString FileSystem::ExpandFileName(const char* filename)
{
	CString result;
	result.Reserve(1024);

#ifdef WIN32
	_fullpath(result, filename, 1024);
#else
	if (filename[0] != '\0' && filename[0] != '/')
	{
		char curDir[MAX_PATH + 1];
		getcwd(curDir, sizeof(curDir) - 1); // 1 char reserved for adding backslash
		int offset = 0;
		if (filename[0] == '.' && filename[1] == '/')
		{
			offset += 2;
		}
		result.Format("%s/%s", curDir, filename + offset);
	}
	else
	{
		result = filename;
	}
#endif

	return result;
}

CString FileSystem::GetExeFileName(const char* argv0)
{
	CString exename;
	exename.Reserve(1024);
	exename[1024 - 1] = '\0';

#ifdef WIN32
	GetModuleFileName(NULL, exename, 1024);
#else
	// Linux
	int r = readlink("/proc/self/exe", exename, 1024 - 1);
	if (r > 0)
	{
		return exename;
	}
	// FreeBSD
	r = readlink("/proc/curproc/file", exename, 1024 - 1);
	if (r > 0)
	{
		return exename;
	}

	exename = ExpandFileName(argv0);
#endif

	return exename;
}

bool FileSystem::SameFilename(const char* filename1, const char* filename2)
{
#ifdef WIN32
	return strcasecmp(filename1, filename2) == 0;
#else
	return strcmp(filename1, filename2) == 0;
#endif
}

bool FileSystem::FlushFileBuffers(int fileDescriptor, CString& errmsg)
{
#ifdef WIN32
	BOOL ok = ::FlushFileBuffers((HANDLE)_get_osfhandle(fileDescriptor));
	if (!ok)
	{
		errmsg.Reserve(1024);
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			errmsg, 1024, NULL);
	}
	return ok;
#else
#ifdef HAVE_FULLFSYNC
	int ret = fcntl(fileDescriptor, F_FULLFSYNC) == -1 ? 1 : 0;
#elif HAVE_FDATASYNC
	int ret = fdatasync(fileDescriptor);
#else
	int ret = fsync(fileDescriptor);
#endif
	if (ret != 0)
	{
		errmsg = GetLastErrorMessage();
	}
	return ret == 0;
#endif
}

bool FileSystem::FlushDirBuffers(const char* filename, CString& errmsg)
{
	BString<1024> parentPath = filename;
	const char* fileMode = FOPEN_RBP;

#ifndef WIN32
	char* p = (char*)strrchr(parentPath, PATH_SEPARATOR);
	if (p)
	{
		*p = '\0';
	}
	fileMode = FOPEN_RB;
#endif

	FILE* file = fopen(parentPath, fileMode);
	if (!file)
	{
		errmsg = GetLastErrorMessage();
		return false;
	}
	bool ok = FlushFileBuffers(fileno(file), errmsg);
	fclose(file);
	return ok;
}

#ifndef WIN32
void FileSystem::FixExecPermission(const char* filename)
{
	struct stat buffer;
	bool ok = !stat(filename, &buffer);
	if (ok)
	{
		buffer.st_mode = buffer.st_mode | S_IXUSR | S_IXGRP | S_IXOTH;
		chmod(filename, buffer.st_mode);
	}
}
#endif

#ifdef WIN32

DirBrowser::DirBrowser(const char* path)
{
	BString<1024> mask("%s%c*.*", path, (int)PATH_SEPARATOR);
	m_file = FindFirstFile(mask, &m_findData);
	m_first = true;
}

DirBrowser::~DirBrowser()
{
	if (m_file != INVALID_HANDLE_VALUE)
	{
		FindClose(m_file);
	}
}

const char* DirBrowser::InternNext()
{
	bool ok = false;
	if (m_first)
	{
		ok = m_file != INVALID_HANDLE_VALUE;
		m_first = false;
	}
	else
	{
		ok = FindNextFile(m_file, &m_findData) != 0;
	}
	if (ok)
	{
		return m_findData.cFileName;
	}
	return NULL;
}

#else

#ifdef DIRBROWSER_SNAPSHOT
DirBrowser::DirBrowser(const char* path, bool snapshot)
#else
DirBrowser::DirBrowser(const char* path)
#endif
{
#ifdef DIRBROWSER_SNAPSHOT
	m_snapshot = snapshot;
	if (m_snapshot)
	{
		DirBrowser dir(path, false);
		while (const char* filename = dir.Next())
		{
			m_snapshot.push_back(strdup(filename));
		}
		m_itSnapshot = m_snapshot.begin();
	}
	else
#endif
	{
		m_dir = opendir(path);
	}
}

DirBrowser::~DirBrowser()
{
#ifdef DIRBROWSER_SNAPSHOT
	if (m_snapshot)
	{
		for (FileList::iterator it = m_snapshot.begin(); it != m_snapshot.end(); it++)
		{
			delete *it;
		}
	}
	else
#endif
	{
		if (m_dir)
		{
			closedir(m_dir);
		}
	}
}

const char* DirBrowser::InternNext()
{
#ifdef DIRBROWSER_SNAPSHOT
	if (m_snapshot)
	{
		return m_itSnapshot == m_snapshot.end() ? NULL : *m_itSnapshot++;
	}
	else
#endif
	{
		if (m_dir)
		{
			m_findData = readdir(m_dir);
			if (m_findData)
			{
				return m_findData->d_name;
			}
		}
		return NULL;
	}
}
#endif

const char* DirBrowser::Next()
{
	const char* filename = NULL;
	for (filename = InternNext(); filename && (!strcmp(filename, ".") || !strcmp(filename, "..")); )
	{
		filename = InternNext();
	}
	return filename;
}


DiskFile::~DiskFile()
{
	if (m_file)
	{
		Close();
	}
}

bool DiskFile::Open(const char* filename, EOpenMode mode)
{
	m_file = fopen(filename, mode == omRead ? FOPEN_RB : mode == omReadWrite ?
		FOPEN_RBP : mode == omWrite ? FOPEN_WB : FOPEN_AB);
	return m_file;
}

bool DiskFile::Close()
{
	if (m_file)
	{
		int ret = fclose(m_file);
		m_file = nullptr;
		return ret;
	}
	else
	{
		return false;
	}
}

int64 DiskFile::Read(void* buffer, int64 size)
{
	return fread(buffer, 1, (size_t)size, m_file);
}

int64 DiskFile::Write(const void* buffer, int64 size)
{
	return fwrite(buffer, 1, (size_t)size, m_file);
}

int64 DiskFile::Print(const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	int ret = vfprintf(m_file, format, ap);
	va_end(ap);
	return ret;
}

char* DiskFile::ReadLine(char* buffer, int64 size)
{
	return fgets(buffer, (int)size, m_file);
}

int64 DiskFile::Position()
{
	return ftell(m_file);
}

int64 DiskFile::Seek(int64 position, ESeekOrigin origin)
{
	return fseek(m_file, position,
		origin == soCur ? SEEK_CUR :
		origin == soEnd ? SEEK_END : SEEK_SET);
}

bool DiskFile::Eof()
{
	return feof(m_file) != 0;
}

bool DiskFile::Error()
{
	return ferror(m_file) != 0;
}

bool DiskFile::SetWriteBuffer(int size)
{
	return setvbuf(m_file, NULL, _IOFBF, size) == 0;
}

bool DiskFile::Flush()
{
	return fflush(m_file) == 0;
}

bool DiskFile::Sync(CString& errmsg)
{
	return FileSystem::FlushFileBuffers(fileno(m_file), errmsg);
}

