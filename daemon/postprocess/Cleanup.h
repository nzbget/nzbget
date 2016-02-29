/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2013-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef CLEANUP_H
#define CLEANUP_H

#include "NString.h"
#include "Log.h"
#include "Thread.h"
#include "DownloadInfo.h"
#include "Script.h"

class MoveController : public Thread, public ScriptController
{
private:
	PostInfo* m_postInfo;
	CString m_interDir;
	CString m_destDir;

	bool MoveFiles();

protected:
	virtual void AddMessage(Message::EKind kind, const char* text);

public:
	virtual void Run();
	static void StartJob(PostInfo* postInfo);
};

class CleanupController : public Thread, public ScriptController
{
private:
	PostInfo* m_postInfo;
	CString m_destDir;
	CString m_finalDir;

	bool Cleanup(const char* destDir, bool *deleted);

protected:
	virtual void AddMessage(Message::EKind kind, const char* text);

public:
	virtual void Run();
	static void StartJob(PostInfo* postInfo);
};

#endif
