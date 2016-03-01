/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2015-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef DUPEMATCHER_H
#define DUPEMATCHER_H

#include "NString.h"
#include "Log.h"

class DupeMatcher
{
public:
	DupeMatcher(const char* destDir, int64 expectedSize) :
		m_destDir(destDir), m_expectedSize(expectedSize) {}
	bool Prepare();
	bool MatchDupeContent(const char* dupeDir);
	static bool SizeDiffOK(int64 size1, int64 size2, int maxDiffPercent);

protected:
	virtual void PrintMessage(Message::EKind kind, const char* format, ...) PRINTF_SYNTAX(3) {}

private:
	CString m_destDir;
	int64 m_expectedSize;
	int64 m_maxSize = -1;
	bool m_compressed = false;

	void FindLargestFile(const char* directory, char* filenameBuf, int bufLen,
		int64* maxSize, bool* compressed);

	friend class RarLister;
};

#endif
