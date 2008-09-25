/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2008 Andrei Prygounkov <hugbug@users.sourceforge.net>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include <sys/prctl.h>
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
void EnableDumpCore();
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
NZBInfoLocker* g_pNZBInfoLocker = NULL;
Log* g_pLog = NULL;
PrePostProcessor* g_pPrePostProcessor = NULL;
DiskState* g_pDiskState = NULL;
Scheduler* g_pScheduler = NULL;


/*
 * Main loop
 */
int main(int argc, char *argv[])
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

#ifdef WIN32
	_set_fmode(_O_BINARY);
	InstallUninstallServiceCheck(argc, argv);
#endif

#ifndef DISABLE_PARCHECK
	DisableCout();
#endif

	// Init options & get the name of the .nzb file
	g_pLog = new Log();
	g_pServerPool = new ServerPool();
	g_pScheduler = new Scheduler();
	debug("Options parsing");
	g_pOptions = new Options(argc, argv);

#ifndef WIN32
	if (g_pOptions->GetUMask() < 01000)
	{
		/* set newly created file permissions */
		umask(g_pOptions->GetUMask());
	}
#endif
	
	if (g_pOptions->GetServerMode() && g_pOptions->GetCreateLog() && g_pOptions->GetResetLog())
	{
		debug("deleting old log-file");
		g_pLog->ResetLog();
	}

	if (g_pOptions->GetDaemonMode())
	{
#ifdef WIN32
		info("nzbget service-mode");
		StartService(Run);
		return 0;
#else
		Daemonize();
		info("nzbget daemon-mode");
#endif
	}
	else if (g_pOptions->GetServerMode())
	{
		info("nzbget server-mode");
	}
	else if (g_pOptions->GetRemoteClientMode())
	{
		info("nzbget remote-mode");
	}

#ifndef WIN32
	if (g_pOptions->GetDumpCore())
	{
		EnableDumpCore();
	}
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
	
	Thread::Init();
	Connection::Init(g_pOptions->GetTLS() && g_pOptions->GetServerMode());

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
		g_pNZBInfoLocker = g_pQueueCoordinator;
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
		if (!g_pOptions->GetServerMode() && !g_pQueueCoordinator->AddFileToQueue(g_pOptions->GetArgFilename(), g_pOptions->GetCategory() ? g_pOptions->GetCategory() : ""))
		{
			abort("FATAL ERROR: Parsing NZB-document %s failed!!\n\n", g_pOptions->GetArgFilename() ? g_pOptions->GetArgFilename() : "N/A");
			return;
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

	if (g_pOptions->GetClientOperation() == Options::opClientRequestList)
	{
		Client->RequestServerList();
	}
	else if (g_pOptions->GetClientOperation() == Options::opClientRequestPause)
	{
		Client->RequestServerPauseUnpause(true);
	}
	else if (g_pOptions->GetClientOperation() == Options::opClientRequestUnpause)
	{
		Client->RequestServerPauseUnpause(false);
	}
	else if (g_pOptions->GetClientOperation() == Options::opClientRequestSetRate)
	{
		Client->RequestServerSetDownloadRate(g_pOptions->GetSetRate());
	}
	else if (g_pOptions->GetClientOperation() == Options::opClientRequestDumpDebug)
	{
		Client->RequestServerDumpDebug();
	}
	else if (g_pOptions->GetClientOperation() == Options::opClientRequestEditQueue)
	{
		Client->RequestServerEditQueue(g_pOptions->GetEditQueueAction(), g_pOptions->GetEditQueueOffset(),
			g_pOptions->GetEditQueueText(), g_pOptions->GetEditQueueIDList(), g_pOptions->GetEditQueueIDCount(), true);
	}
	else if (g_pOptions->GetClientOperation() == Options::opClientRequestLog)
	{
		Client->RequestServerLog(g_pOptions->GetLogLines());
	}
	else if (g_pOptions->GetClientOperation() == Options::opClientRequestShutdown)
	{
		Client->RequestServerShutdown();
	}
	else if (g_pOptions->GetClientOperation() == Options::opClientRequestDownload)
	{
		Client->RequestServerDownload(g_pOptions->GetArgFilename(), g_pOptions->GetCategory(), g_pOptions->GetAddTop());
	}
	else if (g_pOptions->GetClientOperation() == Options::opClientRequestVersion)
	{
		Client->RequestServerVersion();
	}
	else if (g_pOptions->GetClientOperation() == Options::opClientRequestPostQueue)
	{
		Client->RequestPostQueue();
	}
	else if (g_pOptions->GetClientOperation() == Options::opClientRequestWriteLog)
	{
		Client->RequestWriteLog(g_pOptions->GetWriteLogKind(), g_pOptions->GetLastArg());
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

#ifdef DEBUG
		case SIGPIPE:
			// ignoring
			break;
			
		case SIGCHLD:
			// ignoring
			break;
			
		case SIGSEGV:
			signal(SIGSEGV, SIG_DFL);   // Reset the signal handler
			PrintBacktrace();
			break;
		
		default:
			// printf("Signal %i received\n", iSignal);
			if (SignalProcList[iSignal - 1])
			{
				SignalProcList[iSignal - 1](iSignal);
			}
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
	SignalProcList.clear();
	for (int i = 1; i <= 32; i++)
	{
		SignalProcList.push_back((sighandler)signal(i, SignalProc));
	}
	signal(SIGWINCH, SIG_DFL);
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

	Thread::Final();
	Connection::Final();

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
