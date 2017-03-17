/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "CommandScript.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"

void CommandScriptController::StartScript(const char* scriptName, const char* command)
{
	CommandScriptController* scriptController = new CommandScriptController();
	scriptController->m_script = scriptName;
	scriptController->m_command = command;

	scriptController->SetAutoDestroy(true);

	scriptController->Start();
}

void CommandScriptController::Run()
{
	ExecuteScriptList(m_script);
}

void CommandScriptController::ExecuteScript(ScriptConfig::Script* script)
{
	PrintMessage(Message::mkInfo, "Executing script %s with command %s", script->GetName(), *m_command);

	SetArgs({script->GetLocation()});

	BString<1024> infoName("script %s with command %s", script->GetName(), *m_command);
	SetInfoName(infoName);

	SetLogPrefix(script->GetDisplayName());
	PrepareParams(script->GetName());

	Execute();

	SetLogPrefix(nullptr);
}

void CommandScriptController::PrepareParams(const char* scriptName)
{
	ResetEnv();

	SetEnvVar("NZBCP_COMMAND", m_command);

	PrepareEnvScript(nullptr, scriptName);
}
