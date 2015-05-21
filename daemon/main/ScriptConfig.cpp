/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2013-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#include <set>

#include "nzbget.h"
#include "Util.h"
#include "Options.h"
#include "Log.h"
#include "ScriptConfig.h"

static const char* BEGIN_SCRIPT_SIGNATURE = "### NZBGET ";
static const char* POST_SCRIPT_SIGNATURE = "POST-PROCESSING";
static const char* SCAN_SCRIPT_SIGNATURE = "SCAN";
static const char* QUEUE_SCRIPT_SIGNATURE = "QUEUE";
static const char* SCHEDULER_SCRIPT_SIGNATURE = "SCHEDULER";
static const char* END_SCRIPT_SIGNATURE = " SCRIPT";
static const char* QUEUE_EVENTS_SIGNATURE = "### QUEUE EVENTS:";

ScriptConfig* g_pScriptConfig = NULL;


ScriptConfig::ConfigTemplate::ConfigTemplate(Script* pScript, const char* szTemplate)
{
	m_pScript = pScript;
	m_szTemplate = strdup(szTemplate ? szTemplate : "");
}

ScriptConfig::ConfigTemplate::~ConfigTemplate()
{
	delete m_pScript;
	free(m_szTemplate);
}

ScriptConfig::ConfigTemplates::~ConfigTemplates()
{
	for (iterator it = begin(); it != end(); it++)
	{
		delete *it;
	}
}


ScriptConfig::Script::Script(const char* szName, const char* szLocation)
{
	m_szName = strdup(szName);
	m_szLocation = strdup(szLocation);
	m_szDisplayName = strdup(szName);
	m_bPostScript = false;
	m_bScanScript = false;
	m_bQueueScript = false;
	m_bSchedulerScript = false;
	m_szQueueEvents = NULL;
}

ScriptConfig::Script::~Script()
{
	free(m_szName);
	free(m_szLocation);
	free(m_szDisplayName);
	free(m_szQueueEvents);
}

void ScriptConfig::Script::SetDisplayName(const char* szDisplayName)
{
	free(m_szDisplayName);
	m_szDisplayName = strdup(szDisplayName);
}

void ScriptConfig::Script::SetQueueEvents(const char* szQueueEvents)
{
	free(m_szQueueEvents);
	m_szQueueEvents = szQueueEvents ? strdup(szQueueEvents) : NULL;
}


ScriptConfig::Scripts::~Scripts()
{
	Clear();
}

void ScriptConfig::Scripts::Clear()
{
	for (iterator it = begin(); it != end(); it++)
	{
		delete *it;
	}
	clear();
}

ScriptConfig::Script* ScriptConfig::Scripts::Find(const char* szName)
{
	for (iterator it = begin(); it != end(); it++)
	{
		Script* pScript = *it;
		if (!strcmp(pScript->GetName(), szName))
		{
			return pScript;
		}
	}

	return NULL;
}


ScriptConfig::ScriptConfig()
{
	InitScripts();
	InitConfigTemplates();
}

ScriptConfig::~ScriptConfig()
{
}

bool ScriptConfig::LoadConfig(Options::OptEntries* pOptEntries)
{
	// read config file
	FILE* infile = fopen(g_pOptions->GetConfigFilename(), FOPEN_RB);

	if (!infile)
	{
		return false;
	}

	int iBufLen = (int)Util::FileSize(g_pOptions->GetConfigFilename()) + 1;
	char* buf = (char*)malloc(iBufLen);

	while (fgets(buf, iBufLen - 1, infile))
	{
		// remove trailing '\n' and '\r' and spaces
		Util::TrimRight(buf);

		// skip comments and empty lines
		if (buf[0] == 0 || buf[0] == '#' || strspn(buf, " ") == strlen(buf))
		{
			continue;
		}

		char* optname;
		char* optvalue;
		if (g_pOptions->SplitOptionString(buf, &optname, &optvalue))
		{
			Options::OptEntry* pOptEntry = new Options::OptEntry();
			pOptEntry->SetName(optname);
			pOptEntry->SetValue(optvalue);
			pOptEntries->push_back(pOptEntry);

			free(optname);
			free(optvalue);
		}
	}

	fclose(infile);
	free(buf);

	return true;
}

bool ScriptConfig::SaveConfig(Options::OptEntries* pOptEntries)
{
	// save to config file
	FILE* infile = fopen(g_pOptions->GetConfigFilename(), FOPEN_RBP);

	if (!infile)
	{
		return false;
	}

	std::vector<char*> config;
	std::set<Options::OptEntry*> writtenOptions;

	// read config file into memory array
	int iBufLen = (int)Util::FileSize(g_pOptions->GetConfigFilename()) + 1;
	char* buf = (char*)malloc(iBufLen);
	while (fgets(buf, iBufLen - 1, infile))
	{
		config.push_back(strdup(buf));
	}
	free(buf);

	// write config file back to disk, replace old values of existing options with new values
	rewind(infile);
	for (std::vector<char*>::iterator it = config.begin(); it != config.end(); it++)
    {
        char* buf = *it;

		const char* eq = strchr(buf, '=');
		if (eq && buf[0] != '#')
		{
			// remove trailing '\n' and '\r' and spaces
			Util::TrimRight(buf);

			char* optname;
			char* optvalue;
			if (g_pOptions->SplitOptionString(buf, &optname, &optvalue))
			{
				Options::OptEntry *pOptEntry = pOptEntries->FindOption(optname);
				if (pOptEntry)
				{
					fputs(pOptEntry->GetName(), infile);
					fputs("=", infile);
					fputs(pOptEntry->GetValue(), infile);
					fputs("\n", infile);
					writtenOptions.insert(pOptEntry);
				}

				free(optname);
				free(optvalue);
			}
		}
		else
		{
			fputs(buf, infile);
		}

		free(buf);
	}

	// write new options
	for (Options::OptEntries::iterator it = pOptEntries->begin(); it != pOptEntries->end(); it++)
	{
		Options::OptEntry* pOptEntry = *it;
		std::set<Options::OptEntry*>::iterator fit = writtenOptions.find(pOptEntry);
		if (fit == writtenOptions.end())
		{
			fputs(pOptEntry->GetName(), infile);
			fputs("=", infile);
			fputs(pOptEntry->GetValue(), infile);
			fputs("\n", infile);
		}
	}

	// close and truncate the file
	int pos = (int)ftell(infile);
	fclose(infile);

	Util::TruncateFile(g_pOptions->GetConfigFilename(), pos);

	return true;
}

bool ScriptConfig::LoadConfigTemplates(ConfigTemplates* pConfigTemplates)
{
	char* szBuffer;
	int iLength;
	if (!Util::LoadFileIntoBuffer(g_pOptions->GetConfigTemplate(), &szBuffer, &iLength))
	{
		return false;
	}
	ConfigTemplate* pConfigTemplate = new ConfigTemplate(NULL, szBuffer);
	pConfigTemplates->push_back(pConfigTemplate);
	free(szBuffer);

	if (!g_pOptions->GetScriptDir())
	{
		return true;
	}

	Scripts scriptList;
	LoadScripts(&scriptList);

	const int iBeginSignatureLen = strlen(BEGIN_SCRIPT_SIGNATURE);
	const int iQueueEventsSignatureLen = strlen(QUEUE_EVENTS_SIGNATURE);

	for (Scripts::iterator it = scriptList.begin(); it != scriptList.end(); it++)
	{
		Script* pScript = *it;

		FILE* infile = fopen(pScript->GetLocation(), FOPEN_RB);
		if (!infile)
		{
			ConfigTemplate* pConfigTemplate = new ConfigTemplate(pScript, "");
			pConfigTemplates->push_back(pConfigTemplate);
			continue;
		}

		StringBuilder stringBuilder;
		char buf[1024];
		bool bInConfig = false;

		while (fgets(buf, sizeof(buf) - 1, infile))
		{
			if (!strncmp(buf, BEGIN_SCRIPT_SIGNATURE, iBeginSignatureLen) &&
				strstr(buf, END_SCRIPT_SIGNATURE) &&
				(strstr(buf, POST_SCRIPT_SIGNATURE) ||
				 strstr(buf, SCAN_SCRIPT_SIGNATURE) ||
				 strstr(buf, QUEUE_SCRIPT_SIGNATURE) ||
				 strstr(buf, SCHEDULER_SCRIPT_SIGNATURE)))
			{
				if (bInConfig)
				{
					break;
				}
				bInConfig = true;
				continue;
			}

			bool bSkip = !strncmp(buf, QUEUE_EVENTS_SIGNATURE, iQueueEventsSignatureLen);

			if (bInConfig && !bSkip)
			{
				stringBuilder.Append(buf);
			}
		}

		fclose(infile);

		ConfigTemplate* pConfigTemplate = new ConfigTemplate(pScript, stringBuilder.GetBuffer());
		pConfigTemplates->push_back(pConfigTemplate);
	}

	// clearing the list without deleting of objects, which are in pConfigTemplates now 
	scriptList.clear();

	return true;
}

void ScriptConfig::InitConfigTemplates()
{
	if (!LoadConfigTemplates(&m_ConfigTemplates))
	{
		error("Could not read configuration templates");
	}
}

void ScriptConfig::InitScripts()
{
	LoadScripts(&m_Scripts);
}

void ScriptConfig::LoadScripts(Scripts* pScripts)
{
	if (strlen(g_pOptions->GetScriptDir()) == 0)
	{
		return;
	}

	Scripts tmpScripts;
	LoadScriptDir(&tmpScripts, g_pOptions->GetScriptDir(), false);
	tmpScripts.sort(CompareScripts);

	// first add all scripts from m_szScriptOrder
	Tokenizer tok(g_pOptions->GetScriptOrder(), ",;");
	while (const char* szScriptName = tok.Next())
	{
		Script* pScript = tmpScripts.Find(szScriptName);
		if (pScript)
		{
			tmpScripts.remove(pScript);
			pScripts->push_back(pScript);
		}
	}

	// second add all other scripts from scripts directory
	for (Scripts::iterator it = tmpScripts.begin(); it != tmpScripts.end(); it++)
	{
		Script* pScript = *it;
		if (!pScripts->Find(pScript->GetName()))
		{
			pScripts->push_back(pScript);
		}
	}

	tmpScripts.clear();

	BuildScriptDisplayNames(pScripts);
}

void ScriptConfig::LoadScriptDir(Scripts* pScripts, const char* szDirectory, bool bIsSubDir)
{
	int iBufSize = 1024*10;
	char* szBuffer = (char*)malloc(iBufSize+1);

	const int iBeginSignatureLen = strlen(BEGIN_SCRIPT_SIGNATURE);
	const int iQueueEventsSignatureLen = strlen(QUEUE_EVENTS_SIGNATURE);

	DirBrowser dir(szDirectory);
	while (const char* szFilename = dir.Next())
	{
		if (szFilename[0] != '.' && szFilename[0] != '_')
		{
			char szFullFilename[1024];
			snprintf(szFullFilename, 1024, "%s%s", szDirectory, szFilename);
			szFullFilename[1024-1] = '\0';

			if (!Util::DirectoryExists(szFullFilename))
			{
				// check if the file contains pp-script-signature
				FILE* infile = fopen(szFullFilename, FOPEN_RB);
				if (infile)
				{
					// read first 10KB of the file and look for signature
					int iReadBytes = fread(szBuffer, 1, iBufSize, infile);
					fclose(infile);
					szBuffer[iReadBytes] = 0;

					// split buffer into lines
					Tokenizer tok(szBuffer, "\n\r", true);
					while (char* szLine = tok.Next())
					{
						if (!strncmp(szLine, BEGIN_SCRIPT_SIGNATURE, iBeginSignatureLen) &&
							strstr(szLine, END_SCRIPT_SIGNATURE))
						{
							bool bPostScript = strstr(szLine, POST_SCRIPT_SIGNATURE);
							bool bScanScript = strstr(szLine, SCAN_SCRIPT_SIGNATURE);
							bool bQueueScript = strstr(szLine, QUEUE_SCRIPT_SIGNATURE);
							bool bSchedulerScript = strstr(szLine, SCHEDULER_SCRIPT_SIGNATURE);
							if (bPostScript || bScanScript || bQueueScript || bSchedulerScript)
							{
								char szScriptName[1024];
								if (bIsSubDir)
								{
									char szDirectory2[1024];
									snprintf(szDirectory2, 1024, "%s", szDirectory);
									szDirectory2[1024-1] = '\0';
									int iLen = strlen(szDirectory2);
									if (szDirectory2[iLen-1] == PATH_SEPARATOR || szDirectory2[iLen-1] == ALT_PATH_SEPARATOR)
									{
										// trim last path-separator
										szDirectory2[iLen-1] = '\0';
									}

									snprintf(szScriptName, 1024, "%s%c%s", Util::BaseFileName(szDirectory2), PATH_SEPARATOR, szFilename);
								}
								else
								{
									snprintf(szScriptName, 1024, "%s", szFilename);
								}
								szScriptName[1024-1] = '\0';

								char* szQueueEvents = NULL;
								if (bQueueScript)
								{
									while (char* szLine = tok.Next())
									{
										if (!strncmp(szLine, QUEUE_EVENTS_SIGNATURE, iQueueEventsSignatureLen))
										{
											szQueueEvents = szLine + iQueueEventsSignatureLen;
											break;
										}
									}
								}

								Script* pScript = new Script(szScriptName, szFullFilename);
								pScript->SetPostScript(bPostScript);
								pScript->SetScanScript(bScanScript);
								pScript->SetQueueScript(bQueueScript);
								pScript->SetSchedulerScript(bSchedulerScript);
								pScript->SetQueueEvents(szQueueEvents);
								pScripts->push_back(pScript);
								break;
							}
						}
					}
				}
			}
			else if (!bIsSubDir)
			{
				snprintf(szFullFilename, 1024, "%s%s%c", szDirectory, szFilename, PATH_SEPARATOR);
				szFullFilename[1024-1] = '\0';

				LoadScriptDir(pScripts, szFullFilename, true);
			}
		}
	}

	free(szBuffer);
}

bool ScriptConfig::CompareScripts(Script* pScript1, Script* pScript2)
{
	return strcmp(pScript1->GetName(), pScript2->GetName()) < 0;
}

void ScriptConfig::BuildScriptDisplayNames(Scripts* pScripts)
{
	// trying to use short name without path and extension.
	// if there are other scripts with the same short name - using a longer name instead (with ot without extension)

	for (Scripts::iterator it = pScripts->begin(); it != pScripts->end(); it++)
	{
		Script* pScript = *it;

		char szShortName[256];
		strncpy(szShortName, pScript->GetName(), 256);
		szShortName[256-1] = '\0';
		if (char* ext = strrchr(szShortName, '.')) *ext = '\0'; // strip file extension

		const char* szDisplayName = Util::BaseFileName(szShortName);

		for (Scripts::iterator it2 = pScripts->begin(); it2 != pScripts->end(); it2++)
		{
			Script* pScript2 = *it2;

			char szShortName2[256];
			strncpy(szShortName2, pScript2->GetName(), 256);
			szShortName2[256-1] = '\0';
			if (char* ext = strrchr(szShortName2, '.')) *ext = '\0'; // strip file extension

			const char* szDisplayName2 = Util::BaseFileName(szShortName2);

			if (!strcmp(szDisplayName, szDisplayName2) && pScript->GetName() != pScript2->GetName())
			{
				if (!strcmp(szShortName, szShortName2))
				{
					szDisplayName =	pScript->GetName();
				}
				else
				{
					szDisplayName =	szShortName;
				}
				break;
			}
		}

		pScript->SetDisplayName(szDisplayName);
	}
}
