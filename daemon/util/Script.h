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

#include <vector>

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
	void				Append(char* szString);
#ifdef WIN32
	char*				GetStrings();
#else	
	char**				GetStrings();
#endif
};

class ScriptController
{
private:
	const char*			m_szScript;
	const char*			m_szWorkingDir;
	const char**		m_szArgs;
	bool				m_bFreeArgs;
	const char*			m_szStdArgs[2];
	const char*			m_szInfoName;
	const char*			m_szLogPrefix;
	EnvironmentStrings	m_environmentStrings;
	bool				m_bTerminated;
	bool				m_bDetached;
	FILE*				m_pReadpipe;
#ifdef WIN32
	HANDLE				m_hProcess;
	char				m_szCmdLine[2048];
#else
	pid_t				m_hProcess;
#endif

	typedef std::vector<ScriptController*>	RunningScripts;
	static RunningScripts	m_RunningScripts;
	static Mutex			m_mutexRunning;

protected:
	void				ProcessOutput(char* szText);
	virtual bool		ReadLine(char* szBuf, int iBufSize, FILE* pStream);
	void				PrintMessage(Message::EKind eKind, const char* szFormat, ...);
	virtual void		AddMessage(Message::EKind eKind, const char* szText);
	bool				GetTerminated() { return m_bTerminated; }
	void				ResetEnv();
	void				PrepareEnvOptions(const char* szStripPrefix);
	void				PrepareArgs();
	void				UnregisterRunningScript();

public:
						ScriptController();
	virtual				~ScriptController();
	int					Execute();
	void				Terminate();
	void				Resume();
	void				Detach();
	static void			TerminateAll();

	void				SetScript(const char* szScript) { m_szScript = szScript; }
	const char*			GetScript() { return m_szScript; }
	void				SetWorkingDir(const char* szWorkingDir) { m_szWorkingDir = szWorkingDir; }
	void				SetArgs(const char** szArgs, bool bFreeArgs) { m_szArgs = szArgs; m_bFreeArgs = bFreeArgs; }
	void				SetInfoName(const char* szInfoName) { m_szInfoName = szInfoName; }
	const char*			GetInfoName() { return m_szInfoName; }
	void				SetLogPrefix(const char* szLogPrefix) { m_szLogPrefix = szLogPrefix; }
	void				SetEnvVar(const char* szName, const char* szValue);
	void				SetEnvVarSpecial(const char* szPrefix, const char* szName, const char* szValue);
	void				SetIntEnvVar(const char* szName, int iValue);
};

#endif
