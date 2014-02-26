/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2013-2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
	Log::Messages		m_Messages;
	Mutex				m_mutexLog;
	Mutex				m_mutexController;
	int					m_iIDMessageGen;
	UpdateScriptController*	m_UpdateScriptController;
	char*				m_szUpdateScript;

	bool				ReadPackageInfoStr(const char* szKey, char** pValue);

public:
	enum EBranch
	{
		brStable,
		brTesting,
		brDevel
	};

						Maintenance();
						~Maintenance();
	void				ClearMessages();
	void				AppendMessage(Message::EKind eKind, time_t tTime, const char* szText);
	Log::Messages*		LockMessages();
	void				UnlockMessages();
	bool				StartUpdate(EBranch eBranch);
	void				ResetUpdateController();
	bool				CheckUpdates(char** pUpdateInfo);
};

class UpdateScriptController : public Thread, public ScriptController
{
private:
	Maintenance::EBranch	m_eBranch;
	int						m_iPrefixLen;

protected:
	virtual void		AddMessage(Message::EKind eKind, const char* szText);

public:
	virtual void		Run();
	void				SetBranch(Maintenance::EBranch eBranch) { m_eBranch = eBranch; }
};

class UpdateInfoScriptController : public ScriptController
{
private:
	int					m_iPrefixLen;
	StringBuilder		m_UpdateInfo;

protected:
	virtual void		AddMessage(Message::EKind eKind, const char* szText);

public:
	static void			ExecuteScript(const char* szScript, char** pUpdateInfo);
};

#endif
