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
#include <sys/wait.h>
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

PrePostProcessor::PostJob::PostJob(const char * szNZBFilename, const char* szDestDir,
	const char * szParFilename, const char * szInfoName, bool bParCheck)
{
	m_szNZBFilename = strdup(szNZBFilename);
	m_szDestDir = strdup(szDestDir);
	m_szParFilename = strdup(szParFilename);
	m_szInfoName = strdup(szInfoName);
	m_bWorking = false;
	m_bParCheck = bParCheck;
	m_iParStatus = PARSTATUS_NOT_CHECKED;
	m_bParFailed = false;
	m_szProgressLabel = NULL;
	m_iFileProgress = 0;
	m_iStageProgress = 0;
	m_tStartTime = 0;
	m_tStageTime = 0;
	m_eStage = PrePostProcessor::ptQueued;
}

PrePostProcessor::PostJob::~ PostJob()
{
	if (m_szNZBFilename)
	{
		free(m_szNZBFilename);
	}
	if (m_szDestDir)
	{
		free(m_szDestDir);
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

void PrePostProcessor::PostJob::SetProgressLabel(const char* szProgressLabel)
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

	m_PostQueue.clear();

	const char* szScript = g_pOptions->GetPostProcess();
	m_bPostScript = szScript && strlen(szScript) > 0;

#ifndef DISABLE_PARCHECK
	m_ParCheckerObserver.owner = this;
	m_ParChecker.Attach(&m_ParCheckerObserver);
	m_ParChecker.m_Owner = this;
	m_CompletedJobs.clear();
#endif
}

PrePostProcessor::~PrePostProcessor()
{
	debug("Destroying PrePostProcessor");
	
	for (PostQueue::iterator it = m_PostQueue.begin(); it != m_PostQueue.end(); it++)
	{
		delete *it;
	}

#ifndef DISABLE_PARCHECK
	for (PostQueue::iterator it = m_CompletedJobs.begin(); it != m_CompletedJobs.end(); it++)
	{
		delete *it;
	}
#endif
}

void PrePostProcessor::Run()
{
	debug("Entering PrePostProcessor-loop");

	int iNZBDirInterval = 0;
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

		// check post-queue every 200 msec
		CheckPostQueue();

		usleep(200 * 1000);
	}

	debug("Exiting PrePostProcessor-loop");
}

void PrePostProcessor::Stop()
{
	Thread::Stop();
#ifndef DISABLE_PARCHECK
	m_mutexQueue.Lock();
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
	
	m_mutexQueue.Unlock();
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
			IsNZBFileCompleted(pAspect->pDownloadQueue, pAspect->pFileInfo->GetNZBInfo()->GetFilename(), false, true, false) &&
			(!pAspect->pFileInfo->GetPaused() || IsNZBFileCompleted(pAspect->pDownloadQueue, pAspect->pFileInfo->GetNZBInfo()->GetFilename(), false, false, false)))
		{
			char szNZBNiceName[1024];
			pAspect->pFileInfo->GetNZBInfo()->GetNiceNZBName(szNZBNiceName, 1024);
			if (pAspect->eAction == QueueCoordinator::eaFileCompleted)
			{
				info("Collection %s completely downloaded", szNZBNiceName);
			}
			else if (IsNZBFileCompleted(pAspect->pDownloadQueue, pAspect->pFileInfo->GetNZBInfo()->GetFilename(), false, false, false))
			{
				info("Collection %s deleted from queue", szNZBNiceName);
			}

			if (pAspect->eAction == QueueCoordinator::eaFileCompleted)
			{
#ifndef DISABLE_PARCHECK
				(g_pOptions->GetParCheck() && g_pOptions->GetDecode() && 
					CheckPars(pAspect->pDownloadQueue, pAspect->pFileInfo)) ||
#endif
				CheckScript(pAspect->pFileInfo);
			}
		}

		m_mutexQueue.Lock();
		if (IsNZBFileCompleted(pAspect->pDownloadQueue, pAspect->pFileInfo->GetNZBInfo()->GetFilename(), false, false, true))
		{
			ClearCompletedJobs(pAspect->pFileInfo->GetNZBInfo()->GetFilename());
		}
		m_mutexQueue.Unlock();
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
			snprintf(fullfilename, 1024, "%s%s", g_pOptions->GetNzbDir(), filename);
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

void PrePostProcessor::CheckPostQueue()
{
	m_mutexQueue.Lock();

	if (!m_PostQueue.empty())
	{
		PostJob* pPostJob = m_PostQueue.front();
		if (pPostJob->m_bWorking && pPostJob->m_eStage == ptExecutingScript)
		{
			CheckScriptFinished(pPostJob);
		}

		if (!pPostJob->m_bWorking)
		{
#ifndef DISABLE_PARCHECK
			if (pPostJob->m_bParCheck && pPostJob->m_iParStatus == PARSTATUS_NOT_CHECKED)
			{
				StartParJob(pPostJob);
			}
			else
#endif
			if (pPostJob->m_eStage == ptQueued)
			{
				StartScriptJob(pPostJob);
			}
			else if (pPostJob->m_eStage == ptFinished)
			{
				JobCompleted(pPostJob);
			}
			else
			{
				error("Internal error: invalid state in post-processor");
			}
		}
	}
	
	m_mutexQueue.Unlock();
}

/**
 * Mutex "m_mutexQueue" must be locked prior to call of this funtion.
 */
void PrePostProcessor::StartScriptJob(PostJob* pPostJob)
{
	const char* szScript = g_pOptions->GetPostProcess();
	if (!szScript || strlen(szScript) == 0)
	{
		pPostJob->m_eStage = ptFinished;
		return;
	}

	pPostJob->SetProgressLabel("Executing post-process-script");
	pPostJob->m_bWorking = true;
	pPostJob->m_eStage = ptExecutingScript;
	if (!pPostJob->m_tStartTime)
	{
		pPostJob->m_tStartTime = time(NULL);
	}
	pPostJob->m_tStageTime = time(NULL);
	pPostJob->m_iStageProgress = 50;

	info("Executing post-process-script for %s", pPostJob->GetInfoName());
	if (!Util::FileExists(szScript))
	{
		error("Could not start post-process-script: could not find file %s", szScript);
		pPostJob->m_bWorking = false;
		pPostJob->m_eStage = ptFinished;
		return;
	}

	bool bNZBFileCompleted = IsNZBFileCompleted(NULL, pPostJob->GetNZBFilename(), true, true, true);

	char szParStatus[10];
	snprintf(szParStatus, 10, "%i", pPostJob->m_iParStatus);
	szParStatus[10-1] = '\0';

	char szCollectionCompleted[10];
	snprintf(szCollectionCompleted, 10, "%i", (int)bNZBFileCompleted);
	szCollectionCompleted[10-1] = '\0';

#ifndef DISABLE_PARCHECK
	bool bHasFailedParJobs = HasFailedParJobs(pPostJob->GetNZBFilename()) || pPostJob->m_bParFailed;
#else
	bool bHasFailedParJobs = false;
#endif
	char szHasFailedParJobs[10];
	snprintf(szHasFailedParJobs, 10, "%i", (int)bHasFailedParJobs);
	szHasFailedParJobs[10-1] = '\0';

#ifdef WIN32
	char szCmdLine[2048];
	snprintf(szCmdLine, 2048, "%s \"%s\" \"%s\" \"%s\" %s %s %s", szScript, pPostJob->GetDestDir(), 
		pPostJob->GetNZBFilename(), pPostJob->GetParFilename(), szParStatus, szCollectionCompleted, szHasFailedParJobs);
	szCmdLine[2048-1] = '\0';
	
	STARTUPINFO StartupInfo;
	memset(&StartupInfo, 0, sizeof(StartupInfo));
	PROCESS_INFORMATION ProcessInfo;

	BOOL bOK = CreateProcess(NULL, szCmdLine, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, pPostJob->GetDestDir(), &StartupInfo, &ProcessInfo);
	if (bOK)
	{
		pPostJob->m_hProcessID = ProcessInfo.hProcess;
	}
	else
	{
		char szErrMsg[255];
		szErrMsg[255-1] = '\0';
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM || FORMAT_MESSAGE_IGNORE_INSERTS || FORMAT_MESSAGE_ARGUMENT_ARRAY, 
			NULL, GetLastError(), 0, szErrMsg, 255, NULL);
		error("Could not start post-process: %s", szErrMsg);
		pPostJob->m_bWorking = false;
		pPostJob->m_eStage = ptFinished;
	}
#else
	char szDestDir[1024];
	strncpy(szDestDir, pPostJob->GetDestDir(), 1024);
	szDestDir[1024-1] = '\0';
	
	char szNZBFilename[1024];
	strncpy(szNZBFilename, pPostJob->GetNZBFilename(), 1024);
	szNZBFilename[1024-1] = '\0';
	
	char szParFilename[1024];
	strncpy(szParFilename, pPostJob->GetParFilename(), 1024);
	szParFilename[1024-1] = '\0';

	pid_t pid = fork();

	if (pid == -1)
	{
		error("Could not start post-process: errno %i", errno);
		pPostJob->m_bWorking = false;
		pPostJob->m_eStage = ptFinished;
		return;
	}
	else if (pid != 0)
	{
		// continue the first instance
		pPostJob->m_hProcessID = pid;
		return;
	}

	// here goes the second instance
		
	int h;
	for (h = getdtablesize(); h >= 0;--h) close(h); /* close all descriptors */
	h = open("/dev/null", O_RDWR); dup(h); dup(h); /* handle standart I/O */
	
	//pPostJob->m_hProcessID = getpid();
	execlp(szScript, szScript, szDestDir, szNZBFilename, szParFilename, 
		szParStatus, szCollectionCompleted, szHasFailedParJobs, NULL);
	error("Could not start post-process: %s", strerror(errno));
	exit(-1);
#endif
}

void PrePostProcessor::CheckScriptFinished(PostJob* pPostJob)
{
#ifdef WIN32
	if (WaitForSingleObject(pPostJob->m_hProcessID, 0) == WAIT_OBJECT_0)
#else
	int iStatus;
	if (waitpid(pPostJob->m_hProcessID, &iStatus, WNOHANG) == -1 || WIFEXITED(iStatus))
#endif
	{
		pPostJob->m_bWorking = false;
		pPostJob->m_eStage = ptFinished;
	}
}

/**
 * Mutex "m_mutexQueue" must be locked prior to call of this funtion.
 */
void PrePostProcessor::JobCompleted(PostJob* pPostJob)
{
	pPostJob->m_bWorking = false;
	pPostJob->SetProgressLabel("");
	pPostJob->m_eStage = ptFinished;

#ifndef DISABLE_PARCHECK
	if (g_pOptions->GetParCleanupQueue() && 
		IsNZBFileCompleted(NULL, pPostJob->GetNZBFilename(), true, true, true) && 
		!HasFailedParJobs(pPostJob->GetNZBFilename()))
	{
		m_mutexQueue.Unlock();
		ParCleanupQueue(pPostJob->GetNZBFilename());
		m_mutexQueue.Lock();
	}
#endif

	for (PostQueue::iterator it = m_PostQueue.begin(); it != m_PostQueue.end(); it++)
	{
		if (pPostJob == *it)
		{
			m_PostQueue.erase(it);
			break;
		}
	}

	m_CompletedJobs.push_back(pPostJob);

	if (IsNZBFileCompleted(NULL, pPostJob->GetNZBFilename(), false, false, true))
	{
		ClearCompletedJobs(pPostJob->GetNZBFilename());
	}

	m_bHasMoreJobs = !m_PostQueue.empty();
}

PrePostProcessor::PostQueue* PrePostProcessor::LockPostQueue()
{
	m_mutexQueue.Lock();
	return &m_PostQueue;
}

void PrePostProcessor::UnlockPostQueue()
{
	m_mutexQueue.Unlock();
}

bool PrePostProcessor::CheckScript(FileInfo * pFileInfo)
{
	bool bJobAdded = false;

	if (m_bPostScript &&
		!JobExists(&m_PostQueue, pFileInfo->GetNZBInfo()->GetFilename()) && 
		!JobExists(&m_CompletedJobs, pFileInfo->GetNZBInfo()->GetFilename()))
	{
		m_mutexQueue.Lock();

		char szNZBNiceName[1024];
		pFileInfo->GetNZBInfo()->GetNiceNZBName(szNZBNiceName, 1024);

		info("Queueing %s for post-process-script", szNZBNiceName);
		PostJob* pPostJob = new PostJob(pFileInfo->GetNZBInfo()->GetFilename(), pFileInfo->GetNZBInfo()->GetDestDir(), "", szNZBNiceName, false);
		m_PostQueue.push_back(pPostJob);
		m_bHasMoreJobs = true;
		bJobAdded = true;

		m_mutexQueue.Unlock();
	}

	return bJobAdded;
}

/**
 * Mutex "m_mutexQueue" must be locked prior to call of this funtion.
 */
bool PrePostProcessor::JobExists(PostQueue* pPostQueue, const char* szNZBFilename)
{
	for (PostQueue::iterator it = pPostQueue->begin(); it != pPostQueue->end(); it++)
	{
		PostJob* pPostJob = *it;
		if (!strcmp(pPostJob->GetNZBFilename(), szNZBFilename))
		{
			return true;
		}
	}
	return false;
}

/**
 * Delete info about completed par-jobs for nzb-collection after the collection is completely downloaded.
 * Mutex "m_mutexQueue" must be locked prior to call of this funtion.
 */
void PrePostProcessor::ClearCompletedJobs(const char* szNZBFilename)
{
	for (PostQueue::iterator it = m_CompletedJobs.begin(); it != m_CompletedJobs.end();)
	{
		PostJob* pPostJob = *it;
		if (!strcmp(szNZBFilename, pPostJob->GetNZBFilename()))
		{
			debug("Deleting completed job %s", pPostJob->GetInfoName());
			m_CompletedJobs.erase(it);
			delete pPostJob;
			it = m_CompletedJobs.begin();
			continue;
		}
		it++;
	}
}

//*********************************************************************************
// PAR-HANDLING

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
 * Mutex "m_mutexQueue" must be locked prior to call of this funtion.
 */
bool PrePostProcessor::IsNZBFileCompleted(DownloadQueue* pDownloadQueue, const char* szNZBFilename, 
	bool bIgnoreFirstInPostQueue, bool bIgnorePaused, bool bCheckPostQueue)
{
	bool bNZBFileCompleted = true;

	bool bNeedQueueLock = !pDownloadQueue;
	if (bNeedQueueLock)
	{
		pDownloadQueue = g_pQueueCoordinator->LockQueue();
	}

	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if ((!bIgnorePaused || !pFileInfo->GetPaused()) && !pFileInfo->GetDeleted() &&
			!strcmp(pFileInfo->GetNZBInfo()->GetFilename(), szNZBFilename))
		{
			bNZBFileCompleted = false;
			break;
		}
	}

	if (bNeedQueueLock)
	{
		g_pQueueCoordinator->UnlockQueue();
	}
		
	if (bNZBFileCompleted && bCheckPostQueue)
	{
		for (PostQueue::iterator it = m_PostQueue.begin() + int(bIgnoreFirstInPostQueue); it != m_PostQueue.end(); it++)
		{
			PostJob* pPostJob = *it;
			if (!strcmp(pPostJob->GetNZBFilename(), szNZBFilename))
			{
				bNZBFileCompleted = false;
				break;
			}
		}
	}

	return bNZBFileCompleted;
}

#ifndef DISABLE_PARCHECK

/**
 * Mutex "m_mutexQueue" must be locked prior to call of this funtion.
 */
void PrePostProcessor::StartParJob(PostJob* pPostJob)
{
	info("Checking pars for %s", pPostJob->GetInfoName());
	m_ParChecker.SetNZBFilename(pPostJob->GetNZBFilename());
	m_ParChecker.SetParFilename(pPostJob->GetParFilename());
	m_ParChecker.SetInfoName(pPostJob->GetInfoName());
	pPostJob->m_bWorking = true;
	m_ParChecker.Start();
}

bool PrePostProcessor::CheckPars(DownloadQueue * pDownloadQueue, FileInfo * pFileInfo)
{
	char szNZBNiceName[1024];
	pFileInfo->GetNZBInfo()->GetNiceNZBName(szNZBNiceName, 1024);

	bool bJobAdded = false;

	m_mutexQueue.Lock();

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

			if (!ParJobExists(&m_PostQueue, szFullFilename) && 
				!ParJobExists(&m_CompletedJobs, szFullFilename))
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
				PostJob* pPostJob = new PostJob(pFileInfo->GetNZBInfo()->GetFilename(), pFileInfo->GetNZBInfo()->GetDestDir(), szFullFilename, szParInfoName, true);
				m_PostQueue.push_back(pPostJob);
				m_bHasMoreJobs = true;
				bJobAdded = true;
			}

			free(szParFilename);
		}
	}

	m_mutexQueue.Unlock();

	return bJobAdded;
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
	m_mutexQueue.Lock();
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
	m_mutexQueue.Unlock();
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

		m_mutexQueue.Lock();

		PostJob* pPostJob = m_PostQueue.front();
		pPostJob->m_bParFailed = m_ParChecker.GetStatus() == ParChecker::psFailed;
		pPostJob->m_bWorking = false;
		pPostJob->m_eStage = ptQueued;

		if (m_ParChecker.GetStatus() == ParChecker::psFailed)
		{
			pPostJob->m_iParStatus = PARSTATUS_FAILED;
		}
		else if (g_pOptions->GetParRepair() || m_ParChecker.GetRepairNotNeeded())
		{
			pPostJob->m_iParStatus = PARSTATUS_REPAIRED;
		}
		else
		{
			pPostJob->m_iParStatus = PARSTATUS_REPAIR_POSSIBLE;
		}

		m_mutexQueue.Unlock();
	}
}

/**
 * Delete unneeded (paused) par-files from download queue after successful par-check.
 * If the collection has paused non-par-files, none files will be deleted (even pars).
 */
void PrePostProcessor::ParCleanupQueue(const char* szNZBFilename)
{
	debug("Cleaning up download queue from par-files");

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
 *
 * Mutex "m_mutexQueue" must be locked prior to call of this funtion.
 */
bool PrePostProcessor::HasFailedParJobs(const char* szNZBFilename)
{
	bool bHasFailedJobs = false;

	for (PostQueue::iterator it = m_CompletedJobs.begin(); it != m_CompletedJobs.end(); it++)
	{
		PostJob* pPostJob = *it;
		if (pPostJob->m_bParCheck && pPostJob->m_bParFailed &&
			!strcmp(pPostJob->GetNZBFilename(), szNZBFilename))
		{
			bHasFailedJobs = true;
			break;
		}
	}

	return bHasFailedJobs;
}

/**
 * Mutex "m_mutexQueue" must be locked prior to call of this funtion.
 */
bool PrePostProcessor::ParJobExists(PostQueue* pPostQueue, const char* szParFilename)
{
	for (PostQueue::iterator it = pPostQueue->begin(); it != pPostQueue->end(); it++)
	{
		PostJob* pPostJob = *it;
		if (pPostJob->m_bParCheck && !strcmp(pPostJob->GetParFilename(), szParFilename))
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
	m_mutexQueue.Lock();

	PostJob* pPostJob = m_PostQueue.front();
	if (m_ParChecker.GetFileProgress() == 0)
	{
		pPostJob->SetProgressLabel(m_ParChecker.GetProgressLabel());
	}
	pPostJob->m_iFileProgress = m_ParChecker.GetFileProgress();
	pPostJob->m_iStageProgress = m_ParChecker.GetStageProgress();
    EPostJobStage StageKind[] = { ptLoadingPars, ptVerifyingSources, ptRepairing, ptVerifyingRepaired };
	EPostJobStage eStage = StageKind[m_ParChecker.GetStage()];

	if (!pPostJob->m_tStartTime)
	{
		pPostJob->m_tStartTime = time(NULL);
	}

	if (pPostJob->m_eStage != eStage)
	{
		pPostJob->m_eStage = eStage;
		pPostJob->m_tStageTime = time(NULL);
	}

	m_mutexQueue.Unlock();
}

#endif
