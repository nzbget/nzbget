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

#include "FeedFilter.h"

void TestFilter(FeedItemInfo* feedItemInfo, const char* filterDef, FeedItemInfo::EMatchStatus expectedMatch)
{
	feedItemInfo->SetMatchStatus(FeedItemInfo::msIgnored);
	feedItemInfo->SetMatchRule(0);

	FeedFilter filter(filterDef);
	filter.Match(*feedItemInfo);

	INFO(filterDef);
	REQUIRE(feedItemInfo->GetMatchStatus() == expectedMatch);
}

TEST_CASE("Feed filter: one liners", "[FeedFilter][Quick]")
{
	FeedItemInfo item;
	item.SetTitle("Game.of.Clowns.S02E06.REAL.1080p.HDTV.X264-Group.WEB-DL");
	item.SetFilename("Game.of.Clowns.S02E06.REAL.1080p.HDTV.X264-Group.WEB-DL");
	item.SetSize(1600*1024*1024);
	item.SetTime(Util::CurrentTime() - 60*60*15);   // age: 15 hours
	item.SetCategory("TV > HD");
	item.SetRageId(123456);
	item.SetSeason("02");
	item.SetEpisode("06");

	TestFilter(&item, "game", FeedItemInfo::msAccepted);
	TestFilter(&item, "games", FeedItemInfo::msIgnored);
	TestFilter(&item, "gam", FeedItemInfo::msIgnored);
	TestFilter(&item, "gam*", FeedItemInfo::msAccepted);
	TestFilter(&item, "am*", FeedItemInfo::msIgnored);
	TestFilter(&item, "*am*", FeedItemInfo::msAccepted);
	TestFilter(&item, "game of clowns", FeedItemInfo::msAccepted);
	TestFilter(&item, "game of clown", FeedItemInfo::msIgnored);
	TestFilter(&item, "game of clown*", FeedItemInfo::msAccepted);
	TestFilter(&item, "game.of?clowns", FeedItemInfo::msAccepted);
	TestFilter(&item, "game*of*clowns", FeedItemInfo::msIgnored);
	TestFilter(&item, "game*of*clowns*", FeedItemInfo::msIgnored);
	TestFilter(&item, "*game*of*clowns*", FeedItemInfo::msAccepted);
	TestFilter(&item, "game of clowns WEB-DL", FeedItemInfo::msAccepted);
	TestFilter(&item, "game of clowns -WEB-DL", FeedItemInfo::msIgnored);
	TestFilter(&item, "+title:@game", FeedItemInfo::msAccepted);
	TestFilter(&item, "-game", FeedItemInfo::msIgnored);
	TestFilter(&item, "-kings", FeedItemInfo::msAccepted);
	TestFilter(&item, "title:game.of.clowns size:<4GB", FeedItemInfo::msAccepted);
	TestFilter(&item, "title:game.of?clowns", FeedItemInfo::msAccepted);
	TestFilter(&item, "@game", FeedItemInfo::msAccepted);
	TestFilter(&item, "size:<1.4GB", FeedItemInfo::msIgnored);
	TestFilter(&item, "HDTV category:*hd* -badgroup s02e* size:>600MB size:<2000MB", FeedItemInfo::msAccepted);
	TestFilter(&item, "size:<2000MB", FeedItemInfo::msAccepted);
	TestFilter(&item, "age:<10", FeedItemInfo::msAccepted);
	TestFilter(&item, "age:<=10", FeedItemInfo::msAccepted);
	TestFilter(&item, "age:>1h", FeedItemInfo::msAccepted);
	TestFilter(&item, "age:>=1h", FeedItemInfo::msAccepted);
	TestFilter(&item, "$game.*\\.s02e[0-9]*\\..*", FeedItemInfo::msAccepted);
	TestFilter(&item, "R:  game  of", FeedItemInfo::msRejected);
	TestFilter(&item, "A(category:my series, pause:yes, priority:100):game", FeedItemInfo::msAccepted);
	TestFilter(&item, "A(c:my series, p:n, r:100, s:1000, k:1080p):game", FeedItemInfo::msAccepted);
	TestFilter(&item, "A(my series, paused, 100):game", FeedItemInfo::msAccepted);
	TestFilter(&item, "A(k:series=GOT-${1}-${2}): Game of clowns S##E##", FeedItemInfo::msAccepted);
	TestFilter(&item, "A(k:series=GOT-${1}-${2}): $.+S([0-9]{1,2})E([0-9]{1,2})", FeedItemInfo::msAccepted);
}
