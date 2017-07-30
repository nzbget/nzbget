/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

	void Execute();
	void SetHttpMethod(EHttpMethod httpMethod) { m_httpMethod = httpMethod; }
	void SetUserAccess(EUserAccess userAccess) { m_userAccess = userAccess; }
	void SetUrl(const char* url);
	void SetRequest(char* request) { m_request = request; }
	const char* GetResponse() { return m_response; }
	const char* GetContentType() { return m_contentType; }
	static bool IsRpcRequest(const char* url);
	bool IsSafeMethod() { return m_safeMethod; };

private:
	char* m_request = nullptr;
	const char* m_contentType = nullptr;
	ERpcProtocol m_protocol = rpUndefined;
	EHttpMethod m_httpMethod = hmPost;
	EUserAccess m_userAccess;
	CString m_url;
	StringBuilder m_response;
	bool m_safeMethod = false;

	void Dispatch();
	std::unique_ptr<XmlCommand> CreateCommand(const char* methodName);
	void MutliCall();
	void BuildResponse(const char* response, const char* callbackFunc, bool fault, const char* requestId);
	void BuildErrorResponse(int errCode, const char* errText);
};

class XmlCommand
{
public:
	XmlCommand();
	virtual ~XmlCommand() {}
	virtual void Execute() = 0;
	void PrepareParams();
	void SetRequest(char* request) { m_request = request; m_requestPtr = m_request; }
	void SetProtocol(XmlRpcProcessor::ERpcProtocol protocol) { m_protocol = protocol; }
	void SetHttpMethod(XmlRpcProcessor::EHttpMethod httpMethod) { m_httpMethod = httpMethod; }
	void SetUserAccess(XmlRpcProcessor::EUserAccess userAccess) { m_userAccess = userAccess; }
	const char* GetResponse() { return m_response; }
	const char* GetCallbackFunc() { return m_callbackFunc; }
	bool GetFault() { return m_fault; }
	virtual bool IsSafeMethod() { return false; };
	virtual bool IsError() { return false; };

protected:
	char* m_request = nullptr;
	char* m_requestPtr = nullptr;
	char* m_callbackFunc = nullptr;
	StringBuilder m_response;
	bool m_fault = false;
	XmlRpcProcessor::ERpcProtocol m_protocol = XmlRpcProcessor::rpUndefined;
	XmlRpcProcessor::EHttpMethod m_httpMethod;
	XmlRpcProcessor::EUserAccess m_userAccess;

	void BuildErrorResponse(int errCode, const char* errText, ...);
	void BuildBoolResponse(bool ok);
	void BuildIntResponse(int value);
	void AppendResponse(const char* part);
	void AppendFmtResponse(const char* format, ...);
	void AppendCondResponse(const char* part, bool cond);
	bool IsJson();
	bool NextParamAsInt(int* value);
	bool NextParamAsBool(bool* value);
	bool NextParamAsStr(char** valueBuf);
	char* XmlNextValue(char* xml, const char* tag, int* valueLength);
	const char* BoolToStr(bool value);
	CString EncodeStr(const char* str);
	void DecodeStr(char* str);
};

#endif
