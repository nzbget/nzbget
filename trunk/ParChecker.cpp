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
#include "Options.h"
#include "Util.h"

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
	m_szProgressLabel = (char*)malloc(1024);
	m_iFileProgress = 0;
	m_iStageProgress = 0;
	m_eStage = ptPreparing;
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
	free(m_szProgressLabel);

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
	m_bRepairNotNeeded = false;
	m_eStage = ptPreparing;
	m_iProcessedFiles = 0;

	info("Verifying %s", m_szInfoName);
	SetStatus(psWorking);

	snprintf(m_szProgressLabel, 1024, "Verifying %s", m_szInfoName);
	m_szProgressLabel[1024-1] = '\0';
	m_iFileProgress = 0;
	m_iStageProgress = 0;
	UpdateProgress();

    debug("par: %s", m_szParFilename);
    CommandLine commandLine;
    const char* argv[] = { "par2", "r", "-v", "-v", m_szParFilename };
    if (!commandLine.Parse(5, (char**)argv))
    {
        error("Could not start par-check for %s. Par-file: %s", m_szInfoName, m_szParFilename);
		SetStatus(psFailed);
        return;
    }

    Result res;

	Repairer* pRepairer = new Repairer();
	m_pRepairer = pRepairer;

	pRepairer->sig_filename.connect(sigc::mem_fun(*this, &ParChecker::signal_filename));
	pRepairer->sig_progress.connect(sigc::mem_fun(*this, &ParChecker::signal_progress));
	pRepairer->sig_done.connect(sigc::mem_fun(*this, &ParChecker::signal_done));
	
    res = pRepairer->PreProcess(commandLine);
    debug("ParChecker: PreProcess-result=%i", res);

	if (res != eSuccess || IsStopped())
	{
       	error("Could not verify %s: %s", m_szInfoName, IsStopped() ? "due stopping" : "par2-file could not be processed");
		SetStatus(psFailed);
		delete pRepairer;
		return;
	}

	char BufReason[1024];
	BufReason[0] = '\0';
	if (m_szErrMsg)
	{
		free(m_szErrMsg);
	}
	m_szErrMsg = NULL;
	
	m_eStage = ptVerifying;
    res = pRepairer->Process(commandLine, false);
    debug("ParChecker: Process-result=%i", res);
	
	while (!IsStopped() && res == eRepairNotPossible)
	{
		int missingblockcount = pRepairer->missingblockcount - pRepairer->recoverypacketmap.size();
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

		LoadMorePars();
		pRepairer->UpdateVerificationResults();
				
		res = pRepairer->Process(commandLine, false);
		debug("ParChecker: Process-result=%i", res);
	}

	if (IsStopped())
	{
		SetStatus(psFailed);
		delete pRepairer;
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

			snprintf(m_szProgressLabel, 1024, "Repairing %s", m_szInfoName);
			m_szProgressLabel[1024-1] = '\0';
			m_iFileProgress = 0;
			m_iStageProgress = 0;
			m_iProcessedFiles = 0;
			m_eStage = ptCalculating;
			m_iFilesToRepair = pRepairer->damagedfilecount + pRepairer->missingfilecount;
			UpdateProgress();

			res = pRepairer->Process(commandLine, true);
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
	
	delete pRepairer;
}

void ParChecker::LoadMorePars()
{
	m_mutexQueuedParFiles.Lock();
	QueuedParFiles moreFiles;
	moreFiles.assign(m_QueuedParFiles.begin(), m_QueuedParFiles.end());
	m_QueuedParFiles.clear();
	m_mutexQueuedParFiles.Unlock();
	
	for (QueuedParFiles::iterator it = moreFiles.begin(); it != moreFiles.end() ;it++)
	{
		char* szParFilename = *it;
		bool loadedOK = ((Repairer*)m_pRepairer)->LoadPacketsFromFile(szParFilename);
		if (loadedOK)
		{
			info("File %s successfully loaded for par-check", Util::BaseFileName(szParFilename), m_szInfoName);
		}
		else
		{
			info("Could not load file %s for par-check", Util::BaseFileName(szParFilename), m_szInfoName);
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
	info("%s file %s", m_eStage == ptCalculating || m_eStage == ptRepairing ? "Repairing" : "Verifying", str.c_str());

	snprintf(m_szProgressLabel, 1024, "%s file %s", m_eStage == ptCalculating || m_eStage == ptRepairing ? "Repairing" : "Verifying", str.c_str());
	m_szProgressLabel[1024-1] = '\0';
	m_iFileProgress = 0;
	if (m_eStage == ptCalculating)
	{
		m_eStage = ptRepairing;
	}
	UpdateProgress();
}

void ParChecker::signal_progress(double progress)
{
	m_iFileProgress = (int)progress;

	if (m_eStage == ptCalculating)
	{
		// calculating repair-data for all files
		m_iStageProgress = m_iFileProgress;
	}
	else
	{
		// processing individual files

		int iTotalFiles = 0;
		if (m_eStage == ptRepairing)
		{
			// repairing individual files
			iTotalFiles = m_iFilesToRepair;
		}
		else
		{
			// verifying individual files
			iTotalFiles = ((Repairer*)m_pRepairer)->sourcefiles.size();
		}

		if (iTotalFiles > 0)
		{
			if (m_iFileProgress < 1000)
			{
				m_iStageProgress = (m_iProcessedFiles * 1000 + m_iFileProgress) / iTotalFiles;
			}
			else
			{
				m_iStageProgress = m_iProcessedFiles * 1000 / iTotalFiles;
			}
		}
		else
		{
			m_iStageProgress = 0;
		}
	}

	debug("Current-progres: %i, Total-progress: %i", m_iFileProgress, m_iStageProgress);

	UpdateProgress();
}

void ParChecker::signal_done(std::string str, int available, int total)
{
	m_iProcessedFiles++;

	if (m_eStage == ptVerifying)
	{
		if (available < total)
		{
			warn("File %s has %i bad block(s) of total %i block(s)", str.c_str(), total - available, total);
		}
	}
}

#endif
