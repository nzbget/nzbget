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


#ifndef WINCONSOLE_H
#define WINCONSOLE_H

#include "Thread.h"

class WinConsole : public Thread
{
private:
	bool					m_bAppMode;
	char**					m_pDefaultArguments;
	char**					m_pInitialArguments;
	int						m_iInitialArgumentCount;
	HWND					m_hTrayWindow;
	NOTIFYICONDATA*			m_pNidIcon;
	UINT					UM_TASKBARCREATED;
	HMENU					m_hMenu;
	HINSTANCE				m_hInstance;
	bool					m_bModal;
	HFONT					m_hLinkFont;
	HFONT					m_hNameFont;
	HFONT					m_hTitleFont;
	HCURSOR					m_hHandCursor;
	HICON					m_hAboutIcon;
	HICON					m_hRunningIcon;
	HICON					m_hIdleIcon;
	HICON					m_hWorkingIcon;
	HICON					m_hPausedIcon;
	bool					m_bAutostart;
	bool					m_bTray;
	bool					m_bConsole;
	bool					m_bWebUI;
	bool					m_bAutoParam;
	bool					m_bRunning;
	bool					m_bRunningService;

	void					CreateResources();
	void					CreateTrayIcon();
	void					ShowWebUI();
	void					ShowMenu();
	void					ShowInExplorer(const char* szFileName);
	void					ShowAboutBox();
	void					OpenConfigFileInTextEdit();
	void					ShowPrefsDialog();
	void					SavePrefs();
	void					LoadPrefs();
	void					ApplyPrefs();
	void					ShowRunningDialog();
	void					CheckRunning();
	void					UpdateTrayIcon();
	void					BuildMenu();
	void					ShowCategoryDir(int iCatIndex);
	void					SetupConfigFile();
	void					SetupScripts();
	void					ShowFactoryResetDialog();
	void					ResetFactoryDefaults();

	static BOOL WINAPI		ConsoleCtrlHandler(DWORD dwCtrlType);
	static LRESULT CALLBACK	TrayWndProcStat(HWND hwndWin, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT					TrayWndProc(HWND hwndWin, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static BOOL CALLBACK	AboutDialogProcStat(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	BOOL					AboutDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static BOOL CALLBACK	PrefsDialogProcStat(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	BOOL					PrefsDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static BOOL CALLBACK	RunningDialogProcStat(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	BOOL					RunningDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static BOOL CALLBACK	FactoryResetDialogProcStat(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	BOOL					FactoryResetDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

protected:
	virtual void			Run();

public:
							WinConsole();
							~WinConsole();
	virtual void			Stop();
	void					InitAppMode();
	bool					GetAppMode() { return m_bAppMode; }
	void					SetupFirstStart();
};

#endif
