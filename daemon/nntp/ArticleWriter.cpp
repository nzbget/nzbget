/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2014-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "ArticleWriter.h"
#include "DiskState.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"

ArticleWriter::ArticleWriter()
{
	debug("Creating ArticleWriter");

	m_resultFilename = NULL;
	m_format = Decoder::efUnknown;
	m_articleData = NULL;
	m_duplicate = false;
	m_flushing = false;
}

ArticleWriter::~ArticleWriter()
{
	debug("Destroying ArticleWriter");

	if (m_articleData)
	{
		free(m_articleData);
		g_ArticleCache->Free(m_articleSize);
	}

	if (m_flushing)
	{
		g_ArticleCache->UnlockFlush();
	}
}

void ArticleWriter::SetWriteBuffer(FILE* outFile, int recSize)
{
	if (g_Options->GetWriteBuffer() > 0)
	{
		setvbuf(outFile, NULL, _IOFBF,
			recSize > 0 && recSize < g_Options->GetWriteBuffer() * 1024 ?
			recSize : g_Options->GetWriteBuffer() * 1024);
	}
}

void ArticleWriter::Prepare()
{
	BuildOutputFilename();
	m_resultFilename = m_articleInfo->GetResultFilename();
}

bool ArticleWriter::Start(Decoder::EFormat format, const char* filename, int64 fileSize,
	int64 articleOffset, int articleSize)
{
	char errBuf[256];
	m_outFile = NULL;
	m_format = format;
	m_articleOffset = articleOffset;
	m_articleSize = articleSize ? articleSize : m_articleInfo->GetSize();
	m_articlePtr = 0;

	// prepare file for writing
	if (m_format == Decoder::efYenc)
	{
		if (g_Options->GetDupeCheck() &&
			m_fileInfo->GetNzbInfo()->GetDupeMode() != dmForce &&
			!m_fileInfo->GetNzbInfo()->GetManyDupeFiles())
		{
			m_fileInfo->LockOutputFile();
			bool outputInitialized = m_fileInfo->GetOutputInitialized();
			if (!g_Options->GetDirectWrite())
			{
				m_fileInfo->SetOutputInitialized(true);
			}
			m_fileInfo->UnlockOutputFile();
			if (!outputInitialized && filename &&
				Util::FileExists(m_fileInfo->GetNzbInfo()->GetDestDir(), filename))
			{
				m_duplicate = true;
				return false;
			}
		}

		if (g_Options->GetDirectWrite())
		{
			m_fileInfo->LockOutputFile();
			if (!m_fileInfo->GetOutputInitialized())
			{
				if (!CreateOutputFile(fileSize))
				{
					m_fileInfo->UnlockOutputFile();
					return false;
				}
				m_fileInfo->SetOutputInitialized(true);
			}
			m_fileInfo->UnlockOutputFile();
		}
	}

	// allocate cache buffer
	if (g_Options->GetArticleCache() > 0 && g_Options->GetDecode() &&
		(!g_Options->GetDirectWrite() || m_format == Decoder::efYenc))
	{
		if (m_articleData)
		{
			free(m_articleData);
			g_ArticleCache->Free(m_articleSize);
		}

		m_articleData = (char*)g_ArticleCache->Alloc(m_articleSize);

		while (!m_articleData && g_ArticleCache->GetFlushing())
		{
			usleep(5 * 1000);
			m_articleData = (char*)g_ArticleCache->Alloc(m_articleSize);
		}

		if (!m_articleData)
		{
			detail("Article cache is full, using disk for %s", *m_infoName);
		}
	}

	if (!m_articleData)
	{
		bool directWrite = g_Options->GetDirectWrite() && m_format == Decoder::efYenc;
		const char* filename = directWrite ? m_outputFilename : m_tempFilename;
		m_outFile = fopen(filename, directWrite ? FOPEN_RBP : FOPEN_WB);
		if (!m_outFile)
		{
			m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
				"Could not %s file %s: %s", directWrite ? "open" : "create", filename,
				Util::GetLastErrorMessage(errBuf, sizeof(errBuf)));
			return false;
		}
		SetWriteBuffer(m_outFile, m_articleInfo->GetSize());

		if (g_Options->GetDirectWrite() && m_format == Decoder::efYenc)
		{
			fseek(m_outFile, m_articleOffset, SEEK_SET);
		}
	}

	return true;
}

bool ArticleWriter::Write(char* bufffer, int len)
{
	if (g_Options->GetDecode())
	{
		m_articlePtr += len;
	}

	if (g_Options->GetDecode() && m_articleData)
	{
		if (m_articlePtr > m_articleSize)
		{
			detail("Decoding %s failed: article size mismatch", *m_infoName);
			return false;
		}
		memcpy(m_articleData + m_articlePtr - len, bufffer, len);
		return true;
	}

	return fwrite(bufffer, 1, len, m_outFile) > 0;
}

void ArticleWriter::Finish(bool success)
{
	char errBuf[256];

	if (m_outFile)
	{
		fclose(m_outFile);
		m_outFile = NULL;
	}

	if (!success)
	{
		remove(m_tempFilename);
		remove(m_resultFilename);
		return;
	}

	bool directWrite = g_Options->GetDirectWrite() && m_format == Decoder::efYenc;

	if (g_Options->GetDecode())
	{
		if (!directWrite && !m_articleData)
		{
			if (!Util::MoveFile(m_tempFilename, m_resultFilename))
			{
				m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
					"Could not rename file %s to %s: %s", *m_tempFilename, m_resultFilename,
					Util::GetLastErrorMessage(errBuf, sizeof(errBuf)));
			}
		}

		remove(m_tempFilename);

		if (m_articleData)
		{
			if (m_articleSize != m_articlePtr)
			{
				m_articleData = (char*)g_ArticleCache->Realloc(m_articleData, m_articleSize, m_articlePtr);
			}
			g_ArticleCache->LockContent();
			m_articleInfo->AttachSegment(m_articleData, m_articleOffset, m_articlePtr);
			m_fileInfo->SetCachedArticles(m_fileInfo->GetCachedArticles() + 1);
			g_ArticleCache->UnlockContent();
			m_articleData = NULL;
		}
		else
		{
			m_articleInfo->SetSegmentOffset(m_articleOffset);
			m_articleInfo->SetSegmentSize(m_articlePtr);
		}
	}
	else
	{
		// rawmode
		if (!Util::MoveFile(m_tempFilename, m_resultFilename))
		{
			m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
				"Could not move file %s to %s: %s", *m_tempFilename, m_resultFilename,
				Util::GetLastErrorMessage(errBuf, sizeof(errBuf)));
		}
	}
}

/* creates output file and subdirectores */
bool ArticleWriter::CreateOutputFile(int64 size)
{
	if (g_Options->GetDirectWrite() && Util::FileExists(m_outputFilename) &&
		Util::FileSize(m_outputFilename) == size)
	{
		// keep existing old file from previous program session
		return true;
	}

	// delete eventually existing old file from previous program session
	remove(m_outputFilename);

	// ensure the directory exist
	char destDir[1024];
	int maxlen = Util::BaseFileName(m_outputFilename) - m_outputFilename;
	if (maxlen > 1024-1) maxlen = 1024-1;
	strncpy(destDir, m_outputFilename, maxlen);
	destDir[maxlen] = '\0';
	char errBuf[1024];

	if (!Util::ForceDirectories(destDir, errBuf, sizeof(errBuf)))
	{
		m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
			"Could not create directory %s: %s", destDir, errBuf);
		return false;
	}

	if (!Util::CreateSparseFile(m_outputFilename, size, errBuf, sizeof(errBuf)))
	{
		m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
			"Could not create file %s: %s", *m_outputFilename, errBuf);
		return false;
	}

	return true;
}

void ArticleWriter::BuildOutputFilename()
{
	char filename[1024];

	snprintf(filename, 1024, "%s%i.%03i", g_Options->GetTempDir(), m_fileInfo->GetId(), m_articleInfo->GetPartNumber());
	filename[1024-1] = '\0';
	m_articleInfo->SetResultFilename(filename);

	char tmpname[1024];
	snprintf(tmpname, 1024, "%s.tmp", filename);
	tmpname[1024-1] = '\0';
	m_tempFilename = tmpname;

	if (g_Options->GetDirectWrite())
	{
		m_fileInfo->LockOutputFile();

		if (m_fileInfo->GetOutputFilename())
		{
			strncpy(filename, m_fileInfo->GetOutputFilename(), 1024);
			filename[1024-1] = '\0';
		}
		else
		{
			snprintf(filename, 1024, "%s%c%i.out.tmp", m_fileInfo->GetNzbInfo()->GetDestDir(), (int)PATH_SEPARATOR, m_fileInfo->GetId());
			filename[1024-1] = '\0';
			m_fileInfo->SetOutputFilename(filename);
		}

		m_fileInfo->UnlockOutputFile();

		m_outputFilename = filename;
	}
}

void ArticleWriter::CompleteFileParts()
{
	debug("Completing file parts");
	debug("ArticleFilename: %s", m_fileInfo->GetFilename());

	bool directWrite = g_Options->GetDirectWrite() && m_fileInfo->GetOutputInitialized();
	char errBuf[256];

	char nzbName[1024];
	char nzbDestDir[1024];
	// the locking is needed for accessing the members of NZBInfo
	DownloadQueue::Lock();
	strncpy(nzbName, m_fileInfo->GetNzbInfo()->GetName(), 1024);
	strncpy(nzbDestDir, m_fileInfo->GetNzbInfo()->GetDestDir(), 1024);
	DownloadQueue::Unlock();
	nzbName[1024-1] = '\0';
	nzbDestDir[1024-1] = '\0';

	char infoFilename[1024];
	snprintf(infoFilename, 1024, "%s%c%s", nzbName, (int)PATH_SEPARATOR, m_fileInfo->GetFilename());
	infoFilename[1024-1] = '\0';

	bool cached = m_fileInfo->GetCachedArticles() > 0;

	if (!g_Options->GetDecode())
	{
		detail("Moving articles for %s", infoFilename);
	}
	else if (directWrite && cached)
	{
		detail("Writing articles for %s", infoFilename);
	}
	else if (directWrite)
	{
		detail("Checking articles for %s", infoFilename);
	}
	else
	{
		detail("Joining articles for %s", infoFilename);
	}

	// Ensure the DstDir is created
	if (!Util::ForceDirectories(nzbDestDir, errBuf, sizeof(errBuf)))
	{
		m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
			"Could not create directory %s: %s", nzbDestDir, errBuf);
		return;
	}

	char ofn[1024];
	Util::MakeUniqueFilename(ofn, 1024, nzbDestDir, m_fileInfo->GetFilename());

	FILE* outfile = NULL;
	char tmpdestfile[1024];
	snprintf(tmpdestfile, 1024, "%s.tmp", ofn);
	tmpdestfile[1024-1] = '\0';

	if (g_Options->GetDecode() && !directWrite)
	{
		remove(tmpdestfile);
		outfile = fopen(tmpdestfile, FOPEN_WBP);
		if (!outfile)
		{
			m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
				"Could not create file %s: %s", tmpdestfile, Util::GetLastErrorMessage(errBuf, sizeof(errBuf)));
			return;
		}
	}
	else if (directWrite && cached)
	{
		outfile = fopen(m_outputFilename, FOPEN_RBP);
		if (!outfile)
		{
			m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
				"Could not open file %s: %s", *m_outputFilename, Util::GetLastErrorMessage(errBuf, sizeof(errBuf)));
			return;
		}
		strncpy(tmpdestfile, m_outputFilename, 1024);
		tmpdestfile[1024-1] = '\0';
	}
	else if (!g_Options->GetDecode())
	{
		remove(tmpdestfile);
		if (!Util::CreateDirectory(ofn))
		{
			m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
				"Could not create directory %s: %s", ofn, Util::GetLastErrorMessage(errBuf, sizeof(errBuf)));
			return;
		}
	}

	if (outfile)
	{
		SetWriteBuffer(outfile, 0);
	}

	if (cached)
	{
		g_ArticleCache->LockFlush();
		m_flushing = true;
	}

	static const int BUFFER_SIZE = 1024 * 64;
	char* buffer = NULL;
	bool firstArticle = true;
	uint32 crc = 0;

	if (g_Options->GetDecode() && !directWrite)
	{
		buffer = (char*)malloc(BUFFER_SIZE);
	}

	for (FileInfo::Articles::iterator it = m_fileInfo->GetArticles()->begin(); it != m_fileInfo->GetArticles()->end(); it++)
	{
		ArticleInfo* pa = *it;
		if (pa->GetStatus() != ArticleInfo::aiFinished)
		{
			continue;
		}

		if (g_Options->GetDecode() && !directWrite && pa->GetSegmentOffset() > -1 &&
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
		else if (g_Options->GetDecode() && !directWrite)
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
				m_fileInfo->SetFailedArticles(m_fileInfo->GetFailedArticles() + 1);
				m_fileInfo->SetSuccessArticles(m_fileInfo->GetSuccessArticles() - 1);
				m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
					"Could not find file %s for %s%c%s [%i/%i]",
					pa->GetResultFilename(), nzbName, (int)PATH_SEPARATOR, m_fileInfo->GetFilename(),
					pa->GetPartNumber(), (int)m_fileInfo->GetArticles()->size());
			}
		}
		else if (!g_Options->GetDecode())
		{
			char dstFileName[1024];
			snprintf(dstFileName, 1024, "%s%c%03i", ofn, (int)PATH_SEPARATOR, pa->GetPartNumber());
			dstFileName[1024-1] = '\0';
			if (!Util::MoveFile(pa->GetResultFilename(), dstFileName))
			{
				m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
					"Could not move file %s to %s: %s", pa->GetResultFilename(), dstFileName,
					Util::GetLastErrorMessage(errBuf, sizeof(errBuf)));
			}
		}

		if (m_format == Decoder::efYenc)
		{
			crc = firstArticle ? pa->GetCrc() : Util::Crc32Combine(crc, pa->GetCrc(), pa->GetSegmentSize());
			firstArticle = false;
		}
	}

	free(buffer);

	if (cached)
	{
		g_ArticleCache->UnlockFlush();
		m_flushing = false;
	}

	if (outfile)
	{
		fclose(outfile);
		if (!directWrite && !Util::MoveFile(tmpdestfile, ofn))
		{
			m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
				"Could not move file %s to %s: %s", tmpdestfile, ofn,
				Util::GetLastErrorMessage(errBuf, sizeof(errBuf)));
		}
	}

	if (directWrite)
	{
		if (!Util::MoveFile(m_outputFilename, ofn))
		{
			m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
				"Could not move file %s to %s: %s", *m_outputFilename, ofn,
				Util::GetLastErrorMessage(errBuf, sizeof(errBuf)));
		}

		// if destination directory was changed delete the old directory (if empty)
		int len = strlen(nzbDestDir);
		if (!(!strncmp(nzbDestDir, m_outputFilename, len) &&
			(m_outputFilename[len] == PATH_SEPARATOR || m_outputFilename[len] == ALT_PATH_SEPARATOR)))
		{
			debug("Checking old dir for: %s", *m_outputFilename);
			char oldDestDir[1024];
			int maxlen = Util::BaseFileName(m_outputFilename) - m_outputFilename;
			if (maxlen > 1024-1) maxlen = 1024-1;
			strncpy(oldDestDir, m_outputFilename, maxlen);
			oldDestDir[maxlen] = '\0';
			if (Util::DirEmpty(oldDestDir))
			{
				debug("Deleting old dir: %s", oldDestDir);
				rmdir(oldDestDir);
			}
		}
	}

	if (!directWrite)
	{
		for (FileInfo::Articles::iterator it = m_fileInfo->GetArticles()->begin(); it != m_fileInfo->GetArticles()->end(); it++)
		{
			ArticleInfo* pa = *it;
			remove(pa->GetResultFilename());
		}
	}

	if (m_fileInfo->GetMissedArticles() == 0 && m_fileInfo->GetFailedArticles() == 0)
	{
		m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkInfo, "Successfully downloaded %s", infoFilename);
	}
	else
	{
		m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkWarning,
			"%i of %i article downloads failed for \"%s\"",
			m_fileInfo->GetMissedArticles() + m_fileInfo->GetFailedArticles(),
			m_fileInfo->GetTotalArticles(), infoFilename);

		if (g_Options->GetBrokenLog())
		{
			char brokenLogName[1024];
			snprintf(brokenLogName, 1024, "%s%c_brokenlog.txt", nzbDestDir, (int)PATH_SEPARATOR);
			brokenLogName[1024-1] = '\0';
			FILE* file = fopen(brokenLogName, FOPEN_AB);
			fprintf(file, "%s (%i/%i)%s", m_fileInfo->GetFilename(), m_fileInfo->GetSuccessArticles(),
				m_fileInfo->GetTotalArticles(), LINE_ENDING);
			fclose(file);
		}

		crc = 0;

		if (g_Options->GetSaveQueue() && g_Options->GetServerMode())
		{
			g_DiskState->DiscardFile(m_fileInfo, false, true, false);
			g_DiskState->SaveFileState(m_fileInfo, true);
		}
	}

	CompletedFile::EStatus fileStatus = m_fileInfo->GetMissedArticles() == 0 &&
		m_fileInfo->GetFailedArticles() == 0 ? CompletedFile::cfSuccess :
		m_fileInfo->GetSuccessArticles() > 0 ? CompletedFile::cfPartial :
		CompletedFile::cfFailure;

	// the locking is needed for accessing the members of NZBInfo
	DownloadQueue::Lock();
	m_fileInfo->GetNzbInfo()->GetCompletedFiles()->push_back(new CompletedFile(
		m_fileInfo->GetId(), Util::BaseFileName(ofn), fileStatus, crc));
	if (strcmp(m_fileInfo->GetNzbInfo()->GetDestDir(), nzbDestDir))
	{
		// destination directory was changed during completion, need to move the file
		MoveCompletedFiles(m_fileInfo->GetNzbInfo(), nzbDestDir);
	}
	DownloadQueue::Unlock();
}

void ArticleWriter::FlushCache()
{
	detail("Flushing cache for %s", *m_infoName);

	bool directWrite = g_Options->GetDirectWrite() && m_fileInfo->GetOutputInitialized();
	FILE* outfile = NULL;
	bool needBufFile = false;
	char destFile[1024];
	char errBuf[256];
	int flushedArticles = 0;
	int64 flushedSize = 0;

	g_ArticleCache->LockFlush();

	FileInfo::Articles cachedArticles;
	cachedArticles.reserve(m_fileInfo->GetArticles()->size());

	g_ArticleCache->LockContent();
	for (FileInfo::Articles::iterator it = m_fileInfo->GetArticles()->begin(); it != m_fileInfo->GetArticles()->end(); it++)
	{
		ArticleInfo* pa = *it;
		if (pa->GetSegmentContent())
		{
			cachedArticles.push_back(pa);
		}
	}
	g_ArticleCache->UnlockContent();

	for (FileInfo::Articles::iterator it = cachedArticles.begin(); it != cachedArticles.end(); it++)
	{
		if (m_fileInfo->GetDeleted())
		{
			// the file was deleted during flushing: stop flushing immediately
			break;
		}

		ArticleInfo* pa = *it;

		if (directWrite && !outfile)
		{
			outfile = fopen(m_fileInfo->GetOutputFilename(), FOPEN_RBP);
			if (!outfile)
			{
				m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
					"Could not open file %s: %s", m_fileInfo->GetOutputFilename(),
					Util::GetLastErrorMessage(errBuf, sizeof(errBuf)));
				break;
			}
			needBufFile = true;
		}

		if (!directWrite)
		{
			snprintf(destFile, 1024, "%s.tmp", pa->GetResultFilename());
			destFile[1024-1] = '\0';

			outfile = fopen(destFile, FOPEN_WB);
			if (!outfile)
			{
				m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
					"Could not create file %s: %s", "create", destFile,
					Util::GetLastErrorMessage(errBuf, sizeof(errBuf)));
				break;
			}
			needBufFile = true;
		}

		if (outfile && needBufFile)
		{
			SetWriteBuffer(outfile, 0);
			needBufFile = false;
		}

		if (directWrite)
		{
			fseek(outfile, pa->GetSegmentOffset(), SEEK_SET);
		}

		fwrite(pa->GetSegmentContent(), 1, pa->GetSegmentSize(), outfile);

		flushedSize += pa->GetSegmentSize();
		flushedArticles++;

		pa->DiscardSegment();

		if (!directWrite)
		{
			fclose(outfile);
			outfile = NULL;

			if (!Util::MoveFile(destFile, pa->GetResultFilename()))
			{
				m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
					"Could not rename file %s to %s: %s", destFile, pa->GetResultFilename(),
					Util::GetLastErrorMessage(errBuf, sizeof(errBuf)));
			}
		}
	}

	if (outfile)
	{
		fclose(outfile);
	}

	g_ArticleCache->LockContent();
	m_fileInfo->SetCachedArticles(m_fileInfo->GetCachedArticles() - flushedArticles);
	g_ArticleCache->UnlockContent();

	g_ArticleCache->UnlockFlush();

	detail("Saved %i articles (%.2f MB) from cache into disk for %s", flushedArticles,
		(float)(flushedSize / 1024.0 / 1024.0), *m_infoName);
}

bool ArticleWriter::MoveCompletedFiles(NzbInfo* nzbInfo, const char* oldDestDir)
{
	if (nzbInfo->GetCompletedFiles()->empty())
	{
		return true;
	}

	// Ensure the DstDir is created
	char errBuf[1024];
	if (!Util::ForceDirectories(nzbInfo->GetDestDir(), errBuf, sizeof(errBuf)))
	{
		nzbInfo->PrintMessage(Message::mkError, "Could not create directory %s: %s", nzbInfo->GetDestDir(), errBuf);
		return false;
	}

	// move already downloaded files to new destination
	for (CompletedFiles::iterator it = nzbInfo->GetCompletedFiles()->begin(); it != nzbInfo->GetCompletedFiles()->end(); it++)
	{
		CompletedFile* completedFile = *it;

		char oldFileName[1024];
		snprintf(oldFileName, 1024, "%s%c%s", oldDestDir, (int)PATH_SEPARATOR, completedFile->GetFileName());
		oldFileName[1024-1] = '\0';

		char newFileName[1024];
		snprintf(newFileName, 1024, "%s%c%s", nzbInfo->GetDestDir(), (int)PATH_SEPARATOR, completedFile->GetFileName());
		newFileName[1024-1] = '\0';

		// check if file was not moved already
		if (strcmp(oldFileName, newFileName))
		{
			// prevent overwriting of existing files
			Util::MakeUniqueFilename(newFileName, 1024, nzbInfo->GetDestDir(), completedFile->GetFileName());

			detail("Moving file %s to %s", oldFileName, newFileName);
			if (!Util::MoveFile(oldFileName, newFileName))
			{
				char errBuf[256];
				nzbInfo->PrintMessage(Message::mkError, "Could not move file %s to %s: %s",
					oldFileName, newFileName, Util::GetLastErrorMessage(errBuf, sizeof(errBuf)));
			}
		}
	}

	// move brokenlog.txt
	if (g_Options->GetBrokenLog())
	{
		char oldBrokenLogName[1024];
		snprintf(oldBrokenLogName, 1024, "%s%c_brokenlog.txt", oldDestDir, (int)PATH_SEPARATOR);
		oldBrokenLogName[1024-1] = '\0';
		if (Util::FileExists(oldBrokenLogName))
		{
			char brokenLogName[1024];
			snprintf(brokenLogName, 1024, "%s%c_brokenlog.txt", nzbInfo->GetDestDir(), (int)PATH_SEPARATOR);
			brokenLogName[1024-1] = '\0';

			detail("Moving file %s to %s", oldBrokenLogName, brokenLogName);
			if (Util::FileExists(brokenLogName))
			{
				// copy content to existing new file, then delete old file
				FILE* outfile;
				outfile = fopen(brokenLogName, FOPEN_AB);
				if (outfile)
				{
					FILE* infile;
					infile = fopen(oldBrokenLogName, FOPEN_RB);
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
						remove(oldBrokenLogName);
					}
					else
					{
						nzbInfo->PrintMessage(Message::mkError, "Could not open file %s", oldBrokenLogName);
					}
					fclose(outfile);
				}
				else
				{
					nzbInfo->PrintMessage(Message::mkError, "Could not open file %s", brokenLogName);
				}
			}
			else
			{
				// move to new destination
				if (!Util::MoveFile(oldBrokenLogName, brokenLogName))
				{
					char errBuf[256];
					nzbInfo->PrintMessage(Message::mkError, "Could not move file %s to %s: %s",
						oldBrokenLogName, brokenLogName, Util::GetLastErrorMessage(errBuf, sizeof(errBuf)));
				}
			}
		}
	}

	// delete old directory (if empty)
	if (Util::DirEmpty(oldDestDir))
	{
		// check if there are pending writes into directory
		bool pendingWrites = false;
		for (FileList::iterator it = nzbInfo->GetFileList()->begin(); it != nzbInfo->GetFileList()->end() && !pendingWrites; it++)
		{
			FileInfo* fileInfo = *it;
			if (fileInfo->GetActiveDownloads() > 0)
			{
				fileInfo->LockOutputFile();
				pendingWrites = fileInfo->GetOutputInitialized() && !Util::EmptyStr(fileInfo->GetOutputFilename());
				fileInfo->UnlockOutputFile();
			}
			else
			{
				pendingWrites = fileInfo->GetOutputInitialized() && !Util::EmptyStr(fileInfo->GetOutputFilename());
			}
		}

		if (!pendingWrites)
		{
			rmdir(oldDestDir);
		}
	}

	return true;
}


ArticleCache::ArticleCache()
{
	m_allocated = 0;
	m_flushing = false;
	m_fileInfo = NULL;
}

void* ArticleCache::Alloc(int size)
{
	m_allocMutex.Lock();

	void* p = NULL;
	if (m_allocated + size <= (size_t)g_Options->GetArticleCache() * 1024 * 1024)
	{
		p = malloc(size);
		if (p)
		{
			if (!m_allocated && g_Options->GetSaveQueue() && g_Options->GetServerMode() && g_Options->GetContinuePartial())
			{
				g_DiskState->WriteCacheFlag();
			}
			m_allocated += size;
		}
	}
	m_allocMutex.Unlock();

	return p;
}

void* ArticleCache::Realloc(void* buf, int oldSize, int newSize)
{
	m_allocMutex.Lock();

	void* p = realloc(buf, newSize);
	if (p)
	{
		m_allocated += newSize - oldSize;
	}
	else
	{
		p = buf;
	}
	m_allocMutex.Unlock();

	return p;
}

void ArticleCache::Free(int size)
{
	m_allocMutex.Lock();
	m_allocated -= size;
	if (!m_allocated && g_Options->GetSaveQueue() && g_Options->GetServerMode() && g_Options->GetContinuePartial())
	{
		g_DiskState->DeleteCacheFlag();
	}
	m_allocMutex.Unlock();
}

void ArticleCache::LockFlush()
{
	m_flushMutex.Lock();
	m_flushing = true;
}

void ArticleCache::UnlockFlush()
{
	m_flushMutex.Unlock();
	m_flushing = false;
}

void ArticleCache::Run()
{
	// automatically flush the cache if it is filled to 90% (only in DirectWrite mode)
	size_t fillThreshold = (size_t)g_Options->GetArticleCache() * 1024 * 1024 / 100 * 90;

	int resetCounter = 0;
	bool justFlushed = false;
	while (!IsStopped() || m_allocated > 0)
	{
		if ((justFlushed || resetCounter >= 1000  || IsStopped() ||
			 (g_Options->GetDirectWrite() && m_allocated >= fillThreshold)) &&
			m_allocated > 0)
		{
			justFlushed = CheckFlush(m_allocated >= fillThreshold);
			resetCounter = 0;
		}
		else
		{
			usleep(5 * 1000);
			resetCounter += 5;
		}
	}
}

bool ArticleCache::CheckFlush(bool flushEverything)
{
	debug("Checking cache, Allocated: %i, FlushEverything: %i", m_allocated, (int)flushEverything);

	char infoName[1024];

	DownloadQueue* downloadQueue = DownloadQueue::Lock();
	for (NzbList::iterator it = downloadQueue->GetQueue()->begin(); it != downloadQueue->GetQueue()->end() && !m_fileInfo; it++)
	{
		NzbInfo* nzbInfo = *it;
		for (FileList::iterator it2 = nzbInfo->GetFileList()->begin(); it2 != nzbInfo->GetFileList()->end(); it2++)
		{
			FileInfo* fileInfo = *it2;
			if (fileInfo->GetCachedArticles() > 0 && (fileInfo->GetActiveDownloads() == 0 || flushEverything))
			{
				m_fileInfo = fileInfo;
				snprintf(infoName, 1024, "%s%c%s", m_fileInfo->GetNzbInfo()->GetName(), (int)PATH_SEPARATOR, m_fileInfo->GetFilename());
				infoName[1024-1] = '\0';
				break;
			}
		}
	}
	DownloadQueue::Unlock();

	if (m_fileInfo)
	{
		ArticleWriter* articleWriter = new ArticleWriter();
		articleWriter->SetFileInfo(m_fileInfo);
		articleWriter->SetInfoName(infoName);
		articleWriter->FlushCache();
		delete articleWriter;
		m_fileInfo = NULL;
		return true;
	}

	debug("Checking cache... nothing to flush");

	return false;
}
