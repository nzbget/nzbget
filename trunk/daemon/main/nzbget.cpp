/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#ifdef WIN32
#include <winsvc.h>
#else
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/resource.h>
#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif
#include <signal.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#ifndef DISABLE_PARCHECK
#include <iostream>
#endif
#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#endif

#include "nzbget.h"
#include "ServerPool.h"
#include "Log.h"
#include "NZBFile.h"
#include "Options.h"
#include "Thread.h"
#include "ColoredFrontend.h"
#include "NCursesFrontend.h"
#include "QueueCoordinator.h"
#include "UrlCoordinator.h"
#include "RemoteServer.h"
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
#include "Maintenance.h"
#include "ArticleWriter.h"
#include "StatMeter.h"
#include "Util.h"
#ifdef WIN32
#include "NTService.h"
#endif

// Prototypes
void RunMain();
void Run(bool bReload);
void Reload();
void Cleanup();
void ProcessClientRequest();
#ifndef WIN32
void InstallSignalHandlers();
void Daemonize();
void PrintBacktrace();
#ifdef HAVE_SYS_PRCTL_H
void EnableDumpCore();
#endif
#ifdef DEBUG
void MakeSegFault();
#endif
#endif
#ifndef DISABLE_PARCHECK
void DisableCout();
#endif

Thread* g_pFrontend = NULL;
Options* g_pOptions = NULL;
ServerPool* g_pServerPool = NULL;
QueueCoordinator* g_pQueueCoordinator = NULL;
UrlCoordinator* g_pUrlCoordinator = NULL;
RemoteServer* g_pRemoteServer = NULL;
RemoteServer* g_pRemoteSecureServer = NULL;
StatMeter* g_pStatMeter = NULL;
Log* g_pLog = NULL;
PrePostProcessor* g_pPrePostProcessor = NULL;
HistoryCoordinator* g_pHistoryCoordinator = NULL;
DupeCoordinator* g_pDupeCoordinator = NULL;
DiskState* g_pDiskState = NULL;
Scheduler* g_pScheduler = NULL;
Scanner* g_pScanner = NULL;
FeedCoordinator* g_pFeedCoordinator = NULL;
Maintenance* g_pMaintenance = NULL;
ArticleCache* g_pArticleCache = NULL;
int g_iArgumentCount;
char* (*g_szEnvironmentVariables)[] = NULL;
char* (*g_szArguments)[] = NULL;
bool g_bReloading = true;

/*
 * Main loop
 */
int main(int argc, char *argv[], char *argp[])
{
#ifdef WIN32
#ifdef _DEBUG
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF
#ifdef DEBUG_CRTMEMLEAKS
		| _CRTDBG_CHECK_CRT_DF | _CRTDBG_CHECK_ALWAYS_DF
#endif
		);
#endif
#endif

	Util::InitVersionRevision();
	
#ifdef WIN32
	InstallUninstallServiceCheck(argc, argv);
#endif

#ifndef DISABLE_PARCHECK
	DisableCout();
#endif

	srand (time(NULL));

	g_iArgumentCount = argc;
	g_szArguments = (char*(*)[])argv;
	g_szEnvironmentVariables = (char*(*)[])argp;

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

#ifdef WIN32
#ifdef _DEBUG
	_CrtDumpMemoryLeaks();
#endif
#endif

	return 0;
}

void RunMain()
{
	// we need to save and later restore current directory each time
	// the program is reloaded (RPC-Method "reload") in order for
	// config to properly load in a case relative paths are used 
	// in command line
	char szCurDir[MAX_PATH + 1];
	Util::GetCurrentDirectory(szCurDir, sizeof(szCurDir));
	
	bool bReload = false;
	while (g_bReloading)
	{
		g_bReloading = false;
		Util::SetCurrentDirectory(szCurDir);
		Run(bReload);
		bReload = true;
	}
}

void Run(bool bReload)
{
	g_pLog = new Log();

	debug("nzbget %s", Util::VersionRevision());

	if (!bReload)
	{
		Thread::Init();
	}

	g_pServerPool = new ServerPool();
	g_pScheduler = new Scheduler();
	g_pQueueCoordinator = new QueueCoordinator();
	g_pStatMeter = new StatMeter();
	g_pScanner = new Scanner();
	g_pPrePostProcessor = new PrePostProcessor();
	g_pHistoryCoordinator = new HistoryCoordinator();
	g_pDupeCoordinator = new DupeCoordinator();
	g_pUrlCoordinator = new UrlCoordinator();
	g_pFeedCoordinator = new FeedCoordinator();
	g_pArticleCache = new ArticleCache();
	g_pMaintenance = new Maintenance();

	debug("Reading options");
	g_pOptions = new Options(g_iArgumentCount, *g_szArguments);

#ifndef WIN32
	if (g_pOptions->GetUMask() < 01000)
	{
		/* set newly created file permissions */
		umask(g_pOptions->GetUMask());
	}
#endif
	
	g_pLog->InitOptions();
	g_pScanner->InitOptions();

	if (g_pOptions->GetDaemonMode())
	{
#ifdef WIN32
		info("nzbget %s service-mode", Util::VersionRevision());
#else
		if (!bReload)
		{
			Daemonize();
		}
		info("nzbget %s daemon-mode", Util::VersionRevision());
#endif
	}
	else if (g_pOptions->GetServerMode())
	{
		info("nzbget %s server-mode", Util::VersionRevision());
	}
	else if (g_pOptions->GetRemoteClientMode())
	{
		info("nzbget %s remote-mode", Util::VersionRevision());
	}

	if (!bReload)
	{
		Connection::Init();
	}

	if (!g_pOptions->GetRemoteClientMode())
	{
		g_pServerPool->InitConnections();
		g_pStatMeter->Init();
	}

#ifndef WIN32
#ifdef HAVE_SYS_PRCTL_H
	if (g_pOptions->GetDumpCore())
	{
		EnableDumpCore();
	}
#endif
#endif

#ifndef WIN32
	InstallSignalHandlers();
#ifdef DEBUG
	if (g_pOptions->GetTestBacktrace())
	{
		MakeSegFault();
	}
#endif
#endif

	// client request
	if (g_pOptions->GetClientOperation() != Options::opClientNoOperation)
	{
		ProcessClientRequest();
		Cleanup();
		return;
	}

	// Setup the network-server
	if (g_pOptions->GetServerMode())
	{
		g_pRemoteServer = new RemoteServer(false);
		g_pRemoteServer->Start();

		if (g_pOptions->GetSecureControl())
		{
			g_pRemoteSecureServer = new RemoteServer(true);
			g_pRemoteSecureServer->Start();
		}
	}

	// Create the frontend
	if (!g_pOptions->GetDaemonMode())
	{
		switch (g_pOptions->GetOutputMode())
		{
			case Options::omNCurses:
#ifndef DISABLE_CURSES
				g_pFrontend = new NCursesFrontend();
				break;
#endif
			case Options::omColored:
				g_pFrontend = new ColoredFrontend();
				break;
			case Options::omLoggable:
				g_pFrontend = new LoggableFrontend();
				break;
		}
	}

	// Starting a thread with the frontend
	if (g_pFrontend)
	{
		g_pFrontend->Start();
	}

	// Starting QueueCoordinator and PrePostProcessor
	if (!g_pOptions->GetRemoteClientMode())
	{
		// Standalone-mode
		if (!g_pOptions->GetServerMode())
		{
			const char* szCategory = g_pOptions->GetAddCategory() ? g_pOptions->GetAddCategory() : "";
			NZBFile* pNZBFile = NZBFile::Create(g_pOptions->GetArgFilename(), szCategory);
			if (!pNZBFile)
			{
				abort("FATAL ERROR: Parsing NZB-document %s failed\n\n", g_pOptions->GetArgFilename() ? g_pOptions->GetArgFilename() : "N/A");
				return;
			}
			g_pScanner->InitPPParameters(szCategory, pNZBFile->GetNZBInfo()->GetParameters());
			g_pQueueCoordinator->AddNZBFileToQueue(pNZBFile, NULL, false);
			delete pNZBFile;
		}

		if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
		{
			g_pDiskState = new DiskState();
		}

		g_pQueueCoordinator->Start();
		g_pUrlCoordinator->Start();
		g_pPrePostProcessor->Start();
		g_pFeedCoordinator->Start();
		if (g_pOptions->GetArticleCache() > 0)
		{
			g_pArticleCache->Start();
		}

		// enter main program-loop
		while (g_pQueueCoordinator->IsRunning() || 
			g_pUrlCoordinator->IsRunning() || 
			g_pPrePostProcessor->IsRunning() ||
			g_pFeedCoordinator->IsRunning() ||
			g_pArticleCache->IsRunning())
		{
			if (!g_pOptions->GetServerMode() && 
				!g_pQueueCoordinator->HasMoreJobs() && 
				!g_pUrlCoordinator->HasMoreJobs() && 
				!g_pPrePostProcessor->HasMoreJobs())
			{
				// Standalone-mode: download completed
				if (!g_pQueueCoordinator->IsStopped())
				{
					g_pQueueCoordinator->Stop();
				}
				if (!g_pUrlCoordinator->IsStopped())
				{
					g_pUrlCoordinator->Stop();
				}
				if (!g_pPrePostProcessor->IsStopped())
				{
					g_pPrePostProcessor->Stop();
				}
				if (!g_pFeedCoordinator->IsStopped())
				{
					g_pFeedCoordinator->Stop();
				}
				if (!g_pArticleCache->IsStopped())
				{
					g_pArticleCache->Stop();
				}
			}
			usleep(100 * 1000);
		}

		// main program-loop is terminated
		debug("QueueCoordinator stopped");
		debug("UrlCoordinator stopped");
		debug("PrePostProcessor stopped");
		debug("FeedCoordinator stopped");
		debug("ArticleCache stopped");
	}

	// Stop network-server
	if (g_pRemoteServer)
	{
		debug("stopping RemoteServer");
		g_pRemoteServer->Stop();
		int iMaxWaitMSec = 1000;
		while (g_pRemoteServer->IsRunning() && iMaxWaitMSec > 0)
		{
			usleep(100 * 1000);
			iMaxWaitMSec -= 100;
		}
		if (g_pRemoteServer->IsRunning())
		{
			debug("Killing RemoteServer");
			g_pRemoteServer->Kill();
		}
		debug("RemoteServer stopped");
	}

	if (g_pRemoteSecureServer)
	{
		debug("stopping RemoteSecureServer");
		g_pRemoteSecureServer->Stop();
		int iMaxWaitMSec = 1000;
		while (g_pRemoteSecureServer->IsRunning() && iMaxWaitMSec > 0)
		{
			usleep(100 * 1000);
			iMaxWaitMSec -= 100;
		}
		if (g_pRemoteSecureServer->IsRunning())
		{
			debug("Killing RemoteSecureServer");
			g_pRemoteSecureServer->Kill();
		}
		debug("RemoteSecureServer stopped");
	}

	// Stop Frontend
	if (g_pFrontend)
	{
		if (!g_pOptions->GetRemoteClientMode())
		{
			debug("Stopping Frontend");
			g_pFrontend->Stop();
		}
		while (g_pFrontend->IsRunning())
		{
			usleep(50 * 1000);
		}
		debug("Frontend stopped");
	}

	Cleanup();
}

void ProcessClientRequest()
{
	RemoteClient* Client = new RemoteClient();

	switch (g_pOptions->GetClientOperation())
	{
		case Options::opClientRequestListFiles:
			Client->RequestServerList(true, false, g_pOptions->GetMatchMode() == Options::mmRegEx ? g_pOptions->GetEditQueueText() : NULL);
			break;

		case Options::opClientRequestListGroups:
			Client->RequestServerList(false, true, g_pOptions->GetMatchMode() == Options::mmRegEx ? g_pOptions->GetEditQueueText() : NULL);
			break;

		case Options::opClientRequestListStatus:
			Client->RequestServerList(false, false, NULL);
			break;

		case Options::opClientRequestDownloadPause:
			Client->RequestServerPauseUnpause(true, eRemotePauseUnpauseActionDownload);
			break;

		case Options::opClientRequestDownloadUnpause:
			Client->RequestServerPauseUnpause(false, eRemotePauseUnpauseActionDownload);
			break;

		case Options::opClientRequestSetRate:
			Client->RequestServerSetDownloadRate(g_pOptions->GetSetRate());
			break;

		case Options::opClientRequestDumpDebug:
			Client->RequestServerDumpDebug();
			break;

		case Options::opClientRequestEditQueue:
			Client->RequestServerEditQueue((DownloadQueue::EEditAction)g_pOptions->GetEditQueueAction(),
				g_pOptions->GetEditQueueOffset(), g_pOptions->GetEditQueueText(),
				g_pOptions->GetEditQueueIDList(), g_pOptions->GetEditQueueIDCount(),
				g_pOptions->GetEditQueueNameList(), (eRemoteMatchMode)g_pOptions->GetMatchMode());
			break;

		case Options::opClientRequestLog:
			Client->RequestServerLog(g_pOptions->GetLogLines());
			break;

		case Options::opClientRequestShutdown:
			Client->RequestServerShutdown();
			break;

		case Options::opClientRequestReload:
			Client->RequestServerReload();
			break;

		case Options::opClientRequestDownload:
			Client->RequestServerDownload(g_pOptions->GetArgFilename(), g_pOptions->GetAddCategory(), g_pOptions->GetAddTop(), g_pOptions->GetAddPaused(), g_pOptions->GetAddPriority());
			break;

		case Options::opClientRequestVersion:
			Client->RequestServerVersion();
			break;

		case Options::opClientRequestPostQueue:
			Client->RequestPostQueue();
			break;

		case Options::opClientRequestWriteLog:
			Client->RequestWriteLog(g_pOptions->GetWriteLogKind(), g_pOptions->GetLastArg());
			break;

		case Options::opClientRequestScanAsync:
			Client->RequestScan(false);
			break;

		case Options::opClientRequestScanSync:
			Client->RequestScan(true);
			break;

		case Options::opClientRequestPostPause:
			Client->RequestServerPauseUnpause(true, eRemotePauseUnpauseActionPostProcess);
			break;

		case Options::opClientRequestPostUnpause:
			Client->RequestServerPauseUnpause(false, eRemotePauseUnpauseActionPostProcess);
			break;

		case Options::opClientRequestScanPause:
			Client->RequestServerPauseUnpause(true, eRemotePauseUnpauseActionScan);
			break;

		case Options::opClientRequestScanUnpause:
			Client->RequestServerPauseUnpause(false, eRemotePauseUnpauseActionScan);
			break;

		case Options::opClientRequestHistory:	 
			Client->RequestHistory();	 
			break;

		case Options::opClientRequestDownloadUrl:
			Client->RequestServerDownloadUrl(g_pOptions->GetLastArg(), g_pOptions->GetAddNZBFilename(), g_pOptions->GetAddCategory(), g_pOptions->GetAddTop(), g_pOptions->GetAddPaused(), g_pOptions->GetAddPriority());
			break;

		case Options::opClientNoOperation:
			break;
	}

	delete Client;
}

void ExitProc()
{
	if (!g_bReloading)
	{
		info("Stopping, please wait...");
	}
	if (g_pOptions->GetRemoteClientMode())
	{
		if (g_pFrontend)
		{
			debug("Stopping Frontend");
			g_pFrontend->Stop();
		}
	}
	else
	{
		if (g_pQueueCoordinator)
		{
			debug("Stopping QueueCoordinator");
			g_pQueueCoordinator->Stop();
			g_pUrlCoordinator->Stop();
			g_pPrePostProcessor->Stop();
			g_pFeedCoordinator->Stop();
			g_pArticleCache->Stop();
		}
	}
}

void Reload()
{
	g_bReloading = true;
	info("Reloading...");
	ExitProc();
}

#ifndef WIN32
#ifdef DEBUG
typedef void(*sighandler)(int);
std::vector<sighandler> SignalProcList;
#endif

/*
 * Signal handler
 */
void SignalProc(int iSignal)
{
	switch (iSignal)
	{
		case SIGINT:
			signal(SIGINT, SIG_DFL);   // Reset the signal handler
			ExitProc();
			break;

		case SIGTERM:
			signal(SIGTERM, SIG_DFL);   // Reset the signal handler
			ExitProc();
			break;

		case SIGCHLD:
			// ignoring
			break;

#ifdef DEBUG
		case SIGSEGV:
			signal(SIGSEGV, SIG_DFL);   // Reset the signal handler
			PrintBacktrace();
			break;
#endif
	}
}

void InstallSignalHandlers()
{
	signal(SIGINT, SignalProc);
	signal(SIGTERM, SignalProc);
	signal(SIGPIPE, SIG_IGN);
#ifdef DEBUG
	signal(SIGSEGV, SignalProc);
#endif
#ifdef SIGCHLD_HANDLER
    // it could be necessary on some systems to activate a handler for SIGCHLD
    // however it make troubles on other systems and is deactivated by default
	signal(SIGCHLD, SignalProc);
#endif
}

void PrintBacktrace()
{
#ifdef HAVE_BACKTRACE
	printf("Segmentation fault, tracing...\n");
	
	void *array[100];
	size_t size;
	char **strings;
	size_t i;

	size = backtrace(array, 100);
	strings = backtrace_symbols(array, size);

	// first trace to screen
	printf("Obtained %zd stack frames\n", size);
	for (i = 0; i < size; i++)
	{
		printf("%s\n", strings[i]);
	}

	// then trace to log
	error("Segmentation fault, tracing...");
	error("Obtained %zd stack frames", size);
	for (i = 0; i < size; i++)
	{
		error("%s", strings[i]);
	}

	free(strings);
#else
	error("Segmentation fault");
#endif
}

#ifdef DEBUG
void MakeSegFault()
{
	char* N = NULL;
	strcpy(N, "");
}
#endif

#ifdef HAVE_SYS_PRCTL_H
/**
* activates the creation of core-files
*/
void EnableDumpCore()
{
	rlimit rlim;
	rlim.rlim_cur= RLIM_INFINITY;
	rlim.rlim_max= RLIM_INFINITY;
	setrlimit(RLIMIT_CORE, &rlim);
	prctl(PR_SET_DUMPABLE, 1);
}
#endif
#endif

void Cleanup()
{
	debug("Cleaning up global objects");

	debug("Deleting UrlCoordinator");
	delete g_pUrlCoordinator;
	g_pUrlCoordinator = NULL;
	debug("UrlCoordinator deleted");

	debug("Deleting RemoteServer");
	delete g_pRemoteServer;
	g_pRemoteServer = NULL;
	debug("RemoteServer deleted");

	debug("Deleting RemoteSecureServer");
	delete g_pRemoteSecureServer;
	g_pRemoteSecureServer = NULL;
	debug("RemoteSecureServer deleted");

	debug("Deleting PrePostProcessor");
	delete g_pPrePostProcessor;
	g_pPrePostProcessor = NULL;
	delete g_pScanner;
	g_pScanner = NULL;
	debug("PrePostProcessor deleted");

	debug("Deleting HistoryCoordinator");
	delete g_pHistoryCoordinator;
	g_pHistoryCoordinator = NULL;
	debug("HistoryCoordinator deleted");

	debug("Deleting DupeCoordinator");
	delete g_pDupeCoordinator;
	g_pDupeCoordinator = NULL;
	debug("DupeCoordinator deleted");

	debug("Deleting Frontend");
	delete g_pFrontend;
	g_pFrontend = NULL;
	debug("Frontend deleted");

	debug("Deleting QueueCoordinator");
	delete g_pQueueCoordinator;
	g_pQueueCoordinator = NULL;
	debug("QueueCoordinator deleted");

	debug("Deleting DiskState");
	delete g_pDiskState;
	g_pDiskState = NULL;
	debug("DiskState deleted");

	debug("Deleting Options");
	if (g_pOptions)
	{
		if (g_pOptions->GetDaemonMode() && !g_bReloading)
		{
			info("Deleting lock file");
			remove(g_pOptions->GetLockFile());
		}
		delete g_pOptions;
		g_pOptions = NULL;
	}
	debug("Options deleted");

	debug("Deleting ServerPool");
	delete g_pServerPool;
	g_pServerPool = NULL;
	debug("ServerPool deleted");

	debug("Deleting Scheduler");
	delete g_pScheduler;
	g_pScheduler = NULL;
	debug("Scheduler deleted");

	debug("Deleting FeedCoordinator");
	delete g_pFeedCoordinator;
	g_pFeedCoordinator = NULL;
	debug("FeedCoordinator deleted");

	debug("Deleting ArticleCache");
	delete g_pArticleCache;
	g_pArticleCache = NULL;
	debug("ArticleCache deleted");

	debug("Deleting Maintenance");
	delete g_pMaintenance;
	g_pMaintenance = NULL;
	debug("Maintenance deleted");

	debug("Deleting StatMeter");
	delete g_pStatMeter;
	g_pStatMeter = NULL;
	debug("StatMeter deleted");

	if (!g_bReloading)
	{
		Connection::Final();
		Thread::Final();
	}

	debug("Global objects cleaned up");

	delete g_pLog;
	g_pLog = NULL;
}

#ifndef WIN32
void Daemonize()
{
	int i, lfp;
	char str[10];
	if (getppid() == 1) return; /* already a daemon */
	i = fork();
	if (i < 0) exit(1); /* fork error */
	if (i > 0) exit(0); /* parent exits */
	/* child (daemon) continues */
	setsid(); /* obtain a new process group */
	for (i = getdtablesize();i >= 0;--i) close(i); /* close all descriptors */
	i = open("/dev/null", O_RDWR); dup(i); dup(i); /* handle standart I/O */
	chdir(g_pOptions->GetDestDir()); /* change running directory */
	lfp = open(g_pOptions->GetLockFile(), O_RDWR | O_CREAT, 0640);
	if (lfp < 0) exit(1); /* can not open */
	if (lockf(lfp, F_TLOCK, 0) < 0) exit(0); /* can not lock */

	/* Drop user if there is one, and we were run as root */
	if ( getuid() == 0 || geteuid() == 0 )
	{
		struct passwd *pw = getpwnam(g_pOptions->GetDaemonUsername());
		if (pw)
		{
			fchown(lfp, pw->pw_uid, pw->pw_gid); /* change owner of lock file  */
			setgroups( 0, (const gid_t*) 0 ); /* Set aux groups to null. */
			setgid(pw->pw_gid); /* Set primary group. */
			/* Try setting aux groups correctly - not critical if this fails. */
			initgroups( g_pOptions->GetDaemonUsername(),pw->pw_gid); 
			/* Finally, set uid. */
			setuid(pw->pw_uid);
		}
	}

	/* first instance continues */
	sprintf(str, "%d\n", getpid());
	write(lfp, str, strlen(str)); /* record pid to lockfile */
	signal(SIGCHLD, SIG_IGN); /* ignore child */
	signal(SIGTSTP, SIG_IGN); /* ignore tty signals */
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
}
#endif

#ifndef DISABLE_PARCHECK
class NullStreamBuf : public std::streambuf
{
public:
	int sputc ( char c ) { return (int) c; }
} NullStreamBufInstance;

void DisableCout()
{
	// libpar2 prints messages to c++ standard output stream (std::cout).
	// However we do not want these messages to be printed.
	// Since we do not use std::cout in nzbget we just disable it.
	std::cout.rdbuf(&NullStreamBufInstance);
}
#endif
