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

class ScanScriptController : public NzbScriptController
{
private:
	const char*			m_nzbFilename;
	const char*			m_url;
	const char*			m_directory;
	char**				m_nzbName;
	char**				m_category;
	int*				m_priority;
	NzbParameterList*	m_parameters;
	bool*				m_addTop;
	bool*				m_addPaused;
	char**				m_dupeKey;
	int*				m_dupeScore;
	EDupeMode*			m_dupeMode;
	int					m_prefixLen;

	void				PrepareParams(const char* scriptName);

protected:
	virtual void		ExecuteScript(ScriptConfig::Script* script);
	virtual void		AddMessage(Message::EKind kind, const char* text);

public:
	static void			ExecuteScripts(const char* nzbFilename, const char* url,
							const char* directory, char** nzbName, char** category, int* priority,
							NzbParameterList* parameters, bool* addTop, bool* addPaused,
							char** dupeKey, int* dupeScore, EDupeMode* dupeMode);
};

#endif
