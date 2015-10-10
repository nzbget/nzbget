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

void TestNzb(std::string TestFilename)
{
	INFO(std::string("Filename: ") + TestFilename);

	std::string NzbFilename(TestUtil::TestDataDir() + "/nzbfile/"+ TestFilename + ".nzb");
	std::string InfoFilename(TestUtil::TestDataDir() + "/nzbfile/"+ TestFilename + ".txt");

	NZBFile* pNZBFile = new NZBFile(NzbFilename.c_str(), "");
	bool bParsedOK = pNZBFile->Parse();
	REQUIRE(bParsedOK == true);

	FILE* infofile = fopen(InfoFilename.c_str(), FOPEN_RB);
	REQUIRE(infofile != NULL);
	char buffer[1024];

	while (fgets(buffer, sizeof(buffer), infofile) && *buffer == '#') ;
	REQUIRE(*buffer);

	int iFileCount = atoi(buffer);
	REQUIRE(pNZBFile->GetNZBInfo()->GetFileCount() == iFileCount);

	for (int i = 0; i < iFileCount; i++)
	{
		while (fgets(buffer, sizeof(buffer), infofile) && *buffer == '#') ;
		REQUIRE(*buffer);
		FileInfo* pFileInfo = pNZBFile->GetNZBInfo()->GetFileList()->at(i);
		REQUIRE(pFileInfo != NULL);
		Util::TrimRight(buffer);
		REQUIRE(std::string(pFileInfo->GetFilename()) == std::string(buffer));
	}

	fclose(infofile);
	delete pNZBFile;
}

TEST_CASE("Nzb parser", "[NZBFile][TestData]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("SaveQueue=no");
	Options options(&cmdOpts, NULL);

	TestNzb("dotless");
	TestNzb("plain");
}
