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
#include "Decoder.h"
#include "Log.h"
#include "Util.h"
#include "YEncode.h"

Decoder::Decoder()
{
	debug("%s", YEncode::decode_simd ? "SIMD yEnc decoder can be used" : "SIMD yEnc decoder isn't available for this CPU");
	debug("%s", YEncode::crc_simd ? "SIMD Crc routine can be used" : "SIMD Crc routine isn't available for this CPU");

	Clear();
}

void Decoder::Clear()
{
	m_articleFilename.Clear();
	m_body = false;
	m_begin = false;
	m_part = false;
	m_end = false;
	m_crc = false;
	m_eof = false;
	m_expectedCRC = 0;
	m_crc32.Reset();
	m_beginPos = 0;
	m_endPos = 0;
	m_size = 0;
	m_endSize = 0;
	m_state = 0;
	m_crcCheck = false;
	m_lineBuf.Reserve(1024*8);
	m_lineBuf.SetLength(0);
	m_extraChar = '\0';
	m_lastChar1 = '\0';
	m_lastChar2 = '\0';
}

/* At the beginning of article the processing goes line by line to find '=ybegin'-marker.
 * Once the yEnc-data is started switches to blockwise processing.
 * At the end of yEnc-data switches back to line by line mode to
 * process '=yend'-marker and EOF-marker.
 * UU-encoded articles are processed completely in line by line mode.
 */
int Decoder::DecodeBuffer(char* buffer, int len)
{
	int outlen = 0;

	if (m_body && m_format == efYenc)
	{
		outlen = DecodeYenc(buffer, buffer, len);
		if (m_body)
		{
			return outlen;
		}
	}
	else
	{
		m_lineBuf.Append(buffer, len);
	}

	char* line = (char*)m_lineBuf;
	while (char* end = strchr(line, '\n'))
	{
		int llen = (int)(end - line + 1);

		if (line[0] == '.' && line[1] == '\r')
		{
			m_eof = true;
			m_lineBuf.SetLength(0);
			return outlen;
		}

		if (m_format == efUnknown)
		{
			m_format = DetectFormat(line, llen);
		}

		if (m_format == efYenc)
		{
			ProcessYenc(line, llen);
			if (m_body)
			{
				outlen = DecodeYenc(end + 1, buffer, m_lineBuf.Length() - (int)(end + 1 - m_lineBuf));
				if (m_body)
				{
					m_lineBuf.SetLength(0);
					return outlen;
				}
				line = (char*)m_lineBuf;
				continue;
			}
		}
		else if (m_format == efUx)
		{
			outlen += DecodeUx(line, llen);
		}

		line = end + 1;
	}

	if (*line)
	{
		len = m_lineBuf.Length() - (int)(line - m_lineBuf);
		memmove((char*)m_lineBuf, line, len);
		m_lineBuf.SetLength(len);
	}
	else
	{
		m_lineBuf.SetLength(0);
	}

	return outlen;
}

Decoder::EFormat Decoder::DetectFormat(const char* buffer, int len)
{
	if (!strncmp(buffer, "=ybegin ", 8))
	{
		return efYenc;
	}

	if ((len == 62 || len == 63) && (buffer[62] == '\n' || buffer[62] == '\r') && *buffer == 'M')
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

void Decoder::ProcessYenc(char* buffer, int len)
{
	if (!strncmp(buffer, "=ybegin ", 8))
	{
		m_begin = true;
		char* pb = strstr(buffer, " name=");
		if (pb)
		{
			pb += 6; //=strlen(" name=")
			char* pe;
			for (pe = pb; *pe != '\0' && *pe != '\n' && *pe != '\r'; pe++);
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
	else if (!strncmp(buffer, "=ypart ", 7))
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
	else if (!strncmp(buffer, "=yend ", 6))
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
	}
}

// find end of yEnc-data or article data
char* Decoder::FindStreamEnd(char* buffer, int len)
{
	// 0: previous characters are '\r\n' OR there is no previous character
	if (m_state == 0 && len > 1 &&
		((buffer[0] == '=' && buffer[1] == 'y') ||
		 (buffer[0] == '.' && buffer[1] == '\r')))
	{
		return buffer;
	}
	// 1: previous character is '='
	if (m_state == 1 && buffer[0] == 'y')
	{
		m_extraChar = '=';
		return buffer;
	}
	// 2: previous character is '\r'
	if (m_state == 2 && len > 2 && buffer[0] == '\n' &&
		((buffer[1] == '=' && buffer[2] == 'y') ||
		 (buffer[1] == '.' && buffer[2] == '\r')))
	{
		return buffer + 1;
	}

	// previous characters are '\n.'
	if (m_lastChar2 == '\n' && m_lastChar1 == '.' && buffer[0] == '\r')
	{
		m_extraChar = '.';
		return buffer;
	}

	char* last = buffer + len - 1;
	char* line = buffer;
	int llen = len;
	while (char* end = (char*)memchr(line, '\n', llen))
	{
		if (end + 2 <= last &&
			((end[1] == '=' && end[2] == 'y') ||
			 (end[1] == '.' && end[2] == '\r')))
		{
			return end + 1;
		}
		llen -= (int)(end - line) + 1;
		line = end + 1;
	}

	// save last two characters for future use
	m_lastChar1 = buffer[len - 1];
	if (len > 1)
	{
		m_lastChar2 = buffer[len - 2];
	}

	return  nullptr;
}

int Decoder::DecodeYenc(char* buffer, char* outbuf, int len)
{
	int inpLen = len;
	char* end = FindStreamEnd(buffer, len);
	if (end)
	{
		len = (int)(end - buffer);
	}

#ifdef SKIP_ARTICLE_DECODING
	m_state = m_lastChar2 == '\r' && m_lastChar1 == '\n' ? 0 :
		m_lastChar1 == '=' ? 1 : m_lastChar1 == '\r' ? 2 : 3;
#else
	len = (int)YEncode::decode((const uchar*)buffer, (uchar*)outbuf, len, &m_state);
#endif

	if (end)
	{
		// switch back to line mode to process '=yend'- or eof- marker
		m_lineBuf.SetLength(0);
		if (m_extraChar)
		{
			m_lineBuf.Append(&m_extraChar, 1);
		}
		m_lineBuf.Append(end, inpLen - (int)(end - buffer));
		m_body = false;
	}

	if (m_crcCheck)
	{
		m_crc32.Append((uchar*)outbuf, (uint32)len);
	}

	return len;
}

Decoder::EStatus Decoder::Check()
{
#ifdef SKIP_ARTICLE_DECODING
	return dsFinished;
#endif

	switch (m_format)
	{
		case efYenc:
			return CheckYenc();
			 
		case efUx:
			return CheckUx();

		default:
			return dsUnknownError;
	}
}

Decoder::EStatus Decoder::CheckYenc()
{
	m_calculatedCRC = m_crc32.Finish();

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


/* DecodeUx-function uses portions of code from tool UUDECODE by Clem Dye
 * UUDECODE.c (http://www.bastet.com/uue.zip)
 * Copyright (C) 1998 Clem Dye
 *
 * Released under GPL (thanks)
 */

#define UU_DECODE_CHAR(c) (c == '`' ? 0 : (((c) - ' ') & 077))

int Decoder::DecodeUx(char* buffer, int len)
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

Decoder::EStatus Decoder::CheckUx()
{
	if (!m_body)
	{
		return dsNoBinaryData;
	}

	return dsFinished;
}
