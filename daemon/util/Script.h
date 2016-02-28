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


#ifndef SCRIPT_H
#define SCRIPT_H

#include "Thread.h"
#include "Log.h"

class EnvironmentStrings
{
private:
	typedef std::vector<CString>		Strings;

	Strings				m_strings;

public:
	void				Clear();
	void				InitFromCurrentProcess();
	void				Append(const char* envstr);
	void				Append(CString&& envstr);
#ifdef WIN32
	std::unique_ptr<wchar_t[]>	GetStrings();
#else
	std::vector<char*>			GetStrings();
#endif
};

class ScriptController
{
private:
	typedef std::vector<CString> ArgList;

	const char*			m_script = nullptr;
	ArgList				m_args;
	const char*			m_workingDir = nullptr;
	const char*			m_infoName = nullptr;
	const char*			m_logPrefix = nullptr;
	EnvironmentStrings	m_environmentStrings;
	bool				m_terminated = false;
	bool				m_detached = false;
	FILE*				m_readpipe;
#ifdef WIN32
	HANDLE				m_processId = 0;
	char				m_cmdLine[2048];
#else
	pid_t				m_processId = 0;
#endif

	typedef std::vector<ScriptController*>	RunningScripts;
	static RunningScripts	m_runningScripts;
	static Mutex			m_runningMutex;

protected:
	void				ProcessOutput(char* text);
	virtual bool		ReadLine(char* buf, int bufSize, FILE* stream);
	void				PrintMessage(Message::EKind kind, const char* format, ...) PRINTF_SYNTAX(3);
	virtual void		AddMessage(Message::EKind kind, const char* text);
	bool				GetTerminated() { return m_terminated; }
	void				ResetEnv();
	void				PrepareEnvOptions(const char* stripPrefix);
	void				PrepareArgs();
	int					StartProcess();
	int					WaitProcess();
#ifdef WIN32
	void				BuildCommandLine(char* cmdLineBuf, int bufSize);
#endif
	void				UnregisterRunningScript();

public:
						ScriptController();
	virtual				~ScriptController();
	int					Execute();
	void				Terminate();
	void				Resume();
	void				Detach();
	static void			TerminateAll();

	void				SetScript(const char* script) { m_script = script; }
	const char*			GetScript() { return m_script; }
	void				SetWorkingDir(const char* workingDir) { m_workingDir = workingDir; }
	void				SetArgs(ArgList&& args) { m_args = std::move(args); }
	void				SetInfoName(const char* infoName) { m_infoName = infoName; }
	const char*			GetInfoName() { return m_infoName; }
	void				SetLogPrefix(const char* logPrefix) { m_logPrefix = logPrefix; }
	void				SetEnvVar(const char* name, const char* value);
	void				SetEnvVarSpecial(const char* prefix, const char* name, const char* value);
	void				SetIntEnvVar(const char* name, int value);
	void				Reset();
};

#endif
