/*
 *  This file is part of nzbget
 *
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


#ifndef XMLRPC_H
#define XMLRPC_H

#include "NString.h"
#include "Connection.h"
#include "Util.h"

class XmlCommand;

class XmlRpcProcessor
{
public:
	enum ERpcProtocol
	{
		rpUndefined,
		rpXmlRpc,
		rpJsonRpc,
		rpJsonPRpc
	};

	enum EHttpMethod
	{
		hmPost,
		hmGet
	};

	enum EUserAccess
	{
		uaControl,
		uaRestricted,
		uaAdd
	};

private:
	char*				m_request;
	const char*			m_contentType;
	ERpcProtocol		m_protocol;
	EHttpMethod			m_httpMethod;
	EUserAccess			m_userAccess;
	CString				m_url;
	StringBuilder		m_response;

	void				Dispatch();
	std::unique_ptr<XmlCommand>		CreateCommand(const char* methodName);
	void				MutliCall();
	void				BuildResponse(const char* response, const char* callbackFunc, bool fault, const char* requestId);

public:
						XmlRpcProcessor();
	void				Execute();
	void				SetHttpMethod(EHttpMethod httpMethod) { m_httpMethod = httpMethod; }
	void				SetUserAccess(EUserAccess userAccess) { m_userAccess = userAccess; }
	void				SetUrl(const char* url);
	void				SetRequest(char* request) { m_request = request; }
	const char*			GetResponse() { return m_response; }
	const char*			GetContentType() { return m_contentType; }
	static bool			IsRpcRequest(const char* url);
};

class XmlCommand
{
protected:
	char*				m_request;
	char*				m_requestPtr;
	char*				m_callbackFunc;
	StringBuilder		m_response;
	bool				m_fault;
	XmlRpcProcessor::ERpcProtocol	m_protocol;
	XmlRpcProcessor::EHttpMethod	m_httpMethod;
	XmlRpcProcessor::EUserAccess	m_userAccess;

	void				BuildErrorResponse(int errCode, const char* errText, ...);
	void				BuildBoolResponse(bool ok);
	void				BuildIntResponse(int value);
	void				AppendResponse(const char* part);
	void				AppendFmtResponse(const char* format, ...);
	void				AppendCondResponse(const char* part, bool cond);
	bool				IsJson();
	bool				CheckSafeMethod();
	bool				NextParamAsInt(int* value);
	bool				NextParamAsBool(bool* value);
	bool				NextParamAsStr(char** valueBuf);
	char*				XmlNextValue(char* xml, const char* tag, int* valueLength);
	const char*			BoolToStr(bool value);
	CString				EncodeStr(const char* str);
	void				DecodeStr(char* str);

public:
						XmlCommand();
	virtual 			~XmlCommand() {}
	virtual void		Execute() = 0;
	void				PrepareParams();
	void				SetRequest(char* request) { m_request = request; m_requestPtr = m_request; }
	void				SetProtocol(XmlRpcProcessor::ERpcProtocol protocol) { m_protocol = protocol; }
	void				SetHttpMethod(XmlRpcProcessor::EHttpMethod httpMethod) { m_httpMethod = httpMethod; }
	void				SetUserAccess(XmlRpcProcessor::EUserAccess userAccess) { m_userAccess = userAccess; }
	const char*			GetResponse() { return m_response; }
	const char*			GetCallbackFunc() { return m_callbackFunc; }
	bool				GetFault() { return m_fault; }
};

#endif
