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


#ifndef NZBSCRIPT_H
#define NZBSCRIPT_H

#include "Script.h"
#include "DownloadInfo.h"
#include "ScriptConfig.h"

class NZBScriptController : public ScriptController
{
protected:
	void				PrepareEnvParameters(NZBParameterList* pParameters, const char* szStripPrefix);
	void				PrepareEnvScript(NZBParameterList* pParameters, const char* szScriptName);
	void				ExecuteScriptList(const char* szScriptList);
	virtual void		ExecuteScript(ScriptConfig::Script* pScript) = 0;
};

#endif
