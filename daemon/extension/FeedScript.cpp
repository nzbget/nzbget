/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2015-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "FeedScript.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"

static const int FEED_SUCCESS = 93;

void FeedScriptController::ExecuteScripts(const char* feedScript, const char* feedFile, int feedId, bool* success)
{
	FeedScriptController scriptController;
	scriptController.m_feedFile = feedFile;
	scriptController.m_feedId = feedId;

	scriptController.ExecuteScriptList(feedScript);

	if (success)
	{
		*success = scriptController.m_success;
	}
}

void FeedScriptController::ExecuteScript(ScriptConfig::Script* script)
{
	if (!script->GetFeedScript())
	{
		return;
	}

	PrintMessage(Message::mkInfo, "Executing feed-script %s for Feed%i", script->GetName(), m_feedId);

	SetArgs({script->GetLocation()});

	BString<1024> infoName("feed-script %s for Feed%i", script->GetName(), m_feedId);
	SetInfoName(infoName);

	SetLogPrefix(script->GetDisplayName());
	PrepareParams(script->GetName());

	int exitCode = Execute();

	if (exitCode != FEED_SUCCESS)
	{
		infoName[0] = 'F'; // uppercase
		PrintMessage(Message::mkError, "%s failed", *infoName);
		m_success = false;
	}

	SetLogPrefix(nullptr);
}

void FeedScriptController::PrepareParams(const char* scriptName)
{
	ResetEnv();

	SetEnvVar("NZBFP_FILENAME", m_feedFile);
	SetIntEnvVar("NZBFP_FEEDID", m_feedId);

	PrepareEnvScript(nullptr, scriptName);
}
