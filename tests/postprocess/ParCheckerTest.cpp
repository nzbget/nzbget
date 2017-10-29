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
#include "ParChecker.h"
#include "TestUtil.h"

class ParCheckerMock: public ParChecker
{
public:
	ParCheckerMock();
	void Execute();
	void CorruptFile(const char* filename, int offset);

protected:
	virtual bool RequestMorePars(int blockNeeded, int* blockFound) { return false; }
	virtual EFileStatus FindFileCrc(const char* filename, uint32* crc, SegmentList* segments);

private:
	uint32 CalcFileCrc(const char* filename);
};

ParCheckerMock::ParCheckerMock()
{
	TestUtil::PrepareWorkingDir("parchecker");
	SetDestDir(TestUtil::WorkingDir().c_str());
}

void ParCheckerMock::Execute()
{
	TestUtil::DisableCout();
	ParChecker::Execute();
	TestUtil::EnableCout();
}

void ParCheckerMock::CorruptFile(const char* filename, int offset)
{
	std::string fullfilename(TestUtil::WorkingDir() + "/" + filename);

	FILE* file = fopen(fullfilename.c_str(), FOPEN_RBP);
	REQUIRE(file != nullptr);

	fseek(file, offset, SEEK_SET);
	char b = 0;
	int written = fwrite(&b, 1, 1, file);
	REQUIRE(written == 1);

	fclose(file);
}

ParCheckerMock::EFileStatus ParCheckerMock::FindFileCrc(const char* filename, uint32* crc, SegmentList* segments)
{
	std::ifstream sm((TestUtil::WorkingDir() + "/crc.txt").c_str());
	std::string smfilename, smcrc;
	while (!sm.eof())
	{
		sm >> smfilename >> smcrc;
		if (smfilename == filename)
		{
			*crc = strtoul(smcrc.c_str(), nullptr, 16);
			uint32 realCrc = CalcFileCrc((TestUtil::WorkingDir() + "/" + filename).c_str());
			return *crc == realCrc ? ParChecker::fsSuccess : ParChecker::fsUnknown;
		}
	}
	return ParChecker::fsUnknown;
}

uint32 ParCheckerMock::CalcFileCrc(const char* filename)
{
	FILE* infile = fopen(filename, FOPEN_RB);
	REQUIRE(infile);

	CharBuffer buffer(1024 * 64);
	Crc32 downloadCrc;

	int cnt = buffer.Size();
	while (cnt == buffer.Size())
	{
		cnt = (int)fread(buffer, 1, buffer.Size(), infile);
		downloadCrc.Append((uchar*)(char*)buffer, cnt);
	}

	fclose(infile);

	return downloadCrc.Finish();
}

TEST_CASE("Par-checker: repair not needed", "[Par][ParChecker][Slow][TestData]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=no");
	Options options(&cmdOpts, nullptr);

	ParCheckerMock parChecker;
	parChecker.Execute();

	REQUIRE(parChecker.GetStatus() == ParChecker::psRepairNotNeeded);
	REQUIRE(parChecker.GetParFull() == true);
}

TEST_CASE("Par-checker: repair possible", "[Par][ParChecker][Slow][TestData]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=no");
	Options options(&cmdOpts, nullptr);

	ParCheckerMock parChecker;
	parChecker.CorruptFile("testfile.dat", 20000);
	parChecker.Execute();

	REQUIRE(parChecker.GetStatus() == ParChecker::psRepairPossible);
	REQUIRE(parChecker.GetParFull() == true);
}

TEST_CASE("Par-checker: repair successful", "[Par][ParChecker][Slow][TestData]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=yes");
	Options options(&cmdOpts, nullptr);

	ParCheckerMock parChecker;
	parChecker.CorruptFile("testfile.dat", 20000);
	parChecker.Execute();

	REQUIRE(parChecker.GetStatus() == ParChecker::psRepaired);
	REQUIRE(parChecker.GetParFull() == true);
}

TEST_CASE("Par-checker: repair failed", "[Par][ParChecker][Slow][TestData]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=no");
	Options options(&cmdOpts, nullptr);

	ParCheckerMock parChecker;
	parChecker.CorruptFile("testfile.dat", 20000);
	parChecker.CorruptFile("testfile.dat", 30000);
	parChecker.CorruptFile("testfile.dat", 40000);
	parChecker.CorruptFile("testfile.dat", 50000);
	parChecker.CorruptFile("testfile.dat", 60000);
	parChecker.CorruptFile("testfile.dat", 70000);
	parChecker.CorruptFile("testfile.dat", 80000);
	parChecker.Execute();

	REQUIRE(parChecker.GetStatus() == ParChecker::psFailed);
	REQUIRE(parChecker.GetParFull() == true);
}

TEST_CASE("Par-checker: quick verification repair not needed", "[Par][ParChecker][Slow][TestData]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=no");
	Options options(&cmdOpts, nullptr);

	ParCheckerMock parChecker;
	parChecker.SetParQuick(true);
	parChecker.Execute();

	REQUIRE(parChecker.GetStatus() == ParChecker::psRepairNotNeeded);
	REQUIRE(parChecker.GetParFull() == false);
}

TEST_CASE("Par-checker: quick verification repair successful", "[Par][ParChecker][Slow][TestData]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=yes");
	Options options(&cmdOpts, nullptr);

	ParCheckerMock parChecker;
	parChecker.SetParQuick(true);
	parChecker.CorruptFile("testfile.dat", 20000);
	parChecker.Execute();

	REQUIRE(parChecker.GetStatus() == ParChecker::psRepaired);
	REQUIRE(parChecker.GetParFull() == false);
}

TEST_CASE("Par-checker: quick full verification repair successful", "[Par][ParChecker][Slow][TestData]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=yes");
	Options options(&cmdOpts, nullptr);

	ParCheckerMock parChecker;
	parChecker.SetParQuick(true);
	parChecker.CorruptFile("testfile.dat", 20000);
	parChecker.CorruptFile("testfile.nfo", 100);
	parChecker.Execute();

	// All files were damaged, the full verification was performed

	REQUIRE(parChecker.GetStatus() == ParChecker::psRepaired);
	REQUIRE(parChecker.GetParFull() == true);
}

TEST_CASE("Par-checker: ignoring extensions", "[Par][ParChecker][Slow][TestData]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=yes");

	ParChecker::EStatus expectedStatus;

	SECTION("ParIgnoreExt")
	{
		cmdOpts.push_back("ParIgnoreExt=.dat");
		expectedStatus = ParChecker::psRepairNotNeeded;
	}

	SECTION("ExtCleanupDisk")
	{
		cmdOpts.push_back("ExtCleanupDisk=.dat");
		expectedStatus = ParChecker::psFailed;
	}

	Options options(&cmdOpts, nullptr);

	ParCheckerMock parChecker;
	parChecker.CorruptFile("testfile.dat", 20000);
	parChecker.CorruptFile("testfile.dat", 30000);
	parChecker.CorruptFile("testfile.dat", 40000);
	parChecker.CorruptFile("testfile.dat", 50000);
	parChecker.CorruptFile("testfile.dat", 60000);
	parChecker.CorruptFile("testfile.dat", 70000);
	parChecker.CorruptFile("testfile.dat", 80000);

	parChecker.Execute();

	REQUIRE(parChecker.GetStatus() == expectedStatus);
}
