/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "DirectRenamer.h"
#include "Options.h"
#include "FileSystem.h"

#ifndef DISABLE_PARCHECK
#include "par2cmdline.h"
#include "par2fileformat.h"
#include "md5.h"
#endif

class RenameContentAnalyzer : public ArticleContentAnalyzer
{
public:
	virtual void Reset();
	virtual void Append(const void* buffer, int len);
	void Finish();
	const char* GetHash16k() { return m_hash16k; }
	bool GetParFile() { return m_parFile; }

private:
#ifndef DISABLE_PARCHECK
	Par2::MD5Context m_md5Context;
#endif
	int m_dataSize = 0;
	CString m_hash16k;
	bool m_parFile = false;
};

class DirectParRepairer : public Par2::Par2Repairer
{
public:
	DirectParRepairer() : Par2::Par2Repairer(m_nout, m_nout) {};
	friend class DirectParLoader;

private:
	class NullStreamBuf : public std::streambuf {};
	NullStreamBuf m_nullbuf;
	std::ostream m_nout{&m_nullbuf};
};

class DirectParLoader : public Thread
{
public:
	static void StartLoader(DirectRenamer* owner, NzbInfo* nzbInfo, const char* parFilename);
	virtual void Run();

private:
	DirectRenamer* m_owner;
	CString m_parFilename;
	int m_nzbId;
};

void DirectParLoader::StartLoader(DirectRenamer* owner, NzbInfo* nzbInfo, const char* parFilename)
{
	debug("Starting DirectParLoader for %s", parFilename);
	DirectParLoader* directParLoader = new DirectParLoader();
	directParLoader->m_owner = owner;
	directParLoader->m_nzbId = nzbInfo->GetId();
	directParLoader->m_parFilename = BString<1024>("%s%c%s", nzbInfo->GetDestDir(), PATH_SEPARATOR, parFilename);
	directParLoader->SetAutoDestroy(true);
	directParLoader->Start();
}

void DirectParLoader::Run()
{
	debug("Started DirectParLoader for par2-file %s", *m_parFilename);

	DirectParRepairer repairer;

	if (!repairer.LoadPacketsFromFile(*m_parFilename))
	{
		warn("Could not load par2-file %s", *m_parFilename);
		return;
	}

	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	NzbInfo* nzbInfo = downloadQueue->GetQueue()->Find(m_nzbId);
	if (!nzbInfo)
	{
		// nzb isn't in queue anymore
		return;
	}

	nzbInfo->PrintMessage(Message::mkInfo, "Loaded par2-file %s for direct renaming",
		FileSystem::BaseFileName(m_parFilename));

	RenameInfo::FileHashList parHashes;

	for (std::pair<const Par2::MD5Hash, Par2::Par2RepairerSourceFile*>& entry : repairer.sourcefilemap)
	{
		if (IsStopped())
		{
			break;
		}

		Par2::Par2RepairerSourceFile* sourceFile = entry.second;
		if (!sourceFile || !sourceFile->GetDescriptionPacket())
		{
			nzbInfo->PrintMessage(Message::mkWarning, "Damaged par2-file detected: %s",
				FileSystem::BaseFileName(m_parFilename));
			return;
		}
		std::string filename = Par2::DiskFile::TranslateFilename(sourceFile->GetDescriptionPacket()->FileName());
		std::string hash = sourceFile->GetDescriptionPacket()->Hash16k().print();

		debug("file: %s, hash-16k: %s", filename.c_str(), hash.c_str());
		parHashes.emplace_back(filename.c_str(), hash.c_str());
	}

	m_owner->RenameFiles(downloadQueue, nzbInfo, &parHashes);
}


std::unique_ptr<ArticleContentAnalyzer> DirectRenamer::MakeArticleContentAnalyzer()
{
	return std::make_unique<RenameContentAnalyzer>();
}

void DirectRenamer::ArticleDownloaded(DownloadQueue* downloadQueue, FileInfo* fileInfo,
	ArticleInfo* articleInfo, ArticleContentAnalyzer* articleContentAnalyzer)
{
	debug("Applying analyzer data %s for ", fileInfo->GetFilename());

	RenameContentAnalyzer* contentAnalyzer = (RenameContentAnalyzer*)articleContentAnalyzer;
	contentAnalyzer->Finish();

	NzbInfo* nzbInfo = fileInfo->GetNzbInfo();

	// we don't support analyzing of files split into articles smaller than 16KB
	if (articleInfo->GetSize() >= 16 * 1024 || fileInfo->GetArticles()->size() == 1)
	{
		nzbInfo->GetRenameInfo()->GetArticleHashes()->emplace_back(fileInfo->GetFilename(), contentAnalyzer->GetHash16k());
		debug("file: %s; article-hash16k: %s", fileInfo->GetFilename(), contentAnalyzer->GetHash16k());
	}

	nzbInfo->PrintMessage(Message::mkDetail, "Detected %s %s",
		(contentAnalyzer->GetParFile() ? "par2-file" : "non-par2-file"), fileInfo->GetFilename());

	if (fileInfo->GetParFile() != contentAnalyzer->GetParFile())
	{
		debug("Changing par2-flag for %s", fileInfo->GetFilename());
		fileInfo->SetParFile(contentAnalyzer->GetParFile());

		int delta = fileInfo->GetParFile() ? 1 : -1;

		nzbInfo->SetParSize(nzbInfo->GetParSize() + fileInfo->GetSize() * delta);
		nzbInfo->SetParCurrentSuccessSize(nzbInfo->GetParCurrentSuccessSize() + fileInfo->GetSuccessSize() * delta);
		nzbInfo->SetParCurrentFailedSize(nzbInfo->GetParCurrentFailedSize() +
			fileInfo->GetFailedSize() * delta + fileInfo->GetMissedSize() * delta);
		nzbInfo->SetRemainingParCount(nzbInfo->GetRemainingParCount() + 1 * delta);

		downloadQueue->Save();
	}

	if (fileInfo->GetParFile() && !nzbInfo->GetRenameInfo()->GetWaitingPar())
	{
		nzbInfo->PrintMessage(Message::mkDetail, "Increasing priority for par2-file %s", fileInfo->GetFilename());
		fileInfo->SetPaused(false);
		fileInfo->SetExtraPriority(true);
		nzbInfo->GetRenameInfo()->SetWaitingPar(true);
	}

	CheckState(nzbInfo);
}

void DirectRenamer::FileDownloaded(DownloadQueue* downloadQueue, FileInfo* fileInfo)
{
	if (fileInfo->GetParFile())
	{
		fileInfo->GetNzbInfo()->GetRenameInfo()->GetParFiles()->emplace_back(fileInfo->GetFilename());
	}

	CheckState(fileInfo->GetNzbInfo());
}

void DirectRenamer::CheckState(NzbInfo* nzbInfo)
{
	if (!nzbInfo->GetRenameInfo()->GetLoadingPar() &&
		nzbInfo->GetRenameInfo()->GetArticleHashes()->size() == nzbInfo->GetFileCount() &&
		!nzbInfo->GetRenameInfo()->GetParFiles()->empty())
	{
		const char* parFilename = nzbInfo->GetRenameInfo()->GetParFiles()->at(0);
		nzbInfo->PrintMessage(Message::mkDetail, "Loading par2-file %s for direct renaming", parFilename);
		nzbInfo->GetRenameInfo()->SetLoadingPar(true);
		DirectParLoader::StartLoader(this, nzbInfo, parFilename);
	}
}

void DirectRenamer::RenameFiles(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, RenameInfo::FileHashList* parHashes)
{
	for (RenameInfo::FileHash& parHash : parHashes)
	{
		RenameInfo::FileHashList::iterator pos = std::find_if(
			nzbInfo->GetRenameInfo()->GetArticleHashes()->begin(),
			nzbInfo->GetRenameInfo()->GetArticleHashes()->end(),
				[&parHash](RenameInfo::FileHash& articleHash)
				{
					return !strcmp(parHash.GetHash(), articleHash.GetHash());
				});
		if (pos != nzbInfo->GetRenameInfo()->GetArticleHashes()->end())
		{
			RenameInfo::FileHash& articleHash = *pos;
			if (strcasecmp(articleHash.GetFilename(), parHash.GetFilename()))
			{
				RenameFile(nzbInfo, articleHash.GetFilename(), parHash.GetFilename());
			}
			nzbInfo->GetRenameInfo()->GetArticleHashes()->erase(pos);
		}
	}

	RenameCompleted(downloadQueue, nzbInfo);
}

void DirectRenamer::RenameFile(NzbInfo* nzbInfo, const char* oldName, const char* newName)
{
	BString<1024> newFullFilename("%s%c%s", nzbInfo->GetDestDir(), PATH_SEPARATOR, newName);
	if (FileSystem::FileExists(newFullFilename))
	{
		nzbInfo->PrintMessage(Message::mkWarning,
			"Could not rename file %s to %s: destination file already exist",
			oldName, newName);
		return;
	}

	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
		if (!strcmp(fileInfo->GetFilename(), oldName))
		{
			nzbInfo->PrintMessage(Message::mkInfo, "Renaming in-progress file %s to %s", oldName, newName);
			fileInfo->SetFilename(newName);
			return;
		}
	}

	for (CompletedFile& completedFile : nzbInfo->GetCompletedFiles())
	{
		if (!strcmp(completedFile.GetFileName(), oldName))
		{
			BString<1024> oldFullFilename("%s%c%s", nzbInfo->GetDestDir(), PATH_SEPARATOR, oldName);
			nzbInfo->PrintMessage(Message::mkInfo, "Renaming completed file %s to %s", oldName, newName);
			if (!FileSystem::MoveFile(oldFullFilename, newFullFilename))
			{
				nzbInfo->PrintMessage(Message::mkError, "Could not rename completed file %s to %s: %s",
					oldName, newName, *FileSystem::GetLastErrorMessage());
				return;
			}
			completedFile.SetFileName(newName);
			return;
		}
	}
}


void RenameContentAnalyzer::Reset()
{
#ifndef DISABLE_PARCHECK
	m_md5Context.Reset();
#endif
	m_dataSize = 0;
}

void RenameContentAnalyzer::Append(const void* buffer, int len)
{
#ifndef DISABLE_PARCHECK
	if (m_dataSize == 0 && len >= sizeof(Par2::packet_magic) &&
		(*(Par2::MAGIC*)buffer) == Par2::packet_magic)
	{
		m_parFile = true;
	}

	int rem16kSize = std::min(len, 16 * 1024 - m_dataSize);
	if (rem16kSize > 0)
	{
		m_md5Context.Update(buffer, rem16kSize);
	}

	m_dataSize += len;
#endif
}

// Must be called with locked DownloadQueue
void RenameContentAnalyzer::Finish()
{
#ifndef DISABLE_PARCHECK
	Par2::MD5Hash hash;
	m_md5Context.Final(hash);

	m_hash16k = hash.print().c_str();
#endif
}
