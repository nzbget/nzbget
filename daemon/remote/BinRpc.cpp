/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2005 Bo Cordes Petersen <placebodk@sourceforge.net>
 *  Copyright (C) 2007-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "nzbget.h"
#include "BinRpc.h"
#include "Log.h"
#include "Options.h"
#include "WorkState.h"
#include "QueueEditor.h"
#include "Util.h"
#include "FileSystem.h"
#include "DownloadInfo.h"
#include "Scanner.h"
#include "StatMeter.h"
#include "UrlCoordinator.h"

extern void ExitProc();
extern void Reload();

const char* g_MessageRequestNames[] =
	{ "N/A", "Download", "Pause/Unpause", "List", "Set download rate", "Dump debug",
		"Edit queue", "Log", "Quit", "Reload", "Version", "Post-queue", "Write log", "Scan",
		"Pause/Unpause postprocessor", "History" };

const uint32 g_MessageRequestSizes[] =
	{ 0,
		sizeof(SNzbDownloadRequest),
		sizeof(SNzbPauseUnpauseRequest),
		sizeof(SNzbListRequest),
		sizeof(SNzbSetDownloadRateRequest),
		sizeof(SNzbDumpDebugRequest),
		sizeof(SNzbEditQueueRequest),
		sizeof(SNzbLogRequest),
		sizeof(SNzbShutdownRequest),
		sizeof(SNzbReloadRequest),
		sizeof(SNzbVersionRequest),
		sizeof(SNzbPostQueueRequest),
		sizeof(SNzbWriteLogRequest),
		sizeof(SNzbScanRequest),
		sizeof(SNzbHistoryRequest)
	};


class BinCommand
{
public:
	virtual ~BinCommand() {}
	virtual void Execute() = 0;
	void SetConnection(Connection* connection) { m_connection = connection; }
	void SetMessageBase(SNzbRequestBase* messageBase) { m_messageBase = messageBase; }

protected:
	Connection* m_connection;
	SNzbRequestBase* m_messageBase;

	bool ReceiveRequest(void* buffer, int size);
	void SendBoolResponse(bool success, const char* text);
};

class DownloadBinCommand: public BinCommand
{
public:
	virtual void Execute();
};

class ListBinCommand: public BinCommand
{
public:
	virtual void Execute();
};

class LogBinCommand: public BinCommand
{
public:
	virtual void Execute();
};

class PauseUnpauseBinCommand: public BinCommand
{
public:
	virtual void Execute();
};

class EditQueueBinCommand: public BinCommand
{
public:
	virtual void Execute();
};

class SetDownloadRateBinCommand: public BinCommand
{
public:
	virtual void Execute();
};

class DumpDebugBinCommand: public BinCommand
{
public:
	virtual void Execute();
};

class ShutdownBinCommand: public BinCommand
{
public:
	virtual void Execute();
};

class ReloadBinCommand: public BinCommand
{
public:
	virtual void Execute();
};

class VersionBinCommand: public BinCommand
{
public:
	virtual void Execute();
};

class PostQueueBinCommand: public BinCommand
{
public:
	virtual void Execute();
};

class WriteLogBinCommand: public BinCommand
{
public:
	virtual void Execute();
};

class ScanBinCommand: public BinCommand
{
public:
	virtual void Execute();
};

class HistoryBinCommand: public BinCommand
{
public:
	virtual void Execute();
};

//*****************************************************************
// BinProcessor

BinRpcProcessor::BinRpcProcessor()
{
	m_messageBase.m_signature = (int)NZBMESSAGE_SIGNATURE;
}

void BinRpcProcessor::Execute()
{
	// Read the first package which needs to be a request
	if (!m_connection->Recv(((char*)&m_messageBase) + sizeof(m_messageBase.m_signature), sizeof(m_messageBase) - sizeof(m_messageBase.m_signature)))
	{
		warn("Non-nzbget request received on port %i from %s", g_Options->GetControlPort(), m_connection->GetRemoteAddr());
		return;
	}

	if ((strlen(g_Options->GetControlUsername()) > 0 && strcmp(m_messageBase.m_username, g_Options->GetControlUsername())) ||
		strcmp(m_messageBase.m_password, g_Options->GetControlPassword()))
	{
		warn("nzbget request received on port %i from %s, but username or password invalid", g_Options->GetControlPort(), m_connection->GetRemoteAddr());
		return;
	}

	debug("%s request received from %s", g_MessageRequestNames[ntohl(m_messageBase.m_type)], m_connection->GetRemoteAddr());

	Dispatch();
}

void BinRpcProcessor::Dispatch()
{
	if (ntohl(m_messageBase.m_type) >= (int)rrDownload &&
		   ntohl(m_messageBase.m_type) <= (int)rrHistory &&
		   g_MessageRequestSizes[ntohl(m_messageBase.m_type)] != ntohl(m_messageBase.m_structSize))
	{
		error("Invalid size of request: expected %i Bytes, but received %i Bytes",
			 g_MessageRequestSizes[ntohl(m_messageBase.m_type)], ntohl(m_messageBase.m_structSize));
		return;
	}

	std::unique_ptr<BinCommand> command;

	switch (ntohl(m_messageBase.m_type))
	{
		case rrDownload:
			command = std::make_unique<DownloadBinCommand>();
			break;

		case rrList:
			command = std::make_unique<ListBinCommand>();
			break;

		case rrLog:
			command = std::make_unique<LogBinCommand>();
			break;

		case rrPauseUnpause:
			command = std::make_unique<PauseUnpauseBinCommand>();
			break;

		case rrEditQueue:
			command = std::make_unique<EditQueueBinCommand>();
			break;

		case rrSetDownloadRate:
			command = std::make_unique<SetDownloadRateBinCommand>();
			break;

		case rrDumpDebug:
			command = std::make_unique<DumpDebugBinCommand>();
			break;

		case rrShutdown:
			command = std::make_unique<ShutdownBinCommand>();
			break;

		case rrReload:
			command = std::make_unique<ReloadBinCommand>();
			break;

		case rrVersion:
			command = std::make_unique<VersionBinCommand>();
			break;

		case rrPostQueue:
			command = std::make_unique<PostQueueBinCommand>();
			break;

		case rrWriteLog:
			command = std::make_unique<WriteLogBinCommand>();
			break;

		case rrScan:
			command = std::make_unique<ScanBinCommand>();
			break;

		case rrHistory:
			command = std::make_unique<HistoryBinCommand>();
			break;

		default:
			error("Received unsupported request %i", ntohl(m_messageBase.m_type));
			break;
	}

	if (command)
	{
		command->SetConnection(m_connection);
		command->SetMessageBase(&m_messageBase);
		command->Execute();
	}
}

//*****************************************************************
// Commands

void BinCommand::SendBoolResponse(bool success, const char* text)
{
	// all bool-responses have the same format of structure, we use SNZBDownloadResponse here
	SNzbDownloadResponse BoolResponse;
	memset(&BoolResponse, 0, sizeof(BoolResponse));
	BoolResponse.m_messageBase.m_signature = htonl(NZBMESSAGE_SIGNATURE);
	BoolResponse.m_messageBase.m_structSize = htonl(sizeof(BoolResponse));
	BoolResponse.m_success = htonl(success);
	int textLen = strlen(text) + 1;
	BoolResponse.m_trailingDataLength = htonl(textLen);

	// Send the request answer
	m_connection->Send((char*) &BoolResponse, sizeof(BoolResponse));
	m_connection->Send((char*)text, textLen);
}

bool BinCommand::ReceiveRequest(void* buffer, int size)
{
	memcpy(buffer, m_messageBase, sizeof(SNzbRequestBase));
	size -= sizeof(SNzbRequestBase);
	if (size > 0)
	{
		if (!m_connection->Recv(((char*)buffer) + sizeof(SNzbRequestBase), size))
		{
			error("invalid request");
			return false;
		}
	}
	return true;
}

void PauseUnpauseBinCommand::Execute()
{
	SNzbPauseUnpauseRequest PauseUnpauseRequest;
	if (!ReceiveRequest(&PauseUnpauseRequest, sizeof(PauseUnpauseRequest)))
	{
		return;
	}

	g_WorkState->SetResumeTime(0);

	switch (ntohl(PauseUnpauseRequest.m_action))
	{
		case rpDownload:
			g_WorkState->SetPauseDownload(ntohl(PauseUnpauseRequest.m_pause));
			break;

		case rpPostProcess:
			g_WorkState->SetPausePostProcess(ntohl(PauseUnpauseRequest.m_pause));
			break;

		case rpScan:
			g_WorkState->SetPauseScan(ntohl(PauseUnpauseRequest.m_pause));
			break;
	}

	SendBoolResponse(true, "Pause-/Unpause-Command completed successfully");
}

void SetDownloadRateBinCommand::Execute()
{
	SNzbSetDownloadRateRequest SetDownloadRequest;
	if (!ReceiveRequest(&SetDownloadRequest, sizeof(SetDownloadRequest)))
	{
		return;
	}

	g_WorkState->SetSpeedLimit(ntohl(SetDownloadRequest.m_downloadRate));
	SendBoolResponse(true, "Rate-Command completed successfully");
}

void DumpDebugBinCommand::Execute()
{
	SNzbDumpDebugRequest DumpDebugRequest;
	if (!ReceiveRequest(&DumpDebugRequest, sizeof(DumpDebugRequest)))
	{
		return;
	}

	g_Log->LogDebugInfo();
	SendBoolResponse(true, "Debug-Command completed successfully");
}

void ShutdownBinCommand::Execute()
{
	SNzbShutdownRequest ShutdownRequest;
	if (!ReceiveRequest(&ShutdownRequest, sizeof(ShutdownRequest)))
	{
		return;
	}

	SendBoolResponse(true, "Stopping server");
	ExitProc();
}

void ReloadBinCommand::Execute()
{
	SNzbReloadRequest ReloadRequest;
	if (!ReceiveRequest(&ReloadRequest, sizeof(ReloadRequest)))
	{
		return;
	}

	SendBoolResponse(true, "Reloading server");
	Reload();
}

void VersionBinCommand::Execute()
{
	SNzbVersionRequest VersionRequest;
	if (!ReceiveRequest(&VersionRequest, sizeof(VersionRequest)))
	{
		return;
	}

	SendBoolResponse(true, Util::VersionRevision());
}

void DownloadBinCommand::Execute()
{
	SNzbDownloadRequest DownloadRequest;
	if (!ReceiveRequest(&DownloadRequest, sizeof(DownloadRequest)))
	{
		return;
	}

	int bufLen = ntohl(DownloadRequest.m_trailingDataLength);
	CharBuffer nzbContent(bufLen);

	if (!m_connection->Recv(nzbContent, nzbContent.Size()))
	{
		error("invalid request");
		return;
	}

	int priority = ntohl(DownloadRequest.m_priority);
	bool addPaused = ntohl(DownloadRequest.m_addPaused);
	bool addTop = ntohl(DownloadRequest.m_addFirst);
	int dupeMode = ntohl(DownloadRequest.m_dupeMode);
	int dupeScore = ntohl(DownloadRequest.m_dupeScore);

	bool ok = false;

	if (!strncasecmp(nzbContent, "http://", 6) || !strncasecmp(nzbContent, "https://", 7))
	{
		// add url
		std::unique_ptr<NzbInfo> nzbInfo = std::make_unique<NzbInfo>();
		nzbInfo->SetKind(NzbInfo::nkUrl);
		nzbInfo->SetUrl(nzbContent);
		nzbInfo->SetFilename(DownloadRequest.m_nzbFilename);
		nzbInfo->SetCategory(DownloadRequest.m_category);
		nzbInfo->SetPriority(priority);
		nzbInfo->SetAddUrlPaused(addPaused);
		nzbInfo->SetDupeKey(DownloadRequest.m_dupeKey);
		nzbInfo->SetDupeScore(dupeScore);
		nzbInfo->SetDupeMode((EDupeMode)dupeMode);

		g_UrlCoordinator->AddUrlToQueue(std::move(nzbInfo), addTop);

		ok = true;
	}
	else
	{
		ok = g_Scanner->AddExternalFile(DownloadRequest.m_nzbFilename, DownloadRequest.m_category, priority,
			DownloadRequest.m_dupeKey, dupeScore, (EDupeMode)dupeMode, nullptr, addTop, addPaused,
			nullptr, nullptr, nzbContent, bufLen, nullptr) != Scanner::asFailed;
	}

	SendBoolResponse(ok, BString<1024>(ok ? "Collection %s added to queue" :
		"Download Request failed for %s",
		FileSystem::BaseFileName(DownloadRequest.m_nzbFilename)));
}

void ListBinCommand::Execute()
{
	SNzbListRequest ListRequest;
	if (!ReceiveRequest(&ListRequest, sizeof(ListRequest)))
	{
		return;
	}

	SNzbListResponse ListResponse;
	memset(&ListResponse, 0, sizeof(ListResponse));
	ListResponse.m_messageBase.m_signature = htonl(NZBMESSAGE_SIGNATURE);
	ListResponse.m_messageBase.m_structSize = htonl(sizeof(ListResponse));
	ListResponse.m_entrySize = htonl(sizeof(SNzbListResponseFileEntry));
	ListResponse.m_regExValid = 0;

	CharBuffer buf;
	int bufsize = 0;

	if (ntohl(ListRequest.m_fileList))
	{
		ERemoteMatchMode matchMode = (ERemoteMatchMode)ntohl(ListRequest.m_matchMode);
		bool matchGroup = ntohl(ListRequest.m_matchGroup);
		const char* pattern = ListRequest.m_pattern;

		std::unique_ptr<RegEx> regEx;
		if (matchMode == rmRegEx)
		{
			regEx = std::make_unique<RegEx>(pattern);
			ListResponse.m_regExValid = regEx->IsValid();
		}

		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

		// calculate required buffer size for nzbs
		int nrNzbEntries = downloadQueue->GetQueue()->size();
		int nrPPPEntries = 0;
		bufsize += nrNzbEntries * sizeof(SNzbListResponseNzbEntry);
		for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
		{
			bufsize += strlen(nzbInfo->GetFilename()) + 1;
			bufsize += strlen(nzbInfo->GetName()) + 1;
			bufsize += strlen(nzbInfo->GetDestDir()) + 1;
			bufsize += strlen(nzbInfo->GetCategory()) + 1;
			bufsize += strlen(nzbInfo->GetQueuedFilename()) + 1;
			// align struct to 4-bytes, needed by ARM-processor (and may be others)
			bufsize += bufsize % 4 > 0 ? 4 - bufsize % 4 : 0;

			// calculate required buffer size for pp-parameters
			for (NzbParameter& nzbParameter : nzbInfo->GetParameters())
			{
				bufsize += sizeof(SNzbListResponsePPPEntry);
				bufsize += strlen(nzbParameter.GetName()) + 1;
				bufsize += strlen(nzbParameter.GetValue()) + 1;
				// align struct to 4-bytes, needed by ARM-processor (and may be others)
				bufsize += bufsize % 4 > 0 ? 4 - bufsize % 4 : 0;
				nrPPPEntries++;
			}
		}

		// calculate required buffer size for files
		int nrFileEntries = 0;
		for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
		{
			for (FileInfo* fileInfo : nzbInfo->GetFileList())
			{
				nrFileEntries++;
				bufsize += sizeof(SNzbListResponseFileEntry);
				bufsize += strlen(fileInfo->GetSubject()) + 1;
				bufsize += strlen(fileInfo->GetFilename()) + 1;
				// align struct to 4-bytes, needed by ARM-processor (and may be others)
				bufsize += bufsize % 4 > 0 ? 4 - bufsize % 4 : 0;
			}
		}

		buf.Reserve(bufsize);
		char* bufptr = buf;

		// write nzb entries
		for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
		{
			SNzbListResponseNzbEntry* listAnswer = (SNzbListResponseNzbEntry*) bufptr;

			uint32 sizeHi, sizeLo, remainingSizeHi, remainingSizeLo, pausedSizeHi, pausedSizeLo;
			Util::SplitInt64(nzbInfo->GetSize(), &sizeHi, &sizeLo);
			Util::SplitInt64(nzbInfo->GetRemainingSize(), &remainingSizeHi, &remainingSizeLo);
			Util::SplitInt64(nzbInfo->GetPausedSize(), &pausedSizeHi, &pausedSizeLo);

			listAnswer->m_id = htonl(nzbInfo->GetId());
			listAnswer->m_kind = htonl(nzbInfo->GetKind());
			listAnswer->m_sizeLo = htonl(sizeLo);
			listAnswer->m_sizeHi = htonl(sizeHi);
			listAnswer->m_remainingSizeLo = htonl(remainingSizeLo);
			listAnswer->m_remainingSizeHi = htonl(remainingSizeHi);
			listAnswer->m_pausedSizeLo = htonl(pausedSizeLo);
			listAnswer->m_pausedSizeHi = htonl(pausedSizeHi);
			listAnswer->m_pausedCount = htonl(nzbInfo->GetPausedFileCount());
			listAnswer->m_remainingParCount = htonl(nzbInfo->GetRemainingParCount());
			listAnswer->m_priority = htonl(nzbInfo->GetPriority());
			listAnswer->m_match = htonl(matchGroup && (!regEx || regEx->Match(nzbInfo->GetName())));
			listAnswer->m_filenameLen = htonl(strlen(nzbInfo->GetFilename()) + 1);
			listAnswer->m_nameLen = htonl(strlen(nzbInfo->GetName()) + 1);
			listAnswer->m_destDirLen = htonl(strlen(nzbInfo->GetDestDir()) + 1);
			listAnswer->m_categoryLen = htonl(strlen(nzbInfo->GetCategory()) + 1);
			listAnswer->m_queuedFilenameLen = htonl(strlen(nzbInfo->GetQueuedFilename()) + 1);
			bufptr += sizeof(SNzbListResponseNzbEntry);
			strcpy(bufptr, nzbInfo->GetFilename());
			bufptr += ntohl(listAnswer->m_filenameLen);
			strcpy(bufptr, nzbInfo->GetName());
			bufptr += ntohl(listAnswer->m_nameLen);
			strcpy(bufptr, nzbInfo->GetDestDir());
			bufptr += ntohl(listAnswer->m_destDirLen);
			strcpy(bufptr, nzbInfo->GetCategory());
			bufptr += ntohl(listAnswer->m_categoryLen);
			strcpy(bufptr, nzbInfo->GetQueuedFilename());
			bufptr += ntohl(listAnswer->m_queuedFilenameLen);
			// align struct to 4-bytes, needed by ARM-processor (and may be others)
			if ((size_t)bufptr % 4 > 0)
			{
				listAnswer->m_queuedFilenameLen = htonl(ntohl(listAnswer->m_queuedFilenameLen) + 4 - (size_t)bufptr % 4);
				memset(bufptr, 0, 4 - (size_t)bufptr % 4); //suppress valgrind warning "uninitialized data"
				bufptr += 4 - (size_t)bufptr % 4;
			}
		}

		// write ppp entries
		int nzbIndex = 0;
		for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
		{
			nzbIndex++;
			for (NzbParameter& nzbParameter : nzbInfo->GetParameters())
			{
				SNzbListResponsePPPEntry* listAnswer = (SNzbListResponsePPPEntry*) bufptr;
				listAnswer->m_nzbIndex = htonl(nzbIndex);
				listAnswer->m_nameLen = htonl(strlen(nzbParameter.GetName()) + 1);
				listAnswer->m_valueLen = htonl(strlen(nzbParameter.GetValue()) + 1);
				bufptr += sizeof(SNzbListResponsePPPEntry);
				strcpy(bufptr, nzbParameter.GetName());
				bufptr += ntohl(listAnswer->m_nameLen);
				strcpy(bufptr, nzbParameter.GetValue());
				bufptr += ntohl(listAnswer->m_valueLen);
				// align struct to 4-bytes, needed by ARM-processor (and may be others)
				if ((size_t)bufptr % 4 > 0)
				{
					listAnswer->m_valueLen = htonl(ntohl(listAnswer->m_valueLen) + 4 - (size_t)bufptr % 4);
					memset(bufptr, 0, 4 - (size_t)bufptr % 4); //suppress valgrind warning "uninitialized data"
					bufptr += 4 - (size_t)bufptr % 4;
				}
			}
		}

		// write file entries
		for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
		{
			for (FileInfo* fileInfo : nzbInfo->GetFileList())
			{
				uint32 sizeHi, sizeLo;
				SNzbListResponseFileEntry* listAnswer = (SNzbListResponseFileEntry*) bufptr;
				listAnswer->m_id = htonl(fileInfo->GetId());

				int nzbIndex = 0;
				for (uint32 i = 0; i < downloadQueue->GetQueue()->size(); i++)
				{
					nzbIndex++;
					if (downloadQueue->GetQueue()->at(i).get() == fileInfo->GetNzbInfo())
					{
						break;
					}
				}
				listAnswer->m_nzbIndex = htonl(nzbIndex);

				if (regEx && !matchGroup)
				{
					BString<1024> filename("%s/%s", fileInfo->GetNzbInfo()->GetName(),
						FileSystem::BaseFileName(fileInfo->GetFilename()));
					listAnswer->m_match = htonl(regEx->Match(filename));
				}

				Util::SplitInt64(fileInfo->GetSize(), &sizeHi, &sizeLo);
				listAnswer->m_fileSizeLo = htonl(sizeLo);
				listAnswer->m_fileSizeHi = htonl(sizeHi);
				Util::SplitInt64(fileInfo->GetRemainingSize(), &sizeHi, &sizeLo);
				listAnswer->m_remainingSizeLo = htonl(sizeLo);
				listAnswer->m_remainingSizeHi = htonl(sizeHi);
				listAnswer->m_filenameConfirmed = htonl(fileInfo->GetFilenameConfirmed());
				listAnswer->m_paused = htonl(fileInfo->GetPaused());
				listAnswer->m_activeDownloads = htonl(fileInfo->GetActiveDownloads());
				listAnswer->m_subjectLen = htonl(strlen(fileInfo->GetSubject()) + 1);
				listAnswer->m_filenameLen = htonl(strlen(fileInfo->GetFilename()) + 1);
				bufptr += sizeof(SNzbListResponseFileEntry);
				strcpy(bufptr, fileInfo->GetSubject());
				bufptr += ntohl(listAnswer->m_subjectLen);
				strcpy(bufptr, fileInfo->GetFilename());
				bufptr += ntohl(listAnswer->m_filenameLen);
				// align struct to 4-bytes, needed by ARM-processor (and may be others)
				if ((size_t)bufptr % 4 > 0)
				{
					listAnswer->m_filenameLen = htonl(ntohl(listAnswer->m_filenameLen) + 4 - (size_t)bufptr % 4);
					memset(bufptr, 0, 4 - (size_t)bufptr % 4); //suppress valgrind warning "uninitialized data"
					bufptr += 4 - (size_t)bufptr % 4;
				}
			}
		}

		ListResponse.m_nrTrailingNzbEntries = htonl(nrNzbEntries);
		ListResponse.m_nrTrailingPPPEntries = htonl(nrPPPEntries);
		ListResponse.m_nrTrailingFileEntries = htonl(nrFileEntries);
		ListResponse.m_trailingDataLength = htonl(bufsize);
	}

	if (htonl(ListRequest.m_serverState))
	{
		int postJobCount = 0;
		int64 remainingSize;
		{
			GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
			for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
			{
				postJobCount += nzbInfo->GetPostInfo() ? 1 : 0;
			}
			downloadQueue->CalcRemainingSize(&remainingSize, nullptr);
		}

		uint32 sizeHi, sizeLo;
		ListResponse.m_downloadRate = htonl(g_StatMeter->CalcCurrentDownloadSpeed());
		Util::SplitInt64(remainingSize, &sizeHi, &sizeLo);
		ListResponse.m_remainingSizeHi = htonl(sizeHi);
		ListResponse.m_remainingSizeLo = htonl(sizeLo);
		ListResponse.m_downloadLimit = htonl(g_WorkState->GetSpeedLimit());
		ListResponse.m_downloadPaused = htonl(g_WorkState->GetPauseDownload());
		ListResponse.m_postPaused = htonl(g_WorkState->GetPausePostProcess());
		ListResponse.m_scanPaused = htonl(g_WorkState->GetPauseScan());
		ListResponse.m_threadCount = htonl(Thread::GetThreadCount() - 1); // not counting itself
		ListResponse.m_postJobCount = htonl(postJobCount);

		int upTimeSec, dnTimeSec;
		int64 allBytes;
		bool standBy;
		g_StatMeter->CalcTotalStat(&upTimeSec, &dnTimeSec, &allBytes, &standBy);
		ListResponse.m_upTimeSec = htonl(upTimeSec);
		ListResponse.m_downloadTimeSec = htonl(dnTimeSec);
		ListResponse.m_downloadStandBy = htonl(standBy);
		Util::SplitInt64(allBytes, &sizeHi, &sizeLo);
		ListResponse.m_downloadedBytesHi = htonl(sizeHi);
		ListResponse.m_downloadedBytesLo = htonl(sizeLo);
	}

	// Send the request answer
	m_connection->Send((char*) &ListResponse, sizeof(ListResponse));

	// Send the data
	if (bufsize > 0)
	{
		m_connection->Send(buf, bufsize);
	}
}

void LogBinCommand::Execute()
{
	SNzbLogRequest LogRequest;
	if (!ReceiveRequest(&LogRequest, sizeof(LogRequest)))
	{
		return;
	}

	CharBuffer buf;
	int nrEntries;
	int bufsize;

	{
		GuardedMessageList messages = g_Log->GuardMessages();

		nrEntries = ntohl(LogRequest.m_lines);
		uint32 idFrom = ntohl(LogRequest.m_idFrom);
		int start = messages->size();
		if (nrEntries > 0)
		{
			if (nrEntries > (int)messages->size())
			{
				nrEntries = messages->size();
			}
			start = messages->size() - nrEntries;
		}
		if (idFrom > 0 && !messages->empty())
		{
			start = idFrom - messages->front().GetId();
			if (start < 0)
			{
				start = 0;
			}
			nrEntries = messages->size() - start;
			if (nrEntries < 0)
			{
				nrEntries = 0;
			}
		}

		// calculate required buffer size
		bufsize = nrEntries * sizeof(SNzbLogResponseEntry);
		for (uint32 i = (uint32)start; i < messages->size(); i++)
		{
			Message& message = messages->at(i);
			bufsize += strlen(message.GetText()) + 1;
			// align struct to 4-bytes, needed by ARM-processor (and may be others)
			bufsize += bufsize % 4 > 0 ? 4 - bufsize % 4 : 0;
		}

		buf.Reserve(bufsize);
		char* bufptr = buf;
		for (uint32 i = (uint32)start; i < messages->size(); i++)
		{
			Message& message = messages->at(i);
			SNzbLogResponseEntry* logAnswer = (SNzbLogResponseEntry*)bufptr;
			logAnswer->m_id = htonl(message.GetId());
			logAnswer->m_kind = htonl(message.GetKind());
			logAnswer->m_time = htonl((int)message.GetTime());
			logAnswer->m_textLen = htonl(strlen(message.GetText()) + 1);
			bufptr += sizeof(SNzbLogResponseEntry);
			strcpy(bufptr, message.GetText());
			bufptr += ntohl(logAnswer->m_textLen);
			// align struct to 4-bytes, needed by ARM-processor (and may be others)
			if ((size_t)bufptr % 4 > 0)
			{
				logAnswer->m_textLen = htonl(ntohl(logAnswer->m_textLen) + 4 - (size_t)bufptr % 4);
				memset(bufptr, 0, 4 - (size_t)bufptr % 4); //suppress valgrind warning "uninitialized data"
				bufptr += 4 - (size_t)bufptr % 4;
			}
		}
	}

	SNzbLogResponse LogResponse;
	LogResponse.m_messageBase.m_signature = htonl(NZBMESSAGE_SIGNATURE);
	LogResponse.m_messageBase.m_structSize = htonl(sizeof(LogResponse));
	LogResponse.m_entrySize = htonl(sizeof(SNzbLogResponseEntry));
	LogResponse.m_nrTrailingEntries = htonl(nrEntries);
	LogResponse.m_trailingDataLength = htonl(bufsize);

	// Send the request answer
	m_connection->Send((char*) &LogResponse, sizeof(LogResponse));

	// Send the data
	if (bufsize > 0)
	{
		m_connection->Send(buf, bufsize);
	}
}

void EditQueueBinCommand::Execute()
{
	SNzbEditQueueRequest EditQueueRequest;
	if (!ReceiveRequest(&EditQueueRequest, sizeof(EditQueueRequest)))
	{
		return;
	}

	int nrIdEntries = ntohl(EditQueueRequest.m_nrTrailingIdEntries);
	int nrNameEntries = ntohl(EditQueueRequest.m_nrTrailingNameEntries);
	int nameEntriesLen = ntohl(EditQueueRequest.m_trailingNameEntriesLen);
	int action = ntohl(EditQueueRequest.m_action);
	int matchMode = ntohl(EditQueueRequest.m_matchMode);
	int offset = ntohl(EditQueueRequest.m_offset);
	int textLen = ntohl(EditQueueRequest.m_textLen);
	uint32 bufLength = ntohl(EditQueueRequest.m_trailingDataLength);

	if (nrIdEntries * sizeof(int32) + textLen + nameEntriesLen != bufLength)
	{
		error("Invalid struct size");
		return;
	}

	CharBuffer buf(bufLength);

	if (!m_connection->Recv(buf, buf.Size()))
	{
		error("invalid request");
		return;
	}

	if (nrIdEntries <= 0 && nrNameEntries <= 0)
	{
		SendBoolResponse(false, "Edit-Command failed: no IDs/Names specified");
		return;
	}

	char* text = textLen > 0 ? *buf : nullptr;
	int32* ids = (int32*)(buf + textLen);
	char* names = (buf + textLen + nrIdEntries * sizeof(int32));

	IdList cIdList;
	NameList cNameList;

	if (nrIdEntries > 0)
	{
		cIdList.reserve(nrIdEntries);
		for (int i = 0; i < nrIdEntries; i++)
		{
			cIdList.push_back(ntohl(ids[i]));
		}
	}

	if (nrNameEntries > 0)
	{
		cNameList.reserve(nrNameEntries);
		for (int i = 0; i < nrNameEntries; i++)
		{
			cNameList.push_back(names);
			names += strlen(names) + 1;
		}
	}

	bool ok = DownloadQueue::Guard()->EditList(
		nrIdEntries > 0 ? &cIdList : nullptr,
		nrNameEntries > 0 ? &cNameList : nullptr,
		(DownloadQueue::EMatchMode)matchMode, (DownloadQueue::EEditAction)action,
		action == DownloadQueue::eaFileMoveOffset || action == DownloadQueue::eaGroupMoveOffset ?
		*CString::FormatStr("%i", offset) : text);

	if (ok)
	{
		SendBoolResponse(true, "Edit-Command completed successfully");
	}
	else
	{
#ifndef HAVE_REGEX_H
		if ((DownloadQueue::EMatchMode)matchMode == DownloadQueue::mmRegEx)
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
	SNzbPostQueueRequest PostQueueRequest;
	if (!ReceiveRequest(&PostQueueRequest, sizeof(PostQueueRequest)))
	{
		return;
	}

	SNzbPostQueueResponse PostQueueResponse;
	memset(&PostQueueResponse, 0, sizeof(PostQueueResponse));
	PostQueueResponse.m_messageBase.m_signature = htonl(NZBMESSAGE_SIGNATURE);
	PostQueueResponse.m_messageBase.m_structSize = htonl(sizeof(PostQueueResponse));
	PostQueueResponse.m_entrySize = htonl(sizeof(SNzbPostQueueResponseEntry));

	CharBuffer buf;
	int bufsize = 0;

	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

		// Make a data structure and copy all the elements of the list into it
		NzbList* nzbList = downloadQueue->GetQueue();

		// calculate required buffer size
		int NrEntries = 0;
		for (NzbInfo* nzbInfo : nzbList)
		{
			PostInfo* postInfo = nzbInfo->GetPostInfo();
			if (!postInfo)
			{
				continue;
			}

			NrEntries++;
			bufsize += sizeof(SNzbPostQueueResponseEntry);
			bufsize += strlen(postInfo->GetNzbInfo()->GetFilename()) + 1;
			bufsize += strlen(postInfo->GetNzbInfo()->GetName()) + 1;
			bufsize += strlen(postInfo->GetNzbInfo()->GetDestDir()) + 1;
			bufsize += strlen(postInfo->GetProgressLabel()) + 1;
			// align struct to 4-bytes, needed by ARM-processor (and may be others)
			bufsize += bufsize % 4 > 0 ? 4 - bufsize % 4 : 0;
		}

		time_t curTime = Util::CurrentTime();

		buf.Reserve(bufsize);
		char* bufptr = buf;

		for (NzbInfo* nzbInfo : nzbList)
		{
			PostInfo* postInfo = nzbInfo->GetPostInfo();
			if (!postInfo)
			{
				continue;
			}

			SNzbPostQueueResponseEntry* postQueueAnswer = (SNzbPostQueueResponseEntry*)bufptr;
			postQueueAnswer->m_id = htonl(nzbInfo->GetId());
			postQueueAnswer->m_stage = htonl(postInfo->GetStage());
			postQueueAnswer->m_stageProgress = htonl(postInfo->GetStageProgress());
			postQueueAnswer->m_fileProgress = htonl(postInfo->GetFileProgress());
			postQueueAnswer->m_totalTimeSec = htonl((int)(postInfo->GetStartTime() ? curTime - postInfo->GetStartTime() : 0));
			postQueueAnswer->m_stageTimeSec = htonl((int)(postInfo->GetStageTime() ? curTime - postInfo->GetStageTime() : 0));
			postQueueAnswer->m_nzbFilenameLen = htonl(strlen(postInfo->GetNzbInfo()->GetFilename()) + 1);
			postQueueAnswer->m_infoNameLen = htonl(strlen(postInfo->GetNzbInfo()->GetName()) + 1);
			postQueueAnswer->m_destDirLen = htonl(strlen(postInfo->GetNzbInfo()->GetDestDir()) + 1);
			postQueueAnswer->m_progressLabelLen = htonl(strlen(postInfo->GetProgressLabel()) + 1);
			bufptr += sizeof(SNzbPostQueueResponseEntry);
			strcpy(bufptr, postInfo->GetNzbInfo()->GetFilename());
			bufptr += ntohl(postQueueAnswer->m_nzbFilenameLen);
			strcpy(bufptr, postInfo->GetNzbInfo()->GetName());
			bufptr += ntohl(postQueueAnswer->m_infoNameLen);
			strcpy(bufptr, postInfo->GetNzbInfo()->GetDestDir());
			bufptr += ntohl(postQueueAnswer->m_destDirLen);
			strcpy(bufptr, postInfo->GetProgressLabel());
			bufptr += ntohl(postQueueAnswer->m_progressLabelLen);
			// align struct to 4-bytes, needed by ARM-processor (and may be others)
			if ((size_t)bufptr % 4 > 0)
			{
				postQueueAnswer->m_progressLabelLen = htonl(ntohl(postQueueAnswer->m_progressLabelLen) + 4 - (size_t)bufptr % 4);
				memset(bufptr, 0, 4 - (size_t)bufptr % 4); //suppress valgrind warning "uninitialized data"
				bufptr += 4 - (size_t)bufptr % 4;
			}
		}

		PostQueueResponse.m_nrTrailingEntries = htonl(NrEntries);
		PostQueueResponse.m_trailingDataLength = htonl(bufsize);
	}

	// Send the request answer
	m_connection->Send((char*) &PostQueueResponse, sizeof(PostQueueResponse));

	// Send the data
	if (bufsize > 0)
	{
		m_connection->Send(buf, bufsize);
	}
}

void WriteLogBinCommand::Execute()
{
	SNzbWriteLogRequest WriteLogRequest;
	if (!ReceiveRequest(&WriteLogRequest, sizeof(WriteLogRequest)))
	{
		return;
	}

	CharBuffer recvBuffer(ntohl(WriteLogRequest.m_trailingDataLength));

	if (!m_connection->Recv(recvBuffer, recvBuffer.Size()))
	{
		error("invalid request");
		return;
	}

	bool OK = true;
	switch ((Message::EKind)ntohl(WriteLogRequest.m_kind))
	{
		case Message::mkDetail:
			detail("%s", *recvBuffer);
			break;
		case Message::mkInfo:
			info("%s", *recvBuffer);
			break;
		case Message::mkWarning:
			warn("%s", *recvBuffer);
			break;
		case Message::mkError:
			error("%s", *recvBuffer);
			break;
		case Message::mkDebug:
			debug("%s", *recvBuffer);
			break;
		default:
			OK = false;
	}
	SendBoolResponse(OK, OK ? "Message added to log" : "Invalid message-kind");
}

void ScanBinCommand::Execute()
{
	SNzbScanRequest ScanRequest;
	if (!ReceiveRequest(&ScanRequest, sizeof(ScanRequest)))
	{
		return;
	}

	bool syncMode = ntohl(ScanRequest.m_syncMode);

	g_Scanner->ScanNzbDir(syncMode);
	SendBoolResponse(true, syncMode ? "Scan-Command completed" : "Scan-Command scheduled successfully");
}

void HistoryBinCommand::Execute()
{
	SNzbHistoryRequest HistoryRequest;
	if (!ReceiveRequest(&HistoryRequest, sizeof(HistoryRequest)))
	{
		return;
	}

	bool showHidden = ntohl(HistoryRequest.m_hidden);

	SNzbHistoryResponse HistoryResponse;
	memset(&HistoryResponse, 0, sizeof(HistoryResponse));
	HistoryResponse.m_messageBase.m_signature = htonl(NZBMESSAGE_SIGNATURE);
	HistoryResponse.m_messageBase.m_structSize = htonl(sizeof(HistoryResponse));
	HistoryResponse.m_entrySize = htonl(sizeof(SNzbHistoryResponseEntry));

	int bufsize = 0;
	CharBuffer buf;

	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

		// Make a data structure and copy all the elements of the list into it

		// calculate required buffer size for nzbs
		int nrEntries = 0;
		for (HistoryInfo* historyInfo : downloadQueue->GetHistory())
		{
			if (historyInfo->GetKind() != HistoryInfo::hkDup || showHidden)
			{
				nrEntries++;
			}
		}
		bufsize += nrEntries * sizeof(SNzbHistoryResponseEntry);
		for (HistoryInfo* historyInfo : downloadQueue->GetHistory())
		{
			if (historyInfo->GetKind() != HistoryInfo::hkDup || showHidden)
			{
				bufsize += strlen(historyInfo->GetName()) + 1;
				// align struct to 4-bytes, needed by ARM-processor (and may be others)
				bufsize += bufsize % 4 > 0 ? 4 - bufsize % 4 : 0;
			}
		}

		buf.Reserve(bufsize);
		char* bufptr = buf;

		// write nzb entries
		for (HistoryInfo* historyInfo : downloadQueue->GetHistory())
		{
			if (historyInfo->GetKind() != HistoryInfo::hkDup || showHidden)
			{
				SNzbHistoryResponseEntry* listAnswer = (SNzbHistoryResponseEntry*)bufptr;
				listAnswer->m_id = htonl(historyInfo->GetId());
				listAnswer->m_kind = htonl((int)historyInfo->GetKind());
				listAnswer->m_time = htonl((int)historyInfo->GetTime());
				listAnswer->m_nicenameLen = htonl(strlen(historyInfo->GetName()) + 1);

				if (historyInfo->GetKind() == HistoryInfo::hkNzb)
				{
					NzbInfo* nzbInfo = historyInfo->GetNzbInfo();
					uint32 sizeHi, sizeLo;
					Util::SplitInt64(nzbInfo->GetSize(), &sizeHi, &sizeLo);
					listAnswer->m_sizeLo = htonl(sizeLo);
					listAnswer->m_sizeHi = htonl(sizeHi);
					listAnswer->m_fileCount = htonl(nzbInfo->GetFileCount());
					listAnswer->m_parStatus = htonl(nzbInfo->GetParStatus());
					listAnswer->m_scriptStatus = htonl(nzbInfo->GetScriptStatuses()->CalcTotalStatus());
				}
				else if (historyInfo->GetKind() == HistoryInfo::hkDup && showHidden)
				{
					DupInfo* dupInfo = historyInfo->GetDupInfo();
					uint32 sizeHi, sizeLo;
					Util::SplitInt64(dupInfo->GetSize(), &sizeHi, &sizeLo);
					listAnswer->m_sizeLo = htonl(sizeLo);
					listAnswer->m_sizeHi = htonl(sizeHi);
				}
				else if (historyInfo->GetKind() == HistoryInfo::hkUrl)
				{
					NzbInfo* nzbInfo = historyInfo->GetNzbInfo();
					listAnswer->m_urlStatus = htonl(nzbInfo->GetUrlStatus());
				}

				bufptr += sizeof(SNzbHistoryResponseEntry);
				strcpy(bufptr, historyInfo->GetName());
				bufptr += ntohl(listAnswer->m_nicenameLen);
				// align struct to 4-bytes, needed by ARM-processor (and may be others)
				if ((size_t)bufptr % 4 > 0)
				{
					listAnswer->m_nicenameLen = htonl(ntohl(listAnswer->m_nicenameLen) + 4 - (size_t)bufptr % 4);
					memset(bufptr, 0, 4 - (size_t)bufptr % 4); //suppress valgrind warning "uninitialized data"
					bufptr += 4 - (size_t)bufptr % 4;
				}
			}
		}

		HistoryResponse.m_nrTrailingEntries = htonl(nrEntries);
		HistoryResponse.m_trailingDataLength = htonl(bufsize);
	}

	// Send the request answer
	m_connection->Send((char*) &HistoryResponse, sizeof(HistoryResponse));

	// Send the data
	if (bufsize > 0)
	{
		m_connection->Send(buf, bufsize);
	}
}
