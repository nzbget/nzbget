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


#ifndef OPTIONS_H
#define OPTIONS_H

#include <vector>
#include <list>
#include <time.h>

#include "Thread.h"
#include "Util.h"

class Options
{
public:
	enum EWriteLog
	{
		wlNone,
		wlAppend,
		wlReset,
		wlRotate
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
	enum EParCheck
	{
		pcAuto,
		pcAlways,
		pcForce,
		pcManual
	};
	enum EParScan
	{
		psLimited,
		psFull,
		psAuto
	};
	enum EHealthCheck
	{
		hcPause,
		hcDelete,
		hcNone
	};
	enum ESchedulerCommand
	{
		scPauseDownload,
		scUnpauseDownload,
		scPausePostProcess,
		scUnpausePostProcess,
		scDownloadRate,
		scScript,
		scProcess,
		scPauseScan,
		scUnpauseScan,
		scActivateServer,
		scDeactivateServer,
		scFetchFeed
	};

	class OptEntry
	{
	private:
		char*			m_szName;
		char*			m_szValue;
		char*			m_szDefValue;
		int				m_iLineNo;

		void			SetLineNo(int iLineNo) { m_iLineNo = iLineNo; }

		friend class Options;

	public:
						OptEntry();
						OptEntry(const char* szName, const char* szValue);
						~OptEntry();
		void			SetName(const char* szName);
		const char*		GetName() { return m_szName; }
		void			SetValue(const char* szValue);
		const char*		GetValue() { return m_szValue; }
		const char*		GetDefValue() { return m_szDefValue; }
		int				GetLineNo() { return m_iLineNo; }
		bool			Restricted();
	};
	
	typedef std::vector<OptEntry*>  OptEntriesBase;

	class OptEntries: public OptEntriesBase
	{
	public:
						~OptEntries();
		OptEntry*		FindOption(const char* szName);
	};

	typedef std::vector<char*>  NameList;
	typedef std::vector<const char*>  CmdOptList;

	class Category
	{
	private:
		char*			m_szName;
		char*			m_szDestDir;
		bool			m_bUnpack;
		char*			m_szPostScript;
		NameList		m_Aliases;

	public:
						Category(const char* szName, const char* szDestDir, bool bUnpack, const char* szPostScript);
						~Category();
		const char*		GetName() { return m_szName; }
		const char*		GetDestDir() { return m_szDestDir; }
		bool			GetUnpack() { return m_bUnpack; }
		const char*		GetPostScript() { return m_szPostScript; }
		NameList*		GetAliases() { return &m_Aliases; }
	};
	
	typedef std::vector<Category*>  CategoriesBase;

	class Categories: public CategoriesBase
	{
	public:
						~Categories();
		Category*		FindCategory(const char* szName, bool bSearchAliases);
	};

	class Extender
	{
	public:
		virtual void	AddNewsServer(int iID, bool bActive, const char* szName, const char* szHost,
							int iPort, const char* szUser, const char* szPass, bool bJoinGroup,
							bool bTLS, const char* szCipher, int iMaxConnections, int iRetention,
							int iLevel, int iGroup) = 0;
		virtual void	AddFeed(int iID, const char* szName, const char* szUrl, int iInterval,
							const char* szFilter, bool bPauseNzb, const char* szCategory, int iPriority) {}
		virtual void	AddTask(int iID, int iHours, int iMinutes, int iWeekDaysBits, ESchedulerCommand eCommand,
							const char* szParam) {}
		virtual void	SetupFirstStart() {}
	};

private:
	OptEntries			m_OptEntries;
	Mutex				m_mutexOptEntries;
	Categories			m_Categories;
	bool				m_bNoDiskAccess;
	bool				m_bFatalError;
	Extender*			m_pExtender;

	// Options
	bool				m_bConfigErrors;
	int					m_iConfigLine;
	char*				m_szAppDir;
	char*				m_szConfigFilename;
	char*				m_szDestDir;
	char*				m_szInterDir;
	char*				m_szTempDir;
	char*				m_szQueueDir;
	char*				m_szNzbDir;
	char*				m_szWebDir;
	char*				m_szConfigTemplate;
	char*				m_szScriptDir;
	EMessageTarget		m_eInfoTarget;
	EMessageTarget		m_eWarningTarget;
	EMessageTarget		m_eErrorTarget;
	EMessageTarget		m_eDebugTarget;
	EMessageTarget		m_eDetailTarget;
	bool				m_bDecode;
	bool				m_bBrokenLog;
	bool				m_bNzbLog;
	int					m_iArticleTimeout;
	int					m_iUrlTimeout;
	int					m_iTerminateTimeout;
	bool				m_bAppendCategoryDir;
	bool				m_bContinuePartial;
	int					m_iRetries;
	int					m_iRetryInterval;
	bool				m_bSaveQueue;
	bool				m_bDupeCheck;
	char*				m_szControlIP;
	char*				m_szControlUsername;
	char*				m_szControlPassword;
	char*				m_szRestrictedUsername;
	char*				m_szRestrictedPassword;
	char*				m_szAddUsername;
	char*				m_szAddPassword;
	int					m_iControlPort;
	bool				m_bSecureControl;
	int					m_iSecurePort;
	char*				m_szSecureCert;
	char*				m_szSecureKey;
	char*				m_szAuthorizedIP;
	char*				m_szLockFile;
	char*				m_szDaemonUsername;
	EOutputMode			m_eOutputMode;
	bool				m_bReloadQueue;
	int					m_iUrlConnections;
	int					m_iLogBufferSize;
	EWriteLog			m_eWriteLog;
	int					m_iRotateLog;
	char*				m_szLogFile;
	EParCheck			m_eParCheck;
	bool				m_bParRepair;
	EParScan			m_eParScan;
	bool				m_bParQuick;
	bool				m_bParRename;
	int					m_iParBuffer;
	int					m_iParThreads;
	EHealthCheck		m_eHealthCheck;
	char*				m_szPostScript;
	char*				m_szScriptOrder;
	char*				m_szScanScript;
	char*				m_szQueueScript;
	bool				m_bNoConfig;
	int					m_iUMask;
	int					m_iUpdateInterval;
	bool				m_bCursesNZBName;
	bool				m_bCursesTime;
	bool				m_bCursesGroup;
	bool				m_bCrcCheck;
	bool				m_bDirectWrite;
	int					m_iWriteBuffer;
	int					m_iNzbDirInterval;
	int					m_iNzbDirFileAge;
	bool				m_bParCleanupQueue;
	int					m_iDiskSpace;
	bool				m_bTLS;
	bool				m_bDumpCore;
	bool				m_bParPauseQueue;
	bool				m_bScriptPauseQueue;
	bool				m_bNzbCleanupDisk;
	bool				m_bDeleteCleanupDisk;
	int					m_iParTimeLimit;
	int					m_iKeepHistory;
	bool				m_bAccurateRate;
	bool				m_bUnpack;
	bool				m_bUnpackCleanupDisk;
	char*				m_szUnrarCmd;
	char*				m_szSevenZipCmd;
	char*				m_szUnpackPassFile;
	bool				m_bUnpackPauseQueue;
	char*				m_szExtCleanupDisk;
	char*				m_szParIgnoreExt;
	int					m_iFeedHistory;
	bool				m_bUrlForce;
	int					m_iTimeCorrection;
	int					m_iPropagationDelay;
	int					m_iArticleCache;
	int					m_iEventInterval;

	// Current state
	bool				m_bServerMode;
	bool				m_bRemoteClientMode;
	bool				m_bPauseDownload;
	bool				m_bPausePostProcess;
	bool				m_bPauseScan;
	bool				m_bTempPauseDownload;
	int					m_iDownloadRate;
	time_t				m_tResumeTime;
	int					m_iLocalTimeOffset;

	void				Init(const char* szExeName, const char* szConfigFilename, bool bNoConfig,
							 CmdOptList* pCommandLineOptions, bool bNoDiskAccess, Extender* pExtender);
	void				InitDefaults();
	void				InitOptions();
	void				InitOptFile();
	void				InitServers();
	void				InitCategories();
	void				InitScheduler();
	void				InitFeeds();
	void				InitCommandLineOptions(CmdOptList* pCommandLineOptions);
	void				CheckOptions();
	void				Dump();
	int					ParseEnumValue(const char* OptName, int argc, const char* argn[], const int argv[]);
	int					ParseIntValue(const char* OptName, int iBase);
	OptEntry*			FindOption(const char* optname);
	const char*			GetOption(const char* optname);
	void				SetOption(const char* optname, const char* value);
	bool				SetOptionString(const char* option);
	bool				ValidateOptionName(const char* optname, const char* optvalue);
	void				LoadConfigFile();
	void				CheckDir(char** dir, const char* szOptionName, const char* szParentDir,
							bool bAllowEmpty, bool bCreate);
	bool				ParseTime(const char* szTime, int* pHours, int* pMinutes);
	bool				ParseWeekDays(const char* szWeekDays, int* pWeekDaysBits);
	void				ConfigError(const char* msg, ...);
	void				ConfigWarn(const char* msg, ...);
	void				LocateOptionSrcPos(const char *szOptionName);
	void				ConvertOldOption(char *szOption, int iOptionBufLen, char *szValue, int iValueBufLen);

public:
						Options(const char* szExeName, const char* szConfigFilename, bool bNoConfig,
							CmdOptList* pCommandLineOptions, Extender* pExtender);
						Options(CmdOptList* pCommandLineOptions, Extender* pExtender);
						~Options();

	bool				SplitOptionString(const char* option, char** pOptName, char** pOptValue);
	bool				GetFatalError() { return m_bFatalError; }
	OptEntries*			LockOptEntries();
	void				UnlockOptEntries();

	// Options
	const char*			GetConfigFilename() { return m_szConfigFilename; }
	bool				GetConfigErrors() { return m_bConfigErrors; }
	const char*			GetAppDir() { return m_szAppDir; }
	const char*			GetDestDir() { return m_szDestDir; }
	const char*			GetInterDir() { return m_szInterDir; }
	const char*			GetTempDir() { return m_szTempDir; }
	const char*			GetQueueDir() { return m_szQueueDir; }
	const char*			GetNzbDir() { return m_szNzbDir; }
	const char*			GetWebDir() { return m_szWebDir; }
	const char*			GetConfigTemplate() { return m_szConfigTemplate; }
	const char*			GetScriptDir() { return m_szScriptDir; }
	bool				GetBrokenLog() const { return m_bBrokenLog; }
	bool				GetNzbLog() const { return m_bNzbLog; }
	EMessageTarget		GetInfoTarget() const { return m_eInfoTarget; }
	EMessageTarget		GetWarningTarget() const { return m_eWarningTarget; }
	EMessageTarget		GetErrorTarget() const { return m_eErrorTarget; }
	EMessageTarget		GetDebugTarget() const { return m_eDebugTarget; }
	EMessageTarget		GetDetailTarget() const { return m_eDetailTarget; }
	int					GetArticleTimeout() { return m_iArticleTimeout; }
	int					GetUrlTimeout() { return m_iUrlTimeout; }
	int					GetTerminateTimeout() { return m_iTerminateTimeout; }
	bool				GetDecode() { return m_bDecode; };
	bool				GetAppendCategoryDir() { return m_bAppendCategoryDir; }
	bool				GetContinuePartial() { return m_bContinuePartial; }
	int					GetRetries() { return m_iRetries; }
	int					GetRetryInterval() { return m_iRetryInterval; }
	bool				GetSaveQueue() { return m_bSaveQueue; }
	bool				GetDupeCheck() { return m_bDupeCheck; }
	const char*			GetControlIP() { return m_szControlIP; }
	const char*			GetControlUsername() { return m_szControlUsername; }
	const char*			GetControlPassword() { return m_szControlPassword; }
	const char*			GetRestrictedUsername() { return m_szRestrictedUsername; }
	const char*			GetRestrictedPassword() { return m_szRestrictedPassword; }
	const char*			GetAddUsername() { return m_szAddUsername; }
	const char*			GetAddPassword() { return m_szAddPassword; }
	int					GetControlPort() { return m_iControlPort; }
	bool				GetSecureControl() { return m_bSecureControl; }
	int					GetSecurePort() { return m_iSecurePort; }
	const char*			GetSecureCert() { return m_szSecureCert; }
	const char*			GetSecureKey() { return m_szSecureKey; }
	const char*			GetAuthorizedIP() { return m_szAuthorizedIP; }
	const char*			GetLockFile() { return m_szLockFile; }
	const char*			GetDaemonUsername() { return m_szDaemonUsername; }
	EOutputMode			GetOutputMode() { return m_eOutputMode; }
	bool				GetReloadQueue() { return m_bReloadQueue; }
	int					GetUrlConnections() { return m_iUrlConnections; }
	int					GetLogBufferSize() { return m_iLogBufferSize; }
	EWriteLog			GetWriteLog() { return m_eWriteLog; }
	const char*			GetLogFile() { return m_szLogFile; }
	int					GetRotateLog() { return m_iRotateLog; }
	EParCheck			GetParCheck() { return m_eParCheck; }
	bool				GetParRepair() { return m_bParRepair; }
	EParScan			GetParScan() { return m_eParScan; }
	bool				GetParQuick() { return m_bParQuick; }
	bool				GetParRename() { return m_bParRename; }
	int					GetParBuffer() { return m_iParBuffer; }
	int					GetParThreads() { return m_iParThreads; }
	EHealthCheck		GetHealthCheck() { return m_eHealthCheck; }
	const char*			GetScriptOrder() { return m_szScriptOrder; }
	const char*			GetPostScript() { return m_szPostScript; }
	const char*			GetScanScript() { return m_szScanScript; }
	const char*			GetQueueScript() { return m_szQueueScript; }
	int					GetUMask() { return m_iUMask; }
	int					GetUpdateInterval() {return m_iUpdateInterval; }
	bool				GetCursesNZBName() { return m_bCursesNZBName; }
	bool				GetCursesTime() { return m_bCursesTime; }
	bool				GetCursesGroup() { return m_bCursesGroup; }
	bool				GetCrcCheck() { return m_bCrcCheck; }
	bool				GetDirectWrite() { return m_bDirectWrite; }
	int					GetWriteBuffer() { return m_iWriteBuffer; }
	int					GetNzbDirInterval() { return m_iNzbDirInterval; }
	int					GetNzbDirFileAge() { return m_iNzbDirFileAge; }
	bool				GetParCleanupQueue() { return m_bParCleanupQueue; }
	int					GetDiskSpace() { return m_iDiskSpace; }
	bool				GetTLS() { return m_bTLS; }
	bool				GetDumpCore() { return m_bDumpCore; }
	bool				GetParPauseQueue() { return m_bParPauseQueue; }
	bool				GetScriptPauseQueue() { return m_bScriptPauseQueue; }
	bool				GetNzbCleanupDisk() { return m_bNzbCleanupDisk; }
	bool				GetDeleteCleanupDisk() { return m_bDeleteCleanupDisk; }
	int					GetParTimeLimit() { return m_iParTimeLimit; }
	int					GetKeepHistory() { return m_iKeepHistory; }
	bool				GetAccurateRate() { return m_bAccurateRate; }
	bool				GetUnpack() { return m_bUnpack; }
	bool				GetUnpackCleanupDisk() { return m_bUnpackCleanupDisk; }
	const char*			GetUnrarCmd() { return m_szUnrarCmd; }
	const char*			GetSevenZipCmd() { return m_szSevenZipCmd; }
	const char*			GetUnpackPassFile() { return m_szUnpackPassFile; }
	bool				GetUnpackPauseQueue() { return m_bUnpackPauseQueue; }
	const char*			GetExtCleanupDisk() { return m_szExtCleanupDisk; }
	const char*			GetParIgnoreExt() { return m_szParIgnoreExt; }
	int					GetFeedHistory() { return m_iFeedHistory; }
	bool				GetUrlForce() { return m_bUrlForce; }
	int					GetTimeCorrection() { return m_iTimeCorrection; }
	int					GetPropagationDelay() { return m_iPropagationDelay; }
	int					GetArticleCache() { return m_iArticleCache; }
	int					GetEventInterval() { return m_iEventInterval; }

	Categories*			GetCategories() { return &m_Categories; }
	Category*			FindCategory(const char* szName, bool bSearchAliases) { return m_Categories.FindCategory(szName, bSearchAliases); }

	// Current state
	void				SetServerMode(bool bServerMode) { m_bServerMode = bServerMode; }
	bool				GetServerMode() { return m_bServerMode; }
	void				SetRemoteClientMode(bool bRemoteClientMode) { m_bRemoteClientMode = bRemoteClientMode; }
	bool				GetRemoteClientMode() { return m_bRemoteClientMode; }
	void				SetPauseDownload(bool bPauseDownload) { m_bPauseDownload = bPauseDownload; }
	bool				GetPauseDownload() const { return m_bPauseDownload; }
	void				SetPausePostProcess(bool bPausePostProcess) { m_bPausePostProcess = bPausePostProcess; }
	bool				GetPausePostProcess() const { return m_bPausePostProcess; }
	void				SetPauseScan(bool bPauseScan) { m_bPauseScan = bPauseScan; }
	bool				GetPauseScan() const { return m_bPauseScan; }
	void				SetTempPauseDownload(bool bTempPauseDownload) { m_bTempPauseDownload = bTempPauseDownload; }
	bool				GetTempPauseDownload() const { return m_bTempPauseDownload; }
	void				SetDownloadRate(int iRate) { m_iDownloadRate = iRate; }
	int					GetDownloadRate() const { return m_iDownloadRate; }
	void				SetResumeTime(time_t tResumeTime) { m_tResumeTime = tResumeTime; }
	time_t				GetResumeTime() const { return m_tResumeTime; }
	void				SetLocalTimeOffset(int iLocalTimeOffset) { m_iLocalTimeOffset = iLocalTimeOffset; }
	int					GetLocalTimeOffset() { return m_iLocalTimeOffset; }
};

extern Options* g_pOptions;

#endif
