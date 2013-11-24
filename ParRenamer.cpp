/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include <stdio.h>
#include <ctype.h>
#ifdef WIN32
#include <par2cmdline.h>
#include <par2repairer.h>
#include <md5.h>
#else
#include <unistd.h>
#include <libpar2/par2cmdline.h>
#include <libpar2/par2repairer.h>
#include <libpar2/md5.h>
#endif

#include "nzbget.h"
#include "ParRenamer.h"
#include "ParCoordinator.h"
#include "Log.h"
#include "Options.h"
#include "Util.h"

extern Options* g_pOptions;

class ParRenamerRepairer : public Par2Repairer
{
public:
	friend class ParRenamer;
};

ParRenamer::FileHash::FileHash(const char* szFilename, const char* szHash)
{
	m_szFilename = strdup(szFilename);
	m_szHash = strdup(szHash);
}

ParRenamer::FileHash::~FileHash()
{
	free(m_szFilename);
	free(m_szHash);
}

ParRenamer::ParRenamer()
{
    debug("Creating ParRenamer");

	m_eStatus = psFailed;
	m_szDestDir = NULL;
	m_szInfoName = NULL;
	m_szProgressLabel = (char*)malloc(1024);
	m_iStageProgress = 0;
	m_bCancelled = false;
}

ParRenamer::~ParRenamer()
{
    debug("Destroying ParRenamer");

	free(m_szDestDir);
	free(m_szInfoName);
	free(m_szProgressLabel);

	Cleanup();
}

void ParRenamer::Cleanup()
{
	ClearHashList();
	
	for (DirList::iterator it = m_DirList.begin(); it != m_DirList.end(); it++)
	{
		free(*it);
	}
	m_DirList.clear();
}

void ParRenamer::ClearHashList()
{
	for (FileHashList::iterator it = m_FileHashList.begin(); it != m_FileHashList.end(); it++)
	{
		delete *it;
	}
	m_FileHashList.clear();
}

void ParRenamer::SetDestDir(const char * szDestDir)
{
	free(m_szDestDir);
	m_szDestDir = strdup(szDestDir);
}

void ParRenamer::SetInfoName(const char * szInfoName)
{
	free(m_szInfoName);
	m_szInfoName = strdup(szInfoName);
}

void ParRenamer::Cancel()
{
	m_bCancelled = true;
}

void ParRenamer::Run()
{
	Cleanup();
	m_bCancelled = false;
	m_iFileCount = 0;
	m_iCurFile = 0;
	m_iRenamedCount = 0;
	m_eStatus = psFailed;

	snprintf(m_szProgressLabel, 1024, "Checking renamed files for %s", m_szInfoName);
	m_szProgressLabel[1024-1] = '\0';
	m_iStageProgress = 0;
	UpdateProgress();

	BuildDirList(m_szDestDir);

	for (DirList::iterator it = m_DirList.begin(); it != m_DirList.end(); it++)
	{
		char* szDestDir = *it;
		debug("Checking %s", szDestDir);
		ClearHashList();
		LoadParFiles(szDestDir);
		CheckFiles(szDestDir);
	}
	
	if (m_bCancelled)
	{
		PrintMessage(Message::mkWarning, "Renaming cancelled for %s", m_szInfoName);
	}
	else if (m_iRenamedCount > 0)
	{
		PrintMessage(Message::mkInfo, "Successfully renamed %i file(s) for %s", m_iRenamedCount, m_szInfoName);
		m_eStatus = psSuccess;
	}
	else
	{
		PrintMessage(Message::mkInfo, "No renamed files found for %s", m_szInfoName);
	}

	Cleanup();
	Completed();
}

void ParRenamer::BuildDirList(const char* szDestDir)
{
	m_DirList.push_back(strdup(szDestDir));

	char* szFullFilename = (char*)malloc(1024);
	DirBrowser* pDirBrowser = new DirBrowser(szDestDir);
	
	while (const char* filename = pDirBrowser->Next())
	{
		if (strcmp(filename, ".") && strcmp(filename, "..") && !m_bCancelled)
		{
			snprintf(szFullFilename, 1024, "%s%c%s", szDestDir, PATH_SEPARATOR, filename);
			szFullFilename[1024-1] = '\0';
			
			if (Util::DirectoryExists(szFullFilename))
			{
				BuildDirList(szFullFilename);
			}
			else
			{
				m_iFileCount++;
			}
		}
	}
	
	free(szFullFilename);
	delete pDirBrowser;
}

void ParRenamer::LoadParFiles(const char* szDestDir)
{
	ParCoordinator::FileList parFileList;
	ParCoordinator::FindMainPars(szDestDir, &parFileList);
	
	for (ParCoordinator::FileList::iterator it = parFileList.begin(); it != parFileList.end(); it++)
	{
		char* szParFilename = *it;
		
		char szFullParFilename[1024];
		snprintf(szFullParFilename, 1024, "%s%c%s", szDestDir, PATH_SEPARATOR, szParFilename);
		szFullParFilename[1024-1] = '\0';
		
		LoadParFile(szFullParFilename);
		
		free(*it);
	}
}

void ParRenamer::LoadParFile(const char* szParFilename)
{
	ParRenamerRepairer* pRepairer = new ParRenamerRepairer();

	if (!pRepairer->LoadPacketsFromFile(szParFilename))
	{
		PrintMessage(Message::mkWarning, "Could not load par2-file %s", szParFilename);
		delete pRepairer;
		return;
	}

	for (map<MD5Hash, Par2RepairerSourceFile*>::iterator it = pRepairer->sourcefilemap.begin(); it != pRepairer->sourcefilemap.end(); it++)
	{
		if (m_bCancelled)
		{
			break;
		}
		
		Par2RepairerSourceFile* sourceFile = (*it).second;
		m_FileHashList.push_back(new FileHash(sourceFile->GetDescriptionPacket()->FileName().c_str(),
			sourceFile->GetDescriptionPacket()->Hash16k().print().c_str()));
	}

	delete pRepairer;
}

void ParRenamer::CheckFiles(const char* szDestDir)
{
	DirBrowser dir(szDestDir);
	while (const char* filename = dir.Next())
	{
		if (strcmp(filename, ".") && strcmp(filename, "..") && !m_bCancelled)
		{
			char szFullFilename[1024];
			snprintf(szFullFilename, 1024, "%s%c%s", szDestDir, PATH_SEPARATOR, filename);
			szFullFilename[1024-1] = '\0';

			if (!Util::DirectoryExists(szFullFilename))
			{
				snprintf(m_szProgressLabel, 1024, "Checking file %s", filename);
				m_szProgressLabel[1024-1] = '\0';
				m_iStageProgress = m_iCurFile * 1000 / m_iFileCount;
				UpdateProgress();
				m_iCurFile++;
				
				CheckFile(szDestDir, szFullFilename);
			}
		}
	}
}

void ParRenamer::CheckFile(const char* szDestDir, const char* szFilename)
{
	debug("Computing hash for %s", szFilename);

	const int iBlockSize = 16*1024;
	
    FILE* pFile = fopen(szFilename, "rb");
    if (!pFile)
    {
		PrintMessage(Message::mkError, "Could not open file %s", szFilename);
        return;
    }

	// load first 16K of the file into buffer
	
	void* pBuffer = malloc(iBlockSize);
	
    int iReadBytes = fread(pBuffer, 1, iBlockSize, pFile);
	int iError = ferror(pFile);
	if (iReadBytes != iBlockSize && iError)
	{
		PrintMessage(Message::mkError, "Could not read file %s", szFilename);
		return;
	}
	
    fclose(pFile);
	
	MD5Hash hash16k;
	MD5Context context;
	context.Update(pBuffer, iReadBytes);
	context.Final(hash16k);
	
	free(pBuffer);
	
	debug("file: %s; hash16k: %s", Util::BaseFileName(szFilename), hash16k.print().c_str());
	
	for (FileHashList::iterator it = m_FileHashList.begin(); it != m_FileHashList.end(); it++)
	{
		FileHash* pFileHash = *it;
		if (!strcmp(pFileHash->GetHash(), hash16k.print().c_str()))
		{
			debug("Found correct filename: %s", pFileHash->GetFilename());
			
			char szDstFilename[1024];
			snprintf(szDstFilename, 1024, "%s%c%s", szDestDir, PATH_SEPARATOR, pFileHash->GetFilename());
			szDstFilename[1024-1] = '\0';
			
			if (!Util::FileExists(szDstFilename))
			{
				PrintMessage(Message::mkInfo, "Renaming %s to %s", Util::BaseFileName(szFilename), pFileHash->GetFilename());
				if (Util::MoveFile(szFilename, szDstFilename))
				{
					m_iRenamedCount++;
				}
				else
				{
					PrintMessage(Message::mkError, "Could not rename %s to %s", szFilename, szDstFilename);
				}
			}
			
			break;
		}
	}
}

#endif
