/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2009 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef UTIL_H
#define UTIL_H

#ifdef WIN32
#include <stdio.h>
#include <io.h>
#else
#include <dirent.h>
#endif

#ifdef WIN32
extern int optind, opterr;
extern char *optarg;
int getopt(int argc, char *argv[], char *optstring);
#endif

class DirBrowser
{
private:
#ifdef WIN32
	struct _finddata_t	m_FindData;
	intptr_t			m_hFile;
	bool				m_bFirst;
#else
	DIR*				m_pDir;
	struct dirent*		m_pFindData;
#endif

public:
						DirBrowser(const char* szPath);
						~DirBrowser();
	const char*			Next();
};

class Util 
{
public:

	static char* BaseFileName(const char* filename);
	static void NormalizePathSeparators(char* szPath);
	static bool LoadFileIntoBuffer(const char* szFileName, char** pBuffer, int* pBufferLength);
	static bool CreateSparseFile(const char* szFilename, int iSize);
	static bool TruncateFile(const char* szFilename, int iSize);
	static void MakeValidFilename(char* szFilename, char cReplaceChar, bool bAllowSlashes);
	static bool MoveFile(const char* szSrcFilename, const char* szDstFilename);
	static bool FileExists(const char* szFilename);
	static bool DirectoryExists(const char* szDirFilename);
	static bool CreateDirectory(const char* szDirFilename);
	static bool ForceDirectories(const char* szPath);
	static bool CurrentDirectory(char* szBuffer, int iBufSize);
	static long long FileSize(const char* szFilename);
	static long long FreeDiskSize(const char* szPath);
	static bool DirEmpty(const char* szDirFilename);
	static bool RenameBak(const char* szFilename, const char* szBakPart, bool bRemoveOldExtension, char* szNewNameBuf, int iNewNameBufSize);
#ifndef WIN32
	static bool ExpandHomePath(const char* szFilename, char* szBuffer, int iBufSize);
#endif
	static void ExpandFileName(const char* szFilename, char* szBuffer, int iBufSize);
	static void FormatFileSize(char* szBuffer, int iBufLen, long long lFileSize);

	/*
	 * Split command line int arguments.
	 * Uses spaces and single quotation marks as separators.
	 * Returns bool if sucessful or false if bad escaping was detected.
	 * Parameter "argv" may be NULL if only a syntax check is needed.
	 * Parsed parameters returned in Array "argv", which contains at least one element.
	 * The last element in array is NULL.
	 * Restrictions: the number of arguments is limited to 100 and each arguments must
	 * be maximum 1024 chars long.
	 * If these restrictions are exceeded, only first 100 arguments and only first 1024
	 * for each argument are returned (the functions still returns "true").
	 */
	static bool SplitCommandLine(const char* szCommandLine, char*** argv);

	static long long JoinInt64(unsigned long Hi, unsigned long Lo);
	static void SplitInt64(long long Int64, unsigned long* Hi, unsigned long* Lo);

	/**
	 * Int64ToFloat converts Int64 to float.
	 * Simple (float)Int64 does not work on all compilers,
	 * for example on ARM for NSLU2 (unslung).
	 */
	static float Int64ToFloat(long long Int64);

	static void TrimRight(char* szStr);

	/*
	 * Returns program version and revision number as string formatted like "0.7.0-r295".
	 * If revision number is not available only version is returned ("0.7.0").
	 */
	static const char* VersionRevision() { return VersionRevisionBuf; };
	
	/*
	 * Initialize buffer for program version and revision number.
	 * This function must be called during program initialization before any
	 * call to "VersionRevision()".
	 */
	static void InitVersionRevision();
	
	static char VersionRevisionBuf[40];
};

class WebUtil
{
public:
	static unsigned int DecodeBase64(char* szInputBuffer, int iInputBufferLength, char* szOutputBuffer);

	/*
	 * Encodes string to be used as content of xml-tag.
	 * Returns new string allocated with malloc, it need to be freed by caller.
	 */
	static char* XmlEncode(const char* raw);

	/*
	 * Decodes string from xml.
	 * The string is decoded on the place overwriting the content of raw-data.
	 */
	static void XmlDecode(char* raw);

	/*
	 * Returns pointer to tag-content and length of content in iValueLength
	 * The returned pointer points to the part of source-string, no additional strings are allocated.
	 */
	static const char* XmlFindTag(const char* szXml, const char* szTag, int* pValueLength);

	/*
	 * Parses tag-content into szValueBuf.
	 */
	static bool XmlParseTagValue(const char* szXml, const char* szTag, char* szValueBuf, int iValueBufSize, const char** pTagEnd);

	/*
	 * Creates JSON-string by replace the certain characters with escape-sequences.
	 * Returns new string allocated with malloc, it need to be freed by caller.
	 */
	static char* JsonEncode(const char* raw);

	/*
	 * Decodes JSON-string.
	 * The string is decoded on the place overwriting the content of raw-data.
	 */
	static void JsonDecode(char* raw);

	/*
	 * Returns pointer to field-content and length of content in iValueLength
	 * The returned pointer points to the part of source-string, no additional strings are allocated.
	 */
	static const char* JsonFindField(const char* szJsonText, const char* szFieldName, int* pValueLength);

	/*
	 * Returns pointer to field-content and length of content in iValueLength
	 * The returned pointer points to the part of source-string, no additional strings are allocated.
	 */
	static const char* JsonNextValue(const char* szJsonText, int* pValueLength);

	/*
	 * Unquote http quoted string.
	 * The string is decoded on the place overwriting the content of raw-data.
	 */
	static void HttpUnquote(char* raw);
};

class URL
{
private:
	char*				m_szAddress;
	char*				m_szProtocol;
	char*				m_szUser;
	char*				m_szPassword;
	char*				m_szHost;
	char*				m_szResource;
	int					m_iPort;
	bool				m_bTLS;
	bool				m_bValid;
	void				ParseURL();

public:
	 					URL(const char* szAddress);
						~URL();
	bool				IsValid() { return m_bValid; }
	const char*			GetAddress() { return m_szAddress; }
	const char*			GetProtocol() { return m_szProtocol; }
	const char*			GetUser() { return m_szUser; }
	const char*			GetPassword() { return m_szPassword; }
	const char*			GetHost() { return m_szHost; }
	const char*			GetResource() { return m_szResource; }
	int					GetPort() { return m_iPort; }
	bool				GetTLS() { return m_bTLS; }
};

class RegEx
{
private:
	void*				m_pContext;
	bool				m_bValid;

public:
						RegEx(const char *szPattern);
						~RegEx();
	bool				IsValid() { return m_bValid; }
	bool				Match(const char *szStr);
};

#ifndef DISABLE_GZIP
class ZLib
{
public:
	/*
	 * calculates the size required for output buffer
	 */
	static unsigned int GZipLen(int iInputBufferLength);
	
	/*
	 * returns the size of bytes written to szOutputBuffer or 0 if the buffer is too small or an error occured.
	 */
	static unsigned int GZip(const void* szInputBuffer, int iInputBufferLength, void* szOutputBuffer, int iOutputBufferLength);
};

class GUnzipStream
{
public:
	enum EStatus
	{
		zlError,
		zlFinished,
		zlOK
	};

private:
	void*				m_pZStream;
	void*				m_pOutputBuffer;
	int					m_iBufferSize;

public:
						GUnzipStream(int BufferSize);
						~GUnzipStream();

	/*
	 * set next memory block for uncompression
	 */
	void				Write(const void *pInputBuffer, int iInputBufferLength);

	/*
	 * get next uncompressed memory block.
	 * iOutputBufferLength - the size of uncompressed block. if it is "0" the next compressed block must be provided via "Write".
	 */
	EStatus				Read(const void **pOutputBuffer, int *iOutputBufferLength);
};
#endif

#endif
