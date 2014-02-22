/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2005 Bo Cordes Petersen <placebodk@sourceforge.net>
 *  Copyright (C) 2007-2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include "nzbget.h"
#include "BinRpc.h"
#include "Log.h"
#include "Options.h"
#include "QueueCoordinator.h"
#include "UrlCoordinator.h"
#include "QueueEditor.h"
#include "PrePostProcessor.h"
#include "Util.h"
#include "DownloadInfo.h"
#include "Scanner.h"

extern Options* g_pOptions;
extern QueueCoordinator* g_pQueueCoordinator;
extern UrlCoordinator* g_pUrlCoordinator;
extern PrePostProcessor* g_pPrePostProcessor;
extern Scanner* g_pScanner;
extern void ExitProc();
extern void Reload();

const char* g_szMessageRequestNames[] =
    { "N/A", "Download", "Pause/Unpause", "List", "Set download rate", "Dump debug", 
		"Edit queue", "Log", "Quit", "Reload", "Version", "Post-queue", "Write log", "Scan", 
		"Pause/Unpause postprocessor", "History", "Download URL", "URL-queue" };

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
		sizeof(SNZBReloadRequest),
		sizeof(SNZBVersionRequest),
		sizeof(SNZBPostQueueRequest),
		sizeof(SNZBWriteLogRequest),
		sizeof(SNZBScanRequest),
		sizeof(SNZBHistoryRequest),
		sizeof(SNZBDownloadUrlRequest),
		sizeof(SNZBUrlQueueRequest)
    };

//*****************************************************************
// BinProcessor

BinRpcProcessor::BinRpcProcessor()
{
	m_MessageBase.m_iSignature = (int)NZBMESSAGE_SIGNATURE;
}

void BinRpcProcessor::Execute()
{
	// Read the first package which needs to be a request
	if (!m_pConnection->Recv(((char*)&m_MessageBase) + sizeof(m_MessageBase.m_iSignature), sizeof(m_MessageBase) - sizeof(m_MessageBase.m_iSignature)))
	{
		warn("Non-nzbget request received on port %i from %s", g_pOptions->GetControlPort(), m_pConnection->GetRemoteAddr());
		return;
	}

	if ((strlen(g_pOptions->GetControlUsername()) > 0 && strcmp(m_MessageBase.m_szUsername, g_pOptions->GetControlUsername())) ||
		strcmp(m_MessageBase.m_szPassword, g_pOptions->GetControlPassword()))
	{
		warn("nzbget request received on port %i from %s, but username or password invalid", g_pOptions->GetControlPort(), m_pConnection->GetRemoteAddr());
		return;
	}

	debug("%s request received from %s", g_szMessageRequestNames[ntohl(m_MessageBase.m_iType)], m_pConnection->GetRemoteAddr());

	Dispatch();
}

void BinRpcProcessor::Dispatch()
{
	if (ntohl(m_MessageBase.m_iType) >= (int)eRemoteRequestDownload &&
		   ntohl(m_MessageBase.m_iType) <= (int)eRemoteRequestHistory &&
		   g_iMessageRequestSizes[ntohl(m_MessageBase.m_iType)] != ntohl(m_MessageBase.m_iStructSize))
	{
		error("Invalid size of request: expected %i Bytes, but received %i Bytes",
			 g_iMessageRequestSizes[ntohl(m_MessageBase.m_iType)], ntohl(m_MessageBase.m_iStructSize));
		return;
	}
	
	BinCommand* command = NULL;

	switch (ntohl(m_MessageBase.m_iType))
	{
		case eRemoteRequestDownload:
			command = new DownloadBinCommand();
			break;

		case eRemoteRequestList:
			command = new ListBinCommand();
			break;

		case eRemoteRequestLog:
			command = new LogBinCommand();
			break;

		case eRemoteRequestPauseUnpause:
			command = new PauseUnpauseBinCommand();
			break;

		case eRemoteRequestEditQueue:
			command = new EditQueueBinCommand();
			break;

		case eRemoteRequestSetDownloadRate:
			command = new SetDownloadRateBinCommand();
			break;

		case eRemoteRequestDumpDebug:
			command = new DumpDebugBinCommand();
			break;

		case eRemoteRequestShutdown:
			command = new ShutdownBinCommand();
			break;

		case eRemoteRequestReload:
			command = new ReloadBinCommand();
			break;

		case eRemoteRequestVersion:
			command = new VersionBinCommand();
			break;

		case eRemoteRequestPostQueue:
			command = new PostQueueBinCommand();
			break;

		case eRemoteRequestWriteLog:
			command = new WriteLogBinCommand();
			break;

		case eRemoteRequestScan:
			command = new ScanBinCommand();
			break;

		case eRemoteRequestHistory:
			command = new HistoryBinCommand();
			break;

		case eRemoteRequestDownloadUrl:
			command = new DownloadUrlBinCommand();
			break;

		case eRemoteRequestUrlQueue:
			command = new UrlQueueBinCommand();
			break;

		default:
			error("Received unsupported request %i", ntohl(m_MessageBase.m_iType));
			break;
	}

	if (command)
	{
		command->SetConnection(m_pConnection);
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
	m_pConnection->Send((char*) &BoolResponse, sizeof(BoolResponse));
	m_pConnection->Send((char*)szText, iTextLen);
}

bool BinCommand::ReceiveRequest(void* pBuffer, int iSize)
{
	memcpy(pBuffer, m_pMessageBase, sizeof(SNZBRequestBase));
	iSize -= sizeof(SNZBRequestBase);
	if (iSize > 0)
	{
		if (!m_pConnection->Recv(((char*)pBuffer) + sizeof(SNZBRequestBase), iSize))
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

	g_pOptions->SetResumeTime(0);

	switch (ntohl(PauseUnpauseRequest.m_iAction))
	{
		case eRemotePauseUnpauseActionDownload:
			g_pOptions->SetPauseDownload(ntohl(PauseUnpauseRequest.m_bPause));
			break;

		case eRemotePauseUnpauseActionPostProcess:
			g_pOptions->SetPausePostProcess(ntohl(PauseUnpauseRequest.m_bPause));
			break;

		case eRemotePauseUnpauseActionScan:
			g_pOptions->SetPauseScan(ntohl(PauseUnpauseRequest.m_bPause));
			break;
	}

	SendBoolResponse(true, "Pause-/Unpause-Command completed successfully");
}

void SetDownloadRateBinCommand::Execute()
{
	SNZBSetDownloadRateRequest SetDownloadRequest;
	if (!ReceiveRequest(&SetDownloadRequest, sizeof(SetDownloadRequest)))
	{
		return;
	}

	g_pOptions->SetDownloadRate(ntohl(SetDownloadRequest.m_iDownloadRate));
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
	g_pUrlCoordinator->LogDebugInfo();
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

void ReloadBinCommand::Execute()
{
	SNZBReloadRequest ReloadRequest;
	if (!ReceiveRequest(&ReloadRequest, sizeof(ReloadRequest)))
	{
		return;
	}

	SendBoolResponse(true, "Reloading server");
	Reload();
}

void VersionBinCommand::Execute()
{
	SNZBVersionRequest VersionRequest;
	if (!ReceiveRequest(&VersionRequest, sizeof(VersionRequest)))
	{
		return;
	}

	SendBoolResponse(true, Util::VersionRevision());
}

void DownloadBinCommand::Execute()
{
	SNZBDownloadRequest DownloadRequest;
	if (!ReceiveRequest(&DownloadRequest, sizeof(DownloadRequest)))
	{
		return;
	}

	int iBufLen = ntohl(DownloadRequest.m_iTrailingDataLength);
	char* pRecvBuffer = (char*)malloc(iBufLen);

	if (!m_pConnection->Recv(pRecvBuffer, iBufLen))
	{
		error("invalid request");
		free(pRecvBuffer);
		return;
	}
	
	int iPriority = ntohl(DownloadRequest.m_iPriority);
	bool bAddPaused = ntohl(DownloadRequest.m_bAddPaused);
	bool bAddTop = ntohl(DownloadRequest.m_bAddFirst);

	bool bOK = g_pScanner->AddExternalFile(DownloadRequest.m_szFilename, DownloadRequest.m_szCategory,
		iPriority, NULL, 0, dmScore, NULL, bAddTop, bAddPaused, NULL, pRecvBuffer, iBufLen) != Scanner::asFailed;

	char tmp[1024];
	snprintf(tmp, 1024, bOK ? "Collection %s added to queue" : "Download Request failed for %s",
		Util::BaseFileName(DownloadRequest.m_szFilename));
	tmp[1024-1] = '\0';

	SendBoolResponse(bOK, tmp);

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
	ListResponse.m_iEntrySize = htonl(sizeof(SNZBListResponseFileEntry));
	ListResponse.m_bRegExValid = 0;

	char* buf = NULL;
	int bufsize = 0;

	if (ntohl(ListRequest.m_bFileList))
	{
		eRemoteMatchMode eMatchMode = (eRemoteMatchMode)ntohl(ListRequest.m_iMatchMode);
		bool bMatchGroup = ntohl(ListRequest.m_bMatchGroup);
		const char* szPattern = ListRequest.m_szPattern;

		RegEx *pRegEx = NULL;
		if (eMatchMode == eRemoteMatchModeRegEx)
		{
			pRegEx = new RegEx(szPattern);
			ListResponse.m_bRegExValid = pRegEx->IsValid();
		}

		// Make a data structure and copy all the elements of the list into it
		DownloadQueue* pDownloadQueue = DownloadQueue::Lock();

		// calculate required buffer size for nzbs
		int iNrNZBEntries = pDownloadQueue->GetQueue()->size();
		int iNrPPPEntries = 0;
		bufsize += iNrNZBEntries * sizeof(SNZBListResponseNZBEntry);
		for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
		{
			NZBInfo* pNZBInfo = *it;
			bufsize += strlen(pNZBInfo->GetFilename()) + 1;
			bufsize += strlen(pNZBInfo->GetName()) + 1;
			bufsize += strlen(pNZBInfo->GetDestDir()) + 1;
			bufsize += strlen(pNZBInfo->GetCategory()) + 1;
			bufsize += strlen(pNZBInfo->GetQueuedFilename()) + 1;
			// align struct to 4-bytes, needed by ARM-processor (and may be others)
			bufsize += bufsize % 4 > 0 ? 4 - bufsize % 4 : 0;

			// calculate required buffer size for pp-parameters
			for (NZBParameterList::iterator it = pNZBInfo->GetParameters()->begin(); it != pNZBInfo->GetParameters()->end(); it++)
			{
				NZBParameter* pNZBParameter = *it;
				bufsize += sizeof(SNZBListResponsePPPEntry);
				bufsize += strlen(pNZBParameter->GetName()) + 1;
				bufsize += strlen(pNZBParameter->GetValue()) + 1;
				// align struct to 4-bytes, needed by ARM-processor (and may be others)
				bufsize += bufsize % 4 > 0 ? 4 - bufsize % 4 : 0;
				iNrPPPEntries++;
			}
		}

		// calculate required buffer size for files
		int iNrFileEntries = 0;
		for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
		{
			NZBInfo* pNZBInfo = *it;
			for (FileList::iterator it2 = pNZBInfo->GetFileList()->begin(); it2 != pNZBInfo->GetFileList()->end(); it2++)
			{
				FileInfo* pFileInfo = *it2;
				iNrFileEntries++;
				bufsize += sizeof(SNZBListResponseFileEntry);
				bufsize += strlen(pFileInfo->GetSubject()) + 1;
				bufsize += strlen(pFileInfo->GetFilename()) + 1;
				// align struct to 4-bytes, needed by ARM-processor (and may be others)
				bufsize += bufsize % 4 > 0 ? 4 - bufsize % 4 : 0;
			}
		}

		buf = (char*) malloc(bufsize);
		char* bufptr = buf;

		// write nzb entries
		for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
		{
			NZBInfo* pNZBInfo = *it;

			SNZBListResponseNZBEntry* pListAnswer = (SNZBListResponseNZBEntry*) bufptr;
			unsigned long iSizeHi, iSizeLo;
			Util::SplitInt64(pNZBInfo->GetSize(), &iSizeHi, &iSizeLo);
			pListAnswer->m_iID					= htonl(pNZBInfo->GetID());
			pListAnswer->m_iSizeLo				= htonl(iSizeLo);
			pListAnswer->m_iSizeHi				= htonl(iSizeHi);
			pListAnswer->m_iPriority			= htonl(pNZBInfo->GetPriority());
			pListAnswer->m_bMatch				= htonl(bMatchGroup && (!pRegEx || pRegEx->Match(pNZBInfo->GetName())));
			pListAnswer->m_iFilenameLen			= htonl(strlen(pNZBInfo->GetFilename()) + 1);
			pListAnswer->m_iNameLen				= htonl(strlen(pNZBInfo->GetName()) + 1);
			pListAnswer->m_iDestDirLen			= htonl(strlen(pNZBInfo->GetDestDir()) + 1);
			pListAnswer->m_iCategoryLen			= htonl(strlen(pNZBInfo->GetCategory()) + 1);
			pListAnswer->m_iQueuedFilenameLen	= htonl(strlen(pNZBInfo->GetQueuedFilename()) + 1);
			bufptr += sizeof(SNZBListResponseNZBEntry);
			strcpy(bufptr, pNZBInfo->GetFilename());
			bufptr += ntohl(pListAnswer->m_iFilenameLen);
			strcpy(bufptr, pNZBInfo->GetName());
			bufptr += ntohl(pListAnswer->m_iNameLen);
			strcpy(bufptr, pNZBInfo->GetDestDir());
			bufptr += ntohl(pListAnswer->m_iDestDirLen);
			strcpy(bufptr, pNZBInfo->GetCategory());
			bufptr += ntohl(pListAnswer->m_iCategoryLen);
			strcpy(bufptr, pNZBInfo->GetQueuedFilename());
			bufptr += ntohl(pListAnswer->m_iQueuedFilenameLen);
			// align struct to 4-bytes, needed by ARM-processor (and may be others)
			if ((size_t)bufptr % 4 > 0)
			{
				pListAnswer->m_iQueuedFilenameLen = htonl(ntohl(pListAnswer->m_iQueuedFilenameLen) + 4 - (size_t)bufptr % 4);
				memset(bufptr, 0, 4 - (size_t)bufptr % 4); //suppress valgrind warning "uninitialized data"
				bufptr += 4 - (size_t)bufptr % 4;
			}
		}

		// write ppp entries
		int iNZBIndex = 1;
		for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++, iNZBIndex++)
		{
			NZBInfo* pNZBInfo = *it;
			for (NZBParameterList::iterator it = pNZBInfo->GetParameters()->begin(); it != pNZBInfo->GetParameters()->end(); it++)
			{
				NZBParameter* pNZBParameter = *it;
				SNZBListResponsePPPEntry* pListAnswer = (SNZBListResponsePPPEntry*) bufptr;
				pListAnswer->m_iNZBIndex	= htonl(iNZBIndex);
				pListAnswer->m_iNameLen		= htonl(strlen(pNZBParameter->GetName()) + 1);
				pListAnswer->m_iValueLen	= htonl(strlen(pNZBParameter->GetValue()) + 1);
				bufptr += sizeof(SNZBListResponsePPPEntry);
				strcpy(bufptr, pNZBParameter->GetName());
				bufptr += ntohl(pListAnswer->m_iNameLen);
				strcpy(bufptr, pNZBParameter->GetValue());
				bufptr += ntohl(pListAnswer->m_iValueLen);
				// align struct to 4-bytes, needed by ARM-processor (and may be others)
				if ((size_t)bufptr % 4 > 0)
				{
					pListAnswer->m_iValueLen = htonl(ntohl(pListAnswer->m_iValueLen) + 4 - (size_t)bufptr % 4);
					memset(bufptr, 0, 4 - (size_t)bufptr % 4); //suppress valgrind warning "uninitialized data"
					bufptr += 4 - (size_t)bufptr % 4;
				}
			}
		}

		// write file entries
		for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
		{
			NZBInfo* pNZBInfo = *it;
			for (FileList::iterator it2 = pNZBInfo->GetFileList()->begin(); it2 != pNZBInfo->GetFileList()->end(); it2++)
			{
				FileInfo* pFileInfo = *it2;

				unsigned long iSizeHi, iSizeLo;
				SNZBListResponseFileEntry* pListAnswer = (SNZBListResponseFileEntry*) bufptr;
				pListAnswer->m_iID = htonl(pFileInfo->GetID());

				int iNZBIndex = 0;
				for (unsigned int i = 0; i < pDownloadQueue->GetQueue()->size(); i++)
				{
					iNZBIndex++;
					if (pDownloadQueue->GetQueue()->at(i) == pFileInfo->GetNZBInfo())
					{
						break;
					}
				}
				pListAnswer->m_iNZBIndex		= htonl(iNZBIndex);

				if (pRegEx && !bMatchGroup)
				{
					char szFilename[MAX_PATH];
					snprintf(szFilename, sizeof(szFilename) - 1, "%s/%s", pFileInfo->GetNZBInfo()->GetName(), Util::BaseFileName(pFileInfo->GetFilename()));
					pListAnswer->m_bMatch = htonl(pRegEx->Match(szFilename));
				}

				Util::SplitInt64(pFileInfo->GetSize(), &iSizeHi, &iSizeLo);
				pListAnswer->m_iFileSizeLo		= htonl(iSizeLo);
				pListAnswer->m_iFileSizeHi		= htonl(iSizeHi);
				Util::SplitInt64(pFileInfo->GetRemainingSize(), &iSizeHi, &iSizeLo);
				pListAnswer->m_iRemainingSizeLo	= htonl(iSizeLo);
				pListAnswer->m_iRemainingSizeHi	= htonl(iSizeHi);
				pListAnswer->m_bFilenameConfirmed = htonl(pFileInfo->GetFilenameConfirmed());
				pListAnswer->m_bPaused			= htonl(pFileInfo->GetPaused());
				pListAnswer->m_iActiveDownloads	= htonl(pFileInfo->GetActiveDownloads());
				pListAnswer->m_iSubjectLen		= htonl(strlen(pFileInfo->GetSubject()) + 1);
				pListAnswer->m_iFilenameLen		= htonl(strlen(pFileInfo->GetFilename()) + 1);
				bufptr += sizeof(SNZBListResponseFileEntry);
				strcpy(bufptr, pFileInfo->GetSubject());
				bufptr += ntohl(pListAnswer->m_iSubjectLen);
				strcpy(bufptr, pFileInfo->GetFilename());
				bufptr += ntohl(pListAnswer->m_iFilenameLen);
				// align struct to 4-bytes, needed by ARM-processor (and may be others)
				if ((size_t)bufptr % 4 > 0)
				{
					pListAnswer->m_iFilenameLen = htonl(ntohl(pListAnswer->m_iFilenameLen) + 4 - (size_t)bufptr % 4);
					memset(bufptr, 0, 4 - (size_t)bufptr % 4); //suppress valgrind warning "uninitialized data"
					bufptr += 4 - (size_t)bufptr % 4;
				}
			}
		}

		DownloadQueue::Unlock();

		delete pRegEx;

		ListResponse.m_iNrTrailingNZBEntries = htonl(iNrNZBEntries);
		ListResponse.m_iNrTrailingPPPEntries = htonl(iNrPPPEntries);
		ListResponse.m_iNrTrailingFileEntries = htonl(iNrFileEntries);
		ListResponse.m_iTrailingDataLength = htonl(bufsize);
	}

	if (htonl(ListRequest.m_bServerState))
	{
		unsigned long iSizeHi, iSizeLo;
		ListResponse.m_iDownloadRate = htonl(g_pQueueCoordinator->CalcCurrentDownloadSpeed());
		Util::SplitInt64(g_pQueueCoordinator->CalcRemainingSize(), &iSizeHi, &iSizeLo);
		ListResponse.m_iRemainingSizeHi = htonl(iSizeHi);
		ListResponse.m_iRemainingSizeLo = htonl(iSizeLo);
		ListResponse.m_iDownloadLimit = htonl(g_pOptions->GetDownloadRate());
		ListResponse.m_bDownloadPaused = htonl(g_pOptions->GetPauseDownload());
		ListResponse.m_bPostPaused = htonl(g_pOptions->GetPausePostProcess());
		ListResponse.m_bScanPaused = htonl(g_pOptions->GetPauseScan());
		ListResponse.m_iThreadCount = htonl(Thread::GetThreadCount() - 1); // not counting itself
		ListResponse.m_iPostJobCount = htonl(g_pPrePostProcessor->GetJobCount());

		int iUpTimeSec, iDnTimeSec;
		long long iAllBytes;
		bool bStandBy;
		g_pQueueCoordinator->CalcStat(&iUpTimeSec, &iDnTimeSec, &iAllBytes, &bStandBy);
		ListResponse.m_iUpTimeSec = htonl(iUpTimeSec);
		ListResponse.m_iDownloadTimeSec = htonl(iDnTimeSec);
		ListResponse.m_bDownloadStandBy = htonl(bStandBy);
		Util::SplitInt64(iAllBytes, &iSizeHi, &iSizeLo);
		ListResponse.m_iDownloadedBytesHi = htonl(iSizeHi);
		ListResponse.m_iDownloadedBytesLo = htonl(iSizeLo);
	}

	// Send the request answer
	m_pConnection->Send((char*) &ListResponse, sizeof(ListResponse));

	// Send the data
	if (bufsize > 0)
	{
		m_pConnection->Send(buf, bufsize);
	}

	free(buf);
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
			memset(bufptr, 0, 4 - (size_t)bufptr % 4); //suppress valgrind warning "uninitialized data"
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
	m_pConnection->Send((char*) &LogResponse, sizeof(LogResponse));

	// Send the data
	if (bufsize > 0)
	{
		m_pConnection->Send(buf, bufsize);
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

	int iNrIDEntries = ntohl(EditQueueRequest.m_iNrTrailingIDEntries);
	int iNrNameEntries = ntohl(EditQueueRequest.m_iNrTrailingNameEntries);
	int iNameEntriesLen = ntohl(EditQueueRequest.m_iTrailingNameEntriesLen);
	int iAction = ntohl(EditQueueRequest.m_iAction);
	int iMatchMode = ntohl(EditQueueRequest.m_iMatchMode);
	int iOffset = ntohl(EditQueueRequest.m_iOffset);
	int iTextLen = ntohl(EditQueueRequest.m_iTextLen);
	unsigned int iBufLength = ntohl(EditQueueRequest.m_iTrailingDataLength);

	if (iNrIDEntries * sizeof(int32_t) + iTextLen + iNameEntriesLen != iBufLength)
	{
		error("Invalid struct size");
		return;
	}

	char* pBuf = (char*)malloc(iBufLength);
	
	if (!m_pConnection->Recv(pBuf, iBufLength))
	{
		error("invalid request");
		free(pBuf);
		return;
	}

	if (iNrIDEntries <= 0 && iNrNameEntries <= 0)
	{
		SendBoolResponse(false, "Edit-Command failed: no IDs/Names specified");
		return;
	}

	char* szText = iTextLen > 0 ? pBuf : NULL;
	int32_t* pIDs = (int32_t*)(pBuf + iTextLen);
	char* pNames = (pBuf + iTextLen + iNrIDEntries * sizeof(int32_t));

	IDList cIDList;
	NameList cNameList;

	if (iNrIDEntries > 0)
	{
		cIDList.reserve(iNrIDEntries);
		for (int i = 0; i < iNrIDEntries; i++)
		{
			cIDList.push_back(ntohl(pIDs[i]));
		}
	}

	if (iNrNameEntries > 0)
	{
		cNameList.reserve(iNrNameEntries);
		for (int i = 0; i < iNrNameEntries; i++)
		{
			cNameList.push_back(pNames);
			pNames += strlen(pNames) + 1;
		}
	}

	DownloadQueue* pDownloadQueue = DownloadQueue::Lock();
	bool bOK = pDownloadQueue->EditList(
		iNrIDEntries > 0 ? &cIDList : NULL,
		iNrNameEntries > 0 ? &cNameList : NULL,
		(DownloadQueue::EMatchMode)iMatchMode, (DownloadQueue::EEditAction)iAction, iOffset, szText);
	DownloadQueue::Unlock();

	free(pBuf);

	if (bOK)
	{
		SendBoolResponse(true, "Edit-Command completed successfully");
	}
	else
	{
#ifndef HAVE_REGEX_H
		if ((QueueEditor::EMatchMode)iMatchMode == QueueEditor::mmRegEx)
		{
			SendBoolResponse(false, "Edit-Command failed: the program was compiled without RegEx-support");
			return;
		}
#endif
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
	NZBList* pNZBList = DownloadQueue::Lock()->GetQueue();

	// calculate required buffer size
	int NrEntries = 0;
	for (NZBList::iterator it = pNZBList->begin(); it != pNZBList->end(); it++)
	{
		NZBInfo* pNZBInfo = *it;
		PostInfo* pPostInfo = pNZBInfo->GetPostInfo();
		if (!pPostInfo)
		{
			continue;
		}

		NrEntries++;
		bufsize += sizeof(SNZBPostQueueResponseEntry);
		bufsize += strlen(pPostInfo->GetNZBInfo()->GetFilename()) + 1;
		bufsize += strlen(pPostInfo->GetNZBInfo()->GetName()) + 1;
		bufsize += strlen(pPostInfo->GetNZBInfo()->GetDestDir()) + 1;
		bufsize += strlen(pPostInfo->GetProgressLabel()) + 1;
		// align struct to 4-bytes, needed by ARM-processor (and may be others)
		bufsize += bufsize % 4 > 0 ? 4 - bufsize % 4 : 0;
	}

	time_t tCurTime = time(NULL);
	buf = (char*) malloc(bufsize);
	char* bufptr = buf;

	for (NZBList::iterator it = pNZBList->begin(); it != pNZBList->end(); it++)
	{
		NZBInfo* pNZBInfo = *it;
		PostInfo* pPostInfo = pNZBInfo->GetPostInfo();
		if (!pPostInfo)
		{
			continue;
		}

		SNZBPostQueueResponseEntry* pPostQueueAnswer = (SNZBPostQueueResponseEntry*) bufptr;
		pPostQueueAnswer->m_iID				= htonl(pNZBInfo->GetID());
		pPostQueueAnswer->m_iStage			= htonl(pPostInfo->GetStage());
		pPostQueueAnswer->m_iStageProgress	= htonl(pPostInfo->GetStageProgress());
		pPostQueueAnswer->m_iFileProgress	= htonl(pPostInfo->GetFileProgress());
		pPostQueueAnswer->m_iTotalTimeSec	= htonl((int)(pPostInfo->GetStartTime() ? tCurTime - pPostInfo->GetStartTime() : 0));
		pPostQueueAnswer->m_iStageTimeSec	= htonl((int)(pPostInfo->GetStageTime() ? tCurTime - pPostInfo->GetStageTime() : 0));
		pPostQueueAnswer->m_iNZBFilenameLen		= htonl(strlen(pPostInfo->GetNZBInfo()->GetFilename()) + 1);
		pPostQueueAnswer->m_iInfoNameLen		= htonl(strlen(pPostInfo->GetNZBInfo()->GetName()) + 1);
		pPostQueueAnswer->m_iDestDirLen			= htonl(strlen(pPostInfo->GetNZBInfo()->GetDestDir()) + 1);
		pPostQueueAnswer->m_iProgressLabelLen	= htonl(strlen(pPostInfo->GetProgressLabel()) + 1);
		bufptr += sizeof(SNZBPostQueueResponseEntry);
		strcpy(bufptr, pPostInfo->GetNZBInfo()->GetFilename());
		bufptr += ntohl(pPostQueueAnswer->m_iNZBFilenameLen);
		strcpy(bufptr, pPostInfo->GetNZBInfo()->GetName());
		bufptr += ntohl(pPostQueueAnswer->m_iInfoNameLen);
		strcpy(bufptr, pPostInfo->GetNZBInfo()->GetDestDir());
		bufptr += ntohl(pPostQueueAnswer->m_iDestDirLen);
		strcpy(bufptr, pPostInfo->GetProgressLabel());
		bufptr += ntohl(pPostQueueAnswer->m_iProgressLabelLen);
		// align struct to 4-bytes, needed by ARM-processor (and may be others)
		if ((size_t)bufptr % 4 > 0)
		{
			pPostQueueAnswer->m_iProgressLabelLen = htonl(ntohl(pPostQueueAnswer->m_iProgressLabelLen) + 4 - (size_t)bufptr % 4);
			memset(bufptr, 0, 4 - (size_t)bufptr % 4); //suppress valgrind warning "uninitialized data"
			bufptr += 4 - (size_t)bufptr % 4;
		}
	}

	DownloadQueue::Unlock();

	PostQueueResponse.m_iNrTrailingEntries = htonl(NrEntries);
	PostQueueResponse.m_iTrailingDataLength = htonl(bufsize);

	// Send the request answer
	m_pConnection->Send((char*) &PostQueueResponse, sizeof(PostQueueResponse));

	// Send the data
	if (bufsize > 0)
	{
		m_pConnection->Send(buf, bufsize);
	}

	free(buf);
}

void WriteLogBinCommand::Execute()
{
	SNZBWriteLogRequest WriteLogRequest;
	if (!ReceiveRequest(&WriteLogRequest, sizeof(WriteLogRequest)))
	{
		return;
	}

	char* pRecvBuffer = (char*)malloc(ntohl(WriteLogRequest.m_iTrailingDataLength) + 1);
	
	if (!m_pConnection->Recv(pRecvBuffer, ntohl(WriteLogRequest.m_iTrailingDataLength)))
	{
		error("invalid request");
		free(pRecvBuffer);
		return;
	}
	
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

	free(pRecvBuffer);
}

void ScanBinCommand::Execute()
{
	SNZBScanRequest ScanRequest;
	if (!ReceiveRequest(&ScanRequest, sizeof(ScanRequest)))
	{
		return;
	}

	bool bSyncMode = ntohl(ScanRequest.m_bSyncMode);

	g_pScanner->ScanNZBDir(bSyncMode);
	SendBoolResponse(true, bSyncMode ? "Scan-Command completed" : "Scan-Command scheduled successfully");
}

void HistoryBinCommand::Execute()
{
	SNZBHistoryRequest HistoryRequest;
	if (!ReceiveRequest(&HistoryRequest, sizeof(HistoryRequest)))
	{
		return;
	}

	SNZBHistoryResponse HistoryResponse;
	memset(&HistoryResponse, 0, sizeof(HistoryResponse));
	HistoryResponse.m_MessageBase.m_iSignature = htonl(NZBMESSAGE_SIGNATURE);
	HistoryResponse.m_MessageBase.m_iStructSize = htonl(sizeof(HistoryResponse));
	HistoryResponse.m_iEntrySize = htonl(sizeof(SNZBHistoryResponseEntry));

	char* buf = NULL;
	int bufsize = 0;

	// Make a data structure and copy all the elements of the list into it
	DownloadQueue* pDownloadQueue = DownloadQueue::Lock();

	// calculate required buffer size for nzbs
	int iNrEntries = pDownloadQueue->GetHistory()->size();
	bufsize += iNrEntries * sizeof(SNZBHistoryResponseEntry);
	for (HistoryList::iterator it = pDownloadQueue->GetHistory()->begin(); it != pDownloadQueue->GetHistory()->end(); it++)
	{
		HistoryInfo* pHistoryInfo = *it;
		char szNicename[1024];
		pHistoryInfo->GetName(szNicename, sizeof(szNicename));
		bufsize += strlen(szNicename) + 1;
		// align struct to 4-bytes, needed by ARM-processor (and may be others)
		bufsize += bufsize % 4 > 0 ? 4 - bufsize % 4 : 0;
	}

	buf = (char*) malloc(bufsize);
	char* bufptr = buf;

	// write nzb entries
	for (HistoryList::iterator it = pDownloadQueue->GetHistory()->begin(); it != pDownloadQueue->GetHistory()->end(); it++)
	{
		HistoryInfo* pHistoryInfo = *it;
		SNZBHistoryResponseEntry* pListAnswer = (SNZBHistoryResponseEntry*) bufptr;
		pListAnswer->m_iID					= htonl(pHistoryInfo->GetID());
		pListAnswer->m_iKind				= htonl((int)pHistoryInfo->GetKind());
		pListAnswer->m_tTime				= htonl((int)pHistoryInfo->GetTime());

		char szNicename[1024];
		pHistoryInfo->GetName(szNicename, sizeof(szNicename));
		pListAnswer->m_iNicenameLen			= htonl(strlen(szNicename) + 1);

		if (pHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo)
		{
			NZBInfo* pNZBInfo = pHistoryInfo->GetNZBInfo();
			unsigned long iSizeHi, iSizeLo;
			Util::SplitInt64(pNZBInfo->GetSize(), &iSizeHi, &iSizeLo);
			pListAnswer->m_iSizeLo				= htonl(iSizeLo);
			pListAnswer->m_iSizeHi				= htonl(iSizeHi);
			pListAnswer->m_iFileCount			= htonl(pNZBInfo->GetFileCount());
			pListAnswer->m_iParStatus			= htonl(pNZBInfo->GetParStatus());
			pListAnswer->m_iScriptStatus		= htonl(pNZBInfo->GetScriptStatuses()->CalcTotalStatus());
		}
		else if (pHistoryInfo->GetKind() == HistoryInfo::hkUrlInfo)
		{
			UrlInfo* pUrlInfo = pHistoryInfo->GetUrlInfo();
			pListAnswer->m_iUrlStatus			= htonl(pUrlInfo->GetStatus());
		}

		bufptr += sizeof(SNZBHistoryResponseEntry);
		strcpy(bufptr, szNicename);
		bufptr += ntohl(pListAnswer->m_iNicenameLen);
		// align struct to 4-bytes, needed by ARM-processor (and may be others)
		if ((size_t)bufptr % 4 > 0)
		{
			pListAnswer->m_iNicenameLen = htonl(ntohl(pListAnswer->m_iNicenameLen) + 4 - (size_t)bufptr % 4);
			memset(bufptr, 0, 4 - (size_t)bufptr % 4); //suppress valgrind warning "uninitialized data"
			bufptr += 4 - (size_t)bufptr % 4;
		}
	}

	DownloadQueue::Unlock();

	HistoryResponse.m_iNrTrailingEntries = htonl(iNrEntries);
	HistoryResponse.m_iTrailingDataLength = htonl(bufsize);

	// Send the request answer
	m_pConnection->Send((char*) &HistoryResponse, sizeof(HistoryResponse));

	// Send the data
	if (bufsize > 0)
	{
		m_pConnection->Send(buf, bufsize);
	}

	free(buf);
}

void DownloadUrlBinCommand::Execute()
{
	SNZBDownloadUrlRequest DownloadUrlRequest;
	if (!ReceiveRequest(&DownloadUrlRequest, sizeof(DownloadUrlRequest)))
	{
		return;
	}

	URL url(DownloadUrlRequest.m_szURL);
	if (!url.IsValid())
	{
		char tmp[1024];
		snprintf(tmp, 1024, "Url %s is not valid", DownloadUrlRequest.m_szURL);
		tmp[1024-1] = '\0';
		SendBoolResponse(true, tmp);
		return;
	}

	UrlInfo* pUrlInfo = new UrlInfo();
	pUrlInfo->SetURL(DownloadUrlRequest.m_szURL);
	pUrlInfo->SetNZBFilename(DownloadUrlRequest.m_szNZBFilename);
	pUrlInfo->SetCategory(DownloadUrlRequest.m_szCategory);
	pUrlInfo->SetPriority(ntohl(DownloadUrlRequest.m_iPriority));
	pUrlInfo->SetAddTop(ntohl(DownloadUrlRequest.m_bAddFirst));
	pUrlInfo->SetAddPaused(ntohl(DownloadUrlRequest.m_bAddPaused));

	g_pUrlCoordinator->AddUrlToQueue(pUrlInfo, ntohl(DownloadUrlRequest.m_bAddFirst));

	info("Request: Queue url %s", DownloadUrlRequest.m_szURL);

	char tmp[1024];
	snprintf(tmp, 1024, "Url %s added to queue", DownloadUrlRequest.m_szURL);
	tmp[1024-1] = '\0';
	SendBoolResponse(true, tmp);
}

void UrlQueueBinCommand::Execute()
{
	SNZBUrlQueueRequest UrlQueueRequest;
	if (!ReceiveRequest(&UrlQueueRequest, sizeof(UrlQueueRequest)))
	{
		return;
	}

	SNZBUrlQueueResponse UrlQueueResponse;
	memset(&UrlQueueResponse, 0, sizeof(UrlQueueResponse));
	UrlQueueResponse.m_MessageBase.m_iSignature = htonl(NZBMESSAGE_SIGNATURE);
	UrlQueueResponse.m_MessageBase.m_iStructSize = htonl(sizeof(UrlQueueResponse));
	UrlQueueResponse.m_iEntrySize = htonl(sizeof(SNZBUrlQueueResponseEntry));

	char* buf = NULL;
	int bufsize = 0;

	// Make a data structure and copy all the elements of the list into it
	UrlQueue* pUrlQueue = DownloadQueue::Lock()->GetUrlQueue();

	int NrEntries = pUrlQueue->size();

	// calculate required buffer size
	bufsize = NrEntries * sizeof(SNZBUrlQueueResponseEntry);
	for (UrlQueue::iterator it = pUrlQueue->begin(); it != pUrlQueue->end(); it++)
	{
		UrlInfo* pUrlInfo = *it;
		bufsize += strlen(pUrlInfo->GetURL()) + 1;
		bufsize += strlen(pUrlInfo->GetNZBFilename()) + 1;
		// align struct to 4-bytes, needed by ARM-processor (and may be others)
		bufsize += bufsize % 4 > 0 ? 4 - bufsize % 4 : 0;
	}

	buf = (char*) malloc(bufsize);
	char* bufptr = buf;

	for (UrlQueue::iterator it = pUrlQueue->begin(); it != pUrlQueue->end(); it++)
	{
		UrlInfo* pUrlInfo = *it;
		SNZBUrlQueueResponseEntry* pUrlQueueAnswer = (SNZBUrlQueueResponseEntry*) bufptr;
		pUrlQueueAnswer->m_iID				= htonl(pUrlInfo->GetID());
		pUrlQueueAnswer->m_iURLLen			= htonl(strlen(pUrlInfo->GetURL()) + 1);
		pUrlQueueAnswer->m_iNZBFilenameLen		= htonl(strlen(pUrlInfo->GetNZBFilename()) + 1);
		bufptr += sizeof(SNZBUrlQueueResponseEntry);
		strcpy(bufptr, pUrlInfo->GetURL());
		bufptr += ntohl(pUrlQueueAnswer->m_iURLLen);
		strcpy(bufptr, pUrlInfo->GetNZBFilename());
		bufptr += ntohl(pUrlQueueAnswer->m_iNZBFilenameLen);
		// align struct to 4-bytes, needed by ARM-processor (and may be others)
		if ((size_t)bufptr % 4 > 0)
		{
			pUrlQueueAnswer->m_iNZBFilenameLen = htonl(ntohl(pUrlQueueAnswer->m_iNZBFilenameLen) + 4 - (size_t)bufptr % 4);
			memset(bufptr, 0, 4 - (size_t)bufptr % 4); //suppress valgrind warning "uninitialized data"
			bufptr += 4 - (size_t)bufptr % 4;
		}
	}

	DownloadQueue::Unlock();

	UrlQueueResponse.m_iNrTrailingEntries = htonl(NrEntries);
	UrlQueueResponse.m_iTrailingDataLength = htonl(bufsize);

	// Send the request answer
	m_pConnection->Send((char*) &UrlQueueResponse, sizeof(UrlQueueResponse));

	// Send the data
	if (bufsize > 0)
	{
		m_pConnection->Send(buf, bufsize);
	}

	free(buf);
}
