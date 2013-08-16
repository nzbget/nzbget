/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include <vector>

class NewsServer
{
private:
	int				m_iID;
	int				m_iStateID;
	bool			m_bActive;
	char*			m_szName;
	int				m_iGroup;
	char*			m_szHost;
	int				m_iPort;
	char*			m_szUser;
	char*			m_szPassword;
	int				m_iMaxConnections;
	int				m_iLevel;
	int				m_iNormLevel;
	bool			m_bJoinGroup;
	bool			m_bTLS;
	char*			m_szCipher;

public:
					NewsServer(int iID, bool bActive, const char* szName, const char* szHost, int iPort,
						const char* szUser, const char* szPass, bool bJoinGroup,
						bool bTLS, const char* szCipher, int iMaxConnections, int iLevel, int iGroup);
					~NewsServer();
	int				GetID() { return m_iID; }
	int				GetStateID() { return m_iStateID; }
	void			SetStateID(int iStateID) { m_iStateID = iStateID; }
	bool			GetActive() { return m_bActive; }
	void			SetActive(bool bActive) { m_bActive = bActive; }
	const char*		GetName() { return m_szName; }
	int				GetGroup() { return m_iGroup; }
	const char*		GetHost() { return m_szHost; }
	int				GetPort() { return m_iPort; }
	const char*		GetUser() { return m_szUser; }
	const char*		GetPassword() { return m_szPassword; }
	int				GetMaxConnections() { return m_iMaxConnections; }
	int				GetLevel() { return m_iLevel; }
	int				GetNormLevel() { return m_iNormLevel; }
	void			SetNormLevel(int iLevel) { m_iNormLevel = iLevel; }
	int				GetJoinGroup() { return m_bJoinGroup; }
	bool			GetTLS() { return m_bTLS; }
	const char*		GetCipher() { return m_szCipher; }
};

typedef std::vector<NewsServer*>		Servers;

#endif
