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

#define CATCH_CONFIG_RUNNER
#include "catch.h"

#include "Thread.h"
#include "Log.h"
#include "Util.h"
#include "FileSystem.h"
#include "TestUtil.h"

int TestMain(int argc, char * argv[])
{
	TestUtil::Init(argv[0]);
	Log log;
	Thread::Init();

	if (argc == 1)
	{
		printf("Unit and integration tests for nzbget-%s.\nUse '%s -tests [quick]' to run only quick tests or '%s -h' for more options.\n",
			   Util::VersionRevision(), FileSystem::BaseFileName(argv[0]), FileSystem::BaseFileName(argv[0]));
	}

	// shift arguments for catch to not see the parameter "-tests"
	char** testsargv = (char**)malloc(sizeof(char*) * (argc + 1));
	char firstArg[1024];
	snprintf(firstArg, 1024, "%s %s", argv[0], argv[1]);
	firstArg[1024-1] = '\0';
	testsargv[0] = firstArg;
	for (int i = 2; i < argc; i++)
	{
		testsargv[i-1] = argv[i];
	}
	argc--;
	testsargv[argc] = nullptr;

	int ret = Catch::Session().run(argc, testsargv);
	
	free(testsargv);
	TestUtil::Final();

	return ret;
}

void TestCleanup()
{
	// If tests were run (via "TestMain") the Catch-framework does clean up automatically.
	// However, if no tests were run, the global objects remain alive and causing memory leak
	// detection reports. Therefore we clean up the Catch-framework when we don't run any tests.
	Catch::cleanUp();
}
