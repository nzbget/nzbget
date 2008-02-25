/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007  Andrei Prygounkov <hugbug@users.sourceforge.net>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include <sys/timeb.h>
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
	static bool SetFileSize(const char* szFilename, int iSize);
	static void MakeValidFilename(char* szFilename, char cReplaceChar);
	static bool MoveFile(const char* szSrcFilename, const char* szDstFilename);
	static bool FileExists(const char* szFilename);
	static bool DirectoryExists(const char* szDirFilename);
	static bool CreateDirectory(const char* szDirFilename);
	static bool ForceDirectories(const char* szPath);
	static long long FileSize(const char* szFilename);

	static long long JoinInt64(unsigned int Hi, unsigned int Lo);
	static void SplitInt64(long long Int64, unsigned int* Hi, unsigned int* Lo);

	static float EqualTime(_timeval* t1, _timeval* t2);
	static bool EmptyTime(_timeval* t);
	static float DiffTime(_timeval* t1, _timeval* t2);

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
	 * Replace certain characters with escape-sequences.
	 * Returns new string allocated with malloc, it need to be freed by caller.
	 */
	static char* JsonEncode(const char* raw);

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
};

#endif
