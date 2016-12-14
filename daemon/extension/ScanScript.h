/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef SCANSCRIPT_H
#define SCANSCRIPT_H

#include "NzbScript.h"

class ScanScriptController : public NzbScriptController
{
public:
	static void ExecuteScripts(const char* nzbFilename, const char* url,
		const char* directory, CString* nzbName, CString* category, int* priority,
		NzbParameterList* parameters, bool* addTop, bool* addPaused,
		CString* dupeKey, int* dupeScore, EDupeMode* dupeMode);
	static bool HasScripts();

protected:
	virtual void ExecuteScript(ScriptConfig::Script* script);
	virtual void AddMessage(Message::EKind kind, const char* text);

private:
	const char* m_nzbFilename;
	const char* m_url;
	const char* m_directory;
	CString* m_nzbName;
	CString* m_category;
	int* m_priority;
	NzbParameterList* m_parameters;
	bool* m_addTop;
	bool* m_addPaused;
	CString* m_dupeKey;
	int* m_dupeScore;
	EDupeMode* m_dupeMode;
	int m_prefixLen;

	void PrepareParams(const char* scriptName);
};

#endif
