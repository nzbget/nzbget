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


#ifdef HAVE_CONFIG_H
#include "config.h"					  
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <set>
#ifdef WIN32
#include <direct.h>
#include <Shlobj.h>
#else
#include <unistd.h>
#include <getopt.h>
#endif

#include "nzbget.h"
#include "Util.h"
#include "Log.h"
#include "MessageBase.h"
#include "DownloadInfo.h"
#include "CommandLineParser.h"

#ifdef HAVE_GETOPT_LONG
static struct option long_options[] =
    {
	    {"help", no_argument, 0, 'h'},
	    {"configfile", required_argument, 0, 'c'},
	    {"noconfigfile", no_argument, 0, 'n'},
	    {"printconfig", no_argument, 0, 'p'},
	    {"server", no_argument, 0, 's' },
	    {"daemon", no_argument, 0, 'D' },
	    {"version", no_argument, 0, 'v'},
	    {"serverversion", no_argument, 0, 'V'},
	    {"option", required_argument, 0, 'o'},
	    {"append", no_argument, 0, 'A'},
	    {"list", no_argument, 0, 'L'},
	    {"pause", no_argument, 0, 'P'},
	    {"unpause", no_argument, 0, 'U'},
	    {"rate", required_argument, 0, 'R'},
	    {"system", no_argument, 0, 'B'},
	    {"log", required_argument, 0, 'G'},
	    {"top", no_argument, 0, 'T'},
	    {"edit", required_argument, 0, 'E'},
	    {"connect", no_argument, 0, 'C'},
	    {"quit", no_argument, 0, 'Q'},
	    {"reload", no_argument, 0, 'O'},
	    {"write", required_argument, 0, 'W'},
	    {"category", required_argument, 0, 'K'},
	    {"scan", no_argument, 0, 'S'},
	    {0, 0, 0, 0}
    };
#endif

static char short_options[] = "c:hno:psvAB:DCE:G:K:LPR:STUQOVW:";


CommandLineParser::CommandLineParser(int argc, const char* argv[])
{
	m_bNoConfig = false;
	m_szConfigFilename = NULL;
	m_bErrors = false;
	m_bPrintVersion = false;
	m_bPrintUsage = false;

	m_iEditQueueAction		= 0;
	m_pEditQueueIDList		= NULL;
	m_iEditQueueIDCount		= 0;
	m_iEditQueueOffset		= 0;
	m_szEditQueueText		= NULL;
	m_szArgFilename			= NULL;
	m_szLastArg				= NULL;
	m_szAddCategory			= NULL;
	m_iAddPriority			= 0;
	m_szAddNZBFilename		= NULL;
	m_bAddPaused			= false;
	m_bServerMode			= false;
	m_bDaemonMode			= false;
	m_bRemoteClientMode		= false;
	m_bPrintOptions			= false;
	m_bAddTop				= false;
	m_szAddDupeKey			= NULL;
	m_iAddDupeScore			= 0;
	m_iAddDupeMode			= 0;
	m_iLogLines				= 0;
	m_iWriteLogKind			= 0;
	m_bTestBacktrace		= false;
	m_bWebGet				= false;
	m_szWebGetFilename		= NULL;
	m_EMatchMode			= mmID;
	m_bPauseDownload		= false;

	InitCommandLine(argc, argv);

	if (argc == 1)
	{
		m_bPrintUsage = true;
		return;
	}

	if (!m_bPrintOptions)
	{
		InitFileArg(argc, argv);
	}
}

CommandLineParser::~CommandLineParser()
{
	free(m_szConfigFilename);
	free(m_szArgFilename);
	free(m_szAddCategory);
	free(m_szEditQueueText);
	free(m_szLastArg);
	free(m_pEditQueueIDList);
	free(m_szAddNZBFilename);
	free(m_szAddDupeKey);
	free(m_szWebGetFilename);

	for (NameList::iterator it = m_EditQueueNameList.begin(); it != m_EditQueueNameList.end(); it++)
	{
		free(*it);
	}
	m_EditQueueNameList.clear();

	for (NameList::iterator it = m_OptionList.begin(); it != m_OptionList.end(); it++)
	{
		free(*it);
	}
	m_OptionList.clear();
}

void CommandLineParser::InitCommandLine(int argc, const char* const_argv[])
{
	m_eClientOperation = opClientNoOperation; // default

	char** argv = (char**)malloc(sizeof(char*) * argc);
	for (int i = 0; i < argc; i++)
	{
		argv[i] = strdup(const_argv[i]);
	}

	// reset getopt
	optind = 1;

	while (true)
	{
		int c;

#ifdef HAVE_GETOPT_LONG
		int option_index  = 0;
		c = getopt_long(argc, argv, short_options, long_options, &option_index);
#else
		c = getopt(argc, argv, short_options);
#endif

		if (c == -1) break;

		switch (c)
		{
			case 'c':
				m_szConfigFilename = strdup(optarg);
				break;
			case 'n':
				m_szConfigFilename = NULL;
				m_bNoConfig = true;
				break;
			case 'h':
				m_bPrintUsage = true;
				return;
			case 'v':
				m_bPrintVersion = true;
				return;
			case 'p':
				m_bPrintOptions = true;
				break;
			case 'o':
				m_OptionList.push_back(strdup(optarg));
				break;
			case 's':
				m_bServerMode = true;
				break;
			case 'D':
				m_bServerMode = true;
				m_bDaemonMode = true;
				break;
			case 'A':
				m_eClientOperation = opClientRequestDownload;

				while (true)
				{
					optind++;
					optarg = optind > argc ? NULL : argv[optind-1];
					if (optarg && (!strcasecmp(optarg, "F") || !strcasecmp(optarg, "U")))
					{
						// option ignored (but kept for compatibility)
					}
					else if (optarg && !strcasecmp(optarg, "T"))
					{
						m_bAddTop = true;
					}
					else if (optarg && !strcasecmp(optarg, "P"))
					{
						m_bAddPaused = true;
					}
					else if (optarg && !strcasecmp(optarg, "I"))
					{
						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'A'");
							return;
						}
						m_iAddPriority = atoi(argv[optind-1]);
					}
					else if (optarg && !strcasecmp(optarg, "C"))
					{
						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'A'");
							return;
						}
						free(m_szAddCategory);
						m_szAddCategory = strdup(argv[optind-1]);
					}
					else if (optarg && !strcasecmp(optarg, "N"))
					{
						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'A'");
							return;
						}
						free(m_szAddNZBFilename);
						m_szAddNZBFilename = strdup(argv[optind-1]);
					}
					else if (optarg && !strcasecmp(optarg, "DK"))
					{
						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'A'");
							return;
						}
						free(m_szAddDupeKey);
						m_szAddDupeKey = strdup(argv[optind-1]);
					}
					else if (optarg && !strcasecmp(optarg, "DS"))
					{
						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'A'");
							return;
						}
						m_iAddDupeScore = atoi(argv[optind-1]);
					}
					else if (optarg && !strcasecmp(optarg, "DM"))
					{
						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'A'");
							return;
						}

						const char* szDupeMode = argv[optind-1];
						if (!strcasecmp(szDupeMode, "score"))
						{
							m_iAddDupeMode = dmScore;
						}
						else if (!strcasecmp(szDupeMode, "all"))
						{
							m_iAddDupeMode = dmAll;
						}
						else if (!strcasecmp(szDupeMode, "force"))
						{
							m_iAddDupeMode = dmForce;
						}
						else
						{
							ReportError("Could not parse value of option 'A'");
							return;
						}
					}
					else
					{
						optind--;
						break;
					}
				}
				break;
			case 'L':
				optind++;
				optarg = optind > argc ? NULL : argv[optind-1];
				if (!optarg || !strncmp(optarg, "-", 1))
				{
					m_eClientOperation = opClientRequestListFiles;
					optind--;
				}
				else if (!strcasecmp(optarg, "F") || !strcasecmp(optarg, "FR"))
				{
					m_eClientOperation = opClientRequestListFiles;
				}
				else if (!strcasecmp(optarg, "G") || !strcasecmp(optarg, "GR"))
				{
					m_eClientOperation = opClientRequestListGroups;
				}
				else if (!strcasecmp(optarg, "O"))
				{
					m_eClientOperation = opClientRequestPostQueue;
				}
				else if (!strcasecmp(optarg, "S"))
				{
					m_eClientOperation = opClientRequestListStatus;
				}
				else if (!strcasecmp(optarg, "H"))
				{
					m_eClientOperation = opClientRequestHistory;
				}
				else if (!strcasecmp(optarg, "HA"))
				{
					m_eClientOperation = opClientRequestHistoryAll;
				}
				else
				{
					ReportError("Could not parse value of option 'L'");
					return;
				}

				if (optarg && (!strcasecmp(optarg, "FR") || !strcasecmp(optarg, "GR")))
				{
					m_EMatchMode = mmRegEx;

					optind++;
					if (optind > argc)
					{
						ReportError("Could not parse value of option 'L'");
						return;
					}
					m_szEditQueueText = strdup(argv[optind-1]);
				}
				break;
			case 'P':
			case 'U':
				optind++;
				optarg = optind > argc ? NULL : argv[optind-1];
				if (!optarg || !strncmp(optarg, "-", 1))
				{
					m_eClientOperation = c == 'P' ? opClientRequestDownloadPause : opClientRequestDownloadUnpause;
					optind--;
				}
				else if (!strcasecmp(optarg, "D"))
				{
					m_eClientOperation = c == 'P' ? opClientRequestDownloadPause : opClientRequestDownloadUnpause;
				}
				else if (!strcasecmp(optarg, "O"))
				{
					m_eClientOperation = c == 'P' ? opClientRequestPostPause : opClientRequestPostUnpause;
				}
				else if (!strcasecmp(optarg, "S"))
				{
					m_eClientOperation = c == 'P' ? opClientRequestScanPause : opClientRequestScanUnpause;
				}
				else
				{
					ReportError(c == 'P' ? "Could not parse value of option 'P'\n" : "Could not parse value of option 'U'");
					return;
				}
				break;
			case 'R':
				m_eClientOperation = opClientRequestSetRate;
				m_iSetRate = (int)(atof(optarg)*1024);
				break;
			case 'B':
				if (!strcasecmp(optarg, "dump"))
				{
					m_eClientOperation = opClientRequestDumpDebug;
				}
				else if (!strcasecmp(optarg, "trace"))
				{
					m_bTestBacktrace = true;
				}
				else if (!strcasecmp(optarg, "webget"))
				{
					m_bWebGet = true;
					optind++;
					if (optind > argc)
					{
						ReportError("Could not parse value of option 'E'");
						return;
					}
					optarg = argv[optind-1];
					m_szWebGetFilename = strdup(optarg);
				}
				else
				{
					ReportError("Could not parse value of option 'B'");
					return;
				}
				break;
			case 'G':
				m_eClientOperation = opClientRequestLog;
				m_iLogLines = atoi(optarg);
				if (m_iLogLines == 0)
				{
					ReportError("Could not parse value of option 'G'");
					return;
				}
				break;
			case 'T':
				m_bAddTop = true;
				break;
			case 'C':
				m_bRemoteClientMode = true;
				break;
			case 'E':
			{
				m_eClientOperation = opClientRequestEditQueue;
				bool bGroup = !strcasecmp(optarg, "G") || !strcasecmp(optarg, "GN") || !strcasecmp(optarg, "GR");
				bool bFile = !strcasecmp(optarg, "F") || !strcasecmp(optarg, "FN") || !strcasecmp(optarg, "FR");
				if (!strcasecmp(optarg, "GN") || !strcasecmp(optarg, "FN"))
				{
					m_EMatchMode = mmName;
				}
				else if (!strcasecmp(optarg, "GR") || !strcasecmp(optarg, "FR"))
				{
					m_EMatchMode = mmRegEx;
				}
				else
				{
					m_EMatchMode = mmID;
				};
				bool bPost = !strcasecmp(optarg, "O");
				bool bHistory = !strcasecmp(optarg, "H");
				if (bGroup || bFile || bPost || bHistory)
				{
					optind++;
					if (optind > argc)
					{
						ReportError("Could not parse value of option 'E'");
						return;
					}
					optarg = argv[optind-1];
				}

				if (bPost)
				{
					// edit-commands for post-processor-queue
					if (!strcasecmp(optarg, "D"))
					{
						m_iEditQueueAction = DownloadQueue::eaPostDelete;
					}
					else
					{
						ReportError("Could not parse value of option 'E'");
						return;
					}
				}
				else if (bHistory)
				{
					// edit-commands for history
					if (!strcasecmp(optarg, "D"))
					{
						m_iEditQueueAction = DownloadQueue::eaHistoryDelete;
					}
					else if (!strcasecmp(optarg, "R"))
					{
						m_iEditQueueAction = DownloadQueue::eaHistoryReturn;
					}
					else if (!strcasecmp(optarg, "P"))
					{
						m_iEditQueueAction = DownloadQueue::eaHistoryProcess;
					}
					else if (!strcasecmp(optarg, "A"))
					{
						m_iEditQueueAction = DownloadQueue::eaHistoryRedownload;
					}
					else if (!strcasecmp(optarg, "O"))
					{
						m_iEditQueueAction = DownloadQueue::eaHistorySetParameter;
						
						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'E'");
							return;
						}
						m_szEditQueueText = strdup(argv[optind-1]);
						
						if (!strchr(m_szEditQueueText, '='))
						{
							ReportError("Could not parse value of option 'E'");
							return;
						}
					}
					else if (!strcasecmp(optarg, "B"))
					{
						m_iEditQueueAction = DownloadQueue::eaHistoryMarkBad;
					}
					else if (!strcasecmp(optarg, "G"))
					{
						m_iEditQueueAction = DownloadQueue::eaHistoryMarkGood;
					}
					else if (!strcasecmp(optarg, "S"))
					{
						m_iEditQueueAction = DownloadQueue::eaHistoryMarkSuccess;
					}
					else
					{
						ReportError("Could not parse value of option 'E'");
						return;
					}
				}
				else
				{
					// edit-commands for download-queue
					if (!strcasecmp(optarg, "T"))
					{
						m_iEditQueueAction = bGroup ? DownloadQueue::eaGroupMoveTop : DownloadQueue::eaFileMoveTop;
					}
					else if (!strcasecmp(optarg, "B"))
					{
						m_iEditQueueAction = bGroup ? DownloadQueue::eaGroupMoveBottom : DownloadQueue::eaFileMoveBottom;
					}
					else if (!strcasecmp(optarg, "P"))
					{
						m_iEditQueueAction = bGroup ? DownloadQueue::eaGroupPause : DownloadQueue::eaFilePause;
					}
					else if (!strcasecmp(optarg, "A"))
					{
						m_iEditQueueAction = bGroup ? DownloadQueue::eaGroupPauseAllPars : DownloadQueue::eaFilePauseAllPars;
					}
					else if (!strcasecmp(optarg, "R"))
					{
						m_iEditQueueAction = bGroup ? DownloadQueue::eaGroupPauseExtraPars : DownloadQueue::eaFilePauseExtraPars;
					}
					else if (!strcasecmp(optarg, "U"))
					{
						m_iEditQueueAction = bGroup ? DownloadQueue::eaGroupResume : DownloadQueue::eaFileResume;
					}
					else if (!strcasecmp(optarg, "D"))
					{
						m_iEditQueueAction = bGroup ? DownloadQueue::eaGroupDelete : DownloadQueue::eaFileDelete;
					}
					else if (!strcasecmp(optarg, "C") || !strcasecmp(optarg, "K") || !strcasecmp(optarg, "CP"))
					{
						// switch "K" is provided for compatibility with v. 0.8.0 and can be removed in future versions
						if (!bGroup)
						{
							ReportError("Category can be set only for groups");
							return;
						}
						m_iEditQueueAction = !strcasecmp(optarg, "CP") ? DownloadQueue::eaGroupApplyCategory : DownloadQueue::eaGroupSetCategory;

						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'E'");
							return;
						}
						m_szEditQueueText = strdup(argv[optind-1]);
					}
					else if (!strcasecmp(optarg, "N"))
					{
						if (!bGroup)
						{
							ReportError("Only groups can be renamed");
							return;
						}
						m_iEditQueueAction = DownloadQueue::eaGroupSetName;

						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'E'");
							return;
						}
						m_szEditQueueText = strdup(argv[optind-1]);
					}
					else if (!strcasecmp(optarg, "M"))
					{
						if (!bGroup)
						{
							ReportError("Only groups can be merged");
							return;
						}
						m_iEditQueueAction = DownloadQueue::eaGroupMerge;
					}
					else if (!strcasecmp(optarg, "S"))
					{
						m_iEditQueueAction = DownloadQueue::eaFileSplit;

						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'E'");
							return;
						}
						m_szEditQueueText = strdup(argv[optind-1]);
					}
					else if (!strcasecmp(optarg, "O"))
					{
						if (!bGroup)
						{
							ReportError("Post-process parameter can be set only for groups");
							return;
						}
						m_iEditQueueAction = DownloadQueue::eaGroupSetParameter;

						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'E'");
							return;
						}
						m_szEditQueueText = strdup(argv[optind-1]);

						if (!strchr(m_szEditQueueText, '='))
						{
							ReportError("Could not parse value of option 'E'");
							return;
						}
					}
					else if (!strcasecmp(optarg, "I"))
					{
						if (!bGroup)
						{
							ReportError("Priority can be set only for groups");
							return;
						}
						m_iEditQueueAction = DownloadQueue::eaGroupSetPriority;

						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'E'");
							return;
						}
						m_szEditQueueText = strdup(argv[optind-1]);

						if (atoi(m_szEditQueueText) == 0 && strcmp("0", m_szEditQueueText))
						{
							ReportError("Could not parse value of option 'E'");
							return;
						}
					}
					else
					{
						m_iEditQueueOffset = atoi(optarg);
						if (m_iEditQueueOffset == 0)
						{
							ReportError("Could not parse value of option 'E'");
							return;
						}
						m_iEditQueueAction = bGroup ? DownloadQueue::eaGroupMoveOffset : DownloadQueue::eaFileMoveOffset;
					}
				}
				break;
			}
			case 'Q':
				m_eClientOperation = opClientRequestShutdown;
				break;
			case 'O':
				m_eClientOperation = opClientRequestReload;
				break;
			case 'V':
				m_eClientOperation = opClientRequestVersion;
				break;
			case 'W':
				m_eClientOperation = opClientRequestWriteLog;
				if (!strcasecmp(optarg, "I")) {
					m_iWriteLogKind = (int)Message::mkInfo;
				}
				else if (!strcasecmp(optarg, "W")) {
					m_iWriteLogKind = (int)Message::mkWarning;
				}
				else if (!strcasecmp(optarg, "E")) {
					m_iWriteLogKind = (int)Message::mkError;
				}
				else if (!strcasecmp(optarg, "D")) {
					m_iWriteLogKind = (int)Message::mkDetail;
				}
				else if (!strcasecmp(optarg, "G")) {
					m_iWriteLogKind = (int)Message::mkDebug;
				}
				else
				{
					ReportError("Could not parse value of option 'W'");
					return;
				}
				break;
			case 'K':
				// switch "K" is provided for compatibility with v. 0.8.0 and can be removed in future versions
				free(m_szAddCategory);
				m_szAddCategory = strdup(optarg);
				break;
			case 'S':
				optind++;
				optarg = optind > argc ? NULL : argv[optind-1];
				if (!optarg || !strncmp(optarg, "-", 1))
				{
					m_eClientOperation = opClientRequestScanAsync;
					optind--;
				}
				else if (!strcasecmp(optarg, "W"))
				{
					m_eClientOperation = opClientRequestScanSync;
				}
				else
				{
					ReportError("Could not parse value of option 'S'");
					return;
				}
				break;
			case '?':
				m_bErrors = true;
				return;
		}
	}

	for (int i = 0; i < argc; i++)
	{
		free(argv[i]);
	}
	free(argv);

	if (m_bServerMode && m_eClientOperation == opClientRequestDownloadPause)
	{
		m_bPauseDownload = true;
		m_eClientOperation = opClientNoOperation;
	}
}

void CommandLineParser::PrintUsage(const char* com)
{
	printf("Usage:\n"
		"  %s [switches]\n\n"
		"Switches:\n"
	    "  -h, --help                Print this help-message\n"
	    "  -v, --version             Print version and exit\n"
		"  -c, --configfile <file>   Filename of configuration-file\n"
		"  -n, --noconfigfile        Prevent loading of configuration-file\n"
		"                            (required options must be passed with --option)\n"
		"  -p, --printconfig         Print configuration and exit\n"
		"  -o, --option <name=value> Set or override option in configuration-file\n"
		"  -s, --server              Start nzbget as a server in console-mode\n"
#ifndef WIN32
		"  -D, --daemon              Start nzbget as a server in daemon-mode\n"
#endif
	    "  -V, --serverversion       Print server's version and exit\n"
		"  -Q, --quit                Shutdown server\n"
		"  -O, --reload              Reload config and restart all services\n"
		"  -A, --append [<options>] <nzb-file/url> Send file/url to server's\n"
		"                            download queue\n"
		"    <options> are (multiple options must be separated with space):\n"
		"       T                    Add file to the top (beginning) of queue\n"
		"       P                    Pause added files\n"
		"       C <name>             Assign category to nzb-file\n"
		"       N <name>             Use this name as nzb-filename\n"
		"       I <priority>         Set priority (signed integer)\n"
		"       DK <dupekey>         Set duplicate key (string)\n"
		"       DS <dupescore>       Set duplicate score (signed integer)\n"
		"       DM (score|all|force) Set duplicate mode\n"
		"  -C, --connect             Attach client to server\n"
		"  -L, --list    [F|FR|G|GR|O|H|S] [RegEx] Request list of items from server\n"
		"                 F          List individual files and server status (default)\n"
		"                 FR         Like \"F\" but apply regular expression filter\n"
		"                 G          List groups (nzb-files) and server status\n"
		"                 GR         Like \"G\" but apply regular expression filter\n"
		"                 O          List post-processor-queue\n"
		"                 H          List history\n"
		"                 HA         List history, all records (incl. hidden)\n"
		"                 S          Print only server status\n"
		"    <RegEx>                 Regular expression (only with options \"FR\", \"GR\")\n"
		"                            using POSIX Extended Regular Expression Syntax\n"
		"  -P, --pause   [D|O|S]  Pause server\n"
		"                 D          Pause download queue (default)\n"
		"                 O          Pause post-processor queue\n"
		"                 S          Pause scan of incoming nzb-directory\n"
		"  -U, --unpause [D|O|S]  Unpause server\n"
		"                 D          Unpause download queue (default)\n"
		"                 O          Unpause post-processor queue\n"
		"                 S          Unpause scan of incoming nzb-directory\n"
		"  -R, --rate <speed>        Set download rate on server, in KB/s\n"
		"  -G, --log <lines>         Request last <lines> lines from server's screen-log\n"
		"  -W, --write <D|I|W|E|G> \"Text\" Send text to server's log\n"
		"  -S, --scan    [W]         Scan incoming nzb-directory on server\n"
		"                 W          Wait until scan completes (synchronous mode)\n"
		"  -E, --edit [F|FN|FR|G|GN|GR|O|H] <action> <IDs/Names/RegExs> Edit items\n"
		"                            on server\n"
		"              F             Edit individual files (default)\n"
		"              FN            Like \"F\" but uses names (as \"group/file\")\n"
		"                            instead of IDs\n"
		"              FR            Like \"FN\" but with regular expressions\n"
		"              G             Edit all files in the group (same nzb-file)\n"
		"              GN            Like \"G\" but uses group names instead of IDs\n"
		"              GR            Like \"GN\" but with regular expressions\n"
		"              O             Edit post-processor-queue\n"
		"              H             Edit history\n"
		"    <action> is one of:\n"
		"    - for files (F) and groups (G):\n"
		"       <+offset|-offset>    Move in queue relative to current position,\n"
		"                            offset is an integer value\n"
		"       T                    Move to top of queue\n"
		"       B                    Move to bottom of queue\n"
		"       D                    Delete\n"
		"    - for files (F) and groups (G):\n"
		"       P                    Pause\n"
		"       U                    Resume (unpause)\n"
		"    - for groups (G):\n"
		"       A                    Pause all pars\n"
		"       R                    Pause extra pars\n"
		"       I <priority>         Set priority (signed integer)\n"
		"       C <name>             Set category\n"
		"       CP <name>            Set category and apply post-process parameters\n"
		"       N <name>             Rename\n"
		"       M                    Merge\n"
		"       S <name>             Split - create new group from selected files\n"
		"       O <name>=<value>     Set post-process parameter\n"
		"    - for post-jobs (O):\n"
		"       D                    Delete (cancel post-processing)\n"
		"    - for history (H):\n"
		"       D                    Delete\n"
		"       P                    Post-process again\n"
		"       R                    Download remaining files\n"
		"       A                    Download again\n"
		"       O <name>=<value>     Set post-process parameter\n"
		"       B                    Mark as bad\n"
		"       G                    Mark as good\n"
		"       S                    Mark as success\n"
		"    <IDs>                   Comma-separated list of file- or group- ids or\n"
		"                            ranges of file-ids, e. g.: 1-5,3,10-22\n"
		"    <Names>                 List of names (with options \"FN\" and \"GN\"),\n"
		"                            e. g.: \"my nzb download%cmyfile.nfo\" \"another nzb\"\n"
		"    <RegExs>                List of regular expressions (options \"FR\", \"GR\")\n"
		"                            using POSIX Extended Regular Expression Syntax\n",
		Util::BaseFileName(com),
		PATH_SEPARATOR);
}

void CommandLineParser::InitFileArg(int argc, const char* argv[])
{
	if (optind >= argc)
	{
		// no nzb-file passed
		if (!m_bServerMode && !m_bRemoteClientMode &&
		        (m_eClientOperation == opClientNoOperation ||
		         m_eClientOperation == opClientRequestDownload ||
				 m_eClientOperation == opClientRequestWriteLog))
		{
			if (m_eClientOperation == opClientRequestWriteLog)
			{
				ReportError("Log-text not specified");
			}
			else
			{
				ReportError("Nzb-file or Url not specified");
			}
		}
	}
	else if (m_eClientOperation == opClientRequestEditQueue)
	{
		if (m_EMatchMode == mmID)
		{
			ParseFileIDList(argc, argv, optind);
		}
		else
		{
			ParseFileNameList(argc, argv, optind);
		}
	}
	else
	{
		m_szLastArg = strdup(argv[optind]);

		// Check if the file-name is a relative path or an absolute path
		// If the path starts with '/' its an absolute, else relative
		const char* szFileName = argv[optind];

#ifdef WIN32
			m_szArgFilename = strdup(szFileName);
#else
		if (szFileName[0] == '/' || !strncasecmp(szFileName, "http://", 6) || !strncasecmp(szFileName, "https://", 7))
		{
			m_szArgFilename = strdup(szFileName);
		}
		else
		{
			// TEST
			char szFileNameWithPath[1024];
			getcwd(szFileNameWithPath, 1024);
			strcat(szFileNameWithPath, "/");
			strcat(szFileNameWithPath, szFileName);
			m_szArgFilename = strdup(szFileNameWithPath);
		}
#endif

		if (m_bServerMode || m_bRemoteClientMode ||
		        !(m_eClientOperation == opClientNoOperation ||
		          m_eClientOperation == opClientRequestDownload ||
				  m_eClientOperation == opClientRequestWriteLog))
		{
			ReportError("Too many arguments");
		}
	}
}

void CommandLineParser::ParseFileIDList(int argc, const char* argv[], int optind)
{
	std::vector<int> IDs;
	IDs.clear();

	while (optind < argc)
	{
		char* szWritableFileIDList = strdup(argv[optind++]);

		char* optarg = strtok(szWritableFileIDList, ", ");
		while (optarg)
		{
			int iEditQueueIDFrom = 0;
			int iEditQueueIDTo = 0;
			const char* p = strchr(optarg, '-');
			if (p)
			{
				char buf[101];
				int maxlen = (int)(p - optarg < 100 ? p - optarg : 100);
				strncpy(buf, optarg, maxlen);
				buf[maxlen] = '\0';
				iEditQueueIDFrom = atoi(buf);
				iEditQueueIDTo = atoi(p + 1);
				if (iEditQueueIDFrom <= 0 || iEditQueueIDTo <= 0)
				{
					ReportError("invalid list of file IDs");
					return;
				}
			}
			else
			{
				iEditQueueIDFrom = atoi(optarg);
				if (iEditQueueIDFrom <= 0)
				{
					ReportError("invalid list of file IDs");
					return;
				}
				iEditQueueIDTo = iEditQueueIDFrom;
			}

			int iEditQueueIDCount = 0;
			if (iEditQueueIDTo != 0)
			{
				if (iEditQueueIDFrom < iEditQueueIDTo)
				{
					iEditQueueIDCount = iEditQueueIDTo - iEditQueueIDFrom + 1;
				}
				else
				{
					iEditQueueIDCount = iEditQueueIDFrom - iEditQueueIDTo + 1;
				}
			}
			else
			{
				iEditQueueIDCount = 1;
			}

			for (int i = 0; i < iEditQueueIDCount; i++)
			{
				if (iEditQueueIDFrom < iEditQueueIDTo || iEditQueueIDTo == 0)
				{
					IDs.push_back(iEditQueueIDFrom + i);
				}
				else
				{
					IDs.push_back(iEditQueueIDFrom - i);
				}
			}

			optarg = strtok(NULL, ", ");
		}

		free(szWritableFileIDList);
	}

	m_iEditQueueIDCount = IDs.size();
	m_pEditQueueIDList = (int*)malloc(sizeof(int) * m_iEditQueueIDCount);
	for (int i = 0; i < m_iEditQueueIDCount; i++)
	{
		m_pEditQueueIDList[i] = IDs[i];
	}
}

void CommandLineParser::ParseFileNameList(int argc, const char* argv[], int optind)
{
	while (optind < argc)
	{
		m_EditQueueNameList.push_back(strdup(argv[optind++]));
	}
}

void CommandLineParser::ReportError(const char* szErrMessage)
{
	m_bErrors = true;
	printf("%s\n", szErrMessage);
}
