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

#include <vector>
#include <list>
#include <time.h>

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
		mmID = 1,
		mmName,
		mmRegEx
	};

	typedef std::vector<char*>  NameList;

private:
	bool				m_bNoConfig;
	char*				m_szConfigFilename;

	// Parsed command-line parameters
	bool				m_bErrors;
	bool				m_bPrintVersion;
	bool				m_bPrintUsage;
	bool				m_bServerMode;
	bool				m_bDaemonMode;
	bool				m_bRemoteClientMode;
	EClientOperation	m_eClientOperation;
	NameList			m_OptionList;
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
	char*				m_szAddDupeKey;
	int					m_iAddDupeScore;
	int					m_iAddDupeMode;
	int					m_iSetRate;
	int					m_iLogLines;
	int					m_iWriteLogKind;
	bool				m_bTestBacktrace;
	bool				m_bWebGet;
	char*				m_szWebGetFilename;
	bool				m_bPauseDownload;

	void				InitCommandLine(int argc, const char* argv[]);
	void				InitFileArg(int argc, const char* argv[]);
	void				ParseFileIDList(int argc, const char* argv[], int optind);
	void				ParseFileNameList(int argc, const char* argv[], int optind);
	bool				ParseTime(const char* szTime, int* pHours, int* pMinutes);
	void				ReportError(const char* szErrMessage);

public:
						CommandLineParser(int argc, const char* argv[]);
						~CommandLineParser();

	void				PrintUsage(const char* com);

	bool				GetErrors() { return m_bErrors; }
	bool				GetNoConfig() { return m_bNoConfig; }
	const char*			GetConfigFilename() { return m_szConfigFilename; }
	bool				GetServerMode() { return m_bServerMode; }
	bool				GetDaemonMode() { return m_bDaemonMode; }
	bool				GetRemoteClientMode() { return m_bRemoteClientMode; }
	EClientOperation	GetClientOperation() { return m_eClientOperation; }
	NameList*			GetOptionList() { return &m_OptionList; }
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
	const char*			GetAddDupeKey() { return m_szAddDupeKey; }
	int					GetAddDupeScore() { return m_iAddDupeScore; }
	int					GetAddDupeMode() { return m_iAddDupeMode; }
	int					GetSetRate() { return m_iSetRate; }
	int					GetLogLines() { return m_iLogLines; }
	int					GetWriteLogKind() { return m_iWriteLogKind; }
	bool				GetTestBacktrace() { return m_bTestBacktrace; }
	bool				GetWebGet() { return m_bWebGet; }
	const char*			GetWebGetFilename() { return m_szWebGetFilename; }
	bool				GetPrintOptions() { return m_bPrintOptions; }
	bool				GetPrintVersion() { return m_bPrintVersion; }
	bool				GetPrintUsage() { return m_bPrintUsage; }
	bool				GetPauseDownload() const { return m_bPauseDownload; }
};

extern CommandLineParser* g_pCommandLineParser;

#endif
