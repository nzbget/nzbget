/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2013-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef CLEANUP_H
#define CLEANUP_H

#include "NString.h"
#include "Log.h"
#include "Thread.h"
#include "DownloadInfo.h"
#include "Script.h"

class MoveController : public Thread, public ScriptController
{
public:
	virtual void Run();
	static void StartJob(PostInfo* postInfo);

protected:
	virtual void AddMessage(Message::EKind kind, const char* text);

private:
	PostInfo* m_postInfo;
	CString m_interDir;
	CString m_destDir;

	bool MoveFiles();
};

class CleanupController : public Thread, public ScriptController
{
public:
	virtual void Run();
	static void StartJob(PostInfo* postInfo);

protected:
	virtual void AddMessage(Message::EKind kind, const char* text);

private:
	PostInfo* m_postInfo;
	CString m_destDir;
	CString m_finalDir;

	bool Cleanup(const char* destDir, bool *deleted);
};

#endif
