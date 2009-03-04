/*
 *  This file is part of nzbget
 *
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
#include <config.h>
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>

#include "nzbget.h"
#include "DiskState.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"

extern Options* g_pOptions;

static const char* FORMATVERSION_SIGNATURE_V3 = "nzbget diskstate file version 3\n";
static const char* FORMATVERSION_SIGNATURE_V4 = "nzbget diskstate file version 4\n";
static const char* FORMATVERSION_SIGNATURE_V5 = "nzbget diskstate file version 5\n";
static const char* FORMATVERSION_SIGNATURE_V6 = "nzbget diskstate file version 6\n";
static const char* FORMATVERSION_SIGNATURE_V7 = "nzbget diskstate file version 7\n";

/* Parse signature and return format version number
*/
int DiskState::ParseFormatVersion(const char* szFormatSignature)
{
	if (!strcmp(szFormatSignature, FORMATVERSION_SIGNATURE_V3))
	{
		return 3;
	}
	else if (!strcmp(szFormatSignature, FORMATVERSION_SIGNATURE_V4))
	{
		return 4;
	}
	else if (!strcmp(szFormatSignature, FORMATVERSION_SIGNATURE_V5))
	{
		return 5;
	}
	else if (!strcmp(szFormatSignature, FORMATVERSION_SIGNATURE_V6))
	{
		return 6;
	}
	else if (!strcmp(szFormatSignature, FORMATVERSION_SIGNATURE_V7))
	{
		return 7;
	}
	else
	{
		return 0;
	}
}

/* Save Download Queue to Disk.
 * The Disk State consists of file "queue", which contains the order of files
 * and of one diskstate-file for each file in download queue.
 * This function saves file "queue" and files with NZB-info. It does not
 * save file-infos.
 */
bool DiskState::SaveDownloadQueue(DownloadQueue* pDownloadQueue)
{
	debug("Saving queue to disk");

	// prepare list of nzb-infos
	NZBQueue cNZBQueue;
	NZBInfo::BuildNZBList(pDownloadQueue, &cNZBQueue);

	char fileName[1024];
	snprintf(fileName, 1024, "%s%s", g_pOptions->GetQueueDir(), "queue");
	fileName[1024-1] = '\0';

	FILE* outfile = fopen(fileName, "w");

	if (!outfile)
	{
		error("Could not create file %s", fileName);
		perror(fileName);
		return false;
	}

	fprintf(outfile, FORMATVERSION_SIGNATURE_V6);

	// save nzb-infos
	fprintf(outfile, "%i\n", cNZBQueue.size());
	for (NZBQueue::iterator it = cNZBQueue.begin(); it != cNZBQueue.end(); it++)
	{
		NZBInfo* pNZBInfo = *it;
		fprintf(outfile, "%s\n", pNZBInfo->GetFilename());
		fprintf(outfile, "%s\n", pNZBInfo->GetDestDir());
		fprintf(outfile, "%s\n", pNZBInfo->GetQueuedFilename());
		fprintf(outfile, "%s\n", pNZBInfo->GetCategory());
		fprintf(outfile, "%i\n", pNZBInfo->GetPostProcess() ? 1 : 0);
		fprintf(outfile, "%i\n", pNZBInfo->GetFileCount());
		unsigned long High, Low;
		Util::SplitInt64(pNZBInfo->GetSize(), &High, &Low);
		fprintf(outfile, "%lu,%lu\n", High, Low);

		fprintf(outfile, "%i\n", pNZBInfo->GetCompletedFiles()->size());
		for (NZBInfo::Files::iterator it = pNZBInfo->GetCompletedFiles()->begin(); it != pNZBInfo->GetCompletedFiles()->end(); it++)
		{
			char* szFilename = *it;
			fprintf(outfile, "%s\n", szFilename);
		}

		fprintf(outfile, "%i\n", pNZBInfo->GetParameters()->size());
		for (NZBParameterList::iterator it = pNZBInfo->GetParameters()->begin(); it != pNZBInfo->GetParameters()->end(); it++)
		{
			NZBParameter* pParameter = *it;
			fprintf(outfile, "%s=%s\n", pParameter->GetName(), pParameter->GetValue());
		}
	}

	// save file-infos
	fprintf(outfile, "%i\n", pDownloadQueue->size());
	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (!pFileInfo->GetDeleted())
		{
			// find index of nzb-info
			int iNZBIndex = 0;
			for (unsigned int i = 0; i < cNZBQueue.size(); i++)
			{
				iNZBIndex++;
				if (cNZBQueue[i] == pFileInfo->GetNZBInfo())
				{
					break;
				}
			}

			fprintf(outfile, "%i,%i,%i\n", pFileInfo->GetID(), iNZBIndex, (int)pFileInfo->GetPaused());
		}
	}
	fclose(outfile);

	if (pDownloadQueue->empty())
	{
		remove(fileName);
	}

	return true;
}

bool DiskState::LoadDownloadQueue(DownloadQueue* pDownloadQueue)
{
	debug("Loading queue from disk");

	bool bOK = false;

	NZBQueue cNZBQueue;

	char fileName[1024];
	snprintf(fileName, 1024, "%s%s", g_pOptions->GetQueueDir(), "queue");
	fileName[1024-1] = '\0';

	FILE* infile = fopen(fileName, "r");

	if (!infile)
	{
		error("Could not open file %s", fileName);
		return false;
	}

	char FileSignatur[128];
	fgets(FileSignatur, sizeof(FileSignatur), infile);
	int iFormatVersion = ParseFormatVersion(FileSignatur);
	if (iFormatVersion < 3 || iFormatVersion > 6)
	{
		error("Could not load diskstate due file version mismatch");
		fclose(infile);
		return false;
	}

	int size;
	char buf[10240];

	// load nzb-infos
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		NZBInfo* pNZBInfo = new NZBInfo();
		pNZBInfo->AddReference();
		cNZBQueue.push_back(pNZBInfo);

		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		pNZBInfo->SetFilename(buf);

		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		pNZBInfo->SetDestDir(buf);

		if (iFormatVersion >= 5)
		{
			if (!fgets(buf, sizeof(buf), infile)) goto error;
			if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
			pNZBInfo->SetQueuedFilename(buf);
		}

		if (iFormatVersion >= 4)
		{
			if (!fgets(buf, sizeof(buf), infile)) goto error;
			if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
			pNZBInfo->SetCategory(buf);

			int iPostProcess;
			if (fscanf(infile, "%i\n", &iPostProcess) != 1) goto error;
			pNZBInfo->SetPostProcess(iPostProcess == 1);
		}

		int iFileCount;
		if (fscanf(infile, "%i\n", &iFileCount) != 1) goto error;
		pNZBInfo->SetFileCount(iFileCount);

		unsigned long High, Low;
		if (fscanf(infile, "%lu,%lu\n", &High, &Low) != 2) goto error;
		pNZBInfo->SetSize(Util::JoinInt64(High, Low));

		if (iFormatVersion >= 4)
		{
			int iFileCount;
			if (fscanf(infile, "%i\n", &iFileCount) != 1) goto error;
			for (int i = 0; i < iFileCount; i++)
			{
				if (!fgets(buf, sizeof(buf), infile)) goto error;
				if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
				pNZBInfo->GetCompletedFiles()->push_back(strdup(buf));
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
					pNZBInfo->SetParameter(buf, szValue);
				}
			}
		}
	}

	// load file-infos
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		unsigned int id, iNZBIndex, paused;
		if (fscanf(infile, "%i,%i,%i\n", &id, &iNZBIndex, &paused) != 3) goto error;
		if (iNZBIndex < 0 || iNZBIndex > cNZBQueue.size()) goto error;

		char fileName[1024];
		snprintf(fileName, 1024, "%s%i", g_pOptions->GetQueueDir(), id);
		fileName[1024-1] = '\0';
		FileInfo* pFileInfo = new FileInfo();
		bool res = LoadFileInfo(pFileInfo, fileName, true, false);
		if (res)
		{
			pFileInfo->SetID(id);
			pFileInfo->SetPaused(paused);
			pFileInfo->SetNZBInfo(cNZBQueue[iNZBIndex - 1]);
			pDownloadQueue->push_back(pFileInfo);
		}
		else
		{
			warn("Could not load diskstate for file %s", fileName);
			delete pFileInfo;
		}
	}

	bOK = true;

error:

	fclose(infile);
	if (!bOK)
	{
		error("Error reading diskstate for file %s", fileName);
	}

	for (NZBQueue::iterator it = cNZBQueue.begin(); it != cNZBQueue.end(); it++)
	{
		NZBInfo* pNZBInfo = *it;
		pNZBInfo->Release();
	}

	return bOK;
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

	FILE* outfile = fopen(szFilename, "w");

	if (!outfile)
	{
		error("Could not create file %s", szFilename);
		return false;
	}

	fprintf(outfile, "%s\n", pFileInfo->GetSubject());
	fprintf(outfile, "%s\n", pFileInfo->GetFilename());
	fprintf(outfile, "%i\n", pFileInfo->GetFilenameConfirmed());
	unsigned long High, Low;
	Util::SplitInt64(pFileInfo->GetSize(), &High, &Low);
	fprintf(outfile, "%lu,%lu\n", High, Low);

	fprintf(outfile, "%i\n", pFileInfo->GetGroups()->size());
	for (FileInfo::Groups::iterator it = pFileInfo->GetGroups()->begin(); it != pFileInfo->GetGroups()->end(); it++)
	{
		fprintf(outfile, "%s\n", *it);
	}

	fprintf(outfile, "%i\n", pFileInfo->GetArticles()->size());
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

	FILE* infile = fopen(szFilename, "r");

	if (!infile)
	{
		error("Could not open file %s", szFilename);
		return false;
	}

	char buf[1024];

	if (!fgets(buf, sizeof(buf), infile)) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	if (bFileSummary) pFileInfo->SetSubject(buf);

	if (!fgets(buf, sizeof(buf), infile)) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	if (bFileSummary) pFileInfo->SetFilename(buf);

	int iFilenameConfirmed;
	if (fscanf(infile, "%i\n", &iFilenameConfirmed) != 1) goto error;
	if (bFileSummary) pFileInfo->SetFilenameConfirmed(iFilenameConfirmed);
	
	unsigned long High, Low;
	if (fscanf(infile, "%lu,%lu\n", &High, &Low) != 2) goto error;
	if (bFileSummary) pFileInfo->SetSize(Util::JoinInt64(High, Low));
	if (bFileSummary) pFileInfo->SetRemainingSize(pFileInfo->GetSize());

	int size;
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		if (bFileSummary) pFileInfo->GetGroups()->push_back(strdup(buf));
	}

	if (bArticles)
	{
		if (fscanf(infile, "%i\n", &size) != 1) goto error;
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

bool DiskState::SavePostQueue(PostQueue* pPostQueue, bool bCompleted)
{
	debug("Saving post-queue to disk");

	char fileName[1024];
	snprintf(fileName, 1024, "%s%s", g_pOptions->GetQueueDir(), bCompleted ? "postc" : "postq");
	fileName[1024-1] = '\0';

	FILE* outfile = fopen(fileName, "w");

	if (!outfile)
	{
		error("Could not create file %s", fileName);
		perror(fileName);
		return false;
	}

	fprintf(outfile, FORMATVERSION_SIGNATURE_V7);

	fprintf(outfile, "%i\n", pPostQueue->size());
	for (PostQueue::iterator it = pPostQueue->begin(); it != pPostQueue->end(); it++)
	{
		PostInfo* pPostInfo = *it;
		fprintf(outfile, "%s\n", pPostInfo->GetNZBFilename());
		fprintf(outfile, "%s\n", pPostInfo->GetDestDir());
		fprintf(outfile, "%s\n", pPostInfo->GetParFilename());
		fprintf(outfile, "%s\n", pPostInfo->GetInfoName());
		fprintf(outfile, "%s\n", pPostInfo->GetCategory());
		fprintf(outfile, "%s\n", pPostInfo->GetQueuedFilename());
		fprintf(outfile, "%i\n", (int)pPostInfo->GetParCheck());
		fprintf(outfile, "%i\n", (int)pPostInfo->GetParStatus());
		fprintf(outfile, "%i\n", (int)pPostInfo->GetStage());

		fprintf(outfile, "%i\n", pPostInfo->GetParameters()->size());
		for (NZBParameterList::iterator it = pPostInfo->GetParameters()->begin(); it != pPostInfo->GetParameters()->end(); it++)
		{
			NZBParameter* pParameter = *it;
			fprintf(outfile, "%s=%s\n", pParameter->GetName(), pParameter->GetValue());
		}
	}
	fclose(outfile);

	if (pPostQueue->empty())
	{
		remove(fileName);
	}

	return true;
}

bool DiskState::LoadPostQueue(PostQueue* pPostQueue, bool bCompleted)
{
	debug("Loading post-queue from disk");

	char fileName[1024];
	snprintf(fileName, 1024, "%s%s", g_pOptions->GetQueueDir(), bCompleted ? "postc" : "postq");
	fileName[1024-1] = '\0';

	FILE* infile = fopen(fileName, "r");

	if (!infile)
	{
		error("Could not open file %s", fileName);
		return false;
	}

	char FileSignatur[128];
	fgets(FileSignatur, sizeof(FileSignatur), infile);
	int iFormatVersion = ParseFormatVersion(FileSignatur);
	if (iFormatVersion < 3 || iFormatVersion > 7)
	{
		error("Could not load diskstate due file version mismatch");
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
		pPostInfo->SetNZBFilename(buf);

		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		pPostInfo->SetDestDir(buf);

		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		pPostInfo->SetParFilename(buf);

		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		pPostInfo->SetInfoName(buf);

		if (iFormatVersion >= 4)
		{
			if (!fgets(buf, sizeof(buf), infile)) goto error;
			if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
			pPostInfo->SetCategory(buf);
		}
		else
		{
			pPostInfo->SetCategory("");
		}

		if (iFormatVersion >= 5)
		{
			if (!fgets(buf, sizeof(buf), infile)) goto error;
			if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
			pPostInfo->SetQueuedFilename(buf);
		}
		else
		{
			pPostInfo->SetQueuedFilename("");
		}

		if (fscanf(infile, "%i\n", &iIntValue) != 1) goto error;
		pPostInfo->SetParCheck(iIntValue);

		if (fscanf(infile, "%i\n", &iIntValue) != 1) goto error;
		pPostInfo->SetParStatus(iIntValue);

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
					pPostInfo->AddParameter(buf, szValue);
				}
			}
		}

		pPostQueue->push_back(pPostInfo);
	}

	fclose(infile);
	return true;

error:
	fclose(infile);
	error("Error reading diskstate for file %s", fileName);
	return false;
}

/*
 * Delete all files from Queue.
 * Returns true if successful, false if not
 */
bool DiskState::DiscardDownloadQueue()
{
	debug("Discarding queue");

	char fileName[1024];
	snprintf(fileName, 1024, "%s%s", g_pOptions->GetQueueDir(), "queue");
	fileName[1024-1] = '\0';

	FILE* infile = fopen(fileName, "r");

	if (!infile)
	{
		error("Could not open file %s", fileName);
		return false;
	}

	bool res = false;
	char FileSignatur[128];
	fgets(FileSignatur, sizeof(FileSignatur), infile);
	int iFormatVersion = ParseFormatVersion(FileSignatur);
	if (3 <= iFormatVersion && iFormatVersion <= 6)
	{
		// skip nzb-infos
		int size = 0;
		char buf[1024];
		fscanf(infile, "%i\n", &size);
		for (int i = 0; i < size; i++)
		{
			if (!fgets(buf, sizeof(buf), infile)) break; // filename
			if (!fgets(buf, sizeof(buf), infile)) break; // destdir
			if (iFormatVersion >= 4)
			{
				if (!fgets(buf, sizeof(buf), infile)) break; // category
				if (!fgets(buf, sizeof(buf), infile)) break; // postprocess
			}
			if (iFormatVersion >= 5)
			{
				if (!fgets(buf, sizeof(buf), infile)) break; // localfile
			}
			if (!fgets(buf, sizeof(buf), infile)) break; // file count
			if (!fgets(buf, sizeof(buf), infile)) break; // file size
			if (iFormatVersion >= 4)
			{
				// completed files
				int iFileCount;
				if (fscanf(infile, "%i\n", &iFileCount) != 1) break;
				for (int i = 0; i < iFileCount; i++)
				{
					if (!fgets(buf, sizeof(buf), infile)) break; // filename
				}
			}
			if (iFormatVersion >= 6)
			{
				// postprocess-parameters
				int iParameterCount;
				if (fscanf(infile, "%i\n", &iParameterCount) != 1) break;
				for (int i = 0; i < iParameterCount; i++)
				{
					if (!fgets(buf, sizeof(buf), infile)) break;
				}
			}
		}

		// file-infos
		fscanf(infile, "%i\n", &size);
		for (int i = 0; i < size; i++)
		{
			int id, group, paused;
			if (fscanf(infile, "%i,%i,%i\n", &id, &group, &paused) == 3)
			{
				char fileName[1024];
				snprintf(fileName, 1024, "%s%i", g_pOptions->GetQueueDir(), id);
				fileName[1024-1] = '\0';
				remove(fileName);
			}
		}
		res = true;
	}
	else
	{
		error("Could not discard diskstate due file version mismatch");
		res = false;
	}

	fclose(infile);
	if (res)
	{
		remove(fileName);
	}

	return res;
}

/*
 * Delete all files from Queue.
 * Returns true if successful, false if not
 */
bool DiskState::DiscardPostQueue()
{
	debug("Discarding post-queue");

	char fileName[1024];

	snprintf(fileName, 1024, "%s%s", g_pOptions->GetQueueDir(), "postq");
	fileName[1024-1] = '\0';
	remove(fileName);

	snprintf(fileName, 1024, "%s%s", g_pOptions->GetQueueDir(), "postc");
	fileName[1024-1] = '\0';
	remove(fileName);

	return true;
}

bool DiskState::DownloadQueueExists()
{
	debug("Checking if a saved queue exists on disk");

	char fileName[1024];
	snprintf(fileName, 1024, "%s%s", g_pOptions->GetQueueDir(), "queue");
	fileName[1024-1] = '\0';
	return Util::FileExists(fileName);
}

bool DiskState::PostQueueExists(bool bCompleted)
{
	debug("Checking if a saved queue exists on disk");

	char fileName[1024];
	snprintf(fileName, 1024, "%s%s", g_pOptions->GetQueueDir(), bCompleted ? "postc" : "postq");
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
	int* ids = (int*)malloc(sizeof(int) * (pDownloadQueue->size() + 1));
	int* ptr = ids;
	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
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
			((sscanf(filename, "%i.out", &id) == 1) &&
				!(g_pOptions->GetContinuePartial() && g_pOptions->GetDirectWrite()));
		if (!del)
		{
			if ((sscanf(filename, "%i.%i", &id, &part) == 2) ||
				(sscanf(filename, "%i.out", &id) == 1))
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
