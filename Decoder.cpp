/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004  Sven Henkel <sidddy@users.sourceforge.net>
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


#ifdef HAVE_CONFIG_H
#include <config.h>
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

#ifdef ENABLE_UULIB
#ifndef PROTOTYPES
#define PROTOTYPES
#endif
#include <uudeview.h>
#endif

#include "Decoder.h"
#include "Log.h"
#include "Util.h"

const char* Decoder::FormatNames[] = { "Unknown", "yEnc", "UU" };
Mutex UULibDecoder::m_mutexDecoder;
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

	if (m_szArticleFilename)
	{
		free(m_szArticleFilename);
	}
}

Decoder::EFormat Decoder::DetectFormat(const char* buffer, int iLen)
{
	if (!strncmp(buffer, "=ybegin ", 8))
	{
		return efYenc;
	}
	
	if ((iLen == 62 || iLen == 63) && (buffer[62] == '\n' || buffer[62] == '\r') && *buffer == 'M')
	{
		return efUu;
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
			return efUu;
		}
	}

	return efUnknown;
}

/*
 * UULibDecoder
 */

Decoder::EStatus UULibDecoder::Execute()
{
	EStatus res = eUnknownError;

#ifndef ENABLE_UULIB
	error("Program was compiled without option ENABLE_UULIB defined. uulib-Decoder is not available.");
#else

	m_mutexDecoder.Lock();

	UUInitialize();

	UUSetOption(UUOPT_DESPERATE, 1, NULL);
	//  UUSetOption(UUOPT_DUMBNESS,1,NULL);
	// UUSetOption( UUOPT_SAVEPATH, 1, szDestDir );

	UULoadFile((char*) m_szSrcFilename, NULL, 0);

	// choose right attachment

	uulist* attachment = NULL;

	for (int i = 0; ; i++)
	{
		uulist* att_tmp = UUGetFileListItem(i);

		if (!att_tmp)
		{
			break;
		}

		if ((att_tmp) && (att_tmp->haveparts))
		{
			if (!attachment)
			{
				attachment = att_tmp;
			}
			else
			{
				// multiple attachments!? Can't handle this.
				attachment = NULL;
				break;
			}
		}
	}

	if (attachment)
	{
		// okay, we got only one attachment, perfect!
		if ((attachment->haveparts) && (attachment->haveparts[0])) //  && (!attachment->haveparts[1]))  
		{
			int r = UUDecodeFile(attachment, (char*)m_szDestFilename);

			if (r == UURET_OK)
			{
				// we did it!
				res = eFinished;
				if (attachment->filename)
				{
					m_szArticleFilename = strdup(attachment->filename);
				}
			}
		}
		else
		{
			error("[ERROR] Wrong number of parts!\n");
		}
	}
	else
	{
		error("[ERROR] Wrong number of attachments!\n");
	}

	UUCleanUp();

	m_mutexDecoder.Unlock();

#endif // ENABLE_UULIB

	return res;
}

/**
  * YDecoder
  * Very primitive (but fast) implementation of yEnc-Decoder
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
	m_bBody = false;
	m_bBegin = false;
	m_bPart = false;
	m_bEnd = false;
	m_bCrc = false;
	m_lExpectedCRC = 0;
	m_lCalculatedCRC = 0xFFFFFFFF;
	m_iBegin = 0;
	m_iEnd = 0;
	m_iSize = 0;
	m_iEndSize = 0;
	m_bAutoSeek = false;
	m_bNeedSetPos = false;
	m_bCrcCheck = false;
	if (m_szArticleFilename)
	{
		free(m_szArticleFilename);
	}
	m_szArticleFilename = NULL;
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
			m_lCalculatedCRC = crc32m(m_lCalculatedCRC, (unsigned char *)buffer, optr - buffer);
		}
		return optr - buffer;
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
				if (m_szArticleFilename)
				{
					free(m_szArticleFilename);
				}
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

bool YDecoder::Write(char* buffer, FILE* outfile)
{
	unsigned int wcnt = DecodeBuffer(buffer);
	if (wcnt > 0)
	{
		if (m_bNeedSetPos)
		{
			if (m_iBegin == 0 || m_iEnd == 0 || !outfile)
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

Decoder::EStatus YDecoder::Execute()
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
