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

#ifndef DISABLE_PARCHECK

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef WIN32
#include <par2cmdline.h>
#include <par2repairer.h>
#else
#include <libpar2/par2cmdline.h>
#include <libpar2/par2repairer.h>
#endif

#include "nzbget.h"
#include "ParChecker.h"
#include "Log.h"
#include "QueueCoordinator.h"
#include "Options.h"
#include "Util.h"

extern QueueCoordinator* g_pQueueCoordinator;
extern Options* g_pOptions;

const char* Par2CmdLineErrStr[] = { "OK",
	"data files are damaged and there is enough recovery data available to repair them",
	"data files are damaged and there is insufficient recovery data available to be able to repair them",
	"there was something wrong with the command line arguments",
	"the PAR2 files did not contain sufficient information about the data files to be able to verify them",
	"repair completed but the data files still appear to be damaged",
	"an error occured when accessing files",
	"internal error occurred",
	"out of memory" };

class Repairer : public Par2Repairer
{
	friend class ParChecker;
};

ParChecker::ParChecker()
{
    debug("Creating ParChecker");

	m_eStatus = psUndefined;
	m_szParFilename = NULL;
	m_szNZBFilename = NULL;
	m_szInfoName = NULL;
	m_szErrMsg = NULL;
	m_QueuedParFiles.clear();
}

ParChecker::~ParChecker()
{
    debug("Destroying ParChecker");

	if (m_szParFilename)
	{
		free(m_szParFilename);
	}
	if (m_szNZBFilename)
	{
		free(m_szNZBFilename);
	}
	if (m_szInfoName)
	{
		free(m_szInfoName);
	}
	if (m_szErrMsg)
	{
		free(m_szErrMsg);
	}

	for (QueuedParFiles::iterator it = m_QueuedParFiles.begin(); it != m_QueuedParFiles.end() ;it++)
	{
		free(*it);
	}
	m_QueuedParFiles.clear();
}

void ParChecker::SetParFilename(const char * szParFilename)
{
	if (m_szParFilename)
	{
		free(m_szParFilename);
	}
	m_szParFilename = strdup(szParFilename);
}

void ParChecker::SetInfoName(const char * szInfoName)
{
	if (m_szInfoName)
	{
		free(m_szInfoName);
	}
	m_szInfoName = strdup(szInfoName);
}

void ParChecker::SetNZBFilename(const char * szNZBFilename)
{
	if (m_szNZBFilename)
	{
		free(m_szNZBFilename);
	}
	m_szNZBFilename = strdup(szNZBFilename);
}

void ParChecker::SetStatus(EStatus eStatus)
{
	m_eStatus = eStatus;
	Notify(NULL);
}

void ParChecker::Run()
{
    info("Verifying %s", m_szInfoName);
	SetStatus(psWorking);
	
    debug("par: %s", m_szParFilename);
    CommandLine commandLine;
    const char* argv[] = { "par2", "r", "-q", "-q", m_szParFilename };
    if (!commandLine.Parse(5, (char**)argv))
    {
        error("Could not start par-check for %s. Par-file: %s", m_szInfoName, m_szParFilename);
		SetStatus(psFailed);
        return;
    }

    Result res;
    Repairer* repairer = new Repairer();
	repairer->sig_filename.connect(sigc::mem_fun(*this, &ParChecker::signal_filename));
	
    res = repairer->PreProcess(commandLine);
    debug("ParChecker: PreProcess-result=%i", res);

	if (res != eSuccess || IsStopped())
	{
       	error("Could not verify %s: ", m_szInfoName, IsStopped() ? "due stopping" : "par2-file could not be processed");
		SetStatus(psFailed);
		delete repairer;
		return;
	}

	char BufReason[1024];
	BufReason[0] = '\0';
	if (m_szErrMsg)
	{
		free(m_szErrMsg);
	}
	m_szErrMsg = NULL;
	
	m_bRepairNotNeeded = false;
	m_bRepairing = false;
    res = repairer->Process(commandLine, false);
    debug("ParChecker: Process-result=%i", res);
	
	while (!IsStopped() && res == eRepairNotPossible)
	{
		int missingblockcount = repairer->missingblockcount - repairer->recoverypacketmap.size();
		info("Need more %i par-block(s) for %s", missingblockcount, m_szInfoName);
		
		m_mutexQueuedParFiles.Lock();
        bool hasMorePars = !m_QueuedParFiles.empty();
		m_mutexQueuedParFiles.Unlock();
		
		if (!hasMorePars)
		{
			int iBlockFound = 0;
			bool requested = RequestMorePars(missingblockcount, &iBlockFound);
			
			m_mutexQueuedParFiles.Lock();
			hasMorePars = !m_QueuedParFiles.empty();
			m_mutexQueuedParFiles.Unlock();
			
			if (!requested && !hasMorePars)
			{
				snprintf(BufReason, 1024, "not enough par-blocks, %i block(s) needed, but %i block(s) available", missingblockcount, iBlockFound);
                BufReason[1024-1] = '\0';
				m_szErrMsg = strdup(BufReason);
				break;
			}
			
			if (!hasMorePars)
			{
				m_semNeedMoreFiles.Wait();
			}
		}

		if (IsStopped())
		{
			break;
		}

		LoadMorePars(repairer);
		repairer->UpdateVerificationResults();
				
		m_bRepairing = false;
		res = repairer->Process(commandLine, false);
		debug("ParChecker: Process-result=%i", res);
	}

	if (IsStopped())
	{
		SetStatus(psFailed);
		delete repairer;
		return;
	}
	
	if (res == eSuccess)
	{
    	info("Repair not needed for %s", m_szInfoName);
		m_bRepairNotNeeded = true;
	}
	else if (res == eRepairPossible)
	{
		if (g_pOptions->GetParRepair())
		{
    		info("Repairing %s", m_szInfoName);
			m_bRepairing = true;
    		res = repairer->Process(commandLine, true);
    		debug("ParChecker: Process-result=%i", res);
			if (res == eSuccess)
			{
    			info("Successfully repaired %s", m_szInfoName);
			}
		}
		else
		{
    		info("Repair possible for %s", m_szInfoName);
			res = eSuccess;
		}
	}
	
	if (res == eSuccess)
	{
		SetStatus(psFinished);
	}
	else
	{
		if (!m_szErrMsg && (int)res >= 0 && (int)res <= 8)
		{
			m_szErrMsg = strdup(Par2CmdLineErrStr[res]);
		}
		error("Repair failed for %s: %s", m_szInfoName, m_szErrMsg ? m_szErrMsg : "");
		SetStatus(psFailed);
	}
	
	delete repairer;
}

bool ParChecker::ParseParFilename(const char * szParFilename, int* iBaseNameLen, int* iBlocks)
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

/**
* Unpause par2-files
* returns true, if the files with required number of blocks were unpaused,
* or false if there are no more files in queue for this collection or not enough blocks
*/
bool ParChecker::RequestMorePars(int iBlockNeeded, int* pBlockFound)
{
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();
	
	Blocks blocks;
	blocks.clear();
	int iBlockFound = 0;

	FindPars(pDownloadQueue, &blocks, true, &iBlockFound);
	if (iBlockFound == 0 && !g_pOptions->GetStrictParName())
	{
		FindPars(pDownloadQueue, &blocks, false, &iBlockFound);
	}

	if (iBlockFound >= iBlockNeeded)
	{
		char szNZBNiceName[1024];
		FileInfo::MakeNiceNZBName(m_szNZBFilename, szNZBNiceName, 1024);

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

void ParChecker::FindPars(DownloadQueue * pDownloadQueue, Blocks * pBlocks, bool bStrictParName, int* pBlockFound)
{
    *pBlockFound = 0;
	
	// extract base name from m_szParFilename (trim .par2-extension and possible .vol-part)
	char* szBaseParFilename = BaseFileName(m_szParFilename);
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
		if (!strcmp(pFileInfo->GetNZBFilename(), m_szNZBFilename) &&
			ParseParFilename(pFileInfo->GetFilename(), NULL, &iBlocks) &&
			iBlocks > 0)
		{
			if (bStrictParName)
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
					if (!strstr(szLoFileName, szCandidateFileName))
					{
						continue;
					}
				}
			}

			// if it is a par2-file with blocks and it was from the same NZB-request
			// and it belongs to the same file collection (same base name),
			// then OK, we can use it
			BlockInfo* pBlockInfo = new BlockInfo();
			pBlockInfo->m_pFileInfo = pFileInfo;
			pBlockInfo->m_iBlockCount = iBlocks;
			pBlocks->push_back(pBlockInfo);
			*pBlockFound += iBlocks;
		}
	}
}

void ParChecker::LoadMorePars(void* repairer)
{
	m_mutexQueuedParFiles.Lock();
	QueuedParFiles moreFiles;
	moreFiles.assign(m_QueuedParFiles.begin(), m_QueuedParFiles.end());
	m_QueuedParFiles.clear();
	m_mutexQueuedParFiles.Unlock();
	
	for (QueuedParFiles::iterator it = moreFiles.begin(); it != moreFiles.end() ;it++)
	{
		char* szParFilename = *it;
		bool loadedOK = ((Repairer*)repairer)->LoadPacketsFromFile(szParFilename);
		if (loadedOK)
		{
			info("File %s successfully loaded for par-check", BaseFileName(szParFilename), m_szInfoName);
		}
		else
		{
			info("Could not load file %s for par-check", BaseFileName(szParFilename), m_szInfoName);
		}
		free(szParFilename);
	}
}

void ParChecker::AddParFile(const char * szParFilename)
{
	m_mutexQueuedParFiles.Lock();
	m_QueuedParFiles.push_back(strdup(szParFilename));
	m_semNeedMoreFiles.Post();
	m_mutexQueuedParFiles.Unlock();
}

void ParChecker::QueueChanged()
{
	m_mutexQueuedParFiles.Lock();
	m_semNeedMoreFiles.Post();
	m_mutexQueuedParFiles.Unlock();
}

void ParChecker::signal_filename(std::string str)
{
	info("%s file %s", m_bRepairing ? "Repairing" : "Verifying", str.c_str());
}

#endif
