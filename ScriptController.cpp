/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#endif
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>

#include "nzbget.h"
#include "ScriptController.h"
#include "Log.h"
#include "Util.h"

// System global variable holding environments variables
extern char** environ;

extern Options* g_pOptions;
extern char* (*g_szEnvironmentVariables)[];
extern DownloadQueueHolder* g_pDownloadQueueHolder;

static const int POSTPROCESS_PARCHECK = 92;
static const int POSTPROCESS_SUCCESS = 93;
static const int POSTPROCESS_ERROR = 94;
static const int POSTPROCESS_NONE = 95;

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
		Append(strdup(szVar));
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
	m_environmentStrings.InitFromCurrentProcess();
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

/**
 * If szStripPrefix is not NULL, only pp-parameters, whose names start with the prefix
 * are processed. The prefix is then stripped from the names.
 * If szStripPrefix is NULL, all pp-parameters are processed; without stripping.
 */
void ScriptController::PrepareEnvParameters(NZBInfo* pNZBInfo, const char* szStripPrefix)
{
	int iPrefixLen = szStripPrefix ? strlen(szStripPrefix) : 0;

	for (NZBParameterList::iterator it = pNZBInfo->GetParameters()->begin(); it != pNZBInfo->GetParameters()->end(); it++)
	{
		NZBParameter* pParameter = *it;
		
		if (szStripPrefix && !strncmp(pParameter->GetName(), szStripPrefix, iPrefixLen) && (int)strlen(pParameter->GetName()) > iPrefixLen)
		{
			SetEnvVarSpecial("NZBPR", pParameter->GetName() + iPrefixLen, pParameter->GetValue());
		}
		else if (!szStripPrefix)
		{
			SetEnvVarSpecial("NZBPR", pParameter->GetName(), pParameter->GetValue());
		}
	}
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
			error("Could not start %s: %s", m_szInfoName, szErrMsg);
		}
		else
		{
			error("Could not start %s: error %i", m_szInfoName, dwErrCode);
		}
		if (!Util::FileExists(m_szScript))
		{
			error("Could not find file %s", m_szScript);
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
		error("Could not open pipe: errno %i", errno);
		return -1;
	}

	char** pEnvironmentStrings = m_environmentStrings.GetStrings();

	pipein = p[0];
	pipeout = p[1];

	debug("forking");
	pid_t pid = fork();

	if (pid == -1)
	{
		error("Could not start %s: errno %i", m_szInfoName, errno);
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
	FILE* readpipe = fdopen(pipein, "r");
	if (!readpipe)
	{
		error("Could not open pipe to %s", m_szInfoName);
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
	while (!feof(readpipe) && !m_bTerminated)
	{
		if (ReadLine(buf, 10240, readpipe))
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
		usleep(1 * 1000);
	}
	delete pWatchDog;
#endif
	
	free(buf);
	fclose(readpipe);

	if (m_bTerminated)
	{
		warn("Interrupted %s", m_szInfoName);
	}

	iExitCode = 0;

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
#else
	pid_t hKillProcess = m_hProcess;
	if (getpgid(hKillProcess) == hKillProcess)
	{
		// if the child process has its own group (setsid() was successful), kill the whole group
		hKillProcess = -hKillProcess;
	}
	bool bOK = kill(hKillProcess, SIGKILL) == 0;
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
		PrintMessage(Message::mkInfo, szText + 7);
	}
	else if (!strncmp(szText, "[WARNING] ", 10))
	{
		PrintMessage(Message::mkWarning, szText + 10);
	}
	else if (!strncmp(szText, "[ERROR] ", 8))
	{
		PrintMessage(Message::mkError, szText + 8);
	}
	else if (!strncmp(szText, "[DETAIL] ", 9))
	{
		PrintMessage(Message::mkDetail, szText + 9);
	}
	else if (!strncmp(szText, "[DEBUG] ", 8))
	{
		PrintMessage(Message::mkDebug, szText + 8);
	}
	else 
	{	
		switch (m_eDefaultLogKind)
		{
			case Options::slNone:
				break;

			case Options::slDetail:
				PrintMessage(Message::mkDetail, szText);
				break;

			case Options::slInfo:
				PrintMessage(Message::mkInfo, szText);
				break;

			case Options::slWarning:
				PrintMessage(Message::mkWarning, szText);
				break;

			case Options::slError:
				PrintMessage(Message::mkError, szText);
				break;

			case Options::slDebug:
				PrintMessage(Message::mkDebug, szText);
				break;
		}
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

void PostScriptController::StartJob(PostInfo* pPostInfo)
{
	PostScriptController* pScriptController = new PostScriptController();
	pScriptController->m_pPostInfo = pPostInfo;
	pScriptController->SetWorkingDir(g_pOptions->GetDestDir());
	pScriptController->SetAutoDestroy(false);

	pPostInfo->SetPostThread(pScriptController);

	pScriptController->Start();
}

void PostScriptController::Run()
{
	FileList activeList;

	// the locking is needed for accessing the members of NZBInfo
	g_pDownloadQueueHolder->LockQueue();
	for (NZBParameterList::iterator it = m_pPostInfo->GetNZBInfo()->GetParameters()->begin(); it != m_pPostInfo->GetNZBInfo()->GetParameters()->end(); it++)
	{
		NZBParameter* pParameter = *it;
		const char* szVarname = pParameter->GetName();
		if (strlen(szVarname) > 0 && szVarname[0] != '*' && szVarname[strlen(szVarname)-1] == ':' &&
			(!strcasecmp(pParameter->GetValue(), "yes") || !strcasecmp(pParameter->GetValue(), "on") || !strcasecmp(pParameter->GetValue(), "1")))
		{
			char* szScriptName = strdup(szVarname);
			szScriptName[strlen(szScriptName)-1] = '\0'; // remove trailing ':'
			activeList.push_back(szScriptName);
		}
	}
	g_pDownloadQueueHolder->UnlockQueue();

	Options::ScriptList scriptList;
	g_pOptions->LoadScriptList(&scriptList);

	for (Options::ScriptList::iterator it = scriptList.begin(); it != scriptList.end(); it++)
	{
		Options::Script* pScript = *it;
		for (FileList::iterator it2 = activeList.begin(); it2 != activeList.end(); it2++)
		{
			char* szActiveName = *it2;
			// if any script has requested par-check, do not execute other scripts
			if (Util::SameFilename(pScript->GetName(), szActiveName) && !m_pPostInfo->GetRequestParCheck())
			{
				ExecuteScript(pScript->GetName(), pScript->GetLocation());
			}
		}
	}
	
	for (FileList::iterator it = activeList.begin(); it != activeList.end(); it++)
	{
		free(*it);
	}

	m_pPostInfo->SetStage(PostInfo::ptFinished);
	m_pPostInfo->SetWorking(false);
}

void PostScriptController::ExecuteScript(const char* szScriptName, const char* szLocation)
{
	PrintMessage(Message::mkInfo, "Executing post-process-script %s for %s", szScriptName, m_pPostInfo->GetInfoName());

	SetScript(szLocation);
	SetArgs(NULL, false);

	char szInfoName[1024];
	snprintf(szInfoName, 1024, "post-process-script %s for %s", szScriptName, m_pPostInfo->GetInfoName());
	szInfoName[1024-1] = '\0';
	SetInfoName(szInfoName);

	char szLogPrefix[1024];
	strncpy(szLogPrefix, szScriptName, 1024);
	szLogPrefix[1024-1] = '\0';
	if (char* ext = strrchr(szLogPrefix, '.')) *ext = '\0'; // strip file extension
	SetLogPrefix(szLogPrefix);

	SetDefaultLogKind(g_pOptions->GetProcessLogKind());

	PrepareParams(szScriptName);

	int iExitCode = Execute();

	szInfoName[0] = 'P'; // uppercase

	SetLogPrefix(NULL);
	AnalyseExitCode(iExitCode);
}

void PostScriptController::PrepareParams(const char* szScriptName)
{
	// the locking is needed for accessing the members of NZBInfo
	g_pDownloadQueueHolder->LockQueue();

	char szNZBName[1024];
	strncpy(szNZBName, m_pPostInfo->GetNZBInfo()->GetName(), 1024);
	szNZBName[1024-1] = '\0';

	int iParStatus[] = { 0, 0, 1, 2, 3 };
	char szParStatus[10];
	snprintf(szParStatus, 10, "%i", iParStatus[m_pPostInfo->GetNZBInfo()->GetParStatus()]);
	szParStatus[10-1] = '\0';

	int iUnpackStatus[] = { 0, 0, 1, 2 };
	char szUnpackStatus[10];
	snprintf(szUnpackStatus, 10, "%i", iUnpackStatus[m_pPostInfo->GetNZBInfo()->GetUnpackStatus()]);
	szUnpackStatus[10-1] = '\0';

	char szDestDir[1024];
	strncpy(szDestDir, m_pPostInfo->GetNZBInfo()->GetDestDir(), 1024);
	szDestDir[1024-1] = '\0';
	
	char szNZBFilename[1024];
	strncpy(szNZBFilename, m_pPostInfo->GetNZBInfo()->GetFilename(), 1024);
	szNZBFilename[1024-1] = '\0';
	
	char szCategory[1024];
	strncpy(szCategory, m_pPostInfo->GetNZBInfo()->GetCategory(), 1024);
	szCategory[1024-1] = '\0';

	// Reset
	ResetEnv();

	SetEnvVar("NZBPP_NZBNAME", szNZBName);
	SetEnvVar("NZBPP_DIRECTORY", szDestDir);
	SetEnvVar("NZBPP_NZBFILENAME", szNZBFilename);
	SetEnvVar("NZBPP_PARSTATUS", szParStatus);
	SetEnvVar("NZBPP_UNPACKSTATUS", szUnpackStatus);
	SetEnvVar("NZBPP_CATEGORY", szCategory);

	PrepareEnvParameters(m_pPostInfo->GetNZBInfo(), NULL);

	char szParamPrefix[1024];
	snprintf(szParamPrefix, 1024, "%s:", szScriptName);
	szParamPrefix[1024-1] = '\0';
	PrepareEnvParameters(m_pPostInfo->GetNZBInfo(), szParamPrefix);
	PrepareEnvOptions(szParamPrefix);
	
	g_pDownloadQueueHolder->UnlockQueue();
}

void PostScriptController::AnalyseExitCode(int iExitCode)
{
	// The ScriptStatus is accumulated for all scripts:
	// If any script has failed the status is "failure", etc.

	switch (iExitCode)
	{
		case POSTPROCESS_SUCCESS:
			PrintMessage(Message::mkInfo, "%s successful", GetInfoName());
			if (m_pPostInfo->GetNZBInfo()->GetScriptStatus() == NZBInfo::srNone)
			{
				m_pPostInfo->GetNZBInfo()->SetScriptStatus(NZBInfo::srSuccess);
			}
			break;

		case POSTPROCESS_ERROR:
		case -1: // Execute() returns -1 if the process could not be started (file not found or other problem)
			PrintMessage(Message::mkError, "%s failed", GetInfoName());
			m_pPostInfo->GetNZBInfo()->SetScriptStatus(NZBInfo::srFailure);
			break;

		case POSTPROCESS_NONE:
			PrintMessage(Message::mkInfo, "%s skipped", GetInfoName());
			break;

#ifndef DISABLE_PARCHECK
		case POSTPROCESS_PARCHECK:
			if (m_pPostInfo->GetNZBInfo()->GetParStatus() > NZBInfo::psSkipped)
			{
				PrintMessage(Message::mkError, "%s requested par-check/repair, but the collection was already checked", GetInfoName());
				m_pPostInfo->GetNZBInfo()->SetScriptStatus(NZBInfo::srFailure);
			}
			else
			{
				PrintMessage(Message::mkInfo, "%s requested par-check/repair", GetInfoName());
				m_pPostInfo->SetRequestParCheck(true);
				if (m_pPostInfo->GetNZBInfo()->GetScriptStatus() == NZBInfo::srNone)
				{
					m_pPostInfo->GetNZBInfo()->SetScriptStatus(NZBInfo::srSuccess);
				}
			}
			break;
#endif

		default:
			PrintMessage(Message::mkWarning, "%s terminated with unknown status", GetInfoName());
			if (m_pPostInfo->GetNZBInfo()->GetScriptStatus() == NZBInfo::srNone)
			{
				m_pPostInfo->GetNZBInfo()->SetScriptStatus(NZBInfo::srUnknown);
			}
	}
}

void PostScriptController::AddMessage(Message::EKind eKind, const char* szText)
{
	if (!strncmp(szText, "[HISTORY] ", 10))
	{
		m_pPostInfo->GetNZBInfo()->AppendMessage(eKind, 0, szText);
	}
	else
	{
		ScriptController::AddMessage(eKind, szText);
		m_pPostInfo->AppendMessage(eKind, szText);
	}

	if (g_pOptions->GetPausePostProcess())
	{
		time_t tStageTime = m_pPostInfo->GetStageTime();
		time_t tStartTime = m_pPostInfo->GetStartTime();
		time_t tWaitTime = time(NULL);

		// wait until Post-processor is unpaused
		while (g_pOptions->GetPausePostProcess() && !IsStopped())
		{
			usleep(100 * 1000);

			// update time stamps

			time_t tDelta = time(NULL) - tWaitTime;

			if (tStageTime > 0)
			{
				m_pPostInfo->SetStageTime(tStageTime + tDelta);
			}

			if (tStartTime > 0)
			{
				m_pPostInfo->SetStartTime(tStartTime + tDelta);
			}
		}
	}
}

void PostScriptController::Stop()
{
	debug("Stopping post-process-script");
	Thread::Stop();
	Terminate();
}

/**
 * DownloadQueue must be locked prior to call of this function.
 */
void PostScriptController::InitParamsForNewNZB(NZBInfo* pNZBInfo)
{
	const char* szDefScript = g_pOptions->GetDefScript();

	if (pNZBInfo->GetCategory() && strlen(pNZBInfo->GetCategory()) > 0)
	{
		Options::Category* pCategory = g_pOptions->FindCategory(pNZBInfo->GetCategory());
		if (pCategory)
		{
			szDefScript = pCategory->GetDefScript();
		}
	}

	if (!szDefScript || strlen(szDefScript) == 0)
	{
		return;
	}

	// split szDefScript into tokens and create pp-parameter for each token
	char* szDefScript2 = strdup(szDefScript);
	char* saveptr;
	char* szScriptName = strtok_r(szDefScript2, ",;", &saveptr);
	while (szScriptName)
	{
		szScriptName = Util::Trim(szScriptName);
		if (szScriptName[0] != '\0')
		{
			char szParam[1024];
			snprintf(szParam, 1024, "%s:", szScriptName);
			szParam[1024-1] = '\0';
			pNZBInfo->GetParameters()->SetParameter(szParam, "yes");
		}
		szScriptName = strtok_r(NULL, ",;", &saveptr);
	}
	free(szDefScript2);
}

void NZBScriptController::ExecuteScript(const char* szScript, const char* szNZBFilename, 
	const char* szDirectory, char** pCategory, int* iPriority, NZBParameterList* pParameterList)
{
	info("Executing nzb-process-script for %s", Util::BaseFileName(szNZBFilename));

	NZBScriptController* pScriptController = new NZBScriptController();
	pScriptController->SetScript(szScript);
	pScriptController->m_pCategory = pCategory;
	pScriptController->m_pParameterList = pParameterList;
	pScriptController->m_iPriority = iPriority;

	char szInfoName[1024];
	snprintf(szInfoName, 1024, "nzb-process-script for %s", Util::BaseFileName(szNZBFilename));
	szInfoName[1024-1] = '\0';
	pScriptController->SetInfoName(szInfoName);

	// remove trailing slash
	char szDir[1024];
	strncpy(szDir, szDirectory, 1024);
	szDir[1024-1] = '\0';
	int iLen = strlen(szDir);
	if (szDir[iLen-1] == PATH_SEPARATOR)
	{
		szDir[iLen-1] = '\0';
	}

	char szLogPrefix[1024];
	strncpy(szLogPrefix, Util::BaseFileName(szScript), 1024);
	szLogPrefix[1024-1] = '\0';
	if (char* ext = strrchr(szLogPrefix, '.')) *ext = '\0'; // strip file extension
	pScriptController->SetLogPrefix(szLogPrefix);
	pScriptController->m_iPrefixLen = strlen(szLogPrefix) + 2; // 2 = strlen(": ");

	pScriptController->SetDefaultLogKind(g_pOptions->GetProcessLogKind());

	pScriptController->SetEnvVar("NZBNP_DIRECTORY", szDir);
	pScriptController->SetEnvVar("NZBNP_FILENAME", szNZBFilename);

	pScriptController->Execute();

	delete pScriptController;
}

void NZBScriptController::AddMessage(Message::EKind eKind, const char* szText)
{
	szText = szText + m_iPrefixLen;

	if (!strncmp(szText, "[NZB] ", 6))
	{
		debug("Command %s detected", szText + 6);
		if (!strncmp(szText + 6, "CATEGORY=", 9))
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
				m_pParameterList->SetParameter(szParam, szValue + 1);
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
	pScriptController->SetEnvVar("NZBNA_NAME", pNZBInfo->GetName());
	pScriptController->SetEnvVar("NZBNA_FILENAME", pNZBInfo->GetFilename());
	pScriptController->SetEnvVar("NZBNA_CATEGORY", pNZBInfo->GetCategory());

	int iLastID = 0;
	int iMaxPriority = 0;

	for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
    {
        FileInfo* pFileInfo = *it;
		if (pFileInfo->GetNZBInfo() == pNZBInfo && ( pFileInfo->GetPriority() > iMaxPriority || iLastID == 0))
		{
			iMaxPriority = pFileInfo->GetPriority();
		}
		if (pFileInfo->GetNZBInfo() == pNZBInfo && pFileInfo->GetID() > iLastID)
		{
			iLastID = pFileInfo->GetID();
		}
	}

	char buf[100];

	snprintf(buf, 100, "%i", iLastID);
	pScriptController->SetEnvVar("NZBNA_LASTID", buf);

	snprintf(buf, 100, "%i", iMaxPriority);
	pScriptController->SetEnvVar("NZBNA_PRIORITY", buf);

	pScriptController->PrepareEnvParameters(pNZBInfo, NULL);

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

	SetDefaultLogKind(g_pOptions->GetProcessLogKind());

	Execute();

	free(m_szNZBName);
}

void SchedulerScriptController::StartScript(const char* szCommandLine)
{
	char** argv = NULL;
	if (!Util::SplitCommandLine(szCommandLine, &argv))
	{
		error("Could not execute scheduled process-script, failed to parse command line: %s", szCommandLine);
		return;
	}

	info("Executing scheduled process-script %s", Util::BaseFileName(argv[0]));

	SchedulerScriptController* pScriptController = new SchedulerScriptController();
	pScriptController->SetScript(argv[0]);
	pScriptController->SetArgs((const char**)argv, true);
	pScriptController->SetAutoDestroy(true);

	pScriptController->Start();
}

void SchedulerScriptController::Run()
{
	char szInfoName[1024];
	snprintf(szInfoName, 1024, "scheduled process-script %s", Util::BaseFileName(GetScript()));
	szInfoName[1024-1] = '\0';
	SetInfoName(szInfoName);

	char szLogPrefix[1024];
	strncpy(szLogPrefix, Util::BaseFileName(GetScript()), 1024);
	szLogPrefix[1024-1] = '\0';
	if (char* ext = strrchr(szLogPrefix, '.')) *ext = '\0'; // strip file extension
	SetLogPrefix(szLogPrefix);

	SetDefaultLogKind(g_pOptions->GetProcessLogKind());

	Execute();
}
