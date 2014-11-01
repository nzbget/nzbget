/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#ifdef WIN32
#include <direct.h>
#else
#include <unistd.h>
#include <sys/time.h>
#endif
#include <sys/stat.h>
#include <errno.h>
#include <algorithm>

#include "nzbget.h"
#include "ArticleWriter.h"
#include "DiskState.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"

extern Options* g_pOptions;
extern DiskState* g_pDiskState;
extern ArticleCache* g_pArticleCache;


ArticleWriter::ArticleWriter()
{
	debug("Creating ArticleWriter");

	m_szTempFilename = NULL;
	m_szOutputFilename = NULL;
	m_szResultFilename = NULL;
	m_szInfoName = NULL;
	m_eFormat = Decoder::efUnknown;
	m_pArticleData = NULL;
	m_bDuplicate = false;
	m_bFlushing = false;
}

ArticleWriter::~ArticleWriter()
{
	debug("Destroying ArticleWriter");

	free(m_szOutputFilename);
	free(m_szTempFilename);
	free(m_szInfoName);

	if (m_pArticleData)
	{
		free(m_pArticleData);
		g_pArticleCache->Free(m_iArticleSize);
	}

	if (m_bFlushing)
	{
		g_pArticleCache->UnlockFlush();
	}
}

void ArticleWriter::SetInfoName(const char* szInfoName)
{
	m_szInfoName = strdup(szInfoName);
}

void ArticleWriter::SetWriteBuffer(FILE* pOutFile, int iRecSize)
{
	if (g_pOptions->GetWriteBuffer() > 0)
	{
		setvbuf(pOutFile, NULL, _IOFBF,
			iRecSize > 0 && iRecSize < g_pOptions->GetWriteBuffer() * 1024 ?
			iRecSize : g_pOptions->GetWriteBuffer() * 1024);
	}
}

void ArticleWriter::Prepare()
{
	BuildOutputFilename();
	m_szResultFilename = m_pArticleInfo->GetResultFilename();
}

bool ArticleWriter::Start(Decoder::EFormat eFormat, const char* szFilename, long long iFileSize,
	long long iArticleOffset, int iArticleSize)
{
	char szErrBuf[256];
	m_pOutFile = NULL;
	m_eFormat = eFormat;
	m_iArticleOffset = iArticleOffset;
	m_iArticleSize = iArticleSize ? iArticleSize : m_pArticleInfo->GetSize();
	m_iArticlePtr = 0;

	// prepare file for writing
	if (m_eFormat == Decoder::efYenc)
	{
		if (g_pOptions->GetDupeCheck() &&
			m_pFileInfo->GetNZBInfo()->GetDupeMode() != dmForce &&
			!m_pFileInfo->GetNZBInfo()->GetManyDupeFiles())
		{
			m_pFileInfo->LockOutputFile();
			bool bOutputInitialized = m_pFileInfo->GetOutputInitialized();
			if (!g_pOptions->GetDirectWrite())
			{
				m_pFileInfo->SetOutputInitialized(true);
			}
			m_pFileInfo->UnlockOutputFile();
			if (!bOutputInitialized && szFilename &&
				Util::FileExists(m_pFileInfo->GetNZBInfo()->GetDestDir(), szFilename))
			{
				m_bDuplicate = true;
				return false;
			}
		}

		if (g_pOptions->GetDirectWrite())
		{
			m_pFileInfo->LockOutputFile();
			if (!m_pFileInfo->GetOutputInitialized())
			{
				if (!CreateOutputFile(iFileSize))
				{
					m_pFileInfo->UnlockOutputFile();
					return false;
				}
				m_pFileInfo->SetOutputInitialized(true);
			}
			m_pFileInfo->UnlockOutputFile();
		}
	}

	// allocate cache buffer
	if (g_pOptions->GetArticleCache() > 0 && g_pOptions->GetDecode() &&
		(!g_pOptions->GetDirectWrite() || m_eFormat == Decoder::efYenc))
	{
		if (m_pArticleData)
		{
			free(m_pArticleData);
			g_pArticleCache->Free(m_iArticleSize);
		}

		m_pArticleData = (char*)g_pArticleCache->Alloc(m_iArticleSize);

		while (!m_pArticleData && g_pArticleCache->GetFlushing())
		{
			usleep(5 * 1000);
			m_pArticleData = (char*)g_pArticleCache->Alloc(m_iArticleSize);
		}

		if (!m_pArticleData)
		{
			detail("Article cache is full, using disk for %s", m_szInfoName);
		}
	}

	if (!m_pArticleData)
	{
		bool bDirectWrite = g_pOptions->GetDirectWrite() && m_eFormat == Decoder::efYenc;
		const char* szFilename = bDirectWrite ? m_szOutputFilename : m_szTempFilename;
		m_pOutFile = fopen(szFilename, bDirectWrite ? FOPEN_RBP : FOPEN_WB);
		if (!m_pOutFile)
		{
			error("Could not %s file %s: %s", bDirectWrite ? "open" : "create", szFilename, Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
			return false;
		}
		SetWriteBuffer(m_pOutFile, m_pArticleInfo->GetSize());

		if (g_pOptions->GetDirectWrite() && m_eFormat == Decoder::efYenc)
		{
			fseek(m_pOutFile, m_iArticleOffset, SEEK_SET);
		}
	}

	return true;
}

bool ArticleWriter::Write(char* szBufffer, int iLen)
{
	if (g_pOptions->GetDecode())
	{
		m_iArticlePtr += iLen;
	}

	if (g_pOptions->GetDecode() && m_pArticleData)
	{
		if (m_iArticlePtr > m_iArticleSize)
		{
			detail("Decoding %s failed: article size mismatch", m_szInfoName);
			return false;
		}
		memcpy(m_pArticleData + m_iArticlePtr - iLen, szBufffer, iLen);
		return true;
	}

	return fwrite(szBufffer, 1, iLen, m_pOutFile) > 0;
}

void ArticleWriter::Finish(bool bSuccess)
{
	char szErrBuf[256];

	if (m_pOutFile)
	{
		fclose(m_pOutFile);
		m_pOutFile = NULL;
	}

	if (!bSuccess)
	{
		remove(m_szTempFilename);
		remove(m_szResultFilename);
		return;
	}

	bool bDirectWrite = g_pOptions->GetDirectWrite() && m_eFormat == Decoder::efYenc;

	if (g_pOptions->GetDecode())
	{
		if (!bDirectWrite && !m_pArticleData)
		{
			if (!Util::MoveFile(m_szTempFilename, m_szResultFilename))
			{
				error("Could not rename file %s to %s: %s", m_szTempFilename, m_szResultFilename, Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
			}
		}

		remove(m_szTempFilename);

		if (m_pArticleData)
		{
			if (m_iArticleSize != m_iArticlePtr)
			{
				m_pArticleData = (char*)g_pArticleCache->Realloc(m_pArticleData, m_iArticleSize, m_iArticlePtr);
			}
			g_pArticleCache->LockContent();
			m_pArticleInfo->AttachSegment(m_pArticleData, m_iArticleOffset, m_iArticlePtr);
			m_pFileInfo->SetCachedArticles(m_pFileInfo->GetCachedArticles() + 1);
			g_pArticleCache->UnlockContent();
			m_pArticleData = NULL;
		}
		else
		{
			m_pArticleInfo->SetSegmentOffset(m_iArticleOffset);
			m_pArticleInfo->SetSegmentSize(m_iArticlePtr);
		}
	}
	else 
	{
		// rawmode
		if (!Util::MoveFile(m_szTempFilename, m_szResultFilename))
		{
			error("Could not move file %s to %s: %s", m_szTempFilename, m_szResultFilename, Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
		}
	}
}

/* creates output file and subdirectores */
bool ArticleWriter::CreateOutputFile(long long iSize)
{
	if (g_pOptions->GetDirectWrite() && Util::FileExists(m_szOutputFilename) &&
		Util::FileSize(m_szOutputFilename) == iSize)
	{
		// keep existing old file from previous program session
		return true;
	}
		
	// delete eventually existing old file from previous program session
	remove(m_szOutputFilename);

	// ensure the directory exist
	char szDestDir[1024];
	int iMaxlen = Util::BaseFileName(m_szOutputFilename) - m_szOutputFilename;
	if (iMaxlen > 1024-1) iMaxlen = 1024-1;
	strncpy(szDestDir, m_szOutputFilename, iMaxlen);
	szDestDir[iMaxlen] = '\0';
	char szErrBuf[1024];

	if (!Util::ForceDirectories(szDestDir, szErrBuf, sizeof(szErrBuf)))
	{
		error("Could not create directory %s: %s", szDestDir, szErrBuf);
		return false;
	}

	if (!Util::CreateSparseFile(m_szOutputFilename, iSize))
	{
		error("Could not create file %s", m_szOutputFilename);
		return false;
	}

	return true;
}

void ArticleWriter::BuildOutputFilename()
{
	char szFilename[1024];

	snprintf(szFilename, 1024, "%s%i.%03i", g_pOptions->GetTempDir(), m_pFileInfo->GetID(), m_pArticleInfo->GetPartNumber());
	szFilename[1024-1] = '\0';
	m_pArticleInfo->SetResultFilename(szFilename);

	char tmpname[1024];
	snprintf(tmpname, 1024, "%s.tmp", szFilename);
	tmpname[1024-1] = '\0';
	m_szTempFilename = strdup(tmpname);

	if (g_pOptions->GetDirectWrite())
	{
		m_pFileInfo->LockOutputFile();

		if (m_pFileInfo->GetOutputFilename())
		{
			strncpy(szFilename, m_pFileInfo->GetOutputFilename(), 1024);
			szFilename[1024-1] = '\0';
		}
		else
		{
			snprintf(szFilename, 1024, "%s%c%i.out.tmp", m_pFileInfo->GetNZBInfo()->GetDestDir(), (int)PATH_SEPARATOR, m_pFileInfo->GetID());
			szFilename[1024-1] = '\0';
			m_pFileInfo->SetOutputFilename(szFilename);
		}

		m_pFileInfo->UnlockOutputFile();

		m_szOutputFilename = strdup(szFilename);
	}
}

void ArticleWriter::CompleteFileParts()
{
	debug("Completing file parts");
	debug("ArticleFilename: %s", m_pFileInfo->GetFilename());

	bool bDirectWrite = g_pOptions->GetDirectWrite() && m_pFileInfo->GetOutputInitialized();
	char szErrBuf[256];

	char szNZBName[1024];
	char szNZBDestDir[1024];
	// the locking is needed for accessing the members of NZBInfo
	DownloadQueue::Lock();
	strncpy(szNZBName, m_pFileInfo->GetNZBInfo()->GetName(), 1024);
	strncpy(szNZBDestDir, m_pFileInfo->GetNZBInfo()->GetDestDir(), 1024);
	DownloadQueue::Unlock();
	szNZBName[1024-1] = '\0';
	szNZBDestDir[1024-1] = '\0';
	
	char szInfoFilename[1024];
	snprintf(szInfoFilename, 1024, "%s%c%s", szNZBName, (int)PATH_SEPARATOR, m_pFileInfo->GetFilename());
	szInfoFilename[1024-1] = '\0';

	bool bCached = m_pFileInfo->GetCachedArticles() > 0;

	if (!g_pOptions->GetDecode())
	{
		detail("Moving articles for %s", szInfoFilename);
	}
	else if (bDirectWrite && bCached)
	{
		detail("Writing articles for %s", szInfoFilename);
	}
	else if (bDirectWrite)
	{
		detail("Checking articles for %s", szInfoFilename);
	}
	else
	{
		detail("Joining articles for %s", szInfoFilename);
	}

	// Ensure the DstDir is created
	if (!Util::ForceDirectories(szNZBDestDir, szErrBuf, sizeof(szErrBuf)))
	{
		error("Could not create directory %s: %s", szNZBDestDir, szErrBuf);
		return;
	}

	char ofn[1024];
	Util::MakeUniqueFilename(ofn, 1024, szNZBDestDir, m_pFileInfo->GetFilename());

	FILE* outfile = NULL;
	char tmpdestfile[1024];
	snprintf(tmpdestfile, 1024, "%s.tmp", ofn);
	tmpdestfile[1024-1] = '\0';

	if (g_pOptions->GetDecode() && !bDirectWrite)
	{
		remove(tmpdestfile);
		outfile = fopen(tmpdestfile, FOPEN_WBP);
		if (!outfile)
		{
			error("Could not create file %s: %s", tmpdestfile, Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
			return;
		}
	}
	else if (bDirectWrite && bCached)
	{
		outfile = fopen(m_szOutputFilename, FOPEN_RBP);
		if (!outfile)
		{
			error("Could not open file %s: %s", m_szOutputFilename, Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
			return;
		}
		strncpy(tmpdestfile, m_szOutputFilename, 1024);
		tmpdestfile[1024-1] = '\0';
	}
	else if (!g_pOptions->GetDecode())
	{
		remove(tmpdestfile);
		if (!Util::CreateDirectory(ofn))
		{
			error("Could not create directory %s: %s", ofn, Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
			return;
		}
	}

	if (outfile)
	{
		SetWriteBuffer(outfile, 0);
	}

	if (bCached)
	{
		g_pArticleCache->LockFlush();
		m_bFlushing = true;
	}

	static const int BUFFER_SIZE = 1024 * 64;
	char* buffer = NULL;
	bool bFirstArticle = true;
	unsigned long lCrc = 0;

	if (g_pOptions->GetDecode() && !bDirectWrite)
	{
		buffer = (char*)malloc(BUFFER_SIZE);
	}

	for (FileInfo::Articles::iterator it = m_pFileInfo->GetArticles()->begin(); it != m_pFileInfo->GetArticles()->end(); it++)
	{
		ArticleInfo* pa = *it;
		if (pa->GetStatus() != ArticleInfo::aiFinished)
		{
			continue;
		}

		if (g_pOptions->GetDecode() && !bDirectWrite && pa->GetSegmentOffset() > -1 &&
			pa->GetSegmentOffset() > ftell(outfile) && ftell(outfile) > -1)
		{
			memset(buffer, 0, BUFFER_SIZE);
			while (pa->GetSegmentOffset() > ftell(outfile) && ftell(outfile) > -1 &&
				fwrite(buffer, 1, (std::min)((int)(pa->GetSegmentOffset() - ftell(outfile)), BUFFER_SIZE), outfile)) ;
		}

		if (pa->GetSegmentContent())
		{
			fseek(outfile, pa->GetSegmentOffset(), SEEK_SET);
			fwrite(pa->GetSegmentContent(), 1, pa->GetSegmentSize(), outfile);
			pa->DiscardSegment();
			SetLastUpdateTimeNow();
		}
		else if (g_pOptions->GetDecode() && !bDirectWrite)
		{
			FILE* infile = pa->GetResultFilename() ? fopen(pa->GetResultFilename(), FOPEN_RB) : NULL;
			if (infile)
			{
				int cnt = BUFFER_SIZE;
				while (cnt == BUFFER_SIZE)
				{
					cnt = (int)fread(buffer, 1, BUFFER_SIZE, infile);
					fwrite(buffer, 1, cnt, outfile);
					SetLastUpdateTimeNow();
				}
				fclose(infile);
			}
			else
			{
				m_pFileInfo->SetFailedArticles(m_pFileInfo->GetFailedArticles() + 1);
				m_pFileInfo->SetSuccessArticles(m_pFileInfo->GetSuccessArticles() - 1);
				error("Could not find file %s for %s%c%s [%i/%i]",
					pa->GetResultFilename(), szNZBName, (int)PATH_SEPARATOR, m_pFileInfo->GetFilename(),
					pa->GetPartNumber(), (int)m_pFileInfo->GetArticles()->size());
			}
		}
		else if (!g_pOptions->GetDecode())
		{
			char dstFileName[1024];
			snprintf(dstFileName, 1024, "%s%c%03i", ofn, (int)PATH_SEPARATOR, pa->GetPartNumber());
			dstFileName[1024-1] = '\0';
			if (!Util::MoveFile(pa->GetResultFilename(), dstFileName))
			{
				error("Could not move file %s to %s: %s", pa->GetResultFilename(), dstFileName, Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
			}
		}

		if (m_eFormat == Decoder::efYenc)
		{
			lCrc = bFirstArticle ? pa->GetCrc() : Util::Crc32Combine(lCrc, pa->GetCrc(), pa->GetSegmentSize());
			bFirstArticle = false;
		}
	}

	free(buffer);

	if (bCached)
	{
		g_pArticleCache->UnlockFlush();
		m_bFlushing = false;
	}

	if (outfile)
	{
		fclose(outfile);
		if (!bDirectWrite && !Util::MoveFile(tmpdestfile, ofn))
		{
			error("Could not move file %s to %s: %s", tmpdestfile, ofn, Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
		}
	}

	if (bDirectWrite)
	{
		if (!Util::MoveFile(m_szOutputFilename, ofn))
		{
			error("Could not move file %s to %s: %s", m_szOutputFilename, ofn, Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
		}

		// if destination directory was changed delete the old directory (if empty)
		int iLen = strlen(szNZBDestDir);
		if (!(!strncmp(szNZBDestDir, m_szOutputFilename, iLen) && 
			(m_szOutputFilename[iLen] == PATH_SEPARATOR || m_szOutputFilename[iLen] == ALT_PATH_SEPARATOR)))
		{
			debug("Checking old dir for: %s", m_szOutputFilename);
			char szOldDestDir[1024];
			int iMaxlen = Util::BaseFileName(m_szOutputFilename) - m_szOutputFilename;
			if (iMaxlen > 1024-1) iMaxlen = 1024-1;
			strncpy(szOldDestDir, m_szOutputFilename, iMaxlen);
			szOldDestDir[iMaxlen] = '\0';
			if (Util::DirEmpty(szOldDestDir))
			{
				debug("Deleting old dir: %s", szOldDestDir);
				rmdir(szOldDestDir);
			}
		}
	}

	if (!bDirectWrite)
	{
		for (FileInfo::Articles::iterator it = m_pFileInfo->GetArticles()->begin(); it != m_pFileInfo->GetArticles()->end(); it++)
		{
			ArticleInfo* pa = *it;
			remove(pa->GetResultFilename());
		}
	}

	if (m_pFileInfo->GetMissedArticles() == 0 && m_pFileInfo->GetFailedArticles() == 0)
	{
		info("Successfully downloaded %s", szInfoFilename);
	}
	else
	{
		warn("%i of %i article downloads failed for \"%s\"", m_pFileInfo->GetMissedArticles() + m_pFileInfo->GetFailedArticles(),
			m_pFileInfo->GetTotalArticles(), szInfoFilename);

		if (g_pOptions->GetCreateBrokenLog())
		{
			char szBrokenLogName[1024];
			snprintf(szBrokenLogName, 1024, "%s%c_brokenlog.txt", szNZBDestDir, (int)PATH_SEPARATOR);
			szBrokenLogName[1024-1] = '\0';
			FILE* file = fopen(szBrokenLogName, FOPEN_AB);
			fprintf(file, "%s (%i/%i)%s", m_pFileInfo->GetFilename(), m_pFileInfo->GetSuccessArticles(),
				m_pFileInfo->GetTotalArticles(), LINE_ENDING);
			fclose(file);
		}

		lCrc = 0;

		if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
		{
			g_pDiskState->DiscardFile(m_pFileInfo, false, true, false);
			g_pDiskState->SaveFileState(m_pFileInfo, true);
		}
	}

	CompletedFile::EStatus eFileStatus = m_pFileInfo->GetMissedArticles() == 0 &&
		m_pFileInfo->GetFailedArticles() == 0 ? CompletedFile::cfSuccess :
		m_pFileInfo->GetSuccessArticles() > 0 ? CompletedFile::cfPartial :
		CompletedFile::cfFailure;

	// the locking is needed for accessing the members of NZBInfo
	DownloadQueue::Lock();
	m_pFileInfo->GetNZBInfo()->GetCompletedFiles()->push_back(new CompletedFile(
		m_pFileInfo->GetID(), Util::BaseFileName(ofn), eFileStatus, lCrc));
	if (strcmp(m_pFileInfo->GetNZBInfo()->GetDestDir(), szNZBDestDir))
	{
		// destination directory was changed during completion, need to move the file
		MoveCompletedFiles(m_pFileInfo->GetNZBInfo(), szNZBDestDir);
	}
	DownloadQueue::Unlock();
}

void ArticleWriter::FlushCache()
{
	char szInfoFilename[1024];
	// the locking is needed for accessing the members of NZBInfo
	DownloadQueue::Lock();
	snprintf(szInfoFilename, 1024, "%s%c%s", m_pFileInfo->GetNZBInfo()->GetName(), (int)PATH_SEPARATOR, m_pFileInfo->GetFilename());
	szInfoFilename[1024-1] = '\0';
	DownloadQueue::Unlock();

	detail("Flushing cache for %s", szInfoFilename);

	bool bDirectWrite = g_pOptions->GetDirectWrite() && m_pFileInfo->GetOutputInitialized();
	FILE* outfile = NULL;
	bool bNeedBufFile = false;
	char szDestFile[1024];
	char szErrBuf[256];
	int iFlushedArticles = 0;
	long long iFlushedSize = 0;

	g_pArticleCache->LockFlush();

	FileInfo::Articles cachedArticles;
	cachedArticles.reserve(m_pFileInfo->GetArticles()->size());

	g_pArticleCache->LockContent();
	for (FileInfo::Articles::iterator it = m_pFileInfo->GetArticles()->begin(); it != m_pFileInfo->GetArticles()->end(); it++)
	{
		ArticleInfo* pa = *it;
		if (pa->GetSegmentContent())
		{
			cachedArticles.push_back(pa);
		}
	}
	g_pArticleCache->UnlockContent();

	for (FileInfo::Articles::iterator it = cachedArticles.begin(); it != cachedArticles.end(); it++)
	{
		if (m_pFileInfo->GetDeleted())
		{
			// the file was deleted during flushing: stop flushing immediately
			break;
		}

		ArticleInfo* pa = *it;

		if (bDirectWrite && !outfile)
		{
			outfile = fopen(m_pFileInfo->GetOutputFilename(), FOPEN_RBP);
			if (!outfile)
			{
				error("Could not open file %s: %s", m_pFileInfo->GetOutputFilename(), Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
				break;
			}
			bNeedBufFile = true;
		}

		if (!bDirectWrite)
		{
			snprintf(szDestFile, 1024, "%s.tmp", pa->GetResultFilename());
			szDestFile[1024-1] = '\0';

			outfile = fopen(szDestFile, FOPEN_WB);
			if (!outfile)
			{
				error("Could not create file %s: %s", "create", szDestFile, Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
				break;
			}
			bNeedBufFile = true;
		}

		if (outfile && bNeedBufFile)
		{
			SetWriteBuffer(outfile, 0);
			bNeedBufFile = false;
		}

		if (bDirectWrite)
		{
			fseek(outfile, pa->GetSegmentOffset(), SEEK_SET);
		}

		fwrite(pa->GetSegmentContent(), 1, pa->GetSegmentSize(), outfile);

		iFlushedSize += pa->GetSegmentSize();
		iFlushedArticles++;

		pa->DiscardSegment();

		if (!bDirectWrite)
		{
			fclose(outfile);
			outfile = NULL;

			if (!Util::MoveFile(szDestFile, pa->GetResultFilename()))
			{
				error("Could not rename file %s to %s: %s", szDestFile, pa->GetResultFilename(), Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
			}
		}
	}

	if (outfile)
	{
		fclose(outfile);
	}

	g_pArticleCache->LockContent();
	m_pFileInfo->SetCachedArticles(m_pFileInfo->GetCachedArticles() - iFlushedArticles);
	g_pArticleCache->UnlockContent();

	g_pArticleCache->UnlockFlush();

	detail("Saved %i articles (%.2f MB) from cache into disk for %s", iFlushedArticles, (float)(iFlushedSize / 1024.0 / 1024.0), szInfoFilename);
}

bool ArticleWriter::MoveCompletedFiles(NZBInfo* pNZBInfo, const char* szOldDestDir)
{
	if (pNZBInfo->GetCompletedFiles()->empty())
	{
		return true;
	}

	// Ensure the DstDir is created
	char szErrBuf[1024];
	if (!Util::ForceDirectories(pNZBInfo->GetDestDir(), szErrBuf, sizeof(szErrBuf)))
	{
		error("Could not create directory %s: %s", pNZBInfo->GetDestDir(), szErrBuf);
		return false;
	}

	// move already downloaded files to new destination
	for (CompletedFiles::iterator it = pNZBInfo->GetCompletedFiles()->begin(); it != pNZBInfo->GetCompletedFiles()->end(); it++)
    {
		CompletedFile* pCompletedFile = *it;

		char szOldFileName[1024];
		snprintf(szOldFileName, 1024, "%s%c%s", szOldDestDir, (int)PATH_SEPARATOR, pCompletedFile->GetFileName());
		szOldFileName[1024-1] = '\0';

		char szNewFileName[1024];
		snprintf(szNewFileName, 1024, "%s%c%s", pNZBInfo->GetDestDir(), (int)PATH_SEPARATOR, pCompletedFile->GetFileName());
		szNewFileName[1024-1] = '\0';

		// check if file was not moved already
		if (strcmp(szOldFileName, szNewFileName))
		{
			// prevent overwriting of existing files
			Util::MakeUniqueFilename(szNewFileName, 1024, pNZBInfo->GetDestDir(), pCompletedFile->GetFileName());

			detail("Moving file %s to %s", szOldFileName, szNewFileName);
			if (!Util::MoveFile(szOldFileName, szNewFileName))
			{
				char szErrBuf[256];
				error("Could not move file %s to %s: %s", szOldFileName, szNewFileName, Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
			}
		}
    }

	// move brokenlog.txt
	if (g_pOptions->GetCreateBrokenLog())
	{
		char szOldBrokenLogName[1024];
		snprintf(szOldBrokenLogName, 1024, "%s%c_brokenlog.txt", szOldDestDir, (int)PATH_SEPARATOR);
		szOldBrokenLogName[1024-1] = '\0';
		if (Util::FileExists(szOldBrokenLogName))
		{
			char szBrokenLogName[1024];
			snprintf(szBrokenLogName, 1024, "%s%c_brokenlog.txt", pNZBInfo->GetDestDir(), (int)PATH_SEPARATOR);
			szBrokenLogName[1024-1] = '\0';

			detail("Moving file %s to %s", szOldBrokenLogName, szBrokenLogName);
			if (Util::FileExists(szBrokenLogName))
			{
				// copy content to existing new file, then delete old file
				FILE* outfile;
				outfile = fopen(szBrokenLogName, FOPEN_AB);
				if (outfile)
				{
					FILE* infile;
					infile = fopen(szOldBrokenLogName, FOPEN_RB);
					if (infile)
					{
						static const int BUFFER_SIZE = 1024 * 50;
						int cnt = BUFFER_SIZE;
						char* buffer = (char*)malloc(BUFFER_SIZE);
						while (cnt == BUFFER_SIZE)
						{
							cnt = (int)fread(buffer, 1, BUFFER_SIZE, infile);
							fwrite(buffer, 1, cnt, outfile);
						}
						fclose(infile);
						free(buffer);
						remove(szOldBrokenLogName);
					}
					else
					{
						error("Could not open file %s", szOldBrokenLogName);
					}
					fclose(outfile);
				}
				else
				{
					error("Could not open file %s", szBrokenLogName);
				}
			}
			else 
			{
				// move to new destination
				if (!Util::MoveFile(szOldBrokenLogName, szBrokenLogName))
				{
					char szErrBuf[256];
					error("Could not move file %s to %s: %s", szOldBrokenLogName, szBrokenLogName, Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf)));
				}
			}
		}
	}

	// delete old directory (if empty)
	if (Util::DirEmpty(szOldDestDir))
	{
		// check if there are pending writes into directory
		bool bPendingWrites = false;
		for (FileList::iterator it = pNZBInfo->GetFileList()->begin(); it != pNZBInfo->GetFileList()->end() && !bPendingWrites; it++)
		{
			FileInfo* pFileInfo = *it;
			if (pFileInfo->GetActiveDownloads() > 0)
			{
				pFileInfo->LockOutputFile();
				bPendingWrites = pFileInfo->GetOutputInitialized();
				pFileInfo->UnlockOutputFile();
			}
			else
			{
				bPendingWrites = pFileInfo->GetOutputInitialized() && !Util::EmptyStr(pFileInfo->GetOutputFilename());
			}
		}

		if (!bPendingWrites)
		{
			rmdir(szOldDestDir);
		}
	}

	return true;
}


ArticleCache::ArticleCache()
{
	m_iAllocated = 0;
	m_bFlushing = false;
	m_pFileInfo = NULL;
}

void* ArticleCache::Alloc(int iSize)
{
	m_mutexAlloc.Lock();

	void* p = NULL;
	if (m_iAllocated + iSize <= (size_t)g_pOptions->GetArticleCache() * 1024 * 1024)
	{
		p = malloc(iSize);
		if (p)
		{
			if (!m_iAllocated && g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode() && g_pOptions->GetContinuePartial())
			{
				g_pDiskState->WriteCacheFlag();
			}
			m_iAllocated += iSize;
		}
	}
	m_mutexAlloc.Unlock();

	return p;
}

void* ArticleCache::Realloc(void* buf, int iOldSize, int iNewSize)
{
	m_mutexAlloc.Lock();

	void* p = realloc(buf, iNewSize);
	if (p)
	{
		m_iAllocated += iNewSize - iOldSize;
	}
	else
	{
		p = buf;
	}
	m_mutexAlloc.Unlock();

	return p;
}

void ArticleCache::Free(int iSize)
{
	m_mutexAlloc.Lock();
	m_iAllocated -= iSize;
	if (!m_iAllocated && g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode() && g_pOptions->GetContinuePartial())
	{
		g_pDiskState->DeleteCacheFlag();
	}
	m_mutexAlloc.Unlock();
}

void ArticleCache::LockFlush()
{
	m_mutexFlush.Lock();
	m_bFlushing = true;
}

void ArticleCache::UnlockFlush()
{
	m_mutexFlush.Unlock();
	m_bFlushing = false;
}

void ArticleCache::Run()
{
	// automatically flush the cache if it is filled to 90% (only in DirectWrite mode)
	size_t iFillThreshold = (size_t)g_pOptions->GetArticleCache() * 1024 * 1024 / 100 * 90;

	int iResetCounter = 0;
	bool bJustFlushed = false;
	while (!IsStopped() || m_iAllocated > 0)
	{
		if ((bJustFlushed || iResetCounter >= 1000  || IsStopped() ||
			 (g_pOptions->GetDirectWrite() && m_iAllocated >= iFillThreshold)) &&
			m_iAllocated > 0)
		{
			bJustFlushed = CheckFlush(m_iAllocated >= iFillThreshold);
			iResetCounter = 0;
		}
		else
		{
			usleep(5 * 1000);
			iResetCounter += 5;
		}
	}
}

bool ArticleCache::CheckFlush(bool bFlushEverything)
{
	debug("Checking cache, Allocated: %i, FlushEverything: %i", m_iAllocated, (int)bFlushEverything);

	DownloadQueue* pDownloadQueue = DownloadQueue::Lock();
	for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end() && !m_pFileInfo; it++)
	{
		NZBInfo* pNZBInfo = *it;
		for (FileList::iterator it2 = pNZBInfo->GetFileList()->begin(); it2 != pNZBInfo->GetFileList()->end(); it2++)
		{
			FileInfo* pFileInfo = *it2;
			if (pFileInfo->GetCachedArticles() > 0 && (pFileInfo->GetActiveDownloads() == 0 || bFlushEverything))
			{
				m_pFileInfo = pFileInfo;
				break;
			}
		}
	}
	DownloadQueue::Unlock();

	if (m_pFileInfo)
	{
		ArticleWriter* pArticleWriter = new ArticleWriter();
		pArticleWriter->SetFileInfo(m_pFileInfo);
		pArticleWriter->FlushCache();
		delete pArticleWriter;
		m_pFileInfo = NULL;
		return true;
	}

	debug("Checking cache... nothing to flush");

	return false;
}
