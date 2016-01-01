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

#include "NString.h"
#include "Observer.h"
#include "Thread.h"
#include "Connection.h"
#include "FileSystem.h"
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
	CString				m_url;
	CString				m_outputFilename;
	Connection* 		m_connection;
	Mutex			 	m_connectionMutex;
	EStatus				m_status;
	time_t				m_lastUpdateTime;
	CString				m_infoName;
	DiskFile			m_outFile;
	int					m_contentLen;
	bool				m_confirmedLength;
	CString				m_originalFilename;
	bool				m_force;
	bool				m_redirecting;
	bool				m_redirected;
	bool				m_gzip;
	bool				m_retry;
#ifndef DISABLE_GZIP
	GUnzipStream*		m_gUnzipStream;
#endif

	void				SetStatus(EStatus status);
	bool				Write(void* buffer, int len);
	bool				PrepareFile();
	void				FreeConnection();
	EStatus				CheckResponse(const char* response);
	EStatus				CreateConnection(URL *url);
	void				ParseFilename(const char* contentDisposition);
	void				SendHeaders(URL *url);
	EStatus				DownloadHeaders();
	EStatus				DownloadBody();
	void				ParseRedirect(const char* location);

protected:
	virtual void		ProcessHeader(const char* line);

public:
						WebDownloader();
	EStatus				GetStatus() { return m_status; }
	virtual void		Run();
	virtual void		Stop();
	EStatus				Download();
	EStatus				DownloadWithRedirects(int maxRedirects);
	bool				Terminate();
	void				SetInfoName(const char* infoName) { m_infoName = infoName; }
	const char*			GetInfoName() { return m_infoName; }
	void 				SetUrl(const char* url);
	const char*			GetOutputFilename() { return m_outputFilename; }
	void 				SetOutputFilename(const char* outputFilename) { m_outputFilename = outputFilename; }
	time_t				GetLastUpdateTime() { return m_lastUpdateTime; }
	void				SetLastUpdateTimeNow();
	bool				GetConfirmedLength() { return m_confirmedLength; }
	const char*			GetOriginalFilename() { return m_originalFilename; }
	void				SetForce(bool force) { m_force = force; }
	void				SetRetry(bool retry) { m_retry = retry; }

	void				LogDebugInfo();
};

#endif
