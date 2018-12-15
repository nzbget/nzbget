/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include "NString.h"

class NewsServer
{
public:
	NewsServer(int id, bool active, const char* name, const char* host, int port, int ipVersion,
		const char* user, const char* pass, bool joinGroup,
		bool tls, const char* cipher, int maxConnections, int retention,
		int level, int group, bool optional);
	int GetId() { return m_id; }
	int GetStateId() { return m_stateId; }
	void SetStateId(int stateId) { m_stateId = stateId; }
	bool GetActive() { return m_active; }
	void SetActive(bool active) { m_active = active; }
	const char* GetName() { return m_name; }
	int GetGroup() { return m_group; }
	const char* GetHost() { return m_host; }
	int GetPort() { return m_port; }
	int GetIpVersion() { return m_ipVersion; }
	const char* GetUser() { return m_user; }
	const char* GetPassword() { return m_password; }
	int GetMaxConnections() { return m_maxConnections; }
	int GetLevel() { return m_level; }
	int GetNormLevel() { return m_normLevel; }
	void SetNormLevel(int level) { m_normLevel = level; }
	int GetJoinGroup() { return m_joinGroup; }
	bool GetTls() { return m_tls; }
	const char* GetCipher() { return m_cipher; }
	int GetRetention() { return m_retention; }
	bool GetOptional() { return m_optional; }
	time_t GetBlockTime() { return m_blockTime; }
	void SetBlockTime(time_t blockTime) { m_blockTime = blockTime; }

private:
	int m_id;
	int m_stateId = 0;
	bool m_active;
	CString m_name;
	CString m_host;
	int m_port;
	int m_ipVersion;
	CString m_user;
	CString m_password;
	bool m_joinGroup;
	bool m_tls;
	CString m_cipher;
	int m_maxConnections;
	int m_retention;
	int m_level;
	int m_normLevel;
	int m_group;
	bool m_optional = false;
	time_t m_blockTime = 0;
};

typedef std::vector<std::unique_ptr<NewsServer>> Servers;

#endif
