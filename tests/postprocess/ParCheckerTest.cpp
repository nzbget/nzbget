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

#include <fstream>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifndef WIN32
#include <unistd.h>
#endif

#include "catch.h"

#include "nzbget.h"
#include "Options.h"
#include "ParChecker.h"
#include "TestUtil.h"

class ParCheckerMock: public ParChecker
{
private:
	unsigned long	CalcFileCrc(const char* szFilename);
protected:
	virtual bool	RequestMorePars(int iBlockNeeded, int* pBlockFound) { return false; }
	virtual EFileStatus	FindFileCrc(const char* szFilename, unsigned long* lCrc, SegmentList* pSegments);
public:
					ParCheckerMock();
	void			Execute();
	void			CorruptFile(const char* szFilename, int iOffset);
};

ParCheckerMock::ParCheckerMock()
{
	TestUtil::PrepareWorkingDir("parchecker");
	SetDestDir(TestUtil::WorkingDir().c_str());
}

void ParCheckerMock::Execute()
{
	TestUtil::DisableCout();
	Start();
	while (IsRunning())
	{
		usleep(10*1000);
	}
	TestUtil::EnableCout();
}

void ParCheckerMock::CorruptFile(const char* szFilename, int iOffset)
{
	std::string fullfilename(TestUtil::WorkingDir() + "/" + szFilename);

	FILE* pFile = fopen(fullfilename.c_str(), FOPEN_RBP);
	REQUIRE(pFile != NULL);

	fseek(pFile, iOffset, SEEK_SET);
	char b = 0;
	int written = fwrite(&b, 1, 1, pFile);
	REQUIRE(written == 1);

	fclose(pFile);
}

ParCheckerMock::EFileStatus ParCheckerMock::FindFileCrc(const char* szFilename, unsigned long* lCrc, SegmentList* pSegments)
{
	std::ifstream sm((TestUtil::WorkingDir() + "/crc.txt").c_str());
	std::string filename, crc;
	while (!sm.eof())
	{
		sm >> filename >> crc;
		if (filename == szFilename)
		{
			*lCrc = strtoul(crc.c_str(), NULL, 16);
			unsigned long lRealCrc = CalcFileCrc((TestUtil::WorkingDir() + "/" + filename).c_str());
			return *lCrc == lRealCrc ? ParChecker::fsSuccess : ParChecker::fsUnknown;
		}
	}
	return ParChecker::fsUnknown;
}

unsigned long ParCheckerMock::CalcFileCrc(const char* szFilename)
{
	FILE* infile = fopen(szFilename, FOPEN_RB);
	REQUIRE(infile);

	static const int BUFFER_SIZE = 1024 * 64;
	unsigned char* buffer = (unsigned char*)malloc(BUFFER_SIZE);
	unsigned long lDownloadCrc = 0xFFFFFFFF;

	int cnt = BUFFER_SIZE;
	while (cnt == BUFFER_SIZE)
	{
		cnt = (int)fread(buffer, 1, BUFFER_SIZE, infile);
		lDownloadCrc = Util::Crc32m(lDownloadCrc, buffer, cnt);
	}

	free(buffer);
	fclose(infile);

	lDownloadCrc ^= 0xFFFFFFFF;
	return lDownloadCrc;
}

TEST_CASE("Par-checker: repair not needed", "[Par][ParChecker][Slow][TestData]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=no");
	Options options(&cmdOpts, NULL);

	ParCheckerMock parChecker;
	parChecker.Execute();

	REQUIRE(parChecker.GetStatus() == ParChecker::psRepairNotNeeded);
	REQUIRE(parChecker.GetParFull() == true);
}

TEST_CASE("Par-checker: repair possible", "[Par][ParChecker][Slow][TestData]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=no");
	cmdOpts.push_back("BrokenLog=no");
	Options options(&cmdOpts, NULL);

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
	cmdOpts.push_back("BrokenLog=no");
	Options options(&cmdOpts, NULL);

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
	cmdOpts.push_back("BrokenLog=no");
	Options options(&cmdOpts, NULL);

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
	Options options(&cmdOpts, NULL);

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
	cmdOpts.push_back("BrokenLog=no");
	Options options(&cmdOpts, NULL);

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
	cmdOpts.push_back("BrokenLog=no");
	Options options(&cmdOpts, NULL);

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
	cmdOpts.push_back("BrokenLog=no");

	SECTION("ParIgnoreExt")
	{
		cmdOpts.push_back("ParIgnoreExt=.dat");
	}

	SECTION("ExtCleanupDisk")
	{
		cmdOpts.push_back("ExtCleanupDisk=.dat");
	}

	Options options(&cmdOpts, NULL);

	ParCheckerMock parChecker;
	parChecker.CorruptFile("testfile.dat", 20000);
	parChecker.CorruptFile("testfile.dat", 30000);
	parChecker.CorruptFile("testfile.dat", 40000);
	parChecker.CorruptFile("testfile.dat", 50000);
	parChecker.CorruptFile("testfile.dat", 60000);
	parChecker.CorruptFile("testfile.dat", 70000);
	parChecker.CorruptFile("testfile.dat", 80000);

	parChecker.Execute();

	REQUIRE(parChecker.GetStatus() == ParChecker::psRepairNotNeeded);
}
