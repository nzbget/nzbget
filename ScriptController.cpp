/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2008 Andrei Prygounkov <hugbug@users.sourceforge.net>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Revision$
 * $Date$
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifndef WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#endif
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#include "nzbget.h"
#include "ScriptController.h"
#include "Log.h"
#include "Util.h"

extern Options* g_pOptions;

ScriptController::ScriptController()
{
	m_szScript = NULL;
	m_szWorkingDir = NULL;
	m_szArgs = NULL;
	m_szInfoName = NULL;
	m_szDefaultKindPrefix = NULL;
	m_bTerminated = false;
}

void ScriptController::Execute()
{
	if (!Util::FileExists(m_szScript))
	{
		error("Could not start %s: could not find file %s", m_szInfoName, m_szScript);
		return;
	}

	int pipein;

#ifdef WIN32
	// build command line
	char szCmdLine[2048];
	int iUsedLen = 0;
	for (const char** szArgPtr = m_szArgs; *szArgPtr; szArgPtr++)
	{
		snprintf(szCmdLine + iUsedLen, 2048 - iUsedLen, "\"%s\" ", *szArgPtr);
		iUsedLen += strlen(*szArgPtr) + 3;
	}
	szCmdLine[iUsedLen < 2048 ? iUsedLen - 1 : 2048 - 1] = '\0';
	
	// create pipes to write and read data
	HANDLE hReadPipe, hWritePipe;
	SECURITY_ATTRIBUTES SecurityAttributes;
	memset(&SecurityAttributes, 0, sizeof(SecurityAttributes));
	SecurityAttributes.nLength = sizeof(SecurityAttributes);
	SecurityAttributes.bInheritHandle = TRUE;

	CreatePipe(&hReadPipe, &hWritePipe, &SecurityAttributes, 0);

	STARTUPINFO StartupInfo;
	memset(&StartupInfo, 0, sizeof(StartupInfo));
	StartupInfo.cb = sizeof(StartupInfo);
	StartupInfo.dwFlags = STARTF_USESTDHANDLES;
	StartupInfo.hStdInput = 0;
	StartupInfo.hStdOutput = hWritePipe;
	StartupInfo.hStdError = hWritePipe;

	PROCESS_INFORMATION ProcessInfo;
	memset(&ProcessInfo, 0, sizeof(ProcessInfo));

	BOOL bOK = CreateProcess(NULL, szCmdLine, NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW, NULL, m_szWorkingDir, &StartupInfo, &ProcessInfo);
	if (!bOK)
	{
		DWORD dwErrCode = GetLastError();
		char szErrMsg[255];
		szErrMsg[255-1] = '\0';
		if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM || FORMAT_MESSAGE_IGNORE_INSERTS || FORMAT_MESSAGE_ARGUMENT_ARRAY, 
			NULL, dwErrCode, 0, szErrMsg, 255, NULL))
		{
			error("Could not start %s: %s", m_szInfoName, szErrMsg);
		}
		else
		{
			error("Could not start %s: error %i", m_szInfoName, dwErrCode);
		}
		return;
	}

	debug("Child Process-ID: %i", (int)ProcessInfo.dwProcessId);

	m_hProcess = ProcessInfo.hProcess;

	// close unused "write" end
	CloseHandle(hWritePipe);

	pipein = _open_osfhandle((intptr_t)hReadPipe, _O_RDONLY);

#else

	int p[2];
	int pipeout;

	// create the pipe
	if (pipe(p))
	{
		error("Could not open pipe: errno %i", errno);
		return;
	}

	pipein = p[0];
	pipeout = p[1];

	debug("forking");
	pid_t pid = fork();

	if (pid == -1)
	{
		error("Could not start %s: errno %i", m_szInfoName, errno);
		return;
	}
	else if (pid == 0)
	{
		// here goes the second instance
			
		// close up the "read" end
		close(pipein);
      			
		// make the pipeout to be the same as stdout and stderr
		dup2(pipeout, 1);
		dup2(pipeout, 2);
		
		close(pipeout);

		execvp(m_szScript, (char* const*)m_szArgs);
		fprintf(stdout, "[ERROR] Could not start script: %s", strerror(errno));
		fflush(stdout);
		_exit(-1);
	}

	// continue the first instance
	debug("forked");
	debug("Child Process-ID: %i", (int)pid);

	m_hProcess = pid;

	// close unused "write" end
	close(pipeout);
#endif

	// open the read end
	FILE* readpipe = fdopen(pipein, "r");
	if (!readpipe)
	{
		error("Could not open pipe to %s", m_szInfoName);
		return;
	}
	
	char* buf = (char*)malloc(10240);

	debug("Entering pipe-loop");
	while (!feof(readpipe) && !m_bTerminated)
	{
		if (fgets(buf, 10240, readpipe))
		{
			ProcessOutput(buf);
		}
	}
	debug("Exited pipe-loop");
	
	free(buf);
	fclose(readpipe);

	if (m_bTerminated)
	{
		warn("Interrupted %s", m_szInfoName);
	}

#ifdef WIN32
	WaitForSingleObject(m_hProcess, INFINITE);
#else
	waitpid(m_hProcess, NULL, 0);
#endif

	if (!m_bTerminated)
	{
		info("Completed %s", m_szInfoName);
	}
}

void ScriptController::Terminate()
{
	debug("Stopping %s", m_szInfoName);
	m_bTerminated = true;

#ifdef WIN32
	BOOL bOK = TerminateProcess(m_hProcess, -1);
#else
	bool bOK = kill(m_hProcess, 9) == 0;
#endif

	if (bOK)
	{
		debug("Terminated %s", m_szInfoName);
	}
	else
	{
		error("Could not terminate %s", m_szInfoName);
	}

	debug("Stopped %s", m_szInfoName);
}

void ScriptController::ProcessOutput(char* szText)
{
	debug("Processing output received from script");

	for (char* pend = szText + strlen(szText) - 1; pend >= szText && (*pend == '\n' || *pend == '\r' || *pend == ' '); pend--) *pend = '\0';

	if (strlen(szText) == 0)
	{
		// skip empty lines
		return;
	}

	if (!strncmp(szText, "[INFO] ", 7))
	{
		AddMessage(Message::mkInfo, false, g_pOptions->GetInfoTarget(), szText + 7);
	}
	else if (!strncmp(szText, "[WARNING] ", 10))
	{
		AddMessage(Message::mkWarning, false, g_pOptions->GetWarningTarget(), szText + 10);
	}
	else if (!strncmp(szText, "[ERROR] ", 8))
	{
		AddMessage(Message::mkError, false, g_pOptions->GetErrorTarget(), szText + 8);
	}
	else if (!strncmp(szText, "[DETAIL] ", 9))
	{
		AddMessage(Message::mkDetail, false, g_pOptions->GetDetailTarget(), szText + 9);
	}
	else if (!strncmp(szText, "[DEBUG] ", 8))
	{
		AddMessage(Message::mkDebug, false, g_pOptions->GetDebugTarget(), szText + 8);
	}
	else 
	{	
		switch (m_eDefaultLogKind)
		{
			case Options::slNone:
				break;

			case Options::slDetail:
				AddMessage(Message::mkDetail, true, g_pOptions->GetDetailTarget(), szText);
				break;

			case Options::slInfo:
				AddMessage(Message::mkInfo, true, g_pOptions->GetInfoTarget(), szText);
				break;

			case Options::slWarning:
				AddMessage(Message::mkWarning, true, g_pOptions->GetWarningTarget(), szText);
				break;

			case Options::slError:
				AddMessage(Message::mkError, true, g_pOptions->GetErrorTarget(), szText);
				break;

			case Options::slDebug:
				AddMessage(Message::mkDebug, true, g_pOptions->GetDebugTarget(), szText);
				break;
		}
	}

	debug("Processing output received from script - completed");
}

void ScriptController::AddMessage(Message::EKind eKind, bool bDefaultKind, Options::EMessageTarget eMessageTarget, const char* szText)
{
	switch (eKind)
	{
		case Message::mkDetail:
			detail("%s%s", (bDefaultKind && m_szDefaultKindPrefix ? m_szDefaultKindPrefix : ""), szText);
			break;

		case Message::mkInfo:
			info("%s%s", (bDefaultKind && m_szDefaultKindPrefix ? m_szDefaultKindPrefix : ""), szText);
			break;

		case Message::mkWarning:
			warn("%s%s", (bDefaultKind && m_szDefaultKindPrefix ? m_szDefaultKindPrefix : ""), szText);
			break;

		case Message::mkError:
			error("%s%s", (bDefaultKind && m_szDefaultKindPrefix ? m_szDefaultKindPrefix : ""), szText);
			break;

		case Message::mkDebug:
			debug("%s%s", (bDefaultKind && m_szDefaultKindPrefix ? m_szDefaultKindPrefix : ""), szText);
			break;
	}
}

void PostScriptController::StartScriptJob(PostInfo* pPostInfo, const char* szScript, bool bNZBFileCompleted, bool bHasFailedParJobs)
{
	info("Executing post-process-script for %s", pPostInfo->GetInfoName());

	PostScriptController* pScriptController = new PostScriptController();
	pScriptController->m_pPostInfo = pPostInfo;
	pScriptController->SetScript(szScript);
	pScriptController->SetWorkingDir(g_pOptions->GetDestDir());
	pScriptController->m_bNZBFileCompleted = bNZBFileCompleted;
	pScriptController->m_bHasFailedParJobs = bHasFailedParJobs;
	pScriptController->SetAutoDestroy(false);

	pPostInfo->SetScriptThread(pScriptController);

	pScriptController->Start();
}

void PostScriptController::Run()
{
	char szParStatus[10];
	snprintf(szParStatus, 10, "%i", m_pPostInfo->GetParStatus());
	szParStatus[10-1] = '\0';

	char szCollectionCompleted[10];
	snprintf(szCollectionCompleted, 10, "%i", (int)m_bNZBFileCompleted);
	szCollectionCompleted[10-1] = '\0';

	char szHasFailedParJobs[10];
	snprintf(szHasFailedParJobs, 10, "%i", (int)m_bHasFailedParJobs);
	szHasFailedParJobs[10-1] = '\0';

	char szDestDir[1024];
	strncpy(szDestDir, m_pPostInfo->GetDestDir(), 1024);
	szDestDir[1024-1] = '\0';
	
	char szNZBFilename[1024];
	strncpy(szNZBFilename, m_pPostInfo->GetNZBFilename(), 1024);
	szNZBFilename[1024-1] = '\0';
	
	char szParFilename[1024];
	strncpy(szParFilename, m_pPostInfo->GetParFilename(), 1024);
	szParFilename[1024-1] = '\0';

	char szInfoName[1024];
	snprintf(szInfoName, 1024, "post-process-script for %s", m_pPostInfo->GetInfoName());
	szInfoName[1024-1] = '\0';
	SetInfoName(szInfoName);

	SetDefaultKindPrefix("Post-Process: ");
	SetDefaultLogKind(g_pOptions->GetPostLogKind());

	const char* szArgs[9];
	szArgs[0] = GetScript();
	szArgs[1] = m_pPostInfo->GetDestDir();
	szArgs[2] = szNZBFilename;
	szArgs[3] = szParFilename;
	szArgs[4] = szParStatus;
	szArgs[5] = szCollectionCompleted;
	szArgs[6] = szHasFailedParJobs;
	szArgs[7] = m_pPostInfo->GetCategory();
	szArgs[8] = NULL;
	SetArgs(szArgs);

	Execute();

	m_pPostInfo->SetStage(PostInfo::ptFinished);
	m_pPostInfo->SetWorking(false);
}

void PostScriptController::AddMessage(Message::EKind eKind, bool bDefaultKind, Options::EMessageTarget eMessageTarget, const char* szText)
{
	ScriptController::AddMessage(eKind, bDefaultKind, eMessageTarget, szText);

	if (eMessageTarget == Options::mtScreen || eMessageTarget == Options::mtBoth)
	{
		m_pPostInfo->AppendMessage(eKind, szText);
	}
}

void PostScriptController::Stop()
{
	debug("Stopping post-process-script");
	Thread::Stop();
	Terminate();
}

void NZBScriptController::ExecuteScript(const char* szScript, const char* szNZBFilename, const char* szDirectory)
{
	info("Executing nzb-process-script for %s", Util::BaseFileName(szNZBFilename));

	NZBScriptController* pScriptController = new NZBScriptController();
	pScriptController->SetScript(szScript);

	char szInfoName[1024];
	snprintf(szInfoName, 1024, "nzb-process-script for %s", Util::BaseFileName(szNZBFilename));
	szInfoName[1024-1] = '\0';
	pScriptController->SetInfoName(szInfoName);

	pScriptController->SetDefaultKindPrefix("NZB-Process: ");
	pScriptController->SetDefaultLogKind(g_pOptions->GetNZBLogKind());

	const char* szArgs[4];
	szArgs[0] = szScript;
	szArgs[1] = szDirectory;
	szArgs[2] = szNZBFilename;
	szArgs[3] = NULL;
	pScriptController->SetArgs(szArgs);

	pScriptController->Execute();

	delete pScriptController;
}
