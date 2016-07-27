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

#include "CommandLineParser.h"

TEST_CASE("Command line parser: initializing without configuration file", "[CommandLineParser][Quick]")
{
	const char* argv[] = {"nzbget", "-n", "-p", nullptr};
	CommandLineParser commandLineParser(3, argv);

	REQUIRE(commandLineParser.GetConfigFilename() == nullptr);
	REQUIRE(commandLineParser.GetClientOperation() == CommandLineParser::opClientNoOperation);
}

TEST_CASE("Command line parser: initializing with configuration file", "[CommandLineParser][Quick]")
{
	const char* argv[] = {"nzbget", "-c", "/home/user/nzbget.conf", "-p", nullptr};
	CommandLineParser commandLineParser(4, argv);

	REQUIRE(commandLineParser.GetConfigFilename() != nullptr);
	REQUIRE(strcmp(commandLineParser.GetConfigFilename(), "/home/user/nzbget.conf") == 0);
	REQUIRE(commandLineParser.GetClientOperation() == CommandLineParser::opClientNoOperation);
}

TEST_CASE("Command line parser: server mode", "[CommandLineParser][Quick]")
{
	const char* argv[] = {"nzbget", "-n", "-s", nullptr};
	CommandLineParser commandLineParser(3, argv);

	REQUIRE(commandLineParser.GetServerMode() == true);
	REQUIRE(commandLineParser.GetPauseDownload() == false);
}

TEST_CASE("Command line parser: passing pause", "[CommandLineParser][Quick]")
{
	const char* argv[] = {"nzbget", "-n", "-s", "-P", nullptr};
	CommandLineParser commandLineParser(4, argv);

	REQUIRE(commandLineParser.GetPauseDownload() == true);
}

TEST_CASE("Command line parser: extra option (1)", "[CommandLineParser][Quick]")
{
	const char* argv[] = {"nzbget", "-n", "-o", "myoption1=yes", "-o", "myoption2=no", "-p", nullptr};
	CommandLineParser commandLineParser(7, argv);

	REQUIRE(commandLineParser.GetOptionList()->size() == 2);
	REQUIRE(strcmp(commandLineParser.GetOptionList()->at(0), "myoption1=yes") == 0);
	REQUIRE(strcmp(commandLineParser.GetOptionList()->at(0), "myoption2=no") != 0);
	REQUIRE(strcmp(commandLineParser.GetOptionList()->at(1), "myoption2=no") == 0);
}

TEST_CASE("Command line parser: extra option (2)", "[CommandLineParser][Quick]")
{
	const char* argv[] = {"nzbget", "-n", "-o", "myoption1=yes", "-o", "myoption2=no", "-o", "myoption1=no", "-p", nullptr};
	CommandLineParser commandLineParser(9, argv);

	REQUIRE(commandLineParser.GetOptionList()->size() == 3);
	REQUIRE(strcmp(commandLineParser.GetOptionList()->at(0), "myoption1=yes") == 0);
	REQUIRE(strcmp(commandLineParser.GetOptionList()->at(1), "myoption2=no") == 0);
	REQUIRE(strcmp(commandLineParser.GetOptionList()->at(2), "myoption1=no") == 0);
}

// TESTS: Add more tests for:
// - edit-command;
// - all other remote commands;
// - passing nzb-file in standalone mode;
