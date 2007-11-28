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

#include "nzbget.h"
#include "Log.h"
#include "NNTPConnection.h"
#include "Connection.h"
#include "NewsServer.h"

NNTPConnection::NNTPConnection(NewsServer* server) : Connection(server)
{
	m_UnavailableGroups.clear();
	m_szActiveGroup = NULL;
	m_szLineBuf = (char*)malloc(LineBufSize);
}

NNTPConnection::~NNTPConnection()
{
	for (unsigned int i = 0; i < m_UnavailableGroups.size(); i++)
	{
		free(m_UnavailableGroups[i]);
		m_UnavailableGroups[i] = NULL;
	}
	m_UnavailableGroups.clear();

	if (m_szActiveGroup)
	{
		free(m_szActiveGroup);
	}
	if (m_szLineBuf)
	{
		free(m_szLineBuf);
	}
}

char* NNTPConnection::Request(char* req)
{
	if (!req)
	{
		return NULL;
	}

	WriteLine(req);

	char* answer = ReadLine(m_szLineBuf, LineBufSize);

	if (!answer)
	{
		return NULL;
	}

	if (!strncmp(answer, "480", 3))
	{
		debug("%s requested authorization", m_pNetAddress->GetHost());

		//authentication required!
		if (Authenticate() < 0)
		{
			return NULL;
		}

		//try again
		WriteLine(req);
		answer = ReadLine(m_szLineBuf, LineBufSize);
		return answer;
	}

	return answer;
}

int NNTPConnection::Authenticate()
{
	if ((!((NewsServer*)m_pNetAddress)->GetUser()) ||
	        (!((NewsServer*)m_pNetAddress)->GetPassword()))
	{
		return -1;
	}

	return AuthInfoUser();
}

int NNTPConnection::AuthInfoUser(int iRecur)
{
	if (iRecur > 10)
	{
		return -1;
	}

	char tmp[1024];

	snprintf(tmp, 1024, "AUTHINFO USER %s\r\n", ((NewsServer*)m_pNetAddress)->GetUser());
	tmp[1024-1] = '\0';

	WriteLine(tmp);

	char* answer = ReadLine(m_szLineBuf, LineBufSize);

	if (!answer)
	{
		return -1;
	}

	if (!strncmp(answer, "281", 3))
	{
		debug("authorization for %s successful", m_pNetAddress->GetHost());
		return 0;
	}
	else if (!strncmp(answer, "381", 3))
	{
		return AuthInfoPass(++iRecur);
	}
	else if (!strncmp(answer, "480", 3))
	{
		return AuthInfoUser();
	}

	return -1;
}

int NNTPConnection::AuthInfoPass(int iRecur)
{
	if (iRecur > 10)
	{
		return -1;
	}

	char tmp[1024];

	snprintf(tmp, 1024, "AUTHINFO PASS %s\r\n", ((NewsServer*)m_pNetAddress)->GetPassword());
	tmp[1024-1] = '\0';

	WriteLine(tmp);

	char* szAnswer = ReadLine(m_szLineBuf, LineBufSize);
	if (!szAnswer)
	{
		ReportError("authorization for %s failed: Connection closed by remote host.", m_pNetAddress->GetHost(), 0);
		return -1;
	}
	else if (!strncmp(szAnswer, "2", 1))
	{
		debug("authorization for %s successful", m_pNetAddress->GetHost());
		return 0;
	}
	else if (!strncmp(szAnswer, "381", 3))
	{
		return AuthInfoPass(++iRecur);
	}

	error("authorization for %s failed (Answer: %s)", m_pNetAddress->GetHost(), szAnswer);
	return -1;
}

int NNTPConnection::DoConnect()
{
	debug("Opening connection to %s", GetServer()->GetHost());
	int res = Connection::DoConnect();
	if (res < 0)
		return res;
	char* answer = DoReadLine(m_szLineBuf, LineBufSize);

	if (!answer)
	{
		ReportError("Connection to %s failed: Connection closed by remote host.", m_pNetAddress->GetHost(), 0);
		return -1;
	}


	if (strncmp(answer, "2", 1))
	{
		error("Connection to %s failed. Answer: ", m_pNetAddress->GetHost(), answer);
		return -1;
	}

	debug("Connection to %s established", GetServer()->GetHost());
	return 0;
}

int NNTPConnection::DoDisconnect()
{
	if (m_eStatus == csConnected)
	{
		Request("quit\r\n");
	}
	return Connection::DoDisconnect();
}


int NNTPConnection::JoinGroup(char* grp)
{
	if (!grp)
	{
		debug("joinGroup called with NULL-pointer!!");
		return -1;
	}

	if ((m_szActiveGroup) && (!strcmp(m_szActiveGroup, grp)))
		return 0;

	for (unsigned int i = 0; i < m_UnavailableGroups.size(); i++)
	{
		if (!strcmp(grp, m_UnavailableGroups[i]))
		{
			debug("Group %s unavailable on %s.", grp, this->GetServer()->GetHost());
			return -1;
		}
	}

	char tmp[1024];
	snprintf(tmp, 1024, "GROUP %s\r\n", grp);
	tmp[1024-1] = '\0';

	char* answer = Request(tmp);

	if ((answer) && (!strncmp(answer, "2", 1)))
	{
		debug("Changed group to %s on %s", grp, GetServer()->GetHost());

		if (m_szActiveGroup)
			free(m_szActiveGroup);

		m_szActiveGroup = strdup(grp);
		return 0;
	}

	if (!answer)
	{
		warn("Error changing group on %s: Connection closed by remote host.",
		     GetServer()->GetHost());
		return -1;
	}
	else
	{
		warn("Error changing group on %s to %s: Answer was \"%s\".",
		     GetServer()->GetHost(), grp, answer);
		m_UnavailableGroups.push_back(strdup(grp));
	}

	return -1;
}
