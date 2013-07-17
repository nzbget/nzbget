/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
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
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#include <map>

#include "nzbget.h"
#include "DownloadInfo.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"

extern Options* g_pOptions;

int FileInfo::m_iIDGen = 0;
int NZBInfo::m_iIDGen = 0;
int PostInfo::m_iIDGen = 0;
int UrlInfo::m_iIDGen = 0;
int HistoryInfo::m_iIDGen = 0;

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


void NZBParameterList::SetParameter(const char* szName, const char* szValue)
{
	NZBParameter* pParameter = NULL;
	bool bDelete = !szValue || !*szValue;

	for (iterator it = begin(); it != end(); it++)
	{
		NZBParameter* pLookupParameter = *it;
		if (!strcmp(pLookupParameter->GetName(), szName))
		{
			if (bDelete)
			{
				delete pLookupParameter;
				erase(it);
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
		push_back(pParameter);
	}

	pParameter->SetValue(szValue);
}


ScriptStatus::ScriptStatus(const char* szName, EStatus eStatus)
{
	m_szName = strdup(szName);
	m_eStatus = eStatus;
}

ScriptStatus::~ScriptStatus()
{
	if (m_szName)
	{
		free(m_szName);
	}
}


ScriptStatusList::~ScriptStatusList()
{
	Clear();
}

void ScriptStatusList::Clear()
{
	for (iterator it = begin(); it != end(); it++)
	{
		delete *it;
	}
	clear();
}

void ScriptStatusList::Add(const char* szScriptName, ScriptStatus::EStatus eStatus)
{
	push_back(new ScriptStatus(szScriptName, eStatus));
}

ScriptStatus::EStatus ScriptStatusList::CalcTotalStatus()
{
	ScriptStatus::EStatus eStatus = ScriptStatus::srNone;

	for (iterator it = begin(); it != end(); it++)
	{
		ScriptStatus* pScriptStatus = *it;
		// Failure-Status overrides Success-Status
		if ((pScriptStatus->GetStatus() == ScriptStatus::srSuccess && eStatus == ScriptStatus::srNone) ||
			(pScriptStatus->GetStatus() == ScriptStatus::srFailure))
		{
			eStatus = pScriptStatus->GetStatus();
		}
	}
	
	return eStatus;
}


NZBInfo::NZBInfo()
{
	debug("Creating NZBInfo");

	m_szFilename = NULL;
	m_szDestDir = NULL;
	m_szCategory = strdup("");
	m_szName = NULL;
	m_iFileCount = 0;
	m_iParkedFileCount = 0;
	m_lSize = 0;
	m_iRefCount = 0;
	m_bPostProcess = false;
	m_eRenameStatus = rsNone;
	m_eParStatus = psNone;
	m_eUnpackStatus = usNone;
	m_eCleanupStatus = csNone;
	m_eMoveStatus = msNone;
	m_bDeleted = false;
	m_bParCleanup = false;
	m_bCleanupDisk = false;
	m_bUnpackCleanedUpDisk = false;
	m_szQueuedFilename = strdup("");
	m_Owner = NULL;
	m_Messages.clear();
	m_iIDMessageGen = 0;
	m_iIDGen++;
	m_iID = m_iIDGen;
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
	if (m_szName)
	{
		free(m_szName);
	}
	if (m_szQueuedFilename)
	{
		free(m_szQueuedFilename);
	}

	ClearCompletedFiles();

	for (NZBParameterList::iterator it = m_ppParameters.begin(); it != m_ppParameters.end(); it++)
	{
		delete *it;
	}
	m_ppParameters.clear();

	for (Messages::iterator it = m_Messages.begin(); it != m_Messages.end(); it++)
	{
		delete *it;
	}
	m_Messages.clear();

	if (m_Owner)
	{
		m_Owner->Remove(this);
	}
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

void NZBInfo::SetID(int iID)
{
	m_iID = iID;
	if (m_iIDGen < m_iID)
	{
		m_iIDGen = m_iID;
	}
}

void NZBInfo::ClearCompletedFiles()
{
	for (Files::iterator it = m_completedFiles.begin(); it != m_completedFiles.end(); it++)
	{
		free(*it);
	}
	m_completedFiles.clear();
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

	if (!m_szName)
	{
		char szNZBNicename[1024];
		MakeNiceNZBName(m_szFilename, szNZBNicename, sizeof(szNZBNicename), true);
		szNZBNicename[1024-1] = '\0';
		SetName(szNZBNicename);
	}
}

void NZBInfo::SetName(const char* szName)
{
	if (m_szName)
	{
		free(m_szName);
	}
	m_szName = szName ? strdup(szName) : NULL;
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

void NZBInfo::MakeNiceNZBName(const char * szNZBFilename, char * szBuffer, int iSize, bool bRemoveExt)
{
	char postname[1024];
	const char* szBaseName = Util::BaseFileName(szNZBFilename);

	strncpy(postname, szBaseName, 1024);
	postname[1024-1] = '\0';

	if (bRemoveExt)
	{
		// wipe out ".nzb"
		char* p = strrchr(postname, '.');
		if (p && !strcasecmp(p, ".nzb")) *p = '\0';
	}

	Util::MakeValidFilename(postname, '_', false);

	strncpy(szBuffer, postname, iSize);
	szBuffer[iSize-1] = '\0';
}

void NZBInfo::BuildDestDirName()
{
	char szDestDir[1024];

	if (strlen(g_pOptions->GetInterDir()) == 0)
	{
		BuildFinalDirName(szDestDir, 1024);
	}
	else
	{
		snprintf(szDestDir, 1024, "%s%s", g_pOptions->GetInterDir(), GetName());
		szDestDir[1024-1] = '\0';
	}

	SetDestDir(szDestDir);
}

void NZBInfo::BuildFinalDirName(char* szFinalDirBuf, int iBufSize)
{
	char szBuffer[1024];
	bool bUseCategory = m_szCategory && m_szCategory[0] != '\0';

	snprintf(szFinalDirBuf, iBufSize, "%s", g_pOptions->GetDestDir());
	szFinalDirBuf[iBufSize-1] = '\0';

	if (bUseCategory)
	{
		Options::Category *pCategory = g_pOptions->FindCategory(m_szCategory, false);
		if (pCategory && pCategory->GetDestDir() && pCategory->GetDestDir()[0] != '\0')
		{
			snprintf(szFinalDirBuf, iBufSize, "%s", pCategory->GetDestDir());
			szFinalDirBuf[iBufSize-1] = '\0';
			bUseCategory = false;
		}
	}

	if (g_pOptions->GetAppendCategoryDir() && bUseCategory)
	{
		char szCategoryDir[1024];
		strncpy(szCategoryDir, m_szCategory, 1024);
		szCategoryDir[1024 - 1] = '\0';
		Util::MakeValidFilename(szCategoryDir, '_', true);

		snprintf(szBuffer, 1024, "%s%s%c", szFinalDirBuf, szCategoryDir, PATH_SEPARATOR);
		szBuffer[1024-1] = '\0';
		strncpy(szFinalDirBuf, szBuffer, iBufSize);
	}

	snprintf(szBuffer, 1024, "%s%s", szFinalDirBuf, GetName());
	szBuffer[1024-1] = '\0';
	strncpy(szFinalDirBuf, szBuffer, iBufSize);
}

void NZBInfo::SetParameter(const char* szName, const char* szValue)
{
	m_ppParameters.SetParameter(szName, szValue);
}

NZBInfo::Messages* NZBInfo::LockMessages()
{
	m_mutexLog.Lock();
	return &m_Messages;
}

void NZBInfo::UnlockMessages()
{
	m_mutexLog.Unlock();
}

void NZBInfo::AppendMessage(Message::EKind eKind, time_t tTime, const char * szText)
{
	if (tTime == 0)
	{
		tTime = time(NULL);
	}

	m_mutexLog.Lock();
	Message* pMessage = new Message(++m_iIDMessageGen, eKind, tTime, szText);
	m_Messages.push_back(pMessage);
	m_mutexLog.Unlock();
}

void NZBInfoList::Add(NZBInfo* pNZBInfo)
{
	pNZBInfo->m_Owner = this;
	push_back(pNZBInfo);
}

void NZBInfoList::Remove(NZBInfo* pNZBInfo)
{
	for (iterator it = begin(); it != end(); it++)
	{
		NZBInfo* pNZBInfo2 = *it;
		if (pNZBInfo2 == pNZBInfo)
		{
			erase(it);
			break;
		}
	}
}

void NZBInfoList::ReleaseAll()
{
	int i = 0;
	for (iterator it = begin(); it != end(); )
	{
		NZBInfo* pNZBInfo = *it;
		bool bObjDeleted = pNZBInfo->m_iRefCount == 1;
		pNZBInfo->Release();
		if (bObjDeleted)
		{
			it = begin() + i;
		}
		else
		{
			it++;
			i++;
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
	m_szOutputFilename = NULL;
	m_pMutexOutputFile = NULL;
	m_bFilenameConfirmed = false;
	m_lSize = 0;
	m_lRemainingSize = 0;
	m_tTime = 0;
	m_bPaused = false;
	m_bDeleted = false;
	m_iCompleted = 0;
	m_bOutputInitialized = false;
	m_pNZBInfo = NULL;
	m_iPriority = 0;
	m_bExtraPriority = false;
	m_iActiveDownloads = 0;
	m_bAutoDeleted = false;
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
	if (m_szOutputFilename)
	{
		free(m_szOutputFilename);
	}
	if (m_pMutexOutputFile)
	{
		delete m_pMutexOutputFile;
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

void FileInfo::SetID(int iID)
{
	m_iID = iID;
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
	m_pMutexOutputFile->Lock();
}

void FileInfo::UnlockOutputFile()
{
	m_pMutexOutputFile->Unlock();
}

void FileInfo::SetOutputFilename(const char* szOutputFilename)
{
	if (m_szOutputFilename)
	{
		free(m_szOutputFilename);
	}
	m_szOutputFilename = strdup(szOutputFilename);
}

void FileInfo::SetActiveDownloads(int iActiveDownloads)
{
	m_iActiveDownloads = iActiveDownloads;

	if (m_iActiveDownloads > 0 && !m_pMutexOutputFile)
	{
		m_pMutexOutputFile = new Mutex();
	}
	else if (m_iActiveDownloads == 0 && m_pMutexOutputFile)
	{
		delete m_pMutexOutputFile;
		m_pMutexOutputFile = NULL;
	}
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
	m_iPausedFileCount = 0;
	m_lRemainingSize = 0;
	m_lPausedSize = 0;
	m_iRemainingParCount = 0;
	m_tMinTime = 0;
	m_tMaxTime = 0;
	m_iMinPriority = 0;
	m_iMaxPriority = 0;
	m_iActiveDownloads = 0;
}

GroupInfo::~GroupInfo()
{
	if (m_pNZBInfo)
	{
		m_pNZBInfo->Release();
	}
}

PostInfo::PostInfo()
{
	debug("Creating PostInfo");

	m_pNZBInfo = NULL;
	m_szInfoName = NULL;
	m_bWorking = false;
	m_bDeleted = false;
	m_bRequestParCheck = false;
	m_bRequestParRename = false;
	m_szProgressLabel = strdup("");
	m_iFileProgress = 0;
	m_iStageProgress = 0;
	m_tStartTime = 0;
	m_tStageTime = 0;
	m_eStage = ptQueued;
	m_pPostThread = NULL;
	m_Messages.clear();
	m_iIDMessageGen = 0;
	m_iIDGen++;
	m_iID = m_iIDGen;
}

PostInfo::~ PostInfo()
{
	debug("Destroying PostInfo");

	if (m_szInfoName)
	{
		free(m_szInfoName);
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

	if (m_pNZBInfo)
	{
		m_pNZBInfo->Release();
	}
}

void PostInfo::SetNZBInfo(NZBInfo* pNZBInfo)
{
	if (m_pNZBInfo)
	{
		m_pNZBInfo->Release();
	}
	m_pNZBInfo = pNZBInfo;
	m_pNZBInfo->AddReference();
}

void PostInfo::SetInfoName(const char* szInfoName)
{
	m_szInfoName = strdup(szInfoName);
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
	m_mutexLog.Lock();
	Message* pMessage = new Message(++m_iIDMessageGen, eKind, time(NULL), szText);
	m_Messages.push_back(pMessage);

	while (m_Messages.size() > (unsigned int)g_pOptions->GetLogBufferSize())
	{
		Message* pMessage = m_Messages.front();
		delete pMessage;
		m_Messages.pop_front();
	}
	m_mutexLog.Unlock();
}

void DownloadQueue::BuildGroups(GroupQueue* pGroupQueue)
{
	std::map<int, GroupInfo*> groupMap;

	for (FileQueue::iterator it = GetFileQueue()->begin(); it != GetFileQueue()->end(); it++)
    {
        FileInfo* pFileInfo = *it;
		GroupInfo *&pGroupInfo = groupMap[pFileInfo->GetNZBInfo()->GetID()];
		if (!pGroupInfo)
		{
			pGroupInfo = new GroupInfo();
			pGroupInfo->m_pNZBInfo = pFileInfo->GetNZBInfo();
			pGroupInfo->m_pNZBInfo->AddReference();
			pGroupInfo->m_iFirstID = pFileInfo->GetID();
			pGroupInfo->m_iLastID = pFileInfo->GetID();
			pGroupInfo->m_tMinTime = pFileInfo->GetTime();
			pGroupInfo->m_tMaxTime = pFileInfo->GetTime();
			pGroupInfo->m_iMinPriority = pFileInfo->GetPriority();
			pGroupInfo->m_iMaxPriority = pFileInfo->GetPriority();
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
		if (pFileInfo->GetTime() > 0)
		{
			if (pFileInfo->GetTime() < pGroupInfo->GetMinTime())
			{
				pGroupInfo->m_tMinTime = pFileInfo->GetTime();
			}
			if (pFileInfo->GetTime() > pGroupInfo->GetMaxTime())
			{
				pGroupInfo->m_tMaxTime = pFileInfo->GetTime();
			}
		}
		if (pFileInfo->GetPriority() < pGroupInfo->GetMinPriority())
		{
			pGroupInfo->m_iMinPriority = pFileInfo->GetPriority();
		}
		if (pFileInfo->GetPriority() > pGroupInfo->GetMaxPriority())
		{
			pGroupInfo->m_iMaxPriority = pFileInfo->GetPriority();
		}

		pGroupInfo->m_iActiveDownloads += pFileInfo->GetActiveDownloads();
		pGroupInfo->m_iRemainingFileCount++;
		pGroupInfo->m_lRemainingSize += pFileInfo->GetRemainingSize();
		if (pFileInfo->GetPaused())
		{
			pGroupInfo->m_lPausedSize += pFileInfo->GetRemainingSize();
			pGroupInfo->m_iPausedFileCount++;
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


UrlInfo::UrlInfo()
{
	//debug("Creating ArticleInfo");
	m_szURL = NULL;
	m_szNZBFilename = strdup("");
	m_szCategory = strdup("");
	m_iPriority = 0;
	m_bAddTop = false;
	m_bAddPaused = false;
	m_bForce = false;
	m_eStatus = aiUndefined;
	m_iIDGen++;
	m_iID = m_iIDGen;
}

UrlInfo::~ UrlInfo()
{
	if (m_szURL)
	{
		free(m_szURL);
	}
	if (m_szNZBFilename)
	{
		free(m_szNZBFilename);
	}
	if (m_szCategory)
	{
		free(m_szCategory);
	}
}

void UrlInfo::SetURL(const char* szURL)
{
	if (m_szURL)
	{
		free(m_szURL);
	}
	m_szURL = strdup(szURL);
}

void UrlInfo::SetID(int iID)
{
	m_iID = iID;
	if (m_iIDGen < m_iID)
	{
		m_iIDGen = m_iID;
	}
}

void UrlInfo::SetNZBFilename(const char* szNZBFilename)
{
	if (m_szNZBFilename)
	{
		free(m_szNZBFilename);
	}
	m_szNZBFilename = strdup(szNZBFilename);
}

void UrlInfo::SetCategory(const char* szCategory)
{
	if (m_szCategory)
	{
		free(m_szCategory);
	}
	m_szCategory = strdup(szCategory);
}

void UrlInfo::GetName(char* szBuffer, int iSize)
{
	MakeNiceName(m_szURL, m_szNZBFilename, szBuffer, iSize);
}

void UrlInfo::MakeNiceName(const char* szURL, const char* szNZBFilename, char* szBuffer, int iSize)
{
	URL url(szURL);

	if (strlen(szNZBFilename) > 0)
	{
		char szNZBNicename[1024];
		NZBInfo::MakeNiceNZBName(szNZBFilename, szNZBNicename, sizeof(szNZBNicename), true);
		snprintf(szBuffer, iSize, "%s @ %s", szNZBNicename, url.GetHost());
	}
	else
	{
		snprintf(szBuffer, iSize, "%s%s", url.GetHost(), url.GetResource());
	}

	szBuffer[iSize-1] = '\0';
}


HistoryInfo::HistoryInfo(NZBInfo* pNZBInfo)
{
	m_eKind = hkNZBInfo;
	m_pInfo = pNZBInfo;
	pNZBInfo->AddReference();
	m_tTime = 0;
	m_iIDGen++;
	m_iID = m_iIDGen;
}

HistoryInfo::HistoryInfo(UrlInfo* pUrlInfo)
{
	m_eKind = hkUrlInfo;
	m_pInfo = pUrlInfo;
	m_tTime = 0;
	m_iIDGen++;
	m_iID = m_iIDGen;
}

HistoryInfo::~HistoryInfo()
{
	if (m_eKind == hkNZBInfo && m_pInfo)
	{
		((NZBInfo*)m_pInfo)->Release();
	}
	else if (m_eKind == hkUrlInfo && m_pInfo)
	{
		delete (UrlInfo*)m_pInfo;
	}
}

void HistoryInfo::SetID(int iID)
{
	m_iID = iID;
	if (m_iIDGen < m_iID)
	{
		m_iIDGen = m_iID;
	}
}

void HistoryInfo::GetName(char* szBuffer, int iSize)
{
	if (m_eKind == hkNZBInfo)
	{
		strncpy(szBuffer, GetNZBInfo()->GetName(), iSize);
		szBuffer[iSize-1] = '\0';
	}
	else if (m_eKind == hkUrlInfo)
	{
		GetUrlInfo()->GetName(szBuffer, iSize);
	}
	else
	{
		strncpy(szBuffer, "<unknown>", iSize);
	}
}


FeedInfo::FeedInfo(int iID, const char* szName, const char* szUrl, int iInterval,
	const char* szFilter, bool bPauseNzb, const char* szCategory, int iPriority)
{
	m_iID = iID;
	m_szName = strdup(szName ? szName : "");
	m_szUrl = strdup(szUrl ? szUrl : "");
	m_szFilter = strdup(szFilter ? szFilter : "");
	m_szCategory = strdup(szCategory ? szCategory : "");
	m_iInterval = iInterval;
	m_bPauseNzb = bPauseNzb;
	m_iPriority = iPriority;
	m_tLastUpdate = 0;
	m_bPreview = false;
	m_eStatus = fsUndefined;
	m_szOutputFilename = NULL;
	m_bFetch = false;
	m_bForce = false;
}

FeedInfo::~FeedInfo()
{
	if (m_szName)
	{
		free(m_szName);
	}
	if (m_szUrl)
	{
		free(m_szUrl);
	}
	if (m_szFilter)
	{
		free(m_szFilter);
	}
	if (m_szCategory)
	{
		free(m_szCategory);
	}
	if (m_szOutputFilename)
	{
		free(m_szOutputFilename);
	}
}

void FeedInfo::SetOutputFilename(const char* szOutputFilename)
{
	if (m_szOutputFilename)
	{
		free(m_szOutputFilename);
	}
	m_szOutputFilename = strdup(szOutputFilename);
}


FeedItemInfo::FeedItemInfo()
{
	m_szTitle = NULL;
	m_szFilename = NULL;
	m_szUrl = NULL;
	m_szCategory = NULL;
	m_lSize = 0;
	m_tTime = 0;
	m_bFetched = false;
	m_eStatus = isUnknown;
}

FeedItemInfo::~FeedItemInfo()
{
	if (m_szTitle)
	{
		free(m_szTitle);
	}
	if (m_szFilename)
	{
		free(m_szFilename);
	}
	if (m_szUrl)
	{
		free(m_szUrl);
	}
	if (m_szCategory)
	{
		free(m_szCategory);
	}
}

void FeedItemInfo::SetTitle(const char* szTitle)
{
	if (m_szTitle)
	{
		free(m_szTitle);
	}
	m_szTitle = strdup(szTitle ? szTitle : NULL);
}

void FeedItemInfo::SetFilename(const char* szFilename)
{
	if (m_szFilename)
	{
		free(m_szFilename);
	}
	m_szFilename = strdup(szFilename ? szFilename : NULL);
}

void FeedItemInfo::SetUrl(const char* szUrl)
{
	if (m_szUrl)
	{
		free(m_szUrl);
	}
	m_szUrl = strdup(szUrl ? szUrl : NULL);
}

void FeedItemInfo::SetCategory(const char* szCategory)
{
	if (m_szCategory)
	{
		free(m_szCategory);
	}
	m_szCategory = strdup(szCategory ? szCategory : NULL);
}


FeedHistoryInfo::FeedHistoryInfo(const char* szUrl, FeedHistoryInfo::EStatus eStatus, time_t tLastSeen)
{
	m_szUrl = szUrl ? strdup(szUrl) : NULL;
	m_eStatus = eStatus;
	m_tLastSeen = tLastSeen;
}

FeedHistoryInfo::~FeedHistoryInfo()
{
	if (m_szUrl)
	{
		free(m_szUrl);
	}
}


FeedHistory::~FeedHistory()
{
	Clear();
}

void FeedHistory::Clear()
{
	for (iterator it = begin(); it != end(); it++)
	{
		delete *it;
	}
	clear();
}

void FeedHistory::Add(const char* szUrl, FeedHistoryInfo::EStatus eStatus, time_t tLastSeen)
{
	push_back(new FeedHistoryInfo(szUrl, eStatus, tLastSeen));
}

void FeedHistory::Remove(const char* szUrl)
{
	for (iterator it = begin(); it != end(); it++)
	{
		FeedHistoryInfo* pFeedHistoryInfo = *it;
		if (!strcmp(pFeedHistoryInfo->GetUrl(), szUrl))
		{
			delete pFeedHistoryInfo;
			erase(it);
			break;
		}
	}
}

FeedHistoryInfo* FeedHistory::Find(const char* szUrl)
{
	for (iterator it = begin(); it != end(); it++)
	{
		FeedHistoryInfo* pFeedHistoryInfo = *it;
		if (!strcmp(pFeedHistoryInfo->GetUrl(), szUrl))
		{
			return pFeedHistoryInfo;
		}
	}

	return NULL;
}
