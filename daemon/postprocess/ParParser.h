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


#ifndef PARPARSER_H
#define PARPARSER_H

#include "NString.h"
#include "Container.h"

class ParParser
{
public:
	typedef std::vector<CString> ParFileList;

	static bool FindMainPars(const char* path, ParFileList* fileList);
	static bool ParseParFilename(const char* parFilename, bool confirmedFilename, int* baseNameLen, int* blocks);
	static bool SameParCollection(const char* filename1, const char* filename2, bool confirmedFilenames);
};

#endif
