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


#ifndef UNPACK_H
#define UNPACK_H

#include "Log.h"
#include "Thread.h"
#include "DownloadInfo.h"
#include "Script.h"

class UnpackController : public Thread, public ScriptController
{
public:
	virtual void Run();
	virtual void Stop();
	static void StartJob(PostInfo* postInfo);

protected:
	virtual bool ReadLine(char* buf, int bufSize, FILE* stream);
	virtual void AddMessage(Message::EKind kind, const char* text);

private:
	enum EUnpacker
	{
		upUnrar,
		upSevenZip
	};

	typedef std::vector<CString> FileListBase;
	class FileList : public FileListBase
	{
	public:
		bool Exists(const char* filename);
	};

	typedef std::vector<CString> ParamListBase;
	class ParamList : public ParamListBase
	{
	public:
		bool Exists(const char* param);
	};

	PostInfo* m_postInfo;
	CString m_name;
	CString m_infoName;
	CString m_infoNameUp;
	CString m_destDir;
	CString m_finalDir;
	CString m_unpackDir;
	CString m_password;
	bool m_interDir;
	bool m_allOkMessageReceived;
	bool m_noFilesMessageReceived;
	bool m_hasParFiles;
	bool m_hasRarFiles;
	bool m_hasNonStdRarFiles;
	bool m_hasSevenZipFiles;
	bool m_hasSevenZipMultiFiles;
	bool m_hasSplittedFiles;
	bool m_unpackOk;
	bool m_unpackStartError;
	bool m_unpackSpaceError;
	bool m_unpackDecryptError;
	bool m_unpackPasswordError;
	bool m_cleanedUpDisk;
	bool m_autoTerminated;
	EUnpacker m_unpacker;
	bool m_finalDirCreated;
	FileList m_joinedFiles;
	bool m_passListTried;

	void ExecuteUnpack(EUnpacker unpacker, const char* password, bool multiVolumes);
	void ExecuteUnrar(const char* password);
	void ExecuteSevenZip(const char* password, bool multiVolumes);
	void UnpackArchives(EUnpacker unpacker, bool multiVolumes);
	void JoinSplittedFiles();
	bool JoinFile(const char* fragBaseName);
	void Completed();
	void CreateUnpackDir();
	bool Cleanup();
	void CheckArchiveFiles(bool scanNonStdFiles);
	void SetProgressLabel(const char* progressLabel);
#ifndef DISABLE_PARCHECK
	void RequestParCheck(bool forceRepair);
#endif
	bool FileHasRarSignature(const char* filename);
	bool PrepareCmdParams(const char* command, ParamList* params, const char* infoName);
};

#endif
