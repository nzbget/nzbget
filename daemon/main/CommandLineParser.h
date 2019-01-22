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


#ifndef COMMANDLINEPARSER_H
#define COMMANDLINEPARSER_H

#include "NString.h"

class CommandLineParser
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
		opClientRequestPostPause,
		opClientRequestPostUnpause,
		opClientRequestScanPause,
		opClientRequestScanUnpause,
		opClientRequestHistory,
		opClientRequestHistoryAll
	};
	enum EMatchMode
	{
		mmId = 1,
		mmName,
		mmRegEx
	};

	typedef std::vector<int> IdList;
	typedef std::vector<CString> NameList;

	CommandLineParser(int argc, const char* argv[]);
	void PrintUsage(const char* com);
	bool GetErrors() { return m_errors; }
	bool GetNoConfig() { return m_noConfig; }
	const char* GetConfigFilename() { return m_configFilename; }
	bool GetServerMode() { return m_serverMode; }
	bool GetDaemonMode() { return m_daemonMode; }
	bool GetRemoteClientMode() { return m_remoteClientMode; }
	EClientOperation GetClientOperation() { return m_clientOperation; }
	NameList* GetOptionList() { return &m_optionList; }
	int GetEditQueueAction() { return m_editQueueAction; }
	int GetEditQueueOffset() { return m_editQueueOffset; }
	IdList* GetEditQueueIdList() { return &m_editQueueIdList; }
	NameList* GetEditQueueNameList() { return &m_editQueueNameList; }
	EMatchMode GetMatchMode() { return m_matchMode; }
	const char* GetEditQueueText() { return m_editQueueText; }
	const char* GetArgFilename() { return m_argFilename; }
	const char* GetAddCategory() { return m_addCategory; }
	bool GetAddPaused() { return m_addPaused; }
	const char* GetLastArg() { return m_lastArg; }
	int GetAddPriority() { return m_addPriority; }
	const char* GetAddNzbFilename() { return m_addNzbFilename; }
	bool GetAddTop() { return m_addTop; }
	const char* GetAddDupeKey() { return m_addDupeKey; }
	int GetAddDupeScore() { return m_addDupeScore; }
	int GetAddDupeMode() { return m_addDupeMode; }
	int GetSetRate() { return m_setRate; }
	int GetLogLines() { return m_logLines; }
	int GetWriteLogKind() { return m_writeLogKind; }
	bool GetTestBacktrace() { return m_testBacktrace; }
	bool GetWebGet() { return m_webGet; }
	const char* GetWebGetFilename() { return m_webGetFilename; }
	bool GetSigVerify() { return m_sigVerify; }
	const char* GetPubKeyFilename() { return m_pubKeyFilename; }
	const char* GetSigFilename() { return m_sigFilename; }
	bool GetPrintOptions() { return m_printOptions; }
	bool GetPrintVersion() { return m_printVersion; }
	bool GetPrintUsage() { return m_printUsage; }
	bool GetPauseDownload() const { return m_pauseDownload; }

private:
	bool m_noConfig = false;
	CString m_configFilename;

	// Parsed command-line parameters
	bool m_errors = false;
	bool m_printVersion = false;
	bool m_printUsage = false;
	bool m_serverMode = false;
	bool m_daemonMode = false;
	bool m_remoteClientMode = false;
	EClientOperation m_clientOperation;
	NameList m_optionList;
	int m_editQueueAction = 0;
	int m_editQueueOffset = 0;
	IdList m_editQueueIdList;
	NameList m_editQueueNameList;
	EMatchMode m_matchMode = mmId;
	CString m_editQueueText;
	CString m_argFilename;
	CString m_addCategory;
	int m_addPriority = 0;
	bool m_addPaused = false;
	CString m_addNzbFilename;
	CString m_lastArg;
	bool m_printOptions = false;
	bool m_addTop = false;
	CString m_addDupeKey;
	int m_addDupeScore = 0;
	int m_addDupeMode = 0;
	int m_setRate = 0;
	int m_logLines = 0;
	int m_writeLogKind = 0;
	bool m_testBacktrace = false;
	bool m_webGet = false;
	CString m_webGetFilename;
	bool m_sigVerify = false;
	CString m_pubKeyFilename;
	CString m_sigFilename;
	bool m_pauseDownload = false;

	void InitCommandLine(int argc, const char* argv[]);
	void InitFileArg(int argc, const char* argv[]);
	void ParseFileIdList(int argc, const char* argv[], int optind);
	void ParseFileNameList(int argc, const char* argv[], int optind);
	void ReportError(const char* errMessage);
};

#endif
