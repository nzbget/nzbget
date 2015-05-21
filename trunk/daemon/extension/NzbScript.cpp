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
#include <algorithm>

#include "nzbget.h"
#include "NzbScript.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"

		 
/**
 * If szStripPrefix is not NULL, only pp-parameters, whose names start with the prefix
 * are processed. The prefix is then stripped from the names.
 * If szStripPrefix is NULL, all pp-parameters are processed; without stripping.
 */
void NZBScriptController::PrepareEnvParameters(NZBParameterList* pParameters, const char* szStripPrefix)
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

void NZBScriptController::PrepareEnvScript(NZBParameterList* pParameters, const char* szScriptName)
{
	if (pParameters)
	{
		PrepareEnvParameters(pParameters, NULL);
	}

	char szParamPrefix[1024];
	snprintf(szParamPrefix, 1024, "%s:", szScriptName);
	szParamPrefix[1024-1] = '\0';

	if (pParameters)
	{
		PrepareEnvParameters(pParameters, szParamPrefix);
	}

	PrepareEnvOptions(szParamPrefix);
}

void NZBScriptController::ExecuteScriptList(const char* szScriptList)
{
	for (ScriptConfig::Scripts::iterator it = g_pScriptConfig->GetScripts()->begin(); it != g_pScriptConfig->GetScripts()->end(); it++)
	{
		ScriptConfig::Script* pScript = *it;

		if (szScriptList && *szScriptList)
		{
			// split szScriptList into tokens
			Tokenizer tok(szScriptList, ",;");
			while (const char* szScriptName = tok.Next())
			{
				if (Util::SameFilename(szScriptName, pScript->GetName()))
				{
					ExecuteScript(pScript);
					break;
				}
			}
		}
	}
}
