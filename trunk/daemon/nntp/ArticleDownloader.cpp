/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "Decoder.h"
#include "Log.h"
#include "Options.h"
#include "ServerPool.h"
#include "StatMeter.h"
#include "Util.h"

extern Options* g_pOptions;
extern ServerPool* g_pServerPool;
extern StatMeter* g_pStatMeter;

ArticleDownloader::ArticleDownloader()
{
	debug("Creating ArticleDownloader");

	m_szResultFilename = NULL;
	m_szTempFilename = NULL;
	m_szArticleFilename = NULL;
	m_szInfoName = NULL;
	m_szOutputFilename = NULL;
	m_pConnection = NULL;
	m_eStatus = adUndefined;
	m_bDuplicate = false;
	m_eFormat = Decoder::efUnknown;
	SetLastUpdateTimeNow();
}

ArticleDownloader::~ArticleDownloader()
{
	debug("Destroying ArticleDownloader");

	free(m_szTempFilename);
	free(m_szArticleFilename);
	free(m_szInfoName);
	free(m_szOutputFilename);
}

void ArticleDownloader::SetTempFilename(const char* v)
{
	m_szTempFilename = strdup(v);
}

void ArticleDownloader::SetOutputFilename(const char* v)
{
	m_szOutputFilename = strdup(v);
}

void ArticleDownloader::SetInfoName(const char * v)
{
	m_szInfoName = strdup(v);
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

	BuildOutputFilename();

	m_szResultFilename = m_pArticleInfo->GetResultFilename();
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

		// test connection
		bool bConnected = m_pConnection && m_pConnection->Connect();
		if (bConnected && !IsStopped())
		{
			NewsServer* pNewsServer = m_pConnection->GetNewsServer();
			detail("Downloading %s @ %s (%s)", m_szInfoName, pNewsServer->GetName(), m_pConnection->GetHost());

			Status = Download();

			if (Status == adFinished || Status == adFailed || Status == adNotFound || Status == adCrcError)
			{
				m_ServerStats.StatOp(pNewsServer->GetID(), Status == adFinished ? 1 : 0, Status == adFinished ? 0 : 1, ServerStatList::soSet);
			}
		}

		if (bConnected)
		{
			if (Status == adConnectError)
			{
				m_pConnection->Disconnect();
				bConnected = false;
				Status = adFailed;
			}
			else
			{
				// freeing connection allows other threads to start.
				// we doing this only if the problem was with article or group.
				// if the problem occurs by connecting or authorization we do not
				// free the connection, to prevent starting of thousands of threads 
				// (cause each of them will also free it's connection after the 
				// same connect-error).
				FreeConnection(Status == adFinished || Status == adNotFound);
			}
		}

		if (m_pConnection)
		{
			g_pStatMeter->AddServerData(m_pConnection->FetchTotalBytesRead(), m_pConnection->GetNewsServer()->GetID());
		}

		if (Status == adFinished || Status == adFatalError)
		{
			break;
		}

		pWantServer = NULL;

		if (bConnected && Status == adFailed)
		{
			iRemainedRetries--;
		}

		if (!bConnected || (Status == adFailed && iRemainedRetries > 0))
		{
			pWantServer = pLastServer;
		}

		if (pWantServer && 
			!(IsStopped() || (g_pOptions->GetPauseDownload() && !bForce) ||
			  (g_pOptions->GetTempPauseDownload() && !m_pFileInfo->GetExtraPriority()) ||
			  iServerConfigGeneration != g_pServerPool->GetGeneration()))
		{
			detail("Waiting %i sec to retry", g_pOptions->GetRetryInterval());
			SetStatus(adWaiting);
			int msec = 0;
			while (!(IsStopped() || (g_pOptions->GetPauseDownload() && !bForce) ||
					 (g_pOptions->GetTempPauseDownload() && !m_pFileInfo->GetExtraPriority()) ||
					 iServerConfigGeneration != g_pServerPool->GetGeneration()) &&
				  msec < g_pOptions->GetRetryInterval() * 1000)
			{
				usleep(100 * 1000);
				msec += 100;
			}
			SetLastUpdateTimeNow();
			SetStatus(adRunning);
		}

		if (IsStopped() || (g_pOptions->GetPauseDownload() && !bForce) ||
			(g_pOptions->GetTempPauseDownload() && !m_pFileInfo->GetExtraPriority()) ||
			iServerConfigGeneration != g_pServerPool->GetGeneration())
		{
			Status = adRetry;
			break;
		}

		if (!pWantServer)
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

	if (m_bDuplicate)
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

	// positive answer!

	if (g_pOptions->GetDecode())
	{
		m_YDecoder.Clear();
		m_YDecoder.SetAutoSeek(g_pOptions->GetDirectWrite());
		m_YDecoder.SetCrcCheck(g_pOptions->GetCrcCheck());

		m_UDecoder.Clear();
	}

	m_pOutFile = NULL;
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
			g_pStatMeter->AddServerData(m_pConnection->FetchTotalBytesRead(), m_pConnection->GetNewsServer()->GetID());
		}

		// Throttle the bandwidth
		while (!IsStopped() && (g_pOptions->GetDownloadRate() > 0.0f) &&
		        (g_pStatMeter->CalcCurrentDownloadSpeed() > g_pOptions->GetDownloadRate()))
		{
			SetLastUpdateTimeNow();
			usleep(10 * 1000);
		}

		int iLen = 0;
		char* line = m_pConnection->ReadLine(szLineBuf, LineBufSize, &iLen);

		g_pStatMeter->AddSpeedReading(iLen);
		if (g_pOptions->GetAccurateRate())
		{
			g_pStatMeter->AddServerData(m_pConnection->FetchTotalBytesRead(), m_pConnection->GetNewsServer()->GetID());
		}

		// Have we encountered a timeout?
		if (!line)
		{
			if (!IsStopped())
			{
				detail("Article %s @ %s (%s) failed: Unexpected end of article", m_szInfoName,
					m_pConnection->GetNewsServer()->GetName(), m_pConnection->GetHost());
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
					detail("Article %s @ %s (%s) failed: Wrong message-id, expected %s, returned %s", m_szInfoName,
						m_pConnection->GetNewsServer()->GetName(), m_pConnection->GetHost(), m_pArticleInfo->GetMessageID(), p);
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

	if (m_pOutFile)
	{
		if (fclose(m_pOutFile) != 0)
		{
			bool bDirectWrite = g_pOptions->GetDirectWrite() && m_eFormat == Decoder::efYenc;
			char szErrBuf[256];
			error("Could not close file %s: %s", (bDirectWrite ? m_szOutputFilename : m_szTempFilename),
				Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
		}
	}

	if (!bEnd && Status == adRunning && !IsStopped())
	{
		detail("Article %s @ %s (%s) failed: article incomplete", m_szInfoName,
			m_pConnection->GetNewsServer()->GetName(), m_pConnection->GetHost());
		Status = adFailed;
	}

	if (IsStopped())
	{
		Status = adFailed;
	}

	if (Status == adRunning)
	{
		FreeConnection(true);
		return DecodeCheck();
	}
	else
	{
		remove(m_szTempFilename);
		return Status;
	}
}

ArticleDownloader::EStatus ArticleDownloader::CheckResponse(const char* szResponse, const char* szComment)
{
	if (!szResponse)
	{
		if (!IsStopped())
		{
			detail("Article %s @ %s (%s) failed, %s: Connection closed by remote host", m_szInfoName, 
				m_pConnection->GetNewsServer()->GetName(), m_pConnection->GetHost(), szComment);
		}
		return adConnectError;
	}
	else if (m_pConnection->GetAuthError() || !strncmp(szResponse, "400", 3) || !strncmp(szResponse, "499", 3))
	{
		detail("Article %s @ %s (%s) failed, %s: %s", m_szInfoName,
			 m_pConnection->GetNewsServer()->GetName(), m_pConnection->GetHost(), szComment, szResponse);
		return adConnectError;
	}
	else if (!strncmp(szResponse, "41", 2) || !strncmp(szResponse, "42", 2) || !strncmp(szResponse, "43", 2))
	{
		detail("Article %s @ %s (%s) failed, %s: %s", m_szInfoName,
			 m_pConnection->GetNewsServer()->GetName(), m_pConnection->GetHost(), szComment, szResponse);
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
		detail("Article %s @ %s (%s) failed, %s: %s", m_szInfoName,
			 m_pConnection->GetNewsServer()->GetName(), m_pConnection->GetHost(), szComment, szResponse);
		return adFailed;
	}
}

bool ArticleDownloader::Write(char* szLine, int iLen)
{
	if (!m_pOutFile && !PrepareFile(szLine))
	{
		return false;
	}

	if (g_pOptions->GetDecode())
	{
		bool bOK = false;
		if (m_eFormat == Decoder::efYenc)
		{
			bOK = m_YDecoder.Write(szLine, iLen, m_pOutFile);
		}
		else if (m_eFormat == Decoder::efUx)
		{
			bOK = m_UDecoder.Write(szLine, iLen, m_pOutFile);
		}
		else
		{
			detail("Decoding %s failed: unsupported encoding", m_szInfoName);
			return false;
		}
		if (!bOK)
		{
			debug("Failed line: %s", szLine);
			detail("Decoding %s failed", m_szInfoName);
		}
		return bOK;
	}
	else
	{
		return fwrite(szLine, 1, iLen, m_pOutFile) > 0;
	}
}

bool ArticleDownloader::PrepareFile(char* szLine)
{
	bool bOpen = false;

	// prepare file for writing
	if (m_eFormat == Decoder::efYenc)
	{
		if (!strncmp(szLine, "=ybegin ", 8))
		{
			if (g_pOptions->GetDupeCheck() &&
				m_pFileInfo->GetNZBInfo()->GetDupeMode() != dmForce &&
				!m_pFileInfo->GetNZBInfo()->GetManyDupeFiles())
			{
				m_pFileInfo->LockOutputFile();
				bool bOutputInitialized = m_pFileInfo->GetOutputInitialized();
				if (!bOutputInitialized)
				{
					char* pb = strstr(szLine, " name=");
					if (pb)
					{
						pb += 6; //=strlen(" name=")
						char* pe;
						for (pe = pb; *pe != '\0' && *pe != '\n' && *pe != '\r'; pe++) ;
						if (!m_szArticleFilename)
						{
							m_szArticleFilename = (char*)malloc(pe - pb + 1);
							strncpy(m_szArticleFilename, pb, pe - pb);
							m_szArticleFilename[pe - pb] = '\0';
						}
					}
				}
				if (!g_pOptions->GetDirectWrite())
				{
					m_pFileInfo->SetOutputInitialized(true);
				}
				m_pFileInfo->UnlockOutputFile();
				if (!bOutputInitialized && m_szArticleFilename &&
					Util::FileExists(m_pFileInfo->GetNZBInfo()->GetDestDir(), m_szArticleFilename))
				{
					m_bDuplicate = true;
					return false;
				}
			}

			if (g_pOptions->GetDirectWrite())
			{
				char* pb = strstr(szLine, " size=");
				if (pb) 
				{
					m_pFileInfo->LockOutputFile();
					if (!m_pFileInfo->GetOutputInitialized())
					{
						pb += 6; //=strlen(" size=")
						long iArticleFilesize = atol(pb);
						if (!CreateOutputFile(iArticleFilesize))
						{
							m_pFileInfo->UnlockOutputFile();
							return false;
						}
						m_pFileInfo->SetOutputInitialized(true);
					}
					m_pFileInfo->UnlockOutputFile();
					bOpen = true;
				}
			}
			else
			{
				bOpen = true;
			}
		}
	}
	else
	{
		bOpen = true;
	}

	if (bOpen)
	{
		bool bDirectWrite = g_pOptions->GetDirectWrite() && m_eFormat == Decoder::efYenc;
		const char* szFilename = bDirectWrite ? m_szOutputFilename : m_szTempFilename;
		m_pOutFile = fopen(szFilename, bDirectWrite ? "rb+" : "wb");
		if (!m_pOutFile)
		{
			char szSysErrStr[256];
			error("Could not %s file %s! Errcode: %i, %s", bDirectWrite ? "open" : "create", szFilename,
				errno, Util::GetLastErrorMessage(szSysErrStr, sizeof(szSysErrStr)));
			return false;
		}
		if (g_pOptions->GetWriteBufferSize() == -1)
		{
			setvbuf(m_pOutFile, (char *)NULL, _IOFBF, m_pArticleInfo->GetSize());
		}
		else if (g_pOptions->GetWriteBufferSize() > 0)
		{
			setvbuf(m_pOutFile, (char *)NULL, _IOFBF, g_pOptions->GetWriteBufferSize());
		}
	}

	return true;
}

/* creates output file and subdirectores */
bool ArticleDownloader::CreateOutputFile(int iSize)
{
	if (g_pOptions->GetDirectWrite() && Util::FileExists(m_szOutputFilename) &&
		Util::FileSize(m_szOutputFilename) == iSize)
	{
		// keep existing old file from previous program session
		return true;
	}
		
	// delete eventually existing old file from previous program session
	remove(m_szOutputFilename);

	// ensure the directory exist
	char szDestDir[1024];
	int iMaxlen = Util::BaseFileName(m_szOutputFilename) - m_szOutputFilename;
	if (iMaxlen > 1024-1) iMaxlen = 1024-1;
	strncpy(szDestDir, m_szOutputFilename, iMaxlen);
	szDestDir[iMaxlen] = '\0';
	char szErrBuf[1024];

	if (!Util::ForceDirectories(szDestDir, szErrBuf, sizeof(szErrBuf)))
	{
		error("Could not create directory %s: %s", szDestDir, szErrBuf);
		return false;
	}

	if (!Util::CreateSparseFile(m_szOutputFilename, iSize))
	{
		error("Could not create file %s", m_szOutputFilename);
		return false;
	}

	return true;
}

void ArticleDownloader::BuildOutputFilename()
{
	char szFilename[1024];

	snprintf(szFilename, 1024, "%s%i.%03i", g_pOptions->GetTempDir(), m_pFileInfo->GetID(), m_pArticleInfo->GetPartNumber());
	szFilename[1024-1] = '\0';
	m_pArticleInfo->SetResultFilename(szFilename);

	char tmpname[1024];
	snprintf(tmpname, 1024, "%s.tmp", szFilename);
	tmpname[1024-1] = '\0';
	SetTempFilename(tmpname);

	if (g_pOptions->GetDirectWrite())
	{
		m_pFileInfo->LockOutputFile();

		if (m_pFileInfo->GetOutputFilename())
		{
			strncpy(szFilename, m_pFileInfo->GetOutputFilename(), 1024);
			szFilename[1024-1] = '\0';
		}
		else
		{
			snprintf(szFilename, 1024, "%s%c%i.out.tmp", m_pFileInfo->GetNZBInfo()->GetDestDir(), (int)PATH_SEPARATOR, m_pFileInfo->GetID());
			szFilename[1024-1] = '\0';
			m_pFileInfo->SetOutputFilename(szFilename);
		}

		m_pFileInfo->UnlockOutputFile();

		SetOutputFilename(szFilename);
	}
}

ArticleDownloader::EStatus ArticleDownloader::DecodeCheck()
{
	bool bDirectWrite = g_pOptions->GetDirectWrite() && m_eFormat == Decoder::efYenc;

	if (g_pOptions->GetDecode())
	{
		SetStatus(adDecoding);

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
		bool bOK = eStatus == Decoder::eFinished;

		if (!bDirectWrite && bOK)
		{
			if (!Util::MoveFile(m_szTempFilename, m_szResultFilename))
			{
				char szErrBuf[256];
				error("Could not rename file %s to %s: %s", m_szTempFilename, m_szResultFilename, Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
			}
		}

		if (!m_szArticleFilename && pDecoder->GetArticleFilename())
		{
			m_szArticleFilename = strdup(pDecoder->GetArticleFilename());
		}

		remove(m_szTempFilename);

		if (bOK)
		{
			detail("Successfully downloaded %s", m_szInfoName);
			return adFinished;
		}
		else
		{
			remove(m_szResultFilename);
			if (eStatus == Decoder::eCrcError)
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
	}
	else 
	{
		// rawmode
		if (Util::MoveFile(m_szTempFilename, m_szResultFilename))
		{
			detail("Article %s successfully downloaded", m_szInfoName);
		}
		else
		{
			char szErrBuf[256];
			error("Could not move file %s to %s: %s", m_szTempFilename, m_szResultFilename, Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
		}
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

	info("      Download: status=%i, LastUpdateTime=%s, filename=%s", m_eStatus, szTime, Util::BaseFileName(GetTempFilename()));
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
		g_pStatMeter->AddServerData(m_pConnection->FetchTotalBytesRead(), m_pConnection->GetNewsServer()->GetID());
		g_pServerPool->FreeConnection(m_pConnection, true);
		m_pConnection = NULL;
		m_mutexConnection.Unlock();
	}
}

void ArticleDownloader::CompleteFileParts()
{
	debug("Completing file parts");
	debug("ArticleFilename: %s", m_pFileInfo->GetFilename());

	SetStatus(adJoining);

	bool bDirectWrite = g_pOptions->GetDirectWrite() && m_pFileInfo->GetOutputInitialized();

	char szNZBName[1024];
	char szNZBDestDir[1024];
	// the locking is needed for accessing the memebers of NZBInfo
	DownloadQueue::Lock();
	strncpy(szNZBName, m_pFileInfo->GetNZBInfo()->GetName(), 1024);
	strncpy(szNZBDestDir, m_pFileInfo->GetNZBInfo()->GetDestDir(), 1024);
	DownloadQueue::Unlock();
	szNZBName[1024-1] = '\0';
	szNZBDestDir[1024-1] = '\0';
	
	char InfoFilename[1024];
	snprintf(InfoFilename, 1024, "%s%c%s", szNZBName, (int)PATH_SEPARATOR, m_pFileInfo->GetFilename());
	InfoFilename[1024-1] = '\0';

	if (!g_pOptions->GetDecode())
	{
		detail("Moving articles for %s", InfoFilename);
	}
	else if (bDirectWrite)
	{
		detail("Checking articles for %s", InfoFilename);
	}
	else
	{
		detail("Joining articles for %s", InfoFilename);
	}

	// Ensure the DstDir is created
	char szErrBuf[1024];
	if (!Util::ForceDirectories(szNZBDestDir, szErrBuf, sizeof(szErrBuf)))
	{
		error("Could not create directory %s: %s", szNZBDestDir, szErrBuf);
		SetStatus(adJoined);
		return;
	}

	char ofn[1024];
	Util::MakeUniqueFilename(ofn, 1024, szNZBDestDir, m_pFileInfo->GetFilename());

	FILE* outfile = NULL;
	char tmpdestfile[1024];
	snprintf(tmpdestfile, 1024, "%s.tmp", ofn);
	tmpdestfile[1024-1] = '\0';

	if (g_pOptions->GetDecode() && !bDirectWrite)
	{
		remove(tmpdestfile);
		outfile = fopen(tmpdestfile, "wb+");
		if (!outfile)
		{
			error("Could not create file %s!", tmpdestfile);
			SetStatus(adJoined);
			return;
		}
		if (g_pOptions->GetWriteBufferSize() == -1 && (*m_pFileInfo->GetArticles())[0])
		{
			setvbuf(outfile, (char *)NULL, _IOFBF, (*m_pFileInfo->GetArticles())[0]->GetSize());
		}
		else if (g_pOptions->GetWriteBufferSize() > 0)
		{
			setvbuf(outfile, (char *)NULL, _IOFBF, g_pOptions->GetWriteBufferSize());
		}
	}
	else if (!g_pOptions->GetDecode())
	{
		remove(tmpdestfile);
		if (!Util::CreateDirectory(ofn))
		{
			error("Could not create directory %s! Errcode: %i", ofn, errno);
			SetStatus(adJoined);
			return;
		}
	}

	bool complete = true;
	int iBrokenCount = 0;
	static const int BUFFER_SIZE = 1024 * 50;
	char* buffer = NULL;

	if (g_pOptions->GetDecode() && !bDirectWrite)
	{
		buffer = (char*)malloc(BUFFER_SIZE);
	}

	for (FileInfo::Articles::iterator it = m_pFileInfo->GetArticles()->begin(); it != m_pFileInfo->GetArticles()->end(); it++)
	{
		ArticleInfo* pa = *it;
		if (pa->GetStatus() != ArticleInfo::aiFinished)
		{
			iBrokenCount++;
			complete = false;
		}
		else if (g_pOptions->GetDecode() && !bDirectWrite)
		{
			FILE* infile;
			const char* fn = pa->GetResultFilename();

			infile = fopen(fn, "rb");
			if (infile)
			{
				int cnt = BUFFER_SIZE;

				while (cnt == BUFFER_SIZE)
				{
					cnt = (int)fread(buffer, 1, BUFFER_SIZE, infile);
					fwrite(buffer, 1, cnt, outfile);
					SetLastUpdateTimeNow();
				}

				fclose(infile);
			}
			else
			{
				complete = false;
				iBrokenCount++;
				detail("Could not find file %s. Status is broken", fn);
			}
		}
		else if (!g_pOptions->GetDecode())
		{
			const char* fn = pa->GetResultFilename();
			char dstFileName[1024];
			snprintf(dstFileName, 1024, "%s%c%03i", ofn, (int)PATH_SEPARATOR, pa->GetPartNumber());
			dstFileName[1024-1] = '\0';
			if (!Util::MoveFile(fn, dstFileName))
			{
				char szErrBuf[256];
				error("Could not move file %s to %s: %s", fn, dstFileName, Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
			}
		}
	}

	free(buffer);

	if (outfile)
	{
		fclose(outfile);
		if (!Util::MoveFile(tmpdestfile, ofn))
		{
			char szErrBuf[256];
			error("Could not move file %s to %s: %s", tmpdestfile, ofn, Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
		}
	}

	if (bDirectWrite)
	{
		if (!Util::MoveFile(m_szOutputFilename, ofn))
		{
			char szErrBuf[256];
			error("Could not move file %s to %s: %s", m_szOutputFilename, ofn, Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
		}

		// if destination directory was changed delete the old directory (if empty)
		int iLen = strlen(szNZBDestDir);
		if (!(!strncmp(szNZBDestDir, m_szOutputFilename, iLen) && 
			(m_szOutputFilename[iLen] == PATH_SEPARATOR || m_szOutputFilename[iLen] == ALT_PATH_SEPARATOR)))
		{
			debug("Checking old dir for: %s", m_szOutputFilename);
			char szOldDestDir[1024];
			int iMaxlen = Util::BaseFileName(m_szOutputFilename) - m_szOutputFilename;
			if (iMaxlen > 1024-1) iMaxlen = 1024-1;
			strncpy(szOldDestDir, m_szOutputFilename, iMaxlen);
			szOldDestDir[iMaxlen] = '\0';
			if (Util::DirEmpty(szOldDestDir))
			{
				debug("Deleting old dir: %s", szOldDestDir);
				rmdir(szOldDestDir);
			}
		}
	}

	if (!bDirectWrite)
	{
		for (FileInfo::Articles::iterator it = m_pFileInfo->GetArticles()->begin(); it != m_pFileInfo->GetArticles()->end(); it++)
		{
			ArticleInfo* pa = *it;
			remove(pa->GetResultFilename());
		}
	}

	if (complete)
	{
		info("Successfully downloaded %s", InfoFilename);
	}
	else
	{
		warn("%i of %i article downloads failed for \"%s\"",
			iBrokenCount + m_pFileInfo->GetMissedArticles(),
			m_pFileInfo->GetTotalArticles(), InfoFilename);

		if (g_pOptions->GetCreateBrokenLog())
		{
			char szBrokenLogName[1024];
			snprintf(szBrokenLogName, 1024, "%s%c_brokenlog.txt", szNZBDestDir, (int)PATH_SEPARATOR);
			szBrokenLogName[1024-1] = '\0';
			FILE* file = fopen(szBrokenLogName, "ab");
			fprintf(file, "%s (%i/%i)%s", m_pFileInfo->GetFilename(),
				m_pFileInfo->GetTotalArticles() - iBrokenCount - m_pFileInfo->GetMissedArticles(),
				m_pFileInfo->GetTotalArticles(), LINE_ENDING);
			fclose(file);
		}
	}

	// the locking is needed for accessing the members of NZBInfo
	DownloadQueue::Lock();
	m_pFileInfo->GetNZBInfo()->GetCompletedFiles()->push_back(strdup(ofn));
	if (strcmp(m_pFileInfo->GetNZBInfo()->GetDestDir(), szNZBDestDir))
	{
		// destination directory was changed during completion, need to move the file
		MoveCompletedFiles(m_pFileInfo->GetNZBInfo(), szNZBDestDir);
	}
	DownloadQueue::Unlock();

	SetStatus(adJoined);
}

bool ArticleDownloader::MoveCompletedFiles(NZBInfo* pNZBInfo, const char* szOldDestDir)
{
	if (pNZBInfo->GetCompletedFiles()->empty())
	{
		return true;
	}

	// Ensure the DstDir is created
	char szErrBuf[1024];
	if (!Util::ForceDirectories(pNZBInfo->GetDestDir(), szErrBuf, sizeof(szErrBuf)))
	{
		error("Could not create directory %s: %s", pNZBInfo->GetDestDir(), szErrBuf);
		return false;
	}

	// move already downloaded files to new destination
	for (NZBInfo::Files::iterator it = pNZBInfo->GetCompletedFiles()->begin(); it != pNZBInfo->GetCompletedFiles()->end(); it++)
    {
		char* szFileName = *it;
		char szNewFileName[1024];
		snprintf(szNewFileName, 1024, "%s%c%s", pNZBInfo->GetDestDir(), (int)PATH_SEPARATOR, Util::BaseFileName(szFileName));
		szNewFileName[1024-1] = '\0';

		// check if file was not moved already
		if (strcmp(szFileName, szNewFileName))
		{
			// prevent overwriting of existing files
			Util::MakeUniqueFilename(szNewFileName, 1024, pNZBInfo->GetDestDir(), Util::BaseFileName(szFileName));

			detail("Moving file %s to %s", szFileName, szNewFileName);
			if (Util::MoveFile(szFileName, szNewFileName))
			{
				free(szFileName);
				*it = strdup(szNewFileName);
			}
			else
			{
				char szErrBuf[256];
				error("Could not move file %s to %s: %s", szFileName, szNewFileName, Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
			}
		}
    }

	// move brokenlog.txt
	if (g_pOptions->GetCreateBrokenLog())
	{
		char szOldBrokenLogName[1024];
		snprintf(szOldBrokenLogName, 1024, "%s%c_brokenlog.txt", szOldDestDir, (int)PATH_SEPARATOR);
		szOldBrokenLogName[1024-1] = '\0';
		if (Util::FileExists(szOldBrokenLogName))
		{
			char szBrokenLogName[1024];
			snprintf(szBrokenLogName, 1024, "%s%c_brokenlog.txt", pNZBInfo->GetDestDir(), (int)PATH_SEPARATOR);
			szBrokenLogName[1024-1] = '\0';

			detail("Moving file %s to %s", szOldBrokenLogName, szBrokenLogName);
			if (Util::FileExists(szBrokenLogName))
			{
				// copy content to existing new file, then delete old file
				FILE* outfile;
				outfile = fopen(szBrokenLogName, "ab");
				if (outfile)
				{
					FILE* infile;
					infile = fopen(szOldBrokenLogName, "rb");
					if (infile)
					{
						static const int BUFFER_SIZE = 1024 * 50;
						int cnt = BUFFER_SIZE;
						char* buffer = (char*)malloc(BUFFER_SIZE);
						while (cnt == BUFFER_SIZE)
						{
							cnt = (int)fread(buffer, 1, BUFFER_SIZE, infile);
							fwrite(buffer, 1, cnt, outfile);
						}
						fclose(infile);
						free(buffer);
						remove(szOldBrokenLogName);
					}
					else
					{
						error("Could not open file %s", szOldBrokenLogName);
					}
					fclose(outfile);
				}
				else
				{
					error("Could not open file %s", szBrokenLogName);
				}
			}
			else 
			{
				// move to new destination
				if (!Util::MoveFile(szOldBrokenLogName, szBrokenLogName))
				{
					char szErrBuf[256];
					error("Could not move file %s to %s: %s", szOldBrokenLogName, szBrokenLogName, Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
				}
			}
		}
	}

	// delete old directory (if empty)
	if (Util::DirEmpty(szOldDestDir))
	{
		rmdir(szOldDestDir);
	}

	return true;
}
