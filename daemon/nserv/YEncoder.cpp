/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2016-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#include "nzbget.h"
#include "YEncoder.h"
#include "Util.h"
#include "FileSystem.h"
#include "Log.h"

bool YEncoder::OpenFile(CString& errmsg)
{
	if (m_size < 0)
	{
		errmsg = "Invalid segment size";
		return false;
	}

	if (!m_diskfile.Open(m_filename, DiskFile::omRead) || !m_diskfile.Seek(0, DiskFile::soEnd))
	{
		errmsg = "File not found";
		return false;
	}

	m_fileSize = m_diskfile.Position();
	if (m_size == 0)
	{
		m_size = (int)(m_fileSize - m_offset + 1);
	}

	if (m_fileSize < m_offset + m_size)
	{
		errmsg = "Invalid segment size";
		return false;
	}

	if (!m_diskfile.Seek(m_offset))
	{
		errmsg = "Invalid segment offset";
		return false;
	}

	return true;
}

void YEncoder::WriteSegment()
{
	StringBuilder outbuf;
	outbuf.Reserve(std::max(2048, std::min((int)(m_size * 1.1), 16 * 1024 * 1024)));

	outbuf.Append(CString::FormatStr("=ybegin part=%i line=128 size=%" PRIi64 " name=%s\r\n", m_part, m_fileSize, FileSystem::BaseFileName(m_filename)));
	outbuf.Append(CString::FormatStr("=ypart begin=%" PRIi64 " end=%" PRIi64 "\r\n", m_offset + 1, m_offset + m_size));

	Crc32 crc;
	CharBuffer inbuf(std::min(m_size, 16 * 1024 * 1024));
	int lnsz = 0;
	char* out = (char*)outbuf + outbuf.Length();

	while (m_diskfile.Position() < m_offset + m_size)
	{
		int64 needBytes = std::min((int64)inbuf.Size(), m_offset + m_size - m_diskfile.Position());
		int64 readBytes = m_diskfile.Read(inbuf, needBytes);
		bool lastblock = m_diskfile.Position() == m_offset + m_size;
		if (readBytes == 0)
		{
			return; // error;
		}

		crc.Append((uchar*)(const char*)inbuf, (int)readBytes);

		char* in = inbuf;
		while (readBytes > 0)
		{
			char ch = *in++;
			readBytes--;
			ch = (char)(((uchar)(ch) + 42) % 256);
			if (ch == '\0' || ch == '\n' || ch == '\r' || ch == '=' || ch == ' ' || ch == '\t')
			{
				*out++ = '=';
				lnsz++;
				ch = (char)(((uchar)ch + 64) % 256);
			}
			if (ch == '.' && lnsz == 0)
			{
				*out++ = '.';
				lnsz++;
			}
			*out++ = ch;
			lnsz++;

			if (lnsz >= 128 || (readBytes == 0 && lastblock))
			{
				*out++ = '\r';
				*out++ = '\n';
				lnsz += 2;
				outbuf.SetLength(outbuf.Length() + lnsz);

				if (outbuf.Length() > outbuf.Capacity() - 200)
				{
					m_writeFunc(outbuf, outbuf.Length());
					outbuf.SetLength(0);
					out = (char*)outbuf;
				}

				lnsz = 0;
			}
		}
	}
	m_diskfile.Close();

	outbuf.Append(CString::FormatStr("=yend size=%i part=0 pcrc32=%08x\r\n", m_size, (unsigned int)crc.Finish()));
	m_writeFunc(outbuf, outbuf.Length());
}
