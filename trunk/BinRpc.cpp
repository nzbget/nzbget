/*
 *  This file is part of nzbget
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
#include <cstdio>
#include <fstream>
#ifndef WIN32
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include "nzbget.h"
#include "BinRpc.h"
#include "Log.h"
#include "Options.h"
#include "QueueCoordinator.h"
#include "QueueEditor.h"
#include "PrePostProcessor.h"
#include "Util.h"

extern Options* g_pOptions;
extern QueueCoordinator* g_pQueueCoordinator;
extern PrePostProcessor* g_pPrePostProcessor;
extern void ExitProc();

const char* g_szMessageRequestNames[] =
    { "N/A", "Download", "Pause/Unpause", "List", "Set download rate", "Dump debug", 
		"Edit queue", "Log", "Quit", "Version", "Post-queue" };

const unsigned int g_iMessageRequestSizes[] =
    { 0,
		sizeof(SNZBDownloadRequest),
		sizeof(SNZBPauseUnpauseRequest),
		sizeof(SNZBListRequest),
		sizeof(SNZBSetDownloadRateRequest),
		sizeof(SNZBDumpDebugRequest),
		sizeof(SNZBEditQueueRequest),
		sizeof(SNZBLogRequest),
		sizeof(SNZBShutdownRequest),
		sizeof(SNZBVersionRequest),
		sizeof(SNZBPostQueueRequest),
    };

//*****************************************************************
// BinProcessor

void BinRpcProcessor::Execute()
{
	// Read the first package which needs to be a request
	int iBytesReceived = recv(m_iSocket, ((char*)&m_MessageBase) + sizeof(m_MessageBase.m_iSignature), sizeof(m_MessageBase) - sizeof(m_MessageBase.m_iSignature), 0);
	if (iBytesReceived < 0)
	{
		return;
	}

	// Make sure this is a nzbget request from a client
	if ((int)ntohl(m_MessageBase.m_iSignature) != (int)NZBMESSAGE_SIGNATURE)
	{
		warn("Non-nzbget request received on port %i from %s", g_pOptions->GetServerPort(), m_szClientIP);
		return;
	}

	if (strcmp(m_MessageBase.m_szPassword, g_pOptions->GetServerPassword()))
	{
		warn("nzbget request received on port %i from %s, but password invalid", g_pOptions->GetServerPort(), m_szClientIP);
		return;
	}

	debug("%s request received from %s", g_szMessageRequestNames[ntohl(m_MessageBase.m_iType)], m_szClientIP);

	Dispatch();
}

void BinRpcProcessor::Dispatch()
{
	if (ntohl(m_MessageBase.m_iType) >= (int)eRemoteRequestDownload &&
		   ntohl(m_MessageBase.m_iType) <= (int)eRemoteRequestPostQueue &&
		   g_iMessageRequestSizes[ntohl(m_MessageBase.m_iType)] != ntohl(m_MessageBase.m_iStructSize))
	{
		error("Invalid size of request: needed %i Bytes, but received %i Bytes",
			 g_iMessageRequestSizes[ntohl(m_MessageBase.m_iType)], ntohl(m_MessageBase.m_iStructSize));
		return;
	}
	
	BinCommand* command = NULL;

	switch (ntohl(m_MessageBase.m_iType))
	{
		case eRemoteRequestDownload:
			{
				command = new DownloadBinCommand();
				break;
			}

		case eRemoteRequestList:
			{
				command = new ListBinCommand();
				break;
			}

		case eRemoteRequestLog:
			{
				command = new LogBinCommand();
				break;
			}

		case eRemoteRequestPauseUnpause:
			{
				command = new PauseUnpauseBinCommand();
				break;
			}

		case eRemoteRequestEditQueue:
			{
				command = new EditQueueBinCommand();
				break;
			}

		case eRemoteRequestSetDownloadRate:
			{
				command = new SetDownloadRateBinCommand();
				break;
			}

		case eRemoteRequestDumpDebug:
			{
				command = new DumpDebugBinCommand();
				break;
			}

		case eRemoteRequestShutdown:
			{
				command = new ShutdownBinCommand();
				break;
			}

		case eRemoteRequestVersion:
			{
				command = new VersionBinCommand();
				break;
			}

		case eRemoteRequestPostQueue:
			{
				command = new PostQueueBinCommand();
				break;
			}

		case eRemoteRequestWriteLog:
			{
				command = new WriteLogBinCommand();
				break;
			}

		default:
			error("Received unsupported request %i", ntohl(m_MessageBase.m_iType));
			break;
	}

	if (command)
	{
		command->SetSocket(m_iSocket);
		command->SetMessageBase(&m_MessageBase);
		command->Execute();
		delete command;
	}
}

//*****************************************************************
// Commands

void BinCommand::SendBoolResponse(bool bSuccess, const char* szText)
{
	// all bool-responses have the same format of structure, we use SNZBDownloadResponse here
	SNZBDownloadResponse BoolResponse;
	memset(&BoolResponse, 0, sizeof(BoolResponse));
	BoolResponse.m_MessageBase.m_iSignature = htonl(NZBMESSAGE_SIGNATURE);
	BoolResponse.m_MessageBase.m_iStructSize = htonl(sizeof(BoolResponse));
	BoolResponse.m_bSuccess = htonl(bSuccess);
	int iTextLen = strlen(szText) + 1;
	BoolResponse.m_iTrailingDataLength = htonl(iTextLen);

	// Send the request answer
	send(m_iSocket, (char*) &BoolResponse, sizeof(BoolResponse), 0);
	send(m_iSocket, (char*)szText, iTextLen, 0);
}

bool BinCommand::ReceiveRequest(void* pBuffer, int iSize)
{
	memcpy(pBuffer, m_pMessageBase, sizeof(SNZBRequestBase));
	iSize -= sizeof(SNZBRequestBase);
	if (iSize > 0)
	{
		int iBytesReceived = recv(m_iSocket, ((char*)pBuffer) + sizeof(SNZBRequestBase), iSize, 0);
		if (iBytesReceived != iSize)
		{
			error("invalid request");
			return false;
		}
	}
	return true;
}

void PauseUnpauseBinCommand::Execute()
{
	SNZBPauseUnpauseRequest PauseUnpauseRequest;
	if (!ReceiveRequest(&PauseUnpauseRequest, sizeof(PauseUnpauseRequest)))
	{
		return;
	}

	g_pOptions->SetPause(ntohl(PauseUnpauseRequest.m_bPause));
	SendBoolResponse(true, "Pause-/Unpause-Command completed successfully");
}

void SetDownloadRateBinCommand::Execute()
{
	SNZBSetDownloadRateRequest SetDownloadRequest;
	if (!ReceiveRequest(&SetDownloadRequest, sizeof(SetDownloadRequest)))
	{
		return;
	}

	g_pOptions->SetDownloadRate(ntohl(SetDownloadRequest.m_iDownloadRate) / 1024.0f);
	SendBoolResponse(true, "Rate-Command completed successfully");
}

void DumpDebugBinCommand::Execute()
{
	SNZBDumpDebugRequest DumpDebugRequest;
	if (!ReceiveRequest(&DumpDebugRequest, sizeof(DumpDebugRequest)))
	{
		return;
	}

	g_pQueueCoordinator->LogDebugInfo();
	SendBoolResponse(true, "Debug-Command completed successfully");
}

void ShutdownBinCommand::Execute()
{
	SNZBShutdownRequest ShutdownRequest;
	if (!ReceiveRequest(&ShutdownRequest, sizeof(ShutdownRequest)))
	{
		return;
	}

	SendBoolResponse(true, "Stopping server");
	ExitProc();
}

void VersionBinCommand::Execute()
{
	SNZBVersionRequest VersionRequest;
	if (!ReceiveRequest(&VersionRequest, sizeof(VersionRequest)))
	{
		return;
	}

	SendBoolResponse(true, VERSION);
}

void DownloadBinCommand::Execute()
{
	SNZBDownloadRequest DownloadRequest;
	if (!ReceiveRequest(&DownloadRequest, sizeof(DownloadRequest)))
	{
		return;
	}

	char* pRecvBuffer = (char*)malloc(ntohl(DownloadRequest.m_iTrailingDataLength) + 1);
	char* pBufPtr = pRecvBuffer;

	// Read from the socket until nothing remains
	int iResult = 0;
	int NeedBytes = ntohl(DownloadRequest.m_iTrailingDataLength);
	while (NeedBytes > 0)
	{
		iResult = recv(m_iSocket, pBufPtr, NeedBytes, 0);
		// Did the recv succeed?
		if (iResult <= 0)
		{
			error("invalid request");
			break;
		}
		pBufPtr += iResult;
		NeedBytes -= iResult;
	}

	if (NeedBytes == 0)
	{
		NZBFile* pNZBFile = NZBFile::CreateFromBuffer(DownloadRequest.m_szFilename, DownloadRequest.m_szCategory, pRecvBuffer, ntohl(DownloadRequest.m_iTrailingDataLength));

		if (pNZBFile)
		{
			info("Request: Queue collection %s", DownloadRequest.m_szFilename);
			g_pQueueCoordinator->AddNZBFileToQueue(pNZBFile, ntohl(DownloadRequest.m_bAddFirst));
			delete pNZBFile;

			char tmp[1024];
			snprintf(tmp, 1024, "Collection %s added to queue", Util::BaseFileName(DownloadRequest.m_szFilename));
			tmp[1024-1] = '\0';
			SendBoolResponse(true, tmp);
		}
		else
		{
			char tmp[1024];
			snprintf(tmp, 1024, "Download Request failed for %s", Util::BaseFileName(DownloadRequest.m_szFilename));
			tmp[1024-1] = '\0';
			SendBoolResponse(false, tmp);
		}
	}

	free(pRecvBuffer);
}

void ListBinCommand::Execute()
{
	SNZBListRequest ListRequest;
	if (!ReceiveRequest(&ListRequest, sizeof(ListRequest)))
	{
		return;
	}

	SNZBListResponse ListResponse;
	memset(&ListResponse, 0, sizeof(ListResponse));
	ListResponse.m_MessageBase.m_iSignature = htonl(NZBMESSAGE_SIGNATURE);
	ListResponse.m_MessageBase.m_iStructSize = htonl(sizeof(ListResponse));
	ListResponse.m_iEntrySize = htonl(sizeof(SNZBListResponseEntry));

	char* buf = NULL;
	int bufsize = 0;

	if (ntohl(ListRequest.m_bFileList))
	{
		// Make a data structure and copy all the elements of the list into it
		DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

		int NrEntries = pDownloadQueue->size();

		// calculate required buffer size
		bufsize = NrEntries * sizeof(SNZBListResponseEntry);
		for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
		{
			FileInfo* pFileInfo = *it;
			bufsize += strlen(pFileInfo->GetNZBInfo()->GetFilename()) + 1;
			bufsize += strlen(pFileInfo->GetSubject()) + 1;
			bufsize += strlen(pFileInfo->GetFilename()) + 1;
			bufsize += strlen(pFileInfo->GetNZBInfo()->GetDestDir()) + 1;
			bufsize += strlen(pFileInfo->GetNZBInfo()->GetCategory()) + 1;
			// align struct to 4-bytes, needed by ARM-processor (and may be others)
			bufsize += bufsize % 4 > 0 ? 4 - bufsize % 4 : 0;
		}

		buf = (char*) malloc(bufsize);
		char* bufptr = buf;
		for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
		{
			unsigned long iSizeHi, iSizeLo;
			FileInfo* pFileInfo = *it;
			SNZBListResponseEntry* pListAnswer = (SNZBListResponseEntry*) bufptr;
			pListAnswer->m_iID				= htonl(pFileInfo->GetID());
			Util::SplitInt64(pFileInfo->GetSize(), &iSizeHi, &iSizeLo);
			pListAnswer->m_iFileSizeLo		= htonl(iSizeLo);
			pListAnswer->m_iFileSizeHi		= htonl(iSizeHi);
			Util::SplitInt64(pFileInfo->GetRemainingSize(), &iSizeHi, &iSizeLo);
			pListAnswer->m_iRemainingSizeLo	= htonl(iSizeLo);
			pListAnswer->m_iRemainingSizeHi	= htonl(iSizeHi);
			pListAnswer->m_bFilenameConfirmed = htonl(pFileInfo->GetFilenameConfirmed());
			pListAnswer->m_bPaused			= htonl(pFileInfo->GetPaused());
			pListAnswer->m_iNZBFilenameLen	= htonl(strlen(pFileInfo->GetNZBInfo()->GetFilename()) + 1);
			pListAnswer->m_iSubjectLen		= htonl(strlen(pFileInfo->GetSubject()) + 1);
			pListAnswer->m_iFilenameLen		= htonl(strlen(pFileInfo->GetFilename()) + 1);
			pListAnswer->m_iDestDirLen		= htonl(strlen(pFileInfo->GetNZBInfo()->GetDestDir()) + 1);
			pListAnswer->m_iCategoryLen		= htonl(strlen(pFileInfo->GetNZBInfo()->GetCategory()) + 1);
			bufptr += sizeof(SNZBListResponseEntry);
			strcpy(bufptr, pFileInfo->GetNZBInfo()->GetFilename());
			bufptr += ntohl(pListAnswer->m_iNZBFilenameLen);
			strcpy(bufptr, pFileInfo->GetSubject());
			bufptr += ntohl(pListAnswer->m_iSubjectLen);
			strcpy(bufptr, pFileInfo->GetFilename());
			bufptr += ntohl(pListAnswer->m_iFilenameLen);
			strcpy(bufptr, pFileInfo->GetNZBInfo()->GetDestDir());
			bufptr += ntohl(pListAnswer->m_iDestDirLen);
			strcpy(bufptr, pFileInfo->GetNZBInfo()->GetCategory());
			bufptr += ntohl(pListAnswer->m_iCategoryLen);
			// align struct to 4-bytes, needed by ARM-processor (and may be others)
			if ((size_t)bufptr % 4 > 0)
			{
				pListAnswer->m_iCategoryLen = htonl(ntohl(pListAnswer->m_iCategoryLen) + 4 - (size_t)bufptr % 4);
				bufptr += 4 - (size_t)bufptr % 4;
			}
		}

		g_pQueueCoordinator->UnlockQueue();

		ListResponse.m_iNrTrailingEntries = htonl(NrEntries);
		ListResponse.m_iTrailingDataLength = htonl(bufsize);
	}

	if (htonl(ListRequest.m_bServerState))
	{
		unsigned long iSizeHi, iSizeLo;
		ListResponse.m_iDownloadRate = htonl((int)(g_pQueueCoordinator->CalcCurrentDownloadSpeed() * 1024));
		Util::SplitInt64(g_pQueueCoordinator->CalcRemainingSize(), &iSizeHi, &iSizeLo);
		ListResponse.m_iRemainingSizeHi = htonl(iSizeHi);
		ListResponse.m_iRemainingSizeLo = htonl(iSizeLo);
		ListResponse.m_iDownloadLimit = htonl((int)(g_pOptions->GetDownloadRate() * 1024));
		ListResponse.m_bServerPaused = htonl(g_pOptions->GetPause());
		ListResponse.m_iThreadCount = htonl(Thread::GetThreadCount() - 1); // not counting itself
		PostQueue* pPostQueue = g_pPrePostProcessor->LockPostQueue();
		ListResponse.m_iPostJobCount = htonl(pPostQueue->size());
		g_pPrePostProcessor->UnlockPostQueue();

		int iUpTimeSec, iDnTimeSec;
		long long iAllBytes;
		bool bStandBy;
		g_pQueueCoordinator->CalcStat(&iUpTimeSec, &iDnTimeSec, &iAllBytes, &bStandBy);
		ListResponse.m_iUpTimeSec = htonl(iUpTimeSec);
		ListResponse.m_iDownloadTimeSec = htonl(iDnTimeSec);
		ListResponse.m_bServerStandBy = htonl(bStandBy);
		Util::SplitInt64(iAllBytes, &iSizeHi, &iSizeLo);
		ListResponse.m_iDownloadedBytesHi = htonl(iSizeHi);
		ListResponse.m_iDownloadedBytesLo = htonl(iSizeLo);
	}

	// Send the request answer
	send(m_iSocket, (char*) &ListResponse, sizeof(ListResponse), 0);

	// Send the data
	if (bufsize > 0)
	{
		send(m_iSocket, buf, bufsize, 0);
	}

	if (buf)
	{
		free(buf);
	}
}

void LogBinCommand::Execute()
{
	SNZBLogRequest LogRequest;
	if (!ReceiveRequest(&LogRequest, sizeof(LogRequest)))
	{
		return;
	}

	Log::Messages* pMessages = g_pLog->LockMessages();

	int iNrEntries = ntohl(LogRequest.m_iLines);
	unsigned int iIDFrom = ntohl(LogRequest.m_iIDFrom);
	int iStart = pMessages->size();
	if (iNrEntries > 0)
	{
		if (iNrEntries > (int)pMessages->size())
		{
			iNrEntries = pMessages->size();
		}
		iStart = pMessages->size() - iNrEntries;
	}
	if (iIDFrom > 0 && !pMessages->empty())
	{
		iStart = iIDFrom - pMessages->front()->GetID();
		if (iStart < 0)
		{
			iStart = 0;
		}
		iNrEntries = pMessages->size() - iStart;
		if (iNrEntries < 0)
		{
			iNrEntries = 0;
		}
	}

	// calculate required buffer size
	int bufsize = iNrEntries * sizeof(SNZBLogResponseEntry);
	for (unsigned int i = (unsigned int)iStart; i < pMessages->size(); i++)
	{
		Message* pMessage = (*pMessages)[i];
		bufsize += strlen(pMessage->GetText()) + 1;
		// align struct to 4-bytes, needed by ARM-processor (and may be others)
		bufsize += bufsize % 4 > 0 ? 4 - bufsize % 4 : 0;
	}

	char* buf = (char*) malloc(bufsize);
	char* bufptr = buf;
	for (unsigned int i = (unsigned int)iStart; i < pMessages->size(); i++)
	{
		Message* pMessage = (*pMessages)[i];
		SNZBLogResponseEntry* pLogAnswer = (SNZBLogResponseEntry*) bufptr;
		pLogAnswer->m_iID = htonl(pMessage->GetID());
		pLogAnswer->m_iKind = htonl(pMessage->GetKind());
		pLogAnswer->m_tTime = htonl((int)pMessage->GetTime());
		pLogAnswer->m_iTextLen = htonl(strlen(pMessage->GetText()) + 1);
		bufptr += sizeof(SNZBLogResponseEntry);
		strcpy(bufptr, pMessage->GetText());
		bufptr += ntohl(pLogAnswer->m_iTextLen);
		// align struct to 4-bytes, needed by ARM-processor (and may be others)
		if ((size_t)bufptr % 4 > 0)
		{
			pLogAnswer->m_iTextLen = htonl(ntohl(pLogAnswer->m_iTextLen) + 4 - (size_t)bufptr % 4);
			bufptr += 4 - (size_t)bufptr % 4;
		}
	}

	g_pLog->UnlockMessages();

	SNZBLogResponse LogResponse;
	LogResponse.m_MessageBase.m_iSignature = htonl(NZBMESSAGE_SIGNATURE);
	LogResponse.m_MessageBase.m_iStructSize = htonl(sizeof(LogResponse));
	LogResponse.m_iEntrySize = htonl(sizeof(SNZBLogResponseEntry));
	LogResponse.m_iNrTrailingEntries = htonl(iNrEntries);
	LogResponse.m_iTrailingDataLength = htonl(bufsize);

	// Send the request answer
	send(m_iSocket, (char*) &LogResponse, sizeof(LogResponse), 0);

	// Send the data
	if (bufsize > 0)
	{
		send(m_iSocket, buf, bufsize, 0);
	}

	free(buf);
}

void EditQueueBinCommand::Execute()
{
	SNZBEditQueueRequest EditQueueRequest;
	if (!ReceiveRequest(&EditQueueRequest, sizeof(EditQueueRequest)))
	{
		return;
	}

	int iNrEntries = ntohl(EditQueueRequest.m_iNrTrailingEntries);
	int iAction = ntohl(EditQueueRequest.m_iAction);
	int iOffset = ntohl(EditQueueRequest.m_iOffset);
	int iTextLen = ntohl(EditQueueRequest.m_iTextLen);
	bool bSmartOrder = ntohl(EditQueueRequest.m_bSmartOrder);
	unsigned int iBufLength = ntohl(EditQueueRequest.m_iTrailingDataLength);

	if (iNrEntries * sizeof(int32_t) + iTextLen != iBufLength)
	{
		error("Invalid struct size");
		return;
	}

	if (iNrEntries <= 0)
	{
		SendBoolResponse(false, "Edit-Command failed: no IDs specified");
		return;
	}

	char* pBuf = (char*)malloc(iBufLength);
	char* szText = NULL;
	int32_t* pIDs = NULL;

	// Read from the socket until nothing remains
	char* pBufPtr = pBuf;
	int NeedBytes = iBufLength;
	int iResult = 0;
	while (NeedBytes > 0)
	{
		iResult = recv(m_iSocket, pBufPtr, NeedBytes, 0);
		// Did the recv succeed?
		if (iResult <= 0)
		{
			error("invalid request");
			break;
		}
		pBufPtr += iResult;
		NeedBytes -= iResult;
	}
	bool bOK = NeedBytes == 0;

	if (bOK)
	{
		szText = iTextLen > 0 ? pBuf : NULL;
		pIDs = (int32_t*)(pBuf + iTextLen);
	}

	QueueEditor::IDList cIDList;
	cIDList.reserve(iNrEntries);
	for (int i = 0; i < iNrEntries; i++)
	{
		cIDList.push_back(ntohl(pIDs[i]));
	}

	bOK = g_pQueueCoordinator->GetQueueEditor()->EditList(&cIDList, bSmartOrder, (QueueEditor::EEditAction)iAction, iOffset, szText);

	free(pBuf);

	if (bOK)
	{
		SendBoolResponse(true, "Edit-Command completed successfully");
	}
	else
	{
		SendBoolResponse(false, "Edit-Command failed");
	}
}

void PostQueueBinCommand::Execute()
{
	SNZBPostQueueRequest PostQueueRequest;
	if (!ReceiveRequest(&PostQueueRequest, sizeof(PostQueueRequest)))
	{
		return;
	}

	SNZBPostQueueResponse PostQueueResponse;
	memset(&PostQueueResponse, 0, sizeof(PostQueueResponse));
	PostQueueResponse.m_MessageBase.m_iSignature = htonl(NZBMESSAGE_SIGNATURE);
	PostQueueResponse.m_MessageBase.m_iStructSize = htonl(sizeof(PostQueueResponse));
	PostQueueResponse.m_iEntrySize = htonl(sizeof(SNZBPostQueueResponseEntry));

	char* buf = NULL;
	int bufsize = 0;

	// Make a data structure and copy all the elements of the list into it
	PostQueue* pPostQueue = g_pPrePostProcessor->LockPostQueue();

	int NrEntries = pPostQueue->size();

	// calculate required buffer size
	bufsize = NrEntries * sizeof(SNZBPostQueueResponseEntry);
	for (PostQueue::iterator it = pPostQueue->begin(); it != pPostQueue->end(); it++)
	{
		PostInfo* pPostInfo = *it;
		bufsize += strlen(pPostInfo->GetNZBFilename()) + 1;
		bufsize += strlen(pPostInfo->GetParFilename()) + 1;
		bufsize += strlen(pPostInfo->GetInfoName()) + 1;
		bufsize += strlen(pPostInfo->GetDestDir()) + 1;
		bufsize += strlen(pPostInfo->GetProgressLabel()) + 1;
		// align struct to 4-bytes, needed by ARM-processor (and may be others)
		bufsize += bufsize % 4 > 0 ? 4 - bufsize % 4 : 0;
	}

	time_t tCurTime = time(NULL);
	buf = (char*) malloc(bufsize);
	char* bufptr = buf;

	for (PostQueue::iterator it = pPostQueue->begin(); it != pPostQueue->end(); it++)
	{
		PostInfo* pPostInfo = *it;
		SNZBPostQueueResponseEntry* pPostQueueAnswer = (SNZBPostQueueResponseEntry*) bufptr;
		pPostQueueAnswer->m_iID				= htonl(pPostInfo->GetID());
		pPostQueueAnswer->m_iStage			= htonl(pPostInfo->GetStage());
		pPostQueueAnswer->m_iStageProgress	= htonl(pPostInfo->GetStageProgress());
		pPostQueueAnswer->m_iFileProgress	= htonl(pPostInfo->GetFileProgress());
		pPostQueueAnswer->m_iTotalTimeSec	= htonl((int)(pPostInfo->GetStartTime() ? tCurTime - pPostInfo->GetStartTime() : 0));
		pPostQueueAnswer->m_iStageTimeSec	= htonl((int)(pPostInfo->GetStageTime() ? tCurTime - pPostInfo->GetStageTime() : 0));
		pPostQueueAnswer->m_iNZBFilenameLen		= htonl(strlen(pPostInfo->GetNZBFilename()) + 1);
		pPostQueueAnswer->m_iParFilename		= htonl(strlen(pPostInfo->GetParFilename()) + 1);
		pPostQueueAnswer->m_iInfoNameLen		= htonl(strlen(pPostInfo->GetInfoName()) + 1);
		pPostQueueAnswer->m_iDestDirLen			= htonl(strlen(pPostInfo->GetDestDir()) + 1);
		pPostQueueAnswer->m_iProgressLabelLen	= htonl(strlen(pPostInfo->GetProgressLabel()) + 1);
		bufptr += sizeof(SNZBPostQueueResponseEntry);
		strcpy(bufptr, pPostInfo->GetNZBFilename());
		bufptr += ntohl(pPostQueueAnswer->m_iNZBFilenameLen);
		strcpy(bufptr, pPostInfo->GetParFilename());
		bufptr += ntohl(pPostQueueAnswer->m_iParFilename);
		strcpy(bufptr, pPostInfo->GetInfoName());
		bufptr += ntohl(pPostQueueAnswer->m_iInfoNameLen);
		strcpy(bufptr, pPostInfo->GetDestDir());
		bufptr += ntohl(pPostQueueAnswer->m_iDestDirLen);
		strcpy(bufptr, pPostInfo->GetProgressLabel());
		bufptr += ntohl(pPostQueueAnswer->m_iProgressLabelLen);
		// align struct to 4-bytes, needed by ARM-processor (and may be others)
		if ((size_t)bufptr % 4 > 0)
		{
			pPostQueueAnswer->m_iProgressLabelLen = htonl(ntohl(pPostQueueAnswer->m_iProgressLabelLen) + 4 - (size_t)bufptr % 4);
			bufptr += 4 - (size_t)bufptr % 4;
		}
	}

	g_pPrePostProcessor->UnlockPostQueue();

	PostQueueResponse.m_iNrTrailingEntries = htonl(NrEntries);
	PostQueueResponse.m_iTrailingDataLength = htonl(bufsize);

	// Send the request answer
	send(m_iSocket, (char*) &PostQueueResponse, sizeof(PostQueueResponse), 0);

	// Send the data
	if (bufsize > 0)
	{
		send(m_iSocket, buf, bufsize, 0);
	}

	if (buf)
	{
		free(buf);
	}
}

void WriteLogBinCommand::Execute()
{
	SNZBWriteLogRequest WriteLogRequest;
	if (!ReceiveRequest(&WriteLogRequest, sizeof(WriteLogRequest)))
	{
		return;
	}

	char* pRecvBuffer = (char*)malloc(ntohl(WriteLogRequest.m_iTrailingDataLength) + 1);
	char* pBufPtr = pRecvBuffer;

	// Read from the socket until nothing remains
	int iResult = 0;
	int NeedBytes = ntohl(WriteLogRequest.m_iTrailingDataLength);
	pRecvBuffer[NeedBytes] = '\0';
	while (NeedBytes > 0)
	{
		iResult = recv(m_iSocket, pBufPtr, NeedBytes, 0);
		// Did the recv succeed?
		if (iResult <= 0)
		{
			error("invalid request");
			break;
		}
		pBufPtr += iResult;
		NeedBytes -= iResult;
	}

	if (NeedBytes == 0)
	{
		bool OK = true;
		switch ((Message::EKind)ntohl(WriteLogRequest.m_iKind))
		{
			case Message::mkDetail:
				detail(pRecvBuffer);
				break;
			case Message::mkInfo:
				info(pRecvBuffer);
				break;
			case Message::mkWarning:
				warn(pRecvBuffer);
				break;
			case Message::mkError:
				error(pRecvBuffer);
				break;
			case Message::mkDebug:
				debug(pRecvBuffer);
				break;
			default:
				OK = false;
		}
		SendBoolResponse(OK, OK ? "Message added to log" : "Invalid message-kind");
	}

	free(pRecvBuffer);
}
