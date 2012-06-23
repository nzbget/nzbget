/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2012 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 * $Revision: 1 $
 * $Date: 2012-04-12 12:00:00 +0200 (Do, 12 April 2012) $
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
#include <cstdio>
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

	EStatus Status = adFailed;

	while (!IsStopped() && iRemainedDownloadRetries > 0 && iRemainedConnectRetries > 0)
	{
		SetLastUpdateTimeNow();

		Status = Download();

		if ((((Status == adFailed) && (iRemainedDownloadRetries > 1)) ||
			((Status == adConnectError) && (iRemainedConnectRetries > 1)))
			&& !IsStopped() && !(g_pOptions->GetPauseDownload() || g_pOptions->GetPauseDownload2()))
		{
			detail("Waiting %i sec to retry", g_pOptions->GetRetryInterval());
			int msec = 0;
			while (!IsStopped() && (msec < g_pOptions->GetRetryInterval() * 1000) && 
				!(g_pOptions->GetPauseDownload() || g_pOptions->GetPauseDownload2()))
			{
				usleep(100 * 1000);
				msec += 100;
			}
		}

		if (IsStopped() || g_pOptions->GetPauseDownload() || g_pOptions->GetPauseDownload2())
		{
			Status = adRetry;
			break;
		}

		if (Status == adFinished || Status == adFatalError || Status == adNotFound)
		{
			break;
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

	if (!url.IsValid())
	{
		error("URL is not valid: %s", url.GetAddress());
		return adFatalError;
	}

	int iPort = url.GetPort();
	if (iPort == 0 && !strcasecmp(url.GetProtocol(), "http"))
	{
		iPort = 80;
	}
	if (iPort == 0 && !strcasecmp(url.GetProtocol(), "https"))
	{
		iPort = 443;
	}

	if (strcasecmp(url.GetProtocol(), "http") && strcasecmp(url.GetProtocol(), "https"))
	{
		error("Unsupported protocol in URL: %s", url.GetAddress());
		return adFatalError;
	}

#ifdef DISABLE_TLS
	if (!strcasecmp(url.GetProtocol(), "https"))
	{
		error("Program was compiled without TLS/SSL-support. Cannot download using https protocol. URL: %s", url.GetAddress());
		return adFatalError;
	}
#endif

	bool bTLS = !strcasecmp(url.GetProtocol(), "https");

	m_pConnection = new Connection(url.GetHost(), iPort, bTLS);
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

	char tmp[1024];

	// retrieve file
	snprintf(tmp, 1024, "GET %s HTTP/1.1\r\n", url.GetResource());
	tmp[1024-1] = '\0';
	m_pConnection->WriteLine(tmp);

	snprintf(tmp, 1024, "User-Agent: nzbget/%s\r\n", Util::VersionRevision());
	tmp[1024-1] = '\0';
	m_pConnection->WriteLine(tmp);

	snprintf(tmp, 1024, "Host: %s\r\n", url.GetHost());
	tmp[1024-1] = '\0';
	m_pConnection->WriteLine(tmp);

	m_pConnection->WriteLine("Accept: */*\r\n");
	m_pConnection->WriteLine("Connection: close\r\n");

	m_pConnection->WriteLine("\r\n");

	m_bConfirmedLength = false;
	m_pOutFile = NULL;
	bool bBody = false;
	bool bEnd = false;
	const int LineBufSize = 1024*10;
	char* szLineBuf = (char*)malloc(LineBufSize);
	m_iContentLen = -1;
	int iWrittenLen = 0;
	Status = adRunning;
	bool bFirstLine = true;

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
			if (m_iContentLen == -1)
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

		if (bBody)
		{
			// write to output file
			if (!Write(line, iLen))
			{
				Status = adFatalError;
				break;
			}
			iWrittenLen += iLen;

			//detect end of file
			if (iWrittenLen == m_iContentLen)
			{
				bEnd = true;
				break;
			}
		}
		else
		{
			debug("Header: %s", line);

			// detect body of article
			if (*line == '\r' || *line == '\n')
			{
				bBody = true;

				if (m_iContentLen == -1)
				{
					warn("URL %s: Content-Length is not submitted by server, cannot verify whether the file is complete", m_szInfoName);
				}
			}

			ProcessHeader(line);
		}
	}

	free(szLineBuf);

	if (m_pOutFile)
	{
		fclose(m_pOutFile);
	}

	if (!bEnd && Status == adRunning && !IsStopped())
	{
		warn("URL %s failed: file incomplete", m_szInfoName);
		Status = adFailed;
	}

	if (IsStopped())
	{
		Status = adFailed;
	}

	FreeConnection();

	if (bEnd)
	{
		Status = adFinished;
	}

	if (Status != adFinished)
	{
		// Download failed, delete broken output file
		remove(m_szOutputFilename);
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
	if (!strncmp(szLine, "Content-Length: ", 16))
	{
		m_iContentLen = atoi(szLine + 16);
		m_bConfirmedLength = true;
	}

	if (!strncmp(szLine, "Content-Disposition: ", 21))
	{
		ParseFilename(szLine);
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

bool WebDownloader::Write(char* szLine, int iLen)
{
	if (!m_pOutFile && !PrepareFile())
	{
		return false;
	}

	return fwrite(szLine, 1, iLen, m_pOutFile) > 0;
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
