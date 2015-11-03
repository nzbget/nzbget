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


#ifndef UNPACK_H
#define UNPACK_H

#include <deque>
#include <vector>

#include "Log.h"
#include "Thread.h"
#include "DownloadInfo.h"
#include "Script.h"

class UnpackController : public Thread, public ScriptController
{
private:
	enum EUnpacker
	{
		upUnrar,
		upSevenZip
	};

	typedef std::deque<char*>		FileListBase;
	class FileList : public FileListBase
	{
	public:
		void			Clear();
		bool			Exists(const char* filename);
	};

	typedef std::vector<char*>		ParamListBase;
	class ParamList : public ParamListBase
	{
	public:
						~ParamList();
		bool			Exists(const char* param);
	};

private:
	PostInfo*			m_postInfo;
	char				m_name[1024];
	char				m_infoName[1024];
	char				m_infoNameUp[1024];
	char				m_destDir[1024];
	char				m_finalDir[1024];
	char				m_unpackDir[1024];
	char				m_password[1024];
	bool				m_interDir;
	bool				m_allOkMessageReceived;
	bool				m_noFilesMessageReceived;
	bool				m_hasParFiles;
	bool				m_hasRarFiles;
	bool				m_hasNonStdRarFiles;
	bool				m_hasSevenZipFiles;
	bool				m_hasSevenZipMultiFiles;
	bool				m_hasSplittedFiles;
	bool				m_unpackOk;
	bool				m_unpackStartError;
	bool				m_unpackSpaceError;
	bool				m_unpackDecryptError;
	bool				m_unpackPasswordError;
	bool				m_cleanedUpDisk;
	bool				m_autoTerminated;
	EUnpacker			m_unpacker;
	bool				m_finalDirCreated;
	FileList			m_joinedFiles;
	bool				m_passListTried;

protected:
	virtual bool		ReadLine(char* buf, int bufSize, FILE* stream);
	virtual void		AddMessage(Message::EKind kind, const char* text);
	void				ExecuteUnpack(EUnpacker unpacker, const char* password, bool multiVolumes);
	void				ExecuteUnrar(const char* password);
	void				ExecuteSevenZip(const char* password, bool multiVolumes);
	void				UnpackArchives(EUnpacker unpacker, bool multiVolumes);
	void				JoinSplittedFiles();
	bool				JoinFile(const char* fragBaseName);
	void				Completed();
	void				CreateUnpackDir();
	bool				Cleanup();
	void				CheckArchiveFiles(bool scanNonStdFiles);
	void				SetProgressLabel(const char* progressLabel);
#ifndef DISABLE_PARCHECK
	void				RequestParCheck(bool forceRepair);
#endif
	bool				FileHasRarSignature(const char* filename);
	bool				PrepareCmdParams(const char* command, ParamList* params, const char* infoName);

public:
	virtual void		Run();
	virtual void		Stop();
	static void			StartJob(PostInfo* postInfo);
};

#endif
