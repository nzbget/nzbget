/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2007  Andrei Prygounkov <hugbug@users.sourceforge.net>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Revision$
 * $Date$
 *
 */


#ifndef PREPOSTPROCESSOR_H
#define PREPOSTPROCESSOR_H

#include <deque>

#include "Thread.h"
#include "Observer.h"
#include "DownloadInfo.h"

#ifndef DISABLE_PARCHECK
#include "ParChecker.h"
#endif

class PrePostProcessor : public Thread
{
public:
	class ParJob
	{
	private:
		char*				m_szNZBFilename;
		char*				m_szParFilename;
		char*				m_szInfoName;
		
	public:
							ParJob(const char* szNZBFilename, const char* szParFilename, const char* szInfoName);
							~ParJob();
		const char*			GetNZBFilename() { return m_szNZBFilename; }
		const char*			GetParFilename() { return m_szParFilename; }
		const char*			GetInfoName() { return m_szInfoName; }
	};

	typedef std::deque<ParJob*> ParQueue;

private:
	typedef std::deque<char*>		FileList;

	class QueueCoordinatorObserver: public Observer
	{
	public:
		PrePostProcessor* owner;
		virtual void	Update(Subject* Caller, void* Aspect) { owner->QueueCoordinatorUpdate(Caller, Aspect); }
	};

#ifndef DISABLE_PARCHECK
	class ParCheckerObserver: public Observer
	{
	public:
		PrePostProcessor* owner;
		virtual void	Update(Subject* Caller, void* Aspect) { owner->ParCheckerUpdate(Caller, Aspect); }
	};
#endif
	
private:
	bool				m_bCheckIncomingNZBs;
	QueueCoordinatorObserver	m_QueueCoordinatorObserver;
	bool				m_bHasMoreJobs;

	void				PausePars(DownloadQueue* pDownloadQueue, const char* szNZBFilename);
	void				CheckIncomingNZBs();
	bool				WasLastInCollection(DownloadQueue* pDownloadQueue, FileInfo* pFileInfo, bool bIgnorePaused);
	void				ExecPostScript(const char* szPath, const char* szNZBFilename, const char * szParFilename, int iParStatus);


	Mutex			 	m_mutexParChecker;
	ParQueue			m_ParQueue;

#ifndef DISABLE_PARCHECK
	ParChecker			m_ParChecker;
	ParCheckerObserver	m_ParCheckerObserver;

	void				ParCheckerUpdate(Subject* Caller, void* Aspect);
	void				CheckParQueue();
	void				CheckPars(DownloadQueue* pDownloadQueue, FileInfo* pFileInfo);
	bool				AddPar(FileInfo* pFileInfo, bool bDeleted);
	bool				SameParCollection(const char* szFilename1, const char* szFilename2);
	bool				FindMainPars(const char* szPath, FileList* pFileList);
#endif
	
public:
						PrePostProcessor();
	virtual				~PrePostProcessor();
	virtual void		Run();
	virtual void		Stop();
	void				QueueCoordinatorUpdate(Subject* Caller, void* Aspect);
	bool				HasMoreJobs() { return m_bHasMoreJobs; }
	ParQueue*			LockParQueue();
	void				UnlockParQueue();
};

#endif
