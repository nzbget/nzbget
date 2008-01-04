/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2004  Sven Henkel <sidddy@users.sourceforge.net>
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


#ifndef ARTICLEDOWNLOADER_H
#define ARTICLEDOWNLOADER_H

#include <time.h>
#ifdef WIN32
#include <sys/timeb.h>
#endif

#include "Observer.h"
#include "DownloadInfo.h"
#include "Thread.h"
#include "NNTPConnection.h"
#include "Decoder.h"

class ArticleDownloader : public Thread, public Subject
{
public:
	enum EStatus
	{
		adUndefined,
		adRunning,
		adFinished,
		adFailed,
		adDecodeError,
		adCrcError,
		adDecoding,
		adJoining,
		adNotFound,
		adFatalError
	};
			
private:
	FileInfo*			m_pFileInfo;
	ArticleInfo*		m_pArticleInfo;
	NNTPConnection* 	m_pConnection;
	EStatus				m_eStatus;
	Mutex			 	m_mutexConnection;
	const char*			m_szResultFilename;
	char*				m_szTempFilename;
	char*				m_szArticleFilename;
	char*				m_szInfoName;
	char*				m_szOutputFilename;
	time_t				m_tLastUpdateTime;
	Semaphore			m_semInitialized;
	Semaphore			m_semWaited;
	static const char*	m_szJobStatus[];
#ifdef WIN32
	struct _timeb		m_tStartTime;
#else
	struct timeval		m_tStartTime;
#endif
	int					m_iBytes;
	YDecoder			m_YDecoder;
	FILE*				m_pOutFile;

	EStatus				Download();
	bool				Write(char* szLine, int iLen);
	EStatus				Decode();
	void				FreeConnection();

public:
						ArticleDownloader();
						~ArticleDownloader();
	void				SetFileInfo(FileInfo* pFileInfo) { m_pFileInfo = pFileInfo; }
	FileInfo*			GetFileInfo() { return m_pFileInfo; }
	void				SetArticleInfo(ArticleInfo* pArticleInfo) { m_pArticleInfo = pArticleInfo; }
	ArticleInfo*		GetArticleInfo() { return m_pArticleInfo; }
	void				SetStatus(EStatus eStatus);
	EStatus				GetStatus() { return m_eStatus; }
	const char*			GetStatusText() { return m_szJobStatus[m_eStatus]; }
	virtual void		Run();
	virtual void		Stop();
	bool				Terminate();
	time_t				GetLastUpdateTime() { return m_tLastUpdateTime; }
	void				SetLastUpdateTimeNow() { m_tLastUpdateTime = ::time(NULL); }
	const char* 		GetTempFilename() { return m_szTempFilename; }
	void 				SetTempFilename(const char* v);
	void 				SetOutputFilename(const char* v);
	const char* 		GetArticleFilename() { return m_szArticleFilename; }
	void				SetInfoName(const char* v);
	const char*			GetInfoName() { return m_szInfoName; }
	void				CompleteFileParts();
	void				WaitInit();
#ifdef WIN32
	struct _timeb*		GetStartTime() { return &m_tStartTime; }
#else
	struct timeval*		GetStartTime() { return &m_tStartTime; }
#endif
	int					GetBytes() { return m_iBytes; }

	void				LogDebugInfo();
};

class DownloadSpeedMeter
{
public:
	virtual				~DownloadSpeedMeter() {};
	virtual float		CalcCurrentDownloadSpeed() = 0;
};
                      
#endif
