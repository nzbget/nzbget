/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2010 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef OPTIONS_H
#define OPTIONS_H

#include <vector>
#include "Thread.h"

class Options
{
public:
	enum EClientOperation
	{
		opClientNoOperation,
		opClientRequestDownload,
		opClientRequestListFiles,
		opClientRequestListGroups,
		opClientRequestListStatus,
		opClientRequestSetRate,
		opClientRequestDumpDebug,
		opClientRequestEditQueue,
		opClientRequestLog,
		opClientRequestShutdown,
		opClientRequestReload,
		opClientRequestVersion,
		opClientRequestPostQueue,
		opClientRequestWriteLog,
		opClientRequestScanSync,
		opClientRequestScanAsync,
		opClientRequestDownloadPause,
		opClientRequestDownloadUnpause,
		opClientRequestDownload2Pause,
		opClientRequestDownload2Unpause,
		opClientRequestPostPause,
		opClientRequestPostUnpause,
		opClientRequestScanPause,
		opClientRequestScanUnpause,
		opClientRequestHistory,
		opClientRequestDownloadUrl,
		opClientRequestUrlQueue
	};
	enum EMessageTarget
	{
		mtNone,
		mtScreen,
		mtLog,
		mtBoth
	};
	enum EOutputMode
	{
		omLoggable,
		omColored,
		omNCurses
	};
	enum ELoadPars
	{
		lpNone,
		lpOne,
		lpAll
	};
	enum EParScan
	{
		psLimited,
		psFull,
		psAuto
	};
	enum EScriptLogKind
	{
		slNone,
		slDetail,
		slInfo,
		slWarning,
		slError,
		slDebug
	};

	enum EMatchMode
	{
		mmID = 1,
		mmName,
		mmRegEx
	};

	enum EDomain
	{
		dmServer = 1,
		dmPostProcess
	};

	class OptEntry
	{
	private:
		char*			m_szName;
		char*			m_szValue;
		char*			m_szDefValue;
		int				m_iLineNo;

		void			SetName(const char* szName);
		void			SetValue(const char* szValue);
		void			SetLineNo(int iLineNo) { m_iLineNo = iLineNo; }

		friend class Options;

	public:
						OptEntry();
						OptEntry(const char* szName, const char* szValue);
						~OptEntry();
		const char*		GetName() { return m_szName; }
		const char*		GetValue() { return m_szValue; }
		const char*		GetDefValue() { return m_szDefValue; }
		int				GetLineNo() { return m_iLineNo; }
	};
	
	typedef std::vector<OptEntry*>  OptEntriesBase;

	class OptEntries: public OptEntriesBase
	{
	public:
						~OptEntries();
		OptEntry*		FindOption(const char* szName);
	};

	typedef std::vector<char*>  NameList;

	class Category
	{
	private:
		char*			m_szName;
		char*			m_szDestDir;

	public:
						Category(const char* szName, const char* szDestDir);
						~Category();
		const char*		GetName() { return m_szName; }
		const char*		GetDestDir() { return m_szDestDir; }
	};
	
	typedef std::vector<Category*>  CategoriesBase;

	class Categories: public CategoriesBase
	{
	public:
						~Categories();
		Category*		FindCategory(const char* szName);
	};

private:
	OptEntries			m_OptEntries;
	bool				m_bConfigInitialized;
	Mutex				m_mutexOptEntries;
	Categories			m_Categories;

	// Options
	bool				m_bConfigErrors;
	int					m_iConfigLine;
	char*				m_szConfigFilename;
	char*				m_szDestDir;
	char*				m_szTempDir;
	char*				m_szQueueDir;
	char*				m_szNzbDir;
	char*				m_szWebDir;
	EMessageTarget		m_eInfoTarget;
	EMessageTarget		m_eWarningTarget;
	EMessageTarget		m_eErrorTarget;
	EMessageTarget		m_eDebugTarget;
	EMessageTarget		m_eDetailTarget;
	bool				m_bDecode;
	bool				m_bCreateBrokenLog;
	bool				m_bResetLog;
	int					m_iConnectionTimeout;
	int					m_iTerminateTimeout;
	bool				m_bAppendNZBDir;
	bool				m_bAppendCategoryDir;
	bool				m_bContinuePartial;
	bool				m_bRenameBroken;
	int					m_iRetries;
	int					m_iRetryInterval;
	bool				m_bSaveQueue;
	bool				m_bDupeCheck;
	char*				m_szControlIP;
	char*				m_szControlPassword;
	int					m_iControlPort;
	bool				m_bSecureControl;
	int					m_iSecurePort;
	char*				m_szSecureCert;
	char*				m_szSecureKey;
	char*				m_szLockFile;
	char*				m_szDaemonUserName;
	EOutputMode			m_eOutputMode;
	bool				m_bReloadQueue;
	bool				m_bReloadUrlQueue;
	bool				m_bReloadPostQueue;
	int					m_iUrlConnections;
	int					m_iLogBufferSize;
	bool				m_bCreateLog;
	char*				m_szLogFile;
	ELoadPars			m_eLoadPars;
	bool				m_bParCheck;
	bool				m_bParRepair;
	EParScan			m_eParScan;
	char*				m_szPostProcess;
	char*				m_szPostConfigFilename;
	char*				m_szNZBProcess;
	char*				m_szNZBAddedProcess;
	bool				m_bStrictParName;
	bool				m_bNoConfig;
	int					m_iUMask;
	int					m_iUpdateInterval;
	bool				m_bCursesNZBName;
	bool				m_bCursesTime;
	bool				m_bCursesGroup;
	bool				m_bCrcCheck;
	bool				m_bRetryOnCrcError;
	int					m_iThreadLimit;
	bool				m_bDirectWrite;
	int					m_iWriteBufferSize;
	int					m_iNzbDirInterval;
	int					m_iNzbDirFileAge;
	bool				m_bParCleanupQueue;
	int					m_iDiskSpace;
	EScriptLogKind		m_eProcessLogKind;
	bool				m_bAllowReProcess;
	bool				m_bTLS;
	bool				m_bDumpCore;
	bool				m_bParPauseQueue;
	bool				m_bPostPauseQueue;
	bool				m_bNzbCleanupDisk;
	bool				m_bDeleteCleanupDisk;
	bool				m_bMergeNzb;
	int					m_iParTimeLimit;
	int					m_iKeepHistory;
	bool				m_bAccurateRate;

	// Parsed command-line parameters
	bool				m_bServerMode;
	bool				m_bDaemonMode;
	bool				m_bRemoteClientMode;
	int					m_iEditQueueAction;
	int					m_iEditQueueOffset;
	int*				m_pEditQueueIDList;
	int					m_iEditQueueIDCount;
	NameList			m_EditQueueNameList;
	EMatchMode			m_EMatchMode;
	char*				m_szEditQueueText;
	char*				m_szArgFilename;
	char*				m_szAddCategory;
	int					m_iAddPriority;
	bool				m_bAddPaused;
	char*				m_szAddNZBFilename;
	char*				m_szLastArg;
	bool				m_bPrintOptions;
	bool				m_bAddTop;
	float				m_fSetRate;
	int					m_iLogLines;
	int					m_iWriteLogKind;
	bool				m_bTestBacktrace;

	// Current state
	bool				m_bPauseDownload;
	bool				m_bPauseDownload2;
	bool				m_bPausePostProcess;
	bool				m_bPauseScan;
	float				m_fDownloadRate;
	EClientOperation	m_eClientOperation;

	void				InitDefault();
	void				InitOptFile();
	void				InitCommandLine(int argc, char* argv[]);
	void				InitOptions();
	void				InitPostConfig();
	void				InitFileArg(int argc, char* argv[]);
	void				InitServers();
	void				InitCategories();
	void				InitScheduler();
	void				CheckOptions();
	void				PrintUsage(char* com);
	void				Dump();
	int					ParseEnumValue(const char* OptName, int argc, const char* argn[], const int argv[]);
	int					ParseIntValue(const char* OptName, int iBase);
	float				ParseFloatValue(const char* OptName);
	OptEntry*			FindOption(const char* optname);
	const char*			GetOption(const char* optname);
	void				SetOption(const char* optname, const char* value);
	bool				SetOptionString(const char* option);
	bool				ValidateOptionName(const char* optname);
	void				LoadConfigFile();
	void				CheckDir(char** dir, const char* szOptionName, bool bAllowEmpty, bool bCreate);
	void				ParseFileIDList(int argc, char* argv[], int optind);
	void				ParseFileNameList(int argc, char* argv[], int optind);
	bool				ParseTime(const char** pTime, int* pHours, int* pMinutes);
	bool				ParseWeekDays(const char* szWeekDays, int* pWeekDaysBits);
	void				ConfigError(const char* msg, ...);
	void				LocateOptionSrcPos(const char *szOptionName);
	void				ConvertOldOptionName(char *szOption, int iBufLen);

public:
						Options(int argc, char* argv[]);
						~Options();

	bool				LoadConfig(EDomain eDomain, OptEntries* pOptEntries);
	bool				SaveConfig(EDomain eDomain, OptEntries* pOptEntries);

	// Options
	OptEntries*			LockOptEntries();
	void				UnlockOptEntries();
	const char*			GetConfigFilename() { return m_szConfigFilename; }
	const char*			GetDestDir() { return m_szDestDir; }
	const char*			GetTempDir() { return m_szTempDir; }
	const char*			GetQueueDir() { return m_szQueueDir; }
	const char*			GetNzbDir() { return m_szNzbDir; }
	const char*			GetWebDir() { return m_szWebDir; }
	bool				GetCreateBrokenLog() const { return m_bCreateBrokenLog; }
	bool				GetResetLog() const { return m_bResetLog; }
	EMessageTarget		GetInfoTarget() const { return m_eInfoTarget; }
	EMessageTarget		GetWarningTarget() const { return m_eWarningTarget; }
	EMessageTarget		GetErrorTarget() const { return m_eErrorTarget; }
	EMessageTarget		GetDebugTarget() const { return m_eDebugTarget; }
	EMessageTarget		GetDetailTarget() const { return m_eDetailTarget; }
	int					GetConnectionTimeout() { return m_iConnectionTimeout; }
	int					GetTerminateTimeout() { return m_iTerminateTimeout; }
	bool				GetDecode() { return m_bDecode; };
	bool				GetAppendNZBDir() { return m_bAppendNZBDir; }
	bool				GetAppendCategoryDir() { return m_bAppendCategoryDir; }
	bool				GetContinuePartial() { return m_bContinuePartial; }
	bool				GetRenameBroken() { return m_bRenameBroken; }
	int					GetRetries() { return m_iRetries; }
	int					GetRetryInterval() { return m_iRetryInterval; }
	bool				GetSaveQueue() { return m_bSaveQueue; }
	bool				GetDupeCheck() { return m_bDupeCheck; }
	const char*			GetControlIP() { return m_szControlIP; }
	const char*			GetControlPassword() { return m_szControlPassword; }
	int					GetControlPort() { return m_iControlPort; }
	bool				GetSecureControl() { return m_bSecureControl; }
	int					GetSecurePort() { return m_iSecurePort; }
	const char*			GetSecureCert() { return m_szSecureCert; }
	const char*			GetSecureKey() { return m_szSecureKey; }
	const char*			GetLockFile() { return m_szLockFile; }
	const char*			GetDaemonUserName() { return m_szDaemonUserName; }
	EOutputMode			GetOutputMode() { return m_eOutputMode; }
	bool				GetReloadQueue() { return m_bReloadQueue; }
	bool				GetReloadUrlQueue() { return m_bReloadUrlQueue; }
	bool				GetReloadPostQueue() { return m_bReloadPostQueue; }
	int					GetUrlConnections() { return m_iUrlConnections; }
	int					GetLogBufferSize() { return m_iLogBufferSize; }
	bool				GetCreateLog() { return m_bCreateLog; }
	const char*			GetLogFile() { return m_szLogFile; }
	ELoadPars			GetLoadPars() { return m_eLoadPars; }
	bool				GetParCheck() { return m_bParCheck; }
	bool				GetParRepair() { return m_bParRepair; }
	EParScan			GetParScan() { return m_eParScan; }
	const char*			GetPostProcess() { return m_szPostProcess; }
	const char*			GetPostConfigFilename() { return m_szPostConfigFilename; }
	const char*			GetNZBProcess() { return m_szNZBProcess; }
	const char*			GetNZBAddedProcess() { return m_szNZBAddedProcess; }
	bool				GetStrictParName() { return m_bStrictParName; }
	int					GetUMask() { return m_iUMask; }
	int					GetUpdateInterval() {return m_iUpdateInterval; }
	bool				GetCursesNZBName() { return m_bCursesNZBName; }
	bool				GetCursesTime() { return m_bCursesTime; }
	bool				GetCursesGroup() { return m_bCursesGroup; }
	bool				GetCrcCheck() { return m_bCrcCheck; }
	bool				GetRetryOnCrcError() { return m_bRetryOnCrcError; }
	int					GetThreadLimit() { return m_iThreadLimit; }
	bool				GetDirectWrite() { return m_bDirectWrite; }
	int					GetWriteBufferSize() { return m_iWriteBufferSize; }
	int					GetNzbDirInterval() { return m_iNzbDirInterval; }
	int					GetNzbDirFileAge() { return m_iNzbDirFileAge; }
	bool				GetParCleanupQueue() { return m_bParCleanupQueue; }
	int					GetDiskSpace() { return m_iDiskSpace; }
	EScriptLogKind		GetProcessLogKind() { return m_eProcessLogKind; }
	bool				GetAllowReProcess() { return m_bAllowReProcess; }
	bool				GetTLS() { return m_bTLS; }
	bool				GetDumpCore() { return m_bDumpCore; }
	bool				GetParPauseQueue() { return m_bParPauseQueue; }
	bool				GetPostPauseQueue() { return m_bPostPauseQueue; }
	bool				GetNzbCleanupDisk() { return m_bNzbCleanupDisk; }
	bool				GetDeleteCleanupDisk() { return m_bDeleteCleanupDisk; }
	bool				GetMergeNzb() { return m_bMergeNzb; }
	int					GetParTimeLimit() { return m_iParTimeLimit; }
	int					GetKeepHistory() { return m_iKeepHistory; }
	bool				GetAccurateRate() { return m_bAccurateRate; }
	Category*			FindCategory(const char* szName) { return m_Categories.FindCategory(szName); }

	// Parsed command-line parameters
	bool				GetServerMode() { return m_bServerMode; }
	bool				GetDaemonMode() { return m_bDaemonMode; }
	bool				GetRemoteClientMode() { return m_bRemoteClientMode; }
	EClientOperation	GetClientOperation() { return m_eClientOperation; }
	int					GetEditQueueAction() { return m_iEditQueueAction; }
	int					GetEditQueueOffset() { return m_iEditQueueOffset; }
	int*				GetEditQueueIDList() { return m_pEditQueueIDList; }
	int					GetEditQueueIDCount() { return m_iEditQueueIDCount; }
	NameList*			GetEditQueueNameList() { return &m_EditQueueNameList; }
	EMatchMode			GetMatchMode() { return m_EMatchMode; }
	const char*			GetEditQueueText() { return m_szEditQueueText; }
	const char*			GetArgFilename() { return m_szArgFilename; }
	const char*			GetAddCategory() { return m_szAddCategory; }
	bool				GetAddPaused() { return m_bAddPaused; }
	const char*			GetLastArg() { return m_szLastArg; }
	int					GetAddPriority() { return m_iAddPriority; }
	char*				GetAddNZBFilename() { return m_szAddNZBFilename; }
	bool				GetAddTop() { return m_bAddTop; }
	float				GetSetRate() { return m_fSetRate; }
	int					GetLogLines() { return m_iLogLines; }
	int					GetWriteLogKind() { return m_iWriteLogKind; }
	bool				GetTestBacktrace() { return m_bTestBacktrace; }

	// Current state
	void				SetPauseDownload(bool bPauseDownload) { m_bPauseDownload = bPauseDownload; }
	bool				GetPauseDownload() const { return m_bPauseDownload; }
	void				SetPauseDownload2(bool bPauseDownload2) { m_bPauseDownload2 = bPauseDownload2; }
	bool				GetPauseDownload2() const { return m_bPauseDownload2; }
	void				SetPausePostProcess(bool bPausePostProcess) { m_bPausePostProcess = bPausePostProcess; }
	bool				GetPausePostProcess() const { return m_bPausePostProcess; }
	void				SetPauseScan(bool bPauseScan) { m_bPauseScan = bPauseScan; }
	bool				GetPauseScan() const { return m_bPauseScan; }
	void				SetDownloadRate(float fRate) { m_fDownloadRate = fRate; }
	float				GetDownloadRate() const { return m_fDownloadRate; }
};

#endif
