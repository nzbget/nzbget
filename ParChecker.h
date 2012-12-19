/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2009 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef PARCHECKER_H
#define PARCHECKER_H

#ifndef DISABLE_PARCHECK

#include <deque>

#include "Thread.h"
#include "Observer.h"

class ParChecker : public Thread, public Subject
{
public:
	enum EStatus
	{
        psUndefined,
		psWorking,
		psFailed,
		psFinished
	};

	enum EStage
	{
		ptLoadingPars,
		ptVerifyingSources,
		ptRepairing,
		ptVerifyingRepaired,
	};

	typedef std::deque<char*>		FileList;
	
private:
	char*				m_szInfoName;
	char*				m_szParFilename;
	EStatus				m_eStatus;
	EStage				m_eStage;
	void*				m_pRepairer;	// declared as void* to prevent the including of libpar2-headers into this header-file
	char*				m_szErrMsg;
	bool				m_bRepairNotNeeded;
	FileList			m_QueuedParFiles;
	Mutex			 	m_mutexQueuedParFiles;
	bool				m_bQueuedParFilesChanged;
	FileList			m_ProcessedFiles;
	int					m_iProcessedFiles;
	int					m_iFilesToRepair;
	int					m_iExtraFiles;
	int					m_iMissingFiles;
	bool				m_bVerifyingExtraFiles;
	char*				m_szProgressLabel;
	int					m_iFileProgress;
	int					m_iStageProgress;
	bool				m_bCancelled;

	void				Cleanup();
	bool				LoadMorePars();
	bool				CheckSplittedFragments();
	bool				AddSplittedFragments(const char* szFilename);
	bool				AddAllFiles();
	void				signal_filename(std::string str);
	void				signal_progress(double progress);
	void				signal_done(std::string str, int available, int total);

protected:
	/**
	* Unpause par2-files
	* returns true, if the files with required number of blocks were unpaused,
	* or false if there are no more files in queue for this collection or not enough blocks
	*/
	virtual bool		RequestMorePars(int iBlockNeeded, int* pBlockFound) = 0;
	virtual void		UpdateProgress() {}
	EStage				GetStage() { return m_eStage; }
	const char*			GetProgressLabel() { return m_szProgressLabel; }
	int					GetFileProgress() { return m_iFileProgress; }
	int					GetStageProgress() { return m_iStageProgress; }

public:
						ParChecker();
	virtual				~ParChecker();
	virtual void		Run();
	const char*			GetParFilename() { return m_szParFilename; }
	void				SetParFilename(const char* szParFilename);
	const char*			GetInfoName() { return m_szInfoName; }
	void				SetInfoName(const char* szInfoName);
	void				SetStatus(EStatus eStatus);
	EStatus				GetStatus() { return m_eStatus; }
	const char*			GetErrMsg() { return m_szErrMsg; }
	bool				GetRepairNotNeeded() { return m_bRepairNotNeeded; }
	void				AddParFile(const char* szParFilename);
	void				QueueChanged();
	void				Cancel();
	bool				GetCancelled() { return m_bCancelled; }
};

#endif

#endif
