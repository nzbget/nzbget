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

#include "NString.h"

#ifdef WIN32
extern int optind, opterr;
extern char *optarg;
int getopt(int argc, char *argv[], char *optstring);
#endif

class Util
{
public:
	static bool MatchFileExt(const char* filename, const char* extensionList, const char* listSeparator);
	static int64 GetCurrentTicks();

	/*
	 * Split command line into arguments.
	 * Uses spaces and single quotation marks as separators.
	 * Returns bool if sucessful or false if bad escaping was detected.
	 * Parameter "argv" may be nullptr if only a syntax check is needed.
	 * Parsed parameters returned in Array "argv", which contains at least one element.
	 * The last element in array is nullptr.
	 * Restrictions: the number of arguments is limited to 100 and each argument must
	 * be maximum 1024 chars long.
	 * If these restrictions are exceeded, only first 100 arguments and only first 1024
	 * for each argument are returned (the function still returns "true").
	 */
	static bool SplitCommandLine(const char* commandLine, char*** argv);

	static int64 JoinInt64(uint32 Hi, uint32 Lo);
	static void SplitInt64(int64 Int64, uint32* Hi, uint32* Lo);

	static void TrimRight(char* str);
	static char* Trim(char* str);
	static bool EmptyStr(const char* str) { return !str || !*str; }

	/* replace all occurences of szFrom to szTo in string szStr with a limitation that szTo must be shorter than szFrom */
	static char* ReduceStr(char* str, const char* from, const char* to);

	/* Calculate Hash using Bob Jenkins (1996) algorithm */
	static uint32 HashBJ96(const char* buffer, int bufSize, uint32 initValue);

#ifdef WIN32
	static bool RegReadStr(HKEY keyRoot, const char* keyName, const char* valueName, char* buffer, int* bufLen);
#endif

	static void SetStandByMode(bool standBy);

	static time_t CurrentTime();

	/* cross platform version of GNU timegm, which is similar to mktime but takes an UTC time as parameter */
	static time_t Timegm(tm const *t);

	static void FormatTime(time_t timeSec, char* buffer, int bufsize);
	static CString FormatTime(time_t timeSec);

	static CString FormatSpeed(int bytesPerSecond);
	static CString FormatSize(int64 fileSize);

	/*
	 * Returns program version and revision number as string formatted like "0.7.0-r295".
	 * If revision number is not available only version is returned ("0.7.0").
	 */
	static const char* VersionRevision() { return VersionRevisionBuf; };

	static char VersionRevisionBuf[100];

	static void Init();

	static uint32 Crc32(uchar *block, uint32 length);
	static uint32 Crc32m(uint32 startCrc, uchar *block, uint32 length);
	static uint32 Crc32Combine(uint32 crc1, uint32 crc2, uint32 len2);

	/*
	 * Returns number of available CPU cores or -1 if it could not be determined
	 */
	static int NumberOfCpuCores();
};

class WebUtil
{
public:
	static uint32 DecodeBase64(char* inputBuffer, int inputBufferLength, char* outputBuffer);

	/*
	 * Encodes string to be used as content of xml-tag.
	 */
	static CString XmlEncode(const char* raw);

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
	 */
	static CString JsonEncode(const char* raw);

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
	static void UrlDecode(char* raw);

	/*
	 * Makes valid URL by replacing of spaces with "%20".
	 */
	static CString UrlEncode(const char* raw);

	/*
	 * Converts ISO-8859-1 (aka Latin-1) into UTF-8.
	 */
	static CString Latin1ToUtf8(const char* str);

	static time_t ParseRfc822DateTime(const char* dateTimeStr);
};

class URL
{
private:
	CString				m_address;
	CString				m_protocol;
	CString				m_user;
	CString				m_password;
	CString				m_host;
	CString				m_resource;
	int					m_port;
	bool				m_tls;
	bool				m_valid;
	void				ParseUrl();

public:
	 					URL(const char* address);
	bool				IsValid() { return m_valid; }
	const char*			GetAddress() { return m_address; }
	const char*			GetProtocol() { return m_protocol; }
	const char*			GetUser() { return m_user; }
	const char*			GetPassword() { return m_password; }
	const char*			GetHost() { return m_host; }
	const char*			GetResource() { return m_resource; }
	int					GetPort() { return m_port; }
	bool				GetTls() { return m_tls; }
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
	static uint32 GZipLen(int inputBufferLength);

	/*
	 * compresses inputBuffer and returns the size of bytes written to
	 * outputBuffer or 0 if the buffer is too small or an error occured.
	 */
	static uint32 GZip(const void* inputBuffer, int inputBufferLength, void* outputBuffer, int outputBufferLength);
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
	BString<1024>		m_defaultBuf;
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
