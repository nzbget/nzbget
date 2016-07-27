/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2013-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#ifndef MAINTENANCE_H
#define MAINTENANCE_H

#include "NString.h"
#include "Thread.h"
#include "Script.h"
#include "Log.h"
#include "Util.h"

class UpdateScriptController;

class Maintenance
{
public:
	enum EBranch
	{
		brStable,
		brTesting,
		brDevel
	};

	~Maintenance();
	void AddMessage(Message::EKind kind, time_t time, const char* text);
	GuardedMessageList GuardMessages() { return GuardedMessageList(&m_messages, &m_logMutex); }
	bool StartUpdate(EBranch branch);
	void ResetUpdateController();
	bool CheckUpdates(CString& updateInfo);
	static bool VerifySignature(const char* inFilename, const char* sigFilename, const char* pubKeyFilename);

private:
	MessageList m_messages;
	Mutex m_logMutex;
	Mutex m_controllerMutex;
	int m_idMessageGen = 0;
	UpdateScriptController* m_updateScriptController = nullptr;
	CString m_updateScript;

	bool ReadPackageInfoStr(const char* key, CString& value);
};

extern Maintenance* g_Maintenance;

class UpdateScriptController : public Thread, public ScriptController
{
public:
	virtual void Run();
	void SetBranch(Maintenance::EBranch branch) { m_branch = branch; }

protected:
	virtual void AddMessage(Message::EKind kind, const char* text);

private:
	Maintenance::EBranch m_branch;
	int m_prefixLen;
};

class UpdateInfoScriptController : public ScriptController
{
public:
	static void ExecuteScript(const char* script, CString& updateInfo);

private:
	int m_prefixLen;
	StringBuilder m_updateInfo;

protected:
	virtual void AddMessage(Message::EKind kind, const char* text);
};

#endif
