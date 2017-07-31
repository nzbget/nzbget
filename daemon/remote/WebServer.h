/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2012-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

	static void Init();
	void Execute();
	void SetConnection(Connection* connection) { m_connection = connection; }
	void SetUrl(const char* url) { m_url = url; }
	void SetHttpMethod(EHttpMethod httpMethod) { m_httpMethod = httpMethod; }
	bool GetKeepAlive() { return m_keepAlive; }

private:
	enum EUserAccess
	{
		uaControl,
		uaRestricted,
		uaAdd
	};

	Connection* m_connection = nullptr;
	CString m_request;
	CString m_url;
	EHttpMethod m_httpMethod;
	EUserAccess m_userAccess;
	bool m_rpcRequest;
	bool m_authorized;
	bool m_gzip;
	CString m_origin;
	int m_contentLen;
	char m_authInfo[256+1];
	char m_authToken[48+1];
	static char m_serverAuthToken[3][48+1];
	CString m_forwardedFor;
	CString m_oldETag;
	bool m_keepAlive = false;

	void Dispatch();
	void SendAuthResponse();
	void SendOptionsResponse();
	void SendErrorResponse(const char* errCode, bool printWarning);
	void SendSingleFileResponse();
	void SendMultiFileResponse();
	void SendBodyResponse(const char* body, int bodyLen, const char* contentType, bool cachable);
	void SendRedirectResponse(const char* url);
	const char* DetectContentType(const char* filename);
	bool IsAuthorizedIp(const char* remoteAddr);
	void ParseHeaders();
	void ParseUrl();
	bool CheckCredentials();
};

#endif
