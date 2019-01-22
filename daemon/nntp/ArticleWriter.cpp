/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2014-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "ArticleWriter.h"
#include "DiskState.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"
#include "FileSystem.h"

CachedSegmentData::~CachedSegmentData()
{
	g_ArticleCache->Free(this);
}

CachedSegmentData& CachedSegmentData::operator=(CachedSegmentData&& other)
{
	g_ArticleCache->Free(this);
	m_data = other.m_data;
	m_size = other.m_size;
	other.m_data = nullptr;
	other.m_size = 0;
	return *this;
}


void ArticleWriter::SetWriteBuffer(DiskFile& outFile, int recSize)
{
	if (g_Options->GetWriteBuffer() > 0)
	{
		outFile.SetWriteBuffer(recSize > 0 && recSize < g_Options->GetWriteBuffer() * 1024 ?
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
	m_outFile.Close();
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
			bool outputInitialized;
			{
				Guard guard = m_fileInfo->GuardOutputFile();
				outputInitialized = m_fileInfo->GetOutputInitialized();
				if (!g_Options->GetDirectWrite())
				{
					m_fileInfo->SetOutputInitialized(true);
				}
			}

			if (!outputInitialized && filename &&
				FileSystem::FileExists(BString<1024>("%s%c%s", m_fileInfo->GetNzbInfo()->GetDestDir(), PATH_SEPARATOR, filename)))
			{
				m_duplicate = true;
				return false;
			}
		}

		if (g_Options->GetDirectWrite())
		{
			Guard guard = m_fileInfo->GuardOutputFile();
			if (!m_fileInfo->GetOutputInitialized())
			{
				if (!CreateOutputFile(fileSize))
				{
					return false;
				}
				m_fileInfo->SetOutputInitialized(true);
			}
		}
	}

	// allocate cache buffer
	if (g_Options->GetArticleCache() > 0 && !g_Options->GetRawArticle() &&
		(!g_Options->GetDirectWrite() || m_format == Decoder::efYenc))
	{
		m_articleData = g_ArticleCache->Alloc(m_articleSize);

		while (!m_articleData.GetData() && g_ArticleCache->GetFlushing())
		{
			Util::Sleep(5);
			m_articleData = g_ArticleCache->Alloc(m_articleSize);
		}

		if (!m_articleData.GetData())
		{
			detail("Article cache is full, using disk for %s", *m_infoName);
		}
	}

	if (!m_articleData.GetData())
	{
		bool directWrite = (g_Options->GetDirectWrite() || m_fileInfo->GetForceDirectWrite()) && m_format == Decoder::efYenc;
		const char* outFilename = directWrite ? m_outputFilename : m_tempFilename;
		if (!m_outFile.Open(outFilename, directWrite ? DiskFile::omReadWrite : DiskFile::omWrite))
		{
			m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
				"Could not %s file %s: %s", directWrite ? "open" : "create", outFilename,
				*FileSystem::GetLastErrorMessage());
			return false;
		}
		SetWriteBuffer(m_outFile, m_articleInfo->GetSize());

		if (directWrite)
		{
			m_outFile.Seek(m_articleOffset);
		}
	}

	return true;
}

bool ArticleWriter::Write(char* buffer, int len)
{
	if (!g_Options->GetRawArticle())
	{
		m_articlePtr += len;
	}

	if (m_articlePtr > m_articleSize)
	{
		// An attempt to write beyond article border is detected.
		// That's an error condition (damaged article).
		// We return 'false' since this isn't a fatal disk error and
		// article size mismatch will be detected in decoder check anyway.
		return true;
	}

	if (!g_Options->GetRawArticle() && m_articleData.GetData())
	{
		memcpy(m_articleData.GetData() + m_articlePtr - len, buffer, len);
		return true;
	}

	if (g_Options->GetSkipWrite())
	{
		return true;
	}

	return m_outFile.Write(buffer, len) > 0;
}

void ArticleWriter::Finish(bool success)
{
	m_outFile.Close();

	if (!success)
	{
		FileSystem::DeleteFile(m_tempFilename);
		FileSystem::DeleteFile(m_resultFilename);
		return;
	}

	bool directWrite = (g_Options->GetDirectWrite() || m_fileInfo->GetForceDirectWrite()) && m_format == Decoder::efYenc;

	if (!g_Options->GetRawArticle())
	{
		if (!directWrite && !m_articleData.GetData())
		{
			if (!FileSystem::MoveFile(m_tempFilename, m_resultFilename))
			{
				m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
					"Could not rename file %s to %s: %s", *m_tempFilename, m_resultFilename,
					*FileSystem::GetLastErrorMessage());
			}
			FileSystem::DeleteFile(m_tempFilename);
		}

		if (m_articleData.GetData())
		{
			if (m_articleSize != m_articlePtr)
			{
				g_ArticleCache->Realloc(&m_articleData, m_articlePtr);
			}
			Guard contentGuard = g_ArticleCache->GuardContent();
			m_articleInfo->AttachSegment(std::make_unique<CachedSegmentData>(std::move(m_articleData)), m_articleOffset, m_articlePtr);
			m_fileInfo->SetCachedArticles(m_fileInfo->GetCachedArticles() + 1);
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
		if (!FileSystem::MoveFile(m_tempFilename, m_resultFilename))
		{
			m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
				"Could not move file %s to %s: %s", *m_tempFilename, m_resultFilename,
				*FileSystem::GetLastErrorMessage());
		}
	}
}

/* creates output file and subdirectores */
bool ArticleWriter::CreateOutputFile(int64 size)
{
	if (FileSystem::FileExists(m_outputFilename))
	{
		if (FileSystem::FileSize(m_outputFilename) == size)
		{
			// keep existing old file from previous program session
			return true;
		}
		// delete existing old file from previous program session
		FileSystem::DeleteFile(m_outputFilename);
	}

	// ensure the directory exist
	BString<1024> destDir;
	destDir.Set(m_outputFilename, (int)(FileSystem::BaseFileName(m_outputFilename) - m_outputFilename));
	CString errmsg;

	if (!FileSystem::ForceDirectories(destDir, errmsg))
	{
		m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
			"Could not create directory %s: %s", *destDir, *errmsg);
		return false;
	}

	if (!FileSystem::AllocateFile(m_outputFilename, size, g_Options->GetArticleCache() == 0, errmsg))
	{
		m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
			"Could not create file %s: %s", *m_outputFilename, *errmsg);
		return false;
	}

	return true;
}

void ArticleWriter::BuildOutputFilename()
{
	BString<1024> filename("%s%c%i.%03i", g_Options->GetTempDir(), PATH_SEPARATOR,
		m_fileInfo->GetId(), m_articleInfo->GetPartNumber());

	m_articleInfo->SetResultFilename(filename);
	m_tempFilename.Format("%s.tmp", *filename);

	if (g_Options->GetDirectWrite() || m_fileInfo->GetForceDirectWrite())
	{
		Guard guard = m_fileInfo->GuardOutputFile();

		if (m_fileInfo->GetOutputFilename())
		{
			filename = m_fileInfo->GetOutputFilename();
		}
		else
		{
			filename.Format("%s%c%i.out.tmp", m_fileInfo->GetNzbInfo()->GetDestDir(),
				PATH_SEPARATOR, m_fileInfo->GetId());
			m_fileInfo->SetOutputFilename(filename);
		}

		m_outputFilename = *filename;
	}
}

void ArticleWriter::CompleteFileParts()
{
	debug("Completing file parts");
	debug("ArticleFilename: %s", m_fileInfo->GetFilename());

	bool directWrite = (g_Options->GetDirectWrite() || m_fileInfo->GetForceDirectWrite()) && m_fileInfo->GetOutputInitialized();

	BString<1024> nzbName;
	BString<1024> nzbDestDir;
	BString<1024> filename;

	{
		GuardedDownloadQueue guard = DownloadQueue::Guard();
		nzbName = m_fileInfo->GetNzbInfo()->GetName();
		nzbDestDir = m_fileInfo->GetNzbInfo()->GetDestDir();
		filename = m_fileInfo->GetFilename();
	}

	BString<1024> infoFilename("%s%c%s", *nzbName, PATH_SEPARATOR, *filename);

	bool cached = m_fileInfo->GetCachedArticles() > 0;

	if (g_Options->GetRawArticle())
	{
		detail("Moving articles for %s", *infoFilename);
	}
	else if (directWrite && cached)
	{
		detail("Writing articles for %s", *infoFilename);
	}
	else if (directWrite)
	{
		detail("Checking articles for %s", *infoFilename);
	}
	else
	{
		detail("Joining articles for %s", *infoFilename);
	}

	// Ensure the DstDir is created
	CString errmsg;
	if (!FileSystem::ForceDirectories(nzbDestDir, errmsg))
	{
		m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
			"Could not create directory %s: %s", *nzbDestDir, *errmsg);
		return;
	}

	CString ofn;
	if (m_fileInfo->GetForceDirectWrite())
	{
		ofn.Format("%s%c%s", *nzbDestDir, PATH_SEPARATOR, *filename);
	}
	else
	{
		ofn = FileSystem::MakeUniqueFilename(nzbDestDir, *filename);
	}

	DiskFile outfile;
	BString<1024> tmpdestfile("%s.tmp", *ofn);

	if (!g_Options->GetRawArticle() && !directWrite)
	{
		FileSystem::DeleteFile(tmpdestfile);
		if (!outfile.Open(tmpdestfile, DiskFile::omWrite))
		{
			m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
				"Could not create file %s: %s", *tmpdestfile, *FileSystem::GetLastErrorMessage());
			return;
		}
	}
	else if (directWrite && cached)
	{
		if (!outfile.Open(m_outputFilename, DiskFile::omReadWrite))
		{
			m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
				"Could not open file %s: %s", *m_outputFilename, *FileSystem::GetLastErrorMessage());
			return;
		}
		tmpdestfile = *m_outputFilename;
	}
	else if (g_Options->GetRawArticle())
	{
		FileSystem::DeleteFile(tmpdestfile);
		if (!FileSystem::CreateDirectory(ofn))
		{
			m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
				"Could not create directory %s: %s", *ofn, *FileSystem::GetLastErrorMessage());
			return;
		}
	}

	if (outfile.Active())
	{
		SetWriteBuffer(outfile, 0);
	}

	uint32 crc = 0;

	{
		std::unique_ptr<ArticleCache::FlushGuard> flushGuard;
		if (cached)
		{
			flushGuard = std::make_unique<ArticleCache::FlushGuard>(g_ArticleCache->GuardFlush());
		}

		CharBuffer buffer;
		bool firstArticle = true;

		if (!g_Options->GetRawArticle() && !directWrite)
		{
			buffer.Reserve(1024 * 64);
		}

		for (ArticleInfo* pa : m_fileInfo->GetArticles())
		{
			if (pa->GetStatus() != ArticleInfo::aiFinished)
			{
				continue;
			}

			if (!g_Options->GetRawArticle() && !directWrite && pa->GetSegmentOffset() > -1 &&
				pa->GetSegmentOffset() > outfile.Position() && outfile.Position() > -1)
			{
				memset(buffer, 0, buffer.Size());
				if (!g_Options->GetSkipWrite())
				{
					while (pa->GetSegmentOffset() > outfile.Position() && outfile.Position() > -1 &&
						outfile.Write(buffer, std::min((int)(pa->GetSegmentOffset() - outfile.Position()), buffer.Size())));
				}
			}

			if (pa->GetSegmentContent())
			{
				if (!g_Options->GetSkipWrite())
				{
					outfile.Seek(pa->GetSegmentOffset());
					outfile.Write(pa->GetSegmentContent(), pa->GetSegmentSize());
				}
				pa->DiscardSegment();
			}
			else if (!g_Options->GetRawArticle() && !directWrite && !g_Options->GetSkipWrite())
			{
				DiskFile infile;
				if (pa->GetResultFilename() && infile.Open(pa->GetResultFilename(), DiskFile::omRead))
				{
					int cnt = buffer.Size();
					while (cnt == buffer.Size())
					{
						cnt = (int)infile.Read(buffer, buffer.Size());
						outfile.Write(buffer, cnt);
					}
					infile.Close();
				}
				else
				{
					m_fileInfo->SetFailedArticles(m_fileInfo->GetFailedArticles() + 1);
					m_fileInfo->SetSuccessArticles(m_fileInfo->GetSuccessArticles() - 1);
					m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
						"Could not find file %s for %s [%i/%i]",
						pa->GetResultFilename(), *infoFilename, pa->GetPartNumber(),
						(int)m_fileInfo->GetArticles()->size());
				}
			}
			else if (g_Options->GetRawArticle())
			{
				BString<1024> dstFileName("%s%c%03i", *ofn, PATH_SEPARATOR, pa->GetPartNumber());
				if (!FileSystem::MoveFile(pa->GetResultFilename(), dstFileName))
				{
					m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
						"Could not move file %s to %s: %s", pa->GetResultFilename(),
						*dstFileName, *FileSystem::GetLastErrorMessage());
				}
			}

			if (m_format == Decoder::efYenc)
			{
				crc = firstArticle ? pa->GetCrc() : Crc32::Combine(crc, pa->GetCrc(), pa->GetSegmentSize());
				firstArticle = false;
			}
		}

		buffer.Clear();
	}

	if (outfile.Active())
	{
		outfile.Close();
		if (!directWrite && !FileSystem::MoveFile(tmpdestfile, ofn))
		{
			m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
				"Could not move file %s to %s: %s", *tmpdestfile, *ofn,
				*FileSystem::GetLastErrorMessage());
		}
	}

	if (directWrite)
	{
		if (!FileSystem::SameFilename(m_outputFilename, ofn) &&
			!FileSystem::MoveFile(m_outputFilename, ofn))
		{
			m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
				"Could not move file %s to %s: %s", *m_outputFilename, *ofn,
				*FileSystem::GetLastErrorMessage());
		}

		// if destination directory was changed delete the old directory (if empty)
		int len = strlen(nzbDestDir);
		if (!(!strncmp(nzbDestDir, m_outputFilename, len) &&
			(m_outputFilename[len] == PATH_SEPARATOR || m_outputFilename[len] == ALT_PATH_SEPARATOR)))
		{
			debug("Checking old dir for: %s", *m_outputFilename);
			BString<1024> oldDestDir;
			oldDestDir.Set(m_outputFilename, (int)(FileSystem::BaseFileName(m_outputFilename) - m_outputFilename));
			if (FileSystem::DirEmpty(oldDestDir))
			{
				debug("Deleting old dir: %s", *oldDestDir);
				FileSystem::RemoveDirectory(oldDestDir);
			}
		}
	}

	if (!directWrite)
	{
		for (ArticleInfo* pa : m_fileInfo->GetArticles())
		{
			if (pa->GetResultFilename())
			{
				FileSystem::DeleteFile(pa->GetResultFilename());
			}
		}
	}

	if (m_fileInfo->GetTotalArticles() == m_fileInfo->GetSuccessArticles())
	{
		m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkInfo, "Successfully downloaded %s", *infoFilename);
	}
	else if (m_fileInfo->GetMissedArticles() + m_fileInfo->GetFailedArticles() > 0)
	{
		m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkWarning,
			"%i of %i article downloads failed for \"%s\"",
			m_fileInfo->GetMissedArticles() + m_fileInfo->GetFailedArticles(),
			m_fileInfo->GetTotalArticles(), *infoFilename);
	}
	else
	{
		m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkInfo, "Partially downloaded %s", *infoFilename);
	}

	{
		GuardedDownloadQueue guard = DownloadQueue::Guard();

		m_fileInfo->SetCrc(crc);
		m_fileInfo->SetOutputFilename(ofn);

		if (strcmp(m_fileInfo->GetFilename(), filename))
		{
			// file was renamed during completion, need to move the file
			ofn = FileSystem::MakeUniqueFilename(nzbDestDir, m_fileInfo->GetFilename());
			if (!FileSystem::MoveFile(m_fileInfo->GetOutputFilename(), ofn))
			{
				m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
					"Could not rename file %s to %s: %s", m_fileInfo->GetOutputFilename(),
					*ofn, *FileSystem::GetLastErrorMessage());
			}
			m_fileInfo->SetOutputFilename(ofn);
		}

		if (strcmp(m_fileInfo->GetNzbInfo()->GetDestDir(), nzbDestDir))
		{
			// destination directory was changed during completion, need to move the file
			MoveCompletedFiles(m_fileInfo->GetNzbInfo(), nzbDestDir);
		}
	}
}

void ArticleWriter::FlushCache()
{
	detail("Flushing cache for %s", *m_infoName);

	bool directWrite = g_Options->GetDirectWrite() && m_fileInfo->GetOutputInitialized();
	DiskFile outfile;
	bool needBufFile = false;
	int flushedArticles = 0;
	int64 flushedSize = 0;

	{
		ArticleCache::FlushGuard flushGuard = g_ArticleCache->GuardFlush();

		std::vector<ArticleInfo*> cachedArticles;

		{
			Guard contentGuard = g_ArticleCache->GuardContent();

			if (m_fileInfo->GetFlushLocked())
			{
				return;
			}

			m_fileInfo->SetFlushLocked(true);

			cachedArticles.reserve(m_fileInfo->GetArticles()->size());
			for (ArticleInfo* pa : m_fileInfo->GetArticles())
			{
				if (pa->GetSegmentContent())
				{
					cachedArticles.push_back(pa);
				}
			}
		}

		for (ArticleInfo* pa : cachedArticles)
		{
			if (m_fileInfo->GetDeleted() && !m_fileInfo->GetNzbInfo()->GetParking())
			{
				// the file was deleted during flushing: stop flushing immediately
				break;
			}

			if (directWrite && !outfile.Active())
			{
				if (!outfile.Open(m_fileInfo->GetOutputFilename(), DiskFile::omReadWrite))
				{
					m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
						"Could not open file %s: %s", m_fileInfo->GetOutputFilename(),
						*FileSystem::GetLastErrorMessage());
					// prevent multiple error messages
					pa->DiscardSegment();
					flushedArticles++;
					break;
				}
				needBufFile = true;
			}

			BString<1024> destFile;

			if (!directWrite)
			{
				destFile.Format("%s.tmp", pa->GetResultFilename());
				if (!outfile.Open(destFile, DiskFile::omWrite))
				{
					m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
						"Could not create file %s: %s", *destFile,
						*FileSystem::GetLastErrorMessage());
					// prevent multiple error messages
					pa->DiscardSegment();
					flushedArticles++;
					break;
				}
				needBufFile = true;
			}

			if (outfile.Active() && needBufFile)
			{
				SetWriteBuffer(outfile, 0);
				needBufFile = false;
			}

			if (directWrite)
			{
				outfile.Seek(pa->GetSegmentOffset());
			}

			if (!g_Options->GetSkipWrite())
			{
				outfile.Write(pa->GetSegmentContent(), pa->GetSegmentSize());
			}

			flushedSize += pa->GetSegmentSize();
			flushedArticles++;

			pa->DiscardSegment();

			if (!directWrite)
			{
				outfile.Close();

				if (!FileSystem::MoveFile(destFile, pa->GetResultFilename()))
				{
					m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
						"Could not rename file %s to %s: %s", *destFile, pa->GetResultFilename(),
						*FileSystem::GetLastErrorMessage());
				}
			}
		}

		outfile.Close();

		{
			Guard contentGuard = g_ArticleCache->GuardContent();
			m_fileInfo->SetCachedArticles(m_fileInfo->GetCachedArticles() - flushedArticles);
			m_fileInfo->SetFlushLocked(false);
		}
	}

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
	CString errmsg;
	if (!FileSystem::ForceDirectories(nzbInfo->GetDestDir(), errmsg))
	{
		nzbInfo->PrintMessage(Message::mkError, "Could not create directory %s: %s", nzbInfo->GetDestDir(), *errmsg);
		return false;
	}

	// move already downloaded files to new destination
	for (CompletedFile& completedFile : nzbInfo->GetCompletedFiles())
	{
		BString<1024> oldFileName("%s%c%s", oldDestDir, PATH_SEPARATOR, completedFile.GetFilename());
		BString<1024> newFileName("%s%c%s", nzbInfo->GetDestDir(), PATH_SEPARATOR, completedFile.GetFilename());

		// check if file was not moved already
		if (strcmp(oldFileName, newFileName))
		{
			// prevent overwriting of existing files
			newFileName = FileSystem::MakeUniqueFilename(nzbInfo->GetDestDir(), completedFile.GetFilename());

			detail("Moving file %s to %s", *oldFileName, *newFileName);
			if (!FileSystem::MoveFile(oldFileName, newFileName))
			{
				nzbInfo->PrintMessage(Message::mkError, "Could not move file %s to %s: %s",
					*oldFileName, *newFileName, *FileSystem::GetLastErrorMessage());
			}
		}
	}

	// delete old directory (if empty)
	if (FileSystem::DirEmpty(oldDestDir))
	{
		// check if there are pending writes into directory
		bool pendingWrites = false;
		for (FileInfo* fileInfo : nzbInfo->GetFileList())
		{
			if (!pendingWrites)
			{
				break;
			}

			if (fileInfo->GetActiveDownloads() > 0)
			{
				Guard guard = fileInfo->GuardOutputFile();
				pendingWrites = fileInfo->GetOutputInitialized() && !Util::EmptyStr(fileInfo->GetOutputFilename());
			}
			else
			{
				pendingWrites = fileInfo->GetOutputInitialized() && !Util::EmptyStr(fileInfo->GetOutputFilename());
			}
		}

		if (!pendingWrites)
		{
			FileSystem::RemoveDirectory(oldDestDir);
		}
	}

	return true;
}


CachedSegmentData ArticleCache::Alloc(int size)
{
	Guard guard(m_allocMutex);

	void* p = nullptr;

	if (m_allocated + size <= (size_t)g_Options->GetArticleCache() * 1024 * 1024)
	{
		p = malloc(size);
		if (p)
		{
			if (!m_allocated && g_Options->GetServerMode() && g_Options->GetContinuePartial())
			{
				g_DiskState->WriteCacheFlag();
			}
			if (!m_allocated)
			{
				// Resume Run(), the notification arrives later, after releasing m_allocMutex
				m_allocCond.NotifyAll();
			}
			m_allocated += size;
		}
	}

	return CachedSegmentData((char*)p, p ? size : 0);
}

bool ArticleCache::Realloc(CachedSegmentData* segment, int newSize)
{
	Guard guard(m_allocMutex);

	void* p = realloc(segment->m_data, newSize);
	if (p)
	{
		m_allocated += newSize - segment->m_size;
		segment->m_size = newSize;
		segment->m_data = (char*)p;
	}

	return p;
}

void ArticleCache::Free(CachedSegmentData* segment)
{
	if (segment->m_size)
	{
		free(segment->m_data);

		Guard guard(m_allocMutex);
		m_allocated -= segment->m_size;
		if (!m_allocated && g_Options->GetServerMode() && g_Options->GetContinuePartial())
		{
			g_DiskState->DeleteCacheFlag();
		}
	}
}

void ArticleCache::Run()
{
	// automatically flush the cache if it is filled to 90% (only in DirectWrite mode)
	size_t fillThreshold = (size_t)g_Options->GetArticleCache() * 1024 * 1024 / 100 * 90;

	int resetCounter = 0;
	bool justFlushed = false;
	while (!IsStopped() || m_allocated > 0)
	{
		if ((justFlushed || resetCounter >= 1000 || IsStopped() ||
			 (g_Options->GetDirectWrite() && m_allocated >= fillThreshold)) &&
			m_allocated > 0)
		{
			justFlushed = CheckFlush(m_allocated >= fillThreshold);
			resetCounter = 0;
		}
		else if (!m_allocated)
		{
			Guard guard(m_allocMutex);
			m_allocCond.Wait(m_allocMutex, [&]{ return IsStopped() || m_allocated > 0; });
			resetCounter = 0;
		}
		else
		{
			Util::Sleep(5);
			resetCounter += 5;
		}
	}
}

void ArticleCache::Stop()
{
	Thread::Stop();

	// Resume Run() to exit it
	Guard guard(m_allocMutex);
	m_allocCond.NotifyAll();
}

bool ArticleCache::CheckFlush(bool flushEverything)
{
	debug("Checking cache, Allocated: %i, FlushEverything: %i", (int)m_allocated, (int)flushEverything);

	BString<1024> infoName;

	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
		{
			if (m_fileInfo)
			{
				break;
			}

			for (FileInfo* fileInfo : nzbInfo->GetFileList())
			{
				if (fileInfo->GetCachedArticles() > 0 && (fileInfo->GetActiveDownloads() == 0 || flushEverything))
				{
					m_fileInfo = fileInfo;
					infoName.Format("%s%c%s", m_fileInfo->GetNzbInfo()->GetName(), PATH_SEPARATOR, m_fileInfo->GetFilename());
					break;
				}
			}
		}
	}

	if (m_fileInfo)
	{
		ArticleWriter articleWriter;
		articleWriter.SetFileInfo(m_fileInfo);
		articleWriter.SetInfoName(infoName);
		articleWriter.FlushCache();
		m_fileInfo = nullptr;
		return true;
	}

	debug("Checking cache... nothing to flush");

	return false;
}

ArticleCache::FlushGuard::FlushGuard(Mutex& mutex) : m_guard(&mutex)
{
	g_ArticleCache->m_flushing = true;
}

ArticleCache::FlushGuard::~FlushGuard()
{
	if (m_guard)
	{
		g_ArticleCache->m_flushing = false;
	}
}
