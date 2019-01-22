/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2016-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include "Thread.h"
#include "Connection.h"
#include "Log.h"
#include "Util.h"
#include "FileSystem.h"
#include "NServFrontend.h"
#include "NntpServer.h"
#include "NzbGenerator.h"
#include "Options.h"

struct NServOpts
{
	CString dataDir;
	CString cacheDir;
	CString bindAddress;
	int firstPort;
	int instances;
	CString logFile;
	CString secureCert;
	CString secureKey;
	BString<1024> logOpt;
	bool generateNzb;
	int segmentSize;
	bool quit;
	int latency;
	int speed;
	bool memCache;
	bool paramError;

	NServOpts(int argc, char* argv[], Options::CmdOptList& cmdOpts);
};

void NServPrintUsage(const char* com);

int NServMain(int argc, char* argv[])
{
	Log log;

	info("NServ %s (Test NNTP server)", Util::VersionRevision());

	Options::CmdOptList cmdOpts;
	NServOpts opts(argc, argv, cmdOpts);

	if (opts.dataDir.Empty() || opts.paramError)
	{
		NServPrintUsage(argv[0]);
		return 1;
	}

	if (!FileSystem::DirectoryExists(opts.dataDir))
	{
		// dataDir does not exist. Let's find out a bit more, and report:
		if (FileSystem::FileExists(opts.dataDir))
		{
			error("Specified data-dir %s is not a directory, but a file", *opts.dataDir );
		} else {
			error("Specified data-dir %s does not exist", *opts.dataDir );
		}
	}

	Options options(&cmdOpts, nullptr);

	log.InitOptions();
	Thread::Init();
	Connection::Init();
#ifndef DISABLE_TLS
	TlsSocket::Init();
#endif

#ifndef WIN32
	signal(SIGPIPE, SIG_IGN);
#endif

	NServFrontend frontend;
	frontend.Start();

	if (opts.generateNzb)
	{
		NzbGenerator gen(opts.dataDir, opts.segmentSize);
		gen.Execute();
		if (opts.quit)
		{
			return 0;
		}
	}

	CString errmsg;
	if (opts.cacheDir && !FileSystem::ForceDirectories(opts.cacheDir, errmsg))
	{
		error("Could not create directory %s: %s", *opts.cacheDir, *errmsg);
	}

	std::vector<std::unique_ptr<NntpServer>> instances;
	NntpCache cache;

	for (int i = 0; i < opts.instances; i++)
	{
		instances.emplace_back(std::make_unique<NntpServer>(i + 1, opts.bindAddress,
			opts.firstPort + i, opts.secureCert, opts.secureKey, opts.dataDir, opts.cacheDir,
			opts.latency, opts.speed, opts.memCache ? &cache : nullptr));
		instances.back()->Start();
	}

	info("Press Ctrl+C to quit");
	while (getchar()) Util::Sleep(200);

	for (std::unique_ptr<NntpServer>& serv: instances)
	{
		serv->Stop();
	}
	frontend.Stop();

	bool hasRunning = false;
	do
	{
		hasRunning = frontend.IsRunning();
		for (std::unique_ptr<NntpServer>& serv : instances)
		{
			hasRunning |= serv->IsRunning();
		}
		Util::Sleep(50);
	} while (hasRunning);

	return 0;
}

void NServPrintUsage(const char* com)
{
	printf("Usage:\n"
		" %s --nserv -d <data-dir> [optional switches] \n"
		"    -d <data-dir>   - directory whose files will be served\n"
		"  Optional switches:\n"
		"    -c <cache-dir>  - directory to store encoded articles\n"
		"    -m              - in-memory cache (unlimited, use with care)\n"
		"    -l <log-file>   - write into log-file (disabled by default)\n"
		"    -i <instances>  - number of server instances (default is 1)\n"
		"    -b <address>    - ip address to bind to (default is 0.0.0.0)\n"
		"    -p <port>       - port number for the first instance (default is 6791)\n"
		"    -s <cert> <key> - paths to SSL certificate and key files\n"
		"    -v <verbose>    - verbosity level 0..3 (default is 2)\n"
		"    -w <msec>       - response latency (in milliseconds)\n"
		"    -r <KB/s>       - speed throttling (in kilobytes per second)\n"
		"    -z <seg-size>   - generate nzbs for all files in data-dir (size in bytes)\n"
		"    -q              - quit after generating nzbs (in combination with -z)\n"
		, FileSystem::BaseFileName(com));
}

NServOpts::NServOpts(int argc, char* argv[], Options::CmdOptList& cmdOpts)
{
	instances = 1;
	bindAddress = "0.0.0.0";
	firstPort = 6791;
	generateNzb = false;
	segmentSize = 500000;
	quit = false;
	latency = 0;
	memCache = false;
	speed = 0;
	paramError = false;
	int verbosity = 2;

	char short_options[] = "b:c:d:l:p:i:ms:v:w:r:z:q";

	optind = 2;
	while (true)
	{
		int c = getopt(argc, argv, short_options);
		if (c == -1) break;
		switch (c)
		{
			case 'd':
				dataDir = optind > argc ? nullptr : argv[optind - 1];
				break;

			case 'c':
				cacheDir = optind > argc ? nullptr : argv[optind - 1];
				break;

			case 'm':
				memCache = true;
				break;

			case 'l':
				logFile = optind > argc ? nullptr : argv[optind - 1];
				break;

			case 'b':
				bindAddress= optind > argc ? "0.0.0.0" : argv[optind - 1];
				break;

			case 'p':
				firstPort = atoi(optind > argc ? "6791" : argv[optind - 1]);
				break;

			case 's':
				secureCert = optind > argc ? nullptr : argv[optind - 1];
				optind++;
				secureKey = optind > argc ? nullptr : argv[optind - 1];
				break;

			case 'i':
				instances = atoi(optind > argc ? "1" : argv[optind - 1]);
				break;

			case 'v':
				verbosity = atoi(optind > argc ? "1" : argv[optind - 1]);
				break;

			case 'w':
				latency = atoi(optind > argc ? "0" : argv[optind - 1]);
				break;

			case 'r':
				speed = atoi(optind > argc ? "0" : argv[optind - 1]);
				break;

			case 'z':
				generateNzb = true;
				segmentSize = atoi(optind > argc ? "500000" : argv[optind - 1]);
				break;

			case 'q':
				quit = true;
				break;
		}
	}

	if (optind < argc)
	{
		paramError = true;
	}

	if (logFile.Empty())
	{
		cmdOpts.push_back("WriteLog=none");
	}
	else
	{
		cmdOpts.push_back("WriteLog=append");
		logOpt.Format("LogFile=%s", *logFile);
		cmdOpts.push_back(logOpt);
	}

	if (verbosity < 1)
	{
		cmdOpts.push_back("InfoTarget=none");
		cmdOpts.push_back("WarningTarget=none");
		cmdOpts.push_back("ErrorTarget=none");
	}
	if (verbosity < 2)
	{
		cmdOpts.push_back("DetailTarget=none");
	}
	if (verbosity > 2)
	{
		cmdOpts.push_back("DebugTarget=both");
	}
}
