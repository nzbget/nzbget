/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#ifdef WIN32
#include <direct.h>
#include <WinIoCtl.h>
#else
#include <unistd.h>
#include <sys/statvfs.h>
#include <pwd.h>
#endif
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif
#ifndef DISABLE_GZIP
#include <zlib.h>
#endif

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
	m_hFile = _findfirst(szMask, &m_FindData);
	m_bFirst = true;
}

DirBrowser::~DirBrowser()
{
	if (m_hFile != -1L)
	{
		_findclose(m_hFile);
	}
}

const char* DirBrowser::Next()
{
	bool bOK = false;
	if (m_bFirst)
	{
		bOK = m_hFile != -1L;
		m_bFirst = false;
	}
	else
	{
		bOK = _findnext(m_hFile, &m_FindData) == 0;
	}
	if (bOK)
	{
		return m_FindData.name;
	}
	return NULL;
}

#else

DirBrowser::DirBrowser(const char* szPath)
{
	m_pDir = opendir(szPath);
}

DirBrowser::~DirBrowser()
{
	if (m_pDir)
	{
		closedir(m_pDir);
	}
}

const char* DirBrowser::Next()
{
	if (m_pDir)
	{
		m_pFindData = readdir(m_pDir);
		if (m_pFindData)
		{
			return m_pFindData->d_name;
		}
	}
	return NULL;
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
	if (m_szBuffer)
	{
		free(m_szBuffer);
	}
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
		snprintf(szErrBuf, iBufSize, "could not read information for directory %s: %i, %s", szNormPath, errno, strerror(errno));
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
			snprintf(szErrBuf, iBufSize, "could not create directory %s: %s", szNormPath, strerror(errno));
			szErrBuf[iBufSize-1] = 0;
			return false;
		}
			
		if (stat(szNormPath, &buffer) != 0)
		{
			snprintf(szErrBuf, iBufSize, "could not read information for directory %s: %s", szNormPath, strerror(errno));
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
    FILE* pFile = fopen(szFileName, "rb");
    if (!pFile)
    {
        return false;
    }

    // obtain file size.
    fseek(pFile , 0 , SEEK_END);
    int iSize  = ftell(pFile);
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

bool Util::CreateSparseFile(const char* szFilename, int iSize)
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

		SetFilePointer(hFile, iSize, NULL, FILE_END);
		SetEndOfFile(hFile);
		CloseHandle(hFile);
		bOK = true;
	}
#else
	// create file
	FILE* pFile = fopen(szFilename, "ab");
	if (pFile)
	{
		fclose(pFile);
	}
	// there are no reliable function to expand file on POSIX, so we must try different approaches,
	// starting with the fastest one and hoping it will work
	// 1) set file size using function "truncate" (it is fast, if works)
	truncate(szFilename, iSize);
	// check if it worked
	pFile = fopen(szFilename, "ab");
	if (pFile)
	{
		fseek(pFile, 0, SEEK_END);
		bOK = ftell(pFile) == iSize;
		if (!bOK)
		{
			// 2) truncate did not work, expanding the file by writing in it (it is slow)
			fclose(pFile);
			truncate(szFilename, 0);
			pFile = fopen(szFilename, "ab");
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
	FILE *file = fopen(szFilename, "r+b");
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
	const char* szReplaceChars = bAllowSlashes ? ":*?\"><'\n\r\t" : "\\/:*?\"><'\n\r\t";
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
	if (!bOK && (errno == EXDEV))
	{
		FILE* infile = fopen(szSrcFilename, "rb");
		if (!infile)
		{
			return false;
		}

		FILE* outfile = fopen(szDstFilename, "wb+");
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
	struct stat buffer;
	bool bExists = !stat(szFilename, &buffer) && S_ISREG(buffer.st_mode);
	return bExists;
}

bool Util::DirectoryExists(const char* szDirFilename)
{
	struct stat buffer;
	bool bExists = !stat(szDirFilename, &buffer) && S_ISDIR(buffer.st_mode);
	return bExists;
}

bool Util::CreateDirectory(const char* szDirFilename)
{
	mkdir(szDirFilename, S_DIRMODE);
	return DirectoryExists(szDirFilename);
}

bool Util::RemoveDirectory(const char* szDirFilename)
{
#ifdef WIN32
	return ::RemoveDirectory(szDirFilename);
#else
	return remove(szDirFilename) == 0;
#endif
}

bool Util::DeleteDirectoryWithContent(const char* szDirFilename)
{
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
				bOK &= DeleteDirectoryWithContent(szFullFilename);
			}
			else
			{
				bOK &= remove(szFullFilename) == 0;
			}
		}
	}

	return bOK && RemoveDirectory(szDirFilename);
}

long long Util::FileSize(const char* szFilename)
{
#ifdef WIN32
	struct _stat32i64 buffer;
	_stat32i64(szFilename, &buffer);
#else
#ifdef HAVE_STAT64
	struct stat64 buffer;
	stat64(szFilename, &buffer);
#else
	struct stat buffer;
	stat(szFilename, &buffer);
#endif
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
		strncpy(szBuffer, szFilename, iBufSize);
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
	int iLen = strlen(szStr);
	char ch = szStr[iLen-1];
	while (iLen > 0 && (ch == '\n' || ch == '\r' || ch == ' ' || ch == '\t'))
	{
		szStr[iLen-1] = 0;
		iLen--;
		ch = szStr[iLen-1];
	}
}

char* Util::Trim(char* szStr)
{
	TrimRight(szStr);
	char ch = *szStr;
	while (ch == '\n' || ch == '\r' || ch == ' ' || ch == '\t')
	{
		szStr++;
		ch = *szStr;
	}
	return szStr;
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
					iReqSize += 8;
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

					sprintf(output, "\\u%06x", cp);
					output += 8;
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
	if (m_szAddress)
	{
		free(m_szAddress);
	}
	if (m_szProtocol)
	{
		free(m_szProtocol);
	}
	if (m_szUser)
	{
		free(m_szUser);
	}
	if (m_szPassword)
	{
		free(m_szPassword);
	}
	if (m_szHost)
	{
		free(m_szHost);
	}
	if (m_szResource)
	{
		free(m_szResource);
	}
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

RegEx::RegEx(const char *szPattern)
{
#ifdef HAVE_REGEX_H
	m_pContext = malloc(sizeof(regex_t));
	m_bValid = regcomp((regex_t*)m_pContext, szPattern, REG_EXTENDED | REG_ICASE | REG_NOSUB) == 0;
#else
	m_bValid = false;
#endif
}

RegEx::~RegEx()
{
#ifdef HAVE_REGEX_H
	regfree((regex_t*)m_pContext);
	free(m_pContext);
#endif
}

bool RegEx::Match(const char *szStr)
{
#ifdef HAVE_REGEX_H
	return m_bValid ? regexec((regex_t*)m_pContext, szStr, 0, NULL, 0) == 0 : false;
#else
	return false;
#endif
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
