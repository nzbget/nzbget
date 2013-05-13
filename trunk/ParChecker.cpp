/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2009 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#ifndef DISABLE_PARCHECK

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fstream>
#ifdef WIN32
#include <par2cmdline.h>
#include <par2repairer.h>
#else
#include <unistd.h>
#include <libpar2/par2cmdline.h>
#include <libpar2/par2repairer.h>
#endif
#include <algorithm>

#include "nzbget.h"
#include "ParChecker.h"
#include "ParCoordinator.h"
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
private:
    CommandLine	commandLine;

public:
	Result		PreProcess(const char *szParFilename);
	Result		Process(bool dorepair);

	friend class ParChecker;
};

Result Repairer::PreProcess(const char *szParFilename)
{
#ifdef HAVE_PAR2_BUGFIXES_V2
	// Ensure linking against the patched version of libpar2
	BugfixesPatchVersion2();
#endif

	if (g_pOptions->GetParScan() == Options::psFull)
	{
		char szWildcardParam[1024];
		strncpy(szWildcardParam, szParFilename, 1024);
		szWildcardParam[1024-1] = '\0';
		char* szBasename = Util::BaseFileName(szWildcardParam);
		if (szBasename != szWildcardParam && strlen(szBasename) > 0)
		{
			szBasename[0] = '*';
			szBasename[1] = '\0';
		}

		const char* argv[] = { "par2", "r", "-v", "-v", szParFilename, szWildcardParam };
		if (!commandLine.Parse(6, (char**)argv))
		{
			return eInvalidCommandLineArguments;
		}
	}
	else
	{
		const char* argv[] = { "par2", "r", "-v", "-v", szParFilename };
		if (!commandLine.Parse(5, (char**)argv))
		{
			return eInvalidCommandLineArguments;
		}
	}

	return Par2Repairer::PreProcess(commandLine);
}

Result Repairer::Process(bool dorepair)
{
	return Par2Repairer::Process(commandLine, dorepair);
}


class MissingFilesComparator
{
private:
	const char* m_szBaseParFilename;
public:
	MissingFilesComparator(const char* szBaseParFilename) : m_szBaseParFilename(szBaseParFilename) {}
	bool operator()(CommandLine::ExtraFile* pFirst, CommandLine::ExtraFile* pSecond) const;
};


/*
 * Files with the same name as in par-file (and a differnt extension) are
 * placed at the top of the list to be scanned first.
 */
bool MissingFilesComparator::operator()(CommandLine::ExtraFile* pFile1, CommandLine::ExtraFile* pFile2) const
{
	char name1[1024];
	strncpy(name1, Util::BaseFileName(pFile1->FileName().c_str()), 1024);
	name1[1024-1] = '\0';
	if (char* ext = strrchr(name1, '.')) *ext = '\0'; // trim extension

	char name2[1024];
	strncpy(name2, Util::BaseFileName(pFile2->FileName().c_str()), 1024);
	name2[1024-1] = '\0';
	if (char* ext = strrchr(name2, '.')) *ext = '\0'; // trim extension

	return strcmp(name1, m_szBaseParFilename) == 0 && strcmp(name1, name2) != 0;
}


ParChecker::ParChecker()
{
    debug("Creating ParChecker");

	m_eStatus = psFailed;
	m_szDestDir = NULL;
	m_szNZBName = NULL;
	m_szParFilename = NULL;
	m_szInfoName = NULL;
	m_szErrMsg = NULL;
	m_szProgressLabel = (char*)malloc(1024);
	m_iFileProgress = 0;
	m_iStageProgress = 0;
	m_iExtraFiles = 0;
	m_bVerifyingExtraFiles = false;
	m_bCancelled = false;
	m_eStage = ptLoadingPars;
}

ParChecker::~ParChecker()
{
    debug("Destroying ParChecker");

	if (m_szDestDir)
	{
		free(m_szDestDir);
	}
	if (m_szNZBName)
	{
		free(m_szNZBName);
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

	Cleanup();
}

void ParChecker::Cleanup()
{
	for (FileList::iterator it = m_QueuedParFiles.begin(); it != m_QueuedParFiles.end() ;it++)
	{
		free(*it);
	}
	m_QueuedParFiles.clear();

	for (FileList::iterator it = m_ProcessedFiles.begin(); it != m_ProcessedFiles.end() ;it++)
	{
		free(*it);
	}
	m_ProcessedFiles.clear();

	m_sourceFiles.clear();
}

void ParChecker::SetDestDir(const char * szDestDir)
{
	if (m_szDestDir)
	{
		free(m_szDestDir);
	}
	m_szDestDir = strdup(szDestDir);
}

void ParChecker::SetNZBName(const char * szNZBName)
{
	if (m_szNZBName)
	{
		free(m_szNZBName);
	}
	m_szNZBName = strdup(szNZBName);
}

void ParChecker::SetInfoName(const char * szInfoName)
{
	if (m_szInfoName)
	{
		free(m_szInfoName);
	}
	m_szInfoName = strdup(szInfoName);
}

void ParChecker::Run()
{
	ParCoordinator::FileList fileList;
	if (!ParCoordinator::FindMainPars(m_szDestDir, &fileList))
	{
		PrintMessage(Message::mkError, "Could not start par-check for %s. Could not find any par-files", m_szNZBName);
		m_eStatus = psFailed;
		Completed();
		return;
	}

	m_eStatus = psRepairNotNeeded;
	m_bCancelled = false;

	for (ParCoordinator::FileList::iterator it = fileList.begin(); it != fileList.end(); it++)
	{
		char* szParFilename = *it;
		debug("Found par: %s", szParFilename);

		if (!IsStopped() && !m_bCancelled)
		{
			char szFullParFilename[1024];
			snprintf(szFullParFilename, 1024, "%s%c%s", m_szDestDir, (int)PATH_SEPARATOR, szParFilename);
			szFullParFilename[1024-1] = '\0';

			char szInfoName[1024];
			int iBaseLen = 0;
			ParCoordinator::ParseParFilename(szParFilename, &iBaseLen, NULL);
			int maxlen = iBaseLen < 1024 ? iBaseLen : 1024 - 1;
			strncpy(szInfoName, szParFilename, maxlen);
			szInfoName[maxlen] = '\0';
			
			char szParInfoName[1024];
			snprintf(szParInfoName, 1024, "%s%c%s", m_szNZBName, (int)PATH_SEPARATOR, szInfoName);
			szParInfoName[1024-1] = '\0';

			SetInfoName(szParInfoName);

			EStatus eStatus = RunParCheck(szFullParFilename);

			// accumulate total status, the worst status has priority
			if (m_eStatus > eStatus)
			{
				m_eStatus = eStatus;
			}

			if (g_pOptions->GetCreateBrokenLog())
			{
				WriteBrokenLog(eStatus);
			}
		}

		free(szParFilename);
	}

	Completed();
}

void ParChecker::WriteBrokenLog(EStatus eStatus)
{
	char szBrokenLogName[1024];
	snprintf(szBrokenLogName, 1024, "%s%c_brokenlog.txt", m_szDestDir, (int)PATH_SEPARATOR);
	szBrokenLogName[1024-1] = '\0';
	
	if (eStatus != psRepairNotNeeded || Util::FileExists(szBrokenLogName))
	{
		FILE* file = fopen(szBrokenLogName, "ab");
		if (file)
		{
			if (eStatus == psFailed)
			{
				if (m_bCancelled)
				{
					fprintf(file, "Repair cancelled for %s\n", m_szInfoName);
				}
				else
				{
					fprintf(file, "Repair failed for %s: %s\n", m_szInfoName, m_szErrMsg ? m_szErrMsg : "");
				}
			}
			else if (eStatus == psRepairPossible)
			{
				fprintf(file, "Repair possible for %s\n", m_szInfoName);
			}
			else if (eStatus == psRepaired)
			{
				fprintf(file, "Successfully repaired %s\n", m_szInfoName);
			}
			else if (eStatus == psRepairNotNeeded)
			{
				fprintf(file, "Repair not needed for %s\n", m_szInfoName);
			}
			fclose(file);
		}
		else
		{
			PrintMessage(Message::mkError, "Could not open file %s", szBrokenLogName);
		}
	}
}

ParChecker::EStatus ParChecker::RunParCheck(const char* szParFilename)
{
	Cleanup();
	m_szParFilename = szParFilename;
	m_eStage = ptLoadingPars;
	m_iProcessedFiles = 0;
	m_iExtraFiles = 0;
	m_bVerifyingExtraFiles = false;
	EStatus eStatus = psFailed;

	PrintMessage(Message::mkInfo, "Verifying %s", m_szInfoName);

    debug("par: %s", m_szParFilename);
	
    Result res;

	Repairer* pRepairer = new Repairer();
	m_pRepairer = pRepairer;

	pRepairer->sig_filename.connect(sigc::mem_fun(*this, &ParChecker::signal_filename));
	pRepairer->sig_progress.connect(sigc::mem_fun(*this, &ParChecker::signal_progress));
	pRepairer->sig_done.connect(sigc::mem_fun(*this, &ParChecker::signal_done));

	snprintf(m_szProgressLabel, 1024, "Verifying %s", m_szInfoName);
	m_szProgressLabel[1024-1] = '\0';
	m_iFileProgress = 0;
	m_iStageProgress = 0;
	UpdateProgress();

    res = pRepairer->PreProcess(m_szParFilename);
    debug("ParChecker: PreProcess-result=%i", res);

	if (res != eSuccess || IsStopped())
	{
		if (res == eInvalidCommandLineArguments)
		{
			PrintMessage(Message::mkError, "Could not start par-check for %s. Par-file: %s", m_szInfoName, m_szParFilename);
			m_szErrMsg = strdup("Command line could not be parsed");
		}
		else
		{
			PrintMessage(Message::mkError, "Could not verify %s: %s", m_szInfoName, IsStopped() ? "due stopping" : "par2-file could not be processed");
			m_szErrMsg = strdup("par2-file could not be processed");
		}
		delete pRepairer;
		Cleanup();
		return psFailed;
	}

	char BufReason[1024];
	BufReason[0] = '\0';
	if (m_szErrMsg)
	{
		free(m_szErrMsg);
	}
	m_szErrMsg = NULL;
	
	m_eStage = ptVerifyingSources;
    res = pRepairer->Process(false);
    debug("ParChecker: Process-result=%i", res);

	if (!IsStopped() && pRepairer->missingfilecount > 0 && g_pOptions->GetParScan() == Options::psAuto && AddMissingFiles())
	{
		res = pRepairer->Process(false);
		debug("ParChecker: Process-result=%i", res);
	}

	if (!IsStopped() && res == eRepairNotPossible && CheckSplittedFragments())
	{
		pRepairer->UpdateVerificationResults();
		res = pRepairer->Process(false);
		debug("ParChecker: Process-result=%i", res);
	}

	bool bMoreFilesLoaded = true;
	while (!IsStopped() && res == eRepairNotPossible)
	{
		int missingblockcount = pRepairer->missingblockcount - pRepairer->recoverypacketmap.size();
		if (bMoreFilesLoaded)
		{
			PrintMessage(Message::mkInfo, "Need more %i par-block(s) for %s", missingblockcount, m_szInfoName);
		}
		
		m_mutexQueuedParFiles.Lock();
        bool hasMorePars = !m_QueuedParFiles.empty();
		m_mutexQueuedParFiles.Unlock();
		
		if (!hasMorePars)
		{
			int iBlockFound = 0;
			bool requested = RequestMorePars(missingblockcount, &iBlockFound);
			if (requested)
			{
				strncpy(m_szProgressLabel, "Awaiting additional par-files", 1024);
				m_szProgressLabel[1024-1] = '\0';
				m_iFileProgress = 0;
				UpdateProgress();
			}
			
			m_mutexQueuedParFiles.Lock();
			hasMorePars = !m_QueuedParFiles.empty();
			m_bQueuedParFilesChanged = false;
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
				// wait until new files are added by "AddParFile" or a change is signaled by "QueueChanged"
				bool bQueuedParFilesChanged = false;
				while (!bQueuedParFilesChanged && !IsStopped())
				{
					m_mutexQueuedParFiles.Lock();
					bQueuedParFilesChanged = m_bQueuedParFilesChanged;
					m_mutexQueuedParFiles.Unlock();
					usleep(100 * 1000);
				}
			}
		}

		if (IsStopped())
		{
			break;
		}

		bMoreFilesLoaded = LoadMorePars();
		if (bMoreFilesLoaded)
		{
			pRepairer->UpdateVerificationResults();
			res = pRepairer->Process(false);
			debug("ParChecker: Process-result=%i", res);
		}
	}

	if (IsStopped())
	{
		delete pRepairer;
		Cleanup();
		return psFailed;
	}
	
	eStatus = psFailed;
	
	if (res == eSuccess)
	{
    	PrintMessage(Message::mkInfo, "Repair not needed for %s", m_szInfoName);
		eStatus = psRepairNotNeeded;
	}
	else if (res == eRepairPossible)
	{
		eStatus = psRepairPossible;
		if (g_pOptions->GetParRepair())
		{
			PrintMessage(Message::mkInfo, "Repairing %s", m_szInfoName);

			SaveSourceList();
			snprintf(m_szProgressLabel, 1024, "Repairing %s", m_szInfoName);
			m_szProgressLabel[1024-1] = '\0';
			m_iFileProgress = 0;
			m_iStageProgress = 0;
			m_iProcessedFiles = 0;
			m_eStage = ptRepairing;
			m_iFilesToRepair = pRepairer->damagedfilecount + pRepairer->missingfilecount;
			UpdateProgress();

			res = pRepairer->Process(true);
    		debug("ParChecker: Process-result=%i", res);
			if (res == eSuccess)
			{
    			PrintMessage(Message::mkInfo, "Successfully repaired %s", m_szInfoName);
				eStatus = psRepaired;
				DeleteLeftovers();
			}
		}
		else
		{
    		PrintMessage(Message::mkInfo, "Repair possible for %s", m_szInfoName);
		}
	}
	
	if (m_bCancelled)
	{
		if (m_eStage >= ptRepairing)
		{
			PrintMessage(Message::mkWarning, "Repair cancelled for %s", m_szInfoName);
			m_szErrMsg = strdup("repair cancelled");
			eStatus = psRepairPossible;
		}
		else
		{
			PrintMessage(Message::mkWarning, "Par-check cancelled for %s", m_szInfoName);
			m_szErrMsg = strdup("par-check cancelled");
			eStatus = psFailed;
		}
	}
	else if (eStatus == psFailed)
	{
		if (!m_szErrMsg && (int)res >= 0 && (int)res <= 8)
		{
			m_szErrMsg = strdup(Par2CmdLineErrStr[res]);
		}
		PrintMessage(Message::mkError, "Repair failed for %s: %s", m_szInfoName, m_szErrMsg ? m_szErrMsg : "");
	}
	
	delete pRepairer;
	Cleanup();
	return eStatus;
}

bool ParChecker::LoadMorePars()
{
	m_mutexQueuedParFiles.Lock();
	FileList moreFiles;
	moreFiles.assign(m_QueuedParFiles.begin(), m_QueuedParFiles.end());
	m_QueuedParFiles.clear();
	m_mutexQueuedParFiles.Unlock();
	
	for (FileList::iterator it = moreFiles.begin(); it != moreFiles.end() ;it++)
	{
		char* szParFilename = *it;
		bool loadedOK = ((Repairer*)m_pRepairer)->LoadPacketsFromFile(szParFilename);
		if (loadedOK)
		{
			PrintMessage(Message::mkInfo, "File %s successfully loaded for par-check", Util::BaseFileName(szParFilename), m_szInfoName);
		}
		else
		{
			PrintMessage(Message::mkInfo, "Could not load file %s for par-check", Util::BaseFileName(szParFilename), m_szInfoName);
		}
		free(szParFilename);
	}

	return !moreFiles.empty();
}

void ParChecker::AddParFile(const char * szParFilename)
{
	m_mutexQueuedParFiles.Lock();
	m_QueuedParFiles.push_back(strdup(szParFilename));
	m_bQueuedParFilesChanged = true;
	m_mutexQueuedParFiles.Unlock();
}

void ParChecker::QueueChanged()
{
	m_mutexQueuedParFiles.Lock();
	m_bQueuedParFilesChanged = true;
	m_mutexQueuedParFiles.Unlock();
}

bool ParChecker::CheckSplittedFragments()
{
	bool bFragmentsAdded = false;

	for (std::vector<Par2RepairerSourceFile*>::iterator it = ((Repairer*)m_pRepairer)->sourcefiles.begin();
		it != ((Repairer*)m_pRepairer)->sourcefiles.end(); it++)
	{
		Par2RepairerSourceFile *sourcefile = *it;
		if (!sourcefile->GetTargetExists() && AddSplittedFragments(sourcefile->TargetFileName().c_str()))
		{
			bFragmentsAdded = true;
		}
	}

	return bFragmentsAdded;
}

bool ParChecker::AddSplittedFragments(const char* szFilename)
{
	char szDirectory[1024];
	strncpy(szDirectory, szFilename, 1024);
	szDirectory[1024-1] = '\0';

	char* szBasename = Util::BaseFileName(szDirectory);
	if (szBasename == szDirectory)
	{
		return false;
	}
	szBasename[-1] = '\0';
	int iBaseLen = strlen(szBasename);

	std::list<CommandLine::ExtraFile> extrafiles;

	DirBrowser dir(szDirectory);
	while (const char* filename = dir.Next())
	{
		if (!strncasecmp(filename, szBasename, iBaseLen))
		{
			const char* p = filename + iBaseLen;
			if (*p == '.')
			{
				for (p++; *p && strchr("0123456789", *p); p++) ;
				if (!*p)
				{
					debug("Found splitted fragment %s", filename);

					char fullfilename[1024];
					snprintf(fullfilename, 1024, "%s%c%s", szDirectory, PATH_SEPARATOR, filename);
					fullfilename[1024-1] = '\0';

					CommandLine::ExtraFile extrafile(fullfilename, Util::FileSize(fullfilename));
					extrafiles.push_back(extrafile);
				}
			}
		}
	}

	bool bFragmentsAdded = false;

	if (!extrafiles.empty())
	{
		m_iExtraFiles += extrafiles.size();
		m_bVerifyingExtraFiles = true;
		bFragmentsAdded = ((Repairer*)m_pRepairer)->VerifyExtraFiles(extrafiles);
		m_bVerifyingExtraFiles = false;
	}

	return bFragmentsAdded;
}

bool ParChecker::AddMissingFiles()
{
    PrintMessage(Message::mkInfo, "Performing extra par-scan for %s", m_szInfoName);

	char szDirectory[1024];
	strncpy(szDirectory, m_szParFilename, 1024);
	szDirectory[1024-1] = '\0';

	char* szBasename = Util::BaseFileName(szDirectory);
	if (szBasename == szDirectory)
	{
		return false;
	}
	szBasename[-1] = '\0';

	std::list<CommandLine::ExtraFile*> extrafiles;

	DirBrowser dir(szDirectory);
	while (const char* filename = dir.Next())
	{
		if (strcmp(filename, ".") && strcmp(filename, "..") && strcmp(filename, "_brokenlog.txt"))
		{
			bool bAlreadyScanned = false;
			for (FileList::iterator it = m_ProcessedFiles.begin(); it != m_ProcessedFiles.end(); it++)
			{
				const char* szProcessedFilename = *it;
				if (!strcasecmp(Util::BaseFileName(szProcessedFilename), filename))
				{
					bAlreadyScanned = true;
					break;
				}
			}

			if (!bAlreadyScanned)
			{
				char fullfilename[1024];
				snprintf(fullfilename, 1024, "%s%c%s", szDirectory, PATH_SEPARATOR, filename);
				fullfilename[1024-1] = '\0';

				extrafiles.push_back(new CommandLine::ExtraFile(fullfilename, Util::FileSize(fullfilename)));
			}
		}
	}

	// Sort the list
	char* szBaseParFilename = strdup(Util::BaseFileName(m_szParFilename));
	if (char* ext = strrchr(szBaseParFilename, '.')) *ext = '\0'; // trim extension
	extrafiles.sort(MissingFilesComparator(szBaseParFilename));
	free(szBaseParFilename);

	// Scan files
	bool bFilesAdded = false;
	if (!extrafiles.empty())
	{
		m_iExtraFiles += extrafiles.size();
		m_bVerifyingExtraFiles = true;

		std::list<CommandLine::ExtraFile> extrafiles1;

		// adding files one by one until all missing files are found

		while (!IsStopped() && !m_bCancelled && extrafiles.size() > 0 && ((Repairer*)m_pRepairer)->missingfilecount > 0)
		{
			CommandLine::ExtraFile* pExtraFile = extrafiles.front();
			extrafiles.pop_front();

			extrafiles1.clear();
			extrafiles1.push_back(*pExtraFile);

			bFilesAdded = ((Repairer*)m_pRepairer)->VerifyExtraFiles(extrafiles1) || bFilesAdded;
			((Repairer*)m_pRepairer)->UpdateVerificationResults();

			delete pExtraFile;
		}

		m_bVerifyingExtraFiles = false;

		// free any remaining objects
		for (std::list<CommandLine::ExtraFile*>::iterator it = extrafiles.begin(); it != extrafiles.end() ;it++)
		{
			delete *it;
		}
	}

	return bFilesAdded;
}

void ParChecker::signal_filename(std::string str)
{
    const char* szStageMessage[] = { "Loading file", "Verifying file", "Repairing file", "Verifying repaired file" };

	if (m_eStage == ptRepairing)
	{
		m_eStage = ptVerifyingRepaired;
	}

	PrintMessage(Message::mkInfo, "%s %s", szStageMessage[m_eStage], str.c_str());

	if (m_eStage == ptLoadingPars || m_eStage == ptVerifyingSources)
	{
		m_ProcessedFiles.push_back(strdup(str.c_str()));
	}

	snprintf(m_szProgressLabel, 1024, "%s %s", szStageMessage[m_eStage], str.c_str());
	m_szProgressLabel[1024-1] = '\0';
	m_iFileProgress = 0;
	UpdateProgress();
}

void ParChecker::signal_progress(double progress)
{
	m_iFileProgress = (int)progress;

	if (m_eStage == ptRepairing)
	{
		// calculating repair-data for all files
		m_iStageProgress = m_iFileProgress;
	}
	else
	{
		// processing individual files

		int iTotalFiles = 0;
		if (m_eStage == ptVerifyingRepaired)
		{
			// repairing individual files
			iTotalFiles = m_iFilesToRepair;
		}
		else
		{
			// verifying individual files
			iTotalFiles = ((Repairer*)m_pRepairer)->sourcefiles.size() + m_iExtraFiles;
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

	if (m_eStage == ptVerifyingSources)
	{
		if (available < total && !m_bVerifyingExtraFiles)
		{
			bool bFileExists = true;

			for (std::vector<Par2RepairerSourceFile*>::iterator it = ((Repairer*)m_pRepairer)->sourcefiles.begin();
				it != ((Repairer*)m_pRepairer)->sourcefiles.end(); it++)
			{
				Par2RepairerSourceFile *sourcefile = *it;
				if (sourcefile && !strcmp(str.c_str(), Util::BaseFileName(sourcefile->TargetFileName().c_str())) &&
					!sourcefile->GetTargetExists())
				{
					bFileExists = false;
					break;
				}
			}

			if (bFileExists)
			{
				PrintMessage(Message::mkWarning, "File %s has %i bad block(s) of total %i block(s)", str.c_str(), total - available, total);
			}
			else
			{
				PrintMessage(Message::mkWarning, "File %s with %i block(s) is missing", str.c_str(), total);
			}
		}
	}
}

void ParChecker::Cancel()
{
#ifdef HAVE_PAR2_CANCEL
	((Repairer*)m_pRepairer)->cancelled = true;
	m_bCancelled = true;
#else
	PrintMessage(Message::mkError, "Could not cancel par-repair. The program was compiled using version of libpar2 which doesn't support cancelling of par-repair. Please apply libpar2-patches supplied with NZBGet and recompile libpar2 and NZBGet (see README for details).");
#endif
}

void ParChecker::SaveSourceList()
{
	// Buliding a list of DiskFile-objects, marked as source-files
	
	for (std::vector<Par2RepairerSourceFile*>::iterator it = ((Repairer*)m_pRepairer)->sourcefiles.begin();
		it != ((Repairer*)m_pRepairer)->sourcefiles.end(); it++)
	{
		Par2RepairerSourceFile* sourcefile = (Par2RepairerSourceFile*)*it;
		vector<DataBlock>::iterator it2 = sourcefile->SourceBlocks();
		for (int i = 0; i < (int)sourcefile->BlockCount(); i++, it2++)
		{
			DataBlock block = *it2;
			DiskFile* pSourceFile = block.GetDiskFile();
			if (pSourceFile &&
				std::find(m_sourceFiles.begin(), m_sourceFiles.end(), pSourceFile) == m_sourceFiles.end())
			{
				m_sourceFiles.push_back(pSourceFile);
			}
		}
	}
}

void ParChecker::DeleteLeftovers()
{
	// After repairing check if all DiskFile-objects saved by "SaveSourceList()" have
	// corresponding target-files. If not - the source file was replaced. In this case
	// the DiskFile-object points to the renamed bak-file, which we can delete.
	
	for (SourceList::iterator it = m_sourceFiles.begin(); it != m_sourceFiles.end(); it++)
	{
		DiskFile* pSourceFile = (DiskFile*)*it;

		bool bFound = false;
		for (std::vector<Par2RepairerSourceFile*>::iterator it2 = ((Repairer*)m_pRepairer)->sourcefiles.begin();
			it2 != ((Repairer*)m_pRepairer)->sourcefiles.end(); it2++)
		{
			Par2RepairerSourceFile* sourcefile = *it2;
			if (sourcefile->GetTargetFile() == pSourceFile)
			{
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			PrintMessage(Message::mkInfo, "Deleting file %s", Util::BaseFileName(pSourceFile->FileName().c_str()));
			remove(pSourceFile->FileName().c_str());
		}
	}
}

#endif
