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

#include "Options.h"
#include "ParRenamer.h"
#include "FileSystem.h"
#include "TestUtil.h"

class ParRenamerMock: public ParRenamer
{
public:
	ParRenamerMock();
	void Execute();
};

ParRenamerMock::ParRenamerMock()
{
	TestUtil::PrepareWorkingDir("parchecker");
	SetDestDir(TestUtil::WorkingDir().c_str());
}

void ParRenamerMock::Execute()
{
	TestUtil::DisableCout();
	ParRenamer::Execute();
	TestUtil::EnableCout();
}

TEST_CASE("Par-renamer: rename not needed", "[Par][ParRenamer][Slow][TestData]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRename=yes");
	Options options(&cmdOpts, nullptr);

	ParRenamerMock parRenamer;
	parRenamer.Execute();

	REQUIRE(parRenamer.GetRenamedCount() == 0);
}

TEST_CASE("Par-renamer: rename successful", "[Par][ParRenamer][Slow][TestData]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRename=yes");
	Options options(&cmdOpts, nullptr);

	ParRenamerMock parRenamer;
	FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile.dat").c_str(), (TestUtil::WorkingDir() + "/123456").c_str());
	parRenamer.Execute();

	REQUIRE(parRenamer.GetRenamedCount() == 1);
	REQUIRE_FALSE(parRenamer.HasMissedFiles());
}

TEST_CASE("Par-renamer: detecting missing", "[Par][ParRenamer][Slow][TestData]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRename=yes");
	Options options(&cmdOpts, nullptr);

	ParRenamerMock parRenamer;
	FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile.dat").c_str(), (TestUtil::WorkingDir() + "/123456").c_str());
	parRenamer.SetDetectMissing(true);
	REQUIRE(FileSystem::DeleteFile((TestUtil::WorkingDir() + "/testfile.nfo").c_str()));
	parRenamer.Execute();

	REQUIRE(parRenamer.GetRenamedCount() == 1);
	REQUIRE(parRenamer.HasMissedFiles());
}

TEST_CASE("Par-renamer: rename dupe par", "[Par][ParRenamer][Slow][TestData]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRename=yes");
	Options options(&cmdOpts, nullptr);

	ParRenamerMock parRenamer;
	FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile.dat").c_str(), (TestUtil::WorkingDir() + "/123456").c_str());
	FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile.vol00+1.PAR2").c_str(), (TestUtil::WorkingDir() + "/testfil2.par2").c_str());
	parRenamer.SetDetectMissing(true);
	parRenamer.Execute();

	REQUIRE(parRenamer.GetRenamedCount() == 5);
	REQUIRE_FALSE(parRenamer.HasMissedFiles());
}

TEST_CASE("Par-renamer: no par extension", "[Par][ParRenamer][Slow][TestData]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRename=yes");
	Options options(&cmdOpts, nullptr);

	ParRenamerMock parRenamer;
	FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile.par2").c_str(), (TestUtil::WorkingDir() + "/testfile").c_str());
	parRenamer.SetDetectMissing(true);
	parRenamer.Execute();

	REQUIRE(parRenamer.GetRenamedCount() == 4);
	REQUIRE_FALSE(parRenamer.HasMissedFiles());
}
