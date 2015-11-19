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
#include "Util.h"			

#ifndef WIN32
// function "code_revision" is automatically generated in file "code_revision.cpp" on each build
const char* code_revision(void);
#endif

#ifdef WIN32

// getopt for WIN32:
// from http://www.codeproject.com/cpp/xgetopt.asp
// Original Author:  Hans Dietrich (hdietrich2@hotmail.com)
// Released to public domain from author (thanks)
// Slightly modified by Andrey Prygunkov

char	*optarg;		// global argument pointer
int		optind = 0; 	// global argv index

int getopt(int argc, char *argv[], char *optstring)
{
	static char *next = NULL;
	if (optind == 0)
		next = NULL;

	optarg = NULL;

	if (next == NULL || *next == '\0')
	{
		if (optind == 0)
			optind++;

		if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0')
		{
			optarg = NULL;
			if (optind < argc)
				optarg = argv[optind];
			return -1;
		}

		if (strcmp(argv[optind], "--") == 0)
		{
			optind++;
			optarg = NULL;
			if (optind < argc)
				optarg = argv[optind];
			return -1;
		}

		next = argv[optind];
		next++;		// skip past -
		optind++;
	}

	char c = *next++;
	char *cp = strchr(optstring, c);

	if (cp == NULL || c == ':')
	{
		fprintf(stderr, "Invalid option %c", c);
		return '?';
	}

	cp++;
	if (*cp == ':')
	{
		if (*next != '\0')
		{
			optarg = next;
			next = NULL;
		}
		else if (optind < argc)
		{
			optarg = argv[optind];
			optind++;
		}
		else
		{
			fprintf(stderr, "Option %c needs an argument", c);
			return '?';
		}
	}

	return c;
}

DirBrowser::DirBrowser(const char* path)
{
	char mask[MAX_PATH + 1];
	snprintf(mask, MAX_PATH + 1, "%s%c*.*", path, (int)PATH_SEPARATOR);
	mask[MAX_PATH] = '\0';
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

const char* DirBrowser::Next()
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
			closedir((DIR*)m_dir);
		}
	}
}

const char* DirBrowser::Next()
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
			m_findData = readdir((DIR*)m_dir);
			if (m_findData)
			{
				return m_findData->d_name;
			}
		}
		return NULL;
	}
}

#endif


StringBuilder::StringBuilder()
{
	m_buffer = NULL;
	m_bufferSize = 0;
	m_usedSize = 0;
	m_growSize = 10240;
}

StringBuilder::~StringBuilder()
{
	free(m_buffer);
}

void StringBuilder::Clear()
{
	free(m_buffer);
	m_buffer = NULL;
	m_bufferSize = 0;
	m_usedSize = 0;
}

void StringBuilder::Append(const char* str)
{
	int partLen = strlen(str);
	Reserve(partLen + 1);
	strcpy(m_buffer + m_usedSize, str);
	m_usedSize += partLen;
	m_buffer[m_usedSize] = '\0';
}

void StringBuilder::AppendFmt(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	AppendFmtV(format, args);
	va_end(args);
}

void StringBuilder::AppendFmtV(const char* format, va_list ap)
{
	va_list ap2;
	va_copy(ap2, ap);

	int remainingSize = m_bufferSize - m_usedSize;
	int m = vsnprintf(m_buffer + m_usedSize, remainingSize, format, ap);
#ifdef WIN32
	if (m == -1)
	{
		m = _vscprintf(format, ap);
	}
#endif
	if (m + 1 > remainingSize)
	{
		Reserve(m - remainingSize + m_growSize);
		remainingSize = m_bufferSize - m_usedSize;
		m = vsnprintf(m_buffer + m_usedSize, remainingSize, format, ap2);
	}
	if (m >= 0)
	{
		m_buffer[m_usedSize += m] = '\0';
	}

	va_end(ap2);
}

void StringBuilder::Reserve(int size)
{
	if (m_usedSize + size > m_bufferSize)
	{
		m_bufferSize += size + m_growSize;
		m_buffer = (char*)realloc(m_buffer, m_bufferSize);
	}
}


char Util::VersionRevisionBuf[100];

char* Util::BaseFileName(const char* filename)
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

void Util::NormalizePathSeparators(char* path)
{
	for (char* p = path; *p; p++)
	{
		if (*p == ALT_PATH_SEPARATOR)
		{
			*p = PATH_SEPARATOR;
		}
	}
}

bool Util::ForceDirectories(const char* path, char* errBuf, int bufSize)
{
	*errBuf = '\0';
	char sysErrStr[256];
	char normPath[1024];
	strncpy(normPath, path, 1024);
	normPath[1024-1] = '\0';
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
		snprintf(errBuf, bufSize, "could not read information for directory %s: errno %i, %s", normPath, errno, GetLastErrorMessage(sysErrStr, sizeof(sysErrStr)));
		errBuf[bufSize-1] = 0;
		return false;
	}

	if (ok && !S_ISDIR(buffer.st_mode))
	{
		snprintf(errBuf, bufSize, "path %s is not a directory", normPath);
		errBuf[bufSize-1] = 0;
		return false;
	}

	if (!ok
#ifdef WIN32
		&& strlen(normPath) > 2
#endif
		)
	{
		char parentPath[1024];
		strncpy(parentPath, normPath, 1024);
		parentPath[1024-1] = '\0';
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
			if (strlen(parentPath) != strlen(path) && !ForceDirectories(parentPath, errBuf, bufSize))
			{
				return false;
			}
		}

		if (mkdir(normPath, S_DIRMODE) != 0 && errno != EEXIST)
		{
			snprintf(errBuf, bufSize, "could not create directory %s: %s", normPath, GetLastErrorMessage(sysErrStr, sizeof(sysErrStr)));
			errBuf[bufSize-1] = 0;
			return false;
		}

		if (stat(normPath, &buffer) != 0)
		{
			snprintf(errBuf, bufSize, "could not read information for directory %s: %s", normPath, GetLastErrorMessage(sysErrStr, sizeof(sysErrStr)));
			errBuf[bufSize-1] = 0;
			return false;
		}

		if (!S_ISDIR(buffer.st_mode))
		{
			snprintf(errBuf, bufSize, "path %s is not a directory", normPath);
			errBuf[bufSize-1] = 0;
			return false;
		}
	}

	return true;
}

bool Util::GetCurrentDirectory(char* buffer, int bufSize)
{
#ifdef WIN32
	return ::GetCurrentDirectory(bufSize, buffer) != NULL;
#else
	return getcwd(buffer, bufSize) != NULL;
#endif
}

bool Util::SetCurrentDirectory(const char* dirFilename)
{
#ifdef WIN32
	return ::SetCurrentDirectory(dirFilename);
#else
	return chdir(dirFilename) == 0;
#endif
}

bool Util::DirEmpty(const char* dirFilename)
{
	DirBrowser dir(dirFilename);
	while (const char* filename = dir.Next())
	{
		if (strcmp(filename, ".") && strcmp(filename, ".."))
		{
			return false;
		}
	}
	return true;
}

bool Util::LoadFileIntoBuffer(const char* fileName, char** buffer, int* bufferLength)
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

bool Util::SaveBufferIntoFile(const char* fileName, const char* buffer, int bufLen)
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

bool Util::CreateSparseFile(const char* filename, long long size, char* errBuf, int bufSize)
{
	*errBuf = '\0';
	bool ok = false;
#ifdef WIN32
	HANDLE hFile = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_NEW, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		GetLastErrorMessage(errBuf, sizeof(bufSize));
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
		GetLastErrorMessage(errBuf, sizeof(bufSize));
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
			GetLastErrorMessage(errBuf, sizeof(bufSize));
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

bool Util::TruncateFile(const char* filename, int size)
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

//replace bad chars in filename
void Util::MakeValidFilename(char* filename, char cReplaceChar, bool allowSlashes)
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
bool Util::MakeUniqueFilename(char* destBufFilename, int destBufSize, const char* destDir, const char* basename)
{
	snprintf(destBufFilename, destBufSize, "%s%c%s", destDir, (int)PATH_SEPARATOR, basename);
	destBufFilename[destBufSize-1] = '\0';

	int dupeNumber = 0;
	while (FileExists(destBufFilename))
	{
		dupeNumber++;

		const char* extension = strrchr(basename, '.');
		if (extension && extension != basename)
		{
			char filenameWithoutExt[1024];
			strncpy(filenameWithoutExt, basename, 1024);
			int end = extension - basename;
			filenameWithoutExt[end < 1024 ? end : 1024-1] = '\0';

			if (!strcasecmp(extension, ".par2"))
			{
				char* volExtension = strrchr(filenameWithoutExt, '.');
				if (volExtension && volExtension != filenameWithoutExt && !strncasecmp(volExtension, ".vol", 4))
				{
					*volExtension = '\0';
					extension = basename + (volExtension - filenameWithoutExt);
				}
			}

			snprintf(destBufFilename, destBufSize, "%s%c%s.duplicate%d%s", destDir, (int)PATH_SEPARATOR, filenameWithoutExt, dupeNumber, extension);
		}
		else
		{
			snprintf(destBufFilename, destBufSize, "%s%c%s.duplicate%d", destDir, (int)PATH_SEPARATOR, basename, dupeNumber);
		}

		destBufFilename[destBufSize-1] = '\0';
	}

	return dupeNumber > 0;
}

long long Util::JoinInt64(unsigned long Hi, unsigned long Lo)
{
	return (((long long)Hi) << 32) + Lo;
}

void Util::SplitInt64(long long Int64, unsigned long* Hi, unsigned long* Lo)
{
	*Hi = (unsigned long)(Int64 >> 32);
	*Lo = (unsigned long)(Int64 & 0xFFFFFFFF);
}

/* Base64 decryption is taken from
 *  Article "BASE 64 Decoding and Encoding Class 2003" by Jan Raddatz
 *  http://www.codeguru.com/cpp/cpp/algorithms/article.php/c5099/
 */

const static char BASE64_DEALPHABET [128] =
	{
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //   0 -   9
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  10 -  19
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  20 -  29
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  30 -  39
	 0,  0,  0, 62,  0,  0,  0, 63, 52, 53, //  40 -  49
	54, 55, 56, 57, 58, 59, 60, 61,  0,  0, //  50 -  59
	 0, 61,  0,  0,  0,  0,  1,  2,  3,  4, //  60 -  69
	 5,  6,  7,  8,  9, 10, 11, 12, 13, 14, //  70 -  79
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, //  80 -  89
	25,  0,  0,  0,  0,  0,  0, 26, 27, 28, //  90 -  99
	29, 30, 31, 32, 33, 34, 35, 36, 37, 38, // 100 - 109
	39, 40, 41, 42, 43, 44, 45, 46, 47, 48, // 110 - 119
	49, 50, 51,  0,  0,  0,  0,  0			// 120 - 127
	};

unsigned int DecodeByteQuartet(char* inputBuffer, char* outputBuffer)
{
	unsigned int buffer = 0;

	if (inputBuffer[3] == '=')
	{
		if (inputBuffer[2] == '=')
		{
			buffer = (buffer | BASE64_DEALPHABET [(int)inputBuffer[0]]) << 6;
			buffer = (buffer | BASE64_DEALPHABET [(int)inputBuffer[1]]) << 6;
			buffer = buffer << 14;

			outputBuffer [0] = (char)(buffer >> 24);

			return 1;
		}
		else
		{
			buffer = (buffer | BASE64_DEALPHABET [(int)inputBuffer[0]]) << 6;
			buffer = (buffer | BASE64_DEALPHABET [(int)inputBuffer[1]]) << 6;
			buffer = (buffer | BASE64_DEALPHABET [(int)inputBuffer[2]]) << 6;
			buffer = buffer << 8;

			outputBuffer [0] = (char)(buffer >> 24);
			outputBuffer [1] = (char)(buffer >> 16);

			return 2;
		}
	}
	else
	{
		buffer = (buffer | BASE64_DEALPHABET [(int)inputBuffer[0]]) << 6;
		buffer = (buffer | BASE64_DEALPHABET [(int)inputBuffer[1]]) << 6;
		buffer = (buffer | BASE64_DEALPHABET [(int)inputBuffer[2]]) << 6;
		buffer = (buffer | BASE64_DEALPHABET [(int)inputBuffer[3]]) << 6;
		buffer = buffer << 2;

		outputBuffer [0] = (char)(buffer >> 24);
		outputBuffer [1] = (char)(buffer >> 16);
		outputBuffer [2] = (char)(buffer >> 8);

		return 3;
	}

	return 0;
}

bool Util::MoveFile(const char* srcFilename, const char* dstFilename)
{
	bool ok = rename(srcFilename, dstFilename) == 0;

#ifndef WIN32
	if (!ok && errno == EXDEV)
	{
		ok = CopyFile(srcFilename, dstFilename) && remove(srcFilename) == 0;
	}
#endif

	return ok;
}

bool Util::CopyFile(const char* srcFilename, const char* dstFilename)
{
	FILE* infile = fopen(srcFilename, FOPEN_RB);
	if (!infile)
	{
		return false;
	}

	FILE* outfile = fopen(dstFilename, FOPEN_WBP);
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

bool Util::FileExists(const char* filename)
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

bool Util::FileExists(const char* path, const char* filenameWithoutPath)
{
	char fullFilename[1024];
	snprintf(fullFilename, 1024, "%s%c%s", path, (int)PATH_SEPARATOR, filenameWithoutPath);
	fullFilename[1024-1] = '\0';
	bool exists = Util::FileExists(fullFilename);
	return exists;
}

bool Util::DirectoryExists(const char* dirFilename)
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

bool Util::CreateDirectory(const char* dirFilename)
{
	mkdir(dirFilename, S_DIRMODE);
	return DirectoryExists(dirFilename);
}

bool Util::RemoveDirectory(const char* dirFilename)
{
#ifdef WIN32
	return _rmdir(dirFilename) == 0;
#else
	return remove(dirFilename) == 0;
#endif
}

bool Util::DeleteDirectoryWithContent(const char* dirFilename, char* errBuf, int bufSize)
{
	*errBuf = '\0';
	char sysErrStr[256];

	bool del = false;
	bool ok = true;

	DirBrowser dir(dirFilename);
	while (const char* filename = dir.Next())
	{
		char fullFilename[1024];
		snprintf(fullFilename, 1024, "%s%c%s", dirFilename, PATH_SEPARATOR, filename);
		fullFilename[1024-1] = '\0';

		if (strcmp(filename, ".") && strcmp(filename, ".."))
		{
			if (Util::DirectoryExists(fullFilename))
			{
				del = DeleteDirectoryWithContent(fullFilename, sysErrStr, sizeof(sysErrStr));
			}
			else
			{
				del = remove(fullFilename) == 0;
			}
			ok &= del;
			if (!del && !*errBuf)
			{
				snprintf(errBuf, bufSize, "could not delete %s: %s", fullFilename, GetLastErrorMessage(sysErrStr, sizeof(sysErrStr)));
			}
		}
	}

	del = RemoveDirectory(dirFilename);
	ok &= del;
	if (!del && !*errBuf)
	{
		GetLastErrorMessage(errBuf, bufSize);
	}
	return ok;
}

long long Util::FileSize(const char* filename)
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

long long Util::FreeDiskSize(const char* path)
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
		return (long long)diskdata.f_frsize * (long long)diskdata.f_bavail;
	}
#endif
	return -1;
}

bool Util::RenameBak(const char* filename, const char* bakPart, bool removeOldExtension, char* newNameBuf, int newNameBufSize)
{
	char changedFilename[1024];

	if (removeOldExtension)
	{
		strncpy(changedFilename, filename, 1024);
		changedFilename[1024-1] = '\0';
		char* extension = strrchr(changedFilename, '.');
		if (extension)
		{
			*extension = '\0';
		}
	}

	char bakname[1024];
	snprintf(bakname, 1024, "%s.%s", removeOldExtension ? changedFilename : filename, bakPart);
	bakname[1024-1] = '\0';

	int i = 2;
	struct stat buffer;
	while (!stat(bakname, &buffer))
	{
		snprintf(bakname, 1024, "%s.%i.%s", removeOldExtension ? changedFilename : filename, i++, bakPart);
		bakname[1024-1] = '\0';
	}

	if (newNameBuf)
	{
		strncpy(newNameBuf, bakname, newNameBufSize);
	}

	bool ok = !rename(filename, bakname);
	return ok;
}

#ifndef WIN32
bool Util::ExpandHomePath(const char* filename, char* buffer, int bufSize)
{
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
			return false;
		}

		if (home[strlen(home)-1] == '/')
		{
			snprintf(buffer, bufSize, "%s%s", home, filename + 2);
		}
		else
		{
			snprintf(buffer, bufSize, "%s/%s", home, filename + 2);
		}
		buffer[bufSize - 1] = '\0';
	}
	else
	{
		strncpy(buffer, filename ? filename : "", bufSize);
		buffer[bufSize - 1] = '\0';
	}

	return true;
}
#endif

void Util::ExpandFileName(const char* filename, char* buffer, int bufSize)
{
#ifdef WIN32
	_fullpath(buffer, filename, bufSize);
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
		snprintf(buffer, bufSize, "%s/%s", curDir, filename + offset);
	}
	else
	{
		strncpy(buffer, filename, bufSize);
		buffer[bufSize - 1] = '\0';
	}
#endif
}

void Util::GetExeFileName(const char* argv0, char* buffer, int bufSize)
{
#ifdef WIN32
	GetModuleFileName(NULL, buffer, bufSize);
#else
	// Linux
	int r = readlink("/proc/self/exe", buffer, bufSize-1);
	if (r > 0)
	{
		buffer[r] = '\0';
		return;
	}
	// FreeBSD
	r = readlink("/proc/curproc/file", buffer, bufSize-1);
	if (r > 0)
	{
		buffer[r] = '\0';
		return;
	}

	ExpandFileName(argv0, buffer, bufSize);
#endif
}

char* Util::FormatSize(char * buffer, int bufLen, long long fileSize)
{
	if (fileSize > 1024 * 1024 * 1000)
	{
		snprintf(buffer, bufLen, "%.2f GB", (float)((float)fileSize / 1024 / 1024 / 1024));
	}
	else if (fileSize > 1024 * 1000)
	{
		snprintf(buffer, bufLen, "%.2f MB", (float)((float)fileSize / 1024 / 1024));
	}
	else if (fileSize > 1000)
	{
		snprintf(buffer, bufLen, "%.2f KB", (float)((float)fileSize / 1024));
	}
	else if (fileSize == 0)
	{
		strncpy(buffer, "0 MB", bufLen);
	}
	else
	{
		snprintf(buffer, bufLen, "%i B", (int)fileSize);
	}
	buffer[bufLen - 1] = '\0';
	return buffer;
}

char* Util::FormatSpeed(char* buffer, int bufSize, int bytesPerSecond)
{
	if (bytesPerSecond >= 100 * 1024 * 1024)
	{
		snprintf(buffer, bufSize, "%i MB/s", bytesPerSecond / 1024 / 1024);
	}
	else if (bytesPerSecond >= 10 * 1024 * 1024)
	{
		snprintf(buffer, bufSize, "%0.1f MB/s", (float)bytesPerSecond / 1024.0 / 1024.0);
	}
	else if (bytesPerSecond >= 1024 * 1000)
	{
		snprintf(buffer, bufSize, "%0.2f MB/s", (float)bytesPerSecond / 1024.0 / 1024.0);
	}
	else
	{
		snprintf(buffer, bufSize, "%i KB/s", bytesPerSecond / 1024);
	}

	buffer[bufSize - 1] = '\0';
	return buffer;
}

bool Util::SameFilename(const char* filename1, const char* filename2)
{
#ifdef WIN32
	return strcasecmp(filename1, filename2) == 0;
#else
	return strcmp(filename1, filename2) == 0;
#endif
}

bool Util::MatchFileExt(const char* filename, const char* extensionList, const char* listSeparator)
{
	int filenameLen = strlen(filename);

	Tokenizer tok(extensionList, listSeparator);
	while (const char* ext = tok.Next())
	{
		int extLen = strlen(ext);
		if (filenameLen >= extLen && !strcasecmp(ext, filename + filenameLen - extLen))
		{
			return true;
		}
		if (strchr(ext, '*') || strchr(ext, '?'))
		{
			WildMask mask(ext);
			if (mask.Match(filename))
			{
				return true;
			}
		}
	}

	return false;
}

#ifndef WIN32
void Util::FixExecPermission(const char* filename)
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

char* Util::GetLastErrorMessage(char* buffer, int bufLen)
{
	buffer[0] = '\0';
	strerror_r(errno, buffer, bufLen);
	buffer[bufLen-1] = '\0';
	return buffer;
}

void Util::Init()
{
#ifndef WIN32
	if ((strlen(code_revision()) > 0) && strstr(VERSION, "testing"))
	{
		snprintf(VersionRevisionBuf, sizeof(VersionRevisionBuf), "%s-r%s", VERSION, code_revision());
	}
	else
#endif
	{
		snprintf(VersionRevisionBuf, sizeof(VersionRevisionBuf), "%s", VERSION);
	}

	// init static vars there
	GetCurrentTicks();
}

bool Util::SplitCommandLine(const char* commandLine, char*** argv)
{
	int argCount = 0;
	char buf[1024];
	char* pszArgList[100];
	unsigned int len = 0;
	bool escaping = false;
	bool space = true;
	for (const char* p = commandLine; ; p++)
	{
		if (*p)
		{
			const char c = *p;
			if (escaping)
			{
				if (c == '\'')
				{
					if (p[1] == '\'' && len < sizeof(buf) - 1)
					{
						buf[len++] = c;
						p++;
					}
					else
					{
						escaping = false;
						space = true;
					}
				}
				else if (len < sizeof(buf) - 1)
				{
					buf[len++] = c;
				}
			}
			else
			{
				if (c == ' ')
				{
					space = true;
				}
				else if (c == '\'' && space)
				{
					escaping = true;
					space = false;
				}
				else if (len < sizeof(buf) - 1)
				{
					buf[len++] = c;
					space = false;
				}
			}
		}

		if ((space || !*p) && len > 0 && argCount < 100)
		{
			//add token
			buf[len] = '\0';
			if (argv)
			{
				pszArgList[argCount] = strdup(buf);
			}
			(argCount)++;
			len = 0;
		}

		if (!*p)
		{
			break;
		}
	}

	if (argv)
	{
		pszArgList[argCount] = NULL;
		*argv = (char**)malloc((argCount + 1) * sizeof(char*));
		memcpy(*argv, pszArgList, sizeof(char*) * (argCount + 1));
	}

	return argCount > 0;
}

void Util::TrimRight(char* str)
{
	char* end = str + strlen(str) - 1;
	while (end >= str && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t'))
	{
		*end = '\0';
		end--;
	}
}

char* Util::Trim(char* str)
{
	TrimRight(str);
	while (*str == '\n' || *str == '\r' || *str == ' ' || *str == '\t')
	{
		str++;
	}
	return str;
}

char* Util::ReduceStr(char* str, const char* from, const char* to)
{
	int lenFrom = strlen(from);
	int lenTo = strlen(to);
	// assert(iLenTo < iLenFrom);

	while (char* p = strstr(str, from))
	{
		const char* src = to;
		while ((*p++ = *src++)) ;

		src = --p - lenTo + lenFrom;
		while ((*p++ = *src++)) ;
	}

	return str;
}

/* Calculate Hash using Bob Jenkins (1996) algorithm
 * http://burtleburtle.net/bob/c/lookup2.c
 */
typedef  unsigned int  ub4;   /* unsigned 4-byte quantities */
typedef  unsigned char ub1;

#define hashsize(n) ((ub4)1<<(n))
#define hashmask(n) (hashsize(n)-1)

#define mix(a,b,c) \
{ \
a -= b; a -= c; a ^= (c>>13); \
b -= c; b -= a; b ^= (a<<8); \
c -= a; c -= b; c ^= (b>>13); \
a -= b; a -= c; a ^= (c>>12);  \
b -= c; b -= a; b ^= (a<<16); \
c -= a; c -= b; c ^= (b>>5); \
a -= b; a -= c; a ^= (c>>3);  \
b -= c; b -= a; b ^= (a<<10); \
c -= a; c -= b; c ^= (b>>15); \
}

ub4 hash(register ub1 *k, register ub4  length, register ub4  initval)
// register ub1 *k;        /* the key */
// register ub4  length;   /* the length of the key */
// register ub4  initval;    /* the previous hash, or an arbitrary value */
{
	register ub4 a,b,c,len;

	/* Set up the internal state */
	len = length;
	a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
	c = initval;           /* the previous hash value */

	/*---------------------------------------- handle most of the key */
	while (len >= 12)
	{
		a += (k[0] +((ub4)k[1]<<8) +((ub4)k[2]<<16) +((ub4)k[3]<<24));
		b += (k[4] +((ub4)k[5]<<8) +((ub4)k[6]<<16) +((ub4)k[7]<<24));
		c += (k[8] +((ub4)k[9]<<8) +((ub4)k[10]<<16)+((ub4)k[11]<<24));
		mix(a,b,c);
		k += 12; len -= 12;
	}

	/*------------------------------------- handle the last 11 bytes */
	c += length;
	switch(len)              /* all the case statements fall through */
	{
		case 11: c+=((ub4)k[10]<<24);
		case 10: c+=((ub4)k[9]<<16);
		case 9 : c+=((ub4)k[8]<<8);
			/* the first byte of c is reserved for the length */
		case 8 : b+=((ub4)k[7]<<24);
		case 7 : b+=((ub4)k[6]<<16);
		case 6 : b+=((ub4)k[5]<<8);
		case 5 : b+=k[4];
		case 4 : a+=((ub4)k[3]<<24);
		case 3 : a+=((ub4)k[2]<<16);
		case 2 : a+=((ub4)k[1]<<8);
		case 1 : a+=k[0];
			/* case 0: nothing left to add */
	}
	mix(a,b,c);
	/*-------------------------------------------- report the result */
	return c;
}

unsigned int Util::HashBJ96(const char* buffer, int bufSize, unsigned int initValue)
{
	return (unsigned int)hash((ub1*)buffer, (ub4)bufSize, (ub4)initValue);
}

#ifdef WIN32
bool Util::RegReadStr(HKEY keyRoot, const char* keyName, const char* valueName, char* buffer, int* bufLen)
{
	HKEY subKey;
	if (!RegOpenKeyEx(keyRoot, keyName, 0, KEY_READ, &subKey))
	{
		DWORD retBytes = *bufLen;
		LONG ret = RegQueryValueEx(subKey, valueName, NULL, NULL, (LPBYTE)buffer, &retBytes);
		*bufLen = retBytes;
		RegCloseKey(subKey);
		return ret == 0;
	}
	return false;
}
#endif

/* From boost */

inline int is_leap(int year)
{
  if(year % 400 == 0)
  return 1;
  if(year % 100 == 0)
  return 0;
  if(year % 4 == 0)
  return 1;
  return 0;
}
inline int days_from_0(int year)
{
  year--;
  return 365 * year + (year / 400) - (year/100) + (year / 4);
}
inline int days_from_1970(int year)
{
  static const int days_from_0_to_1970 = 719162; // days_from_0(1970);
  return days_from_0(year) - days_from_0_to_1970;
}
inline int days_from_1jan(int year,int month,int day)
{
  static const int days[2][12] =
  {
	{ 0,31,59,90,120,151,181,212,243,273,304,334},
	{ 0,31,60,91,121,152,182,213,244,274,305,335}
  };
  return days[is_leap(year)][month-1] + day - 1;
}

inline time_t internal_timegm(tm const *t)
{
  int year = t->tm_year + 1900;
  int month = t->tm_mon;
  if(month > 11)
  {
	year += month/12;
	month %= 12;
  }
  else if(month < 0)
  {
	int years_diff = (-month + 11)/12;
	year -= years_diff;
	month+=12 * years_diff;
  }
  month++;
  int day = t->tm_mday;
  int day_of_year = days_from_1jan(year,month,day);
  int days_since_epoch = days_from_1970(year) + day_of_year;

  time_t seconds_in_day = 3600 * 24;
  time_t result = seconds_in_day * days_since_epoch + 3600 * t->tm_hour + 60 * t->tm_min + t->tm_sec;

  return result;
}

time_t Util::Timegm(tm const *t)
{
	return internal_timegm(t);
}

// prevent PC from going to sleep
void Util::SetStandByMode(bool standBy)
{
#ifdef WIN32
	SetThreadExecutionState((standBy ? 0 : ES_SYSTEM_REQUIRED) | ES_CONTINUOUS);
#endif
}

static unsigned long crc32_tab[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3,	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de,	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,	0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5,	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,	0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940,	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,	0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

/* This is a modified version of chksum_crc() from
 * crc32.c (http://www.koders.com/c/fid699AFE0A656F0022C9D6B9D1743E697B69CE5815.aspx)
 * (c) 1999,2000 Krzysztof Dabrowski
 * (c) 1999,2000 ElysiuM deeZine
 *
 * chksum_crc() -- to a given block, this one calculates the
 *				crc32-checksum until the length is
 *				reached. the crc32-checksum will be
 *				the result.
 */
unsigned long Util::Crc32m(unsigned long startCrc, unsigned char *block, unsigned long length)
{
	register unsigned long crc = startCrc;
	for (unsigned long i = 0; i < length; i++)
	{
		crc = ((crc >> 8) & 0x00FFFFFF) ^ crc32_tab[(crc ^ *block++) & 0xFF];
	}
	return crc;
}

unsigned long Util::Crc32(unsigned char *block, unsigned long length)
{
	return Util::Crc32m(0xFFFFFFFF, block, length) ^ 0xFFFFFFFF;
}

/* From zlib/crc32.c (http://www.zlib.net/)
 * Copyright (C) 1995-2006, 2010, 2011, 2012 Mark Adler
 */

#define GF2_DIM 32      /* dimension of GF(2) vectors (length of CRC) */

unsigned long gf2_matrix_times(unsigned long *mat, unsigned long vec)
{
	unsigned long sum;

	sum = 0;
	while (vec) {
		if (vec & 1)
			sum ^= *mat;
		vec >>= 1;
		mat++;
	}
	return sum;
}

void gf2_matrix_square(unsigned long *square, unsigned long *mat)
{
	int n;

	for (n = 0; n < GF2_DIM; n++)
		square[n] = gf2_matrix_times(mat, mat[n]);
}

unsigned long Util::Crc32Combine(unsigned long crc1, unsigned long crc2, unsigned long len2)
{
	int n;
	unsigned long row;
	unsigned long even[GF2_DIM];    /* even-power-of-two zeros operator */
	unsigned long odd[GF2_DIM];     /* odd-power-of-two zeros operator */

	/* degenerate case (also disallow negative lengths) */
	if (len2 <= 0)
		return crc1;

	/* put operator for one zero bit in odd */
	odd[0] = 0xedb88320UL;          /* CRC-32 polynomial */
	row = 1;
	for (n = 1; n < GF2_DIM; n++) {
		odd[n] = row;
		row <<= 1;
	}

	/* put operator for two zero bits in even */
	gf2_matrix_square(even, odd);

	/* put operator for four zero bits in odd */
	gf2_matrix_square(odd, even);

	/* apply len2 zeros to crc1 (first square will put the operator for one
	   zero byte, eight zero bits, in even) */
	do {
		/* apply zeros operator for this bit of len2 */
		gf2_matrix_square(even, odd);
		if (len2 & 1)
			crc1 = gf2_matrix_times(even, crc1);
		len2 >>= 1;

		/* if no more bits set, then done */
		if (len2 == 0)
			break;

		/* another iteration of the loop with odd and even swapped */
		gf2_matrix_square(odd, even);
		if (len2 & 1)
			crc1 = gf2_matrix_times(odd, crc1);
		len2 >>= 1;

		/* if no more bits set, then done */
	} while (len2 != 0);

	/* return combined crc */
	crc1 ^= crc2;

	return crc1;
}

int Util::NumberOfCpuCores()
{
#ifdef WIN32
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return sysinfo.dwNumberOfProcessors;
#elif HAVE_SC_NPROCESSORS_ONLN
	return sysconf(_SC_NPROCESSORS_ONLN);
#endif
	return -1;
}

bool Util::FlushFileBuffers(int fileDescriptor, char* errBuf, int bufSize)
{
#ifdef WIN32
	BOOL ok = ::FlushFileBuffers((HANDLE)_get_osfhandle(fileDescriptor));
	if (!ok)
	{
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			errBuf, bufSize, NULL);
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
		GetLastErrorMessage(errBuf, bufSize);
	}
	return ret == 0;
#endif
}

bool Util::FlushDirBuffers(const char* filename, char* errBuf, int bufSize)
{
	char parentPath[1024];
	strncpy(parentPath, filename, 1024);
	parentPath[1024-1] = '\0';
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
		GetLastErrorMessage(errBuf, bufSize);
		return false;
	}
	bool ok = FlushFileBuffers(fileno(file), errBuf, bufSize);
	fclose(file);
	return ok;
}

long long Util::GetCurrentTicks()
{
#ifdef WIN32
	static long long hz=0, hzo=0;
	if (!hz)
	{
		QueryPerformanceFrequency((LARGE_INTEGER*)&hz);
		QueryPerformanceCounter((LARGE_INTEGER*)&hzo);
	}
	long long t;
	QueryPerformanceCounter((LARGE_INTEGER*)&t);
	return ((t-hzo)*1000000)/hz;
#else
	timeval t;
	gettimeofday(&t, NULL);
	return (long long)(t.tv_sec) * 1000000ll + (long long)(t.tv_usec);
#endif
}

unsigned int WebUtil::DecodeBase64(char* inputBuffer, int inputBufferLength, char* outputBuffer)
{
	unsigned int InputBufferIndex  = 0;
	unsigned int OutputBufferIndex = 0;
	unsigned int InputBufferLength = inputBufferLength > 0 ? inputBufferLength : strlen(inputBuffer);

	char ByteQuartet [4];
	int i = 0;
	while (InputBufferIndex < InputBufferLength)
	{
		// Ignore all characters except the ones in BASE64_ALPHABET
		if ((inputBuffer [InputBufferIndex] >= 48 && inputBuffer [InputBufferIndex] <=  57) ||
			(inputBuffer [InputBufferIndex] >= 65 && inputBuffer [InputBufferIndex] <=  90) ||
			(inputBuffer [InputBufferIndex] >= 97 && inputBuffer [InputBufferIndex] <= 122) ||
			inputBuffer [InputBufferIndex] == '+' ||
			inputBuffer [InputBufferIndex] == '/' ||
			inputBuffer [InputBufferIndex] == '=')
		{
			ByteQuartet [i] = inputBuffer [InputBufferIndex];
			i++;
		}

		InputBufferIndex++;

		if (i == 4) {
			OutputBufferIndex += DecodeByteQuartet(ByteQuartet, outputBuffer + OutputBufferIndex);
			i = 0;
		}
	}

	// OutputBufferIndex gives us the next position of the next decoded character
	// inside our output buffer and thus represents the number of decoded characters
	// in our buffer.
	return OutputBufferIndex;
}

/* END - Base64
*/

char* WebUtil::XmlEncode(const char* raw)
{
	// calculate the required outputstring-size based on number of xml-entities and their sizes
	int reqSize = strlen(raw);
	for (const char* p = raw; *p; p++)
	{
		unsigned char ch = *p;
		switch (ch)
		{
			case '>':
			case '<':
				reqSize += 4;
				break;
			case '&':
				reqSize += 5;
				break;
			case '\'':
			case '\"':
				reqSize += 6;
				break;
			default:
				if (ch < 0x20 || ch >= 0x80)
				{
					reqSize += 10;
					break;
				}
		}
	}

	char* result = (char*)malloc(reqSize + 1);

	// copy string
	char* output = result;
	for (const char* p = raw; ; p++)
	{
		unsigned char ch = *p;
		switch (ch)
		{
			case '\0':
				goto BreakLoop;
			case '<':
				strcpy(output, "&lt;");
				output += 4;
				break;
			case '>':
				strcpy(output, "&gt;");
				output += 4;
				break;
			case '&':
				strcpy(output, "&amp;");
				output += 5;
				break;
			case '\'':
				strcpy(output, "&apos;");
				output += 6;
				break;
			case '\"':
				strcpy(output, "&quot;");
				output += 6;
				break;
			default:
				if (ch < 0x20 || ch > 0x80)
				{
					unsigned int cp = ch;

					// decode utf8
					if ((cp >> 5) == 0x6 && (p[1] & 0xc0) == 0x80)
					{
						// 2 bytes
						if (!(ch = *++p)) goto BreakLoop; // read next char
						cp = ((cp << 6) & 0x7ff) + (ch & 0x3f);
					}
					else if ((cp >> 4) == 0xe && (p[1] & 0xc0) == 0x80)
					{
						// 3 bytes
						if (!(ch = *++p)) goto BreakLoop; // read next char
						cp = ((cp << 12) & 0xffff) + ((ch << 6) & 0xfff);
						if (!(ch = *++p)) goto BreakLoop; // read next char
						cp += ch & 0x3f;
					}
					else if ((cp >> 3) == 0x1e && (p[1] & 0xc0) == 0x80)
					{
						// 4 bytes
						if (!(ch = *++p)) goto BreakLoop; // read next char
						cp = ((cp << 18) & 0x1fffff) + ((ch << 12) & 0x3ffff);
						if (!(ch = *++p)) goto BreakLoop; // read next char
						cp += (ch << 6) & 0xfff;
						if (!(ch = *++p)) goto BreakLoop; // read next char
						cp += ch & 0x3f;
					}

					// accept only valid XML 1.0 characters
					if (cp == 0x9 || cp == 0xA || cp == 0xD ||
						(0x20 <= cp && cp <= 0xD7FF) ||
						(0xE000 <= cp && cp <= 0xFFFD) ||
						(0x10000 <= cp && cp <= 0x10FFFF))
					{
						sprintf(output, "&#x%06x;", cp);
						output += 10;
					}
					else
					{
						// replace invalid characters with dots
						sprintf(output, ".");
						output += 1;
					}
				}
				else
				{
					*output++ = ch;
				}
				break;
		}
	}
BreakLoop:

	*output = '\0';

	return result;
}

void WebUtil::XmlDecode(char* raw)
{
	char* output = raw;
	for (char* p = raw;;)
	{
		switch (*p)
		{
			case '\0':
				goto BreakLoop;
			case '&':
				{
					p++;
					if (!strncmp(p, "lt;", 3))
					{
						*output++ = '<';
						p += 3;
					}
					else if (!strncmp(p, "gt;", 3))
					{
						*output++ = '>';
						p += 3;
					}
					else if (!strncmp(p, "amp;", 4))
					{
						*output++ = '&';
						p += 4;
					}
					else if (!strncmp(p, "apos;", 5))
					{
						*output++ = '\'';
						p += 5;
					}
					else if (!strncmp(p, "quot;", 5))
					{
						*output++ = '\"';
						p += 5;
					}
					else if (*p == '#')
					{
						int code = atoi((p++)+1);
						while (strchr("0123456789;", *p)) p++;
						*output++ = (char)code;
					}
					else
					{
						// unknown entity, keep as is
						*output++ = *(p-1);
						*output++ = *p++;
					}
					break;
				}
			default:
				*output++ = *p++;
				break;
		}
	}
BreakLoop:

	*output = '\0';
}

const char* WebUtil::XmlFindTag(const char* xml, const char* tag, int* valueLength)
{
	char openTag[100];
	snprintf(openTag, 100, "<%s>", tag);
	openTag[100-1] = '\0';

	char closeTag[100];
	snprintf(closeTag, 100, "</%s>", tag);
	closeTag[100-1] = '\0';

	char openCloseTag[100];
	snprintf(openCloseTag, 100, "<%s/>", tag);
	openCloseTag[100-1] = '\0';

	const char* pstart = strstr(xml, openTag);
	const char* pstartend = strstr(xml, openCloseTag);
	if (!pstart && !pstartend) return NULL;

	if (pstartend && (!pstart || pstartend < pstart))
	{
		*valueLength = 0;
		return pstartend;
	}

	const char* pend = strstr(pstart, closeTag);
	if (!pend) return NULL;

	int tagLen = strlen(openTag);
	*valueLength = (int)(pend - pstart - tagLen);

	return pstart + tagLen;
}

bool WebUtil::XmlParseTagValue(const char* xml, const char* tag, char* valueBuf, int valueBufSize, const char** tagEnd)
{
	int valueLen = 0;
	const char* value = XmlFindTag(xml, tag, &valueLen);
	if (!value)
	{
		return false;
	}
	int len = valueLen < valueBufSize ? valueLen : valueBufSize - 1;
	strncpy(valueBuf, value, len);
	valueBuf[len] = '\0';
	if (tagEnd)
	{
		*tagEnd = value + valueLen;
	}
	return true;
}

void WebUtil::XmlStripTags(char* xml)
{
	while (char *start = strchr(xml, '<'))
	{
		char *end = strchr(start, '>');
		if (!end)
		{
			break;
		}
		memset(start, ' ', end - start + 1);
		xml = end + 1;
	}
}

void WebUtil::XmlRemoveEntities(char* raw)
{
	char* output = raw;
	for (char* p = raw;;)
	{
		switch (*p)
		{
			case '\0':
				goto BreakLoop;
			case '&':
			{
				char* p2 = p+1;
				while (isalpha(*p2) || strchr("0123456789#", *p2)) p2++;
				if (*p2 == ';')
				{
					*output++ = ' ';
					p = p2+1;
				}
				else
				{
					*output++ = *p++;
				}
				break;
			}
			default:
				*output++ = *p++;
				break;
		}
	}
BreakLoop:

	*output = '\0';
}

char* WebUtil::JsonEncode(const char* raw)
{
	// calculate the required outputstring-size based on number of escape-entities and their sizes
	int reqSize = strlen(raw);
	for (const char* p = raw; *p; p++)
	{
		unsigned char ch = *p;
		switch (ch)
		{
			case '\"':
			case '\\':
			case '/':
			case '\b':
			case '\f':
			case '\n':
			case '\r':
			case '\t':
				reqSize++;
				break;
			default:
				if (ch < 0x20 || ch >= 0x80)
				{
					reqSize += 6;
					break;
				}
		}
	}

	char* result = (char*)malloc(reqSize + 1);

	// copy string
	char* output = result;
	for (const char* p = raw; ; p++)
	{
		unsigned char ch = *p;
		switch (ch)
		{
			case '\0':
				goto BreakLoop;
			case '"':
				strcpy(output, "\\\"");
				output += 2;
				break;
			case '\\':
				strcpy(output, "\\\\");
				output += 2;
				break;
			case '/':
				strcpy(output, "\\/");
				output += 2;
				break;
			case '\b':
				strcpy(output, "\\b");
				output += 2;
				break;
			case '\f':
				strcpy(output, "\\f");
				output += 2;
				break;
			case '\n':
				strcpy(output, "\\n");
				output += 2;
				break;
			case '\r':
				strcpy(output, "\\r");
				output += 2;
				break;
			case '\t':
				strcpy(output, "\\t");
				output += 2;
				break;
			default:
				if (ch < 0x20 || ch > 0x80)
				{
					unsigned int cp = ch;

					// decode utf8
					if ((cp >> 5) == 0x6 && (p[1] & 0xc0) == 0x80)
					{
						// 2 bytes
						if (!(ch = *++p)) goto BreakLoop; // read next char
						cp = ((cp << 6) & 0x7ff) + (ch & 0x3f);
					}
					else if ((cp >> 4) == 0xe && (p[1] & 0xc0) == 0x80)
					{
						// 3 bytes
						if (!(ch = *++p)) goto BreakLoop; // read next char
						cp = ((cp << 12) & 0xffff) + ((ch << 6) & 0xfff);
						if (!(ch = *++p)) goto BreakLoop; // read next char
						cp += ch & 0x3f;
					}
					else if ((cp >> 3) == 0x1e && (p[1] & 0xc0) == 0x80)
					{
						// 4 bytes
						if (!(ch = *++p)) goto BreakLoop; // read next char
						cp = ((cp << 18) & 0x1fffff) + ((ch << 12) & 0x3ffff);
						if (!(ch = *++p)) goto BreakLoop; // read next char
						cp += (ch << 6) & 0xfff;
						if (!(ch = *++p)) goto BreakLoop; // read next char
						cp += ch & 0x3f;
					}

					// we support only Unicode range U+0000-U+FFFF
					sprintf(output, "\\u%04x", cp <= 0xFFFF ? cp : '.');
					output += 6;
				}
				else
				{
					*output++ = ch;
				}
				break;
		}
	}
BreakLoop:

	*output = '\0';

	return result;
}

void WebUtil::JsonDecode(char* raw)
{
	char* output = raw;
	for (char* p = raw;;)
	{
		switch (*p)
		{
			case '\0':
				goto BreakLoop;
			case '\\':
				{
					p++;
					switch (*p)
					{
						case '"':
							*output++ = '"';
							break;
						case '\\':
							*output++ = '\\';
							break;
						case '/':
							*output++ = '/';
							break;
						case 'b':
							*output++ = '\b';
							break;
						case 'f':
							*output++ = '\f';
							break;
						case 'n':
							*output++ = '\n';
							break;
						case 'r':
							*output++ = '\r';
							break;
						case 't':
							*output++ = '\t';
							break;
						case 'u':
							*output++ = (char)strtol(p + 1, NULL, 16);
							p += 4;
							break;
						default:
							// unknown escape-sequence, should never occur
							*output++ = *p;
							break;
					}
					p++;
					break;
				}
			default:
				*output++ = *p++;
				break;
		}
	}
BreakLoop:

	*output = '\0';
}

const char* WebUtil::JsonFindField(const char* jsonText, const char* fieldName, int* valueLength)
{
	char openTag[100];
	snprintf(openTag, 100, "\"%s\"", fieldName);
	openTag[100-1] = '\0';

	const char* pstart = strstr(jsonText, openTag);
	if (!pstart) return NULL;

	pstart += strlen(openTag);

	return JsonNextValue(pstart, valueLength);
}

const char* WebUtil::JsonNextValue(const char* jsonText, int* valueLength)
{
	const char* pstart = jsonText;

	while (*pstart && strchr(" ,[{:\r\n\t\f", *pstart)) pstart++;
	if (!*pstart) return NULL;

	const char* pend = pstart;

	char ch = *pend;
	bool str = ch == '"';
	if (str)
	{
		ch = *++pend;
	}
	while (ch)
	{
		if (ch == '\\')
		{
			if (!*++pend || !*++pend) return NULL;
			ch = *pend;
		}
		if (str && ch == '"')
		{
			pend++;
			break;
		}
		else if (!str && strchr(" ,]}\r\n\t\f", ch))
		{
			break;
		}
		ch = *++pend;
	}

	*valueLength = (int)(pend - pstart);
	return pstart;
}

void WebUtil::HttpUnquote(char* raw)
{
	if (*raw != '"')
	{
		return;
	}

	char *output = raw;
	for (char *p = raw+1;;)
	{
		switch (*p)
		{
			case '\0':
			case '"':
				goto BreakLoop;
			case '\\':
				p++;
				*output++ = *p;
				break;
			default:
				*output++ = *p++;
				break;
		}
	}
BreakLoop:

	*output = '\0';
}

void WebUtil::UrlDecode(char* raw)
{
	char* output = raw;
	for (char* p = raw;;)
	{
		switch (*p)
		{
			case '\0':
				goto BreakLoop;
			case '%':
				{
					p++;
					unsigned char c1 = *p++;
					unsigned char c2 = *p++;
					c1 = '0' <= c1 && c1 <= '9' ? c1 - '0' : 'A' <= c1 && c1 <= 'F' ? c1 - 'A' + 10 :
						'a' <= c1 && c1 <= 'f' ? c1 - 'a' + 10 : 0;
					c2 = '0' <= c2 && c2 <= '9' ? c2 - '0' : 'A' <= c2 && c2 <= 'F' ? c2 - 'A' + 10 :
						'a' <= c2 && c2 <= 'f' ? c2 - 'a' + 10 : 0;
					unsigned char ch = (c1 << 4) + c2;
					*output++ = (char)ch;
					break;
				}
			default:
				*output++ = *p++;
				break;
		}
	}
BreakLoop:

	*output = '\0';
}

char* WebUtil::UrlEncode(const char* raw)
{
	// calculate the required outputstring-size based on number of spaces
	int reqSize = strlen(raw);
	for (const char* p = raw; *p; p++)
	{
		if (*p == ' ')
		{
			reqSize += 3; // length of "%20"
		}
	}

	char* result = (char*)malloc(reqSize + 1);

	// copy string
	char* output = result;
	for (const char* p = raw; ; p++)
	{
		unsigned char ch = *p;
		switch (ch)
		{
			case '\0':
				goto BreakLoop;
			case ' ':
				strcpy(output, "%20");
				output += 3;
				break;
			default:
				*output++ = ch;
		}
	}
BreakLoop:

	*output = '\0';

	return result;
}

#ifdef WIN32
bool WebUtil::Utf8ToAnsi(char* buffer, int bufLen)
{
	WCHAR* wstr = (WCHAR*)malloc(bufLen * 2);
	int errcode = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wstr, bufLen);
	if (errcode > 0)
	{
		errcode = WideCharToMultiByte(CP_ACP, 0, wstr, -1, buffer, bufLen, "_", NULL);
	}
	free(wstr);
	return errcode > 0;
}

bool WebUtil::AnsiToUtf8(char* buffer, int bufLen)
{
	WCHAR* wstr = (WCHAR*)malloc(bufLen * 2);
	int errcode = MultiByteToWideChar(CP_ACP, 0, buffer, -1, wstr, bufLen);
	if (errcode > 0)
	{
		errcode = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, buffer, bufLen, NULL, NULL);
	}
	free(wstr);
	return errcode > 0;
}
#endif

char* WebUtil::Latin1ToUtf8(const char* str)
{
	char *res = (char*)malloc(strlen(str) * 2 + 1);
	const unsigned char *in = (const unsigned char*)str;
	unsigned char *out = (unsigned char*)res;
	while (*in)
	{
		if (*in < 128)
		{
			*out++ = *in++;
		}
		else
		{
			*out++ = 0xc2 + (*in > 0xbf);
			*out++ = (*in++ & 0x3f) + 0x80;
		}
	}
	*out = '\0';
	return res;
}

/*
 The date/time can be formatted according to RFC822 in different ways. Examples:
   Wed, 26 Jun 2013 01:02:54 -0600
   Wed, 26 Jun 2013 01:02:54 GMT
   26 Jun 2013 01:02:54 -0600
   26 Jun 2013 01:02 -0600
   26 Jun 2013 01:02 A
 This function however supports only the first format!
*/
time_t WebUtil::ParseRfc822DateTime(const char* dateTimeStr)
{
	char month[4];
	int day, year, hours, minutes, seconds, zonehours, zoneminutes;
	int r = sscanf(dateTimeStr, "%*s %d %3s %d %d:%d:%d %3d %2d", &day, &month[0], &year, &hours, &minutes, &seconds, &zonehours, &zoneminutes);
	if (r != 8)
	{
		return 0;
	}

	int mon = 0;
	if (!strcasecmp(month, "Jan")) mon = 0;
	else if (!strcasecmp(month, "Feb")) mon = 1;
	else if (!strcasecmp(month, "Mar")) mon = 2;
	else if (!strcasecmp(month, "Apr")) mon = 3;
	else if (!strcasecmp(month, "May")) mon = 4;
	else if (!strcasecmp(month, "Jun")) mon = 5;
	else if (!strcasecmp(month, "Jul")) mon = 6;
	else if (!strcasecmp(month, "Aug")) mon = 7;
	else if (!strcasecmp(month, "Sep")) mon = 8;
	else if (!strcasecmp(month, "Oct")) mon = 9;
	else if (!strcasecmp(month, "Nov")) mon = 10;
	else if (!strcasecmp(month, "Dec")) mon = 11;

	struct tm rawtime;
	memset(&rawtime, 0, sizeof(rawtime));

	rawtime.tm_year = year - 1900;
	rawtime.tm_mon = mon;
	rawtime.tm_mday = day;
	rawtime.tm_hour = hours;
	rawtime.tm_min = minutes;
	rawtime.tm_sec = seconds;

	time_t enctime = Util::Timegm(&rawtime);
	enctime -= (zonehours * 60 + (zonehours > 0 ? zoneminutes : -zoneminutes)) * 60;

	return enctime;
}


URL::URL(const char* address)
{
	m_address = NULL;
	m_protocol = NULL;
	m_user = NULL;
	m_password = NULL;
	m_host = NULL;
	m_resource = NULL;
	m_port = 0;
	m_tls = false;
	m_valid = false;

	if (address)
	{
		m_address = strdup(address);
		ParseUrl();
	}
}

URL::~URL()
{
	free(m_address);
	free(m_protocol);
	free(m_user);
	free(m_password);
	free(m_host);
	free(m_resource);
}

void URL::ParseUrl()
{
	// Examples:
	// http://user:password@host:port/path/to/resource?param
	// http://user@host:port/path/to/resource?param
	// http://host:port/path/to/resource?param
	// http://host/path/to/resource?param
	// http://host

	char* protEnd = strstr(m_address, "://");
	if (!protEnd)
	{
		// Bad URL
		return;
	}

	m_protocol = (char*)malloc(protEnd - m_address + 1);
	strncpy(m_protocol, m_address, protEnd - m_address);
	m_protocol[protEnd - m_address] = 0;

	char* hostStart = protEnd + 3;
	char* slash = strchr(hostStart, '/');
	char* hostEnd = NULL;
	char* amp = strchr(hostStart, '@');

	if (amp && (!slash || amp < slash))
	{
		// parse user/password
		char* userend = amp - 1;
		char* pass = strchr(hostStart, ':');
		if (pass && pass < amp)
		{
			int len = (int)(amp - pass - 1);
			if (len > 0)
			{
				m_password = (char*)malloc(len + 1);
				strncpy(m_password, pass + 1, len);
				m_password[len] = 0;
			}
			userend = pass - 1;
		}

		int len = (int)(userend - hostStart + 1);
		if (len > 0)
		{
			m_user = (char*)malloc(len + 1);
			strncpy(m_user, hostStart, len);
			m_user[len] = 0;
		}

		hostStart = amp + 1;
	}

	if (slash)
	{
		char* resEnd = m_address + strlen(m_address);
		m_resource = (char*)malloc(resEnd - slash + 1 + 1);
		strncpy(m_resource, slash, resEnd - slash + 1);
		m_resource[resEnd - slash + 1] = 0;

		hostEnd = slash - 1;
	}
	else
	{
		m_resource = strdup("/");

		hostEnd = m_address + strlen(m_address);
	}

	char* colon = strchr(hostStart, ':');
	if (colon && colon < hostEnd)
	{
		hostEnd = colon - 1;
		m_port = atoi(colon + 1);
	}

	m_host = (char*)malloc(hostEnd - hostStart + 1 + 1);
	strncpy(m_host, hostStart, hostEnd - hostStart + 1);
	m_host[hostEnd - hostStart + 1] = 0;

	m_valid = true;
}

RegEx::RegEx(const char *pattern, int matchBufSize)
{
#ifdef HAVE_REGEX_H
	m_context = malloc(sizeof(regex_t));
	m_valid = regcomp((regex_t*)m_context, pattern, REG_EXTENDED | REG_ICASE | (matchBufSize > 0 ? 0 : REG_NOSUB)) == 0;
	m_matchBufSize = matchBufSize;
	if (matchBufSize > 0)
	{
		m_matches = malloc(sizeof(regmatch_t) * matchBufSize);
	}
	else
	{
		m_matches = NULL;
	}
#else
	m_valid = false;
#endif
}

RegEx::~RegEx()
{
#ifdef HAVE_REGEX_H
	regfree((regex_t*)m_context);
	free(m_context);
	free(m_matches);
#endif
}

bool RegEx::Match(const char *str)
{
#ifdef HAVE_REGEX_H
	return m_valid ? regexec((regex_t*)m_context, str, m_matchBufSize, (regmatch_t*)m_matches, 0) == 0 : false;
#else
	return false;
#endif
}

int RegEx::GetMatchCount()
{
#ifdef HAVE_REGEX_H
	int count = 0;
	if (m_matches)
	{
		regmatch_t* matches = (regmatch_t*)m_matches;
		while (count < m_matchBufSize && matches[count].rm_so > -1)
		{
			count++;
		}
	}
	return count;
#else
	return 0;
#endif
}

int RegEx::GetMatchStart(int index)
{
#ifdef HAVE_REGEX_H
	regmatch_t* matches = (regmatch_t*)m_matches;
	return matches[index].rm_so;
#else
	return NULL;
#endif
}

int RegEx::GetMatchLen(int index)
{
#ifdef HAVE_REGEX_H
	regmatch_t* matches = (regmatch_t*)m_matches;
	return matches[index].rm_eo - matches[index].rm_so;
#else
	return 0;
#endif
}


WildMask::WildMask(const char *pattern, bool wantsPositions)
{
	m_pattern = strdup(pattern);
	m_wantsPositions = wantsPositions;
	m_wildStart = NULL;
	m_wildLen = NULL;
	m_arrLen = 0;
}

WildMask::~WildMask()
{
	free(m_pattern);
	free(m_wildStart);
	free(m_wildLen);
}

void WildMask::ExpandArray()
{
	m_wildCount++;
	if (m_wildCount > m_arrLen)
	{
		m_arrLen += 100;
		m_wildStart = (int*)realloc(m_wildStart, sizeof(*m_wildStart) * m_arrLen);
		m_wildLen = (int*)realloc(m_wildLen, sizeof(*m_wildLen) * m_arrLen);
	}
}

// Based on code from http://bytes.com/topic/c/answers/212179-string-matching
// Extended to save positions of matches.
bool WildMask::Match(const char* text)
{
	const char* pat = m_pattern;
	const char* str = text;
	const char *spos, *wpos;
	m_wildCount = 0;
	bool qmark = false;
	bool star = false;

	spos = wpos = str;
	while (*str && *pat != '*')
	{
		if (m_wantsPositions && (*pat == '?' || *pat == '#'))
		{
			if (!qmark)
			{
				ExpandArray();
				m_wildStart[m_wildCount-1] = str - text;
				m_wildLen[m_wildCount-1] = 0;
				qmark = true;
			}
		}
		else if (m_wantsPositions && qmark)
		{
			m_wildLen[m_wildCount-1] = str - (text + m_wildStart[m_wildCount-1]);
			qmark = false;
		}

		if (!(tolower(*pat) == tolower(*str) || *pat == '?' ||
			(*pat == '#' && strchr("0123456789", *str))))
		{
			return false;
		}
		str++;
		pat++;
	}

	if (m_wantsPositions && qmark)
	{
		m_wildLen[m_wildCount-1] = str - (text + m_wildStart[m_wildCount-1]);
		qmark = false;
	}

	while (*str)
	{
		if (*pat == '*')
		{
			if (m_wantsPositions && qmark)
			{
				m_wildLen[m_wildCount-1] = str - (text + m_wildStart[m_wildCount-1]);
				qmark = false;
			}
			if (m_wantsPositions && !star)
			{
				ExpandArray();
				m_wildStart[m_wildCount-1] = str - text;
				m_wildLen[m_wildCount-1] = 0;
				star = true;
			}

			if (*++pat == '\0')
			{
				if (m_wantsPositions && star)
				{
					m_wildLen[m_wildCount-1] = strlen(str);
				}

				return true;
			}
			wpos = pat;
			spos = str + 1;
		}
		else if (*pat == '?' || (*pat == '#' && strchr("0123456789", *str)))
		{
			if (m_wantsPositions && !qmark)
			{
				ExpandArray();
				m_wildStart[m_wildCount-1] = str - text;
				m_wildLen[m_wildCount-1] = 0;
				qmark = true;
			}

			pat++;
			str++;
		}
		else if (tolower(*pat) == tolower(*str))
		{
			if (m_wantsPositions && qmark)
			{
				m_wildLen[m_wildCount-1] = str - (text + m_wildStart[m_wildCount-1]);
				qmark = false;
			}
			else if (m_wantsPositions && star)
			{
				m_wildLen[m_wildCount-1] = str - (text + m_wildStart[m_wildCount-1]);
				star = false;
			}

			pat++;
			str++;
		}
		else
		{
			if (m_wantsPositions && qmark)
			{
				m_wildCount--;
				qmark = false;
			}

			pat = wpos;
			str = spos++;
			star = true;
		}
	}

	if (m_wantsPositions && qmark)
	{
		m_wildLen[m_wildCount-1] = str - (text + m_wildStart[m_wildCount-1]);
	}

	if (*pat == '*' && m_wantsPositions && !star)
	{
		ExpandArray();
		m_wildStart[m_wildCount-1] = str - text;
		m_wildLen[m_wildCount-1] = strlen(str);
	}

	while (*pat == '*')
	{
		pat++;
	}

	return *pat == '\0';
}


#ifndef DISABLE_GZIP
unsigned int ZLib::GZipLen(int inputBufferLength)
{
	z_stream zstr;
	memset(&zstr, 0, sizeof(zstr));
	return (unsigned int)deflateBound(&zstr, inputBufferLength);
}

unsigned int ZLib::GZip(const void* inputBuffer, int inputBufferLength, void* outputBuffer, int outputBufferLength)
{
	z_stream zstr;
	zstr.zalloc = Z_NULL;
	zstr.zfree = Z_NULL;
	zstr.opaque = Z_NULL;
	zstr.next_in = (Bytef*)inputBuffer;
	zstr.avail_in = inputBufferLength;
	zstr.next_out = (Bytef*)outputBuffer;
	zstr.avail_out = outputBufferLength;

	/* add 16 to MAX_WBITS to enforce gzip format */
	if (Z_OK != deflateInit2(&zstr, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY))
	{
		return 0;
	}

	unsigned int total_out = 0;
	if (deflate(&zstr, Z_FINISH) == Z_STREAM_END)
	{
		total_out = (unsigned int)zstr.total_out;
	}

	deflateEnd(&zstr);

	return total_out;
}

GUnzipStream::GUnzipStream(int BufferSize)
{
	m_bufferSize = BufferSize;
	m_zStream = malloc(sizeof(z_stream));
	m_outputBuffer = malloc(BufferSize);

	memset(m_zStream, 0, sizeof(z_stream));

	/* add 16 to MAX_WBITS to enforce gzip format */
	int ret = inflateInit2(((z_stream*)m_zStream), MAX_WBITS + 16);
	if (ret != Z_OK)
	{
		free(m_zStream);
		m_zStream = NULL;
	}
}

GUnzipStream::~GUnzipStream()
{
	if (m_zStream)
	{
		inflateEnd(((z_stream*)m_zStream));
		free(m_zStream);
	}
	free(m_outputBuffer);
}

void GUnzipStream::Write(const void *inputBuffer, int inputBufferLength)
{
	((z_stream*)m_zStream)->next_in = (Bytef*)inputBuffer;
	((z_stream*)m_zStream)->avail_in = inputBufferLength;
}

GUnzipStream::EStatus GUnzipStream::Read(const void **outputBuffer, int *outputBufferLength)
{
	((z_stream*)m_zStream)->next_out = (Bytef*)m_outputBuffer;
	((z_stream*)m_zStream)->avail_out = m_bufferSize;

	*outputBufferLength = 0;

	if (!m_zStream)
	{
		return zlError;
	}

	int ret = inflate(((z_stream*)m_zStream), Z_NO_FLUSH);

	switch (ret)
	{
		case Z_STREAM_END:
		case Z_OK:
			*outputBufferLength = m_bufferSize - ((z_stream*)m_zStream)->avail_out;
			*outputBuffer = m_outputBuffer;
			return ret == Z_STREAM_END ? zlFinished : zlOK;

		case Z_BUF_ERROR:
			return zlOK;
	}

	return zlError;
}

#endif

Tokenizer::Tokenizer(const char* dataString, const char* separators)
{
	// an optimization to avoid memory allocation for short data string (shorten than 1024 chars)
	int len = strlen(dataString);
	if (len < sizeof(m_defaultBuf) - 1)
	{
		strncpy(m_defaultBuf, dataString, sizeof(m_defaultBuf));
		m_defaultBuf[1024- 1] = '\0';
		m_dataString = m_defaultBuf;
		m_inplaceBuf = true;
	}
	else
	{
		m_dataString = strdup(dataString);
		m_inplaceBuf = false;
	}

	m_separators = separators;
	m_savePtr = NULL;
	m_working = false;
}

Tokenizer::Tokenizer(char* dataString, const char* separators, bool inplaceBuf)
{
	m_dataString = inplaceBuf ? dataString : strdup(dataString);
	m_separators = separators;
	m_savePtr = NULL;
	m_working = false;
	m_inplaceBuf = inplaceBuf;
}

Tokenizer::~Tokenizer()
{
	if (!m_inplaceBuf)
	{
		free(m_dataString);
	}
}

char* Tokenizer::Next()
{
	char* token = NULL;
	while (!token || !*token)
	{
		token = strtok_r(m_working ? NULL : m_dataString, m_separators, &m_savePtr);
		m_working = true;
		if (!token)
		{
			return NULL;
		}
		token = Util::Trim(token);
	}
	return token;
}
