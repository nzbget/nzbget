/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2012-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef WEBDOWNLOADER_H
#define WEBDOWNLOADER_H

#include <time.h>

#include "Observer.h"
#include "Thread.h"
#include "Connection.h"
#include "Util.h"

class WebDownloader : public Thread, public Subject
{
public:
	enum EStatus
	{
		adUndefined,
		adRunning,
		adFinished,
		adFailed,
		adRetry,
		adNotFound,
		adRedirect,
		adConnectError,
		adFatalError
	};
			
private:
	char*				m_szURL;
	char*				m_szOutputFilename;
	Connection* 		m_pConnection;
	Mutex			 	m_mutexConnection;
	EStatus				m_eStatus;
	time_t				m_tLastUpdateTime;
	char*				m_szInfoName;
	FILE*				m_pOutFile;
	int					m_iContentLen;
	bool				m_bConfirmedLength;
	char*				m_szOriginalFilename;
	bool				m_bForce;
	bool				m_bRedirecting;
	bool				m_bRedirected;
	int					m_iRedirects;
	bool				m_bGZip;
	bool				m_bRetry;
#ifndef DISABLE_GZIP
	GUnzipStream*		m_pGUnzipStream;
#endif

	void				SetStatus(EStatus eStatus);
	bool				Write(void* pBuffer, int iLen);
	bool				PrepareFile();
	void				FreeConnection();
	EStatus				CheckResponse(const char* szResponse);
	EStatus				CreateConnection(URL *pUrl);
	void				ParseFilename(const char* szContentDisposition);
	void				SendHeaders(URL *pUrl);
	EStatus				DownloadHeaders();
	EStatus				DownloadBody();
	void				ParseRedirect(const char* szLocation); 

protected:
	virtual void		ProcessHeader(const char* szLine);

public:
						WebDownloader();
						~WebDownloader();
	EStatus				GetStatus() { return m_eStatus; }
	virtual void		Run();
	virtual void		Stop();
	EStatus				Download();
	bool				Terminate();
	void				SetInfoName(const char* v);
	const char*			GetInfoName() { return m_szInfoName; }
	void 				SetURL(const char* szURL);
	const char*			GetOutputFilename() { return m_szOutputFilename; }
	void 				SetOutputFilename(const char* v);
	time_t				GetLastUpdateTime() { return m_tLastUpdateTime; }
	void				SetLastUpdateTimeNow() { m_tLastUpdateTime = ::time(NULL); }
	bool				GetConfirmedLength() { return m_bConfirmedLength; }
	const char*			GetOriginalFilename() { return m_szOriginalFilename; }
	void				SetForce(bool bForce) { m_bForce = bForce; }
	void				SetRetry(bool bRetry) { m_bRetry = bRetry; }

	void				LogDebugInfo();
};

#endif
