/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
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


#ifndef OPTIONS_H
#define OPTIONS_H

#include "NString.h"
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
		psExtended,
		psFull,
		psDupe
	};
	enum EHealthCheck
	{
		hcPause,
		hcDelete,
		hcPark,
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
	enum EPostStrategy
	{
		ppSequential,
		ppBalanced,
		ppAggressive,
		ppRocket
	};
	enum EFileNaming
	{
		nfAuto,
		nfArticle,
		nfNzb
	};

	class OptEntry
	{
	public:
		OptEntry(const char* name, const char* value) :
			m_name(name), m_value(value) {}
		void SetName(const char* name) { m_name = name; }
		const char* GetName() { return m_name; }
		void SetValue(const char* value);
		const char* GetValue() { return m_value; }
		const char* GetDefValue() { return m_defValue; }
		int GetLineNo() { return m_lineNo; }
		bool Restricted();

	private:
		CString m_name;
		CString m_value;
		CString m_defValue;
		int m_lineNo = 0;

		void SetLineNo(int lineNo) { m_lineNo = lineNo; }

		friend class Options;
	};

	typedef std::deque<OptEntry> OptEntriesBase;

	class OptEntries: public OptEntriesBase
	{
	public:
		OptEntry* FindOption(const char* name);
	};

	typedef GuardedPtr<OptEntries> GuardedOptEntries;

	typedef std::vector<CString> NameList;
	typedef std::vector<const char*> CmdOptList;

	class Category
	{
	public:
		Category(const char* name, const char* destDir, bool unpack, const char* extensions) :
			m_name(name), m_destDir(destDir), m_unpack(unpack), m_extensions(extensions) {}
		const char* GetName() { return m_name; }
		const char* GetDestDir() { return m_destDir; }
		bool GetUnpack() { return m_unpack; }
		const char* GetExtensions() { return m_extensions; }
		NameList* GetAliases() { return &m_aliases; }

	private:
		CString m_name;
		CString m_destDir;
		bool m_unpack;
		CString m_extensions;
		NameList m_aliases;
	};

	typedef std::deque<Category> CategoriesBase;

	class Categories: public CategoriesBase
	{
	public:
		Category* FindCategory(const char* name, bool searchAliases);
	};

	class Extender
	{
	public:
		virtual void AddNewsServer(int id, bool active, const char* name, const char* host,
			int port, int ipVersion, const char* user, const char* pass, bool joinGroup,
			bool tls, const char* cipher, int maxConnections, int retention,
			int level, int group, bool optional) = 0;
		virtual void AddFeed(int id, const char* name, const char* url, int interval,
			const char* filter, bool backlog, bool pauseNzb, const char* category,
			int priority, const char* extensions) {}
		virtual void AddTask(int id, int hours, int minutes, int weekDaysBits, ESchedulerCommand command,
			const char* param) {}
		virtual void SetupFirstStart() {}
	};

	Options(const char* exeName, const char* configFilename, bool noConfig,
		CmdOptList* commandLineOptions, Extender* extender);
	Options(CmdOptList* commandLineOptions, Extender* extender);
	~Options();

	static bool SplitOptionString(const char* option, CString& optName, CString& optValue);
	static void ConvertOldOptions(OptEntries* optEntries);
	bool GetFatalError() { return m_fatalError; }
	GuardedOptEntries GuardOptEntries() { return GuardedOptEntries(&m_optEntries, &m_optEntriesMutex); }
	void CreateSchedulerTask(int id, const char* time, const char* weekDays,
		ESchedulerCommand command, const char* param);

	// Options
	const char* GetConfigFilename() { return m_configFilename; }
	bool GetConfigErrors() { return m_configErrors; }
	const char* GetAppDir() { return m_appDir; }
	const char* GetDestDir() { return m_destDir; }
	const char* GetInterDir() { return m_interDir; }
	const char* GetTempDir() { return m_tempDir; }
	const char* GetQueueDir() { return m_queueDir; }
	const char* GetNzbDir() { return m_nzbDir; }
	const char* GetWebDir() { return m_webDir; }
	const char* GetConfigTemplate() { return m_configTemplate; }
	const char* GetScriptDir() { return m_scriptDir; }
	const char* GetRequiredDir() { return m_requiredDir; }
	bool GetNzbLog() const { return m_nzbLog; }
	EMessageTarget GetInfoTarget() const { return m_infoTarget; }
	EMessageTarget GetWarningTarget() const { return m_warningTarget; }
	EMessageTarget GetErrorTarget() const { return m_errorTarget; }
	EMessageTarget GetDebugTarget() const { return m_debugTarget; }
	EMessageTarget GetDetailTarget() const { return m_detailTarget; }
	int GetArticleTimeout() { return m_articleTimeout; }
	int GetUrlTimeout() { return m_urlTimeout; }
	int GetRemoteTimeout() { return m_remoteTimeout; }
	bool GetRawArticle() { return m_rawArticle; };
	bool GetSkipWrite() { return m_skipWrite; };
	bool GetAppendCategoryDir() { return m_appendCategoryDir; }
	bool GetContinuePartial() { return m_continuePartial; }
	int GetArticleRetries() { return m_articleRetries; }
	int GetArticleInterval() { return m_articleInterval; }
	int GetUrlRetries() { return m_urlRetries; }
	int GetUrlInterval() { return m_urlInterval; }
	bool GetFlushQueue() { return m_flushQueue; }
	bool GetDupeCheck() { return m_dupeCheck; }
	const char* GetControlIp() { return m_controlIp; }
	const char* GetControlUsername() { return m_controlUsername; }
	const char* GetControlPassword() { return m_controlPassword; }
	const char* GetRestrictedUsername() { return m_restrictedUsername; }
	const char* GetRestrictedPassword() { return m_restrictedPassword; }
	const char* GetAddUsername() { return m_addUsername; }
	const char* GetAddPassword() { return m_addPassword; }
	int GetControlPort() { return m_controlPort; }
	bool GetFormAuth() { return m_formAuth; }
	bool GetSecureControl() { return m_secureControl; }
	int GetSecurePort() { return m_securePort; }
	const char* GetSecureCert() { return m_secureCert; }
	const char* GetSecureKey() { return m_secureKey; }
	const char* GetCertStore() { return m_certStore; }
	bool GetCertCheck() { return m_certCheck; }
	const char* GetAuthorizedIp() { return m_authorizedIp; }
	const char* GetLockFile() { return m_lockFile; }
	const char* GetDaemonUsername() { return m_daemonUsername; }
	EOutputMode GetOutputMode() { return m_outputMode; }
	int GetUrlConnections() { return m_urlConnections; }
	int GetLogBuffer() { return m_logBuffer; }
	EWriteLog GetWriteLog() { return m_writeLog; }
	const char* GetLogFile() { return m_logFile; }
	int GetRotateLog() { return m_rotateLog; }
	EParCheck GetParCheck() { return m_parCheck; }
	bool GetParRepair() { return m_parRepair; }
	EParScan GetParScan() { return m_parScan; }
	bool GetParQuick() { return m_parQuick; }
	EPostStrategy GetPostStrategy() { return m_postStrategy; }
	bool GetParRename() { return m_parRename; }
	int GetParBuffer() { return m_parBuffer; }
	int GetParThreads() { return m_parThreads; }
	bool GetRarRename() { return m_rarRename; }
	EHealthCheck GetHealthCheck() { return m_healthCheck; }
	const char* GetScriptOrder() { return m_scriptOrder; }
	const char* GetExtensions() { return m_extensions; }
	int GetUMask() { return m_umask; }
	int GetUpdateInterval() {return m_updateInterval; }
	bool GetCursesNzbName() { return m_cursesNzbName; }
	bool GetCursesTime() { return m_cursesTime; }
	bool GetCursesGroup() { return m_cursesGroup; }
	bool GetCrcCheck() { return m_crcCheck; }
	bool GetDirectWrite() { return m_directWrite; }
	int GetWriteBuffer() { return m_writeBuffer; }
	int GetNzbDirInterval() { return m_nzbDirInterval; }
	int GetNzbDirFileAge() { return m_nzbDirFileAge; }
	int GetDiskSpace() { return m_diskSpace; }
	bool GetTls() { return m_tls; }
	bool GetCrashTrace() { return m_crashTrace; }
	bool GetCrashDump() { return m_crashDump; }
	bool GetParPauseQueue() { return m_parPauseQueue; }
	bool GetScriptPauseQueue() { return m_scriptPauseQueue; }
	bool GetNzbCleanupDisk() { return m_nzbCleanupDisk; }
	int GetParTimeLimit() { return m_parTimeLimit; }
	int GetKeepHistory() { return m_keepHistory; }
	bool GetUnpack() { return m_unpack; }
	bool GetDirectUnpack() { return m_directUnpack; }
	bool GetUnpackCleanupDisk() { return m_unpackCleanupDisk; }
	const char* GetUnrarCmd() { return m_unrarCmd; }
	const char* GetSevenZipCmd() { return m_sevenZipCmd; }
	const char* GetUnpackPassFile() { return m_unpackPassFile; }
	bool GetUnpackPauseQueue() { return m_unpackPauseQueue; }
	const char* GetExtCleanupDisk() { return m_extCleanupDisk; }
	const char* GetParIgnoreExt() { return m_parIgnoreExt; }
	const char* GetUnpackIgnoreExt() { return m_unpackIgnoreExt; }
	int GetFeedHistory() { return m_feedHistory; }
	bool GetUrlForce() { return m_urlForce; }
	int GetTimeCorrection() { return m_timeCorrection; }
	int GetPropagationDelay() { return m_propagationDelay; }
	int GetArticleCache() { return m_articleCache; }
	int GetEventInterval() { return m_eventInterval; }
	const char* GetShellOverride() { return m_shellOverride; }
	int GetMonthlyQuota() { return m_monthlyQuota; }
	int GetQuotaStartDay() { return m_quotaStartDay; }
	int GetDailyQuota() { return m_dailyQuota; }
	bool GetDirectRename() { return m_directRename; }
	bool GetReorderFiles() { return m_reorderFiles; }
	EFileNaming GetFileNaming() { return m_fileNaming; }
	int GetDownloadRate() const { return m_downloadRate; }

	Categories* GetCategories() { return &m_categories; }
	Category* FindCategory(const char* name, bool searchAliases) { return m_categories.FindCategory(name, searchAliases); }

	int GetArticleReadChunkSize() { return m_articleReadChunkSize; }

	// Current state
	void SetServerMode(bool serverMode) { m_serverMode = serverMode; }
	bool GetServerMode() { return m_serverMode; }
	void SetDaemonMode(bool daemonMode) { m_daemonMode = daemonMode; }
	bool GetDaemonMode() { return m_daemonMode; }
	void SetRemoteClientMode(bool remoteClientMode) { m_remoteClientMode = remoteClientMode; }
	bool GetRemoteClientMode() { return m_remoteClientMode; }

private:
	OptEntries m_optEntries;
	Mutex m_optEntriesMutex;
	Categories m_categories;
	bool m_noDiskAccess = false;
	bool m_noConfig = false;
	bool m_fatalError = false;
	Extender* m_extender;

	// Options
	bool m_configErrors = false;
	int m_configLine = 0;
	CString m_appDir;
	CString m_configFilename;
	CString m_destDir;
	CString m_interDir;
	CString m_tempDir;
	CString m_queueDir;
	CString m_nzbDir;
	CString m_webDir;
	CString m_configTemplate;
	CString m_scriptDir;
	CString m_requiredDir;
	EMessageTarget m_infoTarget = mtScreen;
	EMessageTarget m_warningTarget = mtScreen;
	EMessageTarget m_errorTarget = mtScreen;
	EMessageTarget m_debugTarget = mtNone;
	EMessageTarget m_detailTarget = mtScreen;
	bool m_skipWrite = false;
	bool m_rawArticle = false;
	bool m_nzbLog = false;
	int m_articleTimeout = 0;
	int m_urlTimeout = 0;
	int m_remoteTimeout = 0;
	bool m_appendCategoryDir = false;
	bool m_continuePartial = false;
	int m_articleRetries = 0;
	int m_articleInterval = 0;
	int m_urlRetries = 0;
	int m_urlInterval = 0;
	bool m_flushQueue = false;
	bool m_dupeCheck = false;
	CString m_controlIp;
	CString m_controlUsername;
	CString m_controlPassword;
	CString m_restrictedUsername;
	CString m_restrictedPassword;
	CString m_addUsername;
	CString m_addPassword;
	bool m_formAuth = false;
	int m_controlPort = 0;
	bool m_secureControl = false;
	int m_securePort = 0;
	CString m_secureCert;
	CString m_secureKey;
	CString m_certStore;
	bool m_certCheck = false;
	CString m_authorizedIp;
	CString m_lockFile;
	CString m_daemonUsername;
	EOutputMode m_outputMode = omLoggable;
	int m_urlConnections = 0;
	int m_logBuffer = 0;
	EWriteLog m_writeLog = wlAppend;
	int m_rotateLog = 0;
	CString m_logFile;
	EParCheck m_parCheck = pcManual;
	bool m_parRepair = false;
	EParScan m_parScan = psLimited;
	bool m_parQuick = true;
	EPostStrategy m_postStrategy = ppSequential;
	bool m_parRename = false;
	int m_parBuffer = 0;
	int m_parThreads = 0;
	bool m_rarRename = false;
	bool m_directRename = false;
	EHealthCheck m_healthCheck = hcNone;
	CString m_extensions;
	CString m_scriptOrder;
	int m_umask = 0;
	int m_updateInterval = 0;
	bool m_cursesNzbName = false;
	bool m_cursesTime = false;
	bool m_cursesGroup = false;
	bool m_crcCheck = false;
	bool m_directWrite = false;
	int m_writeBuffer = 0;
	int m_nzbDirInterval = 0;
	int m_nzbDirFileAge = 0;
	int m_diskSpace = 0;
	bool m_tls = false;
	bool m_crashTrace = false;
	bool m_crashDump = false;
	bool m_parPauseQueue = false;
	bool m_scriptPauseQueue = false;
	bool m_nzbCleanupDisk = false;
	int m_parTimeLimit = 0;
	int m_keepHistory = 0;
	bool m_unpack = false;
	bool m_directUnpack = false;
	bool m_unpackCleanupDisk = false;
	CString m_unrarCmd;
	CString m_sevenZipCmd;
	CString m_unpackPassFile;
	bool m_unpackPauseQueue;
	CString m_extCleanupDisk;
	CString m_parIgnoreExt;
	CString m_unpackIgnoreExt;
	int m_feedHistory = 0;
	bool m_urlForce = false;
	int m_timeCorrection = 0;
	int m_propagationDelay = 0;
	int m_articleCache = 0;
	int m_eventInterval = 0;
	CString m_shellOverride;
	int m_monthlyQuota = 0;
	int m_quotaStartDay = 0;
	int m_dailyQuota = 0;
	bool m_reorderFiles = false;
	EFileNaming m_fileNaming = nfArticle;
	int m_downloadRate = 0;
	int m_articleReadChunkSize = 0;

	// Application mode
	bool m_serverMode = false;
	bool m_daemonMode = false;
	bool m_remoteClientMode = false;

	void Init(const char* exeName, const char* configFilename, bool noConfig,
		CmdOptList* commandLineOptions, bool noDiskAccess, Extender* extender);
	void InitDefaults();
	void InitOptions();
	void InitOptFile();
	void InitServers();
	void InitCategories();
	void InitScheduler();
	void InitFeeds();
	void InitCommandLineOptions(CmdOptList* commandLineOptions);
	void CheckOptions();
	int ParseEnumValue(const char* OptName, int argc, const char* argn[], const int argv[]);
	int ParseIntValue(const char* OptName, int base);
	OptEntry* FindOption(const char* optname);
	const char* GetOption(const char* optname);
	void SetOption(const char* optname, const char* value);
	bool SetOptionString(const char* option);
	bool ValidateOptionName(const char* optname, const char* optvalue);
	void LoadConfigFile();
	void CheckDir(CString& dir, const char* optionName, const char* parentDir,
		bool allowEmpty, bool create);
	bool ParseTime(const char* time, int* hours, int* minutes);
	bool ParseWeekDays(const char* weekDays, int* weekDaysBits);
	void ConfigError(const char* msg, ...);
	void ConfigWarn(const char* msg, ...);
	void LocateOptionSrcPos(const char *optionName);
	static void ConvertOldOption(CString& option, CString& value);
	static void MergeOldScriptOption(OptEntries* optEntries, const char* optname, bool mergeCategories);
	static bool HasScript(const char* scriptList, const char* scriptName);
};

extern Options* g_Options;

#endif
