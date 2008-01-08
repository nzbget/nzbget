/*
 *  This file if part of nzbget
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
	m_iBytes			= 0;
	memset(&m_tStartTime, 0, sizeof(m_tStartTime));
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
		struct stat buffer;
		bool fileExists = !stat(m_szResultFilename, &buffer);
		if (fileExists)
		{
			// file exists from previous program's start
			info("Article %s already downloaded, skipping", m_szInfoName);
			SetStatus(adFinished);
			FreeConnection(true);
			return;
		}
	}

	info("Downloading %s", m_szInfoName);

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
		bool connected = m_pConnection && m_pConnection->Connect() >= 0;
		if (connected && !IsStopped())
		{
			// Okay, we got a Connection. Now start downloading!!
			Status = Download();
		}

		if (connected)
		{
			// freeing connection allows other threads to start.
			// we doing this only if the problem was with article or group.
			// if the problem occurs by Connect() we do not free the connection,
			// to prevent starting of thousands of threads (cause each of them
			// will also free it's connection after the same connect-error).
			FreeConnection(Status == adFinished);
		}

		if ((Status == adFailed || (Status == adCrcError && g_pOptions->GetRetryOnCrcError())) && 
			((retry > 1) || !connected) && !IsStopped())
		{
			info("Waiting %i sec to retry", g_pOptions->GetRetryInterval());
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
		if (connected)
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

	if (Status != adFinished)
	{
		Status = adFailed;
	}

	if (Status == adFailed)
	{
		if (IsStopped())
		{
			info("Download %s cancelled", m_szInfoName);
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
	// at first, change group
	bool grpchanged = false;
	for (FileInfo::Groups::iterator it = m_pFileInfo->GetGroups()->begin(); it != m_pFileInfo->GetGroups()->end(); it++)
	{
		grpchanged = m_pConnection->JoinGroup(*it);
		if (grpchanged)
		{
			break;
		}
	}

	if (!grpchanged)
	{
		warn("Article %s @ %s failed: Could not join group", m_szInfoName, m_pConnection->GetServer()->GetHost());
		return adFailed;
	}

	// now, let's begin!
	char tmp[1024];
	snprintf(tmp, 1024, "%s %s\r\n", 
		g_pOptions->GetDecoder() == Options::dcYenc ? "BODY" : "ARTICLE", 
		m_pArticleInfo->GetMessageID());
	tmp[1024-1] = '\0';

	char* answer = NULL;

	for (int retry = 3; retry > 0; retry--)
	{
		answer = m_pConnection->Request(tmp);
		if (answer && !strncmp(answer, "2", 1))
		{
			break;
		}
	}

	if (!answer)
	{
		warn("Article %s @ %s failed: Connection closed by remote host", m_szInfoName, m_pConnection->GetServer()->GetHost());
		return adFailed;
	}

	if (strncmp(answer, "2", 1))
	{
		warn("Article %s @ %s failed: %s", m_szInfoName, m_pConnection->GetServer()->GetHost(), answer);
		return (!strncmp(answer, "41", 2) || !strncmp(answer, "42", 2)) ? adNotFound : adFailed;
	}

	// positive answer!

	if (g_pOptions->GetDecoder() == Options::dcYenc)
	{
		m_YDecoder.Clear();
		m_YDecoder.SetAutoSeek(g_pOptions->GetDirectWrite());
		m_YDecoder.SetCrcCheck(g_pOptions->GetCrcCheck());
	}

	gettimeofday(&m_tStartTime, 0);
	m_iBytes = 0;
	m_pOutFile = NULL;
	EStatus Status = adRunning;
	const int LineBufSize = 1024*10;
	char* szLineBuf = (char*)malloc(LineBufSize);

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

		// Have we encountered a timeout?
		if (!line)
		{
			Status = adFailed;
			break;
		}

		//detect end of article
		if ((!strcmp(line, ".\r\n")) || (!strcmp(line, ".\n")))
		{
			break;
		}

		//detect lines starting with "." (marked as "..")
		if (!strncmp(line, "..", 2))
		{
			line++;
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
		fflush(m_pOutFile);
		fclose(m_pOutFile);
	}

	if (IsStopped())
	{
		remove(m_szTempFilename);
		return adFailed;
	}

	if (Status == adFailed)
	{
		warn("Unexpected end of %s", m_szInfoName);
		remove(m_szTempFilename);
		return adFailed;
	}

	if (Status == adDecodeError)
	{
		warn("Decoding failed for %s", m_szInfoName);
		return adFailed;
	}

	FreeConnection(true);

	return Decode();
}

bool ArticleDownloader::Write(char* szLine, int iLen)
{
	if (!m_pOutFile && !g_pOptions->GetDirectWrite())
	{
		m_pOutFile = fopen(m_szTempFilename, "w");
		if (!m_pOutFile)
		{
			error("Could not create file %s", m_szTempFilename);
			return false;
		}
	}

	if (!m_pOutFile && g_pOptions->GetDirectWrite())
	{
		if (!strncmp(szLine, "=ybegin part=", 13))
		{
			char* pb = strstr(szLine, "size=");
			if (pb) 
			{
				m_pFileInfo->LockOutputFile();
				if (!m_pFileInfo->GetOutputInitialized())
				{
					pb += 5; //=strlen("size=")
					long iArticleFilesize = atol(pb);
					if (!SetFileSize(m_szOutputFilename, iArticleFilesize))
					{
						error("Could not create file %s!", m_szOutputFilename);
						return false;
					}
					m_pFileInfo->SetOutputInitialized(true);
				}
				m_pFileInfo->UnlockOutputFile();

				m_pOutFile = fopen(m_szOutputFilename, "r+");
				if (!m_pOutFile)
				{
					error("Could not open file %s", m_szOutputFilename);
					return false;
				}
			}
		}
	}

	bool bOK = false;

	if (g_pOptions->GetDecoder() == Options::dcYenc)
	{
		bOK = m_YDecoder.Write(szLine, m_pOutFile);
	}
	else
	{
		bOK = fwrite(szLine, 1, iLen, m_pOutFile) > 0;
	}

	m_iBytes += iLen;
	return bOK;
}

ArticleDownloader::EStatus ArticleDownloader::Decode()
{
	if ((g_pOptions->GetDecoder() == Options::dcUulib) ||
		(g_pOptions->GetDecoder() == Options::dcYenc))
	{
		SetStatus(adDecoding);
		struct _timeval StartTime, EndTime;
		gettimeofday(&StartTime, 0);

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

		bool bOK = pDecoder->Execute();

		if (!g_pOptions->GetDirectWrite())
		{
			if (bOK)
			{
				rename(szDecoderTempFilename, m_szResultFilename);
			}
			else if (g_pOptions->GetDecoder() == Options::dcUulib)
			{
				remove(szDecoderTempFilename);
			}
		}

		if (pDecoder->GetArticleFilename())
		{
			m_szArticleFilename = strdup(pDecoder->GetArticleFilename());
		}

		gettimeofday(&EndTime, 0);
		remove(m_szTempFilename);
#ifdef WIN32
		float fDeltaTime = (float)((EndTime.time - StartTime.time) * 1000 + (EndTime.millitm - StartTime.millitm));
#else
		float fDeltaTime = ((EndTime.tv_sec - StartTime.tv_sec) * 1000000 + (EndTime.tv_usec - StartTime.tv_usec)) / 1000.0;
#endif
		bool bCrcError = pDecoder->GetCrcError();
		if (pDecoder != &m_YDecoder)
		{
			delete pDecoder;
		}

		if (bOK)
		{
			info("Successfully downloaded %s", m_szInfoName);
			debug("Decode time %.1f ms", fDeltaTime);

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
			if (bCrcError)
			{
				warn("Decoding %s failed: CRC-Error", m_szInfoName);
				return adCrcError;
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
		rename(m_szTempFilename, m_szResultFilename);
		info("Article %s successfully downloaded", m_szInfoName);
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

	debug("      Download: status=%s, LastUpdateTime=%s, filename=%s", GetStatusText(), szTime, BaseFileName(GetTempFilename()));
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

void ArticleDownloader::FreeConnection(bool bKeepConnected)
{
	if (m_pConnection)
	{
		debug("Releasing connection");
		m_mutexConnection.Lock();
		if (!bKeepConnected)
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
	m_pFileInfo->GetNiceNZBName(szNZBNiceName, 1024);
	
	char InfoFilename[1024];
	snprintf(InfoFilename, 1024, "%s%c%s", szNZBNiceName, (int)PATH_SEPARATOR, m_pFileInfo->GetFilename());
	InfoFilename[1024-1] = '\0';

	if (g_pOptions->GetDecoder() == Options::dcNone)
	{
		info("Moving articles for %s", InfoFilename);
	}
	else if (g_pOptions->GetDirectWrite())
	{
		info("Checking articles for %s", InfoFilename);
	}
	else
	{
		info("Joining articles for %s", InfoFilename);
	}

	char ofn[1024];
	snprintf(ofn, 1024, "%s%c%s", m_pFileInfo->GetDestDir(), (int)PATH_SEPARATOR, m_pFileInfo->GetFilename());
	ofn[1024-1] = '\0';

	// Ensure the DstDir is created
	mkdir(m_pFileInfo->GetDestDir(), S_DIRMODE);

	// prevent overwriting existing files
	struct stat statbuf;
	int dupcount = 0;
	while (!stat(ofn, &statbuf))
	{
		dupcount++;
		snprintf(ofn, 1024, "%s%c%s_duplicate%d", m_pFileInfo->GetDestDir(), (int)PATH_SEPARATOR, m_pFileInfo->GetFilename(), dupcount);
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
			SetStatus(adFinished);
			return;
		}
	}
	else if (g_pOptions->GetDecoder() == Options::dcNone)
	{
		remove(tmpdestfile);
		mkdir(ofn, S_DIRMODE);
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
				info("Could not find file %s. Status is broken", fn);
			}
		}
		else if (g_pOptions->GetDecoder() == Options::dcNone)
		{
			const char* fn = pa->GetResultFilename();
			char dstFileName[1024];
			snprintf(dstFileName, 1024, "%s%c%03i", ofn, (int)PATH_SEPARATOR, pa->GetPartNumber());
			dstFileName[1024-1] = '\0';
			rename(fn, dstFileName);
		}
	}

	if (buffer)
	{
		free(buffer);
	}

	if (outfile)
	{
		fclose(outfile);
		rename(tmpdestfile, ofn);
	}

	if (g_pOptions->GetDirectWrite())
	{
		rename(m_szOutputFilename, ofn);
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
			bool OK = rename(ofn, brokenfn) == 0;
			if (OK)
			{
				info("Renaming broken file from %s to %s", ofn, brokenfn);
			}
			else
			{
				warn("Renaming broken file from %s to %s failed", ofn, brokenfn);
			}
		}
		else
		{
			info("Not renaming broken file %s", ofn);
		}

		if (g_pOptions->GetCreateBrokenLog())
		{
			char szBrokenLogName[1024];
			snprintf(szBrokenLogName, 1024, "%s%c_brokenlog.txt", m_pFileInfo->GetDestDir(), (int)PATH_SEPARATOR);
			szBrokenLogName[1024-1] = '\0';
			FILE* file = fopen(szBrokenLogName, "a");
			fprintf(file, "%s (%i/%i)\n", m_pFileInfo->GetFilename(), m_pFileInfo->GetArticles()->size() - iBrokenCount, m_pFileInfo->GetArticles()->size());
			fclose(file);
		}

		warn("%s is incomplete!", InfoFilename);
	}

	SetStatus(adFinished);
}

bool ArticleDownloader::Terminate()
{
	NNTPConnection* pConnection = m_pConnection;
	bool terminated = Kill();
	if (terminated && pConnection)
	{
		debug("Terminating connection");
		pConnection->Cancel();
		g_pServerPool->FreeConnection(pConnection, true);
	}
	return terminated;
}
