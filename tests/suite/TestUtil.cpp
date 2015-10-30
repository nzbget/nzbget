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

#include <string>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifndef WIN32
#include <unistd.h>
#endif

#include "catch.h"

#include "nzbget.h"
#include "Util.h"
#include "TestUtil.h"

bool TestUtil::m_usedWorkingDir = false;
std::string dataDir;

class NullStreamBuf : public std::streambuf
{
public:
	int sputc ( char c ) { return (int) c; }
} NullStreamBufSuiteInstance;

std::streambuf* oldcoutbuf;

void TestUtil::Init(const char* argv0)
{
	m_usedWorkingDir = false;

	char filename[MAX_PATH + 1];
	Util::GetExeFileName(argv0, filename, sizeof(filename));
	Util::NormalizePathSeparators(filename);
	char* end = strrchr(filename, PATH_SEPARATOR);
	if (end) *end = '\0';
	dataDir = filename;
	dataDir += "/testdata";
	if (!Util::DirectoryExists(dataDir.c_str()))
	{
		dataDir = filename;
		dataDir += "/tests/testdata";
	}
	if (!Util::DirectoryExists(dataDir.c_str()))
	{
		dataDir = "";
	}
}

void TestUtil::Final()
{
	if (m_usedWorkingDir)
	{
		CleanupWorkingDir();
	}
}

const std::string TestUtil::TestDataDir()
{
	if (dataDir == "")
	{
		printf("ERROR: Directory \"testdata\" not found.\n");
		exit(1);
	}
	return dataDir;
}

const std::string TestUtil::WorkingDir()
{
	return TestDataDir() + "/temp";
}

void TestUtil::PrepareWorkingDir(const std::string templateDir)
{
	m_usedWorkingDir = true;

	std::string workDir = WorkingDir();
	std::string srcDir(TestDataDir() + "/" + templateDir);

	char errStr[256];
	int retries = 20;

	Util::DeleteDirectoryWithContent(workDir.c_str(), errStr, sizeof(errStr));
	while (Util::DirectoryExists(workDir.c_str()) && retries > 0)
	{
		usleep(1000 * 100);
		retries--;
		Util::DeleteDirectoryWithContent(workDir.c_str(), errStr, sizeof(errStr));
	}
	REQUIRE_FALSE(Util::DirectoryExists(workDir.c_str()));
	Util::CreateDirectory(workDir.c_str());
	REQUIRE(Util::DirEmpty(workDir.c_str()));

	CopyAllFiles(workDir, srcDir);
}

void TestUtil::CopyAllFiles(const std::string destDir, const std::string srcDir)
{
	DirBrowser dir(srcDir.c_str());
	while (const char* filename = dir.Next())
	{
		if (strcmp(filename, ".") && strcmp(filename, ".."))
		{
			std::string srcFile(srcDir + "/" + filename);
			std::string dstFile(destDir + "/" + filename);
			REQUIRE(Util::CopyFile(srcFile.c_str(), dstFile.c_str()));
		}
	}
}

void TestUtil::CleanupWorkingDir()
{
	char errStr[256];
	Util::DeleteDirectoryWithContent(WorkingDir().c_str(), errStr, sizeof(errStr));
}

void TestUtil::DisableCout()
{
	oldcoutbuf = std::cout.rdbuf(&NullStreamBufSuiteInstance);
}

void TestUtil::EnableCout()
{
	std::cout.rdbuf(oldcoutbuf);
}

