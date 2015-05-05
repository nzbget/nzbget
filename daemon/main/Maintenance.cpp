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
#ifndef WIN32
#include <unistd.h>
#endif
#include <errno.h>

#include "nzbget.h"
#include "Log.h"
#include "Util.h"
#include "Maintenance.h"
#include "Options.h"

extern Options* g_pOptions;
extern Maintenance* g_pMaintenance;
extern void ExitProc();
extern int g_iArgumentCount;
extern char* (*g_szArguments)[];

Maintenance::Maintenance()
{
	m_iIDMessageGen = 0;
	m_UpdateScriptController = NULL;
	m_szUpdateScript = NULL;
}

Maintenance::~Maintenance()
{
	m_mutexController.Lock();
	if (m_UpdateScriptController)
	{
		m_UpdateScriptController->Detach();
		m_mutexController.Unlock();
		while (m_UpdateScriptController)
		{
			usleep(20*1000);
		}
	}

	m_Messages.Clear();

	free(m_szUpdateScript);
}

void Maintenance::ResetUpdateController()
{
	m_mutexController.Lock();
	m_UpdateScriptController = NULL;
	m_mutexController.Unlock();
}

MessageList* Maintenance::LockMessages()
{
	m_mutexLog.Lock();
	return &m_Messages;
}

void Maintenance::UnlockMessages()
{
	m_mutexLog.Unlock();
}

void Maintenance::AddMessage(Message::EKind eKind, time_t tTime, const char * szText)
{
	if (tTime == 0)
	{
		tTime = time(NULL);
	}

	m_mutexLog.Lock();
	Message* pMessage = new Message(++m_iIDMessageGen, eKind, tTime, szText);
	m_Messages.push_back(pMessage);
	m_mutexLog.Unlock();
}

bool Maintenance::StartUpdate(EBranch eBranch)
{
	m_mutexController.Lock();
	bool bAlreadyUpdating = m_UpdateScriptController != NULL;
	m_mutexController.Unlock();

	if (bAlreadyUpdating)
	{
		error("Could not start update-script: update-script is already running");
		return false;
	}

	if (m_szUpdateScript)
	{
		free(m_szUpdateScript);
		m_szUpdateScript = NULL;
	}

	if (!ReadPackageInfoStr("install-script", &m_szUpdateScript))
	{
		return false;
	}

	// make absolute path
	if (m_szUpdateScript[0] != PATH_SEPARATOR
#ifdef WIN32
		&& !(strlen(m_szUpdateScript) > 2 && m_szUpdateScript[1] == ':')
#endif
		)
	{
		char szFilename[MAX_PATH + 100];
		snprintf(szFilename, sizeof(szFilename), "%s%c%s", g_pOptions->GetAppDir(), PATH_SEPARATOR, m_szUpdateScript);
		free(m_szUpdateScript);
		m_szUpdateScript = strdup(szFilename);
	}

	m_Messages.Clear();

	m_UpdateScriptController = new UpdateScriptController();
	m_UpdateScriptController->SetScript(m_szUpdateScript);
	m_UpdateScriptController->SetBranch(eBranch);
	m_UpdateScriptController->SetAutoDestroy(true);

	m_UpdateScriptController->Start();

	return true;
}

bool Maintenance::CheckUpdates(char** pUpdateInfo)
{
	char* szUpdateInfoScript;
	if (!ReadPackageInfoStr("update-info-script", &szUpdateInfoScript))
	{
		return false;
	}

	*pUpdateInfo = NULL;
	UpdateInfoScriptController::ExecuteScript(szUpdateInfoScript, pUpdateInfo);

	free(szUpdateInfoScript);

	return *pUpdateInfo;
}

bool Maintenance::ReadPackageInfoStr(const char* szKey, char** pValue)
{
	char szFileName[1024];
	snprintf(szFileName, 1024, "%s%cpackage-info.json", g_pOptions->GetWebDir(), PATH_SEPARATOR);
	szFileName[1024-1] = '\0';

	char* szPackageInfo;
	int iPackageInfoLen;
	if (!Util::LoadFileIntoBuffer(szFileName, &szPackageInfo, &iPackageInfoLen))
	{
		error("Could not load file %s", szFileName);
		return false;
	}

	char szKeyStr[100];
	snprintf(szKeyStr, 100, "\"%s\"", szKey);
	szKeyStr[100-1] = '\0';

	char* p = strstr(szPackageInfo, szKeyStr);
	if (!p)
	{
		error("Could not parse file %s", szFileName);
		free(szPackageInfo);
		return false;
	}

	p = strchr(p + strlen(szKeyStr), '"');
	if (!p)
	{
		error("Could not parse file %s", szFileName);
		free(szPackageInfo);
		return false;
	}

	p++;
	char* pend = strchr(p, '"');
	if (!pend)
	{
		error("Could not parse file %s", szFileName);
		free(szPackageInfo);
		return false;
	}

	int iLen = pend - p;
	if (iLen >= sizeof(szFileName))
	{
		error("Could not parse file %s", szFileName);
		free(szPackageInfo);
		return false;
	}

	*pValue = (char*)malloc(iLen+1);
	strncpy(*pValue, p, iLen);
	(*pValue)[iLen] = '\0';

	WebUtil::JsonDecode(*pValue);

	free(szPackageInfo);

	return true;
}

void UpdateScriptController::Run()
{
	// the update-script should not be automatically terminated when the program quits
	UnregisterRunningScript();

	m_iPrefixLen = 0;
	PrintMessage(Message::mkInfo, "Executing update-script %s", GetScript());

	char szInfoName[1024];
	snprintf(szInfoName, 1024, "update-script %s", Util::BaseFileName(GetScript()));
	szInfoName[1024-1] = '\0';
	SetInfoName(szInfoName);

    const char* szBranchName[] = { "STABLE", "TESTING", "DEVEL" };
	SetEnvVar("NZBUP_BRANCH", szBranchName[m_eBranch]);

	SetEnvVar("NZBUP_RUNMODE", g_pOptions->GetDaemonMode() ? "DAEMON" : "SERVER");

	for (int i = 0; i < g_iArgumentCount; i++)
	{
		char szEnvName[40];
		snprintf(szEnvName, 40, "NZBUP_CMDLINE%i", i);
		szInfoName[40-1] = '\0';
		SetEnvVar(szEnvName, (*g_szArguments)[i]);
	}

	char szProcessID[20];
#ifdef WIN32
	int pid = (int)GetCurrentProcessId();
#else
	int pid = (int)getpid();
#endif
	snprintf(szProcessID, 20, "%i", pid);
	szProcessID[20-1] = '\0';
	SetEnvVar("NZBUP_PROCESSID", szProcessID);

	char szLogPrefix[100];
	strncpy(szLogPrefix, Util::BaseFileName(GetScript()), 100);
	szLogPrefix[100-1] = '\0';
	if (char* ext = strrchr(szLogPrefix, '.')) *ext = '\0'; // strip file extension
	SetLogPrefix(szLogPrefix);
	m_iPrefixLen = strlen(szLogPrefix) + 2; // 2 = strlen(": ");

	Execute();

	g_pMaintenance->ResetUpdateController();
}

void UpdateScriptController::AddMessage(Message::EKind eKind, const char* szText)
{
	szText = szText + m_iPrefixLen;

	if (!strncmp(szText, "[NZB] ", 6))
	{
		debug("Command %s detected", szText + 6);
		if (!strcmp(szText + 6, "QUIT"))
		{
			Detach();
			ExitProc();
		}
		else
		{
			error("Invalid command \"%s\" received", szText);
		}
	}
	else
	{
		g_pMaintenance->AddMessage(eKind, time(NULL), szText);
		ScriptController::AddMessage(eKind, szText);
	}
}

void UpdateInfoScriptController::ExecuteScript(const char* szScript, char** pUpdateInfo)
{
	detail("Executing update-info-script %s", Util::BaseFileName(szScript));

	UpdateInfoScriptController* pScriptController = new UpdateInfoScriptController();
	pScriptController->SetScript(szScript);

	char szInfoName[1024];
	snprintf(szInfoName, 1024, "update-info-script %s", Util::BaseFileName(szScript));
	szInfoName[1024-1] = '\0';
	pScriptController->SetInfoName(szInfoName);

	char szLogPrefix[1024];
	strncpy(szLogPrefix, Util::BaseFileName(szScript), 1024);
	szLogPrefix[1024-1] = '\0';
	if (char* ext = strrchr(szLogPrefix, '.')) *ext = '\0'; // strip file extension
	pScriptController->SetLogPrefix(szLogPrefix);
	pScriptController->m_iPrefixLen = strlen(szLogPrefix) + 2; // 2 = strlen(": ");

	pScriptController->Execute();

	if (pScriptController->m_UpdateInfo.GetBuffer())
	{
		int iLen = strlen(pScriptController->m_UpdateInfo.GetBuffer());
		*pUpdateInfo = (char*)malloc(iLen + 1);
		strncpy(*pUpdateInfo, pScriptController->m_UpdateInfo.GetBuffer(), iLen);
		(*pUpdateInfo)[iLen] = '\0';
	}

	delete pScriptController;
}

void UpdateInfoScriptController::AddMessage(Message::EKind eKind, const char* szText)
{
	szText = szText + m_iPrefixLen;

	if (!strncmp(szText, "[NZB] ", 6))
	{
		debug("Command %s detected", szText + 6);
		if (!strncmp(szText + 6, "[UPDATEINFO]", 12))
		{
			m_UpdateInfo.Append(szText + 6 + 12);
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
