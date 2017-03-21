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
#include "FileSystem.h"

static const int COMMANDPROCESS_SUCCESS = 93;
static const int COMMANDPROCESS_ERROR = 94;

bool CommandScriptController::StartScript(const char* scriptName, const char* command,
	std::unique_ptr<Options::OptEntries> modifiedOptions)
{
	CommandScriptController* scriptController = new CommandScriptController();
	scriptController->m_script = scriptName;
	scriptController->m_command = command;
	scriptController->m_logId = g_CommandScriptLog->Reset();
	scriptController->m_modifiedOptions = std::move(modifiedOptions);

	scriptController->SetAutoDestroy(true);

	scriptController->Start();

	for (ScriptConfig::Script& script : g_ScriptConfig->GetScripts())
	{
		if (FileSystem::SameFilename(scriptName, script.GetName()))
		{
			return true;
		}
	}
	return false;
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

	int exitCode = Execute();

	infoName[0] = 'S'; // uppercase
	SetLogPrefix(nullptr);

	switch (exitCode)
	{
		case COMMANDPROCESS_SUCCESS:
			PrintMessage(Message::mkInfo, "%s successful", *infoName);
			break;

		case COMMANDPROCESS_ERROR:
		case -1: // Execute() returns -1 if the process could not be started (file not found or other problem)
			PrintMessage(Message::mkError, "%s failed", *infoName);
			break;

		default:
			PrintMessage(Message::mkError, "%s failed (terminated with unknown status)", *infoName);
			break;
	}
}

void CommandScriptController::PrepareParams(const char* scriptName)
{
	ResetEnv();

	SetEnvVar("NZBCP_COMMAND", m_command);

	PrepareEnvScript(nullptr, scriptName);
}

const char* CommandScriptController::GetOptValue(const char* name, const char* value)
{
	Options::OptEntry* entry = m_modifiedOptions->FindOption(name);
	return entry ? entry->GetValue() : value;
}

void CommandScriptController::AddMessage(Message::EKind kind, const char * text)
{
	NzbScriptController::AddMessage(kind, text);
	g_CommandScriptLog->AddMessage(m_logId, kind, text);
}


int CommandScriptLog::Reset()
{
	Guard guard(m_logMutex);
	m_messages.clear();
	return ++m_idScriptGen;
}

void CommandScriptLog::AddMessage(int scriptId, Message::EKind kind, const char * text)
{
	Guard guard(m_logMutex);

	// save only messages from the last started script
	if (scriptId == m_idScriptGen)
	{
		m_messages.emplace_back(++m_idMessageGen, kind, Util::CurrentTime(), text);
	}
}
