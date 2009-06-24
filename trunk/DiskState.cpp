/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2009 Andrei Prygounkov <hugbug@users.sourceforge.net>
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

static const char* FORMATVERSION_SIGNATURE = "nzbget diskstate file version ";

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
 */
bool DiskState::SaveDownloadQueue(DownloadQueue* pDownloadQueue)
{
	debug("Saving queue to disk");

	char fileName[1024];
	snprintf(fileName, 1024, "%s%s", g_pOptions->GetQueueDir(), "queue");
	fileName[1024-1] = '\0';

	FILE* outfile = fopen(fileName, "wb");

	if (!outfile)
	{
		error("Could not create file %s", fileName);
		perror(fileName);
		return false;
	}

	fprintf(outfile, "%s%i\n", FORMATVERSION_SIGNATURE, 8);

	// save nzb-infos
	SaveNZBList(pDownloadQueue, outfile);

	// save file-infos
	SaveFileQueue(pDownloadQueue, outfile);

	// save post-queue
	SavePostQueue(pDownloadQueue, outfile);

	fclose(outfile);

	if (pDownloadQueue->GetFileQueue()->empty() && 
		pDownloadQueue->GetPostQueue()->empty())
	{
		remove(fileName);
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
		error("Could not open file %s", fileName);
		return false;
	}

	char FileSignatur[128];
	fgets(FileSignatur, sizeof(FileSignatur), infile);
	int iFormatVersion = ParseFormatVersion(FileSignatur);
	if (iFormatVersion < 3 || iFormatVersion > 8)
	{
		error("Could not load diskstate due file version mismatch");
		fclose(infile);
		return false;
	}

	// load nzb-infos
	if (!LoadNZBList(pDownloadQueue, infile, iFormatVersion)) goto error;

	// load file-infos
	if (!LoadFileQueue(pDownloadQueue, infile)) goto error;

	if (iFormatVersion >= 7 && g_pOptions->GetReloadPostQueue())
	{
		// load post-queue
		if (!LoadPostQueue(pDownloadQueue, infile)) goto error;
	}
	else if (iFormatVersion < 7 && g_pOptions->GetReloadPostQueue())
	{
		// load post-queue created with older version of program
		LoadOldPostQueue(pDownloadQueue);
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

	fprintf(outfile, "%i\n", pDownloadQueue->GetNZBInfoList()->size());
	for (NZBInfoList::iterator it = pDownloadQueue->GetNZBInfoList()->begin(); it != pDownloadQueue->GetNZBInfoList()->end(); it++)
	{
		NZBInfo* pNZBInfo = *it;
		fprintf(outfile, "%s\n", pNZBInfo->GetFilename());
		fprintf(outfile, "%s\n", pNZBInfo->GetDestDir());
		fprintf(outfile, "%s\n", pNZBInfo->GetQueuedFilename());
		fprintf(outfile, "%s\n", pNZBInfo->GetCategory());
		fprintf(outfile, "%i\n", pNZBInfo->GetPostProcess() ? 1 : 0);
		fprintf(outfile, "%i\n", (int)pNZBInfo->GetParFailure());
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
		pNZBInfo->AddReference();
		pDownloadQueue->GetNZBInfoList()->Add(pNZBInfo);

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

		if (iFormatVersion >= 8)
		{
			int iParFailure;
			if (fscanf(infile, "%i\n", &iParFailure) != 1) goto error;
			pNZBInfo->SetParFailure((NZBInfo::EParFailure)iParFailure);
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

	return true;

error:
	error("Error reading nzb list from disk");
	return false;
}

void DiskState::SaveFileQueue(DownloadQueue* pDownloadQueue, FILE* outfile)
{
	debug("Saving file queue to disk");

	// save file-infos
	fprintf(outfile, "%i\n", pDownloadQueue->GetFileQueue()->size());
	for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (!pFileInfo->GetDeleted())
		{
			int iNZBIndex = FindNZBInfoIndex(pDownloadQueue, pFileInfo->GetNZBInfo());
			fprintf(outfile, "%i,%i,%i\n", pFileInfo->GetID(), iNZBIndex, (int)pFileInfo->GetPaused());
		}
	}
}

bool DiskState::LoadFileQueue(DownloadQueue* pDownloadQueue, FILE* infile)
{
	debug("Loading file queue from disk");

	int size;
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		unsigned int id, iNZBIndex, paused;
		if (fscanf(infile, "%i,%i,%i\n", &id, &iNZBIndex, &paused) != 3) goto error;
		if (iNZBIndex < 0 || iNZBIndex > pDownloadQueue->GetNZBInfoList()->size()) goto error;

		char fileName[1024];
		snprintf(fileName, 1024, "%s%i", g_pOptions->GetQueueDir(), id);
		fileName[1024-1] = '\0';
		FileInfo* pFileInfo = new FileInfo();
		bool res = LoadFileInfo(pFileInfo, fileName, true, false);
		if (res)
		{
			pFileInfo->SetID(id);
			pFileInfo->SetPaused(paused);
			pFileInfo->SetNZBInfo(pDownloadQueue->GetNZBInfoList()->at(iNZBIndex - 1));
			pDownloadQueue->GetFileQueue()->push_back(pFileInfo);
		}
		else
		{
			warn("Could not load diskstate for file %s", fileName);
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

	FILE* infile = fopen(szFilename, "rb");

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

void DiskState::SavePostQueue(DownloadQueue* pDownloadQueue, FILE* outfile)
{
	debug("Saving post-queue to disk");

	fprintf(outfile, "%i\n", pDownloadQueue->GetPostQueue()->size());
	for (PostQueue::iterator it = pDownloadQueue->GetPostQueue()->begin(); it != pDownloadQueue->GetPostQueue()->end(); it++)
	{
		PostInfo* pPostInfo = *it;
		int iNZBIndex = FindNZBInfoIndex(pDownloadQueue, pPostInfo->GetNZBInfo());
		fprintf(outfile, "%i,%i,%i,%i\n", iNZBIndex, (int)pPostInfo->GetParCheck(), 
			(int)pPostInfo->GetParStatus(), (int)pPostInfo->GetStage());
		fprintf(outfile, "%s\n", pPostInfo->GetInfoName());
		fprintf(outfile, "%s\n", pPostInfo->GetParFilename());
	}
}

bool DiskState::LoadPostQueue(DownloadQueue* pDownloadQueue, FILE* infile)
{
	debug("Loading post-queue from disk");

	int size;
	char buf[10240];

	// load file-infos
	if (fscanf(infile, "%i\n", &size) != 1) goto error;
	for (int i = 0; i < size; i++)
	{
		PostInfo* pPostInfo = new PostInfo();

		unsigned int iNZBIndex, iParCheck, iParStatus, iStage;
		if (fscanf(infile, "%i,%i,%i,%i\n", &iNZBIndex, &iParCheck, &iParStatus, &iStage) != 4) goto error;
		pPostInfo->SetNZBInfo(pDownloadQueue->GetNZBInfoList()->at(iNZBIndex - 1));
		pPostInfo->SetParCheck(iParCheck);
		pPostInfo->SetParStatus(iParStatus);
		pPostInfo->SetStage((PostInfo::EStage)iStage);

		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		pPostInfo->SetInfoName(buf);

		if (!fgets(buf, sizeof(buf), infile)) goto error;
		if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
		pPostInfo->SetParFilename(buf);

		pDownloadQueue->GetPostQueue()->push_back(pPostInfo);
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
			pNZBInfo->AddReference();
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
					if (bNewNZBInfo)
					{
						pNZBInfo->SetParameter(buf, szValue);
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
 * Delete all files from Queue.
 * Returns true if successful, false if not
 */
bool DiskState::DiscardDownloadQueue()
{
	debug("Discarding queue");

	char fileName[1024];
	snprintf(fileName, 1024, "%s%s", g_pOptions->GetQueueDir(), "queue");
	fileName[1024-1] = '\0';

	FILE* infile = fopen(fileName, "rb");

	if (!infile)
	{
		error("Could not open file %s", fileName);
		return false;
	}

	bool res = false;
	char FileSignatur[128];
	fgets(FileSignatur, sizeof(FileSignatur), infile);
	int iFormatVersion = ParseFormatVersion(FileSignatur);
	if (3 <= iFormatVersion && iFormatVersion <= 8)
	{
		// skip nzb-infos
		int size = 0;
		char buf[1024];
		fscanf(infile, "%i\n", &size);
		for (int i = 0; i < size; i++)
		{
			if (!fgets(buf, sizeof(buf), infile)) break; // filename
			if (!fgets(buf, sizeof(buf), infile)) break; // destdir
			if (iFormatVersion >= 5)
			{
				if (!fgets(buf, sizeof(buf), infile)) break; // localfile
			}
			if (iFormatVersion >= 4)
			{
				if (!fgets(buf, sizeof(buf), infile)) break; // category
				if (!fgets(buf, sizeof(buf), infile)) break; // postprocess
			}
			if (iFormatVersion >= 8)
			{
				if (!fgets(buf, sizeof(buf), infile)) break; // ParFailure
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
