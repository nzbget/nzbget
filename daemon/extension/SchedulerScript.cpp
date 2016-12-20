/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "SchedulerScript.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"
#include "FileSystem.h"

void SchedulerScriptController::StartScript(const char* param, bool externalProcess, int taskId)
{
	std::vector<CString> argv;
	if (externalProcess && (argv = Util::SplitCommandLine(param)).empty())
	{
		error("Could not execute scheduled process-script, failed to parse command line: %s", param);
		return;
	}

	SchedulerScriptController* scriptController = new SchedulerScriptController();

	scriptController->m_externalProcess = externalProcess;
	scriptController->m_script = param;
	scriptController->m_taskId = taskId;

	if (externalProcess)
	{
		scriptController->SetArgs(std::move(argv));
	}

	scriptController->SetAutoDestroy(true);

	scriptController->Start();
}

void SchedulerScriptController::Run()
{
	if (m_externalProcess)
	{
		ExecuteExternalProcess();
	}
	else
	{
		ExecuteScriptList(m_script);
	}
}

void SchedulerScriptController::ExecuteScript(ScriptConfig::Script* script)
{
	if (!script->GetSchedulerScript())
	{
		return;
	}

	BString<1024> taskName(" for Task%i", m_taskId);
	if (m_taskId == 0)
	{
		taskName = "";
	}

	PrintMessage(Message::mkInfo, "Executing scheduler-script %s%s", script->GetName(), *taskName);

	SetArgs({script->GetLocation()});

	BString<1024> infoName("scheduler-script %s%s", script->GetName(), *taskName);
	SetInfoName(infoName);

	SetLogPrefix(script->GetDisplayName());
	PrepareParams(script->GetName());

	Execute();

	SetLogPrefix(nullptr);
}

void SchedulerScriptController::PrepareParams(const char* scriptName)
{
	ResetEnv();

	SetIntEnvVar("NZBSP_TASKID", m_taskId);

	PrepareEnvScript(nullptr, scriptName);
}

void SchedulerScriptController::ExecuteExternalProcess()
{
	info("Executing scheduled process-script %s for Task%i", FileSystem::BaseFileName(GetScript()), m_taskId);

	BString<1024> infoName("scheduled process-script %s for Task%i", FileSystem::BaseFileName(GetScript()), m_taskId);
	SetInfoName(infoName);

	BString<1024> logPrefix = FileSystem::BaseFileName(GetScript());
	if (char* ext = strrchr(logPrefix, '.')) *ext = '\0'; // strip file extension
	SetLogPrefix(logPrefix);

	Execute();
}
