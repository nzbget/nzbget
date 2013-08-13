/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2012-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#ifdef WIN32
#include <direct.h>
#else
#include <unistd.h>
#include <sys/time.h>
#endif
#include <sys/stat.h>
#include <errno.h>

#include "nzbget.h"
#include "WebDownloader.h"
#include "Log.h"
#include "Options.h"
#include "Util.h"

extern Options* g_pOptions;

WebDownloader::WebDownloader()
{
	debug("Creating WebDownloader");

	m_szURL	= NULL;
	m_szOutputFilename	= NULL;
	m_pConnection = NULL;
	m_szInfoName = NULL;
	m_bConfirmedLength = false;
	m_eStatus = adUndefined;
	m_szOriginalFilename = NULL;
	m_bForce = false;
	SetLastUpdateTimeNow();
}

WebDownloader::~WebDownloader()
{
	debug("Destroying WebDownloader");

	if (m_szURL)
	{
		free(m_szURL);
	}
	if (m_szInfoName)
	{
		free(m_szInfoName);
	}
	if (m_szOutputFilename)
	{
		free(m_szOutputFilename);
	}
	if (m_szOriginalFilename)
	{
		free(m_szOriginalFilename);
	}
}

void WebDownloader::SetOutputFilename(const char* v)
{
	m_szOutputFilename = strdup(v);
}

void WebDownloader::SetInfoName(const char* v)
{
	m_szInfoName = strdup(v);
}

void WebDownloader::SetURL(const char * szURL)
{
	if (m_szURL)
	{
		free(m_szURL);
	}
	m_szURL = strdup(szURL);
}

void WebDownloader::SetStatus(EStatus eStatus)
{
	m_eStatus = eStatus;
	Notify(NULL);
}

void WebDownloader::Run()
{
	debug("Entering WebDownloader-loop");

	SetStatus(adRunning);

	int iRemainedDownloadRetries = g_pOptions->GetRetries() > 0 ? g_pOptions->GetRetries() : 1;
	int iRemainedConnectRetries = iRemainedDownloadRetries > 10 ? iRemainedDownloadRetries : 10;
	m_iRedirects = 0;

	EStatus Status = adFailed;

	while (!IsStopped() && iRemainedDownloadRetries > 0 && iRemainedConnectRetries > 0)
	{
		SetLastUpdateTimeNow();

		Status = Download();

		if ((((Status == adFailed) && (iRemainedDownloadRetries > 1)) ||
			((Status == adConnectError) && (iRemainedConnectRetries > 1)))
			&& !IsStopped() && !(!m_bForce && (g_pOptions->GetPauseDownload() || g_pOptions->GetPauseDownload2())))
		{
			detail("Waiting %i sec to retry", g_pOptions->GetRetryInterval());
			int msec = 0;
			while (!IsStopped() && (msec < g_pOptions->GetRetryInterval() * 1000) && 
				!(!m_bForce && (g_pOptions->GetPauseDownload() || g_pOptions->GetPauseDownload2())))
			{
				usleep(100 * 1000);
				msec += 100;
			}
		}

		if (IsStopped() || (!m_bForce && (g_pOptions->GetPauseDownload() || g_pOptions->GetPauseDownload2())))
		{
			Status = adRetry;
			break;
		}

		if (Status == adFinished || Status == adFatalError || Status == adNotFound)
		{
			break;
		}

		if (Status == adRedirect)
		{
			m_iRedirects++;
			if (m_iRedirects > 5)
			{
				warn("Too many redirects for %s", m_szInfoName);
				Status = adFailed;
				break;
			}
		}

		if (Status != adConnectError)
		{
			iRemainedDownloadRetries--;
		}
		else
		{
			iRemainedConnectRetries--;
		}
	}

	if (Status != adFinished && Status != adRetry)
	{
		Status = adFailed;
	}

	if (Status == adFailed)
	{
		if (IsStopped())
		{
			detail("Download %s cancelled", m_szInfoName);
		}
		else
		{
			error("Download %s failed", m_szInfoName);
		}
	}

	if (Status == adFinished)
	{
		detail("Download %s completed", m_szInfoName);
	}

	SetStatus(Status);

	debug("Exiting WebDownloader-loop");
}

WebDownloader::EStatus WebDownloader::Download()
{
	EStatus Status = adRunning;

	URL url(m_szURL);

	Status = CreateConnection(&url);
	if (Status != adRunning)
	{
		return Status;
	}

	m_pConnection->SetSuppressErrors(false);

	// connection
	bool bConnected = m_pConnection->Connect();
	if (!bConnected || IsStopped())
	{
		FreeConnection();
		return adConnectError;
	}

	// Okay, we got a Connection. Now start downloading.
	detail("Downloading %s", m_szInfoName);

	SendHeaders(&url);

	Status = DownloadHeaders();

	if (Status == adRunning)
	{
		Status = DownloadBody();
	}

	if (IsStopped())
	{
		Status = adFailed;
	}

	FreeConnection();

	if (Status != adFinished)
	{
		// Download failed, delete broken output file
		remove(m_szOutputFilename);
	}

	return Status;
}


WebDownloader::EStatus WebDownloader::CreateConnection(URL *pUrl)
{
	if (!pUrl->IsValid())
	{
		error("URL is not valid: %s", pUrl->GetAddress());
		return adFatalError;
	}

	int iPort = pUrl->GetPort();
	if (iPort == 0 && !strcasecmp(pUrl->GetProtocol(), "http"))
	{
		iPort = 80;
	}
	if (iPort == 0 && !strcasecmp(pUrl->GetProtocol(), "https"))
	{
		iPort = 443;
	}

	if (strcasecmp(pUrl->GetProtocol(), "http") && strcasecmp(pUrl->GetProtocol(), "https"))
	{
		error("Unsupported protocol in URL: %s", pUrl->GetAddress());
		return adFatalError;
	}

#ifdef DISABLE_TLS
	if (!strcasecmp(pUrl->GetProtocol(), "https"))
	{
		error("Program was compiled without TLS/SSL-support. Cannot download using https protocol. URL: %s", pUrl->GetAddress());
		return adFatalError;
	}
#endif

	bool bTLS = !strcasecmp(pUrl->GetProtocol(), "https");

	m_pConnection = new Connection(pUrl->GetHost(), iPort, bTLS);

	return adRunning;
}

void WebDownloader::SendHeaders(URL *pUrl)
{
	char tmp[1024];

	// retrieve file
	snprintf(tmp, 1024, "GET %s HTTP/1.0\r\n", pUrl->GetResource());
	tmp[1024-1] = '\0';
	m_pConnection->WriteLine(tmp);

	snprintf(tmp, 1024, "User-Agent: nzbget/%s\r\n", Util::VersionRevision());
	tmp[1024-1] = '\0';
	m_pConnection->WriteLine(tmp);

	snprintf(tmp, 1024, "Host: %s\r\n", pUrl->GetHost());
	tmp[1024-1] = '\0';
	m_pConnection->WriteLine(tmp);

	m_pConnection->WriteLine("Accept: */*\r\n");
#ifndef DISABLE_GZIP
	m_pConnection->WriteLine("Accept-Encoding: gzip\r\n");
#endif
	m_pConnection->WriteLine("Connection: close\r\n");
	m_pConnection->WriteLine("\r\n");
}

WebDownloader::EStatus WebDownloader::DownloadHeaders()
{
	EStatus Status = adRunning;

	m_bConfirmedLength = false;
	const int LineBufSize = 1024*10;
	char* szLineBuf = (char*)malloc(LineBufSize);
	m_iContentLen = -1;
	bool bFirstLine = true;
	m_bGZip = false;
	m_bRedirecting = false;
	m_bRedirected = false;

	// Headers
	while (!IsStopped())
	{
		SetLastUpdateTimeNow();

		int iLen = 0;
		char* line = m_pConnection->ReadLine(szLineBuf, LineBufSize, &iLen);

		if (bFirstLine)
		{
			Status = CheckResponse(szLineBuf);
			if (Status != adRunning)
			{
				break;
			}
			bFirstLine = false;
		}

		// Have we encountered a timeout?
		if (!line)
		{
			if (!IsStopped())
			{
				warn("URL %s failed: Unexpected end of file", m_szInfoName);
			}
			Status = adFailed;
			break;
		}

		debug("Header: %s", line);

		// detect body of response
		if (*line == '\r' || *line == '\n')
		{
			if (m_iContentLen == -1 && !m_bGZip)
			{
				warn("URL %s: Content-Length is not submitted by server, cannot verify whether the file is complete", m_szInfoName);
			}
			break;
		}

		Util::TrimRight(line);
		ProcessHeader(line);

		if (m_bRedirected)
		{
			Status = adRedirect;
			break;
		}
	}

	free(szLineBuf);

	return Status;
}

WebDownloader::EStatus WebDownloader::DownloadBody()
{
	EStatus Status = adRunning;

	m_pOutFile = NULL;
	bool bEnd = false;
	const int LineBufSize = 1024*10;
	char* szLineBuf = (char*)malloc(LineBufSize);
	int iWrittenLen = 0;

#ifndef DISABLE_GZIP
	m_pGUnzipStream = NULL;
	if (m_bGZip)
	{
		m_pGUnzipStream = new GUnzipStream(1024*10);
	}
#endif

	// Body
	while (!IsStopped())
	{
		SetLastUpdateTimeNow();

		char* szBuffer;
		int iLen;
		m_pConnection->ReadBuffer(&szBuffer, &iLen);
		if (iLen == 0)
		{
			iLen = m_pConnection->TryRecv(szLineBuf, LineBufSize);
			szBuffer = szLineBuf;
		}

		// Have we encountered a timeout?
		if (iLen <= 0)
		{
			if (m_iContentLen == -1 && iWrittenLen > 0)
			{
				bEnd = true;
				break;
			}

			if (!IsStopped())
			{
				warn("URL %s failed: Unexpected end of file", m_szInfoName);
			}
			Status = adFailed;
			break;
		}

		// write to output file
		if (!Write(szBuffer, iLen))
		{
			Status = adFatalError;
			break;
		}
		iWrittenLen += iLen;

		//detect end of file
		if (iWrittenLen == m_iContentLen || (m_iContentLen == -1 && m_bGZip && m_bConfirmedLength))
		{
			bEnd = true;
			break;
		}
	}

	free(szLineBuf);

#ifndef DISABLE_GZIP
	if (m_pGUnzipStream)
	{
		delete m_pGUnzipStream;
	}
#endif

	if (m_pOutFile)
	{
		fclose(m_pOutFile);
	}

	if (!bEnd && Status == adRunning && !IsStopped())
	{
		warn("URL %s failed: file incomplete", m_szInfoName);
		Status = adFailed;
	}

	if (bEnd)
	{
		Status = adFinished;
	}

	return Status;
}

WebDownloader::EStatus WebDownloader::CheckResponse(const char* szResponse)
{
	if (!szResponse)
	{
		if (!IsStopped())
		{
			warn("URL %s: Connection closed by remote host", m_szInfoName);
		}
		return adConnectError;
	}

	const char* szHTTPResponse = strchr(szResponse, ' ');
	if (strncmp(szResponse, "HTTP", 4) || !szHTTPResponse)
	{
		warn("URL %s failed: %s", m_szInfoName, szResponse);
		return adFailed;
	}

	szHTTPResponse++;

	if (!strncmp(szHTTPResponse, "400", 3) || !strncmp(szHTTPResponse, "499", 3))
	{
		warn("URL %s failed: %s", m_szInfoName, szHTTPResponse);
		return adConnectError;
	}
	else if (!strncmp(szHTTPResponse, "404", 3))
	{
		warn("URL %s failed: %s", m_szInfoName, szHTTPResponse);
		return adNotFound;
	}
	else if (!strncmp(szHTTPResponse, "301", 3) || !strncmp(szHTTPResponse, "302", 3))
	{
		m_bRedirecting = true;
		return adRunning;
	}
	else if (!strncmp(szHTTPResponse, "200", 3))
	{
		// OK
		return adRunning;
	}
	else 
	{
		// unknown error, no special handling
		warn("URL %s failed: %s", m_szInfoName, szResponse);
		return adFailed;
	}
}

void WebDownloader::ProcessHeader(const char* szLine)
{
	if (!strncasecmp(szLine, "Content-Length: ", 16))
	{
		m_iContentLen = atoi(szLine + 16);
		m_bConfirmedLength = true;
	}
	else if (!strncasecmp(szLine, "Content-Encoding: gzip", 22))
	{
		m_bGZip = true;
	}
	else if (!strncasecmp(szLine, "Content-Disposition: ", 21))
	{
		ParseFilename(szLine);
	}
	else if (m_bRedirecting && !strncasecmp(szLine, "Location: ", 10))
	{
		ParseRedirect(szLine + 10);
		m_bRedirected = true;
	}
}

void WebDownloader::ParseFilename(const char* szContentDisposition)
{
	// Examples:
	// Content-Disposition: attachment; filename="fname.ext"
	// Content-Disposition: attachement;filename=fname.ext
	// Content-Disposition: attachement;filename=fname.ext;
	const char *p = strstr(szContentDisposition, "filename");
	if (!p)
	{
		return;
	}

	p = strchr(p, '=');
	if (!p)
	{
		return;
	}

	p++;

	while (*p == ' ') p++;

	char fname[1024];
	strncpy(fname, p, 1024);
	fname[1024-1] = '\0';

	char *pe = fname + strlen(fname) - 1;
	while ((*pe == ' ' || *pe == '\n' || *pe == '\r' || *pe == ';') && pe > fname) {
		*pe = '\0';
		pe--;
	}

	WebUtil::HttpUnquote(fname);

	if (m_szOriginalFilename)
	{
		free(m_szOriginalFilename);
	}
	m_szOriginalFilename = strdup(Util::BaseFileName(fname));

	debug("OriginalFilename: %s", m_szOriginalFilename);
}

void WebDownloader::ParseRedirect(const char* szLocation)
{
	const char* szNewURL = szLocation;
	char szUrlBuf[1024];
	URL newUrl(szNewURL);
	if (!newUrl.IsValid())
	{
		// relative address
		URL oldUrl(m_szURL);
		if (oldUrl.GetPort() > 0)
		{
			snprintf(szUrlBuf, 1024, "%s://%s:%i%s", oldUrl.GetProtocol(), oldUrl.GetHost(), oldUrl.GetPort(), szNewURL);
		}
		else
		{
			snprintf(szUrlBuf, 1024, "%s://%s%s", oldUrl.GetProtocol(), oldUrl.GetHost(), szNewURL);
		}
		szUrlBuf[1024-1] = '\0';
		szNewURL = szUrlBuf;
	}
	detail("URL %s redirected to %s", m_szURL, szNewURL);
	SetURL(szNewURL);
}

bool WebDownloader::Write(void* pBuffer, int iLen)
{
	if (!m_pOutFile && !PrepareFile())
	{
		return false;
	}

#ifndef DISABLE_GZIP
	if (m_bGZip)
	{
		m_pGUnzipStream->Write(pBuffer, iLen);
		const void *pOutBuf;
		int iOutLen = 1;
		while (iOutLen > 0)
		{
			GUnzipStream::EStatus eGZStatus = m_pGUnzipStream->Read(&pOutBuf, &iOutLen);

			if (eGZStatus == GUnzipStream::zlError)
			{
				error("URL %s: GUnzip failed", m_szInfoName);
				return false;
			}

			if (iOutLen > 0 && fwrite(pOutBuf, 1, iOutLen, m_pOutFile) <= 0)
			{
				return false;
			}

			if (eGZStatus == GUnzipStream::zlFinished)
			{
				m_bConfirmedLength = true;
				return true;
			}
		}
		return true;
	}
	else
#endif

	return fwrite(pBuffer, 1, iLen, m_pOutFile) > 0;
}

bool WebDownloader::PrepareFile()
{
	// prepare file for writing

	const char* szFilename = m_szOutputFilename;
	m_pOutFile = fopen(szFilename, "wb");
	if (!m_pOutFile)
	{
		error("Could not %s file %s", "create", szFilename);
		return false;
	}
	if (g_pOptions->GetWriteBufferSize() > 0)
	{
		setvbuf(m_pOutFile, (char *)NULL, _IOFBF, g_pOptions->GetWriteBufferSize());
	}

	return true;
}

void WebDownloader::LogDebugInfo()
{
	char szTime[50];
#ifdef HAVE_CTIME_R_3
		ctime_r(&m_tLastUpdateTime, szTime, 50);
#else
		ctime_r(&m_tLastUpdateTime, szTime);
#endif

	debug("      Web-Download: status=%i, LastUpdateTime=%s, filename=%s", m_eStatus, szTime, Util::BaseFileName(m_szOutputFilename));
}

void WebDownloader::Stop()
{
	debug("Trying to stop WebDownloader");
	Thread::Stop();
	m_mutexConnection.Lock();
	if (m_pConnection)
	{
		m_pConnection->SetSuppressErrors(true);
		m_pConnection->Cancel();
	}
	m_mutexConnection.Unlock();
	debug("WebDownloader stopped successfully");
}

bool WebDownloader::Terminate()
{
	Connection* pConnection = m_pConnection;
	bool terminated = Kill();
	if (terminated && pConnection)
	{
		debug("Terminating connection");
		pConnection->SetSuppressErrors(true);
		pConnection->Cancel();
		pConnection->Disconnect();
		delete pConnection;
	}
	return terminated;
}

void WebDownloader::FreeConnection()
{
	if (m_pConnection)							
	{
		debug("Releasing connection");
		m_mutexConnection.Lock();
		if (m_pConnection->GetStatus() == Connection::csCancelled)
		{
			m_pConnection->Disconnect();
		}
		delete m_pConnection;
		m_pConnection = NULL;
		m_mutexConnection.Unlock();
	}
}
