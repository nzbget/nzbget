/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#endif

class ParCoordinator 
{
private:
#ifndef DISABLE_PARCHECK
	class PostParChecker: public ParChecker
	{
	private:
		ParCoordinator*	m_pOwner;
		PostInfo*		m_pPostInfo;
	protected:
		virtual bool	RequestMorePars(int iBlockNeeded, int* pBlockFound);
		virtual void	UpdateProgress();
		virtual void	Completed() { m_pOwner->ParCheckCompleted(); }
	public:
		PostInfo*		GetPostInfo() { return m_pPostInfo; }
		void			SetPostInfo(PostInfo* pPostInfo) { m_pPostInfo = pPostInfo; }

		friend class ParCoordinator;
	};

	class PostParRenamer: public ParRenamer
	{
	private:
		ParCoordinator*	m_pOwner;
		PostInfo*		m_pPostInfo;
	protected:
		virtual void	UpdateProgress();
		virtual void	Completed() { m_pOwner->ParRenameCompleted(); }
	public:
		PostInfo*		GetPostInfo() { return m_pPostInfo; }
		void			SetPostInfo(PostInfo* pPostInfo) { m_pPostInfo = pPostInfo; }
		
		friend class ParCoordinator;
	};
	
	struct BlockInfo
	{
		FileInfo*		m_pFileInfo;
		int				m_iBlockCount;
	};

	typedef std::list<BlockInfo*> 	Blocks;
	
	enum EJobKind
	{
		jkParCheck,
		jkParRename
	};

private:
	PostParChecker		m_ParChecker;
	bool				m_bStopped;
	bool				m_bPostScript;
	PostParRenamer		m_ParRenamer;
	EJobKind			m_eCurrentJob;

protected:
	virtual bool		PauseDownload() = 0;
	virtual bool		UnpauseDownload() = 0;
#endif

public:
	typedef std::deque<char*>		FileList;

public:
						ParCoordinator();
	virtual				~ParCoordinator();
	static bool			FindMainPars(const char* szPath, FileList* pFileList);
	static bool			ParseParFilename(const char* szParFilename, int* iBaseNameLen, int* iBlocks);
	static bool			SameParCollection(const char* szFilename1, const char* szFilename2);
	void				PausePars(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);

#ifndef DISABLE_PARCHECK
	void				ParCheckCompleted();
	void				ParRenameCompleted();
	void				CheckPauseState(PostInfo* pPostInfo);
	bool				AddPar(FileInfo* pFileInfo, bool bDeleted);
	bool				RequestMorePars(NZBInfo* pNZBInfo, const char* szParFilename, int iBlockNeeded, int* pBlockFound);
	void				FindPars(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, const char* szParFilename, 
							Blocks* pBlocks, bool bStrictParName, bool bExactParName, int* pBlockFound);
	void				UpdateParCheckProgress();
	void				UpdateParRenameProgress();
	void				StartParCheckJob(PostInfo* pPostInfo);
	void				StartParRenameJob(PostInfo* pPostInfo);
	void				Stop();
	bool				Cancel();
#endif
};

#endif
