/*
 *  This file is part of nzbget
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
	m_bFailed = false;
	m_szProgressLabel = NULL;
	m_iFileProgress = 0;
	m_iStageProgress = 0;
	m_tStartTime = 0;
	m_tStageTime = 0;
	m_eStage = PrePostProcessor::ptQueued;
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
	if (m_szProgressLabel)
	{
		free(m_szProgressLabel);
	}
}

void PrePostProcessor::ParJob::SetProgressLabel(const char* szProgressLabel)
{
	if (m_szProgressLabel)
	{
		free(m_szProgressLabel);
	}
	m_szProgressLabel = strdup(szProgressLabel);
}

#ifndef DISABLE_PARCHECK
bool PrePostProcessor::PostParChecker::RequestMorePars(int iBlockNeeded, int* pBlockFound)
{
	return m_Owner->RequestMorePars(GetNZBFilename(), GetParFilename(), iBlockNeeded, pBlockFound);
}

void PrePostProcessor::PostParChecker::UpdateProgress()
{
	m_Owner->UpdateParProgress();
}
#endif

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
	m_ParChecker.m_Owner = this;
	m_CompletedParJobs.clear();
#endif
}

PrePostProcessor::~PrePostProcessor()
{
	debug("Destroying PrePostProcessor");
	
	for (ParQueue::iterator it = m_ParQueue.begin(); it != m_ParQueue.end(); it++)
	{
		delete *it;
	}

#ifndef DISABLE_PARCHECK
	for (ParQueue::iterator it = m_CompletedParJobs.begin(); it != m_CompletedParJobs.end(); it++)
	{
		delete *it;
	}
#endif
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
				
				if (rename(fullfilename, bakname2))
				{
					error("Could not rename file %s to %s! Errcode: %i", fullfilename, bakname2, errno);
				}
			}
		}
	}
}

void PrePostProcessor::ExecPostScript(const char * szPath, const char * szNZBFilename, const char * szParFilename, int iParStatus)
{
	const char* szScript = g_pOptions->GetPostProcess();
	if (!szScript || strlen(szScript) == 0)
	{
		return;
	}
		
	info("Executing post-process for %s (%s)", szPath, Util::BaseFileName(szNZBFilename));
	if (!Util::FileExists(szScript))
	{
		error("Could not start post-process: could not find file %s", szScript);
		return;
	}

	bool bCollectionCompleted = IsCollectionCompleted(NULL, szNZBFilename, false, true);

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


//*********************************************************************************
// PAR-HANDLING


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
			pAspect->pFileInfo->GetNZBInfo()->GetNiceNZBName(szNZBNiceName, 1024);
			if (pAspect->eAction == QueueCoordinator::eaFileCompleted)
			{
				info("Collection %s completely downloaded", szNZBNiceName);
			}
			else if (WasLastInCollection(pAspect->pDownloadQueue, pAspect->pFileInfo, false))
			{
				info("Collection %s deleted from queue", szNZBNiceName);
			}

			if (pAspect->eAction == QueueCoordinator::eaFileCompleted)
			{
#ifndef DISABLE_PARCHECK
				if (g_pOptions->GetParCheck() && 
					g_pOptions->GetDecoder() != Options::dcNone)
				{
					CheckPars(pAspect->pDownloadQueue, pAspect->pFileInfo);
				}
				else
#endif
				{
					ExecPostScript(pAspect->pFileInfo->GetNZBInfo()->GetDestDir(), pAspect->pFileInfo->GetNZBInfo()->GetFilename(), "", PARSTATUS_NOT_CHECKED);
				}
			}
		}

#ifndef DISABLE_PARCHECK
		if (IsCollectionCompleted(pAspect->pDownloadQueue, pAspect->pFileInfo->GetNZBInfo()->GetFilename(), false, false))
		{
			m_mutexParChecker.Lock();
			ClearCompletedParJobs(pAspect->pFileInfo->GetNZBInfo()->GetFilename());
			m_mutexParChecker.Unlock();
		}
#endif
	}
}

void PrePostProcessor::PausePars(DownloadQueue* pDownloadQueue, const char* szNZBFilename)
{
	debug("PrePostProcessor: Pausing pars");
	
	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (!strcmp(pFileInfo->GetNZBInfo()->GetFilename(), szNZBFilename))
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
* Check if the completed file was last (unpaused, if bIgnorePaused is "true") file in nzb-collection
*/
bool PrePostProcessor::WasLastInCollection(DownloadQueue* pDownloadQueue, FileInfo * pFileInfo, bool bIgnorePaused)
{
	debug("File %s completed or deleted", pFileInfo->GetFilename());

	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
	{
		FileInfo* pFileInfo2 = *it;
		if (pFileInfo2 != pFileInfo && (!bIgnorePaused || !pFileInfo2->GetPaused()) &&
			(pFileInfo2->GetNZBInfo() == pFileInfo->GetNZBInfo()))
		{
			return false;
		}
	}

	return true;
}

bool PrePostProcessor::IsCollectionCompleted(DownloadQueue* pDownloadQueue, const char* szNZBFilename, 
	bool bIgnoreFirstInParQueue, bool bIgnorePaused)
{
	bool bCollectionCompleted = true;

	bool bNeedQueueLock = !pDownloadQueue;
	if (bNeedQueueLock)
	{
		pDownloadQueue = g_pQueueCoordinator->LockQueue();
	}

	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if ((!bIgnorePaused || !pFileInfo->GetPaused()) &&
			!strcmp(pFileInfo->GetNZBInfo()->GetFilename(), szNZBFilename))
		{
			bCollectionCompleted = false;
			break;
		}
	}

	if (bNeedQueueLock)
	{
		g_pQueueCoordinator->UnlockQueue();
	}
		
#ifndef DISABLE_PARCHECK
	if (bCollectionCompleted)
	{
		m_mutexParChecker.Lock();
		for (ParQueue::iterator it = m_ParQueue.begin() + (int)bIgnoreFirstInParQueue; it != m_ParQueue.end(); it++)
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

	return bCollectionCompleted;
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

#ifndef DISABLE_PARCHECK

void PrePostProcessor::CheckPars(DownloadQueue * pDownloadQueue, FileInfo * pFileInfo)
{
	char szNZBNiceName[1024];
	pFileInfo->GetNZBInfo()->GetNiceNZBName(szNZBNiceName, 1024);

	m_mutexParChecker.Lock();

	FileList fileList;
	if (FindMainPars(pFileInfo->GetNZBInfo()->GetDestDir(), &fileList))
	{
		for (FileList::iterator it = fileList.begin(); it != fileList.end(); it++)
		{
			char* szParFilename = *it;
			debug("Found par: %s", szParFilename);

			char szFullFilename[1024];
			snprintf(szFullFilename, 1024, "%s%c%s", pFileInfo->GetNZBInfo()->GetDestDir(), (int)PATH_SEPARATOR, szParFilename);
			szFullFilename[1024-1] = '\0';

			if (!ParJobExists(&m_ParQueue, szFullFilename) && 
				!ParJobExists(&m_CompletedParJobs, szFullFilename))
			{
				char szInfoName[1024];
				int iBaseLen = 0;
				ParseParFilename(szParFilename, &iBaseLen, NULL);
				int maxlen = iBaseLen < 1024 ? iBaseLen : 1024 - 1;
				strncpy(szInfoName, szParFilename, maxlen);
				szInfoName[maxlen] = '\0';
				
				char szParInfoName[1024];
				snprintf(szParInfoName, 1024, "%s%c%s", szNZBNiceName, (int)PATH_SEPARATOR, szInfoName);
				szParInfoName[1024-1] = '\0';
				
				info("Queueing %s%c%s for par-check", szNZBNiceName, (int)PATH_SEPARATOR, szInfoName);
				ParJob* pParJob = new ParJob(pFileInfo->GetNZBInfo()->GetFilename(), szFullFilename, szParInfoName);
				m_ParQueue.push_back(pParJob);
				m_bHasMoreJobs = true;
			}

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
		if (ParseParFilename(filename, &iBaseLen, NULL))
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
		!strcmp(pFileInfo->GetNZBInfo()->GetFilename(), m_ParChecker.GetNZBFilename()) &&
		SameParCollection(pFileInfo->GetFilename(), Util::BaseFileName(m_ParChecker.GetParFilename()));
	if (bSameCollection && !bDeleted)
	{
		char szFullFilename[1024];
		snprintf(szFullFilename, 1024, "%s%c%s", pFileInfo->GetNZBInfo()->GetDestDir(), (int)PATH_SEPARATOR, pFileInfo->GetFilename());
		szFullFilename[1024-1] = '\0';
		m_ParChecker.AddParFile(szFullFilename);
	}
	else
	{
		m_ParChecker.QueueChanged();
	}
	m_mutexParChecker.Unlock();
	return bSameCollection;
}

bool PrePostProcessor::SameParCollection(const char* szFilename1, const char* szFilename2)
{
	int iBaseLen1 = 0, iBaseLen2 = 0;
	return ParseParFilename(szFilename1, &iBaseLen1, NULL) &&
		ParseParFilename(szFilename2, &iBaseLen2, NULL) &&
		iBaseLen1 == iBaseLen2 &&
		!strncasecmp(szFilename1, szFilename2, iBaseLen1);
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
			
			if (!m_ParChecker.GetRepairNotNeeded() || Util::FileExists(szBrokenLogName))
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

		bool bCollectionCompleted = IsCollectionCompleted(NULL, m_ParChecker.GetNZBFilename(), true, true);
		bool bHasFailedParJobs = HasFailedParJobs(m_ParChecker.GetNZBFilename());

		if (g_pOptions->GetParCleanupQueue() && m_ParChecker.GetStatus() == ParChecker::psFinished && 
			bCollectionCompleted && !bHasFailedParJobs)
		{
			ParCleanupQueue(m_ParChecker.GetNZBFilename());
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
		
		m_mutexParChecker.Lock();

		ParJob* pParJob = m_ParQueue.front();
		m_ParQueue.pop_front();
		m_bHasMoreJobs = !m_ParQueue.empty();

		pParJob->m_bFailed = m_ParChecker.GetStatus() == ParChecker::psFailed;
		m_CompletedParJobs.push_back(pParJob);

		m_mutexParChecker.Unlock();

		if (IsCollectionCompleted(NULL, m_ParChecker.GetNZBFilename(), false, false))
		{
			ClearCompletedParJobs(m_ParChecker.GetNZBFilename());
		}

		ExecPostScript(szPath, m_ParChecker.GetNZBFilename(), m_ParChecker.GetParFilename(), iParStatus);
	}
}

/**
 * Delete unneeded (paused) par-files from download queue after successful par-check.
 * If the collection has paused non-par-files, none files will be deleted (even pars).
 */
void PrePostProcessor::ParCleanupQueue(const char* szNZBFilename)
{
	// check if nzb-file has only pars paused
	int ID = 0;
	bool bOnlyPars = true;
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();
	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (!strcmp(pFileInfo->GetNZBInfo()->GetFilename(), szNZBFilename))
		{
			ID = pFileInfo->GetID();
			if (!pFileInfo->GetPaused() && !pFileInfo->GetDeleted() &&
				!ParseParFilename(pFileInfo->GetFilename(), NULL, NULL))
			{
				bOnlyPars = false;
				break;
			}
		}
	}
	g_pQueueCoordinator->UnlockQueue();

	if (bOnlyPars && ID > 0)
	{
		char szNZBNiceName[1024];
		NZBInfo::MakeNiceNZBName(szNZBFilename, szNZBNiceName, sizeof(szNZBNiceName));
		info("Cleaning up download queue for %s", szNZBNiceName);
		g_pQueueCoordinator->GetQueueEditor()->EditEntry(ID, false, QueueEditor::eaGroupDelete, 0);
	}
}

/**
 * Check if nzb-file has failures from other par-jobs
 * (if nzb-file has more than one collections)
 */
bool PrePostProcessor::HasFailedParJobs(const char* szNZBFilename)
{
	bool bHasFailedJobs = false;

	m_mutexParChecker.Lock();
	for (ParQueue::iterator it = m_CompletedParJobs.begin(); it != m_CompletedParJobs.end(); it++)
	{
		ParJob* pParJob = *it;
		if (!strcmp(pParJob->GetNZBFilename(), m_ParChecker.GetNZBFilename()) && pParJob->GetFailed())
		{
			bHasFailedJobs = true;
			break;
		}
	}
	m_mutexParChecker.Unlock();

	return bHasFailedJobs;
}

/**
 * Delete info about completed par-jobs for nzb-collection after the collection is completely downloaded.
 * Mutex "m_mutexParChecker" must be locked prior to call of this funtion.
 */
void PrePostProcessor::ClearCompletedParJobs(const char* szNZBFilename)
{
	for (ParQueue::iterator it = m_CompletedParJobs.begin(); it != m_CompletedParJobs.end();)
	{
		ParJob* pParJob = *it;
		if (!strcmp(szNZBFilename, pParJob->GetNZBFilename()))
		{
			m_CompletedParJobs.erase(it);
			delete pParJob;
			it = m_CompletedParJobs.begin();
			continue;
		}
		it++;
	}
}

/**
 * Mutex "m_mutexParChecker" must be locked prior to call of this funtion.
 */
bool PrePostProcessor::ParJobExists(ParQueue* pParQueue, const char* szParFilename)
{
	for (ParQueue::iterator it = pParQueue->begin(); it != pParQueue->end(); it++)
	{
		ParJob* pParJob = *it;
		if (!strcmp(pParJob->GetParFilename(), szParFilename))
		{
			return true;
		}
	}
	return false;
}

/**
* Unpause par2-files
* returns true, if the files with required number of blocks were unpaused,
* or false if there are no more files in queue for this collection or not enough blocks
*/
bool PrePostProcessor::RequestMorePars(const char* szNZBFilename, const char* szParFilename, int iBlockNeeded, int* pBlockFound)
{
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();
	
	Blocks blocks;
	blocks.clear();
	int iBlockFound = 0;

	FindPars(pDownloadQueue, szNZBFilename, szParFilename, &blocks, true, true, &iBlockFound);
	if (iBlockFound == 0)
	{
		FindPars(pDownloadQueue, szNZBFilename, szParFilename, &blocks, true, false, &iBlockFound);
	}
	if (iBlockFound == 0 && !g_pOptions->GetStrictParName())
	{
		FindPars(pDownloadQueue, szNZBFilename, szParFilename, &blocks, false, false, &iBlockFound);
	}

	if (iBlockFound >= iBlockNeeded)
	{
		char szNZBNiceName[1024];
		NZBInfo::MakeNiceNZBName(szNZBFilename, szNZBNiceName, 1024);

		// 1. first unpause all files with par-blocks less or equal iBlockNeeded
		// starting from the file with max block count.
		// if par-collection was built exponentially and all par-files present,
		// this step selects par-files with exact number of blocks we need.
		while (iBlockNeeded > 0)
		{               
			BlockInfo* pBestBlockInfo = NULL;
			for (Blocks::iterator it = blocks.begin(); it != blocks.end(); it++)
			{
				BlockInfo* pBlockInfo = *it;
				if (pBlockInfo->m_iBlockCount <= iBlockNeeded &&
				   (!pBestBlockInfo || pBestBlockInfo->m_iBlockCount < pBlockInfo->m_iBlockCount))
				{
					pBestBlockInfo = pBlockInfo;
				}
			}
			if (pBestBlockInfo)
			{
				if (pBestBlockInfo->m_pFileInfo->GetPaused())
				{
					info("Unpausing %s%c%s for par-recovery", szNZBNiceName, (int)PATH_SEPARATOR, pBestBlockInfo->m_pFileInfo->GetFilename());
					pBestBlockInfo->m_pFileInfo->SetPaused(false);
				}
				iBlockNeeded -= pBestBlockInfo->m_iBlockCount;
			}
			else
			{
				break;
			}
		}
			
		// 2. then unpause other files
		// this step only needed if the par-collection was built not exponentially 
		// or not all par-files present (or some of them were corrupted)
		// this step is not optimal, but we hope, that the first step will work good 
		// in most cases and we will not need the second step often
		while (iBlockNeeded > 0)
		{
			BlockInfo* pBlockInfo = blocks.front();
			if (pBlockInfo->m_pFileInfo->GetPaused())
			{
				info("Unpausing %s%c%s for par-recovery", szNZBNiceName, (int)PATH_SEPARATOR, pBlockInfo->m_pFileInfo->GetFilename());
				pBlockInfo->m_pFileInfo->SetPaused(false);
			}
			iBlockNeeded -= pBlockInfo->m_iBlockCount;
		}
	}

	g_pQueueCoordinator->UnlockQueue();

	if (pBlockFound)
	{
		*pBlockFound = iBlockFound;
	}

	for (Blocks::iterator it = blocks.begin(); it != blocks.end(); it++)
	{
		delete *it;
	}
	blocks.clear();
	
	return iBlockNeeded <= 0;
}

void PrePostProcessor::FindPars(DownloadQueue * pDownloadQueue, const char* szNZBFilename, const char* szParFilename,
	Blocks * pBlocks, bool bStrictParName, bool bExactParName, int* pBlockFound)
{
    *pBlockFound = 0;
	
	// extract base name from m_szParFilename (trim .par2-extension and possible .vol-part)
	char* szBaseParFilename = Util::BaseFileName(szParFilename);
	char szMainBaseFilename[1024];
	int iMainBaseLen = 0;
	if (!ParseParFilename(szBaseParFilename, &iMainBaseLen, NULL))
	{
		// should not happen
        error("Internal error: could not parse filename %s", szBaseParFilename);
		return;
	}
	int maxlen = iMainBaseLen < 1024 ? iMainBaseLen : 1024 - 1;
	strncpy(szMainBaseFilename, szBaseParFilename, maxlen);
	szMainBaseFilename[maxlen] = '\0';
	for (char* p = szMainBaseFilename; *p; p++) *p = tolower(*p); // convert string to lowercase

	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		int iBlocks = 0;
		if (!strcmp(pFileInfo->GetNZBInfo()->GetFilename(), szNZBFilename) &&
			ParseParFilename(pFileInfo->GetFilename(), NULL, &iBlocks) &&
			iBlocks > 0)
		{
			bool bUseFile = true;

			if (bExactParName)
			{
				bUseFile = SameParCollection(pFileInfo->GetFilename(), Util::BaseFileName(szParFilename));
			}
			else if (bStrictParName)
			{
				// the pFileInfo->GetFilename() may be not confirmed and may contain
				// additional texts if Subject could not be parsed correctly

				char szLoFileName[1024];
				strncpy(szLoFileName, pFileInfo->GetFilename(), 1024);
				szLoFileName[1024-1] = '\0';
				for (char* p = szLoFileName; *p; p++) *p = tolower(*p); // convert string to lowercase
				
				char szCandidateFileName[1024];
				snprintf(szCandidateFileName, 1024, "%s.par2", szMainBaseFilename);
				szCandidateFileName[1024-1] = '\0';
				if (!strstr(szLoFileName, szCandidateFileName))
				{
					snprintf(szCandidateFileName, 1024, "%s.vol", szMainBaseFilename);
					szCandidateFileName[1024-1] = '\0';
					bUseFile = strstr(szLoFileName, szCandidateFileName);
				}
			}

			// if it is a par2-file with blocks and it was from the same NZB-request
			// and it belongs to the same file collection (same base name),
			// then OK, we can use it
			if (bUseFile)
			{
				BlockInfo* pBlockInfo = new BlockInfo();
				pBlockInfo->m_pFileInfo = pFileInfo;
				pBlockInfo->m_iBlockCount = iBlocks;
				pBlocks->push_back(pBlockInfo);
				*pBlockFound += iBlocks;
			}
		}
	}
}

bool PrePostProcessor::ParseParFilename(const char * szParFilename, int* iBaseNameLen, int* iBlocks)
{
	char szFilename[1024];
	strncpy(szFilename, szParFilename, 1024);
	szFilename[1024-1] = '\0';
	for (char* p = szFilename; *p; p++) *p = tolower(*p); // convert string to lowercase

	int iLen = strlen(szFilename);
	if (iLen < 6)
	{
		return false;
	}

	// find last occurence of ".par2" and trim filename after it
	char* szEnd = szFilename;
	while (char* p = strstr(szEnd, ".par2")) szEnd = p + 5;
	*szEnd = '\0';
	iLen = strlen(szFilename);
	
	if (strcasecmp(szFilename + iLen - 5, ".par2"))
	{
		return false;
	}
	*(szFilename + iLen - 5) = '\0';

	int blockcnt = 0;
	char* p = strrchr(szFilename, '.');
	if (p && !strncasecmp(p, ".vol", 4))
	{
		char* b = strchr(p, '+');
		if (!b)
		{
			b = strchr(p, '-');
		}
		if (b)
		{
			blockcnt = atoi(b+1);
			*p = '\0';
		}
	}

	if (iBaseNameLen)
	{
		*iBaseNameLen = strlen(szFilename);
	}
	if (iBlocks)
	{
		*iBlocks = blockcnt;
	}
	
	return true;
}

void PrePostProcessor::UpdateParProgress()
{
	m_mutexParChecker.Lock();

	ParJob* pParJob = m_ParQueue.front();
	if (m_ParChecker.GetFileProgress() == 0)
	{
		pParJob->SetProgressLabel(m_ParChecker.GetProgressLabel());
	}
	pParJob->m_iFileProgress = m_ParChecker.GetFileProgress();
	pParJob->m_iStageProgress = m_ParChecker.GetStageProgress();
    EParJobStage StageKind[] = { ptPreparing, ptVerifying, ptCalculating, ptRepairing };
	EParJobStage eStage = StageKind[m_ParChecker.GetStage()];

	if (!pParJob->m_tStartTime)
	{
		pParJob->m_tStartTime = time(NULL);
	}

	if (pParJob->m_eStage != eStage)
	{
		pParJob->m_eStage = eStage;
		pParJob->m_tStageTime = time(NULL);
	}

	m_mutexParChecker.Unlock();
}

#endif
