/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef YENCODER_H
#define YENCODER_H

#include "NString.h"
#include "FileSystem.h"

class YEncoder
{
public:
	typedef std::function<void(const char* buf, int size)> WriteFunc;

	YEncoder(const char* filename, int part, int64 offset, int size, WriteFunc writeFunc) :
		m_filename(filename), m_part(part), m_offset(offset), m_size(size), m_writeFunc(writeFunc) {};
	bool OpenFile(CString& errmsg);
	void WriteSegment();

private:
	DiskFile m_diskfile;
	CString m_filename;
	int m_part;
	int64 m_offset;
	int m_size;
	int64 m_fileSize;
	WriteFunc m_writeFunc;
};

#endif
