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


#include "nzbget.h"
#include "ServerPool.h"
#include "Log.h"
#include "NzbFile.h"
#include "Options.h"
#include "WorkState.h"
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
#include "CommandScript.h"
#include "YEncode.h"
#ifdef WIN32
#include "WinService.h"
#include "WinConsole.h"
#include "WebDownloader.h"
#endif
#ifdef ENABLE_TESTS
#include "TestMain.h"
#endif
#ifndef DISABLE_NSERV
#include "NServMain.h"
#endif

// Prototypes
void RunMain();

// Globals
Log* g_Log;
Options* g_Options;
WorkState* g_WorkState;
ServerPool* g_ServerPool;
QueueCoordinator* g_QueueCoordinator;
UrlCoordinator* g_UrlCoordinator;
StatMeter* g_StatMeter;
PrePostProcessor* g_PrePostProcessor;
HistoryCoordinator* g_HistoryCoordinator;
DupeCoordinator* g_DupeCoordinator;
DiskState* g_DiskState;
Scanner* g_Scanner;
FeedCoordinator* g_FeedCoordinator;
Maintenance* g_Maintenance;
ArticleCache* g_ArticleCache;
QueueScriptCoordinator* g_QueueScriptCoordinator;
ServiceCoordinator* g_ServiceCoordinator;
ScriptConfig* g_ScriptConfig;
CommandScriptLog* g_CommandScriptLog; 
#ifdef WIN32
WinConsole* g_WinConsole;
#endif
int g_ArgumentCount;
char* (*g_EnvironmentVariables)[] = nullptr;
char* (*g_Arguments)[] = nullptr;


/*
 * Main entry point
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
	YEncode::init();

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

	if (argc > 1 && (!strcmp(argv[1], "--nserv")))
	{
#ifndef DISABLE_NSERV
		return NServMain(argc, argv);
#else
		printf("ERROR: Could not start NServ, the program was compiled without NServ\n");
		return 1;
#endif
	}

#ifdef WIN32
	InstallUninstallServiceCheck(argc, argv);
#endif

	srand((unsigned int)Util::CurrentTime());

#ifdef WIN32
	for (int i=0; i < argc; i++)
	{
		if (!strcmp(argv[i], "-D"))
		{
			AllocConsole(); // needed for sending CTRL+BREAK signal to child processes
			StartService(RunMain);
			return 0;
		}
	}
#endif

	RunMain();

	return 0;
}


class NZBGet : public Options::Extender
{
public:
	~NZBGet();
	void Run(bool reload);
	void Stop(bool reload);
	bool GetReloading() { return m_reloading; }

	// Options::Extender
	virtual void AddNewsServer(int id, bool active, const char* name, const char* host,
		int port, int ipVersion, const char* user, const char* pass, bool joinGroup,
		bool tls, const char* cipher, int maxConnections, int retention,
		int level, int group, bool optional);
	virtual void AddFeed(int id, const char* name, const char* url, int interval,
		const char* filter, bool backlog, bool pauseNzb, const char* category,
		int priority, const char* feedScript);
	virtual void AddTask(int id, int hours, int minutes, int weekDaysBits,
		Options::ESchedulerCommand command, const char* param);
#ifdef WIN32
	virtual void SetupFirstStart();
#endif

private:
	// globals
	std::unique_ptr<Log> m_log;
	std::unique_ptr<Options> m_options;
	std::unique_ptr<WorkState> m_workState;
	std::unique_ptr<ServerPool> m_serverPool;
	std::unique_ptr<QueueCoordinator> m_queueCoordinator;
	std::unique_ptr<UrlCoordinator> m_urlCoordinator;
	std::unique_ptr<StatMeter> m_statMeter;
	std::unique_ptr<PrePostProcessor> m_prePostProcessor;
	std::unique_ptr<HistoryCoordinator> m_historyCoordinator;
	std::unique_ptr<DupeCoordinator> m_dupeCoordinator;
	std::unique_ptr<DiskState> m_diskState;
	std::unique_ptr<Scanner> m_scanner;
	std::unique_ptr<FeedCoordinator> m_feedCoordinator;
	std::unique_ptr<Maintenance> m_maintenance;
	std::unique_ptr<ArticleCache> m_articleCache;
	std::unique_ptr<QueueScriptCoordinator> m_queueScriptCoordinator;
	std::unique_ptr<ServiceCoordinator> m_serviceCoordinator;
	std::unique_ptr<ScriptConfig> m_scriptConfig;
	std::unique_ptr<CommandScriptLog> m_commandScriptLog;
#ifdef WIN32
	std::unique_ptr<WinConsole> m_winConsole;
#endif

	// non-globals
	std::unique_ptr<Thread> m_frontend;
	std::unique_ptr<RemoteServer> m_remoteServer;
	std::unique_ptr<RemoteServer> m_remoteSecureServer;
	std::unique_ptr<DiskService> m_diskService;
	std::unique_ptr<Scheduler> m_scheduler;
	std::unique_ptr<CommandLineParser> m_commandLineParser;

	bool m_reloading = false;
	bool m_daemonized = false;
	bool m_stopped = false;
	Mutex m_waitMutex;
	ConditionVar m_waitCond;

	void Init();
	void Final();
	void BootConfig();
	void CreateGlobals();
	void Cleanup();
	void PrintOptions();
	void ProcessDirect();
	void ProcessClientRequest();
	void ProcessWebGet();
	void ProcessSigVerify();
	void StartRemoteServer();
	void StopRemoteServer();
	void StartFrontend();
	void StopFrontend();
	void ProcessStandalone();
	void DoMainLoop();
#ifndef WIN32
	void Daemonize();
#endif
};

std::unique_ptr<NZBGet> g_NZBGet;

NZBGet::~NZBGet()
{
	Cleanup();
}

void NZBGet::Init()
{
	m_log = std::make_unique<Log>();

	debug("nzbget %s", Util::VersionRevision());

	if (!m_reloading)
	{
		Thread::Init();
		Connection::Init();
#ifndef DISABLE_TLS
		TlsSocket::Init();
#endif
	}

	CreateGlobals();

#ifdef WIN32
	m_winConsole->InitAppMode();
#endif

	BootConfig();

#ifndef WIN32
	if (m_options->GetUMask() < 01000)
	{
		/* set newly created file permissions */
		umask(m_options->GetUMask());
	}
#endif

	m_scanner->InitOptions();
	m_queueScriptCoordinator->InitOptions();
#ifndef DISABLE_TLS
	TlsSocket::InitOptions(g_Options->GetCertCheck() ? g_Options->GetCertStore() : nullptr);
#endif

	if (m_commandLineParser->GetDaemonMode())
	{
#ifdef WIN32
		info("nzbget %s service-mode", Util::VersionRevision());
#else
		if (!m_reloading)
		{
			Daemonize();
		}
		info("nzbget %s daemon-mode", Util::VersionRevision());
#endif
	}
	else if (m_options->GetServerMode())
	{
		info("nzbget %s server-mode", Util::VersionRevision());
	}
	else if (m_commandLineParser->GetRemoteClientMode())
	{
		info("nzbget %s remote-mode", Util::VersionRevision());
	}

	m_reloading = false;

	if (!m_commandLineParser->GetRemoteClientMode())
	{
		m_serverPool->InitConnections();
		m_statMeter->Init();
	}

	InstallErrorHandler();
}

void NZBGet::Final()
{
	if (!m_reloading)
	{
#ifndef DISABLE_TLS
		TlsSocket::Final();
#endif
		Connection::Final();
	}
}

void NZBGet::CreateGlobals()
{
#ifdef WIN32
	m_winConsole = std::make_unique<WinConsole>();
	g_WinConsole = m_winConsole.get();
#endif

	m_workState = std::make_unique<WorkState>();
	g_WorkState = m_workState.get();

	m_serviceCoordinator = std::make_unique<ServiceCoordinator>();
	g_ServiceCoordinator = m_serviceCoordinator.get();

	m_serverPool = std::make_unique<ServerPool>();
	g_ServerPool = m_serverPool.get();

	m_queueCoordinator = std::make_unique<QueueCoordinator>();
	g_QueueCoordinator = m_queueCoordinator.get();

	m_statMeter = std::make_unique<StatMeter>();
	g_StatMeter = m_statMeter.get();

	m_scanner = std::make_unique<Scanner>();
	g_Scanner = m_scanner.get();

	m_prePostProcessor = std::make_unique<PrePostProcessor>();
	g_PrePostProcessor = m_prePostProcessor.get();

	m_historyCoordinator = std::make_unique<HistoryCoordinator>();
	g_HistoryCoordinator = m_historyCoordinator.get();

	m_dupeCoordinator = std::make_unique<DupeCoordinator>();
	g_DupeCoordinator = m_dupeCoordinator.get();

	m_urlCoordinator = std::make_unique<UrlCoordinator>();
	g_UrlCoordinator = m_urlCoordinator.get();

	m_feedCoordinator = std::make_unique<FeedCoordinator>();
	g_FeedCoordinator = m_feedCoordinator.get();

	m_articleCache = std::make_unique<ArticleCache>();
	g_ArticleCache = m_articleCache.get();

	m_maintenance = std::make_unique<Maintenance>();
	g_Maintenance = m_maintenance.get();

	m_queueScriptCoordinator = std::make_unique<QueueScriptCoordinator>();
	g_QueueScriptCoordinator = m_queueScriptCoordinator.get();

	m_diskState = std::make_unique<DiskState>();
	g_DiskState = m_diskState.get();

	m_scriptConfig = std::make_unique<ScriptConfig>();
	g_ScriptConfig = m_scriptConfig.get();

	m_commandScriptLog = std::make_unique<CommandScriptLog>();
	g_CommandScriptLog = m_commandScriptLog.get();

	m_scheduler = std::make_unique<Scheduler>();

	m_diskService = std::make_unique<DiskService>();
}

void NZBGet::BootConfig()
{
	debug("Parsing command line");
	m_commandLineParser = std::make_unique<CommandLineParser>(g_ArgumentCount, (const char**)(*g_Arguments));
	if (m_commandLineParser->GetPrintVersion())
	{
		printf("nzbget version: %s\n", Util::VersionRevision());
		exit(0);
	}
	if (m_commandLineParser->GetPrintUsage() || m_commandLineParser->GetErrors() || g_ArgumentCount <= 1)
	{
		m_commandLineParser->PrintUsage(((const char**)(*g_Arguments))[0]);
		exit(m_commandLineParser->GetPrintUsage() ? 0 : 1);
	}

	debug("Reading options");
	m_options = std::make_unique<Options>((*g_Arguments)[0], m_commandLineParser->GetConfigFilename(),
		m_commandLineParser->GetNoConfig(), (Options::CmdOptList*)m_commandLineParser->GetOptionList(), this);
	m_options->SetRemoteClientMode(m_commandLineParser->GetRemoteClientMode());
	m_options->SetServerMode(m_commandLineParser->GetServerMode());
	m_workState->SetPauseDownload(m_commandLineParser->GetPauseDownload());
	m_workState->SetSpeedLimit(g_Options->GetDownloadRate());

	m_log->InitOptions();

	if (m_options->GetFatalError())
	{
		exit(1);
	}
	else if (m_options->GetConfigErrors() &&
		m_commandLineParser->GetClientOperation() == CommandLineParser::opClientNoOperation)
	{
		info("Pausing all activities due to errors in configuration");
		m_workState->SetPauseDownload(true);
		m_workState->SetPausePostProcess(true);
		m_workState->SetPauseScan(true);
	}

	m_serverPool->SetTimeout(m_options->GetArticleTimeout());
	m_serverPool->SetRetryInterval(m_options->GetArticleInterval());

	m_scriptConfig->InitOptions();
}

void NZBGet::Cleanup()
{
	debug("Cleaning up global objects");

	if (m_options && m_commandLineParser->GetDaemonMode() && !m_reloading && m_daemonized)
	{
		info("Deleting lock file");
		FileSystem::DeleteFile(m_options->GetLockFile());
	}

	g_UrlCoordinator = nullptr;
	g_PrePostProcessor = nullptr;
	g_Scanner = nullptr;
	g_HistoryCoordinator = nullptr;
	g_DupeCoordinator = nullptr;
	g_QueueCoordinator = nullptr;
	g_DiskState = nullptr;
	g_ScriptConfig = nullptr;
	g_ServerPool = nullptr;
	g_FeedCoordinator = nullptr;
	g_ArticleCache = nullptr;
	g_QueueScriptCoordinator = nullptr;
	g_Maintenance = nullptr;
	g_StatMeter = nullptr;
	g_CommandScriptLog = nullptr;
#ifdef WIN32
	g_WinConsole = nullptr;
#endif
}

void NZBGet::ProcessDirect()
{
#ifdef DEBUG
	if (m_commandLineParser->GetTestBacktrace())
	{
		TestSegFault(); // never returns
	}
#endif

	if (m_commandLineParser->GetWebGet())
	{
		ProcessWebGet(); // never returns
	}

	if (m_commandLineParser->GetSigVerify())
	{
		ProcessSigVerify(); // never returns
	}

	// client request
	if (m_commandLineParser->GetClientOperation() != CommandLineParser::opClientNoOperation)
	{
		ProcessClientRequest(); // never returns
	}

	if (m_commandLineParser->GetPrintOptions())
	{
		PrintOptions(); // never returns
	}
}

void NZBGet::StartRemoteServer()
{
	if (!m_options->GetServerMode())
	{
		return;
	}

	WebProcessor::Init();
	m_remoteServer = std::make_unique<RemoteServer>(false);
	m_remoteServer->Start();

	if (m_options->GetSecureControl()
#ifndef WIN32
		&& !(m_options->GetControlIp() && m_options->GetControlIp()[0] == '/')
#endif
)
	{
		m_remoteSecureServer = std::make_unique<RemoteServer>(true);
		m_remoteSecureServer->Start();
	}
}

void NZBGet::StopRemoteServer()
{
	if (m_remoteServer)
	{
		debug("stopping RemoteServer");
		m_remoteServer->Stop();
	}

	if (m_remoteSecureServer)
	{
		debug("stopping RemoteSecureServer");
		m_remoteSecureServer->Stop();
	}

	int maxWaitMSec = 5000;
	while (((m_remoteServer && m_remoteServer->IsRunning()) ||
		(m_remoteSecureServer && m_remoteSecureServer->IsRunning())) &&
		maxWaitMSec > 0)
	{
		Util::Sleep(100);
		maxWaitMSec -= 100;
	}

	if (m_remoteServer && m_remoteServer->IsRunning())
	{
		m_remoteServer->ForceStop();
	}

	if (m_remoteSecureServer && m_remoteSecureServer->IsRunning())
	{
		m_remoteSecureServer->ForceStop();
	}

	maxWaitMSec = 5000;
	while (((m_remoteServer && m_remoteServer->IsRunning()) ||
		(m_remoteSecureServer && m_remoteSecureServer->IsRunning())) &&
		maxWaitMSec > 0)
	{
		Util::Sleep(100);
		maxWaitMSec -= 100;
	}

	if (m_remoteServer && m_remoteServer->IsRunning())
	{
		debug("Killing RemoteServer");
		m_remoteServer->Kill();
	}

	if (m_remoteSecureServer && m_remoteSecureServer->IsRunning())
	{
		debug("Killing RemoteSecureServer");
		m_remoteSecureServer->Kill();
	}

	debug("RemoteServer stopped");
}

void NZBGet::StartFrontend()
{
	if (!m_commandLineParser->GetDaemonMode())
	{
		switch (m_options->GetOutputMode())
		{
		case Options::omNCurses:
#ifndef DISABLE_CURSES
			m_frontend = std::make_unique<NCursesFrontend>();
			break;
#endif

		case Options::omColored:
			m_frontend = std::make_unique<ColoredFrontend>();
			break;

		case Options::omLoggable:
			m_frontend = std::make_unique<LoggableFrontend>();
			break;
		}
	}

	if (m_frontend)
	{
		m_frontend->Start();
	}
}

void NZBGet::StopFrontend()
{
	if (m_frontend)
	{
		if (!m_commandLineParser->GetRemoteClientMode())
		{
			debug("Stopping Frontend");
			m_frontend->Stop();
		}
		while (m_frontend->IsRunning())
		{
			Util::Sleep(50);
		}
		debug("Frontend stopped");
	}
}

void NZBGet::ProcessStandalone()
{
	const char* category = m_commandLineParser->GetAddCategory() ? m_commandLineParser->GetAddCategory() : "";
	NzbFile nzbFile(m_commandLineParser->GetArgFilename(), category);
	if (!nzbFile.Parse())
	{
		printf("Parsing NZB-document %s failed\n\n",
			m_commandLineParser->GetArgFilename() ? m_commandLineParser->GetArgFilename() : "N/A");
		return;
	}
	std::unique_ptr<NzbInfo> nzbInfo = nzbFile.DetachNzbInfo();
	m_scanner->InitPPParameters(category, nzbInfo->GetParameters(), false);
	m_queueCoordinator->AddNzbFileToQueue(std::move(nzbInfo), nullptr, false);
}

void NZBGet::DoMainLoop()
{
	debug("Entering main program loop");

#ifdef WIN32
	m_winConsole->Start();
#endif
	m_queueCoordinator->Start();
	m_urlCoordinator->Start();
	m_prePostProcessor->Start();
	m_feedCoordinator->Start();
	m_serviceCoordinator->Start();
	if (m_options->GetArticleCache() > 0)
	{
		m_articleCache->Start();
	}

	// enter main program-loop
	while (m_queueCoordinator->IsRunning() ||
		m_urlCoordinator->IsRunning() ||
		m_prePostProcessor->IsRunning() ||
		m_feedCoordinator->IsRunning() ||
		m_serviceCoordinator->IsRunning() ||
#ifdef WIN32
		m_winConsole->IsRunning() ||
#endif
		m_articleCache->IsRunning())
	{
		if (!m_options->GetServerMode() &&
			!m_queueCoordinator->HasMoreJobs() &&
			!m_urlCoordinator->HasMoreJobs() &&
			!m_prePostProcessor->HasMoreJobs())
		{
			// Standalone-mode: download completed
			if (!m_queueCoordinator->IsStopped())
			{
				m_queueCoordinator->Stop();
			}
			if (!m_urlCoordinator->IsStopped())
			{
				m_urlCoordinator->Stop();
			}
			if (!m_prePostProcessor->IsStopped())
			{
				m_prePostProcessor->Stop();
			}
			if (!m_feedCoordinator->IsStopped())
			{
				m_feedCoordinator->Stop();
			}
			if (!m_articleCache->IsStopped())
			{
				m_articleCache->Stop();
			}
			if (!m_serviceCoordinator->IsStopped())
			{
				m_serviceCoordinator->Stop();
			}
		}
		Util::Sleep(100);

		if (m_options->GetServerMode() && !m_stopped)
		{
			// wait for stop signal
			Guard guard(m_waitMutex);
			m_waitCond.Wait(m_waitMutex, [&]{ return m_stopped; });
		}
	}

	debug("Main program loop terminated");
}

void NZBGet::Run(bool reload)
{
	m_reloading = reload;

	Init();

	ProcessDirect();

	StartRemoteServer();
	StartFrontend();

	if (!m_commandLineParser->GetRemoteClientMode())
	{
		if (!m_commandLineParser->GetServerMode())
		{
			ProcessStandalone();
		}

		DoMainLoop();
	}

	ScriptController::TerminateAll();

	StopRemoteServer();
	StopFrontend();

	Final();
}

void NZBGet::ProcessClientRequest()
{
	RemoteClient Client;
	bool ok = false;

	switch (m_commandLineParser->GetClientOperation())
	{
		case CommandLineParser::opClientRequestListFiles:
			ok = Client.RequestServerList(true, false, m_commandLineParser->GetMatchMode() == CommandLineParser::mmRegEx ? m_commandLineParser->GetEditQueueText() : nullptr);
			break;

		case CommandLineParser::opClientRequestListGroups:
			ok = Client.RequestServerList(false, true, m_commandLineParser->GetMatchMode() == CommandLineParser::mmRegEx ? m_commandLineParser->GetEditQueueText() : nullptr);
			break;

		case CommandLineParser::opClientRequestListStatus:
			ok = Client.RequestServerList(false, false, nullptr);
			break;

		case CommandLineParser::opClientRequestDownloadPause:
			ok = Client.RequestServerPauseUnpause(true, rpDownload);
			break;

		case CommandLineParser::opClientRequestDownloadUnpause:
			ok = Client.RequestServerPauseUnpause(false, rpDownload);
			break;

		case CommandLineParser::opClientRequestSetRate:
			ok = Client.RequestServerSetDownloadRate(m_commandLineParser->GetSetRate());
			break;

		case CommandLineParser::opClientRequestDumpDebug:
			ok = Client.RequestServerDumpDebug();
			break;

		case CommandLineParser::opClientRequestEditQueue:
			ok = Client.RequestServerEditQueue((DownloadQueue::EEditAction)m_commandLineParser->GetEditQueueAction(),
				m_commandLineParser->GetEditQueueOffset(), m_commandLineParser->GetEditQueueText(),
				m_commandLineParser->GetEditQueueIdList(), m_commandLineParser->GetEditQueueNameList(),
				(ERemoteMatchMode)m_commandLineParser->GetMatchMode());
			break;

		case CommandLineParser::opClientRequestLog:
			ok = Client.RequestServerLog(m_commandLineParser->GetLogLines());
			break;

		case CommandLineParser::opClientRequestShutdown:
			ok = Client.RequestServerShutdown();
			break;

		case CommandLineParser::opClientRequestReload:
			ok = Client.RequestServerReload();
			break;

		case CommandLineParser::opClientRequestDownload:
			ok = Client.RequestServerDownload(m_commandLineParser->GetAddNzbFilename(), m_commandLineParser->GetArgFilename(),
				m_commandLineParser->GetAddCategory(), m_commandLineParser->GetAddTop(), m_commandLineParser->GetAddPaused(), m_commandLineParser->GetAddPriority(),
				m_commandLineParser->GetAddDupeKey(), m_commandLineParser->GetAddDupeMode(), m_commandLineParser->GetAddDupeScore());
			break;

		case CommandLineParser::opClientRequestVersion:
			ok = Client.RequestServerVersion();
			break;

		case CommandLineParser::opClientRequestPostQueue:
			ok = Client.RequestPostQueue();
			break;

		case CommandLineParser::opClientRequestWriteLog:
			ok = Client.RequestWriteLog(m_commandLineParser->GetWriteLogKind(), m_commandLineParser->GetLastArg());
			break;

		case CommandLineParser::opClientRequestScanAsync:
			ok = Client.RequestScan(false);
			break;

		case CommandLineParser::opClientRequestScanSync:
			ok = Client.RequestScan(true);
			break;

		case CommandLineParser::opClientRequestPostPause:
			ok = Client.RequestServerPauseUnpause(true, rpPostProcess);
			break;

		case CommandLineParser::opClientRequestPostUnpause:
			ok = Client.RequestServerPauseUnpause(false, rpPostProcess);
			break;

		case CommandLineParser::opClientRequestScanPause:
			ok = Client.RequestServerPauseUnpause(true, rpScan);
			break;

		case CommandLineParser::opClientRequestScanUnpause:
			ok = Client.RequestServerPauseUnpause(false, rpScan);
			break;

		case CommandLineParser::opClientRequestHistory:
		case CommandLineParser::opClientRequestHistoryAll:
			ok = Client.RequestHistory(m_commandLineParser->GetClientOperation() == CommandLineParser::opClientRequestHistoryAll);
			break;

		case CommandLineParser::opClientNoOperation:
			return;
	}

	exit(ok ? 0 : 1);
}

void NZBGet::ProcessWebGet()
{
	WebDownloader downloader;
	downloader.SetUrl(m_commandLineParser->GetLastArg());
	downloader.SetForce(true);
	downloader.SetRetry(false);
	downloader.SetOutputFilename(m_commandLineParser->GetWebGetFilename());
	downloader.SetInfoName("WebGet");

	WebDownloader::EStatus status = downloader.DownloadWithRedirects(5);
	bool ok = status == WebDownloader::adFinished;

	exit(ok ? 0 : 1);
}

void NZBGet::ProcessSigVerify()
{
#ifdef HAVE_OPENSSL
	bool ok = Maintenance::VerifySignature(m_commandLineParser->GetLastArg(),
		m_commandLineParser->GetSigFilename(), m_commandLineParser->GetPubKeyFilename());
	exit(ok ? 93 : 1);
#else
	printf("ERROR: Could not verify signature, the program was compiled without OpenSSL support\n");
	exit(1);
#endif
}

void NZBGet::Stop(bool reload)
{
	m_reloading = reload;

	if (!m_reloading)
	{
		info("Stopping, please wait...");
	}
	if (m_commandLineParser->GetRemoteClientMode())
	{
		if (m_frontend)
		{
			debug("Stopping Frontend");
			m_frontend->Stop();
		}
	}
	else
	{
		if (m_queueCoordinator)
		{
			debug("Stopping QueueCoordinator");
			m_serviceCoordinator->Stop();
			m_queueCoordinator->Stop();
			m_urlCoordinator->Stop();
			m_prePostProcessor->Stop();
			m_feedCoordinator->Stop();
			m_articleCache->Stop();
			m_queueScriptCoordinator->Stop();
#ifdef WIN32
			m_winConsole->Stop();
#endif
		}
	}

	// trigger stop/reload signal
	Guard guard(m_waitMutex);
	m_stopped = true;
	m_waitCond.NotifyAll();
}

void NZBGet::PrintOptions()
{
	for (Options::OptEntry& optEntry : g_Options->GuardOptEntries())
	{
		printf("%s = \"%s\"\n", optEntry.GetName(), optEntry.GetValue());
	}
	exit(0);
}

#ifndef WIN32
void NZBGet::Daemonize()
{
	int f = fork();
	if (f < 0) exit(1); /* fork error */
	if (f > 0) exit(0); /* parent exits */

	/* child (daemon) continues */
	m_daemonized = true;

	// obtain a new process group
	setsid();

	// redirect standard I/O
	int d = open("/dev/null", O_RDWR);
	dup2(d, 0);
	dup2(d, 1);
	dup2(d, 2);
	close(d);

    struct passwd *pw = getpwnam(m_options->GetDaemonUsername());
    if ( pw != NULL )
    {
        /* retrieve what this process is permitted to do */
        cap_t caps = cap_get_proc();

        if ( caps != NULL )
        {
            cap_flag_value_t canSetGID;
            if ( cap_get_flag( caps, CAP_SETGID, CAP_PERMITTED, &canSetGID ) == -1 )
            {
                canSetGID = CAP_CLEAR;
            }
            if ( canSetGID == CAP_SET ) {
                // Set aux groups to null.
                setgroups(0, (const gid_t *) 0);
                // Set primary group.
                if ( setgid( pw->pw_gid ) == -1 )
                {
                    error("Starting daemon failed: setting group ID");
                }
                // Try setting aux groups correctly - not critical if this fails.
                initgroups(m_options->GetDaemonUsername(), pw->pw_gid);
            }

            cap_flag_value_t canSetUID;
            if ( cap_get_flag( caps, CAP_SETUID, CAP_PERMITTED, &canSetUID ) == -1 )
            {
                canSetUID = CAP_CLEAR;
            }
            if ( canSetUID == CAP_SET )
            {
                // Finally, set uid.
                if ( setuid(pw->pw_uid) == -1 )
                {
                    error("Starting daemon failed: setting group ID");
                }
            }

            cap_free( caps );
        }
    }

	// set up lock-file
	int lfp = -1;
	if (!Util::EmptyStr(m_options->GetLockFile()))
	{
		lfp = open(m_options->GetLockFile(), O_RDWR | O_CREAT, 0640);
		if (lfp < 0)
		{
			error("Starting daemon failed: could not create lock-file %s", m_options->GetLockFile());
			exit(1);
		}

#ifdef HAVE_LOCKF
		if (lockf(lfp, F_TLOCK, 0) < 0)
#else
		if (flock(lfp, LOCK_EX) < 0)
#endif
		{
			error("Starting daemon failed: could not acquire lock on lock-file %s", m_options->GetLockFile());
			exit(1);
		}
	}

	// record pid to lockfile
	if (lfp > -1)
	{
		BString<100> str("%d\n", getpid());
		if ( write(lfp, str, strlen(str) == -1 ) )
        {
            error("Starting daemon failed: unable to write PID to lock-file %s", m_options->GetLockFile());
            exit(1);
        }
	}

	// ignore unwanted signals
	signal(SIGCHLD, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
}
#endif

void NZBGet::AddNewsServer(int id, bool active, const char* name, const char* host,
	int port, int ipVersion, const char* user, const char* pass, bool joinGroup, bool tls,
	const char* cipher, int maxConnections, int retention, int level, int group, bool optional)
{
	m_serverPool->AddServer(std::make_unique<NewsServer>(id, active, name, host, port, ipVersion, user, pass, joinGroup,
		tls, cipher, maxConnections, retention, level, group, optional));
}

void NZBGet::AddFeed(int id, const char* name, const char* url, int interval, const char* filter,
	bool backlog, bool pauseNzb, const char* category, int priority, const char* feedScript)
{
	m_feedCoordinator->AddFeed(std::make_unique<FeedInfo>(id, name, url, backlog, interval, filter,
		pauseNzb, category, priority, feedScript));
}

void NZBGet::AddTask(int id, int hours, int minutes, int weekDaysBits,
	Options::ESchedulerCommand command, const char* param)
{
	m_scheduler->AddTask(std::make_unique<Scheduler::Task>(id, hours, minutes, weekDaysBits,
		(Scheduler::ECommand)command, param));
}

#ifdef WIN32
void NZBGet::SetupFirstStart()
{
	m_winConsole->SetupFirstStart();
}
#endif

void RunMain()
{
	bool reload = false;
	while (!g_NZBGet || g_NZBGet->GetReloading())
	{
		g_NZBGet = std::make_unique<NZBGet>();
		g_NZBGet->Run(reload);
		reload = true;
	}

	g_NZBGet.reset();
}

void Reload()
{
	info("Reloading...");
	g_NZBGet->Stop(true);
}

void ExitProc()
{
	g_NZBGet->Stop(false);
}
