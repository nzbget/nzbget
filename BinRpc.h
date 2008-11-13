/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2005 Bo Cordes Petersen <placebodk@sourceforge.net>
 *  Copyright (C) 2007-2008 Andrei Prygounkov <hugbug@users.sourceforge.net>
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


#ifndef BINRPC_H
#define BINRPC_H

#include "Connection.h"
#include "MessageBase.h"

class BinRpcProcessor
{
private:
	SOCKET				m_iSocket;
	SNZBRequestBase		m_MessageBase;
	const char*			m_szClientIP;

	void				Dispatch();

public:
	void				Execute();
	void				SetSocket(SOCKET iSocket) { m_iSocket = iSocket; }
	void				SetSignature(int iSignature) { m_MessageBase.m_iSignature = iSignature; }
	void				SetClientIP(const char* szClientIP) { m_szClientIP = szClientIP; }
};

class BinCommand
{
protected:
	SOCKET				m_iSocket;
	SNZBRequestBase*	m_pMessageBase;

	bool				ReceiveRequest(void* pBuffer, int iSize);
	void				SendBoolResponse(bool bSuccess, const char* szText);

public:
	virtual				~BinCommand() {}
	virtual void		Execute() = 0;
	void				SetSocket(SOCKET iSocket) { m_iSocket = iSocket; }
	void				SetMessageBase(SNZBRequestBase*	pMessageBase) { m_pMessageBase = pMessageBase; }
};

class DownloadBinCommand: public BinCommand
{
public:
	virtual void		Execute();
};

class ListBinCommand: public BinCommand
{
public:
	virtual void		Execute();
};

class LogBinCommand: public BinCommand
{
public:
	virtual void		Execute();
};

class PauseUnpauseBinCommand: public BinCommand
{
public:
	virtual void		Execute();
};

class EditQueueBinCommand: public BinCommand
{
public:
	virtual void		Execute();
};

class SetDownloadRateBinCommand: public BinCommand
{
public:
	virtual void		Execute();
};

class DumpDebugBinCommand: public BinCommand
{
public:
	virtual void		Execute();
};

class ShutdownBinCommand: public BinCommand
{
public:
	virtual void		Execute();
};

class VersionBinCommand: public BinCommand
{
public:
	virtual void		Execute();
};

class PostQueueBinCommand: public BinCommand
{
public:
	virtual void		Execute();
};

class WriteLogBinCommand: public BinCommand
{
public:
	virtual void		Execute();
};

class ScanBinCommand: public BinCommand
{
public:
	virtual void		Execute();
};

#endif
