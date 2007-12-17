/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2005  Bo Cordes Petersen <placebodk@users.sourceforge.net>
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


#ifndef REMOTESERVER_H
#define REMOTESERVER_H

#include <list>

#include "Thread.h"
#include "NetAddress.h"
#include "Connection.h"
#include "MessageBase.h"

class RemoteServer : public Thread
{
private:
	NetAddress*			m_pNetAddress;
	Connection*			m_pConnection;

public:
						RemoteServer();
						~RemoteServer();
	virtual void		Run();
	virtual void 		Stop();
};

class RequestProcessor : public Thread
{
private:
	SOCKET				m_iSocket;
	SNZBMessageBase		m_MessageBase;

	void				Dispatch();

public:
	virtual void		Run();
	void				SetSocket(SOCKET iSocket) { m_iSocket = iSocket; };
};

class MessageCommand
{
protected:
	SOCKET				m_iSocket;
	SNZBMessageBase*	m_pMessageBase;

	bool				ReceiveRequest(void* pBuffer, int iSize);
	void				SendResponse(char* szAnswer);

public:
	virtual				~MessageCommand() {};
	virtual void		Execute() = 0;
	void				SetSocket(SOCKET iSocket) { m_iSocket = iSocket; };
	void				SetMessageBase(SNZBMessageBase*	pMessageBase) { m_pMessageBase = pMessageBase; };
};

class DownloadCommand: public MessageCommand
{
public:
	virtual void		Execute();
};

class ListCommand: public MessageCommand
{
public:
	virtual void		Execute();
};

class LogCommand: public MessageCommand
{
public:
	virtual void		Execute();
};

class PauseUnpauseCommand: public MessageCommand
{
public:
	virtual void		Execute();
};

class EditQueueCommand: public MessageCommand
{
private:
	class EditItem
	{
	public:
		int		m_iID;
		int		m_iOffset;

		EditItem(int iID, int iOffset);
	};

	typedef std::list<EditItem*> ItemList;

	int					m_iNrEntries;
	int					m_iAction;
	int					m_iOffset;
	void				PrepareList(uint32_t* pIDs, ItemList* pItemList, bool bSmartOrder);

public:
	virtual void		Execute();
};

class SetDownloadRateCommand: public MessageCommand
{
public:
	virtual void		Execute();
};

class DumpDebugCommand: public MessageCommand
{
public:
	virtual void		Execute();
};

class ShutdownCommand: public MessageCommand
{
public:
	virtual void		Execute();
};

#endif
