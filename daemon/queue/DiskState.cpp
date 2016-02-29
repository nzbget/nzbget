/*
 *  This file is part of nzbget
 *
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


#include "nzbget.h"
#include "NString.h"
#include "DiskState.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"
#include "FileSystem.h"

static const char* FORMATVERSION_SIGNATURE = "nzbget diskstate file version ";

#if (defined(WIN32) && _MSC_VER < 1800)
// Visual Studio prior 2013 doesn't have standard "vsscanf"
// Hack from http://stackoverflow.com/questions/2457331/replacement-for-vsscanf-on-msvc
int vsscanf(const char *s, const char *fmt, va_list ap)
{
	// up to max 10 arguments
	void *a[10];
	for (int i = 0; i < sizeof(a)/sizeof(a[0]); i++)
	{
		a[i] = va_arg(ap, void*);
	}
	return sscanf(s, fmt, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9]);
}
#endif

/* Parse signature and return format version number
*/
int ParseFormatVersion(const char* formatSignature)
{
	if (strncmp(formatSignature, FORMATVERSION_SIGNATURE, strlen(FORMATVERSION_SIGNATURE)))
	{
		return 0;
	}

	return atoi(formatSignature + strlen(FORMATVERSION_SIGNATURE));
}


class StateFile
{
private:
	BString<1024> m_destFilename;
	BString<1024> m_tempFilename;
	int m_formatVersion;
	int m_fileVersion;
	DiskFile m_file;

public:
	StateFile(const char* filename, int formatVersion);
	void Discard();
	bool FileExists();
	DiskFile* BeginWriteTransaction();
	bool FinishWriteTransaction();
	DiskFile* BeginReadTransaction();
	int GetFileVersion() { return m_fileVersion; }
	const char* GetDestFilename() { return m_destFilename; }
};


StateFile::StateFile(const char* filename, int formatVersion)
{
	m_formatVersion = formatVersion;
	m_destFilename.Format("%s%c%s", g_Options->GetQueueDir(), PATH_SEPARATOR, filename);
	m_tempFilename.Format("%s%c%s.new", g_Options->GetQueueDir(), PATH_SEPARATOR, filename);
}

void StateFile::Discard()
{
	FileSystem::DeleteFile(m_destFilename);
}

bool StateFile::FileExists()
{
	return FileSystem::FileExists(m_destFilename) || FileSystem::FileExists(m_tempFilename);
}

DiskFile* StateFile::BeginWriteTransaction()
{
	if (!m_file.Open(m_tempFilename, DiskFile::omWrite))
	{
		error("Error saving diskstate: Could not create file %s: %s", *m_tempFilename,
			*FileSystem::GetLastErrorMessage());
		return nullptr;
	}

	m_file.Print("%s%i\n", FORMATVERSION_SIGNATURE, m_formatVersion);

	return &m_file;
}

bool StateFile::FinishWriteTransaction()
{
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

DiskFile* StateFile::BeginReadTransaction()
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

	if (!m_file.Open(m_destFilename, DiskFile::omRead))
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
		error("Could not load diskstate due to file version mismatch");
		m_file.Close();
	}

	return &m_file;
}

/*
 * Standard fscanf scans beoynd current line if the next line is empty.
 * This wrapper fixes that.
 */
int DiskState::fscanf(DiskFile& infile, const char* format, ...)
{
	char line[1024];
	if (!infile.ReadLine(line, sizeof(line)))
	{
		return 0;
	}

	va_list ap;
	va_start(ap, format);
	int res = vsscanf(line, format, ap);
	va_end(ap);

	return res;
}

/* Save Download Queue to Disk.
 * The Disk State consists of file "queue", which contains the order of files,
 * and of one diskstate-file for each file in download queue.
 * This function saves file "queue" and files with NZB-info. It does not
 * save file-infos.
 *
 * For safety:
 * - first save to temp-file (queue.new)
 * - then delete queue
 * - then rename queue.new to queue
 */
bool DiskState::SaveDownloadQueue(DownloadQueue* downloadQueue)
{
	debug("Saving queue to disk");

	StateFile stateFile("queue", 55);

	if (downloadQueue->GetQueue()->empty() &&
		downloadQueue->GetHistory()->empty())
	{
		stateFile.Discard();
		return true;
	}

	DiskFile* outfile = stateFile.BeginWriteTransaction();
	if (!outfile)
	{
		return false;
	}

	// save nzb-infos
	SaveNzbQueue(downloadQueue, *outfile);

	// save history
	SaveHistory(downloadQueue, *outfile);

	// now rename to dest file name
	return stateFile.FinishWriteTransaction();
}

bool DiskState::LoadDownloadQueue(DownloadQueue* downloadQueue, Servers* servers)
{
	debug("Loading queue from disk");

	StateFile stateFile("queue", 55);

	DiskFile* infile = stateFile.BeginReadTransaction();
	if (!infile)
	{
		return false;
	}

	bool ok = false;
	int formatVersion = stateFile.GetFileVersion();

	NzbList nzbList(false);
	NzbList sortList(false);

	if (formatVersion < 43)
	{
		// load nzb-infos
		if (!LoadNzbList(&nzbList, servers, *infile, formatVersion)) goto error;

		// load file-infos
		if (!LoadFileQueue12(&nzbList, &sortList, *infile, formatVersion)) goto error;
	}
	else
	{
		if (!LoadNzbList(downloadQueue->GetQueue(), servers, *infile, formatVersion)) goto error;
	}

	if (formatVersion >= 7 && formatVersion < 45)
	{
		// load post-queue from v12
		if (!LoadPostQueue12(downloadQueue, &nzbList, *infile, formatVersion)) goto error;
	}
	else if (formatVersion < 7)
	{
		// load post-queue from v5
		LoadPostQueue5(downloadQueue, &nzbList);
	}

	if (formatVersion >= 15 && formatVersion < 46)
	{
		// load url-queue
		if (!LoadUrlQueue12(downloadQueue, *infile, formatVersion)) goto error;
	}

	if (formatVersion >= 9)
	{
		// load history
		if (!LoadHistory(downloadQueue, &nzbList, servers, *infile, formatVersion)) goto error;
	}

	if (formatVersion >= 9 && formatVersion < 43)
	{
		// load parked file-infos
		if (!LoadFileQueue12(&nzbList, nullptr, *infile, formatVersion)) goto error;
	}

	if (formatVersion < 29)
	{
		CalcCriticalHealth(&nzbList);
	}

	if (formatVersion < 43)
	{
		// finalize queue reading
		CompleteNzbList12(downloadQueue, &sortList, formatVersion);
	}

	if (formatVersion < 47)
	{
		CompleteDupList12(downloadQueue, formatVersion);
	}

	if (!LoadAllFileStates(downloadQueue, servers)) goto error;

	ok = true;

error:

	if (!ok)
	{
		error("Error reading diskstate for download queue and history");
	}

	NzbInfo::ResetGenId(true);
	FileInfo::ResetGenId(true);

	if (formatVersion > 0)
	{
		CalcFileStats(downloadQueue, formatVersion);
	}

	return ok;
}

void DiskState::CompleteNzbList12(DownloadQueue* downloadQueue, NzbList* nzbList, int formatVersion)
{
	// put all NZBs referenced from file queue into pDownloadQueue->GetQueue()
	for (NzbInfo* nzbInfo: *nzbList)
	{
		downloadQueue->GetQueue()->push_back(nzbInfo);
	}

	if (31 <= formatVersion && formatVersion < 42)
	{
		// due to a bug in r811 (v12-testing) new NZBIDs were generated on each refresh of web-ui
		// this resulted in very high numbers for NZBIDs
		// here we renumber NZBIDs in order to keep them low.
		NzbInfo::ResetGenId(false);
		int id = 1;
		for (NzbInfo* nzbInfo : nzbList)
		{
			nzbInfo->SetId(id++);
		}
	}
}

void DiskState::CompleteDupList12(DownloadQueue* downloadQueue, int formatVersion)
{
	NzbInfo::ResetGenId(true);

	for (HistoryInfo* historyInfo : downloadQueue->GetHistory())
	{
		if (historyInfo->GetKind() == HistoryInfo::hkDup)
		{
			historyInfo->GetDupInfo()->SetId(NzbInfo::GenerateId());
		}
	}
}

void DiskState::SaveNzbQueue(DownloadQueue* downloadQueue, DiskFile& outfile)
{
	debug("Saving nzb list to disk");

	outfile.Print("%i\n", (int)downloadQueue->GetQueue()->size());
	for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
	{
		SaveNzbInfo(nzbInfo, outfile);
	}
}

bool DiskState::LoadNzbList(NzbList* nzbList, Servers* servers, DiskFile& infile, int formatVersion)
{
	debug("Loading nzb list from disk");

	// load nzb-infos
	int size;
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		NzbInfo* nzbInfo = new NzbInfo();
		nzbList->push_back(nzbInfo);
		if (!LoadNzbInfo(nzbInfo, servers, infile, formatVersion)) goto error;
	}

	return true;

error:
	error("Error reading nzb list from disk");
	return false;
}

void DiskState::SaveNzbInfo(NzbInfo* nzbInfo, DiskFile& outfile)
{
	outfile.Print("%i\n", nzbInfo->GetId());
	outfile.Print("%i\n", (int)nzbInfo->GetKind());
	outfile.Print("%s\n", nzbInfo->GetUrl());
	outfile.Print("%s\n", nzbInfo->GetFilename());
	outfile.Print("%s\n", nzbInfo->GetDestDir());
	outfile.Print("%s\n", nzbInfo->GetFinalDir());
	outfile.Print("%s\n", nzbInfo->GetQueuedFilename());
	outfile.Print("%s\n", nzbInfo->GetName());
	outfile.Print("%s\n", nzbInfo->GetCategory());
	outfile.Print("%i,%i,%i,%i,%i\n", (int)nzbInfo->GetPriority(),
		nzbInfo->GetPostInfo() ? (int)nzbInfo->GetPostInfo()->GetStage() + 1 : 0,
		(int)nzbInfo->GetDeletePaused(), (int)nzbInfo->GetManyDupeFiles(), nzbInfo->GetFeedId());
	outfile.Print("%i,%i,%i,%i,%i,%i,%i\n", (int)nzbInfo->GetParStatus(), (int)nzbInfo->GetUnpackStatus(),
		(int)nzbInfo->GetMoveStatus(), (int)nzbInfo->GetRenameStatus(), (int)nzbInfo->GetDeleteStatus(),
		(int)nzbInfo->GetMarkStatus(), (int)nzbInfo->GetUrlStatus());
	outfile.Print("%i,%i,%i\n", (int)nzbInfo->GetUnpackCleanedUpDisk(), (int)nzbInfo->GetHealthPaused(),
		(int)nzbInfo->GetAddUrlPaused());
	outfile.Print("%i,%i,%i\n", nzbInfo->GetFileCount(), nzbInfo->GetParkedFileCount(),
		nzbInfo->GetMessageCount());
	outfile.Print("%i,%i\n", (int)nzbInfo->GetMinTime(), (int)nzbInfo->GetMaxTime());
	outfile.Print("%i,%i,%i,%i\n", (int)nzbInfo->GetParFull(),
		nzbInfo->GetPostInfo() ? (int)nzbInfo->GetPostInfo()->GetForceParFull() : 0,
		nzbInfo->GetPostInfo() ? (int)nzbInfo->GetPostInfo()->GetForceRepair() : 0,
		nzbInfo->GetExtraParBlocks());

	outfile.Print("%u,%u\n", nzbInfo->GetFullContentHash(), nzbInfo->GetFilteredContentHash());

	uint32 High1, Low1, High2, Low2, High3, Low3;
	Util::SplitInt64(nzbInfo->GetSize(), &High1, &Low1);
	Util::SplitInt64(nzbInfo->GetSuccessSize(), &High2, &Low2);
	Util::SplitInt64(nzbInfo->GetFailedSize(), &High3, &Low3);
	outfile.Print("%u,%u,%u,%u,%u,%u\n", High1, Low1, High2, Low2, High3, Low3);

	Util::SplitInt64(nzbInfo->GetParSize(), &High1, &Low1);
	Util::SplitInt64(nzbInfo->GetParSuccessSize(), &High2, &Low2);
	Util::SplitInt64(nzbInfo->GetParFailedSize(), &High3, &Low3);
	outfile.Print("%u,%u,%u,%u,%u,%u\n", High1, Low1, High2, Low2, High3, Low3);

	outfile.Print("%i,%i,%i\n", nzbInfo->GetTotalArticles(), nzbInfo->GetSuccessArticles(), nzbInfo->GetFailedArticles());

	outfile.Print("%s\n", nzbInfo->GetDupeKey());
	outfile.Print("%i,%i\n", (int)nzbInfo->GetDupeMode(), nzbInfo->GetDupeScore());

	Util::SplitInt64(nzbInfo->GetDownloadedSize(), &High1, &Low1);
	outfile.Print("%u,%u,%i,%i,%i,%i,%i\n", High1, Low1, nzbInfo->GetDownloadSec(), nzbInfo->GetPostTotalSec(),
		nzbInfo->GetParSec(), nzbInfo->GetRepairSec(), nzbInfo->GetUnpackSec());

	outfile.Print("%i\n", (int)nzbInfo->GetCompletedFiles()->size());
	for (CompletedFile& completedFile : nzbInfo->GetCompletedFiles())
	{
		outfile.Print("%i,%i,%u,%s\n", completedFile.GetId(), (int)completedFile.GetStatus(),
			completedFile.GetCrc(), completedFile.GetFileName());
	}

	outfile.Print("%i\n", (int)nzbInfo->GetParameters()->size());
	for (NzbParameter& parameter : nzbInfo->GetParameters())
	{
		outfile.Print("%s=%s\n", parameter.GetName(), parameter.GetValue());
	}

	outfile.Print("%i\n", (int)nzbInfo->GetScriptStatuses()->size());
	for (ScriptStatus& scriptStatus : nzbInfo->GetScriptStatuses())
	{
		outfile.Print("%i,%s\n", scriptStatus.GetStatus(), scriptStatus.GetName());
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
	outfile.Print("%i\n", size);
	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
		if (!fileInfo->GetDeleted())
		{
			outfile.Print("%i,%i,%i,%i\n", fileInfo->GetId(), (int)fileInfo->GetPaused(),
				(int)fileInfo->GetTime(), (int)fileInfo->GetExtraPriority());
		}
	}
}

bool DiskState::LoadNzbInfo(NzbInfo* nzbInfo, Servers* servers, DiskFile& infile, int formatVersion)
{
	char buf[10240];

	if (formatVersion >= 24)
	{
		int id;
		if (fscanf(infile, "%i\n", &id) != 1) goto error;
		nzbInfo->SetId(id);
	}

	if (formatVersion >= 46)
	{
		int kind;
		if (fscanf(infile, "%i\n", &kind) != 1) goto error;
		nzbInfo->SetKind((NzbInfo::EKind)kind);

		if (!infile.ReadLine(buf, sizeof(buf))) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		nzbInfo->SetUrl(buf);
	}

	if (!infile.ReadLine(buf, sizeof(buf))) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	nzbInfo->SetFilename(buf);

	if (!infile.ReadLine(buf, sizeof(buf))) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	nzbInfo->SetDestDir(buf);

	if (formatVersion >= 27)
	{
		if (!infile.ReadLine(buf, sizeof(buf))) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		nzbInfo->SetFinalDir(buf);
	}

	if (formatVersion >= 5)
	{
		if (!infile.ReadLine(buf, sizeof(buf))) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		nzbInfo->SetQueuedFilename(buf);
	}

	if (formatVersion >= 13)
	{
		if (!infile.ReadLine(buf, sizeof(buf))) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		if (strlen(buf) > 0)
		{
			nzbInfo->SetName(buf);
		}
	}

	if (formatVersion >= 4)
	{
		if (!infile.ReadLine(buf, sizeof(buf))) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		nzbInfo->SetCategory(buf);
	}

	if (true) // clang requires a block for goto to work
	{
		int priority = 0, postProcess = 0, postStage = 0, deletePaused = 0, manyDupeFiles = 0, feedId = 0;
		if (formatVersion >= 54)
		{
			if (fscanf(infile, "%i,%i,%i,%i,%i\n", &priority, &postStage, &deletePaused, &manyDupeFiles, &feedId) != 5) goto error;
		}
		else if (formatVersion >= 45)
		{
			if (fscanf(infile, "%i,%i,%i,%i\n", &priority, &postStage, &deletePaused, &manyDupeFiles) != 4) goto error;
		}
		else if (formatVersion >= 44)
		{
			if (fscanf(infile, "%i,%i,%i,%i\n", &priority, &postProcess, &deletePaused, &manyDupeFiles) != 4) goto error;
		}
		else if (formatVersion >= 41)
		{
			if (fscanf(infile, "%i,%i,%i\n", &postProcess, &deletePaused, &manyDupeFiles) != 3) goto error;
		}
		else if (formatVersion >= 40)
		{
			if (fscanf(infile, "%i,%i\n", &postProcess, &deletePaused) != 2) goto error;
		}
		else if (formatVersion >= 4)
		{
			if (fscanf(infile, "%i\n", &postProcess) != 1) goto error;
		}
		nzbInfo->SetPriority(priority);
		nzbInfo->SetDeletePaused((bool)deletePaused);
		nzbInfo->SetManyDupeFiles((bool)manyDupeFiles);
		if (postStage > 0)
		{
			nzbInfo->EnterPostProcess();
			nzbInfo->GetPostInfo()->SetStage((PostInfo::EStage)postStage);
		}
		nzbInfo->SetFeedId(feedId);
	}

	if (formatVersion >= 8 && formatVersion < 18)
	{
		int parStatus;
		if (fscanf(infile, "%i\n", &parStatus) != 1) goto error;
		nzbInfo->SetParStatus((NzbInfo::EParStatus)parStatus);
	}

	if (formatVersion >= 9 && formatVersion < 18)
	{
		int scriptStatus;
		if (fscanf(infile, "%i\n", &scriptStatus) != 1) goto error;
		if (scriptStatus > 1) scriptStatus--;
		nzbInfo->GetScriptStatuses()->emplace_back("SCRIPT", (ScriptStatus::EStatus)scriptStatus);
	}

	if (formatVersion >= 18)
	{
		int parStatus, unpackStatus, scriptStatus, moveStatus = 0, renameStatus = 0,
			deleteStatus = 0, markStatus = 0, urlStatus = 0;
		if (formatVersion >= 46)
		{
			if (fscanf(infile, "%i,%i,%i,%i,%i,%i,%i\n", &parStatus, &unpackStatus, &moveStatus,
				&renameStatus, &deleteStatus, &markStatus, &urlStatus) != 7) goto error;
		}
		else if (formatVersion >= 37)
		{
			if (fscanf(infile, "%i,%i,%i,%i,%i,%i\n", &parStatus, &unpackStatus,
				&moveStatus, &renameStatus, &deleteStatus, &markStatus) != 6) goto error;
		}
		else if (formatVersion >= 35)
		{
			if (fscanf(infile, "%i,%i,%i,%i,%i\n", &parStatus, &unpackStatus,
				&moveStatus, &renameStatus, &deleteStatus) != 5) goto error;
		}
		else if (formatVersion >= 23)
		{
			if (fscanf(infile, "%i,%i,%i,%i\n", &parStatus, &unpackStatus,
				&moveStatus, &renameStatus) != 4) goto error;
		}
		else if (formatVersion >= 21)
		{
			if (fscanf(infile, "%i,%i,%i,%i,%i\n", &parStatus, &unpackStatus,
				&scriptStatus, &moveStatus, &renameStatus) != 5) goto error;
		}
		else if (formatVersion >= 20)
		{
			if (fscanf(infile, "%i,%i,%i,%i\n", &parStatus, &unpackStatus,
				&scriptStatus, &moveStatus) != 4) goto error;
		}
		else
		{
			if (fscanf(infile, "%i,%i,%i\n", &parStatus, &unpackStatus, &scriptStatus) != 3) goto error;
		}
		nzbInfo->SetParStatus((NzbInfo::EParStatus)parStatus);
		nzbInfo->SetUnpackStatus((NzbInfo::EUnpackStatus)unpackStatus);
		nzbInfo->SetMoveStatus((NzbInfo::EMoveStatus)moveStatus);
		nzbInfo->SetRenameStatus((NzbInfo::ERenameStatus)renameStatus);
		nzbInfo->SetDeleteStatus((NzbInfo::EDeleteStatus)deleteStatus);
		nzbInfo->SetMarkStatus((NzbInfo::EMarkStatus)markStatus);
		if (nzbInfo->GetKind() == NzbInfo::nkNzb ||
			(NzbInfo::EUrlStatus)urlStatus >= NzbInfo::lsFailed ||
			(NzbInfo::EUrlStatus)urlStatus >= NzbInfo::lsScanSkipped)
		{
			nzbInfo->SetUrlStatus((NzbInfo::EUrlStatus)urlStatus);
		}
		if (formatVersion < 23)
		{
			if (scriptStatus > 1) scriptStatus--;
			nzbInfo->GetScriptStatuses()->emplace_back("SCRIPT", (ScriptStatus::EStatus)scriptStatus);
		}
	}

	if (formatVersion >= 35)
	{
		int unpackCleanedUpDisk, healthPaused, addUrlPaused = 0;
		if (formatVersion >= 46)
		{
			if (fscanf(infile, "%i,%i,%i\n", &unpackCleanedUpDisk, &healthPaused, &addUrlPaused) != 3) goto error;
		}
		else
		{
			if (fscanf(infile, "%i,%i\n", &unpackCleanedUpDisk, &healthPaused) != 2) goto error;
		}
		nzbInfo->SetUnpackCleanedUpDisk((bool)unpackCleanedUpDisk);
		nzbInfo->SetHealthPaused((bool)healthPaused);
		nzbInfo->SetAddUrlPaused((bool)addUrlPaused);
	}
	else if (formatVersion >= 28)
	{
		int deleted, unpackCleanedUpDisk, healthPaused, healthDeleted;
		if (fscanf(infile, "%i,%i,%i,%i\n", &deleted, &unpackCleanedUpDisk, &healthPaused, &healthDeleted) != 4) goto error;
		nzbInfo->SetUnpackCleanedUpDisk((bool)unpackCleanedUpDisk);
		nzbInfo->SetHealthPaused((bool)healthPaused);
		nzbInfo->SetDeleteStatus(healthDeleted ? NzbInfo::dsHealth : deleted ? NzbInfo::dsManual : NzbInfo::dsNone);
	}

	if (formatVersion >= 28)
	{
		int fileCount, parkedFileCount, messageCount = 0;
		if (formatVersion >= 52)
		{
			if (fscanf(infile, "%i,%i,%i\n", &fileCount, &parkedFileCount, &messageCount) != 3) goto error;
		}
		else
		{
			if (fscanf(infile, "%i,%i\n", &fileCount, &parkedFileCount) != 2) goto error;
		}

		nzbInfo->SetFileCount(fileCount);
		nzbInfo->SetParkedFileCount(parkedFileCount);
		nzbInfo->SetMessageCount(messageCount);
	}
	else
	{
		if (formatVersion >= 19)
		{
			int unpackCleanedUpDisk;
			if (fscanf(infile, "%i\n", &unpackCleanedUpDisk) != 1) goto error;
			nzbInfo->SetUnpackCleanedUpDisk((bool)unpackCleanedUpDisk);
		}

		int fileCount;
		if (fscanf(infile, "%i\n", &fileCount) != 1) goto error;
		nzbInfo->SetFileCount(fileCount);

		if (formatVersion >= 10)
		{
			if (fscanf(infile, "%i\n", &fileCount) != 1) goto error;
			nzbInfo->SetParkedFileCount(fileCount);
		}
	}

	if (formatVersion >= 44)
	{
		int minTime, maxTime;
		if (fscanf(infile, "%i,%i\n", &minTime, &maxTime) != 2) goto error;
		nzbInfo->SetMinTime((time_t)minTime);
		nzbInfo->SetMaxTime((time_t)maxTime);
	}

	if (formatVersion >= 51)
	{
		int parFull, forceParFull, forceRepair, extraParBlocks = 0;
		if (formatVersion >= 55)
		{
			if (fscanf(infile, "%i,%i,%i,%i\n", &parFull, &forceParFull, &forceRepair, &extraParBlocks) != 4) goto error;
		}
		else
		{
			if (fscanf(infile, "%i,%i,%i\n", &parFull, &forceParFull, &forceRepair) != 3) goto error;
		}
		nzbInfo->SetParFull((bool)parFull);
		nzbInfo->SetExtraParBlocks(extraParBlocks);
		if (nzbInfo->GetPostInfo())
		{
			nzbInfo->GetPostInfo()->SetForceParFull((bool)forceParFull);
			nzbInfo->GetPostInfo()->SetForceRepair((bool)forceRepair);
		}
	}

	if (true) // clang requires a block for goto to work
	{
		uint32 fullContentHash = 0, filteredContentHash = 0;
		if (formatVersion >= 34)
		{
			if (fscanf(infile, "%u,%u\n", &fullContentHash, &filteredContentHash) != 2) goto error;
		}
		else if (formatVersion >= 32)
		{
			if (fscanf(infile, "%u\n", &fullContentHash) != 1) goto error;
		}
		nzbInfo->SetFullContentHash(fullContentHash);
		nzbInfo->SetFilteredContentHash(filteredContentHash);
	}

	if (formatVersion >= 28)
	{
		uint32 High1, Low1, High2, Low2, High3, Low3;
		if (fscanf(infile, "%u,%u,%u,%u,%u,%u\n", &High1, &Low1, &High2, &Low2, &High3, &Low3) != 6) goto error;
		nzbInfo->SetSize(Util::JoinInt64(High1, Low1));
		nzbInfo->SetSuccessSize(Util::JoinInt64(High2, Low2));
		nzbInfo->SetFailedSize(Util::JoinInt64(High3, Low3));
		nzbInfo->SetCurrentSuccessSize(nzbInfo->GetSuccessSize());
		nzbInfo->SetCurrentFailedSize(nzbInfo->GetFailedSize());

		if (fscanf(infile, "%u,%u,%u,%u,%u,%u\n", &High1, &Low1, &High2, &Low2, &High3, &Low3) != 6) goto error;
		nzbInfo->SetParSize(Util::JoinInt64(High1, Low1));
		nzbInfo->SetParSuccessSize(Util::JoinInt64(High2, Low2));
		nzbInfo->SetParFailedSize(Util::JoinInt64(High3, Low3));
		nzbInfo->SetParCurrentSuccessSize(nzbInfo->GetParSuccessSize());
		nzbInfo->SetParCurrentFailedSize(nzbInfo->GetParFailedSize());
	}
	else
	{
		uint32 High, Low;
		if (fscanf(infile, "%u,%u\n", &High, &Low) != 2) goto error;
		nzbInfo->SetSize(Util::JoinInt64(High, Low));
	}

	if (formatVersion >= 30)
	{
		int totalArticles, successArticles, failedArticles;
		if (fscanf(infile, "%i,%i,%i\n", &totalArticles, &successArticles, &failedArticles) != 3) goto error;
		nzbInfo->SetTotalArticles(totalArticles);
		nzbInfo->SetSuccessArticles(successArticles);
		nzbInfo->SetFailedArticles(failedArticles);
		nzbInfo->SetCurrentSuccessArticles(successArticles);
		nzbInfo->SetCurrentFailedArticles(failedArticles);
	}

	if (formatVersion >= 31)
	{
		if (!infile.ReadLine(buf, sizeof(buf))) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		if (formatVersion < 36) ConvertDupeKey(buf, sizeof(buf));
		nzbInfo->SetDupeKey(buf);
	}

	if (true) // clang requires a block for goto to work
	{
		int dupeMode = 0, dupeScore = 0;
		if (formatVersion >= 39)
		{
			if (fscanf(infile, "%i,%i\n", &dupeMode, &dupeScore) != 2) goto error;
		}
		else if (formatVersion >= 31)
		{
			int dupe;
			if (fscanf(infile, "%i,%i,%i\n", &dupe, &dupeMode, &dupeScore) != 3) goto error;
		}
		nzbInfo->SetDupeMode((EDupeMode)dupeMode);
		nzbInfo->SetDupeScore(dupeScore);
	}

	if (formatVersion >= 48)
	{
		uint32 High1, Low1, downloadSec, postTotalSec, parSec, repairSec, unpackSec;
		if (fscanf(infile, "%u,%u,%i,%i,%i,%i,%i\n", &High1, &Low1, &downloadSec, &postTotalSec, &parSec, &repairSec, &unpackSec) != 7) goto error;
		nzbInfo->SetDownloadedSize(Util::JoinInt64(High1, Low1));
		nzbInfo->SetDownloadSec(downloadSec);
		nzbInfo->SetPostTotalSec(postTotalSec);
		nzbInfo->SetParSec(parSec);
		nzbInfo->SetRepairSec(repairSec);
		nzbInfo->SetUnpackSec(unpackSec);
	}

	if (formatVersion >= 4)
	{
		int fileCount;
		if (fscanf(infile, "%i\n", &fileCount) != 1) goto error;
		for (int i = 0; i < fileCount; i++)
		{
			if (!infile.ReadLine(buf, sizeof(buf))) goto error;
			if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'

			int id = 0;
			char* fileName = buf;
			int status = 0;
			uint32 crc = 0;

			if (formatVersion >= 49)
			{
				if (formatVersion >= 50)
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
			}

			nzbInfo->GetCompletedFiles()->emplace_back(id, fileName, (CompletedFile::EStatus)status, crc);
		}
	}

	if (formatVersion >= 6)
	{
		int parameterCount;
		if (fscanf(infile, "%i\n", &parameterCount) != 1) goto error;
		for (int i = 0; i < parameterCount; i++)
		{
			if (!infile.ReadLine(buf, sizeof(buf))) goto error;
			if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'

			char* value = strchr(buf, '=');
			if (value)
			{
				*value = '\0';
				value++;
				nzbInfo->GetParameters()->SetParameter(buf, value);
			}
		}
	}

	if (formatVersion >= 23)
	{
		int scriptCount;
		if (fscanf(infile, "%i\n", &scriptCount) != 1) goto error;
		for (int i = 0; i < scriptCount; i++)
		{
			if (!infile.ReadLine(buf, sizeof(buf))) goto error;
			if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'

			char* scriptName = strchr(buf, ',');
			if (scriptName)
			{
				scriptName++;
				int status = atoi(buf);
				if (status > 1 && formatVersion < 25) status--;
				nzbInfo->GetScriptStatuses()->emplace_back(scriptName, (ScriptStatus::EStatus)status);
			}
		}
	}

	if (formatVersion >= 30)
	{
		if (!LoadServerStats(nzbInfo->GetServerStats(), servers, infile)) goto error;
		nzbInfo->GetCurrentServerStats()->ListOp(nzbInfo->GetServerStats(), ServerStatList::soSet);
	}

	if (formatVersion >= 11 && formatVersion < 52)
	{
		int logCount;
		if (fscanf(infile, "%i\n", &logCount) != 1) goto error;
		for (int i = 0; i < logCount; i++)
		{
			if (!infile.ReadLine(buf, sizeof(buf))) goto error;
		}
	}

	if (formatVersion < 26)
	{
		NzbParameter* unpackParameter = nzbInfo->GetParameters()->Find("*Unpack:", false);
		if (!unpackParameter)
		{
			nzbInfo->GetParameters()->SetParameter("*Unpack:", g_Options->GetUnpack() ? "yes" : "no");
		}
	}

	if (formatVersion >= 43)
	{
		int fileCount;
		if (fscanf(infile, "%i\n", &fileCount) != 1) goto error;
		for (int i = 0; i < fileCount; i++)
		{
			uint32 id, paused, time = 0;
			int priority = 0, extraPriority = 0;

			if (formatVersion >= 44)
			{
				if (fscanf(infile, "%i,%i,%i,%i\n", &id, &paused, &time, &extraPriority) != 4) goto error;
			}
			else
			{
				if (fscanf(infile, "%i,%i,%i,%i,%i\n", &id, &paused, &time, &priority, &extraPriority) != 5) goto error;
				nzbInfo->SetPriority(priority);
			}

			BString<1024> fileName("%s%c%i", g_Options->GetQueueDir(), PATH_SEPARATOR, id);
			FileInfo* fileInfo = new FileInfo();
			bool res = LoadFileInfo(fileInfo, fileName, true, false);
			if (res)
			{
				fileInfo->SetId(id);
				fileInfo->SetPaused(paused);
				fileInfo->SetTime(time);
				fileInfo->SetExtraPriority(extraPriority != 0);
				fileInfo->SetNzbInfo(nzbInfo);
				if (formatVersion < 30)
				{
					nzbInfo->SetTotalArticles(nzbInfo->GetTotalArticles() + fileInfo->GetTotalArticles());
				}
				nzbInfo->GetFileList()->push_back(fileInfo);
			}
			else
			{
				delete fileInfo;
			}
		}
	}

	return true;

error:
	error("Error reading nzb info from disk");
	return false;
}

bool DiskState::LoadFileQueue12(NzbList* nzbList, NzbList* sortList, DiskFile& infile, int formatVersion)
{
	debug("Loading file queue from disk");

	int size;
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		uint32 id, nzbIndex, paused;
		uint32 time = 0;
		int priority = 0, extraPriority = 0;
		if (formatVersion >= 17)
		{
			if (fscanf(infile, "%i,%i,%i,%i,%i,%i\n", &id, &nzbIndex, &paused, &time, &priority, &extraPriority) != 6) goto error;
		}
		else if (formatVersion >= 14)
		{
			if (fscanf(infile, "%i,%i,%i,%i,%i\n", &id, &nzbIndex, &paused, &time, &priority) != 5) goto error;
		}
		else if (formatVersion >= 12)
		{
			if (fscanf(infile, "%i,%i,%i,%i\n", &id, &nzbIndex, &paused, &time) != 4) goto error;
		}
		else
		{
			if (fscanf(infile, "%i,%i,%i\n", &id, &nzbIndex, &paused) != 3) goto error;
		}
		if (nzbIndex > nzbList->size()) goto error;

		BString<1024> fileName("%s%c%i", g_Options->GetQueueDir(), PATH_SEPARATOR, id);
		FileInfo* fileInfo = new FileInfo();
		bool res = LoadFileInfo(fileInfo, fileName, true, false);
		if (res)
		{
			NzbInfo* nzbInfo = nzbList->at(nzbIndex - 1);
			nzbInfo->SetPriority(priority);
			fileInfo->SetId(id);
			fileInfo->SetPaused(paused);
			fileInfo->SetTime(time);
			fileInfo->SetExtraPriority(extraPriority != 0);
			fileInfo->SetNzbInfo(nzbInfo);
			if (formatVersion < 30)
			{
				nzbInfo->SetTotalArticles(nzbInfo->GetTotalArticles() + fileInfo->GetTotalArticles());
			}
			nzbInfo->GetFileList()->push_back(fileInfo);

			if (sortList && std::find(sortList->begin(), sortList->end(), nzbInfo) == sortList->end())
			{
				sortList->push_back(nzbInfo);
			}
		}
		else
		{
			delete fileInfo;
		}
	}

	return true;

error:
	error("Error reading file queue from disk");
	return false;
}

void DiskState::SaveServerStats(ServerStatList* serverStatList, DiskFile& outfile)
{
	outfile.Print("%i\n", (int)serverStatList->size());
	for (ServerStat& serverStat : serverStatList)
	{
		outfile.Print("%i,%i,%i\n", serverStat.GetServerId(), serverStat.GetSuccessArticles(), serverStat.GetFailedArticles());
	}
}

bool DiskState::LoadServerStats(ServerStatList* serverStatList, Servers* servers, DiskFile& infile)
{
	int statCount;
	if (fscanf(infile, "%i\n", &statCount) != 1) goto error;
	for (int i = 0; i < statCount; i++)
	{
		int serverId, successArticles, failedArticles;
		if (fscanf(infile, "%i,%i,%i\n", &serverId, &successArticles, &failedArticles) != 3) goto error;

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
	BString<1024> fileName("%s%c%i", g_Options->GetQueueDir(), PATH_SEPARATOR, fileInfo->GetId());
	return SaveFileInfo(fileInfo, fileName);
}

bool DiskState::SaveFileInfo(FileInfo* fileInfo, const char* filename)
{
	debug("Saving FileInfo to disk");

	DiskFile outfile;

	if (!outfile.Open(filename, DiskFile::omWrite))
	{
		error("Error saving diskstate: could not create file %s", filename);
		return false;
	}

	outfile.Print("%s%i\n", FORMATVERSION_SIGNATURE, 3);

	outfile.Print("%s\n", fileInfo->GetSubject());
	outfile.Print("%s\n", fileInfo->GetFilename());

	uint32 High, Low;
	Util::SplitInt64(fileInfo->GetSize(), &High, &Low);
	outfile.Print("%u,%u\n", High, Low);

	Util::SplitInt64(fileInfo->GetMissedSize(), &High, &Low);
	outfile.Print("%u,%u\n", High, Low);

	outfile.Print("%i\n", (int)fileInfo->GetParFile());
	outfile.Print("%i,%i\n", fileInfo->GetTotalArticles(), fileInfo->GetMissedArticles());

	outfile.Print("%i\n", (int)fileInfo->GetGroups()->size());
	for (CString& group : fileInfo->GetGroups())
	{
		outfile.Print("%s\n", *group);
	}

	outfile.Print("%i\n", (int)fileInfo->GetArticles()->size());
	for (ArticleInfo* articleInfo : fileInfo->GetArticles())
	{
		outfile.Print("%i,%i\n", articleInfo->GetPartNumber(), articleInfo->GetSize());
		outfile.Print("%s\n", articleInfo->GetMessageId());
	}

	outfile.Close();
	return true;
}

bool DiskState::LoadArticles(FileInfo* fileInfo)
{
	BString<1024> fileName("%s%c%i", g_Options->GetQueueDir(), PATH_SEPARATOR, fileInfo->GetId());
	return LoadFileInfo(fileInfo, fileName, false, true);
}

bool DiskState::LoadFileInfo(FileInfo* fileInfo, const char * filename, bool fileSummary, bool articles)
{
	debug("Loading FileInfo from disk");

	DiskFile infile;

	if (!infile.Open(filename, DiskFile::omRead))
	{
		error("Error reading diskstate: could not open file %s", filename);
		return false;
	}

	char buf[1024];
	int formatVersion = 0;

	if (infile.ReadLine(buf, sizeof(buf)))
	{
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		formatVersion = ParseFormatVersion(buf);
		if (formatVersion > 3)
		{
			error("Could not load diskstate due to file version mismatch");
			goto error;
		}
	}
	else
	{
		goto error;
	}

	if (formatVersion >= 2)
	{
		if (!infile.ReadLine(buf, sizeof(buf))) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	}

	if (fileSummary) fileInfo->SetSubject(buf);

	if (!infile.ReadLine(buf, sizeof(buf))) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	if (fileSummary) fileInfo->SetFilename(buf);

	if (formatVersion < 2)
	{
		int filenameConfirmed;
		if (fscanf(infile, "%i\n", &filenameConfirmed) != 1) goto error;
		if (fileSummary) fileInfo->SetFilenameConfirmed(filenameConfirmed);
	}

	uint32 High, Low;
	if (fscanf(infile, "%u,%u\n", &High, &Low) != 2) goto error;
	if (fileSummary) fileInfo->SetSize(Util::JoinInt64(High, Low));
	if (fileSummary) fileInfo->SetRemainingSize(fileInfo->GetSize());

	if (formatVersion >= 2)
	{
		if (fscanf(infile, "%u,%u\n", &High, &Low) != 2) goto error;
		if (fileSummary) fileInfo->SetMissedSize(Util::JoinInt64(High, Low));
		if (fileSummary) fileInfo->SetRemainingSize(fileInfo->GetSize() - fileInfo->GetMissedSize());

		int parFile;
		if (fscanf(infile, "%i\n", &parFile) != 1) goto error;
		if (fileSummary) fileInfo->SetParFile((bool)parFile);
	}

	if (formatVersion >= 3)
	{
		int totalArticles, missedArticles;
		if (fscanf(infile, "%i,%i\n", &totalArticles, &missedArticles) != 2) goto error;
		if (fileSummary) fileInfo->SetTotalArticles(totalArticles);
		if (fileSummary) fileInfo->SetMissedArticles(missedArticles);
	}

	int size;
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		if (!infile.ReadLine(buf, sizeof(buf))) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		if (fileSummary) fileInfo->GetGroups()->push_back(buf);
	}

	if (fscanf(infile, "%i\n", &size) != 1) goto error;

	if (formatVersion < 3 && fileSummary)
	{
		fileInfo->SetTotalArticles(size);
	}

	if (articles)
	{
		for (int i = 0; i < size; i++)
		{
			int PartNumber, PartSize;
			if (fscanf(infile, "%i,%i\n", &PartNumber, &PartSize) != 2) goto error;

			if (!infile.ReadLine(buf, sizeof(buf))) goto error;
			if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'

			ArticleInfo* articleInfo = new ArticleInfo();
			articleInfo->SetPartNumber(PartNumber);
			articleInfo->SetSize(PartSize);
			articleInfo->SetMessageId(buf);
			fileInfo->GetArticles()->push_back(articleInfo);
		}
	}

	infile.Close();
	return true;

error:
	infile.Close();
	error("Error reading diskstate for file %s", filename);
	return false;
}

bool DiskState::SaveFileState(FileInfo* fileInfo, bool completed)
{
	debug("Saving FileState to disk");

	BString<1024> filename("%s%c%i%s", g_Options->GetQueueDir(), PATH_SEPARATOR,
		fileInfo->GetId(), completed ? "c" : "s");
	DiskFile outfile;

	if (!outfile.Open(filename, DiskFile::omWrite))
	{
		error("Error saving diskstate: could not create file %s", *filename);
		return false;
	}

	outfile.Print("%s%i\n", FORMATVERSION_SIGNATURE, 2);

	outfile.Print("%i,%i\n", fileInfo->GetSuccessArticles(), fileInfo->GetFailedArticles());

	uint32 High1, Low1, High2, Low2, High3, Low3;
	Util::SplitInt64(fileInfo->GetRemainingSize(), &High1, &Low1);
	Util::SplitInt64(fileInfo->GetSuccessSize(), &High2, &Low2);
	Util::SplitInt64(fileInfo->GetFailedSize(), &High3, &Low3);
	outfile.Print("%u,%u,%u,%u,%u,%u\n", High1, Low1, High2, Low2, High3, Low3);

	SaveServerStats(fileInfo->GetServerStats(), outfile);

	outfile.Print("%i\n", (int)fileInfo->GetArticles()->size());
	for (ArticleInfo* articleInfo : fileInfo->GetArticles())
	{
		outfile.Print("%i,%u,%i,%u\n", (int)articleInfo->GetStatus(), (uint32)articleInfo->GetSegmentOffset(),
			articleInfo->GetSegmentSize(), (uint32)articleInfo->GetCrc());
	}

	outfile.Close();
	return true;
}

bool DiskState::LoadFileState(FileInfo* fileInfo, Servers* servers, bool completed)
{
	bool hasArticles = !fileInfo->GetArticles()->empty();

	BString<1024> filename("%s%c%i%s", g_Options->GetQueueDir(), PATH_SEPARATOR,
		fileInfo->GetId(), completed ? "c" : "s");
	DiskFile infile;

	if (!infile.Open(filename, DiskFile::omRead))
	{
		error("Error reading diskstate: could not open file %s", *filename);
		return false;
	}

	char buf[1024];
	int formatVersion = 0;

	if (infile.ReadLine(buf, sizeof(buf)))
	{
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		formatVersion = ParseFormatVersion(buf);
		if (formatVersion > 2)
		{
			error("Could not load diskstate due to file version mismatch");
			goto error;
		}
	}
	else
	{
		goto error;
	}

	int successArticles, failedArticles;
	if (fscanf(infile, "%i,%i\n", &successArticles, &failedArticles) != 2) goto error;
	fileInfo->SetSuccessArticles(successArticles);
	fileInfo->SetFailedArticles(failedArticles);

	uint32 High1, Low1, High2, Low2, High3, Low3;
	if (fscanf(infile, "%u,%u,%u,%u,%u,%u\n", &High1, &Low1, &High2, &Low2, &High3, &Low3) != 6) goto error;
	fileInfo->SetRemainingSize(Util::JoinInt64(High1, Low1));
	fileInfo->SetSuccessSize(Util::JoinInt64(High2, Low3));
	fileInfo->SetFailedSize(Util::JoinInt64(High3, Low3));

	if (!LoadServerStats(fileInfo->GetServerStats(), servers, infile)) goto error;

	int completedArticles;
	completedArticles = 0; //clang requires initialization in a separate line (due to goto statements)

	int size;
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		if (!hasArticles)
		{
			fileInfo->GetArticles()->push_back(new ArticleInfo());
		}
		ArticleInfo* pa = fileInfo->GetArticles()->at(i);

		int statusInt;

		if (formatVersion >= 2)
		{
			uint32 segmentOffset, crc;
			int segmentSize;
			if (fscanf(infile, "%i,%u,%i,%u\n", &statusInt, &segmentOffset, &segmentSize, &crc) != 4) goto error;
			pa->SetSegmentOffset(segmentOffset);
			pa->SetSegmentSize(segmentSize);
			pa->SetCrc(crc);
		}
		else
		{
			if (fscanf(infile, "%i\n", &statusInt) != 1) goto error;
		}

		ArticleInfo::EStatus status = (ArticleInfo::EStatus)statusInt;

		if (status == ArticleInfo::aiRunning)
		{
			status = ArticleInfo::aiUndefined;
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
	error("Error reading diskstate for file %s", *filename);
	return false;
}

void DiskState::DiscardFiles(NzbInfo* nzbInfo)
{
	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
		DiscardFile(fileInfo, true, true, true);
	}

	BString<1024> filename;

	for (CompletedFile& completedFile : nzbInfo->GetCompletedFiles())
	{
		if (completedFile.GetStatus() != CompletedFile::cfSuccess && completedFile.GetId() > 0)
		{
			filename.Format("%s%c%i", g_Options->GetQueueDir(), PATH_SEPARATOR, completedFile.GetId());
			FileSystem::DeleteFile(filename);

			filename.Format("%s%c%is", g_Options->GetQueueDir(), PATH_SEPARATOR, completedFile.GetId());
			FileSystem::DeleteFile(filename);

			filename.Format("%s%c%ic", g_Options->GetQueueDir(), PATH_SEPARATOR, completedFile.GetId());
			FileSystem::DeleteFile(filename);
		}
	}

	filename.Format("%s%cn%i.log", g_Options->GetQueueDir(), PATH_SEPARATOR, nzbInfo->GetId());
	FileSystem::DeleteFile(filename);
}

bool DiskState::LoadPostQueue12(DownloadQueue* downloadQueue, NzbList* nzbList, DiskFile& infile, int formatVersion)
{
	debug("Loading post-queue from disk");

	int size;
	char buf[10240];

	// load post-infos
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		PostInfo* postInfo = nullptr;
		int nzbId = 0;
		uint32 nzbIndex = 0, stage, dummy;
		if (formatVersion < 19)
		{
			if (fscanf(infile, "%i,%i,%i,%i\n", &nzbIndex, &dummy, &dummy, &stage) != 4) goto error;
		}
		else if (formatVersion < 22)
		{
			if (fscanf(infile, "%i,%i,%i,%i\n", &nzbIndex, &dummy, &dummy, &stage) != 4) goto error;
		}
		else if (formatVersion < 43)
		{
			if (fscanf(infile, "%i,%i\n", &nzbIndex, &stage) != 2) goto error;
		}
		else
		{
			if (fscanf(infile, "%i,%i\n", &nzbId, &stage) != 2) goto error;
		}
		if (formatVersion < 18 && stage > (int)PostInfo::ptVerifyingRepaired) stage++;
		if (formatVersion < 21 && stage > (int)PostInfo::ptVerifyingRepaired) stage++;
		if (formatVersion < 20 && stage > (int)PostInfo::ptUnpacking) stage++;

		NzbInfo* nzbInfo = nullptr;

		if (formatVersion < 43)
		{
			nzbInfo = nzbList->at(nzbIndex - 1);
			if (!nzbInfo) goto error;
		}
		else
		{
			nzbInfo = FindNzbInfo(downloadQueue, nzbId);
			if (!nzbInfo) goto error;
		}

		nzbInfo->EnterPostProcess();
		postInfo = nzbInfo->GetPostInfo();

		postInfo->SetStage((PostInfo::EStage)stage);

		// InfoName, ignore
		if (!infile.ReadLine(buf, sizeof(buf))) goto error;

		if (formatVersion < 22)
		{
			// ParFilename, ignore
			if (!infile.ReadLine(buf, sizeof(buf))) goto error;
		}
	}

	return true;

error:
	error("Error reading diskstate for post-processor queue");
	return false;
}

/*
 * Loads post-queue created with older versions of nzbget.
 * Returns true if successful, false if not
 */
bool DiskState::LoadPostQueue5(DownloadQueue* downloadQueue, NzbList* nzbList)
{
	debug("Loading post-queue from disk");

	BString<1024> fileName("%s%c%s", g_Options->GetQueueDir(), PATH_SEPARATOR, "postq");

	if (!FileSystem::FileExists(fileName))
	{
		return true;
	}

	DiskFile infile;

	if (!infile.Open(fileName, DiskFile::omRead))
	{
		error("Error reading diskstate: could not open file %s", *fileName);
		return false;
	}

	char FileSignatur[128];
	infile.ReadLine(FileSignatur, sizeof(FileSignatur));
	int formatVersion = ParseFormatVersion(FileSignatur);
	if (formatVersion < 3 || formatVersion > 7)
	{
		error("Could not load diskstate due to file version mismatch");
		infile.Close();
		return false;
	}

	int size;
	char buf[10240];
	int intValue;

	// load file-infos
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		if (!infile.ReadLine(buf, sizeof(buf))) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'

		// find NZBInfo based on NZBFilename
		NzbInfo* nzbInfo = nullptr;
		for (NzbInfo* nzbInfo2 : nzbList)
		{
			if (!strcmp(nzbInfo2->GetFilename(), buf))
			{
				nzbInfo = nzbInfo2;
				break;
			}
		}

		bool newNzbInfo = !nzbInfo;
		if (newNzbInfo)
		{
			nzbInfo = new NzbInfo();
			nzbList->push_front(nzbInfo);
			nzbInfo->SetFilename(buf);
		}

		nzbInfo->EnterPostProcess();
		PostInfo* postInfo = nzbInfo->GetPostInfo();

		if (!infile.ReadLine(buf, sizeof(buf))) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		if (newNzbInfo)
		{
			nzbInfo->SetDestDir(buf);
		}

		// ParFilename, ignore
		if (!infile.ReadLine(buf, sizeof(buf))) goto error;

		// InfoName, ignore
		if (!infile.ReadLine(buf, sizeof(buf))) goto error;

		if (formatVersion >= 4)
		{
			if (!infile.ReadLine(buf, sizeof(buf))) goto error;
			if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
			if (newNzbInfo)
			{
				nzbInfo->SetCategory(buf);
			}
		}
		else
		{
			if (newNzbInfo)
			{
				nzbInfo->SetCategory("");
			}
		}

		if (formatVersion >= 5)
		{
			if (!infile.ReadLine(buf, sizeof(buf))) goto error;
			if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
			if (newNzbInfo)
			{
				nzbInfo->SetQueuedFilename(buf);
			}
		}
		else
		{
			if (newNzbInfo)
			{
				nzbInfo->SetQueuedFilename("");
			}
		}

		int parCheck;
		if (fscanf(infile, "%i\n", &parCheck) != 1) goto error; // ParCheck

		if (fscanf(infile, "%i\n", &intValue) != 1) goto error;
		nzbInfo->SetParStatus(parCheck ? (NzbInfo::EParStatus)intValue : NzbInfo::psSkipped);

		if (formatVersion < 7)
		{
			// skip old field ParFailed, not used anymore
			if (fscanf(infile, "%i\n", &intValue) != 1) goto error;
		}

		if (fscanf(infile, "%i\n", &intValue) != 1) goto error;
		postInfo->SetStage((PostInfo::EStage)intValue);

		if (formatVersion >= 6)
		{
			int parameterCount;
			if (fscanf(infile, "%i\n", &parameterCount) != 1) goto error;
			for (int i = 0; i < parameterCount; i++)
			{
				if (!infile.ReadLine(buf, sizeof(buf))) goto error;
				if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'

				char* value = strchr(buf, '=');
				if (value)
				{
					*value = '\0';
					value++;
					if (newNzbInfo)
					{
						nzbInfo->GetParameters()->SetParameter(buf, value);
					}
				}
			}
		}
	}

	infile.Close();
	return true;

error:
	infile.Close();
	error("Error reading diskstate for file %s", *fileName);
	return false;
}

bool DiskState::LoadUrlQueue12(DownloadQueue* downloadQueue, DiskFile& infile, int formatVersion)
{
	debug("Loading url-queue from disk");
	int size;

	// load url-infos
	if (fscanf(infile, "%i\n", &size) != 1) goto error;

	for (int i = 0; i < size; i++)
	{
		NzbInfo* nzbInfo = new NzbInfo();
		if (!LoadUrlInfo12(nzbInfo, infile, formatVersion)) goto error;
		downloadQueue->GetQueue()->push_back(nzbInfo);
	}

	return true;

error:
	error("Error reading diskstate for url-queue");
	return false;
}

bool DiskState::LoadUrlInfo12(NzbInfo* nzbInfo, DiskFile& infile, int formatVersion)
{
	char buf[10240];

	if (formatVersion >= 24)
	{
		int id;
		if (fscanf(infile, "%i\n", &id) != 1) goto error;
		nzbInfo->SetId(id);
	}

	int statusDummy, priority;
	if (fscanf(infile, "%i,%i\n", &statusDummy, &priority) != 2) goto error;
	nzbInfo->SetPriority(priority);

	if (formatVersion >= 16)
	{
		int addTopDummy, addPaused;
		if (fscanf(infile, "%i,%i\n", &addTopDummy, &addPaused) != 2) goto error;
		nzbInfo->SetAddUrlPaused(addPaused);
	}

	if (!infile.ReadLine(buf, sizeof(buf))) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	nzbInfo->SetUrl(buf);

	if (!infile.ReadLine(buf, sizeof(buf))) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	nzbInfo->SetFilename(buf);

	if (!infile.ReadLine(buf, sizeof(buf))) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	nzbInfo->SetCategory(buf);

	if (formatVersion >= 31)
	{
		if (!infile.ReadLine(buf, sizeof(buf))) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		if (formatVersion < 36) ConvertDupeKey(buf, sizeof(buf));
		nzbInfo->SetDupeKey(buf);

		int dupeMode, dupeScore;
		if (fscanf(infile, "%i,%i\n", &dupeMode, &dupeScore) != 2) goto error;
		nzbInfo->SetDupeMode((EDupeMode)dupeMode);
		nzbInfo->SetDupeScore(dupeScore);
	}

	return true;

error:
	return false;
}

void DiskState::SaveDupInfo(DupInfo* dupInfo, DiskFile& outfile)
{
	uint32 High, Low;
	Util::SplitInt64(dupInfo->GetSize(), &High, &Low);
	outfile.Print("%i,%u,%u,%u,%u,%i,%i\n", (int)dupInfo->GetStatus(), High, Low,
		dupInfo->GetFullContentHash(), dupInfo->GetFilteredContentHash(),
		dupInfo->GetDupeScore(), (int)dupInfo->GetDupeMode());
	outfile.Print("%s\n", dupInfo->GetName());
	outfile.Print("%s\n", dupInfo->GetDupeKey());
}

bool DiskState::LoadDupInfo(DupInfo* dupInfo, DiskFile& infile, int formatVersion)
{
	char buf[1024];

	int status;
	uint32 High, Low;
	uint32 fullContentHash, filteredContentHash = 0;
	int dupeScore, dupe = 0, dupeMode = 0;

	if (formatVersion >= 39)
	{
		if (fscanf(infile, "%i,%u,%u,%u,%u,%i,%i\n", &status, &High, &Low, &fullContentHash, &filteredContentHash, &dupeScore, &dupeMode) != 7) goto error;
	}
	else if (formatVersion >= 38)
	{
		if (fscanf(infile, "%i,%u,%u,%u,%u,%i,%i,%i\n", &status, &High, &Low, &fullContentHash, &filteredContentHash, &dupeScore, &dupe, &dupeMode) != 8) goto error;
	}
	else if (formatVersion >= 37)
	{
		if (fscanf(infile, "%i,%u,%u,%u,%u,%i,%i\n", &status, &High, &Low, &fullContentHash, &filteredContentHash, &dupeScore, &dupe) != 7) goto error;
	}
	else if (formatVersion >= 34)
	{
		if (fscanf(infile, "%i,%u,%u,%u,%u,%i\n", &status, &High, &Low, &fullContentHash, &filteredContentHash, &dupeScore) != 6) goto error;
	}
	else
	{
		if (fscanf(infile, "%i,%u,%u,%u,%i\n", &status, &High, &Low, &fullContentHash, &dupeScore) != 5) goto error;
	}

	dupInfo->SetStatus((DupInfo::EStatus)status);
	dupInfo->SetFullContentHash(fullContentHash);
	dupInfo->SetFilteredContentHash(filteredContentHash);
	dupInfo->SetSize(Util::JoinInt64(High, Low));
	dupInfo->SetDupeScore(dupeScore);
	dupInfo->SetDupeMode((EDupeMode)dupeMode);

	if (!infile.ReadLine(buf, sizeof(buf))) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	dupInfo->SetName(buf);

	if (!infile.ReadLine(buf, sizeof(buf))) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	if (formatVersion < 36) ConvertDupeKey(buf, sizeof(buf));
	dupInfo->SetDupeKey(buf);

	return true;

error:
	return false;
}

void DiskState::SaveHistory(DownloadQueue* downloadQueue, DiskFile& outfile)
{
	debug("Saving history to disk");

	outfile.Print("%i\n", (int)downloadQueue->GetHistory()->size());
	for (HistoryInfo* historyInfo : downloadQueue->GetHistory())
	{
		outfile.Print("%i,%i,%i\n", historyInfo->GetId(), (int)historyInfo->GetKind(), (int)historyInfo->GetTime());

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

bool DiskState::LoadHistory(DownloadQueue* downloadQueue, NzbList* nzbList, Servers* servers, DiskFile& infile, int formatVersion)
{
	debug("Loading history from disk");

	int size;
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		HistoryInfo* historyInfo = nullptr;
		HistoryInfo::EKind kind = HistoryInfo::hkNzb;
		int id = 0;
		int time;

		if (formatVersion >= 33)
		{
			int kindval = 0;
			if (fscanf(infile, "%i,%i,%i\n", &id, &kindval, &time) != 3) goto error;
			kind = (HistoryInfo::EKind)kindval;
		}
		else
		{
			if (formatVersion >= 24)
			{
				if (fscanf(infile, "%i\n", &id) != 1) goto error;
			}

			if (formatVersion >= 15)
			{
				int kindval = 0;
				if (fscanf(infile, "%i\n", &kindval) != 1) goto error;
				kind = (HistoryInfo::EKind)kindval;
			}
		}

		if (kind == HistoryInfo::hkNzb)
		{
			NzbInfo* nzbInfo = nullptr;

			if (formatVersion < 43)
			{
				uint32 nzbIndex;
				if (fscanf(infile, "%i\n", &nzbIndex) != 1) goto error;
				nzbInfo = nzbList->at(nzbIndex - 1);
			}
			else
			{
				nzbInfo = new NzbInfo();
				if (!LoadNzbInfo(nzbInfo, servers, infile, formatVersion)) goto error;
				nzbInfo->LeavePostProcess();
			}

			historyInfo = new HistoryInfo(nzbInfo);

			if (formatVersion < 28 && nzbInfo->GetParStatus() == 0 &&
				nzbInfo->GetUnpackStatus() == 0 && nzbInfo->GetMoveStatus() == 0)
			{
				nzbInfo->SetDeleteStatus(NzbInfo::dsManual);
			}
		}
		else if (kind == HistoryInfo::hkUrl)
		{
			NzbInfo* nzbInfo = new NzbInfo();
			if (formatVersion >= 46)
			{
				if (!LoadNzbInfo(nzbInfo, servers, infile, formatVersion)) goto error;
			}
			else
			{
				if (!LoadUrlInfo12(nzbInfo, infile, formatVersion)) goto error;
			}
			historyInfo = new HistoryInfo(nzbInfo);
		}
		else if (kind == HistoryInfo::hkDup)
		{
			DupInfo* dupInfo = new DupInfo();
			if (!LoadDupInfo(dupInfo, infile, formatVersion)) goto error;
			if (formatVersion >= 47)
			{
				dupInfo->SetId(id);
			}
			historyInfo = new HistoryInfo(dupInfo);
		}

		if (formatVersion < 33)
		{
			if (fscanf(infile, "%i\n", &time) != 1) goto error;
		}

		historyInfo->SetTime((time_t)time);

		downloadQueue->GetHistory()->push_back(historyInfo);
	}

	return true;

error:
	error("Error reading diskstate for history");
	return false;
}

/*
* Find index of nzb-info.
*/
int DiskState::FindNzbInfoIndex(NzbList* nzbList, NzbInfo* nzbInfo)
{
	int nzbIndex = 0;
	for (NzbInfo* nzbInfo2 : nzbList)
	{
		nzbIndex++;
		if (nzbInfo2 == nzbInfo)
		{
			break;
		}
	}
	return nzbIndex;
}

/*
* Find nzb-info by id.
*/
NzbInfo* DiskState::FindNzbInfo(DownloadQueue* downloadQueue, int id)
{
	for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
	{
		if (nzbInfo->GetId() == id)
		{
			return nzbInfo;
		}
	}
	return nullptr;
}

/*
 * Deletes whole download queue including history.
 */
void DiskState::DiscardDownloadQueue()
{
	debug("Discarding queue");

	BString<1024> fullFilename("%s%c%s", g_Options->GetQueueDir(), PATH_SEPARATOR, "queue");
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

	BString<1024> fileName("%s%c%s", g_Options->GetQueueDir(), PATH_SEPARATOR, "queue");
	return FileSystem::FileExists(fileName);
}

void DiskState::DiscardFile(FileInfo* fileInfo, bool deleteData, bool deletePartialState, bool deleteCompletedState)
{
	BString<1024> fileName;

	// info and articles file
	if (deleteData)
	{
		fileName.Format("%s%c%i", g_Options->GetQueueDir(), PATH_SEPARATOR, fileInfo->GetId());
		FileSystem::DeleteFile(fileName);
	}

	// partial state file
	if (deletePartialState)
	{
		fileName.Format("%s%c%is", g_Options->GetQueueDir(), PATH_SEPARATOR, fileInfo->GetId());
		FileSystem::DeleteFile(fileName);
	}

	// completed state file
	if (deleteCompletedState)
	{
		fileName.Format("%s%c%ic", g_Options->GetQueueDir(), PATH_SEPARATOR, fileInfo->GetId());
		FileSystem::DeleteFile(fileName);
	}
}

void DiskState::CleanupTempDir(DownloadQueue* downloadQueue)
{
	DirBrowser dir(g_Options->GetTempDir());
	while (const char* filename = dir.Next())
	{
		int id, part;
		if (strstr(filename, ".tmp") || strstr(filename, ".dec") ||
			(sscanf(filename, "%i.%i", &id, &part) == 2))
		{
			BString<1024> fullFilename("%s%c%s", g_Options->GetTempDir(), PATH_SEPARATOR, filename);
			FileSystem::DeleteFile(fullFilename);
		}
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

	StateFile stateFile("feeds", 3);

	if (feeds->empty() && feedHistory->empty())
	{
		stateFile.Discard();
		return true;
	}

	DiskFile* outfile = stateFile.BeginWriteTransaction();
	if (!outfile)
	{
		return false;
	}

	// save status
	SaveFeedStatus(feeds, *outfile);

	// save history
	SaveFeedHistory(feedHistory, *outfile);

	// now rename to dest file name
	return stateFile.FinishWriteTransaction();
}

bool DiskState::LoadFeeds(Feeds* feeds, FeedHistory* feedHistory)
{
	debug("Loading feeds state from disk");

	StateFile stateFile("feeds", 3);

	if (!stateFile.FileExists())
	{
		return true;
	}

	DiskFile* infile = stateFile.BeginReadTransaction();
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

bool DiskState::SaveFeedStatus(Feeds* feeds, DiskFile& outfile)
{
	debug("Saving feed status to disk");

	outfile.Print("%i\n", (int)feeds->size());
	for (FeedInfo* feedInfo : feeds)
	{
		outfile.Print("%s\n", feedInfo->GetUrl());
		outfile.Print("%u\n", feedInfo->GetFilterHash());
		outfile.Print("%i\n", (int)feedInfo->GetLastUpdate());
	}

	return true;
}

bool DiskState::LoadFeedStatus(Feeds* feeds, DiskFile& infile, int formatVersion)
{
	debug("Loading feed status from disk");

	int size;
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		char url[1024];
		if (!infile.ReadLine(url, sizeof(url))) goto error;
		if (url[0] != 0) url[strlen(url)-1] = 0; // remove traling '\n'

		char filter[1024];
		if (formatVersion == 2)
		{
			if (!infile.ReadLine(filter, sizeof(filter))) goto error;
			if (filter[0] != 0) filter[strlen(filter)-1] = 0; // remove traling '\n'
		}

		uint32 filterHash = 0;
		if (formatVersion >= 3)
		{
			if (fscanf(infile, "%u\n", &filterHash) != 1) goto error;
		}

		int lastUpdate = 0;
		if (fscanf(infile, "%i\n", &lastUpdate) != 1) goto error;

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

bool DiskState::SaveFeedHistory(FeedHistory* feedHistory, DiskFile& outfile)
{
	debug("Saving feed history to disk");

	outfile.Print("%i\n", (int)feedHistory->size());
	for (FeedHistoryInfo& feedHistoryInfo : feedHistory)
	{
		outfile.Print("%i,%i\n", (int)feedHistoryInfo.GetStatus(), (int)feedHistoryInfo.GetLastSeen());
		outfile.Print("%s\n", feedHistoryInfo.GetUrl());
	}

	return true;
}

bool DiskState::LoadFeedHistory(FeedHistory* feedHistory, DiskFile& infile, int formatVersion)
{
	debug("Loading feed history from disk");

	int size;
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		int status = 0;
		int lastSeen = 0;
		int r = fscanf(infile, "%i,%i\n", &status, &lastSeen);
		if (r != 2) goto error;

		char url[1024];
		if (!infile.ReadLine(url, sizeof(url))) goto error;
		if (url[0] != 0) url[strlen(url)-1] = 0; // remove traling '\n'

		feedHistory->emplace_back(url, (FeedHistoryInfo::EStatus)(status), (time_t)(lastSeen));
	}

	return true;

error:
	error("Error reading feed history from disk");
	return false;
}

// calculate critical health for old NZBs
void DiskState::CalcCriticalHealth(NzbList* nzbList)
{
	// build list of old NZBs which do not have critical health calculated
	for (NzbInfo* nzbInfo : nzbList)
	{
		if (nzbInfo->CalcCriticalHealth(false) == 1000)
		{
			debug("Calculating critical health for %s", nzbInfo->GetName());

			for (FileInfo* fileInfo : nzbInfo->GetFileList())
			{
				BString<1024> loFileName;
				loFileName = fileInfo->GetFilename();
				for (char* p = loFileName; *p; p++) *p = tolower(*p); // convert string to lowercase
				bool parFile = strstr(loFileName, ".par2");

				fileInfo->SetParFile(parFile);
				if (parFile)
				{
					nzbInfo->SetParSize(nzbInfo->GetParSize() + fileInfo->GetSize());
				}
			}
		}
	}
}

void DiskState::CalcFileStats(DownloadQueue* downloadQueue, int formatVersion)
{
	for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
	{
		CalcNzbFileStats(nzbInfo, formatVersion);
	}

	for (HistoryInfo* historyInfo : downloadQueue->GetHistory())
	{
		if (historyInfo->GetKind() == HistoryInfo::hkNzb)
		{
			CalcNzbFileStats(historyInfo->GetNzbInfo(), formatVersion);
		}
	}
}

void DiskState::CalcNzbFileStats(NzbInfo* nzbInfo, int formatVersion)
{
	int pausedFileCount = 0;
	int remainingParCount = 0;
	int successArticles = 0;
	int failedArticles = 0;
	int64 remainingSize = 0;
	int64 pausedSize = 0;
	int64 successSize = 0;
	int64 failedSize = 0;
	int64 parSuccessSize = 0;
	int64 parFailedSize = 0;

	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
		remainingSize += fileInfo->GetRemainingSize();
		successArticles += fileInfo->GetSuccessArticles();
		failedArticles += fileInfo->GetFailedArticles();
		successSize += fileInfo->GetSuccessSize();
		failedSize += fileInfo->GetFailedSize();

		if (fileInfo->GetPaused())
		{
			pausedSize += fileInfo->GetRemainingSize();
			pausedFileCount++;
		}
		if (fileInfo->GetParFile())
		{
			remainingParCount++;
			parSuccessSize += fileInfo->GetSuccessSize();
			parFailedSize += fileInfo->GetFailedSize();
		}

		nzbInfo->GetCurrentServerStats()->ListOp(fileInfo->GetServerStats(), ServerStatList::soAdd);
	}

	nzbInfo->SetRemainingSize(remainingSize);
	nzbInfo->SetPausedSize(pausedSize);
	nzbInfo->SetPausedFileCount(pausedFileCount);
	nzbInfo->SetRemainingParCount(remainingParCount);

	nzbInfo->SetCurrentSuccessArticles(nzbInfo->GetSuccessArticles() + successArticles);
	nzbInfo->SetCurrentFailedArticles(nzbInfo->GetFailedArticles() + failedArticles);
	nzbInfo->SetCurrentSuccessSize(nzbInfo->GetSuccessSize() + successSize);
	nzbInfo->SetCurrentFailedSize(nzbInfo->GetFailedSize() + failedSize);
	nzbInfo->SetParCurrentSuccessSize(nzbInfo->GetParSuccessSize() + parSuccessSize);
	nzbInfo->SetParCurrentFailedSize(nzbInfo->GetParFailedSize() + parFailedSize);

	if (formatVersion < 44)
	{
		nzbInfo->UpdateMinMaxTime();
	}
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
		if (sscanf(filename, "%i%c", &id, &suffix) == 2 && suffix == 's')
		{
			if (g_Options->GetContinuePartial() && !cacheWasActive)
			{
				for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
				{
					for (FileInfo* fileInfo : nzbInfo->GetFileList())
					{
						if (fileInfo->GetId() == id)
						{
							if (!LoadArticles(fileInfo)) goto error;
							if (!LoadFileState(fileInfo, servers, false)) goto error;
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
	}

	return true;

error:
	return false;
}

void DiskState::ConvertDupeKey(char* buf, int bufsize)
{
	if (strncmp(buf, "rageid=", 7))
	{
		return;
	}

	int rageId = atoi(buf + 7);
	int season = 0;
	int episode = 0;
	char* p = strchr(buf + 7, ',');
	if (p)
	{
		season = atoi(p + 1);
		p = strchr(p + 1, ',');
		if (p)
		{
			episode = atoi(p + 1);
		}
	}

	if (rageId != 0 && season != 0 && episode != 0)
	{
		snprintf(buf, bufsize, "rageid=%i-S%02i-E%02i", rageId, season, episode);
	}
}

bool DiskState::SaveStats(Servers* servers, ServerVolumes* serverVolumes)
{
	debug("Saving stats to disk");

	StateFile stateFile("stats", 3);

	if (servers->empty())
	{
		stateFile.Discard();
		return true;
	}

	DiskFile* outfile = stateFile.BeginWriteTransaction();
	if (!outfile)
	{
		return false;
	}

	// save server names
	SaveServerInfo(servers, *outfile);

	// save stat
	SaveVolumeStat(serverVolumes, *outfile);

	// now rename to dest file name
	return stateFile.FinishWriteTransaction();
}

bool DiskState::LoadStats(Servers* servers, ServerVolumes* serverVolumes, bool* perfectMatch)
{
	debug("Loading stats from disk");

	StateFile stateFile("stats", 3);

	if (!stateFile.FileExists())
	{
		return true;
	}

	DiskFile* infile = stateFile.BeginReadTransaction();
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

bool DiskState::SaveServerInfo(Servers* servers, DiskFile& outfile)
{
	debug("Saving server info to disk");

	outfile.Print("%i\n", (int)servers->size());
	for (NewsServer* newsServer : servers)
	{
		outfile.Print("%s\n", newsServer->GetName());
		outfile.Print("%s\n", newsServer->GetHost());
		outfile.Print("%i\n", newsServer->GetPort());
		outfile.Print("%s\n", newsServer->GetUser());
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

typedef std::deque<ServerRef*> ServerRefList;

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

bool DiskState::LoadServerInfo(Servers* servers, DiskFile& infile, int formatVersion, bool* perfectMatch)
{
	debug("Loading server info from disk");

	ServerRefList serverRefs;
	*perfectMatch = true;

	int size;
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		char name[1024];
		if (!infile.ReadLine(name, sizeof(name))) goto error;
		if (name[0] != 0) name[strlen(name)-1] = 0; // remove traling '\n'

		char host[200];
		if (!infile.ReadLine(host, sizeof(host))) goto error;
		if (host[0] != 0) host[strlen(host)-1] = 0; // remove traling '\n'

		int port;
		if (fscanf(infile, "%i\n", &port) != 1) goto error;

		char user[100];
		if (!infile.ReadLine(user, sizeof(user))) goto error;
		if (user[0] != 0) user[strlen(user)-1] = 0; // remove traling '\n'

		ServerRef* ref = new ServerRef();
		ref->m_stateId = i + 1;
		ref->m_name = name;
		ref->m_host = host;
		ref->m_port = port;
		ref->m_user = user;
		ref->m_matched = false;
		ref->m_perfect = false;
		serverRefs.push_back(ref);
	}

	MatchServers(servers, &serverRefs);

	for (ServerRef* ref : serverRefs)
	{
		*perfectMatch = *perfectMatch && ref->GetPerfect();
		delete ref;
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

	for (ServerRef* ref : serverRefs)
	{
		delete ref;
	}

	return false;
}

bool DiskState::SaveVolumeStat(ServerVolumes* serverVolumes, DiskFile& outfile)
{
	debug("Saving volume stats to disk");

	outfile.Print("%i\n", (int)serverVolumes->size());
	for (ServerVolume& serverVolume : serverVolumes)
	{
		outfile.Print("%i,%i,%i\n", serverVolume.GetFirstDay(), (int)serverVolume.GetDataTime(), (int)serverVolume.GetCustomTime());

		uint32 High1, Low1, High2, Low2;
		Util::SplitInt64(serverVolume.GetTotalBytes(), &High1, &Low1);
		Util::SplitInt64(serverVolume.GetCustomBytes(), &High2, &Low2);
		outfile.Print("%u,%u,%u,%u\n", High1, Low1, High2, Low2);

		ServerVolume::VolumeArray* VolumeArrays[] = { serverVolume.BytesPerSeconds(),
			serverVolume.BytesPerMinutes(), serverVolume.BytesPerHours(), serverVolume.BytesPerDays() };
		for (int i=0; i < 4; i++)
		{
			ServerVolume::VolumeArray* volumeArray = VolumeArrays[i];

			outfile.Print("%i\n", (int)volumeArray->size());
			for (int64 bytes : *volumeArray)
			{
				Util::SplitInt64(bytes, &High1, &Low1);
				outfile.Print("%u,%u\n", High1, Low1);
			}
		}
	}

	return true;
}

bool DiskState::LoadVolumeStat(Servers* servers, ServerVolumes* serverVolumes, DiskFile& infile, int formatVersion)
{
	debug("Loading volume stats from disk");

	int size;
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
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
			if (fscanf(infile, "%i,%i,%i\n", &firstDay, &dataTime,&customTime) != 3) goto error;
			if (fscanf(infile, "%u,%u,%u,%u\n", &High1, &Low1, &High2, &Low2) != 4) goto error;
			if (serverVolume) serverVolume->SetCustomTime((time_t)customTime);
		}
		else
		{
			if (fscanf(infile, "%i,%i\n", &firstDay, &dataTime) != 2) goto error;
			if (fscanf(infile, "%u,%u\n", &High1, &Low1) != 2) goto error;
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
			if (fscanf(infile, "%i\n", &arrSize) != 1) goto error;
			if (volumeArray) volumeArray->resize(arrSize);

			for (int j = 0; j < arrSize; j++)
			{
				if (fscanf(infile, "%u,%u\n", &High1, &Low1) != 2) goto error;
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

	DiskFile outfile;
	if (!outfile.Open(flagFilename, DiskFile::omWrite))
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

	DiskFile outfile;
	if (!outfile.Open(logFilename, DiskFile::omAppend))
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

	DiskFile infile;
	if (!infile.Open(logFilename, DiskFile::omRead))
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
