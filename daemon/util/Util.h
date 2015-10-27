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


#ifndef UTIL_H
#define UTIL_H

#ifdef DIRBROWSER_SNAPSHOT
#include <deque>
#endif
#include <time.h>
#include <stdarg.h>

#ifdef WIN32
extern int optind, opterr;
extern char *optarg;
int getopt(int argc, char *argv[], char *optstring);
#endif

class DirBrowser
{
private:
#ifdef WIN32
	WIN32_FIND_DATA		m_findData;
	HANDLE				m_hFile;
	bool				m_first;
#else
	void*				m_dir;    // DIR*, declared as void* to avoid including of <dirent>
	struct dirent*		m_findData;
#endif

#ifdef DIRBROWSER_SNAPSHOT
	bool				m_snapshot;
	typedef std::deque<char*>	FileList;
	FileList			m_snapshot;
	FileList::iterator	m_itSnapshot;
#endif

public:
#ifdef DIRBROWSER_SNAPSHOT
						DirBrowser(const char* path, bool snapshot = true);
#else
						DirBrowser(const char* path);
#endif
						~DirBrowser();
	const char*			Next();
};

class StringBuilder
{
private:
	char*				m_buffer;
	int					m_bufferSize;
	int					m_usedSize;
	int					m_growSize;

	void				Reserve(int size);

public:
						StringBuilder();
						~StringBuilder();
	void				Append(const char* str);
	void				AppendFmt(const char* format, ...);
	void				AppendFmtV(const char* format, va_list ap);
	const char*			GetBuffer() { return m_buffer; }
	void				SetGrowSize(int growSize) { m_growSize = growSize; }
	int					GetUsedSize() { return m_usedSize; }
	void				Clear();
};

class Util
{
public:
	static char* BaseFileName(const char* filename);
	static void NormalizePathSeparators(char* path);
	static bool LoadFileIntoBuffer(const char* fileName, char** buffer, int* bufferLength);
	static bool SaveBufferIntoFile(const char* fileName, const char* buffer, int bufLen);
	static bool CreateSparseFile(const char* filename, long long size, char* errBuf, int bufSize);
	static bool TruncateFile(const char* filename, int size);
	static void MakeValidFilename(char* filename, char cReplaceChar, bool allowSlashes);
	static bool MakeUniqueFilename(char* destBufFilename, int destBufSize, const char* destDir, const char* basename);
	static bool MoveFile(const char* srcFilename, const char* dstFilename);
	static bool CopyFile(const char* srcFilename, const char* dstFilename);
	static bool FileExists(const char* filename);
	static bool FileExists(const char* path, const char* filenameWithoutPath);
	static bool DirectoryExists(const char* dirFilename);
	static bool CreateDirectory(const char* dirFilename);
	static bool RemoveDirectory(const char* dirFilename);
	static bool DeleteDirectoryWithContent(const char* dirFilename, char* errBuf, int bufSize);
	static bool ForceDirectories(const char* path, char* errBuf, int bufSize);
	static bool GetCurrentDirectory(char* buffer, int bufSize);
	static bool SetCurrentDirectory(const char* dirFilename);
	static long long FileSize(const char* filename);
	static long long FreeDiskSize(const char* path);
	static bool DirEmpty(const char* dirFilename);
	static bool RenameBak(const char* filename, const char* bakPart, bool removeOldExtension, char* newNameBuf, int newNameBufSize);
#ifndef WIN32
	static bool ExpandHomePath(const char* filename, char* buffer, int bufSize);
	static void FixExecPermission(const char* filename);
#endif
	static void ExpandFileName(const char* filename, char* buffer, int bufSize);
	static void GetExeFileName(const char* argv0, char* buffer, int bufSize);
	static char* FormatSpeed(char* buffer, int bufSize, int bytesPerSecond);
	static char* FormatSize(char* buffer, int bufLen, long long fileSize);
	static bool SameFilename(const char* filename1, const char* filename2);
	static bool MatchFileExt(const char* filename, const char* extensionList, const char* listSeparator);
	static char* GetLastErrorMessage(char* buffer, int bufLen);
	static long long GetCurrentTicks();

	/* Flush disk buffers for file with given descriptor */
	static bool FlushFileBuffers(int fileDescriptor, char* errBuf, int bufSize);

	/* Flush disk buffers for file metadata (after file renaming) */
	static bool FlushDirBuffers(const char* filename, char* errBuf, int bufSize);

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
	static bool SplitCommandLine(const char* commandLine, char*** argv);

	static long long JoinInt64(unsigned long Hi, unsigned long Lo);
	static void SplitInt64(long long Int64, unsigned long* Hi, unsigned long* Lo);

	static void TrimRight(char* str);
	static char* Trim(char* str);
	static bool EmptyStr(const char* str) { return !str || !*str; }

	/* replace all occurences of szFrom to szTo in string szStr with a limitation that szTo must be shorter than szFrom */
	static char* ReduceStr(char* str, const char* from, const char* to);

	/* Calculate Hash using Bob Jenkins (1996) algorithm */
	static unsigned int HashBJ96(const char* buffer, int bufSize, unsigned int initValue);

#ifdef WIN32
	static bool RegReadStr(HKEY hKey, const char* keyName, const char* valueName, char* buffer, int* bufLen);
#endif

	static void SetStandByMode(bool standBy);

	/* cross platform version of GNU timegm, which is similar to mktime but takes an UTC time as parameter */
	static time_t Timegm(tm const *t);

	/*
	 * Returns program version and revision number as string formatted like "0.7.0-r295".
	 * If revision number is not available only version is returned ("0.7.0").
	 */
	static const char* VersionRevision() { return VersionRevisionBuf; };
	
	static char VersionRevisionBuf[100];

	static void Init();

	static unsigned long Crc32(unsigned char *block, unsigned long length);
	static unsigned long Crc32m(unsigned long startCrc, unsigned char *block, unsigned long length);
	static unsigned long Crc32Combine(unsigned long crc1, unsigned long crc2, unsigned long len2);

	/*
	 * Returns number of available CPU cores or -1 if it could not be determined
	 */
	static int NumberOfCpuCores();
};

class WebUtil
{
public:
	static unsigned int DecodeBase64(char* inputBuffer, int inputBufferLength, char* outputBuffer);

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
	static const char* XmlFindTag(const char* xml, const char* tag, int* valueLength);

	/*
	 * Parses tag-content into szValueBuf.
	 */
	static bool XmlParseTagValue(const char* xml, const char* tag, char* valueBuf, int valueBufSize, const char** tagEnd);

	/*
	 * Replaces all tags with spaces effectively providing the text content only.
	 * The string is transformed in-place overwriting the previous content.
	 */
	static void XmlStripTags(char* xml);

	/*
	 * Replaces all entities with spaces.
	 * The string is transformed in-place overwriting the previous content.
	 */
	static void XmlRemoveEntities(char* raw);

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
	static const char* JsonFindField(const char* jsonText, const char* fieldName, int* valueLength);

	/*
	 * Returns pointer to field-content and length of content in iValueLength
	 * The returned pointer points to the part of source-string, no additional strings are allocated.
	 */
	static const char* JsonNextValue(const char* jsonText, int* valueLength);

	/*
	 * Unquote http quoted string.
	 * The string is decoded on the place overwriting the content of raw-data.
	 */
	static void HttpUnquote(char* raw);

	/*
	 * Decodes URL-string.
	 * The string is decoded on the place overwriting the content of raw-data.
	 */
	static void URLDecode(char* raw);

	/*
	 * Makes valid URL by replacing of spaces with "%20".
	 * Returns new string allocated with malloc, it need to be freed by caller.
	 */
	static char* URLEncode(const char* raw);

#ifdef WIN32
	static bool Utf8ToAnsi(char* buffer, int bufLen);
	static bool AnsiToUtf8(char* buffer, int bufLen);
#endif

	/*
	 * Converts ISO-8859-1 (aka Latin-1) into UTF-8.
	 * Returns new string allocated with malloc, it needs to be freed by caller.
	 */
	static char* Latin1ToUtf8(const char* str);

	static time_t ParseRfc822DateTime(const char* dateTimeStr);
};

class URL
{
private:
	char*				m_address;
	char*				m_protocol;
	char*				m_user;
	char*				m_password;
	char*				m_host;
	char*				m_resource;
	int					m_port;
	bool				m_tLS;
	bool				m_valid;
	void				ParseURL();

public:
	 					URL(const char* address);
						~URL();
	bool				IsValid() { return m_valid; }
	const char*			GetAddress() { return m_address; }
	const char*			GetProtocol() { return m_protocol; }
	const char*			GetUser() { return m_user; }
	const char*			GetPassword() { return m_password; }
	const char*			GetHost() { return m_host; }
	const char*			GetResource() { return m_resource; }
	int					GetPort() { return m_port; }
	bool				GetTLS() { return m_tLS; }
};

class RegEx
{
private:
	void*				m_context;
	bool				m_valid;
	void*				m_matches;
	int					m_matchBufSize;

public:
						RegEx(const char *pattern, int matchBufSize = 100);
						~RegEx();
	bool				IsValid() { return m_valid; }
	bool				Match(const char *str);
	int					GetMatchCount();
	int					GetMatchStart(int index);
	int					GetMatchLen(int index);
};

class WildMask
{
private:
	char*				m_pattern;
	bool				m_wantsPositions;
	int					m_wildCount;
	int*				m_wildStart;
	int*				m_wildLen;
	int					m_arrLen;

	void				ExpandArray();

public:
						WildMask(const char* pattern, bool wantsPositions = false);
						~WildMask();
	bool				Match(const char* text);
	int					GetMatchCount() { return m_wildCount; }
	int					GetMatchStart(int index) { return m_wildStart[index]; }
	int					GetMatchLen(int index) { return m_wildLen[index]; }
};

#ifndef DISABLE_GZIP
class ZLib
{
public:
	/*
	 * calculates the size required for output buffer
	 */
	static unsigned int GZipLen(int inputBufferLength);
	
	/*
	 * returns the size of bytes written to szOutputBuffer or 0 if the buffer is too small or an error occured.
	 */
	static unsigned int GZip(const void* inputBuffer, int inputBufferLength, void* outputBuffer, int outputBufferLength);
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
	void*				m_zStream;
	void*				m_outputBuffer;
	int					m_bufferSize;

public:
						GUnzipStream(int BufferSize);
						~GUnzipStream();

	/*
	 * set next memory block for uncompression
	 */
	void				Write(const void *inputBuffer, int inputBufferLength);

	/*
	 * get next uncompressed memory block.
	 * iOutputBufferLength - the size of uncompressed block. if it is "0" the next compressed block must be provided via "Write".
	 */
	EStatus				Read(const void **outputBuffer, int *outputBufferLength);
};
#endif

class Tokenizer
{
private:
	char				m_defaultBuf[2014];
	char*				m_dataString;
	bool				m_inplaceBuf;
	const char*			m_separators;
	char*				m_savePtr;
	bool				m_working;

public:
						Tokenizer(const char* dataString, const char* separators);
						Tokenizer(char* dataString, const char* separators, bool inplaceBuf);
						~Tokenizer();
	char*				Next();
};

#endif
