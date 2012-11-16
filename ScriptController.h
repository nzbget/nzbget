/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2011 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef SCRIPTCONTROLLER_H
#define SCRIPTCONTROLLER_H

#include <list>

#include "Log.h"
#include "Thread.h"
#include "DownloadInfo.h"
#include "Options.h"

class EnvironmentStrings
{
private:
	typedef std::vector<char*>		Strings;
	
	Strings				m_strings;

public:
						EnvironmentStrings();
						~EnvironmentStrings();
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
	const char*			m_szInfoName;
	const char*			m_szDefaultKindPrefix;
	EnvironmentStrings	m_environmentStrings;
	Options::EScriptLogKind	m_eDefaultLogKind;
	bool				m_bTerminated;
#ifdef WIN32
	HANDLE				m_hProcess;
#else
	pid_t				m_hProcess;
#endif

	void				ProcessOutput(char* szText);
	void				PrepareEnvironmentStrings();

protected:
	virtual void		AddMessage(Message::EKind eKind, bool bDefaultKind, Options::EMessageTarget eMessageTarget, const char* szText);

public:
						ScriptController();
	virtual				~ScriptController();
	int					Execute();
	void				Terminate();

	void				SetScript(const char* szScript) { m_szScript = szScript; }
	const char*			GetScript() { return m_szScript; }
	void				SetWorkingDir(const char* szWorkingDir) { m_szWorkingDir = szWorkingDir; }
	void				SetArgs(const char** szArgs, bool bFreeArgs) { m_szArgs = szArgs; m_bFreeArgs = bFreeArgs; }
	void				SetInfoName(const char* szInfoName) { m_szInfoName = szInfoName; }
	const char*			GetInfoName() { return m_szInfoName; }
	void				SetDefaultKindPrefix(const char* szDefaultKindPrefix) { m_szDefaultKindPrefix = szDefaultKindPrefix; }
	void				SetDefaultLogKind(Options::EScriptLogKind eDefaultLogKind) { m_eDefaultLogKind = eDefaultLogKind; }
	void				SetEnvVar(const char* szName, const char* szValue);
};

class PostScriptController : public Thread, ScriptController
{
private:
	PostInfo*			m_pPostInfo;
	bool				m_bNZBFileCompleted;
	bool				m_bHasFailedParJobs;

protected:
	virtual void		AddMessage(Message::EKind eKind, bool bDefaultKind, Options::EMessageTarget eMessageTarget, const char* szText);

public:
	virtual void		Run();
	virtual void		Stop();
	static void			StartScriptJob(PostInfo* pPostInfo, const char* szScript, 
							bool bNZBFileCompleted, bool bHasFailedParJobs);
};

class NZBScriptController : public ScriptController
{
private:
	char**				m_pCategory;
	int*				m_iPriority;
	NZBParameterList*	m_pParameterList;

protected:
	virtual void		AddMessage(Message::EKind eKind, bool bDefaultKind, Options::EMessageTarget eMessageTarget, const char* szText);

public:
	static void			ExecuteScript(const char* szScript, const char* szNZBFilename, const char* szDirectory, char** pCategory, int* iPriority, NZBParameterList* pParameterList);
};

class NZBAddedScriptController : public Thread, ScriptController
{
private:
	char*				m_szNZBName;

public:
	virtual void		Run();
	static void			StartScript(DownloadQueue* pDownloadQueue, NZBInfo *pNZBInfo, const char* szScript);
};

class SchedulerScriptController : public Thread, ScriptController
{
public:
	virtual void		Run();
	static void			StartScript(const char* szCommandLine);
};

#endif
