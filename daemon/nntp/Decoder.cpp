/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "Decoder.h"
#include "Log.h"
#include "Util.h"
#include "YEncode.h"

const char* Decoder::FormatNames[] = { "Unknown", "yEnc", "UU" };

void Decoder::Init()
{
	YEncode::init();

	debug("%s", YEncode::decode_simd ? "SIMD yEnc decoder can be used" : "SIMD yEnc decoder isn't available for this CPU");
	debug("%s", YEncode::crc32_simd ? "SIMD Crc32 routine can be used" : "SIMD Crc32 routine isn't available for this CPU");
	debug("%s", YEncode::inc_crc32_simd ? "SIMD Crc32 (incremental) routine can be used" : "SIMD Crc32 (incremental) routine isn't available for this CPU");

	printf("%s\n", YEncode::decode_simd ? "SIMD yEnc decoder can be used" : "SIMD yEnc decoder isn't available for this CPU");
	printf("%s\n", YEncode::crc32_simd ? "SIMD Crc32 routine can be used" : "SIMD Crc32 routine isn't available for this CPU");
	printf("%s\n", YEncode::inc_crc32_simd ? "SIMD Crc32 (incremental) routine can be used" : "SIMD Crc32 (incremental) routine isn't available for this CPU");
}

void Decoder::Clear()
{
	m_articleFilename.Clear();
}

Decoder::EFormat Decoder::DetectFormat(const char* buffer, int len, bool inBody)
{
	if (!strncmp(buffer, "=ybegin ", 8))
	{
		return efYenc;
	}

	if (inBody && (len == 62 || len == 63) && (buffer[62] == '\n' || buffer[62] == '\r') && *buffer == 'M')
	{
		return efUx;
	}

	if (!strncmp(buffer, "begin ", 6))
	{
		bool ok = true;
		buffer += 6; //strlen("begin ")
		while (*buffer && *buffer != ' ')
		{
			char ch = *buffer++;
			if (ch < '0' || ch > '7')
			{
				ok = false;
				break;
			}
		}
		if (ok)
		{
			return efUx;
		}
	}

	return efUnknown;
}

/**
  * YDecoder: fast implementation of yEnc-Decoder
  */

YDecoder::YDecoder()
{
	Clear();
}

void YDecoder::Clear()
{
	Decoder::Clear();

	m_body = false;
	m_begin = false;
	m_part = false;
	m_end = false;
	m_crc = false;
	m_expectedCRC = 0;
	m_calculatedCRC = 0xFFFFFFFF;
	m_beginPos = 0;
	m_endPos = 0;
	m_size = 0;
	m_endSize = 0;
	m_crcCheck = false;
}

int YDecoder::DecodeBuffer(char* buffer, int len)
{
	if (m_body && !m_end)
	{
		if (!strncmp(buffer, "=yend ", 6))
		{
			m_end = true;
			char* pb = strstr(buffer, m_part ? " pcrc32=" : " crc32=");
			if (pb)
			{
				m_crc = true;
				pb += 7 + (int)m_part; //=strlen(" crc32=") or strlen(" pcrc32=")
				m_expectedCRC = strtoul(pb, nullptr, 16);
			}
			pb = strstr(buffer, " size=");
			if (pb)
			{
				pb += 6; //=strlen(" size=")
				m_endSize = (int64)atoll(pb);
			}
			return 0;
		}

#ifdef SKIP_ARTICLE_DECODING
		char* optr = buffer + len;
#else
		len -= 2;  // strip trailing \r\n
		char* optr = buffer;
		for (int i = 0; i < len; i++)
		{
			char c = buffer[i];
			if (c == '=')
			{
				c = buffer[++i] - 64;
			}
			*optr++ = c - 42;
		}
#endif

		if (m_crcCheck)
		{
			m_calculatedCRC = Util::Crc32m(m_calculatedCRC, (uchar*)buffer, (uint32)(optr - buffer));
		}
		return (int)(optr - buffer);
	}
	else
	{
		if (!m_part && !strncmp(buffer, "=ybegin ", 8))
		{
			m_begin = true;
			char* pb = strstr(buffer, " name=");
			if (pb)
			{
				pb += 6; //=strlen(" name=")
				char* pe;
				for (pe = pb; *pe != '\0' && *pe != '\n' && *pe != '\r'; pe++) ;
				m_articleFilename = WebUtil::Latin1ToUtf8(CString(pb, (int)(pe - pb)));
			}
			pb = strstr(buffer, " size=");
			if (pb)
			{
				pb += 6; //=strlen(" size=")
				m_size = (int64)atoll(pb);
			}
			m_part = strstr(buffer, " part=");
			if (!m_part)
			{
				m_body = true;
				m_beginPos = 1;
				m_endPos = m_size;
			}
		}
		else if (m_part && !strncmp(buffer, "=ypart ", 7))
		{
			m_part = true;
			m_body = true;
			char* pb = strstr(buffer, " begin=");
			if (pb)
			{
				pb += 7; //=strlen(" begin=")
				m_beginPos = (int64)atoll(pb);
			}
			pb = strstr(buffer, " end=");
			if (pb)
			{
				pb += 5; //=strlen(" end=")
				m_endPos = (int64)atoll(pb);
			}
		}
	}

	return 0;
}

Decoder::EStatus YDecoder::Check()
{
#ifdef SKIP_ARTICLE_DECODING
	return dsFinished;
#endif

	m_calculatedCRC ^= 0xFFFFFFFF;

	debug("Expected crc32=%x", m_expectedCRC);
	debug("Calculated crc32=%x", m_calculatedCRC);

	if (!m_begin)
	{
		return dsNoBinaryData;
	}
	else if (!m_end)
	{
		return dsArticleIncomplete;
	}
	else if (!m_part && m_size != m_endSize)
	{
		return dsInvalidSize;
	}
	else if (m_crcCheck && m_crc && (m_expectedCRC != m_calculatedCRC))
	{
		return dsCrcError;
	}

	return dsFinished;
}


/**
  * UDecoder: supports UU encoding formats
  */

UDecoder::UDecoder()
{
	Clear();
}

void UDecoder::Clear()
{
	Decoder::Clear();

	m_body = false;
	m_end = false;
}

/* DecodeBuffer-function uses portions of code from tool UUDECODE by Clem Dye
 * UUDECODE.c (http://www.bastet.com/uue.zip)
 * Copyright (C) 1998 Clem Dye
 *
 * Released under GPL (thanks)
 */

#define UU_DECODE_CHAR(c) (c == '`' ? 0 : (((c) - ' ') & 077))

int UDecoder::DecodeBuffer(char* buffer, int len)
{
	if (!m_body)
	{
		if (!strncmp(buffer, "begin ", 6))
		{
			char* pb = buffer;
			pb += 6; //strlen("begin ")

			// skip file-permissions
			for (; *pb != ' ' && *pb != '\0' && *pb != '\n' && *pb != '\r'; pb++) ;
			pb++;

			// extracting filename
			char* pe;
			for (pe = pb; *pe != '\0' && *pe != '\n' && *pe != '\r'; pe++) ;
			m_articleFilename = WebUtil::Latin1ToUtf8(CString(pb, (int)(pe - pb)));

			m_body = true;
			return 0;
		}
		else if ((len == 62 || len == 63) && (buffer[62] == '\n' || buffer[62] == '\r') && *buffer == 'M')
		{
			m_body = true;
		}
	}

	if (m_body && (!strncmp(buffer, "end ", 4) || *buffer == '`'))
	{
		m_end = true;
	}

	if (m_body && !m_end)
	{
		int effLen = UU_DECODE_CHAR(buffer[0]);
		if (effLen > len)
		{
			// error;
			return 0;
		}

		char* iptr = buffer;
		char* optr = buffer;
		for (++iptr; effLen > 0; iptr += 4, effLen -= 3)
		{
			if (effLen >= 3)
			{
				*optr++ = UU_DECODE_CHAR (iptr[0]) << 2 | UU_DECODE_CHAR (iptr[1]) >> 4;
				*optr++ = UU_DECODE_CHAR (iptr[1]) << 4 | UU_DECODE_CHAR (iptr[2]) >> 2;
				*optr++ = UU_DECODE_CHAR (iptr[2]) << 6 | UU_DECODE_CHAR (iptr[3]);
			}
			else
			{
				if (effLen >= 1)
				{
					*optr++ = UU_DECODE_CHAR (iptr[0]) << 2 | UU_DECODE_CHAR (iptr[1]) >> 4;
				}
				if (effLen >= 2)
				{
					*optr++ = UU_DECODE_CHAR (iptr[1]) << 4 | UU_DECODE_CHAR (iptr[2]) >> 2;
				}
			}
		}

		return (int)(optr - buffer);
	}

	return 0;
}

Decoder::EStatus UDecoder::Check()
{
	if (!m_body)
	{
		return dsNoBinaryData;
	}

	return dsFinished;
}
