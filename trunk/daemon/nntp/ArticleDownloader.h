/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef ARTICLEDOWNLOADER_H
#define ARTICLEDOWNLOADER_H

#include <time.h>

#include "Observer.h"
#include "DownloadInfo.h"
#include "Thread.h"
#include "NNTPConnection.h"
#include "Decoder.h"
#include "ArticleWriter.h"

class ArticleDownloader : public Thread, public Subject
{
public:
	enum EStatus
	{
		adUndefined,
		adRunning,
		adWaiting,
		adFinished,
		adFailed,
		adRetry,
		adCrcError,
		adNotFound,
		adConnectError,
		adFatalError
	};

	class ArticleWriterImpl : public ArticleWriter
	{
	private:
		 ArticleDownloader*		m_pOwner;
	protected:
		virtual void	SetLastUpdateTimeNow() { m_pOwner->SetLastUpdateTimeNow(); } 
	public:
		void			SetOwner(ArticleDownloader* pOwner) { m_pOwner = pOwner; }
	};
			
private:
	FileInfo*			m_pFileInfo;
	ArticleInfo*		m_pArticleInfo;
	NNTPConnection* 	m_pConnection;
	EStatus				m_eStatus;
	Mutex			 	m_mutexConnection;
	char*				m_szInfoName;
	char*				m_szArticleFilename;
	time_t				m_tLastUpdateTime;
	Decoder::EFormat	m_eFormat;
	YDecoder			m_YDecoder;
	UDecoder			m_UDecoder;
	ArticleWriterImpl	m_ArticleWriter;
	ServerStatList		m_ServerStats;
	bool				m_bWritingStarted;

	EStatus				Download();
	EStatus				DecodeCheck();
	void				FreeConnection(bool bKeepConnected);
	EStatus				CheckResponse(const char* szResponse, const char* szComment);
	void				SetStatus(EStatus eStatus) { m_eStatus = eStatus; }
	bool				Write(char* szLine, int iLen);

public:
						ArticleDownloader();
	virtual				~ArticleDownloader();
	void				SetFileInfo(FileInfo* pFileInfo) { m_pFileInfo = pFileInfo; }
	FileInfo*			GetFileInfo() { return m_pFileInfo; }
	void				SetArticleInfo(ArticleInfo* pArticleInfo) { m_pArticleInfo = pArticleInfo; }
	ArticleInfo*		GetArticleInfo() { return m_pArticleInfo; }
	EStatus				GetStatus() { return m_eStatus; }
	ServerStatList*		GetServerStats() { return &m_ServerStats; }
	virtual void		Run();
	virtual void		Stop();
	bool				Terminate();
	time_t				GetLastUpdateTime() { return m_tLastUpdateTime; }
	void				SetLastUpdateTimeNow() { m_tLastUpdateTime = ::time(NULL); }
	const char* 		GetArticleFilename() { return m_szArticleFilename; }
	void				SetInfoName(const char* szInfoName);
	const char*			GetInfoName() { return m_szInfoName; }
	void				SetConnection(NNTPConnection* pConnection) { m_pConnection = pConnection; }
	void				CompleteFileParts() { m_ArticleWriter.CompleteFileParts(); }

	void				LogDebugInfo();
};

#endif
