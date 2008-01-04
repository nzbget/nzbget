/*
 *  This file if part of nzbget
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

Mutex UULibDecoder::m_mutexDecoder;
unsigned int YDecoder::crc_tab[256];

Decoder::Decoder()
{
	debug("Creating Decoder");

	m_szSrcFilename		= NULL;
	m_szDestFilename	= NULL;
	m_szArticleFilename	= NULL;
	m_bCrcError			= false;
}

Decoder::~ Decoder()
{
	debug("Destroying Decoder");

	if (m_szArticleFilename)
	{
		free(m_szArticleFilename);
	}
}


/*
 * UULibDecoder
 */

bool UULibDecoder::Execute()
{
	bool res = false;

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
				//f**k, multiple attachments!? Can't handle this.
				attachment = NULL;
				break;
			}
		}
	}

	if (attachment)
	{
		// okay, we got only one attachment, perfect!
		if ((attachment->haveparts) && (attachment->haveparts[0])) //  && (!attachment->haveparts[1]))  FUCK UULIB
		{
			int r = UUDecodeFile(attachment, (char*)m_szDestFilename);

			if (r == UURET_OK)
			{
				// we did it!
				res = true;
				m_szArticleFilename = strdup(attachment->filename);
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
	m_bEnd = false;
	m_lExpectedCRC = 0;
	m_lCalculatedCRC = 0xFFFFFFFF;
	m_iBegin = 0;
	m_iEnd = 0;
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
	if (m_bBody)
	{
		if (!strncmp(buffer, "=yend size=", 11))
		{
			m_bEnd = true;
			char* pc = strstr(buffer, "pcrc32=");
			if (pc)
			{
				pc += 7; //=strlen("pcrc32=")
				m_lExpectedCRC = strtoul(pc, NULL, 16);
			}
			return 0;
		}
		char* iptr = buffer;
		char* optr = buffer;
		while (*iptr)
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
				default:	// normal char
					*optr = *iptr - 42;
					optr++;
					break;
			}
			iptr++;
		}
		if (m_bCrcCheck)
		{
			m_lCalculatedCRC = crc32m(m_lCalculatedCRC, (unsigned char *)buffer, optr - buffer);
		}
		return optr - buffer;
	}
	else
	{
		if (!strncmp(buffer, "=ypart begin=", 13))
		{
			m_bBody = true;
			char* pb = strstr(buffer, "begin=");
			if (pb) 
			{
				pb += 6; //=strlen("begin=")
				m_iBegin = (int)atoi(pb);
			}
			pb = strstr(buffer, "end=");
			if (pb) 
			{
				pb += 4; //=strlen("end=")
				m_iEnd = (int)atoi(pb);
			}
		}
		else if (!strncmp(buffer, "=ybegin part=", 13))
		{
			char* pb = strstr(buffer, "name=");
			if (pb)
			{
				pb += 5; //=strlen("name=")
				char* pe;
				for (pe = pb; *pe != '\0' && *pe != '\n' && *pe != '\r'; pe++) ;
				m_szArticleFilename = (char*)malloc(pe - pb + 1);
				strncpy(m_szArticleFilename, pb, pe - pb);
				m_szArticleFilename[pe - pb] = '\0';
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

bool YDecoder::Execute()
{
	m_lCalculatedCRC ^= 0xFFFFFFFF;

	debug("Expected pcrc32=%x", m_lExpectedCRC);
	debug("Calculated pcrc32=%x", m_lCalculatedCRC);
	m_bCrcError = m_bCrcCheck && (m_lExpectedCRC != m_lCalculatedCRC);

	return m_bBody && m_bEnd && !m_bCrcError;
}
