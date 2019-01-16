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


#ifndef REPAIR_H
#define REPAIR_H

#include "DownloadInfo.h"
#include "Thread.h"
#include "Script.h"

#ifndef DISABLE_PARCHECK
#include "ParChecker.h"
#include "DupeMatcher.h"
#endif

class RepairController : public Thread, public ScriptController
{
public:
	RepairController();
	virtual void Stop();

#ifndef DISABLE_PARCHECK
	virtual void Run();
	static void StartJob(PostInfo* postInfo);
	bool AddPar(FileInfo* fileInfo, bool deleted);

protected:
	void UpdateParCheckProgress();
	void ParCheckCompleted();
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
		virtual bool IsStopped() { return m_owner->IsStopped(); };
		virtual void Completed() { m_owner->ParCheckCompleted(); }
		virtual void PrintMessage(Message::EKind kind, const char* format, ...) PRINTF_SYNTAX(3);
		virtual void RegisterParredFile(const char* filename);
		virtual bool IsParredFile(const char* filename);
		virtual EFileStatus FindFileCrc(const char* filename, uint32* crc, SegmentList* segments);
		virtual const char* FindFileOrigname(const char* filename);
		virtual void RequestDupeSources(DupeSourceList* dupeSourceList);
		virtual void StatDupeSources(DupeSourceList* dupeSourceList);
	private:
		RepairController* m_owner;
		PostInfo* m_postInfo;
		time_t m_parTime;
		time_t m_repairTime;
		int m_downloadSec;

		friend class RepairController;
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

	PostInfo* m_postInfo;
	PostParChecker m_parChecker;

	void FindPars(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, const char* parFilename,
		Blocks& blocks, bool strictParName, bool exactParName, int* blockFound);
#endif
};

#endif
