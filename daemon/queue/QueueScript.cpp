/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "QueueScript.h"
#include "Log.h"
#include "Util.h"

/**
 * If szStripPrefix is not NULL, only pp-parameters, whose names start with the prefix
 * are processed. The prefix is then stripped from the names.
 * If szStripPrefix is NULL, all pp-parameters are processed; without stripping.
 */
void QueueScriptController::PrepareEnvParameters(NZBParameterList* pParameters, const char* szStripPrefix)
{
	int iPrefixLen = szStripPrefix ? strlen(szStripPrefix) : 0;

	for (NZBParameterList::iterator it = pParameters->begin(); it != pParameters->end(); it++)
	{
		NZBParameter* pParameter = *it;
		const char* szValue = pParameter->GetValue();
		
#ifdef WIN32
		char* szAnsiValue = strdup(szValue);
		WebUtil::Utf8ToAnsi(szAnsiValue, strlen(szAnsiValue) + 1);
		szValue = szAnsiValue;
#endif

		if (szStripPrefix && !strncmp(pParameter->GetName(), szStripPrefix, iPrefixLen) && (int)strlen(pParameter->GetName()) > iPrefixLen)
		{
			SetEnvVarSpecial("NZBPR", pParameter->GetName() + iPrefixLen, szValue);
		}
		else if (!szStripPrefix)
		{
			SetEnvVarSpecial("NZBPR", pParameter->GetName(), szValue);
		}

#ifdef WIN32
		free(szAnsiValue);
#endif
	}
}


void NZBScriptController::ExecuteScript(const char* szScript, const char* szNZBFilename, const char* szDirectory,
	char** pNZBName, char** pCategory, int* iPriority, NZBParameterList* pParameters, bool* bAddTop, bool* bAddPaused)
{
	info("Executing nzb-process-script for %s", Util::BaseFileName(szNZBFilename));

	NZBScriptController* pScriptController = new NZBScriptController();
	pScriptController->SetScript(szScript);
	pScriptController->m_pNZBName = pNZBName;
	pScriptController->m_pCategory = pCategory;
	pScriptController->m_pParameters = pParameters;
	pScriptController->m_iPriority = iPriority;
	pScriptController->m_bAddTop = bAddTop;
	pScriptController->m_bAddPaused = bAddPaused;

	char szInfoName[1024];
	snprintf(szInfoName, 1024, "nzb-process-script for %s", Util::BaseFileName(szNZBFilename));
	szInfoName[1024-1] = '\0';
	pScriptController->SetInfoName(szInfoName);

	pScriptController->SetEnvVar("NZBNP_FILENAME", szNZBFilename);
	pScriptController->SetEnvVar("NZBNP_NZBNAME", strlen(*pNZBName) > 0 ? *pNZBName : Util::BaseFileName(szNZBFilename));
	pScriptController->SetEnvVar("NZBNP_CATEGORY", *pCategory);
	pScriptController->SetIntEnvVar("NZBNP_PRIORITY", *iPriority);
	pScriptController->SetIntEnvVar("NZBNP_TOP", *bAddTop ? 1 : 0);
	pScriptController->SetIntEnvVar("NZBNP_PAUSED", *bAddPaused ? 1 : 0);

	// remove trailing slash
	char szDir[1024];
	strncpy(szDir, szDirectory, 1024);
	szDir[1024-1] = '\0';
	int iLen = strlen(szDir);
	if (szDir[iLen-1] == PATH_SEPARATOR)
	{
		szDir[iLen-1] = '\0';
	}
	pScriptController->SetEnvVar("NZBNP_DIRECTORY", szDir);

	pScriptController->PrepareEnvParameters(pParameters, NULL);

	char szLogPrefix[1024];
	strncpy(szLogPrefix, Util::BaseFileName(szScript), 1024);
	szLogPrefix[1024-1] = '\0';
	if (char* ext = strrchr(szLogPrefix, '.')) *ext = '\0'; // strip file extension
	pScriptController->SetLogPrefix(szLogPrefix);
	pScriptController->m_iPrefixLen = strlen(szLogPrefix) + 2; // 2 = strlen(": ");

	pScriptController->Execute();

	delete pScriptController;
}

void NZBScriptController::AddMessage(Message::EKind eKind, const char* szText)
{
	szText = szText + m_iPrefixLen;

	if (!strncmp(szText, "[NZB] ", 6))
	{
		debug("Command %s detected", szText + 6);
		if (!strncmp(szText + 6, "NZBNAME=", 8))
		{
			free(*m_pNZBName);
			*m_pNZBName = strdup(szText + 6 + 8);
		}
		else if (!strncmp(szText + 6, "CATEGORY=", 9))
		{
			free(*m_pCategory);
			*m_pCategory = strdup(szText + 6 + 9);
		}
		else if (!strncmp(szText + 6, "NZBPR_", 6))
		{
			char* szParam = strdup(szText + 6 + 6);
			char* szValue = strchr(szParam, '=');
			if (szValue)
			{
				*szValue = '\0';
				m_pParameters->SetParameter(szParam, szValue + 1);
			}
			else
			{
				error("Invalid command \"%s\" received from %s", szText, GetInfoName());
			}
			free(szParam);
		}
		else if (!strncmp(szText + 6, "PRIORITY=", 9))
		{
			*m_iPriority = atoi(szText + 6 + 9);
		}
		else if (!strncmp(szText + 6, "TOP=", 4))
		{
			*m_bAddTop = atoi(szText + 6 + 4) != 0;
		}
		else if (!strncmp(szText + 6, "PAUSED=", 7))
		{
			*m_bAddPaused = atoi(szText + 6 + 7) != 0;
		}
		else
		{
			error("Invalid command \"%s\" received from %s", szText, GetInfoName());
		}
	}
	else
	{
		ScriptController::AddMessage(eKind, szText);
	}
}


void NZBAddedScriptController::StartScript(DownloadQueue* pDownloadQueue, NZBInfo *pNZBInfo, const char* szScript)
{
	NZBAddedScriptController* pScriptController = new NZBAddedScriptController();
	pScriptController->SetScript(szScript);
	pScriptController->m_szNZBName = strdup(pNZBInfo->GetName());
	pScriptController->SetEnvVar("NZBNA_NZBNAME", pNZBInfo->GetName());
	// "NZBNA_NAME" is not correct but kept for compatibility with older versions where this name was used by mistake
	pScriptController->SetEnvVar("NZBNA_NAME", pNZBInfo->GetName());
	pScriptController->SetIntEnvVar("NZBPP_NZBID", pNZBInfo->GetID());
	pScriptController->SetEnvVar("NZBNA_FILENAME", pNZBInfo->GetFilename());
	pScriptController->SetEnvVar("NZBNA_CATEGORY", pNZBInfo->GetCategory());
	pScriptController->SetIntEnvVar("NZBNA_LASTID", pNZBInfo->GetID());
	pScriptController->SetIntEnvVar("NZBNA_PRIORITY", pNZBInfo->GetPriority());

	pScriptController->PrepareEnvParameters(pNZBInfo->GetParameters(), NULL);

	pScriptController->SetAutoDestroy(true);

	pScriptController->Start();
}

void NZBAddedScriptController::Run()
{
	char szInfoName[1024];
	snprintf(szInfoName, 1024, "nzb-added process-script for %s", m_szNZBName);
	szInfoName[1024-1] = '\0';
	SetInfoName(szInfoName);

	info("Executing %s", szInfoName);

	char szLogPrefix[1024];
	strncpy(szLogPrefix, Util::BaseFileName(GetScript()), 1024);
	szLogPrefix[1024-1] = '\0';
	if (char* ext = strrchr(szLogPrefix, '.')) *ext = '\0'; // strip file extension
	SetLogPrefix(szLogPrefix);

	Execute();

	free(m_szNZBName);
}
