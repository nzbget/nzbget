/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2004  Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007  Andrei Prygounkov <hugbug@users.sourceforge.net>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Revision$
 * $Date$
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "NewsServer.h"
#include "Log.h"

NewsServer::NewsServer(const char* host, int port, const char* user, const char* pass, int maxConnections, int level) : NetAddress(host, port)
{
	m_szUser = NULL;
	m_szPassword = NULL;
	m_iLevel = level;
	m_iMaxConnections = maxConnections;

	if (pass)
	{
		m_szPassword = strdup(pass);
	}
	if (user)
	{
		m_szUser = strdup(user);
	}
}

NewsServer::~NewsServer()
{
	free(m_szUser);
	m_szUser = NULL;
	free(m_szPassword);
	m_szPassword = NULL;
}
