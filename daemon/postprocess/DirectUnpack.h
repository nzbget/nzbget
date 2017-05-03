/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef DIRECTUNPACK_H
#define DIRECTUNPACK_H

#include "Log.h"
#include "Thread.h"
#include "DownloadInfo.h"
#include "Script.h"

class DirectUnpack : public Thread, public ScriptController
{
public:
	virtual void Run();
	virtual void Stop();
	static void StartJob(NzbInfo* nzbInfo);
	void FileDownloaded(FileInfo* fileInfo);

protected:
	virtual bool ReadLine(char* buf, int bufSize, FILE* stream);
	virtual void AddMessage(Message::EKind kind, const char* text);

private:
	typedef std::vector<CString> ParamListBase;
	class ParamList : public ParamListBase
	{
	public:
		bool Exists(const char* param);
	};

	NzbInfo* m_nzbInfo;
	CString m_name;
	CString m_infoName;
	CString m_infoNameUp;
	CString m_destDir;
	CString m_finalDir;
	CString m_unpackDir;
	CString m_password;
	CString m_waitingFile;
	bool m_hasRarFiles = false;
	bool m_allOkMessageReceived = false;
	bool m_unpackOk = false;
	bool m_finalDirCreated = false;

	void CreateUnpackDir();
	void CheckArchiveFiles();
	void ExecuteUnrar();
	bool PrepareCmdParams(const char* command, ParamList* params, const char* infoName);
	void WaitNextVolume(const char* filename);
};

#endif
