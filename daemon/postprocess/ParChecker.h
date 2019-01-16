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


#ifndef PARCHECKER_H
#define PARCHECKER_H

#ifndef DISABLE_PARCHECK

#include "NString.h"
#include "Container.h"
#include "FileSystem.h"
#include "Log.h"

class Repairer;

class ParChecker
{
public:
	enum EStatus
	{
		psFailed,
		psRepairPossible,
		psRepaired,
		psRepairNotNeeded
	};

	enum EStage
	{
		ptLoadingPars,
		ptVerifyingSources,
		ptRepairing,
		ptVerifyingRepaired,
	};

	class AbstractRepairer
	{
	public:
		virtual ~AbstractRepairer() {};
		virtual Repairer* GetRepairer() = 0;
	};

	void Execute();
	void SetDestDir(const char* destDir) { m_destDir = destDir; }
	const char* GetParFilename() { return m_parFilename; }
	const char* GetInfoName() { return m_infoName; }
	void SetInfoName(const char* infoName) { m_infoName = infoName; }
	void SetNzbName(const char* nzbName) { m_nzbName = nzbName; }
	void SetParQuick(bool parQuick) { m_parQuick = parQuick; }
	bool GetParQuick() { return m_parQuick; }
	void SetForceRepair(bool forceRepair) { m_forceRepair = forceRepair; }
	bool GetForceRepair() { return m_forceRepair; }
	void SetParFull(bool parFull) { m_parFull = parFull; }
	bool GetParFull() { return m_parFull; }
	EStatus GetStatus() { return m_status; }
	void AddParFile(const char* parFilename);
	void QueueChanged();
	void Cancel();

protected:
	class Segment
	{
	public:
		Segment(bool success, int64 offset, int size, uint32 crc) :
			m_success(success), m_offset(offset), m_size(size), m_crc(crc) {}
		bool GetSuccess() { return m_success; }
		int64 GetOffset() { return m_offset; }
		int GetSize() { return m_size; }
		uint32 GetCrc() { return m_crc; }

	private:
		bool m_success;
		int64 m_offset;
		int m_size;
		uint32 m_crc;
	};

	typedef std::deque<Segment> SegmentList;

	class DupeSource
	{
	public:
		DupeSource(int id, const char* directory) :
			m_id(id), m_directory(directory) {}
		int GetId() { return m_id; }
		const char* GetDirectory() { return m_directory; }
		int GetUsedBlocks() { return m_usedBlocks; }
		void SetUsedBlocks(int usedBlocks) { m_usedBlocks = usedBlocks; }

	private:
		int m_id;
		CString m_directory;
		int m_usedBlocks = 0;
	};

	typedef std::deque<DupeSource> DupeSourceList;

	enum EFileStatus
	{
		fsUnknown,
		fsSuccess,
		fsPartial,
		fsFailure
	};

	/**
	* Unpause par2-files
	* returns true, if the files with required number of blocks were unpaused,
	* or false if there are no more files in queue for this collection or not enough blocks
	*/
	virtual bool RequestMorePars(int blockNeeded, int* blockFound) = 0;
	virtual void UpdateProgress() {}
	virtual bool IsStopped() { return false; };
	virtual void Completed() {}
	virtual void PrintMessage(Message::EKind kind, const char* format, ...) PRINTF_SYNTAX(3) {}
	virtual void RegisterParredFile(const char* filename) {}
	virtual bool IsParredFile(const char* filename) { return false; }
	virtual EFileStatus FindFileCrc(const char* filename, uint32* crc, SegmentList* segments) { return fsUnknown; }
	virtual const char* FindFileOrigname(const char* filename) { return nullptr; }
	virtual void RequestDupeSources(DupeSourceList* dupeSourceList) {}
	virtual void StatDupeSources(DupeSourceList* dupeSourceList) {}
	EStage GetStage() { return m_stage; }
	const char* GetProgressLabel() { return m_progressLabel; }
	int GetFileProgress() { return m_fileProgress; }
	int GetStageProgress() { return m_stageProgress; }

private:
	class StreamBuf : public std::streambuf
	{
	public:
		StreamBuf(ParChecker* owner, Message::EKind kind) : m_owner(owner), m_kind(kind) {}
		virtual int overflow(int ch) override;
	private:
		ParChecker* m_owner;
		Message::EKind m_kind;
		StringBuilder m_buffer;
	};

	typedef std::deque<CString> FileList;
	typedef std::deque<void*> SourceList;
	typedef std::vector<bool> ValidBlocks;

	CString m_infoName;
	CString m_destDir;
	CString m_nzbName;
	const char* m_parFilename = nullptr;
	EStatus m_status = psFailed;
	EStage m_stage;
	CString m_errMsg;
	FileList m_queuedParFiles;
	Mutex m_queuedParFilesMutex;
	bool m_queuedParFilesChanged;
	FileList m_processedFiles;
	int m_processedCount;
	int m_filesToRepair;
	int m_extraFiles;
	int m_quickFiles;
	bool m_verifyingExtraFiles;
	CString m_progressLabel;
	int m_fileProgress;
	int m_stageProgress;
	bool m_cancelled;
	SourceList m_sourceFiles;
	std::string m_lastFilename;
	bool m_hasDamagedFiles;
	bool m_parQuick = false;
	bool m_forceRepair = false;
	bool m_parFull = false;
	DupeSourceList m_dupeSources;
	StreamBuf m_parOutStream{this, Message::mkDetail};
	StreamBuf m_parErrStream{this, Message::mkError};
	std::ostream m_parCout{&m_parOutStream};
	std::ostream m_parCerr{&m_parErrStream};
	Mutex m_repairerMutex;

	// "m_repairer" should be of type "Par2::Par2Repairer", however to prevent the
	// including of libpar2-headers into this header-file we use an empty abstract class.
	std::unique_ptr<AbstractRepairer> m_repairer;
	Repairer* GetRepairer() { return m_repairer->GetRepairer(); }

	void Cleanup();
	EStatus RunParCheckAll();
	EStatus RunParCheck(const char* parFilename);
	int PreProcessPar();
	bool LoadMainParBak();
	int ProcessMorePars();
	bool LoadMorePars();
	bool AddSplittedFragments();
	bool AddMissingFiles();
	bool AddDupeFiles();
	bool AddExtraFiles(bool onlyMissing, bool externalDir, const char* directory);
	bool IsProcessedFile(const char* filename);
	void SaveSourceList();
	void DeleteLeftovers();
	void signal_filename(std::string str);
	void signal_progress(int progress);
	void signal_done(std::string str, int available, int total);
	// declared as void* to prevent the including of libpar2-headers into this header-file
	// Par2::DiskFile* pDiskfile, Par2::Par2RepairerSourceFile* pSourcefile
	EFileStatus VerifyDataFile(void* diskfile, void* sourcefile, int* availableBlocks);
	bool VerifySuccessDataFile(void* diskfile, void* sourcefile, uint32 downloadCrc);
	bool VerifyPartialDataFile(void* diskfile, void* sourcefile, SegmentList* segments, ValidBlocks* validBlocks);
	void SortExtraFiles(void* extrafiles);
	bool SmartCalcFileRangeCrc(DiskFile& file, int64 start, int64 end, SegmentList* segments,
		uint32* downloadCrc);
	bool DumbCalcFileRangeCrc(DiskFile& file, int64 start, int64 end, uint32* downloadCrc);
	void CheckEmptyFiles();
	CString GetPacketCreator();
	bool MaybeSplittedFragement(const char* filename1, const char* filename2);

	friend class Repairer;
};

#endif

#endif
