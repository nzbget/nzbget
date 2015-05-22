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
#include "ScanScript.h"
#include "Scanner.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"

void ScanScriptController::ExecuteScripts(const char* szNZBFilename,
	const char* szUrl, const char* szDirectory, char** pNZBName, char** pCategory,
	int* iPriority, NZBParameterList* pParameters, bool* bAddTop, bool* bAddPaused,
	char** pDupeKey, int* iDupeScore, EDupeMode* eDupeMode)
{
	ScanScriptController* pScriptController = new ScanScriptController();

	pScriptController->m_szNZBFilename = szNZBFilename;
	pScriptController->m_szUrl = szUrl;
	pScriptController->m_szDirectory = szDirectory;
	pScriptController->m_pNZBName = pNZBName;
	pScriptController->m_pCategory = pCategory;
	pScriptController->m_pParameters = pParameters;
	pScriptController->m_iPriority = iPriority;
	pScriptController->m_bAddTop = bAddTop;
	pScriptController->m_bAddPaused = bAddPaused;
	pScriptController->m_pDupeKey = pDupeKey;
	pScriptController->m_iDupeScore = iDupeScore;
	pScriptController->m_eDupeMode = eDupeMode;
	pScriptController->m_iPrefixLen = 0;

	pScriptController->ExecuteScriptList(g_pOptions->GetScanScript());

	delete pScriptController;
}

void ScanScriptController::ExecuteScript(ScriptConfig::Script* pScript)
{
	if (!pScript->GetScanScript() || !Util::FileExists(m_szNZBFilename))
	{
		return;
	}

	PrintMessage(Message::mkInfo, "Executing scan-script %s for %s", pScript->GetName(), Util::BaseFileName(m_szNZBFilename));

	SetScript(pScript->GetLocation());
	SetArgs(NULL, false);

	char szInfoName[1024];
	snprintf(szInfoName, 1024, "scan-script %s for %s", pScript->GetName(), Util::BaseFileName(m_szNZBFilename));
	szInfoName[1024-1] = '\0';
	SetInfoName(szInfoName);

	SetLogPrefix(pScript->GetDisplayName());
	m_iPrefixLen = strlen(pScript->GetDisplayName()) + 2; // 2 = strlen(": ");
	PrepareParams(pScript->GetName());

	Execute();

	SetLogPrefix(NULL);
}

void ScanScriptController::PrepareParams(const char* szScriptName)
{
	ResetEnv();

	SetEnvVar("NZBNP_FILENAME", m_szNZBFilename);
	SetEnvVar("NZBNP_URL", m_szUrl);
	SetEnvVar("NZBNP_NZBNAME", strlen(*m_pNZBName) > 0 ? *m_pNZBName : Util::BaseFileName(m_szNZBFilename));
	SetEnvVar("NZBNP_CATEGORY", *m_pCategory);
	SetIntEnvVar("NZBNP_PRIORITY", *m_iPriority);
	SetIntEnvVar("NZBNP_TOP", *m_bAddTop ? 1 : 0);
	SetIntEnvVar("NZBNP_PAUSED", *m_bAddPaused ? 1 : 0);
	SetEnvVar("NZBNP_DUPEKEY", *m_pDupeKey);
	SetIntEnvVar("NZBNP_DUPESCORE", *m_iDupeScore);

	const char* szDupeModeName[] = { "SCORE", "ALL", "FORCE" };
	SetEnvVar("NZBNP_DUPEMODE", szDupeModeName[*m_eDupeMode]);

	// remove trailing slash
	char szDir[1024];
	strncpy(szDir, m_szDirectory, 1024);
	szDir[1024-1] = '\0';
	int iLen = strlen(szDir);
	if (szDir[iLen-1] == PATH_SEPARATOR)
	{
		szDir[iLen-1] = '\0';
	}
	SetEnvVar("NZBNP_DIRECTORY", szDir);

	PrepareEnvScript(m_pParameters, szScriptName);
}

void ScanScriptController::AddMessage(Message::EKind eKind, const char* szText)
{
	const char* szMsgText = szText + m_iPrefixLen;

	if (!strncmp(szMsgText, "[NZB] ", 6))
	{
		debug("Command %s detected", szMsgText + 6);
		if (!strncmp(szMsgText + 6, "NZBNAME=", 8))
		{
			free(*m_pNZBName);
			*m_pNZBName = strdup(szMsgText + 6 + 8);
		}
		else if (!strncmp(szMsgText + 6, "CATEGORY=", 9))
		{
			free(*m_pCategory);
			*m_pCategory = strdup(szMsgText + 6 + 9);
			g_pScanner->InitPPParameters(*m_pCategory, m_pParameters, true);
		}
		else if (!strncmp(szMsgText + 6, "NZBPR_", 6))
		{
			char* szParam = strdup(szMsgText + 6 + 6);
			char* szValue = strchr(szParam, '=');
			if (szValue)
			{
				*szValue = '\0';
				m_pParameters->SetParameter(szParam, szValue + 1);
			}
			else
			{
				error("Invalid command \"%s\" received from %s", szMsgText, GetInfoName());
			}
			free(szParam);
		}
		else if (!strncmp(szMsgText + 6, "PRIORITY=", 9))
		{
			*m_iPriority = atoi(szMsgText + 6 + 9);
		}
		else if (!strncmp(szMsgText + 6, "TOP=", 4))
		{
			*m_bAddTop = atoi(szMsgText + 6 + 4) != 0;
		}
		else if (!strncmp(szMsgText + 6, "PAUSED=", 7))
		{
			*m_bAddPaused = atoi(szMsgText + 6 + 7) != 0;
		}
		else if (!strncmp(szMsgText + 6, "DUPEKEY=", 8))
		{
			free(*m_pDupeKey);
			*m_pDupeKey = strdup(szMsgText + 6 + 8);
		}
		else if (!strncmp(szMsgText + 6, "DUPESCORE=", 10))
		{
			*m_iDupeScore = atoi(szMsgText + 6 + 10);
		}
		else if (!strncmp(szMsgText + 6, "DUPEMODE=", 9))
		{
			const char* szDupeMode = szMsgText + 6 + 9;
			if (strcasecmp(szDupeMode, "score") && strcasecmp(szDupeMode, "all") && strcasecmp(szDupeMode, "force"))
			{
				error("Invalid value \"%s\" for command \"DUPEMODE\" received from %s", szDupeMode, GetInfoName());
				return;
			}
			*m_eDupeMode = !strcasecmp(szDupeMode, "all") ? dmAll :
				!strcasecmp(szDupeMode, "force") ? dmForce : dmScore;
		}
		else
		{
			error("Invalid command \"%s\" received from %s", szMsgText, GetInfoName());
		}
	}
	else
	{
		ScriptController::AddMessage(eKind, szText);
	}
}
