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

#define CATCH_CONFIG_RUNNER
#include "catch.h"

#include "Thread.h"
#include "Log.h"
#include "Util.h"
#include "TestUtil.h"

int TestMain(int argc, char * argv[])
{
	TestUtil::Init(argv[0]);
	Log::Init();
	Thread::Init();

	if (argc == 1)
	{
		printf("Unit and integration tests for nzbget-%s.\nUse '%s -tests [quick]' to run only quick tests or '%s -h' for more options.\n",
			   Util::VersionRevision(), Util::BaseFileName(argv[0]), Util::BaseFileName(argv[0]));
	}

	// shift arguments for catch to not see the parameter "-tests"
	char** testsargv = (char**)malloc(sizeof(char*) * (argc + 1));
	char szFirstArg[1024];
	snprintf(szFirstArg, 1024, "%s %s", argv[0], argv[1]);
	szFirstArg[1024-1] = '\0';
	testsargv[0] = szFirstArg;
	for (int i = 2; i < argc; i++)
	{
		testsargv[i-1] = argv[i];
	}
	argc--;
	testsargv[argc] = NULL;

	int ret = Catch::Session().run(argc, testsargv);
	
	free(testsargv);
	Thread::Final();
	Log::Final();
	TestUtil::Final();

	return ret;
}
