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


#include "nzbget.h"
#include "NzbGenerator.h"
#include "Util.h"
#include "FileSystem.h"
#include "Log.h"

void NzbGenerator::Execute()
{
	info("Generating nzbs for %s", *m_dataDir);

	DirBrowser dir(m_dataDir);
	while (const char* filename = dir.Next())
	{
		BString<1024> fullFilename("%s%c%s", *m_dataDir, PATH_SEPARATOR, filename);

		int len = strlen(filename);
		if (len > 4 && !strcasecmp(filename + len - 4, ".nzb"))
		{
			// skip nzb-files
			continue;
		}

		GenerateNzb(fullFilename);
	}

	info("Nzb generation finished");
}

void NzbGenerator::GenerateNzb(const char* path)
{
	BString<1024> nzbFilename("%s%c%s.nzb", *m_dataDir, PATH_SEPARATOR, FileSystem::BaseFileName(path));

	if (FileSystem::FileExists(nzbFilename))
	{
		return;
	}

	info("Generating nzb for %s", FileSystem::BaseFileName(path));

	DiskFile outfile;
	if (!outfile.Open(nzbFilename, DiskFile::omWrite))
	{
		error("Could not create file %s", *nzbFilename);
		return;
	}

	outfile.Print("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	outfile.Print("<!DOCTYPE nzb PUBLIC \"-//newzBin//DTD NZB 1.0//EN\" \"http://www.newzbin.com/DTD/nzb/nzb-1.0.dtd\">\n");
	outfile.Print("<nzb xmlns=\"http://www.newzbin.com/DTD/2003/nzb\">\n");

	bool isDir = FileSystem::DirectoryExists(path);
	if (isDir)
	{
		AppendDir(outfile, path);
	}
	else
	{
		AppendFile(outfile, path, nullptr);
	}

	outfile.Print("</nzb>\n");

	outfile.Close();
}

void NzbGenerator::AppendDir(DiskFile& outfile, const char* path)
{
	DirBrowser dir(path);
	while (const char* filename = dir.Next())
	{
		BString<1024> fullFilename("%s%c%s", path, PATH_SEPARATOR, filename);

		bool isDir = FileSystem::DirectoryExists(fullFilename);
		if (!isDir)
		{
			AppendFile(outfile, fullFilename, FileSystem::BaseFileName(path));
		}
	}
}

void NzbGenerator::AppendFile(DiskFile& outfile, const char* filename, const char* relativePath)
{
	detail("Processing %s", FileSystem::BaseFileName(filename));

	int64 fileSize = FileSystem::FileSize(filename);
	time_t timestamp = Util::CurrentTime();

	int segmentCount = (int)((fileSize + m_segmentSize - 1) / m_segmentSize);

	outfile.Print("<file poster=\"nserv\" date=\"%i\" subject=\"&quot;%s&quot; yEnc (1/%i)\">\n",
		(int)timestamp, FileSystem::BaseFileName(filename), segmentCount);
	outfile.Print("<groups>\n");
	outfile.Print("<group>alt.binaries.test</group>\n");
	outfile.Print("</groups>\n");
	outfile.Print("<segments>\n");

	int64 segOffset = 0;
	for (int segno = 1; segno <= segmentCount; segno++)
	{
		int segSize = (int)(segOffset + m_segmentSize < fileSize ? m_segmentSize : fileSize - segOffset);
		outfile.Print("<segment bytes=\"%i\" number=\"%i\">%s%s%s?%i=%" PRIi64 ":%i</segment>\n",
			m_segmentSize, segno,
			relativePath ? relativePath : "",
			relativePath ? "/" : "",
			FileSystem::BaseFileName(filename), segno, segOffset, segSize);
		segOffset += segSize;
	}

	outfile.Print("</segments>\n");
	outfile.Print("</file>\n");
}
