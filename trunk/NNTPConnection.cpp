/*
 *  This file is part of nzbget
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

static const int CONNECTION_LINEBUFFER_SIZE = 1024*10;

NNTPConnection::NNTPConnection(NewsServer* server) : Connection(server)
{
	m_szActiveGroup = NULL;
	m_szLineBuf = (char*)malloc(CONNECTION_LINEBUFFER_SIZE);
	m_bAuthError = false;
}

NNTPConnection::~NNTPConnection()
{
	if (m_szActiveGroup)
	{
		free(m_szActiveGroup);
		m_szActiveGroup = NULL;
	}
	free(m_szLineBuf);
}

const char* NNTPConnection::Request(char* req)
{
	if (!req)
	{
		return NULL;
	}

	m_bAuthError = false;

	WriteLine(req);

	char* answer = ReadLine(m_szLineBuf, CONNECTION_LINEBUFFER_SIZE, NULL);

	if (!answer)
	{
		return NULL;
	}

	if (!strncmp(answer, "480", 3))
	{
		debug("%s requested authorization", m_pNetAddress->GetHost());

		//authentication required!
		if (!Authenticate())
		{
			m_bAuthError = true;
			return NULL;
		}

		//try again
		WriteLine(req);
		answer = ReadLine(m_szLineBuf, CONNECTION_LINEBUFFER_SIZE, NULL);
		return answer;
	}

	return answer;
}

bool NNTPConnection::Authenticate()
{
	if (!((NewsServer*)m_pNetAddress)->GetUser() ||
		!((NewsServer*)m_pNetAddress)->GetPassword())
	{
		return false;
	}

	return AuthInfoUser();
}

bool NNTPConnection::AuthInfoUser(int iRecur)
{
	if (iRecur > 10)
	{
		return false;
	}

	char tmp[1024];
	snprintf(tmp, 1024, "AUTHINFO USER %s\r\n", ((NewsServer*)m_pNetAddress)->GetUser());
	tmp[1024-1] = '\0';

	WriteLine(tmp);

	char* answer = ReadLine(m_szLineBuf, CONNECTION_LINEBUFFER_SIZE, NULL);
	if (!answer)
	{
		ReportError("authorization for %s failed: Connection closed by remote host.", m_pNetAddress->GetHost(), 0);
		return false;
	}

	if (!strncmp(answer, "281", 3))
	{
		debug("authorization for %s successful", m_pNetAddress->GetHost());
		return true;
	}
	else if (!strncmp(answer, "381", 3))
	{
		return AuthInfoPass(++iRecur);
	}
	else if (!strncmp(answer, "480", 3))
	{
		return AuthInfoUser(++iRecur);
	}

	if (char* p = strrchr(answer, '\r')) *p = '\0'; // remove last CRLF from error message

	if (GetStatus() != csCancelled)
	{
		error("authorization for %s failed (Answer: %s)", m_pNetAddress->GetHost(), answer);
	}
	return false;
}

bool NNTPConnection::AuthInfoPass(int iRecur)
{
	if (iRecur > 10)
	{
		return false;
	}

	char tmp[1024];
	snprintf(tmp, 1024, "AUTHINFO PASS %s\r\n", ((NewsServer*)m_pNetAddress)->GetPassword());
	tmp[1024-1] = '\0';

	WriteLine(tmp);

	char* answer = ReadLine(m_szLineBuf, CONNECTION_LINEBUFFER_SIZE, NULL);
	if (!answer)
	{
		ReportError("authorization for %s failed: Connection closed by remote host.", m_pNetAddress->GetHost(), 0);
		return false;
	}
	else if (!strncmp(answer, "2", 1))
	{
		debug("authorization for %s successful", m_pNetAddress->GetHost());
		return true;
	}
	else if (!strncmp(answer, "381", 3))
	{
		return AuthInfoPass(++iRecur);
	}

	if (char* p = strrchr(answer, '\r')) *p = '\0'; // remove last CRLF from error message

	if (GetStatus() != csCancelled)
	{
		error("authorization for %s failed (Answer: %s)", m_pNetAddress->GetHost(), answer);
	}
	return false;
}

const char* NNTPConnection::JoinGroup(char* grp)
{
	if (m_szActiveGroup && !strcmp(m_szActiveGroup, grp))
	{
		// already in group
		strcpy(m_szLineBuf, "211 ");
		return m_szLineBuf;
	}

	char tmp[1024];
	snprintf(tmp, 1024, "GROUP %s\r\n", grp);
	tmp[1024-1] = '\0';

	const char* answer = Request(tmp);
	if (m_bAuthError)
	{
		return answer;
	}

	if (answer && !strncmp(answer, "2", 1))
	{
		debug("Changed group to %s on %s", grp, GetServer()->GetHost());

		if (m_szActiveGroup)
		{
			free(m_szActiveGroup);
		}
		m_szActiveGroup = strdup(grp);
	}
	else
	{
		debug("Error changing group on %s to %s: %s.",
			 GetServer()->GetHost(), grp, answer);
	}

	return answer;
}

int NNTPConnection::DoConnect()
{
	debug("Opening connection to %s", GetServer()->GetHost());
	int res = Connection::DoConnect();
	if (res < 0)
	{
		return res;
	}

	char* answer = DoReadLine(m_szLineBuf, CONNECTION_LINEBUFFER_SIZE, NULL);

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
		if (m_szActiveGroup)
		{
			free(m_szActiveGroup);
			m_szActiveGroup = NULL;
		}
	}
	return Connection::DoDisconnect();
}
