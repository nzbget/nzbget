/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2014-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#define SKIP_DEFAULT_WINDOWS_HEADERS
#include "win32.h"

#include <windows.h>
#include <wincon.h>
#include <shellapi.h>
#include <shlobj.h>
#include <winreg.h>

#include <string.h>
#include <stdio.h>

#include "nzbget.h"
#include "Log.h"
#include "Options.h"
#include "Util.h"
#include "FeedCoordinator.h"
#include "StatMeter.h"
#include "WinConsole.h"
#include "NTService.h"
#include "resource.h"

extern Options* g_pOptions;
extern char* (*g_szArguments)[];
extern int g_iArgumentCount;
extern void ExitProc();
extern void Reload();
extern WinConsole* g_pWinConsole;
extern FeedCoordinator* g_pFeedCoordinator;
extern StatMeter* g_pStatMeter;

#define UM_TRAYICON (WM_USER + 1)
#define UM_QUIT (WM_USER + 2)
#define UM_SHOWWEBUI (WM_USER + 3)
#define UM_PREFSCHANGED (WM_USER + 4)

#pragma comment(linker, \
  "\"/manifestdependency:type='Win32' "\
  "name='Microsoft.Windows.Common-Controls' version='6.0.0.0' "\
  "processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

bool bMayStartBrowser = true;

BOOL WINAPI WinConsole::ConsoleCtrlHandler(DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
		case CTRL_C_EVENT:
		case CTRL_BREAK_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			ExitProc();
			while (g_pWinConsole)
			{
				usleep(20 * 1000);
			}
			return TRUE;

		default:
			return FALSE;
	}
}

WinConsole::WinConsole()
{
	m_pInitialArguments = NULL;
	m_iInitialArgumentCount = 0;
	m_pDefaultArguments = NULL;
	m_pNidIcon = NULL;
	m_bModal = false;
	m_bAutostart = false;
	m_bTray = true;
	m_bConsole = false;
	m_bWebUI = true;
	m_bAutoParam = false;
	m_hTrayWindow = 0;
	m_bRunning = false;
	m_bRunningService = false;
}

WinConsole::~WinConsole()
{
	if (m_pInitialArguments)
	{
		g_szArguments =	(char*(*)[])m_pInitialArguments;
		g_iArgumentCount = m_iInitialArgumentCount;
	}
	free(m_pDefaultArguments);
	delete m_pNidIcon;
}

void WinConsole::InitAppMode()
{
	SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

	m_hInstance = (HINSTANCE)GetModuleHandle(0);
    DWORD dwProcessId;
    GetWindowThreadProcessId(GetConsoleWindow(), &dwProcessId);
    m_bAppMode = false;

	if (GetCurrentProcessId() == dwProcessId && g_iArgumentCount == 1)
	{
		m_pInitialArguments = (char**)g_szArguments;
		m_iInitialArgumentCount = g_iArgumentCount;

		// make command line to start in server mode
		m_pDefaultArguments = (char**)malloc(sizeof(char*) * 3);
		m_pDefaultArguments[0] = (*g_szArguments)[0];
		m_pDefaultArguments[1] = "-s";
		m_pDefaultArguments[2] = NULL;
		g_szArguments = (char*(*)[])m_pDefaultArguments;
		g_iArgumentCount = 2;
		m_bAppMode = true;
	}
	else if (GetCurrentProcessId() == dwProcessId && g_iArgumentCount > 1)
	{
		for (int i = 1; i < g_iArgumentCount; i++)
		{
			if (!strcmp((*g_szArguments)[i], "-D"))
			{
				break;
			}
			if (!strcmp((*g_szArguments)[i], "-app"))
			{
				m_bAppMode = true;
			}

			if (!strcmp((*g_szArguments)[i], "-auto"))
			{
				m_bAutoParam = true;
			}
		}

		if (m_bAppMode)
		{
			m_pInitialArguments = (char**)g_szArguments;
			m_iInitialArgumentCount = g_iArgumentCount;

			// remove "-app" from command line
			int argc = g_iArgumentCount - 1 - (m_bAutoParam ? 1 : 0);
			m_pDefaultArguments = (char**)malloc(sizeof(char*) * (argc + 2));

			int p = 0;
			for (int i = 0; i < g_iArgumentCount; i++)
			{
				if (strcmp((*g_szArguments)[i], "-app") &&
					strcmp((*g_szArguments)[i], "-auto"))
				{
					m_pDefaultArguments[p++] = (*g_szArguments)[i];
				}
			}
			m_pDefaultArguments[p] = NULL;
			g_szArguments = (char*(*)[])m_pDefaultArguments;
			g_iArgumentCount = p;
		}
	}

	// m_bAppMode indicates whether the program was started as a standalone app
	// (not from a dos box window). In that case we hide the console window,
	// show the tray icon and start in server mode

	if (m_bAppMode)
	{
		CreateResources();
		CheckRunning();
	}
}

void WinConsole::Run()
{
	if (!m_bAppMode)
	{
		return;
	}

	CreateTrayIcon();

	LoadPrefs();
	ApplyPrefs();

	BuildMenu();

	if (m_bWebUI && !m_bAutoParam && bMayStartBrowser)
	{
		ShowWebUI();
	}
	bMayStartBrowser = false;

	int iCounter = 0;
	while (!IsStopped())
	{
		MSG msg;
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			usleep(20 * 1000);
			iCounter += 20;
			if (iCounter >= 200)
			{
				UpdateTrayIcon();
				iCounter = 0;
			}
		}
	}

	Shell_NotifyIcon(NIM_DELETE, m_pNidIcon);
}

void WinConsole::Stop()
{
	if (m_bAppMode)
	{
		PostMessage(m_hTrayWindow, WM_QUIT, 0, 0);
	}

	Thread::Stop();
}

void WinConsole::CreateResources()
{
	m_hMenu = LoadMenu(m_hInstance, MAKEINTRESOURCE(IDR_TRAYMENU));
	m_hMenu = GetSubMenu(m_hMenu, 0);

	HDC hdc = GetDC(NULL);
	long lfHeight = -MulDiv(8, GetDeviceCaps(hdc, LOGPIXELSY), 72);
	m_hLinkFont = CreateFont(lfHeight, 0, 0, 0, 0, 0, TRUE, 0, 0, 0, 0, 0, 0, "Tahoma");
	lfHeight = -MulDiv(11, GetDeviceCaps(hdc, LOGPIXELSY), 72);
	m_hNameFont = CreateFont(lfHeight, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma");
	lfHeight = -MulDiv(10, GetDeviceCaps(hdc, LOGPIXELSY), 72);
	m_hTitleFont = CreateFont(lfHeight, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma");
	ReleaseDC(NULL, hdc);

	m_hHandCursor = LoadCursor(NULL, MAKEINTRESOURCE(IDC_HAND));
	m_hAboutIcon = (HICON)LoadImage(m_hInstance, MAKEINTRESOURCE(IDI_MAINICON), IMAGE_ICON, 64, 64, 0);
	m_hRunningIcon = (HICON)LoadImage(m_hInstance, MAKEINTRESOURCE(IDI_MAINICON), IMAGE_ICON, 48, 48, 0);

	m_hIdleIcon = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_TRAYICON_IDLE));
	m_hWorkingIcon = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_TRAYICON_WORKING));
	m_hPausedIcon = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_TRAYICON_PAUSED));
}

void WinConsole::CreateTrayIcon()
{
	UM_TASKBARCREATED = RegisterWindowMessageA("TaskbarCreated") ;

	char className[] = "NZBGet tray window";
	WNDCLASSEX wnd;
	memset(&wnd, 0, sizeof(wnd));
	wnd.hInstance = m_hInstance;
	wnd.lpszClassName = className;
	wnd.lpfnWndProc = TrayWndProcStat;
	wnd.style = 0;
	wnd.cbSize = sizeof(WNDCLASSEX);
	RegisterClassEx(&wnd);

	m_hTrayWindow = CreateWindowEx(0, className, "NZBGet", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, m_hInstance, NULL);

	m_pNidIcon = new NOTIFYICONDATA;
	memset(m_pNidIcon, 0, sizeof(NOTIFYICONDATA));
	m_pNidIcon->cbSize = sizeof(NOTIFYICONDATA);
	m_pNidIcon->hWnd = m_hTrayWindow;
	m_pNidIcon->uID = 100;
	m_pNidIcon->uCallbackMessage = UM_TRAYICON;
	m_pNidIcon->hIcon = m_hWorkingIcon;
	strncpy(m_pNidIcon->szTip, "NZBGet", sizeof(m_pNidIcon->szTip));
	m_pNidIcon->uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
}

LRESULT CALLBACK WinConsole::TrayWndProcStat(HWND hwndWin, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return g_pWinConsole->TrayWndProc(hwndWin, uMsg, wParam, lParam);
}

LRESULT WinConsole::TrayWndProc(HWND hwndWin, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == UM_TASKBARCREATED)
	{
		ApplyPrefs();
		return 0;
	}

	switch (uMsg)
	{
		case UM_TRAYICON:
			if (lParam == WM_LBUTTONUP)
			{
				g_pOptions->SetPauseDownload(!g_pOptions->GetPauseDownload());
				g_pOptions->SetPausePostProcess(g_pOptions->GetPauseDownload());
				g_pOptions->SetPauseScan(g_pOptions->GetPauseDownload());
				g_pOptions->SetResumeTime(0);
				UpdateTrayIcon();
			}
			else if (lParam == WM_RBUTTONDOWN)
			{
				ShowMenu();
			}
			return 0;

		case UM_QUIT:
			ExitProc();
			return 0;

		case UM_SHOWWEBUI:
			ShowWebUI();
			return 0;

		case UM_PREFSCHANGED:
			LoadPrefs();
			ApplyPrefs();
			return 0;

		default:
			return DefWindowProc(hwndWin, uMsg, wParam, lParam);
	}
}

void WinConsole::ShowMenu()
{
	POINT curPoint;
	GetCursorPos(&curPoint);
	SetForegroundWindow(m_hTrayWindow); 

	UINT iItemID = TrackPopupMenu(m_hMenu, TPM_RETURNCMD | TPM_NONOTIFY, curPoint.x, curPoint.y, 0, m_hTrayWindow, NULL);

	switch(iItemID)
	{
		case ID_SHOWWEBUI:
			ShowWebUI();
			break;

		case ID_SHOW_DESTDIR:
			ShowInExplorer(g_pOptions->GetDestDir());
			break;

		case ID_SHOW_INTERDIR:
			ShowInExplorer(g_pOptions->GetInterDir());
			break;

		case ID_SHOW_NZBDIR:
			ShowInExplorer(g_pOptions->GetNzbDir());
			break;

		case ID_SHOW_CONFIGFILE:
			ShowInExplorer(g_pOptions->GetConfigFilename());
			break;

		case ID_SHOW_LOGFILE:
			ShowInExplorer(g_pOptions->GetLogFile());
			break;

		case ID_SHOW_SCRIPTDIR:
			ShowInExplorer(g_pOptions->GetScriptDir());
			break;

		case ID_INFO_HOMEPAGE:
			ShellExecute(0, "open", "http://nzbget.net", NULL, NULL, SW_SHOWNORMAL);
			break;

		case ID_INFO_DOWNLOADS:
			ShellExecute(0, "open", "http://nzbget.net/download", NULL, NULL, SW_SHOWNORMAL);
			break;

		case ID_INFO_FORUM:
			ShellExecute(0, "open", "http://nzbget.net/forum", NULL, NULL, SW_SHOWNORMAL);
			break;

		case ID_ABOUT:
			ShowAboutBox();
			break;

		case ID_PREFERENCES:
			ShowPrefsDialog();
			break;

		case ID_EXIT:
			ExitProc();
			break;

		case ID_TROUBLESHOOTING_RESTART:
			bMayStartBrowser = true;
			Reload();
			break;

		case ID_TROUBLESHOOTING_OPENCONFIG:
			OpenConfigFileInTextEdit();
			break;

		case ID_TROUBLESHOOTING_FACTORYRESET:
			ShowFactoryResetDialog();
			break;
	}

	if (iItemID >= ID_SHOW_DESTDIR + 1000 && iItemID < ID_SHOW_DESTDIR + 2000)
	{
		ShowCategoryDir(iItemID - (ID_SHOW_DESTDIR + 1000));
	}
}

void WinConsole::ShowWebUI()
{
	const char* szIP = g_pOptions->GetControlIP();
	if (!strcmp(g_pOptions->GetControlIP(), "localhost") ||
		!strcmp(g_pOptions->GetControlIP(), "0.0.0.0"))
	{
		szIP = "127.0.0.1";
	}

	char szURL[1024];
	snprintf(szURL, 1024, "http://%s:%i", szIP, g_pOptions->GetControlPort());
	szURL[1024-1] = '\0';
	ShellExecute(0, "open", szURL, NULL, NULL, SW_SHOWNORMAL);
}

void WinConsole::ShowInExplorer(const char* szFileName)
{
	char szFileName2[MAX_PATH + 1];
	strncpy(szFileName2, szFileName, MAX_PATH);
	szFileName2[MAX_PATH] = '\0';
	Util::NormalizePathSeparators(szFileName2);
	if (*szFileName2 && szFileName2[strlen(szFileName2) - 1] == PATH_SEPARATOR) szFileName2[strlen(szFileName2) - 1] = '\0'; // trim slash

	if (!Util::FileExists(szFileName2) && !Util::DirectoryExists(szFileName2))
	{
		char szMessage[400];
		snprintf(szMessage, 400, "Directory or file %s doesn't exist (yet).", szFileName2);
		szMessage[400-1] = '\0';
		MessageBox(m_hTrayWindow, szMessage, "Information", MB_ICONINFORMATION);
		return;
	}

	WCHAR wszFileName2[MAX_PATH + 1];
	MultiByteToWideChar(0, 0, szFileName2, strlen(szFileName2) + 1, wszFileName2, MAX_PATH);
	CoInitialize(NULL);
	LPITEMIDLIST pidl;
	HRESULT H = SHParseDisplayName(wszFileName2, NULL, &pidl, 0, NULL);
	H = SHOpenFolderAndSelectItems(pidl, 0, 0, 0);
}

void WinConsole::ShowAboutBox()
{
	if (m_bModal)
	{
		return;
	}

	m_bModal = true;
	DialogBox(m_hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), m_hTrayWindow, AboutDialogProcStat);
	m_bModal = false;
}

BOOL CALLBACK WinConsole::AboutDialogProcStat(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return g_pWinConsole->AboutDialogProc(hwndDlg, uMsg, wParam, lParam);
}

BOOL WinConsole::AboutDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
		case WM_INITDIALOG:
			SendDlgItemMessage(hwndDlg, IDC_ABOUT_NAME, WM_SETFONT, (WPARAM)m_hNameFont, 0);

			char szVersion[100];
			snprintf(szVersion, 100, "Version %s", Util::VersionRevision());
			SetDlgItemText(hwndDlg, IDC_ABOUT_VERSION, szVersion);

			SendDlgItemMessage(hwndDlg, IDC_ABOUT_ICON, STM_SETICON, (WPARAM)m_hAboutIcon, 0);

			SendDlgItemMessage(hwndDlg, IDC_ABOUT_HOMEPAGE, WM_SETFONT, (WPARAM)m_hLinkFont, 0);
			SendDlgItemMessage(hwndDlg, IDC_ABOUT_GPL, WM_SETFONT, (WPARAM)m_hLinkFont, 0);

			return FALSE;

		case WM_CLOSE:
			EndDialog(hwndDlg, 0);
			return TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK)
			{
				EndDialog(hwndDlg, 0);
			}
			else if (LOWORD(wParam) == IDC_ABOUT_HOMEPAGE && HIWORD(wParam) == STN_CLICKED)
			{
				ShellExecute(0, "open", "http://nzbget.net", NULL, NULL, SW_SHOWNORMAL);
			}
			else if (LOWORD(wParam) == IDC_ABOUT_GPL && HIWORD(wParam) == STN_CLICKED)
			{
				ShellExecute(0, "open", "http://www.gnu.org/licenses/gpl-2.0.html", NULL, NULL, SW_SHOWNORMAL);
			}
			return TRUE;

		case WM_CTLCOLORSTATIC:
			if ((HWND)lParam == GetDlgItem(hwndDlg, IDC_ABOUT_HOMEPAGE) ||
				(HWND)lParam == GetDlgItem(hwndDlg, IDC_ABOUT_GPL))
			{
				SetTextColor((HDC)wParam, RGB(0, 0, 255));
				SetBkColor((HDC)wParam, GetSysColor(COLOR_BTNFACE));
				return (INT_PTR)GetSysColorBrush(COLOR_BTNFACE);
			}
			return FALSE;

		case WM_SETCURSOR:
			if ((HWND)wParam == GetDlgItem(hwndDlg, IDC_ABOUT_HOMEPAGE) ||
				(HWND)wParam == GetDlgItem(hwndDlg, IDC_ABOUT_GPL))
			{
				SetCursor(m_hHandCursor);
				SetWindowLong(hwndDlg, DWL_MSGRESULT, TRUE);
				return TRUE;
			}
			return FALSE;

		default:
			return FALSE;
	}
}

void WinConsole::ShowPrefsDialog()
{
	if (m_bModal)
	{
		return;
	}

	m_bModal = true;
	DialogBox(m_hInstance, MAKEINTRESOURCE(IDD_PREFDIALOG), m_hTrayWindow, PrefsDialogProcStat);
	m_bModal = false;
}

BOOL CALLBACK WinConsole::PrefsDialogProcStat(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return g_pWinConsole->PrefsDialogProc(hwndDlg, uMsg, wParam, lParam);
}

BOOL WinConsole::PrefsDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
		case WM_INITDIALOG:
			LoadPrefs();

			SendDlgItemMessage(hwndDlg, IDC_PREF_AUTOSTART, BM_SETCHECK, m_bAutostart, 0);
			SendDlgItemMessage(hwndDlg, IDC_PREF_TRAY, BM_SETCHECK, m_bTray, 0);
			SendDlgItemMessage(hwndDlg, IDC_PREF_CONSOLE, BM_SETCHECK, m_bConsole, 0);
			SendDlgItemMessage(hwndDlg, IDC_PREF_WEBUI, BM_SETCHECK, m_bWebUI, 0);

			return FALSE;

		case WM_CLOSE:
			EndDialog(hwndDlg, 0);
			return TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK)
			{
				m_bAutostart = SendDlgItemMessage(hwndDlg, IDC_PREF_AUTOSTART, BM_GETCHECK, 0, 0) == BST_CHECKED;
				m_bTray = SendDlgItemMessage(hwndDlg, IDC_PREF_TRAY, BM_GETCHECK, 0, 0) == BST_CHECKED;
				m_bConsole = SendDlgItemMessage(hwndDlg, IDC_PREF_CONSOLE, BM_GETCHECK, 0, 0) == BST_CHECKED;
				m_bWebUI = SendDlgItemMessage(hwndDlg, IDC_PREF_WEBUI, BM_GETCHECK, 0, 0) == BST_CHECKED;

				SavePrefs();
				if (!m_bRunning)
				{
					ApplyPrefs();
				}

				EndDialog(hwndDlg, 0);
			}
			else if (LOWORD(wParam) == IDCANCEL)
			{
				EndDialog(hwndDlg, 0);
			}
			return TRUE;

		default:
			return FALSE;
	}
}

void WinConsole::SavePrefs()
{
	DWORD val;
	HKEY hKey;
	RegCreateKey(HKEY_CURRENT_USER, "Software\\NZBGet", &hKey);

	val = m_bTray;
	RegSetValueEx(hKey, "ShowTrayIcon", 0, REG_DWORD, (BYTE*)&val, sizeof(val));

	val = m_bConsole;
	RegSetValueEx(hKey, "ShowConsole", 0, REG_DWORD, (BYTE*)&val, sizeof(val));

	val = m_bWebUI;
	RegSetValueEx(hKey, "ShowWebUI", 0, REG_DWORD, (BYTE*)&val, sizeof(val));

	RegCloseKey(hKey);

	// Autostart-setting
	RegCreateKey(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", &hKey);
	if (m_bAutostart)
	{
		char szFilename[MAX_PATH + 1];
		GetModuleFileName(NULL, szFilename, sizeof(szFilename));
		char szStartCommand[1024];
		snprintf(szStartCommand, sizeof(szStartCommand), "\"%s\" -s -app -auto", szFilename);
		szStartCommand[1024-1] = '\0';
		RegSetValueEx(hKey, "NZBGet", 0, REG_SZ, (BYTE*)szStartCommand, strlen(szStartCommand) + 1);
	}
	else
	{
		RegDeleteValue(hKey, "NZBGet");
	}
	RegCloseKey(hKey);
}

void WinConsole::LoadPrefs()
{
	DWORD val;
	DWORD cval;
	DWORD typ;
	HKEY hKey;

	if (RegOpenKey(HKEY_CURRENT_USER, "Software\\NZBGet", &hKey) == ERROR_SUCCESS)
	{
		if (RegQueryValueEx(hKey, "ShowTrayIcon", 0, &typ, (LPBYTE)&val, &cval) == ERROR_SUCCESS)
		{
			m_bTray = val;
		}

		if (RegQueryValueEx(hKey, "ShowConsole", 0, &typ, (LPBYTE)&val, &cval) == ERROR_SUCCESS)
		{
			m_bConsole = val;
		}

		if (RegQueryValueEx(hKey, "ShowWebUI", 0, &typ, (LPBYTE)&val, &cval) == ERROR_SUCCESS)
		{
			m_bWebUI = val;
		}

		RegCloseKey(hKey);
	}

	// Autostart-setting
	if (RegOpenKey(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", &hKey) == ERROR_SUCCESS)
	{
		m_bAutostart = RegQueryValueEx(hKey, "NZBGet", 0, &typ, NULL, NULL) == ERROR_SUCCESS;
		RegCloseKey(hKey);
	}
}

void WinConsole::ApplyPrefs()
{
	ShowWindow(GetConsoleWindow(), m_bConsole ? SW_SHOW : SW_HIDE);
	if (m_bTray)
	{
		UpdateTrayIcon();
	}
	Shell_NotifyIcon(m_bTray ? NIM_ADD : NIM_DELETE, m_pNidIcon);
}

void WinConsole::CheckRunning()
{
	HWND hTrayWindow = FindWindow("NZBGet tray window", NULL);
	if (hTrayWindow)
	{
		ShowRunningDialog();
		ExitProcess(1);
	}

	if (IsServiceRunning())
	{
		m_bRunningService = true;
		ShowRunningDialog();
		ExitProcess(1);
	}
}

void WinConsole::ShowRunningDialog()
{
	ShowWindow(GetConsoleWindow(), m_bConsole ? SW_SHOW : SW_HIDE);
	HWND hTrayWindow = FindWindow("NZBGet tray window", NULL);
	m_bRunning = true;

	int iResult = DialogBox(m_hInstance, MAKEINTRESOURCE(IDD_RUNNINGDIALOG), m_hTrayWindow, RunningDialogProcStat);

	switch (iResult)
	{
		case IDC_RUNNING_WEBUI:
			PostMessage(hTrayWindow, UM_SHOWWEBUI, 0, 0);
			break;

		case IDC_RUNNING_PREFS:
			ShowPrefsDialog();
			PostMessage(hTrayWindow, UM_PREFSCHANGED, 0, 0);
			break;

		case IDC_RUNNING_QUIT:
			PostMessage(hTrayWindow, UM_QUIT, 0, 0);
			break;
	}
}

BOOL CALLBACK WinConsole::RunningDialogProcStat(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return g_pWinConsole->RunningDialogProc(hwndDlg, uMsg, wParam, lParam);
}

BOOL WinConsole::RunningDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
		case WM_INITDIALOG:
			SendDlgItemMessage(hwndDlg, IDC_RUNNING_ICON, STM_SETICON, (WPARAM)m_hRunningIcon, 0);
			SendDlgItemMessage(hwndDlg, IDC_RUNNING_TITLE, WM_SETFONT, (WPARAM)m_hTitleFont, 0);

			if (m_bRunningService)
			{
				SetDlgItemText(hwndDlg, IDC_RUNNING_TEXT, "Another instance of NZBGet is running as Windows Service. Please use Management Console to control the service.");
				ShowWindow(GetDlgItem(hwndDlg, IDC_RUNNING_WEBUI), SW_HIDE);
				ShowWindow(GetDlgItem(hwndDlg, IDC_RUNNING_PREFS), SW_HIDE);
				ShowWindow(GetDlgItem(hwndDlg, IDC_RUNNING_QUIT), SW_HIDE);
				ShowWindow(GetDlgItem(hwndDlg, IDC_RUNNING_OK), SW_SHOW);
			}

			return FALSE;

		case WM_CLOSE:
			EndDialog(hwndDlg, 0);
			return TRUE;

		case WM_COMMAND:
			EndDialog(hwndDlg, LOWORD(wParam));
			return TRUE;

		default:
			return FALSE;
	}
}

void WinConsole::UpdateTrayIcon()
{
	if (!m_bTray)
	{
		return;
	}

	HICON hOldIcon = m_pNidIcon->hIcon;

	char szOldTip[200];
	strncpy(szOldTip, m_pNidIcon->szTip, sizeof(m_pNidIcon->szTip));
	szOldTip[200-1] = '\0';

	if (g_pOptions->GetPauseDownload())
	{
		m_pNidIcon->hIcon = m_hPausedIcon;
		strncpy(m_pNidIcon->szTip, "NZBGet - paused", sizeof(m_pNidIcon->szTip));
	}
	else if (!g_pStatMeter->GetStandBy())
	{
		m_pNidIcon->hIcon = m_hWorkingIcon;
		char szSpeed[100];
		Util::FormatSpeed(g_pStatMeter->CalcCurrentDownloadSpeed(), szSpeed, sizeof(szSpeed));
		char szTip[200];
		snprintf(szTip, sizeof(szTip), "NZBGet - downloading at %s", szSpeed);
		szTip[200-1] = '\0';
		strncpy(m_pNidIcon->szTip, szTip, sizeof(m_pNidIcon->szTip));
	}
	else
	{
		DownloadQueue *pDownloadQueue = DownloadQueue::Lock();
		int iPostJobCount = 0;
		int iUrlCount = 0;
		for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
		{
			NZBInfo* pNZBInfo = *it;
			iPostJobCount += pNZBInfo->GetPostInfo() ? 1 : 0;
			iUrlCount += pNZBInfo->GetKind() == NZBInfo::nkUrl ? 1 : 0;
		}
		DownloadQueue::Unlock();

		if (iPostJobCount > 0)
		{
			m_pNidIcon->hIcon = m_hWorkingIcon;
			strncpy(m_pNidIcon->szTip, "NZBGet - post-processing", sizeof(m_pNidIcon->szTip));
		}
		else if (iUrlCount > 0)
		{
			m_pNidIcon->hIcon = m_hWorkingIcon;
			strncpy(m_pNidIcon->szTip, "NZBGet - fetching URLs", sizeof(m_pNidIcon->szTip));
		}
		else if (g_pFeedCoordinator->HasActiveDownloads())
		{
			m_pNidIcon->hIcon = m_hWorkingIcon;
			strncpy(m_pNidIcon->szTip, "NZBGet - fetching feeds", sizeof(m_pNidIcon->szTip));
		}
		else
		{
			m_pNidIcon->hIcon = m_hIdleIcon;
			strncpy(m_pNidIcon->szTip, "NZBGet - idle", sizeof(m_pNidIcon->szTip));
		}
	}

	if (m_pNidIcon->hIcon != hOldIcon || strcmp(szOldTip, m_pNidIcon->szTip))
	{
		Shell_NotifyIcon(NIM_MODIFY, m_pNidIcon);
	}
}

void WinConsole::BuildMenu()
{
	int iIndex = 0;
	for (Options::Categories::iterator it = g_pOptions->GetCategories()->begin(); it != g_pOptions->GetCategories()->end(); it++, iIndex++)
	{
		Options::Category* pCategory = *it;

		char szCaption[250];
		snprintf(szCaption, 250, "Category %i: %s", iIndex + 1, pCategory->GetName());
		szCaption[250 - 1] = '\0';

		MENUITEMINFO item;
		ZeroMemory(&item, sizeof(MENUITEMINFO));
		item.cbSize = sizeof(MENUITEMINFO);
		item.fMask = MIIM_ID | MIIM_STRING;
		item.fType = MFT_STRING;
		item.fState = MFS_DEFAULT;
		item.wID = ID_SHOW_DESTDIR + 1000 + iIndex;
		item.dwTypeData = szCaption;
		InsertMenuItem(GetSubMenu(m_hMenu, 1), 2 + iIndex, TRUE, &item);
	}

/*
BOOL DeleteMenu(

    HMENU hMenu,	// handle to menu
    UINT uPosition,	// menu item identifier or position
    UINT uFlags	// menu item flag
   );	
*/
}

void WinConsole::ShowCategoryDir(int iCatIndex)
{
	Options::Category* pCategory = g_pOptions->GetCategories()->at(iCatIndex);

	char szDestDir[1024];

	if (!Util::EmptyStr(pCategory->GetDestDir()))
	{
		snprintf(szDestDir, 1024, "%s", pCategory->GetDestDir());
		szDestDir[1024-1] = '\0';
	}
	else
	{
		char szCategoryDir[1024];
		strncpy(szCategoryDir, pCategory->GetName(), 1024);
		szCategoryDir[1024 - 1] = '\0';
		Util::MakeValidFilename(szCategoryDir, '_', true);

		snprintf(szDestDir, 1024, "%s%s", g_pOptions->GetDestDir(), szCategoryDir);
		szDestDir[1024-1] = '\0';
	}

	ShowInExplorer(szDestDir);
}

void WinConsole::OpenConfigFileInTextEdit()
{
	char szParam[MAX_PATH + 3];
	snprintf(szParam, sizeof(szParam), "\"%s\"", g_pOptions->GetConfigFilename());
	szParam[sizeof(szParam)-1] = '\0';
	ShellExecute(0, "open", "notepad.exe", szParam, NULL, SW_SHOWNORMAL);
}

void WinConsole::SetupFirstStart()
{
	SetupConfigFile();
	SetupScripts();
}

void WinConsole::SetupConfigFile()
{
	// create new config-file from config template

	char szFilename[MAX_PATH + 30];

	char szCommonAppDataPath[MAX_PATH];
	SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szCommonAppDataPath);

	snprintf(szFilename, sizeof(szFilename), "%s\\NZBGet\\nzbget.conf", szCommonAppDataPath);
	szFilename[sizeof(szFilename)-1] = '\0';

	char szAppDataPath[MAX_PATH + 1];
	snprintf(szAppDataPath, sizeof(szAppDataPath), "%s\\NZBGet", szCommonAppDataPath);
	szAppDataPath[sizeof(szAppDataPath)-1] = '\0';
	Util::CreateDirectory(szAppDataPath);

	char szConfTemplateFilename[MAX_PATH + 30];
	snprintf(szConfTemplateFilename, sizeof(szConfTemplateFilename), "%s\\nzbget.conf.template", g_pOptions->GetAppDir());
	szConfTemplateFilename[sizeof(szConfTemplateFilename)-1] = '\0';

	CopyFile(szConfTemplateFilename, szFilename, FALSE);

	// set MainDir in the config-file
	int iSize = 0;
	char* szConfig = NULL;
	if (Util::LoadFileIntoBuffer(szFilename, &szConfig, &iSize))
	{
		const char* SIGNATURE = "MainDir=${AppDir}\\downloads";
		char* p = strstr(szConfig, SIGNATURE);
		if (p)
		{
			FILE* outfile = fopen(szFilename, FOPEN_WBP);
			if (outfile)
			{
				fwrite(szConfig, 1, p - szConfig, outfile);
				fwrite("MainDir=", 1, 8, outfile);
				fwrite(szAppDataPath, 1, strlen(szAppDataPath), outfile);
				
				fwrite(p + strlen(SIGNATURE), 1, iSize - (p + strlen(SIGNATURE) - szConfig) - 1, outfile);
				fclose(outfile);
			}
		}
		free(szConfig);
	}

	// create default destination directory (which is not created on start automatically)
	snprintf(szFilename, sizeof(szFilename), "%s\\NZBGet\\complete", szCommonAppDataPath);
	szFilename[sizeof(szFilename)-1] = '\0';
	Util::CreateDirectory(szFilename);
}

void WinConsole::SetupScripts()
{
	// copy default scripts

	char szAppDataPath[MAX_PATH];
	SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szAppDataPath);

	char szDestDir[MAX_PATH + 1];
	snprintf(szDestDir, sizeof(szDestDir), "%s\\NZBGet\\scripts", szAppDataPath);
	szDestDir[sizeof(szDestDir)-1] = '\0';
	Util::CreateDirectory(szDestDir);

	char szSrcDir[MAX_PATH + 30];
	snprintf(szSrcDir, sizeof(szSrcDir), "%s\\scripts", g_pOptions->GetAppDir());
	szSrcDir[sizeof(szSrcDir)-1] = '\0';

	DirBrowser dir(szSrcDir);
	while (const char* szFilename = dir.Next())
	{
		if (strcmp(szFilename, ".") && strcmp(szFilename, ".."))
		{
			char szSrcFullFilename[1024];
			snprintf(szSrcFullFilename, 1024, "%s\\%s", szSrcDir, szFilename);
			szSrcFullFilename[1024-1] = '\0';

			char szDstFullFilename[1024];
			snprintf(szDstFullFilename, 1024, "%s\\%s", szDestDir, szFilename);
			szDstFullFilename[1024-1] = '\0';

			CopyFile(szSrcFullFilename, szDstFullFilename, FALSE);
		}
	}
}

void WinConsole::ShowFactoryResetDialog()
{
	HWND hTrayWindow = FindWindow("NZBGet tray window", NULL);
	m_bRunning = true;

	int iResult = DialogBox(m_hInstance, MAKEINTRESOURCE(IDD_FACTORYRESETDIALOG), m_hTrayWindow, FactoryResetDialogProcStat);

	switch (iResult)
	{
		case IDC_FACTORYRESET_RESET:
			ResetFactoryDefaults();
			break;
	}
}

BOOL CALLBACK WinConsole::FactoryResetDialogProcStat(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return g_pWinConsole->FactoryResetDialogProc(hwndDlg, uMsg, wParam, lParam);
}

BOOL WinConsole::FactoryResetDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
		case WM_INITDIALOG:
			SendDlgItemMessage(hwndDlg, IDC_FACTORYRESET_ICON, STM_SETICON, (WPARAM)m_hRunningIcon, 0);
			SendDlgItemMessage(hwndDlg, IDC_FACTORYRESET_TITLE, WM_SETFONT, (WPARAM)m_hTitleFont, 0);
			return FALSE;

		case WM_CLOSE:
			EndDialog(hwndDlg, 0);
			return TRUE;

		case WM_COMMAND:
			EndDialog(hwndDlg, LOWORD(wParam));
			return TRUE;

		default:
			return FALSE;
	}
}

void WinConsole::ResetFactoryDefaults()
{
	char szPath[MAX_PATH + 100];
	char szMessage[1024];
	char szErrBuf[200];

	g_pOptions->SetPauseDownload(true);
	g_pOptions->SetPausePostProcess(true);

	char szCommonAppDataPath[MAX_PATH];
	SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szCommonAppDataPath);

	// delete default directories
	const char* DefDirs[] = {"nzb", "tmp", "queue", "scripts"};
	for (int i=0; i < 4; i++)
	{
		snprintf(szPath, sizeof(szPath), "%s\\NZBGet\\%s", szCommonAppDataPath, DefDirs[i]);
		szPath[sizeof(szPath)-1] = '\0';

		// try to delete the directory
		int iRetry = 10;
		while (iRetry > 0 && Util::DirectoryExists(szPath) &&
			!Util::DeleteDirectoryWithContent(szPath, szErrBuf, sizeof(szErrBuf)))
		{
			usleep(200 * 1000);
			iRetry--;
		}

		if (Util::DirectoryExists(szPath))
		{
			snprintf(szMessage, 1024, "Could not delete directory %s:\n%s.\nPlease delete the directory manually and try again.", szPath, szErrBuf);
			szMessage[1024-1] = '\0';
			MessageBox(m_hTrayWindow, szMessage, "NZBGet", MB_ICONERROR);
			return;
		}
	}

	// delete old config file in the program's directory
	snprintf(szPath, sizeof(szPath), "%s\\nzbget.conf", g_pOptions->GetAppDir());

	remove(szPath);
	Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf));

	if (Util::FileExists(szPath))
	{
		snprintf(szMessage, 1024, "Could not delete file %s:\n%s.\nPlease delete the file manually and try again.", szPath, szErrBuf);
		szMessage[1024-1] = '\0';
		MessageBox(m_hTrayWindow, szMessage, "NZBGet", MB_ICONERROR);
		return;
	}

	// delete config file in default directory
	snprintf(szPath, sizeof(szPath), "%s\\NZBGet\\nzbget.conf", szCommonAppDataPath);
	szPath[sizeof(szPath)-1] = '\0';

	remove(szPath);
	Util::GetLastErrorMessage(szErrBuf, sizeof(szErrBuf));


	if (Util::FileExists(szPath))
	{
		snprintf(szMessage, 1024, "Could not delete file %s:\n%s.\nPlease delete the file manually and try again.", szPath, szErrBuf);
		szMessage[1024-1] = '\0';
		MessageBox(m_hTrayWindow, szMessage, "NZBGet", MB_ICONERROR);
		return;
	}

	// delete log files in default directory
	snprintf(szPath, sizeof(szPath), "%s\\NZBGet", szCommonAppDataPath);
	szPath[sizeof(szPath)-1] = '\0';

	DirBrowser dir(szPath);
	while (const char* szFilename = dir.Next())
	{
		if (Util::MatchFileExt(szFilename, ".log", ","))
		{
			char szFullFilename[1024];
			snprintf(szFullFilename, 1024, "%s%c%s", szPath, PATH_SEPARATOR, szFilename);
			szFullFilename[1024-1] = '\0';

			remove(szFullFilename);

			// ignore errors
		}
	}

	MessageBox(m_hTrayWindow, "The program has been reset to factory defaults.",
		"NZBGet", MB_ICONINFORMATION);

	bMayStartBrowser = true;
	Reload();
}
