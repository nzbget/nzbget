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

#include "catch.h"

#include "Options.h"
#include "RarRenamer.h"
#include "FileSystem.h"
#include "TestUtil.h"

class RarRenamerMock: public RarRenamer
{
public:
	RarRenamerMock();
};

RarRenamerMock::RarRenamerMock()
{
	TestUtil::PrepareWorkingDir("rarrenamer");
	SetDestDir(TestUtil::WorkingDir().c_str());
}

TEST_CASE("Rar-renamer: rename not needed", "[Rar][RarRenamer][Slow][TestData]")
{
	RarRenamerMock rarRenamer;

	rarRenamer.Execute();

	REQUIRE(rarRenamer.GetRenamedCount() == 0);
}

TEST_CASE("Rar-renamer: rename rar3", "[Rar][RarRenamer][Slow][TestData]")
{
	RarRenamerMock rarRenamer;

	REQUIRE(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3.part01.rar").c_str(), (TestUtil::WorkingDir() + "/12345").c_str()));
	REQUIRE(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3.part02.rar").c_str(), (TestUtil::WorkingDir() + "/12342").c_str()));
	REQUIRE(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3.part03.rar").c_str(), (TestUtil::WorkingDir() + "/12346").c_str()));
	REQUIRE(FileSystem::DeleteFile((TestUtil::WorkingDir() + "/testfile5.part01.rar").c_str()));
	REQUIRE(FileSystem::DeleteFile((TestUtil::WorkingDir() + "/testfile5.part02.rar").c_str()));
	REQUIRE(FileSystem::DeleteFile((TestUtil::WorkingDir() + "/testfile5.part03.rar").c_str()));

	rarRenamer.Execute();

	REQUIRE(rarRenamer.GetRenamedCount() == 3);
}

TEST_CASE("Rar-renamer: rename rar5", "[Rar][RarRenamer][Slow][TestData]")
{
	RarRenamerMock rarRenamer;

	REQUIRE(FileSystem::DeleteFile((TestUtil::WorkingDir() + "/testfile3.part01.rar").c_str()));
	REQUIRE(FileSystem::DeleteFile((TestUtil::WorkingDir() + "/testfile3.part02.rar").c_str()));
	REQUIRE(FileSystem::DeleteFile((TestUtil::WorkingDir() + "/testfile3.part03.rar").c_str()));
	REQUIRE(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile5.part01.rar").c_str(), (TestUtil::WorkingDir() + "/12348").c_str()));
	REQUIRE(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile5.part02.rar").c_str(), (TestUtil::WorkingDir() + "/12343").c_str()));
	REQUIRE(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile5.part03.rar").c_str(), (TestUtil::WorkingDir() + "/12344").c_str()));

	rarRenamer.Execute();

	REQUIRE(rarRenamer.GetRenamedCount() == 3);
}

TEST_CASE("Rar-renamer: rename two sets", "[Rar][RarRenamer][Slow][TestData]")
{
	RarRenamerMock rarRenamer;

	REQUIRE(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3.part01.rar").c_str(), (TestUtil::WorkingDir() + "/12345").c_str()));
	REQUIRE(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3.part02.rar").c_str(), (TestUtil::WorkingDir() + "/12342").c_str()));
	REQUIRE(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3.part03.rar").c_str(), (TestUtil::WorkingDir() + "/12346").c_str()));
	REQUIRE(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile5.part01.rar").c_str(), (TestUtil::WorkingDir() + "/12348").c_str()));
	REQUIRE(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile5.part02.rar").c_str(), (TestUtil::WorkingDir() + "/12343").c_str()));
	REQUIRE(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile5.part03.rar").c_str(), (TestUtil::WorkingDir() + "/12344").c_str()));

	rarRenamer.Execute();

	REQUIRE(rarRenamer.GetRenamedCount() == 6);
}
