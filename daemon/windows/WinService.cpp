/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "Log.h"
#include "WinService.h"

extern void ExitProc();
RunProc Run = nullptr;

char* strServiceName = "NZBGet";
SERVICE_STATUS_HANDLE nServiceStatusHandle;
DWORD nServiceCurrentStatus;

BOOL UpdateServiceStatus(DWORD dwCurrentState, DWORD dwWaitHint)
{
	BOOL success;
	SERVICE_STATUS nServiceStatus;
	nServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	nServiceStatus.dwCurrentState = dwCurrentState;
	if (dwCurrentState == SERVICE_START_PENDING)
	{
		nServiceStatus.dwControlsAccepted = 0;
	}
	else
	{
		nServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	}
	nServiceStatus.dwWin32ExitCode = NO_ERROR;
	nServiceStatus.dwServiceSpecificExitCode = 0;
	nServiceStatus.dwCheckPoint = 0;
	nServiceStatus.dwWaitHint = dwWaitHint;

	success = SetServiceStatus(nServiceStatusHandle, &nServiceStatus);

	return success;
}

void ServiceCtrlHandler(DWORD nControlCode)
{
	switch(nControlCode)
	{
	case SERVICE_CONTROL_SHUTDOWN:
	case SERVICE_CONTROL_STOP:
		nServiceCurrentStatus = SERVICE_STOP_PENDING;
		UpdateServiceStatus(SERVICE_STOP_PENDING, 10000);
		ExitProc();
		return;
	default:
		break;
	}
	UpdateServiceStatus(nServiceCurrentStatus, 0);
}

void ServiceMain(DWORD argc, LPTSTR *argv)
{
	BOOL success;
	nServiceStatusHandle = RegisterServiceCtrlHandler(strServiceName,
		(LPHANDLER_FUNCTION)ServiceCtrlHandler);
	if(!nServiceStatusHandle)
	{
		return;
	}
	success = UpdateServiceStatus(SERVICE_START_PENDING, 10000);
	if(!success)
	{
		return;
	}
	nServiceCurrentStatus=SERVICE_RUNNING;
	success=UpdateServiceStatus(SERVICE_RUNNING, 0);
	if(!success)
	{
		return;
	}

	Run();

	UpdateServiceStatus(SERVICE_STOPPED, 0);
}

void StartService(RunProc RunProcPtr)
{
	Run = RunProcPtr;

	SERVICE_TABLE_ENTRY servicetable[]=
	{
		{strServiceName,(LPSERVICE_MAIN_FUNCTION)ServiceMain},
		{nullptr,nullptr}
	};
	BOOL success = StartServiceCtrlDispatcher(servicetable);
	if(!success)
	{
		error("Could not start service");
	}
}

void InstallService(int argc, char *argv[])
{
	SC_HANDLE scm = OpenSCManager(0,0,SC_MANAGER_CREATE_SERVICE);
	if(!scm)
	{
		printf("Could not install service\n");
		return;
	}

	char exeName[1024];
	GetModuleFileName(nullptr, exeName, 1024);
	exeName[1024-1] = '\0';

	BString<1024> cmdLine("%s -D", exeName);

	SC_HANDLE hService = CreateService(scm, strServiceName,
		strServiceName,
		SERVICE_ALL_ACCESS,SERVICE_WIN32_OWN_PROCESS,SERVICE_DEMAND_START,
		SERVICE_ERROR_NORMAL,
		cmdLine,
		0,0,0,0,0);
	if(!hService)
	{
		CloseServiceHandle(scm);
		printf("Could not install service\n");
		return;
	}
	CloseServiceHandle(hService);
	CloseServiceHandle(scm);
	printf("Service \"%s\" sucessfully installed\n", strServiceName);
}

void UnInstallService()
{
	BOOL success;
	SC_HANDLE scm = OpenSCManager(0,0,SC_MANAGER_CONNECT);
	if(!scm)
	{
		printf("Could not uninstall service\n");
		return;
	}

	SC_HANDLE hService = OpenService(scm, strServiceName, STANDARD_RIGHTS_REQUIRED);
	if(!hService)
	{
		CloseServiceHandle(scm);
		printf("Could not uninstall service\n");
		return;
	}

	success = DeleteService(hService);
	if(!success)
	{
		error("Could not uninstall service");
	}
	CloseServiceHandle(hService);
	CloseServiceHandle(scm);
	printf("Service \"%s\" sucessfully uninstalled\n", strServiceName);
}

void InstallUninstallServiceCheck(int argc, char *argv[])
{
	if (argc > 1 && (!strcasecmp(argv[1], "/install") || !strcasecmp(argv[1], "-install")))
	{
		InstallService(argc, argv);
		exit(0);
	}
	else if (argc > 1 && (!strcasecmp(argv[1], "/uninstall") || !strcasecmp(argv[1], "/remove") ||
		!strcasecmp(argv[1], "-uninstall") || !strcasecmp(argv[1], "-remove")))
	{
		UnInstallService();
		exit(0);
	}
}

bool IsServiceRunning()
{
	SC_HANDLE scm = OpenSCManager(0, 0, 0);
	if (!scm)
	{
		return false;
	}

	SC_HANDLE hService = OpenService(scm, "NZBGet", SERVICE_QUERY_STATUS);
	SERVICE_STATUS ServiceStatus;
	bool running = false;
	if (hService && QueryServiceStatus(hService, &ServiceStatus))
	{
		running = ServiceStatus.dwCurrentState != SERVICE_STOPPED;
	}

	CloseServiceHandle(scm);

	return running;
}
