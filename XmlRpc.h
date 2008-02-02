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
 * $Revision: $
 * $Date: 2008-01-23 19:17:00 +0100 (Mi, 23 Jan 2008) $
 *
 */


#ifndef XMLRPC_H
#define XMLRPC_H

#include "Connection.h"

class XmlRpcProcessor
{
private:
	SOCKET				m_iSocket;
	const char*			m_szClientIP;
	char*				m_szRequest;

	void				Dispatch();

public:
	void				Execute();
	void				SetSocket(SOCKET iSocket) { m_iSocket = iSocket; }
	void				SetClientIP(const char* szClientIP) { m_szClientIP = szClientIP; }
};

class XmlCommand
{
protected:
	SOCKET				m_iSocket;
	char*				m_szRequest;
	char*				m_szResponse;
	int					m_iResponseBufSize;
	int					m_iResponseLen;
	const char*			m_szRequestPtr;

	bool				ReceiveRequest(void* pBuffer, int iSize);
	void				SendResponse(const char* szResponse);
	void				SendErrorResponse(int iErrCode, const char* szErrText);
	void				SendBoolResponse(bool bOK);
	void				AppendResponse(const char* szPart);
	bool				NextIntParam(int* iValue);

public:
						XmlCommand();
	virtual				~XmlCommand();
	virtual void		Execute() = 0;
	void				SetSocket(SOCKET iSocket) { m_iSocket = iSocket; }
	void				SetRequest(char* szRequest) { m_szRequest = szRequest; m_szRequestPtr = m_szRequest; }
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

#endif
