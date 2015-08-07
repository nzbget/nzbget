/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef DUPEMATCHER_H
#define DUPEMATCHER_H

#include "Log.h"

class DupeMatcher
{
private:
	char*				m_szDestDir;
	long long			m_lExpectedSize;
	long long			m_lMaxSize;
	bool				m_bCompressed;

	void				FindLargestFile(const char* szDirectory, char* szFilenameBuf, int iBufLen,
							long long* pMaxSize, bool* pCompressed);

	friend class RarLister;

protected:
	virtual void		PrintMessage(Message::EKind eKind, const char* szFormat, ...) {}

public:
						DupeMatcher(const char* szDestDir, long long lExpectedSize);
						~DupeMatcher();
	bool				Prepare();
	bool				MatchDupeContent(const char* szDupeDir);
	static bool			SizeDiffOK(long long lSize1, long long lSize2, int iMaxDiffPercent);
};

#endif
