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
#include "DupeMatcher.h"
#include "FileSystem.h"
#include "TestUtil.h"

TEST_CASE("Dupe matcher", "[Par][DupeMatcher][Unrar][Slow][TestData]")
{
	Options options(nullptr, nullptr);

	TestUtil::PrepareWorkingDir("DupeMatcher");

	CString errmsg;

	// prepare directories

	std::string dupe1(TestUtil::WorkingDir() + "/dupe1");
	REQUIRE(FileSystem::ForceDirectories(dupe1.c_str(), errmsg));
	TestUtil::CopyAllFiles(dupe1, TestUtil::TestDataDir() + "/parchecker");

	std::string dupe2(TestUtil::WorkingDir() + "/dupe2");
	REQUIRE(FileSystem::ForceDirectories(dupe2.c_str(), errmsg));
	TestUtil::CopyAllFiles(dupe2, TestUtil::TestDataDir() + "/parchecker");
	FileSystem::DeleteFile((dupe2 + "/testfile.nfo").c_str());

	std::string rardupe1(TestUtil::TestDataDir() + "/dupematcher1");
	std::string rardupe2(TestUtil::TestDataDir() + "/dupematcher2");

	std::string nondupe(TestUtil::WorkingDir() + "/nondupe");
	REQUIRE(FileSystem::ForceDirectories(nondupe.c_str(), errmsg));
	TestUtil::CopyAllFiles(nondupe, TestUtil::TestDataDir() + "/parchecker");
	remove((nondupe + "/testfile.dat").c_str());

	// now test
	int64 expectedSize = FileSystem::FileSize((dupe1 + "/testfile.dat").c_str());

	INFO("This test requires working unrar 5 in search path");

	DupeMatcher dupe1Matcher(dupe1.c_str(), expectedSize);
	CHECK(dupe1Matcher.Prepare());
	CHECK(dupe1Matcher.MatchDupeContent(dupe2.c_str()));
	CHECK(dupe1Matcher.MatchDupeContent(rardupe1.c_str()));
	CHECK(dupe1Matcher.MatchDupeContent(rardupe2.c_str()));
	CHECK_FALSE(dupe1Matcher.MatchDupeContent(nondupe.c_str()));
	
	DupeMatcher dupe2Matcher(dupe2.c_str(), expectedSize);
	CHECK(dupe2Matcher.Prepare());
	CHECK(dupe2Matcher.MatchDupeContent(dupe1.c_str()));
	CHECK(dupe2Matcher.MatchDupeContent(rardupe1.c_str()));
	CHECK(dupe2Matcher.MatchDupeContent(rardupe2.c_str()));
	CHECK_FALSE(dupe2Matcher.MatchDupeContent(nondupe.c_str()));

	DupeMatcher nonDupeMatcher(nondupe.c_str(), expectedSize);
	CHECK_FALSE(nonDupeMatcher.Prepare());
	CHECK_FALSE(nonDupeMatcher.MatchDupeContent(dupe1.c_str()));
	CHECK_FALSE(nonDupeMatcher.MatchDupeContent(dupe2.c_str()));
	CHECK_FALSE(nonDupeMatcher.MatchDupeContent(rardupe1.c_str()));
	CHECK_FALSE(nonDupeMatcher.MatchDupeContent(rardupe2.c_str()));

	DupeMatcher rardupe1matcher(rardupe1.c_str(), expectedSize);
	CHECK(rardupe1matcher.Prepare());
	CHECK(rardupe1matcher.MatchDupeContent(dupe1.c_str()));
	CHECK(rardupe1matcher.MatchDupeContent(dupe2.c_str()));		    
	CHECK(rardupe1matcher.MatchDupeContent(rardupe2.c_str()));
	CHECK_FALSE(rardupe1matcher.MatchDupeContent(nondupe.c_str()));

	DupeMatcher rardupe2matcher(rardupe2.c_str(), expectedSize);
	CHECK(rardupe2matcher.Prepare());
	CHECK(rardupe2matcher.MatchDupeContent(rardupe1.c_str()));
	CHECK(rardupe2matcher.MatchDupeContent(dupe1.c_str()));
	CHECK(rardupe2matcher.MatchDupeContent(dupe2.c_str()));
	CHECK_FALSE(rardupe2matcher.MatchDupeContent(nondupe.c_str()));
}
