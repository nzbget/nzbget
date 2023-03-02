/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2007-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
 *    the child process is assumed to be hanging and will be killed. Another attempt
 *    will be made.
 */

class ChildWatchDog : public Thread
{
public:
	void SetProcessId(pid_t processId) { m_processId = processId; }
	void SetInfoName(const char* infoName) { m_infoName = infoName; }
protected:
	virtual void Run();
private:
	pid_t m_processId;
	CString m_infoName;
};

void ChildWatchDog::Run()
{
	static const int WAIT_SECONDS = 60;
	time_t start = Util::CurrentTime();
	while (!IsStopped() && (Util::CurrentTime() - start) < WAIT_SECONDS)
	{
		Util::Sleep(10);
	}

	if (!IsStopped())
	{
		info("Restarting hanging child process for %s", *m_infoName);
		kill(m_processId, SIGKILL);
	}
}
#endif


void EnvironmentStrings::Clear()
{
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
			Append(var);
		}
	}
}

void EnvironmentStrings::Append(const char* envstr)
{
	m_strings.emplace_back(envstr);
}

void EnvironmentStrings::Append(CString&& envstr)
{
	m_strings.push_back(std::move(envstr));
}

#ifdef WIN32
/*
 * Returns environment block in format suitable for using with CreateProcess.
 */
std::unique_ptr<wchar_t[]> EnvironmentStrings::GetStrings()
{
	int size = 1;
	for (CString& var : m_strings)
	{
		size += var.Length() + 1;
	}

	std::unique_ptr<wchar_t[]> strings = std::make_unique<wchar_t[]>(size * 2);

	wchar_t* ptr = strings.get();
	for (CString& var : m_strings)
	{
		WString wstr(var);
		wcscpy(ptr, wstr);
		ptr += wstr.Length() + 1;
	}
	*ptr = '\0';

	return strings;
}

#else

/*
 * Returns environment block in format suitable for using with execve.
 */
std::vector<char*> EnvironmentStrings::GetStrings()
{
	std::vector<char*> strings;
	strings.reserve(m_strings.size() + 1);
	std::copy(m_strings.begin(), m_strings.end(), std::back_inserter(strings));
	strings.push_back(nullptr);
	return strings;
}
#endif


ScriptController::ScriptController()
{
	ResetEnv();

	Guard guard(m_runningMutex);
	m_runningScripts.push_back(this);
}

ScriptController::~ScriptController()
{
	UnregisterRunningScript();
}

void ScriptController::UnregisterRunningScript()
{
	Guard guard(m_runningMutex);
	m_runningScripts.erase(std::remove(m_runningScripts.begin(), m_runningScripts.end(), this), m_runningScripts.end());
}

void ScriptController::ResetEnv()
{
	m_environmentStrings.Clear();
	m_environmentStrings.InitFromCurrentProcess();
}

void ScriptController::SetEnvVar(const char* name, const char* value)
{
	m_environmentStrings.Append(CString::FormatStr("%s=%s", name, value));
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

	for (Options::OptEntry& optEntry : g_Options->GuardOptEntries())
	{
		const char* value = GetOptValue(optEntry.GetName(), optEntry.GetValue());
		if (stripPrefix && !strncmp(optEntry.GetName(), stripPrefix, prefixLen) &&
			(int)strlen(optEntry.GetName()) > prefixLen)
		{
			SetEnvVarSpecial("NZBPO", optEntry.GetName() + prefixLen, value);
		}
		else if (!stripPrefix)
		{
			SetEnvVarSpecial("NZBOP", optEntry.GetName(), value);
		}
	}
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
	if (m_args.size() == 1 && !Util::EmptyStr(g_Options->GetShellOverride()))
	{
		const char* extension = strrchr(m_args[0], '.');

		Tokenizer tok(g_Options->GetShellOverride(), ",;");
		while (CString shellover = tok.Next())
		{
			char* shellcmd = strchr(shellover, '=');
			if (shellcmd)
			{
				*shellcmd = '\0';
				shellcmd++;

				if (!strcasecmp(extension, shellover))
				{
					debug("Using shell override for %s: %s", extension, shellcmd);
					m_args.emplace(m_args.begin(), shellcmd);
					break;
				}
			}
		}
	}

#ifdef WIN32
	*m_cmdLine = '\0';

	if (m_args.size() == 1)
	{
		// Special support for script languages:
		// automatically find the app registered for this extension and run it
		const char* extension = strrchr(m_args[0], '.');
		if (extension && strcasecmp(extension, ".exe") && strcasecmp(extension, ".bat") && strcasecmp(extension, ".cmd"))
		{
			debug("Looking for associated program for %s", extension);
			char command[512];
			int bufLen = 512 - 1;
			if (Util::RegReadStr(HKEY_CLASSES_ROOT, extension, nullptr, command, &bufLen))
			{
				command[bufLen] = '\0';
				debug("Extension: %s", command);

				bufLen = 512 - 1;
				if (Util::RegReadStr(HKEY_CLASSES_ROOT, BString<1024>("%s\\shell\\open\\command", command),
					nullptr, command, &bufLen))
				{
					command[bufLen] = '\0';
					debug("Command: %s", command);

					DWORD_PTR args[] = {(DWORD_PTR)*m_args[0], (DWORD_PTR)0};
					if (FormatMessage(FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ARGUMENT_ARRAY, command, 0, 0,
						m_cmdLine, sizeof(m_cmdLine), (va_list*)args))
					{
						Util::TrimRight(Util::ReduceStr(m_cmdLine, "*", ""));
						debug("CmdLine: %s", m_cmdLine);
						return;
					}
				}
			}
			warn("Could not find associated program for %s. Trying to execute %s directly",
				extension, FileSystem::BaseFileName(m_args[0]));
		}
	}
#endif
}

int ScriptController::Execute()
{
	PrepareEnvOptions(nullptr);
	PrepareArgs();

	m_completed = false;
	int exitCode = 0;

#ifdef CHILD_WATCHDOG
	bool childConfirmed = false;
	while (!childConfirmed && !m_terminated)
	{
#endif

	int pipein = -1, pipeout = -1;
	StartProcess(&pipein, &pipeout);
	if (pipein == -1)
	{
		m_completed = true;
		return -1;
	}

	// open the read end
	m_readpipe = fdopen(pipein, "r");
	if (!m_readpipe)
	{
		PrintMessage(Message::mkError, "Could not open read pipe to %s", *m_infoName);
		close(pipein);
		close(pipeout);
		m_completed = true;
		return -1;
	}

	m_writepipe = 0;
	if (m_needWrite)
	{
		// open the write end
		m_writepipe = fdopen(pipeout, "w");
		if (!m_writepipe)
		{
			PrintMessage(Message::mkError, "Could not open write pipe to %s", *m_infoName);
			close(pipein);
			close(pipeout);
			m_completed = true;
			return -1;
		}
	}

#ifdef CHILD_WATCHDOG
	debug("Creating child watchdog");
	ChildWatchDog watchDog;
	watchDog.SetAutoDestroy(false);
	watchDog.SetProcessId(m_processId);
	watchDog.SetInfoName(m_infoName);
	watchDog.Start();
#endif

	CharBuffer buf(1024 * 10);

	debug("Entering pipe-loop");
	bool firstLine = true;
	bool startError = false;
	while (!m_terminated && !m_detached && !feof(m_readpipe))
	{
		if (ReadLine(buf, buf.Size(), m_readpipe) && m_readpipe)
		{
#ifdef CHILD_WATCHDOG
			if (!childConfirmed)
			{
				childConfirmed = true;
				watchDog.Stop();
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
		watchDog.Stop();
	}
	while (watchDog.IsRunning())
	{
		Util::Sleep(5);
	}
#endif

	if (m_readpipe)
	{
		fclose(m_readpipe);
	}

	if (m_writepipe)
	{
		fclose(m_writepipe);
	}

	if (m_terminated && m_infoName)
	{
		warn("Interrupted %s", *m_infoName);
	}

	exitCode = 0;

	if (!m_detached)
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
	m_completed = true;
	return exitCode;
}

#ifdef WIN32
void ScriptController::BuildCommandLine(char* cmdLineBuf, int bufSize)
{
	int usedLen = 0;
	for (const char* arg : m_args)
	{
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
void ScriptController::StartProcess(int* pipein, int* pipeout)
{
	CString workingDir = m_workingDir;
	if (workingDir.Empty())
	{
		workingDir = FileSystem::GetCurrentDirectory();
	}

	const char* script = m_args[0];

#ifdef WIN32
	char* cmdLine = m_cmdLine;
	char cmdLineBuf[2048];
	if (!*m_cmdLine)
	{
		BuildCommandLine(cmdLineBuf, sizeof(cmdLineBuf));
		cmdLine = cmdLineBuf;
	}

	debug("Starting process: %s", cmdLine);

	WString wideWorkingDir = FileSystem::UtfPathToWidePath(workingDir);
	if (strlen(workingDir) > 260 - 14)
	{
		GetShortPathNameW(wideWorkingDir, wideWorkingDir, wideWorkingDir.Length() + 1);
	}

	// create pipes to write and read data
	HANDLE readPipe, readProcPipe;
	HANDLE writePipe = 0, writeProcPipe = 0;
	SECURITY_ATTRIBUTES securityAttributes = { 0 };
	securityAttributes.nLength = sizeof(securityAttributes);
	securityAttributes.bInheritHandle = TRUE;

	CreatePipe(&readPipe, &readProcPipe, &securityAttributes, 0);
	SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

	if (m_needWrite)
	{
		CreatePipe(&writeProcPipe, &writePipe, &securityAttributes, 0);
		SetHandleInformation(writePipe, HANDLE_FLAG_INHERIT, 0);
	}

	STARTUPINFOW startupInfo = { 0 };
	startupInfo.cb = sizeof(startupInfo);
	startupInfo.dwFlags = STARTF_USESTDHANDLES;
	startupInfo.hStdInput = writeProcPipe;
	startupInfo.hStdOutput = readProcPipe;
	startupInfo.hStdError = readProcPipe;

	PROCESS_INFORMATION processInfo = { 0 };

	std::unique_ptr<wchar_t[]> environmentStrings = m_environmentStrings.GetStrings();

	BOOL ok = CreateProcessW(nullptr, WString(cmdLine), nullptr, nullptr, TRUE,
		NORMAL_PRIORITY_CLASS | CREATE_NEW_PROCESS_GROUP | CREATE_UNICODE_ENVIRONMENT,
		environmentStrings.get(), wideWorkingDir, &startupInfo, &processInfo);
	if (!ok)
	{
		DWORD errCode = GetLastError();
		char errMsg[255];
		errMsg[255 - 1] = '\0';
		if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, errCode, 0, errMsg, 255, nullptr))
		{
			PrintMessage(Message::mkError, "Could not start %s: %s", *m_infoName, errMsg);
		}
		else
		{
			PrintMessage(Message::mkError, "Could not start %s: error %i", *m_infoName, errCode);
		}
		if (!FileSystem::FileExists(script))
		{
			PrintMessage(Message::mkError, "Could not find file %s", script);
		}
		if (wcslen(wideWorkingDir) > 260)
		{
			PrintMessage(Message::mkError, "Could not build short path for %s", workingDir);
		}
		CloseHandle(readPipe);
		CloseHandle(readProcPipe);
		CloseHandle(writePipe);
		CloseHandle(writeProcPipe);
		return;
	}

	debug("Child Process-ID: %i", (int)processInfo.dwProcessId);

	m_processId = processInfo.hProcess;
	m_dwProcessId = processInfo.dwProcessId;

	// close unused pipe ends
	CloseHandle(readProcPipe);
	CloseHandle(writeProcPipe);

	*pipein = _open_osfhandle((intptr_t)readPipe, _O_RDONLY);
	if (m_needWrite)
	{
		*pipeout = _open_osfhandle((intptr_t)writePipe, _O_WRONLY);
	}

#else

	int pin[] = {0, 0};
	int pout[] = {0, 0};

	// create the pipes
	if (pipe(pin))
	{
		PrintMessage(Message::mkError, "Could not open read pipe: errno %i", errno);
		return;
	}
	if (m_needWrite && pipe(pout))
	{
		PrintMessage(Message::mkError, "Could not open write pipe: errno %i", errno);
		close(pin[0]);
		close(pin[1]);
		return;
	}

	*pipein = pin[0];
	*pipeout = pout[1];

	std::vector<char*> environmentStrings = m_environmentStrings.GetStrings();
	char** envdata = environmentStrings.data();

	ArgList args;
	std::copy(m_args.begin(), m_args.end(), std::back_inserter(args));
	args.emplace_back(nullptr);
	char* const* argdata = (char* const*)args.data();

#ifdef DEBUG
	debug("Starting  process: %s", script);
	for (const char* arg : m_args)
	{
		debug("arg: %s", arg);
	}
#endif

	debug("forking");
	pid_t pid = fork();

	if (pid == -1)
	{
		PrintMessage(Message::mkError, "Could not start %s: errno %i", *m_infoName, errno);
		close(pin[0]);
		close(pin[1]);
		if (m_needWrite)
		{
			close(pout[0]);
			close(pout[1]);
		}
		return;
	}
	else if (pid == 0)
	{
		// here goes the second instance

		// only certain functions may be used here or the program may hang.
		// for a list of functions see chapter "async-signal-safe functions" in
		// http://man7.org/linux/man-pages/man7/signal.7.html

		// create new process group (see Terminate() where it is used)
		setsid();

		// make the pipeout to be the same as stdout and stderr
		dup2(pin[1], 1);
		dup2(pin[1], 2);
		close(pin[0]);
		close(pin[1]);

		if (m_needWrite)
		{
			// make the pipein to be the same as stdin
			dup2(pout[0], 0);
			close(pout[0]);
			close(pout[1]);
		}

#ifdef CHILD_WATCHDOG
		fputc( '\n', stdout );
		fsync(1);
#endif

		if ( chdir( workingDir ) == -1 )
        {
            fprintf( stdout, "[ERROR] Could not change working directory for %s: %s\n", script, strerror(errno) );
            fsync(1);
            _exit(FORK_ERROR_EXIT_CODE);
        }
		environ = envdata;

		execvp( script, argdata );

		if ( errno == EACCES )
		{
			fprintf( stdout, "[WARNING] Fixing permissions for %s\n", script );
            fsync(1);
			FileSystem::FixExecPermission(script);
			execvp(script, argdata);
		}

		// NOTE: the text "[ERROR] Could not start " is checked later,
		// if changed, adjust the dependent code below.
        fprintf( stdout, "[ERROR] Could not start %s: %s\n", script, strerror(errno) );
		fsync(1);
		_exit(FORK_ERROR_EXIT_CODE);
	}

	// continue the first instance
	debug("forked");
	debug("Child Process-ID: %i", (int)pid);

	m_processId = pid;

	// close unused pipe ends
	close(pin[1]);
	if (m_needWrite)
	{
		close(pout[0]);
	}
#endif
}

int ScriptController::WaitProcess()
{
#ifdef WIN32
	// wait max 60 seconds for terminated processes
	WaitForSingleObject(m_processId, m_terminated ? 60 * 1000 : INFINITE);
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
	debug("Stopping %s", *m_infoName);
	m_terminated = true;

#ifdef WIN32
	BOOL ok = TerminateProcess(m_processId, -1) || m_completed;
#else
	pid_t killId = m_processId;
	if (getpgid(killId) == killId)
	{
		// if the child process has its own group (setsid() was successful), kill the whole group
		killId = -killId;
	}
	bool ok = (killId && kill(killId, SIGKILL) == 0) || m_completed;
#endif

	if (ok)
	{
		debug("Terminated %s", *m_infoName);
	}
	else
	{
		error("Could not terminate %s", *m_infoName);
	}

	debug("Stopped %s", *m_infoName);
}

void ScriptController::TerminateAll()
{
	Guard guard(m_runningMutex);
	for (ScriptController* script : m_runningScripts)
	{
		if (script->m_processId && !script->m_detached)
		{
			// send break signal and wait up to 5 seconds for graceful termination
			if (script->Break())
			{
				time_t curtime = Util::CurrentTime();
				while (!script->m_completed && std::abs(curtime - Util::CurrentTime()) <= 10)
				{
					Util::Sleep(100);
				}
			}
			script->Terminate();
		}
	}
}

bool ScriptController::Break()
{
	debug("Sending break signal to %s", *m_infoName);

#ifdef WIN32
	BOOL ok = GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, m_dwProcessId);
#else
	bool ok = kill(m_processId, SIGINT) == 0;
#endif

	if (ok)
	{
		debug("Sent break signal to %s", *m_infoName);
	}
	else
	{
		warn("Could not send break signal to %s", *m_infoName);
	}

	return ok;
}

void ScriptController::Detach()
{
	debug("Detaching %s", *m_infoName);
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

void ScriptController::Write(const char* str)
{
	fwrite(str, 1, strlen(str), m_writepipe);
	fflush(m_writepipe);
}
