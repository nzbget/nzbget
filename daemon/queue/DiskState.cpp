/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
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
#include "NString.h"
#include "DiskState.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"
#include "FileSystem.h"

static const char* FORMATVERSION_SIGNATURE = "nzbget diskstate file version ";
const int DISKSTATE_QUEUE_VERSION = 62;
const int DISKSTATE_FILE_VERSION = 6;
const int DISKSTATE_STATS_VERSION = 3;
const int DISKSTATE_FEEDS_VERSION = 3;

class StateDiskFile : public DiskFile
{
public:
	int64 PrintLine(const char* format, ...) PRINTF_SYNTAX(2);
	char* ReadLine(char* buffer, int64 size);
	int ScanLine(const char* format, ...) SCANF_SYNTAX(2);
};


int64 StateDiskFile::PrintLine(const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	BString<1024> str;
	int len = str.FormatV(format, ap);
	va_end(ap);

	// replacing terminating <NULL> with <LF>
	str[len++] = '\n';

	Write(*str, len);

	return len;
}

char* StateDiskFile::ReadLine(char* buffer, int64 size)
{
	if (!DiskFile::ReadLine(buffer, size))
	{
		return nullptr;
	}

	// remove traling '\n'
	if (*buffer)
	{
		if (buffer[strlen(buffer) - 1] != '\n')
		{
			// the line is longer than "size", scroll file position to the end of the line
			for (char skipbuf[1024]; DiskFile::ReadLine(skipbuf, 1024) && *skipbuf && skipbuf[strlen(skipbuf) - 1] != '\n'; ) ;
		}

		buffer[strlen(buffer) - 1] = 0;
	}

	return buffer;
}

/*
* Standard "fscanf" scans beoynd current line if the next line is empty.
* This wrapper fixes that.
*/
int StateDiskFile::ScanLine(const char* format, ...)
{
	char line[1024];
	if (!ReadLine(line, sizeof(line)))
	{
		return 0;
	}

	va_list ap;
	va_start(ap, format);
	int res = vsscanf(line, format, ap);
	va_end(ap);

	return res;
}


class StateFile
{
public:
	StateFile(const char* filename, int formatVersion, bool transactional);
	void Discard();
	bool FileExists();
	StateDiskFile* BeginWrite();
	bool FinishWrite();
	StateDiskFile* BeginRead();
	int GetFileVersion() { return m_fileVersion; }
	const char* GetDestFilename() { return m_destFilename; }

private:
	BString<1024> m_destFilename;
	BString<1024> m_tempFilename;
	int m_formatVersion;
	bool m_transactional;
	int m_fileVersion;
	StateDiskFile m_file;

	int ParseFormatVersion(const char* formatSignature);
};


StateFile::StateFile(const char* filename, int formatVersion, bool transactional) :
	m_formatVersion(formatVersion), m_transactional(transactional)
{
	m_destFilename.Format("%s%c%s", g_Options->GetQueueDir(), PATH_SEPARATOR, filename);
	if (m_transactional)
	{
		m_tempFilename.Format("%s%c%s.new", g_Options->GetQueueDir(), PATH_SEPARATOR, filename);
	}
	else
	{
		m_tempFilename = *m_destFilename;
	}
}

void StateFile::Discard()
{
	FileSystem::DeleteFile(m_destFilename);
}

/* Parse signature and return format version number
*/
int StateFile::ParseFormatVersion(const char* formatSignature)
{
	if (strncmp(formatSignature, FORMATVERSION_SIGNATURE, strlen(FORMATVERSION_SIGNATURE)))
	{
		return 0;
	}

	return atoi(formatSignature + strlen(FORMATVERSION_SIGNATURE));
}

bool StateFile::FileExists()
{
	return FileSystem::FileExists(m_destFilename) || (m_transactional && FileSystem::FileExists(m_tempFilename));
}

StateDiskFile* StateFile::BeginWrite()
{
	if (!m_file.Open(m_tempFilename, StateDiskFile::omWrite))
	{
		error("Error saving diskstate: Could not create file %s: %s", *m_tempFilename,
			*FileSystem::GetLastErrorMessage());
		return nullptr;
	}

	m_file.PrintLine("%s%i", FORMATVERSION_SIGNATURE, m_formatVersion);

	return &m_file;
}

bool StateFile::FinishWrite()
{
	if (!m_transactional)
	{
		m_file.Close();
		return true;
	}

	// flush file content before renaming
	if (g_Options->GetFlushQueue())
	{
		debug("Flushing data for file %s", FileSystem::BaseFileName(m_tempFilename));
		m_file.Flush();
		CString errmsg;
		if (!m_file.Sync(errmsg))
		{
			warn("Could not flush file %s into disk: %s", *m_tempFilename, *errmsg);
		}
	}

	m_file.Close();

	// now rename to dest file name
	FileSystem::DeleteFile(m_destFilename);
	if (!FileSystem::MoveFile(m_tempFilename, m_destFilename))
	{
		error("Error saving diskstate: Could not rename file %s to %s: %s",
			*m_tempFilename, *m_destFilename, *FileSystem::GetLastErrorMessage());
		return false;
	}

	// flush directory buffer after renaming
	if (g_Options->GetFlushQueue())
	{
		debug("Flushing directory for file %s", FileSystem::BaseFileName(m_destFilename));
		CString errmsg;
		if (!FileSystem::FlushDirBuffers(m_destFilename, errmsg))
		{
			warn("Could not flush directory buffers for file %s into disk: %s", *m_destFilename, *errmsg);
		}
	}

	return true;
}

StateDiskFile* StateFile::BeginRead()
{
	if (!FileSystem::FileExists(m_destFilename) && FileSystem::FileExists(m_tempFilename))
	{
		// disaster recovery: temp-file exists but the dest-file doesn't
		warn("Restoring diskstate file %s from %s", FileSystem::BaseFileName(m_destFilename), FileSystem::BaseFileName(m_tempFilename));
		if (!FileSystem::MoveFile(m_tempFilename, m_destFilename))
		{
			error("Error restoring diskstate: Could not rename file %s to %s: %s",
				*m_tempFilename, *m_destFilename, *FileSystem::GetLastErrorMessage());
			return nullptr;
		}
	}

	if (!m_file.Open(m_destFilename, StateDiskFile::omRead))
	{
		error("Error reading diskstate: could not open file %s: %s", *m_destFilename,
			*FileSystem::GetLastErrorMessage());
		return nullptr;
	}

	char FileSignatur[128];
	m_file.ReadLine(FileSignatur, sizeof(FileSignatur));
	m_fileVersion = ParseFormatVersion(FileSignatur);
	if (m_fileVersion > m_formatVersion)
	{
		error("Could not load diskstate file %s due to file version mismatch", *m_destFilename);
		m_file.Close();
		return nullptr;
	}

	return &m_file;
}


/* Save Download Queue to Disk.
 * The Disk State consists of file "queue", which contains the order of files,
 * and of one diskstate-file for each file in download queue.
 * This function saves file "queue" and files with NZB-info. It does not
 * save file-infos.
 */
bool DiskState::SaveDownloadQueue(DownloadQueue* downloadQueue, bool saveHistory)
{
	debug("Saving queue and history to disk");

	bool ok = true;

	{
		StateFile stateFile("queue", DISKSTATE_QUEUE_VERSION, true);
		if (!downloadQueue->GetQueue()->empty())
		{
			StateDiskFile* outfile = stateFile.BeginWrite();
			if (!outfile)
			{
				return false;
			}

			// save nzb-infos
			SaveQueue(downloadQueue->GetQueue(), *outfile);

			// now rename to dest file name
			ok = stateFile.FinishWrite();
		}
		else
		{
			stateFile.Discard();
		}
	}

	if (saveHistory)
	{
		StateFile stateFile("history", DISKSTATE_QUEUE_VERSION, true);
		if (!downloadQueue->GetHistory()->empty())
		{
			StateDiskFile* outfile = stateFile.BeginWrite();
			if (!outfile)
			{
				return false;
			}

			// save history
			SaveHistory(downloadQueue->GetHistory(), *outfile);

			// now rename to dest file name
			ok &= stateFile.FinishWrite();
		}
		else
		{
			stateFile.Discard();
		}
	}

	// progress-file isn't needed after saving of full queue data
	StateFile progressStateFile("progress", DISKSTATE_QUEUE_VERSION, true);
	progressStateFile.Discard();

	return ok;
}

bool DiskState::LoadDownloadQueue(DownloadQueue* downloadQueue, Servers* servers)
{
	debug("Loading queue from disk");

	bool ok = false;
	int formatVersion = 0;

	{
		StateFile stateFile("queue", DISKSTATE_QUEUE_VERSION, true);
		if (stateFile.FileExists())
		{
			StateDiskFile* infile = stateFile.BeginRead();
			if (!infile)
			{
				return false;
			}

			formatVersion = stateFile.GetFileVersion();

			if (formatVersion <= 0)
			{
				error("Failed to read queue: diskstate file is corrupted");
				goto error;
			}
			else if (formatVersion < 47)
			{
				error("Failed to read queue and history data. Only queue and history from NZBGet v13 or newer can be converted by this NZBGet version. "
					"Old queue and history data still can be converted using NZBGet v16 as an intermediate version.");
				goto error;
			}

			if (!LoadQueue(downloadQueue->GetQueue(), servers, *infile, formatVersion)) goto error;

			if (formatVersion < 57)
			{
				if (!LoadHistory(downloadQueue->GetHistory(), servers, *infile, formatVersion)) goto error;
			}
		}
	}

	{
		StateFile stateFile("progress", DISKSTATE_QUEUE_VERSION, true);
		if (stateFile.FileExists())
		{
			StateDiskFile* infile = stateFile.BeginRead();
			if (!infile)
			{
				return false;
			}

			if (stateFile.GetFileVersion() <= 0)
			{
				error("Failed to read queue: diskstate file is corrupted");
				goto error;
			}

			if (!LoadProgress(downloadQueue->GetQueue(), servers, *infile, stateFile.GetFileVersion())) goto error;
		}
	}

	if (formatVersion == 0 || formatVersion >= 57)
	{
		StateFile stateFile("history", DISKSTATE_QUEUE_VERSION, true);
		if (stateFile.FileExists())
		{
			StateDiskFile* infile = stateFile.BeginRead();
			if (!infile)
			{
				return false;
			}

			if (stateFile.GetFileVersion() <= 0)
			{
				error("Failed to read queue: diskstate file is corrupted");
				goto error;
			}

			if (!LoadHistory(downloadQueue->GetHistory(), servers, *infile, stateFile.GetFileVersion())) goto error;
		}
	}

	LoadAllFileInfos(downloadQueue);

	CleanupQueueDir(downloadQueue);

	if (!LoadAllFileStates(downloadQueue, servers)) goto error;

	ok = true;

error:

	if (!ok)
	{
		error("Error reading diskstate for download queue and history");
	}

	NzbInfo::ResetGenId(true);
	FileInfo::ResetGenId(true);

	CalcFileStats(downloadQueue, formatVersion);

	return ok;
}

bool DiskState::SaveDownloadProgress(DownloadQueue* downloadQueue)
{
	int count = 0;
	for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
	{
		count += nzbInfo->GetChanged() ? 1 : 0;
	}

	debug("Saving queue progress to disk");

	bool ok = true;

	{
		StateFile stateFile("progress", DISKSTATE_QUEUE_VERSION, true);
		if (count > 0)
		{
			StateDiskFile* outfile = stateFile.BeginWrite();
			if (!outfile)
			{
				return false;
			}

			SaveProgress(downloadQueue->GetQueue(), *outfile, count);

			// now rename to dest file name
			ok = stateFile.FinishWrite();
		}
		else
		{
			stateFile.Discard();
		}
	}

	return ok;
}

void DiskState::SaveQueue(NzbList* queue, StateDiskFile& outfile)
{
	debug("Saving nzb list to disk");

	outfile.PrintLine("%i", (int)queue->size());
	for (NzbInfo* nzbInfo : queue)
	{
		SaveNzbInfo(nzbInfo, outfile);
	}
}

bool DiskState::LoadQueue(NzbList* queue, Servers* servers, StateDiskFile& infile, int formatVersion)
{
	debug("Loading nzb list from disk");

	// load nzb-infos
	int size;
	if (infile.ScanLine("%i", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		std::unique_ptr<NzbInfo> nzbInfo = std::make_unique<NzbInfo>();
		if (!LoadNzbInfo(nzbInfo.get(), servers, infile, formatVersion)) goto error;
		queue->push_back(std::move(nzbInfo));
	}

	return true;

error:
	error("Error reading nzb list from disk");
	return false;
}

void DiskState::SaveProgress(NzbList* queue, StateDiskFile& outfile, int changedCount)
{
	debug("Saving nzb progress to disk");

	outfile.PrintLine("%i", changedCount);
	for (NzbInfo* nzbInfo : queue)
	{
		if (nzbInfo->GetChanged())
		{
			outfile.PrintLine("%i", nzbInfo->GetId());
			SaveNzbInfo(nzbInfo, outfile);
		}
	}
}

bool DiskState::LoadProgress(NzbList* queue, Servers* servers, StateDiskFile& infile, int formatVersion)
{
	debug("Loading nzb progress from disk");

	// load nzb-infos
	int size;
	if (infile.ScanLine("%i", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		int id;
		if (infile.ScanLine("%i", &id) != 1) goto error;

		NzbInfo* nzbInfo = queue->Find(id);
		if (!nzbInfo)
		{
			error("NZB with id %i could not be found", id);
			goto error;
		}

		if (!LoadNzbInfo(nzbInfo, servers, infile, formatVersion)) goto error;
	}

	return true;

error:
	error("Error reading nzb progress from disk");
	return false;
}

void DiskState::SaveNzbInfo(NzbInfo* nzbInfo, StateDiskFile& outfile)
{
	outfile.PrintLine("%i", nzbInfo->GetId());
	outfile.PrintLine("%i", (int)nzbInfo->GetKind());
	outfile.PrintLine("%s", nzbInfo->GetUrl());
	outfile.PrintLine("%s", nzbInfo->GetFilename());
	outfile.PrintLine("%s", nzbInfo->GetDestDir());
	outfile.PrintLine("%s", nzbInfo->GetFinalDir());
	outfile.PrintLine("%s", nzbInfo->GetQueuedFilename());
	outfile.PrintLine("%s", nzbInfo->GetName());
	outfile.PrintLine("%s", nzbInfo->GetCategory());
	outfile.PrintLine("%i,%i,%i,%i,%i", (int)nzbInfo->GetPriority(),
		nzbInfo->GetPostInfo() ? (int)nzbInfo->GetPostInfo()->GetStage() + 1 : 0,
		(int)nzbInfo->GetDeletePaused(), (int)nzbInfo->GetManyDupeFiles(), nzbInfo->GetFeedId());
	outfile.PrintLine("%i,%i,%i,%i,%i,%i,%i,%i,%i", (int)nzbInfo->GetParStatus(), (int)nzbInfo->GetUnpackStatus(),
		(int)nzbInfo->GetMoveStatus(), (int)nzbInfo->GetParRenameStatus(), (int)nzbInfo->GetRarRenameStatus(),
		(int)nzbInfo->GetDirectRenameStatus(), (int)nzbInfo->GetDeleteStatus(), (int)nzbInfo->GetMarkStatus(),
		(int)nzbInfo->GetUrlStatus());
	outfile.PrintLine("%i,%i,%i", (int)nzbInfo->GetUnpackCleanedUpDisk(), (int)nzbInfo->GetHealthPaused(),
		(int)nzbInfo->GetAddUrlPaused());
	outfile.PrintLine("%i,%i,%i", nzbInfo->GetFileCount(), nzbInfo->GetParkedFileCount(),
		nzbInfo->GetMessageCount());
	outfile.PrintLine("%i,%i", (int)nzbInfo->GetMinTime(), (int)nzbInfo->GetMaxTime());
	outfile.PrintLine("%i,%i,%i,%i", (int)nzbInfo->GetParFull(),
		nzbInfo->GetPostInfo() ? (int)nzbInfo->GetPostInfo()->GetForceParFull() : 0,
		nzbInfo->GetPostInfo() ? (int)nzbInfo->GetPostInfo()->GetForceRepair() : 0,
		nzbInfo->GetExtraParBlocks());

	outfile.PrintLine("%u,%u", nzbInfo->GetFullContentHash(), nzbInfo->GetFilteredContentHash());

	uint32 High1, Low1, High2, Low2, High3, Low3;
	Util::SplitInt64(nzbInfo->GetSize(), &High1, &Low1);
	Util::SplitInt64(nzbInfo->GetSuccessSize(), &High2, &Low2);
	Util::SplitInt64(nzbInfo->GetFailedSize(), &High3, &Low3);
	outfile.PrintLine("%u,%u,%u,%u,%u,%u", High1, Low1, High2, Low2, High3, Low3);

	Util::SplitInt64(nzbInfo->GetParSize(), &High1, &Low1);
	Util::SplitInt64(nzbInfo->GetParSuccessSize(), &High2, &Low2);
	Util::SplitInt64(nzbInfo->GetParFailedSize(), &High3, &Low3);
	outfile.PrintLine("%u,%u,%u,%u,%u,%u", High1, Low1, High2, Low2, High3, Low3);

	outfile.PrintLine("%i,%i,%i", nzbInfo->GetTotalArticles(), nzbInfo->GetSuccessArticles(), nzbInfo->GetFailedArticles());

	outfile.PrintLine("%s", nzbInfo->GetDupeKey());
	outfile.PrintLine("%i,%i,%i", (int)nzbInfo->GetDupeMode(), nzbInfo->GetDupeScore(), (int)nzbInfo->GetDupeHint());

	Util::SplitInt64(nzbInfo->GetDownloadedSize(), &High1, &Low1);
	outfile.PrintLine("%u,%u,%i,%i,%i,%i,%i", High1, Low1, nzbInfo->GetDownloadSec(), nzbInfo->GetPostTotalSec(),
		nzbInfo->GetParSec(), nzbInfo->GetRepairSec(), nzbInfo->GetUnpackSec());

	outfile.PrintLine("%i", (int)nzbInfo->GetCompletedFiles()->size());
	for (CompletedFile& completedFile : nzbInfo->GetCompletedFiles())
	{
		outfile.PrintLine("%i,%i,%u,%i,%s,%s", completedFile.GetId(), (int)completedFile.GetStatus(),
			completedFile.GetCrc(), (int)completedFile.GetParFile(),
			completedFile.GetHash16k() ? completedFile.GetHash16k() : "",
			completedFile.GetParSetId() ? completedFile.GetParSetId() : "");
		outfile.PrintLine("%s", completedFile.GetFilename());
		outfile.PrintLine("%s", completedFile.GetOrigname() ? completedFile.GetOrigname() : "");
	}

	outfile.PrintLine("%i", (int)nzbInfo->GetParameters()->size());
	for (NzbParameter& parameter : nzbInfo->GetParameters())
	{
		outfile.PrintLine("%s=%s", parameter.GetName(), parameter.GetValue());
	}

	outfile.PrintLine("%i", (int)nzbInfo->GetScriptStatuses()->size());
	for (ScriptStatus& scriptStatus : nzbInfo->GetScriptStatuses())
	{
		outfile.PrintLine("%i,%s", scriptStatus.GetStatus(), scriptStatus.GetName());
	}

	SaveServerStats(nzbInfo->GetServerStats(), outfile);

	// save file-infos
	int size = 0;
	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
		if (!fileInfo->GetDeleted())
		{
			size++;
		}
	}
	outfile.PrintLine("%i", size);
	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
		if (!fileInfo->GetDeleted())
		{
			outfile.PrintLine("%i,%i,%i", fileInfo->GetId(), (int)fileInfo->GetPaused(),
				(int)fileInfo->GetExtraPriority());
		}
	}
}

bool DiskState::LoadNzbInfo(NzbInfo* nzbInfo, Servers* servers, StateDiskFile& infile, int formatVersion)
{
	char buf[10240];

	int id;
	if (infile.ScanLine("%i", &id) != 1) goto error;
	nzbInfo->SetId(id);

	int kind;
	if (infile.ScanLine("%i", &kind) != 1) goto error;
	nzbInfo->SetKind((NzbInfo::EKind)kind);

	if (!infile.ReadLine(buf, sizeof(buf))) goto error;
	nzbInfo->SetUrl(buf);

	if (!infile.ReadLine(buf, sizeof(buf))) goto error;
	nzbInfo->SetFilename(buf);

	if (!infile.ReadLine(buf, sizeof(buf))) goto error;
	nzbInfo->SetDestDir(buf);

	if (!infile.ReadLine(buf, sizeof(buf))) goto error;
	nzbInfo->SetFinalDir(buf);

	if (!infile.ReadLine(buf, sizeof(buf))) goto error;
	nzbInfo->SetQueuedFilename(buf);

	if (!infile.ReadLine(buf, sizeof(buf))) goto error;
	if (strlen(buf) > 0)
	{
		nzbInfo->SetName(buf);
	}

	if (!infile.ReadLine(buf, sizeof(buf))) goto error;
	nzbInfo->SetCategory(buf);

	int priority, postStage, deletePaused, manyDupeFiles, feedId;
	if (formatVersion >= 54)
	{
		if (infile.ScanLine("%i,%i,%i,%i,%i", &priority, &postStage, &deletePaused, &manyDupeFiles, &feedId) != 5) goto error;
	}
	else
	{
		if (infile.ScanLine("%i,%i,%i,%i", &priority, &postStage, &deletePaused, &manyDupeFiles) != 4) goto error;
		feedId = 0;
	}
	nzbInfo->SetPriority(priority);
	nzbInfo->SetDeletePaused((bool)deletePaused);
	nzbInfo->SetManyDupeFiles((bool)manyDupeFiles);
	if (postStage > 0)
	{
		nzbInfo->EnterPostProcess();
		if (formatVersion < 59 && postStage == 6)
		{
			postStage++;
		}
		else if (formatVersion < 59 && postStage > 6)
		{
			postStage += 2;
		}
		nzbInfo->GetPostInfo()->SetStage((PostInfo::EStage)postStage);
	}
	nzbInfo->SetFeedId(feedId);

	int parStatus, unpackStatus, moveStatus, parRenameStatus, rarRenameStatus,
		directRenameStatus, deleteStatus, markStatus, urlStatus;
	if (formatVersion >= 60)
	{
		if (infile.ScanLine("%i,%i,%i,%i,%i,%i,%i,%i,%i", &parStatus,
			&unpackStatus, &moveStatus, &parRenameStatus, &rarRenameStatus, &directRenameStatus,
			&deleteStatus, &markStatus, &urlStatus) != 9) goto error;
	}
	else if (formatVersion >= 58)
	{
		directRenameStatus = 0;
		if (infile.ScanLine("%i,%i,%i,%i,%i,%i,%i,%i", &parStatus,
			&unpackStatus, &moveStatus, &parRenameStatus, &rarRenameStatus, &deleteStatus,
			&markStatus, &urlStatus) != 8) goto error;
	}
	else
	{
		rarRenameStatus = directRenameStatus = 0;
		if (infile.ScanLine("%i,%i,%i,%i,%i,%i,%i", &parStatus, &unpackStatus, &moveStatus,
			&parRenameStatus, &deleteStatus, &markStatus, &urlStatus) != 7) goto error;
	}
	nzbInfo->SetParStatus((NzbInfo::EParStatus)parStatus);
	nzbInfo->SetUnpackStatus((NzbInfo::EPostUnpackStatus)unpackStatus);
	nzbInfo->SetMoveStatus((NzbInfo::EMoveStatus)moveStatus);
	nzbInfo->SetParRenameStatus((NzbInfo::EPostRenameStatus)parRenameStatus);
	nzbInfo->SetRarRenameStatus((NzbInfo::EPostRenameStatus)rarRenameStatus);
	nzbInfo->SetDirectRenameStatus((NzbInfo::EDirectRenameStatus)directRenameStatus);
	nzbInfo->SetDeleteStatus((NzbInfo::EDeleteStatus)deleteStatus);
	nzbInfo->SetMarkStatus((NzbInfo::EMarkStatus)markStatus);
	if (nzbInfo->GetKind() == NzbInfo::nkNzb ||
		(NzbInfo::EUrlStatus)urlStatus >= NzbInfo::lsFailed ||
		(NzbInfo::EUrlStatus)urlStatus >= NzbInfo::lsScanSkipped)
	{
		nzbInfo->SetUrlStatus((NzbInfo::EUrlStatus)urlStatus);
	}

	int unpackCleanedUpDisk, healthPaused, addUrlPaused;
	if (infile.ScanLine("%i,%i,%i", &unpackCleanedUpDisk, &healthPaused, &addUrlPaused) != 3) goto error;
	nzbInfo->SetUnpackCleanedUpDisk((bool)unpackCleanedUpDisk);
	nzbInfo->SetHealthPaused((bool)healthPaused);
	nzbInfo->SetAddUrlPaused((bool)addUrlPaused);

	int fileCount, parkedFileCount, messageCount;
	if (formatVersion >= 52)
	{
		if (infile.ScanLine("%i,%i,%i", &fileCount, &parkedFileCount, &messageCount) != 3) goto error;
	}
	else
	{
		if (infile.ScanLine("%i,%i", &fileCount, &parkedFileCount) != 2) goto error;
		messageCount = 0;
	}
	nzbInfo->SetFileCount(fileCount);
	nzbInfo->SetParkedFileCount(parkedFileCount);
	nzbInfo->SetMessageCount(messageCount);

	int minTime, maxTime;
	if (infile.ScanLine("%i,%i", &minTime, &maxTime) != 2) goto error;
	nzbInfo->SetMinTime((time_t)minTime);
	nzbInfo->SetMaxTime((time_t)maxTime);

	if (formatVersion >= 51)
	{
		int parFull, forceParFull, forceRepair, extraParBlocks = 0;
		if (formatVersion >= 55)
		{
			if (infile.ScanLine("%i,%i,%i,%i", &parFull, &forceParFull, &forceRepair, &extraParBlocks) != 4) goto error;
		}
		else
		{
			if (infile.ScanLine("%i,%i,%i", &parFull, &forceParFull, &forceRepair) != 3) goto error;
		}
		nzbInfo->SetParFull((bool)parFull);
		nzbInfo->SetExtraParBlocks(extraParBlocks);
		if (nzbInfo->GetPostInfo())
		{
			nzbInfo->GetPostInfo()->SetForceParFull((bool)forceParFull);
			nzbInfo->GetPostInfo()->SetForceRepair((bool)forceRepair);
		}
	}

	uint32 fullContentHash, filteredContentHash;
	if (infile.ScanLine("%u,%u", &fullContentHash, &filteredContentHash) != 2) goto error;
	nzbInfo->SetFullContentHash(fullContentHash);
	nzbInfo->SetFilteredContentHash(filteredContentHash);

	uint32 High1, Low1, High2, Low2, High3, Low3;
	if (infile.ScanLine("%u,%u,%u,%u,%u,%u", &High1, &Low1, &High2, &Low2, &High3, &Low3) != 6) goto error;
	nzbInfo->SetSize(Util::JoinInt64(High1, Low1));
	nzbInfo->SetSuccessSize(Util::JoinInt64(High2, Low2));
	nzbInfo->SetFailedSize(Util::JoinInt64(High3, Low3));
	nzbInfo->SetCurrentSuccessSize(nzbInfo->GetSuccessSize());
	nzbInfo->SetCurrentFailedSize(nzbInfo->GetFailedSize());

	if (infile.ScanLine("%u,%u,%u,%u,%u,%u", &High1, &Low1, &High2, &Low2, &High3, &Low3) != 6) goto error;
	nzbInfo->SetParSize(Util::JoinInt64(High1, Low1));
	nzbInfo->SetParSuccessSize(Util::JoinInt64(High2, Low2));
	nzbInfo->SetParFailedSize(Util::JoinInt64(High3, Low3));
	nzbInfo->SetParCurrentSuccessSize(nzbInfo->GetParSuccessSize());
	nzbInfo->SetParCurrentFailedSize(nzbInfo->GetParFailedSize());

	int totalArticles, successArticles, failedArticles;
	if (infile.ScanLine("%i,%i,%i", &totalArticles, &successArticles, &failedArticles) != 3) goto error;
	nzbInfo->SetTotalArticles(totalArticles);
	nzbInfo->SetSuccessArticles(successArticles);
	nzbInfo->SetFailedArticles(failedArticles);
	nzbInfo->SetCurrentSuccessArticles(successArticles);
	nzbInfo->SetCurrentFailedArticles(failedArticles);

	if (!infile.ReadLine(buf, sizeof(buf))) goto error;
	nzbInfo->SetDupeKey(buf);

	int dupeMode, dupeScore, dupeHint;
	dupeHint = 0; //clang requires initialization in a separate line (due to goto statements)
	if (formatVersion >= 61)
	{
		if (infile.ScanLine("%i,%i,%i", &dupeMode, &dupeScore, &dupeHint) != 3) goto error;
	}
	else
	{
		if (infile.ScanLine("%i,%i", &dupeMode, &dupeScore) != 2) goto error;
	}
	nzbInfo->SetDupeMode((EDupeMode)dupeMode);
	nzbInfo->SetDupeScore(dupeScore);
	nzbInfo->SetDupeMode((EDupeMode)dupeHint);

	if (formatVersion >= 48)
	{
		uint32 High1, Low1, downloadSec, postTotalSec, parSec, repairSec, unpackSec;
		if (infile.ScanLine("%u,%u,%i,%i,%i,%i,%i", &High1, &Low1, &downloadSec, &postTotalSec, &parSec, &repairSec, &unpackSec) != 7) goto error;
		nzbInfo->SetDownloadedSize(Util::JoinInt64(High1, Low1));
		nzbInfo->SetDownloadSec(downloadSec);
		nzbInfo->SetPostTotalSec(postTotalSec);
		nzbInfo->SetParSec(parSec);
		nzbInfo->SetRepairSec(repairSec);
		nzbInfo->SetUnpackSec(unpackSec);
	}

	nzbInfo->GetCompletedFiles()->clear();
	if (infile.ScanLine("%i", &fileCount) != 1) goto error;
	for (int i = 0; i < fileCount; i++)
	{
		if (!infile.ReadLine(buf, sizeof(buf))) goto error;

		int id = 0;
		char* fileName = buf;
		int status = 0;
		uint32 crc = 0;
		int parFile = 0;
		char* hash16k = nullptr;
		char* parSetId = nullptr;
		char filenameBuf[1024];
		char origName[1024];

		if (formatVersion >= 49)
		{
			if (formatVersion >= 60)
			{
				if (sscanf(buf, "%i,%i,%u,%i", &id, &status, &crc, &parFile) != 4) goto error;
				hash16k = strchr(buf, ',');
				if (hash16k) hash16k = strchr(hash16k+1, ',');
				if (hash16k) hash16k = strchr(hash16k+1, ',');
				if (hash16k) hash16k = strchr(hash16k+1, ',');
				if (hash16k)
				{
					parSetId = strchr(++hash16k, ',');
					if (parSetId)
					{
						*parSetId++ = '\0';
						fileName = strchr(parSetId, ',');
						if (fileName) *fileName = '\0';
					}
				}
			}
			else if (formatVersion >= 50)
			{
				if (sscanf(buf, "%i,%i,%u", &id, &status, &crc) != 3) goto error;
				fileName = strchr(buf, ',');
				if (fileName) fileName = strchr(fileName+1, ',');
				if (fileName) fileName = strchr(fileName+1, ',');
			}
			else
			{
				if (sscanf(buf, "%i,%u", &status, &crc) != 2) goto error;
				fileName = strchr(buf + 2, ',');
			}
			if (fileName)
			{
				fileName++;
			}
			if (formatVersion >= 62)
			{
				if (!infile.ReadLine(filenameBuf, sizeof(filenameBuf))) goto error;
				fileName = filenameBuf;
				if (!infile.ReadLine(origName, sizeof(origName))) goto error;
			}
		}

		nzbInfo->GetCompletedFiles()->emplace_back(id, fileName,
			Util::EmptyStr(origName) ? nullptr : origName,
			(CompletedFile::EStatus)status, crc, (bool)parFile,
			Util::EmptyStr(hash16k) ? nullptr : hash16k,
			Util::EmptyStr(parSetId) ? nullptr : parSetId);
	}

	nzbInfo->GetParameters()->clear();
	int parameterCount;
	if (infile.ScanLine("%i", &parameterCount) != 1) goto error;
	for (int i = 0; i < parameterCount; i++)
	{
		if (!infile.ReadLine(buf, sizeof(buf))) goto error;

		char* value = strchr(buf, '=');
		if (value)
		{
			*value = '\0';
			value++;
			nzbInfo->GetParameters()->SetParameter(buf, value);
		}
	}

	nzbInfo->GetScriptStatuses()->clear();
	int scriptCount;
	if (infile.ScanLine("%i", &scriptCount) != 1) goto error;
	for (int i = 0; i < scriptCount; i++)
	{
		if (!infile.ReadLine(buf, sizeof(buf))) goto error;

		char* scriptName = strchr(buf, ',');
		if (scriptName)
		{
			scriptName++;
			int status = atoi(buf);
			if (status > 1 && formatVersion < 25) status--;
			nzbInfo->GetScriptStatuses()->emplace_back(scriptName, (ScriptStatus::EStatus)status);
		}
	}

	if (!LoadServerStats(nzbInfo->GetServerStats(), servers, infile)) goto error;
	nzbInfo->GetCurrentServerStats()->ListOp(nzbInfo->GetServerStats(), ServerStatList::soSet);

	if (formatVersion < 52)
	{
		int logCount;
		if (infile.ScanLine("%i", &logCount) != 1) goto error;
		for (int i = 0; i < logCount; i++)
		{
			if (!infile.ReadLine(buf, sizeof(buf))) goto error;
		}
	}

	nzbInfo->GetFileList()->clear();
	if (infile.ScanLine("%i", &fileCount) != 1) goto error;
	for (int i = 0; i < fileCount; i++)
	{
		uint32 id, paused, time;
		int extraPriority;

		if (formatVersion >= 56)
		{
			if (infile.ScanLine("%i,%i,%i", &id, &paused, &extraPriority) != 3) goto error;
		}
		else
		{
			if (infile.ScanLine("%i,%i,%i,%i", &id, &paused, &time, &extraPriority) != 4) goto error;
		}

		std::unique_ptr<FileInfo> fileInfo = std::make_unique<FileInfo>();
		fileInfo->SetId(id);
		fileInfo->SetPaused(paused);
		if (formatVersion < 56)
		{
			fileInfo->SetTime(time);
		}
		fileInfo->SetExtraPriority((bool)extraPriority);
		fileInfo->SetNzbInfo(nzbInfo);
		nzbInfo->GetFileList()->Add(std::move(fileInfo));
	}

	return true;

error:
	error("Error reading nzb info from disk");
	return false;
}

void DiskState::SaveServerStats(ServerStatList* serverStatList, StateDiskFile& outfile)
{
	outfile.PrintLine("%i", (int)serverStatList->size());
	for (ServerStat& serverStat : serverStatList)
	{
		outfile.PrintLine("%i,%i,%i", serverStat.GetServerId(), serverStat.GetSuccessArticles(), serverStat.GetFailedArticles());
	}
}

bool DiskState::LoadServerStats(ServerStatList* serverStatList, Servers* servers, StateDiskFile& infile)
{
	int statCount;
	if (infile.ScanLine("%i", &statCount) != 1) goto error;
	for (int i = 0; i < statCount; i++)
	{
		int serverId, successArticles, failedArticles;
		if (infile.ScanLine("%i,%i,%i", &serverId, &successArticles, &failedArticles) != 3) goto error;

		if (servers)
		{
			// find server (id could change if config file was edited)
			for (NewsServer* newsServer : servers)
			{
				if (newsServer->GetStateId() == serverId)
				{
					serverStatList->StatOp(newsServer->GetId(), successArticles, failedArticles, ServerStatList::soSet);
				}
			}
		}
	}

	return true;

error:
	error("Error reading server stats from disk");
	return false;
}

bool DiskState::SaveFile(FileInfo* fileInfo)
{
	debug("Saving FileInfo %i to disk", fileInfo->GetId());

	BString<100> filename("%i", fileInfo->GetId());
	StateFile stateFile(filename, DISKSTATE_FILE_VERSION, false);

	StateDiskFile* outfile = stateFile.BeginWrite();
	if (!outfile)
	{
		return false;
	}

	return SaveFileInfo(fileInfo, *outfile, true) && stateFile.FinishWrite();
}

bool DiskState::SaveFileInfo(FileInfo* fileInfo, StateDiskFile& outfile, bool articles)
{
	outfile.PrintLine("%s", fileInfo->GetSubject());
	outfile.PrintLine("%s", fileInfo->GetFilename());
	outfile.PrintLine("%s", fileInfo->GetOrigname() ? fileInfo->GetOrigname() : "");

	outfile.PrintLine("%i,%i", (int)fileInfo->GetFilenameConfirmed(), (int)fileInfo->GetTime());

	uint32 High, Low;
	Util::SplitInt64(fileInfo->GetSize(), &High, &Low);
	outfile.PrintLine("%u,%u", High, Low);

	Util::SplitInt64(fileInfo->GetMissedSize(), &High, &Low);
	outfile.PrintLine("%u,%u", High, Low);

	outfile.PrintLine("%i", (int)fileInfo->GetParFile());
	outfile.PrintLine("%i,%i", fileInfo->GetTotalArticles(), fileInfo->GetMissedArticles());

	outfile.PrintLine("%i", (int)fileInfo->GetGroups()->size());
	for (CString& group : fileInfo->GetGroups())
	{
		outfile.PrintLine("%s", *group);
	}

	if (articles)
	{
		outfile.PrintLine("%i", (int)fileInfo->GetArticles()->size());
		for (ArticleInfo* articleInfo : fileInfo->GetArticles())
		{
			outfile.PrintLine("%i,%i", articleInfo->GetPartNumber(), articleInfo->GetSize());
			outfile.PrintLine("%s", articleInfo->GetMessageId());
		}
	}

	return true;
}

bool DiskState::LoadArticles(FileInfo* fileInfo)
{
	return LoadFile(fileInfo, false, true);
}

bool DiskState::LoadFile(FileInfo* fileInfo, bool fileSummary, bool articles)
{
	debug("Loading FileInfo %i from disk", fileInfo->GetId());

	BString<100> filename("%i", fileInfo->GetId());
	StateFile stateFile(filename, DISKSTATE_FILE_VERSION, false);

	StateDiskFile* infile = stateFile.BeginRead();
	if (!infile)
	{
		return false;
	}

	return LoadFileInfo(fileInfo, *infile, stateFile.GetFileVersion(), fileSummary, articles);
}

bool DiskState::LoadFileInfo(FileInfo* fileInfo, StateDiskFile& infile, int formatVersion, bool fileSummary, bool articles)
{
	char buf[1024];

	if (!infile.ReadLine(buf, sizeof(buf))) goto error;
	if (fileSummary) fileInfo->SetSubject(buf);

	if (!infile.ReadLine(buf, sizeof(buf))) goto error;
	if (fileSummary) fileInfo->SetFilename(buf);

	if (formatVersion >= 6)
	{
		if (!infile.ReadLine(buf, sizeof(buf))) goto error;
		if (fileSummary) fileInfo->SetOrigname(Util::EmptyStr(buf) ? nullptr : buf);
	}

	if (formatVersion >= 5)
	{
		int time, filenameConfirmed;
		if (infile.ScanLine("%i,%i", &filenameConfirmed, &time) != 2) goto error;
		if (fileSummary) fileInfo->SetFilenameConfirmed((bool)filenameConfirmed);
		if (fileSummary) fileInfo->SetTime((time_t)time);
	}
	else if (formatVersion >= 4)
	{
		int time;
		if (infile.ScanLine("%i", &time) != 1) goto error;
		if (fileSummary) fileInfo->SetTime((time_t)time);
	}

	uint32 High, Low;
	if (infile.ScanLine("%u,%u", &High, &Low) != 2) goto error;
	if (fileSummary) fileInfo->SetSize(Util::JoinInt64(High, Low));
	if (fileSummary) fileInfo->SetRemainingSize(fileInfo->GetSize());

	if (infile.ScanLine("%u,%u", &High, &Low) != 2) goto error;
	if (fileSummary) fileInfo->SetMissedSize(Util::JoinInt64(High, Low));
	if (fileSummary) fileInfo->SetRemainingSize(fileInfo->GetSize() - fileInfo->GetMissedSize());

	int parFile;
	if (infile.ScanLine("%i", &parFile) != 1) goto error;
	if (fileSummary) fileInfo->SetParFile((bool)parFile);

	int totalArticles, missedArticles;
	if (infile.ScanLine("%i,%i", &totalArticles, &missedArticles) != 2) goto error;
	if (fileSummary) fileInfo->SetTotalArticles(totalArticles);
	if (fileSummary) fileInfo->SetMissedArticles(missedArticles);

	int size;
	if (infile.ScanLine("%i", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		if (!infile.ReadLine(buf, sizeof(buf))) goto error;
		if (fileSummary) fileInfo->GetGroups()->push_back(buf);
	}

	if (articles)
	{
		if (infile.ScanLine("%i", &size) != 1) goto error;
		for (int i = 0; i < size; i++)
		{
			int PartNumber, PartSize;
			if (infile.ScanLine("%i,%i", &PartNumber, &PartSize) != 2) goto error;

			if (!infile.ReadLine(buf, sizeof(buf))) goto error;

			std::unique_ptr<ArticleInfo> articleInfo = std::make_unique<ArticleInfo>();
			articleInfo->SetPartNumber(PartNumber);
			articleInfo->SetSize(PartSize);
			articleInfo->SetMessageId(buf);
			fileInfo->GetArticles()->push_back(std::move(articleInfo));
		}
	}

	return true;

error:
	error("Error reading diskstate for file %i", fileInfo->GetId());
	return false;
}

bool DiskState::SaveFileState(FileInfo* fileInfo, bool completed)
{
	debug("Saving FileState %i to disk", fileInfo->GetId());

	BString<100> filename("%i%s", fileInfo->GetId(), completed ? "c" : "s");
	StateFile stateFile(filename, DISKSTATE_FILE_VERSION, false);

	StateDiskFile* outfile = stateFile.BeginWrite();
	if (!outfile)
	{
		return false;
	}

	return SaveFileState(fileInfo, *outfile, completed);
}

bool DiskState::SaveFileState(FileInfo* fileInfo, StateDiskFile& outfile, bool completed)
{
	outfile.PrintLine("%i,%i", fileInfo->GetSuccessArticles(), fileInfo->GetFailedArticles());

	uint32 High1, Low1, High2, Low2, High3, Low3;
	Util::SplitInt64(fileInfo->GetRemainingSize(), &High1, &Low1);
	Util::SplitInt64(fileInfo->GetSuccessSize(), &High2, &Low2);
	Util::SplitInt64(fileInfo->GetFailedSize(), &High3, &Low3);
	outfile.PrintLine("%u,%u,%u,%u,%u,%u", High1, Low1, High2, Low2, High3, Low3);

	outfile.PrintLine("%s", fileInfo->GetFilename());
	outfile.PrintLine("%s", fileInfo->GetHash16k() ? fileInfo->GetHash16k() : "");
	outfile.PrintLine("%s", fileInfo->GetParSetId() ? fileInfo->GetParSetId() : "");
	outfile.PrintLine("%i", (int)fileInfo->GetParFile());

	SaveServerStats(fileInfo->GetServerStats(), outfile);

	outfile.PrintLine("%i", (int)fileInfo->GetArticles()->size());
	for (ArticleInfo* articleInfo : fileInfo->GetArticles())
	{
		outfile.PrintLine("%i,%u,%i,%u", (int)articleInfo->GetStatus(), (uint32)articleInfo->GetSegmentOffset(),
			articleInfo->GetSegmentSize(), (uint32)articleInfo->GetCrc());
	}

	outfile.Close();
	return true;
}

bool DiskState::LoadFileState(FileInfo* fileInfo, Servers* servers, bool completed)
{
	debug("Loading FileInfo %i from disk", fileInfo->GetId());

	BString<100> filename("%i%s", fileInfo->GetId(), completed ? "c" : "s");
	StateFile stateFile(filename, DISKSTATE_FILE_VERSION, false);

	StateDiskFile* infile = stateFile.BeginRead();
	if (!infile)
	{
		return false;
	}

	return LoadFileState(fileInfo, servers, *infile, stateFile.GetFileVersion(), completed);
}

bool DiskState::LoadFileState(FileInfo* fileInfo, Servers* servers, StateDiskFile& infile, int formatVersion, bool completed)
{
	bool hasArticles = !fileInfo->GetArticles()->empty();

	int successArticles, failedArticles;
	if (infile.ScanLine("%i,%i", &successArticles, &failedArticles) != 2) goto error;
	fileInfo->SetSuccessArticles(successArticles);
	fileInfo->SetFailedArticles(failedArticles);

	uint32 High1, Low1, High2, Low2, High3, Low3;
	if (infile.ScanLine("%u,%u,%u,%u,%u,%u", &High1, &Low1, &High2, &Low2, &High3, &Low3) != 6) goto error;
	fileInfo->SetRemainingSize(Util::JoinInt64(High1, Low1));
	fileInfo->SetSuccessSize(Util::JoinInt64(High2, Low2));
	fileInfo->SetFailedSize(Util::JoinInt64(High3, Low3));

	char buf[1024];

	if (formatVersion >= 4)
	{
		if (!infile.ReadLine(buf, sizeof(buf))) goto error;
		fileInfo->SetFilename(buf);
	}

	if (formatVersion >= 5)
	{
		if (!infile.ReadLine(buf, sizeof(buf))) goto error;
		fileInfo->SetHash16k(*buf ? buf : nullptr);
		if (formatVersion >= 6)
		{
			if (!infile.ReadLine(buf, sizeof(buf))) goto error;
			fileInfo->SetParSetId(*buf ? buf : nullptr);
		}
		int parFile = 0;
		if (infile.ScanLine("%i", &parFile) != 1) goto error;
		fileInfo->SetParFile((bool)parFile);
	}

	if (!LoadServerStats(fileInfo->GetServerStats(), servers, infile)) goto error;

	int completedArticles;
	completedArticles = 0; //clang requires initialization in a separate line (due to goto statements)

	int size;
	if (infile.ScanLine("%i", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		if (!hasArticles)
		{
			fileInfo->GetArticles()->push_back(std::make_unique<ArticleInfo>());
		}
		std::unique_ptr<ArticleInfo>& pa = fileInfo->GetArticles()->at(i);

		int statusInt;

		if (formatVersion >= 2)
		{
			uint32 segmentOffset, crc;
			int segmentSize;
			if (infile.ScanLine("%i,%u,%i,%u", &statusInt, &segmentOffset, &segmentSize, &crc) != 4) goto error;
			pa->SetSegmentOffset(segmentOffset);
			pa->SetSegmentSize(segmentSize);
			pa->SetCrc(crc);
		}
		else
		{
			if (infile.ScanLine("%i", &statusInt) != 1) goto error;
		}

		ArticleInfo::EStatus status = (ArticleInfo::EStatus)statusInt;

		if (status == ArticleInfo::aiRunning)
		{
			status = ArticleInfo::aiUndefined;
		}

		if (status == ArticleInfo::aiFinished && !g_Options->GetDirectWrite() &&
			!fileInfo->GetForceDirectWrite() && !pa->GetResultFilename())
		{
			pa->SetResultFilename(BString<1024>("%s%c%i.%03i", g_Options->GetTempDir(),
				PATH_SEPARATOR, fileInfo->GetId(), pa->GetPartNumber()));
		}

		// don't allow all articles be completed or the file will stuck.
		// such states should never be saved on disk but just in case.
		if (completedArticles == size - 1 && !completed)
		{
			status = ArticleInfo::aiUndefined;
		}
		if (status != ArticleInfo::aiUndefined)
		{
			completedArticles++;
		}

		pa->SetStatus(status);
	}

	fileInfo->SetCompletedArticles(completedArticles);

	infile.Close();
	return true;

error:
	infile.Close();
	error("Error reading diskstate for file %i", fileInfo->GetId());
	return false;
}

void DiskState::DiscardFiles(NzbInfo* nzbInfo, bool deleteLog)
{
	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
		DiscardFile(fileInfo->GetId(), true, true, true);
	}

	for (CompletedFile& completedFile : nzbInfo->GetCompletedFiles())
	{
		if (completedFile.GetStatus() != CompletedFile::cfSuccess)
		{
			DiscardFile(completedFile.GetId(), true, true, true);
		}
	}

	if (deleteLog)
	{
		BString<1024> filename;
		filename.Format("%s%cn%i.log", g_Options->GetQueueDir(), PATH_SEPARATOR, nzbInfo->GetId());
		FileSystem::DeleteFile(filename);
	}
}

void DiskState::SaveDupInfo(DupInfo* dupInfo, StateDiskFile& outfile)
{
	uint32 High, Low;
	Util::SplitInt64(dupInfo->GetSize(), &High, &Low);
	outfile.PrintLine("%i,%u,%u,%u,%u,%i,%i", (int)dupInfo->GetStatus(), High, Low,
		dupInfo->GetFullContentHash(), dupInfo->GetFilteredContentHash(),
		dupInfo->GetDupeScore(), (int)dupInfo->GetDupeMode());
	outfile.PrintLine("%s", dupInfo->GetName());
	outfile.PrintLine("%s", dupInfo->GetDupeKey());
}

bool DiskState::LoadDupInfo(DupInfo* dupInfo, StateDiskFile& infile, int formatVersion)
{
	char buf[1024];

	int status;
	uint32 High, Low;
	uint32 fullContentHash, filteredContentHash = 0;
	int dupeScore, dupeMode;
	if (infile.ScanLine("%i,%u,%u,%u,%u,%i,%i", &status, &High, &Low, &fullContentHash, &filteredContentHash, &dupeScore, &dupeMode) != 7) goto error;

	dupInfo->SetStatus((DupInfo::EStatus)status);
	dupInfo->SetFullContentHash(fullContentHash);
	dupInfo->SetFilteredContentHash(filteredContentHash);
	dupInfo->SetSize(Util::JoinInt64(High, Low));
	dupInfo->SetDupeScore(dupeScore);
	dupInfo->SetDupeMode((EDupeMode)dupeMode);

	if (!infile.ReadLine(buf, sizeof(buf))) goto error;
	dupInfo->SetName(buf);

	if (!infile.ReadLine(buf, sizeof(buf))) goto error;
	dupInfo->SetDupeKey(buf);

	return true;

error:
	return false;
}

void DiskState::SaveHistory(HistoryList* history, StateDiskFile& outfile)
{
	debug("Saving history to disk");

	outfile.PrintLine("%i", (int)history->size());
	for (HistoryInfo* historyInfo : history)
	{
		outfile.PrintLine("%i,%i,%i", historyInfo->GetId(), (int)historyInfo->GetKind(), (int)historyInfo->GetTime());

		if (historyInfo->GetKind() == HistoryInfo::hkNzb || historyInfo->GetKind() == HistoryInfo::hkUrl)
		{
			SaveNzbInfo(historyInfo->GetNzbInfo(), outfile);
		}
		else if (historyInfo->GetKind() == HistoryInfo::hkDup)
		{
			SaveDupInfo(historyInfo->GetDupInfo(), outfile);
		}
	}
}

bool DiskState::LoadHistory(HistoryList* history, Servers* servers, StateDiskFile& infile, int formatVersion)
{
	debug("Loading history from disk");

	int size;
	if (infile.ScanLine("%i", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		std::unique_ptr<HistoryInfo> historyInfo;
		HistoryInfo::EKind kind = HistoryInfo::hkNzb;
		int id = 0;
		int time;

		int kindval = 0;
		if (infile.ScanLine("%i,%i,%i", &id, &kindval, &time) != 3) goto error;
		kind = (HistoryInfo::EKind)kindval;

		if (kind == HistoryInfo::hkNzb)
		{
			std::unique_ptr<NzbInfo> nzbInfo = std::make_unique<NzbInfo>();
			if (!LoadNzbInfo(nzbInfo.get(), servers, infile, formatVersion)) goto error;
			nzbInfo->LeavePostProcess();
			historyInfo = std::make_unique<HistoryInfo>(std::move(nzbInfo));
		}
		else if (kind == HistoryInfo::hkUrl)
		{
			std::unique_ptr<NzbInfo> nzbInfo = std::make_unique<NzbInfo>();
			if (!LoadNzbInfo(nzbInfo.get(), servers, infile, formatVersion)) goto error;
			historyInfo = std::make_unique<HistoryInfo>(std::move(nzbInfo));
		}
		else if (kind == HistoryInfo::hkDup)
		{
			std::unique_ptr<DupInfo> dupInfo = std::make_unique<DupInfo>();
			if (!LoadDupInfo(dupInfo.get(), infile, formatVersion)) goto error;
			dupInfo->SetId(id);
			historyInfo = std::make_unique<HistoryInfo>(std::move(dupInfo));
		}

		historyInfo->SetTime((time_t)time);

		history->push_back(std::move(historyInfo));
	}

	return true;

error:
	error("Error reading diskstate for history");
	return false;
}

/*
 * Deletes whole download queue including history.
 */
void DiskState::DiscardDownloadQueue()
{
	debug("Discarding queue");

	BString<1024> fullFilename("%s%c%s", g_Options->GetQueueDir(), PATH_SEPARATOR, "queue");
	FileSystem::DeleteFile(fullFilename);

	fullFilename.Format("%s%c%s", g_Options->GetQueueDir(), PATH_SEPARATOR, "history");
	FileSystem::DeleteFile(fullFilename);

	DirBrowser dir(g_Options->GetQueueDir());
	while (const char* filename = dir.Next())
	{
		// delete all files whose names have only characters '0'..'9'
		bool onlyNums = true;
		for (const char* p = filename; *p != '\0'; p++)
		{
			if (!('0' <= *p && *p <= '9'))
			{
				onlyNums = false;
				break;
			}
		}
		if (onlyNums)
		{
			fullFilename.Format("%s%c%s", g_Options->GetQueueDir(), PATH_SEPARATOR, filename);
			FileSystem::DeleteFile(fullFilename);

			// delete file state file
			fullFilename.Format("%s%c%ss", g_Options->GetQueueDir(), PATH_SEPARATOR, filename);
			FileSystem::DeleteFile(fullFilename);

			// delete failed info file
			fullFilename.Format("%s%c%sc", g_Options->GetQueueDir(), PATH_SEPARATOR, filename);
			FileSystem::DeleteFile(fullFilename);
		}
	}
}

bool DiskState::DownloadQueueExists()
{
	debug("Checking if a saved queue exists on disk");

	return FileSystem::FileExists(BString<1024>("%s%c%s", g_Options->GetQueueDir(), PATH_SEPARATOR, "queue")) ||
		FileSystem::FileExists(BString<1024>("%s%c%s", g_Options->GetQueueDir(), PATH_SEPARATOR, "history"));
}

void DiskState::DiscardFile(int fileId, bool deleteData, bool deletePartialState, bool deleteCompletedState)
{
	BString<1024> fileName;

	// info and articles file
	if (deleteData)
	{
		fileName.Format("%s%c%i", g_Options->GetQueueDir(), PATH_SEPARATOR, fileId);
		FileSystem::DeleteFile(fileName);
	}

	// partial state file
	if (deletePartialState)
	{
		fileName.Format("%s%c%is", g_Options->GetQueueDir(), PATH_SEPARATOR, fileId);
		FileSystem::DeleteFile(fileName);
	}

	// completed state file
	if (deleteCompletedState)
	{
		fileName.Format("%s%c%ic", g_Options->GetQueueDir(), PATH_SEPARATOR, fileId);
		FileSystem::DeleteFile(fileName);
	}
}

void DiskState::CleanupTempDir(DownloadQueue* downloadQueue)
{
	DirBrowser dir(g_Options->GetTempDir());
	while (const char* filename = dir.Next())
	{
		bool garbage = strstr(filename, ".tmp") || strstr(filename, ".dec");

		int id, part;
		if (!garbage && sscanf(filename, "%i.%i", &id, &part) == 2)
		{
			garbage = true;
			for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
			{
				if (nzbInfo->GetFileList()->Find(id))
				{
					garbage = false;
					break;
				}
			}
		}

		if (garbage)
		{
			BString<1024> fullFilename("%s%c%s", g_Options->GetTempDir(), PATH_SEPARATOR, filename);
			FileSystem::DeleteFile(fullFilename);
		}
	}
}

void DiskState::CleanupQueueDir(DownloadQueue* downloadQueue)
{
	// Prepare sorted id lists for faster search

	std::vector<int> nzbIdList;
	std::vector<int> fileIdList;

	for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
	{
		nzbIdList.push_back(nzbInfo->GetId());

		for (FileInfo* fileInfo : nzbInfo->GetFileList())
		{
			fileIdList.push_back(fileInfo->GetId());
		}

		for (CompletedFile& completedFile : nzbInfo->GetCompletedFiles())
		{
			fileIdList.push_back(completedFile.GetId());
		}
	}

	for (HistoryInfo* historyInfo : downloadQueue->GetHistory())
	{
		if (historyInfo->GetKind() == HistoryInfo::hkNzb ||
			historyInfo->GetKind() == HistoryInfo::hkUrl)
		{
			NzbInfo* nzbInfo = historyInfo->GetNzbInfo();
			nzbIdList.push_back(nzbInfo->GetId());

			for (FileInfo* fileInfo : nzbInfo->GetFileList())
			{
				fileIdList.push_back(fileInfo->GetId());
			}

			for (CompletedFile& completedFile : nzbInfo->GetCompletedFiles())
			{
				fileIdList.push_back(completedFile.GetId());
			}
		}
	}

	std::sort(nzbIdList.begin(), nzbIdList.end());
	std::sort(fileIdList.begin(), fileIdList.end());

	// Do cleanup

	int deletedFiles = 0;

	DirBrowser dir(g_Options->GetQueueDir());
	while (const char* filename = dir.Next())
	{
		bool del = false;

		int id;
		char suffix;
		if ((sscanf(filename, "%i%c", &id, &suffix) == 2 && (suffix == 's' || suffix == 'c')) ||
			(sscanf(filename, "%i", &id) == 1 && !strchr(filename, '.')))
		{
			del = !std::binary_search(fileIdList.begin(), fileIdList.end(), id);
		}

		if (!del && sscanf(filename, "n%i.log", &id) == 1)
		{
			del = !std::binary_search(nzbIdList.begin(), nzbIdList.end(), id);
		}

		if (del)
		{
			BString<1024> fullFilename("%s%c%s", g_Options->GetQueueDir(), PATH_SEPARATOR, filename);
			detail("Deleting orphaned diskstate file %s", filename);
			FileSystem::DeleteFile(fullFilename);
			deletedFiles++;
		}
	}

	if (deletedFiles > 0)
	{
		info("Deleted %i orphaned diskstate file(s)", deletedFiles);
	}
}

/* For safety:
 * - first save to temp-file (feeds.new)
 * - then delete feeds
 * - then rename feeds.new to feeds
 */
bool DiskState::SaveFeeds(Feeds* feeds, FeedHistory* feedHistory)
{
	debug("Saving feeds state to disk");

	StateFile stateFile("feeds", DISKSTATE_FEEDS_VERSION, true);

	if (feeds->empty() && feedHistory->empty())
	{
		stateFile.Discard();
		return true;
	}

	StateDiskFile* outfile = stateFile.BeginWrite();
	if (!outfile)
	{
		return false;
	}

	// save status
	SaveFeedStatus(feeds, *outfile);

	// save history
	SaveFeedHistory(feedHistory, *outfile);

	// now rename to dest file name
	return stateFile.FinishWrite();
}

bool DiskState::LoadFeeds(Feeds* feeds, FeedHistory* feedHistory)
{
	debug("Loading feeds state from disk");

	StateFile stateFile("feeds", DISKSTATE_FEEDS_VERSION, true);

	if (!stateFile.FileExists())
	{
		return true;
	}

	StateDiskFile* infile = stateFile.BeginRead();
	if (!infile)
	{
		return false;
	}

	bool ok = false;
	int formatVersion = stateFile.GetFileVersion();

	// load feed status
	if (!LoadFeedStatus(feeds, *infile, formatVersion)) goto error;

	// load feed history
	if (!LoadFeedHistory(feedHistory, *infile, formatVersion)) goto error;

	ok = true;

error:

	if (!ok)
	{
		error("Error reading diskstate for feeds");
	}

	return ok;
}

bool DiskState::SaveFeedStatus(Feeds* feeds, StateDiskFile& outfile)
{
	debug("Saving feed status to disk");

	outfile.PrintLine("%i", (int)feeds->size());
	for (FeedInfo* feedInfo : feeds)
	{
		outfile.PrintLine("%s", feedInfo->GetUrl());
		outfile.PrintLine("%u", feedInfo->GetFilterHash());
		outfile.PrintLine("%i", (int)feedInfo->GetLastUpdate());
	}

	return true;
}

bool DiskState::LoadFeedStatus(Feeds* feeds, StateDiskFile& infile, int formatVersion)
{
	debug("Loading feed status from disk");

	int size;
	if (infile.ScanLine("%i", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		char url[1024];
		if (!infile.ReadLine(url, sizeof(url))) goto error;

		char filter[1024];
		if (formatVersion == 2)
		{
			if (!infile.ReadLine(filter, sizeof(filter))) goto error;
		}

		uint32 filterHash = 0;
		if (formatVersion >= 3)
		{
			if (infile.ScanLine("%u", &filterHash) != 1) goto error;
		}

		int lastUpdate = 0;
		if (infile.ScanLine("%i", &lastUpdate) != 1) goto error;

		for (FeedInfo* feedInfo : feeds)
		{
			if (!strcmp(feedInfo->GetUrl(), url) &&
				((formatVersion == 1) ||
				 (formatVersion == 2 && !strcmp(feedInfo->GetFilter(), filter)) ||
				 (formatVersion >= 3 && feedInfo->GetFilterHash() == filterHash)))
			{
				feedInfo->SetLastUpdate((time_t)lastUpdate);
			}
		}
	}

	return true;

error:
	error("Error reading feed status from disk");
	return false;
}

bool DiskState::SaveFeedHistory(FeedHistory* feedHistory, StateDiskFile& outfile)
{
	debug("Saving feed history to disk");

	outfile.PrintLine("%i", (int)feedHistory->size());
	for (FeedHistoryInfo& feedHistoryInfo : feedHistory)
	{
		outfile.PrintLine("%i,%i", (int)feedHistoryInfo.GetStatus(), (int)feedHistoryInfo.GetLastSeen());
		outfile.PrintLine("%s", feedHistoryInfo.GetUrl());
	}

	return true;
}

bool DiskState::LoadFeedHistory(FeedHistory* feedHistory, StateDiskFile& infile, int formatVersion)
{
	debug("Loading feed history from disk");

	int size;
	if (infile.ScanLine("%i", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		int status = 0;
		int lastSeen = 0;
		int r = infile.ScanLine("%i,%i", &status, &lastSeen);
		if (r != 2) goto error;

		char url[1024];
		if (!infile.ReadLine(url, sizeof(url))) goto error;

		feedHistory->emplace_back(url, (FeedHistoryInfo::EStatus)(status), (time_t)(lastSeen));
	}

	return true;

error:
	error("Error reading feed history from disk");
	return false;
}

void DiskState::CalcFileStats(DownloadQueue* downloadQueue, int formatVersion)
{
	for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
	{
		nzbInfo->UpdateCurrentStats();
	}
}

bool DiskState::SaveAllFileInfos(DownloadQueue* downloadQueue)
{
	bool ok = true;
	StateFile stateFile("files", DISKSTATE_FILE_VERSION, true);
	if (!downloadQueue->GetQueue()->empty())
	{
		StateDiskFile* outfile = stateFile.BeginWrite();
		if (!outfile)
		{
			return false;
		}

		// save file-infos

		int fileCount = 0;
		for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
		{
			fileCount += nzbInfo->GetFileList()->size();
		}
		outfile->PrintLine("%i", fileCount);

		for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
		{
			for (FileInfo* fileInfo : nzbInfo->GetFileList())
			{
				outfile->PrintLine("%i", fileInfo->GetId());
				SaveFileInfo(fileInfo, *outfile, false);
			}
		}

		// now rename to dest file name
		ok = stateFile.FinishWrite();
	}
	else
	{
		stateFile.Discard();
	}

	return ok;
}

bool DiskState::LoadAllFileInfos(DownloadQueue* downloadQueue)
{
	if (downloadQueue->GetQueue()->empty())
	{
		return true;
	}

	StateFile stateFile("files", DISKSTATE_FILE_VERSION, false);
	StateDiskFile* infile = nullptr;
	bool useHibernate = false;

	if (stateFile.FileExists())
	{
		infile = stateFile.BeginRead();
		useHibernate = infile != nullptr;
		if (useHibernate)
		{
			int fileCount = 0;
			for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
			{
				fileCount += nzbInfo->GetFileList()->size();
			}
			int size = 0;
			useHibernate = infile->ScanLine("%i", &size) == 1 && size == fileCount;
		}
		if (!useHibernate)
		{
			stateFile.Discard();
		}
	}

	for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
	{
		RawFileList brokenFileInfos;

		for (FileInfo* fileInfo : nzbInfo->GetFileList())
		{
			bool res = false;
			if (useHibernate)
			{
				int id = 0;
				infile->ScanLine("%i", &id);
				if (id == fileInfo->GetId())
				{
					res = LoadFileInfo(fileInfo, *infile, stateFile.GetFileVersion(), true, false);
				}
			}
			if (!res)
			{
				res = LoadFile(fileInfo, true, false);
			}
			if (!res)
			{
				brokenFileInfos.push_back(fileInfo);
			}
		}

		for (FileInfo* fileInfo : brokenFileInfos)
		{
			nzbInfo->GetFileList()->Remove(fileInfo);
		}
	}

	return true;
}

void DiskState::DiscardQuickFileInfos()
{
	StateFile stateFile("files", DISKSTATE_FILE_VERSION, false);
	stateFile.Discard();
}

bool DiskState::LoadAllFileStates(DownloadQueue* downloadQueue, Servers* servers)
{
	BString<1024> cacheFlagFilename("%s%c%s", g_Options->GetQueueDir(), PATH_SEPARATOR, "acache");
	bool cacheWasActive = FileSystem::FileExists(cacheFlagFilename);

	DirBrowser dir(g_Options->GetQueueDir());
	while (const char* filename = dir.Next())
	{
		int id;
		char suffix;
		if (sscanf(filename, "%i%c", &id, &suffix) == 2)
		{
			if (suffix == 'c' || (suffix == 's' && g_Options->GetContinuePartial() && !cacheWasActive))
			{
				for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
				{
					for (FileInfo* fileInfo : nzbInfo->GetFileList())
					{
						if (fileInfo->GetId() == id)
						{
							if (!LoadFileState(fileInfo, servers, suffix == 'c')) goto error;
							fileInfo->GetArticles()->clear();
							fileInfo->SetPartialState(suffix == 'c' ? FileInfo::psCompleted : FileInfo::psPartial);
							goto next;
						}
					}
				}
			}
			else
			{
				BString<1024> fullFilename("%s%c%s", g_Options->GetQueueDir(), PATH_SEPARATOR, filename);
				FileSystem::DeleteFile(fullFilename);
			}
		}
		next:;
	}

	return true;

error:
	return false;
}

bool DiskState::SaveStats(Servers* servers, ServerVolumes* serverVolumes)
{
	debug("Saving stats to disk");

	StateFile stateFile("stats", DISKSTATE_STATS_VERSION, true);

	if (servers->empty())
	{
		stateFile.Discard();
		return true;
	}

	StateDiskFile* outfile = stateFile.BeginWrite();
	if (!outfile)
	{
		return false;
	}

	// save server names
	SaveServerInfo(servers, *outfile);

	// save stat
	SaveVolumeStat(serverVolumes, *outfile);

	// now rename to dest file name
	return stateFile.FinishWrite();
}

bool DiskState::LoadStats(Servers* servers, ServerVolumes* serverVolumes, bool* perfectMatch)
{
	debug("Loading stats from disk");

	StateFile stateFile("stats", DISKSTATE_STATS_VERSION, true);

	if (!stateFile.FileExists())
	{
		return true;
	}

	StateDiskFile* infile = stateFile.BeginRead();
	if (!infile)
	{
		return false;
	}

	bool ok = false;
	int formatVersion = stateFile.GetFileVersion();

	if (!LoadServerInfo(servers, *infile, formatVersion, perfectMatch)) goto error;

	if (formatVersion >=2)
	{
		if (!LoadVolumeStat(servers, serverVolumes, *infile, formatVersion)) goto error;
	}

	ok = true;

error:

	if (!ok)
	{
		error("Error reading diskstate for statistics");
	}

	return ok;
}

bool DiskState::SaveServerInfo(Servers* servers, StateDiskFile& outfile)
{
	debug("Saving server info to disk");

	outfile.PrintLine("%i", (int)servers->size());
	for (NewsServer* newsServer : servers)
	{
		outfile.PrintLine("%s", newsServer->GetName());
		outfile.PrintLine("%s", newsServer->GetHost());
		outfile.PrintLine("%i", newsServer->GetPort());
		outfile.PrintLine("%s", newsServer->GetUser());
	}

	return true;
}

/*
 ***************************************************************************************
 * Server matching
 */

class ServerRef
{
public:
	int m_stateId;
	CString m_name;
	CString m_host;
	int m_port;
	CString m_user;
	bool m_matched;
	bool m_perfect;

	int GetStateId() { return m_stateId; }
	const char* GetName() { return m_name; }
	const char* GetHost() { return m_host; }
	int GetPort() { return m_port; }
	const char* GetUser() { return m_user; }
	bool GetMatched() { return m_matched; }
	void SetMatched(bool matched) { m_matched = matched; }
	bool GetPerfect() { return m_perfect; }
	void SetPerfect(bool perfect) { m_perfect = perfect; }
};

typedef std::vector<ServerRef*> ServerRefList;

class OwnedServerRefList : public ServerRefList
{
public:
	~OwnedServerRefList()
	{
		for (ServerRef* ref : this)
		{
			delete ref;
		}
	}
};

enum ECriteria
{
	name,
	host,
	port,
	user
};

void FindCandidates(NewsServer* newsServer, ServerRefList* refs, ECriteria criteria, bool keepIfNothing)
{
	ServerRefList originalRefs;
	originalRefs.insert(originalRefs.begin(), refs->begin(), refs->end());

	refs->erase(std::remove_if(refs->begin(), refs->end(), 
		[newsServer, criteria](ServerRef* ref)
		{
			bool match = false;
			switch(criteria)
			{
				case name:
					match = !strcasecmp(newsServer->GetName(), ref->GetName());
					break;
				case host:
					match = !strcasecmp(newsServer->GetHost(), ref->GetHost());
					break;
				case port:
					match = newsServer->GetPort() == ref->GetPort();
					break;
				case user:
					match = !strcasecmp(newsServer->GetUser(), ref->GetUser());
					break;
			}
			return !match || ref->GetMatched();
		}),
		refs->end());

	if (refs->size() == 0 && keepIfNothing)
	{
		refs->insert(refs->begin(), originalRefs.begin(), originalRefs.end());
	}
}

void MatchServers(Servers* servers, ServerRefList* serverRefs)
{
	// Step 1: trying perfect match
	for (NewsServer* newsServer : servers)
	{
		ServerRefList matchedRefs;
		matchedRefs.insert(matchedRefs.begin(), serverRefs->begin(), serverRefs->end());
		FindCandidates(newsServer, &matchedRefs, name, false);
		FindCandidates(newsServer, &matchedRefs, host, false);
		FindCandidates(newsServer, &matchedRefs, port, false);
		FindCandidates(newsServer, &matchedRefs, user, false);

		if (matchedRefs.size() == 1)
		{
			ServerRef* ref = matchedRefs.front();
			newsServer->SetStateId(ref->GetStateId());
			ref->SetMatched(true);
			ref->SetPerfect(true);
		}
	}

	// Step 2: matching host, port, username and server-name
	for (NewsServer* newsServer : servers)
	{
		if (!newsServer->GetStateId())
		{
			ServerRefList matchedRefs;
			matchedRefs.insert(matchedRefs.begin(), serverRefs->begin(), serverRefs->end());

			FindCandidates(newsServer, &matchedRefs, host, false);

			if (matchedRefs.size() > 1)
			{
				FindCandidates(newsServer, &matchedRefs, name, true);
			}

			if (matchedRefs.size() > 1)
			{
				FindCandidates(newsServer, &matchedRefs, user, true);
			}

			if (matchedRefs.size() > 1)
			{
				FindCandidates(newsServer, &matchedRefs, port, true);
			}

			if (!matchedRefs.empty())
			{
				ServerRef* ref = matchedRefs.front();
				newsServer->SetStateId(ref->GetStateId());
				ref->SetMatched(true);
			}
		}
	}
}

/*
 * END: Server matching
 ***************************************************************************************
 */

bool DiskState::LoadServerInfo(Servers* servers, StateDiskFile& infile, int formatVersion, bool* perfectMatch)
{
	debug("Loading server info from disk");

	OwnedServerRefList serverRefs;
	*perfectMatch = true;

	int size;
	if (infile.ScanLine("%i", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		char name[1024];
		if (!infile.ReadLine(name, sizeof(name))) goto error;

		char host[200];
		if (!infile.ReadLine(host, sizeof(host))) goto error;

		int port;
		if (infile.ScanLine("%i", &port) != 1) goto error;

		char user[100];
		if (!infile.ReadLine(user, sizeof(user))) goto error;

		std::unique_ptr<ServerRef> ref = std::make_unique<ServerRef>();
		ref->m_stateId = i + 1;
		ref->m_name = name;
		ref->m_host = host;
		ref->m_port = port;
		ref->m_user = user;
		ref->m_matched = false;
		ref->m_perfect = false;
		serverRefs.push_back(ref.release());
	}

	MatchServers(servers, &serverRefs);

	for (ServerRef* ref : serverRefs)
	{
		*perfectMatch = *perfectMatch && ref->GetPerfect();
	}

	debug("******** MATCHING NEWS-SERVERS **********");
	for (NewsServer* newsServer : servers)
	{
		*perfectMatch = *perfectMatch && newsServer->GetStateId();
		debug("Server %i -> %i", newsServer->GetId(), newsServer->GetStateId());
		debug("Server %i.Name: %s", newsServer->GetId(), newsServer->GetName());
		debug("Server %i.Host: %s:%i", newsServer->GetId(), newsServer->GetHost(), newsServer->GetPort());
	}

	debug("All servers perfectly matched: %s", *perfectMatch ? "yes" : "no");

	return true;

error:
	error("Error reading server info from disk");

	return false;
}

bool DiskState::SaveVolumeStat(ServerVolumes* serverVolumes, StateDiskFile& outfile)
{
	debug("Saving volume stats to disk");

	outfile.PrintLine("%i", (int)serverVolumes->size());
	for (ServerVolume& serverVolume : serverVolumes)
	{
		outfile.PrintLine("%i,%i,%i", serverVolume.GetFirstDay(), (int)serverVolume.GetDataTime(), (int)serverVolume.GetCustomTime());

		uint32 High1, Low1, High2, Low2;
		Util::SplitInt64(serverVolume.GetTotalBytes(), &High1, &Low1);
		Util::SplitInt64(serverVolume.GetCustomBytes(), &High2, &Low2);
		outfile.PrintLine("%u,%u,%u,%u", High1, Low1, High2, Low2);

		ServerVolume::VolumeArray* VolumeArrays[] = { serverVolume.BytesPerSeconds(),
			serverVolume.BytesPerMinutes(), serverVolume.BytesPerHours(), serverVolume.BytesPerDays() };
		for (int i=0; i < 4; i++)
		{
			ServerVolume::VolumeArray* volumeArray = VolumeArrays[i];

			outfile.PrintLine("%i", (int)volumeArray->size());
			for (int64 bytes : *volumeArray)
			{
				Util::SplitInt64(bytes, &High1, &Low1);
				outfile.PrintLine("%u,%u", High1, Low1);
			}
		}
	}

	return true;
}

bool DiskState::LoadVolumeStat(Servers* servers, ServerVolumes* serverVolumes, StateDiskFile& infile, int formatVersion)
{
	debug("Loading volume stats from disk");

	int size;
	if (infile.ScanLine("%i", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		ServerVolume* serverVolume = nullptr;

		if (i == 0)
		{
			serverVolume = &serverVolumes->at(0);
		}
		else
		{
			for (NewsServer* newsServer : servers)
			{
				if (newsServer->GetStateId() == i)
				{
					serverVolume = &serverVolumes->at(newsServer->GetId());
				}
			}
		}

		int firstDay, dataTime, customTime;
		uint32 High1, Low1, High2 = 0, Low2 = 0;
		if (formatVersion >= 3)
		{
			if (infile.ScanLine("%i,%i,%i", &firstDay, &dataTime,&customTime) != 3) goto error;
			if (infile.ScanLine("%u,%u,%u,%u", &High1, &Low1, &High2, &Low2) != 4) goto error;
			if (serverVolume) serverVolume->SetCustomTime((time_t)customTime);
		}
		else
		{
			if (infile.ScanLine("%i,%i", &firstDay, &dataTime) != 2) goto error;
			if (infile.ScanLine("%u,%u", &High1, &Low1) != 2) goto error;
		}
		if (serverVolume) serverVolume->SetFirstDay(firstDay);
		if (serverVolume) serverVolume->SetDataTime((time_t)dataTime);
		if (serverVolume) serverVolume->SetTotalBytes(Util::JoinInt64(High1, Low1));
		if (serverVolume) serverVolume->SetCustomBytes(Util::JoinInt64(High2, Low2));

		ServerVolume::VolumeArray* VolumeArrays[] = { serverVolume ? serverVolume->BytesPerSeconds() : nullptr,
			serverVolume ? serverVolume->BytesPerMinutes() : nullptr,
			serverVolume ? serverVolume->BytesPerHours() : nullptr,
			serverVolume ? serverVolume->BytesPerDays() : nullptr };
		for (int k=0; k < 4; k++)
		{
			ServerVolume::VolumeArray* volumeArray = VolumeArrays[k];

			int arrSize;
			if (infile.ScanLine("%i", &arrSize) != 1) goto error;
			if (volumeArray) volumeArray->resize(arrSize);

			for (int j = 0; j < arrSize; j++)
			{
				if (infile.ScanLine("%u,%u", &High1, &Low1) != 2) goto error;
				if (volumeArray) (*volumeArray)[j] = Util::JoinInt64(High1, Low1);
			}
		}
	}

	return true;

error:
	error("Error reading volume stats from disk");

	return false;
}

void DiskState::WriteCacheFlag()
{
	BString<1024> flagFilename("%s%c%s", g_Options->GetQueueDir(), PATH_SEPARATOR, "acache");

	StateDiskFile outfile;
	if (!outfile.Open(flagFilename, StateDiskFile::omWrite))
	{
		error("Error saving diskstate: Could not create file %s", *flagFilename);
		return;
	}

	outfile.Close();
}

void DiskState::DeleteCacheFlag()
{
	BString<1024> flagFilename("%s%c%s", g_Options->GetQueueDir(), PATH_SEPARATOR, "acache");
	FileSystem::DeleteFile(flagFilename);
}

void DiskState::AppendNzbMessage(int nzbId, Message::EKind kind, const char* text)
{
	BString<1024> logFilename("%s%cn%i.log", g_Options->GetQueueDir(), PATH_SEPARATOR, nzbId);

	StateDiskFile outfile;
	if (!outfile.Open(logFilename, StateDiskFile::omAppend))
	{
		error("Error saving log: Could not create file %s", *logFilename);
		return;
	}

	const char* messageType[] = { "INFO", "WARNING", "ERROR", "DEBUG", "DETAIL"};

	BString<1024> tmp2;
	tmp2 = text;

	// replace bad chars
	for (char* p = tmp2; *p; p++)
	{
		char ch = *p;
		if (ch == '\n' || ch == '\r' || ch == '\t')
		{
			*p = ' ';
		}
	}

	time_t tm = Util::CurrentTime();
	time_t rawtime = tm + g_Options->GetTimeCorrection();

	BString<100> time;
	Util::FormatTime(rawtime, time, 100);

	outfile.Print("%s\t%u\t%s\t%s%s", *time, (int)tm, messageType[kind], *tmp2, LINE_ENDING);

	outfile.Close();
}

void DiskState::LoadNzbMessages(int nzbId, MessageList* messages)
{
	// Important:
	//   - Other threads may be writing into the log-file at any time;
	//   - The log-file may also be deleted from another thread;

	BString<1024> logFilename("%s%cn%i.log", g_Options->GetQueueDir(), PATH_SEPARATOR, nzbId);

	if (!FileSystem::FileExists(logFilename))
	{
		return;
	}

	StateDiskFile infile;
	if (!infile.Open(logFilename, StateDiskFile::omRead))
	{
		error("Error reading log: could not open file %s", *logFilename);
		return;
	}

	int id = 0;
	char line[1024];
	while (infile.ReadLine(line, sizeof(line)))
	{
		Util::TrimRight(line);

		// time (skip formatted time first)
		char* p = strchr(line, '\t');
		if (!p) goto exit;
		int time = atoi(p + 1);

		// kind
		p = strchr(p + 1, '\t');
		if (!p) goto exit;
		char* kindStr = p + 1;

		Message::EKind kind = Message::mkError;
		if (!strncmp(kindStr, "INFO", 4))
		{
			kind = Message::mkInfo;
		}
		else if (!strncmp(kindStr, "WARNING", 7))
		{
			kind = Message::mkWarning;
		}
		else if (!strncmp(kindStr, "ERROR", 5))
		{
			kind = Message::mkError;
		}
		else if (!strncmp(kindStr, "DETAIL", 6))
		{
			kind = Message::mkDetail;
		}
		else if (!strncmp(kindStr, "DEBUG", 5))
		{
			kind = Message::mkDebug;
		}

		// text
		p = strchr(p + 1, '\t');
		if (!p) goto exit;
		char* text = p + 1;

		messages->emplace_back(++id, kind, (time_t)time, text);
	}

exit:
	infile.Close();
	return;
}
