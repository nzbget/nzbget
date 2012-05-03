/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2010 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include <config.h>
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
#include "RemoteServer.h"
#include "RemoteClient.h"
#include "MessageBase.h"
#include "DiskState.h"
#include "PrePostProcessor.h"
#include "ParChecker.h"
#include "Scheduler.h"
#include "Util.h"
#ifdef WIN32
#include "NTService.h"
#endif

// Prototypes
void Run();
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
RemoteServer* g_pRemoteServer = NULL;
DownloadSpeedMeter* g_pDownloadSpeedMeter = NULL;
DownloadQueueHolder* g_pDownloadQueueHolder = NULL;
Log* g_pLog = NULL;
PrePostProcessor* g_pPrePostProcessor = NULL;
DiskState* g_pDiskState = NULL;
Scheduler* g_pScheduler = NULL;
char* (*szEnvironmentVariables)[] = NULL;

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

	g_pLog = new Log();

	debug("nzbget %s", Util::VersionRevision());

	g_pServerPool = new ServerPool();
	g_pScheduler = new Scheduler();
	Thread::Init();

	debug("Reading options");
	g_pOptions = new Options(argc, argv);
	szEnvironmentVariables = (char*(*)[])argp;

#ifndef WIN32
	if (g_pOptions->GetUMask() < 01000)
	{
		/* set newly created file permissions */
		umask(g_pOptions->GetUMask());
	}
#endif
	
	if (g_pOptions->GetServerMode() && g_pOptions->GetCreateLog() && g_pOptions->GetResetLog())
	{
		debug("Deleting old log-file");
		g_pLog->ResetLog();
	}

	g_pLog->InitOptions();

	if (g_pOptions->GetDaemonMode())
	{
#ifdef WIN32
		info("nzbget %s service-mode", Util::VersionRevision());
#else
		Daemonize();
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

	if (!g_pOptions->GetRemoteClientMode())
	{
		g_pServerPool->InitConnections();
#ifdef DEBUG
		g_pServerPool->LogDebugInfo();
#endif
	}

#ifdef WIN32
	if (g_pOptions->GetDaemonMode())
	{
		StartService(Run);
		return 0;
	}
#else
#ifdef HAVE_SYS_PRCTL_H
	if (g_pOptions->GetDumpCore())
	{
		EnableDumpCore();
	}
#endif
#endif

	Run();

#ifdef WIN32
#ifdef _DEBUG
	_CrtDumpMemoryLeaks();
#endif
#endif

	return 0;
}

void Run()
{
#ifndef WIN32
	InstallSignalHandlers();
#ifdef DEBUG
	if (g_pOptions->GetTestBacktrace())
	{
		MakeSegFault();
	}
#endif
#endif
	
	Connection::Init(!g_pOptions->GetRemoteClientMode() &&
		(g_pOptions->GetClientOperation() == Options::opClientNoOperation));

	// client request
	if (g_pOptions->GetClientOperation() != Options::opClientNoOperation)
	{
		ProcessClientRequest();
		Cleanup();
		return;
	}

	// Create the queue coordinator
	if (!g_pOptions->GetRemoteClientMode())
	{                                    
		g_pQueueCoordinator = new QueueCoordinator();
		g_pDownloadSpeedMeter = g_pQueueCoordinator;
		g_pDownloadQueueHolder = g_pQueueCoordinator;
	}

	// Setup the network-server
	if (g_pOptions->GetServerMode())
	{
		g_pRemoteServer = new RemoteServer();
		g_pRemoteServer->Start();
	}

	// Creating PrePostProcessor
	if (!g_pOptions->GetRemoteClientMode())
	{
		g_pPrePostProcessor = new PrePostProcessor();
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
			NZBFile* pNZBFile = NZBFile::CreateFromFile(g_pOptions->GetArgFilename(), g_pOptions->GetCategory() ? g_pOptions->GetCategory() : "");
			if (!pNZBFile)
			{
				abort("FATAL ERROR: Parsing NZB-document %s failed\n\n", g_pOptions->GetArgFilename() ? g_pOptions->GetArgFilename() : "N/A");
				return;
			}
			g_pQueueCoordinator->AddNZBFileToQueue(pNZBFile, false);
			delete pNZBFile;
		}

		if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
		{
			g_pDiskState = new DiskState();
		}

		g_pQueueCoordinator->Start();
		g_pPrePostProcessor->Start();

		// enter main program-loop
		while (g_pQueueCoordinator->IsRunning() || g_pPrePostProcessor->IsRunning())
		{
			if (!g_pOptions->GetServerMode() && !g_pQueueCoordinator->HasMoreJobs() && !g_pPrePostProcessor->HasMoreJobs())
			{
				// Standalone-mode: download completed
				if (!g_pQueueCoordinator->IsStopped())
				{
					g_pQueueCoordinator->Stop();
				}
				if (!g_pPrePostProcessor->IsStopped())
				{
					g_pPrePostProcessor->Stop();
				}
			}
			usleep(100 * 1000);
		}

		// main program-loop is terminated
		debug("QueueCoordinator stopped");
		debug("PrePostProcessor stopped");
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
			Client->RequestServerList(true, false);
			break;

		case Options::opClientRequestListGroups:
			Client->RequestServerList(false, true);
			break;

		case Options::opClientRequestListStatus:
			Client->RequestServerList(false, false);
			break;

		case Options::opClientRequestDownloadPause:
			Client->RequestServerPauseUnpause(true, eRemotePauseUnpauseActionDownload);
			break;

		case Options::opClientRequestDownloadUnpause:
			Client->RequestServerPauseUnpause(false, eRemotePauseUnpauseActionDownload);
			break;

		case Options::opClientRequestDownload2Pause:
			Client->RequestServerPauseUnpause(true, eRemotePauseUnpauseActionDownload2);
			break;

		case Options::opClientRequestDownload2Unpause:
			Client->RequestServerPauseUnpause(false, eRemotePauseUnpauseActionDownload2);
			break;

		case Options::opClientRequestSetRate:
			Client->RequestServerSetDownloadRate(g_pOptions->GetSetRate());
			break;

		case Options::opClientRequestDumpDebug:
			Client->RequestServerDumpDebug();
			break;

		case Options::opClientRequestEditQueue:
			Client->RequestServerEditQueue((eRemoteEditAction)g_pOptions->GetEditQueueAction(), g_pOptions->GetEditQueueOffset(),
				g_pOptions->GetEditQueueText(), g_pOptions->GetEditQueueIDList(), g_pOptions->GetEditQueueIDCount(), true);
			break;

		case Options::opClientRequestLog:
			Client->RequestServerLog(g_pOptions->GetLogLines());
			break;

		case Options::opClientRequestShutdown:
			Client->RequestServerShutdown();
			break;

		case Options::opClientRequestDownload:
			Client->RequestServerDownload(g_pOptions->GetArgFilename(), g_pOptions->GetCategory(), g_pOptions->GetAddTop());
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

		case Options::opClientRequestScan:
			Client->RequestScan();
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

		case Options::opClientNoOperation:
			break;
	}

	delete Client;
}

void ExitProc()
{
	info("Stopping, please wait...");
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
			g_pPrePostProcessor->Stop();
		}
	}
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

	debug("Deleting QueueCoordinator");
	if (g_pQueueCoordinator)
	{
		delete g_pQueueCoordinator;
		g_pQueueCoordinator = NULL;
	}
	debug("QueueCoordinator deleted");

	debug("Deleting RemoteServer");
	if (g_pRemoteServer)
	{
		delete g_pRemoteServer;
		g_pRemoteServer = NULL;
	}
	debug("RemoteServer deleted");

	debug("Deleting PrePostProcessor");
	if (g_pPrePostProcessor)
	{
		delete g_pPrePostProcessor;
		g_pPrePostProcessor = NULL;
	}
	debug("PrePostProcessor deleted");

	debug("Deleting Frontend");
	if (g_pFrontend)
	{
		delete g_pFrontend;
		g_pFrontend = NULL;
	}
	debug("Frontend deleted");

	debug("Deleting DiskState");
	if (g_pDiskState)
	{
		delete g_pDiskState;
		g_pDiskState = NULL;
	}
	debug("DiskState deleted");

	debug("Deleting Options");
	if (g_pOptions)
	{
		if (g_pOptions->GetDaemonMode())
		{
			info("Deleting lock file");
			remove(g_pOptions->GetLockFile());
		}
		delete g_pOptions;
		g_pOptions = NULL;
	}
	debug("Options deleted");

	debug("Deleting ServerPool");
	if (g_pServerPool)
	{
		delete g_pServerPool;
		g_pServerPool = NULL;
	}
	debug("ServerPool deleted");

	debug("Deleting Scheduler");
	if (g_pScheduler)
	{
		delete g_pScheduler;
		g_pScheduler = NULL;
	}
	debug("Scheduler deleted");

	Connection::Final();

	Thread::Final();

	debug("Global objects cleaned up");

	if (g_pLog)
	{
		delete g_pLog;
		g_pLog = NULL;
	}
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
		struct passwd *pw = getpwnam(g_pOptions->GetDaemonUserName());
		if (pw)
		{
			fchown(lfp, pw->pw_uid, pw->pw_gid); /* change owner of lock file  */
			setgroups( 0, (const gid_t*) 0 ); /* Set aux groups to null. */
			setgid(pw->pw_gid); /* Set primary group. */
			/* Try setting aux groups correctly - not critical if this fails. */
			initgroups( g_pOptions->GetDaemonUserName(),pw->pw_gid); 
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
