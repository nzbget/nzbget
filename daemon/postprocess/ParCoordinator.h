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


#ifndef PARCOORDINATOR_H
#define PARCOORDINATOR_H

#include "DownloadInfo.h"

#ifndef DISABLE_PARCHECK
#include "ParChecker.h"
#include "ParRenamer.h"
#include "DupeMatcher.h"
#endif

class ParCoordinator
{
public:
	ParCoordinator();
	virtual ~ParCoordinator();
	void PausePars(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);

#ifndef DISABLE_PARCHECK
	bool AddPar(FileInfo* fileInfo, bool deleted);
	void StartParCheckJob(PostInfo* postInfo);
	void StartParRenameJob(PostInfo* postInfo);
	void Stop();
	bool Cancel();

protected:
	void UpdateParCheckProgress();
	void UpdateParRenameProgress();
	void ParCheckCompleted();
	void ParRenameCompleted();
	void CheckPauseState(PostInfo* postInfo);
	bool RequestMorePars(NzbInfo* nzbInfo, const char* parFilename, int blockNeeded, int* blockFound);

private:
	class PostParChecker: public ParChecker
	{
	public:
		PostInfo* GetPostInfo() { return m_postInfo; }
		void SetPostInfo(PostInfo* postInfo) { m_postInfo = postInfo; }
		time_t GetParTime() { return m_parTime; }
		void SetParTime(time_t parTime) { m_parTime = parTime; }
		time_t GetRepairTime() { return m_repairTime; }
		void SetRepairTime(time_t repairTime) { m_repairTime = repairTime; }
		int GetDownloadSec() { return m_downloadSec; }
		void SetDownloadSec(int downloadSec) { m_downloadSec = downloadSec; }
	protected:
		virtual bool RequestMorePars(int blockNeeded, int* blockFound);
		virtual void UpdateProgress();
		virtual void Completed() { m_owner->ParCheckCompleted(); }
		virtual void PrintMessage(Message::EKind kind, const char* format, ...) PRINTF_SYNTAX(3);
		virtual void RegisterParredFile(const char* filename);
		virtual bool IsParredFile(const char* filename);
		virtual EFileStatus FindFileCrc(const char* filename, uint32* crc, SegmentList* segments);
		virtual void RequestDupeSources(DupeSourceList* dupeSourceList);
		virtual void StatDupeSources(DupeSourceList* dupeSourceList);
	private:
		ParCoordinator* m_owner;
		PostInfo* m_postInfo;
		time_t m_parTime;
		time_t m_repairTime;
		int m_downloadSec;

		friend class ParCoordinator;
	};

	class PostParRenamer: public ParRenamer
	{
	public:
		PostInfo* GetPostInfo() { return m_postInfo; }
		void SetPostInfo(PostInfo* postInfo) { m_postInfo = postInfo; }
	protected:
		virtual void UpdateProgress();
		virtual void Completed() { m_owner->ParRenameCompleted(); }
		virtual void PrintMessage(Message::EKind kind, const char* format, ...) PRINTF_SYNTAX(3);
		virtual void RegisterParredFile(const char* filename);
		virtual void RegisterRenamedFile(const char* oldFilename, const char* newFileName);
	private:
		ParCoordinator* m_owner;
		PostInfo* m_postInfo;

		friend class ParCoordinator;
	};

	class PostDupeMatcher: public DupeMatcher
	{
	public:
		PostDupeMatcher(PostInfo* postInfo):
			DupeMatcher(postInfo->GetNzbInfo()->GetDestDir(),
				postInfo->GetNzbInfo()->GetSize() - postInfo->GetNzbInfo()->GetParSize()),
			m_postInfo(postInfo) {}
	protected:
		virtual void PrintMessage(Message::EKind kind, const char* format, ...) PRINTF_SYNTAX(3);
	private:
		PostInfo* m_postInfo;
	};

	struct BlockInfo
	{
		FileInfo* m_fileInfo;
		int m_blockCount;
		BlockInfo(FileInfo* fileInfo, int blockCount) :
			m_fileInfo(fileInfo), m_blockCount(blockCount) {}
	};

	typedef std::deque<BlockInfo> Blocks;

	enum EJobKind
	{
		jkParCheck,
		jkParRename
	};

	PostParChecker m_parChecker;
	bool m_stopped = false;
	PostParRenamer m_parRenamer;
	EJobKind m_currentJob;

	void FindPars(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, const char* parFilename,
		Blocks& blocks, bool strictParName, bool exactParName, int* blockFound);
#endif
};

#endif
