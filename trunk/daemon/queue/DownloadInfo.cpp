/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include <algorithm>

#include "nzbget.h"
#include "DownloadInfo.h"
#include "ArticleWriter.h"
#include "Options.h"
#include "Util.h"

extern Options* g_pOptions;
extern ArticleCache* g_pArticleCache;

int FileInfo::m_iIDGen = 0;
int FileInfo::m_iIDMax = 0;
int NZBInfo::m_iIDGen = 0;
int NZBInfo::m_iIDMax = 0;
DownloadQueue* DownloadQueue::g_pDownloadQueue = NULL;
bool DownloadQueue::g_bLoaded = false;


NZBParameter::NZBParameter(const char* szName)
{
	m_szName = strdup(szName);
	m_szValue = NULL;
}

NZBParameter::~NZBParameter()
{
	free(m_szName);
	free(m_szValue);
}

void NZBParameter::SetValue(const char* szValue)
{
	free(m_szValue);
	m_szValue = strdup(szValue);
}


NZBParameterList::~NZBParameterList()
{
	Clear();
}

void NZBParameterList::Clear()
{
	for (iterator it = begin(); it != end(); it++)
	{
		delete *it;
	}
	clear();
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

NZBParameter* NZBParameterList::Find(const char* szName, bool bCaseSensitive)
{
	for (iterator it = begin(); it != end(); it++)
	{
		NZBParameter* pParameter = *it;
		if ((bCaseSensitive && !strcmp(pParameter->GetName(), szName)) ||
			(!bCaseSensitive && !strcasecmp(pParameter->GetName(), szName)))
		{
			return pParameter;
		}
	}
	
	return NULL;
}

void NZBParameterList::CopyFrom(NZBParameterList* pSourceParameters)
{
	for (iterator it = pSourceParameters->begin(); it != pSourceParameters->end(); it++)
	{
		NZBParameter* pParameter = *it;
		SetParameter(pParameter->GetName(), pParameter->GetValue());
	}
}									  


ScriptStatus::ScriptStatus(const char* szName, EStatus eStatus)
{
	m_szName = strdup(szName);
	m_eStatus = eStatus;
}

ScriptStatus::~ScriptStatus()
{
	free(m_szName);
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


ServerStat::ServerStat(int iServerID)
{
	m_iServerID = iServerID;
	m_iSuccessArticles = 0;
	m_iFailedArticles = 0;
}


ServerStatList::~ServerStatList()
{
	Clear();
}

void ServerStatList::Clear()
{
	for (iterator it = begin(); it != end(); it++)
	{
		delete *it;
	}
	clear();
}

void ServerStatList::StatOp(int iServerID, int iSuccessArticles, int iFailedArticles, EStatOperation eStatOperation)
{
	ServerStat* pServerStat = NULL;
	for (iterator it = begin(); it != end(); it++)
	{
		ServerStat* pServerStat1 = *it;
		if (pServerStat1->GetServerID() == iServerID)
		{
			pServerStat = pServerStat1;
			break;
		}
	}

	if (!pServerStat)
	{
		pServerStat = new ServerStat(iServerID);
		push_back(pServerStat);
	}

	switch (eStatOperation)
	{
		case soSet:
			pServerStat->SetSuccessArticles(iSuccessArticles);
			pServerStat->SetFailedArticles(iFailedArticles);
			break;

		case soAdd:
			pServerStat->SetSuccessArticles(pServerStat->GetSuccessArticles() + iSuccessArticles);
			pServerStat->SetFailedArticles(pServerStat->GetFailedArticles() + iFailedArticles);
			break;

		case soSubtract:
			pServerStat->SetSuccessArticles(pServerStat->GetSuccessArticles() - iSuccessArticles);
			pServerStat->SetFailedArticles(pServerStat->GetFailedArticles() - iFailedArticles);
			break;
	}
}

void ServerStatList::ListOp(ServerStatList* pServerStats, EStatOperation eStatOperation)
{
	for (iterator it = pServerStats->begin(); it != pServerStats->end(); it++)
	{
		ServerStat* pServerStat = *it;
		StatOp(pServerStat->GetServerID(), pServerStat->GetSuccessArticles(), pServerStat->GetFailedArticles(), eStatOperation);
	}
}


NZBInfo::NZBInfo() : m_FileList(true)
{
	debug("Creating NZBInfo");

	m_eKind = nkNzb;
	m_szURL = strdup("");
	m_szFilename = strdup("");
	m_szDestDir = strdup("");
	m_szFinalDir = strdup("");
	m_szCategory = strdup("");
	m_szName = NULL;
	m_iFileCount = 0;
	m_iParkedFileCount = 0;
	m_lSize = 0;
	m_lSuccessSize = 0;
	m_lFailedSize = 0;
	m_lCurrentSuccessSize = 0;
	m_lCurrentFailedSize = 0;
	m_lParSize = 0;
	m_lParSuccessSize = 0;
	m_lParFailedSize = 0;
	m_lParCurrentSuccessSize = 0;
	m_lParCurrentFailedSize = 0;
	m_iTotalArticles = 0;
	m_iSuccessArticles = 0;
	m_iFailedArticles = 0;
	m_iCurrentSuccessArticles = 0;
	m_iCurrentFailedArticles = 0;
	m_eRenameStatus = rsNone;
	m_eParStatus = psNone;
	m_eUnpackStatus = usNone;
	m_eCleanupStatus = csNone;
	m_eMoveStatus = msNone;
	m_eDeleteStatus = dsNone;
	m_eMarkStatus = ksNone;
	m_eUrlStatus = lsNone;
	m_bAddUrlPaused = false;
	m_bDeleting = false;
	m_bDeletePaused = false;
	m_bManyDupeFiles = false;
	m_bAvoidHistory = false;
	m_bHealthPaused = false;
	m_bParCleanup = false;
	m_bCleanupDisk = false;
	m_bUnpackCleanedUpDisk = false;
	m_szQueuedFilename = strdup("");
	m_szDupeKey = strdup("");
	m_iDupeScore = 0;
	m_eDupeMode = dmScore;
	m_iFullContentHash = 0;
	m_iFilteredContentHash = 0;
	m_iPausedFileCount = 0;
	m_lRemainingSize = 0;
	m_lPausedSize = 0;
	m_iRemainingParCount = 0;
	m_tMinTime = 0;
	m_tMaxTime = 0;
	m_iPriority = 0;
	m_iActiveDownloads = 0;
	m_Messages.clear();
	m_pPostInfo = NULL;
	m_iIDMessageGen = 0;
	m_iID = ++m_iIDGen;
	m_lDownloadedSize = 0;
	m_iDownloadSec = 0;
	m_iPostTotalSec = 0;
	m_iParSec = 0;
	m_iRepairSec = 0;
	m_iUnpackSec = 0;
	m_tDownloadStartTime = 0;
}

NZBInfo::~NZBInfo()
{
	debug("Destroying NZBInfo");

	free(m_szURL);
	free(m_szFilename);
	free(m_szDestDir);
	free(m_szFinalDir);
	free(m_szCategory);
	free(m_szName);
	free(m_szQueuedFilename);
	free(m_szDupeKey);
	delete m_pPostInfo;

	ClearCompletedFiles();

	for (Messages::iterator it = m_Messages.begin(); it != m_Messages.end(); it++)
	{
		delete *it;
	}
	m_Messages.clear();

	m_FileList.Clear();
}

void NZBInfo::SetID(int iID)
{
	m_iID = iID;
	if (m_iIDMax < m_iID)
	{
		m_iIDMax = m_iID;
	}
}

void NZBInfo::ResetGenID(bool bMax)
{
	if (bMax)
	{
		m_iIDGen = m_iIDMax;
	}
	else
	{
		m_iIDGen = 0;
		m_iIDMax = 0;
	}
}

int NZBInfo::GenerateID()
{
	return ++m_iIDGen;
}

void NZBInfo::ClearCompletedFiles()
{
	for (CompletedFiles::iterator it = m_completedFiles.begin(); it != m_completedFiles.end(); it++)
	{
		delete *it;
	}
	m_completedFiles.clear();
}

void NZBInfo::SetDestDir(const char* szDestDir)
{
	free(m_szDestDir);
	m_szDestDir = strdup(szDestDir);
}

void NZBInfo::SetFinalDir(const char* szFinalDir)
{
	free(m_szFinalDir);
	m_szFinalDir = strdup(szFinalDir);
}

void NZBInfo::SetURL(const char* szURL)
{
	free(m_szURL);
	m_szURL = strdup(szURL);

	if (!m_szName)
	{
		char szNZBNicename[1024];
		MakeNiceUrlName(szURL, m_szFilename, szNZBNicename, sizeof(szNZBNicename));
		szNZBNicename[1024-1] = '\0';
#ifdef WIN32
		WebUtil::AnsiToUtf8(szNZBNicename, 1024);
#endif
		SetName(szNZBNicename);
	}
}

void NZBInfo::SetFilename(const char* szFilename)
{
	bool bHadFilename = !Util::EmptyStr(m_szFilename);

	free(m_szFilename);
	m_szFilename = strdup(szFilename);

	if ((!m_szName || !bHadFilename) && !Util::EmptyStr(szFilename))
	{
		char szNZBNicename[1024];
		MakeNiceNZBName(m_szFilename, szNZBNicename, sizeof(szNZBNicename), true);
		szNZBNicename[1024-1] = '\0';
#ifdef WIN32
		WebUtil::AnsiToUtf8(szNZBNicename, 1024);
#endif
		SetName(szNZBNicename);
	}
}

void NZBInfo::SetName(const char* szName)
{
	free(m_szName);
	m_szName = szName ? strdup(szName) : NULL;
}

void NZBInfo::SetCategory(const char* szCategory)
{
	free(m_szCategory);
	m_szCategory = strdup(szCategory);
}

void NZBInfo::SetQueuedFilename(const char * szQueuedFilename)
{
	free(m_szQueuedFilename);
	m_szQueuedFilename = strdup(szQueuedFilename);
}

void NZBInfo::SetDupeKey(const char* szDupeKey)
{
	free(m_szDupeKey);
	m_szDupeKey = strdup(szDupeKey ? szDupeKey : "");
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

void NZBInfo::MakeNiceUrlName(const char* szURL, const char* szNZBFilename, char* szBuffer, int iSize)
{
	URL url(szURL);

	if (!Util::EmptyStr(szNZBFilename))
	{
		char szNZBNicename[1024];
		MakeNiceNZBName(szNZBFilename, szNZBNicename, sizeof(szNZBNicename), true);
		snprintf(szBuffer, iSize, "%s @ %s", szNZBNicename, url.GetHost());
	}
	else if (url.IsValid())
	{
		snprintf(szBuffer, iSize, "%s%s", url.GetHost(), url.GetResource());
	}
	else
	{
		snprintf(szBuffer, iSize, "%s", szURL);
	}

	szBuffer[iSize-1] = '\0';
}

void NZBInfo::BuildDestDirName()
{
	char szDestDir[1024];

	if (Util::EmptyStr(g_pOptions->GetInterDir()))
	{
		BuildFinalDirName(szDestDir, 1024);
	}
	else
	{
		snprintf(szDestDir, 1024, "%s%s.#%i", g_pOptions->GetInterDir(), GetName(), GetID());
		szDestDir[1024-1] = '\0';
	}

#ifdef WIN32
	WebUtil::Utf8ToAnsi(szDestDir, 1024);
#endif

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

#ifdef WIN32
	WebUtil::Utf8ToAnsi(szFinalDirBuf, iBufSize);
#endif
}

int NZBInfo::CalcHealth()
{
	if (m_lCurrentFailedSize == 0 || m_lSize == m_lParSize)
	{
		return 1000;
	}

	int iHealth = (int)(Util::Int64ToFloat(m_lSize - m_lParSize -
		(m_lCurrentFailedSize - m_lParCurrentFailedSize)) * 1000.0 /
		Util::Int64ToFloat(m_lSize - m_lParSize));

	if (iHealth == 1000 && m_lCurrentFailedSize - m_lParCurrentFailedSize > 0)
	{
		iHealth = 999;
	}

	return iHealth;
}

int NZBInfo::CalcCriticalHealth(bool bAllowEstimation)
{
	if (m_lSize == 0)
	{
		return 1000;
	}

	long long lGoodParSize = m_lParSize - m_lParCurrentFailedSize;
	int iCriticalHealth = (int)(Util::Int64ToFloat(m_lSize - lGoodParSize*2) * 1000.0 /
		Util::Int64ToFloat(m_lSize - lGoodParSize));

	if (lGoodParSize*2 > m_lSize)
	{
		iCriticalHealth = 0;
	}
	else if (iCriticalHealth == 1000 && m_lParSize > 0)
	{
		iCriticalHealth = 999;
	}

	if (iCriticalHealth == 1000 && bAllowEstimation)
	{
		// using empirical critical health 85%, to avoid false alarms for downloads with renamed par-files
		iCriticalHealth = 850;
	}		

	return iCriticalHealth;
}

void NZBInfo::UpdateMinMaxTime()
{
	m_tMinTime = 0;
	m_tMaxTime = 0;

	bool bFirst = true;
	for (FileList::iterator it = m_FileList.begin(); it != m_FileList.end(); it++)
    {
        FileInfo* pFileInfo = *it;
		if (bFirst)
		{
			m_tMinTime = pFileInfo->GetTime();
			m_tMaxTime = pFileInfo->GetTime();
			bFirst = false;
		}
		if (pFileInfo->GetTime() > 0)
		{
			if (pFileInfo->GetTime() < m_tMinTime)
			{
				m_tMinTime = pFileInfo->GetTime();
			}
			if (pFileInfo->GetTime() > m_tMaxTime)
			{
				m_tMaxTime = pFileInfo->GetTime();
			}
		}
	}
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

void NZBInfo::CopyFileList(NZBInfo* pSrcNZBInfo)
{
	m_FileList.Clear();

	for (FileList::iterator it = pSrcNZBInfo->GetFileList()->begin(); it != pSrcNZBInfo->GetFileList()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		pFileInfo->SetNZBInfo(this);
		m_FileList.push_back(pFileInfo);
	}

	pSrcNZBInfo->GetFileList()->clear(); // only remove references

	SetFullContentHash(pSrcNZBInfo->GetFullContentHash());
	SetFilteredContentHash(pSrcNZBInfo->GetFilteredContentHash());

	SetFileCount(pSrcNZBInfo->GetFileCount());
	SetPausedFileCount(pSrcNZBInfo->GetPausedFileCount());
	SetRemainingParCount(pSrcNZBInfo->GetRemainingParCount());

	SetSize(pSrcNZBInfo->GetSize());
	SetRemainingSize(pSrcNZBInfo->GetRemainingSize());
	SetPausedSize(pSrcNZBInfo->GetPausedSize());
	SetSuccessSize(pSrcNZBInfo->GetSuccessSize());
	SetCurrentSuccessSize(pSrcNZBInfo->GetCurrentSuccessSize());
	SetFailedSize(pSrcNZBInfo->GetFailedSize());
	SetCurrentFailedSize(pSrcNZBInfo->GetCurrentFailedSize());

	SetParSize(pSrcNZBInfo->GetParSize());
	SetParSuccessSize(pSrcNZBInfo->GetParSuccessSize());
	SetParCurrentSuccessSize(pSrcNZBInfo->GetParCurrentSuccessSize());
	SetParFailedSize(pSrcNZBInfo->GetParFailedSize());
	SetParCurrentFailedSize(pSrcNZBInfo->GetParCurrentFailedSize());

	SetSuccessArticles(pSrcNZBInfo->GetSuccessArticles());
	SetFailedArticles(pSrcNZBInfo->GetFailedArticles());
	SetCurrentSuccessArticles(pSrcNZBInfo->GetSuccessArticles());
	SetCurrentFailedArticles(pSrcNZBInfo->GetFailedArticles());

	SetMinTime(pSrcNZBInfo->GetMinTime());
	SetMaxTime(pSrcNZBInfo->GetMaxTime());
}

void NZBInfo::EnterPostProcess()
{
	m_pPostInfo = new PostInfo();
	m_pPostInfo->SetNZBInfo(this);
}

void NZBInfo::LeavePostProcess()
{
	delete m_pPostInfo;
	m_pPostInfo = NULL;
}

void NZBInfo::SetActiveDownloads(int iActiveDownloads)
{
	if (((m_iActiveDownloads == 0 && iActiveDownloads > 0) ||
		 (m_iActiveDownloads > 0 && iActiveDownloads == 0)) &&
		m_eKind == NZBInfo::nkNzb)
	{
		if (iActiveDownloads > 0)
		{
			m_tDownloadStartTime = time(NULL);
		}
		else
		{
			m_iDownloadSec += time(NULL) - m_tDownloadStartTime;
			m_tDownloadStartTime = 0;
		}
	}
	m_iActiveDownloads = iActiveDownloads;
}

bool NZBInfo::IsDupeSuccess()
{
	bool bFailure =
		m_eDeleteStatus != NZBInfo::dsNone ||
		m_eMarkStatus == NZBInfo::ksBad ||
		m_eParStatus == NZBInfo::psFailure ||
		m_eUnpackStatus == NZBInfo::usFailure ||
		m_eUnpackStatus == NZBInfo::usPassword ||
		(m_eParStatus == NZBInfo::psSkipped &&
		 m_eUnpackStatus == NZBInfo::usSkipped &&
		 CalcHealth() < CalcCriticalHealth(true));
	return !bFailure;
}

const char* NZBInfo::MakeTextStatus(bool bIgnoreScriptStatus)
{
	const char* szStatus = "FAILURE/INTERNAL_ERROR";

	if (m_eKind == NZBInfo::nkNzb)
	{
		int iHealth = CalcHealth();
		int iCriticalHealth = CalcCriticalHealth(false);
		ScriptStatus::EStatus eScriptStatus = bIgnoreScriptStatus ? ScriptStatus::srSuccess : m_scriptStatuses.CalcTotalStatus();

		if (m_eMarkStatus == NZBInfo::ksBad)
		{
			szStatus = "FAILURE/BAD";
		}
		else if (m_eMarkStatus == NZBInfo::ksGood)
		{
			szStatus = "SUCCESS/GOOD";
		}
		else if (m_eDeleteStatus == NZBInfo::dsHealth)
		{
			szStatus = "FAILURE/HEALTH";
		}
		else if (m_eDeleteStatus == NZBInfo::dsManual)
		{
			szStatus = "DELETED/MANUAL";
		}
		else if (m_eDeleteStatus == NZBInfo::dsDupe)
		{
			szStatus = "DELETED/DUPE";
		}
		else if (m_eParStatus == NZBInfo::psFailure)
		{
			szStatus = "FAILURE/PAR";
		}
		else if (m_eUnpackStatus == NZBInfo::usFailure)
		{
			szStatus = "FAILURE/UNPACK";
		}
		else if (m_eMoveStatus == NZBInfo::msFailure)
		{
			szStatus = "FAILURE/MOVE";
		}
		else if (m_eParStatus == NZBInfo::psManual)
		{
			szStatus = "WARNING/DAMAGED";
		}
		else if (m_eParStatus == NZBInfo::psRepairPossible)
		{
			szStatus = "WARNING/REPAIRABLE";
		}
		else if ((m_eParStatus == NZBInfo::psNone || m_eParStatus == NZBInfo::psSkipped) &&
				 (m_eUnpackStatus == NZBInfo::usNone || m_eUnpackStatus == NZBInfo::usSkipped) &&
				 iHealth < iCriticalHealth)
		{
			szStatus = "FAILURE/HEALTH";
		}
		else if ((m_eParStatus == NZBInfo::psNone || m_eParStatus == NZBInfo::psSkipped) &&
				 (m_eUnpackStatus == NZBInfo::usNone || m_eUnpackStatus == NZBInfo::usSkipped) &&
				 iHealth < 1000 && iHealth >= iCriticalHealth)
		{
			szStatus = "WARNING/HEALTH";
		}
		else if ((m_eParStatus == NZBInfo::psNone || m_eParStatus == NZBInfo::psSkipped) &&
				 (m_eUnpackStatus == NZBInfo::usNone || m_eUnpackStatus == NZBInfo::usSkipped) &&
				 eScriptStatus != ScriptStatus::srFailure && iHealth == 1000)
		{
			szStatus = "SUCCESS/HEALTH";
		}
		else if (m_eUnpackStatus == NZBInfo::usSpace)
		{
			szStatus = "WARNING/SPACE";
		}
		else if (m_eUnpackStatus == NZBInfo::usPassword)
		{
			szStatus = "WARNING/PASSWORD";
		}
		else if ((m_eUnpackStatus == NZBInfo::usSuccess ||
				  ((m_eUnpackStatus == NZBInfo::usNone || m_eUnpackStatus == NZBInfo::usSkipped) &&
				   m_eParStatus == NZBInfo::psSuccess)) &&
				 eScriptStatus == ScriptStatus::srSuccess)
		{
			szStatus = "SUCCESS/ALL";
		}
		else if (m_eUnpackStatus == NZBInfo::usSuccess && eScriptStatus == ScriptStatus::srNone)
		{
			szStatus = "SUCCESS/UNPACK";
		}
		else if (m_eParStatus == NZBInfo::psSuccess && eScriptStatus == ScriptStatus::srNone)
		{
			szStatus = "SUCCESS/PAR";
		}
		else if (eScriptStatus == ScriptStatus::srFailure)
		{
			szStatus = "WARNING/SCRIPT";
		}
	}
	else if (m_eKind == NZBInfo::nkUrl)
	{
		if (m_eDeleteStatus == NZBInfo::dsManual)
		{
			szStatus = "DELETED/MANUAL";
		}
		else if (m_eDeleteStatus == NZBInfo::dsDupe)
		{
			szStatus = "DELETED/DUPE";
		}
		else
		{
			const char* szUrlStatusName[] = { "FAILURE/INTERNAL_ERROR", "FAILURE/INTERNAL_ERROR", "FAILURE/INTERNAL_ERROR",
				"FAILURE/FETCH", "FAILURE/INTERNAL_ERROR", "WARNING/SKIPPED", "FAILURE/SCAN" };
			szStatus = szUrlStatusName[m_eUrlStatus];
		}
	}

	return szStatus;
}


NZBList::~NZBList()
{
	if (m_bOwnObjects)
	{
		Clear();
	}
}

void NZBList::Clear()
{
	for (iterator it = begin(); it != end(); it++)
	{
		delete *it;
	}
	clear();
}

void NZBList::Add(NZBInfo* pNZBInfo, bool bAddTop)
{
	if (bAddTop)
	{
		push_front(pNZBInfo);
	}
	else
	{
		push_back(pNZBInfo);
	}
}

void NZBList::Remove(NZBInfo* pNZBInfo)
{
	iterator it = std::find(begin(), end(), pNZBInfo);
	if (it != end())
	{
		erase(it);
	}
}


ArticleInfo::ArticleInfo()
{
	//debug("Creating ArticleInfo");
	m_szMessageID = NULL;
	m_iSize = 0;
	m_pSegmentContent = NULL;
	m_iSegmentOffset = 0;
	m_iSegmentSize = 0;
	m_eStatus = aiUndefined;
	m_szResultFilename = NULL;
	m_lCrc = 0;
}

ArticleInfo::~ ArticleInfo()
{
	//debug("Destroying ArticleInfo");
	DiscardSegment();
	free(m_szMessageID);
	free(m_szResultFilename);
}

void ArticleInfo::SetMessageID(const char * szMessageID)
{
	free(m_szMessageID);
	m_szMessageID = strdup(szMessageID);
}

void ArticleInfo::SetResultFilename(const char * v)
{
	free(m_szResultFilename);
	m_szResultFilename = strdup(v);
}

void ArticleInfo::AttachSegment(char* pContent, long long iOffset, int iSize)
{
	DiscardSegment();
	m_pSegmentContent = pContent;
	m_iSegmentOffset = iOffset;
	m_iSegmentSize = iSize;
}

void ArticleInfo::DiscardSegment()
{
	if (m_pSegmentContent)
	{
		free(m_pSegmentContent);
		m_pSegmentContent = NULL;
		g_pArticleCache->Free(m_iSegmentSize);
	}
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
	m_lMissedSize = 0;
	m_lSuccessSize = 0;
	m_lFailedSize = 0;
	m_iTotalArticles = 0;
	m_iMissedArticles = 0;
	m_iFailedArticles = 0;
	m_iSuccessArticles = 0;
	m_tTime = 0;
	m_bPaused = false;
	m_bDeleted = false;
	m_iCompletedArticles = 0;
	m_bParFile = false;
	m_bOutputInitialized = false;
	m_pNZBInfo = NULL;
	m_bExtraPriority = false;
	m_iActiveDownloads = 0;
	m_bAutoDeleted = false;
	m_iCachedArticles = 0;
	m_iID = ++m_iIDGen;
}

FileInfo::~ FileInfo()
{
	debug("Destroying FileInfo");

	free(m_szSubject);
	free(m_szFilename);
	free(m_szOutputFilename);
	delete m_pMutexOutputFile;

	for (Groups::iterator it = m_Groups.begin(); it != m_Groups.end() ;it++)
	{
		free(*it);
	}
	m_Groups.clear();

	ClearArticles();
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
	if (m_iIDMax < m_iID)
	{
		m_iIDMax = m_iID;
	}
}

void FileInfo::ResetGenID(bool bMax)
{
	if (bMax)
	{
		m_iIDGen = m_iIDMax;
	}
	else
	{
		m_iIDGen = 0;
		m_iIDMax = 0;
	}
}

void FileInfo::SetPaused(bool bPaused)
{
	if (m_bPaused != bPaused && m_pNZBInfo)
	{
		m_pNZBInfo->SetPausedFileCount(m_pNZBInfo->GetPausedFileCount() + (bPaused ? 1 : -1));
		m_pNZBInfo->SetPausedSize(m_pNZBInfo->GetPausedSize() + (bPaused ? m_lRemainingSize : - m_lRemainingSize));
	}
	m_bPaused = bPaused;
}

void FileInfo::SetSubject(const char* szSubject)
{
	m_szSubject = strdup(szSubject);
}

void FileInfo::SetFilename(const char* szFilename)
{
	free(m_szFilename);
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
	free(m_szOutputFilename);
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


FileList::~FileList()
{
	if (m_bOwnObjects)
	{
		Clear();
	}
}

void FileList::Clear()
{
	for (iterator it = begin(); it != end(); it++)
	{
		delete *it;
	}
	clear();
}

void FileList::Remove(FileInfo* pFileInfo)
{
	erase(std::find(begin(), end(), pFileInfo));
}

CompletedFile::CompletedFile(const char* szFileName, EStatus eStatus, unsigned long lCrc)
{
	m_szFileName = strdup(szFileName);
	m_eStatus = eStatus;
	m_lCrc = lCrc;
}

void CompletedFile::SetFileName(const char* szFileName)
{
	free(m_szFileName);
	m_szFileName = strdup(szFileName);
}

CompletedFile::~CompletedFile()
{
	free(m_szFileName);
}

PostInfo::PostInfo()
{
	debug("Creating PostInfo");

	m_pNZBInfo = NULL;
	m_bWorking = false;
	m_bDeleted = false;
	m_bRequestParCheck = false;
	m_szProgressLabel = strdup("");
	m_iFileProgress = 0;
	m_iStageProgress = 0;
	m_tStartTime = 0;
	m_tStageTime = 0;
	m_eStage = ptQueued;
	m_pPostThread = NULL;
	m_iIDMessageGen = 0;
}

PostInfo::~ PostInfo()
{
	debug("Destroying PostInfo");

	free(m_szProgressLabel);

	for (Messages::iterator it = m_Messages.begin(); it != m_Messages.end(); it++)
	{
		delete *it;
	}

	for (ParredFiles::iterator it = m_ParredFiles.begin(); it != m_ParredFiles.end(); it++)
	{
		free(*it);
	}
}

void PostInfo::SetProgressLabel(const char* szProgressLabel)
{
	free(m_szProgressLabel);
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


DupInfo::DupInfo()
{
	m_iID = 0;
	m_szName = NULL;
	m_szDupeKey = NULL;
	m_iDupeScore = 0;
	m_eDupeMode = dmScore;
	m_lSize = 0;
	m_iFullContentHash = 0;
	m_iFilteredContentHash = 0;
	m_eStatus = dsUndefined;
}

DupInfo::~DupInfo()
{
	free(m_szName);
	free(m_szDupeKey);
}

void DupInfo::SetID(int iID)
{
	m_iID = iID;
	if (NZBInfo::m_iIDMax < m_iID)
	{
		NZBInfo::m_iIDMax = m_iID;
	}
}

void DupInfo::SetName(const char* szName)
{
	free(m_szName);
	m_szName = strdup(szName);
}

void DupInfo::SetDupeKey(const char* szDupeKey)
{
	free(m_szDupeKey);
	m_szDupeKey = strdup(szDupeKey);
}


HistoryInfo::HistoryInfo(NZBInfo* pNZBInfo)
{
	m_eKind = pNZBInfo->GetKind() == NZBInfo::nkNzb ? hkNzb : hkUrl;
	m_pInfo = pNZBInfo;
	m_tTime = 0;
}

HistoryInfo::HistoryInfo(DupInfo* pDupInfo)
{
	m_eKind = hkDup;
	m_pInfo = pDupInfo;
	m_tTime = 0;
}

HistoryInfo::~HistoryInfo()
{
	if ((m_eKind == hkNzb || m_eKind == hkUrl) && m_pInfo)
	{
		delete (NZBInfo*)m_pInfo;
	}
	else if (m_eKind == hkDup && m_pInfo)
	{
		delete (DupInfo*)m_pInfo;
	}
}

int HistoryInfo::GetID()
{
	if ((m_eKind == hkNzb || m_eKind == hkUrl))
	{
		return ((NZBInfo*)m_pInfo)->GetID();
	}
	else // if (m_eKind == hkDup)
	{
		return ((DupInfo*)m_pInfo)->GetID();
	}
}

void HistoryInfo::GetName(char* szBuffer, int iSize)
{
	if (m_eKind == hkNzb || m_eKind == hkUrl)
	{
		strncpy(szBuffer, GetNZBInfo()->GetName(), iSize);
		szBuffer[iSize-1] = '\0';
	}
	else if (m_eKind == hkDup)
	{
		strncpy(szBuffer, GetDupInfo()->GetName(), iSize);
		szBuffer[iSize-1] = '\0';
	}
	else
	{
		strncpy(szBuffer, "<unknown>", iSize);
	}
}


DownloadQueue* DownloadQueue::Lock()
{
	g_pDownloadQueue->m_LockMutex.Lock();
	return g_pDownloadQueue;
}

void DownloadQueue::Unlock()
{
	g_pDownloadQueue->m_LockMutex.Unlock();
}

void DownloadQueue::CalcRemainingSize(long long* pRemaining, long long* pRemainingForced)
{
	long long lRemainingSize = 0;
	long long lRemainingForced = 0;

	for (NZBList::iterator it = m_Queue.begin(); it != m_Queue.end(); it++)
	{
		NZBInfo* pNZBInfo = *it;
		for (FileList::iterator it2 = pNZBInfo->GetFileList()->begin(); it2 != pNZBInfo->GetFileList()->end(); it2++)
		{
			FileInfo* pFileInfo = *it2;
			if (!pFileInfo->GetPaused() && !pFileInfo->GetDeleted())
			{
				lRemainingSize += pFileInfo->GetRemainingSize();
				if (pNZBInfo->GetForcePriority())
				{
					lRemainingForced += pFileInfo->GetRemainingSize();
				}
			}
		}
	}

	*pRemaining = lRemainingSize;

	if (pRemainingForced)
	{
		*pRemainingForced = lRemainingForced;
	}
}
