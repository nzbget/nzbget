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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdio.h>

#include "catch.h"

#include "nzbget.h"
#include "NZBFile.h"
#include "Options.h"
#include "TestUtil.h"

void TestNzb(std::string testFilename)
{
	INFO(std::string("Filename: ") + testFilename);

	std::string nzbFilename(TestUtil::TestDataDir() + "/nzbfile/"+ testFilename + ".nzb");
	std::string infoFilename(TestUtil::TestDataDir() + "/nzbfile/"+ testFilename + ".txt");

	NZBFile* nzbFile = new NZBFile(nzbFilename.c_str(), "");
	bool parsedOK = nzbFile->Parse();
	REQUIRE(parsedOK == true);

	FILE* infofile = fopen(infoFilename.c_str(), FOPEN_RB);
	REQUIRE(infofile != NULL);
	char buffer[1024];

	while (fgets(buffer, sizeof(buffer), infofile) && *buffer == '#') ;
	REQUIRE(*buffer);

	int fileCount = atoi(buffer);
	REQUIRE(nzbFile->GetNZBInfo()->GetFileCount() == fileCount);

	for (int i = 0; i < fileCount; i++)
	{
		while (fgets(buffer, sizeof(buffer), infofile) && *buffer == '#') ;
		REQUIRE(*buffer);
		FileInfo* fileInfo = nzbFile->GetNZBInfo()->GetFileList()->at(i);
		REQUIRE(fileInfo != NULL);
		Util::TrimRight(buffer);
		REQUIRE(std::string(fileInfo->GetFilename()) == std::string(buffer));
	}

	fclose(infofile);
	delete nzbFile;
}

TEST_CASE("Nzb parser", "[NZBFile][TestData]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("SaveQueue=no");
	Options options(&cmdOpts, NULL);

	TestNzb("dotless");
	TestNzb("plain");
}
