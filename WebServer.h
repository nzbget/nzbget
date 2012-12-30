/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2012 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

private:
	Connection*			m_pConnection;
	char*				m_szRequest;
	char*				m_szUrl;
	EHttpMethod			m_eHttpMethod;
	bool				m_bGZip;
	char*				m_szOrigin;

	void				Dispatch();
	void				SendAuthResponse();
	void				SendOptionsResponse();
	void				SendErrorResponse(const char* szErrCode);
	void				SendFileResponse(const char* szFilename);
	void				SendBodyResponse(const char* szBody, int iBodyLen, const char* szContentType);
	void				SendRedirectResponse(const char* szURL);
	const char*			DetectContentType(const char* szFilename);

public:
						WebProcessor();
						~WebProcessor();
	void				Execute();
	void				SetConnection(Connection* pConnection) { m_pConnection = pConnection; }
	void				SetUrl(const char* szUrl);
	void				SetHttpMethod(EHttpMethod eHttpMethod) { m_eHttpMethod = eHttpMethod; }
};

#endif
