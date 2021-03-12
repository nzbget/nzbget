/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2016-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
		const char* secureCert, const char* secureKey, int latency, int speed, NntpCache* cache) :
		m_id(id), m_serverId(serverId), m_dataDir(dataDir), m_cacheDir(cacheDir),
		m_secureCert(secureCert), m_secureKey(secureKey), m_latency(latency),
		m_speed(speed), m_cache(cache) {}
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
	int m_latency;
	int m_speed;
	const char* m_messageid;
	CString m_filename;
	int m_part;
	int64 m_offset;
	int m_size;
	bool m_sendHeaders;
	int64 m_start;
	NntpCache* m_cache;

	void ServArticle();
	void SendSegment();
	bool ServerInList(const char* servList);
	void SendData(const char* buffer, int size);
};


void NntpServer::Run()
{
	debug("Entering NntpServer-loop");

	info("Listening on port %i", m_port);

#ifdef WIN32
	if (m_speed > 0)
	{
		timeBeginPeriod(1);
	}
#endif

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
			Util::Sleep(500);
			continue;
		}
		
		NntpProcessor* commandThread = new NntpProcessor(num++, m_id, m_dataDir,
			m_cacheDir, m_secureCert, m_secureKey, m_latency, m_speed, m_cache);
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

	info("[%i] Incoming connection from: %s", m_id, m_connection->GetHost() );
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
			m_connection->WriteLine(CString::FormatStr("211 0 0 0 %s\r\n", line + 6));
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

	if (m_latency)
	{
		Util::Sleep(m_latency);
	}

	bool ok = false;

	const char* from = strchr(m_messageid, '?');
	const char* off = strchr(m_messageid, '=');
	const char* to = strchr(m_messageid, ':');
	const char* end = strchr(m_messageid, '>');
	const char* serv = strchr(m_messageid, '!');

	if (from && off && to && end)
	{
		m_filename.Set(m_messageid + 1, (int)(from - m_messageid - 1));
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
	detail("[%i] Sending segment %s (%i=%" PRIi64 ":%i)", m_id, *m_filename, m_part, m_offset, m_size);

	if (m_speed > 0)
	{
		m_start = Util::CurrentTicks();
	}

	BString<1024> fullFilename("%s/%s", m_dataDir, *m_filename);
	BString<1024> cacheFileDir("%s/%s", m_cacheDir, *m_filename);
	BString<1024> cacheFileName("%i=%" PRIi64 "-%i", m_part, m_offset, m_size);
	BString<1024> cacheFullFilename("%s/%s", *cacheFileDir, *cacheFileName);
	BString<1024> cacheKey("%s/%s", *m_filename, *cacheFileName);

	const char* cachedData = nullptr;
	int cachedSize;
	if (m_cache)
	{
		m_cache->Find(cacheKey, cachedData, cachedSize);
	}

	DiskFile cacheFile;
	bool readCache = !cachedData && m_cacheDir && cacheFile.Open(cacheFullFilename, DiskFile::omRead);
	bool writeCache = !cachedData && m_cacheDir && !readCache;
	StringBuilder cacheMem;
	if (m_cache && !cachedData)
	{
		cacheMem.Reserve((int)(m_size * 1.1));
	}

	CString errmsg;
	if (writeCache && !FileSystem::ForceDirectories(cacheFileDir, errmsg))
	{
		error("Could not create directory %s: %s", *cacheFileDir, *errmsg);
	}

	if (writeCache && !cacheFile.Open(cacheFullFilename, DiskFile::omWrite))
	{
		error("Could not create file %s: %s", *cacheFullFilename, *FileSystem::GetLastErrorMessage());
	}

	if (!cachedData && !readCache && !FileSystem::FileExists(fullFilename))
	{
		m_connection->WriteLine(CString::FormatStr("430 Article not found\r\n"));
		return;
	}

	YEncoder encoder(fullFilename, m_part, m_offset, m_size, 
		[proc = this, writeCache, &cacheFile, &cacheMem](const char* buf, int size)
		{
			if (proc->m_cache)
			{
				cacheMem.Append(buf);
			}
			if (writeCache)
			{
				cacheFile.Write(buf, size);
			}
			proc->SendData(buf, size);
		});

	if (!cachedData && !readCache && !encoder.OpenFile(errmsg))
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

	if (cachedData)
	{
		SendData(cachedData, cachedSize);
	}
	else if (readCache)
	{
		cacheFile.Seek(0, DiskFile::soEnd);
		int size = (int)cacheFile.Position();
		CharBuffer buf(size);
		cacheFile.Seek(0);
		if (cacheFile.Read((char*)buf, size) != size)
		{
			error("Could not read file %s: %s", *cacheFullFilename, *FileSystem::GetLastErrorMessage());
		}
		if (m_cache)
		{
			cacheMem.Append(buf, size);
		}
		SendData(buf, size);
	}
	else
	{
		encoder.WriteSegment();
	}

	if (!cachedData && cacheMem.Length() > 0)
	{
		m_cache->Append(cacheKey, cacheMem, cacheMem.Length());
	}

	m_connection->WriteLine(".\r\n");
}

void NntpProcessor::SendData(const char* buffer, int size)
{
	if (m_speed == 0)
	{
		m_connection->Send(buffer, size);
		return;
	}

	int64 expectedTime = (int64)1000 * size / (m_speed * 1024) - (Util::CurrentTicks() - m_start) / 1000;

	int chunkNum = 21;
	int chunkSize = size;
	int pause = 0;

	while (pause < 1 && chunkNum > 1)
	{
		chunkNum--;
		chunkSize = size / chunkNum;
		pause = (int)(expectedTime / chunkNum);
	}

	int sent = 0;
	for (int i = 0; i < chunkNum; i++)
	{
		int len = sent + chunkSize < size ? chunkSize : size - sent;

		while (sent + len < size && *(buffer + sent + len) != '\r')
		{
			len++;
		}
		
		m_connection->Send(buffer + sent, len);
		int64 now = Util::CurrentTicks();
		if (now + pause * 1000 < m_start + expectedTime * 1000)
		{
			Util::Sleep(pause);
		}
		sent += len;
	}
}


void NntpCache::Append(const char* key, const char* data, int len)
{
	Guard guard(m_lock);
	if (!len)
	{
		len = strlen(data);
	}
	m_items.emplace(key, std::make_unique<CacheItem>(key, data, len));
}

bool NntpCache::Find(const char* key, const char*& data, int& size)
{
	Guard guard(m_lock);

	CacheMap::iterator pos = m_items.find(key);
	if (pos != m_items.end())
	{
		data = (*pos).second->m_data;
		size = (*pos).second->m_size;
		return true;
	}

	return false;
}
