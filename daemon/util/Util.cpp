/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#ifdef WIN32
#include <io.h>
#include <direct.h>
#include <WinIoCtl.h>
#else
#include <unistd.h>
#include <sys/statvfs.h>
#include <pwd.h>
#include <dirent.h>
#endif
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif
#ifndef DISABLE_GZIP
#include <zlib.h>
#endif
#include <time.h>

#include "nzbget.h"
#include "Util.h"

#ifndef WIN32
// function "svn_version" is automatically generated in file "svn_version.cpp" on each build
const char* svn_version(void);
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

DirBrowser::DirBrowser(const char* szPath)
{
	char szMask[MAX_PATH + 1];
	snprintf(szMask, MAX_PATH + 1, "%s%c*.*", szPath, (int)PATH_SEPARATOR);
	szMask[MAX_PATH] = '\0';
	m_hFile = FindFirstFile(szMask, &m_FindData);
	m_bFirst = true;
}

DirBrowser::~DirBrowser()
{
	if (m_hFile != INVALID_HANDLE_VALUE)
	{
		FindClose(m_hFile);
	}
}

const char* DirBrowser::Next()
{
	bool bOK = false;
	if (m_bFirst)
	{
		bOK = m_hFile != INVALID_HANDLE_VALUE;
		m_bFirst = false;
	}
	else
	{
		bOK = FindNextFile(m_hFile, &m_FindData) != 0;
	}
	if (bOK)
	{
		return m_FindData.cFileName;
	}
	return NULL;
}

#else

#ifdef DIRBROWSER_SNAPSHOT
DirBrowser::DirBrowser(const char* szPath, bool bSnapshot)
#else
DirBrowser::DirBrowser(const char* szPath)
#endif
{
#ifdef DIRBROWSER_SNAPSHOT
	m_bSnapshot = bSnapshot;
	if (m_bSnapshot)
	{
		DirBrowser dir(szPath, false);
		while (const char* filename = dir.Next())
		{
			m_Snapshot.push_back(strdup(filename));
		}
		m_itSnapshot = m_Snapshot.begin();
	}
	else
#endif
	{
		m_pDir = opendir(szPath);
	}
}

DirBrowser::~DirBrowser()
{
#ifdef DIRBROWSER_SNAPSHOT
	if (m_bSnapshot)
	{
		for (FileList::iterator it = m_Snapshot.begin(); it != m_Snapshot.end(); it++)
		{
			delete *it;
		}
	}
	else
#endif
	{
		if (m_pDir)
		{
			closedir((DIR*)m_pDir);
		}
	}
}

const char* DirBrowser::Next()
{
#ifdef DIRBROWSER_SNAPSHOT
	if (m_bSnapshot)
	{
		return m_itSnapshot == m_Snapshot.end() ? NULL : *m_itSnapshot++;
	}
	else
#endif
	{
		if (m_pDir)
		{
			m_pFindData = readdir((DIR*)m_pDir);
			if (m_pFindData)
			{
				return m_pFindData->d_name;
			}
		}
		return NULL;
	}
}

#endif


StringBuilder::StringBuilder()
{
	m_szBuffer = NULL;
	m_iBufferSize = 0;
	m_iUsedSize = 0;
}

StringBuilder::~StringBuilder()
{
	free(m_szBuffer);
}

void StringBuilder::Append(const char* szStr)
{
	int iPartLen = strlen(szStr);
	if (m_iUsedSize + iPartLen + 1 > m_iBufferSize)
	{
		m_iBufferSize += iPartLen + 10240;
		m_szBuffer = (char*)realloc(m_szBuffer, m_iBufferSize);
	}
	strcpy(m_szBuffer + m_iUsedSize, szStr);
	m_iUsedSize += iPartLen;
	m_szBuffer[m_iUsedSize] = '\0';
}


char Util::VersionRevisionBuf[40];

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

void Util::NormalizePathSeparators(char* szPath)
{
	for (char* p = szPath; *p; p++) 
	{
		if (*p == ALT_PATH_SEPARATOR) 
		{
			*p = PATH_SEPARATOR;
		}
	}
}

bool Util::ForceDirectories(const char* szPath, char* szErrBuf, int iBufSize)
{
	*szErrBuf = '\0';
	char szSysErrStr[256];
	char szNormPath[1024];
	strncpy(szNormPath, szPath, 1024);
	szNormPath[1024-1] = '\0';
	NormalizePathSeparators(szNormPath);
	int iLen = strlen(szNormPath);
	if ((iLen > 0) && szNormPath[iLen-1] == PATH_SEPARATOR
#ifdef WIN32
		&& iLen > 3
#endif
		)
	{
		szNormPath[iLen-1] = '\0';
	}

	struct stat buffer;
	bool bOK = !stat(szNormPath, &buffer);
	if (!bOK && errno != ENOENT)
	{
		snprintf(szErrBuf, iBufSize, "could not read information for directory %s: errno %i, %s", szNormPath, errno, GetLastErrorMessage(szSysErrStr, sizeof(szSysErrStr)));
		szErrBuf[iBufSize-1] = 0;
		return false;
	}
	
	if (bOK && !S_ISDIR(buffer.st_mode))
	{
		snprintf(szErrBuf, iBufSize, "path %s is not a directory", szNormPath);
		szErrBuf[iBufSize-1] = 0;
		return false;
	}
	
	if (!bOK
#ifdef WIN32
		&& strlen(szNormPath) > 2
#endif
		)
	{
		char szParentPath[1024];
		strncpy(szParentPath, szNormPath, 1024);
		szParentPath[1024-1] = '\0';
		char* p = (char*)strrchr(szParentPath, PATH_SEPARATOR);
		if (p)
		{
#ifdef WIN32
			if (p - szParentPath == 2 && szParentPath[1] == ':' && strlen(szParentPath) > 2)
			{
				szParentPath[3] = '\0';
			}
			else
#endif
			{
				*p = '\0';
			}
			if (strlen(szParentPath) != strlen(szPath) && !ForceDirectories(szParentPath, szErrBuf, iBufSize))
			{
				return false;
			}
		}
		
		if (mkdir(szNormPath, S_DIRMODE) != 0 && errno != EEXIST)
		{
			snprintf(szErrBuf, iBufSize, "could not create directory %s: %s", szNormPath, GetLastErrorMessage(szSysErrStr, sizeof(szSysErrStr)));
			szErrBuf[iBufSize-1] = 0;
			return false;
		}
			
		if (stat(szNormPath, &buffer) != 0)
		{
			snprintf(szErrBuf, iBufSize, "could not read information for directory %s: %s", szNormPath, GetLastErrorMessage(szSysErrStr, sizeof(szSysErrStr)));
			szErrBuf[iBufSize-1] = 0;
			return false;
		}
		
		if (!S_ISDIR(buffer.st_mode))
		{
			snprintf(szErrBuf, iBufSize, "path %s is not a directory", szNormPath);
			szErrBuf[iBufSize-1] = 0;
			return false;
		}
	}

	return true;
}

bool Util::GetCurrentDirectory(char* szBuffer, int iBufSize)
{
#ifdef WIN32
	return ::GetCurrentDirectory(iBufSize, szBuffer) != NULL;
#else
	return getcwd(szBuffer, iBufSize) != NULL;
#endif
}

bool Util::SetCurrentDirectory(const char* szDirFilename)
{
#ifdef WIN32
	return ::SetCurrentDirectory(szDirFilename);
#else
	return chdir(szDirFilename) == 0;
#endif
}

bool Util::DirEmpty(const char* szDirFilename)
{
	DirBrowser dir(szDirFilename);
	while (const char* filename = dir.Next())
	{
		if (strcmp(filename, ".") && strcmp(filename, ".."))
		{
			return false;
		}
	}
	return true;
}

bool Util::LoadFileIntoBuffer(const char* szFileName, char** pBuffer, int* pBufferLength)
{
    FILE* pFile = fopen(szFileName, FOPEN_RB);
    if (!pFile)
    {
        return false;
    }

    // obtain file size.
    fseek(pFile , 0 , SEEK_END);
    int iSize  = (int)ftell(pFile);
    rewind(pFile);

    // allocate memory to contain the whole file.
    *pBuffer = (char*) malloc(iSize + 1);
    if (!*pBuffer)
    {
        return false;
    }

    // copy the file into the buffer.
    fread(*pBuffer, 1, iSize, pFile);

    fclose(pFile);

    (*pBuffer)[iSize] = 0;

    *pBufferLength = iSize + 1;

    return true;
}

bool Util::SaveBufferIntoFile(const char* szFileName, const char* szBuffer, int iBufLen)
{
    FILE* pFile = fopen(szFileName, FOPEN_WB);
    if (!pFile)
    {
        return false;
    }

	int iWrittenBytes = fwrite(szBuffer, 1, iBufLen, pFile);
    fclose(pFile);

	return iWrittenBytes == iBufLen;
}

bool Util::CreateSparseFile(const char* szFilename, long long iSize)
{
	bool bOK = false;
#ifdef WIN32
	HANDLE hFile = CreateFile(szFilename, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_NEW, 0, NULL);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		// first try to create sparse file (supported only on NTFS partitions),
		// it may fail but that's OK.
		DWORD dwBytesReturned;
		DeviceIoControl(hFile, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &dwBytesReturned, NULL);

		LARGE_INTEGER iSize64;
		iSize64.QuadPart = iSize;
		SetFilePointerEx(hFile, iSize64, NULL, FILE_END);
		SetEndOfFile(hFile);
		CloseHandle(hFile);
		bOK = true;
	}
#else
	// create file
	FILE* pFile = fopen(szFilename, FOPEN_AB);
	if (pFile)
	{
		fclose(pFile);
	}
	// there are no reliable function to expand file on POSIX, so we must try different approaches,
	// starting with the fastest one and hoping it will work
	// 1) set file size using function "truncate" (it is fast, if it works)
	truncate(szFilename, iSize);
	// check if it worked
	pFile = fopen(szFilename, FOPEN_AB);
	if (pFile)
	{
		fseek(pFile, 0, SEEK_END);
		bOK = ftell(pFile) == iSize;
		if (!bOK)
		{
			// 2) truncate did not work, expanding the file by writing in it (it is slow)
			fclose(pFile);
			truncate(szFilename, 0);
			pFile = fopen(szFilename, FOPEN_AB);
			char c = '0';
			fwrite(&c, 1, iSize, pFile);
			bOK = ftell(pFile) == iSize;
		}
		fclose(pFile);
	}
#endif
	return bOK;
}

bool Util::TruncateFile(const char* szFilename, int iSize)
{
	bool bOK = false;
#ifdef WIN32
	FILE *file = fopen(szFilename, FOPEN_RBP);
	fseek(file, iSize, SEEK_SET);
	bOK = SetEndOfFile((HANDLE)_get_osfhandle(_fileno(file))) != 0;
	fclose(file);
#else
	bOK = truncate(szFilename, iSize) == 0;
#endif
	return bOK;
}

//replace bad chars in filename
void Util::MakeValidFilename(char* szFilename, char cReplaceChar, bool bAllowSlashes)
{
	const char* szReplaceChars = bAllowSlashes ? ":*?\"><\n\r\t" : "\\/:*?\"><\n\r\t";
	char* p = szFilename;
	while (*p)
	{
		if (strchr(szReplaceChars, *p))
		{
			*p = cReplaceChar;
		}
		if (bAllowSlashes && *p == ALT_PATH_SEPARATOR)
		{
			*p = PATH_SEPARATOR;
		}
		p++;
	}

	// remove trailing dots and spaces. they are not allowed in directory names on windows,
	// but we remove them on posix also, in a case the directory is accessed from windows via samba.
	for (int iLen = strlen(szFilename); iLen > 0 && (szFilename[iLen - 1] == '.' || szFilename[iLen - 1] == ' '); iLen--) 
	{
		szFilename[iLen - 1] = '\0';
	}
}

// returns TRUE if the name was changed by adding duplicate-suffix
bool Util::MakeUniqueFilename(char* szDestBufFilename, int iDestBufSize, const char* szDestDir, const char* szBasename)
{
	snprintf(szDestBufFilename, iDestBufSize, "%s%c%s", szDestDir, (int)PATH_SEPARATOR, szBasename);
	szDestBufFilename[iDestBufSize-1] = '\0';

	int iDupeNumber = 0;
	while (FileExists(szDestBufFilename))
	{
		iDupeNumber++;

		const char* szExtension = strrchr(szBasename, '.');
		if (szExtension && szExtension != szBasename)
		{
			char szFilenameWithoutExt[1024];
			strncpy(szFilenameWithoutExt, szBasename, 1024);
			int iEnd = szExtension - szBasename;
			szFilenameWithoutExt[iEnd < 1024 ? iEnd : 1024-1] = '\0';

			if (!strcasecmp(szExtension, ".par2"))
			{
				char* szVolExtension = strrchr(szFilenameWithoutExt, '.');
				if (szVolExtension && szVolExtension != szFilenameWithoutExt && !strncasecmp(szVolExtension, ".vol", 4))
				{
					*szVolExtension = '\0';
					szExtension = szBasename + (szVolExtension - szFilenameWithoutExt);
				}
			}

			snprintf(szDestBufFilename, iDestBufSize, "%s%c%s.duplicate%d%s", szDestDir, (int)PATH_SEPARATOR, szFilenameWithoutExt, iDupeNumber, szExtension);
		}
		else
		{
			snprintf(szDestBufFilename, iDestBufSize, "%s%c%s.duplicate%d", szDestDir, (int)PATH_SEPARATOR, szBasename, iDupeNumber);
		}

		szDestBufFilename[iDestBufSize-1] = '\0';
	}

	return iDupeNumber > 0;
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

float Util::Int64ToFloat(long long Int64)
{
	unsigned long Hi, Lo;
	SplitInt64(Int64, &Hi, &Lo);
	return ((unsigned long)(1 << 30)) * 4.0f * Hi + Lo;
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

unsigned int DecodeByteQuartet(char* szInputBuffer, char* szOutputBuffer)
{
	unsigned int buffer = 0;

	if (szInputBuffer[3] == '=')
	{
		if (szInputBuffer[2] == '=')
		{
			buffer = (buffer | BASE64_DEALPHABET [(int)szInputBuffer[0]]) << 6;
			buffer = (buffer | BASE64_DEALPHABET [(int)szInputBuffer[1]]) << 6;
			buffer = buffer << 14;

			szOutputBuffer [0] = (char)(buffer >> 24);
			
			return 1;
		}
		else
		{
			buffer = (buffer | BASE64_DEALPHABET [(int)szInputBuffer[0]]) << 6;
			buffer = (buffer | BASE64_DEALPHABET [(int)szInputBuffer[1]]) << 6;
			buffer = (buffer | BASE64_DEALPHABET [(int)szInputBuffer[2]]) << 6;
			buffer = buffer << 8;

			szOutputBuffer [0] = (char)(buffer >> 24);
			szOutputBuffer [1] = (char)(buffer >> 16);
			
			return 2;
		}
	}
	else
	{
		buffer = (buffer | BASE64_DEALPHABET [(int)szInputBuffer[0]]) << 6;
		buffer = (buffer | BASE64_DEALPHABET [(int)szInputBuffer[1]]) << 6;
		buffer = (buffer | BASE64_DEALPHABET [(int)szInputBuffer[2]]) << 6;
		buffer = (buffer | BASE64_DEALPHABET [(int)szInputBuffer[3]]) << 6; 
		buffer = buffer << 2;

		szOutputBuffer [0] = (char)(buffer >> 24);
		szOutputBuffer [1] = (char)(buffer >> 16);
		szOutputBuffer [2] = (char)(buffer >> 8);

		return 3;
	}

	return 0;
}

bool Util::MoveFile(const char* szSrcFilename, const char* szDstFilename)
{
	bool bOK = rename(szSrcFilename, szDstFilename) == 0;

#ifndef WIN32
	if (!bOK && errno == EXDEV)
	{
		FILE* infile = fopen(szSrcFilename, FOPEN_RB);
		if (!infile)
		{
			return false;
		}

		FILE* outfile = fopen(szDstFilename, FOPEN_WBP);
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

		bOK = remove(szSrcFilename) == 0;
	}
#endif

	return bOK;
}

bool Util::FileExists(const char* szFilename)
{
#ifdef WIN32
	// we use a native windows call because c-lib function "stat" fails on windows if file date is invalid
	WIN32_FIND_DATA findData;
	HANDLE handle = FindFirstFile(szFilename, &findData);
	if (handle != INVALID_HANDLE_VALUE)
	{
		bool bExists = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
		FindClose(handle);
		return bExists;
	}
	return false;
#else
	struct stat buffer;
	bool bExists = !stat(szFilename, &buffer) && S_ISREG(buffer.st_mode);
	return bExists;
#endif
}

bool Util::FileExists(const char* szPath, const char* szFilenameWithoutPath)
{
	char fullFilename[1024];
	snprintf(fullFilename, 1024, "%s%c%s", szPath, (int)PATH_SEPARATOR, szFilenameWithoutPath);
	fullFilename[1024-1] = '\0';
	bool bExists = Util::FileExists(fullFilename);
	return bExists;
}

bool Util::DirectoryExists(const char* szDirFilename)
{
#ifdef WIN32
	// we use a native windows call because c-lib function "stat" fails on windows if file date is invalid
	WIN32_FIND_DATA findData;
	HANDLE handle = FindFirstFile(szDirFilename, &findData);
	if (handle != INVALID_HANDLE_VALUE)
	{
		bool bExists = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		FindClose(handle);
		return bExists;
	}
	return false;
#else
	struct stat buffer;
	bool bExists = !stat(szDirFilename, &buffer) && S_ISDIR(buffer.st_mode);
	return bExists;
#endif
}

bool Util::CreateDirectory(const char* szDirFilename)
{
	mkdir(szDirFilename, S_DIRMODE);
	return DirectoryExists(szDirFilename);
}

bool Util::RemoveDirectory(const char* szDirFilename)
{
#ifdef WIN32
	return _rmdir(szDirFilename) == 0;
#else
	return remove(szDirFilename) == 0;
#endif
}

bool Util::DeleteDirectoryWithContent(const char* szDirFilename, char* szErrBuf, int iBufSize)
{
	*szErrBuf = '\0';
	char szSysErrStr[256];

	bool bDel = false;
	bool bOK = true;

	DirBrowser dir(szDirFilename);
	while (const char* filename = dir.Next())
	{
		char szFullFilename[1024];
		snprintf(szFullFilename, 1024, "%s%c%s", szDirFilename, PATH_SEPARATOR, filename);
		szFullFilename[1024-1] = '\0';

		if (strcmp(filename, ".") && strcmp(filename, ".."))
		{
			if (Util::DirectoryExists(szFullFilename))
			{
				bDel = DeleteDirectoryWithContent(szFullFilename, szSysErrStr, sizeof(szSysErrStr));
			}
			else
			{
				bDel = remove(szFullFilename) == 0;
			}
			bOK &= bDel;
			if (!bDel && !*szErrBuf)
			{
				snprintf(szErrBuf, iBufSize, "could not delete %s: %s", szFullFilename, GetLastErrorMessage(szSysErrStr, sizeof(szSysErrStr)));
			}
		}
	}

	bDel = RemoveDirectory(szDirFilename);
	bOK &= bDel;
	if (!bDel && !*szErrBuf)
	{
		GetLastErrorMessage(szErrBuf, iBufSize);
	}
	return bOK;
}

long long Util::FileSize(const char* szFilename)
{
#ifdef WIN32
	struct _stat32i64 buffer;
	_stat32i64(szFilename, &buffer);
#else
	struct stat buffer;
	stat(szFilename, &buffer);
#endif
	return buffer.st_size;
}

long long Util::FreeDiskSize(const char* szPath)
{
#ifdef WIN32
	ULARGE_INTEGER lFree, lDummy;
	if (GetDiskFreeSpaceEx(szPath, &lFree, &lDummy, &lDummy))
	{
		return lFree.QuadPart;
	}
#else
	struct statvfs diskdata;
	if (!statvfs(szPath, &diskdata)) 
	{
		return (long long)diskdata.f_frsize * (long long)diskdata.f_bavail;
	}
#endif
	return -1;
}

bool Util::RenameBak(const char* szFilename, const char* szBakPart, bool bRemoveOldExtension, char* szNewNameBuf, int iNewNameBufSize)
{
	char szChangedFilename[1024];

	if (bRemoveOldExtension)
	{
		strncpy(szChangedFilename, szFilename, 1024);
		szChangedFilename[1024-1] = '\0';
		char* szExtension = strrchr(szChangedFilename, '.');
		if (szExtension)
		{
			*szExtension = '\0';
		}
	}

	char bakname[1024];
	snprintf(bakname, 1024, "%s.%s", bRemoveOldExtension ? szChangedFilename : szFilename, szBakPart);
	bakname[1024-1] = '\0';

	int i = 2;
	struct stat buffer;
	while (!stat(bakname, &buffer))
	{
		snprintf(bakname, 1024, "%s.%i.%s", bRemoveOldExtension ? szChangedFilename : szFilename, i++, szBakPart);
		bakname[1024-1] = '\0';
	}

	if (szNewNameBuf)
	{
		strncpy(szNewNameBuf, bakname, iNewNameBufSize);
	}

	bool bOK = !rename(szFilename, bakname);
	return bOK;
}

#ifndef WIN32
bool Util::ExpandHomePath(const char* szFilename, char* szBuffer, int iBufSize)
{
	if (szFilename && (szFilename[0] == '~') && (szFilename[1] == '/'))
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
			snprintf(szBuffer, iBufSize, "%s%s", home, szFilename + 2);
		}
		else
		{
			snprintf(szBuffer, iBufSize, "%s/%s", home, szFilename + 2);
		}
		szBuffer[iBufSize - 1] = '\0';
	}
	else
	{
		strncpy(szBuffer, szFilename ? szFilename : "", iBufSize);
		szBuffer[iBufSize - 1] = '\0';
	}
	
	return true;
}
#endif

void Util::ExpandFileName(const char* szFilename, char* szBuffer, int iBufSize)
{
#ifdef WIN32
	_fullpath(szBuffer, szFilename, iBufSize);
#else
	if (szFilename[0] != '\0' && szFilename[0] != '/')
	{
		char szCurDir[MAX_PATH + 1];
		getcwd(szCurDir, sizeof(szCurDir) - 1); // 1 char reserved for adding backslash
		int iOffset = 0;
		if (szFilename[0] == '.' && szFilename[1] == '/')
		{
			iOffset += 2;
		}
		snprintf(szBuffer, iBufSize, "%s/%s", szCurDir, szFilename + iOffset);
	}
	else
	{
		strncpy(szBuffer, szFilename, iBufSize);
		szBuffer[iBufSize - 1] = '\0';
	}
#endif
}

void Util::FormatFileSize(char * szBuffer, int iBufLen, long long lFileSize)
{
	if (lFileSize > 1024 * 1024 * 1000)
	{
		snprintf(szBuffer, iBufLen, "%.2f GB", (float)(Util::Int64ToFloat(lFileSize) / 1024 / 1024 / 1024));
	}
	else if (lFileSize > 1024 * 1000)
	{
		snprintf(szBuffer, iBufLen, "%.2f MB", (float)(Util::Int64ToFloat(lFileSize) / 1024 / 1024));
	}
	else if (lFileSize > 1000)
	{
		snprintf(szBuffer, iBufLen, "%.2f KB", (float)(Util::Int64ToFloat(lFileSize) / 1024));
	}
	else 
	{
		snprintf(szBuffer, iBufLen, "%i B", (int)lFileSize);
	}
	szBuffer[iBufLen - 1] = '\0';
}

bool Util::SameFilename(const char* szFilename1, const char* szFilename2)
{
#ifdef WIN32
	return strcasecmp(szFilename1, szFilename2) == 0;
#else
	return strcmp(szFilename1, szFilename2) == 0;
#endif
}

bool Util::MatchFileExt(const char* szFilename, const char* szExtensionList, const char* szListSeparator)
{
	int iFilenameLen = strlen(szFilename);

	Tokenizer tok(szExtensionList, szListSeparator);
	while (const char* szExt = tok.Next())
	{
		int iExtLen = strlen(szExt);
		if (iFilenameLen >= iExtLen && !strcasecmp(szExt, szFilename + iFilenameLen - iExtLen))
		{
			return true;
		}
	}

	return false;
}

#ifndef WIN32
void Util::FixExecPermission(const char* szFilename)
{
	struct stat buffer;
	bool bOK = !stat(szFilename, &buffer);
	if (bOK)
	{
		buffer.st_mode = buffer.st_mode | S_IXUSR | S_IXGRP | S_IXOTH;
		chmod(szFilename, buffer.st_mode);
	}
}
#endif

char* Util::GetLastErrorMessage(char* szBuffer, int iBufLen)
{
	szBuffer[0] = '\0';
	strerror_r(errno, szBuffer, iBufLen);
	szBuffer[iBufLen-1] = '\0';
	return szBuffer;
}

void Util::FormatSpeed(int iBytesPerSecond, char* szBuffer, int iBufSize)
{
	if (iBytesPerSecond >= 100 * 1024 * 1024)
	{
		snprintf(szBuffer, iBufSize, "%i MB/s", iBytesPerSecond / 1024 / 1024);
	}
	else if (iBytesPerSecond >= 10 * 1024 * 1024)
	{
		snprintf(szBuffer, iBufSize, "%0.1f MB/s", (float)iBytesPerSecond / 1024.0 / 1024.0);
	}
	else if (iBytesPerSecond >= 1024 * 1000)
	{
		snprintf(szBuffer, iBufSize, "%0.2f MB/s", (float)iBytesPerSecond / 1024.0 / 1024.0);
	}
	else
	{
		snprintf(szBuffer, iBufSize, "%i KB/s", iBytesPerSecond / 1024);
	}

	szBuffer[iBufSize - 1] = '\0';
}

void Util::InitVersionRevision()
{
#ifndef WIN32
	if ((strlen(svn_version()) > 0) && strstr(VERSION, "testing"))
	{
		snprintf(VersionRevisionBuf, sizeof(VersionRevisionBuf), "%s-r%s", VERSION, svn_version());
	}
	else
#endif
	{
		snprintf(VersionRevisionBuf, sizeof(VersionRevisionBuf), "%s", VERSION);
	}
}

bool Util::SplitCommandLine(const char* szCommandLine, char*** argv)
{
	int iArgCount = 0;
	char szBuf[1024];
	char* pszArgList[100];
	unsigned int iLen = 0;
	bool bEscaping = false;
	bool bSpace = true;
	for (const char* p = szCommandLine; ; p++)
	{
		if (*p)
		{
			const char c = *p;
			if (bEscaping)
			{
				if (c == '\'')
				{
					if (p[1] == '\'' && iLen < sizeof(szBuf) - 1)
					{
						szBuf[iLen++] = c;
						p++;
					}
					else
					{
						bEscaping = false;
						bSpace = true;
					}
				}
				else if (iLen < sizeof(szBuf) - 1)
				{
					szBuf[iLen++] = c;
				}
			}
			else
			{
				if (c == ' ')
				{
					bSpace = true;
				}
				else if (c == '\'' && bSpace)
				{
					bEscaping = true;
					bSpace = false;
				}
				else if (iLen < sizeof(szBuf) - 1)
				{
					szBuf[iLen++] = c;
					bSpace = false;
				}
			}
		}

		if ((bSpace || !*p) && iLen > 0 && iArgCount < 100)
		{
			//add token
			szBuf[iLen] = '\0';
			if (argv)
			{
				pszArgList[iArgCount] = strdup(szBuf);
			}
			(iArgCount)++;
			iLen = 0;
		}

		if (!*p)
		{
			break;
		}
	}

	if (argv)
	{
		pszArgList[iArgCount] = NULL;
		*argv = (char**)malloc((iArgCount + 1) * sizeof(char*));
		memcpy(*argv, pszArgList, sizeof(char*) * (iArgCount + 1));
	}

	return iArgCount > 0;
}

void Util::TrimRight(char* szStr)
{
	char* szEnd = szStr + strlen(szStr) - 1;
	while (szEnd >= szStr && (*szEnd == '\n' || *szEnd == '\r' || *szEnd == ' ' || *szEnd == '\t'))
	{
		*szEnd = '\0';
		szEnd--;
	}
}

char* Util::Trim(char* szStr)
{
	TrimRight(szStr);
	while (*szStr == '\n' || *szStr == '\r' || *szStr == ' ' || *szStr == '\t')
	{
		szStr++;
	}
	return szStr;
}

char* Util::ReduceStr(char* szStr, const char* szFrom, const char* szTo)
{
	int iLenFrom = strlen(szFrom);
	int iLenTo = strlen(szTo);
	// assert(iLenTo < iLenFrom);

	while (char* p = strstr(szStr, szFrom))
	{
		const char* src = szTo;
		while ((*p++ = *src++)) ;

		src = --p - iLenTo + iLenFrom;
		while ((*p++ = *src++)) ;
	}

	return szStr;
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

unsigned int Util::HashBJ96(const char* szBuffer, int iBufSize, unsigned int iInitValue)
{
	return (unsigned int)hash((ub1*)szBuffer, (ub4)iBufSize, (ub4)iInitValue);
}

#ifdef WIN32
bool Util::RegReadStr(HKEY hKey, const char* szKeyName, const char* szValueName, char* szBuffer, int* iBufLen)
{
	HKEY hSubKey;
	if (!RegOpenKeyEx(hKey, szKeyName, 0, KEY_READ, &hSubKey))
	{
		DWORD iRetBytes = *iBufLen;
		LONG iRet = RegQueryValueEx(hSubKey, szValueName, NULL, NULL, (LPBYTE)szBuffer, &iRetBytes);
		*iBufLen = iRetBytes;
		RegCloseKey(hSubKey);
		return iRet == 0;
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
  static const int days_from_0_to_1970 = days_from_0(1970);
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

// prevent PC from going to sleep
void Util::SetStandByMode(bool bStandBy)
{
#ifdef WIN32
	SetThreadExecutionState((bStandBy ? 0 : ES_SYSTEM_REQUIRED) | ES_CONTINUOUS);
#endif
}

time_t Util::Timegm(tm const *t)
{
	return internal_timegm(t);
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


unsigned int WebUtil::DecodeBase64(char* szInputBuffer, int iInputBufferLength, char* szOutputBuffer)
{
	unsigned int InputBufferIndex  = 0;
	unsigned int OutputBufferIndex = 0;
	unsigned int InputBufferLength = iInputBufferLength > 0 ? iInputBufferLength : strlen(szInputBuffer);

	char ByteQuartet [4];
	int i = 0;
	while (InputBufferIndex < InputBufferLength)
	{
		// Ignore all characters except the ones in BASE64_ALPHABET
		if ((szInputBuffer [InputBufferIndex] >= 48 && szInputBuffer [InputBufferIndex] <=  57) ||
			(szInputBuffer [InputBufferIndex] >= 65 && szInputBuffer [InputBufferIndex] <=  90) ||
			(szInputBuffer [InputBufferIndex] >= 97 && szInputBuffer [InputBufferIndex] <= 122) ||
			szInputBuffer [InputBufferIndex] == '+' || 
			szInputBuffer [InputBufferIndex] == '/' || 
			szInputBuffer [InputBufferIndex] == '=')
		{
			ByteQuartet [i] = szInputBuffer [InputBufferIndex];
			i++;
		}
		
		InputBufferIndex++;
		
		if (i == 4) {
			OutputBufferIndex += DecodeByteQuartet(ByteQuartet, szOutputBuffer + OutputBufferIndex);
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
	int iReqSize = strlen(raw);
	for (const char* p = raw; *p; p++)
	{
		unsigned char ch = *p;
		switch (ch)
		{
			case '>':
			case '<':
				iReqSize += 4;
				break;
			case '&':
				iReqSize += 5;
				break;
			case '\'':
			case '\"':
				iReqSize += 6;
				break;
			default:
				if (ch < 0x20 || ch >= 0x80)
				{
					iReqSize += 10;
					break;
				}
		}
	}

	char* result = (char*)malloc(iReqSize + 1);

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
						int code = atoi(p+1);
						p = strchr(p+1, ';');
						if (p) p++;
						*output++ = (char)code;
					}
					else
					{
						// unknown entity
						*output++ = *(p-1);
						p++;
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

const char* WebUtil::XmlFindTag(const char* szXml, const char* szTag, int* pValueLength)
{
	char szOpenTag[100];
	snprintf(szOpenTag, 100, "<%s>", szTag);
	szOpenTag[100-1] = '\0';

	char szCloseTag[100];
	snprintf(szCloseTag, 100, "</%s>", szTag);
	szCloseTag[100-1] = '\0';

	char szOpenCloseTag[100];
	snprintf(szOpenCloseTag, 100, "<%s/>", szTag);
	szOpenCloseTag[100-1] = '\0';

	const char* pstart = strstr(szXml, szOpenTag);
	const char* pstartend = strstr(szXml, szOpenCloseTag);
	if (!pstart && !pstartend) return NULL;

	if (pstartend && (!pstart || pstartend < pstart))
	{
		*pValueLength = 0;
		return pstartend;
	}

	const char* pend = strstr(pstart, szCloseTag);
	if (!pend) return NULL;

	int iTagLen = strlen(szOpenTag);
	*pValueLength = (int)(pend - pstart - iTagLen);

	return pstart + iTagLen;
}

bool WebUtil::XmlParseTagValue(const char* szXml, const char* szTag, char* szValueBuf, int iValueBufSize, const char** pTagEnd)
{
	int iValueLen = 0;
	const char* szValue = XmlFindTag(szXml, szTag, &iValueLen);
	if (!szValue)
	{
		return false;
	}
	int iLen = iValueLen < iValueBufSize ? iValueLen : iValueBufSize - 1;
	strncpy(szValueBuf, szValue, iLen);
	szValueBuf[iLen] = '\0';
	if (pTagEnd)
	{
		*pTagEnd = szValue + iValueLen;
	}
	return true;
}

char* WebUtil::JsonEncode(const char* raw)
{
	// calculate the required outputstring-size based on number of escape-entities and their sizes
	int iReqSize = strlen(raw);
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
				iReqSize++;
                break;
			default:
				if (ch < 0x20 || ch >= 0x80)
				{
					iReqSize += 6;
					break;
				}
		}
	}

	char* result = (char*)malloc(iReqSize + 1);

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

const char* WebUtil::JsonFindField(const char* szJsonText, const char* szFieldName, int* pValueLength)
{
	char szOpenTag[100];
	snprintf(szOpenTag, 100, "\"%s\"", szFieldName);
	szOpenTag[100-1] = '\0';

	const char* pstart = strstr(szJsonText, szOpenTag);
	if (!pstart) return NULL;

	pstart += strlen(szOpenTag);

	return JsonNextValue(pstart, pValueLength);
}

const char* WebUtil::JsonNextValue(const char* szJsonText, int* pValueLength)
{
	const char* pstart = szJsonText;

	while (*pstart && strchr(" ,[{:\r\n\t\f", *pstart)) pstart++;
	if (!*pstart) return NULL;

	const char* pend = pstart;

	char ch = *pend;
	bool bStr = ch == '"';
	if (bStr)
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
		if (bStr && ch == '"')
		{
			pend++;
			break;
		}
		else if (!bStr && strchr(" ,]}\r\n\t\f", ch))
		{
			break;
		}
		ch = *++pend;
	}

	*pValueLength = (int)(pend - pstart);
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

#ifdef WIN32
bool WebUtil::Utf8ToAnsi(char* szBuffer, int iBufLen)
{
	WCHAR* wstr = (WCHAR*)malloc(iBufLen * 2);
	int errcode = MultiByteToWideChar(CP_UTF8, 0, szBuffer, -1, wstr, iBufLen);
	if (errcode > 0)
	{
		errcode = WideCharToMultiByte(CP_ACP, 0, wstr, -1, szBuffer, iBufLen, "_", NULL);
	}
	free(wstr);
	return errcode > 0;
}

bool WebUtil::AnsiToUtf8(char* szBuffer, int iBufLen)
{
	WCHAR* wstr = (WCHAR*)malloc(iBufLen * 2);
	int errcode = MultiByteToWideChar(CP_ACP, 0, szBuffer, -1, wstr, iBufLen);
	if (errcode > 0)
	{
		errcode = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, szBuffer, iBufLen, NULL, NULL);
	}
	free(wstr);
	return errcode > 0;
}
#endif

char* WebUtil::Latin1ToUtf8(const char* szStr)
{
	char *res = (char*)malloc(strlen(szStr) * 2 + 1);
	const unsigned char *in = (const unsigned char*)szStr;
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
time_t WebUtil::ParseRfc822DateTime(const char* szDateTimeStr)
{
	char month[4];
	int day, year, hours, minutes, seconds, zonehours, zoneminutes;
	int r = sscanf(szDateTimeStr, "%*s %d %3s %d %d:%d:%d %3d %2d", &day, &month[0], &year, &hours, &minutes, &seconds, &zonehours, &zoneminutes);
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


URL::URL(const char* szAddress)
{
	m_szAddress = NULL;
	m_szProtocol = NULL;
	m_szUser = NULL;
	m_szPassword = NULL;
	m_szHost = NULL;
	m_szResource = NULL;
	m_iPort = 0;
	m_bTLS = false;
	m_bValid = false;

	if (szAddress)
	{
		m_szAddress = strdup(szAddress);
		ParseURL();
	}
}

URL::~URL()
{
	free(m_szAddress);
	free(m_szProtocol);
	free(m_szUser);
	free(m_szPassword);
	free(m_szHost);
	free(m_szResource);
}

void URL::ParseURL()
{
	// Examples:
	// http://user:password@host:port/path/to/resource?param
	// http://user@host:port/path/to/resource?param
	// http://host:port/path/to/resource?param
	// http://host/path/to/resource?param
	// http://host

	char* protEnd = strstr(m_szAddress, "://");
	if (!protEnd)
	{
		// Bad URL
		return;
	}

	m_szProtocol = (char*)malloc(protEnd - m_szAddress + 1);
	strncpy(m_szProtocol, m_szAddress, protEnd - m_szAddress);
	m_szProtocol[protEnd - m_szAddress] = 0;

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
			int iLen = (int)(amp - pass - 1);
			if (iLen > 0)
			{
				m_szPassword = (char*)malloc(iLen + 1);
				strncpy(m_szPassword, pass + 1, iLen);
				m_szPassword[iLen] = 0;
			}
			userend = pass - 1;
		}

		int iLen = (int)(userend - hostStart + 1);
		if (iLen > 0)
		{
			m_szUser = (char*)malloc(iLen + 1);
			strncpy(m_szUser, hostStart, iLen);
			m_szUser[iLen] = 0;
		}

		hostStart = amp + 1;
	}

	if (slash)
	{
		char* resEnd = m_szAddress + strlen(m_szAddress);
		m_szResource = (char*)malloc(resEnd - slash + 1 + 1);
		strncpy(m_szResource, slash, resEnd - slash + 1);
		m_szResource[resEnd - slash + 1] = 0;

		hostEnd = slash - 1;
	}
	else
	{
		m_szResource = strdup("/");

		hostEnd = m_szAddress + strlen(m_szAddress);
	}

	char* colon = strchr(hostStart, ':');
	if (colon && colon < hostEnd)
	{
		hostEnd = colon - 1;
		m_iPort = atoi(colon + 1);
	}

	m_szHost = (char*)malloc(hostEnd - hostStart + 1 + 1);
	strncpy(m_szHost, hostStart, hostEnd - hostStart + 1);
	m_szHost[hostEnd - hostStart + 1] = 0;

	m_bValid = true;
}

RegEx::RegEx(const char *szPattern, int iMatchBufSize)
{
#ifdef HAVE_REGEX_H
	m_pContext = malloc(sizeof(regex_t));
	m_bValid = regcomp((regex_t*)m_pContext, szPattern, REG_EXTENDED | REG_ICASE | (iMatchBufSize > 0 ? 0 : REG_NOSUB)) == 0;
	m_iMatchBufSize = iMatchBufSize;
	if (iMatchBufSize > 0)
	{
		m_pMatches = malloc(sizeof(regmatch_t) * iMatchBufSize);
	}
	else
	{
		m_pMatches = NULL;
	}
#else
	m_bValid = false;
#endif
}

RegEx::~RegEx()
{
#ifdef HAVE_REGEX_H
	regfree((regex_t*)m_pContext);
	free(m_pContext);
	free(m_pMatches);
#endif
}

bool RegEx::Match(const char *szStr)
{
#ifdef HAVE_REGEX_H
	return m_bValid ? regexec((regex_t*)m_pContext, szStr, m_iMatchBufSize, (regmatch_t*)m_pMatches, 0) == 0 : false;
#else
	return false;
#endif
}

int RegEx::GetMatchCount()
{
#ifdef HAVE_REGEX_H
	int iCount = 0;
	if (m_pMatches)
	{
		regmatch_t* pMatches = (regmatch_t*)m_pMatches;
		while (iCount < m_iMatchBufSize && pMatches[iCount].rm_so > -1)
		{
			iCount++;
		}
	}
	return iCount;
#else
	return 0;
#endif
}

int RegEx::GetMatchStart(int index)
{
#ifdef HAVE_REGEX_H
	regmatch_t* pMatches = (regmatch_t*)m_pMatches;
	return pMatches[index].rm_so;
#else
	return NULL;
#endif
}

int RegEx::GetMatchLen(int index)
{
#ifdef HAVE_REGEX_H
	regmatch_t* pMatches = (regmatch_t*)m_pMatches;
	return pMatches[index].rm_eo - pMatches[index].rm_so;
#else
	return 0;
#endif
}


WildMask::WildMask(const char *szPattern, bool bWantsPositions)
{
	m_szPattern = strdup(szPattern);
	m_bWantsPositions = bWantsPositions;
	m_WildStart = NULL;
	m_WildLen = NULL;
	m_iArrLen = 0;
}

WildMask::~WildMask()
{
	free(m_szPattern);
	free(m_WildStart);
	free(m_WildLen);
}

void WildMask::ExpandArray()
{
	m_iWildCount++;
	if (m_iWildCount > m_iArrLen)
	{
		m_iArrLen += 100;
		m_WildStart = (int*)realloc(m_WildStart, sizeof(*m_WildStart) * m_iArrLen);
		m_WildLen = (int*)realloc(m_WildLen, sizeof(*m_WildLen) * m_iArrLen);
	}
}

// Based on code from http://bytes.com/topic/c/answers/212179-string-matching
// Extended to save positions of matches.
bool WildMask::Match(const char *szStr)
{
	const char* pat = m_szPattern;
	const char* str = szStr;
	const char *spos, *wpos;
	m_iWildCount = 0;
	bool qmark = false;
	bool star = false;

	spos = wpos = str;
	while (*str && *pat != '*')
	{
		if (m_bWantsPositions && (*pat == '?' || *pat == '#'))
		{
			if (!qmark)
			{
				ExpandArray();
				m_WildStart[m_iWildCount-1] = str - szStr;
				m_WildLen[m_iWildCount-1] = 0;
				qmark = true;
			}
		}
		else if (m_bWantsPositions && qmark)
		{
			m_WildLen[m_iWildCount-1] = str - (szStr + m_WildStart[m_iWildCount-1]);
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

	if (m_bWantsPositions && qmark)
	{
		m_WildLen[m_iWildCount-1] = str - (szStr + m_WildStart[m_iWildCount-1]);
		qmark = false;
	}

	while (*str)
	{
		if (*pat == '*')
		{
			if (m_bWantsPositions && qmark)
			{
				m_WildLen[m_iWildCount-1] = str - (szStr + m_WildStart[m_iWildCount-1]);
				qmark = false;
			}
			if (m_bWantsPositions && !star)
			{
				ExpandArray();
				m_WildStart[m_iWildCount-1] = str - szStr;
				m_WildLen[m_iWildCount-1] = 0;
				star = true;
			}

			if (*++pat == '\0')
			{
				if (m_bWantsPositions && star)
				{
					m_WildLen[m_iWildCount-1] = strlen(str);
				}

				return true;
			}
			wpos = pat;
			spos = str + 1;
		}
		else if (*pat == '?' || (*pat == '#' && strchr("0123456789", *str)))
		{
			if (m_bWantsPositions && !qmark)
			{
				ExpandArray();
				m_WildStart[m_iWildCount-1] = str - szStr;
				m_WildLen[m_iWildCount-1] = 0;
				qmark = true;
			}

			pat++;
			str++;
		}
		else if (tolower(*pat) == tolower(*str))
		{
			if (m_bWantsPositions && qmark)
			{
				m_WildLen[m_iWildCount-1] = str - (szStr + m_WildStart[m_iWildCount-1]);
				qmark = false;
			}
			else if (m_bWantsPositions && star)
			{
				m_WildLen[m_iWildCount-1] = str - (szStr + m_WildStart[m_iWildCount-1]);
				star = false;
			}

			pat++;
			str++;
		}
		else
		{
			if (m_bWantsPositions && qmark)
			{
				m_iWildCount--;
				qmark = false;
			}

			pat = wpos;
			str = spos++;
			star = true;
		}
	}

	if (m_bWantsPositions && qmark)
	{
		m_WildLen[m_iWildCount-1] = str - (szStr + m_WildStart[m_iWildCount-1]);
	}

	if (*pat == '*' && m_bWantsPositions && !star)
	{
		ExpandArray();
		m_WildStart[m_iWildCount-1] = str - szStr;
		m_WildLen[m_iWildCount-1] = strlen(str);
	}

	while (*pat == '*')
	{
		pat++;
	}

	return *pat == '\0';
}


#ifndef DISABLE_GZIP
unsigned int ZLib::GZipLen(int iInputBufferLength)
{
	z_stream zstr;
	memset(&zstr, 0, sizeof(zstr));
	return (unsigned int)deflateBound(&zstr, iInputBufferLength);
}

unsigned int ZLib::GZip(const void* szInputBuffer, int iInputBufferLength, void* szOutputBuffer, int iOutputBufferLength)
{
	z_stream zstr;
	zstr.zalloc = Z_NULL;
	zstr.zfree = Z_NULL;
	zstr.opaque = Z_NULL;
	zstr.next_in = (Bytef*)szInputBuffer;
	zstr.avail_in = iInputBufferLength;
	zstr.next_out = (Bytef*)szOutputBuffer;
	zstr.avail_out = iOutputBufferLength;
	
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
	m_iBufferSize = BufferSize;
	m_pZStream = malloc(sizeof(z_stream));
	m_pOutputBuffer = malloc(BufferSize);

	memset(m_pZStream, 0, sizeof(z_stream));

	/* add 16 to MAX_WBITS to enforce gzip format */
	int ret = inflateInit2(((z_stream*)m_pZStream), MAX_WBITS + 16);
	if (ret != Z_OK)
	{
		free(m_pZStream);
		m_pZStream = NULL;
	}
}

GUnzipStream::~GUnzipStream()
{
	if (m_pZStream)
	{
		inflateEnd(((z_stream*)m_pZStream));
		free(m_pZStream);
	}
	free(m_pOutputBuffer);
}

void GUnzipStream::Write(const void *pInputBuffer, int iInputBufferLength)
{
	((z_stream*)m_pZStream)->next_in = (Bytef*)pInputBuffer;
	((z_stream*)m_pZStream)->avail_in = iInputBufferLength;
}

GUnzipStream::EStatus GUnzipStream::Read(const void **pOutputBuffer, int *iOutputBufferLength)
{
	((z_stream*)m_pZStream)->next_out = (Bytef*)m_pOutputBuffer;
	((z_stream*)m_pZStream)->avail_out = m_iBufferSize;

	*iOutputBufferLength = 0;

	if (!m_pZStream)
	{
		return zlError;
	}

	int ret = inflate(((z_stream*)m_pZStream), Z_NO_FLUSH);

	switch (ret)
	{
		case Z_STREAM_END:
		case Z_OK:
			*iOutputBufferLength = m_iBufferSize - ((z_stream*)m_pZStream)->avail_out;
			*pOutputBuffer = m_pOutputBuffer;
			return ret == Z_STREAM_END ? zlFinished : zlOK;

		case Z_BUF_ERROR:
			return zlOK;
	}

	return zlError;
}

#endif

Tokenizer::Tokenizer(const char* szDataString, const char* szSeparators)
{
	// an optimization to avoid memory allocation for short data string (shorten than 1024 chars)
	int iLen = strlen(szDataString);
	if (iLen < sizeof(m_szDefaultBuf) - 1)
	{
		strncpy(m_szDefaultBuf, szDataString, sizeof(m_szDefaultBuf));
		m_szDefaultBuf[1024- 1] = '\0';
		m_szDataString = m_szDefaultBuf;
		m_bInplaceBuf = true;
	}
	else
	{
		m_szDataString = strdup(szDataString);
		m_bInplaceBuf = false;
	}

	m_szSeparators = szSeparators;
	m_szSavePtr = NULL;
	m_bWorking = false;
}

Tokenizer::Tokenizer(char* szDataString, const char* szSeparators, bool bInplaceBuf)
{
	m_szDataString = bInplaceBuf ? szDataString : strdup(szDataString);
	m_szSeparators = szSeparators;
	m_szSavePtr = NULL;
	m_bWorking = false;
	m_bInplaceBuf = bInplaceBuf;
}

Tokenizer::~Tokenizer()
{
	if (!m_bInplaceBuf)
	{
		free(m_szDataString);
	}
}

char* Tokenizer::Next()
{
	char* szToken = NULL;
	while (!szToken || !*szToken)
	{
		szToken = strtok_r(m_bWorking ? NULL : m_szDataString, m_szSeparators, &m_szSavePtr);
		m_bWorking = true;
		if (!szToken)
		{
			return NULL;
		}
		szToken = Util::Trim(szToken);
	}
	return szToken;
}
