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

bool mayStartBrowser = true;

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
	m_initialArguments = NULL;
	m_initialArgumentCount = 0;
	m_defaultArguments = NULL;
	m_iconData = NULL;
	m_modal = false;
	m_autostart = false;
	m_showTrayIcon = true;
	m_showConsole = false;
	m_showWebUI = true;
	m_autoParam = false;
	m_doubleClick = false;
	m_trayWindow = 0;
	m_running = false;
	m_runningService = false;
}

WinConsole::~WinConsole()
{
	if (m_initialArguments)
	{
		g_szArguments =	(char*(*)[])m_initialArguments;
		g_iArgumentCount = m_initialArgumentCount;
	}
	free(m_defaultArguments);
	delete m_iconData;
}

void WinConsole::InitAppMode()
{
	SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

	m_instance = (HINSTANCE)GetModuleHandle(0);
    DWORD processId;
    GetWindowThreadProcessId(GetConsoleWindow(), &processId);
    m_appMode = false;

	if (GetCurrentProcessId() == processId && g_iArgumentCount == 1)
	{
		m_initialArguments = (char**)g_szArguments;
		m_initialArgumentCount = g_iArgumentCount;

		// make command line to start in server mode
		m_defaultArguments = (char**)malloc(sizeof(char*) * 3);
		m_defaultArguments[0] = (*g_szArguments)[0];
		m_defaultArguments[1] = "-s";
		m_defaultArguments[2] = NULL;
		g_szArguments = (char*(*)[])m_defaultArguments;
		g_iArgumentCount = 2;
		m_appMode = true;
	}
	else if (GetCurrentProcessId() == processId && g_iArgumentCount > 1)
	{
		for (int i = 1; i < g_iArgumentCount; i++)
		{
			if (!strcmp((*g_szArguments)[i], "-D"))
			{
				break;
			}
			if (!strcmp((*g_szArguments)[i], "-app"))
			{
				m_appMode = true;
			}

			if (!strcmp((*g_szArguments)[i], "-auto"))
			{
				m_autoParam = true;
			}
		}

		if (m_appMode)
		{
			m_initialArguments = (char**)g_szArguments;
			m_initialArgumentCount = g_iArgumentCount;

			// remove "-app" from command line
			int argc = g_iArgumentCount - 1 - (m_autoParam ? 1 : 0);
			m_defaultArguments = (char**)malloc(sizeof(char*) * (argc + 2));

			int p = 0;
			for (int i = 0; i < g_iArgumentCount; i++)
			{
				if (strcmp((*g_szArguments)[i], "-app") &&
					strcmp((*g_szArguments)[i], "-auto"))
				{
					m_defaultArguments[p++] = (*g_szArguments)[i];
				}
			}
			m_defaultArguments[p] = NULL;
			g_szArguments = (char*(*)[])m_defaultArguments;
			g_iArgumentCount = p;
		}
	}

	// m_bAppMode indicates whether the program was started as a standalone app
	// (not from a dos box window). In that case we hide the console window,
	// show the tray icon and start in server mode

	if (m_appMode)
	{
		CreateResources();
		CheckRunning();
	}
}

void WinConsole::Run()
{
	if (!m_appMode)
	{
		return;
	}

	CreateTrayIcon();

	LoadPrefs();
	ApplyPrefs();

	BuildMenu();

	if (m_showWebUI && !m_autoParam && mayStartBrowser)
	{
		ShowWebUI();
	}
	mayStartBrowser = false;

	int counter = 0;
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
			counter += 20;
			if (counter >= 200)
			{
				UpdateTrayIcon();
				counter = 0;
			}
		}
	}

	Shell_NotifyIcon(NIM_DELETE, m_iconData);
}

void WinConsole::Stop()
{
	if (m_appMode)
	{
		PostMessage(m_trayWindow, WM_QUIT, 0, 0);
	}

	Thread::Stop();
}

void WinConsole::CreateResources()
{
	m_menu = LoadMenu(m_instance, MAKEINTRESOURCE(IDR_TRAYMENU));
	m_menu = GetSubMenu(m_menu, 0);

	HDC hdc = GetDC(NULL);
	long height = -MulDiv(8, GetDeviceCaps(hdc, LOGPIXELSY), 72);
	m_linkFont = CreateFont(height, 0, 0, 0, 0, 0, TRUE, 0, 0, 0, 0, 0, 0, "Tahoma");
	height = -MulDiv(11, GetDeviceCaps(hdc, LOGPIXELSY), 72);
	m_nameFont = CreateFont(height, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma");
	height = -MulDiv(10, GetDeviceCaps(hdc, LOGPIXELSY), 72);
	m_titleFont = CreateFont(height, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma");
	ReleaseDC(NULL, hdc);

	m_handCursor = LoadCursor(NULL, MAKEINTRESOURCE(IDC_HAND));
	m_aboutIcon = (HICON)LoadImage(m_instance, MAKEINTRESOURCE(IDI_MAINICON), IMAGE_ICON, 64, 64, 0);
	m_runningIcon = (HICON)LoadImage(m_instance, MAKEINTRESOURCE(IDI_MAINICON), IMAGE_ICON, 48, 48, 0);

	m_idleIcon = LoadIcon(m_instance, MAKEINTRESOURCE(IDI_TRAYICON_IDLE));
	m_workingIcon = LoadIcon(m_instance, MAKEINTRESOURCE(IDI_TRAYICON_WORKING));
	m_pausedIcon = LoadIcon(m_instance, MAKEINTRESOURCE(IDI_TRAYICON_PAUSED));
}

void WinConsole::CreateTrayIcon()
{
	m_taskbarCreatedMessage = RegisterWindowMessageA("TaskbarCreated") ;

	char className[] = "NZBGet tray window";
	WNDCLASSEX wnd;
	memset(&wnd, 0, sizeof(wnd));
	wnd.hInstance = m_instance;
	wnd.lpszClassName = className;
	wnd.lpfnWndProc = TrayWndProcStat;
	wnd.style = 0;
	wnd.cbSize = sizeof(WNDCLASSEX);
	RegisterClassEx(&wnd);

	m_trayWindow = CreateWindowEx(0, className, "NZBGet", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, m_instance, NULL);

	m_iconData = new NOTIFYICONDATA;
	memset(m_iconData, 0, sizeof(NOTIFYICONDATA));
	m_iconData->cbSize = sizeof(NOTIFYICONDATA);
	m_iconData->hWnd = m_trayWindow;
	m_iconData->uID = 100;
	m_iconData->uCallbackMessage = UM_TRAYICON;
	m_iconData->hIcon = m_workingIcon;
	strncpy(m_iconData->szTip, "NZBGet", sizeof(m_iconData->szTip));
	m_iconData->uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
}

LRESULT CALLBACK WinConsole::TrayWndProcStat(HWND hwndWin, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return g_pWinConsole->TrayWndProc(hwndWin, uMsg, wParam, lParam);
}

LRESULT WinConsole::TrayWndProc(HWND hwndWin, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == m_taskbarCreatedMessage)
	{
		ApplyPrefs();
		return 0;
	}

	switch (uMsg)
	{
		case UM_TRAYICON:
			if (lParam == WM_LBUTTONUP && !m_doubleClick)
			{
				g_pOptions->SetPauseDownload(!g_pOptions->GetPauseDownload());
				g_pOptions->SetPausePostProcess(g_pOptions->GetPauseDownload());
				g_pOptions->SetPauseScan(g_pOptions->GetPauseDownload());
				g_pOptions->SetResumeTime(0);
				UpdateTrayIcon();
			}
			else if (lParam == WM_LBUTTONDBLCLK && m_doubleClick)
			{
				ShowWebUI();
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
	SetForegroundWindow(m_trayWindow); 

	UINT itemId = TrackPopupMenu(m_menu, TPM_RETURNCMD | TPM_NONOTIFY, curPoint.x, curPoint.y, 0, m_trayWindow, NULL);

	switch(itemId)
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
			mayStartBrowser = true;
			Reload();
			break;

		case ID_TROUBLESHOOTING_OPENCONFIG:
			OpenConfigFileInTextEdit();
			break;

		case ID_TROUBLESHOOTING_FACTORYRESET:
			ShowFactoryResetDialog();
			break;
	}

	if (itemId >= ID_SHOW_DESTDIR + 1000 && itemId < ID_SHOW_DESTDIR + 2000)
	{
		ShowCategoryDir(itemId - (ID_SHOW_DESTDIR + 1000));
	}
}

void WinConsole::ShowWebUI()
{
	const char* iP = g_pOptions->GetControlIP();
	if (!strcmp(g_pOptions->GetControlIP(), "localhost") ||
		!strcmp(g_pOptions->GetControlIP(), "0.0.0.0"))
	{
		iP = "127.0.0.1";
	}

	char url[1024];
	snprintf(url, 1024, "http://%s:%i", iP, g_pOptions->GetControlPort());
	url[1024-1] = '\0';
	ShellExecute(0, "open", url, NULL, NULL, SW_SHOWNORMAL);
}

void WinConsole::ShowInExplorer(const char* filename)
{
	char fileName2[MAX_PATH + 1];
	strncpy(fileName2, filename, MAX_PATH);
	fileName2[MAX_PATH] = '\0';
	Util::NormalizePathSeparators(fileName2);
	if (*fileName2 && fileName2[strlen(fileName2) - 1] == PATH_SEPARATOR) fileName2[strlen(fileName2) - 1] = '\0'; // trim slash

	if (!Util::FileExists(fileName2) && !Util::DirectoryExists(fileName2))
	{
		char message[400];
		snprintf(message, 400, "Directory or file %s doesn't exist (yet).", fileName2);
		message[400-1] = '\0';
		MessageBox(m_trayWindow, message, "Information", MB_ICONINFORMATION);
		return;
	}

	WCHAR wszFileName2[MAX_PATH + 1];
	MultiByteToWideChar(0, 0, fileName2, strlen(fileName2) + 1, wszFileName2, MAX_PATH);
	CoInitialize(NULL);
	LPITEMIDLIST pidl;
	HRESULT H = SHParseDisplayName(wszFileName2, NULL, &pidl, 0, NULL);
	H = SHOpenFolderAndSelectItems(pidl, 0, 0, 0);
}

void WinConsole::ShowAboutBox()
{
	if (m_modal)
	{
		return;
	}

	m_modal = true;
	DialogBox(m_instance, MAKEINTRESOURCE(IDD_ABOUTBOX), m_trayWindow, AboutDialogProcStat);
	m_modal = false;
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
			SendDlgItemMessage(hwndDlg, IDC_ABOUT_NAME, WM_SETFONT, (WPARAM)m_nameFont, 0);

			char version[100];
			snprintf(version, 100, "Version %s", Util::VersionRevision());
			SetDlgItemText(hwndDlg, IDC_ABOUT_VERSION, version);

			SendDlgItemMessage(hwndDlg, IDC_ABOUT_ICON, STM_SETICON, (WPARAM)m_aboutIcon, 0);

			SendDlgItemMessage(hwndDlg, IDC_ABOUT_HOMEPAGE, WM_SETFONT, (WPARAM)m_linkFont, 0);
			SendDlgItemMessage(hwndDlg, IDC_ABOUT_GPL, WM_SETFONT, (WPARAM)m_linkFont, 0);

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
				SetCursor(m_handCursor);
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
	if (m_modal)
	{
		return;
	}

	m_modal = true;
	DialogBox(m_instance, MAKEINTRESOURCE(IDD_PREFDIALOG), m_trayWindow, PrefsDialogProcStat);
	m_modal = false;
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

			SendDlgItemMessage(hwndDlg, IDC_PREF_AUTOSTART, BM_SETCHECK, m_autostart, 0);
			SendDlgItemMessage(hwndDlg, IDC_PREF_TRAY, BM_SETCHECK, m_showTrayIcon, 0);
			SendDlgItemMessage(hwndDlg, IDC_PREF_CONSOLE, BM_SETCHECK, m_showConsole, 0);
			SendDlgItemMessage(hwndDlg, IDC_PREF_WEBUI, BM_SETCHECK, m_showWebUI, 0);
			SendDlgItemMessage(hwndDlg, IDC_PREF_TRAYPAUSE, BM_SETCHECK, !m_doubleClick, 0);
			SendDlgItemMessage(hwndDlg, IDC_PREF_TRAYWEBUI, BM_SETCHECK, m_doubleClick, 0);

			return FALSE;

		case WM_CLOSE:
			EndDialog(hwndDlg, 0);
			return TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK)
			{
				m_autostart = SendDlgItemMessage(hwndDlg, IDC_PREF_AUTOSTART, BM_GETCHECK, 0, 0) == BST_CHECKED;
				m_showTrayIcon = SendDlgItemMessage(hwndDlg, IDC_PREF_TRAY, BM_GETCHECK, 0, 0) == BST_CHECKED;
				m_showConsole = SendDlgItemMessage(hwndDlg, IDC_PREF_CONSOLE, BM_GETCHECK, 0, 0) == BST_CHECKED;
				m_showWebUI = SendDlgItemMessage(hwndDlg, IDC_PREF_WEBUI, BM_GETCHECK, 0, 0) == BST_CHECKED;
				m_doubleClick = SendDlgItemMessage(hwndDlg, IDC_PREF_TRAYWEBUI, BM_GETCHECK, 0, 0) == BST_CHECKED;

				SavePrefs();
				if (!m_running)
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

	val = m_showTrayIcon;
	RegSetValueEx(hKey, "ShowTrayIcon", 0, REG_DWORD, (BYTE*)&val, sizeof(val));

	val = m_showConsole;
	RegSetValueEx(hKey, "ShowConsole", 0, REG_DWORD, (BYTE*)&val, sizeof(val));

	val = m_showWebUI;
	RegSetValueEx(hKey, "ShowWebUI", 0, REG_DWORD, (BYTE*)&val, sizeof(val));

	val = m_doubleClick;
	RegSetValueEx(hKey, "TrayDoubleClick", 0, REG_DWORD, (BYTE*)&val, sizeof(val));

	RegCloseKey(hKey);

	// Autostart-setting
	RegCreateKey(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", &hKey);
	if (m_autostart)
	{
		char filename[MAX_PATH + 1];
		GetModuleFileName(NULL, filename, sizeof(filename));
		char startCommand[1024];
		snprintf(startCommand, sizeof(startCommand), "\"%s\" -s -app -auto", filename);
		startCommand[1024-1] = '\0';
		RegSetValueEx(hKey, "NZBGet", 0, REG_SZ, (BYTE*)startCommand, strlen(startCommand) + 1);
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
			m_showTrayIcon = val;
		}

		if (RegQueryValueEx(hKey, "ShowConsole", 0, &typ, (LPBYTE)&val, &cval) == ERROR_SUCCESS)
		{
			m_showConsole = val;
		}

		if (RegQueryValueEx(hKey, "ShowWebUI", 0, &typ, (LPBYTE)&val, &cval) == ERROR_SUCCESS)
		{
			m_showWebUI = val;
		}

		if (RegQueryValueEx(hKey, "TrayDoubleClick", 0, &typ, (LPBYTE)&val, &cval) == ERROR_SUCCESS)
		{
			m_doubleClick = val;
		}

		RegCloseKey(hKey);
	}

	// Autostart-setting
	if (RegOpenKey(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", &hKey) == ERROR_SUCCESS)
	{
		m_autostart = RegQueryValueEx(hKey, "NZBGet", 0, &typ, NULL, NULL) == ERROR_SUCCESS;
		RegCloseKey(hKey);
	}
}

void WinConsole::ApplyPrefs()
{
	ShowWindow(GetConsoleWindow(), m_showConsole ? SW_SHOW : SW_HIDE);
	if (m_showTrayIcon)
	{
		UpdateTrayIcon();
	}
	Shell_NotifyIcon(m_showTrayIcon ? NIM_ADD : NIM_DELETE, m_iconData);
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
		m_runningService = true;
		ShowRunningDialog();
		ExitProcess(1);
	}
}

void WinConsole::ShowRunningDialog()
{
	ShowWindow(GetConsoleWindow(), m_showConsole ? SW_SHOW : SW_HIDE);
	HWND hTrayWindow = FindWindow("NZBGet tray window", NULL);
	m_running = true;

	int result = DialogBox(m_instance, MAKEINTRESOURCE(IDD_RUNNINGDIALOG), m_trayWindow, RunningDialogProcStat);

	switch (result)
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
			SendDlgItemMessage(hwndDlg, IDC_RUNNING_ICON, STM_SETICON, (WPARAM)m_runningIcon, 0);
			SendDlgItemMessage(hwndDlg, IDC_RUNNING_TITLE, WM_SETFONT, (WPARAM)m_titleFont, 0);

			if (m_runningService)
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
	if (!m_showTrayIcon)
	{
		return;
	}

	HICON hOldIcon = m_iconData->hIcon;

	char oldTip[200];
	strncpy(oldTip, m_iconData->szTip, sizeof(m_iconData->szTip));
	oldTip[200-1] = '\0';

	if (g_pOptions->GetPauseDownload())
	{
		m_iconData->hIcon = m_pausedIcon;
		strncpy(m_iconData->szTip, "NZBGet - paused", sizeof(m_iconData->szTip));
	}
	else if (!g_pStatMeter->GetStandBy())
	{
		m_iconData->hIcon = m_workingIcon;
		char speed[100];
		Util::FormatSpeed(speed, sizeof(speed), g_pStatMeter->CalcCurrentDownloadSpeed());
		char tip[200];
		snprintf(tip, sizeof(tip), "NZBGet - downloading at %s", speed);
		tip[200-1] = '\0';
		strncpy(m_iconData->szTip, tip, sizeof(m_iconData->szTip));
	}
	else
	{
		DownloadQueue *downloadQueue = DownloadQueue::Lock();
		int postJobCount = 0;
		int urlCount = 0;
		for (NZBList::iterator it = downloadQueue->GetQueue()->begin(); it != downloadQueue->GetQueue()->end(); it++)
		{
			NZBInfo* nzbInfo = *it;
			postJobCount += nzbInfo->GetPostInfo() ? 1 : 0;
			urlCount += nzbInfo->GetKind() == NZBInfo::nkUrl ? 1 : 0;
		}
		DownloadQueue::Unlock();

		if (postJobCount > 0)
		{
			m_iconData->hIcon = m_workingIcon;
			strncpy(m_iconData->szTip, "NZBGet - post-processing", sizeof(m_iconData->szTip));
		}
		else if (urlCount > 0)
		{
			m_iconData->hIcon = m_workingIcon;
			strncpy(m_iconData->szTip, "NZBGet - fetching URLs", sizeof(m_iconData->szTip));
		}
		else if (g_pFeedCoordinator->HasActiveDownloads())
		{
			m_iconData->hIcon = m_workingIcon;
			strncpy(m_iconData->szTip, "NZBGet - fetching feeds", sizeof(m_iconData->szTip));
		}
		else
		{
			m_iconData->hIcon = m_idleIcon;
			strncpy(m_iconData->szTip, "NZBGet - idle", sizeof(m_iconData->szTip));
		}
	}

	if (m_iconData->hIcon != hOldIcon || strcmp(oldTip, m_iconData->szTip))
	{
		Shell_NotifyIcon(NIM_MODIFY, m_iconData);
	}
}

void WinConsole::BuildMenu()
{
	int index = 0;
	for (Options::Categories::iterator it = g_pOptions->GetCategories()->begin(); it != g_pOptions->GetCategories()->end(); it++, index++)
	{
		Options::Category* category = *it;

		char caption[250];
		snprintf(caption, 250, "Category %i: %s", index + 1, category->GetName());
		caption[250 - 1] = '\0';

		MENUITEMINFO item;
		ZeroMemory(&item, sizeof(MENUITEMINFO));
		item.cbSize = sizeof(MENUITEMINFO);
		item.fMask = MIIM_ID | MIIM_STRING;
		item.fType = MFT_STRING;
		item.fState = MFS_DEFAULT;
		item.wID = ID_SHOW_DESTDIR + 1000 + index;
		item.dwTypeData = caption;
		InsertMenuItem(GetSubMenu(m_menu, 1), 2 + index, TRUE, &item);
	}

/*
BOOL DeleteMenu(

    HMENU hMenu,	// handle to menu
    UINT uPosition,	// menu item identifier or position
    UINT uFlags	// menu item flag
   );	
*/
}

void WinConsole::ShowCategoryDir(int catIndex)
{
	Options::Category* category = g_pOptions->GetCategories()->at(catIndex);

	char destDir[1024];

	if (!Util::EmptyStr(category->GetDestDir()))
	{
		snprintf(destDir, 1024, "%s", category->GetDestDir());
		destDir[1024-1] = '\0';
	}
	else
	{
		char categoryDir[1024];
		strncpy(categoryDir, category->GetName(), 1024);
		categoryDir[1024 - 1] = '\0';
		Util::MakeValidFilename(categoryDir, '_', true);

		snprintf(destDir, 1024, "%s%s", g_pOptions->GetDestDir(), categoryDir);
		destDir[1024-1] = '\0';
	}

	ShowInExplorer(destDir);
}

void WinConsole::OpenConfigFileInTextEdit()
{
	char lParam[MAX_PATH + 3];
	snprintf(lParam, sizeof(lParam), "\"%s\"", g_pOptions->GetConfigFilename());
	lParam[sizeof(lParam)-1] = '\0';
	ShellExecute(0, "open", "notepad.exe", lParam, NULL, SW_SHOWNORMAL);
}

void WinConsole::SetupFirstStart()
{
	SetupConfigFile();
	SetupScripts();
}

void WinConsole::SetupConfigFile()
{
	// create new config-file from config template

	char filename[MAX_PATH + 30];

	char commonAppDataPath[MAX_PATH];
	SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, commonAppDataPath);

	snprintf(filename, sizeof(filename), "%s\\NZBGet\\nzbget.conf", commonAppDataPath);
	filename[sizeof(filename)-1] = '\0';

	char appDataPath[MAX_PATH + 1];
	snprintf(appDataPath, sizeof(appDataPath), "%s\\NZBGet", commonAppDataPath);
	appDataPath[sizeof(appDataPath)-1] = '\0';
	Util::CreateDirectory(appDataPath);

	char confTemplateFilename[MAX_PATH + 30];
	snprintf(confTemplateFilename, sizeof(confTemplateFilename), "%s\\nzbget.conf.template", g_pOptions->GetAppDir());
	confTemplateFilename[sizeof(confTemplateFilename)-1] = '\0';

	CopyFile(confTemplateFilename, filename, FALSE);

	// set MainDir in the config-file
	int size = 0;
	char* config = NULL;
	if (Util::LoadFileIntoBuffer(filename, &config, &size))
	{
		const char* SIGNATURE = "MainDir=${AppDir}\\downloads";
		char* p = strstr(config, SIGNATURE);
		if (p)
		{
			FILE* outfile = fopen(filename, FOPEN_WBP);
			if (outfile)
			{
				fwrite(config, 1, p - config, outfile);
				fwrite("MainDir=", 1, 8, outfile);
				fwrite(appDataPath, 1, strlen(appDataPath), outfile);
				
				fwrite(p + strlen(SIGNATURE), 1, size - (p + strlen(SIGNATURE) - config) - 1, outfile);
				fclose(outfile);
			}
		}
		free(config);
	}

	// create default destination directory (which is not created on start automatically)
	snprintf(filename, sizeof(filename), "%s\\NZBGet\\complete", commonAppDataPath);
	filename[sizeof(filename)-1] = '\0';
	Util::CreateDirectory(filename);
}

void WinConsole::SetupScripts()
{
	// copy default scripts

	char appDataPath[MAX_PATH];
	SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, appDataPath);

	char destDir[MAX_PATH + 1];
	snprintf(destDir, sizeof(destDir), "%s\\NZBGet\\scripts", appDataPath);
	destDir[sizeof(destDir)-1] = '\0';
	Util::CreateDirectory(destDir);

	char srcDir[MAX_PATH + 30];
	snprintf(srcDir, sizeof(srcDir), "%s\\scripts", g_pOptions->GetAppDir());
	srcDir[sizeof(srcDir)-1] = '\0';

	DirBrowser dir(srcDir);
	while (const char* filename = dir.Next())
	{
		if (strcmp(filename, ".") && strcmp(filename, ".."))
		{
			char srcFullFilename[1024];
			snprintf(srcFullFilename, 1024, "%s\\%s", srcDir, filename);
			srcFullFilename[1024-1] = '\0';

			char dstFullFilename[1024];
			snprintf(dstFullFilename, 1024, "%s\\%s", destDir, filename);
			dstFullFilename[1024-1] = '\0';

			CopyFile(srcFullFilename, dstFullFilename, FALSE);
		}
	}
}

void WinConsole::ShowFactoryResetDialog()
{
	HWND hTrayWindow = FindWindow("NZBGet tray window", NULL);
	m_running = true;

	int result = DialogBox(m_instance, MAKEINTRESOURCE(IDD_FACTORYRESETDIALOG), m_trayWindow, FactoryResetDialogProcStat);

	switch (result)
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
			SendDlgItemMessage(hwndDlg, IDC_FACTORYRESET_ICON, STM_SETICON, (WPARAM)m_runningIcon, 0);
			SendDlgItemMessage(hwndDlg, IDC_FACTORYRESET_TITLE, WM_SETFONT, (WPARAM)m_titleFont, 0);
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
	char path[MAX_PATH + 100];
	char message[1024];
	char errBuf[200];

	g_pOptions->SetPauseDownload(true);
	g_pOptions->SetPausePostProcess(true);

	char commonAppDataPath[MAX_PATH];
	SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, commonAppDataPath);

	// delete default directories
	const char* DefDirs[] = {"nzb", "tmp", "queue", "scripts"};
	for (int i=0; i < 4; i++)
	{
		snprintf(path, sizeof(path), "%s\\NZBGet\\%s", commonAppDataPath, DefDirs[i]);
		path[sizeof(path)-1] = '\0';

		// try to delete the directory
		int retry = 10;
		while (retry > 0 && Util::DirectoryExists(path) &&
			!Util::DeleteDirectoryWithContent(path, errBuf, sizeof(errBuf)))
		{
			usleep(200 * 1000);
			retry--;
		}

		if (Util::DirectoryExists(path))
		{
			snprintf(message, 1024, "Could not delete directory %s:\n%s.\nPlease delete the directory manually and try again.", path, errBuf);
			message[1024-1] = '\0';
			MessageBox(m_trayWindow, message, "NZBGet", MB_ICONERROR);
			return;
		}
	}

	// delete old config file in the program's directory
	snprintf(path, sizeof(path), "%s\\nzbget.conf", g_pOptions->GetAppDir());

	remove(path);
	Util::GetLastErrorMessage(errBuf, sizeof(errBuf));

	if (Util::FileExists(path))
	{
		snprintf(message, 1024, "Could not delete file %s:\n%s.\nPlease delete the file manually and try again.", path, errBuf);
		message[1024-1] = '\0';
		MessageBox(m_trayWindow, message, "NZBGet", MB_ICONERROR);
		return;
	}

	// delete config file in default directory
	snprintf(path, sizeof(path), "%s\\NZBGet\\nzbget.conf", commonAppDataPath);
	path[sizeof(path)-1] = '\0';

	remove(path);
	Util::GetLastErrorMessage(errBuf, sizeof(errBuf));


	if (Util::FileExists(path))
	{
		snprintf(message, 1024, "Could not delete file %s:\n%s.\nPlease delete the file manually and try again.", path, errBuf);
		message[1024-1] = '\0';
		MessageBox(m_trayWindow, message, "NZBGet", MB_ICONERROR);
		return;
	}

	// delete log files in default directory
	snprintf(path, sizeof(path), "%s\\NZBGet", commonAppDataPath);
	path[sizeof(path)-1] = '\0';

	DirBrowser dir(path);
	while (const char* filename = dir.Next())
	{
		if (Util::MatchFileExt(filename, ".log", ","))
		{
			char fullFilename[1024];
			snprintf(fullFilename, 1024, "%s%c%s", path, PATH_SEPARATOR, filename);
			fullFilename[1024-1] = '\0';

			remove(fullFilename);

			// ignore errors
		}
	}

	MessageBox(m_trayWindow, "The program has been reset to factory defaults.",
		"NZBGet", MB_ICONINFORMATION);

	mayStartBrowser = true;
	Reload();
}
