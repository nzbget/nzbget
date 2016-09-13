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


#ifndef NZBGENERATOR_H
#define NZBGENERATOR_H

#include "NString.h"
#include "FileSystem.h"

class NzbGenerator
{
public:
	NzbGenerator(const char* dataDir, int segmentSize) :
		m_dataDir(dataDir), m_segmentSize(segmentSize) {};
	void Execute();

private:
	CString m_dataDir;
	int m_segmentSize;

	void GenerateNzb(const char* path);
	void AppendFile(DiskFile& outfile, const char* filename, const char* relativePath);
	void AppendDir(DiskFile& outfile, const char* path);
};

#endif
