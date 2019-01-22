/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2017-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "DirectUnpack.h"
#include "FileSystem.h"
#include "TestUtil.h"

class DirectUnpackDownloadQueueMock : public DownloadQueue
{
public:
	DirectUnpackDownloadQueueMock() { Init(this); }
	virtual bool EditEntry(int ID, EEditAction action, const char* args) { return false; };
	virtual bool EditList(IdList* idList, NameList* nameList, EMatchMode matchMode,
		EEditAction action, const char* args) { return false; }
	virtual void HistoryChanged() {}
	virtual void Save() {};
	virtual void SaveChanged() {}
};

TEST_CASE("Direct-unpack simple", "[Rar][DirectUnpack][Unrar][Slow][TestData]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("WriteLog=none");
	cmdOpts.push_back("NzbLog=no");
	Options options(&cmdOpts, nullptr);

	DirectUnpackDownloadQueueMock downloadQueue;

	TestUtil::PrepareWorkingDir("empty");

	INFO("This test requires working unrar 5 in search path");

	REQUIRE(FileSystem::CopyFile((TestUtil::TestDataDir() + "/rarrenamer/testfile3.part01.rar").c_str(),
		(TestUtil::WorkingDir() + "/testfile3.part01.rar").c_str()));
	REQUIRE(FileSystem::CopyFile((TestUtil::TestDataDir() + "/rarrenamer/testfile3.part02.rar").c_str(),
		(TestUtil::WorkingDir() + "/testfile3.part02.rar").c_str()));
	REQUIRE(FileSystem::CopyFile((TestUtil::TestDataDir() + "/rarrenamer/testfile3.part03.rar").c_str(),
		(TestUtil::WorkingDir() + "/testfile3.part03.rar").c_str()));

	std::unique_ptr<NzbInfo> nzbInfo = std::make_unique<NzbInfo>();
	NzbInfo* nzbPtr = nzbInfo.get();
	nzbInfo->SetName("test");
	nzbInfo->SetDestDir(TestUtil::WorkingDir().c_str());
	downloadQueue.GetQueue()->Add(std::move(nzbInfo), false);

	DirectUnpack::StartJob(nzbPtr);

	while (true)
	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		if (nzbPtr->GetUnpackThread())
		{
			((DirectUnpack*)nzbPtr->GetUnpackThread())->NzbDownloaded(downloadQueue, nzbPtr);
			break;
		}
		Util::Sleep(50);
	}

	while (nzbPtr->GetDirectUnpackStatus() == NzbInfo::nsRunning)
	{
		Util::Sleep(20);
	}

	REQUIRE(nzbPtr->GetDirectUnpackStatus() == NzbInfo::nsSuccess);
	REQUIRE(FileSystem::FileExists((TestUtil::WorkingDir() + "/_unpack/testfile3.dat").c_str()));
}

TEST_CASE("Direct-unpack two archives", "[Rar][DirectUnpack][Unrar][Slow][TestData]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("WriteLog=none");
	cmdOpts.push_back("NzbLog=no");
	Options options(&cmdOpts, nullptr);

	DirectUnpackDownloadQueueMock downloadQueue;

	TestUtil::PrepareWorkingDir("empty");

	INFO("This test requires working unrar 5 in search path");

	REQUIRE(FileSystem::CopyFile((TestUtil::TestDataDir() + "/rarrenamer/testfile3.part01.rar").c_str(),
		(TestUtil::WorkingDir() + "/testfile3.part01.rar").c_str()));
	REQUIRE(FileSystem::CopyFile((TestUtil::TestDataDir() + "/rarrenamer/testfile3.part02.rar").c_str(),
		(TestUtil::WorkingDir() + "/testfile3.part02.rar").c_str()));
	REQUIRE(FileSystem::CopyFile((TestUtil::TestDataDir() + "/rarrenamer/testfile3.part03.rar").c_str(),
		(TestUtil::WorkingDir() + "/testfile3.part03.rar").c_str()));

	REQUIRE(FileSystem::CopyFile((TestUtil::TestDataDir() + "/rarrenamer/testfile5.part01.rar").c_str(),
		(TestUtil::WorkingDir() + "/testfile5.part01.rar").c_str()));
	REQUIRE(FileSystem::CopyFile((TestUtil::TestDataDir() + "/rarrenamer/testfile5.part02.rar").c_str(),
		(TestUtil::WorkingDir() + "/testfile5.part02.rar").c_str()));
	REQUIRE(FileSystem::CopyFile((TestUtil::TestDataDir() + "/rarrenamer/testfile5.part03.rar").c_str(),
		(TestUtil::WorkingDir() + "/testfile5.part03.rar").c_str()));

	std::unique_ptr<NzbInfo> nzbInfo = std::make_unique<NzbInfo>();
	NzbInfo* nzbPtr = nzbInfo.get();
	nzbInfo->SetName("test");
	nzbInfo->SetDestDir(TestUtil::WorkingDir().c_str());
	downloadQueue.GetQueue()->Add(std::move(nzbInfo), false);

	DirectUnpack::StartJob(nzbPtr);

	while (true)
	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		if (nzbPtr->GetUnpackThread())
		{
			((DirectUnpack*)nzbPtr->GetUnpackThread())->NzbDownloaded(downloadQueue, nzbPtr);
			break;
		}
		Util::Sleep(50);
	}

	while (nzbPtr->GetDirectUnpackStatus() == NzbInfo::nsRunning)
	{
		Util::Sleep(20);
	}

	REQUIRE(nzbPtr->GetDirectUnpackStatus() == NzbInfo::nsSuccess);
	REQUIRE(FileSystem::FileExists((TestUtil::WorkingDir() + "/_unpack/testfile3.dat").c_str()));
	REQUIRE(FileSystem::FileExists((TestUtil::WorkingDir() + "/_unpack/testfile5.dat").c_str()));
}
