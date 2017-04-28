/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef RENAME_H
#define RENAME_H

#include "Thread.h"
#include "DownloadInfo.h"
#include "Script.h"
#include "RarRenamer.h"

#ifndef DISABLE_PARCHECK
#include "ParRenamer.h"
#endif

class RenameController : public Thread, public ScriptController
{
public:
	enum EJobKind
	{
		jkPar,
		jkRar
	};

	RenameController();
	virtual void Run();
	static void StartJob(PostInfo* postInfo, EJobKind kind);

protected:
	virtual void AddMessage(Message::EKind kind, const char* text);

private:
	PostInfo* m_postInfo;
	CString m_destDir;
	int m_renamedCount = 0;
	EJobKind m_kind;

#ifndef DISABLE_PARCHECK
	class PostParRenamer : public ParRenamer
	{
	protected:
		virtual void UpdateProgress() { m_owner->UpdateParRenameProgress(); }
		virtual void PrintMessage(Message::EKind kind, const char* format, ...) PRINTF_SYNTAX(3);
		virtual void RegisterParredFile(const char* filename) 
			{ m_owner->m_postInfo->GetParredFiles()->push_back(filename); }
		virtual void RegisterRenamedFile(const char* oldFilename, const char* newFileName) 
			{ m_owner->RegisterRenamedFile(oldFilename, newFileName); }
		virtual bool IsStopped() { return m_owner->IsStopped(); };
	private:
		RenameController* m_owner;
		friend class RenameController;
	};

	PostParRenamer m_parRenamer;

	void UpdateParRenameProgress();
#endif

	class PostRarRenamer : public RarRenamer
	{
	protected:
		virtual void UpdateProgress() { m_owner->UpdateRarRenameProgress(); }
		virtual void PrintMessage(Message::EKind kind, const char* format, ...) PRINTF_SYNTAX(3);
		virtual void RegisterRenamedFile(const char* oldFilename, const char* newFilename)
			{ m_owner->RegisterRenamedFile(oldFilename, newFilename); }
		virtual bool IsStopped() { return m_owner->IsStopped(); };
	private:
		RenameController* m_owner;
		friend class RenameController;
	};

	PostRarRenamer m_rarRenamer;

	void UpdateRarRenameProgress();

	void ExecRename(const char* destDir, const char* finalDir, const char* nzbName);
	void RenameCompleted();
	void RegisterRenamedFile(const char* oldFilename, const char* newFilename);
};

#endif
