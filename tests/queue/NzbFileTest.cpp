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


#include "nzbget.h"

#include "catch.h"

#include "NzbFile.h"
#include "Options.h"
#include "TestUtil.h"

void TestNzb(std::string testFilename)
{
	INFO(std::string("Filename: ") + testFilename);

	std::string nzbFilename(TestUtil::TestDataDir() + "/nzbfile/"+ testFilename + ".nzb");
	std::string infoFilename(TestUtil::TestDataDir() + "/nzbfile/"+ testFilename + ".txt");

	NzbFile nzbFile(nzbFilename.c_str(), "");
	bool parsedOK = nzbFile.Parse();
	REQUIRE(parsedOK == true);

	FILE* infofile = fopen(infoFilename.c_str(), FOPEN_RB);
	REQUIRE(infofile != nullptr);
	char buffer[1024];

	while (fgets(buffer, sizeof(buffer), infofile) && *buffer == '#') ;
	REQUIRE(*buffer);

	int fileCount = atoi(buffer);
	std::unique_ptr<NzbInfo> nzbInfo = nzbFile.DetachNzbInfo();
	REQUIRE(nzbInfo->GetFileCount() == fileCount);

	for (int i = 0; i < fileCount; i++)
	{
		while (fgets(buffer, sizeof(buffer), infofile) && *buffer == '#') ;
		REQUIRE(*buffer);
		FileInfo* fileInfo = nzbInfo->GetFileList()->at(i).get();
		REQUIRE(fileInfo != nullptr);
		Util::TrimRight(buffer);
		REQUIRE(std::string(fileInfo->GetFilename()) == std::string(buffer));
	}

	fclose(infofile);
}

TEST_CASE("Nzb parser", "[NzbFile][TestData]")
{
	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

	TestNzb("dotless");
	TestNzb("plain");
}
