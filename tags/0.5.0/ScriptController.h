/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2008 Andrei Prygounkov <hugbug@users.sourceforge.net>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Revision$
 * $Date$
 *
 */


#ifndef SCRIPTCONTROLLER_H
#define SCRIPTCONTROLLER_H

#include "Thread.h"
#include "PostInfo.h"

class ScriptController : public Thread
{
private:
	PostInfo*			m_pPostInfo;
	const char*			m_szScript;
	bool				m_bNZBFileCompleted;
	bool				m_bHasFailedParJobs;
#ifdef WIN32
	HANDLE				m_hProcess;
#else
	pid_t				m_hProcess;
#endif

	void				AddMessage(char* szText);
	void				Finished();

public:
	virtual void		Run();
	virtual void		Stop();
	static void			StartScriptJob(PostInfo* pPostInfo, const char* szScript, 
							bool bNZBFileCompleted, bool bHasFailedParJobs);
};

#endif
