/*
 *  This file is part of nzbget
 *
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
#include <sys/stat.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <ctype.h>
#include <deque>
#include <algorithm>

#include "nzbget.h"
#include "DiskState.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"

extern Options* g_pOptions;

static const char* FORMATVERSION_SIGNATURE = "nzbget diskstate file version ";

#ifdef WIN32
// Windows doesn't have standard "vsscanf"
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

/*
 * Standard fscanf scans beoynd current line if the next line is empty.
 * This wrapper fixes that.
 */
int DiskState::fscanf(FILE* infile, const char* Format, ...)
{
	char szLine[1024];
	if (!fgets(szLine, sizeof(szLine), infile)) 
	{
		return 0;
	}

	va_list ap;
	va_start(ap, Format);
	int res = vsscanf(szLine, Format, ap);
	va_end(ap);

	return res;
}

/* Parse signature and return format version number
*/
int DiskState::ParseFormatVersion(const char* szFormatSignature)
{
	if (strncmp(szFormatSignature, FORMATVERSION_SIGNATURE, strlen(FORMATVERSION_SIGNATURE)))
	{
		return 0;
	}

	return atoi(szFormatSignature + strlen(FORMATVERSION_SIGNATURE));
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
bool DiskState::SaveDownloadQueue(DownloadQueue* pDownloadQueue)
{
	debug("Saving queue to disk");

	char destFilename[1024];
	snprintf(destFilename, 1024, "%s%s", g_pOptions->GetQueueDir(), "queue");
	destFilename[1024-1] = '\0';

	char tempFilename[1024];
	snprintf(tempFilename, 1024, "%s%s", g_pOptions->GetQueueDir(), "queue.new");
	tempFilename[1024-1] = '\0';

	if (pDownloadQueue->GetQueue()->empty() && 
		pDownloadQueue->GetHistory()->empty())
	{
		remove(destFilename);
		return true;
	}

	FILE* outfile = fopen(tempFilename, "wb");

	if (!outfile)
	{
		error("Error saving diskstate: Could not create file %s", tempFilename);
		return false;
	}

	fprintf(outfile, "%s%i\n", FORMATVERSION_SIGNATURE, 46);

	// save nzb-infos
	SaveNZBQueue(pDownloadQueue, outfile);

	// save history
	SaveHistory(pDownloadQueue, outfile);

	fclose(outfile);

	// now rename to dest file name
	remove(destFilename);
	if (rename(tempFilename, destFilename))
	{
		error("Error saving diskstate: Could not rename file %s to %s", tempFilename, destFilename);
		return false;
	}

	return true;
}

bool DiskState::LoadDownloadQueue(DownloadQueue* pDownloadQueue)
{
	debug("Loading queue from disk");

	bool bOK = false;

	char fileName[1024];
	snprintf(fileName, 1024, "%s%s", g_pOptions->GetQueueDir(), "queue");
	fileName[1024-1] = '\0';

	FILE* infile = fopen(fileName, "rb");

	if (!infile)
	{
		error("Error reading diskstate: could not open file %s", fileName);
		return false;
	}

	char FileSignatur[128];
	fgets(FileSignatur, sizeof(FileSignatur), infile);
	int iFormatVersion = ParseFormatVersion(FileSignatur);
	if (iFormatVersion < 3 || iFormatVersion > 46)
	{
		error("Could not load diskstate due to file version mismatch");
		fclose(infile);
		return false;
	}

	NZBList nzbList(false);
	NZBList sortList(false);

	if (iFormatVersion < 43)
	{
		// load nzb-infos
		if (!LoadNZBList(&nzbList, infile, iFormatVersion)) goto error;

		// load file-infos
		if (!LoadFileQueue12(&nzbList, &sortList, infile, iFormatVersion)) goto error;
	}
	else
	{
		if (!LoadNZBList(pDownloadQueue->GetQueue(), infile, iFormatVersion)) goto error;
	}

	if (iFormatVersion >= 7 && iFormatVersion < 45)
	{
		// load post-queue from v12
		if (!LoadPostQueue12(pDownloadQueue, &nzbList, infile, iFormatVersion)) goto error;
	}
	else if (iFormatVersion < 7)
	{
		// load post-queue from v5
		LoadPostQueue5(pDownloadQueue, &nzbList);
	}

	if (iFormatVersion >= 15 && iFormatVersion < 46)
	{
		// load url-queue
		if (!LoadUrlQueue12(pDownloadQueue, infile, iFormatVersion)) goto error;
	}

	if (iFormatVersion >= 9)
	{
		// load history
		if (!LoadHistory(pDownloadQueue, &nzbList, infile, iFormatVersion)) goto error;
	}

	if (iFormatVersion >= 9 && iFormatVersion < 43)
	{
		// load parked file-infos
		if (!LoadFileQueue12(&nzbList, NULL, infile, iFormatVersion)) goto error;
	}

	if (iFormatVersion < 29)
	{
		CalcCriticalHealth(&nzbList);
	}

	if (iFormatVersion < 43)
	{
		// finalize queue reading
		CompleteNZBList12(pDownloadQueue, &sortList, iFormatVersion);
	}

	CalcFileStats(pDownloadQueue, iFormatVersion);

	bOK = true;

error:

	fclose(infile);
	if (!bOK)
	{
		error("Error reading diskstate for file %s", fileName);
	}

	NZBInfo::ResetGenID(true);
	FileInfo::ResetGenID(true);
	HistoryInfo::ResetGenID(true);

	return bOK;
}

void DiskState::CompleteNZBList12(DownloadQueue* pDownloadQueue, NZBList* pNZBList, int iFormatVersion)
{
	// put all NZBs referenced from file queue into pDownloadQueue->GetQueue()
	for (NZBList::iterator it = pNZBList->begin(); it != pNZBList->end(); it++)
	{
		NZBInfo* pNZBInfo = *it;
		pDownloadQueue->GetQueue()->push_back(pNZBInfo);
	}

	if (31 <= iFormatVersion && iFormatVersion < 42)
	{
		// due to a bug in r811 (v12-testing) new NZBIDs were generated on each refresh of web-ui
		// this resulted in very high numbers for NZBIDs
		// here we renumber NZBIDs in order to keep them low.
		NZBInfo::ResetGenID(false);
		int iID = 1;
		for (NZBList::iterator it = pNZBList->begin(); it != pNZBList->end(); it++)
		{
			NZBInfo* pNZBInfo = *it;
			pNZBInfo->SetID(iID++);
		}
	}
}

void DiskState::SaveNZBQueue(DownloadQueue* pDownloadQueue, FILE* outfile)
{
	debug("Saving nzb list to disk");

	fprintf(outfile, "%i\n", (int)pDownloadQueue->GetQueue()->size());
	for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
	{
		NZBInfo* pNZBInfo = *it;
		SaveNZBInfo(pNZBInfo, outfile);
	}
}

bool DiskState::LoadNZBList(NZBList* pNZBList, FILE* infile, int iFormatVersion)
{
	debug("Loading nzb list from disk");

	// load nzb-infos
	int size;
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		NZBInfo* pNZBInfo = new NZBInfo();
		pNZBList->push_back(pNZBInfo);
		if (!LoadNZBInfo(pNZBInfo, infile, iFormatVersion)) goto error;
	}

	return true;

error:
	error("Error reading nzb list from disk");
	return false;
}

void DiskState::SaveNZBInfo(NZBInfo* pNZBInfo, FILE* outfile)
{
	fprintf(outfile, "%i\n", pNZBInfo->GetID());
	fprintf(outfile, "%i\n", (int)pNZBInfo->GetKind());
	fprintf(outfile, "%s\n", pNZBInfo->GetURL());
	fprintf(outfile, "%s\n", pNZBInfo->GetFilename());
	fprintf(outfile, "%s\n", pNZBInfo->GetDestDir());
	fprintf(outfile, "%s\n", pNZBInfo->GetFinalDir());
	fprintf(outfile, "%s\n", pNZBInfo->GetQueuedFilename());
	fprintf(outfile, "%s\n", pNZBInfo->GetName());
	fprintf(outfile, "%s\n", pNZBInfo->GetCategory());
	fprintf(outfile, "%i,%i,%i,%i\n", (int)pNZBInfo->GetPriority(), 
		pNZBInfo->GetPostInfo() ? (int)pNZBInfo->GetPostInfo()->GetStage() + 1 : 0,
		(int)pNZBInfo->GetDeletePaused(), (int)pNZBInfo->GetManyDupeFiles());
	fprintf(outfile, "%i,%i,%i,%i,%i,%i,%i\n", (int)pNZBInfo->GetParStatus(), (int)pNZBInfo->GetUnpackStatus(),
		(int)pNZBInfo->GetMoveStatus(), (int)pNZBInfo->GetRenameStatus(), (int)pNZBInfo->GetDeleteStatus(),
		(int)pNZBInfo->GetMarkStatus(), (int)pNZBInfo->GetUrlStatus());
	fprintf(outfile, "%i,%i,%i\n", (int)pNZBInfo->GetUnpackCleanedUpDisk(), (int)pNZBInfo->GetHealthPaused(),
		(int)pNZBInfo->GetAddUrlPaused());
	fprintf(outfile, "%i,%i\n", pNZBInfo->GetFileCount(), pNZBInfo->GetParkedFileCount());
	fprintf(outfile, "%i,%i\n", (int)pNZBInfo->GetMinTime(), (int)pNZBInfo->GetMaxTime());

	fprintf(outfile, "%u,%u\n", pNZBInfo->GetFullContentHash(), pNZBInfo->GetFilteredContentHash());

	unsigned long High1, Low1, High2, Low2, High3, Low3;
	Util::SplitInt64(pNZBInfo->GetSize(), &High1, &Low1);
	Util::SplitInt64(pNZBInfo->GetSuccessSize(), &High2, &Low2);
	Util::SplitInt64(pNZBInfo->GetFailedSize(), &High3, &Low3);
	fprintf(outfile, "%lu,%lu,%lu,%lu,%lu,%lu\n", High1, Low1, High2, Low2, High3, Low3);

	Util::SplitInt64(pNZBInfo->GetParSize(), &High1, &Low1);
	Util::SplitInt64(pNZBInfo->GetParSuccessSize(), &High2, &Low2);
	Util::SplitInt64(pNZBInfo->GetParFailedSize(), &High3, &Low3);
	fprintf(outfile, "%lu,%lu,%lu,%lu,%lu,%lu\n", High1, Low1, High2, Low2, High3, Low3);

	fprintf(outfile, "%i,%i,%i\n", pNZBInfo->GetTotalArticles(), pNZBInfo->GetSuccessArticles(), pNZBInfo->GetFailedArticles());

	fprintf(outfile, "%s\n", pNZBInfo->GetDupeKey());
	fprintf(outfile, "%i,%i\n", (int)pNZBInfo->GetDupeMode(), pNZBInfo->GetDupeScore());

	char DestDirSlash[1024];
	snprintf(DestDirSlash, 1023, "%s%c", pNZBInfo->GetDestDir(), PATH_SEPARATOR);
	int iDestDirLen = strlen(DestDirSlash);

	fprintf(outfile, "%i\n", (int)pNZBInfo->GetCompletedFiles()->size());
	for (NZBInfo::Files::iterator it = pNZBInfo->GetCompletedFiles()->begin(); it != pNZBInfo->GetCompletedFiles()->end(); it++)
	{
		char* szFilename = *it;
		// do not save full path to reduce the size of queue-file
		if (!strncmp(DestDirSlash, szFilename, iDestDirLen))
		{
			fprintf(outfile, "%s\n", szFilename + iDestDirLen);
		}
		else
		{
			fprintf(outfile, "%s\n", szFilename);
		}
	}

	fprintf(outfile, "%i\n", (int)pNZBInfo->GetParameters()->size());
	for (NZBParameterList::iterator it = pNZBInfo->GetParameters()->begin(); it != pNZBInfo->GetParameters()->end(); it++)
	{
		NZBParameter* pParameter = *it;
		fprintf(outfile, "%s=%s\n", pParameter->GetName(), pParameter->GetValue());
	}

	fprintf(outfile, "%i\n", (int)pNZBInfo->GetScriptStatuses()->size());
	for (ScriptStatusList::iterator it = pNZBInfo->GetScriptStatuses()->begin(); it != pNZBInfo->GetScriptStatuses()->end(); it++)
	{
		ScriptStatus* pScriptStatus = *it;
		fprintf(outfile, "%i,%s\n", pScriptStatus->GetStatus(), pScriptStatus->GetName());
	}

	fprintf(outfile, "%i\n", (int)pNZBInfo->GetServerStats()->size());
	for (ServerStatList::iterator it = pNZBInfo->GetServerStats()->begin(); it != pNZBInfo->GetServerStats()->end(); it++)
	{
		ServerStat* pServerStat = *it;
		fprintf(outfile, "%i,%i,%i\n", pServerStat->GetServerID(), pServerStat->GetSuccessArticles(), pServerStat->GetFailedArticles());
	}

	NZBInfo::Messages* pMessages = pNZBInfo->LockMessages();
	fprintf(outfile, "%i\n", (int)pMessages->size());
	for (NZBInfo::Messages::iterator it = pMessages->begin(); it != pMessages->end(); it++)
	{
		Message* pMessage = *it;
		fprintf(outfile, "%i,%i,%s\n", pMessage->GetKind(), (int)pMessage->GetTime(), pMessage->GetText());
	}
	pNZBInfo->UnlockMessages();

	// save file-infos
	int iSize = 0;
	for (FileList::iterator it = pNZBInfo->GetFileList()->begin(); it != pNZBInfo->GetFileList()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (!pFileInfo->GetDeleted())
		{
			iSize++;
		}
	}
	fprintf(outfile, "%i\n", iSize);
	for (FileList::iterator it = pNZBInfo->GetFileList()->begin(); it != pNZBInfo->GetFileList()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (!pFileInfo->GetDeleted())
		{
			fprintf(outfile, "%i,%i,%i,%i\n", pFileInfo->GetID(), (int)pFileInfo->GetPaused(), 
				(int)pFileInfo->GetTime(), (int)pFileInfo->GetExtraPriority());
		}
	}
}

bool DiskState::LoadNZBInfo(NZBInfo* pNZBInfo, FILE* infile, int iFormatVersion)
{
	char buf[10240];

	if (iFormatVersion >= 24)
	{
		int iID;
		if (fscanf(infile, "%i\n", &iID) != 1) goto error;
		pNZBInfo->SetID(iID);
	}

	if (iFormatVersion >= 46)
	{
		int iKind;
		if (fscanf(infile, "%i\n", &iKind) != 1) goto error;
		pNZBInfo->SetKind((NZBInfo::EKind)iKind);

		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		pNZBInfo->SetURL(buf);
	}

	if (!fgets(buf, sizeof(buf), infile)) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	pNZBInfo->SetFilename(buf);

	if (!fgets(buf, sizeof(buf), infile)) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	pNZBInfo->SetDestDir(buf);

	if (iFormatVersion >= 27)
	{
		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		pNZBInfo->SetFinalDir(buf);
	}

	if (iFormatVersion >= 5)
	{
		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		pNZBInfo->SetQueuedFilename(buf);
	}

	if (iFormatVersion >= 13)
	{
		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		if (strlen(buf) > 0)
		{
			pNZBInfo->SetName(buf);
		}
	}

	if (iFormatVersion >= 4)
	{
		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		pNZBInfo->SetCategory(buf);
	}

	if (true) // clang requires a block for goto to work
	{
		int iPriority = 0, iPostProcess = 0, iPostStage = 0, iDeletePaused = 0, iManyDupeFiles = 0;
		if (iFormatVersion >= 45)
		{
			if (fscanf(infile, "%i,%i,%i,%i\n", &iPriority, &iPostStage, &iDeletePaused, &iManyDupeFiles) != 4) goto error;
		}
		else if (iFormatVersion >= 44)
		{
			if (fscanf(infile, "%i,%i,%i,%i\n", &iPriority, &iPostProcess, &iDeletePaused, &iManyDupeFiles) != 4) goto error;
		}
		else if (iFormatVersion >= 41)
		{
			if (fscanf(infile, "%i,%i,%i\n", &iPostProcess, &iDeletePaused, &iManyDupeFiles) != 3) goto error;
		}
		else if (iFormatVersion >= 40)
		{
			if (fscanf(infile, "%i,%i\n", &iPostProcess, &iDeletePaused) != 2) goto error;
		}
		else if (iFormatVersion >= 4)
		{
			if (fscanf(infile, "%i\n", &iPostProcess) != 1) goto error;
		}
		pNZBInfo->SetPriority(iPriority);
		pNZBInfo->SetDeletePaused((bool)iDeletePaused);
		pNZBInfo->SetManyDupeFiles((bool)iManyDupeFiles);
		if (iPostStage > 0)
		{
			pNZBInfo->EnterPostProcess();
			pNZBInfo->GetPostInfo()->SetStage((PostInfo::EStage)iPostStage);
		}
	}

	if (iFormatVersion >= 8 && iFormatVersion < 18)
	{
		int iParStatus;
		if (fscanf(infile, "%i\n", &iParStatus) != 1) goto error;
		pNZBInfo->SetParStatus((NZBInfo::EParStatus)iParStatus);
	}

	if (iFormatVersion >= 9 && iFormatVersion < 18)
	{
		int iScriptStatus;
		if (fscanf(infile, "%i\n", &iScriptStatus) != 1) goto error;
		if (iScriptStatus > 1) iScriptStatus--;
		pNZBInfo->GetScriptStatuses()->Add("SCRIPT", (ScriptStatus::EStatus)iScriptStatus);
	}

	if (iFormatVersion >= 18)
	{
		int iParStatus, iUnpackStatus, iScriptStatus, iMoveStatus = 0,
			iRenameStatus = 0, iDeleteStatus = 0, iMarkStatus = 0, iUrlStatus = 0;
		if (iFormatVersion >= 46)
		{
			if (fscanf(infile, "%i,%i,%i,%i,%i,%i,%i\n", &iParStatus, &iUnpackStatus, &iMoveStatus,
				&iRenameStatus, &iDeleteStatus, &iMarkStatus, &iUrlStatus) != 7) goto error;
		}
		else if (iFormatVersion >= 37)
		{
			if (fscanf(infile, "%i,%i,%i,%i,%i,%i\n", &iParStatus, &iUnpackStatus,
				&iMoveStatus, &iRenameStatus, &iDeleteStatus, &iMarkStatus) != 6) goto error;
		}
		else if (iFormatVersion >= 35)
		{
			if (fscanf(infile, "%i,%i,%i,%i,%i\n", &iParStatus, &iUnpackStatus,
				&iMoveStatus, &iRenameStatus, &iDeleteStatus) != 5) goto error;
		}
		else if (iFormatVersion >= 23)
		{
			if (fscanf(infile, "%i,%i,%i,%i\n", &iParStatus, &iUnpackStatus,
				&iMoveStatus, &iRenameStatus) != 4) goto error;
		}
		else if (iFormatVersion >= 21)
		{
			if (fscanf(infile, "%i,%i,%i,%i,%i\n", &iParStatus, &iUnpackStatus,
				&iScriptStatus, &iMoveStatus, &iRenameStatus) != 5) goto error;
		}
		else if (iFormatVersion >= 20)
		{
			if (fscanf(infile, "%i,%i,%i,%i\n", &iParStatus, &iUnpackStatus,
				&iScriptStatus, &iMoveStatus) != 4) goto error;
		}
		else
		{
			if (fscanf(infile, "%i,%i,%i\n", &iParStatus, &iUnpackStatus, &iScriptStatus) != 3) goto error;
		}
		pNZBInfo->SetParStatus((NZBInfo::EParStatus)iParStatus);
		pNZBInfo->SetUnpackStatus((NZBInfo::EUnpackStatus)iUnpackStatus);
		pNZBInfo->SetMoveStatus((NZBInfo::EMoveStatus)iMoveStatus);
		pNZBInfo->SetRenameStatus((NZBInfo::ERenameStatus)iRenameStatus);
		pNZBInfo->SetDeleteStatus((NZBInfo::EDeleteStatus)iDeleteStatus);
		pNZBInfo->SetMarkStatus((NZBInfo::EMarkStatus)iMarkStatus);
		if (pNZBInfo->GetKind() == NZBInfo::nkNzb)
		{
			pNZBInfo->SetUrlStatus((NZBInfo::EUrlStatus)iUrlStatus);
		}
		if (iFormatVersion < 23)
		{
			if (iScriptStatus > 1) iScriptStatus--;
			pNZBInfo->GetScriptStatuses()->Add("SCRIPT", (ScriptStatus::EStatus)iScriptStatus);
		}
	}

	if (iFormatVersion >= 35)
	{
		int iUnpackCleanedUpDisk, iHealthPaused, iAddUrlPaused = 0;
		if (iFormatVersion >= 46)
		{
			if (fscanf(infile, "%i,%i,%i\n", &iUnpackCleanedUpDisk, &iHealthPaused, &iAddUrlPaused) != 3) goto error;
		}
		else
		{
			if (fscanf(infile, "%i,%i\n", &iUnpackCleanedUpDisk, &iHealthPaused) != 2) goto error;
		}
		pNZBInfo->SetUnpackCleanedUpDisk((bool)iUnpackCleanedUpDisk);
		pNZBInfo->SetHealthPaused((bool)iHealthPaused);
		pNZBInfo->SetAddUrlPaused((bool)iAddUrlPaused);
	}
	else if (iFormatVersion >= 28)
	{
		int iDeleted, iUnpackCleanedUpDisk, iHealthPaused, iHealthDeleted;
		if (fscanf(infile, "%i,%i,%i,%i\n", &iDeleted, &iUnpackCleanedUpDisk, &iHealthPaused, &iHealthDeleted) != 4) goto error;
		pNZBInfo->SetUnpackCleanedUpDisk((bool)iUnpackCleanedUpDisk);
		pNZBInfo->SetHealthPaused((bool)iHealthPaused);
		pNZBInfo->SetDeleteStatus(iHealthDeleted ? NZBInfo::dsHealth : iDeleted ? NZBInfo::dsManual : NZBInfo::dsNone);
	}

	if (iFormatVersion >= 28)
	{
		int iFileCount, iParkedFileCount;
		if (fscanf(infile, "%i,%i\n", &iFileCount, &iParkedFileCount) != 2) goto error;
		pNZBInfo->SetFileCount(iFileCount);
		pNZBInfo->SetParkedFileCount(iParkedFileCount);
	}
	else
	{
		if (iFormatVersion >= 19)
		{
			int iUnpackCleanedUpDisk;
			if (fscanf(infile, "%i\n", &iUnpackCleanedUpDisk) != 1) goto error;
			pNZBInfo->SetUnpackCleanedUpDisk((bool)iUnpackCleanedUpDisk);
		}
		
		int iFileCount;
		if (fscanf(infile, "%i\n", &iFileCount) != 1) goto error;
		pNZBInfo->SetFileCount(iFileCount);

		if (iFormatVersion >= 10)
		{
			if (fscanf(infile, "%i\n", &iFileCount) != 1) goto error;
			pNZBInfo->SetParkedFileCount(iFileCount);
		}
	}

	if (iFormatVersion >= 44)
	{
		int iMinTime, iMaxTime;
		if (fscanf(infile, "%i,%i\n", &iMinTime, &iMaxTime) != 2) goto error;
		pNZBInfo->SetMinTime((time_t)iMinTime);
		pNZBInfo->SetMaxTime((time_t)iMaxTime);
	}

	if (true) // clang requires a block for goto to work
	{
		unsigned int iFullContentHash = 0, iFilteredContentHash = 0;
		if (iFormatVersion >= 34)
		{
			if (fscanf(infile, "%u,%u\n", &iFullContentHash, &iFilteredContentHash) != 2) goto error;
		}
		else if (iFormatVersion >= 32)
		{
			if (fscanf(infile, "%u\n", &iFullContentHash) != 1) goto error;
		}
		pNZBInfo->SetFullContentHash(iFullContentHash);
		pNZBInfo->SetFilteredContentHash(iFilteredContentHash);
	}

	if (iFormatVersion >= 28)
	{
		unsigned long High1, Low1, High2, Low2, High3, Low3;
		if (fscanf(infile, "%lu,%lu,%lu,%lu,%lu,%lu\n", &High1, &Low1, &High2, &Low2, &High3, &Low3) != 6) goto error;
		pNZBInfo->SetSize(Util::JoinInt64(High1, Low1));
		pNZBInfo->SetSuccessSize(Util::JoinInt64(High2, Low2));
		pNZBInfo->SetCurrentSuccessSize(pNZBInfo->GetSuccessSize());
		pNZBInfo->SetFailedSize(Util::JoinInt64(High3, Low3));
		pNZBInfo->SetCurrentFailedSize(pNZBInfo->GetFailedSize());

		if (fscanf(infile, "%lu,%lu,%lu,%lu,%lu,%lu\n", &High1, &Low1, &High2, &Low2, &High3, &Low3) != 6) goto error;
		pNZBInfo->SetParSize(Util::JoinInt64(High1, Low1));
		pNZBInfo->SetParSuccessSize(Util::JoinInt64(High2, Low2));
		pNZBInfo->SetParCurrentSuccessSize(pNZBInfo->GetParSuccessSize());
		pNZBInfo->SetParFailedSize(Util::JoinInt64(High3, Low3));
		pNZBInfo->SetParCurrentFailedSize(pNZBInfo->GetParFailedSize());
	}
	else
	{
		unsigned long High, Low;
		if (fscanf(infile, "%lu,%lu\n", &High, &Low) != 2) goto error;
		pNZBInfo->SetSize(Util::JoinInt64(High, Low));
	}
	
	if (iFormatVersion >= 30)
	{
		int iTotalArticles, iSuccessArticles, iFailedArticles;
		if (fscanf(infile, "%i,%i,%i\n", &iTotalArticles, &iSuccessArticles, &iFailedArticles) != 3) goto error;
		pNZBInfo->SetTotalArticles(iTotalArticles);
		pNZBInfo->SetSuccessArticles(iSuccessArticles);
		pNZBInfo->SetFailedArticles(iFailedArticles);
	}

	if (iFormatVersion >= 31)
	{
		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		if (iFormatVersion < 36) ConvertDupeKey(buf, sizeof(buf));
		pNZBInfo->SetDupeKey(buf);
	}

	if (true) // clang requires a block for goto to work
	{
		int iDupeMode = 0, iDupeScore = 0;
		if (iFormatVersion >= 39)
		{
			if (fscanf(infile, "%i,%i\n", &iDupeMode, &iDupeScore) != 2) goto error;
		}
		else if (iFormatVersion >= 31)
		{
			int iDupe;
			if (fscanf(infile, "%i,%i,%i\n", &iDupe, &iDupeMode, &iDupeScore) != 3) goto error;
		}
		pNZBInfo->SetDupeMode((EDupeMode)iDupeMode);
		pNZBInfo->SetDupeScore(iDupeScore);
	}

	if (iFormatVersion >= 4)
	{
		int iFileCount;
		if (fscanf(infile, "%i\n", &iFileCount) != 1) goto error;
		for (int i = 0; i < iFileCount; i++)
		{
			if (!fgets(buf, sizeof(buf), infile)) goto error;
			if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'

			// restore full file name.
			char* szFileName = buf;
			char FullFileName[1024];
			if (!strchr(buf, PATH_SEPARATOR))
			{
				snprintf(FullFileName, 1023, "%s%c%s", pNZBInfo->GetDestDir(), PATH_SEPARATOR, buf);
				szFileName = FullFileName;
			}

			pNZBInfo->GetCompletedFiles()->push_back(strdup(szFileName));
		}
	}

	if (iFormatVersion >= 6)
	{
		int iParameterCount;
		if (fscanf(infile, "%i\n", &iParameterCount) != 1) goto error;
		for (int i = 0; i < iParameterCount; i++)
		{
			if (!fgets(buf, sizeof(buf), infile)) goto error;
			if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'

			char* szValue = strchr(buf, '=');
			if (szValue)
			{
				*szValue = '\0';
				szValue++;
				pNZBInfo->GetParameters()->SetParameter(buf, szValue);
			}
		}
	}

	if (iFormatVersion >= 23)
	{
		int iScriptCount;
		if (fscanf(infile, "%i\n", &iScriptCount) != 1) goto error;
		for (int i = 0; i < iScriptCount; i++)
		{
			if (!fgets(buf, sizeof(buf), infile)) goto error;
			if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
			
			char* szScriptName = strchr(buf, ',');
			if (szScriptName)
			{
				szScriptName++;
				int iStatus = atoi(buf);
				if (iStatus > 1 && iFormatVersion < 25) iStatus--;
				pNZBInfo->GetScriptStatuses()->Add(szScriptName, (ScriptStatus::EStatus)iStatus);
			}
		}
	}

	if (iFormatVersion >= 30)
	{
		int iStatCount;
		if (fscanf(infile, "%i\n", &iStatCount) != 1) goto error;
		for (int i = 0; i < iStatCount; i++)
		{
			int iServerID, iSuccessArticles, iFailedArticles;
			if (fscanf(infile, "%i,%i,%i\n", &iServerID, &iSuccessArticles, &iFailedArticles) != 3) goto error;
			pNZBInfo->GetServerStats()->SetStat(iServerID, iSuccessArticles, iFailedArticles, false);
		}
	}

	if (iFormatVersion >= 11)
	{
		int iLogCount;
		if (fscanf(infile, "%i\n", &iLogCount) != 1) goto error;
		for (int i = 0; i < iLogCount; i++)
		{
			if (!fgets(buf, sizeof(buf), infile)) goto error;
			if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'

			int iKind, iTime;
			sscanf(buf, "%i,%i", &iKind, &iTime);
			char* szText = strchr(buf + 2, ',');
			if (szText) {
				szText++;
			}
			pNZBInfo->AppendMessage((Message::EKind)iKind, (time_t)iTime, szText);
		}
	}
	
	if (iFormatVersion < 26)
	{
		NZBParameter* pUnpackParameter = pNZBInfo->GetParameters()->Find("*Unpack:", false);
		if (!pUnpackParameter)
		{
			pNZBInfo->GetParameters()->SetParameter("*Unpack:", g_pOptions->GetUnpack() ? "yes" : "no");
		}
	}

	if (iFormatVersion >= 43)
	{
		int iFileCount;
		if (fscanf(infile, "%i\n", &iFileCount) != 1) goto error;
		for (int i = 0; i < iFileCount; i++)
		{
			unsigned int id, paused, iTime = 0;
			int iPriority = 0, iExtraPriority = 0;

			if (iFormatVersion >= 44)
			{
				if (fscanf(infile, "%i,%i,%i,%i\n", &id, &paused, &iTime, &iExtraPriority) != 4) goto error;
			}
			else
			{
				if (fscanf(infile, "%i,%i,%i,%i,%i\n", &id, &paused, &iTime, &iPriority, &iExtraPriority) != 5) goto error;
				pNZBInfo->SetPriority(iPriority);
			}

			char fileName[1024];
			snprintf(fileName, 1024, "%s%i", g_pOptions->GetQueueDir(), id);
			fileName[1024-1] = '\0';
			FileInfo* pFileInfo = new FileInfo();
			bool res = LoadFileInfo(pFileInfo, fileName, true, false);
			if (res)
			{
				pFileInfo->SetID(id);
				pFileInfo->SetPaused(paused);
				pFileInfo->SetTime(iTime);
				pFileInfo->SetExtraPriority(iExtraPriority != 0);
				pFileInfo->SetNZBInfo(pNZBInfo);
				if (iFormatVersion < 30)
				{
					pNZBInfo->SetTotalArticles(pNZBInfo->GetTotalArticles() + pFileInfo->GetTotalArticles());
				}
				pNZBInfo->GetFileList()->push_back(pFileInfo);
			}
			else
			{
				delete pFileInfo;
			}
		}
	}

	return true;

error:
	error("Error reading nzb info from disk");
	return false;
}

bool DiskState::LoadFileQueue12(NZBList* pNZBList, NZBList* pSortList, FILE* infile, int iFormatVersion)
{
	debug("Loading file queue from disk");

	int size;
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		unsigned int id, iNZBIndex, paused;
		unsigned int iTime = 0;
		int iPriority = 0, iExtraPriority = 0;
		if (iFormatVersion >= 17)
		{
			if (fscanf(infile, "%i,%i,%i,%i,%i,%i\n", &id, &iNZBIndex, &paused, &iTime, &iPriority, &iExtraPriority) != 6) goto error;
		}
		else if (iFormatVersion >= 14)
		{
			if (fscanf(infile, "%i,%i,%i,%i,%i\n", &id, &iNZBIndex, &paused, &iTime, &iPriority) != 5) goto error;
		}
		else if (iFormatVersion >= 12)
		{
			if (fscanf(infile, "%i,%i,%i,%i\n", &id, &iNZBIndex, &paused, &iTime) != 4) goto error;
		}
		else
		{
			if (fscanf(infile, "%i,%i,%i\n", &id, &iNZBIndex, &paused) != 3) goto error;
		}
		if (iNZBIndex > pNZBList->size()) goto error;

		char fileName[1024];
		snprintf(fileName, 1024, "%s%i", g_pOptions->GetQueueDir(), id);
		fileName[1024-1] = '\0';
		FileInfo* pFileInfo = new FileInfo();
		bool res = LoadFileInfo(pFileInfo, fileName, true, false);
		if (res)
		{
			NZBInfo* pNZBInfo = pNZBList->at(iNZBIndex - 1);
			pNZBInfo->SetPriority(iPriority);
			pFileInfo->SetID(id);
			pFileInfo->SetPaused(paused);
			pFileInfo->SetTime(iTime);
			pFileInfo->SetExtraPriority(iExtraPriority != 0);
			pFileInfo->SetNZBInfo(pNZBInfo);
			if (iFormatVersion < 30)
			{
				pNZBInfo->SetTotalArticles(pNZBInfo->GetTotalArticles() + pFileInfo->GetTotalArticles());
			}
			pNZBInfo->GetFileList()->push_back(pFileInfo);

			if (pSortList && std::find(pSortList->begin(), pSortList->end(), pNZBInfo) == pSortList->end())
			{
				pSortList->push_back(pNZBInfo);
			}
		}
		else
		{
			delete pFileInfo;
		}
	}

	return true;

error:
	error("Error reading file queue from disk");
	return false;
}

bool DiskState::SaveFile(FileInfo* pFileInfo)
{
	char fileName[1024];
	snprintf(fileName, 1024, "%s%i", g_pOptions->GetQueueDir(), pFileInfo->GetID());
	fileName[1024-1] = '\0';
	return SaveFileInfo(pFileInfo, fileName);
}

bool DiskState::SaveFileInfo(FileInfo* pFileInfo, const char* szFilename)
{
	debug("Saving FileInfo to disk");

	FILE* outfile = fopen(szFilename, "wb");

	if (!outfile)
	{
		error("Error saving diskstate: could not create file %s", szFilename);
		return false;
	}

	fprintf(outfile, "%s%i\n", FORMATVERSION_SIGNATURE, 3);

	fprintf(outfile, "%s\n", pFileInfo->GetSubject());
	fprintf(outfile, "%s\n", pFileInfo->GetFilename());

	unsigned long High, Low;
	Util::SplitInt64(pFileInfo->GetSize(), &High, &Low);
	fprintf(outfile, "%lu,%lu\n", High, Low);

	Util::SplitInt64(pFileInfo->GetMissedSize(), &High, &Low);
	fprintf(outfile, "%lu,%lu\n", High, Low);

	fprintf(outfile, "%i\n", (int)pFileInfo->GetParFile());
	fprintf(outfile, "%i,%i\n", pFileInfo->GetTotalArticles(), pFileInfo->GetMissedArticles());

	fprintf(outfile, "%i\n", (int)pFileInfo->GetGroups()->size());
	for (FileInfo::Groups::iterator it = pFileInfo->GetGroups()->begin(); it != pFileInfo->GetGroups()->end(); it++)
	{
		fprintf(outfile, "%s\n", *it);
	}

	fprintf(outfile, "%i\n", (int)pFileInfo->GetArticles()->size());
	for (FileInfo::Articles::iterator it = pFileInfo->GetArticles()->begin(); it != pFileInfo->GetArticles()->end(); it++)
	{
		ArticleInfo* pArticleInfo = *it;
		fprintf(outfile, "%i,%i\n", pArticleInfo->GetPartNumber(), pArticleInfo->GetSize());
		fprintf(outfile, "%s\n", pArticleInfo->GetMessageID());
	}

	fclose(outfile);
	return true;
}

bool DiskState::LoadArticles(FileInfo* pFileInfo)
{
	char fileName[1024];
	snprintf(fileName, 1024, "%s%i", g_pOptions->GetQueueDir(), pFileInfo->GetID());
	fileName[1024-1] = '\0';
	return LoadFileInfo(pFileInfo, fileName, false, true);
}

bool DiskState::LoadFileInfo(FileInfo* pFileInfo, const char * szFilename, bool bFileSummary, bool bArticles)
{
	debug("Loading FileInfo from disk");

	FILE* infile = fopen(szFilename, "rb");

	if (!infile)
	{
		error("Error reading diskstate: could not open file %s", szFilename);
		return false;
	}

	char buf[1024];
	int iFormatVersion = 0;

	if (fgets(buf, sizeof(buf), infile))
	{
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		iFormatVersion = ParseFormatVersion(buf);
		if (iFormatVersion > 3)
		{
			error("Could not load diskstate due to file version mismatch");
			goto error;
		}
	}
	else
	{
		goto error;
	}
	
	if (iFormatVersion >= 2)
	{
		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	}

	if (bFileSummary) pFileInfo->SetSubject(buf);

	if (!fgets(buf, sizeof(buf), infile)) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	if (bFileSummary) pFileInfo->SetFilename(buf);

	if (iFormatVersion < 2)
	{
		int iFilenameConfirmed;
		if (fscanf(infile, "%i\n", &iFilenameConfirmed) != 1) goto error;
		if (bFileSummary) pFileInfo->SetFilenameConfirmed(iFilenameConfirmed);
	}
	
	unsigned long High, Low;
	if (fscanf(infile, "%lu,%lu\n", &High, &Low) != 2) goto error;
	if (bFileSummary) pFileInfo->SetSize(Util::JoinInt64(High, Low));
	if (bFileSummary) pFileInfo->SetRemainingSize(pFileInfo->GetSize());

	if (iFormatVersion >= 2)
	{
		if (fscanf(infile, "%lu,%lu\n", &High, &Low) != 2) goto error;
		if (bFileSummary) pFileInfo->SetMissedSize(Util::JoinInt64(High, Low));
		if (bFileSummary) pFileInfo->SetRemainingSize(pFileInfo->GetSize() - pFileInfo->GetMissedSize());

		int iParFile;
		if (fscanf(infile, "%i\n", &iParFile) != 1) goto error;
		if (bFileSummary) pFileInfo->SetParFile((bool)iParFile);
	}

	if (iFormatVersion >= 3)
	{
		int iTotalArticles, iMissedArticles;
		if (fscanf(infile, "%i,%i\n", &iTotalArticles, &iMissedArticles) != 2) goto error;
		if (bFileSummary) pFileInfo->SetTotalArticles(iTotalArticles);
		if (bFileSummary) pFileInfo->SetMissedArticles(iMissedArticles);
	}
	
	int size;
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		if (bFileSummary) pFileInfo->GetGroups()->push_back(strdup(buf));
	}

	if (fscanf(infile, "%i\n", &size) != 1) goto error;

	if (iFormatVersion < 3 && bFileSummary)
	{
		pFileInfo->SetTotalArticles(size);
	}

	if (bArticles)
	{
		for (int i = 0; i < size; i++)
		{
			int PartNumber, PartSize;
			if (fscanf(infile, "%i,%i\n", &PartNumber, &PartSize) != 2) goto error;

			if (!fgets(buf, sizeof(buf), infile)) goto error;
			if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'

			ArticleInfo* pArticleInfo = new ArticleInfo();
			pArticleInfo->SetPartNumber(PartNumber);
			pArticleInfo->SetSize(PartSize);
			pArticleInfo->SetMessageID(buf);
			pFileInfo->GetArticles()->push_back(pArticleInfo);
		}
	}

	fclose(infile);
	return true;

error:
	fclose(infile);
	error("Error reading diskstate for file %s", szFilename);
	return false;
}

bool DiskState::LoadPostQueue12(DownloadQueue* pDownloadQueue, NZBList* pNZBList, FILE* infile, int iFormatVersion)
{
	debug("Loading post-queue from disk");

	int size;
	char buf[10240];

	// load post-infos
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		PostInfo* pPostInfo = NULL;
		int iNZBID = 0;
		unsigned int iNZBIndex = 0, iStage, iDummy;
		if (iFormatVersion < 19)
		{
			if (fscanf(infile, "%i,%i,%i,%i\n", &iNZBIndex, &iDummy, &iDummy, &iStage) != 4) goto error;
		}
		else if (iFormatVersion < 22)
		{
			if (fscanf(infile, "%i,%i,%i,%i\n", &iNZBIndex, &iDummy, &iDummy, &iStage) != 4) goto error;
		}
		else if (iFormatVersion < 43)
		{
			if (fscanf(infile, "%i,%i\n", &iNZBIndex, &iStage) != 2) goto error;
		}
		else
		{
			if (fscanf(infile, "%i,%i\n", &iNZBID, &iStage) != 2) goto error;
		}
		if (iFormatVersion < 18 && iStage > (int)PostInfo::ptVerifyingRepaired) iStage++;
		if (iFormatVersion < 21 && iStage > (int)PostInfo::ptVerifyingRepaired) iStage++;
		if (iFormatVersion < 20 && iStage > (int)PostInfo::ptUnpacking) iStage++;

		NZBInfo* pNZBInfo = NULL;

		if (iFormatVersion < 43)
		{
			pNZBInfo = pNZBList->at(iNZBIndex - 1);
			if (!pNZBInfo) goto error;
		}
		else
		{
			pNZBInfo = FindNZBInfo(pDownloadQueue, iNZBID);
			if (!pNZBInfo) goto error;
		}

		pNZBInfo->EnterPostProcess();
		pPostInfo = pNZBInfo->GetPostInfo();

		pPostInfo->SetStage((PostInfo::EStage)iStage);

		// InfoName, ignore
		if (!fgets(buf, sizeof(buf), infile)) goto error;

		if (iFormatVersion < 22)
		{
			// ParFilename, ignore
			if (!fgets(buf, sizeof(buf), infile)) goto error;
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
bool DiskState::LoadPostQueue5(DownloadQueue* pDownloadQueue, NZBList* pNZBList)
{
	debug("Loading post-queue from disk");

	char fileName[1024];
	snprintf(fileName, 1024, "%s%s", g_pOptions->GetQueueDir(), "postq");
	fileName[1024-1] = '\0';

	if (!Util::FileExists(fileName))
	{
		return true;
	}

	FILE* infile = fopen(fileName, "rb");

	if (!infile)
	{
		error("Error reading diskstate: could not open file %s", fileName);
		return false;
	}

	char FileSignatur[128];
	fgets(FileSignatur, sizeof(FileSignatur), infile);
	int iFormatVersion = ParseFormatVersion(FileSignatur);
	if (iFormatVersion < 3 || iFormatVersion > 7)
	{
		error("Could not load diskstate due to file version mismatch");
		fclose(infile);
		return false;
	}

	int size;
	char buf[10240];
	int iIntValue;

	// load file-infos
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'

		// find NZBInfo based on NZBFilename
		NZBInfo* pNZBInfo = NULL;
		for (NZBList::iterator it = pNZBList->begin(); it != pNZBList->end(); it++)
		{
			NZBInfo* pNZBInfo2 = *it;
			if (!strcmp(pNZBInfo2->GetFilename(), buf))
			{
				pNZBInfo = pNZBInfo2;
				break;
			}
		}

		bool bNewNZBInfo = !pNZBInfo;
		if (bNewNZBInfo)
		{
			pNZBInfo = new NZBInfo();
			pNZBList->push_front(pNZBInfo);
			pNZBInfo->SetFilename(buf);
		}

		pNZBInfo->EnterPostProcess();
		PostInfo* pPostInfo = pNZBInfo->GetPostInfo();

		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		if (bNewNZBInfo)
		{
			pNZBInfo->SetDestDir(buf);
		}

		// ParFilename, ignore
		if (!fgets(buf, sizeof(buf), infile)) goto error;

		// InfoName, ignore
		if (!fgets(buf, sizeof(buf), infile)) goto error;

		if (iFormatVersion >= 4)
		{
			if (!fgets(buf, sizeof(buf), infile)) goto error;
			if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
			if (bNewNZBInfo)
			{
				pNZBInfo->SetCategory(buf);
			}
		}
		else
		{
			if (bNewNZBInfo)
			{
				pNZBInfo->SetCategory("");
			}
		}

		if (iFormatVersion >= 5)
		{
			if (!fgets(buf, sizeof(buf), infile)) goto error;
			if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
			if (bNewNZBInfo)
			{
				pNZBInfo->SetQueuedFilename(buf);
			}
		}
		else
		{
			if (bNewNZBInfo)
			{
				pNZBInfo->SetQueuedFilename("");
			}
		}

		int iParCheck;
		if (fscanf(infile, "%i\n", &iParCheck) != 1) goto error; // ParCheck

		if (fscanf(infile, "%i\n", &iIntValue) != 1) goto error;
		pNZBInfo->SetParStatus(iParCheck ? (NZBInfo::EParStatus)iIntValue : NZBInfo::psSkipped);

		if (iFormatVersion < 7)
		{
			// skip old field ParFailed, not used anymore
			if (fscanf(infile, "%i\n", &iIntValue) != 1) goto error;
		}

		if (fscanf(infile, "%i\n", &iIntValue) != 1) goto error;
		pPostInfo->SetStage((PostInfo::EStage)iIntValue);

		if (iFormatVersion >= 6)
		{
			int iParameterCount;
			if (fscanf(infile, "%i\n", &iParameterCount) != 1) goto error;
			for (int i = 0; i < iParameterCount; i++)
			{
				if (!fgets(buf, sizeof(buf), infile)) goto error;
				if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'

				char* szValue = strchr(buf, '=');
				if (szValue)
				{
					*szValue = '\0';
					szValue++;
					if (bNewNZBInfo)
					{
						pNZBInfo->GetParameters()->SetParameter(buf, szValue);
					}
				}
			}
		}
	}

	fclose(infile);
	return true;

error:
	fclose(infile);
	error("Error reading diskstate for file %s", fileName);
	return false;
}

bool DiskState::LoadUrlQueue12(DownloadQueue* pDownloadQueue, FILE* infile, int iFormatVersion)
{
	debug("Loading url-queue from disk");
	int size;

	// load url-infos
	if (fscanf(infile, "%i\n", &size) != 1) goto error;

	for (int i = 0; i < size; i++)
	{
		NZBInfo* pNZBInfo = new NZBInfo();
		if (!LoadUrlInfo12(pNZBInfo, infile, iFormatVersion)) goto error;
		pDownloadQueue->GetQueue()->push_back(pNZBInfo);
	}

	return true;

error:
	error("Error reading diskstate for url-queue");
	return false;
}

bool DiskState::LoadUrlInfo12(NZBInfo* pNZBInfo, FILE* infile, int iFormatVersion)
{
	char buf[10240];

	if (iFormatVersion >= 24)
	{
		int iID;
		if (fscanf(infile, "%i\n", &iID) != 1) goto error;
		pNZBInfo->SetID(iID);
	}

	int iStatusDummy, iPriority;
	if (fscanf(infile, "%i,%i\n", &iStatusDummy, &iPriority) != 2) goto error;
	pNZBInfo->SetPriority(iPriority);

	if (iFormatVersion >= 16)
	{
		int iAddTopDummy, iAddPaused;
		if (fscanf(infile, "%i,%i\n", &iAddTopDummy, &iAddPaused) != 2) goto error;
		pNZBInfo->SetAddUrlPaused(iAddPaused);
	}

	if (!fgets(buf, sizeof(buf), infile)) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	pNZBInfo->SetURL(buf);

	if (!fgets(buf, sizeof(buf), infile)) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	pNZBInfo->SetFilename(buf);

	if (!fgets(buf, sizeof(buf), infile)) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	pNZBInfo->SetCategory(buf);

	if (iFormatVersion >= 31)
	{
		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		if (iFormatVersion < 36) ConvertDupeKey(buf, sizeof(buf));
		pNZBInfo->SetDupeKey(buf);

		int iDupeMode, iDupeScore;
		if (fscanf(infile, "%i,%i\n", &iDupeMode, &iDupeScore) != 2) goto error;
		pNZBInfo->SetDupeMode((EDupeMode)iDupeMode);
		pNZBInfo->SetDupeScore(iDupeScore);
	}

	return true;

error:
	return false;
}

void DiskState::SaveDupInfo(DupInfo* pDupInfo, FILE* outfile)
{
	unsigned long High, Low;
	Util::SplitInt64(pDupInfo->GetSize(), &High, &Low);
	fprintf(outfile, "%i,%lu,%lu,%u,%u,%i,%i\n", (int)pDupInfo->GetStatus(), High, Low,
		pDupInfo->GetFullContentHash(), pDupInfo->GetFilteredContentHash(),
		pDupInfo->GetDupeScore(), (int)pDupInfo->GetDupeMode());
	fprintf(outfile, "%s\n", pDupInfo->GetName());
	fprintf(outfile, "%s\n", pDupInfo->GetDupeKey());
}

bool DiskState::LoadDupInfo(DupInfo* pDupInfo, FILE* infile, int iFormatVersion)
{
	char buf[1024];

	int iStatus;
	unsigned long High, Low;
	unsigned int iFullContentHash, iFilteredContentHash = 0;
	int iDupeScore, iDupe = 0, iDupeMode = 0;
	
	if (iFormatVersion >= 39)
	{
		if (fscanf(infile, "%i,%lu,%lu,%u,%u,%i,%i\n", &iStatus, &High, &Low, &iFullContentHash, &iFilteredContentHash, &iDupeScore, &iDupeMode) != 7) goto error;
	}
	else if (iFormatVersion >= 38)
	{
		if (fscanf(infile, "%i,%lu,%lu,%u,%u,%i,%i,%i\n", &iStatus, &High, &Low, &iFullContentHash, &iFilteredContentHash, &iDupeScore, &iDupe, &iDupeMode) != 8) goto error;
	}
	else if (iFormatVersion >= 37)
	{
		if (fscanf(infile, "%i,%lu,%lu,%u,%u,%i,%i\n", &iStatus, &High, &Low, &iFullContentHash, &iFilteredContentHash, &iDupeScore, &iDupe) != 7) goto error;
	}
	else if (iFormatVersion >= 34)
	{
		if (fscanf(infile, "%i,%lu,%lu,%u,%u,%i\n", &iStatus, &High, &Low, &iFullContentHash, &iFilteredContentHash, &iDupeScore) != 6) goto error;
	}
	else
	{
		if (fscanf(infile, "%i,%lu,%lu,%u,%i\n", &iStatus, &High, &Low, &iFullContentHash, &iDupeScore) != 5) goto error;
	}

	pDupInfo->SetStatus((DupInfo::EStatus)iStatus);
	pDupInfo->SetFullContentHash(iFullContentHash);
	pDupInfo->SetFilteredContentHash(iFilteredContentHash);
	pDupInfo->SetSize(Util::JoinInt64(High, Low));
	pDupInfo->SetDupeScore(iDupeScore);
	pDupInfo->SetDupeMode((EDupeMode)iDupeMode);

	if (!fgets(buf, sizeof(buf), infile)) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	pDupInfo->SetName(buf);

	if (!fgets(buf, sizeof(buf), infile)) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	if (iFormatVersion < 36) ConvertDupeKey(buf, sizeof(buf));
	pDupInfo->SetDupeKey(buf);

	return true;

error:
	return false;
}

void DiskState::SaveHistory(DownloadQueue* pDownloadQueue, FILE* outfile)
{
	debug("Saving history to disk");

	fprintf(outfile, "%i\n", (int)pDownloadQueue->GetHistory()->size());
	for (HistoryList::iterator it = pDownloadQueue->GetHistory()->begin(); it != pDownloadQueue->GetHistory()->end(); it++)
	{
		HistoryInfo* pHistoryInfo = *it;

		fprintf(outfile, "%i,%i,%i\n", pHistoryInfo->GetID(), (int)pHistoryInfo->GetKind(), (int)pHistoryInfo->GetTime());

		if (pHistoryInfo->GetKind() == HistoryInfo::hkNzb || pHistoryInfo->GetKind() == HistoryInfo::hkUrl)
		{
			SaveNZBInfo(pHistoryInfo->GetNZBInfo(), outfile);
		}
		else if (pHistoryInfo->GetKind() == HistoryInfo::hkDup)
		{
			SaveDupInfo(pHistoryInfo->GetDupInfo(), outfile);
		}
	}
}

bool DiskState::LoadHistory(DownloadQueue* pDownloadQueue, NZBList* pNZBList, FILE* infile, int iFormatVersion)
{
	debug("Loading history from disk");

	int size;
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		HistoryInfo* pHistoryInfo = NULL;
		HistoryInfo::EKind eKind = HistoryInfo::hkNzb;
		int iID = 0;
		int iTime;
	
		if (iFormatVersion >= 33)
		{
			int iKind = 0;
			if (fscanf(infile, "%i,%i,%i\n", &iID, &iKind, &iTime) != 3) goto error;
			eKind = (HistoryInfo::EKind)iKind;
		}
		else
		{
			if (iFormatVersion >= 24)
			{
				if (fscanf(infile, "%i\n", &iID) != 1) goto error;
			}

			if (iFormatVersion >= 15)
			{
				int iKind = 0;
				if (fscanf(infile, "%i\n", &iKind) != 1) goto error;
				eKind = (HistoryInfo::EKind)iKind;
			}
		}

		if (eKind == HistoryInfo::hkNzb)
		{
			NZBInfo* pNZBInfo = NULL;

			if (iFormatVersion < 43)
			{
				unsigned int iNZBIndex;
				if (fscanf(infile, "%i\n", &iNZBIndex) != 1) goto error;
				pNZBInfo = pNZBList->at(iNZBIndex - 1);
			}
			else
			{
				pNZBInfo = new NZBInfo();
				if (!LoadNZBInfo(pNZBInfo, infile, iFormatVersion)) goto error;
			}

			pHistoryInfo = new HistoryInfo(pNZBInfo);
			
			if (iFormatVersion < 28 && pNZBInfo->GetParStatus() == 0 &&
				pNZBInfo->GetUnpackStatus() == 0 && pNZBInfo->GetMoveStatus() == 0)
			{
				pNZBInfo->SetDeleteStatus(NZBInfo::dsManual);
			}
		}
		else if (eKind == HistoryInfo::hkUrl)
		{
			NZBInfo* pNZBInfo = new NZBInfo();
			if (iFormatVersion >= 46)
			{
				if (!LoadNZBInfo(pNZBInfo, infile, iFormatVersion)) goto error;
			}
			else
			{
				if (!LoadUrlInfo12(pNZBInfo, infile, iFormatVersion)) goto error;
			}
			pHistoryInfo = new HistoryInfo(pNZBInfo);
		}
		else if (eKind == HistoryInfo::hkDup)
		{
			DupInfo* pDupInfo = new DupInfo();
			if (!LoadDupInfo(pDupInfo, infile, iFormatVersion)) goto error;
			pHistoryInfo = new HistoryInfo(pDupInfo);
		}

		if (iFormatVersion >= 24)
		{
			pHistoryInfo->SetID(iID);
		}

		if (iFormatVersion < 33)
		{
			if (fscanf(infile, "%i\n", &iTime) != 1) goto error;
		}

		pHistoryInfo->SetTime((time_t)iTime);

		pDownloadQueue->GetHistory()->push_back(pHistoryInfo);
	}

	return true;

error:
	error("Error reading diskstate for history");
	return false;
}

/*
* Find index of nzb-info.
*/
int DiskState::FindNZBInfoIndex(NZBList* pNZBList, NZBInfo* pNZBInfo)
{
	int iNZBIndex = 0;
	for (NZBList::iterator it = pNZBList->begin(); it != pNZBList->end(); it++)
	{
		NZBInfo* pNZBInfo2 = *it;
		iNZBIndex++;
		if (pNZBInfo2 == pNZBInfo)
		{
			break;
		}
	}
	return iNZBIndex;
}

/*
* Find nzb-info by id.
*/
NZBInfo* DiskState::FindNZBInfo(DownloadQueue* pDownloadQueue, int iID)
{
	for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
	{
		NZBInfo* pNZBInfo = *it;
		if (pNZBInfo->GetID() == iID)
		{
			return pNZBInfo;
		}
	}
	return NULL;
}

/*
 * Deletes whole download queue including history.
 */
void DiskState::DiscardDownloadQueue()
{
	debug("Discarding queue");

	char szFullFilename[1024];
	snprintf(szFullFilename, 1024, "%s%s", g_pOptions->GetQueueDir(), "queue");
	szFullFilename[1024-1] = '\0';
	remove(szFullFilename);

	DirBrowser dir(g_pOptions->GetQueueDir());
	while (const char* filename = dir.Next())
	{
		// delete all files whose names have only characters '0'..'9'
		bool bOnlyNums = true;
		for (const char* p = filename; *p != '\0'; p++)
		{
			if (!('0' <= *p && *p <= '9'))
			{
				bOnlyNums = false;
				break;
			}
		}
		if (bOnlyNums)
		{
			snprintf(szFullFilename, 1024, "%s%s", g_pOptions->GetQueueDir(), filename);
			szFullFilename[1024-1] = '\0';
			remove(szFullFilename);
		}
	}
}

bool DiskState::DownloadQueueExists()
{
	debug("Checking if a saved queue exists on disk");

	char fileName[1024];
	snprintf(fileName, 1024, "%s%s", g_pOptions->GetQueueDir(), "queue");
	fileName[1024-1] = '\0';
	return Util::FileExists(fileName);
}

bool DiskState::DiscardFile(FileInfo* pFileInfo)
{
	// delete diskstate-file for file-info
	char fileName[1024];
	snprintf(fileName, 1024, "%s%i", g_pOptions->GetQueueDir(), pFileInfo->GetID());
	fileName[1024-1] = '\0';
	remove(fileName);

	return true;
}

void DiskState::CleanupTempDir(DownloadQueue* pDownloadQueue)
{
	// build array of IDs of files in queue for faster access
	int iFileCount = 0;
	for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
	{
		NZBInfo* pNZBInfo = *it;
		iFileCount += (int)pNZBInfo->GetFileList()->size();
	}

	int* ids = (int*)malloc(sizeof(int) * (iFileCount + 1));
	int* ptr = ids;
	for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
	{
		NZBInfo* pNZBInfo = *it;
		for (FileList::iterator it = pNZBInfo->GetFileList()->begin(); it != pNZBInfo->GetFileList()->end(); it++)
		{
			FileInfo* pFileInfo = *it;
			*ptr++ = pFileInfo->GetID();
		}
	}
	*ptr = 0;

	// read directory
	DirBrowser dir(g_pOptions->GetTempDir());
	while (const char* filename = dir.Next())
	{
		int id, part;
		bool del = strstr(filename, ".tmp") || strstr(filename, ".dec") ||
			((strstr(filename, ".out") && (sscanf(filename, "%i.out", &id) == 1) &&
			!(g_pOptions->GetContinuePartial() && g_pOptions->GetDirectWrite())) ||
			((sscanf(filename, "%i.%i", &id, &part) == 2) && !g_pOptions->GetContinuePartial()));
		if (!del)
		{
			if ((sscanf(filename, "%i.%i", &id, &part) == 2) ||
				(strstr(filename, ".out") && (sscanf(filename, "%i.out", &id) == 1)))
			{
				del = true;
				ptr = ids;
				while (*ptr)
				{
					if (*ptr == id)
					{
						del = false;
						break;
					}
					ptr++;
				}
			}
		}
		if (del)
		{
			char szFullFilename[1024];
			snprintf(szFullFilename, 1024, "%s%s", g_pOptions->GetTempDir(), filename);
			szFullFilename[1024-1] = '\0';
			remove(szFullFilename);
		}
	}
     
	free(ids);
}

/* For safety:
 * - first save to temp-file (feeds.new)
 * - then delete feeds
 * - then rename feeds.new to feeds
 */
bool DiskState::SaveFeeds(Feeds* pFeeds, FeedHistory* pFeedHistory)
{
	debug("Saving feeds state to disk");

	char destFilename[1024];
	snprintf(destFilename, 1024, "%s%s", g_pOptions->GetQueueDir(), "feeds");
	destFilename[1024-1] = '\0';

	char tempFilename[1024];
	snprintf(tempFilename, 1024, "%s%s", g_pOptions->GetQueueDir(), "feeds.new");
	tempFilename[1024-1] = '\0';

	if (pFeeds->empty() && pFeedHistory->empty())
	{
		remove(destFilename);
		return true;
	}

	FILE* outfile = fopen(tempFilename, "wb");

	if (!outfile)
	{
		error("Error saving diskstate: Could not create file %s", tempFilename);
		return false;
	}

	fprintf(outfile, "%s%i\n", FORMATVERSION_SIGNATURE, 3);

	// save status
	SaveFeedStatus(pFeeds, outfile);

	// save history
	SaveFeedHistory(pFeedHistory, outfile);

	fclose(outfile);

	// now rename to dest file name
	remove(destFilename);
	if (rename(tempFilename, destFilename))
	{
		error("Error saving diskstate: Could not rename file %s to %s", tempFilename, destFilename);
		return false;
	}

	return true;
}

bool DiskState::LoadFeeds(Feeds* pFeeds, FeedHistory* pFeedHistory)
{
	debug("Loading feeds state from disk");

	bool bOK = false;

	char fileName[1024];
	snprintf(fileName, 1024, "%s%s", g_pOptions->GetQueueDir(), "feeds");
	fileName[1024-1] = '\0';

	if (!Util::FileExists(fileName))
	{
		return true;
	}

	FILE* infile = fopen(fileName, "rb");

	if (!infile)
	{
		error("Error reading diskstate: could not open file %s", fileName);
		return false;
	}

	char FileSignatur[128];
	fgets(FileSignatur, sizeof(FileSignatur), infile);
	int iFormatVersion = ParseFormatVersion(FileSignatur);
	if (iFormatVersion > 3)
	{
		error("Could not load diskstate due to file version mismatch");
		fclose(infile);
		return false;
	}

	// load feed status
	if (!LoadFeedStatus(pFeeds, infile, iFormatVersion)) goto error;

	// load feed history
	if (!LoadFeedHistory(pFeedHistory, infile, iFormatVersion)) goto error;

	bOK = true;

error:

	fclose(infile);
	if (!bOK)
	{
		error("Error reading diskstate for file %s", fileName);
	}

	return bOK;
}

bool DiskState::SaveFeedStatus(Feeds* pFeeds, FILE* outfile)
{
	debug("Saving feed status to disk");

	fprintf(outfile, "%i\n", (int)pFeeds->size());
	for (Feeds::iterator it = pFeeds->begin(); it != pFeeds->end(); it++)
	{
		FeedInfo* pFeedInfo = *it;

		fprintf(outfile, "%s\n", pFeedInfo->GetUrl());
		fprintf(outfile, "%u\n", pFeedInfo->GetFilterHash());
		fprintf(outfile, "%i\n", (int)pFeedInfo->GetLastUpdate());
	}

	return true;
}

bool DiskState::LoadFeedStatus(Feeds* pFeeds, FILE* infile, int iFormatVersion)
{
	debug("Loading feed status from disk");

	int size;
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		char szUrl[1024];
		if (!fgets(szUrl, sizeof(szUrl), infile)) goto error;
		if (szUrl[0] != 0) szUrl[strlen(szUrl)-1] = 0; // remove traling '\n'

		char szFilter[1024];
		if (iFormatVersion == 2)
		{
			if (!fgets(szFilter, sizeof(szFilter), infile)) goto error;
			if (szFilter[0] != 0) szFilter[strlen(szFilter)-1] = 0; // remove traling '\n'
		}

		unsigned int iFilterHash = 0;
		if (iFormatVersion >= 3)
		{
			if (fscanf(infile, "%u\n", &iFilterHash) != 1) goto error;
		}

		int iLastUpdate = 0;
		if (fscanf(infile, "%i\n", &iLastUpdate) != 1) goto error;

		for (Feeds::iterator it = pFeeds->begin(); it != pFeeds->end(); it++)
		{
			FeedInfo* pFeedInfo = *it;

			if (!strcmp(pFeedInfo->GetUrl(), szUrl) &&
				((iFormatVersion == 1) ||
				 (iFormatVersion == 2 && !strcmp(pFeedInfo->GetFilter(), szFilter)) ||
				 (iFormatVersion >= 3 && pFeedInfo->GetFilterHash() == iFilterHash)))
			{
				pFeedInfo->SetLastUpdate((time_t)iLastUpdate);
			}
		}
	}

	return true;

error:
	error("Error reading feed status from disk");
	return false;
}

bool DiskState::SaveFeedHistory(FeedHistory* pFeedHistory, FILE* outfile)
{
	debug("Saving feed history to disk");

	fprintf(outfile, "%i\n", (int)pFeedHistory->size());
	for (FeedHistory::iterator it = pFeedHistory->begin(); it != pFeedHistory->end(); it++)
	{
		FeedHistoryInfo* pFeedHistoryInfo = *it;

		fprintf(outfile, "%i,%i\n", (int)pFeedHistoryInfo->GetStatus(), (int)pFeedHistoryInfo->GetLastSeen());
		fprintf(outfile, "%s\n", pFeedHistoryInfo->GetUrl());
	}

	return true;
}

bool DiskState::LoadFeedHistory(FeedHistory* pFeedHistory, FILE* infile, int iFormatVersion)
{
	debug("Loading feed history from disk");

	int size;
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		int iStatus = 0;
		int iLastSeen = 0;
		int r = fscanf(infile, "%i,%i\n", &iStatus, &iLastSeen);
		if (r != 2) goto error;

		char szUrl[1024];
		if (!fgets(szUrl, sizeof(szUrl), infile)) goto error;
		if (szUrl[0] != 0) szUrl[strlen(szUrl)-1] = 0; // remove traling '\n'

		pFeedHistory->Add(szUrl, (FeedHistoryInfo::EStatus)(iStatus), (time_t)(iLastSeen));
	}

	return true;

error:
	error("Error reading feed history from disk");
	return false;
}

// calculate critical health for old NZBs
void DiskState::CalcCriticalHealth(NZBList* pNZBList)
{
	// build list of old NZBs which do not have critical health calculated
	for (NZBList::iterator it = pNZBList->begin(); it != pNZBList->end(); it++)
	{
		NZBInfo* pNZBInfo = *it;
		if (pNZBInfo->CalcCriticalHealth(false) == 1000)
		{
			debug("Calculating critical health for %s", pNZBInfo->GetName());

			for (FileList::iterator it = pNZBInfo->GetFileList()->begin(); it != pNZBInfo->GetFileList()->end(); it++)
			{
				FileInfo* pFileInfo = *it;

				char szLoFileName[1024];
				strncpy(szLoFileName, pFileInfo->GetFilename(), 1024);
				szLoFileName[1024-1] = '\0';
				for (char* p = szLoFileName; *p; p++) *p = tolower(*p); // convert string to lowercase
				bool bParFile = strstr(szLoFileName, ".par2");

				pFileInfo->SetParFile(bParFile);
				if (bParFile)
				{
					pNZBInfo->SetParSize(pNZBInfo->GetParSize() + pFileInfo->GetSize());
				}
			}
		}
	}
}

void DiskState::CalcFileStats(DownloadQueue* pDownloadQueue, int iFormatVersion)
{
	for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
	{
		NZBInfo* pNZBInfo = *it;

		int iPausedFileCount = 0;
		long long lRemainingSize = 0;
		long long lPausedSize = 0;
		int iRemainingParCount = 0;

		for (FileList::iterator it2 = pNZBInfo->GetFileList()->begin(); it2 != pNZBInfo->GetFileList()->end(); it2++)
		{
			FileInfo* pFileInfo = *it2;

			lRemainingSize += pFileInfo->GetRemainingSize();
			if (pFileInfo->GetPaused())
			{
				lPausedSize += pFileInfo->GetRemainingSize();
				iPausedFileCount++;
			}
			if (pFileInfo->GetParFile())
			{
				iRemainingParCount++;
			}
		}

		pNZBInfo->SetRemainingSize(lRemainingSize);
		pNZBInfo->SetPausedSize(lPausedSize);
		pNZBInfo->SetPausedFileCount(iPausedFileCount);
		pNZBInfo->SetRemainingParCount(iRemainingParCount);

		if (iFormatVersion < 44)
		{
			pNZBInfo->UpdateMinMaxTime();
		}
	}
}

void DiskState::ConvertDupeKey(char* buf, int bufsize)
{
	if (strncmp(buf, "rageid=", 7))
	{
		return;
	}

	int iRageId = atoi(buf + 7);
	int iSeason = 0;
	int iEpisode = 0;
	char* p = strchr(buf + 7, ',');
	if (p)
	{
		iSeason = atoi(p + 1);
		p = strchr(p + 1, ',');
		if (p)
		{
			iEpisode = atoi(p + 1);
		}
	}

	if (iRageId != 0 && iSeason != 0 && iEpisode != 0)
	{
		snprintf(buf, bufsize, "rageid=%i-S%02i-E%02i", iRageId, iSeason, iEpisode);
	}
}

bool DiskState::SaveStats(Servers* pServers, ServerVolumes* pServerVolumes)
{
	debug("Saving stats to disk");

	char destFilename[1024];
	snprintf(destFilename, 1024, "%s%s", g_pOptions->GetQueueDir(), "stats");
	destFilename[1024-1] = '\0';

	char tempFilename[1024];
	snprintf(tempFilename, 1024, "%s%s", g_pOptions->GetQueueDir(), "stats.new");
	tempFilename[1024-1] = '\0';

	if (pServers->empty())
	{
		remove(destFilename);
		return true;
	}

	FILE* outfile = fopen(tempFilename, "wb");

	if (!outfile)
	{
		error("Error saving diskstate: Could not create file %s", tempFilename);
		return false;
	}

	fprintf(outfile, "%s%i\n", FORMATVERSION_SIGNATURE, 2);

	// save server names
	SaveServerInfo(pServers, outfile);

	// save stat
	SaveVolumeStat(pServerVolumes, outfile);

	fclose(outfile);

	// now rename to dest file name
	remove(destFilename);
	if (rename(tempFilename, destFilename))
	{
		error("Error saving diskstate: Could not rename file %s to %s", tempFilename, destFilename);
		return false;
	}

	return true;
}

bool DiskState::LoadStats(Servers* pServers, ServerVolumes* pServerVolumes)
{
	debug("Loading stats from disk");

	bool bOK = false;

	char fileName[1024];
	snprintf(fileName, 1024, "%s%s", g_pOptions->GetQueueDir(), "stats");
	fileName[1024-1] = '\0';

	if (!Util::FileExists(fileName))
	{
		return true;
	}

	FILE* infile = fopen(fileName, "rb");

	if (!infile)
	{
		error("Error reading diskstate: could not open file %s", fileName);
		return false;
	}

	char FileSignatur[128];
	fgets(FileSignatur, sizeof(FileSignatur), infile);
	int iFormatVersion = ParseFormatVersion(FileSignatur);
	if (iFormatVersion > 2)
	{
		error("Could not load diskstate due to file version mismatch");
		fclose(infile);
		return false;
	}

	if (!LoadServerInfo(pServers, infile, iFormatVersion)) goto error;

	if (iFormatVersion >=2)
	{
		if (!LoadVolumeStat(pServers, pServerVolumes, infile, iFormatVersion)) goto error;
	}

	bOK = true;

error:

	fclose(infile);
	if (!bOK)
	{
		error("Error reading diskstate for file %s", fileName);
	}

	return bOK;
}

bool DiskState::SaveServerInfo(Servers* pServers, FILE* outfile)
{
	debug("Saving server info to disk");

	fprintf(outfile, "%i\n", (int)pServers->size());
	for (Servers::iterator it = pServers->begin(); it != pServers->end(); it++)
	{
		NewsServer* pNewsServer = *it;

		fprintf(outfile, "%s\n", pNewsServer->GetName());
		fprintf(outfile, "%s\n", pNewsServer->GetHost());
		fprintf(outfile, "%i\n", pNewsServer->GetPort());
		fprintf(outfile, "%s\n", pNewsServer->GetUser());
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
	int				m_iStateID;
	char*			m_szName;
	char*			m_szHost;
	int				m_iPort;
	char*			m_szUser;
	bool			m_bMatched;

					~ServerRef();
	int				GetStateID() { return m_iStateID; }
	const char*		GetName() { return m_szName; }
	const char*		GetHost() { return m_szHost; }
	int				GetPort() { return m_iPort; }
	const char*		GetUser() { return m_szUser; }
	bool			GetMatched() { return m_bMatched; }
	void			SetMatched(bool bMatched) { m_bMatched = bMatched; }
};

typedef std::deque<ServerRef*> ServerRefList;

ServerRef::~ServerRef()
{
	free(m_szName);
	free(m_szHost);
	free(m_szUser);
}

enum ECriteria
{
	eName,
	eHost,
	ePort,
	eUser
};

void FindCandidates(NewsServer* pNewsServer, ServerRefList* pRefs, ECriteria eCriteria, bool bKeepIfNothing)
{
	ServerRefList originalRefs;
	originalRefs.insert(originalRefs.begin(), pRefs->begin(), pRefs->end());

	int index = 0;
	for (ServerRefList::iterator it = pRefs->begin(); it != pRefs->end(); )
	{
		ServerRef* pRef = *it;
		bool bMatch = false;
		switch(eCriteria)
		{
			case eName:
				bMatch = !strcasecmp(pNewsServer->GetName(), pRef->GetName());
				break;
			case eHost:
				bMatch = !strcasecmp(pNewsServer->GetHost(), pRef->GetHost());
				break;
			case ePort:
				bMatch = pNewsServer->GetPort() == pRef->GetPort();
				break;
			case eUser:
				bMatch = !strcasecmp(pNewsServer->GetUser(), pRef->GetUser());
				break;
		}
		if (bMatch && !pRef->GetMatched())
		{
			it++;
			index++;
		}
		else
		{
			pRefs->erase(it);
			it = pRefs->begin() + index;
		}
	}

	if (pRefs->size() == 0 && bKeepIfNothing)
	{
		pRefs->insert(pRefs->begin(), originalRefs.begin(), originalRefs.end());
	}
}

void MatchServers(Servers* pServers, ServerRefList* pServerRefs)
{
	// Step 1: trying perfect match
	for (Servers::iterator it = pServers->begin(); it != pServers->end(); it++)
	{
		NewsServer* pNewsServer = *it;
		ServerRefList matchedRefs;
		matchedRefs.insert(matchedRefs.begin(), pServerRefs->begin(), pServerRefs->end());
		FindCandidates(pNewsServer, &matchedRefs, eName, false);
		FindCandidates(pNewsServer, &matchedRefs, eHost, false);
		FindCandidates(pNewsServer, &matchedRefs, ePort, false);
		FindCandidates(pNewsServer, &matchedRefs, eUser, false);

		if (matchedRefs.size() == 1)
		{
			ServerRef* pRef = matchedRefs.front();
			pNewsServer->SetStateID(pRef->GetStateID());
			pRef->SetMatched(true);
		}
	}

	// Step 2: matching host, port, username and server-name
	for (Servers::iterator it = pServers->begin(); it != pServers->end(); it++)
	{
		NewsServer* pNewsServer = *it;
		if (!pNewsServer->GetStateID())
		{
			ServerRefList matchedRefs;
			matchedRefs.insert(matchedRefs.begin(), pServerRefs->begin(), pServerRefs->end());

			FindCandidates(pNewsServer, &matchedRefs, eHost, false);

			if (matchedRefs.size() > 1)
			{
				FindCandidates(pNewsServer, &matchedRefs, eName, true);
			}

			if (matchedRefs.size() > 1)
			{
				FindCandidates(pNewsServer, &matchedRefs, eUser, true);
			}

			if (matchedRefs.size() > 1)
			{
				FindCandidates(pNewsServer, &matchedRefs, ePort, true);
			}

			if (!matchedRefs.empty())
			{
				ServerRef* pRef = matchedRefs.front();
				pNewsServer->SetStateID(pRef->GetStateID());
				pRef->SetMatched(true);
			}
		}
	}
}

/*
 * END: Server matching
 ***************************************************************************************
 */

bool DiskState::LoadServerInfo(Servers* pServers, FILE* infile, int iFormatVersion)
{
	debug("Loading server info from disk");

	ServerRefList serverRefs;

	int size;
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		char szName[1024];
		if (!fgets(szName, sizeof(szName), infile)) goto error;
		if (szName[0] != 0) szName[strlen(szName)-1] = 0; // remove traling '\n'

		char szHost[200];
		if (!fgets(szHost, sizeof(szHost), infile)) goto error;
		if (szHost[0] != 0) szHost[strlen(szHost)-1] = 0; // remove traling '\n'

		int iPort;
		if (fscanf(infile, "%i\n", &iPort) != 1) goto error;

		char szUser[100];
		if (!fgets(szUser, sizeof(szUser), infile)) goto error;
		if (szUser[0] != 0) szUser[strlen(szUser)-1] = 0; // remove traling '\n'

		ServerRef* pRef = new ServerRef();
		pRef->m_iStateID = i + 1;
		pRef->m_szName = strdup(szName);
		pRef->m_szHost = strdup(szHost);
		pRef->m_iPort = iPort;
		pRef->m_szUser = strdup(szUser);
		pRef->m_bMatched = false;
		serverRefs.push_back(pRef);
	}

	MatchServers(pServers, &serverRefs);

	for (ServerRefList::iterator it = serverRefs.begin(); it != serverRefs.end(); it++)
	{
		delete *it;
	}

	debug("******** MATCHING NEWS-SERVERS **********");
	for (Servers::iterator it = pServers->begin(); it != pServers->end(); it++)
	{
		NewsServer* pNewsServer = *it;
		debug("Server %i -> %i", pNewsServer->GetID(), pNewsServer->GetStateID());
		debug("Server %i.Name: %s", pNewsServer->GetID(), pNewsServer->GetName());
		debug("Server %i.Host: %s:%i", pNewsServer->GetID(), pNewsServer->GetHost(), pNewsServer->GetPort());
	}

	return true;

error:
	error("Error reading server info from disk");

	for (ServerRefList::iterator it = serverRefs.begin(); it != serverRefs.end(); it++)
	{
		delete *it;
	}

	return false;
}

bool DiskState::SaveVolumeStat(ServerVolumes* pServerVolumes, FILE* outfile)
{
	debug("Saving volume stats to disk");

	fprintf(outfile, "%i\n", (int)pServerVolumes->size());
	for (ServerVolumes::iterator it = pServerVolumes->begin(); it != pServerVolumes->end(); it++)
	{
		ServerVolume* pServerVolume = *it;

		fprintf(outfile, "%i,%i\n", pServerVolume->GetFirstDay(), (int)pServerVolume->GetDataTime());

		unsigned long High1, Low1;
		Util::SplitInt64(pServerVolume->GetTotalBytes(), &High1, &Low1);
		fprintf(outfile, "%lu,%lu\n", High1, Low1);

		ServerVolume::VolumeArray* VolumeArrays[] = { pServerVolume->BytesPerSeconds(),
			pServerVolume->BytesPerMinutes(), pServerVolume->BytesPerHours(), pServerVolume->BytesPerDays() };
		for (int i=0; i < 4; i++)
		{
			ServerVolume::VolumeArray* pVolumeArray = VolumeArrays[i];

			fprintf(outfile, "%i\n", (int)pVolumeArray->size());
			for (ServerVolume::VolumeArray::iterator it2 = pVolumeArray->begin(); it2 != pVolumeArray->end(); it2++)
			{
				long long lBytes = *it2;
				Util::SplitInt64(lBytes, &High1, &Low1);
				fprintf(outfile, "%lu,%lu\n", High1, Low1);
			}
		}
	}

	return true;
}

bool DiskState::LoadVolumeStat(Servers* pServers, ServerVolumes* pServerVolumes, FILE* infile, int iFormatVersion)
{
	debug("Loading volume stats from disk");

	int size;
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		ServerVolume* pServerVolume = NULL;

		if (i == 0)
		{
			pServerVolume = pServerVolumes->at(0);
		}
		else
		{
			for (Servers::iterator it = pServers->begin(); it != pServers->end(); it++)
			{
				NewsServer* pNewsServer = *it;
				if (pNewsServer->GetStateID() == i)
				{
					pServerVolume = pServerVolumes->at(pNewsServer->GetStateID());
				}
			}
		}

		int iFirstDay, iDataTime;
		if (fscanf(infile, "%i,%i\n", &iFirstDay, &iDataTime) != 2) goto error;
		if (pServerVolume) pServerVolume->SetFirstDay(iFirstDay);
		if (pServerVolume) pServerVolume->SetDataTime((time_t)iDataTime);

		unsigned long High1, Low1;
		if (fscanf(infile, "%lu,%lu\n", &High1, &Low1) != 2) goto error;
		if (pServerVolume) pServerVolume->SetTotalBytes(Util::JoinInt64(High1, Low1));

		ServerVolume::VolumeArray* VolumeArrays[] = { pServerVolume ? pServerVolume->BytesPerSeconds() : NULL,
			pServerVolume ? pServerVolume->BytesPerMinutes() : NULL,
			pServerVolume ? pServerVolume->BytesPerHours() : NULL,
			pServerVolume ? pServerVolume->BytesPerDays() : NULL };
		for (int k=0; k < 4; k++)
		{
			ServerVolume::VolumeArray* pVolumeArray = VolumeArrays[k];

			int iArrSize;
			if (fscanf(infile, "%i\n", &iArrSize) != 1) goto error;
			if (pVolumeArray) pVolumeArray->resize(iArrSize);

			for (int j = 0; j < iArrSize; j++)
			{
				if (fscanf(infile, "%lu,%lu\n", &High1, &Low1) != 2) goto error;
				if (pVolumeArray) (*pVolumeArray)[j] = Util::JoinInt64(High1, Low1);
			}
		}
	}

	return true;

error:
	error("Error reading volume stats from disk");

	return false;
}
