/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "ParCoordinator.h"
#include "DupeCoordinator.h"
#include "ParParser.h"
#include "Options.h"
#include "DiskState.h"
#include "Log.h"
#include "FileSystem.h"

#ifndef DISABLE_PARCHECK
bool ParCoordinator::PostParChecker::RequestMorePars(int blockNeeded, int* blockFound)
{
	return m_owner->RequestMorePars(m_postInfo->GetNzbInfo(), GetParFilename(), blockNeeded, blockFound);
}

void ParCoordinator::PostParChecker::UpdateProgress()
{
	m_owner->UpdateParCheckProgress();
}

void ParCoordinator::PostParChecker::PrintMessage(Message::EKind kind, const char* format, ...)
{
	char text[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(text, 1024, format, args);
	va_end(args);
	text[1024-1] = '\0';

	m_postInfo->GetNzbInfo()->AddMessage(kind, text);
}

void ParCoordinator::PostParChecker::RegisterParredFile(const char* filename)
{
	m_postInfo->GetParredFiles()->push_back(filename);
}

bool ParCoordinator::PostParChecker::IsParredFile(const char* filename)
{
	for (CString& parredFile : m_postInfo->GetParredFiles())
	{
		if (!strcasecmp(parredFile, filename))
		{
			return true;
		}
	}
	return false;
}

ParChecker::EFileStatus ParCoordinator::PostParChecker::FindFileCrc(const char* filename,
	uint32* crc, SegmentList* segments)
{
	CompletedFile* completedFile = nullptr;

	for (CompletedFile& completedFile2 : m_postInfo->GetNzbInfo()->GetCompletedFiles())
	{
		if (!strcasecmp(completedFile2.GetFileName(), filename))
		{
			completedFile = &completedFile2;
			break;
		}
	}
	if (!completedFile)
	{
		return ParChecker::fsUnknown;
	}

	debug("Found completed file: %s, CRC: %.8x, Status: %i", FileSystem::BaseFileName(completedFile->GetFileName()), completedFile->GetCrc(), (int)completedFile->GetStatus());

	*crc = completedFile->GetCrc();

	if (completedFile->GetStatus() == CompletedFile::cfPartial && completedFile->GetId() > 0 &&
		!m_postInfo->GetNzbInfo()->GetReprocess())
	{
		FileInfo tmpFileInfo(completedFile->GetId());

		if (!g_DiskState->LoadFileState(&tmpFileInfo, nullptr, true))
		{
			return ParChecker::fsUnknown;
		}

		for (ArticleInfo* pa : tmpFileInfo.GetArticles())
		{
			segments->emplace_back(pa->GetStatus() == ArticleInfo::aiFinished,
				pa->GetSegmentOffset(), pa->GetSegmentSize(), pa->GetCrc());
		}
	}

	return completedFile->GetStatus() == CompletedFile::cfSuccess ? ParChecker::fsSuccess :
		completedFile->GetStatus() == CompletedFile::cfFailure &&
			!m_postInfo->GetNzbInfo()->GetReprocess() ? ParChecker::fsFailure :
		completedFile->GetStatus() == CompletedFile::cfPartial && segments->size() > 0 &&
			!m_postInfo->GetNzbInfo()->GetReprocess()? ParChecker::fsPartial :
		ParChecker::fsUnknown;
}

void ParCoordinator::PostParChecker::RequestDupeSources(DupeSourceList* dupeSourceList)
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	RawNzbList dupeList = g_DupeCoordinator->ListHistoryDupes(downloadQueue, m_postInfo->GetNzbInfo());

	if (!dupeList.empty())
	{
		PostDupeMatcher dupeMatcher(m_postInfo);
		PrintMessage(Message::mkInfo, "Checking %s for dupe scan usability", m_postInfo->GetNzbInfo()->GetName());
		bool sizeComparisonPossible = dupeMatcher.Prepare();
		for (NzbInfo* dupeNzbInfo : dupeList)
		{
			if (sizeComparisonPossible)
			{
				PrintMessage(Message::mkInfo, "Checking %s for dupe scan usability", FileSystem::BaseFileName(dupeNzbInfo->GetDestDir()));
			}
			bool useDupe = !sizeComparisonPossible || dupeMatcher.MatchDupeContent(dupeNzbInfo->GetDestDir());
			if (useDupe)
			{
				PrintMessage(Message::mkInfo, "Adding %s to dupe scan sources", FileSystem::BaseFileName(dupeNzbInfo->GetDestDir()));
				dupeSourceList->emplace_back(dupeNzbInfo->GetId(), dupeNzbInfo->GetDestDir());
			}
		}
		if (dupeSourceList->empty())
		{
			PrintMessage(Message::mkInfo, "No usable dupe scan sources found");
		}
	}
}

void ParCoordinator::PostParChecker::StatDupeSources(DupeSourceList* dupeSourceList)
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	int totalExtraParBlocks = 0;
	for (DupeSource& dupeSource : dupeSourceList)
	{
		if (dupeSource.GetUsedBlocks() > 0)
		{
			for (HistoryInfo* historyInfo : downloadQueue->GetHistory())
			{
				if (historyInfo->GetKind() == HistoryInfo::hkNzb &&
					historyInfo->GetNzbInfo()->GetId() == dupeSource.GetId())
				{
					historyInfo->GetNzbInfo()->SetExtraParBlocks(historyInfo->GetNzbInfo()->GetExtraParBlocks() - dupeSource.GetUsedBlocks());
				}
			}
		}
		totalExtraParBlocks += dupeSource.GetUsedBlocks();
	}

	m_postInfo->GetNzbInfo()->SetExtraParBlocks(m_postInfo->GetNzbInfo()->GetExtraParBlocks() + totalExtraParBlocks);
}

void ParCoordinator::PostParRenamer::UpdateProgress()
{
	m_owner->UpdateParRenameProgress();
}

void ParCoordinator::PostParRenamer::PrintMessage(Message::EKind kind, const char* format, ...)
{
	char text[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(text, 1024, format, args);
	va_end(args);
	text[1024-1] = '\0';

	m_postInfo->GetNzbInfo()->AddMessage(kind, text);
}

void ParCoordinator::PostParRenamer::RegisterParredFile(const char* filename)
{
	m_postInfo->GetParredFiles()->push_back(filename);
}

/**
 *  Update file name in the CompletedFiles-list of NZBInfo
 */
void ParCoordinator::PostParRenamer::RegisterRenamedFile(const char* oldFilename, const char* newFileName)
{
	for (CompletedFile& completedFile : m_postInfo->GetNzbInfo()->GetCompletedFiles())
	{
		if (!strcasecmp(completedFile.GetFileName(), oldFilename))
		{
			completedFile.SetFileName(newFileName);
			break;
		}
	}
}

void ParCoordinator::PostDupeMatcher::PrintMessage(Message::EKind kind, const char* format, ...)
{
	char text[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(text, 1024, format, args);
	va_end(args);
	text[1024-1] = '\0';

	m_postInfo->GetNzbInfo()->AddMessage(kind, text);
}

#endif

ParCoordinator::ParCoordinator()
{
	debug("Creating ParCoordinator");

#ifndef DISABLE_PARCHECK
	m_parChecker.m_owner = this;
	m_parRenamer.m_owner = this;
#endif
}

ParCoordinator::~ParCoordinator()
{
	debug("Destroying ParCoordinator");
}

#ifndef DISABLE_PARCHECK
void ParCoordinator::Stop()
{
	debug("Stopping ParCoordinator");

	m_stopped = true;

	if (m_parChecker.IsRunning())
	{
		m_parChecker.Stop();
		int mSecWait = 5000;
		while (m_parChecker.IsRunning() && mSecWait > 0)
		{
			usleep(50 * 1000);
			mSecWait -= 50;
		}
		if (m_parChecker.IsRunning())
		{
			warn("Terminating par-check for %s", m_parChecker.GetInfoName());
			m_parChecker.Kill();
		}
	}
}
#endif

void ParCoordinator::PausePars(DownloadQueue* downloadQueue, NzbInfo* nzbInfo)
{
	debug("ParCoordinator: Pausing pars");

	downloadQueue->EditEntry(nzbInfo->GetId(),
		DownloadQueue::eaGroupPauseExtraPars, 0, nullptr);
}

#ifndef DISABLE_PARCHECK

/**
 * DownloadQueue must be locked prior to call of this function.
 */
void ParCoordinator::StartParCheckJob(PostInfo* postInfo)
{
	m_currentJob = jkParCheck;
	m_parChecker.SetPostInfo(postInfo);
	m_parChecker.SetDestDir(postInfo->GetNzbInfo()->GetDestDir());
	m_parChecker.SetNzbName(postInfo->GetNzbInfo()->GetName());
	m_parChecker.SetParTime(Util::CurrentTime());
	m_parChecker.SetDownloadSec(postInfo->GetNzbInfo()->GetDownloadSec());
	m_parChecker.SetParQuick(g_Options->GetParQuick() && !postInfo->GetForceParFull());
	m_parChecker.SetForceRepair(postInfo->GetForceRepair());
	m_parChecker.PrintMessage(Message::mkInfo, "Checking pars for %s", postInfo->GetNzbInfo()->GetName());
	postInfo->SetWorking(true);
	m_parChecker.Start();
}

/**
 * DownloadQueue must be locked prior to call of this function.
 */
void ParCoordinator::StartParRenameJob(PostInfo* postInfo)
{
	const char* destDir = postInfo->GetNzbInfo()->GetDestDir();

	if (postInfo->GetNzbInfo()->GetUnpackStatus() == NzbInfo::usSuccess &&
		!Util::EmptyStr(postInfo->GetNzbInfo()->GetFinalDir()))
	{
		destDir = postInfo->GetNzbInfo()->GetFinalDir();
	}

	m_currentJob = jkParRename;
	m_parRenamer.SetPostInfo(postInfo);
	m_parRenamer.SetDestDir(destDir);
	m_parRenamer.SetInfoName(postInfo->GetNzbInfo()->GetName());
	m_parRenamer.SetDetectMissing(postInfo->GetNzbInfo()->GetUnpackStatus() == NzbInfo::usNone);
	m_parRenamer.PrintMessage(Message::mkInfo, "Checking renamed files for %s", postInfo->GetNzbInfo()->GetName());
	postInfo->SetWorking(true);
	m_parRenamer.Start();
}

bool ParCoordinator::Cancel()
{
	if (m_currentJob == jkParCheck)
	{
		if (!m_parChecker.GetCancelled())
		{
			debug("Cancelling par-repair for %s", m_parChecker.GetInfoName());
			m_parChecker.Cancel();
			return true;
		}
	}
	else if (m_currentJob == jkParRename)
	{
		if (!m_parRenamer.GetCancelled())
		{
			debug("Cancelling par-rename for %s", m_parRenamer.GetInfoName());
			m_parRenamer.Cancel();
			return true;
		}
	}
	return false;
}

/**
 * DownloadQueue must be locked prior to call of this function.
 */
bool ParCoordinator::AddPar(FileInfo* fileInfo, bool deleted)
{
	bool sameCollection = m_parChecker.IsRunning() &&
		fileInfo->GetNzbInfo() == m_parChecker.GetPostInfo()->GetNzbInfo();
	if (sameCollection && !deleted)
	{
		BString<1024> fullFilename("%s%c%s", fileInfo->GetNzbInfo()->GetDestDir(), (int)PATH_SEPARATOR, fileInfo->GetFilename());
		m_parChecker.AddParFile(fullFilename);
	}
	else
	{
		m_parChecker.QueueChanged();
	}
	return sameCollection;
}

void ParCoordinator::ParCheckCompleted()
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	PostInfo* postInfo = m_parChecker.GetPostInfo();

	// Update ParStatus (accumulate result)
	if ((m_parChecker.GetStatus() == ParChecker::psRepaired ||
		m_parChecker.GetStatus() == ParChecker::psRepairNotNeeded) &&
		postInfo->GetNzbInfo()->GetParStatus() <= NzbInfo::psSkipped)
	{
		postInfo->GetNzbInfo()->SetParStatus(NzbInfo::psSuccess);
		postInfo->SetParRepaired(m_parChecker.GetStatus() == ParChecker::psRepaired);
	}
	else if (m_parChecker.GetStatus() == ParChecker::psRepairPossible &&
		postInfo->GetNzbInfo()->GetParStatus() != NzbInfo::psFailure)
	{
		postInfo->GetNzbInfo()->SetParStatus(NzbInfo::psRepairPossible);
	}
	else
	{
		postInfo->GetNzbInfo()->SetParStatus(NzbInfo::psFailure);
	}

	int waitTime = postInfo->GetNzbInfo()->GetDownloadSec() - m_parChecker.GetDownloadSec();
	postInfo->SetStartTime(postInfo->GetStartTime() + (time_t)waitTime);
	int parSec = (int)(Util::CurrentTime() - m_parChecker.GetParTime()) - waitTime;
	postInfo->GetNzbInfo()->SetParSec(postInfo->GetNzbInfo()->GetParSec() + parSec);

	postInfo->GetNzbInfo()->SetParFull(m_parChecker.GetParFull());

	postInfo->SetWorking(false);
	postInfo->SetStage(PostInfo::ptQueued);

	downloadQueue->Save();
}

/**
* Unpause par2-files
* returns true, if the files with required number of blocks were unpaused,
* or false if there are no more files in queue for this collection or not enough blocks.
* special case: returns true if there are any unpaused par2-files in the queue regardless
* of the amount of blocks; this is to keep par-checker wait for download completion.
*/
bool ParCoordinator::RequestMorePars(NzbInfo* nzbInfo, const char* parFilename, int blockNeeded, int* blockFoundOut)
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	Blocks blocks;
	blocks.clear();
	int blockFound = 0;
	int curBlockFound = 0;

	FindPars(downloadQueue, nzbInfo, parFilename, blocks, true, true, &curBlockFound);
	blockFound += curBlockFound;
	if (blockFound < blockNeeded)
	{
		FindPars(downloadQueue, nzbInfo, parFilename, blocks, true, false, &curBlockFound);
		blockFound += curBlockFound;
	}
	if (blockFound < blockNeeded)
	{
		FindPars(downloadQueue, nzbInfo, parFilename, blocks, false, false, &curBlockFound);
		blockFound += curBlockFound;
	}

	if (blockFound >= blockNeeded)
	{
		// 1. first unpause all files with par-blocks less or equal iBlockNeeded
		// starting from the file with max block count.
		// if par-collection was built exponentially and all par-files present,
		// this step selects par-files with exact number of blocks we need.
		while (blockNeeded > 0)
		{
			BlockInfo* bestBlockInfo = nullptr;
			Blocks::iterator bestBlockIter;
			for (Blocks::iterator it = blocks.begin(); it != blocks.end(); it++)
			{
				BlockInfo& blockInfo = *it;
				if (blockInfo.m_blockCount <= blockNeeded &&
				   (!bestBlockInfo || bestBlockInfo->m_blockCount < blockInfo.m_blockCount))
				{
					bestBlockInfo = &blockInfo;
					bestBlockIter = it;
				}
			}
			if (bestBlockInfo)
			{
				if (bestBlockInfo->m_fileInfo->GetPaused())
				{
					m_parChecker.PrintMessage(Message::mkInfo, "Unpausing %s%c%s for par-recovery", nzbInfo->GetName(), (int)PATH_SEPARATOR, bestBlockInfo->m_fileInfo->GetFilename());
					bestBlockInfo->m_fileInfo->SetPaused(false);
					bestBlockInfo->m_fileInfo->SetExtraPriority(true);
				}
				blockNeeded -= bestBlockInfo->m_blockCount;
				blocks.erase(bestBlockIter);
			}
			else
			{
				break;
			}
		}

		// 2. then unpause other files
		// this step only needed if the par-collection was built not exponentially
		// or not all par-files present (or some of them were corrupted)
		// this step is not optimal, but we hope, that the first step will work good
		// in most cases and we will not need the second step often
		while (blockNeeded > 0)
		{
			BlockInfo& blockInfo = blocks.front();
			if (blockInfo.m_fileInfo->GetPaused())
			{
				m_parChecker.PrintMessage(Message::mkInfo, "Unpausing %s%c%s for par-recovery", nzbInfo->GetName(), (int)PATH_SEPARATOR, blockInfo.m_fileInfo->GetFilename());
				blockInfo.m_fileInfo->SetPaused(false);
				blockInfo.m_fileInfo->SetExtraPriority(true);
			}
			blockNeeded -= blockInfo.m_blockCount;
		}
	}

	bool hasUnpausedParFiles = false;
	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
		if (fileInfo->GetParFile() && !fileInfo->GetPaused())
		{
			hasUnpausedParFiles = true;
			break;
		}
	}

	if (blockFoundOut)
	{
		*blockFoundOut = blockFound;
	}

	bool ok = blockNeeded <= 0 || hasUnpausedParFiles;

	return ok;
}

void ParCoordinator::FindPars(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, const char* parFilename,
	Blocks& blocks, bool strictParName, bool exactParName, int* blockFound)
{
	*blockFound = 0;

	// extract base name from m_szParFilename (trim .par2-extension and possible .vol-part)
	char* baseParFilename = FileSystem::BaseFileName(parFilename);
	int mainBaseLen = 0;
	if (!ParParser::ParseParFilename(baseParFilename, &mainBaseLen, nullptr))
	{
		// should not happen
		nzbInfo->PrintMessage(Message::mkError, "Internal error: could not parse filename %s", baseParFilename);
		return;
	}
	BString<1024> mainBaseFilename;
	mainBaseFilename.Set(baseParFilename, mainBaseLen);
	for (char* p = mainBaseFilename; *p; p++) *p = tolower(*p); // convert string to lowercase

	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
		int blockCount = 0;
		if (ParParser::ParseParFilename(fileInfo->GetFilename(), nullptr, &blockCount) &&
			blockCount > 0)
		{
			bool useFile = true;

			if (exactParName)
			{
				useFile = ParParser::SameParCollection(fileInfo->GetFilename(), FileSystem::BaseFileName(parFilename));
			}
			else if (strictParName)
			{
				// the pFileInfo->GetFilename() may be not confirmed and may contain
				// additional texts if Subject could not be parsed correctly

				BString<1024> loFileName = fileInfo->GetFilename();
				for (char* p = loFileName; *p; p++) *p = tolower(*p); // convert string to lowercase

				BString<1024> candidateFileName("%s.par2", *mainBaseFilename);
				if (!strstr(loFileName, candidateFileName))
				{
					candidateFileName.Format("%s.vol", *mainBaseFilename);
					useFile = strstr(loFileName, candidateFileName);
				}
			}

			bool alreadyAdded = false;
			// check if file is not in the list already
			if (useFile)
			{
				for (BlockInfo& blockInfo : blocks)
				{
					if (blockInfo.m_fileInfo == fileInfo)
					{
						alreadyAdded = true;
						break;
					}
				}
			}

			// if it is a par2-file with blocks and it was from the same NZB-request
			// and it belongs to the same file collection (same base name),
			// then OK, we can use it
			if (useFile && !alreadyAdded)
			{
				blocks.emplace_back(fileInfo, blockCount);
				*blockFound += blockCount;
			}
		}
	}
}

void ParCoordinator::UpdateParCheckProgress()
{
	PostInfo* postInfo;

	{
		GuardedDownloadQueue guard = DownloadQueue::Guard();

		postInfo = m_parChecker.GetPostInfo();
		if (m_parChecker.GetFileProgress() == 0)
		{
			postInfo->SetProgressLabel(m_parChecker.GetProgressLabel());
		}
		postInfo->SetFileProgress(m_parChecker.GetFileProgress());
		postInfo->SetStageProgress(m_parChecker.GetStageProgress());
		PostInfo::EStage StageKind[] = {PostInfo::ptLoadingPars, PostInfo::ptVerifyingSources, PostInfo::ptRepairing, PostInfo::ptVerifyingRepaired};
		PostInfo::EStage stage = StageKind[m_parChecker.GetStage()];
		time_t current = Util::CurrentTime();

		if (postInfo->GetStage() != stage)
		{
			postInfo->SetStage(stage);
			postInfo->SetStageTime(current);
			if (postInfo->GetStage() == PostInfo::ptRepairing)
			{
				m_parChecker.SetRepairTime(current);
			}
			else if (postInfo->GetStage() == PostInfo::ptVerifyingRepaired)
			{
				int repairSec = (int)(current - m_parChecker.GetRepairTime());
				postInfo->GetNzbInfo()->SetRepairSec(postInfo->GetNzbInfo()->GetRepairSec() + repairSec);
			}
		}

		bool parCancel = false;
		if (!m_parChecker.GetCancelled())
		{
			if ((g_Options->GetParTimeLimit() > 0) &&
				m_parChecker.GetStage() == PostParChecker::ptRepairing &&
				((g_Options->GetParTimeLimit() > 5 && current - postInfo->GetStageTime() > 5 * 60) ||
					(g_Options->GetParTimeLimit() <= 5 && current - postInfo->GetStageTime() > 1 * 60)))
			{
				// first five (or one) minutes elapsed, now can check the estimated time
				int estimatedRepairTime = (int)((current - postInfo->GetStartTime()) * 1000 /
					(postInfo->GetStageProgress() > 0 ? postInfo->GetStageProgress() : 1));
				if (estimatedRepairTime > g_Options->GetParTimeLimit() * 60)
				{
					debug("Estimated repair time %i seconds", estimatedRepairTime);
					m_parChecker.PrintMessage(Message::mkWarning, "Cancelling par-repair for %s, estimated repair time (%i minutes) exceeds allowed repair time", m_parChecker.GetInfoName(), estimatedRepairTime / 60);
					parCancel = true;
				}
			}
		}

		if (parCancel)
		{
			m_parChecker.Cancel();
		}
	}

	CheckPauseState(postInfo);
}

void ParCoordinator::CheckPauseState(PostInfo* postInfo)
{
	if (g_Options->GetPausePostProcess() && !postInfo->GetNzbInfo()->GetForcePriority())
	{
		time_t stageTime = postInfo->GetStageTime();
		time_t startTime = postInfo->GetStartTime();
		time_t parTime = m_parChecker.GetParTime();
		time_t repairTime = m_parChecker.GetRepairTime();
		time_t waitTime = Util::CurrentTime();

		// wait until Post-processor is unpaused
		while (g_Options->GetPausePostProcess() && !postInfo->GetNzbInfo()->GetForcePriority() && !m_stopped)
		{
			usleep(50 * 1000);

			// update time stamps

			time_t delta = Util::CurrentTime() - waitTime;

			if (stageTime > 0)
			{
				postInfo->SetStageTime(stageTime + delta);
			}
			if (startTime > 0)
			{
				postInfo->SetStartTime(startTime + delta);
			}
			if (parTime > 0)
			{
				m_parChecker.SetParTime(parTime + delta);
			}
			if (repairTime > 0)
			{
				m_parChecker.SetRepairTime(repairTime + delta);
			}
		}
	}
}

void ParCoordinator::ParRenameCompleted()
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	PostInfo* postInfo = m_parRenamer.GetPostInfo();
	postInfo->GetNzbInfo()->SetRenameStatus(m_parRenamer.GetStatus() == ParRenamer::psSuccess ? NzbInfo::rsSuccess : NzbInfo::rsFailure);

	if (m_parRenamer.HasMissedFiles() && postInfo->GetNzbInfo()->GetParStatus() <= NzbInfo::psSkipped)
	{
		m_parRenamer.PrintMessage(Message::mkInfo, "Requesting par-check/repair for %s to restore missing files ", m_parRenamer.GetInfoName());
		postInfo->SetRequestParCheck(true);
	}

	postInfo->SetWorking(false);
	postInfo->SetStage(PostInfo::ptQueued);

	downloadQueue->Save();
}

void ParCoordinator::UpdateParRenameProgress()
{
	PostInfo* postInfo;
	{
		GuardedDownloadQueue guard = DownloadQueue::Guard();

		postInfo = m_parRenamer.GetPostInfo();
		postInfo->SetProgressLabel(m_parRenamer.GetProgressLabel());
		postInfo->SetStageProgress(m_parRenamer.GetStageProgress());
		time_t current = Util::CurrentTime();

		if (postInfo->GetStage() != PostInfo::ptRenaming)
		{
			postInfo->SetStage(PostInfo::ptRenaming);
			postInfo->SetStageTime(current);
		}
	}

	CheckPauseState(postInfo);
}

#endif
