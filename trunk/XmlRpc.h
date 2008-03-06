/*
 *  This file is part of nzbget
 *
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


#ifndef XMLRPC_H
#define XMLRPC_H

#include <vector>

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
		rpJsonRpc
	};

private:
	SOCKET				m_iSocket;
	const char*			m_szClientIP;
	char*				m_szRequest;
	ERpcProtocol		m_eProtocol;

	void				Dispatch();
	void				SendResponse(const char* szResponse, bool bFault);
	XmlCommand*			CreateCommand(const char* szMethodName);
	void				MutliCall();

public:
						XmlRpcProcessor();
	void				Execute();
	void				SetSocket(SOCKET iSocket) { m_iSocket = iSocket; }
	void				SetProtocol(ERpcProtocol eProtocol) { m_eProtocol = eProtocol; }
	void				SetClientIP(const char* szClientIP) { m_szClientIP = szClientIP; }
};

class XmlCommand
{
protected:
	char*				m_szRequest;
	char*				m_szRequestPtr;
	StringBuilder		m_StringBuilder;
	bool				m_bFault;
	XmlRpcProcessor::ERpcProtocol	m_eProtocol;

	void				BuildErrorResponse(int iErrCode, const char* szErrText);
	void				BuildBoolResponse(bool bOK);
	void				AppendResponse(const char* szPart);
	bool				IsJson() { return m_eProtocol == XmlRpcProcessor::rpJsonRpc; }
	bool				NextParamAsInt(int* iValue);
	bool				NextParamAsBool(bool* bValue);
	bool				NextParamAsStr(char** szValueBuf);
	const char*			BoolToStr(bool bValue);
	char*				EncodeStr(const char* szStr);

public:
						XmlCommand();
	virtual 			~XmlCommand() {}
	virtual void		Execute() = 0;
	void				PrepareParams();
	void				SetRequest(char* szRequest) { m_szRequest = szRequest; m_szRequestPtr = m_szRequest; }
	void				SetProtocol(XmlRpcProcessor::ERpcProtocol eProtocol) { m_eProtocol = eProtocol; }
	const char*			GetResponse() { return m_StringBuilder.GetBuffer(); }
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

class PauseXmlCommand: public XmlCommand
{
public:
	virtual void		Execute();
};

class UnPauseXmlCommand: public XmlCommand
{
public:
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

#endif
