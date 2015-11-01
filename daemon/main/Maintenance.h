/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2013-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#ifndef MAINTENANCE_H
#define MAINTENANCE_H

#include "Thread.h"
#include "Script.h"
#include "Log.h"
#include "Util.h"

class UpdateScriptController;

class Maintenance
{
private:
	MessageList			m_messages;
	Mutex				m_logMutex;
	Mutex				m_controllerMutex;
	int					m_idMessageGen;
	UpdateScriptController*	m_updateScriptController;
	char*				m_updateScript;

	bool				ReadPackageInfoStr(const char* key, char** value);

public:
	enum EBranch
	{
		brStable,
		brTesting,
		brDevel
	};

						Maintenance();
						~Maintenance();
	void				AddMessage(Message::EKind kind, time_t time, const char* text);
	MessageList*		LockMessages();
	void				UnlockMessages();
	bool				StartUpdate(EBranch branch);
	void				ResetUpdateController();
	bool				CheckUpdates(char** updateInfo);
	static bool			VerifySignature(const char* inFilename, const char* sigFilename, const char* pubKeyFilename);
};

extern Maintenance* g_Maintenance;

class UpdateScriptController : public Thread, public ScriptController
{
private:
	Maintenance::EBranch	m_branch;
	int						m_prefixLen;

protected:
	virtual void		AddMessage(Message::EKind kind, const char* text);

public:
	virtual void		Run();
	void				SetBranch(Maintenance::EBranch branch) { m_branch = branch; }
};

class UpdateInfoScriptController : public ScriptController
{
private:
	int					m_prefixLen;
	StringBuilder		m_updateInfo;

protected:
	virtual void		AddMessage(Message::EKind kind, const char* text);

public:
	static void			ExecuteScript(const char* script, char** updateInfo);
};

#endif
