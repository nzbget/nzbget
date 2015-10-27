/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "FeedScript.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"


void FeedScriptController::ExecuteScripts(const char* feedScript, const char* feedFile, int feedId)
{
	FeedScriptController* scriptController = new FeedScriptController();

	scriptController->m_feedFile = feedFile;
	scriptController->m_feedId = feedId;

	scriptController->ExecuteScriptList(feedScript);

	delete scriptController;
}

void FeedScriptController::ExecuteScript(ScriptConfig::Script* script)
{
	if (!script->GetFeedScript())
	{
		return;
	}

	PrintMessage(Message::mkInfo, "Executing feed-script %s for Feed%i", script->GetName(), m_feedId);

	SetScript(script->GetLocation());
	SetArgs(NULL, false);

	char infoName[1024];
	snprintf(infoName, 1024, "feed-script %s for Feed%i", script->GetName(), m_feedId);
	infoName[1024-1] = '\0';
	SetInfoName(infoName);

	SetLogPrefix(script->GetDisplayName());
	PrepareParams(script->GetName());

	Execute();

	SetLogPrefix(NULL);
}

void FeedScriptController::PrepareParams(const char* scriptName)
{
	ResetEnv();

	SetEnvVar("NZBFP_FILENAME", m_feedFile);
	SetIntEnvVar("NZBFP_FEEDID", m_feedId);

	PrepareEnvScript(NULL, scriptName);
}
