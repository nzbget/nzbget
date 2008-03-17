/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007  Andrei Prygounkov <hugbug@users.sourceforge.net>
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
	if (!pPostInfo->GetStartTime())
	{
		pPostInfo->SetStartTime(time(NULL));
	}
	pPostInfo->SetStageTime(time(NULL));
	pPostInfo->SetStageProgress(0);

	if (!Util::FileExists(szScript))
	{
		error("Could not start post-process-script: could not find file %s", szScript);
		pPostInfo->SetStage(PostInfo::ptFinished);
		pPostInfo->SetWorking(false);
		return;
	}

	info("Executing post-process-script for %s", pPostInfo->GetInfoName());

	ScriptController* pScriptController = new ScriptController();
	pScriptController->m_pPostInfo = pPostInfo;
	pScriptController->m_szScript = szScript;
	pScriptController->m_bNZBFileCompleted = bNZBFileCompleted;
	pScriptController->m_bHasFailedParJobs = bHasFailedParJobs;
	pScriptController->SetAutoDestroy(true);

	pScriptController->Start();

	// wait until process starts or fails to start
	while (pPostInfo->GetWorking() && pPostInfo->GetStageProgress() == 0)
	{
		usleep(50 * 1000);
	}
}

void ScriptController::Run()
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

	int pipein;

#ifdef WIN32
	char szCmdLine[2048];
	snprintf(szCmdLine, 2048, "%s \"%s\" \"%s\" \"%s\" %s %s %s", m_szScript, m_pPostInfo->GetDestDir(), 
		m_pPostInfo->GetNZBFilename(), m_pPostInfo->GetParFilename(), szParStatus, szCollectionCompleted, szHasFailedParJobs);
	szCmdLine[2048-1] = '\0';
	
	//Create pipes to write and read data
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
		char szErrMsg[255];
		szErrMsg[255-1] = '\0';
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM || FORMAT_MESSAGE_IGNORE_INSERTS || FORMAT_MESSAGE_ARGUMENT_ARRAY, 
			NULL, GetLastError(), 0, szErrMsg, 255, NULL);
		error("Could not start post-process: %s", szErrMsg);
		m_pPostInfo->SetStage(PostInfo::ptFinished);
		m_pPostInfo->SetWorking(false);
		return;
	}
	
	/* close unused "write" end */
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

	/* create the pipe */
	if (pipe(p) != 0)
	{
		error("Could not open pipe: errno %i", errno);
		m_pPostInfo->SetStage(PostInfo::ptFinished);
		m_pPostInfo->SetWorking(false);
		return;
	}

	/* orient the pipe */
	pipein = p[0];
	pipeout = p[1];
	
	pid_t pid = fork();

	if (pid == -1)
	{
		error("Could not start post-process: errno %i", errno);
		m_pPostInfo->SetStage(PostInfo::ptFinished);
		m_pPostInfo->SetWorking(false);
		return;
	}
	else if (pid == 0)
	{
		// here goes the second instance
			
		/* close up the "read" end */
		close(pipein);
      			
		/* make the pipeout to be the same as stdout and stderr */
		dup2(pipeout, 1);
		dup2(pipeout, 2);
		close(pipeout);

		/* close all other descriptors */
		for (int h = getdtablesize(); h > 2; --h)
		{
			close(h);
		}
		
		execlp(m_szScript, m_szScript, szDestDir, szNZBFilename, szParFilename, 
			szParStatus, szCollectionCompleted, szHasFailedParJobs, NULL);
		error("Could not start post-process: %s", strerror(errno));
		exit(-1);
	}

	// continue the first instance

	/* close unused "write" end */
	close(pipeout);
#endif

	m_pPostInfo->SetStageProgress(50);

	/* open the read end */
	FILE* readpipe = fdopen(pipein, "r");
	char* buf = (char*)malloc(10240);

	debug("Entering pipe-loop");
	while (!feof(readpipe))
	{
		if (fgets(buf, 10240, readpipe))
		{
			AddMessage(buf);
		}
	}
	debug("Exited pipe-loop");
	
	free(buf);
	fclose(readpipe);

#ifdef WIN32
	WaitForSingleObject(ProcessInfo.hProcess, INFINITE);
#else
	waitpid(pid, NULL, 0);
#endif

	info("Completed post-process-script for %s", m_pPostInfo->GetInfoName());
	m_pPostInfo->SetStage(PostInfo::ptFinished);
	m_pPostInfo->SetWorking(false);
}

void ScriptController::AddMessage(char* szText)
{
	for (char* pend = szText + strlen(szText) - 1; pend >= szText && (*pend == '\n' || *pend == '\r' || *pend == ' '); pend--) *pend = '\0';

	if (strlen(szText) == 0)
	{
		// skip empty lines
		return;
	}

	if (!strncmp(szText, "[INFO] ", 7))
	{
		info(szText + 7);
	}
	else if (!strncmp(szText, "[WARNING] ", 10))
	{
		warn(szText + 10);
	}
	else if (!strncmp(szText, "[ERROR] ", 8))
	{
		error(szText + 8);
	}
	else if (!strncmp(szText, "[DETAIL] ", 9))
	{
		detail(szText + 9);
	}
	else if (!strncmp(szText, "[DEBUG] ", 8))
	{
		debug(szText + 8);
	}
	else 
	{
		switch (g_pOptions->GetPostLogKind())
		{
			case Options::plNone:
				break;

			case Options::plDetail:
				detail("Post-Process: %s", szText);
				break;

			case Options::plInfo:
				info("Post-Process: %s", szText);
				break;

			case Options::plWarning:
				warn("Post-Process: %s", szText);
				break;

			case Options::plError:
				error("Post-Process: %s", szText);
				break;

			case Options::plDebug:
				debug("Post-Process: %s", szText);
				break;
		}
	}
}
