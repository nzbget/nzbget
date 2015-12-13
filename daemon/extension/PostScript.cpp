/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#include "nzbget.h"
#include "PostScript.h"
#include "Log.h"
#include "Util.h"
#include "Options.h"

static const int POSTPROCESS_PARCHECK = 92;
static const int POSTPROCESS_SUCCESS = 93;
static const int POSTPROCESS_ERROR = 94;
static const int POSTPROCESS_NONE = 95;

void PostScriptController::StartJob(PostInfo* postInfo)
{
	PostScriptController* scriptController = new PostScriptController();
	scriptController->m_postInfo = postInfo;
	scriptController->SetWorkingDir(g_Options->GetDestDir());
	scriptController->SetAutoDestroy(false);
	scriptController->m_prefixLen = 0;

	postInfo->SetPostThread(scriptController);

	scriptController->Start();
}

void PostScriptController::Run()
{
	CString scriptCommaList;

	// the locking is needed for accessing the members of NZBInfo
	DownloadQueue::Lock();
	for (NzbParameterList::iterator it = m_postInfo->GetNzbInfo()->GetParameters()->begin(); it != m_postInfo->GetNzbInfo()->GetParameters()->end(); it++)
	{
		NzbParameter* parameter = *it;
		const char* varname = parameter->GetName();
		if (strlen(varname) > 0 && varname[0] != '*' && varname[strlen(varname)-1] == ':' &&
			(!strcasecmp(parameter->GetValue(), "yes") || !strcasecmp(parameter->GetValue(), "on") || !strcasecmp(parameter->GetValue(), "1")))
		{
			char* scriptName = strdup(varname);
			scriptName[strlen(scriptName)-1] = '\0'; // remove trailing ':'
			scriptCommaList.Append(scriptName);
			scriptCommaList.Append(",");
			free(scriptName);
		}
	}
	m_postInfo->GetNzbInfo()->GetScriptStatuses()->Clear();
	DownloadQueue::Unlock();

	ExecuteScriptList(scriptCommaList);

	m_postInfo->SetStage(PostInfo::ptFinished);
	m_postInfo->SetWorking(false);
}

void PostScriptController::ExecuteScript(ScriptConfig::Script* script)
{
	// if any script has requested par-check, do not execute other scripts
	if (!script->GetPostScript() || m_postInfo->GetRequestParCheck())
	{
		return;
	}

	PrintMessage(Message::mkInfo, "Executing post-process-script %s for %s", script->GetName(), m_postInfo->GetNzbInfo()->GetName());

	char progressLabel[1024];
	snprintf(progressLabel, 1024, "Executing post-process-script %s", script->GetName());
	progressLabel[1024-1] = '\0';

	DownloadQueue::Lock();
	m_postInfo->SetProgressLabel(progressLabel);
	DownloadQueue::Unlock();

	SetScript(script->GetLocation());
	SetArgs(NULL, false);

	char infoName[1024];
	snprintf(infoName, 1024, "post-process-script %s for %s", script->GetName(), m_postInfo->GetNzbInfo()->GetName());
	infoName[1024-1] = '\0';
	SetInfoName(infoName);

	m_script = script;
	SetLogPrefix(script->GetDisplayName());
	m_prefixLen = strlen(script->GetDisplayName()) + 2; // 2 = strlen(": ");
	PrepareParams(script->GetName());

	int exitCode = Execute();

	infoName[0] = 'P'; // uppercase

	SetLogPrefix(NULL);
	ScriptStatus::EStatus status = AnalyseExitCode(exitCode);

	// the locking is needed for accessing the members of NZBInfo
	DownloadQueue::Lock();
	m_postInfo->GetNzbInfo()->GetScriptStatuses()->Add(script->GetName(), status);
	DownloadQueue::Unlock();
}

void PostScriptController::PrepareParams(const char* scriptName)
{
	// the locking is needed for accessing the members of NZBInfo
	DownloadQueue::Lock();

	ResetEnv();

	SetIntEnvVar("NZBPP_NZBID", m_postInfo->GetNzbInfo()->GetId());
	SetEnvVar("NZBPP_NZBNAME", m_postInfo->GetNzbInfo()->GetName());
	SetEnvVar("NZBPP_DIRECTORY", m_postInfo->GetNzbInfo()->GetDestDir());
	SetEnvVar("NZBPP_NZBFILENAME", m_postInfo->GetNzbInfo()->GetFilename());
	SetEnvVar("NZBPP_URL", m_postInfo->GetNzbInfo()->GetUrl());
	SetEnvVar("NZBPP_FINALDIR", m_postInfo->GetNzbInfo()->GetFinalDir());
	SetEnvVar("NZBPP_CATEGORY", m_postInfo->GetNzbInfo()->GetCategory());
	SetIntEnvVar("NZBPP_HEALTH", m_postInfo->GetNzbInfo()->CalcHealth());
	SetIntEnvVar("NZBPP_CRITICALHEALTH", m_postInfo->GetNzbInfo()->CalcCriticalHealth(false));

	SetEnvVar("NZBPP_DUPEKEY", m_postInfo->GetNzbInfo()->GetDupeKey());
	SetIntEnvVar("NZBPP_DUPESCORE", m_postInfo->GetNzbInfo()->GetDupeScore());

	const char* dupeModeName[] = { "SCORE", "ALL", "FORCE" };
	SetEnvVar("NZBPP_DUPEMODE", dupeModeName[m_postInfo->GetNzbInfo()->GetDupeMode()]);

	char status[256];
	strncpy(status, m_postInfo->GetNzbInfo()->MakeTextStatus(true), sizeof(status));
	status[256-1] = '\0';
	SetEnvVar("NZBPP_STATUS", status);

	char* detail = strchr(status, '/');
	if (detail) *detail = '\0';
	SetEnvVar("NZBPP_TOTALSTATUS", status);

	const char* scriptStatusName[] = { "NONE", "FAILURE", "SUCCESS" };
	SetEnvVar("NZBPP_SCRIPTSTATUS", scriptStatusName[m_postInfo->GetNzbInfo()->GetScriptStatuses()->CalcTotalStatus()]);

	// deprecated
	int parStatusCodes[] = { 0, 0, 1, 2, 3, 4 };
	NzbInfo::EParStatus parStatus = m_postInfo->GetNzbInfo()->GetParStatus();
	// for downloads marked as bad and for deleted downloads pass par status "Failure"
	// for compatibility with older scripts which don't check "NZBPP_TOTALSTATUS"
	if (m_postInfo->GetNzbInfo()->GetDeleteStatus() != NzbInfo::dsNone ||
		m_postInfo->GetNzbInfo()->GetMarkStatus() == NzbInfo::ksBad)
	{
		parStatus = NzbInfo::psFailure;
	}
	SetIntEnvVar("NZBPP_PARSTATUS", parStatusCodes[parStatus]);

	// deprecated
	int unpackStatus[] = { 0, 0, 1, 2, 3, 4 };
	SetIntEnvVar("NZBPP_UNPACKSTATUS", unpackStatus[m_postInfo->GetNzbInfo()->GetUnpackStatus()]);

	// deprecated
	SetIntEnvVar("NZBPP_HEALTHDELETED", (int)m_postInfo->GetNzbInfo()->GetDeleteStatus() == NzbInfo::dsHealth);

	SetIntEnvVar("NZBPP_TOTALARTICLES", (int)m_postInfo->GetNzbInfo()->GetTotalArticles());
	SetIntEnvVar("NZBPP_SUCCESSARTICLES", (int)m_postInfo->GetNzbInfo()->GetSuccessArticles());
	SetIntEnvVar("NZBPP_FAILEDARTICLES", (int)m_postInfo->GetNzbInfo()->GetFailedArticles());

	for (ServerStatList::iterator it = m_postInfo->GetNzbInfo()->GetServerStats()->begin(); it != m_postInfo->GetNzbInfo()->GetServerStats()->end(); it++)
	{
		ServerStat* serverStat = *it;

		char name[50];

		snprintf(name, 50, "NZBPP_SERVER%i_SUCCESSARTICLES", serverStat->GetServerId());
		name[50-1] = '\0';
		SetIntEnvVar(name, serverStat->GetSuccessArticles());

		snprintf(name, 50, "NZBPP_SERVER%i_FAILEDARTICLES", serverStat->GetServerId());
		name[50-1] = '\0';
		SetIntEnvVar(name, serverStat->GetFailedArticles());
	}

	PrepareEnvScript(m_postInfo->GetNzbInfo()->GetParameters(), scriptName);

	DownloadQueue::Unlock();
}

ScriptStatus::EStatus PostScriptController::AnalyseExitCode(int exitCode)
{
	// The ScriptStatus is accumulated for all scripts:
	// If any script has failed the status is "failure", etc.

	switch (exitCode)
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
			if (m_postInfo->GetNzbInfo()->GetParStatus() > NzbInfo::psSkipped)
			{
				PrintMessage(Message::mkError, "%s requested par-check/repair, but the collection was already checked", GetInfoName());
				return ScriptStatus::srFailure;
			}
			else
			{
				PrintMessage(Message::mkInfo, "%s requested par-check/repair", GetInfoName());
				m_postInfo->SetRequestParCheck(true);
				m_postInfo->SetForceRepair(true);
				return ScriptStatus::srSuccess;
			}
			break;
#endif

		default:
			PrintMessage(Message::mkError, "%s failed (terminated with unknown status)", GetInfoName());
			return ScriptStatus::srFailure;
	}
}

void PostScriptController::AddMessage(Message::EKind kind, const char* text)
{
	const char* msgText = text + m_prefixLen;

	if (!strncmp(msgText, "[NZB] ", 6))
	{
		debug("Command %s detected", msgText + 6);
		if (!strncmp(msgText + 6, "FINALDIR=", 9))
		{
			DownloadQueue::Lock();
			m_postInfo->GetNzbInfo()->SetFinalDir(msgText + 6 + 9);
			DownloadQueue::Unlock();
		}
		else if (!strncmp(msgText + 6, "DIRECTORY=", 10))
		{
			DownloadQueue::Lock();
			m_postInfo->GetNzbInfo()->SetDestDir(msgText + 6 + 10);
			DownloadQueue::Unlock();
		}
		else if (!strncmp(msgText + 6, "NZBPR_", 6))
		{
			char* param = strdup(msgText + 6 + 6);
			char* value = strchr(param, '=');
			if (value)
			{
				*value = '\0';
				DownloadQueue::Lock();
				m_postInfo->GetNzbInfo()->GetParameters()->SetParameter(param, value + 1);
				DownloadQueue::Unlock();
			}
			else
			{
				m_postInfo->GetNzbInfo()->PrintMessage(Message::mkError,
					"Invalid command \"%s\" received from %s", msgText, GetInfoName());
			}
			free(param);
		}
		else if (!strncmp(msgText + 6, "MARK=BAD", 8))
		{
			SetLogPrefix(NULL);
			PrintMessage(Message::mkWarning, "Marking %s as bad", m_postInfo->GetNzbInfo()->GetName());
			SetLogPrefix(m_script->GetDisplayName());
			m_postInfo->GetNzbInfo()->SetMarkStatus(NzbInfo::ksBad);
		}
		else
		{
			m_postInfo->GetNzbInfo()->PrintMessage(Message::mkError,
				"Invalid command \"%s\" received from %s", msgText, GetInfoName());
		}
	}
	else
	{
		m_postInfo->GetNzbInfo()->AddMessage(kind, text);
		DownloadQueue::Lock();
		m_postInfo->SetProgressLabel(text);
		DownloadQueue::Unlock();
	}

	if (g_Options->GetPausePostProcess() && !m_postInfo->GetNzbInfo()->GetForcePriority())
	{
		time_t stageTime = m_postInfo->GetStageTime();
		time_t startTime = m_postInfo->GetStartTime();
		time_t waitTime = time(NULL);

		// wait until Post-processor is unpaused
		while (g_Options->GetPausePostProcess() && !m_postInfo->GetNzbInfo()->GetForcePriority() && !IsStopped())
		{
			usleep(100 * 1000);

			// update time stamps

			time_t delta = time(NULL) - waitTime;

			if (stageTime > 0)
			{
				m_postInfo->SetStageTime(stageTime + delta);
			}

			if (startTime > 0)
			{
				m_postInfo->SetStartTime(startTime + delta);
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
