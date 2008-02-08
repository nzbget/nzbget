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
#include "Util.h"

extern DownloadSpeedMeter* g_pDownloadSpeedMeter;
extern Options* g_pOptions;
extern ServerPool* g_pServerPool;

const char* ArticleDownloader::m_szJobStatus[] = { "WAITING", "RUNNING", "FINISHED", "FAILED", "DECODING", "JOINING", "NOT_FOUND", "FATAL_ERROR" };

ArticleDownloader::ArticleDownloader()
{
	debug("Creating ArticleDownloader");

	m_szResultFilename	= NULL;
	m_szTempFilename	= NULL;
	m_szArticleFilename	= NULL;
	m_szInfoName		= NULL;
	m_szOutputFilename	= NULL;
	m_pConnection		= NULL;
	m_eStatus			= adUndefined;
	m_bDuplicate		= false;
	SetLastUpdateTimeNow();
}

ArticleDownloader::~ArticleDownloader()
{
	debug("Destroying ArticleDownloader");

	if (m_szTempFilename)
	{
		free(m_szTempFilename);
	}
	if (m_szArticleFilename)
	{
		free(m_szArticleFilename);
	}
	if (m_szInfoName)
	{
		free(m_szInfoName);
	}
	if (m_szOutputFilename)
	{
		free(m_szOutputFilename);
	}
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

void ArticleDownloader::SetStatus(EStatus eStatus)
{
	m_eStatus = eStatus;
	Notify(NULL);
}

void ArticleDownloader::Run()
{
	debug("Entering ArticleDownloader-loop");

	SetStatus(adRunning);
	m_szResultFilename = m_pArticleInfo->GetResultFilename();

	if (g_pOptions->GetContinuePartial())
	{
		if (Util::FileExists(m_szResultFilename))
		{
			// file exists from previous program's start
			detail("Article %s already downloaded, skipping", m_szInfoName);
			SetStatus(adFinished);
			FreeConnection(true);
			return;
		}
	}

	detail("Downloading %s", m_szInfoName);

	int retry = g_pOptions->GetRetries();

	EStatus Status = adFailed;
	int iMaxLevel = g_pServerPool->GetMaxLevel();
	int* LevelStatus = (int*)malloc((iMaxLevel + 1) * sizeof(int));
	for (int i = 0; i <= iMaxLevel; i++)
	{
		LevelStatus[i] = 0;
	}
	int level = 0;

	while (!IsStopped() && (retry > 0))
	{
		SetLastUpdateTimeNow();

		Status = adFailed;

		if (!m_pConnection)
		{
			m_pConnection = g_pServerPool->GetConnection(level, true);
		}

		if (IsStopped())
		{
			Status = adFailed;
			break;
		}

		if (!m_pConnection)
		{
			debug("m_pConnection is NULL");
			error("Serious error: Connection is NULL");
		}
		
		// test connection
		bool bConnected = m_pConnection && m_pConnection->Connect() >= 0;
		if (bConnected && !IsStopped())
		{
			// Okay, we got a Connection. Now start downloading.
			Status = Download();
		}

		if (bConnected)
		{
			if (Status == adConnectError)
			{
				m_pConnection->Disconnect();
			}
			else
			{
				// freeing connection allows other threads to start.
				// we doing this only if the problem was with article or group.
				// if the problem occurs by connecting or authorization we do not
				// free the connection, to prevent starting of thousands of threads 
				// (cause each of them will also free it's connection after the 
				// same connect-error).
				FreeConnection(Status == adFinished);
			}
		}

		if (((Status == adFailed) || ((Status == adCrcError) && g_pOptions->GetRetryOnCrcError())) && 
			((retry > 1) || !bConnected || (Status == adConnectError)) && !IsStopped())
		{
			detail("Waiting %i sec to retry", g_pOptions->GetRetryInterval());
			int msec = 0;
			while (!IsStopped() && (msec < g_pOptions->GetRetryInterval() * 1000))
			{
				usleep(100 * 1000);
				msec += 100;
			}
		}

		if (IsStopped())
		{
			Status = adFailed;
			break;
		}
				 
		if ((Status == adFinished) || (Status == adFatalError) ||
			(Status == adCrcError && !g_pOptions->GetRetryOnCrcError()))
		{
			break;
		}

		LevelStatus[level] = Status;

		bool bAllLevelNotFound = true;
		for (int lev = 0; lev <= iMaxLevel; lev++)
		{
			if (LevelStatus[lev] != adNotFound)
			{
				bAllLevelNotFound = false;
				break;
			}
		}
		if (bAllLevelNotFound)
		{
			if (iMaxLevel > 0)
			{
				warn("Article %s @ all servers failed: Article not found", m_szInfoName);
			}
			break;
		}

		// do not count connect-errors, only article- and group-errors
		if (bConnected && (Status != adConnectError))
		{
			level++;
			if (level > iMaxLevel)
			{
				level = 0;
			}
			retry--;
		}
	}

	FreeConnection(Status == adFinished);

	free(LevelStatus);

	if (m_bDuplicate)
	{
		Status = adFinished;
	}

	if (Status != adFinished)
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
			warn("Download %s failed", m_szInfoName);
		}
	}

	SetStatus(Status);

	debug("Exiting ArticleDownloader-loop");
}

ArticleDownloader::EStatus ArticleDownloader::Download()
{
	const char* szResponse = NULL;
	EStatus Status = adRunning;

	// at first, change group
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

	// retrieve article
	char tmp[1024];
	snprintf(tmp, 1024, "ARTICLE %s\r\n", m_pArticleInfo->GetMessageID());
	tmp[1024-1] = '\0';

	for (int retry = 3; retry > 0; retry--)
	{
		szResponse = m_pConnection->Request(tmp);
		if (szResponse && !strncmp(szResponse, "2", 1))
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

	if (g_pOptions->GetDecoder() == Options::dcYenc)
	{
		m_YDecoder.Clear();
		m_YDecoder.SetAutoSeek(g_pOptions->GetDirectWrite());
		m_YDecoder.SetCrcCheck(g_pOptions->GetCrcCheck());
	}

	m_pOutFile = NULL;
	bool bBody = false;
	const int LineBufSize = 1024*10;
	char* szLineBuf = (char*)malloc(LineBufSize);
	Status = adRunning;

	while (!IsStopped())
	{
		SetLastUpdateTimeNow();

		// Throttle the bandwidth
		while (!IsStopped() && (g_pOptions->GetDownloadRate() > 0.0f) &&
		        (g_pDownloadSpeedMeter->CalcCurrentDownloadSpeed() > g_pOptions->GetDownloadRate()))
		{
			SetLastUpdateTimeNow();
			usleep(200 * 1000);
		}

		int iLen = 0;
		char* line = m_pConnection->ReadLine(szLineBuf, LineBufSize, &iLen);
		g_pDownloadSpeedMeter->AddSpeedReading(iLen);

		// Have we encountered a timeout?
		if (!line)
		{
			if (!IsStopped())
			{
				warn("Article %s @ %s failed: Unexpected end of article", m_szInfoName, m_pConnection->GetServer()->GetHost());
			}
			Status = adFailed;
			break;
		}

		//detect end of article
		if (!strcmp(line, ".\r\n") || !strcmp(line, ".\n"))
		{
			break;
		}

		//detect lines starting with "." (marked as "..")
		if (!strncmp(line, "..", 2))
		{
			line++;
		}

		// check id of returned article
		if (!bBody)
		{
			if (!strcmp(line, "\r\n") || !strcmp(line, "\n"))
			{
				bBody = true;
			}
			else if (!strncmp(line, "Message-ID: ", 12))
			{
				char* p = line + 12;
				if (strncmp(p, m_pArticleInfo->GetMessageID(), strlen(m_pArticleInfo->GetMessageID())))
				{
					if (char* e = strrchr(p, '\r')) *e = '\0'; // remove trailing CR-character
					warn("Article %s @ %s failed: Wrong message-id, expected %s, returned %s", m_szInfoName, m_pConnection->GetServer()->GetHost(), m_pArticleInfo->GetMessageID(), p);
					Status = adFailed;
					break;
				}
			}
		}

		if (!Write(line, iLen))
		{
			Status = adFatalError;
			break;
		}
	}

	free(szLineBuf);

	if (m_pOutFile)
	{
		fclose(m_pOutFile);
	}

	if (IsStopped())
	{
		remove(m_szTempFilename);
		return adFailed;
	}

	if (Status == adFailed)
	{
		remove(m_szTempFilename);
		return adFailed;
	}

	if (Status == adRunning)
	{
		FreeConnection(true);
		return Decode();
	}
	else
	{
		return Status;
	}
}

ArticleDownloader::EStatus ArticleDownloader::CheckResponse(const char* szResponse, const char* szComment)
{
	if (!szResponse)
	{
		if (!IsStopped())
		{
			warn("Article %s @ %s failed, %s: Connection closed by remote host", m_szInfoName, m_pConnection->GetServer()->GetHost(), szComment);
		}
		return adConnectError;
	}
	else if (m_pConnection->GetAuthError() || !strncmp(szResponse, "400", 3) || !strncmp(szResponse, "499", 3))
	{
		warn("Article %s @ %s failed, %s: %s", m_szInfoName, m_pConnection->GetServer()->GetHost(), szComment, szResponse);
		return adConnectError;
	}
	else if (!strncmp(szResponse, "41", 2) || !strncmp(szResponse, "42", 2))
	{
		warn("Article %s @ %s failed, %s: %s", m_szInfoName, m_pConnection->GetServer()->GetHost(), szComment, szResponse);
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
		warn("Article %s @ %s failed, %s: %s", m_szInfoName, m_pConnection->GetServer()->GetHost(), szComment, szResponse);
		return adFailed;
	}
}

bool ArticleDownloader::Write(char* szLine, int iLen)
{
	if (!m_pOutFile && !PrepareFile(szLine))
	{
		return false;
	}

	if (g_pOptions->GetDecoder() == Options::dcYenc)
	{
		return m_YDecoder.Write(szLine, m_pOutFile);
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
	if (g_pOptions->GetDecoder() == Options::dcYenc)
	{
		if (!strncmp(szLine, "=ybegin part=", 13))
		{
			if (g_pOptions->GetDupeCheck())
			{
				m_pFileInfo->LockOutputFile();
				if (!m_pFileInfo->GetOutputInitialized())
				{
					char* pb = strstr(szLine, "name=");
					if (pb)
					{
						pb += 5; //=strlen("name=")
						char* pe;
						for (pe = pb; *pe != '\0' && *pe != '\n' && *pe != '\r'; pe++) ;
						if (!m_szArticleFilename)
						{
							m_szArticleFilename = (char*)malloc(pe - pb + 1);
							strncpy(m_szArticleFilename, pb, pe - pb);
							m_szArticleFilename[pe - pb] = '\0';
						}
						if (m_pFileInfo->IsDupe(m_szArticleFilename))
						{
							m_bDuplicate = true;
							return false;
						}
					}
				}
				if (!g_pOptions->GetDirectWrite())
				{
					m_pFileInfo->SetOutputInitialized(true);
				}
				m_pFileInfo->UnlockOutputFile();
			}

			if (g_pOptions->GetDirectWrite())
			{
				char* pb = strstr(szLine, "size=");
				if (pb) 
				{
					m_pFileInfo->LockOutputFile();
					if (!m_pFileInfo->GetOutputInitialized())
					{
						pb += 5; //=strlen("size=")
						long iArticleFilesize = atol(pb);
						if (!Util::SetFileSize(m_szOutputFilename, iArticleFilesize))
						{
							error("Could not create file %s!", m_szOutputFilename);
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
		const char* szFilename = g_pOptions->GetDirectWrite() ? m_szOutputFilename : m_szTempFilename;
		m_pOutFile = fopen(szFilename, g_pOptions->GetDirectWrite() ? "r+" : "w");
		if (!m_pOutFile)
		{
			error("Could not %s file %s", g_pOptions->GetDirectWrite() ? "open" : "create", szFilename);
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

ArticleDownloader::EStatus ArticleDownloader::Decode()
{
	if ((g_pOptions->GetDecoder() == Options::dcUulib) ||
		(g_pOptions->GetDecoder() == Options::dcYenc))
	{
		SetStatus(adDecoding);

		char tmpdestfile[1024];
		char* szDecoderTempFilename = NULL;
		Decoder* pDecoder = NULL;

		if (g_pOptions->GetDecoder() == Options::dcYenc)
		{
			pDecoder = &m_YDecoder;
			szDecoderTempFilename = m_szTempFilename;
		}
		else if (g_pOptions->GetDecoder() == Options::dcUulib)
		{
			pDecoder = new UULibDecoder();
			pDecoder->SetSrcFilename(m_szTempFilename);
			snprintf(tmpdestfile, 1024, "%s.dec", m_szResultFilename);
			tmpdestfile[1024-1] = '\0';
			szDecoderTempFilename = tmpdestfile;
			pDecoder->SetDestFilename(szDecoderTempFilename);
		}

		Decoder::EStatus eStatus = pDecoder->Execute();
		bool bOK = eStatus == Decoder::eFinished;

		if (!g_pOptions->GetDirectWrite())
		{
			if (bOK)
			{
				if (!Util::MoveFile(szDecoderTempFilename, m_szResultFilename))
				{
					error("Could not rename file %s to %s! Errcode: %i", szDecoderTempFilename, m_szResultFilename, errno);
				}
			}
			else if (g_pOptions->GetDecoder() == Options::dcUulib)
			{
				remove(szDecoderTempFilename);
			}
		}

		if (!m_szArticleFilename && pDecoder->GetArticleFilename())
		{
			m_szArticleFilename = strdup(pDecoder->GetArticleFilename());
		}

		remove(m_szTempFilename);
		if (pDecoder != &m_YDecoder)
		{
			delete pDecoder;
		}

		if (bOK)
		{
			detail("Successfully downloaded %s", m_szInfoName);

			if (g_pOptions->GetDirectWrite() && g_pOptions->GetContinuePartial())
			{
				// create empty flag-file to indicate that the artcile was downloaded
				FILE* flagfile = fopen(m_szResultFilename, "w");
				if (!flagfile)
				{
					error("Could not create file %s", m_szResultFilename);
					// this error can be ignored
				}
				fclose(flagfile);
			}

			return adFinished;
		}
		else
		{
			remove(m_szResultFilename);
			if (eStatus == Decoder::eCrcError)
			{
				warn("Decoding %s failed: CRC-Error", m_szInfoName);
				return adCrcError;
			}
			else if (eStatus == Decoder::eArticleIncomplete)
			{
				warn("Decoding %s failed: article incomplete", m_szInfoName);
				return adFailed;
			}
			else
			{
				warn("Decoding %s failed", m_szInfoName);
				return adFailed;
			}
		}
	}
	else if (g_pOptions->GetDecoder() == Options::dcNone)
	{
		// rawmode
		if (Util::MoveFile(m_szTempFilename, m_szResultFilename))
		{
			detail("Article %s successfully downloaded", m_szInfoName);
		}
		else
		{
			error("Could not move file %s to %s! Errcode: %i", m_szTempFilename, m_szResultFilename, errno);
		}
		return adFinished;
	}
	else
	{
		// should not occur
		error("Internal error: Decoding %s failed", m_szInfoName);
		return adFatalError;
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

	debug("      Download: status=%s, LastUpdateTime=%s, filename=%s", GetStatusText(), szTime, Util::BaseFileName(GetTempFilename()));
}

void ArticleDownloader::Stop()
{
	debug("Trying to stop ArticleDownloader");
	Thread::Stop();
	m_mutexConnection.Lock();
	if (m_pConnection)
	{
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
		pConnection->Cancel();
		pConnection->Disconnect();
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

	char szNZBNiceName[1024];
	m_pFileInfo->GetNZBInfo()->GetNiceNZBName(szNZBNiceName, 1024);
	
	char InfoFilename[1024];
	snprintf(InfoFilename, 1024, "%s%c%s", szNZBNiceName, (int)PATH_SEPARATOR, m_pFileInfo->GetFilename());
	InfoFilename[1024-1] = '\0';

	if (g_pOptions->GetDecoder() == Options::dcNone)
	{
		detail("Moving articles for %s", InfoFilename);
	}
	else if (g_pOptions->GetDirectWrite())
	{
		detail("Checking articles for %s", InfoFilename);
	}
	else
	{
		detail("Joining articles for %s", InfoFilename);
	}

	char ofn[1024];
	snprintf(ofn, 1024, "%s%c%s", m_pFileInfo->GetNZBInfo()->GetDestDir(), (int)PATH_SEPARATOR, m_pFileInfo->GetFilename());
	ofn[1024-1] = '\0';

	// Ensure the DstDir is created
	if (!Util::CreateDirectory(m_pFileInfo->GetNZBInfo()->GetDestDir()))
	{
		error("Could not create directory %s! Errcode: %i", m_pFileInfo->GetNZBInfo()->GetDestDir(), errno);
		SetStatus(adJoined);
		return;
	}

	// prevent overwriting existing files
	int dupcount = 0;
	while (Util::FileExists(ofn))
	{
		dupcount++;
		snprintf(ofn, 1024, "%s%c%s_duplicate%d", m_pFileInfo->GetNZBInfo()->GetDestDir(), (int)PATH_SEPARATOR, m_pFileInfo->GetFilename(), dupcount);
		ofn[1024-1] = '\0';
	}

	FILE* outfile = NULL;
	char tmpdestfile[1024];
	snprintf(tmpdestfile, 1024, "%s.tmp", ofn);
	tmpdestfile[1024-1] = '\0';

	if (((g_pOptions->GetDecoder() == Options::dcUulib) ||
		(g_pOptions->GetDecoder() == Options::dcYenc)) &&
		!g_pOptions->GetDirectWrite())
	{
		remove(tmpdestfile);
		outfile = fopen(tmpdestfile, "w+");
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
	else if (g_pOptions->GetDecoder() == Options::dcNone)
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

	if (((g_pOptions->GetDecoder() == Options::dcUulib) ||
		(g_pOptions->GetDecoder() == Options::dcYenc)) &&
		!g_pOptions->GetDirectWrite())
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
		else if (((g_pOptions->GetDecoder() == Options::dcUulib) ||
		         (g_pOptions->GetDecoder() == Options::dcYenc)) && 
				 !g_pOptions->GetDirectWrite())
		{
			FILE* infile;
			const char* fn = pa->GetResultFilename();

			infile = fopen(fn, "r");
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
		else if (g_pOptions->GetDecoder() == Options::dcNone)
		{
			const char* fn = pa->GetResultFilename();
			char dstFileName[1024];
			snprintf(dstFileName, 1024, "%s%c%03i", ofn, (int)PATH_SEPARATOR, pa->GetPartNumber());
			dstFileName[1024-1] = '\0';
			if (!Util::MoveFile(fn, dstFileName))
			{
				error("Could not move file %s to %s! Errcode: %i", fn, dstFileName, errno);
			}
		}
	}

	if (buffer)
	{
		free(buffer);
	}

	if (outfile)
	{
		fclose(outfile);
		if (!Util::MoveFile(tmpdestfile, ofn))
		{
			error("Could not move file %s to %s! Errcode: %i", tmpdestfile, ofn, errno);
		}
	}

	if (g_pOptions->GetDirectWrite())
	{
		if (!Util::MoveFile(m_szOutputFilename, ofn))
		{
			error("Could not move file %s to %s! Errcode: %i", m_szOutputFilename, ofn, errno);
		}
	}

	if (!g_pOptions->GetDirectWrite() || g_pOptions->GetContinuePartial())
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
		warn("%i of %i article downloads failed for \"%s\"", iBrokenCount, m_pFileInfo->GetArticles()->size(), InfoFilename);

		if (g_pOptions->GetRenameBroken())
		{
			char brokenfn[1024];
			snprintf(brokenfn, 1024, "%s_broken", ofn);
			brokenfn[1024-1] = '\0';
			if (Util::MoveFile(ofn, brokenfn))
			{
				detail("Renaming broken file from %s to %s", ofn, brokenfn);
			}
			else
			{
				warn("Renaming broken file from %s to %s failed", ofn, brokenfn);
			}
		}
		else
		{
			detail("Not renaming broken file %s", ofn);
		}

		if (g_pOptions->GetCreateBrokenLog())
		{
			char szBrokenLogName[1024];
			snprintf(szBrokenLogName, 1024, "%s%c_brokenlog.txt", m_pFileInfo->GetNZBInfo()->GetDestDir(), (int)PATH_SEPARATOR);
			szBrokenLogName[1024-1] = '\0';
			FILE* file = fopen(szBrokenLogName, "a");
			fprintf(file, "%s (%i/%i)\n", m_pFileInfo->GetFilename(), m_pFileInfo->GetArticles()->size() - iBrokenCount, m_pFileInfo->GetArticles()->size());
			fclose(file);
		}
	}

	SetStatus(adJoined);
}
