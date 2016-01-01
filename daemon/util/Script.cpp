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
#include "Script.h"
#include "Log.h"
#include "Util.h"
#include "FileSystem.h"
#include "Options.h"

// System global variable holding environments variables
extern char** environ;
extern char* (*g_EnvironmentVariables)[];

ScriptController::RunningScripts ScriptController::m_runningScripts;
Mutex ScriptController::m_runningMutex;

const int FORK_ERROR_EXIT_CODE = 254;

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
	pid_t			m_processId;
protected:
	virtual void	Run();
public:
	void			SetProcessId(pid_t processId) { m_processId = processId; }
};

void ChildWatchDog::Run()
{
	static const int WAIT_SECONDS = 60;
	time_t start = Util::CurrentTime();
	while (!IsStopped() && (Util::CurrentTime() - start) < WAIT_SECONDS)
	{
		usleep(10 * 1000);
	}

	if (!IsStopped())
	{
		info("Restarting hanging child process");
		kill(m_processId, SIGKILL);
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
	for (int i = 0; (*g_EnvironmentVariables)[i]; i++)
	{
		char* var = (*g_EnvironmentVariables)[i];
		// Ignore all env vars set by NZBGet.
		// This is to avoid the passing of env vars after program update (when NZBGet is
		// started from a script which was started by a previous instance of NZBGet).
		// Format: NZBXX_YYYY (XX are any two characters, YYYY are any number of any characters).
		if (!(!strncmp(var, "NZB", 3) && strlen(var) > 5 && var[5] == '_'))
		{
			Append(strdup(var));
		}
	}
}

void EnvironmentStrings::Append(char* string)
{
	m_strings.push_back(string);
}

#ifdef WIN32
/*
 * Returns environment block in format suitable for using with CreateProcess.
 * The allocated memory must be freed by caller using "free()".
 */
wchar_t* EnvironmentStrings::GetStrings()
{
	int size = 1;
	for (Strings::iterator it = m_strings.begin(); it != m_strings.end(); it++)
	{
		char* var = *it;
		size += strlen(var) + 1;
	}

	wchar_t* strings = (wchar_t*)malloc(size * 2);
	wchar_t* ptr = strings;
	for (Strings::iterator it = m_strings.begin(); it != m_strings.end(); it++)
	{
		char* var = *it;
		WString wstr(var);
		wcscpy(ptr, wstr);
		ptr += wstr.Length() + 1;
	}
	*ptr = '\0';

	return strings;
}

#else

/*
 * Returns environment block in format suitable for using with execve
 * The allocated memory must be freed by caller using "free()".
 */
char** EnvironmentStrings::GetStrings()
{
	char** strings = (char**)malloc((m_strings.size() + 1) * sizeof(char*));
	char** ptr = strings;
	for (Strings::iterator it = m_strings.begin(); it != m_strings.end(); it++)
	{
		char* var = *it;
		*ptr = var;
		ptr++;
	}
	*ptr = nullptr;

	return strings;
}
#endif


ScriptController::ScriptController()
{
	m_script = nullptr;
	m_workingDir = nullptr;
	m_args = nullptr;
	m_freeArgs = false;
	m_infoName = nullptr;
	m_logPrefix = nullptr;
	m_terminated = false;
	m_detached = false;
	m_processId = 0;
	ResetEnv();

	m_runningMutex.Lock();
	m_runningScripts.push_back(this);
	m_runningMutex.Unlock();
}

ScriptController::~ScriptController()
{
	if (m_freeArgs)
	{
		for (const char** argPtr = m_args; *argPtr; argPtr++)
		{
			free((char*)*argPtr);
		}
		free(m_args);
	}

	UnregisterRunningScript();
}

void ScriptController::UnregisterRunningScript()
{
	m_runningMutex.Lock();
	RunningScripts::iterator it = std::find(m_runningScripts.begin(), m_runningScripts.end(), this);
	if (it != m_runningScripts.end())
	{
		m_runningScripts.erase(it);
	}
	m_runningMutex.Unlock();
}

void ScriptController::ResetEnv()
{
	m_environmentStrings.Clear();
	m_environmentStrings.InitFromCurrentProcess();
}

void ScriptController::SetEnvVar(const char* name, const char* value)
{
	m_environmentStrings.Append(CString::FormatStr("%s=%s", name, value).Unbind());
}

void ScriptController::SetIntEnvVar(const char* name, int value)
{
	BString<1024> strValue("%i", value);
	SetEnvVar(name, strValue);
}

/**
 * If szStripPrefix is not nullptr, only options, whose names start with the prefix
 * are processed. The prefix is then stripped from the names.
 * If szStripPrefix is nullptr, all options are processed; without stripping.
 */
void ScriptController::PrepareEnvOptions(const char* stripPrefix)
{
	int prefixLen = stripPrefix ? strlen(stripPrefix) : 0;

	Options::OptEntries* optEntries = g_Options->LockOptEntries();

	for (Options::OptEntries::iterator it = optEntries->begin(); it != optEntries->end(); it++)
	{
		Options::OptEntry* optEntry = *it;

		if (stripPrefix && !strncmp(optEntry->GetName(), stripPrefix, prefixLen) && (int)strlen(optEntry->GetName()) > prefixLen)
		{
			SetEnvVarSpecial("NZBPO", optEntry->GetName() + prefixLen, optEntry->GetValue());
		}
		else if (!stripPrefix)
		{
			SetEnvVarSpecial("NZBOP", optEntry->GetName(), optEntry->GetValue());
		}
	}

	g_Options->UnlockOptEntries();
}

void ScriptController::SetEnvVarSpecial(const char* prefix, const char* name, const char* value)
{
	BString<1024> varname("%s_%s", prefix, name);

	// Original name
	SetEnvVar(varname, value);

	BString<1024> normVarname = *varname;

	// Replace special characters  with "_" and convert to upper case
	for (char* ptr = normVarname; *ptr; ptr++)
	{
		if (strchr(".:*!\"$%&/()=`+~#'{}[]@- ", *ptr)) *ptr = '_';
		*ptr = toupper(*ptr);
	}

	// Another env var with normalized name (replaced special chars and converted to upper case)
	if (strcmp(varname, normVarname))
	{
		SetEnvVar(normVarname, value);
	}
}

void ScriptController::PrepareArgs()
{
#ifdef WIN32
	if (!m_args)
	{
		// Special support for script languages:
		// automatically find the app registered for this extension and run it
		const char* extension = strrchr(GetScript(), '.');
		if (extension && strcasecmp(extension, ".exe") && strcasecmp(extension, ".bat") && strcasecmp(extension, ".cmd"))
		{
			debug("Looking for associated program for %s", extension);
			char command[512];
			int bufLen = 512-1;
			if (Util::RegReadStr(HKEY_CLASSES_ROOT, extension, nullptr, command, &bufLen))
			{
				command[bufLen] = '\0';
				debug("Extension: %s", command);

				bufLen = 512-1;
				if (Util::RegReadStr(HKEY_CLASSES_ROOT, BString<1024>("%s\\shell\\open\\command", command),
					nullptr, command, &bufLen))
				{
					command[bufLen] = '\0';
					debug("Command: %s", command);

					DWORD_PTR args[] = { (DWORD_PTR)GetScript(), (DWORD_PTR)0 };
					if (FormatMessage(FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ARGUMENT_ARRAY, command, 0, 0,
									  m_cmdLine, sizeof(m_cmdLine), (va_list*)args))
					{
						debug("CmdLine: %s", m_cmdLine);
						return;
					}
				}
			}
			warn("Could not found associated program for %s. Trying to execute %s directly",
				extension, FileSystem::BaseFileName(GetScript()));
		}
	}
#endif

	if (!m_args)
	{
		m_stdArgs[0] = GetScript();
		m_stdArgs[1] = nullptr;
		SetArgs(m_stdArgs, false);
	}
}

int ScriptController::Execute()
{
	PrepareEnvOptions(nullptr);
	PrepareArgs();

	int exitCode = 0;

#ifdef CHILD_WATCHDOG
	bool childConfirmed = false;
	while (!childConfirmed && !m_terminated)
	{
#endif

	int pipein = StartProcess();
	if (pipein == -1)
	{
		return -1;
	}

	// open the read end
	m_readpipe = fdopen(pipein, "r");
	if (!m_readpipe)
	{
		PrintMessage(Message::mkError, "Could not open pipe to %s", m_infoName);
		close(pipein);
		return -1;
	}

#ifdef CHILD_WATCHDOG
	debug("Creating child watchdog");
	ChildWatchDog* watchDog = new ChildWatchDog();
	watchDog->SetAutoDestroy(false);
	watchDog->SetProcessId(m_processId);
	watchDog->Start();
#endif

	char* buf = (char*)malloc(10240);

	debug("Entering pipe-loop");
	bool firstLine = true;
	bool startError = false;
	while (!m_terminated && !m_detached && !feof(m_readpipe))
	{
		if (ReadLine(buf, 10240, m_readpipe) && m_readpipe)
		{
#ifdef CHILD_WATCHDOG
			if (!childConfirmed)
			{
				childConfirmed = true;
				watchDog->Stop();
				debug("Child confirmed");
				continue;
			}
#endif
			if (firstLine && !strncmp(buf, "[ERROR] Could not start ", 24))
			{
				startError = true;
			}
			ProcessOutput(buf);
			firstLine = false;
		}
	}
	debug("Exited pipe-loop");

#ifdef CHILD_WATCHDOG
	debug("Destroying WatchDog");
	if (!childConfirmed)
	{
		watchDog->Stop();
	}
	while (watchDog->IsRunning())
	{
		usleep(5 * 1000);
	}
	delete watchDog;
#endif

	free(buf);
	if (m_readpipe)
	{
		fclose(m_readpipe);
	}

	if (m_terminated && m_infoName)
	{
		warn("Interrupted %s", m_infoName);
	}

	exitCode = 0;

	if (!m_terminated && !m_detached)
	{
		exitCode = WaitProcess();
#ifndef WIN32
		if (exitCode == FORK_ERROR_EXIT_CODE && startError)
		{
			exitCode = -1;
		}
#endif
	}

#ifdef CHILD_WATCHDOG
	}	// while (!bChildConfirmed && !m_bTerminated)
#endif

	debug("Exit code %i", exitCode);

	return exitCode;
}

#ifdef WIN32
void ScriptController::BuildCommandLine(char* cmdLineBuf, int bufSize)
{
	int usedLen = 0;
	for (const char** argPtr = m_args; *argPtr; argPtr++)
	{
		const char* arg = *argPtr;
		int len = strlen(arg);
		bool endsWithBackslash = arg[len - 1] == '\\';
		bool isDirectPath = !strncmp(arg, "\\\\?", 3);
		snprintf(cmdLineBuf + usedLen, bufSize - usedLen, endsWithBackslash && ! isDirectPath ? "\"%s\\\" " : "\"%s\" ", arg);
		usedLen += len + 3 + (endsWithBackslash ? 1 : 0);
	}
	cmdLineBuf[usedLen < bufSize ? usedLen - 1 : bufSize - 1] = '\0';
}
#endif

/*
* Returns file descriptor of the read-pipe or -1 on error.
*/
int ScriptController::StartProcess()
{
#ifdef WIN32
	char* cmdLine = m_cmdLine;
	char cmdLineBuf[2048];
	if (m_args)
	{
		BuildCommandLine(cmdLineBuf, sizeof(cmdLineBuf));
		cmdLine = cmdLineBuf;
	}

	WString wideWorkingDir = FileSystem::UtfPathToWidePath(m_workingDir);
	if (strlen(m_workingDir) > 260 - 14)
	{
		GetShortPathNameW(wideWorkingDir, wideWorkingDir, wideWorkingDir.Length() + 1);
	}

	// create pipes to write and read data
	HANDLE readPipe, writePipe;
	SECURITY_ATTRIBUTES securityAttributes = { 0 };
	securityAttributes.nLength = sizeof(securityAttributes);
	securityAttributes.bInheritHandle = TRUE;

	CreatePipe(&readPipe, &writePipe, &securityAttributes, 0);

	SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFOW startupInfo = { 0 };
	startupInfo.cb = sizeof(startupInfo);
	startupInfo.dwFlags = STARTF_USESTDHANDLES;
	startupInfo.hStdInput = 0;
	startupInfo.hStdOutput = writePipe;
	startupInfo.hStdError = writePipe;

	PROCESS_INFORMATION processInfo = { 0 };

	wchar_t* environmentStrings = m_environmentStrings.GetStrings();

	BOOL ok = CreateProcessW(nullptr, WString(cmdLine), nullptr, nullptr, TRUE,
		NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
		environmentStrings, wideWorkingDir, &startupInfo, &processInfo);
	if (!ok)
	{
		DWORD errCode = GetLastError();
		char errMsg[255];
		errMsg[255 - 1] = '\0';
		if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, errCode, 0, errMsg, 255, nullptr))
		{
			PrintMessage(Message::mkError, "Could not start %s: %s", m_infoName, errMsg);
		}
		else
		{
			PrintMessage(Message::mkError, "Could not start %s: error %i", m_infoName, errCode);
		}
		if (!FileSystem::FileExists(m_script))
		{
			PrintMessage(Message::mkError, "Could not find file %s", m_script);
		}
		if (wcslen(wideWorkingDir) > 260)
		{
			PrintMessage(Message::mkError, "Could not build short path for %s", m_workingDir);
		}
		free(environmentStrings);
		CloseHandle(readPipe);
		CloseHandle(writePipe);
		return -1;
	}

	free(environmentStrings);

	debug("Child Process-ID: %i", (int)processInfo.dwProcessId);

	m_processId = processInfo.hProcess;

	// close unused "write" end
	CloseHandle(writePipe);

	int pipein = _open_osfhandle((intptr_t)readPipe, _O_RDONLY);
	return pipein;

#else

	int p[2];
	int pipein;
	int pipeout;

	// create the pipe
	if (pipe(p))
	{
		PrintMessage(Message::mkError, "Could not open pipe: errno %i", errno);
		return -1;
	}

	char** environmentStrings = m_environmentStrings.GetStrings();

	pipein = p[0];
	pipeout = p[1];

	debug("forking");
	pid_t pid = fork();

	if (pid == -1)
	{
		PrintMessage(Message::mkError, "Could not start %s: errno %i", m_infoName, errno);
		free(environmentStrings);
		close(pipein);
		close(pipeout);
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

		chdir(m_workingDir);
		environ = environmentStrings;
		execvp(m_script, (char* const*)m_args);

		if (errno == EACCES)
		{
			fprintf(stdout, "[WARNING] Fixing permissions for %s\n", m_script);
			fflush(stdout);
			FileSystem::FixExecPermission(m_script);
			execvp(m_script, (char* const*)m_args);
		}

		// NOTE: the text "[ERROR] Could not start " is checked later,
		// by changing adjust the dependent code below.
		fprintf(stdout, "[ERROR] Could not start %s: %s", m_script, strerror(errno));
		fflush(stdout);
		_exit(FORK_ERROR_EXIT_CODE);
	}

	// continue the first instance
	debug("forked");
	debug("Child Process-ID: %i", (int)pid);

	free(environmentStrings);

	m_processId = pid;

	// close unused "write" end
	close(pipeout);
#endif

	return pipein;
}

int ScriptController::WaitProcess()
{
#ifdef WIN32
	WaitForSingleObject(m_processId, INFINITE);
	DWORD exitCode = 0;
	GetExitCodeProcess(m_processId, &exitCode);
	return exitCode;
#else
	int status = 0;
	waitpid(m_processId, &status, 0);
	if (WIFEXITED(status))
	{
		int exitCode = WEXITSTATUS(status);
		return exitCode;
	}
	return 0;
#endif
}

void ScriptController::Terminate()
{
	debug("Stopping %s", m_infoName);
	m_terminated = true;

#ifdef WIN32
	BOOL ok = TerminateProcess(m_processId, -1);
	if (ok)
	{
		// wait 60 seconds for process to terminate
		WaitForSingleObject(m_processId, 60 * 1000);
	}
	else
	{
		DWORD exitCode = 0;
		GetExitCodeProcess(m_processId, &exitCode);
		ok = exitCode != STILL_ACTIVE;
	}
#else
	pid_t killId = m_processId;
	if (getpgid(killId) == killId)
	{
		// if the child process has its own group (setsid() was successful), kill the whole group
		killId = -killId;
	}
	bool ok = killId && kill(killId, SIGKILL) == 0;
#endif

	if (ok)
	{
		debug("Terminated %s", m_infoName);
	}
	else
	{
		error("Could not terminate %s", m_infoName);
	}

	debug("Stopped %s", m_infoName);
}

void ScriptController::TerminateAll()
{
	m_runningMutex.Lock();
	for (RunningScripts::iterator it = m_runningScripts.begin(); it != m_runningScripts.end(); it++)
	{
		ScriptController* script = *it;
		if (script->m_processId && !script->m_detached)
		{
			script->Terminate();
		}
	}
	m_runningMutex.Unlock();
}

void ScriptController::Detach()
{
	debug("Detaching %s", m_infoName);
	m_detached = true;
	FILE* readpipe = m_readpipe;
	m_readpipe = nullptr;
	fclose(readpipe);
}

void ScriptController::Resume()
{
	m_terminated = false;
	m_detached = false;
	m_processId = 0;
}

bool ScriptController::ReadLine(char* buf, int bufSize, FILE* stream)
{
	return fgets(buf, bufSize, stream);
}

void ScriptController::ProcessOutput(char* text)
{
	debug("Processing output received from script");

	for (char* pend = text + strlen(text) - 1; pend >= text && (*pend == '\n' || *pend == '\r' || *pend == ' '); pend--) *pend = '\0';

	if (text[0] == '\0')
	{
		// skip empty lines
		return;
	}

	if (!strncmp(text, "[INFO] ", 7))
	{
		PrintMessage(Message::mkInfo, "%s", text + 7);
	}
	else if (!strncmp(text, "[WARNING] ", 10))
	{
		PrintMessage(Message::mkWarning, "%s", text + 10);
	}
	else if (!strncmp(text, "[ERROR] ", 8))
	{
		PrintMessage(Message::mkError, "%s", text + 8);
	}
	else if (!strncmp(text, "[DETAIL] ", 9))
	{
		PrintMessage(Message::mkDetail, "%s", text + 9);
	}
	else if (!strncmp(text, "[DEBUG] ", 8))
	{
		PrintMessage(Message::mkDebug, "%s", text + 8);
	}
	else
	{
		PrintMessage(Message::mkInfo, "%s", text);
	}

	debug("Processing output received from script - completed");
}

void ScriptController::AddMessage(Message::EKind kind, const char* text)
{
	switch (kind)
	{
		case Message::mkDetail:
			detail("%s", text);
			break;

		case Message::mkInfo:
			info("%s", text);
			break;

		case Message::mkWarning:
			warn("%s", text);
			break;

		case Message::mkError:
			error("%s", text);
			break;

		case Message::mkDebug:
			debug("%s", text);
			break;
	}
}

void ScriptController::PrintMessage(Message::EKind kind, const char* format, ...)
{
	BString<1024> tmp2;

	va_list ap;
	va_start(ap, format);
	tmp2.FormatV(format, ap);
	va_end(ap);

	if (m_logPrefix)
	{
		AddMessage(kind, BString<1024>("%s: %s", m_logPrefix, *tmp2));
	}
	else
	{
		AddMessage(kind, tmp2);
	}
}
