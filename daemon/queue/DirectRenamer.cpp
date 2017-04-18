/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "DirectRenamer.h"
#include "Options.h"

#ifndef DISABLE_PARCHECK
#include "par2cmdline.h"
#include "par2fileformat.h"
#include "md5.h"
#endif

RenameContentAnalyzer::~RenameContentAnalyzer()
{
	Reset();
}

void RenameContentAnalyzer::Reset()
{
#ifndef DISABLE_PARCHECK
	delete (Par2::MD5Context*)m_md5Context;
#endif
	m_md5Context = nullptr;
	m_dataSize = 0;
}

void RenameContentAnalyzer::Append(const void* buffer, int len)
{
#ifndef DISABLE_PARCHECK
	if (!m_md5Context)
	{
		m_md5Context = new Par2::MD5Context();
	}

	if (m_dataSize == 0 && len >= sizeof(Par2::packet_magic) &&
		(*(Par2::MAGIC*)buffer) == Par2::packet_magic)
	{
		m_parFile = true;
	}

	int rem16kSize = std::min(len, 16 * 1024 - m_dataSize);
	if (rem16kSize > 0)
	{
		((Par2::MD5Context*)m_md5Context)->Update(buffer, rem16kSize);
	}

	m_dataSize += len;
#endif
}

// Must be called with locked DownloadQueue
void RenameContentAnalyzer::Finish()
{
#ifndef DISABLE_PARCHECK
	Par2::MD5Hash hash;
	((Par2::MD5Context*)m_md5Context)->Final(hash);

	m_hash16k = hash.print().c_str();
#endif
}
