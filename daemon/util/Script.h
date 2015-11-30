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
	typedef std::vector<char*>		Strings;

	Strings				m_strings;

public:
						EnvironmentStrings();
						~EnvironmentStrings();
	void				Clear();
	void				InitFromCurrentProcess();
	void				Append(char* string);
#ifdef WIN32
	char*				GetStrings();
#else
	char**				GetStrings();
#endif
};

class ScriptController
{
private:
	const char*			m_script;
	const char*			m_workingDir;
	const char**		m_args;
	bool				m_freeArgs;
	const char*			m_stdArgs[2];
	const char*			m_infoName;
	const char*			m_logPrefix;
	EnvironmentStrings	m_environmentStrings;
	bool				m_terminated;
	bool				m_detached;
	FILE*				m_readpipe;
#ifdef WIN32
	HANDLE				m_processId;
	char				m_cmdLine[2048];
#else
	pid_t				m_processId;
#endif

	typedef std::vector<ScriptController*>	RunningScripts;
	static RunningScripts	m_runningScripts;
	static Mutex			m_runningMutex;

protected:
	void				ProcessOutput(char* text);
	virtual bool		ReadLine(char* buf, int bufSize, FILE* stream);
	void				PrintMessage(Message::EKind kind, const char* format, ...);
	virtual void		AddMessage(Message::EKind kind, const char* text);
	bool				GetTerminated() { return m_terminated; }
	void				ResetEnv();
	void				PrepareEnvOptions(const char* stripPrefix);
	void				PrepareArgs();
	int					StartProcess();
	int					WaitProcess();
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
	void				SetArgs(const char** args, bool freeArgs) { m_args = args; m_freeArgs = freeArgs; }
	void				SetInfoName(const char* infoName) { m_infoName = infoName; }
	const char*			GetInfoName() { return m_infoName; }
	void				SetLogPrefix(const char* logPrefix) { m_logPrefix = logPrefix; }
	void				SetEnvVar(const char* name, const char* value);
	void				SetEnvVarSpecial(const char* prefix, const char* name, const char* value);
	void				SetIntEnvVar(const char* name, int value);
};

#endif
