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
	m_outSize = 0;
	m_state = 0;
	m_crcCheck = false;
	m_lineBuf.Reserve(1024*8);
	m_lineBuf.SetLength(0);
}

/* At the beginning of article the processing goes line by line to find '=ybegin'-marker.
 * Once the yEnc-data is started switches to blockwise processing.
 * At the end of yEnc-data switches back to line by line mode to
 * process '=yend'-marker and EOF-marker.
 * UU-encoded articles are processed completely in line by line mode.
 */
int Decoder::DecodeBuffer(char* buffer, int len)
{
	if (m_rawMode)
	{
		ProcessRaw(buffer, len);
		return len;
	}

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

int Decoder::DecodeYenc(char* buffer, char* outbuf, int len)
{
	const unsigned char* src = (unsigned char*)buffer;
	unsigned char* dst = (unsigned char*)outbuf;

	int endseq = YEncode::decode(&src, &dst, len, (YEncode::YencDecoderState*)&m_state);
	int outlen = (int)((char*)dst - outbuf);

	// endseq:
	//   0: no end sequence found
	//   1: \r\n=y sequence found, src points to byte after 'y'
	//   2: \r\n.\r\n sequence found, src points to byte after last '\n'
	if (endseq != 0)
	{
		// switch back to line mode to process '=yend'- or eof- marker
		m_lineBuf.SetLength(0);
		m_lineBuf.Append(endseq == 1 ? "=y" : ".\r\n");
		int rem = len - (int)((const char*)src - buffer);
		if (rem > 0)
		{
			m_lineBuf.Append((const char*)src, rem);
		}
		m_body = false;
	}

	if (m_crcCheck)
	{
		m_crc32.Append((uchar*)outbuf, (uint32)outlen);
	}

	m_outSize += outlen;

	return outlen;
}

Decoder::EStatus Decoder::Check()
{
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
	else if ((!m_part && m_size != m_endSize) || (m_endSize != m_outSize))
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
				*optr++ = UU_DECODE_CHAR (iptr[0]) << 2 | UU_DECODE_CHAR (iptr[1]) >> 4;
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

void Decoder::ProcessRaw(char* buffer, int len)
{
	switch (m_state)
	{
		case 1:
			m_eof = len >= 4 && buffer[0] == '\n' &&
				buffer[1] == '.' && buffer[2] == '\r' && buffer[3] == '\n';
			break;

		case 2:
			m_eof = len >= 3 && buffer[0] == '.' && buffer[1] == '\r' && buffer[2] == '\n';
			break;

		case 3:
			m_eof = len >= 2 && buffer[0] == '\r' && buffer[1] == '\n';
			break;

		case 4:
			m_eof = len >= 1 && buffer[0] == '\n';
			break;
	}

	m_eof |= len >= 5 && strstr(buffer, "\r\n.\r\n");

	if (len >= 4 && buffer[len-4] == '\r' && buffer[len-3] == '\n' &&
		buffer[len-2] == '.' && buffer[len-1] == '\r')
	{
		m_state = 4;
	}
	else if (len >= 3 && buffer[len-3] == '\r' && buffer[len-2] == '\n' && buffer[len-1] == '.')
	{
		m_state = 3;
	}
	else if (len >= 2 && buffer[len-2] == '\r' && buffer[len-1] == '\n')
	{
		m_state = 2;
	}
	else if (len >= 1 && buffer[len-1] == '\r')
	{
		m_state = 1;
	}
	else
	{
		m_state = 0;
	}
}
