/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include <fstream>
#ifdef WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include "nzbget.h"
#include "ParCoordinator.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"
#include "QueueCoordinator.h"
#include "DiskState.h"

extern QueueCoordinator* g_pQueueCoordinator;
extern Options* g_pOptions;
extern DiskState* g_pDiskState;

#ifndef DISABLE_PARCHECK
bool ParCoordinator::PostParChecker::RequestMorePars(int iBlockNeeded, int* pBlockFound)
{
	return m_pOwner->RequestMorePars(m_pPostInfo->GetNZBInfo(), GetParFilename(), iBlockNeeded, pBlockFound);
}

void ParCoordinator::PostParChecker::UpdateProgress()
{
	m_pOwner->UpdateParCheckProgress();
}

void ParCoordinator::PostParRenamer::UpdateProgress()
{
	m_pOwner->UpdateParRenameProgress();
}
#endif

ParCoordinator::ParCoordinator()
{
	debug("Creating ParCoordinator");

#ifndef DISABLE_PARCHECK
	m_bStopped = false;
	m_ParChecker.m_pOwner = this;
	m_ParRenamer.m_pOwner = this;

	const char* szPostScript = g_pOptions->GetPostProcess();
	m_bPostScript = szPostScript && strlen(szPostScript) > 0;
#endif
}

ParCoordinator::~ParCoordinator()
{
	debug("Destroying ParCoordinator");
}

#ifndef DISABLE_PARCHECK
void ParCoordinator::Stop()
{
	debug("Stopping ParCoordinator");

	m_bStopped = true;

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
}
#endif

void ParCoordinator::PausePars(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	debug("ParCoordinator: Pausing pars");
	
	for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (pFileInfo->GetNZBInfo() == pNZBInfo)
		{
			g_pQueueCoordinator->GetQueueEditor()->LockedEditEntry(pDownloadQueue, pFileInfo->GetID(), false, 
				(g_pOptions->GetLoadPars() == Options::lpOne ||
					(g_pOptions->GetLoadPars() == Options::lpNone && g_pOptions->GetParCheck()))
				? QueueEditor::eaGroupPauseExtraPars : QueueEditor::eaGroupPauseAllPars,
				0, NULL);
			break;
		}
	}
}

bool ParCoordinator::FindMainPars(const char* szPath, FileList* pFileList)
{
	if (pFileList)
	{
		pFileList->clear();
	}

	DirBrowser dir(szPath);
	while (const char* filename = dir.Next())
	{
		int iBaseLen = 0;
		if (ParseParFilename(filename, &iBaseLen, NULL))
		{
			if (!pFileList)
			{
				return true;
			}

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
	return pFileList && !pFileList->empty();
}

bool ParCoordinator::SameParCollection(const char* szFilename1, const char* szFilename2)
{
	int iBaseLen1 = 0, iBaseLen2 = 0;
	return ParseParFilename(szFilename1, &iBaseLen1, NULL) &&
		ParseParFilename(szFilename2, &iBaseLen2, NULL) &&
		iBaseLen1 == iBaseLen2 &&
		!strncasecmp(szFilename1, szFilename2, iBaseLen1);
}

bool ParCoordinator::ParseParFilename(const char* szParFilename, int* iBaseNameLen, int* iBlocks)
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

#ifndef DISABLE_PARCHECK

/**
 * DownloadQueue must be locked prior to call of this function.
 */
void ParCoordinator::StartParCheckJob(PostInfo* pPostInfo)
{
	info("Checking pars for %s", pPostInfo->GetInfoName());
	m_eCurrentJob = jkParCheck;
	m_ParChecker.SetPostInfo(pPostInfo);
	m_ParChecker.SetDestDir(pPostInfo->GetNZBInfo()->GetDestDir());
	m_ParChecker.SetNZBName(pPostInfo->GetNZBInfo()->GetName());
	pPostInfo->SetWorking(true);
	m_ParChecker.Start();
}

/**
 * DownloadQueue must be locked prior to call of this function.
 */
void ParCoordinator::StartParRenameJob(PostInfo* pPostInfo)
{
	info("Checking renamed files for %s", pPostInfo->GetNZBInfo()->GetName());
	m_eCurrentJob = jkParRename;
	m_ParRenamer.SetPostInfo(pPostInfo);
	m_ParRenamer.SetDestDir(pPostInfo->GetNZBInfo()->GetDestDir());
	m_ParRenamer.SetInfoName(pPostInfo->GetNZBInfo()->GetName());
	pPostInfo->SetWorking(true);
	m_ParRenamer.Start();
}

bool ParCoordinator::Cancel()
{
	if (m_eCurrentJob == jkParCheck)
	{
#ifdef HAVE_PAR2_CANCEL
		if (!m_ParChecker.GetCancelled())
		{
			debug("Cancelling par-repair for %s", m_ParChecker.GetInfoName());
			m_ParChecker.Cancel();
			return true;
		}
#else
		warn("Cannot cancel par-repair for %s, used version of libpar2 does not support cancelling", m_ParChecker.GetInfoName());
#endif
	}
	else if (m_eCurrentJob == jkParRename)
	{
		if (!m_ParRenamer.GetCancelled())
		{
			debug("Cancelling par-rename for %s", m_ParRenamer.GetInfoName());
			m_ParRenamer.Cancel();
			return true;
		}
	}
	return false;
}

/**
 * DownloadQueue must be locked prior to call of this function.
 */
bool ParCoordinator::AddPar(FileInfo* pFileInfo, bool bDeleted)
{
	bool bSameCollection = m_ParChecker.IsRunning() &&
		pFileInfo->GetNZBInfo() == m_ParChecker.GetPostInfo()->GetNZBInfo() &&
		SameParCollection(pFileInfo->GetFilename(), Util::BaseFileName(m_ParChecker.GetParFilename()));
	if (bSameCollection && !bDeleted)
	{
		char szFullFilename[1024];
		snprintf(szFullFilename, 1024, "%s%c%s", pFileInfo->GetNZBInfo()->GetDestDir(), (int)PATH_SEPARATOR, pFileInfo->GetFilename());
		szFullFilename[1024-1] = '\0';
		m_ParChecker.AddParFile(szFullFilename);

		if (g_pOptions->GetParPauseQueue())
		{
			PauseDownload();
		}
	}
	else
	{
		m_ParChecker.QueueChanged();
	}
	return bSameCollection;
}

void ParCoordinator::ParCheckCompleted()
{
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	PostInfo* pPostInfo = m_ParChecker.GetPostInfo();

	// Update ParStatus (accumulate result)
	if ((m_ParChecker.GetStatus() == ParChecker::psRepaired ||
		m_ParChecker.GetStatus() == ParChecker::psRepairNotNeeded) &&
		pPostInfo->GetNZBInfo()->GetParStatus() <= NZBInfo::psSkipped)
	{
		pPostInfo->GetNZBInfo()->SetParStatus(NZBInfo::psSuccess);
	}
	else if (m_ParChecker.GetStatus() == ParChecker::psRepairPossible &&
		pPostInfo->GetNZBInfo()->GetParStatus() != NZBInfo::psFailure)
	{
		pPostInfo->GetNZBInfo()->SetParStatus(NZBInfo::psRepairPossible);
	}
	else
	{
		pPostInfo->GetNZBInfo()->SetParStatus(NZBInfo::psFailure);
	}

	pPostInfo->SetWorking(false);
	pPostInfo->SetStage(PostInfo::ptQueued);

	if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
	{
		g_pDiskState->SaveDownloadQueue(pDownloadQueue);
	}

	g_pQueueCoordinator->UnlockQueue();
}

/**
* Unpause par2-files
* returns true, if the files with required number of blocks were unpaused,
* or false if there are no more files in queue for this collection or not enough blocks
*/
bool ParCoordinator::RequestMorePars(NZBInfo* pNZBInfo, const char* szParFilename, int iBlockNeeded, int* pBlockFound)
{
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();
	
	Blocks blocks;
	blocks.clear();
	int iBlockFound = 0;
    int iCurBlockFound = 0;

	FindPars(pDownloadQueue, pNZBInfo, szParFilename, &blocks, true, true, &iCurBlockFound);
    iBlockFound += iCurBlockFound;
	if (iBlockFound < iBlockNeeded)
	{
		FindPars(pDownloadQueue, pNZBInfo, szParFilename, &blocks, true, false, &iCurBlockFound);
        iBlockFound += iCurBlockFound;
	}
	if (iBlockFound < iBlockNeeded && !g_pOptions->GetStrictParName())
	{
		FindPars(pDownloadQueue, pNZBInfo, szParFilename, &blocks, false, false, &iCurBlockFound);
        iBlockFound += iCurBlockFound;
	}

	if (iBlockFound >= iBlockNeeded)
	{
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
					info("Unpausing %s%c%s for par-recovery", pNZBInfo->GetName(), (int)PATH_SEPARATOR, pBestBlockInfo->m_pFileInfo->GetFilename());
					pBestBlockInfo->m_pFileInfo->SetPaused(false);
					pBestBlockInfo->m_pFileInfo->SetExtraPriority(true);
				}
				iBlockNeeded -= pBestBlockInfo->m_iBlockCount;
				blocks.remove(pBestBlockInfo);
				delete pBestBlockInfo;
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
				info("Unpausing %s%c%s for par-recovery", pNZBInfo->GetName(), (int)PATH_SEPARATOR, pBlockInfo->m_pFileInfo->GetFilename());
				pBlockInfo->m_pFileInfo->SetPaused(false);
				pBlockInfo->m_pFileInfo->SetExtraPriority(true);
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

	bool bOK = iBlockNeeded <= 0;

	if (bOK && g_pOptions->GetParPauseQueue())
	{
		UnpauseDownload();
	}

	return bOK;
}

void ParCoordinator::FindPars(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, const char* szParFilename,
	Blocks* pBlocks, bool bStrictParName, bool bExactParName, int* pBlockFound)
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

	for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		int iBlocks = 0;
		if (pFileInfo->GetNZBInfo() == pNZBInfo &&
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

            bool bAlreadyAdded = false;
            // check if file is not in the list already
			if (bUseFile)
			{
				for (Blocks::iterator it = pBlocks->begin(); it != pBlocks->end(); it++)
				{
					BlockInfo* pBlockInfo = *it;
					if (pBlockInfo->m_pFileInfo == pFileInfo)
					{
						bAlreadyAdded = true;
						break;
                	}
        		}
			}
                
			// if it is a par2-file with blocks and it was from the same NZB-request
			// and it belongs to the same file collection (same base name),
			// then OK, we can use it
            if (bUseFile && !bAlreadyAdded)
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

void ParCoordinator::UpdateParCheckProgress()
{
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	PostInfo* pPostInfo = pDownloadQueue->GetPostQueue()->front();
	if (m_ParChecker.GetFileProgress() == 0)
	{
		pPostInfo->SetProgressLabel(m_ParChecker.GetProgressLabel());
	}
	pPostInfo->SetFileProgress(m_ParChecker.GetFileProgress());
	pPostInfo->SetStageProgress(m_ParChecker.GetStageProgress());
    PostInfo::EStage StageKind[] = { PostInfo::ptLoadingPars, PostInfo::ptVerifyingSources, PostInfo::ptRepairing, PostInfo::ptVerifyingRepaired };
	PostInfo::EStage eStage = StageKind[m_ParChecker.GetStage()];
	time_t tCurrent = time(NULL);

	if (!pPostInfo->GetStartTime())
	{
		pPostInfo->SetStartTime(tCurrent);
	}

	if (pPostInfo->GetStage() != eStage)
	{
		pPostInfo->SetStage(eStage);
		pPostInfo->SetStageTime(tCurrent);
	}

	bool bParCancel = false;
#ifdef HAVE_PAR2_CANCEL
	if (!m_ParChecker.GetCancelled())
	{
		if ((g_pOptions->GetParTimeLimit() > 0) &&
			m_ParChecker.GetStage() == ParChecker::ptRepairing &&
			((g_pOptions->GetParTimeLimit() > 5 && tCurrent - pPostInfo->GetStageTime() > 5 * 60) ||
			(g_pOptions->GetParTimeLimit() <= 5 && tCurrent - pPostInfo->GetStageTime() > 1 * 60)))
		{
			// first five (or one) minutes elapsed, now can check the estimated time
			int iEstimatedRepairTime = (int)((tCurrent - pPostInfo->GetStartTime()) * 1000 / 
				(pPostInfo->GetStageProgress() > 0 ? pPostInfo->GetStageProgress() : 1));
			if (iEstimatedRepairTime > g_pOptions->GetParTimeLimit() * 60)
			{
				debug("Estimated repair time %i seconds", iEstimatedRepairTime);
				warn("Cancelling par-repair for %s, estimated repair time (%i minutes) exceeds allowed repair time", m_ParChecker.GetInfoName(), iEstimatedRepairTime / 60);
				bParCancel = true;
			}
		}
	}
#endif

	if (bParCancel)
	{
		m_ParChecker.Cancel();
	}

	g_pQueueCoordinator->UnlockQueue();
	
	CheckPauseState(pPostInfo);
}

void ParCoordinator::CheckPauseState(PostInfo* pPostInfo)
{
	if (g_pOptions->GetPausePostProcess())
	{
		time_t tStageTime = pPostInfo->GetStageTime();
		time_t tStartTime = pPostInfo->GetStartTime();
		time_t tWaitTime = time(NULL);
		
		// wait until Post-processor is unpaused
		while (g_pOptions->GetPausePostProcess() && !m_bStopped)
		{
			usleep(100 * 1000);
			
			// update time stamps
			
			time_t tDelta = time(NULL) - tWaitTime;
			
			if (tStageTime > 0)
			{
				pPostInfo->SetStageTime(tStageTime + tDelta);
			}
			
			if (tStartTime > 0)
			{
				pPostInfo->SetStartTime(tStartTime + tDelta);
			}
		}
	}
}

void ParCoordinator::ParRenameCompleted()
{
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();
	
	PostInfo* pPostInfo = m_ParRenamer.GetPostInfo();
	pPostInfo->GetNZBInfo()->SetRenameStatus(m_ParRenamer.GetStatus() == ParRenamer::psSuccess ? NZBInfo::rsSuccess : NZBInfo::rsFailure);
	pPostInfo->SetWorking(false);
	pPostInfo->SetStage(PostInfo::ptQueued);
	
	if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
	{
		g_pDiskState->SaveDownloadQueue(pDownloadQueue);
	}
	
	g_pQueueCoordinator->UnlockQueue();
}

void ParCoordinator::UpdateParRenameProgress()
{
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();
	
	PostInfo* pPostInfo = pDownloadQueue->GetPostQueue()->front();
	pPostInfo->SetProgressLabel(m_ParRenamer.GetProgressLabel());
	pPostInfo->SetStageProgress(m_ParRenamer.GetStageProgress());
	time_t tCurrent = time(NULL);
	
	if (!pPostInfo->GetStartTime())
	{
		pPostInfo->SetStartTime(tCurrent);
	}
	
	if (pPostInfo->GetStage() != PostInfo::ptRenaming)
	{
		pPostInfo->SetStage(PostInfo::ptRenaming);
		pPostInfo->SetStageTime(tCurrent);
	}
	
	g_pQueueCoordinator->UnlockQueue();
	
	CheckPauseState(pPostInfo);
}

#endif
