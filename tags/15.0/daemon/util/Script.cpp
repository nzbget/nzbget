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
#include <sys/wait.h>
#include <signal.h>
#else
#include <io.h>
#endif
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <algorithm>

#include "nzbget.h"
#include "Script.h"
#include "Log.h"
#include "Util.h"
#include "Options.h"

// System global variable holding environments variables
extern char** environ;

extern Options* g_pOptions;
extern char* (*g_szEnvironmentVariables)[];

ScriptController::RunningScripts ScriptController::m_RunningScripts;
Mutex ScriptController::m_mutexRunning;

#ifndef WIN32
#define CHILD_WATCHDOG 1
#endif

#ifdef CHILD_WATCHDOG
/**
 * Sometimes the forked child process doesn't start properly and hangs
 * just during the starting. I didn't find any explanation about what
 * could cause that problem except of a general advice, that
 * "a forking in a multithread application is not recommended".
 *
 * Workaround:
 * 1) child process prints a line into stdout directly after the start;
 * 2) parent process waits for a line for 60 seconds. If it didn't receive it
 *    the cild process assumed to hang and will be killed. Another attempt
 *    will be made.
 */
 
class ChildWatchDog : public Thread
{
private:
	pid_t			m_hProcessID;
protected:
	virtual void	Run();
public:
	void			SetProcessID(pid_t hProcessID) { m_hProcessID = hProcessID; }
};

void ChildWatchDog::Run()
{
	static const int WAIT_SECONDS = 60;
	time_t tStart = time(NULL);
	while (!IsStopped() && (time(NULL) - tStart) < WAIT_SECONDS)
	{
		usleep(10 * 1000);
	}

	if (!IsStopped())
	{
		info("Restarting hanging child process");
		kill(m_hProcessID, SIGKILL);
	}
}
#endif


EnvironmentStrings::EnvironmentStrings()
{
}

EnvironmentStrings::~EnvironmentStrings()
{
	Clear();
}

void EnvironmentStrings::Clear()
{
	for (Strings::iterator it = m_strings.begin(); it != m_strings.end(); it++)
	{
		free(*it);
	}
	m_strings.clear();
}

void EnvironmentStrings::InitFromCurrentProcess()
{
	for (int i = 0; (*g_szEnvironmentVariables)[i]; i++)
	{
		char* szVar = (*g_szEnvironmentVariables)[i];
		// Ignore all env vars set by NZBGet.
		// This is to avoid the passing of env vars after program update (when NZBGet is
		// started from a script which was started by a previous instance of NZBGet).
		// Format: NZBXX_YYYY (XX are any two characters, YYYY are any number of any characters).
		if (!(!strncmp(szVar, "NZB", 3) && strlen(szVar) > 5 && szVar[5] == '_'))
		{
			Append(strdup(szVar));
		}
	}
}

void EnvironmentStrings::Append(char* szString)
{
	m_strings.push_back(szString);
}

#ifdef WIN32
/*
 * Returns environment block in format suitable for using with CreateProcess. 
 * The allocated memory must be freed by caller using "free()".
 */
char* EnvironmentStrings::GetStrings()
{
	int iSize = 1;
	for (Strings::iterator it = m_strings.begin(); it != m_strings.end(); it++)
	{
		char* szVar = *it;
		iSize += strlen(szVar) + 1;
	}

	char* szStrings = (char*)malloc(iSize);
	char* szPtr = szStrings;
	for (Strings::iterator it = m_strings.begin(); it != m_strings.end(); it++)
	{
		char* szVar = *it;
		strcpy(szPtr, szVar);
		szPtr += strlen(szVar) + 1;
	}
	*szPtr = '\0';

	return szStrings;
}

#else

/*
 * Returns environment block in format suitable for using with execve 
 * The allocated memory must be freed by caller using "free()".
 */
char** EnvironmentStrings::GetStrings()
{
	char** pStrings = (char**)malloc((m_strings.size() + 1) * sizeof(char*));
	char** pPtr = pStrings;
	for (Strings::iterator it = m_strings.begin(); it != m_strings.end(); it++)
	{
		char* szVar = *it;
		*pPtr = szVar;
		pPtr++;
	}
	*pPtr = NULL;

	return pStrings;
}
#endif


ScriptController::ScriptController()
{
	m_szScript = NULL;
	m_szWorkingDir = NULL;
	m_szArgs = NULL;
	m_bFreeArgs = false;
	m_szInfoName = NULL;
	m_szLogPrefix = NULL;
	m_bTerminated = false;
	m_bDetached = false;
	m_hProcess = 0;
	ResetEnv();

	m_mutexRunning.Lock();
	m_RunningScripts.push_back(this);
	m_mutexRunning.Unlock();
}

ScriptController::~ScriptController()
{
	if (m_bFreeArgs)
	{
		for (const char** szArgPtr = m_szArgs; *szArgPtr; szArgPtr++)
		{
			free((char*)*szArgPtr);
		}
		free(m_szArgs);
	}

	UnregisterRunningScript();
}

void ScriptController::UnregisterRunningScript()
{
    m_mutexRunning.Lock();
    RunningScripts::iterator it = std::find(m_RunningScripts.begin(), m_RunningScripts.end(), this);
    if (it != m_RunningScripts.end())
    {
        m_RunningScripts.erase(it);
    }
    m_mutexRunning.Unlock();
}

void ScriptController::ResetEnv()
{
	m_environmentStrings.Clear();
	m_environmentStrings.InitFromCurrentProcess();
}

void ScriptController::SetEnvVar(const char* szName, const char* szValue)
{
	int iLen = strlen(szName) + strlen(szValue) + 2;
	char* szVar = (char*)malloc(iLen);
	snprintf(szVar, iLen, "%s=%s", szName, szValue);
	m_environmentStrings.Append(szVar);
}

void ScriptController::SetIntEnvVar(const char* szName, int iValue)
{
	char szValue[1024];
	snprintf(szValue, 10, "%i", iValue);
	szValue[1024-1] = '\0';
	SetEnvVar(szName, szValue);
}

/**
 * If szStripPrefix is not NULL, only options, whose names start with the prefix
 * are processed. The prefix is then stripped from the names.
 * If szStripPrefix is NULL, all options are processed; without stripping.
 */
void ScriptController::PrepareEnvOptions(const char* szStripPrefix)
{
	int iPrefixLen = szStripPrefix ? strlen(szStripPrefix) : 0;

	Options::OptEntries* pOptEntries = g_pOptions->LockOptEntries();

	for (Options::OptEntries::iterator it = pOptEntries->begin(); it != pOptEntries->end(); it++)
	{
		Options::OptEntry* pOptEntry = *it;
		
		if (szStripPrefix && !strncmp(pOptEntry->GetName(), szStripPrefix, iPrefixLen) && (int)strlen(pOptEntry->GetName()) > iPrefixLen)
		{
			SetEnvVarSpecial("NZBPO", pOptEntry->GetName() + iPrefixLen, pOptEntry->GetValue());
		}
		else if (!szStripPrefix)
		{
			SetEnvVarSpecial("NZBOP", pOptEntry->GetName(), pOptEntry->GetValue());
		}
	}

	g_pOptions->UnlockOptEntries();
}

void ScriptController::SetEnvVarSpecial(const char* szPrefix, const char* szName, const char* szValue)
{
	char szVarname[1024];
	snprintf(szVarname, sizeof(szVarname), "%s_%s", szPrefix, szName);
	szVarname[1024-1] = '\0';
	
	// Original name
	SetEnvVar(szVarname, szValue);
	
	char szNormVarname[1024];
	strncpy(szNormVarname, szVarname, sizeof(szVarname));
	szNormVarname[1024-1] = '\0';
	
	// Replace special characters  with "_" and convert to upper case
	for (char* szPtr = szNormVarname; *szPtr; szPtr++)
	{
		if (strchr(".:*!\"$%&/()=`+~#'{}[]@- ", *szPtr)) *szPtr = '_';
		*szPtr = toupper(*szPtr);
	}
	
	// Another env var with normalized name (replaced special chars and converted to upper case)
	if (strcmp(szVarname, szNormVarname))
	{
		SetEnvVar(szNormVarname, szValue);
	}
}

void ScriptController::PrepareArgs()
{
#ifdef WIN32
	if (!m_szArgs)
	{
		// Special support for script languages:
		// automatically find the app registered for this extension and run it
		const char* szExtension = strrchr(GetScript(), '.');
		if (szExtension && strcasecmp(szExtension, ".exe") && strcasecmp(szExtension, ".bat") && strcasecmp(szExtension, ".cmd"))
		{
			debug("Looking for associated program for %s", szExtension);
			char szCommand[512];
			int iBufLen = 512-1;
			if (Util::RegReadStr(HKEY_CLASSES_ROOT, szExtension, NULL, szCommand, &iBufLen))
			{
				szCommand[iBufLen] = '\0';
				debug("Extension: %s", szCommand);
				
				char szRegPath[512];
				snprintf(szRegPath, 512, "%s\\shell\\open\\command", szCommand);
				szRegPath[512-1] = '\0';
				
				iBufLen = 512-1;
				if (Util::RegReadStr(HKEY_CLASSES_ROOT, szRegPath, NULL, szCommand, &iBufLen))
				{
					szCommand[iBufLen] = '\0';
					debug("Command: %s", szCommand);
					
					DWORD_PTR pArgs[] = { (DWORD_PTR)GetScript(), (DWORD_PTR)0 };
					if (FormatMessage(FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ARGUMENT_ARRAY, szCommand, 0, 0,
									  m_szCmdLine, sizeof(m_szCmdLine), (va_list*)pArgs))
					{
						debug("CmdLine: %s", m_szCmdLine);
						return;
					}
				}
			}
			warn("Could not found associated program for %s. Trying to execute %s directly", szExtension, Util::BaseFileName(GetScript()));
		}
	}
#endif

	if (!m_szArgs)
	{
		m_szStdArgs[0] = GetScript();
		m_szStdArgs[1] = NULL;
		SetArgs(m_szStdArgs, false);
	}
}

int ScriptController::Execute()
{
	PrepareEnvOptions(NULL);
	PrepareArgs();

	int iExitCode = 0;
	int pipein;

#ifdef CHILD_WATCHDOG
	bool bChildConfirmed = false;
	while (!bChildConfirmed && !m_bTerminated)
	{
#endif

#ifdef WIN32
	// build command line

	char* szCmdLine = NULL;
	if (m_szArgs)
	{
		char szCmdLineBuf[2048];
		int iUsedLen = 0;
		for (const char** szArgPtr = m_szArgs; *szArgPtr; szArgPtr++)
		{
			snprintf(szCmdLineBuf + iUsedLen, 2048 - iUsedLen, "\"%s\" ", *szArgPtr);
			iUsedLen += strlen(*szArgPtr) + 3;
		}
		szCmdLineBuf[iUsedLen < 2048 ? iUsedLen - 1 : 2048 - 1] = '\0';
		szCmdLine = szCmdLineBuf;
	}
	else
	{
		szCmdLine = m_szCmdLine;
	}
	
	// create pipes to write and read data
	HANDLE hReadPipe, hWritePipe;
	SECURITY_ATTRIBUTES SecurityAttributes;
	memset(&SecurityAttributes, 0, sizeof(SecurityAttributes));
	SecurityAttributes.nLength = sizeof(SecurityAttributes);
	SecurityAttributes.bInheritHandle = TRUE;

	CreatePipe(&hReadPipe, &hWritePipe, &SecurityAttributes, 0);

	SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFO StartupInfo;
	memset(&StartupInfo, 0, sizeof(StartupInfo));
	StartupInfo.cb = sizeof(StartupInfo);
	StartupInfo.dwFlags = STARTF_USESTDHANDLES;
	StartupInfo.hStdInput = 0;
	StartupInfo.hStdOutput = hWritePipe;
	StartupInfo.hStdError = hWritePipe;

	PROCESS_INFORMATION ProcessInfo;
	memset(&ProcessInfo, 0, sizeof(ProcessInfo));

	char* szEnvironmentStrings = m_environmentStrings.GetStrings();

	BOOL bOK = CreateProcess(NULL, szCmdLine, NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW, szEnvironmentStrings, m_szWorkingDir, &StartupInfo, &ProcessInfo);
	if (!bOK)
	{
		DWORD dwErrCode = GetLastError();
		char szErrMsg[255];
		szErrMsg[255-1] = '\0';
		if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwErrCode, 0, szErrMsg, 255, NULL))
		{
			PrintMessage(Message::mkError, "Could not start %s: %s", m_szInfoName, szErrMsg);
		}
		else
		{
			PrintMessage(Message::mkError, "Could not start %s: error %i", m_szInfoName, dwErrCode);
		}
		if (!Util::FileExists(m_szScript))
		{
			PrintMessage(Message::mkError, "Could not find file %s", m_szScript);
		}
		free(szEnvironmentStrings);
		return -1;
	}

	free(szEnvironmentStrings);

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
		PrintMessage(Message::mkError, "Could not open pipe: errno %i", errno);
		return -1;
	}

	char** pEnvironmentStrings = m_environmentStrings.GetStrings();

	pipein = p[0];
	pipeout = p[1];

	debug("forking");
	pid_t pid = fork();

	if (pid == -1)
	{
		PrintMessage(Message::mkError, "Could not start %s: errno %i", m_szInfoName, errno);
		free(pEnvironmentStrings);
		return -1;
	}
	else if (pid == 0)
	{
		// here goes the second instance

		// create new process group (see Terminate() where it is used)
		setsid();
			
		// close up the "read" end
		close(pipein);
      			
		// make the pipeout to be the same as stdout and stderr
		dup2(pipeout, 1);
		dup2(pipeout, 2);
		
		close(pipeout);

#ifdef CHILD_WATCHDOG
		fwrite("\n", 1, 1, stdout);
		fflush(stdout);
#endif

		chdir(m_szWorkingDir);
		environ = pEnvironmentStrings;
		execvp(m_szScript, (char* const*)m_szArgs);

		if (errno == EACCES)
		{
			fprintf(stdout, "[WARNING] Fixing permissions for %s\n", m_szScript);
			fflush(stdout);
			Util::FixExecPermission(m_szScript);
			execvp(m_szScript, (char* const*)m_szArgs);
		}

		// NOTE: the text "[ERROR] Could not start " is checked later,
		// by changing adjust the dependent code below.
		fprintf(stdout, "[ERROR] Could not start %s: %s", m_szScript, strerror(errno));
		fflush(stdout);
		_exit(254);
	}

	// continue the first instance
	debug("forked");
	debug("Child Process-ID: %i", (int)pid);

	free(pEnvironmentStrings);

	m_hProcess = pid;

	// close unused "write" end
	close(pipeout);
#endif

	// open the read end
	m_pReadpipe = fdopen(pipein, "r");
	if (!m_pReadpipe)
	{
		PrintMessage(Message::mkError, "Could not open pipe to %s", m_szInfoName);
		return -1;
	}
	
#ifdef CHILD_WATCHDOG
	debug("Creating child watchdog");
	ChildWatchDog* pWatchDog = new ChildWatchDog();
	pWatchDog->SetAutoDestroy(false);
	pWatchDog->SetProcessID(pid);
	pWatchDog->Start();
#endif
	
	char* buf = (char*)malloc(10240);

	debug("Entering pipe-loop");
	bool bFirstLine = true;
	bool bStartError = false;
	while (!m_bTerminated && !m_bDetached && !feof(m_pReadpipe))
	{
		if (ReadLine(buf, 10240, m_pReadpipe) && m_pReadpipe)
		{
#ifdef CHILD_WATCHDOG
			if (!bChildConfirmed)
			{
				bChildConfirmed = true;
				pWatchDog->Stop();
				debug("Child confirmed");
				continue;
			}
#endif
			if (bFirstLine && !strncmp(buf, "[ERROR] Could not start ", 24))
			{
				bStartError = true;
			}
			ProcessOutput(buf);
			bFirstLine = false;
		}
	}
	debug("Exited pipe-loop");

#ifdef CHILD_WATCHDOG
	debug("Destroying WatchDog");
	if (!bChildConfirmed)
	{
		pWatchDog->Stop();
	}
	while (pWatchDog->IsRunning())
	{
		usleep(5 * 1000);
	}
	delete pWatchDog;
#endif
	
	free(buf);
	if (m_pReadpipe)
	{
		fclose(m_pReadpipe);
	}

	if (m_bTerminated)
	{
		warn("Interrupted %s", m_szInfoName);
	}

	iExitCode = 0;

	if (!m_bTerminated && !m_bDetached)
	{
#ifdef WIN32
	WaitForSingleObject(m_hProcess, INFINITE);
	DWORD dExitCode = 0;
	GetExitCodeProcess(m_hProcess, &dExitCode);
	iExitCode = dExitCode;
#else
	int iStatus = 0;
	waitpid(m_hProcess, &iStatus, 0);
	if (WIFEXITED(iStatus))
	{
		iExitCode = WEXITSTATUS(iStatus);
		if (iExitCode == 254 && bStartError)
		{
			iExitCode = -1;
		}
	}
#endif
	}
	
#ifdef CHILD_WATCHDOG
	}	// while (!bChildConfirmed && !m_bTerminated)
#endif
	
	debug("Exit code %i", iExitCode);

	return iExitCode;
}

void ScriptController::Terminate()
{
	debug("Stopping %s", m_szInfoName);
	m_bTerminated = true;

#ifdef WIN32
	BOOL bOK = TerminateProcess(m_hProcess, -1);
	if (bOK)
	{
		// wait 60 seconds for process to terminate
		WaitForSingleObject(m_hProcess, 60 * 1000);
	}
	else
	{
		DWORD dExitCode = 0;
		GetExitCodeProcess(m_hProcess, &dExitCode);
		bOK = dExitCode != STILL_ACTIVE;
	}
#else
	pid_t hKillProcess = m_hProcess;
	if (getpgid(hKillProcess) == hKillProcess)
	{
		// if the child process has its own group (setsid() was successful), kill the whole group
		hKillProcess = -hKillProcess;
	}
	bool bOK = hKillProcess && kill(hKillProcess, SIGKILL) == 0;
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

void ScriptController::TerminateAll()
{
	m_mutexRunning.Lock();
	for (RunningScripts::iterator it = m_RunningScripts.begin(); it != m_RunningScripts.end(); it++)
	{
		ScriptController* pScript = *it;
		if (pScript->m_hProcess && !pScript->m_bDetached)
		{
			pScript->Terminate();
		}
	}
	m_mutexRunning.Unlock();
}

void ScriptController::Detach()
{
	debug("Detaching %s", m_szInfoName);
	m_bDetached = true;
	FILE* pReadpipe = m_pReadpipe;
	m_pReadpipe = NULL;
	fclose(pReadpipe);
}
	
void ScriptController::Resume()
{
	m_bTerminated = false;
	m_bDetached = false;
	m_hProcess = 0;
}

bool ScriptController::ReadLine(char* szBuf, int iBufSize, FILE* pStream)
{
	return fgets(szBuf, iBufSize, pStream);
}

void ScriptController::ProcessOutput(char* szText)
{
	debug("Processing output received from script");

	for (char* pend = szText + strlen(szText) - 1; pend >= szText && (*pend == '\n' || *pend == '\r' || *pend == ' '); pend--) *pend = '\0';

	if (szText[0] == '\0')
	{
		// skip empty lines
		return;
	}

	if (!strncmp(szText, "[INFO] ", 7))
	{
		PrintMessage(Message::mkInfo, "%s", szText + 7);
	}
	else if (!strncmp(szText, "[WARNING] ", 10))
	{
		PrintMessage(Message::mkWarning, "%s", szText + 10);
	}
	else if (!strncmp(szText, "[ERROR] ", 8))
	{
		PrintMessage(Message::mkError, "%s", szText + 8);
	}
	else if (!strncmp(szText, "[DETAIL] ", 9))
	{
		PrintMessage(Message::mkDetail, "%s", szText + 9);
	}
	else if (!strncmp(szText, "[DEBUG] ", 8))
	{
		PrintMessage(Message::mkDebug, "%s", szText + 8);
	}
	else 
	{
		PrintMessage(Message::mkInfo, "%s", szText);
	}

	debug("Processing output received from script - completed");
}

void ScriptController::AddMessage(Message::EKind eKind, const char* szText)
{
	switch (eKind)
	{
		case Message::mkDetail:
			detail("%s", szText);
			break;

		case Message::mkInfo:
			info("%s", szText);
			break;

		case Message::mkWarning:
			warn("%s", szText);
			break;

		case Message::mkError:
			error("%s", szText);
			break;

		case Message::mkDebug:
			debug("%s", szText);
			break;
	}
}

void ScriptController::PrintMessage(Message::EKind eKind, const char* szFormat, ...)
{
	char tmp2[1024];

	va_list ap;
	va_start(ap, szFormat);
	vsnprintf(tmp2, 1024, szFormat, ap);
	tmp2[1024-1] = '\0';
	va_end(ap);

	char tmp3[1024];
	if (m_szLogPrefix)
	{
		snprintf(tmp3, 1024, "%s: %s", m_szLogPrefix, tmp2);
	}
	else
	{
		strncpy(tmp3, tmp2, 1024);
	}
	tmp3[1024-1] = '\0';

	AddMessage(eKind, tmp3);
}
