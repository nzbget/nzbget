/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2007-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

	/*
	* Split command line into arguments.
	* Uses spaces and single quotation marks as separators.
	* May return empty list if bad escaping was detected.
	*/
	static std::vector<CString> SplitCommandLine(const char* commandLine);

	static int64 JoinInt64(uint32 Hi, uint32 Lo);
	static void SplitInt64(int64 Int64, uint32* Hi, uint32* Lo);

	static void TrimRight(char* str);
	static char* Trim(char* str);
	static bool EmptyStr(const char* str) { return !str || !*str; }
	static std::vector<CString> SplitStr(const char* str, const char* separators);
	static bool EndsWith(const char* str, const char* suffix, bool caseSensitive);
	static bool AlphaNum(const char* str);
    static bool IsObfuscated(const char* str);

	/* replace all occurences of szFrom to szTo in string szStr with a limitation that szTo must be shorter than szFrom */
	static char* ReduceStr(char* str, const char* from, const char* to);

	/* Calculate Hash using Bob Jenkins (1996) algorithm */
	static uint32 HashBJ96(const char* buffer, int bufSize, uint32 initValue);

#ifdef WIN32
	static bool RegReadStr(HKEY keyRoot, const char* keyName, const char* valueName, char* buffer, int* bufLen);
#endif

	static void SetStandByMode(bool standBy);

	static time_t CurrentTime();
	static int64 CurrentTicks();
	static void Sleep(int milliseconds);

	/* cross platform version of GNU timegm, which is similar to mktime but takes an UTC time as parameter */
	static time_t Timegm(tm const *t);

	static void FormatTime(time_t timeSec, char* buffer, int bufsize);
	static CString FormatTime(time_t timeSec);

	static CString FormatSpeed(int bytesPerSecond);
	static CString FormatSize(int64 fileSize);
	static CString FormatBuffer(const char* buf, int len);

	/*
	* Returns program version and revision number as string formatted like "0.7.0-r295".
	* If revision number is not available only version is returned ("0.7.0").
	*/
	static const char * VersionRevision() { return VersionRevisionString; };

	static const char * VersionRevisionString;

	static void Init();

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
public:
	URL(const char* address);
	bool IsValid() { return m_valid; }
	const char* GetAddress() { return m_address; }
	const char* GetProtocol() { return m_protocol; }
	const char* GetUser() { return m_user; }
	const char* GetPassword() { return m_password; }
	const char* GetHost() { return m_host; }
	const char* GetResource() { return m_resource; }
	int GetPort() { return m_port; }
	bool GetTls() { return m_tls; }

private:
	CString m_address;
	CString m_protocol;
	CString m_user;
	CString m_password;
	CString m_host;
	CString m_resource;
	int m_port = 0;
	bool m_tls = false;
	bool m_valid = false;

	void ParseUrl();
};

class RegEx
{
public:
	RegEx(const char *pattern, int matchBufSize = 100);
	~RegEx();
	bool IsValid() { return m_valid; }
	bool Match(const char* str);
	int GetMatchCount();
	int GetMatchStart(int index);
	int GetMatchLen(int index);

private:
#ifdef HAVE_REGEX_H
	regex_t m_context;
	std::unique_ptr<regmatch_t[]> m_matches;
#endif
	bool m_valid;
	int m_matchBufSize;
};

class WildMask
{
public:
	WildMask(const char* pattern, bool wantsPositions = false):
		m_pattern(pattern), m_wantsPositions(wantsPositions) {}
	bool Match(const char* text);
	int GetMatchCount() { return m_wildCount; }
	int GetMatchStart(int index) { return m_wildStart[index]; }
	int GetMatchLen(int index) { return m_wildLen[index]; }

private:
	typedef std::vector<int> IntList;

	CString m_pattern;
	bool m_wantsPositions;
	int m_wildCount;
	IntList m_wildStart;
	IntList m_wildLen;

	void ExpandArray();
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

	GUnzipStream(int BufferSize);
	~GUnzipStream();

	/*
	* set next memory block for uncompression
	*/
	void Write(const void *inputBuffer, int inputBufferLength);

	/*
	* get next uncompressed memory block.
	* iOutputBufferLength - the size of uncompressed block. if it is "0" the next compressed block must be provided via "Write".
	*/
	EStatus Read(const void **outputBuffer, int *outputBufferLength);

private:
	z_stream m_zStream = {0};
	std::unique_ptr<Bytef[]> m_outputBuffer;
	int m_bufferSize;
	bool m_active = false;
};
#endif

class Tokenizer
{
public:
	Tokenizer(const char* dataString, const char* separators);
	Tokenizer(char* dataString, const char* separators, bool inplaceBuf);
	char* Next();

private:
	BString<1024> m_shortString;
	CString m_longString;
	char* m_dataString;
	const char* m_separators;
	char* m_savePtr = nullptr;
	bool m_working = false;
};

class Crc32
{
public:
	Crc32() { Reset(); }
	void Reset();
	void Append(uchar* block, uint32 length);
	uint32 Finish();
	static uint32 Combine(uint32 crc1, uint32 crc2, uint32 len2);

private:
#if defined(WIN32) && !defined(_WIN64)
	// VC++ in 32 bit mode can not "alignas(16)" dynamically allocated objects
	alignas(8) uint32_t m_state[4 * 5 + 8]; // = YEncode::crc_state
	void* State() { void* p = &m_state; size_t s = sizeof(m_state); return std::align(16, 4 * 5, p, s); }
#else
	alignas(16) uint32_t m_state[4 * 5]; // = YEncode::crc_state
	void* State() { return &m_state; }
#endif
};

#endif
