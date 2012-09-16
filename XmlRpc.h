/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2010 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include "Connection.h"

class StringBuilder
{
private:
	char*				m_szBuffer;
	int					m_iBufferSize;
	int					m_iUsedSize;
public:
						StringBuilder();
						~StringBuilder();
	void				Append(const char* szStr);
	const char*			GetBuffer() { return m_szBuffer; }
};

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

private:
	const char*			m_szClientIP;
	char*				m_szRequest;
	const char*			m_szContentType;
	ERpcProtocol		m_eProtocol;
	EHttpMethod			m_eHttpMethod;
	char*				m_szUrl;
	StringBuilder		m_cResponse;

	void				Dispatch();
	XmlCommand*			CreateCommand(const char* szMethodName);
	void				MutliCall();
	void				BuildResponse(const char* szResponse, const char* szCallbackFunc, bool bFault);

public:
						XmlRpcProcessor();
						~XmlRpcProcessor();
	void				Execute();
	void				SetHttpMethod(EHttpMethod eHttpMethod) { m_eHttpMethod = eHttpMethod; }
	void				SetUrl(const char* szUrl);
	void				SetClientIP(const char* szClientIP) { m_szClientIP = szClientIP; }
	void				SetRequest(char* szRequest) { m_szRequest = szRequest; }
	const char*			GetResponse() { return m_cResponse.GetBuffer(); }
	const char*			GetContentType() { return m_szContentType; }
	static bool			IsRpcRequest(const char* szUrl);
};

class XmlCommand
{
protected:
	char*				m_szRequest;
	char*				m_szRequestPtr;
	char*				m_szCallbackFunc;
	StringBuilder		m_StringBuilder;
	bool				m_bFault;
	XmlRpcProcessor::ERpcProtocol	m_eProtocol;
	XmlRpcProcessor::EHttpMethod	m_eHttpMethod;

	void				BuildErrorResponse(int iErrCode, const char* szErrText, ...);
	void				BuildBoolResponse(bool bOK);
	void				AppendResponse(const char* szPart);
	bool				IsJson();
	bool				CheckSafeMethod();
	bool				NextParamAsInt(int* iValue);
	bool				NextParamAsBool(bool* bValue);
	bool				NextParamAsStr(char** szValueBuf);
	const char*			BoolToStr(bool bValue);
	char*				EncodeStr(const char* szStr);
	void				DecodeStr(char* szStr);

public:
						XmlCommand();
	virtual 			~XmlCommand() {}
	virtual void		Execute() = 0;
	void				PrepareParams();
	void				SetRequest(char* szRequest) { m_szRequest = szRequest; m_szRequestPtr = m_szRequest; }
	void				SetProtocol(XmlRpcProcessor::ERpcProtocol eProtocol) { m_eProtocol = eProtocol; }
	void				SetHttpMethod(XmlRpcProcessor::EHttpMethod eHttpMethod) { m_eHttpMethod = eHttpMethod; }
	const char*			GetResponse() { return m_StringBuilder.GetBuffer(); }
	const char*			GetCallbackFunc() { return m_szCallbackFunc; }
	bool				GetFault() { return m_bFault; }
};

class ErrorXmlCommand: public XmlCommand
{
private:
	int					m_iErrCode;
	const char*			m_szErrText;

public:
						ErrorXmlCommand(int iErrCode, const char* szErrText);
	virtual void		Execute();
};

class PauseUnpauseXmlCommand: public XmlCommand
{
public:
	enum EPauseAction
	{
		paDownload,
		paDownload2,
		paPostProcess,
		paScan
	};

private:
	bool			m_bPause;
	EPauseAction	m_eEPauseAction;

public:
						PauseUnpauseXmlCommand(bool bPause, EPauseAction eEPauseAction);
	virtual void		Execute();
};

class ShutdownXmlCommand: public XmlCommand
{
public:
	virtual void		Execute();
};

class VersionXmlCommand: public XmlCommand
{
public:
	virtual void		Execute();
};

class DumpDebugXmlCommand: public XmlCommand
{
public:
	virtual void		Execute();
};

class SetDownloadRateXmlCommand: public XmlCommand
{
public:
	virtual void		Execute();
};

class StatusXmlCommand: public XmlCommand
{
public:
	virtual void		Execute();
};

class LogXmlCommand: public XmlCommand
{
public:
	virtual void		Execute();
};

class ListFilesXmlCommand: public XmlCommand
{
public:
	virtual void		Execute();
};

class ListGroupsXmlCommand: public XmlCommand
{
public:
	virtual void		Execute();
};

class EditQueueXmlCommand: public XmlCommand
{
public:
	virtual void		Execute();
};

class DownloadXmlCommand: public XmlCommand
{
public:
	virtual void		Execute();
};

class PostQueueXmlCommand: public XmlCommand
{
public:
	virtual void		Execute();
};

class WriteLogXmlCommand: public XmlCommand
{
public:
	virtual void		Execute();
};

class ScanXmlCommand: public XmlCommand
{
public:
	virtual void		Execute();
};

class HistoryXmlCommand: public XmlCommand
{
public:
	virtual void		Execute();
};

class DownloadUrlXmlCommand: public XmlCommand
{
public:
	virtual void		Execute();
};

class UrlQueueXmlCommand: public XmlCommand
{
public:
	virtual void		Execute();
};

class ConfigXmlCommand: public XmlCommand
{
public:
	virtual void		Execute();
};

class LoadConfigXmlCommand: public XmlCommand
{
public:
	virtual void		Execute();
};

class SaveConfigXmlCommand: public XmlCommand
{
public:
	virtual void		Execute();
	void				Save(const char *szFilename);
};

#endif
