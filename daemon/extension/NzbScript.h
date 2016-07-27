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


#ifndef NZBSCRIPT_H
#define NZBSCRIPT_H

#include "Script.h"
#include "DownloadInfo.h"
#include "ScriptConfig.h"

class NzbScriptController : public ScriptController
{
protected:
	void PrepareEnvParameters(NzbParameterList* parameters, const char* stripPrefix);
	void PrepareEnvScript(NzbParameterList* parameters, const char* scriptName);
	void ExecuteScriptList(const char* scriptList);
	virtual void ExecuteScript(ScriptConfig::Script* script) = 0;
};

#endif
