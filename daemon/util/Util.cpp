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


#include "nzbget.h"
#include "Util.h"			
#include "YEncode.h"

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
	static char *next = nullptr;
	if (optind == 0)
		next = nullptr;

	optarg = nullptr;

	if (next == nullptr || *next == '\0')
	{
		if (optind == 0)
			optind++;

		if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0')
		{
			optarg = nullptr;
			if (optind < argc)
				optarg = argv[optind];
			return -1;
		}

		if (strcmp(argv[optind], "--") == 0)
		{
			optind++;
			optarg = nullptr;
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

	if (cp == nullptr || c == ':')
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
			next = nullptr;
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
#endif


const char * Util::VersionRevisionString = VERSION;

void Util::Init()
{
    if ( VersionRevisionString[0] == 'v') {
        ++VersionRevisionString;
    }

    // init static vars there
	CurrentTicks();
}

int64 Util::JoinInt64(uint32 Hi, uint32 Lo)
{
	return (((int64)Hi) << 32) + Lo;
}

void Util::SplitInt64(int64 Int64, uint32* Hi, uint32* Lo)
{
	*Hi = (uint32)(Int64 >> 32);
	*Lo = (uint32)(Int64 & 0xFFFFFFFF);
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

uint32 DecodeByteQuartet(char* inputBuffer, char* outputBuffer)
{
	uint32 buffer = 0;

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

CString Util::FormatSize(int64 fileSize)
{
	CString result;

	if (fileSize > 1024 * 1024 * 1000)
	{
		result.Format("%.2f GB", (float)((float)fileSize / 1024 / 1024 / 1024));
	}
	else if (fileSize > 1024 * 1000)
	{
		result.Format("%.2f MB", (float)((float)fileSize / 1024 / 1024));
	}
	else if (fileSize > 1000)
	{
		result.Format("%.2f KB", (float)((float)fileSize / 1024));
	}
	else if (fileSize == 0)
	{
		result = "0 MB";
	}
	else
	{
		result.Format("%i B", (int)fileSize);
	}
	return result;
}

CString Util::FormatSpeed(int bytesPerSecond)
{
	CString result;

	if (bytesPerSecond >= 100 * 1024 * 1024)
	{
		result.Format("%i MB/s", bytesPerSecond / 1024 / 1024);
	}
	else if (bytesPerSecond >= 10 * 1024 * 1024)
	{
		result.Format("%0.1f MB/s", (float)bytesPerSecond / 1024.0 / 1024.0);
	}
	else if (bytesPerSecond >= 1024 * 1000)
	{
		result.Format("%0.2f MB/s", (float)bytesPerSecond / 1024.0 / 1024.0);
	}
	else
	{
		result.Format("%i KB/s", bytesPerSecond / 1024);
	}

	return result;
}

void Util::FormatTime(time_t timeSec, char* buffer, int bufsize)
{
#ifdef HAVE_CTIME_R_3
	ctime_r(&timeSec, buffer, bufsize);
#else
	ctime_r(&timeSec, buffer);
#endif
	buffer[bufsize-1] = '\0';

	// trim LF
	buffer[strlen(buffer) - 1] = '\0';
}

CString Util::FormatTime(time_t timeSec)
{
	CString result;
	result.Reserve(50);
	FormatTime(timeSec, result, 50);
	return result;
}

CString Util::FormatBuffer(const char* buf, int len)
{
	CString result;
	result.Reserve(len * 3 + 1);
	while (len--)
	{
		result.AppendFmt("%02x ", (int)(uchar)*buf++);
	}
	return result;
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

std::vector<CString> Util::SplitCommandLine(const char* commandLine)
{
	std::vector<CString> result;
	char buf[1024];
	uint32 len = 0;
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

		if ((space || !*p) && len > 0)
		{
			//add token
			buf[len] = '\0';
			result.emplace_back(buf);
			len = 0;
		}

		if (!*p)
		{
			break;
		}
	}

	return result;
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

std::vector<CString> Util::SplitStr(const char* str, const char* separators)
{
	std::vector<CString> result;
	Tokenizer tok(str, separators);
	while (const char* substr = tok.Next())
	{
		result.emplace_back(substr);
	}
	return result;
}

bool Util::EndsWith(const char* str, const char* suffix, bool caseSensitive)
{
	if (!str)
	{
		return false;
	}

	if (EmptyStr(suffix))
	{
		return true;
	}

	int lenStr = strlen(str);
	int lenSuf = strlen(suffix);

	if (lenSuf > lenStr)
	{
		return false;
	}

	if (caseSensitive)
	{
		return !strcmp(str + lenStr - lenSuf, suffix);
	}
	else
	{
		return !strcasecmp(str + lenStr - lenSuf, suffix);
	}
}

bool Util::AlphaNum(const char* str)
{
	for (const char* p = str; *p; p++)
	{
		char ch = *p;
		if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')))
		{
			return false;
		}
	}
	return true;
}

/*
 * like AlphaNum, but also ignores any extension and explicitly
 * checks for the format 'abc.xyz.<string of hex digits>.ext'
 * for which AlphaNum returns false.
 */
bool Util::IsObfuscated(const char* str)
{
    const char * p;

    if (strncmp( str, "abc.xyz.", 8 ) == 0)
    {
        /* validate that the leading 'abc.xyz.' is followed by a series
         * of hexadecimal chars, up to the extension/end of string */
        for ( p = str + 8; *p != '\0' && *p != '.' ; ++p )
        {
            if ( !((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f')) )
            {
                return false;
            }
        }
    }
    else for ( p = str; *p != '\0'; ++p )
    {
        if ( !((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) )
        {
            return false;
        }
    }
    return true;
}


/* Calculate Hash using Bob Jenkins (1996) algorithm
 * http://burtleburtle.net/bob/c/lookup2.c
 */

#define hashsize(n) ((uint32)1<<(n))
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

uint32 hash(uint8 *k, uint32 length, uint32 initval)
// register uint8 *k;        /* the key */
// register uint32  length;   /* the length of the key */
// register uint32  initval;    /* the previous hash, or an arbitrary value */
{
	uint32 a,b,c,len;

	/* Set up the internal state */
	len = length;
	a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
	c = initval;           /* the previous hash value */

	/*---------------------------------------- handle most of the key */
	while (len >= 12)
	{
		a += (k[0] +((uint32)k[1]<<8) +((uint32)k[2]<<16) +((uint32)k[3]<<24));
		b += (k[4] +((uint32)k[5]<<8) +((uint32)k[6]<<16) +((uint32)k[7]<<24));
		c += (k[8] +((uint32)k[9]<<8) +((uint32)k[10]<<16)+((uint32)k[11]<<24));
		mix(a,b,c);
		k += 12; len -= 12;
	}

	/*------------------------------------- handle the last 11 bytes */
	c += length;
	switch(len)              /* all the case statements fall through */
	{
		case 11: c+=((uint32)k[10]<<24);
		case 10: c+=((uint32)k[9]<<16);
		case 9 : c+=((uint32)k[8]<<8);
			/* the first byte of c is reserved for the length */
		case 8 : b+=((uint32)k[7]<<24);
		case 7 : b+=((uint32)k[6]<<16);
		case 6 : b+=((uint32)k[5]<<8);
		case 5 : b+=k[4];
		case 4 : a+=((uint32)k[3]<<24);
		case 3 : a+=((uint32)k[2]<<16);
		case 2 : a+=((uint32)k[1]<<8);
		case 1 : a+=k[0];
			/* case 0: nothing left to add */
	}
	mix(a,b,c);
	/*-------------------------------------------- report the result */
	return c;
}

uint32 Util::HashBJ96(const char* buffer, int bufSize, uint32 initValue)
{
	return (uint32)hash((uint8*)buffer, (uint32)bufSize, (uint32)initValue);
}

#ifdef WIN32
bool Util::RegReadStr(HKEY keyRoot, const char* keyName, const char* valueName, char* buffer, int* bufLen)
{
	HKEY subKey;
	if (!RegOpenKeyEx(keyRoot, keyName, 0, KEY_READ, &subKey))
	{
		DWORD retBytes = *bufLen;
		LONG ret = RegQueryValueEx(subKey, valueName, nullptr, nullptr, (LPBYTE)buffer, &retBytes);
		*bufLen = retBytes;
		RegCloseKey(subKey);
		return ret == 0;
	}
	return false;
}
#endif

time_t Util::CurrentTime()
{
#ifdef WIN32
	// C-library function "time()" works on Windows too but is very CPU intensive
	// since it uses high performance timer which we don't need anyway.
	// A combination of GetSystemTime() + Timegm() works much faster.
	SYSTEMTIME systm;
	GetSystemTime(&systm);
	struct tm tm;
	tm.tm_year = systm.wYear - 1900;
	tm.tm_mon = systm.wMonth - 1;
	tm.tm_mday = systm.wDay;
	tm.tm_hour = systm.wHour;
	tm.tm_min = systm.wMinute;
	tm.tm_sec = systm.wSecond;
	return Timegm(&tm);
#else
	return ::time(nullptr);
#endif
}

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

int64 Util::CurrentTicks()
{
#ifdef WIN32
	static int64 hz=0, hzo=0;
	if (!hz)
	{
		QueryPerformanceFrequency((LARGE_INTEGER*)&hz);
		QueryPerformanceCounter((LARGE_INTEGER*)&hzo);
	}
	int64 t;
	QueryPerformanceCounter((LARGE_INTEGER*)&t);
	return ((t-hzo)*1000000)/hz;
#else
	timeval t;
	gettimeofday(&t, nullptr);
	return (int64)(t.tv_sec) * 1000000ll + (int64)(t.tv_usec);
#endif
}

void Util::Sleep(int milliseconds)
{
#ifdef WIN32
	::Sleep(milliseconds);
#else
	usleep(milliseconds * 1000);
#endif
}

uint32 WebUtil::DecodeBase64(char* inputBuffer, int inputBufferLength, char* outputBuffer)
{
	uint32 InputBufferIndex  = 0;
	uint32 OutputBufferIndex = 0;
	uint32 InputBufferLength = inputBufferLength > 0 ? inputBufferLength : strlen(inputBuffer);

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

CString WebUtil::XmlEncode(const char* raw)
{
	// calculate the required outputstring-size based on number of xml-entities and their sizes
	int reqSize = strlen(raw);
	for (const char* p = raw; *p; p++)
	{
		uchar ch = *p;
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

	CString result;
	result.Reserve(reqSize);

	// copy string
	char* output = result;
	for (const char* p = raw; ; p++)
	{
		uchar ch = *p;
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
					uint32 cp = ch;

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
						*output++ = '.';
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
	BString<100> openTag("<%s>", tag);
	BString<100> closeTag("</%s>", tag);
	BString<100> openCloseTag("<%s/>", tag);

	const char* pstart = strstr(xml, openTag);
	const char* pstartend = strstr(xml, openCloseTag);
	if (!pstart && !pstartend) return nullptr;

	if (pstartend && (!pstart || pstartend < pstart))
	{
		*valueLength = 0;
		return pstartend;
	}

	const char* pend = strstr(pstart, closeTag);
	if (!pend) return nullptr;

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

CString WebUtil::JsonEncode(const char* raw)
{
	// calculate the required outputstring-size based on number of escape-entities and their sizes
	int reqSize = strlen(raw);
	for (const char* p = raw; *p; p++)
	{
		uchar ch = *p;
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

	CString result;
	result.Reserve(reqSize);

	// copy string
	char* output = result;
	for (const char* p = raw; ; p++)
	{
		uchar ch = *p;
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
					uint32 cp = ch;

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
							*output++ = (char)strtol(p + 1, nullptr, 16);
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
	BString<100> openTag("\"%s\"", fieldName);

	const char* pstart = strstr(jsonText, openTag);
	if (!pstart) return nullptr;

	pstart += strlen(openTag);

	return JsonNextValue(pstart, valueLength);
}

const char* WebUtil::JsonNextValue(const char* jsonText, int* valueLength)
{
	const char* pstart = jsonText;

	while (*pstart && strchr(" ,[{:\r\n\t\f", *pstart)) pstart++;
	if (!*pstart) return nullptr;

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
			if (!*++pend || !*++pend) return nullptr;
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
					uchar c1 = *p++;
					uchar c2 = *p++;
					c1 = '0' <= c1 && c1 <= '9' ? c1 - '0' : 'A' <= c1 && c1 <= 'F' ? c1 - 'A' + 10 :
						'a' <= c1 && c1 <= 'f' ? c1 - 'a' + 10 : 0;
					c2 = '0' <= c2 && c2 <= '9' ? c2 - '0' : 'A' <= c2 && c2 <= 'F' ? c2 - 'A' + 10 :
						'a' <= c2 && c2 <= 'f' ? c2 - 'a' + 10 : 0;
					uchar ch = (c1 << 4) + c2;
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

CString WebUtil::UrlEncode(const char* raw)
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

	CString result;
	result.Reserve(reqSize);

	// copy string
	char* output = result;
	for (const char* p = raw; ; p++)
	{
		uchar ch = *p;
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

CString WebUtil::Latin1ToUtf8(const char* str)
{
	CString res;
	res.Reserve(strlen(str) * 2);
	const uchar *in = (const uchar*)str;
	uchar *out = (uchar*)(char*)res;
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


URL::URL(const char* address) :
	m_address(address)
{
	if (address)
	{
		ParseUrl();
	}
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

	m_protocol.Set(m_address, (int)(protEnd - m_address));

	char* hostStart = protEnd + 3;
	char* slash = strchr(hostStart, '/');
	char* hostEnd = nullptr;
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
				m_password.Set(pass + 1, len);
			}
			userend = pass - 1;
		}

		int len = (int)(userend - hostStart + 1);
		if (len > 0)
		{
			m_user.Set(hostStart, len);
		}

		hostStart = amp + 1;
	}

	if (slash)
	{
		char* resEnd = m_address + strlen(m_address);
		m_resource.Set(slash, (int)(resEnd - slash + 1));

		hostEnd = slash - 1;
	}
	else
	{
		m_resource = "/";

		hostEnd = m_address + strlen(m_address);
	}

	char* colon = strchr(hostStart, ':');
	if (colon && colon < hostEnd)
	{
		hostEnd = colon - 1;
		m_port = atoi(colon + 1);
	}

	m_host.Set(hostStart, (int)(hostEnd - hostStart + 1));

	m_valid = true;
}


RegEx::RegEx(const char *pattern, int matchBufSize) :
	m_matchBufSize(matchBufSize)
{
#ifdef HAVE_REGEX_H
	m_valid = regcomp(&m_context, pattern, REG_EXTENDED | REG_ICASE | (matchBufSize > 0 ? 0 : REG_NOSUB)) == 0;
	if (matchBufSize > 0)
	{
		m_matches = std::make_unique<regmatch_t[]>(matchBufSize);
	}
	else
	{
		m_matches = nullptr;
	}
#else
	m_valid = false;
#endif
}

RegEx::~RegEx()
{
#ifdef HAVE_REGEX_H
	regfree(&m_context);
#endif
}

bool RegEx::Match(const char *str)
{
#ifdef HAVE_REGEX_H
	return m_valid ? regexec(&m_context, str, m_matchBufSize, m_matches.get(), 0) == 0 : false;
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
		while (count < m_matchBufSize && m_matches[count].rm_so > -1)
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
	return m_matches[index].rm_so;
#else
	return 0;
#endif
}

int RegEx::GetMatchLen(int index)
{
#ifdef HAVE_REGEX_H
	return m_matches[index].rm_eo - m_matches[index].rm_so;
#else
	return 0;
#endif
}


void WildMask::ExpandArray()
{
	m_wildCount++;
	m_wildStart.resize(m_wildCount);
	m_wildLen.resize(m_wildCount);
}

// Based on code from http://bytes.com/topic/c/answers/212179-string-matching
// Extended to save positions of matches.
bool WildMask::Match(const char* text)
{
	m_wildCount = 0;
	m_wildStart.clear();
	m_wildStart.reserve(100);
	m_wildLen.clear();
	m_wildLen.reserve(100);

	const char* pat = m_pattern;
	const char* str = text;
	const char *spos, *wpos;
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
				m_wildStart[m_wildCount-1] = (int)(str - text);
				m_wildLen[m_wildCount-1] = 0;
				qmark = true;
			}
		}
		else if (m_wantsPositions && qmark)
		{
			m_wildLen[m_wildCount-1] = (int)(str - (text + m_wildStart[m_wildCount-1]));
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
		m_wildLen[m_wildCount-1] = (int)(str - (text + m_wildStart[m_wildCount-1]));
		qmark = false;
	}

	while (*str)
	{
		if (*pat == '*')
		{
			if (m_wantsPositions && qmark)
			{
				m_wildLen[m_wildCount-1] = (int)(str - (text + m_wildStart[m_wildCount-1]));
				qmark = false;
			}
			if (m_wantsPositions && !star)
			{
				ExpandArray();
				m_wildStart[m_wildCount-1] = (int)(str - text);
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
				m_wildStart[m_wildCount-1] = (int)(str - text);
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
				m_wildLen[m_wildCount-1] = (int)(str - (text + m_wildStart[m_wildCount-1]));
				qmark = false;
			}
			else if (m_wantsPositions && star)
			{
				m_wildLen[m_wildCount-1] = (int)(str - (text + m_wildStart[m_wildCount-1]));
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
		m_wildLen[m_wildCount-1] = (int)(str - (text + m_wildStart[m_wildCount-1]));
	}

	if (*pat == '*' && m_wantsPositions && !star)
	{
		ExpandArray();
		m_wildStart[m_wildCount-1] = (int)(str - text);
		m_wildLen[m_wildCount-1] = strlen(str);
	}

	while (*pat == '*')
	{
		pat++;
	}

	return *pat == '\0';
}


#ifndef DISABLE_GZIP
uint32 ZLib::GZipLen(int inputBufferLength)
{
	z_stream zstr{0};
	return (uint32)deflateBound(&zstr, inputBufferLength);
}

uint32 ZLib::GZip(const void* inputBuffer, int inputBufferLength, void* outputBuffer, int outputBufferLength)
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

	uint32 total_out = 0;
	if (deflate(&zstr, Z_FINISH) == Z_STREAM_END)
	{
		total_out = (uint32)zstr.total_out;
	}

	deflateEnd(&zstr);

	return total_out;
}

GUnzipStream::GUnzipStream(int BufferSize) :
	m_bufferSize(BufferSize)
{
	m_outputBuffer = std::make_unique<Bytef[]>(BufferSize);

	/* add 16 to MAX_WBITS to enforce gzip format */
	int ret = inflateInit2(&m_zStream, MAX_WBITS + 16);
	m_active = ret == Z_OK;
}

GUnzipStream::~GUnzipStream()
{
	if (m_active)
	{
		inflateEnd(&m_zStream);
	}
}

void GUnzipStream::Write(const void *inputBuffer, int inputBufferLength)
{
	m_zStream.next_in = (Bytef*)inputBuffer;
	m_zStream.avail_in = inputBufferLength;
}

GUnzipStream::EStatus GUnzipStream::Read(const void **outputBuffer, int *outputBufferLength)
{
	m_zStream.next_out = (Bytef*)m_outputBuffer.get();
	m_zStream.avail_out = m_bufferSize;

	*outputBufferLength = 0;

	if (!m_active)
	{
		return zlError;
	}

	int ret = inflate(&m_zStream, Z_NO_FLUSH);

	switch (ret)
	{
		case Z_STREAM_END:
		case Z_OK:
			*outputBufferLength = m_bufferSize - m_zStream.avail_out;
			*outputBuffer = m_outputBuffer.get();
			return ret == Z_STREAM_END ? zlFinished : zlOK;

		case Z_BUF_ERROR:
			return zlOK;
	}

	return zlError;
}
#endif

Tokenizer::Tokenizer(const char* dataString, const char* separators) :
	m_separators(separators)
{
	// an optimization to avoid memory allocation for short data string
	int len = strlen(dataString);
	if (len < m_shortString.Capacity())
	{
		m_shortString.Set(dataString);
		m_dataString = m_shortString;
	}
	else
	{
		m_longString.Set(dataString);
		m_dataString = m_longString;
	}

}

Tokenizer::Tokenizer(char* dataString, const char* separators, bool inplaceBuf) :
	m_separators(separators)
{
	if (inplaceBuf)
	{
		m_dataString = dataString;
	}
	else
	{
		m_longString.Set(dataString);
		m_dataString = m_longString;
	}
}

char* Tokenizer::Next()
{
	char* token = nullptr;
	while (!token || !*token)
	{
		token = strtok_r(m_working ? nullptr : m_dataString, m_separators, &m_savePtr);
		m_working = true;
		if (!token)
		{
			return nullptr;
		}
		token = Util::Trim(token);
	}
	return token;
}

void Crc32::Reset()
{
	static_assert(sizeof(m_state) >= sizeof(YEncode::crc_state), "m_state has invalid size");

	YEncode::crc_init((YEncode::crc_state*)State());
}

void Crc32::Append(uchar* block, uint32 length)
{
	YEncode::crc_incr((YEncode::crc_state*)State(), block, length);
}

uint32 Crc32::Finish()
{
	return YEncode::crc_finish((YEncode::crc_state*)State());
}

/* From zlib/crc32.c (http://www.zlib.net/)
 * Copyright (C) 1995-2006, 2010, 2011, 2012 Mark Adler
 */

#define GF2_DIM 32      /* dimension of GF(2) vectors (length of CRC) */

uint32 gf2_matrix_times(uint32 *mat, uint32 vec)
{
	uint32 sum;

	sum = 0;
	while (vec) {
		if (vec & 1)
			sum ^= *mat;
		vec >>= 1;
		mat++;
	}
	return sum;
}

void gf2_matrix_square(uint32 *square, uint32 *mat)
{
	int n;

	for (n = 0; n < GF2_DIM; n++)
		square[n] = gf2_matrix_times(mat, mat[n]);
}

uint32 Crc32::Combine(uint32 crc1, uint32 crc2, uint32 len2)
{
	int n;
	uint32 row;
	uint32 even[GF2_DIM];    /* even-power-of-two zeros operator */
	uint32 odd[GF2_DIM];     /* odd-power-of-two zeros operator */

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
