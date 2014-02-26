/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef QUEUESCRIPT_H
#define QUEUESCRIPT_H

#include "Script.h"
#include "Thread.h"
#include "DownloadInfo.h"

class QueueScriptController : public ScriptController
{
protected:
	void				PrepareEnvParameters(NZBParameterList* pParameters, const char* szStripPrefix);
};

class NZBScriptController : public QueueScriptController
{
private:
	char**				m_pNZBName;
	char**				m_pCategory;
	int*				m_iPriority;
	NZBParameterList*	m_pParameters;
	bool*				m_bAddTop;
	bool*				m_bAddPaused;
	int					m_iPrefixLen;

protected:
	virtual void		AddMessage(Message::EKind eKind, const char* szText);

public:
	static void			ExecuteScript(const char* szScript, const char* szNZBFilename, const char* szDirectory,
							char** pNZBName, char** pCategory, int* iPriority, NZBParameterList* pParameters,
							bool* bAddTop, bool* bAddPaused);
};

class NZBAddedScriptController : public Thread, public QueueScriptController
{
private:
	char*				m_szNZBName;

public:
	virtual void		Run();
	static void			StartScript(DownloadQueue* pDownloadQueue, NZBInfo *pNZBInfo, const char* szScript);
};

#endif
