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


#ifndef DECODER_H
#define DECODER_H

#include "NString.h"
#include "Util.h"

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

	Decoder();
	EStatus Check();
	void Clear();
	int DecodeBuffer(char* buffer, int len);
	void SetCrcCheck(bool crcCheck) { m_crcCheck = crcCheck; }
	void SetRawMode(bool rawMode) { m_rawMode = rawMode; }
	EFormat GetFormat() { return m_format; }
	int64 GetBeginPos() { return m_beginPos; }
	int64 GetEndPos() { return m_endPos; }
	int64 GetSize() { return m_size; }
	uint32 GetExpectedCrc() { return m_expectedCRC; }
	uint32 GetCalculatedCrc() { return m_calculatedCRC; }
	bool GetEof() { return m_eof; }
	const char* GetArticleFilename() { return m_articleFilename; }

private: 
	EFormat m_format = efUnknown;
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
	int64 m_outSize;
	bool m_eof;
	bool m_crcCheck;
	char m_state;
	bool m_rawMode = false;
	CString m_articleFilename;
	StringBuilder m_lineBuf;
	Crc32 m_crc32;

	EFormat DetectFormat(const char* buffer, int len);
	void ProcessYenc(char* buffer, int len);
	int DecodeYenc(char* buffer, char* outbuf, int len);
	EStatus CheckYenc();
	int DecodeUx(char* buffer, int len);
	EStatus CheckUx();
	void ProcessRaw(char* buffer, int len);
};

#endif
