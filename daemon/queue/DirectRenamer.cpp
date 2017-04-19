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

#ifndef DISABLE_PARCHECK
#include "par2cmdline.h"
#include "par2fileformat.h"
#include "md5.h"
#endif

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
	static void StartLoader(NzbInfo* nzbInfo, FileInfo* fileInfo);
	virtual void Run();

private:
	CString m_parFilename;
	int m_nzbId;
};

void DirectParLoader::StartLoader(NzbInfo* nzbInfo, FileInfo* fileInfo)
{
	debug("Starting DirectParLoader for %s", fileInfo->GetFilename());
	DirectParLoader* directParLoader = new DirectParLoader();
	directParLoader->m_nzbId = nzbInfo->GetId();
	directParLoader->m_parFilename = BString<1024>("%s%c%s", nzbInfo->GetDestDir(), PATH_SEPARATOR, fileInfo->GetFilename());
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

	g_Options->SetTempPauseDownload(false);

	NzbInfo* nzbInfo = downloadQueue->GetQueue()->Find(m_nzbId);
	if (!nzbInfo)
	{
		// nzb isn't in queue anymore
		return;
	}

	nzbInfo->PrintMessage(Message::mkInfo, "Loaded par2-file %s for direct renaming",
		FileSystem::BaseFileName(m_parFilename));

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
		nzbInfo->PrintMessage(Message::mkDetail, "Found filename %s", filename.c_str());
		nzbInfo->GetRenameInfo()->GetParHashes()->emplace_back(filename.c_str(), hash.c_str());
	}
}


DirectRenamer::DirectRenamer()
{
	debug("Creating PrePostProcessor");

	DownloadQueue::Guard()->Attach(this);
}

void DirectRenamer::DownloadQueueUpdate(void* aspect)
{
	DownloadQueue::Aspect* queueAspect = (DownloadQueue::Aspect*)aspect;
	if (queueAspect->action == DownloadQueue::eaFileCompleted &&
		queueAspect->fileInfo->GetParFile() &&
		!queueAspect->nzbInfo->GetRenameInfo()->GetLoadingPar() &&
		queueAspect->nzbInfo->GetRenameInfo()->GetParHashes()->empty())
	{
		queueAspect->nzbInfo->PrintMessage(Message::mkDetail, "Loading par2-file %s for direct rename", queueAspect->fileInfo->GetFilename());
		g_Options->SetTempPauseDownload(true);
		queueAspect->nzbInfo->GetRenameInfo()->SetLoadingPar(true);
		DirectParLoader::StartLoader(queueAspect->nzbInfo, queueAspect->fileInfo);
	}
}


RenameContentAnalyzer::~RenameContentAnalyzer()
{
	Reset();
}

void RenameContentAnalyzer::Reset()
{
#ifndef DISABLE_PARCHECK
	delete (Par2::MD5Context*)m_md5Context;
#endif
	m_md5Context = nullptr;
	m_dataSize = 0;
}

void RenameContentAnalyzer::Append(const void* buffer, int len)
{
#ifndef DISABLE_PARCHECK
	if (!m_md5Context)
	{
		m_md5Context = new Par2::MD5Context();
	}

	if (m_dataSize == 0 && len >= sizeof(Par2::packet_magic) &&
		(*(Par2::MAGIC*)buffer) == Par2::packet_magic)
	{
		m_parFile = true;
	}

	int rem16kSize = std::min(len, 16 * 1024 - m_dataSize);
	if (rem16kSize > 0)
	{
		((Par2::MD5Context*)m_md5Context)->Update(buffer, rem16kSize);
	}

	m_dataSize += len;
#endif
}

// Must be called with locked DownloadQueue
void RenameContentAnalyzer::Finish()
{
#ifndef DISABLE_PARCHECK
	Par2::MD5Hash hash;
	((Par2::MD5Context*)m_md5Context)->Final(hash);

	m_hash16k = hash.print().c_str();
#endif
}
