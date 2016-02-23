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


#include "nzbget.h"

#include "catch.h"

#include "Util.h"

TEST_CASE("WebUtil: XmlStripTags", "[Util][Quick]")
{
	const char* xml  = "<div><img style=\"margin-left:10px;margin-bottom:10px;float:right;\" src=\"https://xxx/cover.jpg\"/><ul><li>ID: 12345678</li><li>Name: <a href=\"https://xxx/12344\">Show name</a></li><li>Size: 3.00 GB </li><li>Attributes: Category - <a href=\"https://xxx/2040\">Movies > HD</a></li></li></ul></div>";
	const char* text = "                                                                                                        ID: 12345678         Name:                             Show name             Size: 3.00 GB          Attributes: Category -                            Movies > HD                         ";
	char* testString = strdup(xml);
	WebUtil::XmlStripTags(testString);

	REQUIRE(strcmp(testString, text) == 0);

	free(testString);
}

TEST_CASE("WebUtil: XmlDecode", "[Util][Quick]")
{
	const char* xml  = "Poster: Bob &lt;bob@home&gt; bad&mdash;and there&#039;s one thing";
	const char* text = "Poster: Bob <bob@home> bad&mdash;and there's one thing";
	char* testString = strdup(xml);
	WebUtil::XmlDecode(testString);

	REQUIRE(strcmp(testString, text) == 0);

	free(testString);
}

TEST_CASE("WebUtil: XmlRemoveEntities", "[Util][Quick]")
{
	const char* xml  = "Poster: Bob &lt;bob@home&gt; bad&mdash;and there&#039;s one thing";
	const char* text = "Poster: Bob  bob@home  bad and there s one thing";
	char* testString = strdup(xml);
	WebUtil::XmlRemoveEntities(testString);

	REQUIRE(strcmp(testString, text) == 0);

	free(testString);
}

TEST_CASE("WebUtil: URLEncode", "[Util][Quick]")
{
	const char* badUrl = "http://www.example.com/nzb_get/12344/Debian V7 6 64 bit OS.nzb";
	const char* correctedUrl = "http://www.example.com/nzb_get/12344/Debian%20V7%206%2064%20bit%20OS.nzb";
	CString testString = WebUtil::UrlEncode(badUrl);

	REQUIRE(strcmp(testString, correctedUrl) == 0);
}

TEST_CASE("Util: WildMask", "[Util][Quick]")
{
	WildMask mask("*.par2", true);
	REQUIRE_FALSE(mask.Match("Debian V7 6 64 bit OS.nzb"));
	REQUIRE_FALSE(mask.Match("Debian V7 6 64 bit OS.par2.nzb"));
	REQUIRE(mask.Match("Debian V7 6 64 bit OS.par2"));
	REQUIRE(mask.Match(".par2"));
	REQUIRE_FALSE(mask.Match("par2"));
}
