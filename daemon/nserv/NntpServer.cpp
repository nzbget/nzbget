/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "NntpServer.h"
#include "Log.h"
#include "Util.h"
#include "YEncoder.h"

class NntpProcessor : public Thread
{
public:
	NntpProcessor(int id, int serverId, const char* dataDir, const char* cacheDir,
		const char* secureCert, const char* secureKey) :
		m_id(id), m_serverId(serverId), m_dataDir(dataDir), m_cacheDir(cacheDir),
		m_secureCert(secureCert), m_secureKey(secureKey) {}
	~NntpProcessor() { m_connection->Disconnect(); }
	virtual void Run();
	void SetConnection(std::unique_ptr<Connection>&& connection) { m_connection = std::move(connection); }

private:
	int m_id;
	int m_serverId;
	std::unique_ptr<Connection> m_connection;
	const char* m_dataDir;
	const char* m_cacheDir;
	const char* m_secureCert;
	const char* m_secureKey;
	const char* m_messageid;
	CString m_filename;
	int m_part;
	int64 m_offset;
	int m_size;
	bool m_sendHeaders;

	void ServArticle();
	void SendSegment();
	bool ServerInList(const char* servList);
};

void NntpServer::Run()
{
	debug("Entering NntpServer-loop");

	info("Listening on port %i", m_port);

	int num = 1;

	while (!IsStopped())
	{
		bool bind = true;

		if (!m_connection)
		{
			m_connection = std::make_unique<Connection>(m_host, m_port, m_secureCert);
			m_connection->SetTimeout(10);
			m_connection->SetSuppressErrors(false);
			bind = m_connection->Bind();
		}

		// Accept connections and store the new Connection
		std::unique_ptr<Connection> acceptedConnection;
		if (bind)
		{
			acceptedConnection = m_connection->Accept();
		}
		if (!bind || !acceptedConnection)
		{
			// Server could not bind or accept connection, waiting 1/2 sec and try again
			if (IsStopped())
			{
				break;
			}
			m_connection.reset();
			usleep(500 * 1000);
			continue;
		}
		
		NntpProcessor* commandThread = new NntpProcessor(num++, m_id,
			m_dataDir, m_cacheDir, m_secureCert, m_secureKey);
		commandThread->SetAutoDestroy(true);
		commandThread->SetConnection(std::move(acceptedConnection));
		commandThread->Start();
	}

	if (m_connection)
	{
		m_connection->Disconnect();
	}

	debug("Exiting NntpServer-loop");
}

void NntpServer::Stop()
{
	Thread::Stop();
	if (m_connection)
	{
		m_connection->SetSuppressErrors(true);
		m_connection->Cancel();
#ifdef WIN32
		m_connection->Disconnect();
#endif
	}
}


void NntpProcessor::Run()
{
	m_connection->SetSuppressErrors(false);

#ifndef DISABLE_TLS
	if (m_secureCert && !m_connection->StartTls(false, m_secureCert, m_secureKey))
	{
		error("Could not establish secure connection to nntp-client: Start TLS failed");
		return;
	}
#endif

	m_connection->WriteLine("200 Welcome (NServ)\r\n");

	CharBuffer buf(1024);
	int bytesRead = 0;
	while (CString line = m_connection->ReadLine(buf, 1024, &bytesRead))
	{
		line.TrimRight();
		detail("[%i] Received: %s", m_id, *line);

		if (!strncasecmp(line, "ARTICLE ", 8))
		{
			m_messageid = line + 8;
			m_sendHeaders = true;
			ServArticle();
		}
		else if (!strncasecmp(line, "BODY ", 5))
		{
			m_messageid = line + 5;
			m_sendHeaders = false;
			ServArticle();
		}
		else if (!strncasecmp(line, "GROUP ", 6))
		{
			m_connection->WriteLine(CString::FormatStr("211 0 0 0 %s\r\n", line + 7));
		}
		else if (!strncasecmp(line, "AUTHINFO ", 9))
		{
			m_connection->WriteLine("281 Authentication accepted\r\n");
		}
		else if (!strcasecmp(line, "QUIT"))
		{
			detail("[%i] Closing connection", m_id);
			m_connection->WriteLine("205 Connection closing\r\n");
			break;
		}
		else
		{
			warn("[%i] Unknown command: %s", m_id, *line);
			m_connection->WriteLine("500 Unknown command\r\n");
		}
	}

	m_connection->SetGracefull(true);
	m_connection->Disconnect();
}

/* 
 Message-id format:
   <file-path-relative-to-dataDir?xxx=yyy:zzz!1,2,3>
where:
   xxx   - part number (integer)
   xxx   - offset from which to read the files (integer)
   yyy   - size of file block to return (integer)
   1,2,3 - list of server ids, which have the article (optional),
           if the list is given and current server is not in the list
           the "article not found"-error is returned.
 Examples:
	<parchecker/testfile.dat?1=0:50000>	       - return first 50000 bytes starting from beginning
	<parchecker/testfile.dat?2=50000:50000>      - return 50000 bytes starting from offset 50000
	<parchecker/testfile.dat?2=50000:50000!2>    - article is missing on server 1
*/
void NntpProcessor::ServArticle()
{
	detail("[%i] Serving: %s", m_id, m_messageid);

	bool ok = false;

	const char* from = strchr(m_messageid, '?');
	const char* off = strchr(m_messageid, '=');
	const char* to = strchr(m_messageid, ':');
	const char* end = strchr(m_messageid, '>');
	const char* serv = strchr(m_messageid, '!');

	if (from && off && to && end)
	{
		m_filename.Set(m_messageid + 1, from - m_messageid - 1);
		m_part = atoi(from + 1);
		m_offset = atoll(off + 1);
		m_size = atoi(to + 1);

		ok = !serv || ServerInList(serv + 1);

		if (ok)
		{
			SendSegment();
			return;
		}

		if (!ok)
		{
			m_connection->WriteLine("430 No Such Article Found\r\n");
		}
	}
	else
	{
		m_connection->WriteLine("430 No Such Article Found (invalid message id format)\r\n");
	}
}

bool NntpProcessor::ServerInList(const char* servList)
{
	Tokenizer tok(servList, ",");
	while (const char* servid = tok.Next())
	{
		if (atoi(servid) == m_serverId)
		{
			return true;
		}
	}
	return false;
}

void NntpProcessor::SendSegment()
{
	detail("[%i] Sending segment %s (%i=%lli:%i)", m_id, *m_filename, m_part, (long long)m_offset, m_size);

	BString<1024> fullFilename("%s/%s", m_dataDir, *m_filename);
	BString<1024> cacheFileDir("%s/%s", m_cacheDir, *m_filename);
	BString<1024> cacheFileName("%i=%lli-%i", m_part, (long long)m_offset, m_size);
	BString<1024> cacheFullFilename("%s/%s", *cacheFileDir, *cacheFileName);

	DiskFile cacheFile;
	bool readCache = m_cacheDir && cacheFile.Open(cacheFullFilename, DiskFile::omRead);
	bool writeCache = m_cacheDir && !readCache;

	CString errmsg;
	if (writeCache && !FileSystem::ForceDirectories(cacheFileDir, errmsg))
	{
		error("Could not create directory %s: %s", *cacheFileDir, *errmsg);
	}

	if (writeCache && !cacheFile.Open(cacheFullFilename, DiskFile::omWrite))
	{
		error("Could not create file %s: %s", *cacheFullFilename, *FileSystem::GetLastErrorMessage());
	}

	if (!readCache && !FileSystem::FileExists(fullFilename))
	{
		m_connection->WriteLine(CString::FormatStr("430 Article not found\r\n"));
		return;
	}

	YEncoder encoder(fullFilename, m_part, m_offset, m_size, 
		[con = m_connection.get(), writeCache, &cacheFile](const char* buf, int size)
		{
			if (writeCache)
			{
				cacheFile.Write(buf, size);
			}
			con->Send(buf, size);
		});

	if (!readCache && !encoder.OpenFile(errmsg))
	{
		m_connection->WriteLine(CString::FormatStr("403 %s\r\n", *errmsg));
		return;
	}

	m_connection->WriteLine(CString::FormatStr("%i, 0 %s\r\n", m_sendHeaders ? 222 : 220, m_messageid));
	if (m_sendHeaders)
	{
		m_connection->WriteLine(CString::FormatStr("Message-ID: %s\r\n", m_messageid));
		m_connection->WriteLine(CString::FormatStr("Subject: \"%s\"\r\n", FileSystem::BaseFileName(m_filename)));
		m_connection->WriteLine("\r\n");
	}

	if (readCache)
	{
		cacheFile.Seek(0, DiskFile::soEnd);
		int size = (int)cacheFile.Position();
		CharBuffer buf(size);
		cacheFile.Seek(0);
		if (cacheFile.Read((char*)buf, size) != size)
		{
			error("Could not read file %s: %s", *cacheFullFilename, *FileSystem::GetLastErrorMessage());
		}
		m_connection->Send(buf, size);
	}
	else
	{
		encoder.WriteSegment();
	}

	m_connection->WriteLine(".\r\n");
}
