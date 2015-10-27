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


#ifndef POSTSCRIPT_H
#define POSTSCRIPT_H

#include "Thread.h"
#include "NzbScript.h"

class PostScriptController : public Thread, public NZBScriptController
{
private:
	PostInfo*			m_postInfo;
 	int					m_prefixLen;
	ScriptConfig::Script*	m_script;

	void				PrepareParams(const char* scriptName);
	ScriptStatus::EStatus	AnalyseExitCode(int exitCode);

protected:
	virtual void		ExecuteScript(ScriptConfig::Script* script);
	virtual void		AddMessage(Message::EKind kind, const char* text);

public:
	virtual void		Run();
	virtual void		Stop();
	static void			StartJob(PostInfo* postInfo);
};

#endif
