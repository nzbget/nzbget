/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004  Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007  Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include <stdio.h>
#ifndef WIN32
#include <unistd.h>
#endif

#include "nzbget.h"
#include "Decoder.h"
#include "Log.h"
#include "Util.h"

const char* Decoder::FormatNames[] = { "Unknown", "yEnc", "UU" };
unsigned int YDecoder::crc_tab[256];

Decoder::Decoder()
{
	debug("Creating Decoder");

	m_szSrcFilename		= NULL;
	m_szDestFilename	= NULL;
	m_szArticleFilename	= NULL;
}

Decoder::~ Decoder()
{
	debug("Destroying Decoder");

	free(m_szArticleFilename);
}

void Decoder::Clear()
{
	free(m_szArticleFilename);
	m_szArticleFilename = NULL;
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
		bool bOK = true;
		buffer += 6; //strlen("begin ")
		while (*buffer && *buffer != ' ')
		{
			char ch = *buffer++;
			if (ch < '0' || ch > '7')
			{
				bOK = false;
				break;
			}
		}
		if (bOK)
		{
			return efUx;
		}
	}

	return efUnknown;
}

/**
  * YDecoder: fast implementation of yEnc-Decoder
  */

void YDecoder::Init()
{
	debug("Initializing global decoder");
	crc32gentab();
}

void YDecoder::Final()
{
	debug("Finalizing global Decoder");
}

YDecoder::YDecoder()
{
	Clear();
}

void YDecoder::Clear()
{
	Decoder::Clear();

	m_bBody = false;
	m_bBegin = false;
	m_bPart = false;
	m_bEnd = false;
	m_bCrc = false;
	m_lExpectedCRC = 0;
	m_lCalculatedCRC = 0xFFFFFFFF;
	m_iBegin = 0;
	m_iEnd = 0xFFFFFFFF;
	m_iSize = 0;
	m_iEndSize = 0;
	m_bAutoSeek = false;
	m_bNeedSetPos = false;
	m_bCrcCheck = false;
}

/* from crc32.c (http://www.koders.com/c/fid699AFE0A656F0022C9D6B9D1743E697B69CE5815.aspx)
 *
 * (c) 1999,2000 Krzysztof Dabrowski
 * (c) 1999,2000 ElysiuM deeZine
 * Released under GPL (thanks)
 *
 * chksum_crc32gentab() --      to a global crc_tab[256], this one will
 *				calculate the crcTable for crc32-checksums.
 *				it is generated to the polynom [..]
 */
void YDecoder::crc32gentab()
{
	unsigned long crc, poly;
	int i, j;

	poly = 0xEDB88320L;
	for (i = 0; i < 256; i++)
	{
		crc = i;
		for (j = 8; j > 0; j--)
		{
			if (crc & 1)
			{
				crc = (crc >> 1) ^ poly;
			}
			else
			{
				crc >>= 1;
			}
		}
		crc_tab[i] = crc;
	}
}

/* This is modified version of chksum_crc() from
 * crc32.c (http://www.koders.com/c/fid699AFE0A656F0022C9D6B9D1743E697B69CE5815.aspx)
 * (c) 1999,2000 Krzysztof Dabrowski
 * (c) 1999,2000 ElysiuM deeZine
 *
 * chksum_crc() -- to a given block, this one calculates the
 *				crc32-checksum until the length is
 *				reached. the crc32-checksum will be
 *				the result.
 */
unsigned long YDecoder::crc32m(unsigned long startCrc, unsigned char *block, unsigned int length)
{
	register unsigned long crc = startCrc;
	for (unsigned long i = 0; i < length; i++)
	{
		crc = ((crc >> 8) & 0x00FFFFFF) ^ crc_tab[(crc ^ *block++) & 0xFF];
	}
	return crc;
}

unsigned int YDecoder::DecodeBuffer(char* buffer)
{
	if (m_bBody && !m_bEnd)
	{
		if (!strncmp(buffer, "=yend ", 6))
		{
			m_bEnd = true;
			char* pb = strstr(buffer, m_bPart ? " pcrc32=" : " crc32=");
			if (pb)
			{
				m_bCrc = true;
				pb += 7 + (int)m_bPart; //=strlen(" crc32=") or strlen(" pcrc32=")
				m_lExpectedCRC = strtoul(pb, NULL, 16);
			}
			pb = strstr(buffer, " size=");
			if (pb) 
			{
				pb += 6; //=strlen(" size=")
				m_iEndSize = (int)atoi(pb);
			}
			return 0;
		}

		char* iptr = buffer;
		char* optr = buffer;
		while (true)
		{
			switch (*iptr)
			{
				case '=':	//escape-sequence
					iptr++;
					*optr = *iptr - 64 - 42;
					optr++;
					break;
				case '\n':	// ignored char
				case '\r':	// ignored char
					break;
				case '\0':
					goto BreakLoop;
				default:	// normal char
					*optr = *iptr - 42;
					optr++;
					break;
			}
			iptr++;
		}
BreakLoop:

		if (m_bCrcCheck)
		{
			m_lCalculatedCRC = crc32m(m_lCalculatedCRC, (unsigned char *)buffer, (unsigned int)(optr - buffer));
		}
		return (unsigned int)(optr - buffer);
	}
	else 
	{
		if (!m_bPart && !strncmp(buffer, "=ybegin ", 8))
		{
			m_bBegin = true;
			char* pb = strstr(buffer, " name=");
			if (pb)
			{
				pb += 6; //=strlen(" name=")
				char* pe;
				for (pe = pb; *pe != '\0' && *pe != '\n' && *pe != '\r'; pe++) ;
				free(m_szArticleFilename);
				m_szArticleFilename = (char*)malloc(pe - pb + 1);
				strncpy(m_szArticleFilename, pb, pe - pb);
				m_szArticleFilename[pe - pb] = '\0';
			}
			pb = strstr(buffer, " size=");
			if (pb) 
			{
				pb += 6; //=strlen(" size=")
				m_iSize = (int)atoi(pb);
			}
			m_bPart = strstr(buffer, " part=");
			if (!m_bPart)
			{
				m_bBody = true;
				m_iBegin = 1;
				m_iEnd = m_iSize;
			}
		}
		else if (m_bPart && !strncmp(buffer, "=ypart ", 7))
		{
			m_bPart = true;
			m_bBody = true;
			char* pb = strstr(buffer, " begin=");
			if (pb) 
			{
				pb += 7; //=strlen(" begin=")
				m_iBegin = (int)atoi(pb);
			}
			pb = strstr(buffer, " end=");
			if (pb) 
			{
				pb += 5; //=strlen(" end=")
				m_iEnd = (int)atoi(pb);
			}
		}
	}

	return 0;
}

bool YDecoder::Write(char* buffer, int len, FILE* outfile)
{
	unsigned int wcnt = DecodeBuffer(buffer);
	if (wcnt > 0)
	{
		if (m_bNeedSetPos)
		{
			if (m_iBegin == 0 || m_iEnd == 0xFFFFFFFF || !outfile)
			{
				return false;
			}
			if (fseek(outfile, m_iBegin - 1, SEEK_SET))
			{
				return false;
			}
			m_bNeedSetPos = false;
		}
		fwrite(buffer, 1, wcnt, outfile);
	}
	return true;
}

Decoder::EStatus YDecoder::Check()
{
	m_lCalculatedCRC ^= 0xFFFFFFFF;

	debug("Expected crc32=%x", m_lExpectedCRC);
	debug("Calculated crc32=%x", m_lCalculatedCRC);

	if (!m_bBegin)
	{
		return eNoBinaryData;
	}
	else if (!m_bEnd)
	{
		return eArticleIncomplete;
	}
	else if (!m_bPart && m_iSize != m_iEndSize)
	{
		return eInvalidSize;
	}
	else if (m_bCrcCheck && m_bCrc && (m_lExpectedCRC != m_lCalculatedCRC))
	{
		return eCrcError;
	}

	return eFinished;
}


/**
  * UDecoder: supports UU encoding formats
  */

UDecoder::UDecoder()
{

}

void UDecoder::Clear()
{
	Decoder::Clear();

	m_bBody = false;
	m_bEnd = false;
}

/* DecodeBuffer-function uses portions of code from tool UUDECODE by Clem Dye
 * UUDECODE.c (http://www.bastet.com/uue.zip)
 * Copyright (C) 1998 Clem Dye
 *
 * Released under GPL (thanks)
 */

#define UU_DECODE_CHAR(c) (c == '`' ? 0 : (((c) - ' ') & 077))

unsigned int UDecoder::DecodeBuffer(char* buffer, int len)
{
	if (!m_bBody)
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
			free(m_szArticleFilename);
			m_szArticleFilename = (char*)malloc(pe - pb + 1);
			strncpy(m_szArticleFilename, pb, pe - pb);
			m_szArticleFilename[pe - pb] = '\0';

			m_bBody = true;
			return 0;
		}
		else if ((len == 62 || len == 63) && (buffer[62] == '\n' || buffer[62] == '\r') && *buffer == 'M')
		{
			m_bBody = true;
		}
	}

	if (m_bBody && (!strncmp(buffer, "end ", 4) || *buffer == '`'))
	{
		m_bEnd = true;
	}

	if (m_bBody && !m_bEnd)
	{
		int iEffLen = UU_DECODE_CHAR(buffer[0]);
		if (iEffLen > len)
		{
			// error;
			return 0;
		}

		char* iptr = buffer;
		char* optr = buffer;
		for (++iptr; iEffLen > 0; iptr += 4, iEffLen -= 3)
		{
			if (iEffLen >= 3)
			{
				*optr++ = UU_DECODE_CHAR (iptr[0]) << 2 | UU_DECODE_CHAR (iptr[1]) >> 4; 
				*optr++ = UU_DECODE_CHAR (iptr[1]) << 4 | UU_DECODE_CHAR (iptr[2]) >> 2; 
				*optr++ = UU_DECODE_CHAR (iptr[2]) << 6 | UU_DECODE_CHAR (iptr[3]);
			}
			else
			{
				if (iEffLen >= 1)
				{
					*optr++ = UU_DECODE_CHAR (iptr[0]) << 2 | UU_DECODE_CHAR (iptr[1]) >> 4; 
				}
				if (iEffLen >= 2)
				{
					*optr++ = UU_DECODE_CHAR (iptr[1]) << 4 | UU_DECODE_CHAR (iptr[2]) >> 2; 
				}
			}
		}

		return (unsigned int)(optr - buffer);
	}

	return 0;
}

bool UDecoder::Write(char* buffer, int len, FILE* outfile)
{
	unsigned int wcnt = DecodeBuffer(buffer, len);
	if (wcnt > 0)
	{
		fwrite(buffer, 1, wcnt, outfile);
	}
	return true;
}

Decoder::EStatus UDecoder::Check()
{
	if (!m_bBody)
	{
		return eNoBinaryData;
	}

	return eFinished;
}
