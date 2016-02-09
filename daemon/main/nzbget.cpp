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


#include "nzbget.h"
#include "ServerPool.h"
#include "Log.h"
#include "NzbFile.h"
#include "Options.h"
#include "CommandLineParser.h"
#include "ScriptConfig.h"
#include "Thread.h"
#include "ColoredFrontend.h"
#include "NCursesFrontend.h"
#include "QueueCoordinator.h"
#include "UrlCoordinator.h"
#include "RemoteServer.h"
#include "WebServer.h"
#include "RemoteClient.h"
#include "MessageBase.h"
#include "DiskState.h"
#include "PrePostProcessor.h"
#include "HistoryCoordinator.h"
#include "DupeCoordinator.h"
#include "ParChecker.h"
#include "Scheduler.h"
#include "Scanner.h"
#include "FeedCoordinator.h"
#include "Service.h"
#include "DiskService.h"
#include "Maintenance.h"
#include "ArticleWriter.h"
#include "StatMeter.h"
#include "QueueScript.h"
#include "Util.h"
#include "FileSystem.h"
#include "StackTrace.h"
#ifdef WIN32
#include "WinService.h"
#include "WinConsole.h"
#include "WebDownloader.h"
#endif
#ifdef ENABLE_TESTS
#include "TestMain.h"
#endif

// Prototypes
void RunMain();
void Run(bool reload);
void Reload();
void Cleanup();
void ProcessClientRequest();
void ProcessWebGet();
void ProcessSigVerify();
#ifndef WIN32
void Daemonize();
#endif
void BootConfig();

Thread* g_Frontend = nullptr;
CommandLineParser* g_CommandLineParser = nullptr;
ServerPool* g_ServerPool = nullptr;
QueueCoordinator* g_QueueCoordinator = nullptr;
UrlCoordinator* g_UrlCoordinator = nullptr;
RemoteServer* g_RemoteServer = nullptr;
RemoteServer* g_RemoteSecureServer = nullptr;
StatMeter* g_StatMeter = nullptr;
PrePostProcessor* g_PrePostProcessor = nullptr;
HistoryCoordinator* g_HistoryCoordinator = nullptr;
DupeCoordinator* g_DupeCoordinator = nullptr;
DiskState* g_DiskState = nullptr;
Scheduler* g_Scheduler = nullptr;
Scanner* g_Scanner = nullptr;
FeedCoordinator* g_FeedCoordinator = nullptr;
Maintenance* g_Maintenance = nullptr;
ArticleCache* g_ArticleCache = nullptr;
QueueScriptCoordinator* g_QueueScriptCoordinator = nullptr;
ServiceCoordinator* g_ServiceCoordinator = nullptr;
DiskService* g_DiskService = nullptr;
int g_ArgumentCount;
char* (*g_EnvironmentVariables)[] = nullptr;
char* (*g_Arguments)[] = nullptr;
bool g_Reloading = true;
#ifdef WIN32
WinConsole* g_WinConsole = nullptr;
#endif

/*
 * Main loop
 */
int main(int argc, char *argv[], char *argp[])
{
#ifdef WIN32
#ifdef _DEBUG
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF
#ifdef DEBUG_CRTMEMLEAKS
		| _CRTDBG_CHECK_CRT_DF | _CRTDBG_CHECK_ALWAYS_DF
#endif
		);
#endif
#endif

	Util::Init();

	g_ArgumentCount = argc;
	g_Arguments = (char*(*)[])argv;
	g_EnvironmentVariables = (char*(*)[])argp;

	if (argc > 1 && (!strcmp(argv[1], "-tests") || !strcmp(argv[1], "--tests")))
	{
#ifdef ENABLE_TESTS
		return TestMain(argc, argv);
#else
		printf("ERROR: Could not start tests, the program was compiled without tests\n");
		return 1;
#endif
	}

#ifdef ENABLE_TESTS
	TestCleanup();
#endif

#ifdef WIN32
	InstallUninstallServiceCheck(argc, argv);
#endif

	srand(Util::CurrentTime());

#ifdef WIN32
	for (int i=0; i < argc; i++)
	{
		if (!strcmp(argv[i], "-D"))
		{
			StartService(RunMain);
			return 0;
		}
	}
#endif

	RunMain();

	return 0;
}

void RunMain()
{
	bool reload = false;
	while (g_Reloading)
	{
		g_Reloading = false;
		Run(reload);
		reload = true;
	}
}

void Run(bool reload)
{
	Log::Init();

	debug("nzbget %s", Util::VersionRevision());

	if (!reload)
	{
		Thread::Init();
	}

#ifdef WIN32
	g_WinConsole = new WinConsole();
	g_WinConsole->InitAppMode();
#endif

	g_ServiceCoordinator = new ServiceCoordinator();
	g_ServerPool = new ServerPool();
	g_Scheduler = new Scheduler();
	g_QueueCoordinator = new QueueCoordinator();
	g_StatMeter = new StatMeter();
	g_Scanner = new Scanner();
	g_PrePostProcessor = new PrePostProcessor();
	g_HistoryCoordinator = new HistoryCoordinator();
	g_DupeCoordinator = new DupeCoordinator();
	g_UrlCoordinator = new UrlCoordinator();
	g_FeedCoordinator = new FeedCoordinator();
	g_ArticleCache = new ArticleCache();
	g_Maintenance = new Maintenance();
	g_QueueScriptCoordinator = new QueueScriptCoordinator();
	g_DiskService = new DiskService();

	BootConfig();

#ifndef WIN32
	if (g_Options->GetUMask() < 01000)
	{
		/* set newly created file permissions */
		umask(g_Options->GetUMask());
	}
#endif

	g_Scanner->InitOptions();
	g_QueueScriptCoordinator->InitOptions();

	if (g_CommandLineParser->GetDaemonMode())
	{
#ifdef WIN32
		info("nzbget %s service-mode", Util::VersionRevision());
#else
		if (!reload)
		{
			Daemonize();
		}
		info("nzbget %s daemon-mode", Util::VersionRevision());
#endif
	}
	else if (g_Options->GetServerMode())
	{
		info("nzbget %s server-mode", Util::VersionRevision());
	}
	else if (g_CommandLineParser->GetRemoteClientMode())
	{
		info("nzbget %s remote-mode", Util::VersionRevision());
	}

	if (!reload)
	{
		Connection::Init();
	}

	if (!g_CommandLineParser->GetRemoteClientMode())
	{
		g_ServerPool->InitConnections();
		g_StatMeter->Init();
	}

	InstallErrorHandler();

#ifdef DEBUG
	if (g_CommandLineParser->GetTestBacktrace())
	{
		TestSegFault();
	}
#endif

	if (g_CommandLineParser->GetWebGet())
	{
		ProcessWebGet();
		return;
	}

	if (g_CommandLineParser->GetSigVerify())
	{
		ProcessSigVerify();
		return;
	}

	// client request
	if (g_CommandLineParser->GetClientOperation() != CommandLineParser::opClientNoOperation)
	{
		ProcessClientRequest();
		Cleanup();
		return;
	}

	// Setup the network-server
	if (g_Options->GetServerMode())
	{
		WebProcessor::Init();
		g_RemoteServer = new RemoteServer(false);
		g_RemoteServer->Start();

		if (g_Options->GetSecureControl())
		{
			g_RemoteSecureServer = new RemoteServer(true);
			g_RemoteSecureServer->Start();
		}
	}

	// Create the frontend
	if (!g_CommandLineParser->GetDaemonMode())
	{
		switch (g_Options->GetOutputMode())
		{
			case Options::omNCurses:
#ifndef DISABLE_CURSES
				g_Frontend = new NCursesFrontend();
				break;
#endif
			case Options::omColored:
				g_Frontend = new ColoredFrontend();
				break;
			case Options::omLoggable:
				g_Frontend = new LoggableFrontend();
				break;
		}
	}

	// Starting a thread with the frontend
	if (g_Frontend)
	{
		g_Frontend->Start();
	}

	// Starting QueueCoordinator and PrePostProcessor
	if (!g_CommandLineParser->GetRemoteClientMode())
	{
		// Standalone-mode
		if (!g_CommandLineParser->GetServerMode())
		{
			const char* category = g_CommandLineParser->GetAddCategory() ? g_CommandLineParser->GetAddCategory() : "";
			NzbFile nzbFile(g_CommandLineParser->GetArgFilename(), category);
			if (!nzbFile.Parse())
			{
				printf("Parsing NZB-document %s failed\n\n", g_CommandLineParser->GetArgFilename() ? g_CommandLineParser->GetArgFilename() : "N/A");
				return;
			}
			g_Scanner->InitPPParameters(category, nzbFile.GetNzbInfo()->GetParameters(), false);
			g_QueueCoordinator->AddNzbFileToQueue(&nzbFile, nullptr, false);
		}

		if (g_Options->GetSaveQueue() && g_Options->GetServerMode())
		{
			g_DiskState = new DiskState();
		}

#ifdef WIN32
		g_WinConsole->Start();
#endif
		g_QueueCoordinator->Start();
		g_UrlCoordinator->Start();
		g_PrePostProcessor->Start();
		g_FeedCoordinator->Start();
		g_ServiceCoordinator->Start();
		if (g_Options->GetArticleCache() > 0)
		{
			g_ArticleCache->Start();
		}

		// enter main program-loop
		while (g_QueueCoordinator->IsRunning() ||
			g_UrlCoordinator->IsRunning() ||
			g_PrePostProcessor->IsRunning() ||
			g_FeedCoordinator->IsRunning() ||
			g_ServiceCoordinator->IsRunning() ||
#ifdef WIN32
			g_WinConsole->IsRunning() ||
#endif
			g_ArticleCache->IsRunning())
		{
			if (!g_Options->GetServerMode() &&
				!g_QueueCoordinator->HasMoreJobs() &&
				!g_UrlCoordinator->HasMoreJobs() &&
				!g_PrePostProcessor->HasMoreJobs())
			{
				// Standalone-mode: download completed
				if (!g_QueueCoordinator->IsStopped())
				{
					g_QueueCoordinator->Stop();
				}
				if (!g_UrlCoordinator->IsStopped())
				{
					g_UrlCoordinator->Stop();
				}
				if (!g_PrePostProcessor->IsStopped())
				{
					g_PrePostProcessor->Stop();
				}
				if (!g_FeedCoordinator->IsStopped())
				{
					g_FeedCoordinator->Stop();
				}
				if (!g_ArticleCache->IsStopped())
				{
					g_ArticleCache->Stop();
				}
				if (!g_ServiceCoordinator->IsStopped())
				{
					g_ServiceCoordinator->Stop();
				}
			}
			usleep(100 * 1000);
		}

		// main program-loop is terminated
		debug("QueueCoordinator stopped");
		debug("UrlCoordinator stopped");
		debug("PrePostProcessor stopped");
		debug("FeedCoordinator stopped");
		debug("ServiceCoordinator stopped");
		debug("ArticleCache stopped");
	}

	ScriptController::TerminateAll();

	// Stop network-server
	if (g_RemoteServer)
	{
		debug("stopping RemoteServer");
		g_RemoteServer->Stop();
		int maxWaitMSec = 1000;
		while (g_RemoteServer->IsRunning() && maxWaitMSec > 0)
		{
			usleep(100 * 1000);
			maxWaitMSec -= 100;
		}
		if (g_RemoteServer->IsRunning())
		{
			debug("Killing RemoteServer");
			g_RemoteServer->Kill();
		}
		debug("RemoteServer stopped");
	}

	if (g_RemoteSecureServer)
	{
		debug("stopping RemoteSecureServer");
		g_RemoteSecureServer->Stop();
		int maxWaitMSec = 1000;
		while (g_RemoteSecureServer->IsRunning() && maxWaitMSec > 0)
		{
			usleep(100 * 1000);
			maxWaitMSec -= 100;
		}
		if (g_RemoteSecureServer->IsRunning())
		{
			debug("Killing RemoteSecureServer");
			g_RemoteSecureServer->Kill();
		}
		debug("RemoteSecureServer stopped");
	}

	// Stop Frontend
	if (g_Frontend)
	{
		if (!g_CommandLineParser->GetRemoteClientMode())
		{
			debug("Stopping Frontend");
			g_Frontend->Stop();
		}
		while (g_Frontend->IsRunning())
		{
			usleep(50 * 1000);
		}
		debug("Frontend stopped");
	}

	Cleanup();
}

class OptionsExtender : public Options::Extender
{
protected:
#ifdef WIN32
	virtual void		SetupFirstStart()
	{
		g_WinConsole->SetupFirstStart();
	}
#endif

	virtual void		AddNewsServer(int id, bool active, const char* name, const char* host,
							int port, const char* user, const char* pass, bool joinGroup,
							bool tls, const char* cipher, int maxConnections, int retention,
							int level, int group)
	{
		g_ServerPool->AddServer(new NewsServer(id, active, name, host, port, user, pass, joinGroup,
							tls, cipher, maxConnections, retention, level, group));
	}

	virtual void		AddFeed(int id, const char* name, const char* url, int interval,
							const char* filter, bool backlog, bool pauseNzb, const char* category,
							int priority, const char* feedScript)
	{
		g_FeedCoordinator->AddFeed(new FeedInfo(id, name, url, backlog, interval, filter, pauseNzb, category, priority, feedScript));
	}

	virtual void		AddTask(int id, int hours, int minutes, int weekDaysBits,
							Options::ESchedulerCommand command, const char* param)
	{
		g_Scheduler->AddTask(new Scheduler::Task(id, hours, minutes, weekDaysBits, (Scheduler::ECommand)command, param));
	}
} g_OptionsExtender;

void BootConfig()
{
	debug("Parsing command line");
	g_CommandLineParser = new CommandLineParser(g_ArgumentCount, (const char**)(*g_Arguments));
	if (g_CommandLineParser->GetPrintVersion())
	{
		printf("nzbget version: %s\n", Util::VersionRevision());
		exit(0);
	}
	if (g_CommandLineParser->GetPrintUsage() || g_CommandLineParser->GetErrors() || g_ArgumentCount <= 1)
	{
		g_CommandLineParser->PrintUsage(((const char**)(*g_Arguments))[0]);
		exit(0);
	}

	debug("Reading options");
	g_Options = new Options((*g_Arguments)[0], g_CommandLineParser->GetConfigFilename(),
							 g_CommandLineParser->GetNoConfig(), (Options::CmdOptList*)g_CommandLineParser->GetOptionList(),
							 &g_OptionsExtender);
	g_Options->SetRemoteClientMode(g_CommandLineParser->GetRemoteClientMode());
	g_Options->SetServerMode(g_CommandLineParser->GetServerMode());
	g_Options->SetPauseDownload(g_CommandLineParser->GetPauseDownload());

	g_Log->InitOptions();

	if (g_Options->GetFatalError())
	{
		exit(1);
	}
	else if (g_Options->GetConfigErrors() &&
			 g_CommandLineParser->GetClientOperation() == CommandLineParser::opClientNoOperation)
	{
		info("Pausing all activities due to errors in configuration");
		g_Options->SetPauseDownload(true);
		g_Options->SetPausePostProcess(true);
		g_Options->SetPauseScan(true);
	}

	g_ServerPool->SetTimeout(g_Options->GetArticleTimeout());
	g_ServerPool->SetRetryInterval(g_Options->GetRetryInterval());

	g_ScriptConfig = new ScriptConfig();
}

void ProcessClientRequest()
{
	RemoteClient Client;

	switch (g_CommandLineParser->GetClientOperation())
	{
		case CommandLineParser::opClientRequestListFiles:
			Client.RequestServerList(true, false, g_CommandLineParser->GetMatchMode() == CommandLineParser::mmRegEx ? g_CommandLineParser->GetEditQueueText() : nullptr);
			break;

		case CommandLineParser::opClientRequestListGroups:
			Client.RequestServerList(false, true, g_CommandLineParser->GetMatchMode() == CommandLineParser::mmRegEx ? g_CommandLineParser->GetEditQueueText() : nullptr);
			break;

		case CommandLineParser::opClientRequestListStatus:
			Client.RequestServerList(false, false, nullptr);
			break;

		case CommandLineParser::opClientRequestDownloadPause:
			Client.RequestServerPauseUnpause(true, rpDownload);
			break;

		case CommandLineParser::opClientRequestDownloadUnpause:
			Client.RequestServerPauseUnpause(false, rpDownload);
			break;

		case CommandLineParser::opClientRequestSetRate:
			Client.RequestServerSetDownloadRate(g_CommandLineParser->GetSetRate());
			break;

		case CommandLineParser::opClientRequestDumpDebug:
			Client.RequestServerDumpDebug();
			break;

		case CommandLineParser::opClientRequestEditQueue:
			Client.RequestServerEditQueue((DownloadQueue::EEditAction)g_CommandLineParser->GetEditQueueAction(),
				g_CommandLineParser->GetEditQueueOffset(), g_CommandLineParser->GetEditQueueText(),
				g_CommandLineParser->GetEditQueueIdList(), g_CommandLineParser->GetEditQueueNameList(),
				(ERemoteMatchMode)g_CommandLineParser->GetMatchMode());
			break;

		case CommandLineParser::opClientRequestLog:
			Client.RequestServerLog(g_CommandLineParser->GetLogLines());
			break;

		case CommandLineParser::opClientRequestShutdown:
			Client.RequestServerShutdown();
			break;

		case CommandLineParser::opClientRequestReload:
			Client.RequestServerReload();
			break;

		case CommandLineParser::opClientRequestDownload:
			Client.RequestServerDownload(g_CommandLineParser->GetAddNzbFilename(), g_CommandLineParser->GetArgFilename(),
				g_CommandLineParser->GetAddCategory(), g_CommandLineParser->GetAddTop(), g_CommandLineParser->GetAddPaused(), g_CommandLineParser->GetAddPriority(),
				g_CommandLineParser->GetAddDupeKey(), g_CommandLineParser->GetAddDupeMode(), g_CommandLineParser->GetAddDupeScore());
			break;

		case CommandLineParser::opClientRequestVersion:
			Client.RequestServerVersion();
			break;

		case CommandLineParser::opClientRequestPostQueue:
			Client.RequestPostQueue();
			break;

		case CommandLineParser::opClientRequestWriteLog:
			Client.RequestWriteLog(g_CommandLineParser->GetWriteLogKind(), g_CommandLineParser->GetLastArg());
			break;

		case CommandLineParser::opClientRequestScanAsync:
			Client.RequestScan(false);
			break;

		case CommandLineParser::opClientRequestScanSync:
			Client.RequestScan(true);
			break;

		case CommandLineParser::opClientRequestPostPause:
			Client.RequestServerPauseUnpause(true, rpPostProcess);
			break;

		case CommandLineParser::opClientRequestPostUnpause:
			Client.RequestServerPauseUnpause(false, rpPostProcess);
			break;

		case CommandLineParser::opClientRequestScanPause:
			Client.RequestServerPauseUnpause(true, rpScan);
			break;

		case CommandLineParser::opClientRequestScanUnpause:
			Client.RequestServerPauseUnpause(false, rpScan);
			break;

		case CommandLineParser::opClientRequestHistory:
		case CommandLineParser::opClientRequestHistoryAll:
			Client.RequestHistory(g_CommandLineParser->GetClientOperation() == CommandLineParser::opClientRequestHistoryAll);
			break;

		case CommandLineParser::opClientNoOperation:
			break;
	}
}

void ProcessWebGet()
{
	WebDownloader downloader;
	downloader.SetUrl(g_CommandLineParser->GetLastArg());
	downloader.SetForce(true);
	downloader.SetRetry(false);
	downloader.SetOutputFilename(g_CommandLineParser->GetWebGetFilename());
	downloader.SetInfoName("WebGet");

	WebDownloader::EStatus status = downloader.DownloadWithRedirects(5);
	bool ok = status == WebDownloader::adFinished;

	exit(ok ? 0 : 1);
}

void ProcessSigVerify()
{
#ifdef HAVE_OPENSSL
	bool ok = Maintenance::VerifySignature(g_CommandLineParser->GetLastArg(),
		g_CommandLineParser->GetSigFilename(), g_CommandLineParser->GetPubKeyFilename());
	exit(ok ? 93 : 1);
#else
	printf("ERROR: Could not verify signature, the program was compiled without OpenSSL support\n");
	exit(1);
#endif
}

void ExitProc()
{
	if (!g_Reloading)
	{
		info("Stopping, please wait...");
	}
	if (g_CommandLineParser->GetRemoteClientMode())
	{
		if (g_Frontend)
		{
			debug("Stopping Frontend");
			g_Frontend->Stop();
		}
	}
	else
	{
		if (g_QueueCoordinator)
		{
			debug("Stopping QueueCoordinator");
			g_ServiceCoordinator->Stop();
			g_QueueCoordinator->Stop();
			g_UrlCoordinator->Stop();
			g_PrePostProcessor->Stop();
			g_FeedCoordinator->Stop();
			g_ArticleCache->Stop();
			g_QueueScriptCoordinator->Stop();
#ifdef WIN32
			g_WinConsole->Stop();
#endif
		}
	}
}

void Reload()
{
	g_Reloading = true;
	info("Reloading...");
	ExitProc();
}

void Cleanup()
{
	debug("Cleaning up global objects");

	debug("Deleting UrlCoordinator");
	delete g_UrlCoordinator;
	g_UrlCoordinator = nullptr;
	debug("UrlCoordinator deleted");

	debug("Deleting RemoteServer");
	delete g_RemoteServer;
	g_RemoteServer = nullptr;
	debug("RemoteServer deleted");

	debug("Deleting RemoteSecureServer");
	delete g_RemoteSecureServer;
	g_RemoteSecureServer = nullptr;
	debug("RemoteSecureServer deleted");

	debug("Deleting PrePostProcessor");
	delete g_PrePostProcessor;
	g_PrePostProcessor = nullptr;
	delete g_Scanner;
	g_Scanner = nullptr;
	debug("PrePostProcessor deleted");

	debug("Deleting HistoryCoordinator");
	delete g_HistoryCoordinator;
	g_HistoryCoordinator = nullptr;
	debug("HistoryCoordinator deleted");

	debug("Deleting DupeCoordinator");
	delete g_DupeCoordinator;
	g_DupeCoordinator = nullptr;
	debug("DupeCoordinator deleted");

	debug("Deleting Frontend");
	delete g_Frontend;
	g_Frontend = nullptr;
	debug("Frontend deleted");

	debug("Deleting QueueCoordinator");
	delete g_QueueCoordinator;
	g_QueueCoordinator = nullptr;
	debug("QueueCoordinator deleted");

	debug("Deleting DiskState");
	delete g_DiskState;
	g_DiskState = nullptr;
	debug("DiskState deleted");

	debug("Deleting Options");
	if (g_Options)
	{
		if (g_CommandLineParser->GetDaemonMode() && !g_Reloading)
		{
			info("Deleting lock file");
			FileSystem::DeleteFile(g_Options->GetLockFile());
		}
		delete g_Options;
	}
	debug("Options deleted");

	debug("Deleting CommandLineParser");
	if (g_CommandLineParser)
	{
		delete g_CommandLineParser;
		g_CommandLineParser = nullptr;
	}
	debug("CommandLineParser deleted");

	debug("Deleting ScriptConfig");
	if (g_ScriptConfig)
	{
		delete g_ScriptConfig;
		g_ScriptConfig = nullptr;
	}
	debug("ScriptConfig deleted");

	debug("Deleting ServerPool");
	delete g_ServerPool;
	g_ServerPool = nullptr;
	debug("ServerPool deleted");

	debug("Deleting Scheduler");
	delete g_Scheduler;
	g_Scheduler = nullptr;
	debug("Scheduler deleted");

	debug("Deleting FeedCoordinator");
	delete g_FeedCoordinator;
	g_FeedCoordinator = nullptr;
	debug("FeedCoordinator deleted");

	debug("Deleting ArticleCache");
	delete g_ArticleCache;
	g_ArticleCache = nullptr;
	debug("ArticleCache deleted");

	debug("Deleting QueueScriptCoordinator");
	delete g_QueueScriptCoordinator;
	g_QueueScriptCoordinator = nullptr;
	debug("QueueScriptCoordinator deleted");

	debug("Deleting Maintenance");
	delete g_Maintenance;
	g_Maintenance = nullptr;
	debug("Maintenance deleted");

	debug("Deleting StatMeter");
	delete g_StatMeter;
	g_StatMeter = nullptr;
	debug("StatMeter deleted");

	debug("Deleting ServiceCoordinator");
	delete g_ServiceCoordinator;
	g_ServiceCoordinator = nullptr;
	debug("ServiceCoordinator deleted");

	debug("Deleting DiskService");
	delete g_DiskService;
	g_DiskService = nullptr;
	debug("DiskService deleted");

	if (!g_Reloading)
	{
		Connection::Final();
		Thread::Final();
	}

#ifdef WIN32
	delete g_WinConsole;
	g_WinConsole = nullptr;
#endif

	debug("Global objects cleaned up");

	Log::Final();
}

#ifndef WIN32
void Daemonize()
{
	int f = fork();
	if (f < 0) exit(1); /* fork error */
	if (f > 0) exit(0); /* parent exits */

	/* child (daemon) continues */

	// obtain a new process group
	setsid();

	// close all descriptors
	for (int i = getdtablesize(); i >= 0; --i)
	{
		close(i);
	}

	// handle standart I/O
	int d = open("/dev/null", O_RDWR);
	dup(d);
	dup(d);

	// set up lock-file
	int lfp = -1;
	if (!Util::EmptyStr(g_Options->GetLockFile()))
	{
		lfp = open(g_Options->GetLockFile(), O_RDWR | O_CREAT, 0640);
		if (lfp < 0)
		{
			error("Starting daemon failed: could not create lock-file %s", g_Options->GetLockFile());
			exit(1);
		}
		if (lockf(lfp, F_TLOCK, 0) < 0)
		{
			error("Starting daemon failed: could not acquire lock on lock-file %s", g_Options->GetLockFile());
			exit(1);
		}
	}

	/* Drop user if there is one, and we were run as root */
	if (getuid() == 0 || geteuid() == 0)
	{
		struct passwd *pw = getpwnam(g_Options->GetDaemonUsername());
		if (pw)
		{
			// Change owner of lock file
			fchown(lfp, pw->pw_uid, pw->pw_gid);
			// Set aux groups to null.
			setgroups(0, (const gid_t*)0);
			// Set primary group.
			setgid(pw->pw_gid);
			// Try setting aux groups correctly - not critical if this fails.
			initgroups(g_Options->GetDaemonUsername(), pw->pw_gid);
			// Finally, set uid.
			setuid(pw->pw_uid);
		}
	}

	// record pid to lockfile
	if (lfp > -1)
	{
		BString<100> str("%d\n", getpid());
		write(lfp, str, strlen(str));
	}

	// ignore unwanted signals
	signal(SIGCHLD, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
}
#endif
