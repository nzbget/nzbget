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


#ifndef SCANNER_H
#define SCANNER_H

#include <deque>
#include <time.h>
#include "DownloadInfo.h"
#include "Thread.h"

class Scanner
{
private:
	class FileData
	{
	private:
		char*			m_szFilename;
		long long		m_iSize;
		time_t			m_tLastChange;

	public:
						FileData(const char* szFilename);
						~FileData();
		const char*		GetFilename() { return m_szFilename; }
		long long		GetSize() { return m_iSize; }
		void			SetSize(long long lSize) { m_iSize = lSize; }
		time_t			GetLastChange() { return m_tLastChange; }
		void			SetLastChange(time_t tLastChange) { m_tLastChange = tLastChange; }
	};

	typedef std::deque<FileData*>		FileList;

	class QueueData
	{
	private:
		char*				m_szFilename;
		char*				m_szNZBName;
		char*				m_szCategory;
		int					m_iPriority;
		NZBParameterList	m_Parameters;
		bool				m_bAddTop;
		bool				m_bAddPaused;

	public:
							QueueData(const char* szFilename, const char* szNZBName, const char* szCategory, int iPriority,
								NZBParameterList* pParameters, bool bAddTop, bool bAddPaused);
							~QueueData();
		const char*			GetFilename() { return m_szFilename; }
		const char*			GetNZBName() { return m_szNZBName; }
		const char*			GetCategory() { return m_szCategory; }
		int					GetPriority() { return m_iPriority; }
		NZBParameterList*	GetParameters() { return &m_Parameters; }
		bool				GetAddTop() { return m_bAddTop; }
		bool				GetAddPaused() { return m_bAddPaused; }
	};

	typedef std::deque<QueueData*>		QueueList;

	bool				m_bRequestedNZBDirScan;
	int					m_iNZBDirInterval;
	bool				m_bNZBScript;
	int					m_iPass;
	FileList			m_FileList;
	QueueList			m_QueueList;
	bool				m_bScanning;
	Mutex				m_mutexScan;

	void				CheckIncomingNZBs(const char* szDirectory, const char* szCategory, bool bCheckStat);
	void				AddFileToQueue(const char* szFilename, const char* szNZBName, const char* szCategory, int iPriority,
							NZBParameterList* pParameters, bool bAddTop, bool bAddPaused);
	void				ProcessIncomingFile(const char* szDirectory, const char* szBaseFilename, const char* szFullFilename, const char* szCategory);
	bool				CanProcessFile(const char* szFullFilename, bool bCheckStat);
	void				InitPPParameters(const char* szCategory, NZBParameterList* pParameters);
	void				DropOldFiles();
	void				ClearQueueList();

public:
						Scanner();
						~Scanner();
	void				ScanNZBDir(bool bSyncMode);
	void				Check();
	bool				AddExternalFile(const char* szNZBName, const char* szCategory, int iPriority,
							NZBParameterList* pParameters, bool bAddPaused, bool bAddTop,
							const char* szFileName, const char* szBuffer, int iBufSize, bool bSyncMode);
};

#endif
