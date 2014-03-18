/*
 *  This file is part of nzbget
 *
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
#include <ctype.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <stdio.h>

#include "nzbget.h"
#include "PostScript.h"
#include "Log.h"
#include "Util.h"
#include "Options.h"

extern Options* g_pOptions;

static const int POSTPROCESS_PARCHECK = 92;
static const int POSTPROCESS_SUCCESS = 93;
static const int POSTPROCESS_ERROR = 94;
static const int POSTPROCESS_NONE = 95;

void PostScriptController::StartJob(PostInfo* pPostInfo)
{
	PostScriptController* pScriptController = new PostScriptController();
	pScriptController->m_pPostInfo = pPostInfo;
	pScriptController->SetWorkingDir(g_pOptions->GetDestDir());
	pScriptController->SetAutoDestroy(false);
	pScriptController->m_iPrefixLen = 0;

	pPostInfo->SetPostThread(pScriptController);

	pScriptController->Start();
}

void PostScriptController::Run()
{
	FileList activeList;

	// the locking is needed for accessing the members of NZBInfo
	DownloadQueue::Lock();
	for (NZBParameterList::iterator it = m_pPostInfo->GetNZBInfo()->GetParameters()->begin(); it != m_pPostInfo->GetNZBInfo()->GetParameters()->end(); it++)
	{
		NZBParameter* pParameter = *it;
		const char* szVarname = pParameter->GetName();
		if (strlen(szVarname) > 0 && szVarname[0] != '*' && szVarname[strlen(szVarname)-1] == ':' &&
			(!strcasecmp(pParameter->GetValue(), "yes") || !strcasecmp(pParameter->GetValue(), "on") || !strcasecmp(pParameter->GetValue(), "1")))
		{
			char* szScriptName = strdup(szVarname);
			szScriptName[strlen(szScriptName)-1] = '\0'; // remove trailing ':'
			activeList.push_back(szScriptName);
		}
	}
	m_pPostInfo->GetNZBInfo()->GetScriptStatuses()->Clear();
	DownloadQueue::Unlock();

	Options::ScriptList scriptList;
	g_pOptions->LoadScriptList(&scriptList);

	for (Options::ScriptList::iterator it = scriptList.begin(); it != scriptList.end(); it++)
	{
		Options::Script* pScript = *it;
		for (FileList::iterator it2 = activeList.begin(); it2 != activeList.end(); it2++)
		{
			char* szActiveName = *it2;
			// if any script has requested par-check, do not execute other scripts
			if (Util::SameFilename(pScript->GetName(), szActiveName) && !m_pPostInfo->GetRequestParCheck())
			{
				ExecuteScript(pScript->GetName(), pScript->GetDisplayName(), pScript->GetLocation());
			}
		}
	}
	
	for (FileList::iterator it = activeList.begin(); it != activeList.end(); it++)
	{
		free(*it);
	}

	m_pPostInfo->SetStage(PostInfo::ptFinished);
	m_pPostInfo->SetWorking(false);
}

void PostScriptController::ExecuteScript(const char* szScriptName, const char* szDisplayName, const char* szLocation)
{
	PrintMessage(Message::mkInfo, "Executing post-process-script %s for %s", szScriptName, m_pPostInfo->GetNZBInfo()->GetName());

	SetScript(szLocation);
	SetArgs(NULL, false);

	char szInfoName[1024];
	snprintf(szInfoName, 1024, "post-process-script %s for %s", szScriptName, m_pPostInfo->GetNZBInfo()->GetName());
	szInfoName[1024-1] = '\0';
	SetInfoName(szInfoName);

	SetLogPrefix(szDisplayName);
	m_iPrefixLen = strlen(szDisplayName) + 2; // 2 = strlen(": ");
	PrepareParams(szScriptName);

	int iExitCode = Execute();

	szInfoName[0] = 'P'; // uppercase

	SetLogPrefix(NULL);
	ScriptStatus::EStatus eStatus = AnalyseExitCode(iExitCode);
	
	// the locking is needed for accessing the members of NZBInfo
	DownloadQueue::Lock();
	m_pPostInfo->GetNZBInfo()->GetScriptStatuses()->Add(szScriptName, eStatus);
	DownloadQueue::Unlock();
}

void PostScriptController::PrepareParams(const char* szScriptName)
{
	// the locking is needed for accessing the members of NZBInfo
	DownloadQueue::Lock();

	// Reset
	ResetEnv();

	SetEnvVar("NZBPP_NZBNAME", m_pPostInfo->GetNZBInfo()->GetName());
	SetEnvVar("NZBPP_DIRECTORY", m_pPostInfo->GetNZBInfo()->GetDestDir());
	SetEnvVar("NZBPP_NZBFILENAME", m_pPostInfo->GetNZBInfo()->GetFilename());
	SetEnvVar("NZBPP_URL", m_pPostInfo->GetNZBInfo()->GetURL());
	SetEnvVar("NZBPP_FINALDIR", m_pPostInfo->GetNZBInfo()->GetFinalDir());
	SetEnvVar("NZBPP_CATEGORY", m_pPostInfo->GetNZBInfo()->GetCategory());
	SetIntEnvVar("NZBPP_HEALTH", m_pPostInfo->GetNZBInfo()->CalcHealth());
	SetIntEnvVar("NZBPP_CRITICALHEALTH", m_pPostInfo->GetNZBInfo()->CalcCriticalHealth(false));

	int iParStatus[] = { 0, 0, 1, 2, 3, 4 };
	SetIntEnvVar("NZBPP_PARSTATUS", iParStatus[m_pPostInfo->GetNZBInfo()->GetParStatus()]);

	int iUnpackStatus[] = { 0, 0, 1, 2, 3, 4 };
	SetIntEnvVar("NZBPP_UNPACKSTATUS", iUnpackStatus[m_pPostInfo->GetNZBInfo()->GetUnpackStatus()]);

	SetIntEnvVar("NZBPP_NZBID", m_pPostInfo->GetNZBInfo()->GetID());
	SetIntEnvVar("NZBPP_HEALTHDELETED", (int)m_pPostInfo->GetNZBInfo()->GetDeleteStatus() == NZBInfo::dsHealth);
	SetIntEnvVar("NZBPP_TOTALARTICLES", (int)m_pPostInfo->GetNZBInfo()->GetTotalArticles());
	SetIntEnvVar("NZBPP_SUCCESSARTICLES", (int)m_pPostInfo->GetNZBInfo()->GetSuccessArticles());
	SetIntEnvVar("NZBPP_FAILEDARTICLES", (int)m_pPostInfo->GetNZBInfo()->GetFailedArticles());

	for (ServerStatList::iterator it = m_pPostInfo->GetNZBInfo()->GetServerStats()->begin(); it != m_pPostInfo->GetNZBInfo()->GetServerStats()->end(); it++)
	{
		ServerStat* pServerStat = *it;

		char szName[50];

		snprintf(szName, 50, "NZBPP_SERVER%i_SUCCESSARTICLES", pServerStat->GetServerID());
		szName[50-1] = '\0';
		SetIntEnvVar(szName, pServerStat->GetSuccessArticles());

		snprintf(szName, 50, "NZBPP_SERVER%i_FAILEDARTICLES", pServerStat->GetServerID());
		szName[50-1] = '\0';
		SetIntEnvVar(szName, pServerStat->GetFailedArticles());
	}

	PrepareEnvParameters(m_pPostInfo->GetNZBInfo()->GetParameters(), NULL);

	char szParamPrefix[1024];
	snprintf(szParamPrefix, 1024, "%s:", szScriptName);
	szParamPrefix[1024-1] = '\0';
	PrepareEnvParameters(m_pPostInfo->GetNZBInfo()->GetParameters(), szParamPrefix);
	PrepareEnvOptions(szParamPrefix);
	
	DownloadQueue::Unlock();
}

ScriptStatus::EStatus PostScriptController::AnalyseExitCode(int iExitCode)
{
	// The ScriptStatus is accumulated for all scripts:
	// If any script has failed the status is "failure", etc.

	switch (iExitCode)
	{
		case POSTPROCESS_SUCCESS:
			PrintMessage(Message::mkInfo, "%s successful", GetInfoName());
			return ScriptStatus::srSuccess;

		case POSTPROCESS_ERROR:
		case -1: // Execute() returns -1 if the process could not be started (file not found or other problem)
			PrintMessage(Message::mkError, "%s failed", GetInfoName());
			return ScriptStatus::srFailure;

		case POSTPROCESS_NONE:
			PrintMessage(Message::mkInfo, "%s skipped", GetInfoName());
			return ScriptStatus::srNone;

#ifndef DISABLE_PARCHECK
		case POSTPROCESS_PARCHECK:
			if (m_pPostInfo->GetNZBInfo()->GetParStatus() > NZBInfo::psSkipped)
			{
				PrintMessage(Message::mkError, "%s requested par-check/repair, but the collection was already checked", GetInfoName());
				return ScriptStatus::srFailure;
			}
			else
			{
				PrintMessage(Message::mkInfo, "%s requested par-check/repair", GetInfoName());
				m_pPostInfo->SetRequestParCheck(true);
				return ScriptStatus::srSuccess;
			}
			break;
#endif

		default:
			PrintMessage(Message::mkError, "%s failed (terminated with unknown status)", GetInfoName());
			return ScriptStatus::srFailure;
	}
}

void PostScriptController::AddMessage(Message::EKind eKind, const char* szText)
{
	const char* szMsgText = szText + m_iPrefixLen;

	if (!strncmp(szMsgText, "[NZB] ", 6))
	{
		debug("Command %s detected", szMsgText + 6);
		if (!strncmp(szMsgText + 6, "FINALDIR=", 9))
		{
			DownloadQueue::Lock();
			m_pPostInfo->GetNZBInfo()->SetFinalDir(szMsgText + 6 + 9);
			DownloadQueue::Unlock();
		}
		else if (!strncmp(szMsgText + 6, "NZBPR_", 6))
		{
			char* szParam = strdup(szMsgText + 6 + 6);
			char* szValue = strchr(szParam, '=');
			if (szValue)
			{
				*szValue = '\0';
				DownloadQueue::Lock();
				m_pPostInfo->GetNZBInfo()->GetParameters()->SetParameter(szParam, szValue + 1);
				DownloadQueue::Unlock();
			}
			else
			{
				error("Invalid command \"%s\" received from %s", szMsgText, GetInfoName());
			}
			free(szParam);
		}
		else
		{
			error("Invalid command \"%s\" received from %s", szMsgText, GetInfoName());
		}
	}
	else if (!strncmp(szMsgText, "[HISTORY] ", 10))
	{
		m_pPostInfo->GetNZBInfo()->AppendMessage(eKind, 0, szMsgText);
	}
	else
	{
		ScriptController::AddMessage(eKind, szText);
		m_pPostInfo->AppendMessage(eKind, szText);
	}

	if (g_pOptions->GetPausePostProcess())
	{
		time_t tStageTime = m_pPostInfo->GetStageTime();
		time_t tStartTime = m_pPostInfo->GetStartTime();
		time_t tWaitTime = time(NULL);

		// wait until Post-processor is unpaused
		while (g_pOptions->GetPausePostProcess() && !IsStopped())
		{
			usleep(100 * 1000);

			// update time stamps

			time_t tDelta = time(NULL) - tWaitTime;

			if (tStageTime > 0)
			{
				m_pPostInfo->SetStageTime(tStageTime + tDelta);
			}

			if (tStartTime > 0)
			{
				m_pPostInfo->SetStartTime(tStartTime + tDelta);
			}
		}
	}
}

void PostScriptController::Stop()
{
	debug("Stopping post-process-script");
	Thread::Stop();
	Terminate();
}
