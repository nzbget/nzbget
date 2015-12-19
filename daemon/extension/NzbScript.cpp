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
void NzbScriptController::PrepareEnvParameters(NzbParameterList* parameters, const char* stripPrefix)
{
	int prefixLen = stripPrefix ? strlen(stripPrefix) : 0;

	for (NzbParameterList::iterator it = parameters->begin(); it != parameters->end(); it++)
	{
		NzbParameter* parameter = *it;
		const char* value = parameter->GetValue();

#ifdef WIN32
		char* ansiValue = strdup(value);
		WebUtil::Utf8ToAnsi(ansiValue, strlen(ansiValue) + 1);
		value = ansiValue;
#endif

		if (stripPrefix && !strncmp(parameter->GetName(), stripPrefix, prefixLen) && (int)strlen(parameter->GetName()) > prefixLen)
		{
			SetEnvVarSpecial("NZBPR", parameter->GetName() + prefixLen, value);
		}
		else if (!stripPrefix)
		{
			SetEnvVarSpecial("NZBPR", parameter->GetName(), value);
		}

#ifdef WIN32
		free(ansiValue);
#endif
	}
}

void NzbScriptController::PrepareEnvScript(NzbParameterList* parameters, const char* scriptName)
{
	if (parameters)
	{
		PrepareEnvParameters(parameters, NULL);
	}

	BString<1024> paramPrefix("%s:", scriptName);

	if (parameters)
	{
		PrepareEnvParameters(parameters, paramPrefix);
	}

	PrepareEnvOptions(paramPrefix);
}

void NzbScriptController::ExecuteScriptList(const char* scriptList)
{
	for (ScriptConfig::Scripts::iterator it = g_ScriptConfig->GetScripts()->begin(); it != g_ScriptConfig->GetScripts()->end(); it++)
	{
		ScriptConfig::Script* script = *it;

		if (scriptList && *scriptList)
		{
			// split szScriptList into tokens
			Tokenizer tok(scriptList, ",;");
			while (const char* scriptName = tok.Next())
			{
				if (Util::SameFilename(scriptName, script->GetName()))
				{
					ExecuteScript(script);
					break;
				}
			}
		}
	}
}
