/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2005  Bo Cordes Petersen <placebodk@sourceforge.net>
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
#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <stdarg.h>

#include "nzbget.h"
#include "RemoteClient.h"
#include "Options.h"
#include "NZBFile.h"
#include "Log.h"
#include "Util.h"

extern Options* g_pOptions;

RemoteClient::RemoteClient()
{
	m_pConnection	= NULL;
	m_pNetAddress	= NULL;
	m_bVerbose		= true;

	/*
	printf("sizeof(SNZBMessageBase)=%i\n", sizeof(SNZBMessageBase));
	printf("sizeof(SNZBDownloadRequest)=%i\n", sizeof(SNZBDownloadRequest));
	printf("sizeof(SNZBListRequest)=%i\n", sizeof(SNZBListRequest));
	printf("sizeof(SNZBListRequestAnswer)=%i\n", sizeof(SNZBListRequestAnswer));
	printf("sizeof(SNZBListRequestAnswerEntry)=%i\n", sizeof(SNZBListRequestAnswerEntry));
	printf("sizeof(SNZBLogRequest)=%i\n", sizeof(SNZBLogRequest));
	printf("sizeof(SNZBLogRequestAnswer)=%i\n", sizeof(SNZBLogRequestAnswer));
	printf("sizeof(SNZBLogRequestAnswerEntry)=%i\n", sizeof(SNZBLogRequestAnswerEntry));
	printf("sizeof(SNZBPauseUnpauseRequest)=%i\n", sizeof(SNZBPauseUnpauseRequest));
	printf("sizeof(SNZBSetDownloadRateRequest)=%i\n", sizeof(SNZBSetDownloadRateRequest));
	printf("sizeof(SNZBEditQueueRequest)=%i\n", sizeof(SNZBEditQueueRequest));
	printf("sizeof(SNZBDumpDebugRequest)=%i\n", sizeof(SNZBDumpDebugRequest));
	*/
}

RemoteClient::~RemoteClient()
{
	if (m_pConnection)
	{
		delete m_pConnection;
	}

	if (m_pNetAddress)
	{
		delete m_pNetAddress;
	}
}

void RemoteClient::printf(char * msg,...)
{
	if (m_bVerbose)
	{
		va_list ap;
		va_start(ap, msg);
		::vprintf(msg, ap);
		va_end(ap);
	}
}

void RemoteClient::perror(char * msg)
{
	if (m_bVerbose)
	{
		::perror(msg);
	}
}

bool RemoteClient::InitConnection()
{
	// Create a connection to the server
	m_pNetAddress	= new NetAddress(g_pOptions->GetServerIP(), g_pOptions->GetServerPort());
	m_pConnection	= new Connection(m_pNetAddress);

	bool OK = m_pConnection->Connect() >= 0;
	if (!OK)
	{
		printf("Unable to send request to nzbserver at %s (port %i)\n", g_pOptions->GetServerIP(), g_pOptions->GetServerPort());
	}
	return OK;
}

void RemoteClient::InitMessageBase(SNZBMessageBase* pMessageBase, int iRequest, int iSize)
{
	pMessageBase->m_iId	= NZBMESSAGE_SIGNATURE;
	pMessageBase->m_iType = iRequest;
	pMessageBase->m_iSize = iSize;
	strncpy(pMessageBase->m_szPassword, g_pOptions->GetServerPassword(), NZBREQUESTPASSWORDSIZE - 1);
	pMessageBase->m_szPassword[NZBREQUESTPASSWORDSIZE - 1] = '\0';
}

void RemoteClient::ReceiveCommandResult()
{
	char szAnswer[1024];
	strcpy(szAnswer, "N/A");
	printf("request sent\n");
	m_pConnection->Recv(szAnswer, 1024);
	printf("nzbserver returned: %s\n", szAnswer);
}

/*
 * Sends a message to the running nzbget process.
 */
bool RemoteClient::RequestServerDownload(const char* szName, bool bAddFirst)
{
	// Read the file into the buffer
	char* szBuffer	= NULL;
	int iLength		= 0;
	if (!NZBFile::LoadFileIntoBuffer(szName, &szBuffer, &iLength))
	{
		printf("Could not load file %s\n", szName);
		return false;
	}

	bool OK = InitConnection();
	if (OK)
	{
		SNZBDownloadRequest DownloadRequest;
		InitMessageBase(&DownloadRequest.m_MessageBase, NZBMessageRequest::eRequestDownload, sizeof(DownloadRequest));
		DownloadRequest.m_bAddFirst = bAddFirst;
		DownloadRequest.m_iTrailingDataLength = iLength;

		strncpy(DownloadRequest.m_szFilename, szName, NZBREQUESTFILENAMESIZE - 1);
		DownloadRequest.m_szFilename[NZBREQUESTFILENAMESIZE-1] = '\0';

		if (m_pConnection->Send((char*)(&DownloadRequest), sizeof(DownloadRequest)) < 0)
		{
			perror("m_pConnection->Send");
			OK = false;
		}
		else
		{
			m_pConnection->Send(szBuffer, iLength);
			ReceiveCommandResult();
			m_pConnection->Disconnect();
		}
	}

	// Cleanup
	if (szBuffer)
	{
		free(szBuffer);
	}

	return OK;
}

bool RemoteClient::RequestServerList()
{
	if (!InitConnection()) return false;

	SNZBListRequest ListRequest;
	InitMessageBase(&ListRequest.m_MessageBase, NZBMessageRequest::eRequestList, sizeof(ListRequest));
	ListRequest.m_bFileList = true;
	ListRequest.m_bServerState = true;

	if (m_pConnection->Send((char*)(&ListRequest), sizeof(ListRequest)) < 0)
	{
		perror("m_pConnection->Send");
		return false;
	}

	// Now listen for the returned list
	SNZBListRequestAnswer ListRequestAnswer;
	if (m_pConnection->Recv((char*) &ListRequestAnswer, sizeof(ListRequestAnswer)) < 0)
	{
		return false;
	}

	char* pBuf = NULL;
	if (ListRequestAnswer.m_iTrailingDataLength > 0)
	{
		pBuf = (char*)malloc(ListRequestAnswer.m_iTrailingDataLength);
		if (!m_pConnection->RecvAll(pBuf, ListRequestAnswer.m_iTrailingDataLength))
		{
			free(pBuf);
			return false;
		}
	}

	m_pConnection->Disconnect();

	if (ListRequestAnswer.m_iTrailingDataLength == 0)
	{
		printf("Server has no files queued for download\n");
	}
	else
	{
		printf("Queue List\n");
		printf("-----------------------------------\n");

		long long lRemaining = 0;
		long long lPaused = 0;
		char* pBufPtr = (char*)pBuf;
		for (int i = 0; i < ListRequestAnswer.m_iNrTrailingEntries; i++)
		{
			SNZBListRequestAnswerEntry* pListAnswer = (SNZBListRequestAnswerEntry*) pBufPtr;

			char szCompleted[100];
			szCompleted[0] = '\0';
			if (pListAnswer->m_iRemainingSize < pListAnswer->m_iFileSize)
			{
				sprintf(szCompleted, ", %i%s", (int)(100 - pListAnswer->m_iRemainingSize * 100.0 / pListAnswer->m_iFileSize), "\%");
			}
			char szStatus[100];
			if (pListAnswer->m_bPaused)
			{
				sprintf(szStatus, " (paused)");
				lPaused += pListAnswer->m_iRemainingSize;
			}
			else
			{
				szStatus[0] = '\0';
				lRemaining += pListAnswer->m_iRemainingSize;
			}
			char* szNZBFilename = pBufPtr + sizeof(SNZBListRequestAnswerEntry);
			char* szFilename = pBufPtr + sizeof(SNZBListRequestAnswerEntry) + pListAnswer->m_iNZBFilenameLen + pListAnswer->m_iSubjectLen;
			
			char szNZBNiceName[1024];
			FileInfo::MakeNiceNZBName(szNZBFilename, szNZBNiceName, 1024);
			
			printf("[%i] %s%c%s (%.2f MB%s)%s\n", pListAnswer->m_iID, szNZBNiceName, (int)PATH_SEPARATOR, szFilename, pListAnswer->m_iFileSize / 1024.0 / 1024.0, szCompleted, szStatus);

			pBufPtr += sizeof(SNZBListRequestAnswerEntry) + pListAnswer->m_iNZBFilenameLen + pListAnswer->m_iSubjectLen +
			           pListAnswer->m_iFilenameLen + pListAnswer->m_iDestDirLen;
		}

		printf("-----------------------------------\n");
		printf("Files: %i\n", ListRequestAnswer.m_iNrTrailingEntries);
		if (lPaused > 0)
		{
			printf("Remaining size: %.2f MB (+%.2f MB paused)\n", lRemaining / 1024.0 / 1024.0, lPaused / 1024.0 / 1024.0);
		}
		else
		{
			printf("Remaining size: %.2f MB\n", lRemaining / 1024.0 / 1024.0);
		}
		printf("Download rate: %.1f KB/s\n", ListRequestAnswer.m_fDownloadRate);

		free(pBuf);
	}

	printf("Threads running: %i\n", ListRequestAnswer.m_iThreadCount);
	printf("Server state: %s\n", ListRequestAnswer.m_bServerPaused ? "Paused" : "Running");

	if (ListRequestAnswer.m_fDownloadLimit > 0)
	{
		printf("Speed limit: %.1f KB/s\n", ListRequestAnswer.m_fDownloadLimit);
	}
	else
	{
		printf("Speed limit: Unlimited\n");
	}

	return true;
}

bool RemoteClient::RequestServerLog(int iLines)
{
	if (!InitConnection()) return false;

	SNZBLogRequest LogRequest;
	InitMessageBase(&LogRequest.m_MessageBase, NZBMessageRequest::eRequestLog, sizeof(LogRequest));
	LogRequest.m_iLines = iLines;
	LogRequest.m_iIDFrom = 0;

	if (m_pConnection->Send((char*)(&LogRequest), sizeof(LogRequest)) < 0)
	{
		perror("m_pConnection->Send");
		return false;
	}

	// Now listen for the returned log
	SNZBLogRequestAnswer LogRequestAnswer;
	if (m_pConnection->Recv((char*) &LogRequestAnswer, sizeof(LogRequestAnswer)) < 0)
	{
		return false;
	}

	char* pBuf = NULL;
	if (LogRequestAnswer.m_iTrailingDataLength > 0)
	{
		pBuf = (char*)malloc(LogRequestAnswer.m_iTrailingDataLength);
		if (!m_pConnection->RecvAll(pBuf, LogRequestAnswer.m_iTrailingDataLength))
		{
			free(pBuf);
			return false;
		}
	}

	m_pConnection->Disconnect();

	if (LogRequestAnswer.m_iTrailingDataLength == 0)
	{
		printf("Log is empty\n");
	}
	else
	{
		printf("Log (last %i entries)\n", LogRequestAnswer.m_iNrTrailingEntries);
		printf("-----------------------------------\n");

		char* pBufPtr = (char*)pBuf;
		for (int i = 0; i < LogRequestAnswer.m_iNrTrailingEntries; i++)
		{
			SNZBLogRequestAnswerEntry* pLogAnswer = (SNZBLogRequestAnswerEntry*) pBufPtr;

			char* szText = pBufPtr + sizeof(SNZBLogRequestAnswerEntry);
			switch (pLogAnswer->m_iKind)
			{
				case Message::mkDebug:
					printf("[DEBUG] %s\n", szText);
					break;
				case Message::mkError:
					printf("[ERROR] %s\n", szText);
					break;
				case Message::mkWarning:
					printf("[WARNING] %s\n", szText);
					break;
				case Message::mkInfo:
					printf("[INFO] %s\n", szText);
					break;
			}

			pBufPtr += sizeof(SNZBLogRequestAnswerEntry) + pLogAnswer->m_iTextLen;
		}

		printf("-----------------------------------\n");

		free(pBuf);
	}

	return true;
}

bool RemoteClient::RequestServerPauseUnpause(bool bPause)
{
	if (!InitConnection()) return false;

	SNZBPauseUnpauseRequest PauseUnpauseRequest;
	InitMessageBase(&PauseUnpauseRequest.m_MessageBase, NZBMessageRequest::eRequestPauseUnpause, sizeof(PauseUnpauseRequest));
	PauseUnpauseRequest.m_bPause = bPause;

	if (m_pConnection->Send((char*)(&PauseUnpauseRequest), sizeof(PauseUnpauseRequest)) < 0)
	{
		perror("m_pConnection->Send");
		m_pConnection->Disconnect();
		return false;
	}

	ReceiveCommandResult();
	m_pConnection->Disconnect();

	return true;
}

bool RemoteClient::RequestServerSetDownloadRate(float fRate)
{
	if (!InitConnection()) return false;

	SNZBSetDownloadRateRequest SetDownloadRateRequest;
	InitMessageBase(&SetDownloadRateRequest.m_MessageBase, NZBMessageRequest::eRequestSetDownloadRate, sizeof(SetDownloadRateRequest));
	SetDownloadRateRequest.m_fDownloadRate = fRate;

	if (m_pConnection->Send((char*)(&SetDownloadRateRequest), sizeof(SetDownloadRateRequest)) < 0)
	{
		perror("m_pConnection->Send");
		m_pConnection->Disconnect();
		return false;
	}

	ReceiveCommandResult();
	m_pConnection->Disconnect();

	return true;
}

bool RemoteClient::RequestServerDumpDebug()
{
	if (!InitConnection()) return false;

	SNZBDumpDebugRequest DumpDebugInfo;
	InitMessageBase(&DumpDebugInfo.m_MessageBase, NZBMessageRequest::eRequestDumpDebug, sizeof(DumpDebugInfo));
	DumpDebugInfo.m_iLevel = 0;

	if (m_pConnection->Send((char*)(&DumpDebugInfo), sizeof(DumpDebugInfo)) < 0)
	{
		perror("m_pConnection->Send");
		m_pConnection->Disconnect();
		return false;
	}

	ReceiveCommandResult();
	m_pConnection->Disconnect();

	return true;
}

bool RemoteClient::RequestServerEditQueue(int iAction, int iOffset, int iIDFrom, int iIDTo)
{
	if (iIDTo <= 0)
	{
		printf("File(s) not specified (use option -I)\n");
		return false;
	}

	if (!InitConnection()) return false;

	SNZBEditQueueRequest EditQueueRequest;
	InitMessageBase(&EditQueueRequest.m_MessageBase, NZBMessageRequest::eRequestEditQueue, sizeof(EditQueueRequest));
	EditQueueRequest.m_iAction = iAction;
	EditQueueRequest.m_iOffset = iOffset;
	EditQueueRequest.m_iIDFrom = iIDFrom;
	EditQueueRequest.m_iIDTo = iIDTo;

	bool OK = m_pConnection->Send((char*)(&EditQueueRequest), sizeof(EditQueueRequest)) >= 0;
	if (OK)
	{
		ReceiveCommandResult();
	}
	else
	{
		perror("m_pConnection->Send");
	}

	m_pConnection->Disconnect();
	return OK;
}

bool RemoteClient::RequestServerShutdown()
{
	if (!InitConnection()) return false;

	SNZBMessageBase QuitRequest;

	InitMessageBase(&QuitRequest, NZBMessageRequest::eRequestShutdown, sizeof(QuitRequest));

	bool OK = m_pConnection->Send((char*)(&QuitRequest), sizeof(QuitRequest)) >= 0;
	if (OK)
	{
		ReceiveCommandResult();
	}
	else
	{
		perror("m_pConnection->Send");
	}

	m_pConnection->Disconnect();
	return OK;
}
