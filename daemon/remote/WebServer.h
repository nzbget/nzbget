/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2012-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "NString.h"
#include "Connection.h"

class WebProcessor
{
public:
	enum EHttpMethod
	{
		hmPost,
		hmGet,
		hmOptions
	};

	enum EUserAccess
	{
		uaControl,
		uaRestricted,
		uaAdd
	};

private:
	Connection*			m_connection;
	char*				m_request;
	char*				m_url;
	EHttpMethod			m_httpMethod;
	EUserAccess			m_userAccess;
	bool				m_gzip;
	CString				m_origin;
	int					m_contentLen;
	char				m_authInfo[256+1];
	char				m_authToken[48+1];
	static char			m_serverAuthToken[3][48+1];

	void				Dispatch();
	void				SendAuthResponse();
	void				SendOptionsResponse();
	void				SendErrorResponse(const char* errCode, bool printWarning);
	void				SendFileResponse(const char* filename);
	void				SendBodyResponse(const char* body, int bodyLen, const char* contentType);
	void				SendRedirectResponse(const char* url);
	const char*			DetectContentType(const char* filename);
	bool				IsAuthorizedIp(const char* remoteAddr);
	void				ParseHeaders();
	void				ParseUrl();
	bool				CheckCredentials();

public:
						WebProcessor();
						~WebProcessor();
	static void			Init();
	void				Execute();
	void				SetConnection(Connection* connection) { m_connection = connection; }
	void				SetUrl(const char* url);
	void				SetHttpMethod(EHttpMethod httpMethod) { m_httpMethod = httpMethod; }
};

#endif
