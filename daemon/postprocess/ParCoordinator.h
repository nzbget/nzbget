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


#ifndef PARCOORDINATOR_H
#define PARCOORDINATOR_H

#include <list>
#include <deque>

#include "DownloadInfo.h"

#ifndef DISABLE_PARCHECK
#include "ParChecker.h"
#include "ParRenamer.h"
#include "DupeMatcher.h"
#endif

class ParCoordinator 
{
private:
#ifndef DISABLE_PARCHECK
	class PostParChecker: public ParChecker
	{
	private:
		ParCoordinator*	m_owner;
		PostInfo*		m_postInfo;
		time_t			m_parTime;
		time_t			m_repairTime;
		int				m_downloadSec;
	protected:
		virtual bool	RequestMorePars(int blockNeeded, int* blockFound);
		virtual void	UpdateProgress();
		virtual void	Completed() { m_owner->ParCheckCompleted(); }
		virtual void	PrintMessage(Message::EKind kind, const char* format, ...);
		virtual void	RegisterParredFile(const char* filename);
		virtual bool	IsParredFile(const char* filename);
		virtual EFileStatus	FindFileCrc(const char* filename, unsigned long* crc, SegmentList* segments);
		virtual void	RequestDupeSources(DupeSourceList* dupeSourceList);
		virtual void	StatDupeSources(DupeSourceList* dupeSourceList);
	public:
		PostInfo*		GetPostInfo() { return m_postInfo; }
		void			SetPostInfo(PostInfo* postInfo) { m_postInfo = postInfo; }
		time_t			GetParTime() { return m_parTime; }
		void			SetParTime(time_t parTime) { m_parTime = parTime; }
		time_t			GetRepairTime() { return m_repairTime; }
		void			SetRepairTime(time_t repairTime) { m_repairTime = repairTime; }
		int				GetDownloadSec() { return m_downloadSec; }
		void			SetDownloadSec(int downloadSec) { m_downloadSec = downloadSec; }

		friend class ParCoordinator;
	};

	class PostParRenamer: public ParRenamer
	{
	private:
		ParCoordinator*	m_owner;
		PostInfo*		m_postInfo;
	protected:
		virtual void	UpdateProgress();
		virtual void	Completed() { m_owner->ParRenameCompleted(); }
		virtual void	PrintMessage(Message::EKind kind, const char* format, ...);
		virtual void	RegisterParredFile(const char* filename);
		virtual void	RegisterRenamedFile(const char* oldFilename, const char* newFileName);
	public:
		PostInfo*		GetPostInfo() { return m_postInfo; }
		void			SetPostInfo(PostInfo* postInfo) { m_postInfo = postInfo; }
		
		friend class ParCoordinator;
	};

	class PostDupeMatcher: public DupeMatcher
	{
	private:
		PostInfo*		m_postInfo;
	protected:
		virtual void	PrintMessage(Message::EKind kind, const char* format, ...);
	public:
		PostDupeMatcher(PostInfo* postInfo):
			DupeMatcher(postInfo->GetNZBInfo()->GetDestDir(),
				postInfo->GetNZBInfo()->GetSize() - postInfo->GetNZBInfo()->GetParSize()),
				m_postInfo(postInfo) {}
	};

	struct BlockInfo
	{
		FileInfo*		m_fileInfo;
		int				m_blockCount;
	};

	typedef std::list<BlockInfo*> 	Blocks;
	
	enum EJobKind
	{
		jkParCheck,
		jkParRename
	};

private:
	PostParChecker		m_parChecker;
	bool				m_stopped;
	PostParRenamer		m_parRenamer;
	EJobKind			m_currentJob;

protected:
	void				UpdateParCheckProgress();
	void				UpdateParRenameProgress();
	void				ParCheckCompleted();
	void				ParRenameCompleted();
	void				CheckPauseState(PostInfo* postInfo);
	bool				RequestMorePars(NZBInfo* nzbInfo, const char* parFilename, int blockNeeded, int* blockFound);
#endif

public:
						ParCoordinator();
	virtual				~ParCoordinator();
	void				PausePars(DownloadQueue* downloadQueue, NZBInfo* nzbInfo);

#ifndef DISABLE_PARCHECK
	bool				AddPar(FileInfo* fileInfo, bool deleted);
	void				FindPars(DownloadQueue* downloadQueue, NZBInfo* nzbInfo, const char* parFilename, 
							Blocks* blocks, bool strictParName, bool exactParName, int* blockFound);
	void				StartParCheckJob(PostInfo* postInfo);
	void				StartParRenameJob(PostInfo* postInfo);
	void				Stop();
	bool				Cancel();
#endif
};

#endif
