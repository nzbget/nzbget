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

TEST_CASE("Util: RegEx", "[Util][Quick]")
{
	RegEx regExRar(".*\\.rar$");
	RegEx regExRarMultiSeq(".*\\.[r-z][0-9][0-9]$");
	RegEx regExSevenZip(".*\\.7z$|.*\\.7z\\.[0-9]+$");
	RegEx regExNumExt(".*\\.[0-9]+$");

	REQUIRE(regExRar.Match("filename.rar"));
	REQUIRE(regExRar.Match("filename.part001.rar"));
	REQUIRE_FALSE(regExRar.Match("filename.rar.txt"));

	REQUIRE_FALSE(regExRarMultiSeq.Match("filename.rar"));
	REQUIRE(regExRarMultiSeq.Match("filename.r01"));
	REQUIRE(regExRarMultiSeq.Match("filename.r99"));
	REQUIRE_FALSE(regExRarMultiSeq.Match("filename.r001"));
	REQUIRE(regExRarMultiSeq.Match("filename.s01"));
	REQUIRE(regExRarMultiSeq.Match("filename.t99"));

	REQUIRE(regExSevenZip.Match("filename.7z"));
	REQUIRE_FALSE(regExSevenZip.Match("filename.7z.rar"));
	REQUIRE(regExSevenZip.Match("filename.7z.1"));
	REQUIRE(regExSevenZip.Match("filename.7z.001"));
	REQUIRE(regExSevenZip.Match("filename.7z.123"));
	REQUIRE(regExSevenZip.Match("filename.7z.999"));

	REQUIRE(regExNumExt.Match("filename.7z.1"));
	REQUIRE(regExNumExt.Match("filename.7z.9"));
	REQUIRE(regExNumExt.Match("filename.7z.001"));
	REQUIRE(regExNumExt.Match("filename.7z.123"));
	REQUIRE(regExNumExt.Match("filename.7z.999"));

	const char* testStr = "My.Show.Name.S01E02.ABC.720";
	RegEx seasonEpisode(".*S([0-9]+)E([0-9]+).*");
	REQUIRE(seasonEpisode.IsValid());
	REQUIRE(seasonEpisode.Match(testStr));
	REQUIRE(seasonEpisode.GetMatchCount() == 3);
	REQUIRE(seasonEpisode.GetMatchStart(1) == 14);
	REQUIRE(seasonEpisode.GetMatchLen(1) == 2);
}
