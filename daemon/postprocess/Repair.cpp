/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2007-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "Repair.h"
#include "DupeCoordinator.h"
#include "ParParser.h"
#include "Options.h"
#include "WorkState.h"
#include "DiskState.h"
#include "Log.h"
#include "FileSystem.h"

#ifndef DISABLE_PARCHECK
bool RepairController::PostParChecker::RequestMorePars(int blockNeeded, int* blockFound)
{
	return m_owner->RequestMorePars(m_postInfo->GetNzbInfo(), GetParFilename(), blockNeeded, blockFound);
}

void RepairController::PostParChecker::UpdateProgress()
{
	m_owner->UpdateParCheckProgress();
}

void RepairController::PostParChecker::PrintMessage(Message::EKind kind, const char* format, ...)
{
	char text[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(text, 1024, format, args);
	va_end(args);
	text[1024-1] = '\0';

	m_postInfo->GetNzbInfo()->AddMessage(kind, text);
}

void RepairController::PostParChecker::RegisterParredFile(const char* filename)
{
	m_postInfo->GetParredFiles()->push_back(filename);
}

bool RepairController::PostParChecker::IsParredFile(const char* filename)
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

ParChecker::EFileStatus RepairController::PostParChecker::FindFileCrc(const char* filename,
	uint32* crc, SegmentList* segments)
{
	CompletedFile* completedFile = nullptr;

	for (CompletedFile& completedFile2 : m_postInfo->GetNzbInfo()->GetCompletedFiles())
	{
		if (!strcasecmp(completedFile2.GetFilename(), filename))
		{
			completedFile = &completedFile2;
			break;
		}
	}
	if (!completedFile)
	{
		return ParChecker::fsUnknown;
	}

	debug("Found completed file: %s, CRC: %.8x, Status: %i", FileSystem::BaseFileName(completedFile->GetFilename()), completedFile->GetCrc(), (int)completedFile->GetStatus());

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

const char* RepairController::PostParChecker::FindFileOrigname(const char* filename)
{
	for (CompletedFile& completedFile : m_postInfo->GetNzbInfo()->GetCompletedFiles())
	{
		if (!strcasecmp(completedFile.GetFilename(), filename))
		{
			return completedFile.GetOrigname();
		}
	}

	return nullptr;
}

void RepairController::PostParChecker::RequestDupeSources(DupeSourceList* dupeSourceList)
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

void RepairController::PostParChecker::StatDupeSources(DupeSourceList* dupeSourceList)
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


void RepairController::PostDupeMatcher::PrintMessage(Message::EKind kind, const char* format, ...)
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

RepairController::RepairController()
{
	debug("Creating RepairController");

#ifndef DISABLE_PARCHECK
	m_parChecker.m_owner = this;
#endif
}

void RepairController::Stop()
{
	debug("Stopping RepairController");
	Thread::Stop();
#ifndef DISABLE_PARCHECK
	m_parChecker.Cancel();
#endif
}

#ifndef DISABLE_PARCHECK

void RepairController::StartJob(PostInfo* postInfo)
{
	RepairController* repairController = new RepairController();
	repairController->m_postInfo = postInfo;
	repairController->SetAutoDestroy(false);

	postInfo->SetPostThread(repairController);

	repairController->Start();
}

void RepairController::Run()
{
	BString<1024> nzbName;
	CString destDir;
	{
		GuardedDownloadQueue guard = DownloadQueue::Guard();
		nzbName = m_postInfo->GetNzbInfo()->GetName();
		destDir = m_postInfo->GetNzbInfo()->GetDestDir();
	}

	m_parChecker.SetPostInfo(m_postInfo);
	m_parChecker.SetDestDir(destDir);
	m_parChecker.SetNzbName(nzbName);
	m_parChecker.SetParTime(Util::CurrentTime());
	m_parChecker.SetDownloadSec(m_postInfo->GetNzbInfo()->GetDownloadSec());
	m_parChecker.SetParQuick(g_Options->GetParQuick() && !m_postInfo->GetForceParFull());
	m_parChecker.SetForceRepair(m_postInfo->GetForceRepair());

	m_parChecker.PrintMessage(Message::mkInfo, "Checking pars for %s", *nzbName);

	m_parChecker.Execute();
}

/**
 * DownloadQueue must be locked prior to call of this function.
 */
bool RepairController::AddPar(FileInfo* fileInfo, bool deleted)
{
	bool sameCollection = fileInfo->GetNzbInfo() == m_parChecker.GetPostInfo()->GetNzbInfo();
	if (sameCollection && !deleted)
	{
		BString<1024> fullFilename("%s%c%s", fileInfo->GetNzbInfo()->GetDestDir(), PATH_SEPARATOR, fileInfo->GetFilename());
		m_parChecker.AddParFile(fullFilename);
	}
	else
	{
		m_parChecker.QueueChanged();
	}
	return sameCollection;
}

void RepairController::ParCheckCompleted()
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
}

/**
* Unpause par2-files
* returns true, if the files with required number of blocks were unpaused,
* or false if there are no more files in queue for this collection or not enough blocks.
* special case: returns true if there are any unpaused par2-files in the queue regardless
* of the amount of blocks; this is to keep par-checker wait for download completion.
*/
bool RepairController::RequestMorePars(NzbInfo* nzbInfo, const char* parFilename, int blockNeeded, int* blockFoundOut)
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	Blocks availableBlocks;
	Blocks selectedBlocks;
	int blockFound = 0;
	int curBlockFound = 0;

	FindPars(downloadQueue, nzbInfo, parFilename, availableBlocks, true, true, &curBlockFound);
	blockFound += curBlockFound;
	if (blockFound < blockNeeded)
	{
		FindPars(downloadQueue, nzbInfo, parFilename, availableBlocks, true, false, &curBlockFound);
		blockFound += curBlockFound;
	}
	if (blockFound < blockNeeded)
	{
		FindPars(downloadQueue, nzbInfo, parFilename, availableBlocks, false, false, &curBlockFound);
		blockFound += curBlockFound;
	}

	std::sort(availableBlocks.begin(), availableBlocks.end(),
		[](const BlockInfo& block1, const BlockInfo& block2)
		{
			return block1.m_blockCount < block2.m_blockCount;
		});

	if (blockFound >= blockNeeded)
	{
		// collect as much blocks as needed
		for (Blocks::iterator it = availableBlocks.begin(); blockNeeded > 0 && it != availableBlocks.end(); it++)
		{
			BlockInfo& blockInfo = *it;
			selectedBlocks.push_front(blockInfo);
			blockNeeded -= blockInfo.m_blockCount;
		}

		// discarding superfluous blocks
		for (Blocks::iterator it = selectedBlocks.begin(); it != selectedBlocks.end(); )
		{
			BlockInfo& blockInfo = *it;
			if (blockNeeded + blockInfo.m_blockCount <= 0)
			{
				blockNeeded += blockInfo.m_blockCount;
				it = selectedBlocks.erase(it);
			}
			else
			{
				it++;
			}
		}

		// unpause files with blocks
		for (BlockInfo& blockInfo : selectedBlocks)
		{
			if (blockInfo.m_fileInfo->GetPaused())
			{
				m_parChecker.PrintMessage(Message::mkInfo, "Unpausing %s%c%s for par-recovery", nzbInfo->GetName(), PATH_SEPARATOR, blockInfo.m_fileInfo->GetFilename());
				blockInfo.m_fileInfo->SetPaused(false);
				blockInfo.m_fileInfo->SetExtraPriority(true);
			}
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

void RepairController::FindPars(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, const char* parFilename,
	Blocks& blocks, bool strictParName, bool exactParName, int* blockFound)
{
	*blockFound = 0;

	// extract base name from m_szParFilename (trim .par2-extension and possible .vol-part)
	char* baseParFilename = FileSystem::BaseFileName(parFilename);
	int mainBaseLen = 0;
	if (!ParParser::ParseParFilename(baseParFilename, true, &mainBaseLen, nullptr))
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
		if (ParParser::ParseParFilename(fileInfo->GetFilename(), fileInfo->GetFilenameConfirmed(), nullptr, &blockCount) &&
			blockCount > 0)
		{
			bool useFile = true;

			if (exactParName)
			{
				useFile = ParParser::SameParCollection(fileInfo->GetFilename(),
					FileSystem::BaseFileName(parFilename), fileInfo->GetFilenameConfirmed());
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

void RepairController::UpdateParCheckProgress()
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
		if (!IsStopped())
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
			Stop();
		}
	}

	CheckPauseState(postInfo);
}

void RepairController::CheckPauseState(PostInfo* postInfo)
{
	if (g_WorkState->GetPausePostProcess() && !postInfo->GetNzbInfo()->GetForcePriority())
	{
		time_t stageTime = postInfo->GetStageTime();
		time_t startTime = postInfo->GetStartTime();
		time_t parTime = m_parChecker.GetParTime();
		time_t repairTime = m_parChecker.GetRepairTime();
		time_t waitTime = Util::CurrentTime();

		// wait until Post-processor is unpaused
		while (g_WorkState->GetPausePostProcess() && !postInfo->GetNzbInfo()->GetForcePriority() && !IsStopped())
		{
			Util::Sleep(50);

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

#endif
