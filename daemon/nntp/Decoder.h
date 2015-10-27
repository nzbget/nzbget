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
		dsUnknownError,
		dsFinished,
		dsArticleIncomplete,
		dsCrcError,
		dsInvalidSize,
		dsNoBinaryData
	};

	enum EFormat
	{
		efUnknown,
		efYenc,
		efUx,
	};

	static const char* FormatNames[];

protected:
	char*					m_articleFilename;

public:
							Decoder();
	virtual					~Decoder();
	virtual EStatus			Check() = 0;
	virtual void			Clear();
	virtual int				DecodeBuffer(char* buffer, int len) = 0;
	const char*				GetArticleFilename() { return m_articleFilename; }
	static EFormat			DetectFormat(const char* buffer, int len);
};

class YDecoder: public Decoder
{
protected:
	bool					m_begin;
	bool					m_part;
	bool					m_body;
	bool					m_end;
	bool					m_crc;
	unsigned long			m_expectedCRC;
	unsigned long			m_calculatedCRC;
	long long				m_beginPos;
	long long				m_endPos;
	long long				m_size;
	long long				m_endSize;
	bool					m_crcCheck;

public:
							YDecoder();
	virtual EStatus			Check();
	virtual void			Clear();
	virtual int				DecodeBuffer(char* buffer, int len);
	void					SetCrcCheck(bool crcCheck) { m_crcCheck = crcCheck; }
	long long				GetBegin() { return m_beginPos; }
	long long				GetEnd() { return m_endPos; }
	long long				GetSize() { return m_size; }
	unsigned long			GetExpectedCrc() { return m_expectedCRC; }
	unsigned long			GetCalculatedCrc() { return m_calculatedCRC; }
};

class UDecoder: public Decoder
{
private:
	bool					m_body;
	bool					m_end;

public:
							UDecoder();
	virtual EStatus			Check();
	virtual void			Clear();
	virtual int				DecodeBuffer(char* buffer, int len);
};

#endif
