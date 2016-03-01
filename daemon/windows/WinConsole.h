/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2014-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
	WinConsole();
	~WinConsole();
	virtual void Stop();
	void InitAppMode();
	bool GetAppMode() { return m_appMode; }
	void SetupFirstStart();

protected:
	virtual void Run();

private:
	bool m_appMode;
	char** m_defaultArguments;
	char** m_initialArguments;
	int m_initialArgumentCount;
	HWND m_trayWindow;
	NOTIFYICONDATA* m_iconData;
	UINT m_taskbarCreatedMessage;
	HMENU m_menu;
	HINSTANCE m_instance;
	bool m_modal;
	HFONT m_linkFont;
	HFONT m_nameFont;
	HFONT m_titleFont;
	HCURSOR m_handCursor;
	HICON m_aboutIcon;
	HICON m_runningIcon;
	HICON m_idleIcon;
	HICON m_workingIcon;
	HICON m_pausedIcon;
	bool m_autostart;
	bool m_showTrayIcon;
	bool m_showConsole;
	bool m_showWebUI;
	bool m_autoParam;
	bool m_running;
	bool m_runningService;
	bool m_doubleClick;

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
	void SetupScripts();
	void ShowFactoryResetDialog();
	void ResetFactoryDefaults();

	static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType);
	static LRESULT CALLBACK TrayWndProcStat(HWND hwndWin, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT TrayWndProc(HWND hwndWin, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static BOOL CALLBACK AboutDialogProcStat(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	BOOL AboutDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static BOOL CALLBACK PrefsDialogProcStat(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	BOOL PrefsDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static BOOL CALLBACK RunningDialogProcStat(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	BOOL RunningDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static BOOL CALLBACK FactoryResetDialogProcStat(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	BOOL FactoryResetDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#endif
