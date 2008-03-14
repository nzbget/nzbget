/*
 *  This file is part of nzbget
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


#ifndef POSTINFO_H
#define POSTINFO_H

#include <deque>

class PostInfo
{
public:
	enum EStage
	{
		ptQueued,
		ptLoadingPars,
		ptVerifyingSources,
		ptRepairing,
		ptVerifyingRepaired,
		ptExecutingScript,
		ptFinished
	};

private:
	char*			m_szNZBFilename;
	char*			m_szDestDir;
	char*			m_szParFilename;
	char*			m_szInfoName;
	bool			m_bWorking;
	bool			m_bParCheck;
	int				m_iParStatus;
	bool			m_bParFailed;
	EStage			m_eStage;
	char*			m_szProgressLabel;
	int				m_iFileProgress;
	int				m_iStageProgress;
	time_t			m_tStartTime;
	time_t			m_tStageTime;
	
public:
					PostInfo();
					~PostInfo();
	const char*		GetNZBFilename() { return m_szNZBFilename; }
	void			SetNZBFilename(const char* szNZBFilename);
	const char*		GetDestDir() { return m_szDestDir; }
	void			SetDestDir(const char* szDestDir);
	const char*		GetParFilename() { return m_szParFilename; }
	void			SetParFilename(const char* szParFilename);
	const char*		GetInfoName() { return m_szInfoName; }
	void			SetInfoName(const char* szInfoName);
	EStage			GetStage() { return m_eStage; }
	void			SetStage(EStage eStage) { m_eStage = eStage; }
	void			SetProgressLabel(const char* szProgressLabel);
	const char*		GetProgressLabel() { return m_szProgressLabel; }
	int				GetFileProgress() { return m_iFileProgress; }
	void			SetFileProgress(int iFileProgress) { m_iFileProgress = iFileProgress; }
	int				GetStageProgress() { return m_iStageProgress; }
	void			SetStageProgress(int iStageProgress) { m_iStageProgress = iStageProgress; }
	time_t			GetStartTime() { return m_tStartTime; }
	void			SetStartTime(time_t tStartTime) { m_tStartTime = tStartTime; }
	time_t			GetStageTime() { return m_tStageTime; }
	void			SetStageTime(time_t tStageTime) { m_tStageTime = tStageTime; }
	bool			GetWorking() { return m_bWorking; }
	void			SetWorking(bool bWorking) { m_bWorking = bWorking; }
	bool			GetParCheck() { return m_bParCheck; }
	void			SetParCheck(bool bParCheck) { m_bParCheck = bParCheck; }
	int				GetParStatus() { return m_iParStatus; }
	void			SetParStatus(int iParStatus) { m_iParStatus = iParStatus; }
	bool			GetParFailed() { return m_bParFailed; }
	void			SetParFailed(bool bParFailed) { m_bParFailed = bParFailed; }
};

typedef std::deque<PostInfo*> PostQueue;

#endif
