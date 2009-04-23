/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2008 Andrei Prygounkov <hugbug@users.sourceforge.net>
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
#include <cctype>
#include <cstdio>
#include <sys/stat.h>

#include "nzbget.h"
#include "DownloadInfo.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"

extern Options* g_pOptions;

int FileInfo::m_iIDGen = 0;
int PostInfo::m_iIDGen = 0;

NZBParameter::NZBParameter(const char* szName)
{
	m_szName = strdup(szName);
	m_szValue = NULL;
}

NZBParameter::~NZBParameter()
{
	if (m_szName)
	{
		free(m_szName);
	}
	if (m_szValue)
	{
		free(m_szValue);
	}
}

void NZBParameter::SetValue(const char* szValue)
{
	if (m_szValue)
	{
		free(m_szValue);
	}
	m_szValue = strdup(szValue);
}


NZBInfo::NZBInfo()
{
	debug("Creating NZBInfo");

	m_szFilename = NULL;
	m_szDestDir = NULL;
	m_szCategory = strdup("");
	m_iFileCount = 0;
	m_lSize = 0;
	m_iRefCount = 0;
	m_bPostProcess = false;
	m_bDeleted = false;
	m_bParCleanup = false;
	m_bCleanupDisk = false;
	m_szQueuedFilename = strdup("");
}

NZBInfo::~NZBInfo()
{
	debug("Destroying NZBInfo");

	if (m_szFilename)
	{
		free(m_szFilename);
	}
	if (m_szDestDir)
	{
		free(m_szDestDir);
	}
	if (m_szCategory)
	{
		free(m_szCategory);
	}
	if (m_szQueuedFilename)
	{
		free(m_szQueuedFilename);
	}

	for (Files::iterator it = m_completedFiles.begin(); it != m_completedFiles.end(); it++)
	{
		free(*it);
	}
	m_completedFiles.clear();

	for (NZBParameterList::iterator it = m_ppParameters.begin(); it != m_ppParameters.end(); it++)
	{
		delete *it;
	}
	m_ppParameters.clear();
}

void NZBInfo::AddReference()
{
	m_iRefCount++;
}

void NZBInfo::Release()
{
	m_iRefCount--;
	if (m_iRefCount <= 0)
	{
		delete this;
	}
}

void NZBInfo::SetDestDir(const char* szDestDir)
{
	if (m_szDestDir)
	{
		free(m_szDestDir);
	}
	m_szDestDir = strdup(szDestDir);
}

void NZBInfo::SetFilename(const char * szFilename)
{
	if (m_szFilename)
	{
		free(m_szFilename);
	}
	m_szFilename = strdup(szFilename);
}

void NZBInfo::SetCategory(const char* szCategory)
{
	if (m_szCategory)
	{
		free(m_szCategory);
	}
	m_szCategory = strdup(szCategory);
}

void NZBInfo::SetQueuedFilename(const char * szQueuedFilename)
{
	if (m_szQueuedFilename)
	{
		free(m_szQueuedFilename);
	}
	m_szQueuedFilename = strdup(szQueuedFilename);
}

void NZBInfo::GetNiceNZBName(char* szBuffer, int iSize)
{
	MakeNiceNZBName(m_szFilename, szBuffer, iSize);
}

void NZBInfo::MakeNiceNZBName(const char * szNZBFilename, char * szBuffer, int iSize)
{
	char postname[1024];
	const char* szBaseName = Util::BaseFileName(szNZBFilename);

	// if .nzb file has a certain structure, try to strip out certain elements
	if (sscanf(szBaseName, "msgid_%*d_%1023s", postname) == 1)
	{
		// OK, using stripped name
	}
	else
	{
		// using complete filename
		strncpy(postname, szBaseName, 1024);
		postname[1024-1] = '\0';
	}

	// wipe out ".nzb"
	if (char* p = strrchr(postname, '.')) *p = '\0';

	Util::MakeValidFilename(postname, '_', false);

	// if the resulting name is empty, use basename without cleaning up "msgid_"
	if (strlen(postname) == 0)
	{
		// using complete filename
		strncpy(postname, szBaseName, 1024);
		postname[1024-1] = '\0';

		// wipe out ".nzb"
		if (char* p = strrchr(postname, '.')) *p = '\0';

		Util::MakeValidFilename(postname, '_', false);

		// if the resulting name is STILL empty, use "noname"
		if (strlen(postname) == 0)
		{
			strncpy(postname, "noname", 1024);
		}
	}

	strncpy(szBuffer, postname, iSize);
	szBuffer[iSize-1] = '\0';
}

void NZBInfo::BuildDestDirName()
{
	char szBuffer[1024];
	char szCategory[1024];
	bool bHasCategory = m_szCategory && m_szCategory[0] != '\0';
	if (g_pOptions->GetAppendCategoryDir() && bHasCategory)
	{
		strncpy(szCategory, m_szCategory, 1024);
		szCategory[1024 - 1] = '\0';
		Util::MakeValidFilename(szCategory, '_', true);
	}

	if (g_pOptions->GetAppendNZBDir())
	{
		char szNiceNZBName[1024];
		GetNiceNZBName(szNiceNZBName, 1024);
		if (g_pOptions->GetAppendCategoryDir() && bHasCategory)
		{
			snprintf(szBuffer, 1024, "%s%s%c%s", g_pOptions->GetDestDir(), szCategory, PATH_SEPARATOR, szNiceNZBName);
		}
		else
		{
			snprintf(szBuffer, 1024, "%s%s", g_pOptions->GetDestDir(), szNiceNZBName);
		}
		szBuffer[1024-1] = '\0';
	}
	else
	{
		if (g_pOptions->GetAppendCategoryDir() && bHasCategory)
		{
			snprintf(szBuffer, 1024, "%s%s", g_pOptions->GetDestDir(), szCategory);
		}
		else
		{
			strncpy(szBuffer, g_pOptions->GetDestDir(), 1024);
		}
		szBuffer[1024-1] = '\0'; // trim the last slash, always returned by GetDestDir()
	}

	SetDestDir(szBuffer);
}

void NZBInfo::SetParameter(const char* szName, const char* szValue)
{
	NZBParameter* pParameter = NULL;
	bool bDelete = !szValue || !*szValue;

	for (NZBParameterList::iterator it = m_ppParameters.begin(); it != m_ppParameters.end(); it++)
	{
		NZBParameter* pLookupParameter = *it;
		if (!strcmp(pLookupParameter->GetName(), szName))
		{
			if (bDelete)
			{
				delete pLookupParameter;
				m_ppParameters.erase(it);
				return;
			}
			pParameter = pLookupParameter;
			break;
		}
	}

	if (bDelete)
	{
		return;
	}

	if (!pParameter)
	{
		pParameter = new NZBParameter(szName);
		m_ppParameters.push_back(pParameter);
	}

	pParameter->SetValue(szValue);
}

void NZBInfo::BuildNZBList(DownloadQueue* pDownloadQueue, NZBQueue* pNZBQueue)
{
	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		bool inlist = false;
		for (NZBQueue::iterator it = pNZBQueue->begin(); it != pNZBQueue->end(); it++)
		{
			NZBInfo* pNZBInfo = *it;
			if (pNZBInfo == pFileInfo->GetNZBInfo())
			{
				inlist = true;
				break;
			}
		}
		if (!inlist)
		{
			pNZBQueue->push_back(pFileInfo->GetNZBInfo());
		}
	}
}

ArticleInfo::ArticleInfo()
{
	//debug("Creating ArticleInfo");
	m_szMessageID		= NULL;
	m_iSize 			= 0;
	m_eStatus			= aiUndefined;
	m_szResultFilename	= NULL;
}

ArticleInfo::~ ArticleInfo()
{
	//debug("Destroying ArticleInfo");

	if (m_szMessageID)
	{
		free(m_szMessageID);
	}
	if (m_szResultFilename)
	{
		free(m_szResultFilename);
	}
}

void ArticleInfo::SetMessageID(const char * szMessageID)
{
	m_szMessageID = strdup(szMessageID);
}

void ArticleInfo::SetResultFilename(const char * v)
{
	if (m_szResultFilename)
	{
		free(m_szResultFilename);
	}
	m_szResultFilename = strdup(v);
}


FileInfo::FileInfo()
{
	debug("Creating FileInfo");

	m_Articles.clear();
	m_Groups.clear();
	m_szSubject = NULL;
	m_szFilename = NULL;
	m_bFilenameConfirmed = false;
	m_lSize = 0;
	m_lRemainingSize = 0;
	m_bPaused = false;
	m_bDeleted = false;
	m_iCompleted = 0;
	m_bOutputInitialized = false;
	m_pNZBInfo = NULL;
	m_iIDGen++;
	m_iID = m_iIDGen;
}

FileInfo::~ FileInfo()
{
	debug("Destroying FileInfo");

	if (m_szSubject)
	{
		free(m_szSubject);
	}
	if (m_szFilename)
	{
		free(m_szFilename);
	}

	for (Groups::iterator it = m_Groups.begin(); it != m_Groups.end() ;it++)
	{
		free(*it);
	}
	m_Groups.clear();

	ClearArticles();

	if (m_pNZBInfo)
	{
		m_pNZBInfo->Release();
	}
}

void FileInfo::ClearArticles()
{
	for (Articles::iterator it = m_Articles.begin(); it != m_Articles.end() ;it++)
	{
		delete *it;
	}
	m_Articles.clear();
}

void FileInfo::SetID(int s)
{
	m_iID = s;
	if (m_iIDGen < m_iID)
	{
		m_iIDGen = m_iID;
	}
}

void FileInfo::SetNZBInfo(NZBInfo* pNZBInfo)
{
	if (m_pNZBInfo)
	{
		m_pNZBInfo->Release();
	}
	m_pNZBInfo = pNZBInfo;
	m_pNZBInfo->AddReference();
}

void FileInfo::SetSubject(const char* szSubject)
{
	m_szSubject = strdup(szSubject);
}

void FileInfo::SetFilename(const char* szFilename)
{
	if (m_szFilename)
	{
		free(m_szFilename);
	}
	m_szFilename = strdup(szFilename);
}

void FileInfo::MakeValidFilename()
{
	Util::MakeValidFilename(m_szFilename, '_', false);
}

void FileInfo::LockOutputFile()
{
	m_mutexOutputFile.Lock();
}

void FileInfo::UnlockOutputFile()
{
	m_mutexOutputFile.Unlock();
}

bool FileInfo::IsDupe(const char* szFilename)
{
	char fileName[1024];
	snprintf(fileName, 1024, "%s%c%s", m_pNZBInfo->GetDestDir(), (int)PATH_SEPARATOR, szFilename);
	fileName[1024-1] = '\0';
	if (Util::FileExists(fileName))
	{
		return true;
	}
	snprintf(fileName, 1024, "%s%c%s_broken", m_pNZBInfo->GetDestDir(), (int)PATH_SEPARATOR, szFilename);
	fileName[1024-1] = '\0';
	if (Util::FileExists(fileName))
	{
		return true;
	}

	return false;
}

GroupInfo::GroupInfo()
{
	m_iFirstID = 0;
	m_iLastID = 0;
	m_iRemainingFileCount = 0;
	m_lRemainingSize = 0;
	m_lPausedSize = 0;
	m_iRemainingParCount = 0;
}

GroupInfo::~GroupInfo()
{
	if (m_pNZBInfo)
	{
		m_pNZBInfo->Release();
	}
}

void GroupInfo::BuildGroups(DownloadQueue* pDownloadQueue, GroupQueue* pGroupQueue)
{
    for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
    {
        FileInfo* pFileInfo = *it;
		GroupInfo* pGroupInfo = NULL;
		for (GroupQueue::iterator itg = pGroupQueue->begin(); itg != pGroupQueue->end(); itg++)
		{
			GroupInfo* pGroupInfo1 = *itg;
			if (pGroupInfo1->GetNZBInfo() == pFileInfo->GetNZBInfo())
			{
				pGroupInfo = pGroupInfo1;
				break;
			}
		}
		if (!pGroupInfo)
		{
			pGroupInfo = new GroupInfo();
			pGroupInfo->m_pNZBInfo = pFileInfo->GetNZBInfo();
			pGroupInfo->m_pNZBInfo->AddReference();
			pGroupInfo->m_iFirstID = pFileInfo->GetID();
			pGroupInfo->m_iLastID = pFileInfo->GetID();
			pGroupQueue->push_back(pGroupInfo);
		}
		if (pFileInfo->GetID() < pGroupInfo->GetFirstID())
		{
			pGroupInfo->m_iFirstID = pFileInfo->GetID();
		}
		if (pFileInfo->GetID() > pGroupInfo->GetLastID())
		{
			pGroupInfo->m_iLastID = pFileInfo->GetID();
		}
		pGroupInfo->m_iRemainingFileCount++;
		pGroupInfo->m_lRemainingSize += pFileInfo->GetRemainingSize();
		if (pFileInfo->GetPaused())
		{
			pGroupInfo->m_lPausedSize += pFileInfo->GetRemainingSize();
		}

		char szLoFileName[1024];
		strncpy(szLoFileName, pFileInfo->GetFilename(), 1024);
		szLoFileName[1024-1] = '\0';
		for (char* p = szLoFileName; *p; p++) *p = tolower(*p); // convert string to lowercase
		if (strstr(szLoFileName, ".par2"))
		{
			pGroupInfo->m_iRemainingParCount++;
		}
	}
}

PostInfo::PostInfo()
{
	debug("Creating PostInfo");

	m_szNZBFilename = NULL;
	m_szDestDir = NULL;
	m_szParFilename = NULL;
	m_szInfoName = NULL;
	m_szCategory = NULL;
	m_szQueuedFilename = NULL;
	m_bWorking = false;
	m_bParCheck = false;
	m_iParStatus = 0;
	m_eRequestParCheck = rpNone;
	m_bRequestParCleanup = false;
	m_szProgressLabel = strdup("");
	m_iFileProgress = 0;
	m_iStageProgress = 0;
	m_tStartTime = 0;
	m_tStageTime = 0;
	m_eStage = ptQueued;
	m_pScriptThread = NULL;
	m_Messages.clear();
	m_iIDGen++;
	m_iID = m_iIDGen;
}

PostInfo::~ PostInfo()
{
	debug("Destroying PostInfo");

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
	if (m_szCategory)
	{
		free(m_szCategory);
	}
	if (m_szQueuedFilename)
	{
		free(m_szQueuedFilename);
	}
	if (m_szProgressLabel)
	{
		free(m_szProgressLabel);
	}

	for (Messages::iterator it = m_Messages.begin(); it != m_Messages.end(); it++)
	{
		delete *it;
	}
	m_Messages.clear();

	for (NZBParameterList::iterator it = m_ppParameters.begin(); it != m_ppParameters.end(); it++)
	{
		delete *it;
	}
	m_ppParameters.clear();
}

void PostInfo::SetNZBFilename(const char* szNZBFilename)
{
	m_szNZBFilename = strdup(szNZBFilename);
}

void PostInfo::SetDestDir(const char* szDestDir)
{
	m_szDestDir = strdup(szDestDir);
}

void PostInfo::SetParFilename(const char* szParFilename)
{
	m_szParFilename = strdup(szParFilename);
}

void PostInfo::SetInfoName(const char* szInfoName)
{
	m_szInfoName = strdup(szInfoName);
}

void PostInfo::SetCategory(const char* szCategory)
{
	m_szCategory = strdup(szCategory);
}

void PostInfo::SetQueuedFilename(const char * szQueuedFilename)
{
	m_szQueuedFilename = strdup(szQueuedFilename);
}

void PostInfo::SetProgressLabel(const char* szProgressLabel)
{
	if (m_szProgressLabel)
	{
		free(m_szProgressLabel);
	}
	m_szProgressLabel = strdup(szProgressLabel);
}

PostInfo::Messages* PostInfo::LockMessages()
{
	m_mutexLog.Lock();
	return &m_Messages;
}

void PostInfo::UnlockMessages()
{
	m_mutexLog.Unlock();
}

void PostInfo::AppendMessage(Message::EKind eKind, const char * szText)
{
	Message* pMessage = new Message(++m_iIDGen, eKind, time(NULL), szText);

	m_mutexLog.Lock();
	m_Messages.push_back(pMessage);

	while (m_Messages.size() > (unsigned int)g_pOptions->GetLogBufferSize())
	{
		Message* pMessage = m_Messages.front();
		delete pMessage;
		m_Messages.pop_front();
	}
	m_mutexLog.Unlock();
}

void PostInfo::AddParameter(const char* szName, const char* szValue)
{
	NZBParameter* pParameter = new NZBParameter(szName);
	pParameter->SetValue(szValue);
	m_ppParameters.push_back(pParameter);
}

void PostInfo::AssignParameter(NZBParameterList* pSrcParameters)
{
	for (NZBParameterList::iterator it = pSrcParameters->begin(); it != pSrcParameters->end(); it++)
	{
		NZBParameter* pParameter = *it;
		AddParameter(pParameter->GetName(), pParameter->GetValue());
	}
}
