/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef DECODER_H
#define DECODER_H

class Decoder
{
public:
	enum EStatus
	{
		eUnknownError,
		eFinished,
		eArticleIncomplete,
		eCrcError,
		eInvalidSize,
		eNoBinaryData
	};

	enum EFormat
	{
		efUnknown,
		efYenc,
		efUx,
	};

	static const char* FormatNames[];

protected:
	char*					m_szArticleFilename;

public:
							Decoder();
	virtual					~Decoder();
	virtual EStatus			Check() = 0;
	virtual void			Clear();
	virtual int				DecodeBuffer(char* buffer, int len) = 0;
	const char*				GetArticleFilename() { return m_szArticleFilename; }
	static EFormat			DetectFormat(const char* buffer, int len);
};

class YDecoder: public Decoder
{
protected:
	static unsigned int		crc_tab[256];
	bool					m_bBegin;
	bool					m_bPart;
	bool					m_bBody;
	bool					m_bEnd;
	bool					m_bCrc;
	unsigned long			m_lExpectedCRC;
	unsigned long			m_lCalculatedCRC;
	unsigned long			m_iBegin;
	unsigned long			m_iEnd;
	unsigned long			m_iSize;
	unsigned long			m_iEndSize;
	bool					m_bCrcCheck;

	static void				crc32gentab();
	unsigned long			crc32m(unsigned long startCrc, unsigned char *block, unsigned int length);

public:
							YDecoder();
	virtual EStatus			Check();
	virtual void			Clear();
	virtual int				DecodeBuffer(char* buffer, int len);
	void					SetCrcCheck(bool bCrcCheck) { m_bCrcCheck = bCrcCheck; }
	unsigned long			GetBegin() { return m_iBegin; }
	unsigned long			GetEnd() { return m_iEnd; }
	unsigned long			GetSize() { return m_iSize; }

	static void				Init();
	static void				Final();
};

class UDecoder: public Decoder
{
private:
	bool					m_bBody;
	bool					m_bEnd;

public:
							UDecoder();
	virtual EStatus			Check();
	virtual void			Clear();
	virtual int				DecodeBuffer(char* buffer, int len);
};

#endif
