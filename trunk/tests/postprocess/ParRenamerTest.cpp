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

#include <iostream>
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
#include "ParRenamer.h"
#include "TestUtil.h"

class ParRenamerMock: public ParRenamer
{
private:
	int				m_iRenamed;
protected:
	virtual void	RegisterRenamedFile(const char* szOldFilename, const char* szNewFileName) { m_iRenamed++; }
public:
					ParRenamerMock();
	void			Execute();
	int				GetRenamedCount() { return m_iRenamed; }
};

ParRenamerMock::ParRenamerMock()
{
	TestUtil::PrepareWorkingDir("parchecker");
	SetDestDir(TestUtil::WorkingDir().c_str());
}

void ParRenamerMock::Execute()
{
	TestUtil::DisableCout();
	m_iRenamed = 0;
	Start();
	while (IsRunning())
	{
		usleep(10*1000);
	}
	TestUtil::EnableCout();
}

TEST_CASE("Par-renamer: rename not needed", "[Par][ParRenamer][Slow][TestData]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRename=yes");
	Options options(&cmdOpts, NULL);

	ParRenamerMock parRenamer;
	parRenamer.Execute();

	REQUIRE(parRenamer.GetStatus() == ParRenamer::psFailed);
	REQUIRE(parRenamer.GetRenamedCount() == 0);
}

TEST_CASE("Par-renamer: rename successful", "[Par][ParRenamer][Slow][TestData]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRename=yes");
	Options options(&cmdOpts, NULL);

	ParRenamerMock parRenamer;
	Util::MoveFile((TestUtil::WorkingDir() + "/testfile.dat").c_str(), (TestUtil::WorkingDir() + "/123456").c_str());
	parRenamer.Execute();

	REQUIRE(parRenamer.GetStatus() == ParRenamer::psSuccess);
	REQUIRE(parRenamer.GetRenamedCount() == 1);
	REQUIRE_FALSE(parRenamer.HasMissedFiles());
}

TEST_CASE("Par-renamer: detecting missing", "[Par][ParRenamer][Slow][TestData]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRename=yes");
	Options options(&cmdOpts, NULL);

	ParRenamerMock parRenamer;
	Util::MoveFile((TestUtil::WorkingDir() + "/testfile.dat").c_str(), (TestUtil::WorkingDir() + "/123456").c_str());
	parRenamer.SetDetectMissing(true);
	REQUIRE(remove((TestUtil::WorkingDir() + "/testfile.nfo").c_str()) == 0);
	parRenamer.Execute();

	REQUIRE(parRenamer.GetStatus() == ParRenamer::psSuccess);
	REQUIRE(parRenamer.GetRenamedCount() == 1);
	REQUIRE(parRenamer.HasMissedFiles());
}
