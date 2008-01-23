/*
 *  This file if part of nzbget
 *
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
#include <ctype.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

#include "nzbget.h"
#include "PrePostProcessor.h"
#include "Options.h"
#include "Log.h"
#include "QueueCoordinator.h"
#include "Util.h"

extern QueueCoordinator* g_pQueueCoordinator;
extern Options* g_pOptions;

static const int PARSTATUS_NOT_CHECKED = 0;
static const int PARSTATUS_FAILED = 1;
static const int PARSTATUS_REPAIRED = 2;
static const int PARSTATUS_REPAIR_POSSIBLE = 3;

PrePostProcessor::ParJob::ParJob(const char * szNZBFilename, const char * szParFilename, const char * szInfoName)
{
	m_szNZBFilename = strdup(szNZBFilename);
	m_szParFilename = strdup(szParFilename);
	m_szInfoName = strdup(szInfoName);
}

PrePostProcessor::ParJob::~ ParJob()
{
	if (m_szNZBFilename)
	{
		free(m_szNZBFilename);
	}
	if (m_szParFilename)
	{
		free(m_szParFilename);
	}
	if (m_szInfoName)
	{
		free(m_szInfoName);
	}
}

PrePostProcessor::PrePostProcessor()
{
	debug("Creating PrePostProcessor");

	m_bHasMoreJobs = false;

	m_QueueCoordinatorObserver.owner = this;
	g_pQueueCoordinator->Attach(&m_QueueCoordinatorObserver);

	m_ParQueue.clear();

#ifndef DISABLE_PARCHECK
	m_ParCheckerObserver.owner = this;
	m_ParChecker.Attach(&m_ParCheckerObserver);
#endif
}

PrePostProcessor::~PrePostProcessor()
{
	debug("Destroying PrePostProcessor");
	
	for (ParQueue::iterator it = m_ParQueue.begin(); it != m_ParQueue.end(); it++)
	{
		delete *it;
	}
}

void PrePostProcessor::Run()
{
	debug("Entering PrePostProcessor-loop");

	int iNZBDirInterval = 0;
#ifndef DISABLE_PARCHECK
	int iParQueueInterval = 0;
#endif
	while (!IsStopped())
	{
		if (g_pOptions->GetNzbDir() && g_pOptions->GetNzbDirInterval() > 0 && 
			iNZBDirInterval == g_pOptions->GetNzbDirInterval() * 1000)
		{
			// check nzbdir every g_pOptions->GetNzbDirInterval() seconds
			CheckIncomingNZBs();
			iNZBDirInterval = 0;
		}
		iNZBDirInterval += 200;
#ifndef DISABLE_PARCHECK
		if (iParQueueInterval == 1000 && g_pOptions->GetParCheck())
		{
			// check par-queue every 1 second
			CheckParQueue();
			iParQueueInterval = 0;
		}
		iParQueueInterval += 200;
#endif
		usleep(200 * 1000);
	}

	debug("Exiting PrePostProcessor-loop");
}

void PrePostProcessor::Stop()
{
	Thread::Stop();
#ifndef DISABLE_PARCHECK
	m_mutexParChecker.Lock();
	if (m_ParChecker.IsRunning())
	{
		m_ParChecker.Stop();
		int iMSecWait = 5000;
		while (m_ParChecker.IsRunning() && iMSecWait > 0)
		{
			usleep(50 * 1000);
			iMSecWait -= 50;
		}
		if (m_ParChecker.IsRunning())
		{
			warn("Terminating par-check for %s", m_ParChecker.GetInfoName());
			m_ParChecker.Kill();
		}
	}
	
	m_mutexParChecker.Unlock();
#endif
}

void PrePostProcessor::QueueCoordinatorUpdate(Subject * Caller, void * Aspect)
{
	if (IsStopped())
	{
		return;
	}

	QueueCoordinator::Aspect* pAspect = (QueueCoordinator::Aspect*)Aspect;
	if (pAspect->eAction == QueueCoordinator::eaNZBFileAdded &&
		g_pOptions->GetLoadPars() != Options::plAll)
	{
		PausePars(pAspect->pDownloadQueue, pAspect->szNZBFilename);
	}
	else if ((pAspect->eAction == QueueCoordinator::eaFileCompleted ||
		pAspect->eAction == QueueCoordinator::eaFileDeleted))
	{
		if (
#ifndef DISABLE_PARCHECK
			!AddPar(pAspect->pFileInfo, pAspect->eAction == QueueCoordinator::eaFileDeleted) &&
#endif
			WasLastInCollection(pAspect->pDownloadQueue, pAspect->pFileInfo, true))
		{
			char szNZBNiceName[1024];
			pAspect->pFileInfo->GetNiceNZBName(szNZBNiceName, 1024);
			if (pAspect->eAction == QueueCoordinator::eaFileCompleted)
			{
				info("Collection %s completely downloaded", szNZBNiceName);
			}
			else if (WasLastInCollection(pAspect->pDownloadQueue, pAspect->pFileInfo, false))
			{
				info("Collection %s deleted from queue", szNZBNiceName);
			}
#ifndef DISABLE_PARCHECK
			if (g_pOptions->GetParCheck() &&
				pAspect->eAction == QueueCoordinator::eaFileCompleted)
			{
				CheckPars(pAspect->pDownloadQueue, pAspect->pFileInfo);
			}
			else
#endif
			{
				ExecPostScript(pAspect->pFileInfo->GetDestDir(), pAspect->pFileInfo->GetNZBFilename(), "", PARSTATUS_NOT_CHECKED);
			}
		}
	}
}

void PrePostProcessor::PausePars(DownloadQueue* pDownloadQueue, const char* szNZBFilename)
{
	debug("PrePostProcessor: Pausing pars");
	
	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (!strcmp(pFileInfo->GetNZBFilename(), szNZBFilename))
		{
			g_pQueueCoordinator->GetQueueEditor()->LockedEditEntry(pDownloadQueue, pFileInfo->GetID(), false, 
				(g_pOptions->GetLoadPars() == Options::plOne ||
					(g_pOptions->GetLoadPars() == Options::plNone && g_pOptions->GetParCheck()))
				? QueueEditor::eaGroupPauseExtraPars : QueueEditor::eaGroupPauseAllPars,
				0);
			break;
		}
	}
}

/**
* Check if there are files in directory for incoming nzb-files
* and add them to download queue
*/
void PrePostProcessor::CheckIncomingNZBs()
{
	DirBrowser dir(g_pOptions->GetNzbDir());
	while (const char* filename = dir.Next())
	{
		int len = strlen(filename);
		if (len > 4 && !strcasecmp(filename + len - 4, ".nzb"))
		{
			// file found, checking modification-time
			struct stat buffer;
			char fullfilename[1024];
			snprintf(fullfilename, 1024, "%s%c%s", g_pOptions->GetNzbDir(), (int)PATH_SEPARATOR, filename);
			fullfilename[1024-1] = '\0';
			if (!stat(fullfilename, &buffer) &&
				time(NULL) - buffer.st_mtime > g_pOptions->GetNzbDirFileAge() &&
				time(NULL) - buffer.st_ctime > g_pOptions->GetNzbDirFileAge())
			{
				// the file is at least g_pOptions->GetNzbDirFileAge() seconds old, we can process it
				info("Collection %s found", filename);
				char bakname[1024];
				if (g_pQueueCoordinator->AddFileToQueue(fullfilename))
				{
					info("Collection %s added to queue", filename);
					snprintf(bakname, 1024, "%s.queued", fullfilename);
					bakname[1024-1] = '\0';
				}
				else
				{
					error("Could not add collection %s to queue", filename);
					snprintf(bakname, 1024, "%s.error", fullfilename);
					bakname[1024-1] = '\0';
				}

				char bakname2[1024];
				strcpy(bakname2, bakname);
				int i = 2;
				while (!stat(bakname2, &buffer))
				{
					snprintf(bakname2, 1024, "%s%i", bakname, i++);
					bakname2[1024-1] = '\0';
				}
				
				rename(fullfilename, bakname2);
			}
		}
	}
}

/**
* Check if the completed file was last (unpaused, if bIgnorePaused is "true") file in nzb-collection
*/
bool PrePostProcessor::WasLastInCollection(DownloadQueue* pDownloadQueue, FileInfo * pFileInfo, bool bIgnorePaused)
{
	debug("File %s completed or deleted", pFileInfo->GetFilename());

	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
	{
		FileInfo* pFileInfo2 = *it;
		if (pFileInfo2 != pFileInfo && (!bIgnorePaused || !pFileInfo2->GetPaused()) &&
			!strcmp(pFileInfo2->GetNZBFilename(), pFileInfo->GetNZBFilename()))
		{
			return false;
		}
	}

	return true;
}

#ifndef DISABLE_PARCHECK

void PrePostProcessor::CheckPars(DownloadQueue * pDownloadQueue, FileInfo * pFileInfo)
{
	char szNZBNiceName[1024];
	pFileInfo->GetNiceNZBName(szNZBNiceName, 1024);

	m_mutexParChecker.Lock();

	FileList fileList;
	if (FindMainPars(pFileInfo->GetDestDir(), &fileList))
	{
		for (FileList::iterator it = fileList.begin(); it != fileList.end(); it++)
		{
			char* szParFilename = *it;
			debug("Found par: %s", szParFilename);
			
			char szFullFilename[1024];
			snprintf(szFullFilename, 1024, "%s%c%s", pFileInfo->GetDestDir(), (int)PATH_SEPARATOR, szParFilename);
			szFullFilename[1024-1] = '\0';

			char szInfoName[1024];
			int iBaseLen = 0;
			ParChecker::ParseParFilename(szParFilename, &iBaseLen, NULL);
			int maxlen = iBaseLen < 1024 ? iBaseLen : 1024 - 1;
			strncpy(szInfoName, szParFilename, maxlen);
			szInfoName[maxlen] = '\0';
			
			char szParInfoName[1024];
			snprintf(szParInfoName, 1024, "%s%c%s", szNZBNiceName, (int)PATH_SEPARATOR, szInfoName);
			szParInfoName[1024-1] = '\0';
			
			info("Queueing %s%c%s for par-check", szNZBNiceName, (int)PATH_SEPARATOR, szInfoName);
			ParJob* pParJob = new ParJob(pFileInfo->GetNZBFilename(), szFullFilename, szParInfoName);
			m_ParQueue.push_back(pParJob);
			m_bHasMoreJobs = true;

			free(szParFilename);
		}
	}

	m_mutexParChecker.Unlock();
}

bool PrePostProcessor::FindMainPars(const char * szPath, FileList * pFileList)
{
	pFileList->clear();
	DirBrowser dir(szPath);
	while (const char* filename = dir.Next())
	{
		int iBaseLen = 0;
		if (ParChecker::ParseParFilename(filename, &iBaseLen, NULL))
		{
			// check if the base file already added to list
			bool exists = false;
			for (FileList::iterator it = pFileList->begin(); it != pFileList->end(); it++)
			{
				const char* filename2 = *it;
				exists = SameParCollection(filename, filename2);
				if (exists)
				{
					break;
				}
			}
			if (!exists)
			{
				pFileList->push_back(strdup(filename));
			}
		}
	}
	return !pFileList->empty();
}

bool PrePostProcessor::AddPar(FileInfo * pFileInfo, bool bDeleted)
{
	m_mutexParChecker.Lock();
	bool bSameCollection = m_ParChecker.IsRunning() &&
		!strcmp(pFileInfo->GetNZBFilename(), m_ParChecker.GetNZBFilename()) &&
		SameParCollection(pFileInfo->GetFilename(), BaseFileName(m_ParChecker.GetParFilename()));
	if (bSameCollection)
	{
		if (!bDeleted)
		{
			char szFullFilename[1024];
			snprintf(szFullFilename, 1024, "%s%c%s", pFileInfo->GetDestDir(), (int)PATH_SEPARATOR, pFileInfo->GetFilename());
			szFullFilename[1024-1] = '\0';
			m_ParChecker.AddParFile(szFullFilename);
		}
		else
		{
			m_ParChecker.QueueChanged();
		}
	}
	m_mutexParChecker.Unlock();
	return bSameCollection;
}

bool PrePostProcessor::SameParCollection(const char* szFilename1, const char* szFilename2)
{
	int iBaseLen1 = 0, iBaseLen2 = 0;
	return ParChecker::ParseParFilename(szFilename1, &iBaseLen1, NULL) &&
		ParChecker::ParseParFilename(szFilename2, &iBaseLen2, NULL) &&
		iBaseLen1 == iBaseLen2 &&
		!strncasecmp(szFilename1, szFilename2, iBaseLen1);
}

PrePostProcessor::ParQueue* PrePostProcessor::LockParQueue()
{
	m_mutexParChecker.Lock();
	return &m_ParQueue;
}

void PrePostProcessor::UnlockParQueue()
{
	m_mutexParChecker.Unlock();
}

void PrePostProcessor::CheckParQueue()
{
	m_mutexParChecker.Lock();

	if (!m_ParChecker.IsRunning() && !m_ParQueue.empty())
	{
		ParJob* pParJob = m_ParQueue.front();

		info("Checking pars for %s", pParJob->GetInfoName());
		m_ParChecker.SetNZBFilename(pParJob->GetNZBFilename());
		m_ParChecker.SetParFilename(pParJob->GetParFilename());
		m_ParChecker.SetInfoName(pParJob->GetInfoName());
		m_ParChecker.Start();
	}
	
	m_mutexParChecker.Unlock();
}

void PrePostProcessor::ParCheckerUpdate(Subject * Caller, void * Aspect)
{
	if (m_ParChecker.GetStatus() == ParChecker::psFinished ||
		m_ParChecker.GetStatus() == ParChecker::psFailed)
	{
		char szPath[1024];
		strncpy(szPath, m_ParChecker.GetParFilename(), 1024);
		szPath[1024-1] = '\0';
		if (char* p = strrchr(szPath, PATH_SEPARATOR)) *p = '\0';

		if (g_pOptions->GetCreateBrokenLog())
		{
			char szBrokenLogName[1024];
			snprintf(szBrokenLogName, 1024, "%s%c_brokenlog.txt", szPath, (int)PATH_SEPARATOR);
			szBrokenLogName[1024-1] = '\0';
			
			bool bExists = false;
			if (m_ParChecker.GetRepairNotNeeded())
			{
				struct stat buffer;
				bExists = !stat(szBrokenLogName, &buffer);
			}
			if (!m_ParChecker.GetRepairNotNeeded() || bExists)
			{
				FILE* file = fopen(szBrokenLogName, "a");
				if (m_ParChecker.GetStatus() == ParChecker::psFailed)
				{
					fprintf(file, "Repair failed for %s: %s\n", m_ParChecker.GetInfoName(), m_ParChecker.GetErrMsg() ? m_ParChecker.GetErrMsg() : "");
				}
				else if (m_ParChecker.GetRepairNotNeeded())
				{
					fprintf(file, "Repair not needed for %s\n", m_ParChecker.GetInfoName());
				}
				else
				{
					if (g_pOptions->GetParRepair())
					{
						fprintf(file, "Successfully repaired %s\n", m_ParChecker.GetInfoName());
					}
					else
					{
						fprintf(file, "Repair possible for %s\n", m_ParChecker.GetInfoName());
					}
				}
				fclose(file);
			}
		}
		
		int iParStatus = 0;
		if (m_ParChecker.GetStatus() == ParChecker::psFailed)
		{
			iParStatus = PARSTATUS_FAILED;
		}
		else if (g_pOptions->GetParRepair() || m_ParChecker.GetRepairNotNeeded())
		{
			iParStatus = PARSTATUS_REPAIRED;
		}
		else
		{
			iParStatus = PARSTATUS_REPAIR_POSSIBLE;
		}
		
		ExecPostScript(szPath, m_ParChecker.GetNZBFilename(), m_ParChecker.GetParFilename(), iParStatus);

		m_mutexParChecker.Lock();
		ParJob* pParJob = m_ParQueue.front();
		m_ParQueue.pop_front();
		delete pParJob;
		m_bHasMoreJobs = !m_ParQueue.empty();
		m_mutexParChecker.Unlock();
	}
}

#endif

void PrePostProcessor::ExecPostScript(const char * szPath, const char * szNZBFilename, const char * szParFilename, int iParStatus)
{
	const char* szScript = g_pOptions->GetPostProcess();
	if (!szScript || strlen(szScript) == 0)
	{
		return;
	}
		
	info("Executing post-process for %s (%s)", szPath, BaseFileName(szNZBFilename));
	struct stat buffer;
	bool bExists = !stat(szScript, &buffer);
	if (!bExists)
	{
		error("Could not start post-process: could not find file %s", szScript);
		return;
	}

	bool bCollectionCompleted = true;
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();
	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
	{
		FileInfo* pFileInfo2 = *it;
		if (!pFileInfo2->GetPaused() &&
			!strcmp(pFileInfo2->GetNZBFilename(), szNZBFilename))
		{
			bCollectionCompleted = false;
			break;
		}
	}
	g_pQueueCoordinator->UnlockQueue();
		
#ifndef DISABLE_PARCHECK
	if (bCollectionCompleted)
	{
		m_mutexParChecker.Lock();
		for (ParQueue::iterator it = m_ParQueue.begin(); it != m_ParQueue.end(); it++)
		{
			ParJob* pParJob = *it;
			if (!strcmp(pParJob->GetNZBFilename(), szNZBFilename))
			{
				bCollectionCompleted = false;
				break;
			}
		}
		m_mutexParChecker.Unlock();
	}
#endif

	char szParStatus[10];
	snprintf(szParStatus, 10, "%i", iParStatus);
	szParStatus[10-1] = '\0';

	char szCollectionCompleted[10];
	snprintf(szCollectionCompleted, 10, "%i", (int)bCollectionCompleted);
	szCollectionCompleted[10-1] = '\0';

#ifdef WIN32
	char szCmdLine[2048];
	snprintf(szCmdLine, 2048, "%s \"%s\" \"%s\" \"%s\" %s %s", szScript, szPath, szNZBFilename, szParFilename, szParStatus, szCollectionCompleted);
	szCmdLine[2048-1] = '\0';
	UINT ErrCode = WinExec(szCmdLine, SW_HIDE);
	if (ErrCode < 32)
	{
		char szErrMsg[255];
		szErrMsg[255-1] = '\0';
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM || FORMAT_MESSAGE_IGNORE_INSERTS || FORMAT_MESSAGE_ARGUMENT_ARRAY, 
			NULL, ErrCode, 0, szErrMsg, 255, NULL);
		error("Could not start post-process: %s", szErrMsg);
	}
#else
	if (fork())
	{
		// continue the first instance
		return;
	}

	// here goes the second instance
		
	int h;
	for (h = getdtablesize(); h >= 0;--h) close(h); /* close all descriptors */
	h = open("/dev/null", O_RDWR); dup(h); dup(h); /* handle standart I/O */
	
	execlp(szScript, szScript, szPath, szNZBFilename, szParFilename, szParStatus, szCollectionCompleted, NULL);
	error("Could not start post-process: %s", strerror(errno));
	exit(-1);
#endif
}
