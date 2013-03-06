/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2008 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef NEWSSERVER_H
#define NEWSSERVER_H

class NewsServer
{
private:
	char*			m_szHost;
	int				m_iPort;
	char*			m_szUser;
	char*			m_szPassword;
	int				m_iMaxConnections;
	int				m_iLevel;
	bool			m_bJoinGroup;
	bool			m_bTLS;
	char*			m_szCipher;

public:
					NewsServer(const char* szHost, int iPort, const char* szUser, const char* szPass, bool bJoinGroup,
						bool bTLS, const char* szCipher, int iMaxConnections, int iLevel);
					~NewsServer();
	const char*		GetHost() { return m_szHost; }
	int				GetPort() { return m_iPort; }
	const char*		GetUser() { return m_szUser; }
	const char*		GetPassword() { return m_szPassword; }
	int				GetMaxConnections() { return m_iMaxConnections; }
	int				GetLevel() { return m_iLevel; }
	void			SetLevel(int iLevel) { m_iLevel = iLevel; }
	int				GetJoinGroup() { return m_bJoinGroup; }
	bool			GetTLS() { return m_bTLS; }
	const char*		GetCipher() { return m_szCipher; }
};

#endif
