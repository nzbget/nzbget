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


#ifndef POSTSCRIPT_H
#define POSTSCRIPT_H

#include "Thread.h"
#include "NzbScript.h"

class PostScriptController : public Thread, public NzbScriptController
{
public:
	virtual void Run();
	virtual void Stop();
	static void StartJob(PostInfo* postInfo);

protected:
	virtual void ExecuteScript(ScriptConfig::Script* script);
	virtual void AddMessage(Message::EKind kind, const char* text);

private:
	PostInfo* m_postInfo;
	int m_prefixLen;
	ScriptConfig::Script* m_script;

	void PrepareParams(const char* scriptName);
	ScriptStatus::EStatus AnalyseExitCode(int exitCode, const char* upInfoName);
};

#endif
