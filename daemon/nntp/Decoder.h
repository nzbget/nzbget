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


#ifndef DECODER_H
#define DECODER_H

#include "NString.h"

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

	virtual ~Decoder() {}
	virtual EStatus Check() = 0;
	virtual void Clear();
	virtual int DecodeBuffer(char* buffer, int len) = 0;
	const char* GetArticleFilename() { return m_articleFilename; }
	static EFormat DetectFormat(const char* buffer, int len, bool inBody);

protected:
	CString m_articleFilename;
};

class YDecoder: public Decoder
{
public:
	YDecoder();
	virtual EStatus Check();
	virtual void Clear();
	virtual int DecodeBuffer(char* buffer, int len);
	void SetCrcCheck(bool crcCheck) { m_crcCheck = crcCheck; }
	int64 GetBegin() { return m_beginPos; }
	int64 GetEnd() { return m_endPos; }
	int64 GetSize() { return m_size; }
	uint32 GetExpectedCrc() { return m_expectedCRC; }
	uint32 GetCalculatedCrc() { return m_calculatedCRC; }

private:
	bool m_begin;
	bool m_part;
	bool m_body;
	bool m_end;
	bool m_crc;
	uint32 m_expectedCRC;
	uint32 m_calculatedCRC;
	int64 m_beginPos;
	int64 m_endPos;
	int64 m_size;
	int64 m_endSize;
	bool m_crcCheck;
};

class UDecoder: public Decoder
{
public:
	UDecoder();
	virtual EStatus Check();
	virtual void Clear();
	virtual int DecodeBuffer(char* buffer, int len);

private:
	bool m_body;
	bool m_end;
};

#endif
