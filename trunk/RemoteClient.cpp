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
#include <arpa/inet.h>
#include <netinet/in.h>
#endif
#include <stdarg.h>

#include "nzbget.h"
#include "RemoteClient.h"
#include "DownloadInfo.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"

extern Options* g_pOptions;

RemoteClient::RemoteClient()
{
	m_pConnection	= NULL;
	m_pNetAddress	= NULL;
	m_bVerbose		= true;

	/*
	printf("sizeof(SNZBRequestBase)=%i\n", sizeof(SNZBRequestBase));
	printf("sizeof(SNZBDownloadRequest)=%i\n", sizeof(SNZBDownloadRequest));
	printf("sizeof(SNZBListRequest)=%i\n", sizeof(SNZBListRequest));
	printf("sizeof(SNZBListResponse)=%i\n", sizeof(SNZBListResponse));
	printf("sizeof(SNZBListResponseEntry)=%i\n", sizeof(SNZBListResponseEntry));
	printf("sizeof(SNZBLogRequest)=%i\n", sizeof(SNZBLogRequest));
	printf("sizeof(SNZBLogResponse)=%i\n", sizeof(SNZBLogResponse));
	printf("sizeof(SNZBLogResponseEntry)=%i\n", sizeof(SNZBLogResponseEntry));
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

void RemoteClient::InitMessageBase(SNZBRequestBase* pMessageBase, int iRequest, int iSize)
{
	pMessageBase->m_iSignature	= htonl(NZBMESSAGE_SIGNATURE);
	pMessageBase->m_iType = htonl(iRequest);
	pMessageBase->m_iStructSize = htonl(iSize);
	strncpy(pMessageBase->m_szPassword, g_pOptions->GetServerPassword(), NZBREQUESTPASSWORDSIZE - 1);
	pMessageBase->m_szPassword[NZBREQUESTPASSWORDSIZE - 1] = '\0';
}

bool RemoteClient::ReceiveBoolResponse()
{
	printf("request sent\n");

	// all bool-responses have the same format of structure, we use SNZBDownloadResponse here
	SNZBDownloadResponse BoolResponse;
	memset(&BoolResponse, 0, sizeof(BoolResponse));

	int iResponseLen = m_pConnection->Recv((char*)&BoolResponse, sizeof(BoolResponse));
	if (iResponseLen != sizeof(BoolResponse) || 
		(int)ntohl(BoolResponse.m_MessageBase.m_iSignature) != (int)NZBMESSAGE_SIGNATURE ||
		ntohl(BoolResponse.m_MessageBase.m_iStructSize) != sizeof(BoolResponse))
	{
		printf("invaid response received: either not nzbget-server or wrong server version\n");
		return false;
	}

	int iTextLen = ntohl(BoolResponse.m_iTrailingDataLength);
	char* buf = (char*)malloc(iTextLen);
	iResponseLen = m_pConnection->Recv(buf, iTextLen);
	if (iResponseLen != iTextLen)
	{
		printf("invaid response received: either not nzbget-server or wrong server version\n");
		return false;
	}

	printf("server returned: %s\n", buf);
	free(buf);
	return ntohl(BoolResponse.m_bSuccess);
}

/*
 * Sends a message to the running nzbget process.
 */
bool RemoteClient::RequestServerDownload(const char* szName, bool bAddFirst)
{
	// Read the file into the buffer
	char* szBuffer	= NULL;
	int iLength		= 0;
	if (!LoadFileIntoBuffer(szName, &szBuffer, &iLength))
	{
		printf("Could not load file %s\n", szName);
		return false;
	}

	bool OK = InitConnection();
	if (OK)
	{
		SNZBDownloadRequest DownloadRequest;
		InitMessageBase(&DownloadRequest.m_MessageBase, eRemoteRequestDownload, sizeof(DownloadRequest));
		DownloadRequest.m_bAddFirst = htonl(bAddFirst);
		DownloadRequest.m_iTrailingDataLength = htonl(iLength);

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
			OK = ReceiveBoolResponse();
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
	InitMessageBase(&ListRequest.m_MessageBase, eRemoteRequestList, sizeof(ListRequest));
	ListRequest.m_bFileList = htonl(true);
	ListRequest.m_bServerState = htonl(true);

	if (m_pConnection->Send((char*)(&ListRequest), sizeof(ListRequest)) < 0)
	{
		perror("m_pConnection->Send");
		return false;
	}

	// Now listen for the returned list
	SNZBListResponse ListResponse;
	int iResponseLen = m_pConnection->Recv((char*) &ListResponse, sizeof(ListResponse));
	if (iResponseLen != sizeof(ListResponse) || 
		(int)ntohl(ListResponse.m_MessageBase.m_iSignature) != (int)NZBMESSAGE_SIGNATURE ||
		ntohl(ListResponse.m_MessageBase.m_iStructSize) != sizeof(ListResponse))
	{
		printf("invaid response received: either not nzbget-server or wrong server version\n");
		return false;
	}

	char* pBuf = NULL;
	if (ntohl(ListResponse.m_iTrailingDataLength) > 0)
	{
		pBuf = (char*)malloc(ntohl(ListResponse.m_iTrailingDataLength));
		if (!m_pConnection->RecvAll(pBuf, ntohl(ListResponse.m_iTrailingDataLength)))
		{
			free(pBuf);
			return false;
		}
	}

	m_pConnection->Disconnect();

	if (ntohl(ListResponse.m_iTrailingDataLength) == 0)
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
		for (unsigned int i = 0; i < ntohl(ListResponse.m_iNrTrailingEntries); i++)
		{
			SNZBListResponseEntry* pListAnswer = (SNZBListResponseEntry*) pBufPtr;

			long long lFileSize = JoinInt64(ntohl(pListAnswer->m_iFileSizeHi), ntohl(pListAnswer->m_iFileSizeLo));
			long long lRemainingSize = JoinInt64(ntohl(pListAnswer->m_iRemainingSizeHi), ntohl(pListAnswer->m_iRemainingSizeLo));

			char szCompleted[100];
			szCompleted[0] = '\0';
			if (lRemainingSize < lFileSize)
			{
				sprintf(szCompleted, ", %i%s", (int)(100 - lRemainingSize * 100.0 / lFileSize), "%");
			}
			char szStatus[100];
			if (ntohl(pListAnswer->m_bPaused))
			{
				sprintf(szStatus, " (paused)");
				lPaused += lRemainingSize;
			}
			else
			{
				szStatus[0] = '\0';
				lRemaining += lRemainingSize;
			}
			char* szNZBFilename = pBufPtr + sizeof(SNZBListResponseEntry);
			char* szFilename = pBufPtr + sizeof(SNZBListResponseEntry) + ntohl(pListAnswer->m_iNZBFilenameLen) + ntohl(pListAnswer->m_iSubjectLen);
			
			char szNZBNiceName[1024];
			FileInfo::MakeNiceNZBName(szNZBFilename, szNZBNiceName, 1024);
			
			printf("[%i] %s%c%s (%.2f MB%s)%s\n", ntohl(pListAnswer->m_iID), szNZBNiceName, (int)PATH_SEPARATOR, szFilename, lFileSize / 1024.0 / 1024.0, szCompleted, szStatus);

			pBufPtr += sizeof(SNZBListResponseEntry) + ntohl(pListAnswer->m_iNZBFilenameLen) +
				ntohl(pListAnswer->m_iSubjectLen) + ntohl(pListAnswer->m_iFilenameLen) + ntohl(pListAnswer->m_iDestDirLen);
		}

		printf("-----------------------------------\n");
		printf("Files: %i\n", ntohl(ListResponse.m_iNrTrailingEntries));
		if (lPaused > 0)
		{
			printf("Remaining size: %.2f MB (+%.2f MB paused)\n", lRemaining / 1024.0 / 1024.0, lPaused / 1024.0 / 1024.0);
		}
		else
		{
			printf("Remaining size: %.2f MB\n", lRemaining / 1024.0 / 1024.0);
		}
		printf("Download rate: %.1f KB/s\n", (float)(ntohl(ListResponse.m_iDownloadRate) / 1024.0));

		free(pBuf);
	}

	printf("Threads running: %i\n", ntohl(ListResponse.m_iThreadCount));
	printf("Server state: %s\n", ntohl(ListResponse.m_bServerPaused) ? "Paused" : "Running");

	if (ntohl(ListResponse.m_iDownloadLimit) > 0)
	{
		printf("Speed limit: %.1f KB/s\n", (float)(ntohl(ListResponse.m_iDownloadLimit) / 1024.0));
	}

	if (ntohl(ListResponse.m_iParJobCount) > 0)
	{
		printf("Par-jobs: %i\n", (int)ntohl(ListResponse.m_iParJobCount));
	}

	return true;
}

bool RemoteClient::RequestServerLog(int iLines)
{
	if (!InitConnection()) return false;

	SNZBLogRequest LogRequest;
	InitMessageBase(&LogRequest.m_MessageBase, eRemoteRequestLog, sizeof(LogRequest));
	LogRequest.m_iLines = htonl(iLines);
	LogRequest.m_iIDFrom = 0;

	if (m_pConnection->Send((char*)(&LogRequest), sizeof(LogRequest)) < 0)
	{
		perror("m_pConnection->Send");
		return false;
	}

	// Now listen for the returned log
	SNZBLogResponse LogResponse;
	int iResponseLen = m_pConnection->Recv((char*) &LogResponse, sizeof(LogResponse));
	if (iResponseLen != sizeof(LogResponse) || 
		(int)ntohl(LogResponse.m_MessageBase.m_iSignature) != (int)NZBMESSAGE_SIGNATURE ||
		ntohl(LogResponse.m_MessageBase.m_iStructSize) != sizeof(LogResponse))
	{
		printf("invaid response received: either not nzbget-server or wrong server version\n");
		return false;
	}

	char* pBuf = NULL;
	if (ntohl(LogResponse.m_iTrailingDataLength) > 0)
	{
		pBuf = (char*)malloc(ntohl(LogResponse.m_iTrailingDataLength));
		if (!m_pConnection->RecvAll(pBuf, ntohl(LogResponse.m_iTrailingDataLength)))
		{
			free(pBuf);
			return false;
		}
	}

	m_pConnection->Disconnect();

	if (LogResponse.m_iTrailingDataLength == 0)
	{
		printf("Log is empty\n");
	}
	else
	{
		printf("Log (last %i entries)\n", ntohl(LogResponse.m_iNrTrailingEntries));
		printf("-----------------------------------\n");

		char* pBufPtr = (char*)pBuf;
		for (unsigned int i = 0; i < ntohl(LogResponse.m_iNrTrailingEntries); i++)
		{
			SNZBLogResponseEntry* pLogAnswer = (SNZBLogResponseEntry*) pBufPtr;

			char* szText = pBufPtr + sizeof(SNZBLogResponseEntry);
			switch (ntohl(pLogAnswer->m_iKind))
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

			pBufPtr += sizeof(SNZBLogResponseEntry) + ntohl(pLogAnswer->m_iTextLen);
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
	InitMessageBase(&PauseUnpauseRequest.m_MessageBase, eRemoteRequestPauseUnpause, sizeof(PauseUnpauseRequest));
	PauseUnpauseRequest.m_bPause = htonl(bPause);

	if (m_pConnection->Send((char*)(&PauseUnpauseRequest), sizeof(PauseUnpauseRequest)) < 0)
	{
		perror("m_pConnection->Send");
		m_pConnection->Disconnect();
		return false;
	}

	bool OK = ReceiveBoolResponse();
	m_pConnection->Disconnect();

	return OK;
}

bool RemoteClient::RequestServerSetDownloadRate(float fRate)
{
	if (!InitConnection()) return false;

	SNZBSetDownloadRateRequest SetDownloadRateRequest;
	InitMessageBase(&SetDownloadRateRequest.m_MessageBase, eRemoteRequestSetDownloadRate, sizeof(SetDownloadRateRequest));
	SetDownloadRateRequest.m_iDownloadRate = htonl((unsigned int)(fRate * 1024));

	if (m_pConnection->Send((char*)(&SetDownloadRateRequest), sizeof(SetDownloadRateRequest)) < 0)
	{
		perror("m_pConnection->Send");
		m_pConnection->Disconnect();
		return false;
	}

	bool OK = ReceiveBoolResponse();
	m_pConnection->Disconnect();

	return OK;
}

bool RemoteClient::RequestServerDumpDebug()
{
	if (!InitConnection()) return false;

	SNZBDumpDebugRequest DumpDebugInfo;
	InitMessageBase(&DumpDebugInfo.m_MessageBase, eRemoteRequestDumpDebug, sizeof(DumpDebugInfo));

	if (m_pConnection->Send((char*)(&DumpDebugInfo), sizeof(DumpDebugInfo)) < 0)
	{
		perror("m_pConnection->Send");
		m_pConnection->Disconnect();
		return false;
	}

	bool OK = ReceiveBoolResponse();
	m_pConnection->Disconnect();

	return OK;
}

bool RemoteClient::RequestServerEditQueue(int iAction, int iOffset, int* pIDList, int iIDCount, bool bSmartOrder)
{
	if (iIDCount <= 0 || pIDList == NULL)
	{
		printf("File(s) not specified\n");
		return false;
	}

	if (!InitConnection()) return false;

	int iLength = sizeof(int32_t) * iIDCount;

	SNZBEditQueueRequest EditQueueRequest;
	InitMessageBase(&EditQueueRequest.m_MessageBase, eRemoteRequestEditQueue, sizeof(EditQueueRequest));
	EditQueueRequest.m_iAction = htonl(iAction);
	EditQueueRequest.m_iOffset = htonl((int)iOffset);
	EditQueueRequest.m_bSmartOrder = htonl(bSmartOrder);
	EditQueueRequest.m_iNrTrailingEntries = htonl(iIDCount);
	EditQueueRequest.m_iTrailingDataLength = htonl(iLength);

	int32_t* pIDs = (int32_t*)malloc(iLength);
	
	for (int i = 0; i < iIDCount; i++)
	{
		pIDs[i] = htonl(pIDList[i]);
	}
			
	bool OK = false;
	if (m_pConnection->Send((char*)(&EditQueueRequest), sizeof(EditQueueRequest)) < 0)
	{
		perror("m_pConnection->Send");
	}
	else
	{
		m_pConnection->Send((char*)pIDs, iLength);
		OK = ReceiveBoolResponse();
		m_pConnection->Disconnect();
	}

	free(pIDs);

	m_pConnection->Disconnect();
	return OK;
}

bool RemoteClient::RequestServerShutdown()
{
	if (!InitConnection()) return false;

	SNZBShutdownRequest ShutdownRequest;
	InitMessageBase(&ShutdownRequest.m_MessageBase, eRemoteRequestShutdown, sizeof(ShutdownRequest));

	bool OK = m_pConnection->Send((char*)(&ShutdownRequest), sizeof(ShutdownRequest)) >= 0;
	if (OK)
	{
		OK = ReceiveBoolResponse();
	}
	else
	{
		perror("m_pConnection->Send");
	}

	m_pConnection->Disconnect();
	return OK;
}
