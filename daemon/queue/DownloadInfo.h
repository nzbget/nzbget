/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * $Revision$
 * $Date$
 *
 */


#ifndef DOWNLOADINFO_H
#define DOWNLOADINFO_H

#include <vector>
#include <deque>
#include <time.h>

#include "Observer.h"
#include "Log.h"
#include "Thread.h"

class NzbInfo;
class DownloadQueue;
class PostInfo;

class ServerStat
{
private:
	int					m_serverId;
	int					m_successArticles;
	int					m_failedArticles;

public:
						ServerStat(int serverId);
	int					GetServerId() { return m_serverId; }
	int					GetSuccessArticles() { return m_successArticles; }
	void				SetSuccessArticles(int successArticles) { m_successArticles = successArticles; }
	int					GetFailedArticles() { return m_failedArticles; }
	void				SetFailedArticles(int failedArticles) { m_failedArticles = failedArticles; }
};

typedef std::vector<ServerStat*>	ServerStatListBase;

class ServerStatList : public ServerStatListBase
{
public:
	enum EStatOperation
	{
		soSet,
		soAdd,
		soSubtract
	};
	
public:
						~ServerStatList();
	void				StatOp(int serverId, int successArticles, int failedArticles, EStatOperation statOperation);
	void				ListOp(ServerStatList* serverStats, EStatOperation statOperation);
	void				Clear();
};

class ArticleInfo
{
public:
	enum EStatus
	{
		aiUndefined,
		aiRunning,
		aiFinished,
		aiFailed
	};
	
private:
	int					m_partNumber;
	char*				m_messageId;
	int					m_size;
	char*				m_segmentContent;
	long long			m_segmentOffset;
	int					m_segmentSize;
	EStatus				m_status;
	char*				m_resultFilename;
	unsigned long		m_crc;

public:
						ArticleInfo();
						~ArticleInfo();
	void 				SetPartNumber(int s) { m_partNumber = s; }
	int 				GetPartNumber() { return m_partNumber; }
	const char* 		GetMessageId() { return m_messageId; }
	void 				SetMessageId(const char* messageId);
	void 				SetSize(int size) { m_size = size; }
	int 				GetSize() { return m_size; }
	void				AttachSegment(char* content, long long offset, int size);
	void				DiscardSegment();
	const char* 		GetSegmentContent() { return m_segmentContent; }
	void				SetSegmentOffset(long long segmentOffset) { m_segmentOffset = segmentOffset; }
	long long			GetSegmentOffset() { return m_segmentOffset; }
	void 				SetSegmentSize(int segmentSize) { m_segmentSize = segmentSize; }
	int 				GetSegmentSize() { return m_segmentSize; }
	EStatus				GetStatus() { return m_status; }
	void				SetStatus(EStatus Status) { m_status = Status; }
	const char*			GetResultFilename() { return m_resultFilename; }
	void 				SetResultFilename(const char* v);
	unsigned long		GetCrc() { return m_crc; }
	void				SetCrc(unsigned long crc) { m_crc = crc; }
};

class FileInfo
{
public:
	typedef std::vector<ArticleInfo*>	Articles;
	typedef std::vector<char*>			Groups;

private:
	int					m_id;
	NzbInfo*			m_nzbInfo;
	Articles			m_articles;
	Groups				m_groups;
	ServerStatList		m_serverStats;
	char* 				m_subject;
	char*				m_filename;
	long long 			m_size;
	long long 			m_remainingSize;
	long long			m_successSize;
	long long			m_failedSize;
	long long			m_missedSize;
	int					m_totalArticles;
	int					m_missedArticles;
	int					m_failedArticles;
	int					m_successArticles;
	time_t				m_time;
	bool				m_paused;
	bool				m_deleted;
	bool				m_filenameConfirmed;
	bool				m_parFile;
	int					m_completedArticles;
	bool				m_outputInitialized;
	char*				m_outputFilename;
	Mutex*				m_mutexOutputFile;
	bool				m_extraPriority;
	int					m_activeDownloads;
	bool				m_autoDeleted;
	int					m_cachedArticles;
	bool				m_partialChanged;

	static int			m_idGen;
	static int			m_idMax;

	friend class CompletedFile;

public:
						FileInfo(int id = 0);
						~FileInfo();
	int					GetId() { return m_id; }
	void				SetId(int id);
	static void			ResetGenId(bool max);
	NzbInfo*			GetNzbInfo() { return m_nzbInfo; }
	void				SetNzbInfo(NzbInfo* nzbInfo) { m_nzbInfo = nzbInfo; }
	Articles* 			GetArticles() { return &m_articles; }
	Groups* 			GetGroups() { return &m_groups; }
	const char*			GetSubject() { return m_subject; }
	void 				SetSubject(const char* subject);
	const char*			GetFilename() { return m_filename; }
	void 				SetFilename(const char* filename);
	void				MakeValidFilename();
	bool				GetFilenameConfirmed() { return m_filenameConfirmed; }
	void				SetFilenameConfirmed(bool filenameConfirmed) { m_filenameConfirmed = filenameConfirmed; }
	void 				SetSize(long long size) { m_size = size; m_remainingSize = size; }
	long long 			GetSize() { return m_size; }
	long long 			GetRemainingSize() { return m_remainingSize; }
	void 				SetRemainingSize(long long remainingSize) { m_remainingSize = remainingSize; }
	long long			GetMissedSize() { return m_missedSize; }
	void 				SetMissedSize(long long missedSize) { m_missedSize = missedSize; }
	long long			GetSuccessSize() { return m_successSize; }
	void 				SetSuccessSize(long long successSize) { m_successSize = successSize; }
	long long			GetFailedSize() { return m_failedSize; }
	void 				SetFailedSize(long long failedSize) { m_failedSize = failedSize; }
	int					GetTotalArticles() { return m_totalArticles; }
	void 				SetTotalArticles(int totalArticles) { m_totalArticles = totalArticles; }
	int					GetMissedArticles() { return m_missedArticles; }
	void 				SetMissedArticles(int missedArticles) { m_missedArticles = missedArticles; }
	int					GetFailedArticles() { return m_failedArticles; }
	void 				SetFailedArticles(int failedArticles) { m_failedArticles = failedArticles; }
	int					GetSuccessArticles() { return m_successArticles; }
	void 				SetSuccessArticles(int successArticles) { m_successArticles = successArticles; }
	time_t				GetTime() { return m_time; }
	void				SetTime(time_t time) { m_time = time; }
	bool				GetPaused() { return m_paused; }
	void				SetPaused(bool paused);
	bool				GetDeleted() { return m_deleted; }
	void				SetDeleted(bool Deleted) { m_deleted = Deleted; }
	int					GetCompletedArticles() { return m_completedArticles; }
	void				SetCompletedArticles(int completedArticles) { m_completedArticles = completedArticles; }
	bool				GetParFile() { return m_parFile; }
	void				SetParFile(bool parFile) { m_parFile = parFile; }
	void				ClearArticles();
	void				LockOutputFile();
	void				UnlockOutputFile();
	const char*			GetOutputFilename() { return m_outputFilename; }
	void 				SetOutputFilename(const char* outputFilename);
	bool				GetOutputInitialized() { return m_outputInitialized; }
	void				SetOutputInitialized(bool outputInitialized) { m_outputInitialized = outputInitialized; }
	bool				GetExtraPriority() { return m_extraPriority; }
	void				SetExtraPriority(bool extraPriority) { m_extraPriority = extraPriority; }
	int					GetActiveDownloads() { return m_activeDownloads; }
	void				SetActiveDownloads(int activeDownloads);
	bool				GetAutoDeleted() { return m_autoDeleted; }
	void				SetAutoDeleted(bool autoDeleted) { m_autoDeleted = autoDeleted; }
	int					GetCachedArticles() { return m_cachedArticles; }
	void				SetCachedArticles(int cachedArticles) { m_cachedArticles = cachedArticles; }
	bool				GetPartialChanged() { return m_partialChanged; }
	void				SetPartialChanged(bool partialChanged) { m_partialChanged = partialChanged; }
	ServerStatList*		GetServerStats() { return &m_serverStats; }
};
                              
typedef std::deque<FileInfo*> FileListBase;

class FileList : public FileListBase
{
private:
	bool				m_ownObjects;
public:
						FileList(bool ownObjects = false) { m_ownObjects = ownObjects; }
						~FileList();
	void				Clear();
	void				Remove(FileInfo* fileInfo);
};

class CompletedFile
{
public:
	enum EStatus
	{
		cfUnknown,
		cfSuccess,
		cfPartial,
		cfFailure
	};

private:
	int					m_id;
	char*				m_fileName;
	EStatus				m_status;
	unsigned long		m_crc;

public:
						CompletedFile(int id, const char* fileName, EStatus status, unsigned long crc);
						~CompletedFile();
	int					GetId() { return m_id; }
	void				SetFileName(const char* fileName);
	const char*			GetFileName() { return m_fileName; }
	EStatus				GetStatus() { return m_status; }
	unsigned long		GetCrc() { return m_crc; }
};

typedef std::deque<CompletedFile*>	CompletedFiles;

class NzbParameter
{
private:
	char* 				m_name;
	char* 				m_value;

	void				SetValue(const char* value);

	friend class NzbParameterList;

public:
						NzbParameter(const char* name);
						~NzbParameter();
	const char*			GetName() { return m_name; }
	const char*			GetValue() { return m_value; }
};

typedef std::deque<NzbParameter*> NzbParameterListBase;

class NzbParameterList : public NzbParameterListBase
{
public:
						~NzbParameterList();
	void				SetParameter(const char* name, const char* value);
	NzbParameter*		Find(const char* name, bool caseSensitive);
	void				Clear();
	void				CopyFrom(NzbParameterList* sourceParameters);
};

class ScriptStatus
{
public:
	enum EStatus
	{
		srNone,
		srFailure,
		srSuccess
	};

private:
	char* 				m_name;
	EStatus				m_status;

	friend class ScriptStatusList;
	
public:
						ScriptStatus(const char* name, EStatus status);
						~ScriptStatus();
	const char*			GetName() { return m_name; }
	EStatus				GetStatus() { return m_status; }
};

typedef std::deque<ScriptStatus*> ScriptStatusListBase;

class ScriptStatusList : public ScriptStatusListBase
{
public:
						~ScriptStatusList();
	void				Add(const char* scriptName, ScriptStatus::EStatus status);
	void				Clear();
	ScriptStatus::EStatus	CalcTotalStatus();
};

enum EDupeMode
{
	dmScore,
	dmAll,
	dmForce
};

class NzbInfo
{
public:
	enum ERenameStatus
	{
		rsNone,
		rsSkipped,
		rsFailure,
		rsSuccess
	};

	enum EParStatus
	{
		psNone,
		psSkipped,
		psFailure,
		psSuccess,
		psRepairPossible,
		psManual
	};

	enum EUnpackStatus
	{
		usNone,
		usSkipped,
		usFailure,
		usSuccess,
		usSpace,
		usPassword
	};

	enum ECleanupStatus
	{
		csNone,
		csFailure,
		csSuccess
	};

	enum EMoveStatus
	{
		msNone,
		msFailure,
		msSuccess
	};

	enum EDeleteStatus
	{
		dsNone,
		dsManual,
		dsHealth,
		dsDupe,
		dsBad,
		dsGood,
		dsCopy,
		dsScan
	};

	enum EMarkStatus
	{
		ksNone,
		ksBad,
		ksGood,
		ksSuccess
	};

	enum EUrlStatus
	{
		lsNone,
		lsRunning,
		lsFinished,
		lsFailed,
		lsRetry,
		lsScanSkipped,
		lsScanFailed
	};

	enum EKind
	{
		nkNzb,
		nkUrl
	};

	static const int FORCE_PRIORITY = 900;

	friend class DupInfo;

private:
	int					m_id;
	EKind				m_kind;
	char*				m_url;
	char* 				m_filename;
	char*				m_name;
	char* 				m_destDir;
	char* 				m_finalDir;
	char* 				m_category;
	int		 			m_fileCount;
	int		 			m_parkedFileCount;
	long long 			m_size;
	long long 			m_remainingSize;
	int					m_pausedFileCount;
	long long 			m_pausedSize;
	int					m_remainingParCount;
	int					m_activeDownloads;
	long long			m_successSize;
	long long			m_failedSize;
	long long			m_currentSuccessSize;
	long long			m_currentFailedSize;
	long long			m_parSize;
	long long			m_parSuccessSize;
	long long			m_parFailedSize;
	long long			m_parCurrentSuccessSize;
	long long			m_parCurrentFailedSize;
	int					m_totalArticles;
	int					m_successArticles;
	int					m_failedArticles;
	int					m_currentSuccessArticles;
	int					m_currentFailedArticles;
	time_t				m_minTime;
	time_t				m_maxTime;
	int					m_priority;
	CompletedFiles		m_completedFiles;
	ERenameStatus		m_renameStatus;
	EParStatus			m_parStatus;
	EUnpackStatus		m_unpackStatus;
	ECleanupStatus		m_cleanupStatus;
	EMoveStatus			m_moveStatus;
	EDeleteStatus		m_deleteStatus;
	EMarkStatus			m_markStatus;
	EUrlStatus			m_urlStatus;
	int					m_extraParBlocks;
	bool				m_addUrlPaused;
	bool				m_deletePaused;
	bool				m_manyDupeFiles;
	char*				m_queuedFilename;
	bool				m_deleting;
	bool				m_avoidHistory;
	bool				m_healthPaused;
	bool				m_parCleanup;
	bool				m_parManual;
	bool				m_cleanupDisk;
	bool				m_unpackCleanedUpDisk;
	char*				m_dupeKey;
	int					m_dupeScore;
	EDupeMode			m_dupeMode;
	unsigned int		m_fullContentHash;
	unsigned int		m_filteredContentHash;
	FileList			m_fileList;
	NzbParameterList	m_ppParameters;
	ScriptStatusList	m_scriptStatuses;
	ServerStatList		m_serverStats;
	ServerStatList		m_currentServerStats;
	Mutex				m_logMutex;
	MessageList			m_messages;
	int					m_idMessageGen;
	PostInfo*			m_postInfo;
	long long 			m_downloadedSize;
	time_t				m_downloadStartTime;
	int					m_downloadSec;
	int					m_postTotalSec;
	int					m_parSec;
	int					m_repairSec;
	int					m_unpackSec;
	bool				m_reprocess;
	time_t				m_queueScriptTime;
	bool				m_parFull;
	int					m_messageCount;
	int					m_cachedMessageCount;
	int					m_feedId;

	static int			m_idGen;
	static int			m_idMax;

	void				ClearMessages();

public:
						NzbInfo();
						~NzbInfo();
	int					GetId() { return m_id; }
	void				SetId(int id);
	static void			ResetGenId(bool max);
	static int			GenerateId();
	EKind				GetKind() { return m_kind; }
	void				SetKind(EKind kind) { m_kind = kind; }
	const char*			GetUrl() { return m_url; }			// needs locking (for shared objects)
	void				SetUrl(const char* url);				// needs locking (for shared objects)
	const char*			GetFilename() { return m_filename; }
	void				SetFilename(const char* filename);
	static void			MakeNiceNzbName(const char* nzbFilename, char* buffer, int size, bool removeExt);
	static void			MakeNiceUrlName(const char* url, const char* nzbFilename, char* buffer, int size);
	const char*			GetDestDir() { return m_destDir; }   // needs locking (for shared objects)
	void				SetDestDir(const char* destDir);     // needs locking (for shared objects)
	const char*			GetFinalDir() { return m_finalDir; }   // needs locking (for shared objects)
	void				SetFinalDir(const char* finalDir);     // needs locking (for shared objects)
	const char*			GetCategory() { return m_category; } // needs locking (for shared objects)
	void				SetCategory(const char* category);   // needs locking (for shared objects)
	const char*			GetName() { return m_name; } 	   // needs locking (for shared objects)
	void				SetName(const char* name);	   // needs locking (for shared objects)
	int					GetFileCount() { return m_fileCount; }
	void 				SetFileCount(int fileCount) { m_fileCount = fileCount; }
	int					GetParkedFileCount() { return m_parkedFileCount; }
	void 				SetParkedFileCount(int parkedFileCount) { m_parkedFileCount = parkedFileCount; }
	long long 			GetSize() { return m_size; }
	void 				SetSize(long long size) { m_size = size; }
	long long 			GetRemainingSize() { return m_remainingSize; }
	void	 			SetRemainingSize(long long remainingSize) { m_remainingSize = remainingSize; }
	long long 			GetPausedSize() { return m_pausedSize; }
	void	 			SetPausedSize(long long pausedSize) { m_pausedSize = pausedSize; }
	int					GetPausedFileCount() { return m_pausedFileCount; }
	void 				SetPausedFileCount(int pausedFileCount) { m_pausedFileCount = pausedFileCount; }
	int					GetRemainingParCount() { return m_remainingParCount; }
	void 				SetRemainingParCount(int remainingParCount) { m_remainingParCount = remainingParCount; }
	int					GetActiveDownloads() { return m_activeDownloads; }
	void				SetActiveDownloads(int activeDownloads);
	long long			GetSuccessSize() { return m_successSize; }
	void 				SetSuccessSize(long long successSize) { m_successSize = successSize; }
	long long			GetFailedSize() { return m_failedSize; }
	void 				SetFailedSize(long long failedSize) { m_failedSize = failedSize; }
	long long			GetCurrentSuccessSize() { return m_currentSuccessSize; }
	void 				SetCurrentSuccessSize(long long currentSuccessSize) { m_currentSuccessSize = currentSuccessSize; }
	long long			GetCurrentFailedSize() { return m_currentFailedSize; }
	void 				SetCurrentFailedSize(long long currentFailedSize) { m_currentFailedSize = currentFailedSize; }
	long long			GetParSize() { return m_parSize; }
	void 				SetParSize(long long parSize) { m_parSize = parSize; }
	long long			GetParSuccessSize() { return m_parSuccessSize; }
	void 				SetParSuccessSize(long long parSuccessSize) { m_parSuccessSize = parSuccessSize; }
	long long			GetParFailedSize() { return m_parFailedSize; }
	void 				SetParFailedSize(long long parFailedSize) { m_parFailedSize = parFailedSize; }
	long long			GetParCurrentSuccessSize() { return m_parCurrentSuccessSize; }
	void 				SetParCurrentSuccessSize(long long parCurrentSuccessSize) { m_parCurrentSuccessSize = parCurrentSuccessSize; }
	long long			GetParCurrentFailedSize() { return m_parCurrentFailedSize; }
	void 				SetParCurrentFailedSize(long long parCurrentFailedSize) { m_parCurrentFailedSize = parCurrentFailedSize; }
	int					GetTotalArticles() { return m_totalArticles; }
	void 				SetTotalArticles(int totalArticles) { m_totalArticles = totalArticles; }
	int					GetSuccessArticles() { return m_successArticles; }
	void 				SetSuccessArticles(int successArticles) { m_successArticles = successArticles; }
	int					GetFailedArticles() { return m_failedArticles; }
	void 				SetFailedArticles(int failedArticles) { m_failedArticles = failedArticles; }
	int					GetCurrentSuccessArticles() { return m_currentSuccessArticles; }
	void 				SetCurrentSuccessArticles(int currentSuccessArticles) { m_currentSuccessArticles = currentSuccessArticles; }
	int					GetCurrentFailedArticles() { return m_currentFailedArticles; }
	void 				SetCurrentFailedArticles(int currentFailedArticles) { m_currentFailedArticles = currentFailedArticles; }
	int					GetPriority() { return m_priority; }
	void				SetPriority(int priority) { m_priority = priority; }
	bool				GetForcePriority() { return m_priority >= FORCE_PRIORITY; }
	time_t				GetMinTime() { return m_minTime; }
	void				SetMinTime(time_t minTime) { m_minTime = minTime; }
	time_t				GetMaxTime() { return m_maxTime; }
	void				SetMaxTime(time_t maxTime) { m_maxTime = maxTime; }
	void				BuildDestDirName();
	void				BuildFinalDirName(char* finalDirBuf, int bufSize);
	CompletedFiles*		GetCompletedFiles() { return &m_completedFiles; }		// needs locking (for shared objects)
	void				ClearCompletedFiles();
	ERenameStatus		GetRenameStatus() { return m_renameStatus; }
	void				SetRenameStatus(ERenameStatus renameStatus) { m_renameStatus = renameStatus; }
	EParStatus			GetParStatus() { return m_parStatus; }
	void				SetParStatus(EParStatus parStatus) { m_parStatus = parStatus; }
	EUnpackStatus		GetUnpackStatus() { return m_unpackStatus; }
	void				SetUnpackStatus(EUnpackStatus unpackStatus) { m_unpackStatus = unpackStatus; }
	ECleanupStatus		GetCleanupStatus() { return m_cleanupStatus; }
	void				SetCleanupStatus(ECleanupStatus cleanupStatus) { m_cleanupStatus = cleanupStatus; }
	EMoveStatus			GetMoveStatus() { return m_moveStatus; }
	void				SetMoveStatus(EMoveStatus moveStatus) { m_moveStatus = moveStatus; }
	EDeleteStatus		GetDeleteStatus() { return m_deleteStatus; }
	void				SetDeleteStatus(EDeleteStatus deleteStatus) { m_deleteStatus = deleteStatus; }
	EMarkStatus			GetMarkStatus() { return m_markStatus; }
	void				SetMarkStatus(EMarkStatus markStatus) { m_markStatus = markStatus; }
	EUrlStatus			GetUrlStatus() { return m_urlStatus; }
	int					GetExtraParBlocks() { return m_extraParBlocks; }
	void				SetExtraParBlocks(int extraParBlocks) { m_extraParBlocks = extraParBlocks; }
	void				SetUrlStatus(EUrlStatus urlStatus) { m_urlStatus = urlStatus; }
	const char*			GetQueuedFilename() { return m_queuedFilename; }
	void				SetQueuedFilename(const char* queuedFilename);
	bool				GetDeleting() { return m_deleting; }
	void				SetDeleting(bool deleting) { m_deleting = deleting; }
	bool				GetDeletePaused() { return m_deletePaused; }
	void				SetDeletePaused(bool deletePaused) { m_deletePaused = deletePaused; }
	bool				GetManyDupeFiles() { return m_manyDupeFiles; }
	void				SetManyDupeFiles(bool manyDupeFiles) { m_manyDupeFiles = manyDupeFiles; }
	bool				GetAvoidHistory() { return m_avoidHistory; }
	void				SetAvoidHistory(bool avoidHistory) { m_avoidHistory = avoidHistory; }
	bool				GetHealthPaused() { return m_healthPaused; }
	void				SetHealthPaused(bool healthPaused) { m_healthPaused = healthPaused; }
	bool				GetParCleanup() { return m_parCleanup; }
	void				SetParCleanup(bool parCleanup) { m_parCleanup = parCleanup; }
	bool				GetCleanupDisk() { return m_cleanupDisk; }
	void				SetCleanupDisk(bool cleanupDisk) { m_cleanupDisk = cleanupDisk; }
	bool				GetUnpackCleanedUpDisk() { return m_unpackCleanedUpDisk; }
	void				SetUnpackCleanedUpDisk(bool unpackCleanedUpDisk) { m_unpackCleanedUpDisk = unpackCleanedUpDisk; }
	bool				GetAddUrlPaused() { return m_addUrlPaused; }
	void				SetAddUrlPaused(bool addUrlPaused) { m_addUrlPaused = addUrlPaused; }
	FileList*			GetFileList() { return &m_fileList; }					// needs locking (for shared objects)
	NzbParameterList*	GetParameters() { return &m_ppParameters; }				// needs locking (for shared objects)
	ScriptStatusList*	GetScriptStatuses() { return &m_scriptStatuses; }        // needs locking (for shared objects)
	ServerStatList*		GetServerStats() { return &m_serverStats; }
	ServerStatList*		GetCurrentServerStats() { return &m_currentServerStats; }
	int					CalcHealth();
	int					CalcCriticalHealth(bool allowEstimation);
	const char*			GetDupeKey() { return m_dupeKey; }					// needs locking (for shared objects)
	void				SetDupeKey(const char* dupeKey);						// needs locking (for shared objects)
	int					GetDupeScore() { return m_dupeScore; }
	void				SetDupeScore(int dupeScore) { m_dupeScore = dupeScore; }
	EDupeMode			GetDupeMode() { return m_dupeMode; }
	void				SetDupeMode(EDupeMode dupeMode) { m_dupeMode = dupeMode; }
	unsigned int		GetFullContentHash() { return m_fullContentHash; }
	void				SetFullContentHash(unsigned int fullContentHash) { m_fullContentHash = fullContentHash; }
	unsigned int		GetFilteredContentHash() { return m_filteredContentHash; }
	void				SetFilteredContentHash(unsigned int filteredContentHash) { m_filteredContentHash = filteredContentHash; }
	long long 			GetDownloadedSize() { return m_downloadedSize; }
	void 				SetDownloadedSize(long long downloadedSize) { m_downloadedSize = downloadedSize; }
	int					GetDownloadSec() { return m_downloadSec; }
	void 				SetDownloadSec(int downloadSec) { m_downloadSec = downloadSec; }
	int					GetPostTotalSec() { return m_postTotalSec; }
	void 				SetPostTotalSec(int postTotalSec) { m_postTotalSec = postTotalSec; }
	int					GetParSec() { return m_parSec; }
	void 				SetParSec(int parSec) { m_parSec = parSec; }
	int					GetRepairSec() { return m_repairSec; }
	void 				SetRepairSec(int repairSec) { m_repairSec = repairSec; }
	int					GetUnpackSec() { return m_unpackSec; }
	void 				SetUnpackSec(int unpackSec) { m_unpackSec = unpackSec; }
	time_t				GetDownloadStartTime() { return m_downloadStartTime; }
	void 				SetDownloadStartTime(time_t downloadStartTime) { m_downloadStartTime = downloadStartTime; }
	void				SetReprocess(bool reprocess) { m_reprocess = reprocess; }
	bool				GetReprocess() { return m_reprocess; }
	time_t				GetQueueScriptTime() { return m_queueScriptTime; }
	void 				SetQueueScriptTime(time_t queueScriptTime) { m_queueScriptTime = queueScriptTime; }
	void				SetParFull(bool parFull) { m_parFull = parFull; }
	bool				GetParFull() { return m_parFull; }
	int					GetFeedId() { return m_feedId; }
	void				SetFeedId(int feedId) { m_feedId = feedId; }

	void				CopyFileList(NzbInfo* srcNzbInfo);
	void				UpdateMinMaxTime();
	PostInfo*			GetPostInfo() { return m_postInfo; }
	void				EnterPostProcess();
	void				LeavePostProcess();
	bool				IsDupeSuccess();
	const char*			MakeTextStatus(bool ignoreScriptStatus);

	void				AddMessage(Message::EKind kind, const char* text);
	void				PrintMessage(Message::EKind kind, const char* format, ...);
	int					GetMessageCount() { return m_messageCount; }
	void				SetMessageCount(int messageCount) { m_messageCount = messageCount; }
	int					GetCachedMessageCount() { return m_cachedMessageCount; }
	MessageList*		LockCachedMessages();
	void				UnlockCachedMessages();
};

typedef std::deque<NzbInfo*> NzbQueueBase;

class NzbList : public NzbQueueBase
{
private:
	bool				m_ownObjects;
public:
						NzbList(bool ownObjects = false) { m_ownObjects = ownObjects; }
						~NzbList();
	void				Clear();
	void				Add(NzbInfo* nzbInfo, bool addTop);
	void				Remove(NzbInfo* nzbInfo);
	NzbInfo*			Find(int id);
};

class PostInfo
{
public:
	enum EStage
	{
		ptQueued,
		ptLoadingPars,
		ptVerifyingSources,
		ptRepairing,
		ptVerifyingRepaired,
		ptRenaming,
		ptUnpacking,
		ptMoving,
		ptExecutingScript,
		ptFinished
	};

	typedef std::vector<char*>		ParredFiles;

private:
	NzbInfo*			m_nzbInfo;
	bool				m_working;
	bool				m_deleted;
	bool				m_requestParCheck;
	bool				m_forceParFull;
	bool				m_forceRepair;
	bool				m_parRepaired;
	bool				m_unpackTried;
	bool				m_passListTried;
	int					m_lastUnpackStatus;
	EStage				m_stage;
	char*				m_progressLabel;
	int					m_fileProgress;
	int					m_stageProgress;
	time_t				m_startTime;
	time_t				m_stageTime;
	Thread*				m_postThread;
	
	ParredFiles			m_parredFiles;

public:
						PostInfo();
						~PostInfo();
	NzbInfo*			GetNzbInfo() { return m_nzbInfo; }
	void				SetNzbInfo(NzbInfo* nzbInfo) { m_nzbInfo = nzbInfo; }
	EStage				GetStage() { return m_stage; }
	void				SetStage(EStage stage) { m_stage = stage; }
	void				SetProgressLabel(const char* progressLabel);
	const char*			GetProgressLabel() { return m_progressLabel; }
	int					GetFileProgress() { return m_fileProgress; }
	void				SetFileProgress(int fileProgress) { m_fileProgress = fileProgress; }
	int					GetStageProgress() { return m_stageProgress; }
	void				SetStageProgress(int stageProgress) { m_stageProgress = stageProgress; }
	time_t				GetStartTime() { return m_startTime; }
	void				SetStartTime(time_t startTime) { m_startTime = startTime; }
	time_t				GetStageTime() { return m_stageTime; }
	void				SetStageTime(time_t stageTime) { m_stageTime = stageTime; }
	bool				GetWorking() { return m_working; }
	void				SetWorking(bool working) { m_working = working; }
	bool				GetDeleted() { return m_deleted; }
	void				SetDeleted(bool deleted) { m_deleted = deleted; }
	bool				GetRequestParCheck() { return m_requestParCheck; }
	void				SetRequestParCheck(bool requestParCheck) { m_requestParCheck = requestParCheck; }
	bool				GetForceParFull() { return m_forceParFull; }
	void				SetForceParFull(bool forceParFull) { m_forceParFull = forceParFull; }
	bool				GetForceRepair() { return m_forceRepair; }
	void				SetForceRepair(bool forceRepair) { m_forceRepair = forceRepair; }
	bool				GetParRepaired() { return m_parRepaired; }
	void				SetParRepaired(bool parRepaired) { m_parRepaired = parRepaired; }
	bool				GetUnpackTried() { return m_unpackTried; }
	void				SetUnpackTried(bool unpackTried) { m_unpackTried = unpackTried; }
	bool				GetPassListTried() { return m_passListTried; }
	void				SetPassListTried(bool passListTried) { m_passListTried = passListTried; }
	int					GetLastUnpackStatus() { return m_lastUnpackStatus; }
	void				SetLastUnpackStatus(int unpackStatus) { m_lastUnpackStatus = unpackStatus; }
	Thread*				GetPostThread() { return m_postThread; }
	void				SetPostThread(Thread* postThread) { m_postThread = postThread; }
	ParredFiles*		GetParredFiles() { return &m_parredFiles; }
};

typedef std::vector<int> IdList;

typedef std::vector<char*> NameList;

class DupInfo
{
public:
	enum EStatus
	{
		dsUndefined,
		dsSuccess,
		dsFailed,
		dsDeleted,
		dsDupe,
		dsBad,
		dsGood
	};

private:
	int					m_id;
	char*				m_name;
	char*				m_dupeKey;
	int					m_dupeScore;
	EDupeMode			m_dupeMode;
	long long 			m_size;
	unsigned int		m_fullContentHash;
	unsigned int		m_filteredContentHash;
	EStatus				m_status;

public:
						DupInfo();
						~DupInfo();
	int					GetId() { return m_id; }
	void				SetId(int id);
	const char*			GetName() { return m_name; }			// needs locking (for shared objects)
	void				SetName(const char* name);			// needs locking (for shared objects)
	const char*			GetDupeKey() { return m_dupeKey; }	// needs locking (for shared objects)
	void				SetDupeKey(const char* dupeKey);		// needs locking (for shared objects)
	int					GetDupeScore() { return m_dupeScore; }
	void				SetDupeScore(int dupeScore) { m_dupeScore = dupeScore; }
	EDupeMode			GetDupeMode() { return m_dupeMode; }
	void				SetDupeMode(EDupeMode dupeMode) { m_dupeMode = dupeMode; }
	long long			GetSize() { return m_size; }
	void 				SetSize(long long size) { m_size = size; }
	unsigned int		GetFullContentHash() { return m_fullContentHash; }
	void				SetFullContentHash(unsigned int fullContentHash) { m_fullContentHash = fullContentHash; }
	unsigned int		GetFilteredContentHash() { return m_filteredContentHash; }
	void				SetFilteredContentHash(unsigned int filteredContentHash) { m_filteredContentHash = filteredContentHash; }
	EStatus				GetStatus() { return m_status; }
	void				SetStatus(EStatus Status) { m_status = Status; }
};

class HistoryInfo
{
public:
	enum EKind
	{
		hkUnknown,
		hkNzb,
		hkUrl,
		hkDup
	};

private:
	EKind				m_kind;
	void*				m_info;
	time_t				m_time;

public:
						HistoryInfo(NzbInfo* nzbInfo);
						HistoryInfo(DupInfo* dupInfo);
						~HistoryInfo();
	EKind				GetKind() { return m_kind; }
	int					GetId();
	NzbInfo*			GetNzbInfo() { return (NzbInfo*)m_info; }
	DupInfo*			GetDupInfo() { return (DupInfo*)m_info; }
	void				DiscardNzbInfo() { m_info = NULL; }
	time_t				GetTime() { return m_time; }
	void				SetTime(time_t time) { m_time = time; }
	void				GetName(char* buffer, int size);		// needs locking (for shared objects)
};

typedef std::deque<HistoryInfo*> HistoryListBase;

class HistoryList : public HistoryListBase
{
public:
						~HistoryList();
	HistoryInfo*		Find(int id);
};

class DownloadQueue : public Subject
{
public:
	enum EAspectAction
	{
		eaNzbFound,
		eaNzbAdded,
		eaNzbDeleted,
		eaFileCompleted,
		eaFileDeleted,
		eaUrlCompleted
	};

	struct Aspect
	{
		EAspectAction action;
		DownloadQueue* downloadQueue;
		NzbInfo* nzbInfo;
		FileInfo* fileInfo;
	};

	enum EEditAction
	{
		eaFileMoveOffset = 1,	// move files to m_iOffset relative to the current position in download-queue
		eaFileMoveTop,			// move files to the top of download-queue
		eaFileMoveBottom,		// move files to the bottom of download-queue
		eaFilePause,			// pause files
		eaFileResume,			// resume (unpause) files
		eaFileDelete,			// delete files
		eaFilePauseAllPars,		// pause only (all) pars (does not affect other files)
		eaFilePauseExtraPars,	// pause only (almost all) pars, except main par-file (does not affect other files)
		eaFileReorder,			// set file order
		eaFileSplit,			// split - create new group from selected files
		eaGroupMoveOffset,		// move group to m_iOffset relative to the current position in download-queue
		eaGroupMoveTop,			// move group to the top of download-queue
		eaGroupMoveBottom,		// move group to the bottom of download-queue
		eaGroupPause,			// pause group
		eaGroupResume,			// resume (unpause) group
		eaGroupDelete,			// delete group and put to history
		eaGroupDupeDelete,		// delete group, put to history and mark as duplicate
		eaGroupFinalDelete,		// delete group without adding to history
		eaGroupPauseAllPars,	// pause only (all) pars (does not affect other files) in group
		eaGroupPauseExtraPars,	// pause only (almost all) pars in group, except main par-file (does not affect other files)
		eaGroupSetPriority,		// set priority for groups
		eaGroupSetCategory,		// set or change category for a group
		eaGroupApplyCategory,	// set or change category for a group and reassign pp-params according to category settings
		eaGroupMerge,			// merge groups
		eaGroupSetParameter,	// set post-process parameter for group
		eaGroupSetName,			// set group name (rename group)
		eaGroupSetDupeKey,		// set duplicate key
		eaGroupSetDupeScore,	// set duplicate score
		eaGroupSetDupeMode,		// set duplicate mode
		eaGroupSort,			// sort groups
		eaPostDelete,			// cancel post-processing
		eaHistoryDelete,		// hide history-item
		eaHistoryFinalDelete,	// delete history-item
		eaHistoryReturn,		// move history-item back to download queue
		eaHistoryProcess,		// move history-item back to download queue and start postprocessing
		eaHistoryRedownload,	// move history-item back to download queue for redownload
		eaHistorySetParameter,	// set post-process parameter for history-item
		eaHistorySetDupeKey,	// set duplicate key
		eaHistorySetDupeScore,	// set duplicate score
		eaHistorySetDupeMode,	// set duplicate mode
		eaHistorySetDupeBackup,	// set duplicate backup flag
		eaHistoryMarkBad,		// mark history-item as bad (and download other duplicate)
		eaHistoryMarkGood,		// mark history-item as good (and push it into dup-history)
		eaHistoryMarkSuccess,	// mark history-item as success (and do nothing more)
		eaHistorySetCategory,	// set or change category for history-item
		eaHistorySetName		// set history-item name (rename)
	};

	enum EMatchMode
	{
		mmId = 1,
		mmName,
		mmRegEx
	};

private:
	NzbList					m_queue;
	HistoryList				m_history;
	Mutex	 				m_lockMutex;

	static DownloadQueue*	g_pDownloadQueue;
	static bool				g_bLoaded;

protected:
							DownloadQueue() : m_queue(true) {}
	static void				Init(DownloadQueue* globalInstance) { g_pDownloadQueue = globalInstance; }
	static void				Final() { g_pDownloadQueue = NULL; }
	static void				Loaded() { g_bLoaded = true; }

public:
	static bool				IsLoaded() { return g_bLoaded; }
	static DownloadQueue*	Lock();
	static void				Unlock();
	NzbList*				GetQueue() { return &m_queue; }
	HistoryList*			GetHistory() { return &m_history; }
	virtual bool			EditEntry(int ID, EEditAction action, int offset, const char* text) = 0;
	virtual bool			EditList(IdList* idList, NameList* nameList, EMatchMode matchMode, EEditAction action, int offset, const char* text) = 0;
	virtual void			Save() = 0;
	void					CalcRemainingSize(long long* remaining, long long* remainingForced);
};

#endif
