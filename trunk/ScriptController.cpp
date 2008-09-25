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
#include "Options.h"

extern Options* g_pOptions;

void ScriptController::StartScriptJob(PostInfo* pPostInfo, const char* szScript, bool bNZBFileCompleted, bool bHasFailedParJobs)
{
	info("Executing post-process-script for %s", pPostInfo->GetInfoName());

	ScriptController* pScriptController = new ScriptController();
	pScriptController->m_pPostInfo = pPostInfo;
	pScriptController->m_szScript = szScript;
	pScriptController->m_bNZBFileCompleted = bNZBFileCompleted;
	pScriptController->m_bHasFailedParJobs = bHasFailedParJobs;
	pScriptController->SetAutoDestroy(false);

	pPostInfo->SetScriptThread(pScriptController);

	pScriptController->Start();
}

void ScriptController::Run()
{
	if (!Util::FileExists(m_szScript))
	{
		error("Could not start post-process-script: could not find file %s", m_szScript);
		Finished();
		return;
	}

	char szParStatus[10];
	snprintf(szParStatus, 10, "%i", m_pPostInfo->GetParStatus());
	szParStatus[10-1] = '\0';

	char szCollectionCompleted[10];
	snprintf(szCollectionCompleted, 10, "%i", (int)m_bNZBFileCompleted);
	szCollectionCompleted[10-1] = '\0';

	char szHasFailedParJobs[10];
	snprintf(szHasFailedParJobs, 10, "%i", (int)m_bHasFailedParJobs);
	szHasFailedParJobs[10-1] = '\0';

	int pipein;

#ifdef WIN32
	char szCmdLine[2048];
	snprintf(szCmdLine, 2048, "\"%s\" \"%s\" \"%s\" \"%s\" %s %s %s \"%s\"", m_szScript, m_pPostInfo->GetDestDir(), 
		m_pPostInfo->GetNZBFilename(), m_pPostInfo->GetParFilename(), szParStatus, szCollectionCompleted, 
		szHasFailedParJobs, m_pPostInfo->GetCategory());
	szCmdLine[2048-1] = '\0';
	
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

	BOOL bOK = CreateProcess(NULL, szCmdLine, NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW, NULL, m_pPostInfo->GetDestDir(), &StartupInfo, &ProcessInfo);
	if (!bOK)
	{
		DWORD dwErrCode = GetLastError();
		char szErrMsg[255];
		szErrMsg[255-1] = '\0';
		if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM || FORMAT_MESSAGE_IGNORE_INSERTS || FORMAT_MESSAGE_ARGUMENT_ARRAY, 
			NULL, dwErrCode, 0, szErrMsg, 255, NULL))
		{
			error("Could not start post-process-script: %s", szErrMsg);
		}
		else
		{
			error("Could not start post-process-script: error %i", dwErrCode);
		}
		Finished();
		return;
	}

	debug("Child Process-ID: %i", (int)ProcessInfo.dwProcessId);

	m_hProcess = ProcessInfo.hProcess;

	// close unused "write" end
	CloseHandle(hWritePipe);

	pipein = _open_osfhandle((intptr_t)hReadPipe, _O_RDONLY);

#else

	char szDestDir[1024];
	strncpy(szDestDir, m_pPostInfo->GetDestDir(), 1024);
	szDestDir[1024-1] = '\0';
	
	char szNZBFilename[1024];
	strncpy(szNZBFilename, m_pPostInfo->GetNZBFilename(), 1024);
	szNZBFilename[1024-1] = '\0';
	
	char szParFilename[1024];
	strncpy(szParFilename, m_pPostInfo->GetParFilename(), 1024);
	szParFilename[1024-1] = '\0';

	int p[2];
	int pipeout;

	// create the pipe
	if (pipe(p))
	{
		error("Could not open pipe: errno %i", errno);
		Finished();
		return;
	}

	pipein = p[0];
	pipeout = p[1];

	debug("forking");
	pid_t pid = fork();

	if (pid == -1)
	{
		error("Could not start post-process-script: errno %i", errno);
		Finished();
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

		execlp(m_szScript, m_szScript, szDestDir, szNZBFilename, szParFilename, szParStatus, 
			szCollectionCompleted, szHasFailedParJobs, m_pPostInfo->GetCategory(), NULL);
		fprintf(stdout, "[ERROR] Could not start post-process-script: %s", strerror(errno));
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
		error("Could not open pipe to post-process-script");
		Finished();
		return;
	}
	
	char* buf = (char*)malloc(10240);

	debug("Entering pipe-loop");
	while (!feof(readpipe) && !IsStopped())
	{
		if (fgets(buf, 10240, readpipe))
		{
			AddMessage(buf);
		}
	}
	debug("Exited pipe-loop");
	
	free(buf);
	fclose(readpipe);

	if (IsStopped())
	{
		warn("Interrupted post-process-script for %s", m_pPostInfo->GetInfoName());
	}

#ifdef WIN32
	WaitForSingleObject(m_hProcess, INFINITE);
#else
	waitpid(m_hProcess, NULL, 0);
#endif

	if (!IsStopped())
	{
		info("Completed post-process-script for %s", m_pPostInfo->GetInfoName());
	}

	Finished();
}

void ScriptController::Finished()
{
	m_pPostInfo->SetStage(PostInfo::ptFinished);
	m_pPostInfo->SetWorking(false);
}

void ScriptController::AddMessage(char* szText)
{
	debug("Adding message received from post-process-script");

	for (char* pend = szText + strlen(szText) - 1; pend >= szText && (*pend == '\n' || *pend == '\r' || *pend == ' '); pend--) *pend = '\0';

	if (strlen(szText) == 0)
	{
		// skip empty lines
		return;
	}

	if (!strncmp(szText, "[INFO] ", 7))
	{
		info(szText + 7);
		Options::EMessageTarget eMessageTarget = g_pOptions->GetInfoTarget();
		if (eMessageTarget == Options::mtScreen || eMessageTarget == Options::mtBoth)
		{
			m_pPostInfo->AppendMessage(Message::mkInfo, szText + 7);
		}
	}
	else if (!strncmp(szText, "[WARNING] ", 10))
	{
		warn(szText + 10);
		Options::EMessageTarget eMessageTarget = g_pOptions->GetWarningTarget();
		if (eMessageTarget == Options::mtScreen || eMessageTarget == Options::mtBoth)
		{
			m_pPostInfo->AppendMessage(Message::mkWarning, szText + 10);
		}
	}
	else if (!strncmp(szText, "[ERROR] ", 8))
	{
		error(szText + 8);
		Options::EMessageTarget eMessageTarget = g_pOptions->GetErrorTarget();
		if (eMessageTarget == Options::mtScreen || eMessageTarget == Options::mtBoth)
		{
			m_pPostInfo->AppendMessage(Message::mkError, szText + 8);
		}
	}
	else if (!strncmp(szText, "[DETAIL] ", 9))
	{
		detail(szText + 9);
		Options::EMessageTarget eMessageTarget = g_pOptions->GetDetailTarget();
		if (eMessageTarget == Options::mtScreen || eMessageTarget == Options::mtBoth)
		{
			m_pPostInfo->AppendMessage(Message::mkDetail, szText + 9);
		}
	}
	else if (!strncmp(szText, "[DEBUG] ", 8))
	{
		debug(szText + 8);
		Options::EMessageTarget eMessageTarget = g_pOptions->GetDebugTarget();
		if (eMessageTarget == Options::mtScreen || eMessageTarget == Options::mtBoth)
		{
			m_pPostInfo->AppendMessage(Message::mkDebug, szText + 8);
		}
	}
	else 
	{
		Options::EMessageTarget eMessageTarget = Options::mtNone;
		Message::EKind eKind = Message::mkDebug;
		switch (g_pOptions->GetPostLogKind())
		{
			case Options::plNone:
				break;

			case Options::plDetail:
				detail("Post-Process: %s", szText);
				eMessageTarget = g_pOptions->GetDetailTarget();
				eKind = Message::mkDetail;
				break;

			case Options::plInfo:
				info("Post-Process: %s", szText);
				eMessageTarget = g_pOptions->GetInfoTarget();
				eKind = Message::mkInfo;
				break;

			case Options::plWarning:
				warn("Post-Process: %s", szText);
				eMessageTarget = g_pOptions->GetWarningTarget();
				eKind = Message::mkWarning;
				break;

			case Options::plError:
				error("Post-Process: %s", szText);
				eMessageTarget = g_pOptions->GetErrorTarget();
				eKind = Message::mkError;
				break;

			case Options::plDebug:
				debug("Post-Process: %s", szText);
				eMessageTarget = g_pOptions->GetDebugTarget();
				eKind = Message::mkDebug;
				break;
		}
		if (eMessageTarget == Options::mtScreen || eMessageTarget == Options::mtBoth)
		{
			m_pPostInfo->AppendMessage(eKind, szText);
		}
	}

	debug("Adding message received from post-process-script - completed");
}

void ScriptController::Stop()
{
	debug("Stopping post-process-script");
	Thread::Stop();

#ifdef WIN32
	BOOL bOK = TerminateProcess(m_hProcess, -1);
#else
	bool bOK = kill(m_hProcess, 9) == 0;
#endif

	if (bOK)
	{
		debug("Terminated post-process-script");
	}
	else
	{
		error("Could not terminate post-process-script");
	}

	debug("Post-process-script stopped");
}
