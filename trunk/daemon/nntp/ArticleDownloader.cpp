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
#ifdef WIN32
#include <direct.h>
#else
#include <unistd.h>
#include <sys/time.h>
#endif
#include <sys/stat.h>
#include <errno.h>

#include "nzbget.h"
#include "ArticleDownloader.h"
#include "ArticleWriter.h"
#include "Decoder.h"
#include "Log.h"
#include "Options.h"
#include "ServerPool.h"
#include "StatMeter.h"
#include "Util.h"

ArticleDownloader::ArticleDownloader()
{
	debug("Creating ArticleDownloader");

	m_szInfoName = NULL;
	m_szConnectionName[0] = '\0';
	m_pConnection = NULL;
	m_eStatus = adUndefined;
	m_eFormat = Decoder::efUnknown;
	m_szArticleFilename = NULL;
	m_iDownloadedSize = 0;
	m_ArticleWriter.SetOwner(this);
	SetLastUpdateTimeNow();
}

ArticleDownloader::~ArticleDownloader()
{
	debug("Destroying ArticleDownloader");

	free(m_szInfoName);
	free(m_szArticleFilename);
}

void ArticleDownloader::SetInfoName(const char* szInfoName)
{
	m_szInfoName = strdup(szInfoName);
	m_ArticleWriter.SetInfoName(m_szInfoName);
}

/*
 * How server management (for one particular article) works:
	- there is a list of failed servers which is initially empty;
	- level is initially 0;

	<loop>
		- request a connection from server pool for current level;
		  Exception: this step is skipped for the very first download attempt, because a
		  level-0 connection is initially passed from queue manager;
		- try to download from server;
		- if connection to server cannot be established or download fails due to interrupted connection,
		  try again (as many times as needed without limit) the same server until connection is OK;
		- if download fails with error "Not-Found" (article or group not found) or with CRC error,
		  add the server to failed server list;
		- if download fails with general failure error (article incomplete, other unknown error
		  codes), try the same server again as many times as defined by option <Retries>; if all attempts
		  fail, add the server to failed server list;
		- if all servers from current level were tried, increase level;
		- if all servers from all levels were tried, break the loop with failure status.
	<end-loop>
*/
void ArticleDownloader::Run()
{
	debug("Entering ArticleDownloader-loop");

	SetStatus(adRunning);

	m_ArticleWriter.SetFileInfo(m_pFileInfo);
	m_ArticleWriter.SetArticleInfo(m_pArticleInfo);
	m_ArticleWriter.Prepare();

	EStatus Status = adFailed;
	int iRetries = g_pOptions->GetRetries() > 0 ? g_pOptions->GetRetries() : 1;
	int iRemainedRetries = iRetries;
	Servers failedServers;
	failedServers.reserve(g_pServerPool->GetServers()->size());
	NewsServer* pWantServer = NULL;
	NewsServer* pLastServer = NULL;
	int iLevel = 0;
	int iServerConfigGeneration = g_pServerPool->GetGeneration();
	bool bForce = m_pFileInfo->GetNZBInfo()->GetForcePriority();

	while (!IsStopped())
	{
		Status = adFailed;

		SetStatus(adWaiting);
		while (!m_pConnection && !(IsStopped() || iServerConfigGeneration != g_pServerPool->GetGeneration()))
		{
			m_pConnection = g_pServerPool->GetConnection(iLevel, pWantServer, &failedServers);
			usleep(5 * 1000);
		}
		SetLastUpdateTimeNow();
		SetStatus(adRunning);

		if (IsStopped() || (g_pOptions->GetPauseDownload() && !bForce) ||
			(g_pOptions->GetTempPauseDownload() && !m_pFileInfo->GetExtraPriority()) ||
			iServerConfigGeneration != g_pServerPool->GetGeneration())
		{
			Status = adRetry;
			break;
		}

		pLastServer = m_pConnection->GetNewsServer();

		m_pConnection->SetSuppressErrors(false);

		snprintf(m_szConnectionName, sizeof(m_szConnectionName), "%s (%s)",
			m_pConnection->GetNewsServer()->GetName(), m_pConnection->GetHost());
		m_szConnectionName[sizeof(m_szConnectionName) - 1] = '\0';

		// check server retention
		bool bRetentionFailure = m_pConnection->GetNewsServer()->GetRetention() > 0 &&
			(time(NULL) - m_pFileInfo->GetTime()) / 86400 > m_pConnection->GetNewsServer()->GetRetention();
		if (bRetentionFailure)
		{
			detail("Article %s @ %s failed: out of server retention (file age: %i, configured retention: %i)",
				m_szInfoName, m_szConnectionName,
				(time(NULL) - m_pFileInfo->GetTime()) / 86400,
				m_pConnection->GetNewsServer()->GetRetention());
			Status = adFailed;
			FreeConnection(true);
		}

		if (m_pConnection && !IsStopped())
		{
			detail("Downloading %s @ %s", m_szInfoName, m_szConnectionName);
		}

		// test connection
		bool bConnected = m_pConnection && m_pConnection->Connect();
		if (bConnected && !IsStopped())
		{
			NewsServer* pNewsServer = m_pConnection->GetNewsServer();

			// Download article
			Status = Download();

			if (Status == adFinished || Status == adFailed || Status == adNotFound || Status == adCrcError)
			{
				m_ServerStats.StatOp(pNewsServer->GetID(), Status == adFinished ? 1 : 0, Status == adFinished ? 0 : 1, ServerStatList::soSet);
			}
		}

		if (m_pConnection)
		{
			AddServerData();
		}

		if (!bConnected && m_pConnection)
		{
			detail("Article %s @ %s failed: could not establish connection", m_szInfoName, m_szConnectionName);
		}

		if (Status == adConnectError)
		{
			bConnected = false;
			Status = adFailed;
		}

		if (bConnected && Status == adFailed)
		{
			iRemainedRetries--;
		}

		if (!bConnected && m_pConnection && !IsStopped())
		{
			g_pServerPool->BlockServer(pLastServer);
		}

		pWantServer = NULL;
		if (bConnected && Status == adFailed && iRemainedRetries > 0 && !bRetentionFailure)
		{
			pWantServer = pLastServer;
		}
		else
		{
			FreeConnection(Status == adFinished || Status == adNotFound);
		}

		if (Status == adFinished || Status == adFatalError)
		{
			break;
		}

		if (IsStopped() || (g_pOptions->GetPauseDownload() && !bForce) ||
			(g_pOptions->GetTempPauseDownload() && !m_pFileInfo->GetExtraPriority()) ||
			iServerConfigGeneration != g_pServerPool->GetGeneration())
		{
			Status = adRetry;
			break;
		}

		if (!pWantServer && (bConnected || bRetentionFailure))
		{
			failedServers.push_back(pLastServer);

			// if all servers from current level were tried, increase level
			// if all servers from all levels were tried, break the loop with failure status

			bool bAllServersOnLevelFailed = true;
			for (Servers::iterator it = g_pServerPool->GetServers()->begin(); it != g_pServerPool->GetServers()->end(); it++)
			{
				NewsServer* pCandidateServer = *it;
				if (pCandidateServer->GetNormLevel() == iLevel)
				{
					bool bServerFailed = !pCandidateServer->GetActive() || pCandidateServer->GetMaxConnections() == 0;
					if (!bServerFailed)
					{
						for (Servers::iterator it = failedServers.begin(); it != failedServers.end(); it++)
						{
							NewsServer* pIgnoreServer = *it;
							if (pIgnoreServer == pCandidateServer ||
								(pIgnoreServer->GetGroup() > 0 && pIgnoreServer->GetGroup() == pCandidateServer->GetGroup() &&
								 pIgnoreServer->GetNormLevel() == pCandidateServer->GetNormLevel()))
							{
								bServerFailed = true;
								break;
							}					
						}
					}
					if (!bServerFailed)
					{
						bAllServersOnLevelFailed = false;
						break;
					}
				}
			}

			if (bAllServersOnLevelFailed)
			{
				if (iLevel < g_pServerPool->GetMaxNormLevel())
				{
					detail("Article %s @ all level %i servers failed, increasing level", m_szInfoName, iLevel);
					iLevel++;
				}
				else
				{
					detail("Article %s @ all servers failed", m_szInfoName);
					Status = adFailed;
					break;
				}
			}
			
			iRemainedRetries = iRetries;
		}
	}

	FreeConnection(Status == adFinished);

	if (m_ArticleWriter.GetDuplicate())
	{
		Status = adFinished;
	}

	if (Status != adFinished && Status != adRetry)
	{
		Status = adFailed;
	}

	if (IsStopped())
	{
		detail("Download %s cancelled", m_szInfoName);
		Status = adRetry;
	}

	if (Status == adFailed)
	{
		detail("Download %s failed", m_szInfoName);
	}

	SetStatus(Status);
	Notify(NULL);

	debug("Exiting ArticleDownloader-loop");
}

ArticleDownloader::EStatus ArticleDownloader::Download()
{
	const char* szResponse = NULL;
	EStatus Status = adRunning;
	m_bWritingStarted = false;
	m_pArticleInfo->SetCrc(0);

	if (m_pConnection->GetNewsServer()->GetJoinGroup())
	{
		// change group
		for (FileInfo::Groups::iterator it = m_pFileInfo->GetGroups()->begin(); it != m_pFileInfo->GetGroups()->end(); it++)
		{
			szResponse = m_pConnection->JoinGroup(*it);
			if (szResponse && !strncmp(szResponse, "2", 1))
			{
				break; 
			}
		}

		Status = CheckResponse(szResponse, "could not join group");
		if (Status != adFinished)
		{
			return Status;
		}
	}

	// retrieve article
	char tmp[1024];
	snprintf(tmp, 1024, "ARTICLE %s\r\n", m_pArticleInfo->GetMessageID());
	tmp[1024-1] = '\0';

	for (int retry = 3; retry > 0; retry--)
	{
		szResponse = m_pConnection->Request(tmp);
		if ((szResponse && !strncmp(szResponse, "2", 1)) || m_pConnection->GetAuthError())
		{
			break;
		}
	}

	Status = CheckResponse(szResponse, "could not fetch article");
	if (Status != adFinished)
	{
		return Status;
	}

	if (g_pOptions->GetDecode())
	{
		m_YDecoder.Clear();
		m_YDecoder.SetCrcCheck(g_pOptions->GetCrcCheck());
		m_UDecoder.Clear();
	}

	bool bBody = false;
	bool bEnd = false;
	const int LineBufSize = 1024*10;
	char* szLineBuf = (char*)malloc(LineBufSize);
	Status = adRunning;

	while (!IsStopped())
	{
		time_t tOldTime = m_tLastUpdateTime;
		SetLastUpdateTimeNow();
		if (tOldTime != m_tLastUpdateTime)
		{
			AddServerData();
		}

		// Throttle the bandwidth
		while (!IsStopped() && (g_pOptions->GetDownloadRate() > 0.0f) &&
			(g_pStatMeter->CalcCurrentDownloadSpeed() > g_pOptions->GetDownloadRate() ||
			g_pStatMeter->CalcMomentaryDownloadSpeed() > g_pOptions->GetDownloadRate()))
		{
			SetLastUpdateTimeNow();
			usleep(10 * 1000);
		}

		int iLen = 0;
		char* line = m_pConnection->ReadLine(szLineBuf, LineBufSize, &iLen);

		g_pStatMeter->AddSpeedReading(iLen);
		if (g_pOptions->GetAccurateRate())
		{
			AddServerData();
		}

		// Have we encountered a timeout?
		if (!line)
		{
			if (!IsStopped())
			{
				detail("Article %s @ %s failed: Unexpected end of article", m_szInfoName, m_szConnectionName);
			}
			Status = adFailed;
			break;
		}

		//detect end of article
		if (!strcmp(line, ".\r\n") || !strcmp(line, ".\n"))
		{
			bEnd = true;
			break;
		}

		//detect lines starting with "." (marked as "..")
		if (!strncmp(line, "..", 2))
		{
			line++;
			iLen--;
		}

		if (!bBody)
		{
			// detect body of article
			if (*line == '\r' || *line == '\n')
			{
				bBody = true;
			}
			// check id of returned article
			else if (!strncmp(line, "Message-ID: ", 12))
			{
				char* p = line + 12;
				if (strncmp(p, m_pArticleInfo->GetMessageID(), strlen(m_pArticleInfo->GetMessageID())))
				{
					if (char* e = strrchr(p, '\r')) *e = '\0'; // remove trailing CR-character
					detail("Article %s @ %s failed: Wrong message-id, expected %s, returned %s", m_szInfoName,
						m_szConnectionName, m_pArticleInfo->GetMessageID(), p);
					Status = adFailed;
					break;
				}
			}
		}
		else if (m_eFormat == Decoder::efUnknown && g_pOptions->GetDecode())
		{
			m_eFormat = Decoder::DetectFormat(line, iLen);
		}

		// write to output file
		if (((bBody && m_eFormat != Decoder::efUnknown) || !g_pOptions->GetDecode()) && !Write(line, iLen))
		{
			Status = adFatalError;
			break;
		}
	}

	free(szLineBuf);

	if (!bEnd && Status == adRunning && !IsStopped())
	{
		detail("Article %s @ %s failed: article incomplete", m_szInfoName, m_szConnectionName);
		Status = adFailed;
	}

	if (IsStopped())
	{
		Status = adFailed;
	}

	if (Status == adRunning)
	{
		FreeConnection(true);
		Status = DecodeCheck();
	}

	if (m_bWritingStarted)
	{
		m_ArticleWriter.Finish(Status == adFinished);
	}

	if (Status == adFinished)
	{
		detail("Successfully downloaded %s", m_szInfoName);
	}

	return Status;
}

ArticleDownloader::EStatus ArticleDownloader::CheckResponse(const char* szResponse, const char* szComment)
{
	if (!szResponse)
	{
		if (!IsStopped())
		{
			detail("Article %s @ %s failed, %s: Connection closed by remote host",
				m_szInfoName, m_szConnectionName, szComment);
		}
		return adConnectError;
	}
	else if (m_pConnection->GetAuthError() || !strncmp(szResponse, "400", 3) || !strncmp(szResponse, "499", 3))
	{
		detail("Article %s @ %s failed, %s: %s", m_szInfoName, m_szConnectionName, szComment, szResponse);
		return adConnectError;
	}
	else if (!strncmp(szResponse, "41", 2) || !strncmp(szResponse, "42", 2) || !strncmp(szResponse, "43", 2))
	{
		detail("Article %s @ %s failed, %s: %s", m_szInfoName, m_szConnectionName, szComment, szResponse);
		return adNotFound;
	}
	else if (!strncmp(szResponse, "2", 1))
	{
		// OK
		return adFinished;
	}
	else 
	{
		// unknown error, no special handling
		detail("Article %s @ %s failed, %s: %s", m_szInfoName, m_szConnectionName, szComment, szResponse);
		return adFailed;
	}
}

bool ArticleDownloader::Write(char* szLine, int iLen)
{
	const char* szArticleFilename = NULL;
	long long iArticleFileSize = 0;
	long long iArticleOffset = 0;
	int iArticleSize = 0;

	if (g_pOptions->GetDecode())
	{
		if (m_eFormat == Decoder::efYenc)
		{
			iLen = m_YDecoder.DecodeBuffer(szLine, iLen);
			szArticleFilename = m_YDecoder.GetArticleFilename();
			iArticleFileSize = m_YDecoder.GetSize();
		}
		else if (m_eFormat == Decoder::efUx)
		{
			iLen = m_UDecoder.DecodeBuffer(szLine, iLen);
			szArticleFilename = m_UDecoder.GetArticleFilename();
		}
		else
		{
			detail("Decoding %s failed: unsupported encoding", m_szInfoName);
			return false;
		}

		if (iLen > 0 && m_eFormat == Decoder::efYenc)
		{
			if (m_YDecoder.GetBegin() == 0 || m_YDecoder.GetEnd() == 0)
			{
				return false;
			}
			iArticleOffset = m_YDecoder.GetBegin() - 1;
			iArticleSize = (int)(m_YDecoder.GetEnd() - m_YDecoder.GetBegin() + 1);
		}
	}

	if (!m_bWritingStarted && iLen > 0)
	{
		if (!m_ArticleWriter.Start(m_eFormat, szArticleFilename, iArticleFileSize, iArticleOffset, iArticleSize))
		{
			return false;
		}
		m_bWritingStarted = true;
	}

	bool bOK = iLen == 0 || m_ArticleWriter.Write(szLine, iLen);

	return bOK;
}

ArticleDownloader::EStatus ArticleDownloader::DecodeCheck()
{
	if (g_pOptions->GetDecode())
	{
		Decoder* pDecoder = NULL;
		if (m_eFormat == Decoder::efYenc)
		{
			pDecoder = &m_YDecoder;
		}
		else if (m_eFormat == Decoder::efUx)
		{
			pDecoder = &m_UDecoder;
		}
		else
		{
			detail("Decoding %s failed: no binary data or unsupported encoding format", m_szInfoName);
			return adFailed;
		}

		Decoder::EStatus eStatus = pDecoder->Check();

		if (eStatus == Decoder::eFinished)
		{
			if (pDecoder->GetArticleFilename())
			{
				free(m_szArticleFilename);
				m_szArticleFilename = strdup(pDecoder->GetArticleFilename());
			}

			if (m_eFormat == Decoder::efYenc)
			{
				m_pArticleInfo->SetCrc(g_pOptions->GetCrcCheck() ?
					m_YDecoder.GetCalculatedCrc() : m_YDecoder.GetExpectedCrc());
			}

			return adFinished;
		}
		else if (eStatus == Decoder::eCrcError)
		{
			detail("Decoding %s failed: CRC-Error", m_szInfoName);
			return adCrcError;
		}
		else if (eStatus == Decoder::eArticleIncomplete)
		{
			detail("Decoding %s failed: article incomplete", m_szInfoName);
			return adFailed;
		}
		else if (eStatus == Decoder::eInvalidSize)
		{
			detail("Decoding %s failed: size mismatch", m_szInfoName);
			return adFailed;
		}
		else if (eStatus == Decoder::eNoBinaryData)
		{
			detail("Decoding %s failed: no binary data found", m_szInfoName);
			return adFailed;
		}
		else
		{
			detail("Decoding %s failed", m_szInfoName);
			return adFailed;
		}
	}
	else 
	{
		return adFinished;
	}
}

void ArticleDownloader::LogDebugInfo()
{
	char szTime[50];
#ifdef HAVE_CTIME_R_3
		ctime_r(&m_tLastUpdateTime, szTime, 50);
#else
		ctime_r(&m_tLastUpdateTime, szTime);
#endif
	info("      Download: Status=%i, LastUpdateTime=%s, InfoName=%s", m_eStatus, szTime, m_szInfoName);
}

void ArticleDownloader::Stop()
{
	debug("Trying to stop ArticleDownloader");
	Thread::Stop();
	m_mutexConnection.Lock();
	if (m_pConnection)
	{
		m_pConnection->SetSuppressErrors(true);
		m_pConnection->Cancel();
	}
	m_mutexConnection.Unlock();
	debug("ArticleDownloader stopped successfully");
}

bool ArticleDownloader::Terminate()
{
	NNTPConnection* pConnection = m_pConnection;
	bool terminated = Kill();
	if (terminated && pConnection)
	{
		debug("Terminating connection");
		pConnection->SetSuppressErrors(true);
		pConnection->Cancel();
		pConnection->Disconnect();
		g_pStatMeter->AddServerData(pConnection->FetchTotalBytesRead(), pConnection->GetNewsServer()->GetID());
		g_pServerPool->FreeConnection(pConnection, true);
	}
	return terminated;
}

void ArticleDownloader::FreeConnection(bool bKeepConnected)
{
	if (m_pConnection)							
	{
		debug("Releasing connection");
		m_mutexConnection.Lock();
		if (!bKeepConnected || m_pConnection->GetStatus() == Connection::csCancelled)
		{
			m_pConnection->Disconnect();
		}
		AddServerData();
		g_pServerPool->FreeConnection(m_pConnection, true);
		m_pConnection = NULL;
		m_mutexConnection.Unlock();
	}
}

void ArticleDownloader::AddServerData()
{
	int iBytesRead = m_pConnection->FetchTotalBytesRead();
	g_pStatMeter->AddServerData(iBytesRead, m_pConnection->GetNewsServer()->GetID());
	m_iDownloadedSize += iBytesRead;
}
