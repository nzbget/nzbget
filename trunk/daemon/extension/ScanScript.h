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


#ifndef SCANSCRIPT_H
#define SCANSCRIPT_H

#include "NzbScript.h"

class ScanScriptController : public NZBScriptController
{
private:
	const char*			m_szNZBFilename;
	const char*			m_szUrl;
	const char*			m_szDirectory;
	char**				m_pNZBName;
	char**				m_pCategory;
	int*				m_iPriority;
	NZBParameterList*	m_pParameters;
	bool*				m_bAddTop;
	bool*				m_bAddPaused;
	char**				m_pDupeKey;
	int*				m_iDupeScore;
	EDupeMode*			m_eDupeMode;
	int					m_iPrefixLen;

	void				PrepareParams(const char* szScriptName);

protected:
	virtual void		ExecuteScript(ScriptConfig::Script* pScript);
	virtual void		AddMessage(Message::EKind eKind, const char* szText);

public:
	static void			ExecuteScripts(const char* szNZBFilename, const char* szUrl,
							const char* szDirectory, char** pNZBName, char** pCategory, int* iPriority,
							NZBParameterList* pParameters, bool* bAddTop, bool* bAddPaused,
							char** pDupeKey, int* iDupeScore, EDupeMode* eDupeMode);
};

#endif
