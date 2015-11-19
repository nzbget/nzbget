/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "NewsServer.h"

NewsServer::NewsServer(int id, bool active, const char* name, const char* host, int port,
	const char* user, const char* pass, bool joinGroup, bool tls,
	const char* cipher, int maxConnections, int retention, int level, int group)
{
	m_id = id;
	m_stateId = 0;
	m_active = active;
	m_port = port;
	m_level = level;
	m_normLevel = level;
	m_group = group;
	m_maxConnections = maxConnections;
	m_joinGroup = joinGroup;
	m_tls = tls;
	m_host = strdup(host ? host : "");
	m_user = strdup(user ? user : "");
	m_password = strdup(pass ? pass : "");
	m_cipher = strdup(cipher ? cipher : "");
	m_retention = retention;
	m_blockTime = 0;

	if (name && strlen(name) > 0)
	{
		m_name = strdup(name);
	}
	else
	{
		m_name = (char*)malloc(20);
		snprintf(m_name, 20, "server%i", id);
		m_name[20-1] = '\0';
	}
}

NewsServer::~NewsServer()
{
	free(m_name);
	free(m_host);
	free(m_user);
	free(m_password);
	free(m_cipher);
}
