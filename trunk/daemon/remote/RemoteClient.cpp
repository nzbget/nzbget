/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2005 Bo Cordes Petersen <placebodk@sourceforge.net>
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
	delete m_pConnection;
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
	m_pConnection = new Connection(g_pOptions->GetControlIP(), g_pOptions->GetControlPort(), false);

	bool OK = m_pConnection->Connect();
	if (!OK)
	{
		printf("Unable to send request to nzbget-server at %s (port %i)\n", g_pOptions->GetControlIP(), g_pOptions->GetControlPort());
	}
	return OK;
}

void RemoteClient::InitMessageBase(SNZBRequestBase* pMessageBase, int iRequest, int iSize)
{
	pMessageBase->m_iSignature	= htonl(NZBMESSAGE_SIGNATURE);
	pMessageBase->m_iType = htonl(iRequest);
	pMessageBase->m_iStructSize = htonl(iSize);

	strncpy(pMessageBase->m_szUsername, g_pOptions->GetControlUsername(), NZBREQUESTPASSWORDSIZE - 1);
	pMessageBase->m_szUsername[NZBREQUESTPASSWORDSIZE - 1] = '\0';

	strncpy(pMessageBase->m_szPassword, g_pOptions->GetControlPassword(), NZBREQUESTPASSWORDSIZE - 1);
	pMessageBase->m_szPassword[NZBREQUESTPASSWORDSIZE - 1] = '\0';
}

bool RemoteClient::ReceiveBoolResponse()
{
	printf("Request sent\n");

	// all bool-responses have the same format of structure, we use SNZBDownloadResponse here
	SNZBDownloadResponse BoolResponse;
	memset(&BoolResponse, 0, sizeof(BoolResponse));

	bool bRead = m_pConnection->Recv((char*)&BoolResponse, sizeof(BoolResponse));
	if (!bRead || 
		(int)ntohl(BoolResponse.m_MessageBase.m_iSignature) != (int)NZBMESSAGE_SIGNATURE ||
		ntohl(BoolResponse.m_MessageBase.m_iStructSize) != sizeof(BoolResponse))
	{
		printf("No response or invalid response (timeout, not nzbget-server or wrong nzbget-server version)\n");
		return false;
	}

	int iTextLen = ntohl(BoolResponse.m_iTrailingDataLength);
	char* buf = (char*)malloc(iTextLen);
	bRead = m_pConnection->Recv(buf, iTextLen);
	if (!bRead)
	{
		printf("No response or invalid response (timeout, not nzbget-server or wrong nzbget-server version)\n");
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
bool RemoteClient::RequestServerDownload(const char* szNZBFilename, const char* szNZBContent,
	const char* szCategory, bool bAddFirst, bool bAddPaused, int iPriority,
	const char* szDupeKey, int iDupeMode, int iDupeScore)
{
	// Read the file into the buffer
	char* szBuffer = NULL;
	int iLength = 0;
	bool bIsUrl = !strncasecmp(szNZBContent, "http://", 6) || !strncasecmp(szNZBContent, "https://", 7);
	if (bIsUrl)
	{
		iLength = strlen(szNZBContent) + 1;
	}
	else
	{
		if (!Util::LoadFileIntoBuffer(szNZBContent, &szBuffer, &iLength))
		{
			printf("Could not load file %s\n", szNZBContent);
			return false;
		}
		iLength--;
	}

	bool OK = InitConnection();
	if (OK)
	{
		SNZBDownloadRequest DownloadRequest;
		InitMessageBase(&DownloadRequest.m_MessageBase, eRemoteRequestDownload, sizeof(DownloadRequest));
		DownloadRequest.m_bAddFirst = htonl(bAddFirst);
		DownloadRequest.m_bAddPaused = htonl(bAddPaused);
		DownloadRequest.m_iPriority = htonl(iPriority);
		DownloadRequest.m_iDupeMode = htonl(iDupeMode);
		DownloadRequest.m_iDupeScore = htonl(iDupeScore);
		DownloadRequest.m_iTrailingDataLength = htonl(iLength);

		DownloadRequest.m_szNZBFilename[0] = '\0';
		if (!Util::EmptyStr(szNZBFilename))
		{
			strncpy(DownloadRequest.m_szNZBFilename, szNZBFilename, NZBREQUESTFILENAMESIZE - 1);
		}
		else if (!bIsUrl)
		{
			strncpy(DownloadRequest.m_szNZBFilename, szNZBContent, NZBREQUESTFILENAMESIZE - 1);
		}
		DownloadRequest.m_szNZBFilename[NZBREQUESTFILENAMESIZE-1] = '\0';

		DownloadRequest.m_szCategory[0] = '\0';
		if (szCategory)
		{
			strncpy(DownloadRequest.m_szCategory, szCategory, NZBREQUESTFILENAMESIZE - 1);
		}
		DownloadRequest.m_szCategory[NZBREQUESTFILENAMESIZE-1] = '\0';

		DownloadRequest.m_szDupeKey[0] = '\0';
		if (!Util::EmptyStr(szDupeKey))
		{
			strncpy(DownloadRequest.m_szDupeKey, szDupeKey, NZBREQUESTFILENAMESIZE - 1);
		}
		DownloadRequest.m_szDupeKey[NZBREQUESTFILENAMESIZE-1] = '\0';

		if (!m_pConnection->Send((char*)(&DownloadRequest), sizeof(DownloadRequest)))
		{
			perror("m_pConnection->Send");
			OK = false;
		}
		else
		{
			m_pConnection->Send(bIsUrl ? szNZBContent : szBuffer, iLength);
			OK = ReceiveBoolResponse();
			m_pConnection->Disconnect();
		}
	}

	// Cleanup
	free(szBuffer);

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
			pNZBInfo->SetID(ntohl(pListAnswer->m_iID));
			pNZBInfo->SetKind((NZBInfo::EKind)ntohl(pListAnswer->m_iKind));
			pNZBInfo->SetSize(Util::JoinInt64(ntohl(pListAnswer->m_iSizeHi), ntohl(pListAnswer->m_iSizeLo)));
			pNZBInfo->SetRemainingSize(Util::JoinInt64(ntohl(pListAnswer->m_iRemainingSizeHi), ntohl(pListAnswer->m_iRemainingSizeLo)));
			pNZBInfo->SetPausedSize(Util::JoinInt64(ntohl(pListAnswer->m_iPausedSizeHi), ntohl(pListAnswer->m_iPausedSizeLo)));
			pNZBInfo->SetPausedFileCount(ntohl(pListAnswer->m_iPausedCount));
			pNZBInfo->SetRemainingParCount(ntohl(pListAnswer->m_iRemainingParCount));
			pNZBInfo->SetFilename(szFileName);
			pNZBInfo->SetName(szName);
			pNZBInfo->SetDestDir(szDestDir);
			pNZBInfo->SetCategory(szCategory);
			pNZBInfo->SetQueuedFilename(m_szQueuedFilename);
			pNZBInfo->SetPriority(ntohl(pListAnswer->m_iPriority));
			pNZBInfo->m_bMatch = ntohl(pListAnswer->m_bMatch);

			pDownloadQueue->GetQueue()->push_back(pNZBInfo);

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

			NZBInfo* pNZBInfo = pDownloadQueue->GetQueue()->at(ntohl(pListAnswer->m_iNZBIndex) - 1);
			pNZBInfo->GetParameters()->SetParameter(szName, szValue);

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
			pFileInfo->m_bMatch = ntohl(pListAnswer->m_bMatch);

			NZBInfo* pNZBInfo = pDownloadQueue->GetQueue()->at(ntohl(pListAnswer->m_iNZBIndex) - 1);
			pFileInfo->SetNZBInfo(pNZBInfo);
			pNZBInfo->GetFileList()->push_back(pFileInfo);

			pBufPtr += sizeof(SNZBListResponseFileEntry) + ntohl(pListAnswer->m_iSubjectLen) + 
				ntohl(pListAnswer->m_iFilenameLen);
		}
	}
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

	if (!m_pConnection->Send((char*)(&ListRequest), sizeof(ListRequest)))
	{
		perror("m_pConnection->Send");
		return false;
	}

	printf("Request sent\n");

	// Now listen for the returned list
	SNZBListResponse ListResponse;
	bool bRead = m_pConnection->Recv((char*) &ListResponse, sizeof(ListResponse));
	if (!bRead || 
		(int)ntohl(ListResponse.m_MessageBase.m_iSignature) != (int)NZBMESSAGE_SIGNATURE ||
		ntohl(ListResponse.m_MessageBase.m_iStructSize) != sizeof(ListResponse))
	{
		printf("No response or invalid response (timeout, not nzbget-server or wrong nzbget-server version)\n");
		return false;
	}

	char* pBuf = NULL;
	if (ntohl(ListResponse.m_iTrailingDataLength) > 0)
	{
		pBuf = (char*)malloc(ntohl(ListResponse.m_iTrailingDataLength));
		if (!m_pConnection->Recv(pBuf, ntohl(ListResponse.m_iTrailingDataLength)))
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

			DownloadQueue* pDownloadQueue = DownloadQueue::Lock();
			BuildFileList(&ListResponse, pBuf, pDownloadQueue);

			long long lRemaining = 0;
			long long lPaused = 0;
			int iMatches = 0;
			int iNrFileEntries = 0;

			for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
			{
				NZBInfo* pNZBInfo = *it;
				for (FileList::iterator it2 = pNZBInfo->GetFileList()->begin(); it2 != pNZBInfo->GetFileList()->end(); it2++)
				{
					FileInfo* pFileInfo = *it2;

					iNrFileEntries++;

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
						printf("[%i] %s/%s (%.2f MB%s%s)%s\n", pFileInfo->GetID(), pFileInfo->GetNZBInfo()->GetName(),
							pFileInfo->GetFilename(),
							(float)(Util::Int64ToFloat(pFileInfo->GetSize()) / 1024.0 / 1024.0), 
							szCompleted, szThreads, szStatus);
						iMatches++;
					}
				}
			}

			DownloadQueue::Unlock();

			if (iMatches == 0)
			{
				printf("No matches founds\n");
			}

			printf("-----------------------------------\n");
			printf("Files: %i\n", iNrFileEntries);
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

			DownloadQueue* pDownloadQueue = DownloadQueue::Lock();
			BuildFileList(&ListResponse, pBuf, pDownloadQueue);

			long long lRemaining = 0;
			long long lPaused = 0;
			int iMatches = 0;
			int iNrFileEntries = 0;

			for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
			{
				NZBInfo* pNZBInfo = *it;

				iNrFileEntries += pNZBInfo->GetFileList()->size();

				long long lUnpausedRemainingSize = pNZBInfo->GetRemainingSize() - pNZBInfo->GetPausedSize();
				lRemaining += lUnpausedRemainingSize;

				char szRemaining[20];
				Util::FormatFileSize(szRemaining, sizeof(szRemaining), lUnpausedRemainingSize);

				char szPriority[100];
				szPriority[0] = '\0';
				if (pNZBInfo->GetPriority() != 0)
				{
					sprintf(szPriority, "[%+i] ", pNZBInfo->GetPriority());
				}

				char szPaused[20];
				szPaused[0] = '\0';
				if (pNZBInfo->GetPausedSize() > 0)
				{
					char szPausedSize[20];
					Util::FormatFileSize(szPausedSize, sizeof(szPausedSize), pNZBInfo->GetPausedSize());
					sprintf(szPaused, " + %s paused", szPausedSize);
					lPaused += pNZBInfo->GetPausedSize();
				}

				char szCategory[1024];
				szCategory[0] = '\0';
				if (pNZBInfo->GetCategory() && strlen(pNZBInfo->GetCategory()) > 0)
				{
					sprintf(szCategory, " (%s)", pNZBInfo->GetCategory());
				}

				char szThreads[100];
				szThreads[0] = '\0';
				if (pNZBInfo->GetActiveDownloads() > 0)
				{
					sprintf(szThreads, ", %i thread%s", pNZBInfo->GetActiveDownloads(), (pNZBInfo->GetActiveDownloads() > 1 ? "s" : ""));
				}

				char szParameters[1024];
				szParameters[0] = '\0';
				for (NZBParameterList::iterator it = pNZBInfo->GetParameters()->begin(); it != pNZBInfo->GetParameters()->end(); it++)
				{
					if (szParameters[0] == '\0')
					{
						strncat(szParameters, " (", sizeof(szParameters) - strlen(szParameters) - 1);
					}
					else
					{
						strncat(szParameters, ", ", sizeof(szParameters) - strlen(szParameters) - 1);
					}
					NZBParameter* pNZBParameter = *it;
					strncat(szParameters, pNZBParameter->GetName(), sizeof(szParameters) - strlen(szParameters) - 1);
					strncat(szParameters, "=", sizeof(szParameters) - strlen(szParameters) - 1);
					strncat(szParameters, pNZBParameter->GetValue(), sizeof(szParameters) - strlen(szParameters) - 1);
				}
				if (szParameters[0] != '\0')
				{
					strncat(szParameters, ")", sizeof(szParameters) - strlen(szParameters) - 1);
				}

				char szUrlOrFile[100];
				if (pNZBInfo->GetKind() == NZBInfo::nkUrl)
				{
					strncpy(szUrlOrFile, "URL", sizeof(szUrlOrFile));
				}
				else
				{
					snprintf(szUrlOrFile, sizeof(szUrlOrFile), "%i file%s", (int)pNZBInfo->GetFileList()->size(),
						pNZBInfo->GetFileList()->size() > 1 ? "s" : "");
					szUrlOrFile[100-1] = '\0';
				}

				if (!szPattern || ((MatchedNZBInfo*)pNZBInfo)->m_bMatch)
				{
					printf("[%i] %s%s (%s, %s%s%s)%s%s\n", pNZBInfo->GetID(), szPriority, 
						pNZBInfo->GetName(), szUrlOrFile, szRemaining, 
						szPaused, szThreads, szCategory, szParameters);
					iMatches++;
				}
			}

			if (iMatches == 0)
			{
				printf("No matches founds\n");
			}

			printf("-----------------------------------\n");
			printf("Groups: %i\n", pDownloadQueue->GetQueue()->size());
			if (szPattern)
			{
				printf("Matches: %i\n", iMatches);
			}
			printf("Files: %i\n", iNrFileEntries);
			if (lPaused > 0)
			{
				printf("Remaining size: %.2f MB (+%.2f MB paused)\n", (float)(Util::Int64ToFloat(lRemaining) / 1024.0 / 1024.0), 
					(float)(Util::Int64ToFloat(lPaused) / 1024.0 / 1024.0));
			}
			else
			{
				printf("Remaining size: %.2f MB\n", (float)(Util::Int64ToFloat(lRemaining) / 1024.0 / 1024.0));
			}

			DownloadQueue::Unlock();
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
		strncat(szServerState, strlen(szServerState) > 0 ? ", Post-Processing" : "Post-Processing", sizeof(szServerState) - strlen(szServerState) - 1);
		if (ntohl(ListResponse.m_bPostPaused))
		{
			strncat(szServerState, " paused", sizeof(szServerState) - strlen(szServerState) - 1);
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

	if (!m_pConnection->Send((char*)(&LogRequest), sizeof(LogRequest)))
	{
		perror("m_pConnection->Send");
		return false;
	}

	printf("Request sent\n");

	// Now listen for the returned log
	SNZBLogResponse LogResponse;
	bool bRead = m_pConnection->Recv((char*) &LogResponse, sizeof(LogResponse));
	if (!bRead || 
		(int)ntohl(LogResponse.m_MessageBase.m_iSignature) != (int)NZBMESSAGE_SIGNATURE ||
		ntohl(LogResponse.m_MessageBase.m_iStructSize) != sizeof(LogResponse))
	{
		printf("No response or invalid response (timeout, not nzbget-server or wrong nzbget-server version)\n");
		return false;
	}

	char* pBuf = NULL;
	if (ntohl(LogResponse.m_iTrailingDataLength) > 0)
	{
		pBuf = (char*)malloc(ntohl(LogResponse.m_iTrailingDataLength));
		if (!m_pConnection->Recv(pBuf, ntohl(LogResponse.m_iTrailingDataLength)))
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

	if (!m_pConnection->Send((char*)(&PauseUnpauseRequest), sizeof(PauseUnpauseRequest)))
	{
		perror("m_pConnection->Send");
		m_pConnection->Disconnect();
		return false;
	}

	bool OK = ReceiveBoolResponse();
	m_pConnection->Disconnect();

	return OK;
}

bool RemoteClient::RequestServerSetDownloadRate(int iRate)
{
	if (!InitConnection()) return false;

	SNZBSetDownloadRateRequest SetDownloadRateRequest;
	InitMessageBase(&SetDownloadRateRequest.m_MessageBase, eRemoteRequestSetDownloadRate, sizeof(SetDownloadRateRequest));
	SetDownloadRateRequest.m_iDownloadRate = htonl(iRate);

	if (!m_pConnection->Send((char*)(&SetDownloadRateRequest), sizeof(SetDownloadRateRequest)))
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

	if (!m_pConnection->Send((char*)(&DumpDebugInfo), sizeof(DumpDebugInfo)))
	{
		perror("m_pConnection->Send");
		m_pConnection->Disconnect();
		return false;
	}

	bool OK = ReceiveBoolResponse();
	m_pConnection->Disconnect();

	return OK;
}

bool RemoteClient::RequestServerEditQueue(DownloadQueue::EEditAction eAction, int iOffset, const char* szText,
	int* pIDList, int iIDCount, NameList* pNameList, eRemoteMatchMode iMatchMode)
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
	EditQueueRequest.m_iAction = htonl(eAction);
	EditQueueRequest.m_iMatchMode = htonl(iMatchMode);
	EditQueueRequest.m_iOffset = htonl((int)iOffset);
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
	if (!m_pConnection->Send((char*)(&EditQueueRequest), sizeof(EditQueueRequest)))
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

	bool OK = m_pConnection->Send((char*)(&ShutdownRequest), sizeof(ShutdownRequest));
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

bool RemoteClient::RequestServerReload()
{
	if (!InitConnection()) return false;

	SNZBReloadRequest ReloadRequest;
	InitMessageBase(&ReloadRequest.m_MessageBase, eRemoteRequestReload, sizeof(ReloadRequest));

	bool OK = m_pConnection->Send((char*)(&ReloadRequest), sizeof(ReloadRequest));
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

	bool OK = m_pConnection->Send((char*)(&VersionRequest), sizeof(VersionRequest));
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

	if (!m_pConnection->Send((char*)(&PostQueueRequest), sizeof(PostQueueRequest)))
	{
		perror("m_pConnection->Send");
		return false;
	}

	printf("Request sent\n");

	// Now listen for the returned list
	SNZBPostQueueResponse PostQueueResponse;
	bool bRead = m_pConnection->Recv((char*) &PostQueueResponse, sizeof(PostQueueResponse));
	if (!bRead || 
		(int)ntohl(PostQueueResponse.m_MessageBase.m_iSignature) != (int)NZBMESSAGE_SIGNATURE ||
		ntohl(PostQueueResponse.m_MessageBase.m_iStructSize) != sizeof(PostQueueResponse))
	{
		printf("No response or invalid response (timeout, not nzbget-server or wrong nzbget-server version)\n");
		return false;
	}

	char* pBuf = NULL;
	if (ntohl(PostQueueResponse.m_iTrailingDataLength) > 0)
	{
		pBuf = (char*)malloc(ntohl(PostQueueResponse.m_iTrailingDataLength));
		if (!m_pConnection->Recv(pBuf, ntohl(PostQueueResponse.m_iTrailingDataLength)))
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

			char szCompleted[100];
			szCompleted[0] = '\0';
			if (iStageProgress > 0 && (int)ntohl(pPostQueueAnswer->m_iStage) != (int)PostInfo::ptExecutingScript)
			{
				sprintf(szCompleted, ", %i%s", (int)(iStageProgress / 10), "%");
			}

			const char* szPostStageName[] = { "", ", Loading Pars", ", Verifying source files", ", Repairing", ", Verifying repaired files", ", Unpacking", ", Executing postprocess-script", "" };
			char* szInfoName = pBufPtr + sizeof(SNZBPostQueueResponseEntry) + ntohl(pPostQueueAnswer->m_iNZBFilenameLen);
			
			printf("[%i] %s%s%s\n", ntohl(pPostQueueAnswer->m_iID), szInfoName, szPostStageName[ntohl(pPostQueueAnswer->m_iStage)], szCompleted);

			pBufPtr += sizeof(SNZBPostQueueResponseEntry) + ntohl(pPostQueueAnswer->m_iNZBFilenameLen) + 
				ntohl(pPostQueueAnswer->m_iInfoNameLen) + ntohl(pPostQueueAnswer->m_iDestDirLen) +
				ntohl(pPostQueueAnswer->m_iProgressLabelLen);
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

	if (!m_pConnection->Send((char*)(&WriteLogRequest), sizeof(WriteLogRequest)))
	{
		perror("m_pConnection->Send");
		return false;
	}

	m_pConnection->Send(szText, iLength);
	bool OK = ReceiveBoolResponse();
	m_pConnection->Disconnect();
	return OK;
}

bool RemoteClient::RequestScan(bool bSyncMode)
{
	if (!InitConnection()) return false;

	SNZBScanRequest ScanRequest;
	InitMessageBase(&ScanRequest.m_MessageBase, eRemoteRequestScan, sizeof(ScanRequest));

	ScanRequest.m_bSyncMode = htonl(bSyncMode);

	bool OK = m_pConnection->Send((char*)(&ScanRequest), sizeof(ScanRequest));
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

bool RemoteClient::RequestHistory(bool bWithHidden)
{
	if (!InitConnection()) return false;

	SNZBHistoryRequest HistoryRequest;
	InitMessageBase(&HistoryRequest.m_MessageBase, eRemoteRequestHistory, sizeof(HistoryRequest));
	HistoryRequest.m_bHidden = htonl(bWithHidden);

	if (!m_pConnection->Send((char*)(&HistoryRequest), sizeof(HistoryRequest)))
	{
		perror("m_pConnection->Send");
		return false;
	}

	printf("Request sent\n");

	// Now listen for the returned list
	SNZBHistoryResponse HistoryResponse;
	bool bRead = m_pConnection->Recv((char*) &HistoryResponse, sizeof(HistoryResponse));
	if (!bRead || 
		(int)ntohl(HistoryResponse.m_MessageBase.m_iSignature) != (int)NZBMESSAGE_SIGNATURE ||
		ntohl(HistoryResponse.m_MessageBase.m_iStructSize) != sizeof(HistoryResponse))
	{
		printf("No response or invalid response (timeout, not nzbget-server or wrong nzbget-server version)\n");
		return false;
	}

	char* pBuf = NULL;
	if (ntohl(HistoryResponse.m_iTrailingDataLength) > 0)
	{
		pBuf = (char*)malloc(ntohl(HistoryResponse.m_iTrailingDataLength));
		if (!m_pConnection->Recv(pBuf, ntohl(HistoryResponse.m_iTrailingDataLength)))
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

			if (eKind == HistoryInfo::hkNzb || eKind == HistoryInfo::hkDup)
			{
				char szFiles[20];
				snprintf(szFiles, sizeof(szFiles), "%i files, ", ntohl(pListAnswer->m_iFileCount));
				szFiles[20 - 1] = '\0';

				long long lSize = Util::JoinInt64(ntohl(pListAnswer->m_iSizeHi), ntohl(pListAnswer->m_iSizeLo));

				char szSize[20];
				Util::FormatFileSize(szSize, sizeof(szSize), lSize);

				const char* szParStatusText[] = { "", "", ", Par failed", ", Par successful", ", Repair possible", ", Repair needed" };
				const char* szScriptStatusText[] = { "", ", Script status unknown", ", Script failed", ", Script successful" };

				printf("[%i] %s (%s%s%s%s%s)\n", ntohl(pListAnswer->m_iID), szNicename, 
					(eKind == HistoryInfo::hkDup ? "Hidden, " : ""),
					(eKind == HistoryInfo::hkDup ? "" : szFiles), szSize, 
					szParStatusText[ntohl(pListAnswer->m_iParStatus)], 
					szScriptStatusText[ntohl(pListAnswer->m_iScriptStatus)]);
			}
			else if (eKind == HistoryInfo::hkUrl)
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
