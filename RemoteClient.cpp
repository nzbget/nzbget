/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2005 Bo Cordes Petersen <placebodk@sourceforge.net>
 *  Copyright (C) 2007-2011 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include <cstdio>
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
#include "Options.h"
#include "Log.h"
#include "Util.h"

extern Options* g_pOptions;

RemoteClient::RemoteClient()
{
	m_pConnection	= NULL;
	m_bVerbose		= true;

	/*
	printf("sizeof(SNZBRequestBase)=%i\n", sizeof(SNZBRequestBase));
	printf("sizeof(SNZBDownloadRequest)=%i\n", sizeof(SNZBDownloadRequest));
	printf("sizeof(SNZBListRequest)=%i\n", sizeof(SNZBListRequest));
	printf("sizeof(SNZBListResponse)=%i\n", sizeof(SNZBListResponse));
	printf("sizeof(SNZBListResponseFileEntry)=%i\n", sizeof(SNZBListResponseFileEntry));
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
}

void RemoteClient::printf(const char * msg,...)
{
	if (m_bVerbose)
	{
		va_list ap;
		va_start(ap, msg);
		::vprintf(msg, ap);
		va_end(ap);
	}
}

void RemoteClient::perror(const char * msg)
{
	if (m_bVerbose)
	{
		::perror(msg);
	}
}

bool RemoteClient::InitConnection()
{
	// Create a connection to the server
	m_pConnection = new Connection(g_pOptions->GetServerIP(), g_pOptions->GetServerPort(), false);

	bool OK = m_pConnection->Connect();
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
	printf("Request sent\n");

	// all bool-responses have the same format of structure, we use SNZBDownloadResponse here
	SNZBDownloadResponse BoolResponse;
	memset(&BoolResponse, 0, sizeof(BoolResponse));

	int iResponseLen = m_pConnection->Recv((char*)&BoolResponse, sizeof(BoolResponse));
	if (iResponseLen != sizeof(BoolResponse) || 
		(int)ntohl(BoolResponse.m_MessageBase.m_iSignature) != (int)NZBMESSAGE_SIGNATURE ||
		ntohl(BoolResponse.m_MessageBase.m_iStructSize) != sizeof(BoolResponse))
	{
		if (iResponseLen < 0)
		{
			printf("No response received (timeout)\n");
		}
		else
		{
			printf("Invalid response received: either not nzbget-server or wrong server version\n");
		}
		return false;
	}

	int iTextLen = ntohl(BoolResponse.m_iTrailingDataLength);
	char* buf = (char*)malloc(iTextLen);
	iResponseLen = m_pConnection->Recv(buf, iTextLen);
	if (iResponseLen != iTextLen)
	{
		if (iResponseLen < 0)
		{
			printf("No response received (timeout)\n");
		}
		else
		{
			printf("Invalid response received: either not nzbget-server or wrong server version\n");
		}
		free(buf);
		return false;
	}

	printf("server returned: %s\n", buf);
	free(buf);
	return ntohl(BoolResponse.m_bSuccess);
}

/*
 * Sends a message to the running nzbget process.
 */
bool RemoteClient::RequestServerDownload(const char* szFilename, const char* szCategory, bool bAddFirst)
{
	// Read the file into the buffer
	char* szBuffer	= NULL;
	int iLength		= 0;
	if (!Util::LoadFileIntoBuffer(szFilename, &szBuffer, &iLength))
	{
		printf("Could not load file %s\n", szFilename);
		return false;
	}

	bool OK = InitConnection();
	if (OK)
	{
		SNZBDownloadRequest DownloadRequest;
		InitMessageBase(&DownloadRequest.m_MessageBase, eRemoteRequestDownload, sizeof(DownloadRequest));
		DownloadRequest.m_bAddFirst = htonl(bAddFirst);
		DownloadRequest.m_iTrailingDataLength = htonl(iLength);

		strncpy(DownloadRequest.m_szFilename, szFilename, NZBREQUESTFILENAMESIZE - 1);
		DownloadRequest.m_szFilename[NZBREQUESTFILENAMESIZE-1] = '\0';
		DownloadRequest.m_szCategory[0] = '\0';
		if (szCategory)
		{
			strncpy(DownloadRequest.m_szCategory, szCategory, NZBREQUESTFILENAMESIZE - 1);
		}
		DownloadRequest.m_szCategory[NZBREQUESTFILENAMESIZE-1] = '\0';

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

void RemoteClient::BuildFileList(SNZBListResponse* pListResponse, const char* pTrailingData, DownloadQueue* pDownloadQueue)
{
	if (ntohl(pListResponse->m_iTrailingDataLength) > 0)
	{
		const char* pBufPtr = pTrailingData;

		// read nzb entries
		for (unsigned int i = 0; i < ntohl(pListResponse->m_iNrTrailingNZBEntries); i++)
		{
			SNZBListResponseNZBEntry* pListAnswer = (SNZBListResponseNZBEntry*) pBufPtr;

			const char* szFileName = pBufPtr + sizeof(SNZBListResponseNZBEntry);
			const char* szName = pBufPtr + sizeof(SNZBListResponseNZBEntry) + ntohl(pListAnswer->m_iFilenameLen);
			const char* szDestDir = pBufPtr + sizeof(SNZBListResponseNZBEntry) + ntohl(pListAnswer->m_iFilenameLen) + 
				ntohl(pListAnswer->m_iNameLen);
			const char* szCategory = pBufPtr + sizeof(SNZBListResponseNZBEntry) + ntohl(pListAnswer->m_iFilenameLen) + 
				ntohl(pListAnswer->m_iNameLen) + ntohl(pListAnswer->m_iDestDirLen);
			const char* m_szQueuedFilename = pBufPtr + sizeof(SNZBListResponseNZBEntry) + ntohl(pListAnswer->m_iFilenameLen) + 
				ntohl(pListAnswer->m_iNameLen) + ntohl(pListAnswer->m_iDestDirLen) + ntohl(pListAnswer->m_iCategoryLen);
			
			MatchedNZBInfo* pNZBInfo = new MatchedNZBInfo();
			pNZBInfo->SetSize(Util::JoinInt64(ntohl(pListAnswer->m_iSizeHi), ntohl(pListAnswer->m_iSizeLo)));
			pNZBInfo->SetFilename(szFileName);
			pNZBInfo->SetName(szName);
			pNZBInfo->SetDestDir(szDestDir);
			pNZBInfo->SetCategory(szCategory);
			pNZBInfo->SetQueuedFilename(m_szQueuedFilename);
			pNZBInfo->m_bMatch = ntohl(pListAnswer->m_bMatch);

			pNZBInfo->AddReference();
			pDownloadQueue->GetNZBInfoList()->Add(pNZBInfo);

			pBufPtr += sizeof(SNZBListResponseNZBEntry) + ntohl(pListAnswer->m_iFilenameLen) +
				ntohl(pListAnswer->m_iNameLen) + ntohl(pListAnswer->m_iDestDirLen) + 
				ntohl(pListAnswer->m_iCategoryLen) + ntohl(pListAnswer->m_iQueuedFilenameLen);
		}

		//read ppp entries
		for (unsigned int i = 0; i < ntohl(pListResponse->m_iNrTrailingPPPEntries); i++)
		{
			SNZBListResponsePPPEntry* pListAnswer = (SNZBListResponsePPPEntry*) pBufPtr;

			const char* szName = pBufPtr + sizeof(SNZBListResponsePPPEntry);
			const char* szValue = pBufPtr + sizeof(SNZBListResponsePPPEntry) + ntohl(pListAnswer->m_iNameLen);

			NZBInfo* pNZBInfo = pDownloadQueue->GetNZBInfoList()->at(ntohl(pListAnswer->m_iNZBIndex) - 1);
			pNZBInfo->SetParameter(szName, szValue);

			pBufPtr += sizeof(SNZBListResponsePPPEntry) + ntohl(pListAnswer->m_iNameLen) +
				ntohl(pListAnswer->m_iValueLen);
		}

		//read file entries
		for (unsigned int i = 0; i < ntohl(pListResponse->m_iNrTrailingFileEntries); i++)
		{
			SNZBListResponseFileEntry* pListAnswer = (SNZBListResponseFileEntry*) pBufPtr;

			const char* szSubject = pBufPtr + sizeof(SNZBListResponseFileEntry);
			const char* szFileName = pBufPtr + sizeof(SNZBListResponseFileEntry) + ntohl(pListAnswer->m_iSubjectLen);
			
			MatchedFileInfo* pFileInfo = new MatchedFileInfo();
			pFileInfo->SetID(ntohl(pListAnswer->m_iID));
			pFileInfo->SetSize(Util::JoinInt64(ntohl(pListAnswer->m_iFileSizeHi), ntohl(pListAnswer->m_iFileSizeLo)));
			pFileInfo->SetRemainingSize(Util::JoinInt64(ntohl(pListAnswer->m_iRemainingSizeHi), ntohl(pListAnswer->m_iRemainingSizeLo)));
			pFileInfo->SetPaused(ntohl(pListAnswer->m_bPaused));
			pFileInfo->SetSubject(szSubject);
			pFileInfo->SetFilename(szFileName);
			pFileInfo->SetFilenameConfirmed(ntohl(pListAnswer->m_bFilenameConfirmed));
			pFileInfo->SetActiveDownloads(ntohl(pListAnswer->m_iActiveDownloads));
			pFileInfo->SetPriority(ntohl(pListAnswer->m_iPriority));
			pFileInfo->m_bMatch = ntohl(pListAnswer->m_bMatch);

			NZBInfo* pNZBInfo = pDownloadQueue->GetNZBInfoList()->at(ntohl(pListAnswer->m_iNZBIndex) - 1);

			pFileInfo->SetNZBInfo(pNZBInfo);

			pDownloadQueue->GetFileQueue()->push_back(pFileInfo);

			pBufPtr += sizeof(SNZBListResponseFileEntry) + ntohl(pListAnswer->m_iSubjectLen) + 
				ntohl(pListAnswer->m_iFilenameLen);
		}
	}

	pDownloadQueue->GetNZBInfoList()->ReleaseAll();
}

bool RemoteClient::RequestServerList(bool bFiles, bool bGroups, const char* szPattern)
{
	if (!InitConnection()) return false;

	SNZBListRequest ListRequest;
	InitMessageBase(&ListRequest.m_MessageBase, eRemoteRequestList, sizeof(ListRequest));
	ListRequest.m_bFileList = htonl(true);
	ListRequest.m_bServerState = htonl(true);
	ListRequest.m_iMatchMode = htonl(szPattern ? eRemoteMatchModeRegEx : eRemoteMatchModeID);
	ListRequest.m_bMatchGroup = htonl(bGroups);
	if (szPattern)
	{
		strncpy(ListRequest.m_szPattern, szPattern, NZBREQUESTFILENAMESIZE - 1);
		ListRequest.m_szPattern[NZBREQUESTFILENAMESIZE-1] = '\0';
	}

	if (m_pConnection->Send((char*)(&ListRequest), sizeof(ListRequest)) < 0)
	{
		perror("m_pConnection->Send");
		return false;
	}

	printf("Request sent\n");

	// Now listen for the returned list
	SNZBListResponse ListResponse;
	int iResponseLen = m_pConnection->Recv((char*) &ListResponse, sizeof(ListResponse));
	if (iResponseLen != sizeof(ListResponse) || 
		(int)ntohl(ListResponse.m_MessageBase.m_iSignature) != (int)NZBMESSAGE_SIGNATURE ||
		ntohl(ListResponse.m_MessageBase.m_iStructSize) != sizeof(ListResponse))
	{
		if (iResponseLen < 0)
		{
			printf("No response received (timeout)\n");
		}
		else
		{
			printf("Invalid response received: either not nzbget-server or wrong server version\n");
		}
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

	if (szPattern && !ListResponse.m_bRegExValid)
	{
		printf("Error in regular expression\n");
		free(pBuf);
		return false;
	}

	if (bFiles)
	{
		if (ntohl(ListResponse.m_iTrailingDataLength) == 0)
		{
			printf("Server has no files queued for download\n");
		}
		else
		{
			printf("Queue List\n");
			printf("-----------------------------------\n");

			DownloadQueue cRemoteQueue;
			BuildFileList(&ListResponse, pBuf, &cRemoteQueue);

			long long lRemaining = 0;
			long long lPaused = 0;
			int iMatches = 0;

			for (FileQueue::iterator it = cRemoteQueue.GetFileQueue()->begin(); it != cRemoteQueue.GetFileQueue()->end(); it++)
			{
				FileInfo* pFileInfo = *it;

				char szPriority[100];
				szPriority[0] = '\0';
				if (pFileInfo->GetPriority() != 0)
				{
					sprintf(szPriority, "[%+i] ", pFileInfo->GetPriority());
				}

				char szCompleted[100];
				szCompleted[0] = '\0';
				if (pFileInfo->GetRemainingSize() < pFileInfo->GetSize())
				{
					sprintf(szCompleted, ", %i%s", (int)(100 - Util::Int64ToFloat(pFileInfo->GetRemainingSize()) * 100.0 / Util::Int64ToFloat(pFileInfo->GetSize())), "%");
				}

				char szThreads[100];
				szThreads[0] = '\0';
				if (pFileInfo->GetActiveDownloads() > 0)
				{
					sprintf(szThreads, ", %i thread%s", pFileInfo->GetActiveDownloads(), (pFileInfo->GetActiveDownloads() > 1 ? "s" : ""));
				}

				char szStatus[100];
				if (pFileInfo->GetPaused())
				{
					sprintf(szStatus, " (paused)");
					lPaused += pFileInfo->GetRemainingSize();
				}
				else
				{
					szStatus[0] = '\0';
					lRemaining += pFileInfo->GetRemainingSize();
				}

				if (!szPattern || ((MatchedFileInfo*)pFileInfo)->m_bMatch)
				{
					printf("[%i] %s%s/%s (%.2f MB%s%s)%s\n", pFileInfo->GetID(), szPriority, pFileInfo->GetNZBInfo()->GetName(),
						pFileInfo->GetFilename(),
						(float)(Util::Int64ToFloat(pFileInfo->GetSize()) / 1024.0 / 1024.0), 
						szCompleted, szThreads, szStatus);
					iMatches++;
				}

				delete pFileInfo;
			}

			if (iMatches == 0)
			{
				printf("No matches founds\n");
			}

			printf("-----------------------------------\n");
			printf("Files: %i\n", cRemoteQueue.GetFileQueue()->size());
			if (szPattern)
			{
				printf("Matches: %i\n", iMatches);
			}
			if (lPaused > 0)
			{
				printf("Remaining size: %.2f MB (+%.2f MB paused)\n", (float)(Util::Int64ToFloat(lRemaining) / 1024.0 / 1024.0), 
					(float)(Util::Int64ToFloat(lPaused) / 1024.0 / 1024.0));
			}
			else
			{
				printf("Remaining size: %.2f MB\n", (float)(Util::Int64ToFloat(lRemaining) / 1024.0 / 1024.0));
			}
		}
	}

	if (bGroups)
	{
		if (ntohl(ListResponse.m_iTrailingDataLength) == 0)
		{
			printf("Server has no files queued for download\n");
		}
		else
		{
			printf("Queue List\n");
			printf("-----------------------------------\n");

			DownloadQueue cRemoteQueue;
			BuildFileList(&ListResponse, pBuf, &cRemoteQueue);

			GroupQueue cGroupQueue;
			cRemoteQueue.BuildGroups(&cGroupQueue);

			long long lRemaining = 0;
			long long lPaused = 0;
			int iMatches = 0;

			for (GroupQueue::iterator it = cGroupQueue.begin(); it != cGroupQueue.end(); it++)
			{
				GroupInfo* pGroupInfo = *it;

				long long lUnpausedRemainingSize = pGroupInfo->GetRemainingSize() - pGroupInfo->GetPausedSize();
				lRemaining += lUnpausedRemainingSize;

				char szRemaining[20];
				Util::FormatFileSize(szRemaining, sizeof(szRemaining), lUnpausedRemainingSize);

				char szPriority[100];
				szPriority[0] = '\0';
				if (pGroupInfo->GetMinPriority() != 0 || pGroupInfo->GetMaxPriority() != 0)
				{
					if (pGroupInfo->GetMinPriority() == pGroupInfo->GetMaxPriority())
					{
						sprintf(szPriority, "[%+i] ", pGroupInfo->GetMinPriority());
					}
					else
					{
						sprintf(szPriority, "[%+i..%+i] ", pGroupInfo->GetMinPriority(), pGroupInfo->GetMaxPriority());
					}
				}

				char szPaused[20];
				szPaused[0] = '\0';
				if (pGroupInfo->GetPausedSize() > 0)
				{
					char szPausedSize[20];
					Util::FormatFileSize(szPausedSize, sizeof(szPausedSize), pGroupInfo->GetPausedSize());
					sprintf(szPaused, " + %s paused", szPausedSize);
					lPaused += pGroupInfo->GetPausedSize();
				}

				char szCategory[1024];
				szCategory[0] = '\0';
				if (pGroupInfo->GetNZBInfo()->GetCategory() && strlen(pGroupInfo->GetNZBInfo()->GetCategory()) > 0)
				{
					sprintf(szCategory, " (%s)", pGroupInfo->GetNZBInfo()->GetCategory());
				}

				char szThreads[100];
				szThreads[0] = '\0';
				if (pGroupInfo->GetActiveDownloads() > 0)
				{
					sprintf(szThreads, ", %i thread%s", pGroupInfo->GetActiveDownloads(), (pGroupInfo->GetActiveDownloads() > 1 ? "s" : ""));
				}

				char szParameters[1024];
				szParameters[0] = '\0';
				for (NZBParameterList::iterator it = pGroupInfo->GetNZBInfo()->GetParameters()->begin(); it != pGroupInfo->GetNZBInfo()->GetParameters()->end(); it++)
				{
					if (szParameters[0] == '\0')
					{
						strncat(szParameters, " (", 1024);
					}
					else
					{
						strncat(szParameters, ", ", 1024);
					}
					NZBParameter* pNZBParameter = *it;
					strncat(szParameters, pNZBParameter->GetName(), 1024);
					strncat(szParameters, "=", 1024);
					strncat(szParameters, pNZBParameter->GetValue(), 1024);
				}
				if (szParameters[0] != '\0')
				{
					strncat(szParameters, ")", 1024);
				}

				if (!szPattern || ((MatchedNZBInfo*)pGroupInfo->GetNZBInfo())->m_bMatch)
				{
					printf("[%i-%i] %s%s (%i file%s, %s%s%s)%s%s\n", pGroupInfo->GetFirstID(), pGroupInfo->GetLastID(), szPriority, 
						pGroupInfo->GetNZBInfo()->GetName(), pGroupInfo->GetRemainingFileCount(),
						pGroupInfo->GetRemainingFileCount() > 1 ? "s" : "", szRemaining, 
						szPaused, szThreads, szCategory, szParameters);
					iMatches++;
				}

				delete pGroupInfo;
			}

			for (FileQueue::iterator it = cRemoteQueue.GetFileQueue()->begin(); it != cRemoteQueue.GetFileQueue()->end(); it++)
			{
				delete *it;
			}

			if (iMatches == 0)
			{
				printf("No matches founds\n");
			}

			printf("-----------------------------------\n");
			printf("Groups: %i\n", cGroupQueue.size());
			if (szPattern)
			{
				printf("Matches: %i\n", iMatches);
			}
			printf("Files: %i\n", cRemoteQueue.GetFileQueue()->size());
			if (lPaused > 0)
			{
				printf("Remaining size: %.2f MB (+%.2f MB paused)\n", (float)(Util::Int64ToFloat(lRemaining) / 1024.0 / 1024.0), 
					(float)(Util::Int64ToFloat(lPaused) / 1024.0 / 1024.0));
			}
			else
			{
				printf("Remaining size: %.2f MB\n", (float)(Util::Int64ToFloat(lRemaining) / 1024.0 / 1024.0));
			}
		}
	}

	free(pBuf);

	long long lRemaining = Util::JoinInt64(ntohl(ListResponse.m_iRemainingSizeHi), ntohl(ListResponse.m_iRemainingSizeLo));

	if (!bFiles && !bGroups)
	{
		printf("Remaining size: %.2f MB\n", (float)(Util::Int64ToFloat(lRemaining) / 1024.0 / 1024.0));
	}

    if (ntohl(ListResponse.m_iDownloadRate) > 0 && 
		!ntohl(ListResponse.m_bDownloadPaused) && 
		!ntohl(ListResponse.m_bDownload2Paused) && 
		!ntohl(ListResponse.m_bDownloadStandBy))
    {
        long long remain_sec = (long long)(lRemaining / ntohl(ListResponse.m_iDownloadRate));
		int h = (int)(remain_sec / 3600);
		int m = (int)((remain_sec % 3600) / 60);
		int s = (int)(remain_sec % 60);
		printf("Remaining time: %.2d:%.2d:%.2d\n", h, m, s);
    }

	printf("Current download rate: %.1f KB/s\n", (float)(ntohl(ListResponse.m_iDownloadRate) / 1024.0));

	long long iAllBytes = Util::JoinInt64(ntohl(ListResponse.m_iDownloadedBytesHi), ntohl(ListResponse.m_iDownloadedBytesLo));
	float fAverageSpeed = Util::Int64ToFloat(ntohl(ListResponse.m_iDownloadTimeSec) > 0 ? iAllBytes / ntohl(ListResponse.m_iDownloadTimeSec) : 0);
	printf("Session download rate: %.1f KB/s\n", (float)(fAverageSpeed / 1024.0));

	if (ntohl(ListResponse.m_iDownloadLimit) > 0)
	{
		printf("Speed limit: %.1f KB/s\n", (float)(ntohl(ListResponse.m_iDownloadLimit) / 1024.0));
	}

	int sec = ntohl(ListResponse.m_iUpTimeSec);
	int h = sec / 3600;
	int m = (sec % 3600) / 60;
	int s = sec % 60;
	printf("Up time: %.2d:%.2d:%.2d\n", h, m, s);

	sec = ntohl(ListResponse.m_iDownloadTimeSec);
	h = sec / 3600;
	m = (sec % 3600) / 60;
	s = sec % 60;
	printf("Download time: %.2d:%.2d:%.2d\n", h, m, s);

	printf("Downloaded: %.2f MB\n", (float)(Util::Int64ToFloat(iAllBytes) / 1024.0 / 1024.0));

	printf("Threads running: %i\n", ntohl(ListResponse.m_iThreadCount));

	if (ntohl(ListResponse.m_iPostJobCount) > 0)
	{
		printf("Post-jobs: %i\n", (int)ntohl(ListResponse.m_iPostJobCount));
	}

	if (ntohl(ListResponse.m_bScanPaused))
	{
		printf("Scan state: Paused\n");
	}

	char szServerState[50];

	if (ntohl(ListResponse.m_bDownloadPaused) || ntohl(ListResponse.m_bDownload2Paused))
	{
		snprintf(szServerState, sizeof(szServerState), "%s%s", 
			ntohl(ListResponse.m_bDownloadStandBy) ? "Paused" : "Pausing",
			ntohl(ListResponse.m_bDownloadPaused) && ntohl(ListResponse.m_bDownload2Paused) ?
			" (+2)" : ntohl(ListResponse.m_bDownload2Paused) ? " (2)" : "");
	}
	else
	{
		snprintf(szServerState, sizeof(szServerState), "%s", ntohl(ListResponse.m_bDownloadStandBy) ? "" : "Downloading");
	}

	if (ntohl(ListResponse.m_iPostJobCount) > 0 || ntohl(ListResponse.m_bPostPaused))
	{
		strncat(szServerState, strlen(szServerState) > 0 ? ", Post-Processing" : "Post-Processing", sizeof(szServerState));
		if (ntohl(ListResponse.m_bPostPaused))
		{
			strncat(szServerState, " paused", sizeof(szServerState));
		}
	}

	if (strlen(szServerState) == 0)
	{
		strncpy(szServerState, "Stand-By", sizeof(szServerState));
	}

	printf("Server state: %s\n", szServerState);

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

	printf("Request sent\n");

	// Now listen for the returned log
	SNZBLogResponse LogResponse;
	int iResponseLen = m_pConnection->Recv((char*) &LogResponse, sizeof(LogResponse));
	if (iResponseLen != sizeof(LogResponse) || 
		(int)ntohl(LogResponse.m_MessageBase.m_iSignature) != (int)NZBMESSAGE_SIGNATURE ||
		ntohl(LogResponse.m_MessageBase.m_iStructSize) != sizeof(LogResponse))
	{
		if (iResponseLen < 0)
		{
			printf("No response received (timeout)\n");
		}
		else
		{
			printf("Invalid response received: either not nzbget-server or wrong server version\n");
		}
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
				case Message::mkDetail:
					printf("[DETAIL] %s\n", szText);
					break;
			}

			pBufPtr += sizeof(SNZBLogResponseEntry) + ntohl(pLogAnswer->m_iTextLen);
		}

		printf("-----------------------------------\n");

		free(pBuf);
	}

	return true;
}

bool RemoteClient::RequestServerPauseUnpause(bool bPause, eRemotePauseUnpauseAction iAction)
{
	if (!InitConnection()) return false;

	SNZBPauseUnpauseRequest PauseUnpauseRequest;
	InitMessageBase(&PauseUnpauseRequest.m_MessageBase, eRemoteRequestPauseUnpause, sizeof(PauseUnpauseRequest));
	PauseUnpauseRequest.m_bPause = htonl(bPause);
	PauseUnpauseRequest.m_iAction = htonl(iAction);

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

bool RemoteClient::RequestServerEditQueue(eRemoteEditAction iAction, int iOffset, const char* szText, 
	int* pIDList, int iIDCount, NameList* pNameList, eRemoteMatchMode iMatchMode, bool bSmartOrder)
{
	if ((iIDCount <= 0 || pIDList == NULL) && (pNameList == NULL || pNameList->size() == 0))
	{
		printf("File(s) not specified\n");
		return false;
	}

	if (!InitConnection()) return false;

	int iIDLength = sizeof(int32_t) * iIDCount;

	int iNameCount = 0;
	int iNameLength = 0;
	if (pNameList && pNameList->size() > 0)
	{
		for (NameList::iterator it = pNameList->begin(); it != pNameList->end(); it++)
		{
			const char *szName = *it;
			iNameLength += strlen(szName) + 1;
			iNameCount++;
		}
		// align size to 4-bytes, needed by ARM-processor (and may be others)
		iNameLength += iNameLength % 4 > 0 ? 4 - iNameLength % 4 : 0;
	}

	int iTextLen = szText ? strlen(szText) + 1 : 0;
	// align size to 4-bytes, needed by ARM-processor (and may be others)
	iTextLen += iTextLen % 4 > 0 ? 4 - iTextLen % 4 : 0;

	int iLength = iTextLen + iIDLength + iNameLength;

	SNZBEditQueueRequest EditQueueRequest;
	InitMessageBase(&EditQueueRequest.m_MessageBase, eRemoteRequestEditQueue, sizeof(EditQueueRequest));
	EditQueueRequest.m_iAction = htonl(iAction);
	EditQueueRequest.m_iMatchMode = htonl(iMatchMode);
	EditQueueRequest.m_iOffset = htonl((int)iOffset);
	EditQueueRequest.m_bSmartOrder = htonl(bSmartOrder);
	EditQueueRequest.m_iTextLen = htonl(iTextLen);
	EditQueueRequest.m_iNrTrailingIDEntries = htonl(iIDCount);
	EditQueueRequest.m_iNrTrailingNameEntries = htonl(iNameCount);
	EditQueueRequest.m_iTrailingNameEntriesLen = htonl(iNameLength);
	EditQueueRequest.m_iTrailingDataLength = htonl(iLength);

	char* pTrailingData = (char*)malloc(iLength);

	if (iTextLen > 0)
	{
		strcpy(pTrailingData, szText);
	}

	int32_t* pIDs = (int32_t*)(pTrailingData + iTextLen);
	
	for (int i = 0; i < iIDCount; i++)
	{
		pIDs[i] = htonl(pIDList[i]);
	}
	
	if (iNameCount > 0)
	{
		char *pNames = pTrailingData + iTextLen + iIDLength;
		for (NameList::iterator it = pNameList->begin(); it != pNameList->end(); it++)
		{
			const char *szName = *it;
			int iLen = strlen(szName);
			strncpy(pNames, szName, iLen + 1);
			pNames += iLen + 1;
		}
	}

	bool OK = false;
	if (m_pConnection->Send((char*)(&EditQueueRequest), sizeof(EditQueueRequest)) < 0)
	{
		perror("m_pConnection->Send");
	}
	else
	{
		m_pConnection->Send(pTrailingData, iLength);
		OK = ReceiveBoolResponse();
		m_pConnection->Disconnect();
	}

	free(pTrailingData);

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

bool RemoteClient::RequestServerVersion()
{
	if (!InitConnection()) return false;

	SNZBVersionRequest VersionRequest;
	InitMessageBase(&VersionRequest.m_MessageBase, eRemoteRequestVersion, sizeof(VersionRequest));

	bool OK = m_pConnection->Send((char*)(&VersionRequest), sizeof(VersionRequest)) >= 0;
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

bool RemoteClient::RequestPostQueue()
{
	if (!InitConnection()) return false;

	SNZBPostQueueRequest PostQueueRequest;
	InitMessageBase(&PostQueueRequest.m_MessageBase, eRemoteRequestPostQueue, sizeof(PostQueueRequest));

	if (m_pConnection->Send((char*)(&PostQueueRequest), sizeof(PostQueueRequest)) < 0)
	{
		perror("m_pConnection->Send");
		return false;
	}

	printf("Request sent\n");

	// Now listen for the returned list
	SNZBPostQueueResponse PostQueueResponse;
	int iResponseLen = m_pConnection->Recv((char*) &PostQueueResponse, sizeof(PostQueueResponse));
	if (iResponseLen != sizeof(PostQueueResponse) || 
		(int)ntohl(PostQueueResponse.m_MessageBase.m_iSignature) != (int)NZBMESSAGE_SIGNATURE ||
		ntohl(PostQueueResponse.m_MessageBase.m_iStructSize) != sizeof(PostQueueResponse))
	{
		if (iResponseLen < 0)
		{
			printf("No response received (timeout)\n");
		}
		else
		{
			printf("Invalid response received: either not nzbget-server or wrong server version\n");
		}
		return false;
	}

	char* pBuf = NULL;
	if (ntohl(PostQueueResponse.m_iTrailingDataLength) > 0)
	{
		pBuf = (char*)malloc(ntohl(PostQueueResponse.m_iTrailingDataLength));
		if (!m_pConnection->RecvAll(pBuf, ntohl(PostQueueResponse.m_iTrailingDataLength)))
		{
			free(pBuf);
			return false;
		}
	}

	m_pConnection->Disconnect();

	if (ntohl(PostQueueResponse.m_iTrailingDataLength) == 0)
	{
		printf("Server has no jobs queued for post-processing\n");
	}
	else
	{
		printf("Post-Processing List\n");
		printf("-----------------------------------\n");

		char* pBufPtr = (char*)pBuf;
		for (unsigned int i = 0; i < ntohl(PostQueueResponse.m_iNrTrailingEntries); i++)
		{
			SNZBPostQueueResponseEntry* pPostQueueAnswer = (SNZBPostQueueResponseEntry*) pBufPtr;

			int iStageProgress = ntohl(pPostQueueAnswer->m_iStageProgress);

			static const int EXECUTING_SCRIPT = 5;
			char szCompleted[100];
			szCompleted[0] = '\0';
			if (iStageProgress > 0 && (int)ntohl(pPostQueueAnswer->m_iStage) != EXECUTING_SCRIPT)
			{
				sprintf(szCompleted, ", %i%s", (int)(iStageProgress / 10), "%");
			}

			const char* szPostStageName[] = { "", ", Loading Pars", ", Verifying source files", ", Repairing", ", Verifying repaired files", ", Executing postprocess-script", "" };
			char* szInfoName = pBufPtr + sizeof(SNZBPostQueueResponseEntry) + ntohl(pPostQueueAnswer->m_iNZBFilenameLen) + ntohl(pPostQueueAnswer->m_iParFilename);
			
			printf("[%i] %s%s%s\n", ntohl(pPostQueueAnswer->m_iID), szInfoName, szPostStageName[ntohl(pPostQueueAnswer->m_iStage)], szCompleted);

			pBufPtr += sizeof(SNZBPostQueueResponseEntry) + ntohl(pPostQueueAnswer->m_iNZBFilenameLen) + 
				ntohl(pPostQueueAnswer->m_iParFilename) + ntohl(pPostQueueAnswer->m_iInfoNameLen) +
				ntohl(pPostQueueAnswer->m_iDestDirLen) + ntohl(pPostQueueAnswer->m_iProgressLabelLen);
		}

		free(pBuf);

		printf("-----------------------------------\n");
	}

	return true;
}

bool RemoteClient::RequestWriteLog(int iKind, const char* szText)
{
	if (!InitConnection()) return false;

	SNZBWriteLogRequest WriteLogRequest;
	InitMessageBase(&WriteLogRequest.m_MessageBase, eRemoteRequestWriteLog, sizeof(WriteLogRequest));
	WriteLogRequest.m_iKind = htonl(iKind);
	int iLength = strlen(szText) + 1;
	WriteLogRequest.m_iTrailingDataLength = htonl(iLength);

	if (m_pConnection->Send((char*)(&WriteLogRequest), sizeof(WriteLogRequest)) < 0)
	{
		perror("m_pConnection->Send");
		return false;
	}

	m_pConnection->Send(szText, iLength);
	bool OK = ReceiveBoolResponse();
	m_pConnection->Disconnect();
	return OK;
}

bool RemoteClient::RequestScan()
{
	if (!InitConnection()) return false;

	SNZBScanRequest ScanRequest;
	InitMessageBase(&ScanRequest.m_MessageBase, eRemoteRequestScan, sizeof(ScanRequest));

	bool OK = m_pConnection->Send((char*)(&ScanRequest), sizeof(ScanRequest)) >= 0;
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

bool RemoteClient::RequestHistory()
{
	if (!InitConnection()) return false;

	SNZBHistoryRequest HistoryRequest;
	InitMessageBase(&HistoryRequest.m_MessageBase, eRemoteRequestHistory, sizeof(HistoryRequest));

	if (m_pConnection->Send((char*)(&HistoryRequest), sizeof(HistoryRequest)) < 0)
	{
		perror("m_pConnection->Send");
		return false;
	}

	printf("Request sent\n");

	// Now listen for the returned list
	SNZBHistoryResponse HistoryResponse;
	int iResponseLen = m_pConnection->Recv((char*) &HistoryResponse, sizeof(HistoryResponse));
	if (iResponseLen != sizeof(HistoryResponse) || 
		(int)ntohl(HistoryResponse.m_MessageBase.m_iSignature) != (int)NZBMESSAGE_SIGNATURE ||
		ntohl(HistoryResponse.m_MessageBase.m_iStructSize) != sizeof(HistoryResponse))
	{
		if (iResponseLen < 0)
		{
			printf("No response received (timeout)\n");
		}
		else
		{
			printf("Invalid response received: either not nzbget-server or wrong server version\n");
		}
		return false;
	}

	char* pBuf = NULL;
	if (ntohl(HistoryResponse.m_iTrailingDataLength) > 0)
	{
		pBuf = (char*)malloc(ntohl(HistoryResponse.m_iTrailingDataLength));
		if (!m_pConnection->RecvAll(pBuf, ntohl(HistoryResponse.m_iTrailingDataLength)))
		{
			free(pBuf);
			return false;
		}
	}

	m_pConnection->Disconnect();

	if (ntohl(HistoryResponse.m_iTrailingDataLength) == 0)
	{
		printf("Server has no files in history\n");
	}
	else
	{
		printf("History (most recent first)\n");
		printf("-----------------------------------\n");

		char* pBufPtr = (char*)pBuf;
		for (unsigned int i = 0; i < ntohl(HistoryResponse.m_iNrTrailingEntries); i++)
		{
			SNZBHistoryResponseEntry* pListAnswer = (SNZBHistoryResponseEntry*) pBufPtr;

			HistoryInfo::EKind eKind = (HistoryInfo::EKind)ntohl(pListAnswer->m_iKind);
			const char* szNicename = pBufPtr + sizeof(SNZBHistoryResponseEntry);

			if (eKind == HistoryInfo::hkNZBInfo)
			{
				long long lSize = Util::JoinInt64(ntohl(pListAnswer->m_iSizeHi), ntohl(pListAnswer->m_iSizeLo));

				char szSize[20];
				Util::FormatFileSize(szSize, sizeof(szSize), lSize);

				const char* szParStatusText[] = { "", ", Par failed", ", Par possible", ", Par successful" };
				const char* szScriptStatusText[] = { "", ", Script status unknown", ", Script failed", ", Script successful" };

				printf("[%i] %s (%i files, %s%s%s)\n", ntohl(pListAnswer->m_iID), szNicename, 
					ntohl(pListAnswer->m_iFileCount), szSize, 
					szParStatusText[ntohl(pListAnswer->m_iParStatus)], 
					szScriptStatusText[ntohl(pListAnswer->m_iScriptStatus)]);
			}
			else if (eKind == HistoryInfo::hkUrlInfo)
			{
				const char* szUrlStatusText[] = { "", "", "Url download successful", "Url download failed", "" };

				printf("[%i] %s (%s)\n", ntohl(pListAnswer->m_iID), szNicename, 
					szUrlStatusText[ntohl(pListAnswer->m_iUrlStatus)]);
			}

			pBufPtr += sizeof(SNZBHistoryResponseEntry) + ntohl(pListAnswer->m_iNicenameLen);
		}

		printf("-----------------------------------\n");
		printf("Items: %i\n", ntohl(HistoryResponse.m_iNrTrailingEntries));
	}

	free(pBuf);

	return true;
}

bool RemoteClient::RequestServerDownloadUrl(const char* szURL, const char* szCategory, bool bAddFirst)
{
	if (!InitConnection()) return false;

	SNZBDownloadUrlRequest DownloadUrlRequest;
	InitMessageBase(&DownloadUrlRequest.m_MessageBase, eRemoteRequestDownloadUrl, sizeof(DownloadUrlRequest));
	DownloadUrlRequest.m_bAddFirst = htonl(bAddFirst);

	strncpy(DownloadUrlRequest.m_szURL, szURL, NZBREQUESTFILENAMESIZE - 1);
	DownloadUrlRequest.m_szURL[NZBREQUESTFILENAMESIZE-1] = '\0';
	DownloadUrlRequest.m_szCategory[0] = '\0';
	if (szCategory)
	{
		strncpy(DownloadUrlRequest.m_szCategory, szCategory, NZBREQUESTFILENAMESIZE - 1);
	}
	DownloadUrlRequest.m_szCategory[NZBREQUESTFILENAMESIZE-1] = '\0';

	bool OK = m_pConnection->Send((char*)(&DownloadUrlRequest), sizeof(DownloadUrlRequest)) >= 0;
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

bool RemoteClient::RequestUrlQueue()
{
	if (!InitConnection()) return false;

	SNZBUrlQueueRequest UrlQueueRequest;
	InitMessageBase(&UrlQueueRequest.m_MessageBase, eRemoteRequestUrlQueue, sizeof(UrlQueueRequest));

	if (m_pConnection->Send((char*)(&UrlQueueRequest), sizeof(UrlQueueRequest)) < 0)
	{
		perror("m_pConnection->Send");
		return false;
	}

	printf("Request sent\n");

	// Now listen for the returned list
	SNZBUrlQueueResponse UrlQueueResponse;
	int iResponseLen = m_pConnection->Recv((char*) &UrlQueueResponse, sizeof(UrlQueueResponse));
	if (iResponseLen != sizeof(UrlQueueResponse) || 
		(int)ntohl(UrlQueueResponse.m_MessageBase.m_iSignature) != (int)NZBMESSAGE_SIGNATURE ||
		ntohl(UrlQueueResponse.m_MessageBase.m_iStructSize) != sizeof(UrlQueueResponse))
	{
		if (iResponseLen < 0)
		{
			printf("No response received (timeout)\n");
		}
		else
		{
			printf("Invalid response received: either not nzbget-server or wrong server version\n");
		}
		return false;
	}

	char* pBuf = NULL;
	if (ntohl(UrlQueueResponse.m_iTrailingDataLength) > 0)
	{
		pBuf = (char*)malloc(ntohl(UrlQueueResponse.m_iTrailingDataLength));
		if (!m_pConnection->RecvAll(pBuf, ntohl(UrlQueueResponse.m_iTrailingDataLength)))
		{
			free(pBuf);
			return false;
		}
	}

	m_pConnection->Disconnect();

	if (ntohl(UrlQueueResponse.m_iTrailingDataLength) == 0)
	{
		printf("Server has no urls queued for download\n");
	}
	else
	{
		printf("Url-Queue\n");
		printf("-----------------------------------\n");

		char* pBufPtr = (char*)pBuf;
		for (unsigned int i = 0; i < ntohl(UrlQueueResponse.m_iNrTrailingEntries); i++)
		{
			SNZBUrlQueueResponseEntry* pUrlQueueAnswer = (SNZBUrlQueueResponseEntry*) pBufPtr;

			const char* szURL = pBufPtr + sizeof(SNZBUrlQueueResponseEntry);
			const char* szTitle = pBufPtr + sizeof(SNZBUrlQueueResponseEntry) + ntohl(pUrlQueueAnswer->m_iURLLen);

			char szNiceName[1024];
			UrlInfo::MakeNiceName(szURL, szTitle, szNiceName, 1024);

			printf("[%i] %s\n", ntohl(pUrlQueueAnswer->m_iID), szNiceName);

			pBufPtr += sizeof(SNZBUrlQueueResponseEntry) + ntohl(pUrlQueueAnswer->m_iURLLen) + 
				ntohl(pUrlQueueAnswer->m_iNZBFilenameLen);
		}

		free(pBuf);

		printf("-----------------------------------\n");
	}

	return true;
}
