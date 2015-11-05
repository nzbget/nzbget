/*
 *  This file is part of nzbget
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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "nzbget.h"
#include "Log.h"
#include "NntpConnection.h"
#include "Connection.h"
#include "NewsServer.h"

static const int CONNECTION_LINEBUFFER_SIZE = 1024*10;

NntpConnection::NntpConnection(NewsServer* newsServer) : Connection(newsServer->GetHost(), newsServer->GetPort(), newsServer->GetTls())
{
	m_newsServer = newsServer;
	m_activeGroup = NULL;
	m_lineBuf = (char*)malloc(CONNECTION_LINEBUFFER_SIZE);
	m_authError = false;
	SetCipher(newsServer->GetCipher());
}

NntpConnection::~NntpConnection()
{
	free(m_activeGroup);
	free(m_lineBuf);
}

const char* NntpConnection::Request(const char* req)
{
	if (!req)
	{
		return NULL;
	}

	m_authError = false;

	WriteLine(req);

	char* answer = ReadLine(m_lineBuf, CONNECTION_LINEBUFFER_SIZE, NULL);

	if (!answer)
	{
		return NULL;
	}

	if (!strncmp(answer, "480", 3))
	{
		debug("%s requested authorization", GetHost());

		if (!Authenticate())
		{
			return NULL;
		}

		//try again
		WriteLine(req);
		answer = ReadLine(m_lineBuf, CONNECTION_LINEBUFFER_SIZE, NULL);
	}

	return answer;
}

bool NntpConnection::Authenticate()
{
	if (strlen(m_newsServer->GetUser()) == 0 || strlen(m_newsServer->GetPassword()) == 0)
	{
		ReportError("Could not connect to %s: server requested authorization but username/password are not set in settings",
			m_newsServer->GetHost(), false, 0);
		m_authError = true;
		return false;
	}

	m_authError = !AuthInfoUser(0);
	return !m_authError;
}

bool NntpConnection::AuthInfoUser(int recur)
{
	if (recur > 10)
	{
		return false;
	}

	char tmp[1024];
	snprintf(tmp, 1024, "AUTHINFO USER %s\r\n", m_newsServer->GetUser());
	tmp[1024-1] = '\0';

	WriteLine(tmp);

	char* answer = ReadLine(m_lineBuf, CONNECTION_LINEBUFFER_SIZE, NULL);
	if (!answer)
	{
		ReportErrorAnswer("Authorization for %s (%s) failed: Connection closed by remote host", NULL);
		return false;
	}

	if (!strncmp(answer, "281", 3))
	{
		debug("Authorization for %s successful", GetHost());
		return true;
	}
	else if (!strncmp(answer, "381", 3))
	{
		return AuthInfoPass(++recur);
	}
	else if (!strncmp(answer, "480", 3))
	{
		return AuthInfoUser(++recur);
	}

	if (char* p = strrchr(answer, '\r')) *p = '\0'; // remove last CRLF from error message

	if (GetStatus() != csCancelled)
	{
		ReportErrorAnswer("Authorization for %s (%s) failed: %s", answer);
	}
	return false;
}

bool NntpConnection::AuthInfoPass(int recur)
{
	if (recur > 10)
	{
		return false;
	}

	char tmp[1024];
	snprintf(tmp, 1024, "AUTHINFO PASS %s\r\n", m_newsServer->GetPassword());
	tmp[1024-1] = '\0';

	WriteLine(tmp);

	char* answer = ReadLine(m_lineBuf, CONNECTION_LINEBUFFER_SIZE, NULL);
	if (!answer)
	{
		ReportErrorAnswer("Authorization failed for %s (%s): Connection closed by remote host", NULL);
		return false;
	}
	else if (!strncmp(answer, "2", 1))
	{
		debug("Authorization for %s successful", GetHost());
		return true;
	}
	else if (!strncmp(answer, "381", 3))
	{
		return AuthInfoPass(++recur);
	}

	if (char* p = strrchr(answer, '\r')) *p = '\0'; // remove last CRLF from error message

	if (GetStatus() != csCancelled)
	{
		ReportErrorAnswer("Authorization for %s (%s) failed: %s", answer);
	}
	return false;
}

const char* NntpConnection::JoinGroup(const char* grp)
{
	if (m_activeGroup && !strcmp(m_activeGroup, grp))
	{
		// already in group
		strcpy(m_lineBuf, "211 ");
		return m_lineBuf;
	}

	char tmp[1024];
	snprintf(tmp, 1024, "GROUP %s\r\n", grp);
	tmp[1024-1] = '\0';

	const char* answer = Request(tmp);

	if (answer && !strncmp(answer, "2", 1))
	{
		debug("Changed group to %s on %s", grp, GetHost());
		free(m_activeGroup);
		m_activeGroup = strdup(grp);
	}
	else
	{
		debug("Error changing group on %s to %s: %s.", GetHost(), grp, answer);
	}

	return answer;
}

bool NntpConnection::Connect()
{
	debug("Opening connection to %s", GetHost());

	if (m_status == csConnected)
	{
		return true;
	}

	if (!Connection::Connect())
	{
		return false;
	}

	char* answer = ReadLine(m_lineBuf, CONNECTION_LINEBUFFER_SIZE, NULL);

	if (!answer)
	{
		ReportErrorAnswer("Connection to %s (%s) failed: Connection closed by remote host", NULL);
		Disconnect();
		return false;
	}

	if (strncmp(answer, "2", 1))
	{
		ReportErrorAnswer("Connection to %s (%s) failed: %s", answer);
		Disconnect();
		return false;
	}

	if ((strlen(m_newsServer->GetUser()) > 0 && strlen(m_newsServer->GetPassword()) > 0) &&
		!Authenticate())
	{
		return false;
	}

	debug("Connection to %s established", GetHost());

	return true;
}

bool NntpConnection::Disconnect()
{
	if (m_status == csConnected)
	{
		if (!m_broken)
		{
			Request("quit\r\n");
		}
		free(m_activeGroup);
		m_activeGroup = NULL;
	}
	return Connection::Disconnect();
}

void NntpConnection::ReportErrorAnswer(const char* msgPrefix, const char* answer)
{
	char errStr[1024];
	snprintf(errStr, 1024, msgPrefix, m_newsServer->GetName(), m_newsServer->GetHost(), answer);
	errStr[1024-1] = '\0';

	ReportError(errStr, NULL, false, 0);
}
