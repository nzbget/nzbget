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
#include "ParParser.h"

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
	const char* GetParSetId() { return m_parSetId; }

private:
#ifndef DISABLE_PARCHECK
	Par2::MD5Context m_md5Context;
#endif
	int m_dataSize = 0;
	CString m_hash16k;
	CString m_parSetId;
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
	static void StartLoader(DirectRenamer* owner, NzbInfo* nzbInfo);
	virtual void Run();

private:
	DirectRenamer* m_owner;
	NameList m_parFiles;
	RenameInfo::FileHashList m_parHashes;
	int m_nzbId;

	void LoadParFile(const char* parFile);
};

void DirectParLoader::StartLoader(DirectRenamer* owner, NzbInfo* nzbInfo)
{
	debug("Starting DirectParLoader for %s", nzbInfo->GetName());
	DirectParLoader* directParLoader = new DirectParLoader();
	directParLoader->m_owner = owner;
	directParLoader->m_nzbId = nzbInfo->GetId();

	for (RenameInfo::ParFile& parFile : nzbInfo->GetRenameInfo()->GetParFiles())
	{
		if (parFile.GetCompleted())
		{
			directParLoader->m_parFiles.emplace_back(BString<1024>("%s%c%s",
				nzbInfo->GetDestDir(), PATH_SEPARATOR, parFile.GetFilename()));
		}
	}

	directParLoader->SetAutoDestroy(true);
	directParLoader->Start();
}

void DirectParLoader::Run()
{
	debug("Started DirectParLoader");

	for (CString& parFile : m_parFiles)
	{
		LoadParFile(parFile);
	}

	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	NzbInfo* nzbInfo = downloadQueue->GetQueue()->Find(m_nzbId);
	if (nzbInfo)
	{
		// nzb is still in queue
		m_owner->RenameFiles(downloadQueue, nzbInfo, &m_parHashes);
	}
}

void DirectParLoader::LoadParFile(const char* parFile)
{
	DirectParRepairer repairer;

	if (!repairer.LoadPacketsFromFile(parFile))
	{
		warn("Could not load par2-file %s", parFile);
		return;
	}

	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	NzbInfo* nzbInfo = downloadQueue->GetQueue()->Find(m_nzbId);
	if (!nzbInfo)
	{
		// nzb isn't in queue anymore
		return;
	}

	nzbInfo->PrintMessage(Message::mkInfo, "Loaded par2-file %s for direct renaming", FileSystem::BaseFileName(parFile));

	for (std::pair<const Par2::MD5Hash, Par2::Par2RepairerSourceFile*>& entry : repairer.sourcefilemap)
	{
		if (IsStopped())
		{
			break;
		}

		Par2::Par2RepairerSourceFile* sourceFile = entry.second;
		if (!sourceFile || !sourceFile->GetDescriptionPacket())
		{
			nzbInfo->PrintMessage(Message::mkWarning, "Damaged par2-file detected: %s", FileSystem::BaseFileName(parFile));
			return;
		}
		std::string filename = Par2::DiskFile::TranslateFilename(sourceFile->GetDescriptionPacket()->FileName());
		std::string hash = sourceFile->GetDescriptionPacket()->Hash16k().print();

		debug("file: %s, hash-16k: %s", filename.c_str(), hash.c_str());
		m_parHashes.emplace_back(filename.c_str(), hash.c_str());
	}
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
	if (!contentAnalyzer->GetParFile() &&
		(articleInfo->GetSize() >= 16 * 1024 || fileInfo->GetArticles()->size() == 1))
	{
		nzbInfo->GetRenameInfo()->GetArticleHashes()->emplace_back(fileInfo->GetFilename(), contentAnalyzer->GetHash16k());
		debug("file: %s; article-hash16k: %s", fileInfo->GetFilename(), contentAnalyzer->GetHash16k());
	}

	detail("Detected %s %s", (contentAnalyzer->GetParFile() ? "par2-file" : "non-par2-file"), fileInfo->GetFilename());

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

	if (fileInfo->GetParFile())
	{
		nzbInfo->GetRenameInfo()->GetParFiles()->emplace_back(fileInfo->GetId(),
			fileInfo->GetFilename(), contentAnalyzer->GetParSetId());
		debug("file: %s; setid: %s", fileInfo->GetFilename(), contentAnalyzer->GetParSetId());
	}

	CheckState(nzbInfo);
}

void DirectRenamer::FileDownloaded(DownloadQueue* downloadQueue, FileInfo* fileInfo)
{
	if (fileInfo->GetParFile())
	{
		RenameInfo::ParFileList::iterator pos = std::find_if(
			fileInfo->GetNzbInfo()->GetRenameInfo()->GetParFiles()->begin(),
			fileInfo->GetNzbInfo()->GetRenameInfo()->GetParFiles()->end(),
			[id = fileInfo->GetId()](RenameInfo::ParFile& parFile)
			{
				return parFile.GetId() == id;
			});
		if (pos != fileInfo->GetNzbInfo()->GetRenameInfo()->GetParFiles()->end())
		{
			(*pos).SetCompleted(true);
		}
	}

	CheckState(fileInfo->GetNzbInfo());
}

void DirectRenamer::CheckState(NzbInfo* nzbInfo)
{
	if (nzbInfo->GetRenameInfo()->GetArticleHashes()->size() +
		nzbInfo->GetRenameInfo()->GetParFiles()->size() == nzbInfo->GetFileCount() &&
		!nzbInfo->GetRenameInfo()->GetWaitingPar())
	{
		// all first articles downloaded
		UnpausePars(nzbInfo);
		nzbInfo->GetRenameInfo()->SetWaitingPar(true);
	}

	if (nzbInfo->GetRenameInfo()->GetWaitingPar() &&
		!nzbInfo->GetRenameInfo()->GetLoadingPar())
	{
		// check if all par2-files scheduled for downloading already completed
		RenameInfo::ParFileList::iterator pos = std::find_if(
			nzbInfo->GetRenameInfo()->GetParFiles()->begin(),
			nzbInfo->GetRenameInfo()->GetParFiles()->end(),
			[](RenameInfo::ParFile& parFile)
			{
				return parFile.GetWanted() && !parFile.GetCompleted();
			});
		if (pos == nzbInfo->GetRenameInfo()->GetParFiles()->end())
		{
			// all wanted par2-files are downloaded
			detail("Loading par2-files for direct renaming");
			nzbInfo->GetRenameInfo()->SetLoadingPar(true);
			DirectParLoader::StartLoader(this, nzbInfo);
			return;
		}
	}
}

// Unpause smallest par-files from each par-set
void DirectRenamer::UnpausePars(NzbInfo* nzbInfo)
{
	std::vector<CString> parsets;

	// sort by size
	std::sort(
		nzbInfo->GetRenameInfo()->GetParFiles()->begin(),
		nzbInfo->GetRenameInfo()->GetParFiles()->end(),
		[nzbInfo](const RenameInfo::ParFile& parFile1, const RenameInfo::ParFile& parFile2)
		{
			FileInfo* fileInfo1 = nzbInfo->GetFileList()->Find(const_cast<RenameInfo::ParFile&>(parFile1).GetId());
			FileInfo* fileInfo2 = nzbInfo->GetFileList()->Find(const_cast<RenameInfo::ParFile&>(parFile2).GetId());
			return (!fileInfo1 && fileInfo2) ||
				(fileInfo1 && fileInfo2 && fileInfo1->GetSize() < fileInfo2->GetSize());
		});

	// 1. count already downloaded files
	for (RenameInfo::ParFile& parFile : nzbInfo->GetRenameInfo()->GetParFiles())
	{
		if (parFile.GetCompleted())
		{
			parsets.emplace_back(parFile.GetSetId());
			parFile.SetWanted(true);
		}
	}

	// 2. find smallest par-file from each par-set from not yet completely downloaded files
	for (RenameInfo::ParFile& parFile : nzbInfo->GetRenameInfo()->GetParFiles())
	{
		std::vector<CString>::iterator pos = std::find(parsets.begin(), parsets.end(), parFile.GetSetId());
		if (pos == parsets.end())
		{
			// this par-set is not yet downloaded
			parsets.emplace_back(parFile.GetSetId());

			FileInfo* fileInfo = nzbInfo->GetFileList()->Find(parFile.GetId());
			if (fileInfo)
			{
				nzbInfo->PrintMessage(Message::mkDetail, "Increasing priority for par2-file %s", fileInfo->GetFilename());
				fileInfo->SetPaused(false);
				fileInfo->SetExtraPriority(true);
				parFile.SetWanted(true);
			}
		}
	}
}

void DirectRenamer::RenameFiles(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, RenameInfo::FileHashList* parHashes)
{
	// rename regular files
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

	// rename par2-files
	if (NeedRenamePars(nzbInfo))
	{
		int num = 1;
		for (RenameInfo::ParFile& parFile : nzbInfo->GetRenameInfo()->GetParFiles())
		{
			BString<1024> newName;
			BString<1024> destFileName;

			// trying to reuse file suffix
			const char* suffix = strstr(parFile.GetFilename(), ".vol");
			const char* extension = suffix ? strrchr(suffix, '.') : nullptr;
			if (suffix && extension && !strcasecmp(extension, ".par2"))
			{
				newName.Format("%s%s", parFile.GetSetId(), suffix);
				destFileName.Format("%s%c%s", nzbInfo->GetDestDir(), PATH_SEPARATOR, *newName);
			}

			while (destFileName.Empty() || FileSystem::FileExists(destFileName))
			{
				newName.Format("%s.vol%03i+01.PAR2", parFile.GetSetId(), num);
				destFileName.Format("%s%c%s", nzbInfo->GetDestDir(), PATH_SEPARATOR, *newName);
				num++;
			}
			RenameFile(nzbInfo, parFile.GetFilename(), newName);
		}
	}

	RenameCompleted(downloadQueue, nzbInfo);
}

bool DirectRenamer::NeedRenamePars(NzbInfo* nzbInfo)
{
	// renaming is needed if par2-files from same par-set have different base names

	for (RenameInfo::ParFile& parFile : nzbInfo->GetRenameInfo()->GetParFiles())
	{
		int baseLen;
		ParParser::ParseParFilename(parFile.GetFilename(), false, &baseLen, nullptr);
		BString<1024> basename;
		basename.Set(parFile.GetFilename(), baseLen);

		for (RenameInfo::ParFile& parFile2 : nzbInfo->GetRenameInfo()->GetParFiles())
		{
			ParParser::ParseParFilename(parFile.GetFilename(), false, &baseLen, nullptr);
			BString<1024> basename2;
			basename2.Set(parFile2.GetFilename(), baseLen);

			if (&parFile != &parFile2 && strcmp(basename, basename2))
			{
				return true;
			}
		}
	}

	return false;
}

void DirectRenamer::RenameFile(NzbInfo* nzbInfo, const char* oldName, const char* newName)
{
	BString<1024> newFullFilename("%s%c%s", nzbInfo->GetDestDir(), PATH_SEPARATOR, newName);
	if (FileSystem::FileExists(newFullFilename))
	{
		nzbInfo->PrintMessage(Message::mkWarning,
			"Could not rename file %s to %s: destination file already exists",
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
		if (len >= sizeof(Par2::PACKET_HEADER))
		{
			m_parSetId = ((Par2::PACKET_HEADER*)buffer)->setid.print().c_str();
		}
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
