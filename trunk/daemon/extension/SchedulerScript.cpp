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
#include <sys/stat.h>
#include <stdio.h>

#include "nzbget.h"
#include "SchedulerScript.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"


SchedulerScriptController::~SchedulerScriptController()
{
	free(m_szScript);
}

void SchedulerScriptController::StartScript(const char* szParam, bool bExternalProcess, int iTaskID)
{
	char** argv = NULL;
	if (bExternalProcess && !Util::SplitCommandLine(szParam, &argv))
	{
		error("Could not execute scheduled process-script, failed to parse command line: %s", szParam);
		return;
	}

	SchedulerScriptController* pScriptController = new SchedulerScriptController();

	pScriptController->m_bExternalProcess = bExternalProcess;
	pScriptController->m_szScript = strdup(szParam);
	pScriptController->m_iTaskID = iTaskID;

	if (bExternalProcess)
	{
		pScriptController->SetScript(argv[0]);
		pScriptController->SetArgs((const char**)argv, true);
	}

	pScriptController->SetAutoDestroy(true);

	pScriptController->Start();
}

void SchedulerScriptController::Run()
{
	if (m_bExternalProcess)
	{
		ExecuteExternalProcess();
	}
	else
	{
		ExecuteScriptList(m_szScript);
	}
}

void SchedulerScriptController::ExecuteScript(ScriptConfig::Script* pScript)
{
	if (!pScript->GetSchedulerScript())
	{
		return;
	}

	PrintMessage(Message::mkInfo, "Executing scheduler-script %s for Task%i", pScript->GetName(), m_iTaskID);

	SetScript(pScript->GetLocation());
	SetArgs(NULL, false);

	char szInfoName[1024];
	snprintf(szInfoName, 1024, "scheduler-script %s for Task%i", pScript->GetName(), m_iTaskID);
	szInfoName[1024-1] = '\0';
	SetInfoName(szInfoName);

	SetLogPrefix(pScript->GetDisplayName());
	PrepareParams(pScript->GetName());

	Execute();

	SetLogPrefix(NULL);
}

void SchedulerScriptController::PrepareParams(const char* szScriptName)
{
	ResetEnv();

	SetIntEnvVar("NZBSP_TASKID", m_iTaskID);

	PrepareEnvScript(NULL, szScriptName);
}

void SchedulerScriptController::ExecuteExternalProcess()
{
	info("Executing scheduled process-script %s for Task%i", Util::BaseFileName(GetScript()), m_iTaskID);

	char szInfoName[1024];
	snprintf(szInfoName, 1024, "scheduled process-script %s for Task%i", Util::BaseFileName(GetScript()), m_iTaskID);
	szInfoName[1024-1] = '\0';
	SetInfoName(szInfoName);

	char szLogPrefix[1024];
	strncpy(szLogPrefix, Util::BaseFileName(GetScript()), 1024);
	szLogPrefix[1024-1] = '\0';
	if (char* ext = strrchr(szLogPrefix, '.')) *ext = '\0'; // strip file extension
	SetLogPrefix(szLogPrefix);

	Execute();
}
