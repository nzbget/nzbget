/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2017-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
	char m_signature[sizeof(Par2::PACKET_HEADER)];
#endif
	int m_dataSize = 0;
	CString m_hash16k;
	CString m_parSetId;
	bool m_parFile = false;
};

#ifndef DISABLE_PARCHECK
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
	typedef std::vector<CString> ParFiles;

	DirectRenamer* m_owner;
	ParFiles m_parFiles;
	DirectRenamer::FileHashList m_parHashes;
	int m_nzbId;

	void LoadParFile(const char* parFile);
};

void DirectParLoader::StartLoader(DirectRenamer* owner, NzbInfo* nzbInfo)
{
	nzbInfo->PrintMessage(Message::mkInfo, "Directly checking renamed files for %s", nzbInfo->GetName());

	DirectParLoader* directParLoader = new DirectParLoader();
	directParLoader->m_owner = owner;
	directParLoader->m_nzbId = nzbInfo->GetId();

	for (CompletedFile& completedFile : nzbInfo->GetCompletedFiles())
	{
		if (completedFile.GetParFile())
		{
			directParLoader->m_parFiles.emplace_back(BString<1024>("%s%c%s",
				nzbInfo->GetDestDir(), PATH_SEPARATOR, completedFile.GetFilename()));
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

	nzbInfo->PrintMessage(Message::mkInfo, "Loaded par2-file %s for direct-rename", FileSystem::BaseFileName(parFile));

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
#endif

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
		fileInfo->SetHash16k(contentAnalyzer->GetHash16k());
		debug("file: %s; article-hash16k: %s", fileInfo->GetFilename(), fileInfo->GetHash16k());
	}

	fileInfo->GetNzbInfo()->PrintMessage(Message::mkDetail, "Detected %s %s", (contentAnalyzer->GetParFile() ? "par2-file" : "non-par2-file"), fileInfo->GetFilename());

	if (fileInfo->GetParFile() != contentAnalyzer->GetParFile())
	{
		debug("Changing par2-flag for %s", fileInfo->GetFilename());
		fileInfo->SetParFile(contentAnalyzer->GetParFile());

		int delta = fileInfo->GetParFile() ? 1 : -1;

		nzbInfo->SetParSize(nzbInfo->GetParSize() + fileInfo->GetSize() * delta);
		nzbInfo->SetParCurrentSuccessSize(nzbInfo->GetParCurrentSuccessSize() + fileInfo->GetSuccessSize() * delta);
		nzbInfo->SetParCurrentFailedSize(nzbInfo->GetParCurrentFailedSize() +
			fileInfo->GetFailedSize() * delta + fileInfo->GetMissedSize() * delta);
		nzbInfo->SetParFailedSize(nzbInfo->GetParFailedSize() + fileInfo->GetMissedSize() * delta);
		nzbInfo->SetRemainingParCount(nzbInfo->GetRemainingParCount() + 1 * delta);

		if (!fileInfo->GetParFile() && fileInfo->GetPaused())
		{
			fileInfo->GetNzbInfo()->PrintMessage(Message::mkInfo, "Resuming non-par2-file %s", fileInfo->GetFilename());
			fileInfo->SetPaused(false);
		}

		nzbInfo->SetChanged(true);
		downloadQueue->SaveChanged();
	}

	if (fileInfo->GetParFile())
	{
		fileInfo->SetParSetId(contentAnalyzer->GetParSetId());
		debug("file: %s; setid: %s", fileInfo->GetFilename(), fileInfo->GetParSetId());
	}

	CheckState(downloadQueue, nzbInfo);
}

void DirectRenamer::FileDownloaded(DownloadQueue* downloadQueue, FileInfo* fileInfo)
{
	CheckState(downloadQueue, fileInfo->GetNzbInfo());
}

void DirectRenamer::CheckState(DownloadQueue* downloadQueue, NzbInfo* nzbInfo)
{
#ifndef DISABLE_PARCHECK
	if (nzbInfo->GetDirectRenameStatus() > NzbInfo::tsRunning)
	{
		return;
	}

	// check if all first articles are successfully downloaded (1)
	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
		if (Util::EmptyStr(fileInfo->GetHash16k()) ||
			(fileInfo->GetParFile() && Util::EmptyStr(fileInfo->GetParSetId())))
		{
			return;
		}
	}

	// check if all first articles are successfully downloaded (2)
	for (CompletedFile& completedFile : nzbInfo->GetCompletedFiles())
	{
		if (Util::EmptyStr(completedFile.GetHash16k()) ||
			(completedFile.GetParFile() && Util::EmptyStr(completedFile.GetParSetId())))
		{
			return;
		}
	}

	if (!nzbInfo->GetWaitingPar())
	{
		// all first articles downloaded
		UnpausePars(nzbInfo);
		nzbInfo->SetWaitingPar(true);
		nzbInfo->SetChanged(true);
		downloadQueue->SaveChanged();
	}

	if (nzbInfo->GetWaitingPar() && !nzbInfo->GetLoadingPar())
	{
		// check if all par2-files scheduled for downloading already completed
		FileList::iterator pos = std::find_if(
			nzbInfo->GetFileList()->begin(), nzbInfo->GetFileList()->end(),
			[](std::unique_ptr<FileInfo>& fileInfo)
		{
			return fileInfo->GetExtraPriority();
		});

		if (pos == nzbInfo->GetFileList()->end())
		{
			// all wanted par2-files are downloaded
			nzbInfo->SetLoadingPar(true);
			DirectParLoader::StartLoader(this, nzbInfo);
			return;
		}
	}
#endif
}

// Unpause smallest par-files from each par-set
void DirectRenamer::UnpausePars(NzbInfo* nzbInfo)
{
	ParFileList parFiles;
	CollectPars(nzbInfo, &parFiles);

	std::vector<CString> parsets;

	// sort by size
	std::sort(parFiles.begin(), parFiles.end(),
		[nzbInfo](const ParFile& parFile1, const ParFile& parFile2)
		{
			FileInfo* fileInfo1 = nzbInfo->GetFileList()->Find(const_cast<ParFile&>(parFile1).GetId());
			FileInfo* fileInfo2 = nzbInfo->GetFileList()->Find(const_cast<ParFile&>(parFile2).GetId());
			return (!fileInfo1 && fileInfo2) ||
				(fileInfo1 && fileInfo2 && fileInfo1->GetSize() < fileInfo2->GetSize());
		});

	// 1. count already downloaded files
	for (ParFile& parFile : parFiles)
	{
		if (parFile.GetCompleted())
		{
			parsets.emplace_back(parFile.GetSetId());
		}
	}

	// 2. find smallest par-file from each par-set from not yet completely downloaded files
	for (ParFile& parFile : parFiles)
	{
		std::vector<CString>::iterator pos = std::find(parsets.begin(), parsets.end(), parFile.GetSetId());
		if (pos == parsets.end())
		{
			// this par-set is not yet downloaded
			parsets.emplace_back(parFile.GetSetId());

			FileInfo* fileInfo = nzbInfo->GetFileList()->Find(parFile.GetId());
			if (fileInfo)
			{
				nzbInfo->PrintMessage(Message::mkInfo, "Increasing priority for par2-file %s", fileInfo->GetFilename());
				fileInfo->SetPaused(false);
				fileInfo->SetExtraPriority(true);
			}
		}
	}
}

void DirectRenamer::CollectPars(NzbInfo* nzbInfo, ParFileList* parFiles)
{
	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
		if (fileInfo->GetParFile())
		{
			parFiles->emplace_back(fileInfo->GetId(), fileInfo->GetFilename(), fileInfo->GetParSetId(), false);
		}
	}

	for (CompletedFile& completedFile : nzbInfo->GetCompletedFiles())
	{
		if (completedFile.GetParFile())
		{
			parFiles->emplace_back(completedFile.GetId(), completedFile.GetFilename(), completedFile.GetParSetId(), true);
		}
	}
}

void DirectRenamer::RenameFiles(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, FileHashList* parHashes)
{
	int renamedCount = 0;

	bool renamePars = NeedRenamePars(nzbInfo);
	int vol = 1;

	// rename in-progress files
	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
		CString newName;
		if (fileInfo->GetParFile() && renamePars)
		{
			newName = BuildNewParName(fileInfo->GetFilename(), nzbInfo->GetDestDir(), fileInfo->GetParSetId(), vol);
		}
		else if (!fileInfo->GetParFile())
		{
			newName = BuildNewRegularName(fileInfo->GetFilename(), parHashes, fileInfo->GetHash16k());
		}

		if (newName)
		{
			bool written = fileInfo->GetOutputFilename() &&
				!Util::EndsWith(fileInfo->GetOutputFilename(), ".out.tmp", true);
			if (!written)
			{
				nzbInfo->PrintMessage(Message::mkInfo, "Renaming in-progress file %s to %s",
					fileInfo->GetFilename(), *newName);
				if (Util::EmptyStr(fileInfo->GetOrigname()))
				{
					fileInfo->SetOrigname(fileInfo->GetFilename());
				}
				fileInfo->SetFilename(newName);
				fileInfo->SetFilenameConfirmed(true);
				renamedCount++;
			}
			else if (RenameCompletedFile(nzbInfo, fileInfo->GetFilename(), newName))
			{
				if (Util::EmptyStr(fileInfo->GetOrigname()))
				{
					fileInfo->SetOrigname(fileInfo->GetFilename());
				}
				fileInfo->SetFilename(newName);
				fileInfo->SetFilenameConfirmed(true);
				renamedCount++;
			}
		}
	}

	// rename completed files
	for (CompletedFile& completedFile : nzbInfo->GetCompletedFiles())
	{
		CString newName;
		if (completedFile.GetParFile() && renamePars)
		{
			newName = BuildNewParName(completedFile.GetFilename(), nzbInfo->GetDestDir(), completedFile.GetParSetId(), vol);
		}
		else if (!completedFile.GetParFile())
		{
			newName = BuildNewRegularName(completedFile.GetFilename(), parHashes, completedFile.GetHash16k());
		}

		if (newName && RenameCompletedFile(nzbInfo, completedFile.GetFilename(), newName))
		{
			if (Util::EmptyStr(completedFile.GetOrigname()))
			{
				completedFile.SetOrigname(completedFile.GetFilename());
			}
			completedFile.SetFilename(newName);
			renamedCount++;
		}
	}

	if (renamedCount > 0)
	{
		nzbInfo->PrintMessage(Message::mkInfo, "Successfully renamed %i file(s) for %s", renamedCount, nzbInfo->GetName());
	}
	else
	{
		nzbInfo->PrintMessage(Message::mkInfo, "No renamed files found for %s", nzbInfo->GetName());
	}

	RenameCompleted(downloadQueue, nzbInfo);
}

CString DirectRenamer::BuildNewRegularName(const char* oldName, FileHashList* parHashes, const char* hash16k)
{
	if (Util::EmptyStr(hash16k))
	{
		return nullptr;
	}

	FileHashList::iterator pos = std::find_if(parHashes->begin(), parHashes->end(),
		[hash16k](FileHash& parHash)
	{
		return !strcmp(parHash.GetHash(), hash16k);
	});

	if (pos != parHashes->end())
	{
		FileHash& parHash = *pos;
		if (strcasecmp(oldName, parHash.GetFilename()))
		{
			return parHash.GetFilename();
		}
	}

	return nullptr;
}

CString DirectRenamer::BuildNewParName(const char* oldName, const char* destDir, const char* setId, int& vol)
{
	BString<1024> newName;
	BString<1024> destFileName;

	// trying to reuse file suffix
	const char* suffix = strstr(oldName, ".vol");
	const char* extension = suffix ? strrchr(suffix, '.') : nullptr;
	if (suffix && extension && !strcasecmp(extension, ".par2"))
	{
		newName.Format("%s%s", setId, suffix);
		destFileName.Format("%s%c%s", destDir, PATH_SEPARATOR, *newName);
	}

	while (destFileName.Empty() || FileSystem::FileExists(destFileName))
	{
		newName.Format("%s.vol%03i+01.PAR2", setId, vol);
		destFileName.Format("%s%c%s", destDir, PATH_SEPARATOR, *newName);
		vol++;
	}

	return *newName;
}

bool DirectRenamer::NeedRenamePars(NzbInfo* nzbInfo)
{
	// renaming is needed if par2-files from same par-set have different base names
	// or if any par2-file has non .par2-extension
	ParFileList parFiles;
	CollectPars(nzbInfo, &parFiles);

	for (ParFile& parFile : parFiles)
	{
		if (!Util::EndsWith(parFile.GetFilename(), ".par2", false))
		{
			return true;
		}

		for (ParFile& parFile2 : parFiles)
		{
			if (&parFile != &parFile2 && !strcmp(parFile.GetSetId(), parFile2.GetSetId()) &&
				!ParParser::SameParCollection(parFile.GetFilename(), parFile2.GetFilename(), false))
			{
					return true;
			}
		}
	}

	return false;
}

bool DirectRenamer::RenameCompletedFile(NzbInfo* nzbInfo, const char* oldName, const char* newName)
{
	BString<1024> oldFullFilename("%s%c%s", nzbInfo->GetDestDir(), PATH_SEPARATOR, oldName);
	BString<1024> newFullFilename("%s%c%s", nzbInfo->GetDestDir(), PATH_SEPARATOR, newName);
	nzbInfo->PrintMessage(Message::mkInfo, "Renaming completed file %s to %s", oldName, newName);
	if (!FileSystem::MoveFile(oldFullFilename, newFullFilename))
	{
		nzbInfo->PrintMessage(Message::mkError, "Could not rename completed file %s to %s: %s",
			*oldFullFilename, *newFullFilename, *FileSystem::GetLastErrorMessage());
		return false;
	}
	return true;
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
	if ((size_t)m_dataSize < sizeof(m_signature))
	{
		memcpy(m_signature + m_dataSize, buffer, std::min((size_t)len, sizeof(m_signature) - m_dataSize));
	}

	if ((size_t)m_dataSize >= sizeof(m_signature) && (*(Par2::MAGIC*)m_signature) == Par2::packet_magic)
	{
		m_parFile = true;
		m_parSetId = ((Par2::PACKET_HEADER*)m_signature)->setid.print().c_str();
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
