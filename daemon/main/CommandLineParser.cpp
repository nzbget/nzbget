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


#include "nzbget.h"
#include "CommandLineParser.h"
#include "Log.h"
#include "MessageBase.h"
#include "DownloadInfo.h"
#include "FileSystem.h"
#include "Util.h"

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
	InitCommandLine(argc, argv);

	if (argc == 1)
	{
		m_printUsage = true;
		return;
	}

	if (!m_printOptions && !m_printUsage && !m_printVersion)
	{
		InitFileArg(argc, argv);
	}
}

void CommandLineParser::InitCommandLine(int argc, const char* const_argv[])
{
	m_clientOperation = opClientNoOperation; // default

	std::vector<CString> argv;
	argv.reserve(argc);
	for (int i = 0; i < argc; i++)
	{
		argv.emplace_back(const_argv[i]);
	}

	// reset getopt
	optind = 0;

	while (true)
	{
		int c;

#ifdef HAVE_GETOPT_LONG
		int option_index  = 0;
		c = getopt_long(argc, (char**)argv.data(), short_options, long_options, &option_index);
#else
		c = getopt(argc, (char**)argv.data(), short_options);
#endif

		if (c == -1) break;

		switch (c)
		{
			case 'c':
				m_configFilename = optarg;
				break;
			case 'n':
				m_configFilename = nullptr;
				m_noConfig = true;
				break;
			case 'h':
				m_printUsage = true;
				return;
			case 'v':
				m_printVersion = true;
				return;
			case 'p':
				m_printOptions = true;
				break;
			case 'o':
				m_optionList.push_back(optarg);
				break;
			case 's':
				m_serverMode = true;
				break;
			case 'D':
				m_serverMode = true;
				m_daemonMode = true;
				break;
			case 'A':
				m_clientOperation = opClientRequestDownload;

				while (true)
				{
					optind++;
					optarg = optind > argc ? nullptr : (char*)argv[optind-1];
					if (optarg && (!strcasecmp(optarg, "F") || !strcasecmp(optarg, "U")))
					{
						// option ignored (but kept for compatibility)
					}
					else if (optarg && !strcasecmp(optarg, "T"))
					{
						m_addTop = true;
					}
					else if (optarg && !strcasecmp(optarg, "P"))
					{
						m_addPaused = true;
					}
					else if (optarg && !strcasecmp(optarg, "I"))
					{
						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'A'");
							return;
						}
						m_addPriority = atoi(argv[optind-1]);
					}
					else if (optarg && !strcasecmp(optarg, "C"))
					{
						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'A'");
							return;
						}
						m_addCategory = std::move(argv[optind-1]);
					}
					else if (optarg && !strcasecmp(optarg, "N"))
					{
						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'A'");
							return;
						}
						m_addNzbFilename = std::move(argv[optind-1]);
					}
					else if (optarg && !strcasecmp(optarg, "DK"))
					{
						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'A'");
							return;
						}
						m_addDupeKey = std::move(argv[optind-1]);
					}
					else if (optarg && !strcasecmp(optarg, "DS"))
					{
						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'A'");
							return;
						}
						m_addDupeScore = atoi(argv[optind-1]);
					}
					else if (optarg && !strcasecmp(optarg, "DM"))
					{
						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'A'");
							return;
						}

						const char* dupeMode = argv[optind-1];
						if (!strcasecmp(dupeMode, "score"))
						{
							m_addDupeMode = dmScore;
						}
						else if (!strcasecmp(dupeMode, "all"))
						{
							m_addDupeMode = dmAll;
						}
						else if (!strcasecmp(dupeMode, "force"))
						{
							m_addDupeMode = dmForce;
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
				optarg = optind > argc ? nullptr : (char*)argv[optind-1];
				if (!optarg || !strncmp(optarg, "-", 1))
				{
					m_clientOperation = opClientRequestListFiles;
					optind--;
				}
				else if (!strcasecmp(optarg, "F") || !strcasecmp(optarg, "FR"))
				{
					m_clientOperation = opClientRequestListFiles;
				}
				else if (!strcasecmp(optarg, "G") || !strcasecmp(optarg, "GR"))
				{
					m_clientOperation = opClientRequestListGroups;
				}
				else if (!strcasecmp(optarg, "O"))
				{
					m_clientOperation = opClientRequestPostQueue;
				}
				else if (!strcasecmp(optarg, "S"))
				{
					m_clientOperation = opClientRequestListStatus;
				}
				else if (!strcasecmp(optarg, "H"))
				{
					m_clientOperation = opClientRequestHistory;
				}
				else if (!strcasecmp(optarg, "HA"))
				{
					m_clientOperation = opClientRequestHistoryAll;
				}
				else
				{
					ReportError("Could not parse value of option 'L'");
					return;
				}

				if (optarg && (!strcasecmp(optarg, "FR") || !strcasecmp(optarg, "GR")))
				{
					m_matchMode = mmRegEx;

					optind++;
					if (optind > argc)
					{
						ReportError("Could not parse value of option 'L'");
						return;
					}
					m_editQueueText = std::move(argv[optind-1]);
				}
				break;
			case 'P':
			case 'U':
				optind++;
				optarg = optind > argc ? nullptr : (char*)argv[optind-1];
				if (!optarg || !strncmp(optarg, "-", 1))
				{
					m_clientOperation = c == 'P' ? opClientRequestDownloadPause : opClientRequestDownloadUnpause;
					optind--;
				}
				else if (!strcasecmp(optarg, "D"))
				{
					m_clientOperation = c == 'P' ? opClientRequestDownloadPause : opClientRequestDownloadUnpause;
				}
				else if (!strcasecmp(optarg, "O"))
				{
					m_clientOperation = c == 'P' ? opClientRequestPostPause : opClientRequestPostUnpause;
				}
				else if (!strcasecmp(optarg, "S"))
				{
					m_clientOperation = c == 'P' ? opClientRequestScanPause : opClientRequestScanUnpause;
				}
				else
				{
					ReportError(c == 'P' ? "Could not parse value of option 'P'\n" : "Could not parse value of option 'U'");
					return;
				}
				break;
			case 'R':
				m_clientOperation = opClientRequestSetRate;
				m_setRate = (int)(atof(optarg)*1024);
				break;
			case 'B':
				if (!strcasecmp(optarg, "dump"))
				{
					m_clientOperation = opClientRequestDumpDebug;
				}
				else if (!strcasecmp(optarg, "trace"))
				{
					m_testBacktrace = true;
				}
				else if (!strcasecmp(optarg, "webget"))
				{
					m_webGet = true;
					optind++;
					if (optind > argc)
					{
						ReportError("Could not parse value of option 'B'");
						return;
					}
					optarg = argv[optind-1];
					m_webGetFilename = optarg;
				}
				else if (!strcasecmp(optarg, "verify"))
				{
					m_sigVerify = true;
					optind++;
					if (optind > argc)
					{
						ReportError("Could not parse value of option 'B'");
						return;
					}
					optarg = argv[optind-1];
					m_pubKeyFilename = optarg;

					optind++;
					if (optind > argc)
					{
						ReportError("Could not parse value of option 'B'");
						return;
					}
					optarg = argv[optind-1];
					m_sigFilename = optarg;
				}
				else
				{
					ReportError("Could not parse value of option 'B'");
					return;
				}
				break;
			case 'G':
				m_clientOperation = opClientRequestLog;
				m_logLines = atoi(optarg);
				if (m_logLines == 0)
				{
					ReportError("Could not parse value of option 'G'");
					return;
				}
				break;
			case 'T':
				m_addTop = true;
				break;
			case 'C':
				m_remoteClientMode = true;
				break;
			case 'E':
			{
				m_clientOperation = opClientRequestEditQueue;
				bool group = !strcasecmp(optarg, "G") || !strcasecmp(optarg, "GN") || !strcasecmp(optarg, "GR");
				bool file = !strcasecmp(optarg, "F") || !strcasecmp(optarg, "FN") || !strcasecmp(optarg, "FR");
				if (!strcasecmp(optarg, "GN") || !strcasecmp(optarg, "FN"))
				{
					m_matchMode = mmName;
				}
				else if (!strcasecmp(optarg, "GR") || !strcasecmp(optarg, "FR"))
				{
					m_matchMode = mmRegEx;
				}
				else
				{
					m_matchMode = mmId;
				};
				bool post = !strcasecmp(optarg, "O");
				bool history = !strcasecmp(optarg, "H");
				if (group || file || post || history)
				{
					optind++;
					if (optind > argc)
					{
						ReportError("Could not parse value of option 'E'");
						return;
					}
					optarg = argv[optind-1];
				}

				if (post)
				{
					// edit-commands for post-processor-queue
					if (!strcasecmp(optarg, "D"))
					{
						m_editQueueAction = DownloadQueue::eaPostDelete;
					}
					else
					{
						ReportError("Could not parse value of option 'E'");
						return;
					}
				}
				else if (history)
				{
					// edit-commands for history
					if (!strcasecmp(optarg, "D"))
					{
						m_editQueueAction = DownloadQueue::eaHistoryDelete;
					}
					else if (!strcasecmp(optarg, "R"))
					{
						m_editQueueAction = DownloadQueue::eaHistoryReturn;
					}
					else if (!strcasecmp(optarg, "P"))
					{
						m_editQueueAction = DownloadQueue::eaHistoryProcess;
					}
					else if (!strcasecmp(optarg, "A"))
					{
						m_editQueueAction = DownloadQueue::eaHistoryRedownload;
					}
					else if (!strcasecmp(optarg, "F"))
					{
						m_editQueueAction = DownloadQueue::eaHistoryRetryFailed;
					}
					else if (!strcasecmp(optarg, "O"))
					{
						m_editQueueAction = DownloadQueue::eaHistorySetParameter;

						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'E'");
							return;
						}
						m_editQueueText = std::move(argv[optind-1]);

						if (!strchr(m_editQueueText, '='))
						{
							ReportError("Could not parse value of option 'E'");
							return;
						}
					}
					else if (!strcasecmp(optarg, "B"))
					{
						m_editQueueAction = DownloadQueue::eaHistoryMarkBad;
					}
					else if (!strcasecmp(optarg, "G"))
					{
						m_editQueueAction = DownloadQueue::eaHistoryMarkGood;
					}
					else if (!strcasecmp(optarg, "S"))
					{
						m_editQueueAction = DownloadQueue::eaHistoryMarkSuccess;
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
						m_editQueueAction = group ? DownloadQueue::eaGroupMoveTop : DownloadQueue::eaFileMoveTop;
					}
					else if (!strcasecmp(optarg, "B"))
					{
						m_editQueueAction = group ? DownloadQueue::eaGroupMoveBottom : DownloadQueue::eaFileMoveBottom;
					}
					else if (!strcasecmp(optarg, "P"))
					{
						m_editQueueAction = group ? DownloadQueue::eaGroupPause : DownloadQueue::eaFilePause;
					}
					else if (!strcasecmp(optarg, "A"))
					{
						m_editQueueAction = group ? DownloadQueue::eaGroupPauseAllPars : DownloadQueue::eaFilePauseAllPars;
					}
					else if (!strcasecmp(optarg, "R"))
					{
						m_editQueueAction = group ? DownloadQueue::eaGroupPauseExtraPars : DownloadQueue::eaFilePauseExtraPars;
					}
					else if (!strcasecmp(optarg, "U"))
					{
						m_editQueueAction = group ? DownloadQueue::eaGroupResume : DownloadQueue::eaFileResume;
					}
					else if (!strcasecmp(optarg, "D"))
					{
						m_editQueueAction = group ? DownloadQueue::eaGroupDelete : DownloadQueue::eaFileDelete;
					}
					else if (!strcasecmp(optarg, "DP"))
					{
						m_editQueueAction = DownloadQueue::eaGroupParkDelete;
					}
					else if (!strcasecmp(optarg, "SF"))
					{
						m_editQueueAction = DownloadQueue::eaGroupSortFiles;
					}
					else if (!strcasecmp(optarg, "C") || !strcasecmp(optarg, "K") || !strcasecmp(optarg, "CP"))
					{
						// switch "K" is provided for compatibility with v. 0.8.0 and can be removed in future versions
						if (!group)
						{
							ReportError("Category can be set only for groups");
							return;
						}
						m_editQueueAction = !strcasecmp(optarg, "CP") ? DownloadQueue::eaGroupApplyCategory : DownloadQueue::eaGroupSetCategory;

						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'E'");
							return;
						}
						m_editQueueText = std::move(argv[optind-1]);
					}
					else if (!strcasecmp(optarg, "N"))
					{
						if (!group)
						{
							ReportError("Only groups can be renamed");
							return;
						}
						m_editQueueAction = DownloadQueue::eaGroupSetName;

						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'E'");
							return;
						}
						m_editQueueText = std::move(argv[optind-1]);
					}
					else if (!strcasecmp(optarg, "M"))
					{
						if (!group)
						{
							ReportError("Only groups can be merged");
							return;
						}
						m_editQueueAction = DownloadQueue::eaGroupMerge;
					}
					else if (!strcasecmp(optarg, "S"))
					{
						m_editQueueAction = DownloadQueue::eaFileSplit;

						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'E'");
							return;
						}
						m_editQueueText = std::move(argv[optind-1]);
					}
					else if (!strcasecmp(optarg, "O"))
					{
						if (!group)
						{
							ReportError("Post-process parameter can be set only for groups");
							return;
						}
						m_editQueueAction = DownloadQueue::eaGroupSetParameter;

						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'E'");
							return;
						}
						m_editQueueText = std::move(argv[optind-1]);

						if (!strchr(m_editQueueText, '='))
						{
							ReportError("Could not parse value of option 'E'");
							return;
						}
					}
					else if (!strcasecmp(optarg, "I"))
					{
						if (!group)
						{
							ReportError("Priority can be set only for groups");
							return;
						}
						m_editQueueAction = DownloadQueue::eaGroupSetPriority;

						optind++;
						if (optind > argc)
						{
							ReportError("Could not parse value of option 'E'");
							return;
						}
						m_editQueueText = std::move(argv[optind-1]);

						if (atoi(m_editQueueText) == 0 && strcmp("0", m_editQueueText))
						{
							ReportError("Could not parse value of option 'E'");
							return;
						}
					}
					else
					{
						m_editQueueOffset = atoi(optarg);
						if (m_editQueueOffset == 0)
						{
							ReportError("Could not parse value of option 'E'");
							return;
						}
						m_editQueueAction = group ? DownloadQueue::eaGroupMoveOffset : DownloadQueue::eaFileMoveOffset;
					}
				}
				break;
			}
			case 'Q':
				m_clientOperation = opClientRequestShutdown;
				break;
			case 'O':
				m_clientOperation = opClientRequestReload;
				break;
			case 'V':
				m_clientOperation = opClientRequestVersion;
				break;
			case 'W':
				m_clientOperation = opClientRequestWriteLog;
				if (!strcasecmp(optarg, "I")) {
					m_writeLogKind = (int)Message::mkInfo;
				}
				else if (!strcasecmp(optarg, "W")) {
					m_writeLogKind = (int)Message::mkWarning;
				}
				else if (!strcasecmp(optarg, "E")) {
					m_writeLogKind = (int)Message::mkError;
				}
				else if (!strcasecmp(optarg, "D")) {
					m_writeLogKind = (int)Message::mkDetail;
				}
				else if (!strcasecmp(optarg, "G")) {
					m_writeLogKind = (int)Message::mkDebug;
				}
				else
				{
					ReportError("Could not parse value of option 'W'");
					return;
				}
				break;
			case 'K':
				// switch "K" is provided for compatibility with v. 0.8.0 and can be removed in future versions
				m_addCategory = optarg;
				break;
			case 'S':
				optind++;
				optarg = optind > argc ? nullptr : (char*)argv[optind-1];
				if (!optarg || !strncmp(optarg, "-", 1))
				{
					m_clientOperation = opClientRequestScanAsync;
					optind--;
				}
				else if (!strcasecmp(optarg, "W"))
				{
					m_clientOperation = opClientRequestScanSync;
				}
				else
				{
					ReportError("Could not parse value of option 'S'");
					return;
				}
				break;
			case '?':
				m_errors = true;
				return;
		}
	}

	if (m_serverMode && m_clientOperation == opClientRequestDownloadPause)
	{
		m_pauseDownload = true;
		m_clientOperation = opClientNoOperation;
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
		"  -P, --pause   [D|O|S]     Pause server\n"
		"                 D          Pause download queue (default)\n"
		"                 O          Pause post-processor queue\n"
		"                 S          Pause scan of incoming nzb-directory\n"
		"  -U, --unpause [D|O|S]     Unpause server\n"
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
		"       P                    Pause\n"
		"       U                    Resume (unpause)\n"
		"    - for groups (G):\n"
		"       A                    Pause all pars\n"
		"       R                    Pause extra pars\n"
		"       DP                   Delete but keep downloaded files\n"
		"       I <priority>         Set priority (signed integer)\n"
		"       C <name>             Set category\n"
		"       CP <name>            Set category and apply post-process parameters\n"
		"       N <name>             Rename\n"
		"       M                    Merge\n"
		"       SF                   Sort inner files for optimal order\n"
		"       S <name>             Split - create new group from selected files\n"
		"       O <name>=<value>     Set post-process parameter\n"
		"    - for post-jobs (O):\n"
		"       D                    Delete (cancel post-processing)\n"
		"    - for history (H):\n"
		"       D                    Delete\n"
		"       P                    Post-process again\n"
		"       R                    Download remaining files\n"
		"       A                    Download again\n"
		"       F                    Retry download of failed articles\n"
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
		FileSystem::BaseFileName(com),
		PATH_SEPARATOR);
}

void CommandLineParser::InitFileArg(int argc, const char* argv[])
{
	if (optind >= argc)
	{
		// no nzb-file passed
		if (!m_serverMode && !m_remoteClientMode &&
				(m_clientOperation == opClientNoOperation ||
				 m_clientOperation == opClientRequestDownload ||
				 m_clientOperation == opClientRequestWriteLog))
		{
			if (m_clientOperation == opClientRequestWriteLog)
			{
				ReportError("Log-text not specified");
			}
			else
			{
				ReportError("Nzb-file or Url not specified");
			}
		}
	}
	else if (m_clientOperation == opClientRequestEditQueue)
	{
		if (m_matchMode == mmId)
		{
			ParseFileIdList(argc, argv, optind);
		}
		else
		{
			ParseFileNameList(argc, argv, optind);
		}
	}
	else
	{
		m_lastArg = argv[optind];

		// Check if the file-name is a relative path or an absolute path
		// If the path starts with '/' its an absolute, else relative
		const char* fileName = argv[optind];

#ifdef WIN32
		m_argFilename = fileName;
#else
		if ( strncasecmp(fileName, "http://", 7) == 0 || strncasecmp(fileName, "https://", 8) == 0 )
		{
			m_argFilename = fileName;
		}
		else
		{
            char * absPath = realpath( fileName, NULL );
			if ( absPath != NULL )
            {
                m_argFilename = absPath;
                free( absPath );
            }
		}
#endif

		if (m_serverMode || m_remoteClientMode ||
				!(m_clientOperation == opClientNoOperation ||
				  m_clientOperation == opClientRequestDownload ||
				  m_clientOperation == opClientRequestWriteLog))
		{
			ReportError("Too many arguments");
		}
	}
}

void CommandLineParser::ParseFileIdList(int argc, const char* argv[], int optind)
{
	m_editQueueIdList.clear();

	while (optind < argc)
	{
		CString writableFileIdList = argv[optind++];

		char* optarg = strtok(writableFileIdList, ", ");
		while (optarg)
		{
			int editQueueIdFrom = 0;
			int editQueueIdTo = 0;
			const char* p = strchr(optarg, '-');
			if (p)
			{
				BString<100> buf;
				buf.Set(optarg, (int)(p - optarg));
				editQueueIdFrom = atoi(buf);
				editQueueIdTo = atoi(p + 1);
				if (editQueueIdFrom <= 0 || editQueueIdTo <= 0)
				{
					ReportError("invalid list of file IDs");
					return;
				}
			}
			else
			{
				editQueueIdFrom = atoi(optarg);
				if (editQueueIdFrom <= 0)
				{
					ReportError("invalid list of file IDs");
					return;
				}
				editQueueIdTo = editQueueIdFrom;
			}

			int editQueueIdCount = 0;
			if (editQueueIdFrom < editQueueIdTo)
			{
				editQueueIdCount = editQueueIdTo - editQueueIdFrom + 1;
			}
			else
			{
				editQueueIdCount = editQueueIdFrom - editQueueIdTo + 1;
			}

			for (int i = 0; i < editQueueIdCount; i++)
			{
				if (editQueueIdFrom < editQueueIdTo)
				{
					m_editQueueIdList.push_back(editQueueIdFrom + i);
				}
				else
				{
					m_editQueueIdList.push_back(editQueueIdFrom - i);
				}
			}

			optarg = strtok(nullptr, ", ");
		}
	}
}

void CommandLineParser::ParseFileNameList(int argc, const char* argv[], int optind)
{
	while (optind < argc)
	{
		m_editQueueNameList.push_back(argv[optind++]);
	}
}

void CommandLineParser::ReportError(const char* errMessage)
{
	m_errors = true;
	printf("%s\n", errMessage);
}
