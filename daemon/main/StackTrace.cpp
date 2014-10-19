/*
 *  This file is part of nzbget
 *
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
#include <stdio.h>

#ifdef WIN32
#include <dbghelp.h>
#else
#include <unistd.h>
#include <sys/resource.h>
#include <signal.h>
#endif
#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif
#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#endif

#include "nzbget.h"
#include "Log.h"
#include "Options.h"
#include "StackTrace.h"

extern Options* g_pOptions;
extern void ExitProc();

#ifdef WIN32

#ifdef DEBUG

void PrintBacktrace(PCONTEXT pContext)
{
	HANDLE hProcess = GetCurrentProcess();
	HANDLE hThread = GetCurrentThread();

	char szAppDir[MAX_PATH + 1];
	GetModuleFileName(NULL, szAppDir, sizeof(szAppDir));
	char* end = strrchr(szAppDir, PATH_SEPARATOR);
	if (end) *end = '\0';

	SymSetOptions(SymGetOptions() | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS);

	if (!SymInitialize(hProcess, szAppDir, TRUE))
	{
		warn("Could not obtain detailed exception information: SymInitialize failed");
		return;
	}

	const int MAX_NAMELEN = 1024;
	IMAGEHLP_SYMBOL64* pSym = (IMAGEHLP_SYMBOL64 *) malloc(sizeof(IMAGEHLP_SYMBOL64) + MAX_NAMELEN);
	memset(pSym, 0, sizeof(IMAGEHLP_SYMBOL64) + MAX_NAMELEN);
	pSym->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
	pSym->MaxNameLength = MAX_NAMELEN;

	IMAGEHLP_LINE64 ilLine;
	memset(&ilLine, 0, sizeof(ilLine));
	ilLine.SizeOfStruct = sizeof(ilLine);

	STACKFRAME64 sfStackFrame;
	memset(&sfStackFrame, 0, sizeof(sfStackFrame));
	DWORD imageType;
#ifdef _M_IX86
	imageType = IMAGE_FILE_MACHINE_I386;
	sfStackFrame.AddrPC.Offset = pContext->Eip;
	sfStackFrame.AddrPC.Mode = AddrModeFlat;
	sfStackFrame.AddrFrame.Offset = pContext->Ebp;
	sfStackFrame.AddrFrame.Mode = AddrModeFlat;
	sfStackFrame.AddrStack.Offset = pContext->Esp;
	sfStackFrame.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
	imageType = IMAGE_FILE_MACHINE_AMD64;
	sfStackFrame.AddrPC.Offset = pContext->Rip;
	sfStackFrame.AddrPC.Mode = AddrModeFlat;
	sfStackFrame.AddrFrame.Offset = pContext->Rsp;
	sfStackFrame.AddrFrame.Mode = AddrModeFlat;
	sfStackFrame.AddrStack.Offset = pContext->Rsp;
	sfStackFrame.AddrStack.Mode = AddrModeFlat;
#else
	warn("Could not obtain detailed exception information: platform not supported");
	return;
#endif

	for (int frameNum = 0; ; frameNum++)
	{
		if (frameNum > 1000)
		{
			warn("Endless stack, abort tracing");
			return;
		}

		if (!StackWalk64(imageType, hProcess, hThread, &sfStackFrame, pContext, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
		{
			warn("Could not obtain detailed exception information: StackWalk64 failed");
			return;
		}

		DWORD64 dwAddr = sfStackFrame.AddrPC.Offset;
		char szSymName[1024];
		char szSrcFileName[1024];
		int iLineNumber = 0;

		DWORD64 dwSymbolDisplacement;
		if (SymGetSymFromAddr64(hProcess, dwAddr, &dwSymbolDisplacement, pSym))
		{
			UnDecorateSymbolName(pSym->Name, szSymName, sizeof(szSymName), UNDNAME_COMPLETE);
			szSymName[sizeof(szSymName) - 1] = '\0';
		}
		else
		{
			strncpy(szSymName, "<symbol not available>", sizeof(szSymName));
		}

		DWORD dwLineDisplacement;
		if (SymGetLineFromAddr64(hProcess, dwAddr, &dwLineDisplacement, &ilLine))
		{
			iLineNumber = ilLine.LineNumber;
			char* szUseFileName = ilLine.FileName;
			char* szRoot = strstr(szUseFileName, "\\daemon\\");
			if (szRoot)
			{
				szUseFileName = szRoot;
			}
			strncpy(szSrcFileName, szUseFileName, sizeof(szSrcFileName));
			szSrcFileName[sizeof(szSrcFileName) - 1] = '\0';
		}
		else
		{
			strncpy(szSrcFileName, "<filename not available>", sizeof(szSymName));
		}

		info("%s (%i) : %s", szSrcFileName, iLineNumber, szSymName);

		if (sfStackFrame.AddrReturn.Offset == 0)
		{
			break;
		}
	}
}
#endif

LONG __stdcall ExceptionFilter(EXCEPTION_POINTERS* pExPtrs)
{
	error("Unhandled Exception: code: 0x%8.8X, flags: %d, address: 0x%8.8X",
		pExPtrs->ExceptionRecord->ExceptionCode,
		pExPtrs->ExceptionRecord->ExceptionFlags,
		pExPtrs->ExceptionRecord->ExceptionAddress);

#ifdef DEBUG
	PrintBacktrace(pExPtrs->ContextRecord);
#else
	 info("Detailed exception information can be printed by debug version of NZBGet (available from download page)");
#endif

	ExitProcess(-1);
	return EXCEPTION_CONTINUE_SEARCH;
}

void InstallErrorHandler()
{
	SetUnhandledExceptionFilter(ExceptionFilter);
}

#else

#ifdef DEBUG
typedef void(*sighandler)(int);
std::vector<sighandler> SignalProcList;
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

void InstallErrorHandler()
{
#ifdef HAVE_SYS_PRCTL_H
	if (g_pOptions->GetDumpCore())
	{
		EnableDumpCore();
	}
#endif

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

#endif

#ifdef DEBUG
class SegFault
{
public:
	void DoSegFault()
	{
		char* N = NULL;
		strcpy(N, "");
	}
};

void TestSegFault()
{
	SegFault s;
	s.DoSegFault();
}
#endif
