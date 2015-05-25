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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifndef WIN32
#include <unistd.h>
#endif

#include "catch.h"

#include "Options.h"

class OptionsExtenderMock : public Options::Extender
{
public:
	int					m_iNewsServers;
	int					m_iFeeds;
	int					m_iTasks;

protected:
	virtual void		AddNewsServer(int iID, bool bActive, const char* szName, const char* szHost,
							int iPort, const char* szUser, const char* szPass, bool bJoinGroup,
							bool bTLS, const char* szCipher, int iMaxConnections, int iRetention,
							int iLevel, int iGroup)
	{
		m_iNewsServers++;
	}

	virtual void		AddFeed(int iID, const char* szName, const char* szUrl, int iInterval,
								const char* szFilter, bool bPauseNzb, const char* szCategory, int iPriority)
	{
		m_iFeeds++;
	}

	virtual void		AddTask(int iID, int iHours, int iMinutes, int iWeekDaysBits, Options::ESchedulerCommand eCommand, const char* szParam)
	{
		m_iTasks++;
	}

public:
						OptionsExtenderMock() : m_iNewsServers(0), m_iFeeds(0), m_iTasks(0) {}
};

TEST_CASE("Options: initializing without configuration file", "[Options][Quick]")
{
	Options options(NULL, NULL);

	REQUIRE(options.GetConfigFilename() == NULL);
#ifdef WIN32
	REQUIRE(strcmp(options.GetTempDir(), "nzbget/tmp") == 0);
#else
	REQUIRE(strcmp(options.GetTempDir(), "~/downloads/tmp") == 0);
#endif
}

TEST_CASE("Options: passing command line options", "[Options][Quick]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ControlUsername=my-user-name-1");
	cmdOpts.push_back("ControlUsername=my-user-name-2");

	Options options(&cmdOpts, NULL);

	REQUIRE(options.GetConfigFilename() == NULL);
	REQUIRE(strcmp(options.GetControlUsername(), "my-user-name-2") == 0);
}

TEST_CASE("Options: calling extender", "[Options][Quick]")
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("Server1.Host=news.mynewsserver.com");
	cmdOpts.push_back("Server1.Port=119");
	cmdOpts.push_back("Server1.Connections=4");

	cmdOpts.push_back("Server2.Host=news1.mynewsserver.com");
	cmdOpts.push_back("Server2.Port=563");
	cmdOpts.push_back("Server2.Connections=2");

	cmdOpts.push_back("Feed1.Url=http://my.feed.com");

	cmdOpts.push_back("Task1.Time=*:00");
	cmdOpts.push_back("Task1.WeekDays=1-7");
	cmdOpts.push_back("Task1.Command=pausedownload");

	OptionsExtenderMock extender;
	Options options(&cmdOpts, &extender);

	REQUIRE(extender.m_iNewsServers == 2);
	REQUIRE(extender.m_iFeeds == 1);
	REQUIRE(extender.m_iTasks == 24);
}
