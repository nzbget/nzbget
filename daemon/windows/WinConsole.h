/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2014-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef WINCONSOLE_H
#define WINCONSOLE_H

#include "Thread.h"

class WinConsole : public Thread
{
public:
	~WinConsole();
	virtual void Stop();
	void InitAppMode();
	bool GetAppMode() { return m_appMode; }
	void SetupFirstStart();

protected:
	virtual void Run();

private:
	bool m_appMode = false;
	char** m_defaultArguments = nullptr;
	char** m_initialArguments = nullptr;
	int m_initialArgumentCount = 0;
	HWND m_trayWindow = 0;
	NOTIFYICONDATA* m_iconData = nullptr;
	UINT m_taskbarCreatedMessage;
	HMENU m_menu;
	HINSTANCE m_instance;
	bool m_modal = false;
	HFONT m_linkFont;
	HFONT m_nameFont;
	HFONT m_titleFont;
	HCURSOR m_handCursor;
	HICON m_aboutIcon;
	HICON m_runningIcon;
	HICON m_idleIcon;
	HICON m_workingIcon;
	HICON m_pausedIcon;
	bool m_autostart = false;
	bool m_showTrayIcon = true;
	bool m_showConsole = false;
	bool m_showWebUI = true;
	bool m_autoParam = false;
	bool m_addParam = false;
	bool m_running = false;
	bool m_runningService = false;
	bool m_doubleClick = false;

	void CreateResources();
	void CreateTrayIcon();
	void ShowWebUI();
	void ShowMenu();
	void ShowInExplorer(const char* filename);
	void ShowAboutBox();
	void OpenConfigFileInTextEdit();
	void ShowPrefsDialog();
	void SavePrefs();
	void LoadPrefs();
	void ApplyPrefs();
	void ShowRunningDialog();
	void CheckRunning();
	void UpdateTrayIcon();
	void BuildMenu();
	void ShowCategoryDir(int catIndex);
	void SetupConfigFile();
	void ShowFactoryResetDialog();
	void ResetFactoryDefaults();
	void RunAnotherInstance();

	static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType);
	static LRESULT CALLBACK TrayWndProcStat(HWND hwndWin, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT TrayWndProc(HWND hwndWin, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK AboutDialogProcStat(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	INT_PTR AboutDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK PrefsDialogProcStat(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	INT_PTR PrefsDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK RunningDialogProcStat(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	INT_PTR RunningDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK FactoryResetDialogProcStat(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	INT_PTR FactoryResetDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#endif
