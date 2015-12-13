/*
 *  This file is part of nzbget
 *
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

	typedef std::vector<char*>  NameList;

private:
	bool				m_noConfig;
	CString				m_configFilename;

	// Parsed command-line parameters
	bool				m_errors;
	bool				m_printVersion;
	bool				m_printUsage;
	bool				m_serverMode;
	bool				m_daemonMode;
	bool				m_remoteClientMode;
	EClientOperation	m_clientOperation;
	NameList			m_optionList;
	int					m_editQueueAction;
	int					m_editQueueOffset;
	int*				m_editQueueIdList;
	int					m_editQueueIdCount;
	NameList			m_editQueueNameList;
	EMatchMode			m_matchMode;
	CString				m_editQueueText;
	CString				m_argFilename;
	CString				m_addCategory;
	int					m_addPriority;
	bool				m_addPaused;
	CString				m_addNzbFilename;
	CString				m_lastArg;
	bool				m_printOptions;
	bool				m_addTop;
	CString				m_addDupeKey;
	int					m_addDupeScore;
	int					m_addDupeMode;
	int					m_setRate;
	int					m_logLines;
	int					m_writeLogKind;
	bool				m_testBacktrace;
	bool				m_webGet;
	CString				m_webGetFilename;
	bool				m_sigVerify;
	CString				m_pubKeyFilename;
	CString				m_sigFilename;
	bool				m_pauseDownload;

	void				InitCommandLine(int argc, const char* argv[]);
	void				InitFileArg(int argc, const char* argv[]);
	void				ParseFileIdList(int argc, const char* argv[], int optind);
	void				ParseFileNameList(int argc, const char* argv[], int optind);
	void				ReportError(const char* errMessage);

public:
						CommandLineParser(int argc, const char* argv[]);
						~CommandLineParser();

	void				PrintUsage(const char* com);

	bool				GetErrors() { return m_errors; }
	bool				GetNoConfig() { return m_noConfig; }
	const char*			GetConfigFilename() { return m_configFilename; }
	bool				GetServerMode() { return m_serverMode; }
	bool				GetDaemonMode() { return m_daemonMode; }
	bool				GetRemoteClientMode() { return m_remoteClientMode; }
	EClientOperation	GetClientOperation() { return m_clientOperation; }
	NameList*			GetOptionList() { return &m_optionList; }
	int					GetEditQueueAction() { return m_editQueueAction; }
	int					GetEditQueueOffset() { return m_editQueueOffset; }
	int*				GetEditQueueIdList() { return m_editQueueIdList; }
	int					GetEditQueueIdCount() { return m_editQueueIdCount; }
	NameList*			GetEditQueueNameList() { return &m_editQueueNameList; }
	EMatchMode			GetMatchMode() { return m_matchMode; }
	const char*			GetEditQueueText() { return m_editQueueText; }
	const char*			GetArgFilename() { return m_argFilename; }
	const char*			GetAddCategory() { return m_addCategory; }
	bool				GetAddPaused() { return m_addPaused; }
	const char*			GetLastArg() { return m_lastArg; }
	int					GetAddPriority() { return m_addPriority; }
	const char*			GetAddNzbFilename() { return m_addNzbFilename; }
	bool				GetAddTop() { return m_addTop; }
	const char*			GetAddDupeKey() { return m_addDupeKey; }
	int					GetAddDupeScore() { return m_addDupeScore; }
	int					GetAddDupeMode() { return m_addDupeMode; }
	int					GetSetRate() { return m_setRate; }
	int					GetLogLines() { return m_logLines; }
	int					GetWriteLogKind() { return m_writeLogKind; }
	bool				GetTestBacktrace() { return m_testBacktrace; }
	bool				GetWebGet() { return m_webGet; }
	const char*			GetWebGetFilename() { return m_webGetFilename; }
	bool				GetSigVerify() { return m_sigVerify; }
	const char*			GetPubKeyFilename() { return m_pubKeyFilename; }
	const char*			GetSigFilename() { return m_sigFilename; }
	bool				GetPrintOptions() { return m_printOptions; }
	bool				GetPrintVersion() { return m_printVersion; }
	bool				GetPrintUsage() { return m_printUsage; }
	bool				GetPauseDownload() const { return m_pauseDownload; }
};

extern CommandLineParser* g_CommandLineParser;

#endif
