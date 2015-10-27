/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
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
#include "RemoteClient.h"
#include "Util.h"
#include "StatMeter.h"

Frontend::Frontend()
{
	debug("Creating Frontend");

	m_neededLogFirstId = 0;
	m_neededLogEntries = 0;
	m_summary = false;
	m_fileList = false;
	m_currentDownloadSpeed = 0;
	m_remainingSize = 0;
	m_pauseDownload = false;
	m_downloadLimit = 0;
	m_threadCount = 0;
	m_postJobCount = 0;
	m_upTimeSec = 0;
	m_dnTimeSec = 0;
	m_allBytes = 0;
	m_standBy = 0;
	m_updateInterval = g_pOptions->GetUpdateInterval();
}

bool Frontend::PrepareData()
{
	if (IsRemoteMode())
	{
		if (IsStopped())
		{
			return false;
		}
		if (!RequestMessages() || ((m_summary || m_fileList) && !RequestFileList()))
		{
			const char* controlIP = !strcmp(g_pOptions->GetControlIP(), "0.0.0.0") ? "127.0.0.1" : g_pOptions->GetControlIP();
			printf("\nUnable to send request to nzbget-server at %s (port %i)    \n", controlIP, g_pOptions->GetControlPort());
			Stop();
			return false;
		}
	}
	else
	{
		if (m_summary)
		{
			m_currentDownloadSpeed = g_pStatMeter->CalcCurrentDownloadSpeed();
			m_pauseDownload = g_pOptions->GetPauseDownload();
			m_downloadLimit = g_pOptions->GetDownloadRate();
			m_threadCount = Thread::GetThreadCount();
			g_pStatMeter->CalcTotalStat(&m_upTimeSec, &m_dnTimeSec, &m_allBytes, &m_standBy);

			DownloadQueue *downloadQueue = DownloadQueue::Lock();
			m_postJobCount = 0;
			for (NZBList::iterator it = downloadQueue->GetQueue()->begin(); it != downloadQueue->GetQueue()->end(); it++)
			{
				NZBInfo* nzbInfo = *it;
				m_postJobCount += nzbInfo->GetPostInfo() ? 1 : 0;
			}
			downloadQueue->CalcRemainingSize(&m_remainingSize, NULL);
			DownloadQueue::Unlock();

		}
	}
	return true;
}

void Frontend::FreeData()
{
	if (IsRemoteMode())
	{
		m_remoteMessages.Clear();

		DownloadQueue* downloadQueue = DownloadQueue::Lock();
		downloadQueue->GetQueue()->Clear();
		DownloadQueue::Unlock();
	}
}

MessageList* Frontend::LockMessages()
{
	if (IsRemoteMode())
	{
		return &m_remoteMessages;
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

DownloadQueue* Frontend::LockQueue()
{
	return DownloadQueue::Lock();
}

void Frontend::UnlockQueue()
{
	DownloadQueue::Unlock();
}

bool Frontend::IsRemoteMode()
{
	return g_pOptions->GetRemoteClientMode();
}

void Frontend::ServerPauseUnpause(bool pause)
{
	if (IsRemoteMode())
	{
		RequestPauseUnpause(pause);
	}
	else
	{
		g_pOptions->SetResumeTime(0);
		g_pOptions->SetPauseDownload(pause);
	}
}

void Frontend::ServerSetDownloadRate(int rate)
{
	if (IsRemoteMode())
	{
		RequestSetDownloadRate(rate);
	}
	else
	{
		g_pOptions->SetDownloadRate(rate);
	}
}

bool Frontend::ServerEditQueue(DownloadQueue::EEditAction action, int offset, int id)
{
	if (IsRemoteMode())
	{
		return RequestEditQueue(action, offset, id);
	}
	else
	{
		DownloadQueue* downloadQueue = LockQueue();
		bool ok = downloadQueue->EditEntry(id, action, offset, NULL);
		UnlockQueue();
		return ok;
	}
	return false;
}

void Frontend::InitMessageBase(SNZBRequestBase* messageBase, int request, int size)
{
	messageBase->m_signature	= htonl(NZBMESSAGE_SIGNATURE);
	messageBase->m_type = htonl(request);
	messageBase->m_structSize = htonl(size);

	strncpy(messageBase->m_username, g_pOptions->GetControlUsername(), NZBREQUESTPASSWORDSIZE - 1);
	messageBase->m_username[NZBREQUESTPASSWORDSIZE - 1] = '\0';

	strncpy(messageBase->m_password, g_pOptions->GetControlPassword(), NZBREQUESTPASSWORDSIZE);
	messageBase->m_password[NZBREQUESTPASSWORDSIZE - 1] = '\0';
}

bool Frontend::RequestMessages()
{
	const char* controlIP = !strcmp(g_pOptions->GetControlIP(), "0.0.0.0") ? "127.0.0.1" : g_pOptions->GetControlIP();
	Connection connection(controlIP, g_pOptions->GetControlPort(), false);

	bool OK = connection.Connect();
	if (!OK)
	{
		return false;
	}

	SNZBLogRequest LogRequest;
	InitMessageBase(&LogRequest.m_messageBase, remoteRequestLog, sizeof(LogRequest));
	LogRequest.m_lines = htonl(m_neededLogEntries);
	if (m_neededLogEntries == 0)
	{
		LogRequest.m_idFrom = htonl(m_neededLogFirstId > 0 ? m_neededLogFirstId : 1);
	}
	else
	{
		LogRequest.m_idFrom = 0;
	}

	if (!connection.Send((char*)(&LogRequest), sizeof(LogRequest)))
	{
		return false;
	}

	// Now listen for the returned log
	SNZBLogResponse LogResponse;
	bool read = connection.Recv((char*) &LogResponse, sizeof(LogResponse));
	if (!read || 
		(int)ntohl(LogResponse.m_messageBase.m_signature) != (int)NZBMESSAGE_SIGNATURE ||
		ntohl(LogResponse.m_messageBase.m_structSize) != sizeof(LogResponse))
	{
		return false;
	}

	char* buf = NULL;
	if (ntohl(LogResponse.m_trailingDataLength) > 0)
	{
		buf = (char*)malloc(ntohl(LogResponse.m_trailingDataLength));
		if (!connection.Recv(buf, ntohl(LogResponse.m_trailingDataLength)))
		{
			free(buf);
			return false;
		}
	}

	connection.Disconnect();

	if (ntohl(LogResponse.m_trailingDataLength) > 0)
	{
		char* bufPtr = (char*)buf;
		for (unsigned int i = 0; i < ntohl(LogResponse.m_nrTrailingEntries); i++)
		{
			SNZBLogResponseEntry* logAnswer = (SNZBLogResponseEntry*) bufPtr;

			char* text = bufPtr + sizeof(SNZBLogResponseEntry);

			Message* message = new Message(ntohl(logAnswer->m_id), (Message::EKind)ntohl(logAnswer->m_kind), ntohl(logAnswer->m_time), text);
			m_remoteMessages.push_back(message);

			bufPtr += sizeof(SNZBLogResponseEntry) + ntohl(logAnswer->m_textLen);
		}

		free(buf);
	}

	return true;
}

bool Frontend::RequestFileList()
{
	const char* controlIP = !strcmp(g_pOptions->GetControlIP(), "0.0.0.0") ? "127.0.0.1" : g_pOptions->GetControlIP();
	Connection connection(controlIP, g_pOptions->GetControlPort(), false);

	bool OK = connection.Connect();
	if (!OK)
	{
		return false;
	}

	SNZBListRequest ListRequest;
	InitMessageBase(&ListRequest.m_messageBase, remoteRequestList, sizeof(ListRequest));
	ListRequest.m_fileList = htonl(m_fileList);
	ListRequest.m_serverState = htonl(m_summary);

	if (!connection.Send((char*)(&ListRequest), sizeof(ListRequest)))
	{
		return false;
	}

	// Now listen for the returned list
	SNZBListResponse ListResponse;
	bool read = connection.Recv((char*) &ListResponse, sizeof(ListResponse));
	if (!read || 
		(int)ntohl(ListResponse.m_messageBase.m_signature) != (int)NZBMESSAGE_SIGNATURE ||
		ntohl(ListResponse.m_messageBase.m_structSize) != sizeof(ListResponse))
	{
		return false;
	}

	char* buf = NULL;
	if (ntohl(ListResponse.m_trailingDataLength) > 0)
	{
		buf = (char*)malloc(ntohl(ListResponse.m_trailingDataLength));
		if (!connection.Recv(buf, ntohl(ListResponse.m_trailingDataLength)))
		{
			free(buf);
			return false;
		}
	}

	connection.Disconnect();

	if (m_summary)
	{
		m_pauseDownload = ntohl(ListResponse.m_downloadPaused);
		m_remainingSize = Util::JoinInt64(ntohl(ListResponse.m_remainingSizeHi), ntohl(ListResponse.m_remainingSizeLo));
		m_currentDownloadSpeed = ntohl(ListResponse.m_downloadRate);
		m_downloadLimit = ntohl(ListResponse.m_downloadLimit);
		m_threadCount = ntohl(ListResponse.m_threadCount);
		m_postJobCount = ntohl(ListResponse.m_postJobCount);
		m_upTimeSec = ntohl(ListResponse.m_upTimeSec);
		m_dnTimeSec = ntohl(ListResponse.m_downloadTimeSec);
		m_standBy = ntohl(ListResponse.m_downloadStandBy);
		m_allBytes = Util::JoinInt64(ntohl(ListResponse.m_downloadedBytesHi), ntohl(ListResponse.m_downloadedBytesLo));
	}

	if (m_fileList && ntohl(ListResponse.m_trailingDataLength) > 0)
	{
		RemoteClient client;
		client.SetVerbose(false);
		
		DownloadQueue* downloadQueue = LockQueue();
		client.BuildFileList(&ListResponse, buf, downloadQueue);
		UnlockQueue();
	}

	if (buf)
	{
		free(buf);
	}

	return true;
}

bool Frontend::RequestPauseUnpause(bool pause)
{
	RemoteClient client;
	client.SetVerbose(false);
	return client.RequestServerPauseUnpause(pause, remotePauseUnpauseActionDownload);
}

bool Frontend::RequestSetDownloadRate(int rate)
{
	RemoteClient client;
	client.SetVerbose(false);
	return client.RequestServerSetDownloadRate(rate);
}

bool Frontend::RequestEditQueue(DownloadQueue::EEditAction action, int offset, int id)
{
	RemoteClient client;
	client.SetVerbose(false);
	return client.RequestServerEditQueue(action, offset, NULL, &id, 1, NULL, remoteMatchModeId);
}
