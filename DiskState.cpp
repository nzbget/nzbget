/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include <set>

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

	if (pDownloadQueue->GetFileQueue()->empty() && 
		pDownloadQueue->GetUrlQueue()->empty() &&
		pDownloadQueue->GetPostQueue()->empty() &&
		pDownloadQueue->GetHistoryList()->empty())
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

	fprintf(outfile, "%s%i\n", FORMATVERSION_SIGNATURE, 34);

	// save nzb-infos
	SaveNZBList(pDownloadQueue, outfile);

	// save file-infos
	SaveFileQueue(pDownloadQueue, pDownloadQueue->GetFileQueue(), outfile);

	// save post-queue
	SavePostQueue(pDownloadQueue, outfile);

	// save url-queue
	SaveUrlQueue(pDownloadQueue, outfile);

	// save history
	SaveHistory(pDownloadQueue, outfile);

	// save parked file-infos
	SaveFileQueue(pDownloadQueue, pDownloadQueue->GetParkedFiles(), outfile);

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
	if (iFormatVersion < 3 || iFormatVersion > 34)
	{
		error("Could not load diskstate due to file version mismatch");
		fclose(infile);
		return false;
	}

	// load nzb-infos
	if (!LoadNZBList(pDownloadQueue, infile, iFormatVersion)) goto error;

	// load file-infos
	if (!LoadFileQueue(pDownloadQueue, pDownloadQueue->GetFileQueue(), infile, iFormatVersion)) goto error;

	if (iFormatVersion >= 7)
	{
		// load post-queue
		if (!LoadPostQueue(pDownloadQueue, infile, iFormatVersion)) goto error;
	}
	else if (iFormatVersion < 7 && g_pOptions->GetReloadPostQueue())
	{
		// load post-queue created with older version of program
		LoadOldPostQueue(pDownloadQueue);
	}

	if (iFormatVersion >= 15)
	{
		// load url-queue
		if (!LoadUrlQueue(pDownloadQueue, infile, iFormatVersion)) goto error;
	}

	if (iFormatVersion >= 9)
	{
		// load history
		if (!LoadHistory(pDownloadQueue, infile, iFormatVersion)) goto error;

		// load parked file-infos
		if (!LoadFileQueue(pDownloadQueue, pDownloadQueue->GetParkedFiles(), infile, iFormatVersion)) goto error;
	}

	if (iFormatVersion < 29)
	{
		CalcCriticalHealth(pDownloadQueue);
	}

	bOK = true;

error:

	fclose(infile);
	if (!bOK)
	{
		error("Error reading diskstate for file %s", fileName);
	}

	pDownloadQueue->GetNZBInfoList()->ReleaseAll();

	return bOK;
}

void DiskState::SaveNZBList(DownloadQueue* pDownloadQueue, FILE* outfile)
{
	debug("Saving nzb list to disk");

	fprintf(outfile, "%i\n", (int)pDownloadQueue->GetNZBInfoList()->size());
	for (NZBInfoList::iterator it = pDownloadQueue->GetNZBInfoList()->begin(); it != pDownloadQueue->GetNZBInfoList()->end(); it++)
	{
		NZBInfo* pNZBInfo = *it;
		fprintf(outfile, "%i\n", pNZBInfo->GetID());
		fprintf(outfile, "%s\n", pNZBInfo->GetFilename());
		fprintf(outfile, "%s\n", pNZBInfo->GetDestDir());
		fprintf(outfile, "%s\n", pNZBInfo->GetFinalDir());
		fprintf(outfile, "%s\n", pNZBInfo->GetQueuedFilename());
		fprintf(outfile, "%s\n", pNZBInfo->GetName());
		fprintf(outfile, "%s\n", pNZBInfo->GetCategory());
		fprintf(outfile, "%i\n", (int)pNZBInfo->GetPostProcess());
		fprintf(outfile, "%i,%i,%i,%i\n", (int)pNZBInfo->GetParStatus(), (int)pNZBInfo->GetUnpackStatus(),
			(int)pNZBInfo->GetMoveStatus(), (int)pNZBInfo->GetRenameStatus());
		fprintf(outfile, "%i,%i,%i,%i\n", (int)pNZBInfo->GetDeleted(), (int)pNZBInfo->GetUnpackCleanedUpDisk(),
			(int)pNZBInfo->GetHealthPaused(), (int)pNZBInfo->GetHealthDeleted());
		fprintf(outfile, "%i,%i\n", pNZBInfo->GetFileCount(), pNZBInfo->GetParkedFileCount());

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
		fprintf(outfile, "%i,%i,%i\n", (bool)pNZBInfo->GetDupe(), (bool)pNZBInfo->GetNoDupeCheck(), pNZBInfo->GetDupeScore());

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
	}
}

bool DiskState::LoadNZBList(DownloadQueue* pDownloadQueue, FILE* infile, int iFormatVersion)
{
	debug("Loading nzb list from disk");

	int size;
	char buf[10240];

	// load nzb-infos
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		NZBInfo* pNZBInfo = new NZBInfo();
		pNZBInfo->Retain();
		pDownloadQueue->GetNZBInfoList()->Add(pNZBInfo);

		if (iFormatVersion >= 24)
		{
			int iID;
			if (fscanf(infile, "%i\n", &iID) != 1) goto error;
			pNZBInfo->SetID(iID);
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

			int iPostProcess;
			if (fscanf(infile, "%i\n", &iPostProcess) != 1) goto error;
			pNZBInfo->SetPostProcess((bool)iPostProcess);
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
			int iParStatus, iUnpackStatus, iScriptStatus, iMoveStatus = 0, iRenameStatus = 0;
			if (iFormatVersion >= 23)
			{
				if (fscanf(infile, "%i,%i,%i,%i\n", &iParStatus, &iUnpackStatus, &iMoveStatus, &iRenameStatus) != 4) goto error;
			}
			else if (iFormatVersion >= 21)
			{
				if (fscanf(infile, "%i,%i,%i,%i,%i\n", &iParStatus, &iUnpackStatus, &iScriptStatus, &iMoveStatus, &iRenameStatus) != 5) goto error;
			}
			else if (iFormatVersion >= 20)
			{
				if (fscanf(infile, "%i,%i,%i,%i\n", &iParStatus, &iUnpackStatus, &iScriptStatus, &iMoveStatus) != 4) goto error;
			}
			else
			{
				if (fscanf(infile, "%i,%i,%i\n", &iParStatus, &iUnpackStatus, &iScriptStatus) != 3) goto error;
			}
			pNZBInfo->SetParStatus((NZBInfo::EParStatus)iParStatus);
			pNZBInfo->SetUnpackStatus((NZBInfo::EUnpackStatus)iUnpackStatus);
			pNZBInfo->SetMoveStatus((NZBInfo::EMoveStatus)iMoveStatus);
			pNZBInfo->SetRenameStatus((NZBInfo::ERenameStatus)iRenameStatus);
			if (iFormatVersion < 23)
			{
				if (iScriptStatus > 1) iScriptStatus--;
				pNZBInfo->GetScriptStatuses()->Add("SCRIPT", (ScriptStatus::EStatus)iScriptStatus);
			}
		}

		if (iFormatVersion >= 28)
		{
			int iDeleted, iUnpackCleanedUpDisk, bHealthPaused, bHealthDeleted;
			if (fscanf(infile, "%i,%i,%i,%i\n", &iDeleted, &iUnpackCleanedUpDisk, &bHealthPaused, &bHealthDeleted) != 4) goto error;
			pNZBInfo->SetDeleted((bool)iDeleted);
			pNZBInfo->SetUnpackCleanedUpDisk((bool)iUnpackCleanedUpDisk);
			pNZBInfo->SetHealthPaused((bool)bHealthPaused);
			pNZBInfo->SetHealthDeleted((bool)bHealthDeleted);

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
			pNZBInfo->SetDupeKey(buf);

			int iDupe, iNoDupeCheck, iDupeScore;
			if (fscanf(infile, "%i,%i,%i\n", &iDupe, &iNoDupeCheck, &iDupeScore) != 3) goto error;
			pNZBInfo->SetDupe((bool)iDupe);
			pNZBInfo->SetNoDupeCheck((bool)iNoDupeCheck);
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
	}

	return true;

error:
	error("Error reading nzb list from disk");
	return false;
}

void DiskState::SaveFileQueue(DownloadQueue* pDownloadQueue, FileQueue* pFileQueue, FILE* outfile)
{
	debug("Saving file queue to disk");

	// save file-infos
	fprintf(outfile, "%i\n", (int)pFileQueue->size());
	for (FileQueue::iterator it = pFileQueue->begin(); it != pFileQueue->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (!pFileInfo->GetDeleted())
		{
			int iNZBIndex = FindNZBInfoIndex(pDownloadQueue, pFileInfo->GetNZBInfo());
			fprintf(outfile, "%i,%i,%i,%i,%i,%i\n", pFileInfo->GetID(), iNZBIndex, (int)pFileInfo->GetPaused(), 
				(int)pFileInfo->GetTime(), pFileInfo->GetPriority(), (int)pFileInfo->GetExtraPriority());
		}
	}
}

bool DiskState::LoadFileQueue(DownloadQueue* pDownloadQueue, FileQueue* pFileQueue, FILE* infile, int iFormatVersion)
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
		if (iNZBIndex > pDownloadQueue->GetNZBInfoList()->size()) goto error;

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
			pFileInfo->SetPriority(iPriority);
			pFileInfo->SetExtraPriority(iExtraPriority != 0);
			pFileInfo->SetNZBInfo(pDownloadQueue->GetNZBInfoList()->at(iNZBIndex - 1));
			if (iFormatVersion < 30)
			{
				pFileInfo->GetNZBInfo()->SetTotalArticles(
					pFileInfo->GetNZBInfo()->GetTotalArticles() + pFileInfo->GetTotalArticles());
			}
			pFileQueue->push_back(pFileInfo);
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

void DiskState::SavePostQueue(DownloadQueue* pDownloadQueue, FILE* outfile)
{
	debug("Saving post-queue to disk");

	fprintf(outfile, "%i\n", (int)pDownloadQueue->GetPostQueue()->size());
	for (PostQueue::iterator it = pDownloadQueue->GetPostQueue()->begin(); it != pDownloadQueue->GetPostQueue()->end(); it++)
	{
		PostInfo* pPostInfo = *it;
		int iNZBIndex = FindNZBInfoIndex(pDownloadQueue, pPostInfo->GetNZBInfo());
		fprintf(outfile, "%i,%i\n", iNZBIndex, (int)pPostInfo->GetStage());
		fprintf(outfile, "%s\n", pPostInfo->GetInfoName());
	}
}

bool DiskState::LoadPostQueue(DownloadQueue* pDownloadQueue, FILE* infile, int iFormatVersion)
{
	debug("Loading post-queue from disk");

	bool bSkipPostQueue = !g_pOptions->GetReloadPostQueue();
	int size;
	char buf[10240];

	// load file-infos
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		PostInfo* pPostInfo = NULL;
		unsigned int iNZBIndex, iStage, iDummy;
		if (iFormatVersion < 19)
		{
			if (fscanf(infile, "%i,%i,%i,%i\n", &iNZBIndex, &iDummy, &iDummy, &iStage) != 4) goto error;
		}
		else if (iFormatVersion < 22)
		{
			if (fscanf(infile, "%i,%i,%i,%i\n", &iNZBIndex, &iDummy, &iDummy, &iStage) != 4) goto error;
		}
		else
		{
			if (fscanf(infile, "%i,%i\n", &iNZBIndex, &iStage) != 2) goto error;
		}
		if (iFormatVersion < 18 && iStage > (int)PostInfo::ptVerifyingRepaired) iStage++;
		if (iFormatVersion < 21 && iStage > (int)PostInfo::ptVerifyingRepaired) iStage++;
		if (iFormatVersion < 20 && iStage > (int)PostInfo::ptUnpacking) iStage++;

		if (!bSkipPostQueue)
		{
			pPostInfo = new PostInfo();
			pPostInfo->SetNZBInfo(pDownloadQueue->GetNZBInfoList()->at(iNZBIndex - 1));
			pPostInfo->SetStage((PostInfo::EStage)iStage);
		}

		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		if (!bSkipPostQueue) pPostInfo->SetInfoName(buf);

		if (iFormatVersion < 22)
		{
			// ParFilename, ignore
			if (!fgets(buf, sizeof(buf), infile)) goto error;
		}

		if (!bSkipPostQueue)
		{
			pDownloadQueue->GetPostQueue()->push_back(pPostInfo);
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
bool DiskState::LoadOldPostQueue(DownloadQueue* pDownloadQueue)
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
		PostInfo* pPostInfo = new PostInfo();

		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'

		// find NZBInfo based on NZBFilename
		NZBInfo* pNZBInfo = NULL;
		for (NZBInfoList::iterator it = pDownloadQueue->GetNZBInfoList()->begin(); it != pDownloadQueue->GetNZBInfoList()->end(); it++)
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
			pNZBInfo->Retain();
			pDownloadQueue->GetNZBInfoList()->Add(pNZBInfo);
			pNZBInfo->SetFilename(buf);
		}

		pPostInfo->SetNZBInfo(pNZBInfo);

		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		if (bNewNZBInfo)
		{
			pNZBInfo->SetDestDir(buf);
		}

		// ParFilename, ignore
		if (!fgets(buf, sizeof(buf), infile)) goto error;

		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		pPostInfo->SetInfoName(buf);

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

		pDownloadQueue->GetPostQueue()->push_back(pPostInfo);
	}

	fclose(infile);
	return true;

error:
	fclose(infile);
	error("Error reading diskstate for file %s", fileName);
	return false;
}

void DiskState::SaveUrlQueue(DownloadQueue* pDownloadQueue, FILE* outfile)
{
	debug("Saving url-queue to disk");

	fprintf(outfile, "%i\n", (int)pDownloadQueue->GetUrlQueue()->size());
	for (UrlQueue::iterator it = pDownloadQueue->GetUrlQueue()->begin(); it != pDownloadQueue->GetUrlQueue()->end(); it++)
	{
		UrlInfo* pUrlInfo = *it;
		SaveUrlInfo(pUrlInfo, outfile);
	}
}

bool DiskState::LoadUrlQueue(DownloadQueue* pDownloadQueue, FILE* infile, int iFormatVersion)
{
	debug("Loading url-queue from disk");

	bool bSkipUrlQueue = !g_pOptions->GetReloadUrlQueue();
	int size;

	// load url-infos
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		UrlInfo* pUrlInfo = new UrlInfo();

		if (!LoadUrlInfo(pUrlInfo, infile, iFormatVersion)) goto error;

		if (!bSkipUrlQueue)
		{
			pDownloadQueue->GetUrlQueue()->push_back(pUrlInfo);
		}
		else
		{
			delete pUrlInfo;
		}
	}

	return true;

error:
	error("Error reading diskstate for url-queue");
	return false;
}

void DiskState::SaveUrlInfo(UrlInfo* pUrlInfo, FILE* outfile)
{
	fprintf(outfile, "%i\n", pUrlInfo->GetID());
	fprintf(outfile, "%i,%i\n", (int)pUrlInfo->GetStatus(), pUrlInfo->GetPriority());
	fprintf(outfile, "%i,%i\n", (int)pUrlInfo->GetAddTop(), pUrlInfo->GetAddPaused());
	fprintf(outfile, "%s\n", pUrlInfo->GetURL());
	fprintf(outfile, "%s\n", pUrlInfo->GetNZBFilename());
	fprintf(outfile, "%s\n", pUrlInfo->GetCategory());
	fprintf(outfile, "%s\n", pUrlInfo->GetDupeKey());
	fprintf(outfile, "%i,%i\n", (int)pUrlInfo->GetNoDupeCheck(), pUrlInfo->GetDupeScore());
}

bool DiskState::LoadUrlInfo(UrlInfo* pUrlInfo, FILE* infile, int iFormatVersion)
{
	char buf[10240];

	if (iFormatVersion >= 24)
	{
		int iID;
		if (fscanf(infile, "%i\n", &iID) != 1) goto error;
		pUrlInfo->SetID(iID);
	}

	int iStatus, iPriority;
	if (fscanf(infile, "%i,%i\n", &iStatus, &iPriority) != 2) goto error;
	pUrlInfo->SetStatus((UrlInfo::EStatus)iStatus);
	pUrlInfo->SetPriority(iPriority);

	if (iFormatVersion >= 16)
	{
		int iAddTop, iAddPaused;
		if (fscanf(infile, "%i,%i\n", &iAddTop, &iAddPaused) != 2) goto error;
		pUrlInfo->SetAddTop(iAddTop);
		pUrlInfo->SetAddPaused(iAddPaused);
	}

	if (!fgets(buf, sizeof(buf), infile)) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	pUrlInfo->SetURL(buf);

	if (!fgets(buf, sizeof(buf), infile)) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	pUrlInfo->SetNZBFilename(buf);

	if (!fgets(buf, sizeof(buf), infile)) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	pUrlInfo->SetCategory(buf);

	if (iFormatVersion >= 31)
	{
		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		pUrlInfo->SetDupeKey(buf);

		int iNoDupeCheck, iDupeScore;
		if (fscanf(infile, "%i,%i\n", &iNoDupeCheck, &iDupeScore) != 2) goto error;
		pUrlInfo->SetNoDupeCheck(iNoDupeCheck);
		pUrlInfo->SetDupeScore(iDupeScore);
	}

	return true;

error:
	return false;
}

void DiskState::SaveDupInfo(DupInfo* pDupInfo, FILE* outfile)
{
	unsigned long High, Low;
	Util::SplitInt64(pDupInfo->GetSize(), &High, &Low);
	fprintf(outfile, "%i,%lu,%lu,%u,%u,%i\n", (int)pDupInfo->GetStatus(), High, Low,
		pDupInfo->GetFullContentHash(), pDupInfo->GetFilteredContentHash(), pDupInfo->GetDupeScore());
	fprintf(outfile, "%s\n", pDupInfo->GetName());
	fprintf(outfile, "%s\n", pDupInfo->GetDupeKey());
}

bool DiskState::LoadDupInfo(DupInfo* pDupInfo, FILE* infile, int iFormatVersion)
{
	char buf[1024];

	int iStatus;
	unsigned long High, Low;
	unsigned int iFullContentHash, iFilteredContentHash = 0;
	int iDupeScore;
	
	if (iFormatVersion >= 34)
	{
		if (fscanf(infile, "%i,%lu,%lu,%u,%i\n", &iStatus, &High, &Low, &iFullContentHash, &iFilteredContentHash, &iDupeScore) != 6) goto error;
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

	if (!fgets(buf, sizeof(buf), infile)) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	pDupInfo->SetName(buf);

	if (!fgets(buf, sizeof(buf), infile)) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	pDupInfo->SetDupeKey(buf);

	return true;

error:
	return false;
}

void DiskState::SaveHistory(DownloadQueue* pDownloadQueue, FILE* outfile)
{
	debug("Saving history to disk");

	fprintf(outfile, "%i\n", (int)pDownloadQueue->GetHistoryList()->size());
	for (HistoryList::iterator it = pDownloadQueue->GetHistoryList()->begin(); it != pDownloadQueue->GetHistoryList()->end(); it++)
	{
		HistoryInfo* pHistoryInfo = *it;

		fprintf(outfile, "%i,%i,%i\n", pHistoryInfo->GetID(), (int)pHistoryInfo->GetKind(), (int)pHistoryInfo->GetTime());

		if (pHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo)
		{
			int iNZBIndex = FindNZBInfoIndex(pDownloadQueue, pHistoryInfo->GetNZBInfo());
			fprintf(outfile, "%i\n", iNZBIndex);
		}
		else if (pHistoryInfo->GetKind() == HistoryInfo::hkUrlInfo)
		{
			SaveUrlInfo(pHistoryInfo->GetUrlInfo(), outfile);
		}
		else if (pHistoryInfo->GetKind() == HistoryInfo::hkDupInfo)
		{
			SaveDupInfo(pHistoryInfo->GetDupInfo(), outfile);
		}
	}
}

bool DiskState::LoadHistory(DownloadQueue* pDownloadQueue, FILE* infile, int iFormatVersion)
{
	debug("Loading history from disk");

	int size;
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		HistoryInfo* pHistoryInfo = NULL;
		HistoryInfo::EKind eKind = HistoryInfo::hkNZBInfo;
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

		if (eKind == HistoryInfo::hkNZBInfo)
		{
			unsigned int iNZBIndex;
			if (fscanf(infile, "%i\n", &iNZBIndex) != 1) goto error;
			NZBInfo* pNZBInfo = pDownloadQueue->GetNZBInfoList()->at(iNZBIndex - 1);
			pHistoryInfo = new HistoryInfo(pNZBInfo);
			
			if (iFormatVersion < 28 && pNZBInfo->GetParStatus() == 0 &&
				pNZBInfo->GetUnpackStatus() == 0 && pNZBInfo->GetMoveStatus() == 0)
			{
				pNZBInfo->SetDeleted(true);
			}
		}
		else if (eKind == HistoryInfo::hkUrlInfo)
		{
			UrlInfo* pUrlInfo = new UrlInfo();
			if (!LoadUrlInfo(pUrlInfo, infile, iFormatVersion)) goto error;
			pHistoryInfo = new HistoryInfo(pUrlInfo);
		}
		else if (eKind == HistoryInfo::hkDupInfo)
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

		pDownloadQueue->GetHistoryList()->push_back(pHistoryInfo);
	}

	return true;

error:
	error("Error reading diskstate for history");
	return false;
}

int DiskState::FindNZBInfoIndex(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	// find index of nzb-info
	int iNZBIndex = 0;
	for (unsigned int i = 0; i < pDownloadQueue->GetNZBInfoList()->size(); i++)
	{
		iNZBIndex++;
		if (pDownloadQueue->GetNZBInfoList()->at(i) == pNZBInfo)
		{
			break;
		}
	}
	return iNZBIndex;
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
	int* ids = (int*)malloc(sizeof(int) * (pDownloadQueue->GetFileQueue()->size() + 1));
	int* ptr = ids;
	for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		*ptr++ = pFileInfo->GetID();
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
void DiskState::CalcCriticalHealth(DownloadQueue* pDownloadQueue)
{
	std::set<NZBInfo*> oldNZBs;

	// build list of old NZBs which do not have critical health calculated
	for (NZBInfoList::iterator it = pDownloadQueue->GetNZBInfoList()->begin(); it != pDownloadQueue->GetNZBInfoList()->end(); it++)
	{
		NZBInfo* pNZBInfo = *it;
		if (pNZBInfo->CalcCriticalHealth() == 1000)
		{
			oldNZBs.insert(pNZBInfo);
		}
	}

	for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		NZBInfo* pNZBInfo = pFileInfo->GetNZBInfo();
		if (oldNZBs.find(pNZBInfo) != oldNZBs.end())
		{
			debug("Calculating critical health for %s", pNZBInfo->GetName());

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

bool DiskState::SaveStats(Servers* pServers)
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

	fprintf(outfile, "%s%i\n", FORMATVERSION_SIGNATURE, 1);

	// save status
	SaveServerStats(pServers, outfile);

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

bool DiskState::LoadStats(Servers* pServers)
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
	if (iFormatVersion > 1)
	{
		error("Could not load diskstate due to file version mismatch");
		fclose(infile);
		return false;
	}

	if (!LoadServerStats(pServers, infile, iFormatVersion)) goto error;

	bOK = true;

error:

	fclose(infile);
	if (!bOK)
	{
		error("Error reading diskstate for file %s", fileName);
	}

	return bOK;
}

bool DiskState::SaveServerStats(Servers* pServers, FILE* outfile)
{
	debug("Saving server stats to disk");

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

	// Step 2: matching host, port, username ans server-name
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

bool DiskState::LoadServerStats(Servers* pServers, FILE* infile, int iFormatVersion)
{
	debug("Loading server stats from disk");

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
	error("Error reading server stats from disk");

	for (ServerRefList::iterator it = serverRefs.begin(); it != serverRefs.end(); it++)
	{
		delete *it;
	}

	return false;
}
