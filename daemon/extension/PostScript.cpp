/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2007-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "nzbget.h"
#include "PostScript.h"
#include "Log.h"
#include "Util.h"
#include "Options.h"
#include "WorkState.h"

static const int POSTPROCESS_PARCHECK = 92;
static const int POSTPROCESS_SUCCESS = 93;
static const int POSTPROCESS_ERROR = 94;
static const int POSTPROCESS_NONE = 95;

void PostScriptController::StartJob(PostInfo* postInfo)
{
	PostScriptController* scriptController = new PostScriptController();
	scriptController->m_postInfo = postInfo;
	scriptController->SetAutoDestroy(false);
	scriptController->m_prefixLen = 0;

	postInfo->SetPostThread(scriptController);

	scriptController->Start();
}

void PostScriptController::Run()
{
	StringBuilder scriptCommaList;

	{
		GuardedDownloadQueue guard = DownloadQueue::Guard();
		for (NzbParameter& parameter : m_postInfo->GetNzbInfo()->GetParameters())
		{
			const char* varname = parameter.GetName();
			if (strlen(varname) > 0 && varname[0] != '*' && varname[strlen(varname) - 1] == ':' &&
				(!strcasecmp(parameter.GetValue(), "yes") || !strcasecmp(parameter.GetValue(), "on") || !strcasecmp(parameter.GetValue(), "1")))
			{
				CString scriptName(varname);
				scriptName[strlen(scriptName) - 1] = '\0'; // remove trailing ':'
				scriptCommaList.Append(scriptName);
				scriptCommaList.Append(",");
			}
		}
		m_postInfo->GetNzbInfo()->GetScriptStatuses()->clear();
	}

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

	BString<1024> progressLabel("Executing post-process-script %s", script->GetName());

	{
		GuardedDownloadQueue guard = DownloadQueue::Guard();
		m_postInfo->SetProgressLabel(progressLabel);
	}

	SetArgs({script->GetLocation()});

	BString<1024> infoName("post-process-script %s for %s", script->GetName(), m_postInfo->GetNzbInfo()->GetName());
	SetInfoName(infoName);

	m_script = script;
	SetLogPrefix(script->GetDisplayName());
	m_prefixLen = strlen(script->GetDisplayName()) + 2; // 2 = strlen(": ");
	PrepareParams(script->GetName());

	int exitCode = Execute();

	infoName[0] = 'P'; // uppercase

	SetLogPrefix(nullptr);
	ScriptStatus::EStatus status = AnalyseExitCode(exitCode, infoName);

	{
		GuardedDownloadQueue guard = DownloadQueue::Guard();
		m_postInfo->GetNzbInfo()->GetScriptStatuses()->emplace_back(script->GetName(), status);
	}
}

void PostScriptController::PrepareParams(const char* scriptName)
{
	GuardedDownloadQueue guard = DownloadQueue::Guard();

	ResetEnv();

	SetIntEnvVar("NZBPP_NZBID", m_postInfo->GetNzbInfo()->GetId());
	SetEnvVar("NZBPP_NZBNAME", m_postInfo->GetNzbInfo()->GetName());
	SetEnvVar("NZBPP_DIRECTORY", m_postInfo->GetNzbInfo()->GetDestDir());
	SetEnvVar("NZBPP_NZBFILENAME", m_postInfo->GetNzbInfo()->GetFilename());
	SetEnvVar("NZBPP_QUEUEDFILE", m_postInfo->GetNzbInfo()->GetQueuedFilename());
	SetEnvVar("NZBPP_URL", m_postInfo->GetNzbInfo()->GetUrl());
	SetEnvVar("NZBPP_FINALDIR", m_postInfo->GetNzbInfo()->GetFinalDir());
	SetEnvVar("NZBPP_CATEGORY", m_postInfo->GetNzbInfo()->GetCategory());
	SetIntEnvVar("NZBPP_HEALTH", m_postInfo->GetNzbInfo()->CalcHealth());
	SetIntEnvVar("NZBPP_CRITICALHEALTH", m_postInfo->GetNzbInfo()->CalcCriticalHealth(false));

	SetEnvVar("NZBPP_DUPEKEY", m_postInfo->GetNzbInfo()->GetDupeKey());
	SetIntEnvVar("NZBPP_DUPESCORE", m_postInfo->GetNzbInfo()->GetDupeScore());

	const char* dupeModeName[] = { "SCORE", "ALL", "FORCE" };
	SetEnvVar("NZBPP_DUPEMODE", dupeModeName[m_postInfo->GetNzbInfo()->GetDupeMode()]);

	BString<1024> status = m_postInfo->GetNzbInfo()->MakeTextStatus(true);
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

	for (ServerStat& serverStat : m_postInfo->GetNzbInfo()->GetServerStats())
	{
		SetIntEnvVar(BString<1024>("NZBPP_SERVER%i_SUCCESSARTICLES", serverStat.GetServerId()),
			serverStat.GetSuccessArticles());

		SetIntEnvVar(BString<1024>("NZBPP_SERVER%i_FAILEDARTICLES", serverStat.GetServerId()),
			serverStat.GetFailedArticles());
	}

	PrepareEnvScript(m_postInfo->GetNzbInfo()->GetParameters(), scriptName);
}

ScriptStatus::EStatus PostScriptController::AnalyseExitCode(int exitCode, const char* upInfoName)
{
	// The ScriptStatus is accumulated for all scripts:
	// If any script has failed the status is "failure", etc.

	switch (exitCode)
	{
		case POSTPROCESS_SUCCESS:
			PrintMessage(Message::mkInfo, "%s successful", upInfoName);
			return ScriptStatus::srSuccess;

		case POSTPROCESS_ERROR:
		case -1: // Execute() returns -1 if the process could not be started (file not found or other problem)
			PrintMessage(Message::mkError, "%s failed", upInfoName);
			return ScriptStatus::srFailure;

		case POSTPROCESS_NONE:
			PrintMessage(Message::mkInfo, "%s skipped", upInfoName);
			return ScriptStatus::srNone;

#ifndef DISABLE_PARCHECK
		case POSTPROCESS_PARCHECK:
			if (m_postInfo->GetNzbInfo()->GetParStatus() > NzbInfo::psSkipped)
			{
				PrintMessage(Message::mkError, "%s requested par-check/repair, but the collection was already checked", upInfoName);
				return ScriptStatus::srFailure;
			}
			else
			{
				PrintMessage(Message::mkInfo, "%s requested par-check/repair", upInfoName);
				m_postInfo->SetRequestParCheck(true);
				m_postInfo->SetForceRepair(true);
				return ScriptStatus::srSuccess;
			}
			break;
#endif

		default:
			PrintMessage(Message::mkError, "%s failed (terminated with unknown status)", upInfoName);
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
			GuardedDownloadQueue guard = DownloadQueue::Guard();
			m_postInfo->GetNzbInfo()->SetFinalDir(msgText + 6 + 9);
		}
		else if (!strncmp(msgText + 6, "DIRECTORY=", 10))
		{
			GuardedDownloadQueue guard = DownloadQueue::Guard();
			m_postInfo->GetNzbInfo()->SetDestDir(msgText + 6 + 10);
		}
		else if (!strncmp(msgText + 6, "NZBPR_", 6))
		{
			CString param = msgText + 6 + 6;
			char* value = strchr(param, '=');
			if (value)
			{
				*value = '\0';
				GuardedDownloadQueue guard = DownloadQueue::Guard();
				m_postInfo->GetNzbInfo()->GetParameters()->SetParameter(param, value + 1);
			}
			else
			{
				m_postInfo->GetNzbInfo()->PrintMessage(Message::mkError,
					"Invalid command \"%s\" received from %s", msgText, GetInfoName());
			}
		}
		else if (!strncmp(msgText + 6, "MARK=BAD", 8))
		{
			SetLogPrefix(nullptr);
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
		GuardedDownloadQueue guard = DownloadQueue::Guard();
		m_postInfo->SetProgressLabel(text);
	}

	if (g_WorkState->GetPausePostProcess() && !m_postInfo->GetNzbInfo()->GetForcePriority())
	{
		time_t stageTime = m_postInfo->GetStageTime();
		time_t startTime = m_postInfo->GetStartTime();
		time_t waitTime = Util::CurrentTime();

		// wait until Post-processor is unpaused
		while (g_WorkState->GetPausePostProcess() && !m_postInfo->GetNzbInfo()->GetForcePriority() && !IsStopped())
		{
			Util::Sleep(100);

			// update time stamps

			time_t delta = Util::CurrentTime() - waitTime;

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
