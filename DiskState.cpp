/*
 *  This file if part of nzbget
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

#include "nzbget.h"
#include "DiskState.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"

extern Options* g_pOptions;

/* Save Download Queue to Disk.
 * The Disk State consists of file "queue", which contains the order of files
 * and of one diskstate-file for each file in download queue.
 * This function saves only file "queue".
 */
bool DiskState::Save(DownloadQueue* pDownloadQueue)
{
	debug("Saving queue to disk");

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

	fprintf(outfile, "nzbget diskstate file version 1\n");

	int cnt = 0;
	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (!pFileInfo->GetDeleted())
		{
			fprintf(outfile, "%i,%i\n", pFileInfo->GetID(), (int)pFileInfo->GetPaused());
			cnt++;
		}
	}
	fclose(outfile);

	if (cnt == 0)
	{
		remove(fileName);
	}

	return true;
}

bool DiskState::SaveFile(FileInfo* pFileInfo)
{
	char fileName[1024];
	snprintf(fileName, 1024, "%s%i", g_pOptions->GetQueueDir(), pFileInfo->GetID());
	fileName[1024-1] = '\0';
	return SaveFileInfo(pFileInfo, fileName);
}

bool DiskState::Load(DownloadQueue* pDownloadQueue)
{
	debug("Loading queue from disk");

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
	if (!strcmp(FileSignatur, "nzbget diskstate file version 1\n"))
	{
		int id, paused;
		while (fscanf(infile, "%i,%i\n", &id, &paused) != EOF)
		{
			char fileName[1024];
			snprintf(fileName, 1024, "%s%i", g_pOptions->GetQueueDir(), id);
			fileName[1024-1] = '\0';
			FileInfo* pFileInfo = new FileInfo();
			bool res = LoadFileInfo(pFileInfo, fileName, true, false);
			if (res)
			{
				pFileInfo->SetID(id);
				pFileInfo->SetPaused(paused);
				pDownloadQueue->push_back(pFileInfo);
			}
			else
			{
				warn("Could not load diskstate for file %s", fileName);
				delete pFileInfo;
			}
		}
		res = true;
	}
	else
	{
		error("Could not load diskstate due file version mismatch");
		res = false;
	}

	fclose(infile);

	return res;
}

bool DiskState::LoadArticles(FileInfo* pFileInfo)
{
	char fileName[1024];
	snprintf(fileName, 1024, "%s%i", g_pOptions->GetQueueDir(), pFileInfo->GetID());
	fileName[1024-1] = '\0';
	return LoadFileInfo(pFileInfo, fileName, false, true);
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

	fprintf(outfile, "%s\n", pFileInfo->GetNZBFilename());
	fprintf(outfile, "%s\n", pFileInfo->GetSubject());
	fprintf(outfile, "%s\n", pFileInfo->GetDestDir());
	fprintf(outfile, "%s\n", pFileInfo->GetFilename());
	fprintf(outfile, "%i\n", pFileInfo->GetFilenameConfirmed());

	fprintf(outfile, "%lu,%lu\n", (unsigned long)(pFileInfo->GetSize() >> 32), (unsigned long)(pFileInfo->GetSize()));

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
	if (bFileSummary) pFileInfo->SetNZBFilename(buf);

	if (!fgets(buf, sizeof(buf), infile)) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	if (bFileSummary) pFileInfo->SetSubject(buf);

	if (!fgets(buf, sizeof(buf), infile)) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	if (bFileSummary) pFileInfo->SetDestDir(buf);

	if (!fgets(buf, sizeof(buf), infile)) goto error;
	if (buf[0] != 0) buf[strlen(buf)-1] = 0; // remove traling '\n'
	if (bFileSummary) pFileInfo->SetFilename(buf);

	int iFilenameConfirmed;
	if (fscanf(infile, "%i\n", &iFilenameConfirmed) != 1) goto error;
	if (bFileSummary) pFileInfo->SetFilenameConfirmed(iFilenameConfirmed);
	
	unsigned long High, Low;
	if (fscanf(infile, "%lu,%lu\n", &High, &Low) != 2) goto error;
	if (bFileSummary) pFileInfo->SetSize((((unsigned long long)High) << 32) + Low);
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

/*
 * Delete all files from Queue.
 * Returns true if successful, false if not
 */
bool DiskState::Discard()
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
	if (!strcmp(FileSignatur, "nzbget diskstate file version 1\n"))
	{
		int id, paused;
		while (fscanf(infile, "%i,%i\n", &id, &paused) == 2)
		{
			char fileName[1024];
			snprintf(fileName, 1024, "%s%i", g_pOptions->GetQueueDir(), id);
			fileName[1024-1] = '\0';
			remove(fileName);
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

bool DiskState::Exists()
{
	debug("Checking if a saved queue exists on disk");

	char fileName[1024];
	snprintf(fileName, 1024, "%s%s", g_pOptions->GetQueueDir(), "queue");
	fileName[1024-1] = '\0';
	struct stat buffer;
	bool fileExists = !stat(fileName, &buffer);
	return fileExists;
}

bool DiskState::DiscardFile(DownloadQueue* pDownloadQueue, FileInfo* pFileInfo)
{
	// delete diskstate-file
	char fileName[1024];
	snprintf(fileName, 1024, "%s%i", g_pOptions->GetQueueDir(), pFileInfo->GetID());
	fileName[1024-1] = '\0';
	remove(fileName);

	return Save(pDownloadQueue);
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
		bool del = strstr(filename, ".tmp") || strstr(filename, ".dec");
		if (!del)
		{
			int id, part;
			if (sscanf(filename, "%i.%i", &id, &part) == 2)
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
