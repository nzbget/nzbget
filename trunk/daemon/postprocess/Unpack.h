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
		bool			Exists(const char* szFilename);
	};

	typedef std::vector<char*>		ParamListBase;
	class ParamList : public ParamListBase
	{
	public:
						~ParamList();
		bool			Exists(const char* szParam);
	};

private:
	PostInfo*			m_pPostInfo;
	char				m_szName[1024];
	char				m_szInfoName[1024];
	char				m_szInfoNameUp[1024];
	char				m_szDestDir[1024];
	char				m_szFinalDir[1024];
	char				m_szUnpackDir[1024];
	char				m_szPassword[1024];
	bool				m_bInterDir;
	bool				m_bAllOKMessageReceived;
	bool				m_bNoFilesMessageReceived;
	bool				m_bHasParFiles;
	bool				m_bHasRarFiles;
	bool				m_bHasNonStdRarFiles;
	bool				m_bHasSevenZipFiles;
	bool				m_bHasSevenZipMultiFiles;
	bool				m_bHasSplittedFiles;
	bool				m_bUnpackOK;
	bool				m_bUnpackStartError;
	bool				m_bUnpackSpaceError;
	bool				m_bUnpackDecryptError;
	bool				m_bUnpackPasswordError;
	bool				m_bCleanedUpDisk;
	bool				m_bAutoTerminated;
	EUnpacker			m_eUnpacker;
	bool				m_bFinalDirCreated;
	FileList			m_JoinedFiles;
	bool				m_bPassListTried;

protected:
	virtual bool		ReadLine(char* szBuf, int iBufSize, FILE* pStream);
	virtual void		AddMessage(Message::EKind eKind, const char* szText);
	void				ExecuteUnpack(EUnpacker eUnpacker, const char* szPassword, bool bMultiVolumes);
	void				ExecuteUnrar(const char* szPassword);
	void				ExecuteSevenZip(const char* szPassword, bool bMultiVolumes);
	void				UnpackArchives(EUnpacker eUnpacker, bool bMultiVolumes);
	void				JoinSplittedFiles();
	bool				JoinFile(const char* szFragBaseName);
	void				Completed();
	void				CreateUnpackDir();
	bool				Cleanup();
	void				CheckArchiveFiles(bool bScanNonStdFiles);
	void				SetProgressLabel(const char* szProgressLabel);
#ifndef DISABLE_PARCHECK
	void				RequestParCheck(bool bForceRepair);
#endif
	bool				FileHasRarSignature(const char* szFilename);
	bool				PrepareCmdParams(const char* szCommand, ParamList* pParams, const char* szInfoName);

public:
	virtual void		Run();
	virtual void		Stop();
	static void			StartJob(PostInfo* pPostInfo);
};

class MoveController : public Thread, public ScriptController
{
private:
	PostInfo*			m_pPostInfo;
	char				m_szInterDir[1024];
	char				m_szDestDir[1024];

	bool				MoveFiles();

protected:
	virtual void		AddMessage(Message::EKind eKind, const char* szText);

public:
	virtual void		Run();
	static void			StartJob(PostInfo* pPostInfo);
};

class CleanupController : public Thread, public ScriptController
{
private:
	PostInfo*			m_pPostInfo;
	char				m_szDestDir[1024];
	char				m_szFinalDir[1024];

	bool				Cleanup(const char* szDestDir, bool *bDeleted);

protected:
	virtual void		AddMessage(Message::EKind eKind, const char* szText);

public:
	virtual void		Run();
	static void			StartJob(PostInfo* pPostInfo);
};

#endif
