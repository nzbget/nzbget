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


#ifndef SCRIPT_H
#define SCRIPT_H

#include "NString.h"
#include "Container.h"
#include "Thread.h"
#include "Log.h"

class EnvironmentStrings
{
public:
	void Clear();
	void InitFromCurrentProcess();
	void Append(const char* envstr);
	void Append(CString&& envstr);
#ifdef WIN32
	std::unique_ptr<wchar_t[]> GetStrings();
#else
	std::vector<char*> GetStrings();
#endif

private:
	typedef std::vector<CString> Strings;

	Strings m_strings;
};

class ScriptController
{
public:
	typedef std::vector<CString> ArgList;

	ScriptController();
	virtual ~ScriptController();
	int Execute();
	void Terminate();
	bool Break();
	void Resume();
	void Detach();
	static void TerminateAll();

	const char* GetScript() { return !m_args.empty() ? *m_args[0] : nullptr; }
	void SetWorkingDir(const char* workingDir) { m_workingDir = workingDir; }
	void SetArgs(ArgList&& args) { m_args = std::move(args); }
	void SetInfoName(const char* infoName) { m_infoName = infoName; }
	const char* GetInfoName() { return m_infoName; }
	void SetLogPrefix(const char* logPrefix) { m_logPrefix = logPrefix; }
	void SetEnvVar(const char* name, const char* value);
	void SetEnvVarSpecial(const char* prefix, const char* name, const char* value);
	void SetIntEnvVar(const char* name, int value);

protected:
	void ProcessOutput(char* text);
	virtual bool ReadLine(char* buf, int bufSize, FILE* stream);
	void PrintMessage(Message::EKind kind, const char* format, ...) PRINTF_SYNTAX(3);
	virtual void AddMessage(Message::EKind kind, const char* text);
	bool GetTerminated() { return m_terminated; }
	void ResetEnv();
	void PrepareEnvOptions(const char* stripPrefix);
	void PrepareArgs();
	virtual const char* GetOptValue(const char* name, const char* value) { return value; }
	void StartProcess(int* pipein, int* pipeout);
	int WaitProcess();
	void SetNeedWrite(bool needWrite) { m_needWrite = needWrite; }
	void Write(const char* str);
#ifdef WIN32
	void BuildCommandLine(char* cmdLineBuf, int bufSize);
#endif
	void UnregisterRunningScript();

private:
	ArgList m_args;
	const char* m_workingDir = nullptr;
	CString m_infoName;
	const char* m_logPrefix = nullptr;
	EnvironmentStrings m_environmentStrings;
	bool m_terminated = false;
	bool m_completed = false;
	bool m_detached = false;
	bool m_needWrite = false;
	FILE* m_readpipe = 0;
	FILE* m_writepipe = 0;
#ifdef WIN32
	HANDLE m_processId = 0;
	DWORD m_dwProcessId = 0;
	char m_cmdLine[2048];
#else
	pid_t m_processId = 0;
#endif

	typedef std::vector<ScriptController*> RunningScripts;

	static RunningScripts m_runningScripts;
	static Mutex m_runningMutex;
};

#endif
