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
#include "ScanScript.h"
#include "Scanner.h"
#include "Options.h"
#include "Log.h"
#include "FileSystem.h"

class ScanScriptCheck : public NzbScriptController
{
protected:
	virtual void ExecuteScript(ScriptConfig::Script* script) { has |= script->GetScanScript(); }
	bool has = false;
	friend class ScanScriptController;
};


bool ScanScriptController::HasScripts()
{
	ScanScriptCheck check;
	check.ExecuteScriptList(g_Options->GetExtensions());
	return check.has;
}

void ScanScriptController::ExecuteScripts(const char* nzbFilename,
	const char* url, const char* directory, CString* nzbName, CString* category,
	int* priority, NzbParameterList* parameters, bool* addTop, bool* addPaused,
	CString* dupeKey, int* dupeScore, EDupeMode* dupeMode)
{
	ScanScriptController scriptController;
	scriptController.m_nzbFilename = nzbFilename;
	scriptController.m_url = url;
	scriptController.m_directory = directory;
	scriptController.m_nzbName = nzbName;
	scriptController.m_category = category;
	scriptController.m_parameters = parameters;
	scriptController.m_priority = priority;
	scriptController.m_addTop = addTop;
	scriptController.m_addPaused = addPaused;
	scriptController.m_dupeKey = dupeKey;
	scriptController.m_dupeScore = dupeScore;
	scriptController.m_dupeMode = dupeMode;
	scriptController.m_prefixLen = 0;

	const char* extensions = g_Options->GetExtensions();

	if (!Util::EmptyStr(*category))
	{
		Options::Category* categoryObj = g_Options->FindCategory(*category, false);
		if (categoryObj && !Util::EmptyStr(categoryObj->GetExtensions()))
		{
			extensions = categoryObj->GetExtensions();
		}
	}

	scriptController.ExecuteScriptList(extensions);
}

void ScanScriptController::ExecuteScript(ScriptConfig::Script* script)
{
	if (!script->GetScanScript() || !FileSystem::FileExists(m_nzbFilename))
	{
		return;
	}

	PrintMessage(Message::mkInfo, "Executing scan-script %s for %s", script->GetName(), FileSystem::BaseFileName(m_nzbFilename));

	SetArgs({script->GetLocation()});

	BString<1024> infoName("scan-script %s for %s", script->GetName(), FileSystem::BaseFileName(m_nzbFilename));
	SetInfoName(infoName);

	SetLogPrefix(script->GetDisplayName());
	m_prefixLen = strlen(script->GetDisplayName()) + 2; // 2 = strlen(": ");
	PrepareParams(script->GetName());

	Execute();

	SetLogPrefix(nullptr);
}

void ScanScriptController::PrepareParams(const char* scriptName)
{
	ResetEnv();

	SetEnvVar("NZBNP_FILENAME", m_nzbFilename);
	SetEnvVar("NZBNP_URL", m_url);
	SetEnvVar("NZBNP_NZBNAME", strlen(*m_nzbName) > 0 ? **m_nzbName : FileSystem::BaseFileName(m_nzbFilename));
	SetEnvVar("NZBNP_CATEGORY", *m_category);
	SetIntEnvVar("NZBNP_PRIORITY", *m_priority);
	SetIntEnvVar("NZBNP_TOP", *m_addTop ? 1 : 0);
	SetIntEnvVar("NZBNP_PAUSED", *m_addPaused ? 1 : 0);
	SetEnvVar("NZBNP_DUPEKEY", *m_dupeKey);
	SetIntEnvVar("NZBNP_DUPESCORE", *m_dupeScore);

	const char* dupeModeName[] = { "SCORE", "ALL", "FORCE" };
	SetEnvVar("NZBNP_DUPEMODE", dupeModeName[*m_dupeMode]);

	// remove trailing slash
	BString<1024> dir = m_directory;
	int len = strlen(dir);
	if (dir[len-1] == PATH_SEPARATOR)
	{
		dir[len-1] = '\0';
	}
	SetEnvVar("NZBNP_DIRECTORY", dir);

	PrepareEnvScript(m_parameters, scriptName);
}

void ScanScriptController::AddMessage(Message::EKind kind, const char* text)
{
	const char* msgText = text + m_prefixLen;

	if (!strncmp(msgText, "[NZB] ", 6))
	{
		debug("Command %s detected", msgText + 6);
		if (!strncmp(msgText + 6, "NZBNAME=", 8))
		{
			*m_nzbName = msgText + 6 + 8;
		}
		else if (!strncmp(msgText + 6, "CATEGORY=", 9))
		{
			*m_category = msgText + 6 + 9;
			g_Scanner->InitPPParameters(*m_category, m_parameters, true);
		}
		else if (!strncmp(msgText + 6, "NZBPR_", 6))
		{
			CString param = msgText + 6 + 6;
			char* value = strchr(param, '=');
			if (value)
			{
				*value = '\0';
				m_parameters->SetParameter(param, value + 1);
			}
			else
			{
				error("Invalid command \"%s\" received from %s", msgText, GetInfoName());
			}
		}
		else if (!strncmp(msgText + 6, "PRIORITY=", 9))
		{
			*m_priority = atoi(msgText + 6 + 9);
		}
		else if (!strncmp(msgText + 6, "TOP=", 4))
		{
			*m_addTop = atoi(msgText + 6 + 4) != 0;
		}
		else if (!strncmp(msgText + 6, "PAUSED=", 7))
		{
			*m_addPaused = atoi(msgText + 6 + 7) != 0;
		}
		else if (!strncmp(msgText + 6, "DUPEKEY=", 8))
		{
			*m_dupeKey = msgText + 6 + 8;
		}
		else if (!strncmp(msgText + 6, "DUPESCORE=", 10))
		{
			*m_dupeScore = atoi(msgText + 6 + 10);
		}
		else if (!strncmp(msgText + 6, "DUPEMODE=", 9))
		{
			const char* dupeMode = msgText + 6 + 9;
			if (strcasecmp(dupeMode, "score") && strcasecmp(dupeMode, "all") && strcasecmp(dupeMode, "force"))
			{
				error("Invalid value \"%s\" for command \"DUPEMODE\" received from %s", dupeMode, GetInfoName());
				return;
			}
			*m_dupeMode = !strcasecmp(dupeMode, "all") ? dmAll :
				!strcasecmp(dupeMode, "force") ? dmForce : dmScore;
		}
		else
		{
			error("Invalid command \"%s\" received from %s", msgText, GetInfoName());
		}
	}
	else
	{
		ScriptController::AddMessage(kind, text);
	}
}
