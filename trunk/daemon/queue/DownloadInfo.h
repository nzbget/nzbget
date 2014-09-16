/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

class NZBInfo;
class DownloadQueue;
class PostInfo;

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
	enum EStatOperation
	{
		soSet,
		soAdd,
		soSubtract
	};
	
public:
						~ServerStatList();
	void				StatOp(int iServerID, int iSuccessArticles, int iFailedArticles, EStatOperation eStatOperation);
	void				ListOp(ServerStatList* pServerStats, EStatOperation eStatOperation);
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
	int					m_iPartNumber;
	char*				m_szMessageID;
	int					m_iSize;
	char*				m_pSegmentContent;
	long long			m_iSegmentOffset;
	int					m_iSegmentSize;
	EStatus				m_eStatus;
	char*				m_szResultFilename;
	unsigned long		m_lCrc;

public:
						ArticleInfo();
						~ArticleInfo();
	void 				SetPartNumber(int s) { m_iPartNumber = s; }
	int 				GetPartNumber() { return m_iPartNumber; }
	const char* 		GetMessageID() { return m_szMessageID; }
	void 				SetMessageID(const char* szMessageID);
	void 				SetSize(int iSize) { m_iSize = iSize; }
	int 				GetSize() { return m_iSize; }
	void				AttachSegment(char* pContent, long long iOffset, int iSize);
	void				DiscardSegment();
	const char* 		GetSegmentContent() { return m_pSegmentContent; }
	void				SetSegmentOffset(long long iSegmentOffset) { m_iSegmentOffset = iSegmentOffset; }
	long long			GetSegmentOffset() { return m_iSegmentOffset; }
	void 				SetSegmentSize(int iSegmentSize) { m_iSegmentSize = iSegmentSize; }
	int 				GetSegmentSize() { return m_iSegmentSize; }
	EStatus				GetStatus() { return m_eStatus; }
	void				SetStatus(EStatus Status) { m_eStatus = Status; }
	const char*			GetResultFilename() { return m_szResultFilename; }
	void 				SetResultFilename(const char* v);
	unsigned long		GetCrc() { return m_lCrc; }
	void				SetCrc(unsigned long lCrc) { m_lCrc = lCrc; }
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
	ServerStatList		m_ServerStats;
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
	bool				m_bExtraPriority;
	int					m_iActiveDownloads;
	bool				m_bAutoDeleted;
	int					m_iCachedArticles;
	bool				m_bPartialChanged;

	static int			m_iIDGen;
	static int			m_iIDMax;

	friend class CompletedFile;

public:
						FileInfo(int iID = 0);
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
	bool				GetExtraPriority() { return m_bExtraPriority; }
	void				SetExtraPriority(bool bExtraPriority) { m_bExtraPriority = bExtraPriority; }
	int					GetActiveDownloads() { return m_iActiveDownloads; }
	void				SetActiveDownloads(int iActiveDownloads);
	bool				GetAutoDeleted() { return m_bAutoDeleted; }
	void				SetAutoDeleted(bool bAutoDeleted) { m_bAutoDeleted = bAutoDeleted; }
	int					GetCachedArticles() { return m_iCachedArticles; }
	void				SetCachedArticles(int iCachedArticles) { m_iCachedArticles = iCachedArticles; }
	bool				GetPartialChanged() { return m_bPartialChanged; }
	void				SetPartialChanged(bool bPartialChanged) { m_bPartialChanged = bPartialChanged; }
	ServerStatList*		GetServerStats() { return &m_ServerStats; }
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
	int					m_iID;
	char*				m_szFileName;
	EStatus				m_eStatus;
	unsigned long		m_lCrc;

public:
						CompletedFile(int iID, const char* szFileName, EStatus eStatus, unsigned long lCrc);
						~CompletedFile();
	int					GetID() { return m_iID; }
	void				SetFileName(const char* szFileName);
	const char*			GetFileName() { return m_szFileName; }
	EStatus				GetStatus() { return m_eStatus; }
	unsigned long		GetCrc() { return m_lCrc; }
};

typedef std::deque<CompletedFile*>	CompletedFiles;

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
		dsDupe,
		dsBad
	};

	enum EMarkStatus
	{
		ksNone,
		ksBad,
		ksGood
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

	typedef std::deque<Message*>		Messages;

	static const int FORCE_PRIORITY = 900;

	friend class DupInfo;

private:
	int					m_iID;
	EKind				m_eKind;
	char*				m_szURL;
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
	int					m_iCurrentSuccessArticles;
	int					m_iCurrentFailedArticles;
	time_t				m_tMinTime;
	time_t				m_tMaxTime;
	int					m_iPriority;
	CompletedFiles		m_completedFiles;
	ERenameStatus		m_eRenameStatus;
	EParStatus			m_eParStatus;
	EUnpackStatus		m_eUnpackStatus;
	ECleanupStatus		m_eCleanupStatus;
	EMoveStatus			m_eMoveStatus;
	EDeleteStatus		m_eDeleteStatus;
	EMarkStatus			m_eMarkStatus;
	EUrlStatus			m_eUrlStatus;
	bool				m_bAddUrlPaused;
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
	ServerStatList		m_CurrentServerStats;
	Mutex				m_mutexLog;
	Messages			m_Messages;
	int					m_iIDMessageGen;
	PostInfo*			m_pPostInfo;
	long long 			m_lDownloadedSize;
	time_t				m_tDownloadStartTime;
	int					m_iDownloadSec;
	int					m_iPostTotalSec;
	int					m_iParSec;
	int					m_iRepairSec;
	int					m_iUnpackSec;
	bool				m_bReprocess;
	time_t				m_tQueueScriptTime;

	static int			m_iIDGen;
	static int			m_iIDMax;

public:
						NZBInfo();
						~NZBInfo();
	int					GetID() { return m_iID; }
	void				SetID(int iID);
	static void			ResetGenID(bool bMax);
	static int			GenerateID();
	EKind				GetKind() { return m_eKind; }
	void				SetKind(EKind eKind) { m_eKind = eKind; }
	const char*			GetURL() { return m_szURL; }			// needs locking (for shared objects)
	void				SetURL(const char* szURL);				// needs locking (for shared objects)
	const char*			GetFilename() { return m_szFilename; }
	void				SetFilename(const char* szFilename);
	static void			MakeNiceNZBName(const char* szNZBFilename, char* szBuffer, int iSize, bool bRemoveExt);
	static void			MakeNiceUrlName(const char* szURL, const char* szNZBFilename, char* szBuffer, int iSize);
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
	void				SetActiveDownloads(int iActiveDownloads);
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
	int					GetCurrentSuccessArticles() { return m_iCurrentSuccessArticles; }
	void 				SetCurrentSuccessArticles(int iCurrentSuccessArticles) { m_iCurrentSuccessArticles = iCurrentSuccessArticles; }
	int					GetCurrentFailedArticles() { return m_iCurrentFailedArticles; }
	void 				SetCurrentFailedArticles(int iCurrentFailedArticles) { m_iCurrentFailedArticles = iCurrentFailedArticles; }
	int					GetPriority() { return m_iPriority; }
	void				SetPriority(int iPriority) { m_iPriority = iPriority; }
	bool				GetForcePriority() { return m_iPriority >= FORCE_PRIORITY; }
	time_t				GetMinTime() { return m_tMinTime; }
	void				SetMinTime(time_t tMinTime) { m_tMinTime = tMinTime; }
	time_t				GetMaxTime() { return m_tMaxTime; }
	void				SetMaxTime(time_t tMaxTime) { m_tMaxTime = tMaxTime; }
	void				BuildDestDirName();
	void				BuildFinalDirName(char* szFinalDirBuf, int iBufSize);
	CompletedFiles*		GetCompletedFiles() { return &m_completedFiles; }		// needs locking (for shared objects)
	void				ClearCompletedFiles();
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
	EUrlStatus			GetUrlStatus() { return m_eUrlStatus; }
	void				SetUrlStatus(EUrlStatus eUrlStatus) { m_eUrlStatus = eUrlStatus; }
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
	bool				GetAddUrlPaused() { return m_bAddUrlPaused; }
	void				SetAddUrlPaused(bool bAddUrlPaused) { m_bAddUrlPaused = bAddUrlPaused; }
	FileList*			GetFileList() { return &m_FileList; }					// needs locking (for shared objects)
	NZBParameterList*	GetParameters() { return &m_ppParameters; }				// needs locking (for shared objects)
	ScriptStatusList*	GetScriptStatuses() { return &m_scriptStatuses; }        // needs locking (for shared objects)
	ServerStatList*		GetServerStats() { return &m_ServerStats; }
	ServerStatList*		GetCurrentServerStats() { return &m_CurrentServerStats; }
	int					CalcHealth();
	int					CalcCriticalHealth(bool bAllowEstimation);
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
	long long 			GetDownloadedSize() { return m_lDownloadedSize; }
	void 				SetDownloadedSize(long long lDownloadedSize) { m_lDownloadedSize = lDownloadedSize; }
	int					GetDownloadSec() { return m_iDownloadSec; }
	void 				SetDownloadSec(int iDownloadSec) { m_iDownloadSec = iDownloadSec; }
	int					GetPostTotalSec() { return m_iPostTotalSec; }
	void 				SetPostTotalSec(int iPostTotalSec) { m_iPostTotalSec = iPostTotalSec; }
	int					GetParSec() { return m_iParSec; }
	void 				SetParSec(int iParSec) { m_iParSec = iParSec; }
	int					GetRepairSec() { return m_iRepairSec; }
	void 				SetRepairSec(int iRepairSec) { m_iRepairSec = iRepairSec; }
	int					GetUnpackSec() { return m_iUnpackSec; }
	void 				SetUnpackSec(int iUnpackSec) { m_iUnpackSec = iUnpackSec; }
	time_t				GetDownloadStartTime() { return m_tDownloadStartTime; }
	void 				SetDownloadStartTime(time_t tDownloadStartTime) { m_tDownloadStartTime = tDownloadStartTime; }
	void				SetReprocess(bool bReprocess) { m_bReprocess = bReprocess; }
	bool				GetReprocess() { return m_bReprocess; }
	time_t				GetQueueScriptTime() { return m_tQueueScriptTime; }
	void 				SetQueueScriptTime(time_t tQueueScriptTime) { m_tQueueScriptTime = tQueueScriptTime; }

	void				CopyFileList(NZBInfo* pSrcNZBInfo);
	void				UpdateMinMaxTime();
	PostInfo*			GetPostInfo() { return m_pPostInfo; }
	void				EnterPostProcess();
	void				LeavePostProcess();
	bool				IsDupeSuccess();
	const char*			MakeTextStatus(bool bIgnoreScriptStatus);

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
	void				Add(NZBInfo* pNZBInfo, bool bAddTop);
	void				Remove(NZBInfo* pNZBInfo);
	NZBInfo*			Find(int iID);
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
	typedef std::vector<char*>		ParredFiles;

private:
	NZBInfo*			m_pNZBInfo;
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
	ParredFiles			m_ParredFiles;

public:
						PostInfo();
						~PostInfo();
	NZBInfo*			GetNZBInfo() { return m_pNZBInfo; }
	void				SetNZBInfo(NZBInfo* pNZBInfo) { m_pNZBInfo = pNZBInfo; }
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
	Thread*				GetPostThread() { return m_pPostThread; }
	void				SetPostThread(Thread* pPostThread) { m_pPostThread = pPostThread; }
	void				AppendMessage(Message::EKind eKind, const char* szText);
	Messages*			LockMessages();
	void				UnlockMessages();
	ParredFiles*		GetParredFiles() { return &m_ParredFiles; }
};

typedef std::vector<int> IDList;

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
	int					m_iID;
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
	int					GetID() { return m_iID; }
	void				SetID(int iID);
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
		hkNzb,
		hkUrl,
		hkDup
	};

private:
	EKind				m_eKind;
	void*				m_pInfo;
	time_t				m_tTime;

public:
						HistoryInfo(NZBInfo* pNZBInfo);
						HistoryInfo(DupInfo* pDupInfo);
						~HistoryInfo();
	EKind				GetKind() { return m_eKind; }
	int					GetID();
	NZBInfo*			GetNZBInfo() { return (NZBInfo*)m_pInfo; }
	DupInfo*			GetDupInfo() { return (DupInfo*)m_pInfo; }
	void				DiscardNZBInfo() { m_pInfo = NULL; }
	time_t				GetTime() { return m_tTime; }
	void				SetTime(time_t tTime) { m_tTime = tTime; }
	void				GetName(char* szBuffer, int iSize);		// needs locking (for shared objects)
};

typedef std::deque<HistoryInfo*> HistoryList;

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
		EAspectAction eAction;
		DownloadQueue* pDownloadQueue;
		NZBInfo* pNZBInfo;
		FileInfo* pFileInfo;
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
		eaHistoryMarkGood		// mark history-item as good (and push it into dup-history)
	};

	enum EMatchMode
	{
		mmID = 1,
		mmName,
		mmRegEx
	};

private:
	NZBList					m_Queue;
	HistoryList				m_History;
	Mutex	 				m_LockMutex;

	static DownloadQueue*	g_pDownloadQueue;
	static bool				g_bLoaded;

protected:
							DownloadQueue() : m_Queue(true) {}
	static void				Init(DownloadQueue* pGlobalInstance) { g_pDownloadQueue = pGlobalInstance; }
	static void				Final() { g_pDownloadQueue = NULL; }
	static void				Loaded() { g_bLoaded = true; }

public:
	virtual					~DownloadQueue() {}
	static bool				IsLoaded() { return g_bLoaded; }
	static DownloadQueue*	Lock();
	static void				Unlock();
	NZBList*				GetQueue() { return &m_Queue; }
	HistoryList*			GetHistory() { return &m_History; }
	virtual bool			EditEntry(int ID, EEditAction eAction, int iOffset, const char* szText) = 0;
	virtual bool			EditList(IDList* pIDList, NameList* pNameList, EMatchMode eMatchMode, EEditAction eAction, int iOffset, const char* szText) = 0;
	virtual void			Save() = 0;
	void					CalcRemainingSize(long long* pRemaining, long long* pRemainingForced);
};

#endif
