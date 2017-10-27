/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "Log.h"
#include "NntpConnection.h"
#include "Connection.h"
#include "NewsServer.h"

static const int CONNECTION_LINEBUFFER_SIZE = 1024*10;

NntpConnection::NntpConnection(NewsServer* newsServer) :
	Connection(newsServer->GetHost(), newsServer->GetPort(), newsServer->GetTls()), m_newsServer(newsServer)
{
	m_lineBuf.Reserve(CONNECTION_LINEBUFFER_SIZE);
	SetCipher(newsServer->GetCipher());
	SetIPVersion(newsServer->GetIpVersion() == 4 ? Connection::ipV4 :
		newsServer->GetIpVersion() == 6 ? Connection::ipV6 : Connection::ipAuto);
}

const char* NntpConnection::Request(const char* req)
{
	if (!req)
	{
		return nullptr;
	}

	m_authError = false;

	WriteLine(req);

	char* answer = ReadLine(m_lineBuf, m_lineBuf.Size(), nullptr);

	if (!answer)
	{
		return nullptr;
	}

	if (!strncmp(answer, "480", 3))
	{
		debug("%s requested authorization", GetHost());

		if (!Authenticate())
		{
			return nullptr;
		}

		//try again
		WriteLine(req);
		answer = ReadLine(m_lineBuf, m_lineBuf.Size(), nullptr);
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

	WriteLine(BString<1024>("AUTHINFO USER %s\r\n", m_newsServer->GetUser()));

	char* answer = ReadLine(m_lineBuf, m_lineBuf.Size(), nullptr);
	if (!answer)
	{
		ReportErrorAnswer("Authorization for %s (%s) failed: Connection closed by remote host", nullptr);
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

	WriteLine(BString<1024>("AUTHINFO PASS %s\r\n", m_newsServer->GetPassword()));

	char* answer = ReadLine(m_lineBuf, m_lineBuf.Size(), nullptr);
	if (!answer)
	{
		ReportErrorAnswer("Authorization failed for %s (%s): Connection closed by remote host", nullptr);
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
	if (!m_activeGroup.Empty() && !strcmp(m_activeGroup, grp))
	{
		// already in group
		strcpy(m_lineBuf, "211 ");
		return m_lineBuf;
	}

	const char* answer = Request(BString<1024>("GROUP %s\r\n", grp));

	if (answer && !strncmp(answer, "2", 1))
	{
		debug("Changed group to %s on %s", grp, GetHost());
		m_activeGroup = grp;
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

	char* answer = ReadLine(m_lineBuf, m_lineBuf.Size(), nullptr);

	if (!answer)
	{
		ReportErrorAnswer("Connection to %s (%s) failed: Connection closed by remote host", nullptr);
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
		Request("quit\r\n");
		m_activeGroup = nullptr;
	}
	return Connection::Disconnect();
}

void NntpConnection::ReportErrorAnswer(const char* msgPrefix, const char* answer)
{
	BString<1024> errStr(msgPrefix, m_newsServer->GetName(), m_newsServer->GetHost(), answer);
	ReportError(errStr, nullptr, false, 0);
}
