/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2011 Andrei Prygounkov <hugbug@users.sourceforge.net>
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


#ifndef SCANNER_H
#define SCANNER_H

#include <deque>
#include <time.h>
#include "DownloadInfo.h"

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

	bool				m_bRequestedNZBDirScan;
	int					m_iNZBDirInterval;
	bool				m_bNZBScript;
	int					m_iPass;
	int					m_iStepMSec;
	FileList			m_FileList;

	void				CheckIncomingNZBs(const char* szDirectory, const char* szCategory, bool bCheckStat);
	void				AddFileToQueue(const char* szFilename, const char* szCategory, int iPriority, NZBParameterList* pParameterList);
	void				ProcessIncomingFile(const char* szDirectory, const char* szBaseFilename, const char* szFullFilename, const char* szCategory);
	bool				CanProcessFile(const char* szFullFilename, bool bCheckStat);
	void				DropOldFiles();

public:
						Scanner();
						~Scanner();
	void				SetStepInterval(int iStepMSec) { m_iStepMSec = iStepMSec; }
	void				ScanNZBDir();
	void				Check();
};

#endif
