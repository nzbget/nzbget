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


#ifndef NEWSSERVER_H
#define NEWSSERVER_H

#include "NetAddress.h"

class NewsServer : public NetAddress
{
private:
	char*		m_szUser;
	char*		m_szPassword;
	int			m_iMaxConnections;
	int			m_iLevel;

public:
	NewsServer(const char* host, int port, const char* user, const char* pass, int maxConnections, int level);
	virtual			~NewsServer();
	const char*		GetUser() { return m_szUser; }
	const char*		GetPassword() { return m_szPassword; }
	int				GetMaxConnections() { return m_iMaxConnections; }
	int				GetLevel() { return m_iLevel; }
};

#endif
