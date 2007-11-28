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


#ifndef DECODER_H
#define DECODER_H

#include "Thread.h"

//#define DECODER_INTERNAL_FGETS

class Decoder
{
public:
	enum EKind
	{
	    dcUulib,
	    dcYenc
	};

private:
	static Mutex	m_mutexDecoder;
	static unsigned int		crc_tab[256];
	EKind			m_eKind;
	const char*		m_szSrcFilename;
	const char*		m_szDestFilename;
	char*			m_szArticleFilename;
	int				m_iDebugStatus;
	int				m_iDebugLines;

	bool			DecodeUulib();
	bool			DecodeYenc();
	static void		crc32gentab();
	unsigned long		crc32m(unsigned long startCrc, unsigned char *block, unsigned int length);

public:
	Decoder();
	~Decoder();
	bool			Execute();
	void			SetKind(EKind eKind) { m_eKind = eKind; }
	void			SetSrcFilename(const char* szSrcFilename) { m_szSrcFilename = szSrcFilename; }
	void			SetDestFilename(const char* szDestFilename) { m_szDestFilename = szDestFilename; }
	const char*		GetArticleFilename() { return m_szArticleFilename; }
	void			LogDebugInfo();

	static void		Init();
	static void		Final();
};

#endif
