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

//#define USEEXTERNALDECODER	// not working
//#define DEBUGDECODER

#include "Decoder.h"
#include "Log.h"
#include "Options.h"
#include "Util.h"

extern Options* g_pOptions;

#ifdef DEBUGDECODER
int g_iDecoderID = 0;
#endif

Mutex Decoder::m_mutexDecoder;
unsigned int Decoder::crc_tab[256];

void Decoder::Init()
{
	debug("Initializing global decoder");
	crc32gentab();
}

void Decoder::Final()
{
	debug("Finalizing global Decoder");
}

Decoder::Decoder()
{
	debug("Creating Decoder");

	m_szSrcFilename		= NULL;
	m_szDestFilename	= NULL;
	m_szArticleFilename	= NULL;
	m_eKind				= dcYenc;
	m_iDebugStatus		= 0;
	m_iDebugLines		= 0;
}

Decoder::~ Decoder()
{
	debug("Destroying Decoder");

	if (m_szArticleFilename)
	{
		free(m_szArticleFilename);
	}
}

bool Decoder::Execute()
{
	if (m_eKind == dcUulib)
	{
		return DecodeUulib();
	}
	else
	{
		return DecodeYenc();
	}
}

bool Decoder::DecodeUulib()
{
	bool res = false;

#ifndef ENABLE_UULIB
	error("Program was compiled without option ENABLE_UULIB defined. uulib-Decoder is not available.");
#else

	m_mutexDecoder.Lock();

#ifdef DEBUGDECODER
	debug("Decoding ID %i (%s)", g_iDecoderID, szSrcFilename);
#endif

#ifndef USEEXTERNALDECODER
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
#else
	execl("/usr/local/bin", "uudeview", szSrcFilename, szDestFilename);
#endif

#ifdef DEBUGDECODER
	debug("Finished decoding ID %i (%s)", g_iDecoderID++, szDestFilename);
#endif

	m_mutexDecoder.Unlock();

#endif // ENABLE_UULIB

	return res;
}

/**
  * Very primitive (but fast) implementation of yEnc-Decoder
  */
bool Decoder::DecodeYenc()
{
	FILE* infile = fopen(m_szSrcFilename, "r");
	if (!infile)
	{
		error("Could not open file \"%s\"", m_szSrcFilename);
		return false;
	}

	FILE* outfile = fopen(m_szDestFilename, "w");
	if (!outfile)
	{
		error("Could not create file \"%s\"", m_szDestFilename);
		fclose(infile);
		return false;
	}

	static const int MAX_LINE_LEN = 1024;
	char buffer[MAX_LINE_LEN];
	bool body = false;
	bool end = false;
	unsigned long expectedCRC = 0;
	unsigned long calculatedCRC = 0xFFFFFFFF;
	m_iDebugStatus = 1;
	bool eof = !fgets(buffer, sizeof(buffer), infile);
	m_iDebugLines++;
	m_iDebugStatus = 2;
	while (!eof)
	{
		if (body)
		{
			if (strstr(buffer, "=yend size="))
			{
				end = true;
				m_iDebugStatus = 3;
				char* pc = strstr(buffer, "pcrc32=");
				if (pc)
				{
					pc += 7; //=strlen("pcrc32=")
					expectedCRC = strtoul(pc, NULL, 16);
				}
				break;
			}
			m_iDebugStatus = 4;
			char* iptr = buffer;
			char* optr = buffer;
			while (*iptr)
			{
				switch (*iptr)
				{
					case '=':	//escape-sequence
						iptr++;
						*optr = *iptr - 64 - 42;
						*optr++;
						break;
					case '\n':	// ignored char
					case '\r':	// ignored char
						break;
					default:	// normal char
						*optr = *iptr - 42;
						*optr++;
						break;
				}
				iptr++;
			}
			m_iDebugStatus = 5;
			calculatedCRC = crc32m(calculatedCRC, (unsigned char *)buffer, optr - buffer);
			fwrite(buffer, 1, optr - buffer, outfile);
			m_iDebugStatus = 6;
		}
		else
		{
			if (strstr(buffer, "=ypart begin="))
			{
				m_iDebugStatus = 7;
				body = true;
			}
			else if (strstr(buffer, "=ybegin part="))
			{
				m_iDebugStatus = 8;
				char* pb = strstr(buffer, "name=");
				if (pb)
				{
					m_iDebugStatus = 9;
					pb += 5; //=strlen("name=")
					char* pe;
					for (pe = pb; *pe != '\0' && *pe != '\n' && *pe != '\r'; pe++) ;
					m_szArticleFilename = (char*)malloc(pe - pb + 1);
					strncpy(m_szArticleFilename, pb, pe - pb);
					m_szArticleFilename[pe - pb] = '\0';
					m_iDebugStatus = 10;
				}
				m_iDebugStatus = 11;
			}
		}
		m_iDebugStatus = 12;
		eof = !fgets(buffer, sizeof(buffer), infile);
		m_iDebugStatus = 13;
		m_iDebugLines++;
	}
	m_iDebugStatus = 14;

	calculatedCRC ^= 0xFFFFFFFF;

	debug("Expected pcrc32=%x", expectedCRC);
	debug("Calculated pcrc32=%x", calculatedCRC);
	bool CrcOK = expectedCRC == calculatedCRC;
	if (!CrcOK)
	{
		warn("CRC-Error for \"%s\"", m_szDestFilename);
	}

	fclose(infile);
	fclose(outfile);

	return body && end && (CrcOK || !g_pOptions->GetRetryOnCrcError());
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
void Decoder::crc32gentab()
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
unsigned long Decoder::crc32m(unsigned long startCrc, unsigned char *block, unsigned int length)
{
	register unsigned long crc;
	unsigned long i;

	crc = startCrc;
	for (i = 0; i < length; i++)
	{
		crc = ((crc >> 8) & 0x00FFFFFF) ^ crc_tab[(crc ^ *block++) & 0xFF];
	}
	return crc;
}

void Decoder::LogDebugInfo()
{
	debug("        Decoder: status=%i, lines=%i, filename=%s, ArticleFileName=%s",
	      m_iDebugStatus, m_iDebugLines, BaseFileName(m_szSrcFilename), m_szArticleFilename);
}
