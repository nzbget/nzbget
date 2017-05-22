/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include "ServerPool.h"

void AddTestServer(ServerPool* pool, int id, bool active, int level, bool optional, int group, int connections)
{
	pool->AddServer(std::make_unique<NewsServer>(id, active, nullptr, "", 119, 0,
		"", "", false, false, nullptr, connections, 0, level, group, optional));
}

TEST_CASE("Server pool: simple levels", "[ServerPool]")
{
	ServerPool pool;
	AddTestServer(&pool, 1, true, 2, false, 0, 2);
	pool.InitConnections();
	REQUIRE(pool.GetMaxNormLevel() == 0);

	AddTestServer(&pool, 2, true, 10, false, 0, 3);
	pool.InitConnections();
	REQUIRE(pool.GetMaxNormLevel() == 1);

	NntpConnection* con1 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con2 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con3 = pool.GetConnection(0, nullptr, nullptr);
	REQUIRE(con1 != nullptr);
	REQUIRE(con2 != nullptr);
	REQUIRE(con3 == nullptr);

	pool.FreeConnection(con1, false);
	con3 = pool.GetConnection(0, nullptr, nullptr);
	REQUIRE(con3 != nullptr);

	con1 = pool.GetConnection(1, nullptr, nullptr);
	con2 = pool.GetConnection(1, nullptr, nullptr);
	con3 = pool.GetConnection(1, nullptr, nullptr);
	NntpConnection* con4 = pool.GetConnection(1, nullptr, nullptr);
	REQUIRE(con1 != nullptr);
	REQUIRE(con2 != nullptr);
	REQUIRE(con3 != nullptr);
	REQUIRE(con4 == nullptr);
}

TEST_CASE("Server pool: want server", "[ServerPool]")
{
	ServerPool pool;
	AddTestServer(&pool, 1, true, 0, false, 0, 2);
	AddTestServer(&pool, 2, true, 0, false, 0, 1);
	AddTestServer(&pool, 3, true, 1, false, 0, 3);
	pool.InitConnections();

	NewsServer* serv1 = pool.GetServers()->at(0).get();

	NntpConnection* con1 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con2 = pool.GetConnection(0, serv1, nullptr);
	NntpConnection* con3 = pool.GetConnection(0, serv1, nullptr);
	REQUIRE(con1 != nullptr);
	REQUIRE(con2 != nullptr);
	REQUIRE(con3 == nullptr);
}

TEST_CASE("Server pool: active on/off", "[ServerPool]")
{
	ServerPool pool;
	AddTestServer(&pool, 1, true, 0, false, 0, 2);
	AddTestServer(&pool, 2, true, 0, false, 0, 1);
	pool.InitConnections();

	NntpConnection* con1 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con2 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con3 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con4 = pool.GetConnection(0, nullptr, nullptr);
	REQUIRE(con1 != nullptr);
	REQUIRE(con2 != nullptr);
	REQUIRE(con3 != nullptr);
	REQUIRE(con4 == nullptr);

	pool.FreeConnection(con1, false);
	pool.FreeConnection(con2, false);
	pool.FreeConnection(con3, false);

	REQUIRE(pool.GetGeneration() == 1);

	NewsServer* serv1 = pool.GetServers()->at(0).get();
	serv1->SetActive(false);
	pool.Changed();
	REQUIRE(pool.GetGeneration() == 2);

	con1 = pool.GetConnection(0, nullptr, nullptr);
	con2 = pool.GetConnection(0, nullptr, nullptr);
	con3 = pool.GetConnection(0, nullptr, nullptr);
	REQUIRE(con1 != nullptr);
	REQUIRE(con2 == nullptr);
	REQUIRE(con3 == nullptr);
}

TEST_CASE("Server pool: ignore servers", "[ServerPool]")
{
	ServerPool pool;
	AddTestServer(&pool, 1, true, 0, false, 0, 2);
	AddTestServer(&pool, 2, true, 0, false, 0, 2);
	pool.InitConnections();

	NewsServer* serv1 = pool.GetServers()->at(0).get();
	ServerPool::RawServerList ignoreServers;
	ignoreServers.push_back(serv1);

	NntpConnection* con1 = pool.GetConnection(0, nullptr, &ignoreServers);
	NntpConnection* con2 = pool.GetConnection(0, nullptr, &ignoreServers);
	NntpConnection* con3 = pool.GetConnection(0, nullptr, &ignoreServers);
	NntpConnection* con4 = pool.GetConnection(0, nullptr, &ignoreServers);

	REQUIRE(con1 != nullptr);
	REQUIRE(con2 != nullptr);
	REQUIRE(con3 == nullptr);
	REQUIRE(con4 == nullptr);
}

TEST_CASE("Server pool: ignore servers (grouped)", "[ServerPool]")
{
	ServerPool pool;
	AddTestServer(&pool, 1, true, 0, false, 1, 2);
	AddTestServer(&pool, 2, true, 0, false, 1, 2);
	pool.InitConnections();

	NewsServer* serv1 = pool.GetServers()->at(0).get();
	ServerPool::RawServerList ignoreServers;
	ignoreServers.push_back(serv1);

	NntpConnection* con1 = pool.GetConnection(0, nullptr, &ignoreServers);
	NntpConnection* con2 = pool.GetConnection(0, nullptr, &ignoreServers);
	NntpConnection* con3 = pool.GetConnection(0, nullptr, &ignoreServers);
	NntpConnection* con4 = pool.GetConnection(0, nullptr, &ignoreServers);
	REQUIRE(con1 == nullptr);
	REQUIRE(con2 == nullptr);
	REQUIRE(con3 == nullptr);
	REQUIRE(con4 == nullptr);

	AddTestServer(&pool, 3, true, 0, false, 2, 2);
	pool.InitConnections();

	con1 = pool.GetConnection(0, nullptr, &ignoreServers);
	con2 = pool.GetConnection(0, nullptr, &ignoreServers);
	con3 = pool.GetConnection(0, nullptr, &ignoreServers);
	REQUIRE(con1 != nullptr);
	REQUIRE(con2 != nullptr);
	REQUIRE(con3 == nullptr);
}

TEST_CASE("Server pool: block servers", "[ServerPool]")
{
	int group;
	SECTION("ungrouped") { group = 0; }
	SECTION("grouped") { group = 1; }

	ServerPool pool;
	AddTestServer(&pool, 1, true, 0, false, group, 2);
	AddTestServer(&pool, 2, true, 0, false, group, 2);
	AddTestServer(&pool, 3, true, 1, false, 0, 2);
	pool.InitConnections();
	pool.SetRetryInterval(60);

	NewsServer* serv1 = pool.GetServers()->at(0).get();
	pool.BlockServer(serv1);

	NntpConnection* con1 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con2 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con3 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con4 = pool.GetConnection(0, nullptr, nullptr);
	REQUIRE(con1 != nullptr);
	REQUIRE(con2 != nullptr);
	REQUIRE(con3 == nullptr);
	REQUIRE(con4 == nullptr);
	CHECK(con1->GetNewsServer()->GetLevel() == 0);
	CHECK(con2->GetNewsServer()->GetLevel() == 0);

	pool.FreeConnection(con1, false);
	pool.FreeConnection(con2, false);

	NewsServer* serv2 = pool.GetServers()->at(1).get();
	pool.BlockServer(serv2);

	con1 = pool.GetConnection(0, nullptr, nullptr);
	con2 = pool.GetConnection(0, nullptr, nullptr);
	REQUIRE(con1 == nullptr);
	REQUIRE(con2 == nullptr);
}

TEST_CASE("Server pool: block optional servers", "[ServerPool]")
{
	int group;
	SECTION("ungrouped") { group = 0; }
	SECTION("grouped") { group = 1; }

	ServerPool pool;
	AddTestServer(&pool, 1, true, 0, true, group, 2);
	AddTestServer(&pool, 2, true, 0, true, group, 2);
	AddTestServer(&pool, 3, true, 1, false, 0, 2);
	pool.InitConnections();
	pool.SetRetryInterval(60);

	NewsServer* serv1 = pool.GetServers()->at(0).get();
	NewsServer* serv2 = pool.GetServers()->at(1).get();
	pool.BlockServer(serv1);
	pool.BlockServer(serv2);

	NntpConnection* con1 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con2 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con3 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con4 = pool.GetConnection(0, nullptr, nullptr);

	// all servers on level 0 are optional and blocked;
	// we should get a connection from level-1 server (server 3)
	REQUIRE(con1 != nullptr);
	REQUIRE(con2 != nullptr);
	REQUIRE(con3 == nullptr);
	REQUIRE(con4 == nullptr);
	CHECK(con1->GetNewsServer()->GetLevel() == 1);
	CHECK(con2->GetNewsServer()->GetLevel() == 1);
}

TEST_CASE("Server pool: block optional and non-optional servers", "[ServerPool]")
{
	int group;
	SECTION("ungrouped") { group = 0; }
	SECTION("grouped") { group = 1; }

	ServerPool pool;
	AddTestServer(&pool, 1, true, 0, true, group, 2);
	AddTestServer(&pool, 2, true, 0, false, group, 2);
	AddTestServer(&pool, 3, true, 1, false, 0, 2);
	pool.InitConnections();
	pool.SetRetryInterval(60);

	NewsServer* serv1 = pool.GetServers()->at(0).get();
	NewsServer* serv2 = pool.GetServers()->at(1).get();
	pool.BlockServer(serv1);
	pool.BlockServer(serv2);

	NntpConnection* con1 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con2 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con3 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con4 = pool.GetConnection(0, nullptr, nullptr);

	// all servers on level 0 are blocked but one of them is non-optional
	// we should NOT get any connections
	REQUIRE(con1 == nullptr);
	REQUIRE(con2 == nullptr);
	REQUIRE(con3 == nullptr);
	REQUIRE(con4 == nullptr);
}
