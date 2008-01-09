/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2004  Sven Henkel <sidddy@users.sourceforge.net>
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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include "nzbget.h"
#include "Options.h"
#include "Frontend.h"
#include "Log.h"
#include "Connection.h"
#include "MessageBase.h"
#include "QueueCoordinator.h"
#include "PrePostProcessor.h"
#include "RemoteClient.h"
#include "Util.h"

extern QueueCoordinator* g_pQueueCoordinator;
extern PrePostProcessor* g_pPrePostProcessor;
extern Options* g_pOptions;

Frontend::Frontend()
{
	debug("Creating Frontend");

	m_iNeededLogFirstID = 0;
	m_iNeededLogEntries = 0;
	m_bSummary = false;
	m_bFileList = false;
	m_fCurrentDownloadSpeed = 0;
	m_lRemainingSize = 0;
	m_bPause = false;
	m_fDownloadLimit = 0;
	m_iThreadCount = 0;
	m_iParJobCount = 0;
	m_RemoteMessages.clear();
	m_RemoteQueue.clear();
	m_iUpdateInterval = g_pOptions->GetUpdateInterval();
}

bool Frontend::PrepareData()
{
	if (IsRemoteMode())
	{
		if (IsStopped())
		{
			return false;
		}
		if (!RequestMessages() || ((m_bSummary || m_bFileList) && !RequestFileList()))
		{
			printf("\nUnable to send request to nzbserver at %s (port %i)    \n", g_pOptions->GetServerIP(), g_pOptions->GetServerPort());
			Stop();
			return false;
		}
	}
	else
	{
		if (m_bSummary)
		{
			m_fCurrentDownloadSpeed = g_pQueueCoordinator->CalcCurrentDownloadSpeed();
			m_lRemainingSize = g_pQueueCoordinator->CalcRemainingSize();
			m_bPause = g_pOptions->GetPause();
			m_fDownloadLimit = g_pOptions->GetDownloadRate();
			m_iThreadCount = Thread::GetThreadCount();
			PrePostProcessor::ParQueue* pParQueue = g_pPrePostProcessor->LockParQueue();
			m_iParJobCount = pParQueue->size();
			g_pPrePostProcessor->UnlockParQueue();
		}
	}
	return true;
}

void Frontend::FreeData()
{
	if (IsRemoteMode())
	{
		for (Log::Messages::iterator it = m_RemoteMessages.begin(); it != m_RemoteMessages.end(); it++)
		{
			delete *it;
		}
		m_RemoteMessages.clear();

		for (DownloadQueue::iterator it = m_RemoteQueue.begin(); it != m_RemoteQueue.end(); it++)
		{
			delete *it;
		}
		m_RemoteQueue.clear();
	}
}

Log::Messages * Frontend::LockMessages()
{
	if (IsRemoteMode())
	{
		return &m_RemoteMessages;
	}
	else
	{
		return g_pLog->LockMessages();
	}
}

void Frontend::UnlockMessages()
{
	if (!IsRemoteMode())
	{
		g_pLog->UnlockMessages();
	}
}

DownloadQueue * Frontend::LockQueue()
{
	if (IsRemoteMode())
	{
		return &m_RemoteQueue;
	}
	else
	{
		return g_pQueueCoordinator->LockQueue();
	}
}

void Frontend::UnlockQueue()
{
	if (!IsRemoteMode())
	{
		g_pQueueCoordinator->UnlockQueue();
	}
}

bool Frontend::IsRemoteMode()
{
	return g_pOptions->GetRemoteClientMode();
}

void Frontend::ServerPauseUnpause(bool bPause)
{
	if (IsRemoteMode())
	{
		RequestPauseUnpause(bPause);
	}
	else
	{
		g_pOptions->SetPause(bPause);
	}
}

void Frontend::ServerSetDownloadRate(float fRate)
{
	if (IsRemoteMode())
	{
		RequestSetDownloadRate(fRate);
	}
	else
	{
		g_pOptions->SetDownloadRate(fRate);
	}
}

void Frontend::ServerDumpDebug()
{
	if (IsRemoteMode())
	{
		RequestDumpDebug();
	}
	else
	{
		g_pQueueCoordinator->LogDebugInfo();
	}
}

bool Frontend::ServerEditQueue(QueueEditor::EEditAction eAction, int iOffset, int iID)
{
	if (IsRemoteMode())
	{
		return RequestEditQueue(eAction, iOffset, iID);
	}
	else
	{
		return g_pQueueCoordinator->GetQueueEditor()->EditEntry(iID, true, eAction, iOffset);
	}
	return false;
}

void Frontend::InitMessageBase(SNZBMessageBase* pMessageBase, int iRequest, int iSize)
{
	pMessageBase->m_iSignature	= htonl(NZBMESSAGE_SIGNATURE);
	pMessageBase->m_iType = htonl(iRequest);
	pMessageBase->m_iStructSize = htonl(iSize);
	strncpy(pMessageBase->m_szPassword, g_pOptions->GetServerPassword(), NZBREQUESTPASSWORDSIZE);
	pMessageBase->m_szPassword[NZBREQUESTPASSWORDSIZE - 1] = '\0';
}

bool Frontend::RequestMessages()
{
	NetAddress netAddress(g_pOptions->GetServerIP(), g_pOptions->GetServerPort());
	Connection connection(&netAddress);

	bool OK = connection.Connect() >= 0;
	if (!OK)
	{
		return false;
	}

	SNZBLogRequest LogRequest;
	InitMessageBase(&LogRequest.m_MessageBase, eRemoteRequestLog, sizeof(LogRequest));
	LogRequest.m_iLines = htonl(m_iNeededLogEntries);
	if (m_iNeededLogEntries == 0)
	{
		LogRequest.m_iIDFrom = htonl(m_iNeededLogFirstID > 0 ? m_iNeededLogFirstID : 1);
	}
	else
	{
		LogRequest.m_iIDFrom = 0;
	}

	if (connection.Send((char*)(&LogRequest), sizeof(LogRequest)) < 0)
	{
		return false;
	}

	// Now listen for the returned log
	SNZBLogRequestAnswer LogRequestAnswer;
	if (connection.Recv((char*) &LogRequestAnswer, sizeof(LogRequestAnswer)) < 0)
	{
		return false;
	}

	char* pBuf = NULL;
	if (ntohl(LogRequestAnswer.m_iTrailingDataLength) > 0)
	{
		pBuf = (char*)malloc(ntohl(LogRequestAnswer.m_iTrailingDataLength));
		if (!connection.RecvAll(pBuf, ntohl(LogRequestAnswer.m_iTrailingDataLength)))
		{
			free(pBuf);
			return false;
		}
	}

	connection.Disconnect();

	if (ntohl(LogRequestAnswer.m_iTrailingDataLength) > 0)
	{
		char* pBufPtr = (char*)pBuf;
		for (unsigned int i = 0; i < ntohl(LogRequestAnswer.m_iNrTrailingEntries); i++)
		{
			SNZBLogRequestAnswerEntry* pLogAnswer = (SNZBLogRequestAnswerEntry*) pBufPtr;

			char* szText = pBufPtr + sizeof(SNZBLogRequestAnswerEntry);

			Message* pMessage = new Message(ntohl(pLogAnswer->m_iID), (Message::EKind)ntohl(pLogAnswer->m_iKind), ntohl(pLogAnswer->m_tTime), szText);
			m_RemoteMessages.push_back(pMessage);

			pBufPtr += sizeof(SNZBLogRequestAnswerEntry) + ntohl(pLogAnswer->m_iTextLen);
		}

		free(pBuf);
	}

	return true;
}

bool Frontend::RequestFileList()
{
	NetAddress netAddress(g_pOptions->GetServerIP(), g_pOptions->GetServerPort());
	Connection connection(&netAddress);

	bool OK = connection.Connect() >= 0;
	if (!OK)
	{
		return false;
	}

	SNZBListRequest ListRequest;
	InitMessageBase(&ListRequest.m_MessageBase, eRemoteRequestList, sizeof(ListRequest));
	ListRequest.m_bFileList = htonl(m_bFileList);
	ListRequest.m_bServerState = htonl(m_bSummary);

	if (connection.Send((char*)(&ListRequest), sizeof(ListRequest)) < 0)
	{
		return false;
	}

	// Now listen for the returned list
	SNZBListRequestAnswer ListRequestAnswer;
	if (connection.Recv((char*) &ListRequestAnswer, sizeof(ListRequestAnswer)) < 0)
	{
		return false;
	}

	char* pBuf = NULL;
	if (ntohl(ListRequestAnswer.m_iTrailingDataLength) > 0)
	{
		pBuf = (char*)malloc(ntohl(ListRequestAnswer.m_iTrailingDataLength));
		if (!connection.RecvAll(pBuf, ntohl(ListRequestAnswer.m_iTrailingDataLength)))
		{
			free(pBuf);
			return false;
		}
	}

	connection.Disconnect();

	if (m_bSummary)
	{
		m_bPause = ntohl(ListRequestAnswer.m_bServerPaused);
		m_lRemainingSize = JoinInt64(ntohl(ListRequestAnswer.m_iRemainingSizeHi), ntohl(ListRequestAnswer.m_iRemainingSizeLo));
		m_fCurrentDownloadSpeed = ntohl(ListRequestAnswer.m_iDownloadRate) / 1024.0;
		m_fDownloadLimit = ntohl(ListRequestAnswer.m_iDownloadLimit) / 1024.0;
		m_iThreadCount = ntohl(ListRequestAnswer.m_iThreadCount);
		m_iParJobCount = ntohl(ListRequestAnswer.m_iParJobCount);
	}

	if (m_bFileList && ntohl(ListRequestAnswer.m_iTrailingDataLength) > 0)
	{
		char* pBufPtr = (char*)pBuf;
		for (unsigned int i = 0; i < ntohl(ListRequestAnswer.m_iNrTrailingEntries); i++)
		{
			SNZBListRequestAnswerEntry* pListAnswer = (SNZBListRequestAnswerEntry*) pBufPtr;

			char* szNZBFilename = pBufPtr + sizeof(SNZBListRequestAnswerEntry);
			char* szSubject = pBufPtr + sizeof(SNZBListRequestAnswerEntry) + ntohl(pListAnswer->m_iNZBFilenameLen);
			char* szFileName = pBufPtr + sizeof(SNZBListRequestAnswerEntry) + ntohl(pListAnswer->m_iNZBFilenameLen) + ntohl(pListAnswer->m_iSubjectLen);
			char* szDestDir = pBufPtr + sizeof(SNZBListRequestAnswerEntry) + ntohl(pListAnswer->m_iNZBFilenameLen) + ntohl(pListAnswer->m_iSubjectLen) + ntohl(pListAnswer->m_iFilenameLen);
			
			FileInfo* pFileInfo = new FileInfo();
			pFileInfo->SetID(ntohl(pListAnswer->m_iID));
			pFileInfo->SetSize(JoinInt64(ntohl(pListAnswer->m_iFileSizeHi), ntohl(pListAnswer->m_iFileSizeLo)));
			pFileInfo->SetRemainingSize(JoinInt64(ntohl(pListAnswer->m_iRemainingSizeHi), ntohl(pListAnswer->m_iRemainingSizeLo)));
			pFileInfo->SetPaused(ntohl(pListAnswer->m_bPaused));
			pFileInfo->SetNZBFilename(szNZBFilename);
			pFileInfo->SetSubject(szSubject);
			pFileInfo->SetFilename(szFileName);
			pFileInfo->SetFilenameConfirmed(ntohl(pListAnswer->m_bFilenameConfirmed));
			pFileInfo->SetDestDir(szDestDir);

			m_RemoteQueue.push_back(pFileInfo);

			pBufPtr += sizeof(SNZBListRequestAnswerEntry) + ntohl(pListAnswer->m_iNZBFilenameLen) +
				ntohl(pListAnswer->m_iSubjectLen) + ntohl(pListAnswer->m_iFilenameLen) + ntohl(pListAnswer->m_iDestDirLen);
		}
	}
	if (pBuf)
	{
		free(pBuf);
	}

	return true;
}

bool Frontend::RequestPauseUnpause(bool bPause)
{
	RemoteClient client;
	client.SetVerbose(false);
	return client.RequestServerPauseUnpause(bPause);
}

bool Frontend::RequestSetDownloadRate(float fRate)
{
	RemoteClient client;
	client.SetVerbose(false);
	return client.RequestServerSetDownloadRate(fRate);
}

bool Frontend::RequestDumpDebug()
{
	RemoteClient client;
	client.SetVerbose(false);
	return client.RequestServerDumpDebug();
}

bool Frontend::RequestEditQueue(int iAction, int iOffset, int iID)
{
	RemoteClient client;
	client.SetVerbose(false);
	return client.RequestServerEditQueue(iAction, iOffset, &iID, 1, false);
}
