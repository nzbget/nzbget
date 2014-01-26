/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include "Log.h"
#include "Thread.h"

class NZBInfo;
class DownloadQueue;

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
	int					m_iPartNumber;
	char*				m_szMessageID;
	int					m_iSize;
	EStatus				m_eStatus;
	char*				m_szResultFilename;

public:
						ArticleInfo();
						~ArticleInfo();
	void 				SetPartNumber(int s) { m_iPartNumber = s; }
	int 				GetPartNumber() { return m_iPartNumber; }
	const char* 		GetMessageID() { return m_szMessageID; }
	void 				SetMessageID(const char* szMessageID);
	void 				SetSize(int s) { m_iSize = s; }
	int 				GetSize() { return m_iSize; }
	EStatus				GetStatus() { return m_eStatus; }
	void				SetStatus(EStatus Status) { m_eStatus = Status; }
	const char*			GetResultFilename() { return m_szResultFilename; }
	void 				SetResultFilename(const char* v);
};

class FileInfo
{
public:
	typedef std::vector<ArticleInfo*>	Articles;
	typedef std::vector<char*>			Groups;

private:
	int					m_iID;
	NZBInfo*			m_pNZBInfo;
	Articles			m_Articles;
	Groups				m_Groups;
	char* 				m_szSubject;
	char*				m_szFilename;
	long long 			m_lSize;
	long long 			m_lRemainingSize;
	long long			m_lSuccessSize;
	long long			m_lFailedSize;
	long long			m_lMissedSize;
	int					m_iTotalArticles;
	int					m_iMissedArticles;
	int					m_iFailedArticles;
	int					m_iSuccessArticles;
	time_t				m_tTime;
	bool				m_bPaused;
	bool				m_bDeleted;
	bool				m_bFilenameConfirmed;
	bool				m_bParFile;
	int					m_iCompletedArticles;
	bool				m_bOutputInitialized;
	char*				m_szOutputFilename;
	Mutex*				m_pMutexOutputFile;
	int					m_iPriority;
	bool				m_bExtraPriority;
	int					m_iActiveDownloads;
	bool				m_bAutoDeleted;

	static int			m_iIDGen;
	static int			m_iIDMax;

public:
						FileInfo();
						~FileInfo();
	int					GetID() { return m_iID; }
	void				SetID(int iID);
	static void			ResetGenID(bool bMax);
	NZBInfo*			GetNZBInfo() { return m_pNZBInfo; }
	void				SetNZBInfo(NZBInfo* pNZBInfo) { m_pNZBInfo = pNZBInfo; }
	Articles* 			GetArticles() { return &m_Articles; }
	Groups* 			GetGroups() { return &m_Groups; }
	const char*			GetSubject() { return m_szSubject; }
	void 				SetSubject(const char* szSubject);
	const char*			GetFilename() { return m_szFilename; }
	void 				SetFilename(const char* szFilename);
	void				MakeValidFilename();
	bool				GetFilenameConfirmed() { return m_bFilenameConfirmed; }
	void				SetFilenameConfirmed(bool bFilenameConfirmed) { m_bFilenameConfirmed = bFilenameConfirmed; }
	void 				SetSize(long long lSize) { m_lSize = lSize; m_lRemainingSize = lSize; }
	long long 			GetSize() { return m_lSize; }
	long long 			GetRemainingSize() { return m_lRemainingSize; }
	void 				SetRemainingSize(long long lRemainingSize) { m_lRemainingSize = lRemainingSize; }
	long long			GetMissedSize() { return m_lMissedSize; }
	void 				SetMissedSize(long long lMissedSize) { m_lMissedSize = lMissedSize; }
	long long			GetSuccessSize() { return m_lSuccessSize; }
	void 				SetSuccessSize(long long lSuccessSize) { m_lSuccessSize = lSuccessSize; }
	long long			GetFailedSize() { return m_lFailedSize; }
	void 				SetFailedSize(long long lFailedSize) { m_lFailedSize = lFailedSize; }
	int					GetTotalArticles() { return m_iTotalArticles; }
	void 				SetTotalArticles(int iTotalArticles) { m_iTotalArticles = iTotalArticles; }
	int					GetMissedArticles() { return m_iMissedArticles; }
	void 				SetMissedArticles(int iMissedArticles) { m_iMissedArticles = iMissedArticles; }
	int					GetFailedArticles() { return m_iFailedArticles; }
	void 				SetFailedArticles(int iFailedArticles) { m_iFailedArticles = iFailedArticles; }
	int					GetSuccessArticles() { return m_iSuccessArticles; }
	void 				SetSuccessArticles(int iSuccessArticles) { m_iSuccessArticles = iSuccessArticles; }
	time_t				GetTime() { return m_tTime; }
	void				SetTime(time_t tTime) { m_tTime = tTime; }
	bool				GetPaused() { return m_bPaused; }
	void				SetPaused(bool bPaused);
	bool				GetDeleted() { return m_bDeleted; }
	void				SetDeleted(bool Deleted) { m_bDeleted = Deleted; }
	int					GetCompletedArticles() { return m_iCompletedArticles; }
	void				SetCompletedArticles(int iCompletedArticles) { m_iCompletedArticles = iCompletedArticles; }
	bool				GetParFile() { return m_bParFile; }
	void				SetParFile(bool bParFile) { m_bParFile = bParFile; }
	void				ClearArticles();
	void				LockOutputFile();
	void				UnlockOutputFile();
	const char*			GetOutputFilename() { return m_szOutputFilename; }
	void 				SetOutputFilename(const char* szOutputFilename);
	bool				GetOutputInitialized() { return m_bOutputInitialized; }
	void				SetOutputInitialized(bool bOutputInitialized) { m_bOutputInitialized = bOutputInitialized; }
	int					GetPriority() { return m_iPriority; }
	void				SetPriority(int iPriority) { m_iPriority = iPriority; }
	bool				GetExtraPriority() { return m_bExtraPriority; }
	void				SetExtraPriority(bool bExtraPriority) { m_bExtraPriority = bExtraPriority; };
	int					GetActiveDownloads() { return m_iActiveDownloads; }
	void				SetActiveDownloads(int iActiveDownloads);
	bool				GetAutoDeleted() { return m_bAutoDeleted; }
	void				SetAutoDeleted(bool bAutoDeleted) { m_bAutoDeleted = bAutoDeleted; }
};
                              
typedef std::deque<FileInfo*> FileListBase;

class FileList : public FileListBase
{
private:
	bool				m_bOwnObjects;
public:
						FileList(bool bOwnObjects = false) { m_bOwnObjects = bOwnObjects; }
						~FileList();
	void				Clear();
	void				Remove(FileInfo* pFileInfo);
};


class NZBParameter
{
private:
	char* 				m_szName;
	char* 				m_szValue;

	void				SetValue(const char* szValue);

	friend class NZBParameterList;

public:
						NZBParameter(const char* szName);
						~NZBParameter();
	const char*			GetName() { return m_szName; }
	const char*			GetValue() { return m_szValue; }
};

typedef std::deque<NZBParameter*> NZBParameterListBase;

class NZBParameterList : public NZBParameterListBase
{
public:
						~NZBParameterList();
	void				SetParameter(const char* szName, const char* szValue);
	NZBParameter*		Find(const char* szName, bool bCaseSensitive);
	void				Clear();
	void				CopyFrom(NZBParameterList* pSourceParameters);
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
	char* 				m_szName;
	EStatus				m_eStatus;
	
	friend class ScriptStatusList;
	
public:
						ScriptStatus(const char* szName, EStatus eStatus);
						~ScriptStatus();
	const char*			GetName() { return m_szName; }
	EStatus				GetStatus() { return m_eStatus; }
};

typedef std::deque<ScriptStatus*> ScriptStatusListBase;

class ScriptStatusList : public ScriptStatusListBase
{
public:
						~ScriptStatusList();
	void				Add(const char* szScriptName, ScriptStatus::EStatus eStatus);
	void				Clear();
	ScriptStatus::EStatus	CalcTotalStatus();
};

class ServerStat
{
private:
	int					m_iServerID;
	int					m_iSuccessArticles;
	int					m_iFailedArticles;

public:
						ServerStat(int iServerID);
	int					GetServerID() { return m_iServerID; }
	int					GetSuccessArticles() { return m_iSuccessArticles; }
	void				SetSuccessArticles(int iSuccessArticles) { m_iSuccessArticles = iSuccessArticles; }
	int					GetFailedArticles() { return m_iFailedArticles; }
	void				SetFailedArticles(int iFailedArticles) { m_iFailedArticles = iFailedArticles; }
};

typedef std::vector<ServerStat*>	ServerStatListBase;

class ServerStatList : public ServerStatListBase
{
public:
						~ServerStatList();
	void				SetStat(int iServerID, int iSuccessArticles, int iFailedArticles, bool bAdd);
	void				Add(ServerStatList* pServerStats);
	void				Clear();
};

enum EDupeMode
{
	dmScore,
	dmAll,
	dmForce
};

class NZBInfo
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
		dsDupe
	};

	enum EMarkStatus
	{
		ksNone,
		ksBad,
		ksGood
	};

	typedef std::vector<char*>			Files;
	typedef std::deque<Message*>		Messages;

private:
	int					m_iID;
	char* 				m_szFilename;
	char*				m_szName;
	char* 				m_szDestDir;
	char* 				m_szFinalDir;
	char* 				m_szCategory;
	int		 			m_iFileCount;
	int		 			m_iParkedFileCount;
	long long 			m_lSize;
	long long 			m_lRemainingSize;
	int					m_iPausedFileCount;
	long long 			m_lPausedSize;
	int					m_iRemainingParCount;
	int					m_iActiveDownloads;
	long long			m_lSuccessSize;
	long long			m_lFailedSize;
	long long			m_lCurrentSuccessSize;
	long long			m_lCurrentFailedSize;
	long long			m_lParSize;
	long long			m_lParSuccessSize;
	long long			m_lParFailedSize;
	long long			m_lParCurrentSuccessSize;
	long long			m_lParCurrentFailedSize;
	int					m_iTotalArticles;
	int					m_iSuccessArticles;
	int					m_iFailedArticles;
	Files				m_completedFiles;
	bool				m_bPostProcess;
	ERenameStatus		m_eRenameStatus;
	EParStatus			m_eParStatus;
	EUnpackStatus		m_eUnpackStatus;
	ECleanupStatus		m_eCleanupStatus;
	EMoveStatus			m_eMoveStatus;
	EDeleteStatus		m_eDeleteStatus;
	EMarkStatus			m_eMarkStatus;
	bool				m_bDeletePaused;
	bool				m_bManyDupeFiles;
	char*				m_szQueuedFilename;
	bool				m_bDeleting;
	bool				m_bAvoidHistory;
	bool				m_bHealthPaused;
	bool				m_bParCleanup;
	bool				m_bParManual;
	bool				m_bCleanupDisk;
	bool				m_bUnpackCleanedUpDisk;
	char*				m_szDupeKey;
	int					m_iDupeScore;
	EDupeMode			m_eDupeMode;
	unsigned int		m_iFullContentHash;
	unsigned int		m_iFilteredContentHash;
	FileList			m_FileList;
	NZBParameterList	m_ppParameters;
	ScriptStatusList	m_scriptStatuses;
	ServerStatList		m_ServerStats;
	Mutex				m_mutexLog;
	Messages			m_Messages;
	int					m_iIDMessageGen;

	// File statistics
	time_t				m_tMinTime;
	time_t				m_tMaxTime;
	int					m_iMinPriority;
	int					m_iMaxPriority;

	static int			m_iIDGen;
	static int			m_iIDMax;

public:
						NZBInfo(bool bPersistent = true);
						~NZBInfo();
	int					GetID() { return m_iID; }
	void				SetID(int iID);
	static void			ResetGenID(bool bMax);
	const char*			GetFilename() { return m_szFilename; }
	void				SetFilename(const char* szFilename);
	static void			MakeNiceNZBName(const char* szNZBFilename, char* szBuffer, int iSize, bool bRemoveExt);
	const char*			GetDestDir() { return m_szDestDir; }   // needs locking (for shared objects)
	void				SetDestDir(const char* szDestDir);     // needs locking (for shared objects)
	const char*			GetFinalDir() { return m_szFinalDir; }   // needs locking (for shared objects)
	void				SetFinalDir(const char* szFinalDir);     // needs locking (for shared objects)
	const char*			GetCategory() { return m_szCategory; } // needs locking (for shared objects)
	void				SetCategory(const char* szCategory);   // needs locking (for shared objects)
	const char*			GetName() { return m_szName; } 	   // needs locking (for shared objects)
	void				SetName(const char* szName);	   // needs locking (for shared objects)
	int					GetFileCount() { return m_iFileCount; }
	void 				SetFileCount(int iFileCount) { m_iFileCount = iFileCount; }
	int					GetParkedFileCount() { return m_iParkedFileCount; }
	void 				SetParkedFileCount(int iParkedFileCount) { m_iParkedFileCount = iParkedFileCount; }
	long long 			GetSize() { return m_lSize; }
	void 				SetSize(long long lSize) { m_lSize = lSize; }
	long long 			GetRemainingSize() { return m_lRemainingSize; }
	void	 			SetRemainingSize(long long lRemainingSize) { m_lRemainingSize = lRemainingSize; }
	long long 			GetPausedSize() { return m_lPausedSize; }
	void	 			SetPausedSize(long long lPausedSize) { m_lPausedSize = lPausedSize; }
	int					GetPausedFileCount() { return m_iPausedFileCount; }
	void 				SetPausedFileCount(int iPausedFileCount) { m_iPausedFileCount = iPausedFileCount; }
	int					GetRemainingParCount() { return m_iRemainingParCount; }
	void 				SetRemainingParCount(int iRemainingParCount) { m_iRemainingParCount = iRemainingParCount; }
	int					GetActiveDownloads() { return m_iActiveDownloads; }
	void				SetActiveDownloads(int iActiveDownloads) { m_iActiveDownloads = iActiveDownloads; }
	long long			GetSuccessSize() { return m_lSuccessSize; }
	void 				SetSuccessSize(long long lSuccessSize) { m_lSuccessSize = lSuccessSize; }
	long long			GetFailedSize() { return m_lFailedSize; }
	void 				SetFailedSize(long long lFailedSize) { m_lFailedSize = lFailedSize; }
	long long			GetCurrentSuccessSize() { return m_lCurrentSuccessSize; }
	void 				SetCurrentSuccessSize(long long lCurrentSuccessSize) { m_lCurrentSuccessSize = lCurrentSuccessSize; }
	long long			GetCurrentFailedSize() { return m_lCurrentFailedSize; }
	void 				SetCurrentFailedSize(long long lCurrentFailedSize) { m_lCurrentFailedSize = lCurrentFailedSize; }
	long long			GetParSize() { return m_lParSize; }
	void 				SetParSize(long long lParSize) { m_lParSize = lParSize; }
	long long			GetParSuccessSize() { return m_lParSuccessSize; }
	void 				SetParSuccessSize(long long lParSuccessSize) { m_lParSuccessSize = lParSuccessSize; }
	long long			GetParFailedSize() { return m_lParFailedSize; }
	void 				SetParFailedSize(long long lParFailedSize) { m_lParFailedSize = lParFailedSize; }
	long long			GetParCurrentSuccessSize() { return m_lParCurrentSuccessSize; }
	void 				SetParCurrentSuccessSize(long long lParCurrentSuccessSize) { m_lParCurrentSuccessSize = lParCurrentSuccessSize; }
	long long			GetParCurrentFailedSize() { return m_lParCurrentFailedSize; }
	void 				SetParCurrentFailedSize(long long lParCurrentFailedSize) { m_lParCurrentFailedSize = lParCurrentFailedSize; }
	int					GetTotalArticles() { return m_iTotalArticles; }
	void 				SetTotalArticles(int iTotalArticles) { m_iTotalArticles = iTotalArticles; }
	int					GetSuccessArticles() { return m_iSuccessArticles; }
	void 				SetSuccessArticles(int iSuccessArticles) { m_iSuccessArticles = iSuccessArticles; }
	int					GetFailedArticles() { return m_iFailedArticles; }
	void 				SetFailedArticles(int iFailedArticles) { m_iFailedArticles = iFailedArticles; }
	void				BuildDestDirName();
	void				BuildFinalDirName(char* szFinalDirBuf, int iBufSize);
	Files*				GetCompletedFiles() { return &m_completedFiles; }		// needs locking (for shared objects)
	void				ClearCompletedFiles();
	bool				GetPostProcess() { return m_bPostProcess; }
	void				SetPostProcess(bool bPostProcess) { m_bPostProcess = bPostProcess; }
	ERenameStatus		GetRenameStatus() { return m_eRenameStatus; }
	void				SetRenameStatus(ERenameStatus eRenameStatus) { m_eRenameStatus = eRenameStatus; }
	EParStatus			GetParStatus() { return m_eParStatus; }
	void				SetParStatus(EParStatus eParStatus) { m_eParStatus = eParStatus; }
	EUnpackStatus		GetUnpackStatus() { return m_eUnpackStatus; }
	void				SetUnpackStatus(EUnpackStatus eUnpackStatus) { m_eUnpackStatus = eUnpackStatus; }
	ECleanupStatus		GetCleanupStatus() { return m_eCleanupStatus; }
	void				SetCleanupStatus(ECleanupStatus eCleanupStatus) { m_eCleanupStatus = eCleanupStatus; }
	EMoveStatus			GetMoveStatus() { return m_eMoveStatus; }
	void				SetMoveStatus(EMoveStatus eMoveStatus) { m_eMoveStatus = eMoveStatus; }
	EDeleteStatus		GetDeleteStatus() { return m_eDeleteStatus; }
	void				SetDeleteStatus(EDeleteStatus eDeleteStatus) { m_eDeleteStatus = eDeleteStatus; }
	EMarkStatus			GetMarkStatus() { return m_eMarkStatus; }
	void				SetMarkStatus(EMarkStatus eMarkStatus) { m_eMarkStatus = eMarkStatus; }
	const char*			GetQueuedFilename() { return m_szQueuedFilename; }
	void				SetQueuedFilename(const char* szQueuedFilename);
	bool				GetDeleting() { return m_bDeleting; }
	void				SetDeleting(bool bDeleting) { m_bDeleting = bDeleting; }
	bool				GetDeletePaused() { return m_bDeletePaused; }
	void				SetDeletePaused(bool bDeletePaused) { m_bDeletePaused = bDeletePaused; }
	bool				GetManyDupeFiles() { return m_bManyDupeFiles; }
	void				SetManyDupeFiles(bool bManyDupeFiles) { m_bManyDupeFiles = bManyDupeFiles; }
	bool				GetAvoidHistory() { return m_bAvoidHistory; }
	void				SetAvoidHistory(bool bAvoidHistory) { m_bAvoidHistory = bAvoidHistory; }
	bool				GetHealthPaused() { return m_bHealthPaused; }
	void				SetHealthPaused(bool bHealthPaused) { m_bHealthPaused = bHealthPaused; }
	bool				GetParCleanup() { return m_bParCleanup; }
	void				SetParCleanup(bool bParCleanup) { m_bParCleanup = bParCleanup; }
	bool				GetCleanupDisk() { return m_bCleanupDisk; }
	void				SetCleanupDisk(bool bCleanupDisk) { m_bCleanupDisk = bCleanupDisk; }
	bool				GetUnpackCleanedUpDisk() { return m_bUnpackCleanedUpDisk; }
	void				SetUnpackCleanedUpDisk(bool bUnpackCleanedUpDisk) { m_bUnpackCleanedUpDisk = bUnpackCleanedUpDisk; }
	FileList*			GetFileList() { return &m_FileList; }					// needs locking (for shared objects)
	NZBParameterList*	GetParameters() { return &m_ppParameters; }				// needs locking (for shared objects)
	ScriptStatusList*	GetScriptStatuses() { return &m_scriptStatuses; }        // needs locking (for shared objects)
	ServerStatList*		GetServerStats() { return &m_ServerStats; }
	int					CalcHealth();
	int					CalcCriticalHealth();
	const char*			GetDupeKey() { return m_szDupeKey; }					// needs locking (for shared objects)
	void				SetDupeKey(const char* szDupeKey);						// needs locking (for shared objects)
	int					GetDupeScore() { return m_iDupeScore; }
	void				SetDupeScore(int iDupeScore) { m_iDupeScore = iDupeScore; }
	EDupeMode			GetDupeMode() { return m_eDupeMode; }
	void				SetDupeMode(EDupeMode eDupeMode) { m_eDupeMode = eDupeMode; }
	unsigned int		GetFullContentHash() { return m_iFullContentHash; }
	void				SetFullContentHash(unsigned int iFullContentHash) { m_iFullContentHash = iFullContentHash; }
	unsigned int		GetFilteredContentHash() { return m_iFilteredContentHash; }
	void				SetFilteredContentHash(unsigned int iFilteredContentHash) { m_iFilteredContentHash = iFilteredContentHash; }
	int					GetGroupID();
	void				CopyFileList(NZBInfo* pSrcNZBInfo);

	// File statistics
	void				CalcFileStats();
	time_t				GetMinTime() { return m_tMinTime; }
	time_t				GetMaxTime() { return m_tMaxTime; }
	int					GetMinPriority() { return m_iMinPriority; }
	int					GetMaxPriority() { return m_iMaxPriority; }

	void				AppendMessage(Message::EKind eKind, time_t tTime, const char* szText);
	Messages*			LockMessages();
	void				UnlockMessages();
};

typedef std::deque<NZBInfo*> NZBQueueBase;

class NZBList : public NZBQueueBase
{
private:
	bool				m_bOwnObjects;
public:
						NZBList(bool bOwnObjects = false) { m_bOwnObjects = bOwnObjects; }
						~NZBList();
	void				Clear();
	void				Remove(NZBInfo* pNZBInfo);
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

	typedef std::deque<Message*>	Messages;

private:
	int					m_iID;
	NZBInfo*			m_pNZBInfo;
	char*				m_szInfoName;
	bool				m_bWorking;
	bool				m_bDeleted;
	bool				m_bRequestParCheck;
	EStage				m_eStage;
	char*				m_szProgressLabel;
	int					m_iFileProgress;
	int					m_iStageProgress;
	time_t				m_tStartTime;
	time_t				m_tStageTime;
	Thread*				m_pPostThread;
	
	Mutex				m_mutexLog;
	Messages			m_Messages;
	int					m_iIDMessageGen;

	static int			m_iIDGen;
	static int			m_iIDMax;

public:
						PostInfo();
						~PostInfo();
	int					GetID() { return m_iID; }
	NZBInfo*			GetNZBInfo() { return m_pNZBInfo; }
	void				SetNZBInfo(NZBInfo* pNZBInfo) { m_pNZBInfo = pNZBInfo; }
	const char*			GetInfoName() { return m_szInfoName; }
	void				SetInfoName(const char* szInfoName);
	EStage				GetStage() { return m_eStage; }
	void				SetStage(EStage eStage) { m_eStage = eStage; }
	void				SetProgressLabel(const char* szProgressLabel);
	const char*			GetProgressLabel() { return m_szProgressLabel; }
	int					GetFileProgress() { return m_iFileProgress; }
	void				SetFileProgress(int iFileProgress) { m_iFileProgress = iFileProgress; }
	int					GetStageProgress() { return m_iStageProgress; }
	void				SetStageProgress(int iStageProgress) { m_iStageProgress = iStageProgress; }
	time_t				GetStartTime() { return m_tStartTime; }
	void				SetStartTime(time_t tStartTime) { m_tStartTime = tStartTime; }
	time_t				GetStageTime() { return m_tStageTime; }
	void				SetStageTime(time_t tStageTime) { m_tStageTime = tStageTime; }
	bool				GetWorking() { return m_bWorking; }
	void				SetWorking(bool bWorking) { m_bWorking = bWorking; }
	bool				GetDeleted() { return m_bDeleted; }
	void				SetDeleted(bool bDeleted) { m_bDeleted = bDeleted; }
	bool				GetRequestParCheck() { return m_bRequestParCheck; }
	void				SetRequestParCheck(bool bRequestParCheck) { m_bRequestParCheck = bRequestParCheck; }
	void				AppendMessage(Message::EKind eKind, const char* szText);
	Thread*				GetPostThread() { return m_pPostThread; }
	void				SetPostThread(Thread* pPostThread) { m_pPostThread = pPostThread; }
	Messages*			LockMessages();
	void				UnlockMessages();
};

typedef std::deque<PostInfo*> PostQueue;

typedef std::vector<int> IDList;

typedef std::vector<char*> NameList;

class UrlInfo
{
public:
	enum EStatus
	{
		aiUndefined,
		aiRunning,
		aiFinished,
		aiFailed,
		aiRetry,
		aiScanSkipped,
		aiScanFailed
	};

private:
	int					m_iID;
	char*				m_szURL;
	char*				m_szNZBFilename;
	char* 				m_szCategory;
	int					m_iPriority;
	char*				m_szDupeKey;
	int					m_iDupeScore;
	EDupeMode			m_eDupeMode;
	bool				m_bAddTop;
	bool				m_bAddPaused;
	bool				m_bForce;
	EStatus				m_eStatus;

	static int			m_iIDGen;
	static int			m_iIDMax;

public:
						UrlInfo();
						~UrlInfo();
	int					GetID() { return m_iID; }
	void				SetID(int iID);
	static void			ResetGenID(bool bMax);
	const char*			GetURL() { return m_szURL; }			// needs locking (for shared objects)
	void				SetURL(const char* szURL);				// needs locking (for shared objects)
	const char*			GetNZBFilename() { return m_szNZBFilename; }		// needs locking (for shared objects)
	void				SetNZBFilename(const char* szNZBFilename);			// needs locking (for shared objects)
	const char*			GetCategory() { return m_szCategory; }	// needs locking (for shared objects)
	void				SetCategory(const char* szCategory);	// needs locking (for shared objects)
	int					GetPriority() { return m_iPriority; }
	void				SetPriority(int iPriority) { m_iPriority = iPriority; }
	const char*			GetDupeKey() { return m_szDupeKey; }
	void				SetDupeKey(const char* szDupeKey);
	int					GetDupeScore() { return m_iDupeScore; }
	void				SetDupeScore(int iDupeScore) { m_iDupeScore = iDupeScore; }
	EDupeMode			GetDupeMode() { return m_eDupeMode; }
	void				SetDupeMode(EDupeMode eDupeMode) { m_eDupeMode = eDupeMode; }
	bool				GetAddTop() { return m_bAddTop; }
	void				SetAddTop(bool bAddTop) { m_bAddTop = bAddTop; }
	bool				GetAddPaused() { return m_bAddPaused; }
	void				SetAddPaused(bool bAddPaused) { m_bAddPaused = bAddPaused; }
	void				GetName(char* szBuffer, int iSize);		// needs locking (for shared objects)
	static void			MakeNiceName(const char* szURL, const char* szNZBFilename, char* szBuffer, int iSize);
	bool				GetForce() { return m_bForce; }
	void				SetForce(bool bForce) { m_bForce = bForce; }
	EStatus				GetStatus() { return m_eStatus; }
	void				SetStatus(EStatus Status) { m_eStatus = Status; }
};

typedef std::deque<UrlInfo*> UrlQueue;

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
	char*				m_szName;
	char*				m_szDupeKey;
	int					m_iDupeScore;
	EDupeMode			m_eDupeMode;
	long long 			m_lSize;
	unsigned int		m_iFullContentHash;
	unsigned int		m_iFilteredContentHash;
	EStatus				m_eStatus;

public:
						DupInfo();
						~DupInfo();
	const char*			GetName() { return m_szName; }			// needs locking (for shared objects)
	void				SetName(const char* szName);			// needs locking (for shared objects)
	const char*			GetDupeKey() { return m_szDupeKey; }	// needs locking (for shared objects)
	void				SetDupeKey(const char* szDupeKey);		// needs locking (for shared objects)
	int					GetDupeScore() { return m_iDupeScore; }
	void				SetDupeScore(int iDupeScore) { m_iDupeScore = iDupeScore; }
	EDupeMode			GetDupeMode() { return m_eDupeMode; }
	void				SetDupeMode(EDupeMode eDupeMode) { m_eDupeMode = eDupeMode; }
	long long			GetSize() { return m_lSize; }
	void 				SetSize(long long lSize) { m_lSize = lSize; }
	unsigned int		GetFullContentHash() { return m_iFullContentHash; }
	void				SetFullContentHash(unsigned int iFullContentHash) { m_iFullContentHash = iFullContentHash; }
	unsigned int		GetFilteredContentHash() { return m_iFilteredContentHash; }
	void				SetFilteredContentHash(unsigned int iFilteredContentHash) { m_iFilteredContentHash = iFilteredContentHash; }
	EStatus				GetStatus() { return m_eStatus; }
	void				SetStatus(EStatus Status) { m_eStatus = Status; }
};

class HistoryInfo
{
public:
	enum EKind
	{
		hkUnknown,
		hkNZBInfo,
		hkUrlInfo,
		hkDupInfo
	};

private:
	int					m_iID;
	EKind				m_eKind;
	void*				m_pInfo;
	time_t				m_tTime;

	static int			m_iIDGen;
	static int			m_iIDMax;

public:
						HistoryInfo(NZBInfo* pNZBInfo);
						HistoryInfo(UrlInfo* pUrlInfo);
						HistoryInfo(DupInfo* pDupInfo);
						~HistoryInfo();
	int					GetID() { return m_iID; }
	void				SetID(int iID);
	static void			ResetGenID(bool bMax);
	EKind				GetKind() { return m_eKind; }
	NZBInfo*			GetNZBInfo() { return (NZBInfo*)m_pInfo; }
	UrlInfo*			GetUrlInfo() { return (UrlInfo*)m_pInfo; }
	DupInfo*			GetDupInfo() { return (DupInfo*)m_pInfo; }
	void				DiscardNZBInfo() { m_pInfo = NULL; }
	void				DiscardUrlInfo() { m_pInfo = NULL; }
	time_t				GetTime() { return m_tTime; }
	void				SetTime(time_t tTime) { m_tTime = tTime; }
	void				GetName(char* szBuffer, int iSize);		// needs locking (for shared objects)
};

typedef std::deque<HistoryInfo*> HistoryList;

class DownloadQueue
{
protected:
	NZBList				m_Queue;
	HistoryList			m_History;
	UrlQueue			m_UrlQueue;		//TODO: merge m_UrlQueue with m_Queue
	PostQueue			m_PostQueue;	//TODO: merge m_PostQueue with m_Queue

public:
						DownloadQueue() : m_Queue(true) {}
	NZBList*			GetQueue() { return &m_Queue; }
	HistoryList*		GetHistory() { return &m_History; }
	UrlQueue*			GetUrlQueue() { return &m_UrlQueue; }
	PostQueue*			GetPostQueue() { return &m_PostQueue; }
};

class DownloadQueueHolder
{
public:
	virtual					~DownloadQueueHolder() {};
	virtual DownloadQueue*	LockQueue() = 0;
	virtual void			UnlockQueue() = 0;
};

#endif
