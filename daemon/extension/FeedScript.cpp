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


void FeedScriptController::ExecuteScripts(const char* szFeedFile, int iFeedID)
{
	FeedScriptController* pScriptController = new FeedScriptController();

	pScriptController->m_szFeedFile = szFeedFile;
	pScriptController->m_iFeedID = iFeedID;

	pScriptController->ExecuteScriptList(g_pOptions->GetFeedScript());

	delete pScriptController;
}

void FeedScriptController::ExecuteScript(ScriptConfig::Script* pScript)
{
	if (!pScript->GetFeedScript())
	{
		return;
	}

	PrintMessage(Message::mkInfo, "Executing feed-script %s for Feed%i", pScript->GetName(), m_iFeedID);

	SetScript(pScript->GetLocation());
	SetArgs(NULL, false);

	char szInfoName[1024];
	snprintf(szInfoName, 1024, "feed-script %s for Feed%i", pScript->GetName(), m_iFeedID);
	szInfoName[1024-1] = '\0';
	SetInfoName(szInfoName);

	SetLogPrefix(pScript->GetDisplayName());
	PrepareParams(pScript->GetName());

	Execute();

	SetLogPrefix(NULL);
}

void FeedScriptController::PrepareParams(const char* szScriptName)
{
	ResetEnv();

	SetEnvVar("NZBFP_FILENAME", m_szFeedFile);
	SetIntEnvVar("NZBFP_FEEDID", m_iFeedID);

	PrepareEnvScript(NULL, szScriptName);
}
