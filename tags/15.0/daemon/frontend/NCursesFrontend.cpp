/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#ifndef DISABLE_CURSES

#ifdef HAVE_NCURSES_H
#include <ncurses.h>
#endif
#ifdef HAVE_NCURSES_NCURSES_H
#include <ncurses/ncurses.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifndef WIN32
#include <unistd.h>
#endif

#include "nzbget.h"
#include "NCursesFrontend.h"
#include "Options.h"
#include "Util.h"

#ifdef HAVE_CURSES_H
// curses.h header must be included last to avoid problems on Solaris
// (and possibly other systems, that uses curses.h (not ncurses.h)
#include <curses.h>
// "#undef erase" is neccessary on Solaris
#undef erase
#endif

#ifndef WIN32
// curses.h on Solaris declares "clear()" via DEFINE. That causes problems, because
// it also affects calls to deque's method "clear()", producing compiler errors.
// We use function "curses_clear()" to call macro "clear" of curses, then
// undefine macro "clear".
void curses_clear()
{
    clear();
}
#undef clear
#endif

extern Options* g_pOptions;
extern void ExitProc();

static const int NCURSES_COLORPAIR_TEXT			= 1;
static const int NCURSES_COLORPAIR_INFO			= 2;
static const int NCURSES_COLORPAIR_WARNING		= 3;
static const int NCURSES_COLORPAIR_ERROR		= 4;
static const int NCURSES_COLORPAIR_DEBUG		= 5;
static const int NCURSES_COLORPAIR_DETAIL		= 6;
static const int NCURSES_COLORPAIR_STATUS		= 7;
static const int NCURSES_COLORPAIR_KEYBAR		= 8;
static const int NCURSES_COLORPAIR_INFOLINE		= 9;
static const int NCURSES_COLORPAIR_TEXTHIGHL	= 10;
static const int NCURSES_COLORPAIR_CURSOR		= 11;
static const int NCURSES_COLORPAIR_HINT			= 12;

static const int MAX_SCREEN_WIDTH				= 512;

#ifdef WIN32
static const int COLOR_BLACK	= 0;
static const int COLOR_BLUE		= FOREGROUND_BLUE;
static const int COLOR_RED		= FOREGROUND_RED;
static const int COLOR_GREEN	= FOREGROUND_GREEN;
static const int COLOR_WHITE	= FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN;
static const int COLOR_MAGENTA	= FOREGROUND_RED | FOREGROUND_BLUE;
static const int COLOR_CYAN		= FOREGROUND_BLUE | FOREGROUND_GREEN;
static const int COLOR_YELLOW	= FOREGROUND_RED | FOREGROUND_GREEN;

static const int READKEY_EMPTY = 0;

#define KEY_DOWN VK_DOWN
#define KEY_UP VK_UP
#define KEY_PPAGE VK_PRIOR
#define KEY_NPAGE VK_NEXT
#define KEY_END VK_END
#define KEY_HOME VK_HOME
#define KEY_BACKSPACE VK_BACK

#else

static const int READKEY_EMPTY = ERR;

#endif

NCursesFrontend::NCursesFrontend()
{
    m_iScreenHeight = 0;
    m_iScreenWidth = 0;
    m_iInputNumberIndex = 0;
    m_eInputMode = eNormal;
    m_bSummary = true;
    m_bFileList = true;
    m_iNeededLogEntries = 0;
	m_iQueueWinTop = 0;
	m_iQueueWinHeight = 0;
	m_iQueueWinClientHeight = 0;
	m_iMessagesWinTop = 0;
	m_iMessagesWinHeight = 0;
	m_iMessagesWinClientHeight = 0;
    m_iSelectedQueueEntry = 0;
	m_iQueueScrollOffset = 0;
	m_bShowNZBname = g_pOptions->GetCursesNZBName();
	m_bShowTimestamp = g_pOptions->GetCursesTime();
	m_bGroupFiles = g_pOptions->GetCursesGroup();
	m_QueueWindowPercentage = 0.5f;
	m_iDataUpdatePos = 0;
    m_bUpdateNextTime = false;
	m_iLastEditEntry = -1;
	m_bLastPausePars = false;
	m_szHint = NULL;

    // Setup curses
#ifdef WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	m_pScreenBuffer = NULL;
	m_pOldScreenBuffer = NULL;
	m_ColorAttr.clear();

	CONSOLE_CURSOR_INFO ConsoleCursorInfo;
	GetConsoleCursorInfo(hConsole, &ConsoleCursorInfo);
	ConsoleCursorInfo.bVisible = false;
	SetConsoleCursorInfo(hConsole, &ConsoleCursorInfo);
	if (IsRemoteMode())
	{
		SetConsoleTitle("NZBGet - remote mode");
	}
	else
	{
		SetConsoleTitle("NZBGet");
	}

	m_bUseColor = true;
#else
    m_pWindow = initscr();
    if (m_pWindow == NULL)
    {
        printf("ERROR: m_pWindow == NULL\n");
        exit(-1);
    }
	keypad(stdscr, true);
    nodelay((WINDOW*)m_pWindow, true);
    noecho();
    curs_set(0);
    m_bUseColor = has_colors();
#endif

    if (m_bUseColor)
    {
#ifndef WIN32
        start_color();
#endif
        init_pair(0,							COLOR_WHITE,	COLOR_BLUE);
        init_pair(NCURSES_COLORPAIR_TEXT,		COLOR_WHITE,	COLOR_BLACK);
        init_pair(NCURSES_COLORPAIR_INFO,		COLOR_GREEN,	COLOR_BLACK);
        init_pair(NCURSES_COLORPAIR_WARNING,	COLOR_MAGENTA,	COLOR_BLACK);
        init_pair(NCURSES_COLORPAIR_ERROR,		COLOR_RED,		COLOR_BLACK);
        init_pair(NCURSES_COLORPAIR_DEBUG,		COLOR_WHITE,	COLOR_BLACK);
        init_pair(NCURSES_COLORPAIR_DETAIL,		COLOR_GREEN,	COLOR_BLACK);
        init_pair(NCURSES_COLORPAIR_STATUS,		COLOR_BLUE,		COLOR_WHITE);
        init_pair(NCURSES_COLORPAIR_KEYBAR,		COLOR_WHITE,	COLOR_BLUE);
        init_pair(NCURSES_COLORPAIR_INFOLINE,	COLOR_WHITE,	COLOR_BLUE);
        init_pair(NCURSES_COLORPAIR_TEXTHIGHL,	COLOR_BLACK,	COLOR_CYAN);
        init_pair(NCURSES_COLORPAIR_CURSOR,		COLOR_BLACK,	COLOR_YELLOW);
        init_pair(NCURSES_COLORPAIR_HINT,		COLOR_WHITE,	COLOR_RED);
    }
}

NCursesFrontend::~NCursesFrontend()
{
#ifdef WIN32
	free(m_pScreenBuffer);
	free(m_pOldScreenBuffer);

	m_ColorAttr.clear();

	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_CURSOR_INFO ConsoleCursorInfo;
	GetConsoleCursorInfo(hConsole, &ConsoleCursorInfo);
	ConsoleCursorInfo.bVisible = true;
	SetConsoleCursorInfo(hConsole, &ConsoleCursorInfo);
#else
    keypad(stdscr, false);
    echo();
    curs_set(1);
    endwin();
#endif
    printf("\n");
	SetHint(NULL);
}

void NCursesFrontend::Run()
{
    debug("Entering NCursesFrontend-loop");

	m_iDataUpdatePos = 0;

    while (!IsStopped())
    {
        // The data (queue and log) is updated each m_iUpdateInterval msec,
        // but the window is updated more often for better reaction on user's input

		bool updateNow = false;
		int iKey = ReadConsoleKey();

		if (iKey != READKEY_EMPTY)
		{
			// Update now and next if a key is pressed.
			updateNow = true;
			m_bUpdateNextTime = true;
		} 
		else if (m_bUpdateNextTime)
		{
			// Update due to key being pressed during previous call.
			updateNow = true;
			m_bUpdateNextTime = false;
		} 
		else if (m_iDataUpdatePos <= 0) 
		{
			updateNow = true;
			m_bUpdateNextTime = false;
		}

		if (updateNow) 
		{
			Update(iKey);
		}

		if (m_iDataUpdatePos <= 0)
		{
			m_iDataUpdatePos = m_iUpdateInterval;
		}

		usleep(10 * 1000);
		m_iDataUpdatePos -= 10;
    }

    FreeData();
	
    debug("Exiting NCursesFrontend-loop");
}

void NCursesFrontend::NeedUpdateData()
{
	m_iDataUpdatePos = 10;
	m_bUpdateNextTime = true;
}

void NCursesFrontend::Update(int iKey)
{
    // Figure out how big the screen is
	CalcWindowSizes();

    if (m_iDataUpdatePos <= 0)
    {
        FreeData();
		m_iNeededLogEntries = m_iMessagesWinClientHeight;
        if (!PrepareData())
        {
            return;
        }

		// recalculate frame sizes
		CalcWindowSizes();
    }

	if (m_eInputMode == eEditQueue)
	{
		int iQueueSize = CalcQueueSize();
		if (iQueueSize == 0)
		{
			m_iSelectedQueueEntry = 0;
			m_eInputMode = eNormal;
		}
	}

    //------------------------------------------
    // Print Current NZBInfoList
    //------------------------------------------
	if (m_iQueueWinHeight > 0)
	{
		PrintQueue();
	}

    //------------------------------------------
    // Print Messages
    //------------------------------------------
	if (m_iMessagesWinHeight > 0)
	{
		PrintMessages();
	}

    PrintStatus();

	PrintKeyInputBar();

    UpdateInput(iKey);

	RefreshScreen();
}

void NCursesFrontend::CalcWindowSizes()
{
    int iNrRows, iNrColumns;
#ifdef WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO BufInfo;
	GetConsoleScreenBufferInfo(hConsole, &BufInfo);
	iNrRows = BufInfo.srWindow.Bottom - BufInfo.srWindow.Top + 1;
	iNrColumns = BufInfo.srWindow.Right - BufInfo.srWindow.Left + 1;
#else
    getmaxyx(stdscr, iNrRows, iNrColumns);
#endif
    if (iNrRows != m_iScreenHeight || iNrColumns != m_iScreenWidth)
    {
#ifdef WIN32
		m_iScreenBufferSize = iNrRows * iNrColumns * sizeof(CHAR_INFO);
		m_pScreenBuffer = (CHAR_INFO*)realloc(m_pScreenBuffer, m_iScreenBufferSize);
		memset(m_pScreenBuffer, 0, m_iScreenBufferSize);
		m_pOldScreenBuffer = (CHAR_INFO*)realloc(m_pOldScreenBuffer, m_iScreenBufferSize);
		memset(m_pOldScreenBuffer, 0, m_iScreenBufferSize);
#else
        curses_clear();
#endif
        m_iScreenHeight = iNrRows;
        m_iScreenWidth = iNrColumns;
    }

    int iQueueSize = CalcQueueSize();
    
	m_iQueueWinTop = 0;
	m_iQueueWinHeight = (int)((float) (m_iScreenHeight - 2) * m_QueueWindowPercentage);
	if (m_iQueueWinHeight - 1 > iQueueSize)
	{
		m_iQueueWinHeight = iQueueSize > 0 ? iQueueSize + 1 : 1 + 1;
	}
	m_iQueueWinClientHeight = m_iQueueWinHeight - 1;
	if (m_iQueueWinClientHeight < 0)
	{
		m_iQueueWinClientHeight = 0;
	}

	m_iMessagesWinTop = m_iQueueWinTop + m_iQueueWinHeight;
	m_iMessagesWinHeight = m_iScreenHeight - m_iQueueWinHeight - 2;
	m_iMessagesWinClientHeight = m_iMessagesWinHeight - 1;
	if (m_iMessagesWinClientHeight < 0)
	{
		m_iMessagesWinClientHeight = 0;
	}
}

int NCursesFrontend::CalcQueueSize()
{
	int iQueueSize = 0;
	DownloadQueue* pDownloadQueue = LockQueue();
	if (m_bGroupFiles)
	{
		iQueueSize = pDownloadQueue->GetQueue()->size();
	}
	else
	{
		for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
		{
			NZBInfo* pNZBInfo = *it;
			iQueueSize += pNZBInfo->GetFileList()->size();
		}
	}
	UnlockQueue();
	return iQueueSize;
}

void NCursesFrontend::PlotLine(const char * szString, int iRow, int iPos, int iColorPair)
{
    char szBuffer[MAX_SCREEN_WIDTH];
    snprintf(szBuffer, sizeof(szBuffer), "%-*s", m_iScreenWidth, szString);
	szBuffer[MAX_SCREEN_WIDTH - 1] = '\0';
	int iLen = strlen(szBuffer);
	if (iLen > m_iScreenWidth - iPos && m_iScreenWidth - iPos < MAX_SCREEN_WIDTH)
	{
		szBuffer[m_iScreenWidth - iPos] = '\0';
	}

	PlotText(szBuffer, iRow, iPos, iColorPair, false);
}

void NCursesFrontend::PlotText(const char * szString, int iRow, int iPos, int iColorPair, bool bBlink)
{
#ifdef WIN32
	int iBufPos = iRow * m_iScreenWidth + iPos;
	int len = strlen(szString);
	for (int i = 0; i < len; i++)
	{
		char c = szString[i];
		CharToOemBuff(&c, &c, 1);
		m_pScreenBuffer[iBufPos + i].Char.AsciiChar = c;
		m_pScreenBuffer[iBufPos + i].Attributes = m_ColorAttr[iColorPair];
	}
#else
	if( m_bUseColor ) 
	{
		attron(COLOR_PAIR(iColorPair));
		if (bBlink)
		{
			attron(A_BLINK);
		}
	}
    mvaddstr(iRow, iPos, (char*)szString);
	if( m_bUseColor ) 
	{
		attroff(COLOR_PAIR(iColorPair));
		if (bBlink)
		{
			attroff(A_BLINK);
		}
	}
#endif
}

void NCursesFrontend::RefreshScreen()
{
#ifdef WIN32
	bool bBufChanged = memcmp(m_pScreenBuffer, m_pOldScreenBuffer, m_iScreenBufferSize);
	if (bBufChanged)
	{
		COORD BufSize;
		BufSize.X = m_iScreenWidth;
		BufSize.Y = m_iScreenHeight;

		COORD BufCoord;
		BufCoord.X = 0;
		BufCoord.Y = 0;

		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		CONSOLE_SCREEN_BUFFER_INFO BufInfo;
		GetConsoleScreenBufferInfo(hConsole, &BufInfo);
		WriteConsoleOutput(hConsole, m_pScreenBuffer, BufSize, BufCoord, &BufInfo.srWindow);

		BufInfo.dwCursorPosition.X = BufInfo.srWindow.Right;
		BufInfo.dwCursorPosition.Y = BufInfo.srWindow.Bottom;
		SetConsoleCursorPosition(hConsole, BufInfo.dwCursorPosition);

		memcpy(m_pOldScreenBuffer, m_pScreenBuffer, m_iScreenBufferSize);
	}
#else
    // Cursor placement
    wmove((WINDOW*)m_pWindow, m_iScreenHeight, m_iScreenWidth);

    // NCurses refresh
    refresh();
#endif
}

#ifdef WIN32
void NCursesFrontend::init_pair(int iColorNumber, WORD wForeColor, WORD wBackColor)
{
	m_ColorAttr.resize(iColorNumber + 1);
	m_ColorAttr[iColorNumber] = wForeColor | (wBackColor << 4);
}
#endif

void NCursesFrontend::PrintMessages()
{
	int iLineNr = m_iMessagesWinTop;

	char szBuffer[MAX_SCREEN_WIDTH];
	snprintf(szBuffer, sizeof(szBuffer), "%s Messages", m_bUseColor ? "" : "*** ");
	szBuffer[MAX_SCREEN_WIDTH - 1] = '\0';
    PlotLine(szBuffer, iLineNr++, 0, NCURSES_COLORPAIR_INFOLINE);
	
    int iLine = iLineNr + m_iMessagesWinClientHeight - 1;
	int iLinesToPrint = m_iMessagesWinClientHeight;

    MessageList* pMessages = LockMessages();
    
	// print messages from bottom
	for (int i = (int)pMessages->size() - 1; i >= 0 && iLinesToPrint > 0; i--)
    {
        int iPrintedLines = PrintMessage((*pMessages)[i], iLine, iLinesToPrint);
		iLine -= iPrintedLines;
		iLinesToPrint -= iPrintedLines;
    }

	if (iLinesToPrint > 0)
	{
		// too few messages, print them again from top
    	iLine = iLineNr + m_iMessagesWinClientHeight - 1;
		while (iLinesToPrint-- > 0)
		{
			PlotLine("", iLine--, 0, NCURSES_COLORPAIR_TEXT);
		}
		int iLinesToPrint2 = m_iMessagesWinClientHeight;
		for (int i = (int)pMessages->size() - 1; i >= 0 && iLinesToPrint2 > 0; i--)
		{
			int iPrintedLines = PrintMessage((*pMessages)[i], iLine, iLinesToPrint2);
			iLine -= iPrintedLines;
			iLinesToPrint2 -= iPrintedLines;
		}
	}
	
    UnlockMessages();
}

int NCursesFrontend::PrintMessage(Message* Msg, int iRow, int iMaxLines)
{
    const char* szMessageType[] = { "INFO    ", "WARNING ", "ERROR   ", "DEBUG   ", "DETAIL  "};
    const int iMessageTypeColor[] = { NCURSES_COLORPAIR_INFO, NCURSES_COLORPAIR_WARNING,
    	NCURSES_COLORPAIR_ERROR, NCURSES_COLORPAIR_DEBUG, NCURSES_COLORPAIR_DETAIL };

	char* szText = (char*)Msg->GetText();

	if (m_bShowTimestamp)
	{
		int iLen = strlen(szText) + 50;
		szText = (char*)malloc(iLen);

		time_t rawtime = Msg->GetTime();
		rawtime += g_pOptions->GetTimeCorrection();

		char szTime[50];
#ifdef HAVE_CTIME_R_3
		ctime_r(&rawtime, szTime, 50);
#else
		ctime_r(&rawtime, szTime);
#endif
		szTime[50-1] = '\0';
		szTime[strlen(szTime) - 1] = '\0'; // trim LF

		snprintf(szText, iLen, "%s - %s", szTime, Msg->GetText());
		szText[iLen - 1] = '\0';
	}
	else
	{
		szText = strdup(szText);
	}

	// replace some special characters with spaces
	for (char* p = szText; *p; p++)
	{
		if (*p == '\n' || *p == '\r' || *p == '\b')
		{
			*p = ' ';
		}
	}

	int iLen = strlen(szText);
	int iWinWidth = m_iScreenWidth - 8;
	int iMsgLines = iLen / iWinWidth;
	if (iLen % iWinWidth > 0)
	{
		iMsgLines++;
	}
	
	int iLines = 0;
	for (int i = iMsgLines - 1; i >= 0 && iLines < iMaxLines; i--)
	{
		int iR = iRow - iMsgLines + i + 1;
		PlotLine(szText + iWinWidth * i, iR, 8, NCURSES_COLORPAIR_TEXT);
		if (i == 0)
		{
			PlotText(szMessageType[Msg->GetKind()], iR, 0, iMessageTypeColor[Msg->GetKind()], false);
		}
		else
		{
			PlotText("        ", iR, 0, iMessageTypeColor[Msg->GetKind()], false);
		}
		iLines++;
	}

	free(szText);

	return iLines;
}

void NCursesFrontend::PrintStatus()
{
    char tmp[MAX_SCREEN_WIDTH];
    int iStatusRow = m_iScreenHeight - 2;

    char timeString[100];
    timeString[0] = '\0';

	int iCurrentDownloadSpeed = m_bStandBy ? 0 : m_iCurrentDownloadSpeed;
    if (iCurrentDownloadSpeed > 0 && !m_bPauseDownload)
    {
        long long remain_sec = (long long)(m_lRemainingSize / iCurrentDownloadSpeed);
		int h = (int)(remain_sec / 3600);
		int m = (int)((remain_sec % 3600) / 60);
		int s = (int)(remain_sec % 60);
		sprintf(timeString, " (~ %.2d:%.2d:%.2d)", h, m, s);
    }

    char szDownloadLimit[128];
    if (m_iDownloadLimit > 0)
    {
        sprintf(szDownloadLimit, ", Limit %.0f KB/s", (float)m_iDownloadLimit / 1024.0);
    }
    else
    {
        szDownloadLimit[0] = 0;
    }

    char szPostStatus[128];
    if (m_iPostJobCount > 0)
    {
        sprintf(szPostStatus, ", %i post-job%s", m_iPostJobCount, m_iPostJobCount > 1 ? "s" : "");
    }
    else
    {
        szPostStatus[0] = 0;
    }

	float fAverageSpeed = (float)(Util::Int64ToFloat(m_iDnTimeSec > 0 ? m_iAllBytes / m_iDnTimeSec : 0) / 1024.0);

	snprintf(tmp, MAX_SCREEN_WIDTH, " %d threads, %.*f KB/s, %.2f MB remaining%s%s%s%s, Avg. %.*f KB/s", 
		m_iThreadCount, (iCurrentDownloadSpeed >= 10*1024 ? 0 : 1), (float)iCurrentDownloadSpeed / 1024.0, 
		(float)(Util::Int64ToFloat(m_lRemainingSize) / 1024.0 / 1024.0), timeString, szPostStatus, 
		m_bPauseDownload ? (m_bStandBy ? ", Paused" : ", Pausing") : "",
		szDownloadLimit, (fAverageSpeed >= 10 ? 0 : 1), fAverageSpeed);
	tmp[MAX_SCREEN_WIDTH - 1] = '\0';
    PlotLine(tmp, iStatusRow, 0, NCURSES_COLORPAIR_STATUS);
}

void NCursesFrontend::PrintKeyInputBar()
{
    int iQueueSize = CalcQueueSize();
	int iInputBarRow = m_iScreenHeight - 1;
	
	if (m_szHint)
	{
		time_t tTime = time(NULL);
		if (tTime - m_tStartHint < 5)
		{
			PlotLine(m_szHint, iInputBarRow, 0, NCURSES_COLORPAIR_HINT);
			return;
		}
		else
		{
			SetHint(NULL);
		}
	}

    switch (m_eInputMode)
    {
    case eNormal:
		if (m_bGroupFiles)
		{
	        PlotLine("(Q)uit | (E)dit | (P)ause | (R)ate | (W)indow | (G)roup | (T)ime", iInputBarRow, 0, NCURSES_COLORPAIR_KEYBAR);
		}
		else
		{
	        PlotLine("(Q)uit | (E)dit | (P)ause | (R)ate | (W)indow | (G)roup | (T)ime | n(Z)b", iInputBarRow, 0, NCURSES_COLORPAIR_KEYBAR);
		}
        break;
    case eEditQueue:
    {
		const char* szStatus = NULL;
		if (m_iSelectedQueueEntry > 0 && iQueueSize > 1 && m_iSelectedQueueEntry == iQueueSize - 1)
		{
			szStatus = "(Q)uit | (E)xit | (P)ause | (D)elete | (U)p/(T)op";
		}
		else if (iQueueSize > 1 && m_iSelectedQueueEntry == 0)
		{
			szStatus = "(Q)uit | (E)xit | (P)ause | (D)elete | dow(N)/(B)ottom";
		}
		else if (iQueueSize > 1)
		{
			szStatus = "(Q)uit | (E)xit | (P)ause | (D)elete | (U)p/dow(N)/(T)op/(B)ottom";
		}
		else
		{
			szStatus = "(Q)uit | (E)xit | (P)ause | (D)elete";
		}

		PlotLine(szStatus, iInputBarRow, 0, NCURSES_COLORPAIR_KEYBAR);
        break;
    }
    case eDownloadRate:
        char szString[128];
        snprintf(szString, 128, "Download rate: %i", m_iInputValue);
		szString[128-1] = '\0';
        PlotLine(szString, iInputBarRow, 0, NCURSES_COLORPAIR_KEYBAR);
        // Print the cursor
		PlotText(" ", iInputBarRow, 15 + m_iInputNumberIndex, NCURSES_COLORPAIR_CURSOR, true);
        break;
    }
}

void NCursesFrontend::SetHint(const char* szHint)
{
	free(m_szHint);
	m_szHint = NULL;
	if (szHint)
	{
		m_szHint = strdup(szHint);
		m_tStartHint = time(NULL);
	}
}

void NCursesFrontend::PrintQueue()
{
	if (m_bGroupFiles)
	{
		PrintGroupQueue();
	}
	else
	{
		PrintFileQueue();
	}
}

void NCursesFrontend::PrintFileQueue()
{
    DownloadQueue* pDownloadQueue = LockQueue();

	int iLineNr = m_iQueueWinTop + 1;
	long long lRemaining = 0;
	long long lPaused = 0;
	int iPausedFiles = 0;
	int iFileNum = 0;

	for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
	{
		NZBInfo* pNZBInfo = *it;
		for (FileList::iterator it2 = pNZBInfo->GetFileList()->begin(); it2 != pNZBInfo->GetFileList()->end(); it2++, iFileNum++)
		{
			FileInfo* pFileInfo = *it2;

			if (iFileNum >= m_iQueueScrollOffset && iFileNum < m_iQueueScrollOffset + m_iQueueWinHeight -1)
			{
				PrintFilename(pFileInfo, iLineNr++, iFileNum == m_iSelectedQueueEntry);
			}

			if (pFileInfo->GetPaused())
			{
				iPausedFiles++;
				lPaused += pFileInfo->GetRemainingSize();
			}
			lRemaining += pFileInfo->GetRemainingSize();
		}
	}

	if (iFileNum > 0)
	{
		char szRemaining[20];
		Util::FormatFileSize(szRemaining, sizeof(szRemaining), lRemaining);

		char szUnpaused[20];
		Util::FormatFileSize(szUnpaused, sizeof(szUnpaused), lRemaining - lPaused);
		
		char szBuffer[MAX_SCREEN_WIDTH];
		snprintf(szBuffer, sizeof(szBuffer), " %sFiles for downloading - %i / %i files in queue - %s / %s", 
			m_bUseColor ? "" : "*** ", iFileNum,
			iFileNum - iPausedFiles, szRemaining, szUnpaused);
		szBuffer[MAX_SCREEN_WIDTH - 1] = '\0';
		PrintTopHeader(szBuffer, m_iQueueWinTop, true);
    }
	else
	{
		iLineNr--;
		char szBuffer[MAX_SCREEN_WIDTH];
		snprintf(szBuffer, sizeof(szBuffer), "%s Files for downloading", m_bUseColor ? "" : "*** ");
		szBuffer[MAX_SCREEN_WIDTH - 1] = '\0';
		PrintTopHeader(szBuffer, iLineNr++, true);
        PlotLine("Ready to receive nzb-job", iLineNr++, 0, NCURSES_COLORPAIR_TEXT);
	}

    UnlockQueue();
}

void NCursesFrontend::PrintFilename(FileInfo * pFileInfo, int iRow, bool bSelected)
{
	int color = 0;
	const char* Brace1 = "[";
	const char* Brace2 = "]";
	if (m_eInputMode == eEditQueue && bSelected)
	{
		color = NCURSES_COLORPAIR_TEXTHIGHL;
		if (!m_bUseColor)
		{
			Brace1 = "<";
			Brace2 = ">";
		}
	}
	else
	{
		color = NCURSES_COLORPAIR_TEXT;
	}

	const char* szDownloading = "";
	if (pFileInfo->GetActiveDownloads() > 0)
	{
		szDownloading = " *";
	}

	char szPriority[100];
	szPriority[0] = '\0';
	if (pFileInfo->GetNZBInfo()->GetPriority() != 0)
	{
		sprintf(szPriority, " [%+i]", pFileInfo->GetNZBInfo()->GetPriority());
	}

	char szCompleted[20];
	szCompleted[0] = '\0';
	if (pFileInfo->GetRemainingSize() < pFileInfo->GetSize())
	{
		sprintf(szCompleted, ", %i%%", (int)(100 - Util::Int64ToFloat(pFileInfo->GetRemainingSize()) * 100.0 / Util::Int64ToFloat(pFileInfo->GetSize())));
	}

	char szNZBNiceName[1024];
	if (m_bShowNZBname)
	{
		strncpy(szNZBNiceName, pFileInfo->GetNZBInfo()->GetName(), 1023);
		int len = strlen(szNZBNiceName);
		szNZBNiceName[len] = PATH_SEPARATOR;
		szNZBNiceName[len + 1] = '\0';
	}
	else
	{
		szNZBNiceName[0] = '\0';
	}

	char szBuffer[MAX_SCREEN_WIDTH];
	snprintf(szBuffer, MAX_SCREEN_WIDTH, "%s%i%s%s%s %s%s (%.2f MB%s)%s", Brace1, pFileInfo->GetID(),
		Brace2, szPriority, szDownloading, szNZBNiceName, pFileInfo->GetFilename(), 
		(float)(Util::Int64ToFloat(pFileInfo->GetSize()) / 1024.0 / 1024.0),
		szCompleted, pFileInfo->GetPaused() ? " (paused)" : "");
	szBuffer[MAX_SCREEN_WIDTH - 1] = '\0';

	PlotLine(szBuffer, iRow, 0, color);
}

void NCursesFrontend::PrintTopHeader(char* szHeader, int iLineNr, bool bUpTime)
{
    char szBuffer[MAX_SCREEN_WIDTH];
    snprintf(szBuffer, sizeof(szBuffer), "%-*s", m_iScreenWidth, szHeader);
	szBuffer[MAX_SCREEN_WIDTH - 1] = '\0';
	int iHeaderLen = strlen(szHeader);
	int iCharsLeft = m_iScreenWidth - iHeaderLen - 2;

	int iTime = bUpTime ? m_iUpTimeSec : m_iDnTimeSec;
	int d = iTime / 3600 / 24;
	int h = (iTime % (3600 * 24)) / 3600;
	int m = (iTime % 3600) / 60;
	int s = iTime % 60;
	char szTime[30];

	if (d == 0)
	{
		snprintf(szTime, 30, "%.2d:%.2d:%.2d", h, m, s);
		if ((int)strlen(szTime) > iCharsLeft)
		{
			snprintf(szTime, 30, "%.2d:%.2d", h, m);
		}
	}
	else 
	{
		snprintf(szTime, 30, "%i %s %.2d:%.2d:%.2d", d, (d == 1 ? "day" : "days"), h, m, s);
		if ((int)strlen(szTime) > iCharsLeft)
		{
			snprintf(szTime, 30, "%id %.2d:%.2d:%.2d", d, h, m, s);
		}
		if ((int)strlen(szTime) > iCharsLeft)
		{
			snprintf(szTime, 30, "%id %.2d:%.2d", d, h, m);
		}
	}

	szTime[29] = '\0';
	const char* szShortCap = bUpTime ? " Up " : "Dn ";
	const char* szLongCap = bUpTime ? " Uptime " : " Download-time ";

	int iTimeLen = strlen(szTime);
	int iShortCapLen = strlen(szShortCap);
	int iLongCapLen = strlen(szLongCap);

	if (iCharsLeft - iTimeLen - iLongCapLen >= 0)
	{
		snprintf(szBuffer + m_iScreenWidth - iTimeLen - iLongCapLen, MAX_SCREEN_WIDTH - (m_iScreenWidth - iTimeLen - iLongCapLen), "%s%s", szLongCap, szTime);
	}
	else if (iCharsLeft - iTimeLen - iShortCapLen >= 0)
	{
		snprintf(szBuffer + m_iScreenWidth - iTimeLen - iShortCapLen, MAX_SCREEN_WIDTH - (m_iScreenWidth - iTimeLen - iShortCapLen), "%s%s", szShortCap, szTime);
	}
	else if (iCharsLeft - iTimeLen >= 0)
	{
		snprintf(szBuffer + m_iScreenWidth - iTimeLen, MAX_SCREEN_WIDTH - (m_iScreenWidth - iTimeLen), "%s", szTime);
	}

    PlotLine(szBuffer, iLineNr, 0, NCURSES_COLORPAIR_INFOLINE);
}

void NCursesFrontend::PrintGroupQueue()
{
	int iLineNr = m_iQueueWinTop;

    DownloadQueue* pDownloadQueue = LockQueue();
	if (pDownloadQueue->GetQueue()->empty())
    {
		char szBuffer[MAX_SCREEN_WIDTH];
		snprintf(szBuffer, sizeof(szBuffer), "%s NZBs for downloading", m_bUseColor ? "" : "*** ");
		szBuffer[MAX_SCREEN_WIDTH - 1] = '\0';
		PrintTopHeader(szBuffer, iLineNr++, false);
        PlotLine("Ready to receive nzb-job", iLineNr++, 0, NCURSES_COLORPAIR_TEXT);
    }
    else
    {
		iLineNr++;

		ResetColWidths();
		int iCalcLineNr = iLineNr;
		int i = 0;
        for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++, i++)
        {
            NZBInfo* pNZBInfo = *it;
			if (i >= m_iQueueScrollOffset && i < m_iQueueScrollOffset + m_iQueueWinHeight -1)
			{
				PrintGroupname(pNZBInfo, iCalcLineNr++, false, true);
			}
        }

		long long lRemaining = 0;
		long long lPaused = 0;
		i = 0;
        for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++, i++)
        {
            NZBInfo* pNZBInfo = *it;
			if (i >= m_iQueueScrollOffset && i < m_iQueueScrollOffset + m_iQueueWinHeight -1)
			{
				PrintGroupname(pNZBInfo, iLineNr++, i == m_iSelectedQueueEntry, false);
			}
			lRemaining += pNZBInfo->GetRemainingSize();
			lPaused += pNZBInfo->GetPausedSize();
        }
		
		char szRemaining[20];
		Util::FormatFileSize(szRemaining, sizeof(szRemaining), lRemaining);

		char szUnpaused[20];
		Util::FormatFileSize(szUnpaused, sizeof(szUnpaused), lRemaining - lPaused);
		
		char szBuffer[MAX_SCREEN_WIDTH];
		snprintf(szBuffer, sizeof(szBuffer), " %sNZBs for downloading - %i NZBs in queue - %s / %s", 
			m_bUseColor ? "" : "*** ", (int)pDownloadQueue->GetQueue()->size(), szRemaining, szUnpaused);
		szBuffer[MAX_SCREEN_WIDTH - 1] = '\0';
		PrintTopHeader(szBuffer, m_iQueueWinTop, false);
    }
    UnlockQueue();
}

void NCursesFrontend::ResetColWidths()
{
	m_iColWidthFiles = 0;
	m_iColWidthTotal = 0;
	m_iColWidthLeft = 0;
}

void NCursesFrontend::PrintGroupname(NZBInfo* pNZBInfo, int iRow, bool bSelected, bool bCalcColWidth)
{
	int color = NCURSES_COLORPAIR_TEXT;
	char chBrace1 = '[';
	char chBrace2 = ']';
	if (m_eInputMode == eEditQueue && bSelected)
	{
		color = NCURSES_COLORPAIR_TEXTHIGHL;
		if (!m_bUseColor)
		{
			chBrace1 = '<';
			chBrace2 = '>';
		}
	}

	const char* szDownloading = "";
	if (pNZBInfo->GetActiveDownloads() > 0)
	{
		szDownloading = " *";
	}

	long long lUnpausedRemainingSize = pNZBInfo->GetRemainingSize() - pNZBInfo->GetPausedSize();

	char szRemaining[20];
	Util::FormatFileSize(szRemaining, sizeof(szRemaining), lUnpausedRemainingSize);

	char szPriority[100];
	szPriority[0] = '\0';
	if (pNZBInfo->GetPriority() != 0)
	{
		sprintf(szPriority, " [%+i]", pNZBInfo->GetPriority());
	}

	char szBuffer[MAX_SCREEN_WIDTH];

	// Format:
	// [id - id] Name   Left-Files/Paused     Total      Left     Time
	// [1-2] Nzb-name             999/999 999.99 MB 999.99 MB 00:00:00

	int iNameLen = 0;
	if (bCalcColWidth)
	{
		iNameLen = m_iScreenWidth - 1 - 9 - 11 - 11 - 9;
	}
	else
	{
		iNameLen = m_iScreenWidth - 1 - m_iColWidthFiles - 2 - m_iColWidthTotal - 2 - m_iColWidthLeft - 2 - 9;
	}

	bool bPrintFormatted = iNameLen > 20;

	if (bPrintFormatted)
	{
		char szFiles[20];
		snprintf(szFiles, 20, "%i/%i", (int)pNZBInfo->GetFileList()->size(), pNZBInfo->GetPausedFileCount());
		szFiles[20-1] = '\0';
		
		char szTotal[20];
		Util::FormatFileSize(szTotal, sizeof(szTotal), pNZBInfo->GetSize());

		char szNameWithIds[1024];
		snprintf(szNameWithIds, 1024, "%c%i%c%s%s %s", chBrace1, pNZBInfo->GetID(), chBrace2, 
			szPriority, szDownloading, pNZBInfo->GetName());
		szNameWithIds[iNameLen] = '\0';

		char szTime[100];
		szTime[0] = '\0';
		int iCurrentDownloadSpeed = m_bStandBy ? 0 : m_iCurrentDownloadSpeed;
		if (pNZBInfo->GetPausedSize() > 0 && lUnpausedRemainingSize == 0)
		{
			snprintf(szTime, 100, "[paused]");
			Util::FormatFileSize(szRemaining, sizeof(szRemaining), pNZBInfo->GetRemainingSize());
		}
		else if (iCurrentDownloadSpeed > 0 && !m_bPauseDownload)
		{
			long long remain_sec = (long long)(lUnpausedRemainingSize / iCurrentDownloadSpeed);
			int h = (int)(remain_sec / 3600);
			int m = (int)((remain_sec % 3600) / 60);
			int s = (int)(remain_sec % 60);
			if (h < 100)
			{
				snprintf(szTime, 100, "%.2d:%.2d:%.2d", h, m, s);
			}
			else
			{
				snprintf(szTime, 100, "99:99:99");
			}
		}

		if (bCalcColWidth)
		{
			int iColWidthFiles = strlen(szFiles);
			m_iColWidthFiles = iColWidthFiles > m_iColWidthFiles ? iColWidthFiles : m_iColWidthFiles;

			int iColWidthTotal = strlen(szTotal);
			m_iColWidthTotal = iColWidthTotal > m_iColWidthTotal ? iColWidthTotal : m_iColWidthTotal;

			int iColWidthLeft = strlen(szRemaining);
			m_iColWidthLeft = iColWidthLeft > m_iColWidthLeft ? iColWidthLeft : m_iColWidthLeft;
		}
		else
		{
			snprintf(szBuffer, MAX_SCREEN_WIDTH, "%-*s  %*s  %*s  %*s  %8s", iNameLen, szNameWithIds, m_iColWidthFiles, szFiles, m_iColWidthTotal, szTotal, m_iColWidthLeft, szRemaining, szTime);
		}
	}
	else
	{
		snprintf(szBuffer, MAX_SCREEN_WIDTH, "%c%i%c%s %s", chBrace1, pNZBInfo->GetID(), 
			chBrace2, szDownloading, pNZBInfo->GetName());
	}

	szBuffer[MAX_SCREEN_WIDTH - 1] = '\0';

	if (!bCalcColWidth)
	{
		PlotLine(szBuffer, iRow, 0, color);
	}
}

bool NCursesFrontend::EditQueue(DownloadQueue::EEditAction eAction, int iOffset)
{
	int ID = 0;

	if (m_bGroupFiles)
	{
		DownloadQueue* pDownloadQueue = LockQueue();
		if (m_iSelectedQueueEntry >= 0 && m_iSelectedQueueEntry < (int)pDownloadQueue->GetQueue()->size())
		{
			NZBInfo* pNZBInfo = pDownloadQueue->GetQueue()->at(m_iSelectedQueueEntry);
			ID = pNZBInfo->GetID();
			if (eAction == DownloadQueue::eaFilePause)
			{
				if (pNZBInfo->GetRemainingSize() == pNZBInfo->GetPausedSize())
				{
					eAction = DownloadQueue::eaFileResume;
				}
				else if (pNZBInfo->GetPausedSize() == 0 && (pNZBInfo->GetRemainingParCount() > 0) &&
					!(m_bLastPausePars && m_iLastEditEntry == m_iSelectedQueueEntry))
				{
					eAction = DownloadQueue::eaFilePauseExtraPars;
					m_bLastPausePars = true;
				}
				else
				{
					eAction = DownloadQueue::eaFilePause;
					m_bLastPausePars = false;
				}
			}
		}
		UnlockQueue();

		// map file-edit-actions to group-edit-actions
		 DownloadQueue::EEditAction FileToGroupMap[] = {
			(DownloadQueue::EEditAction)0,
			DownloadQueue::eaGroupMoveOffset, 
			DownloadQueue::eaGroupMoveTop, 
			DownloadQueue::eaGroupMoveBottom, 
			DownloadQueue::eaGroupPause, 
			DownloadQueue::eaGroupResume, 
			DownloadQueue::eaGroupDelete,
			DownloadQueue::eaGroupPauseAllPars,
			DownloadQueue::eaGroupPauseExtraPars };
		eAction = FileToGroupMap[eAction];
	}
	else
	{
		DownloadQueue* pDownloadQueue = LockQueue();

		int iFileNum = 0;
		for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
		{
			NZBInfo* pNZBInfo = *it;
			for (FileList::iterator it2 = pNZBInfo->GetFileList()->begin(); it2 != pNZBInfo->GetFileList()->end(); it2++, iFileNum++)
			{
				if (m_iSelectedQueueEntry == iFileNum)
				{
					FileInfo* pFileInfo = *it2;
					ID = pFileInfo->GetID();
					if (eAction == DownloadQueue::eaFilePause)
					{
						eAction = !pFileInfo->GetPaused() ? DownloadQueue::eaFilePause : DownloadQueue::eaFileResume;
					}
				}
			}
		}

		UnlockQueue();
	}

	m_iLastEditEntry = m_iSelectedQueueEntry;

	NeedUpdateData();

	if (ID != 0)
	{
		return ServerEditQueue(eAction, iOffset, ID);
	}
	else
	{
		return false;
	}
}

void NCursesFrontend::SetCurrentQueueEntry(int iEntry)
{
    int iQueueSize = CalcQueueSize();

	if (iEntry < 0)
	{
		iEntry = 0;
	}
	else if (iEntry > iQueueSize - 1)
	{
		iEntry = iQueueSize - 1;
	}

	if (iEntry > m_iQueueScrollOffset + m_iQueueWinClientHeight ||
	   iEntry < m_iQueueScrollOffset - m_iQueueWinClientHeight)
	{
		m_iQueueScrollOffset = iEntry - m_iQueueWinClientHeight / 2;
	}
	else if (iEntry < m_iQueueScrollOffset)
	{
		m_iQueueScrollOffset -= m_iQueueWinClientHeight;
	}
	else if (iEntry >= m_iQueueScrollOffset + m_iQueueWinClientHeight)
	{
		m_iQueueScrollOffset += m_iQueueWinClientHeight;
	}
	
	if (m_iQueueScrollOffset > iQueueSize - m_iQueueWinClientHeight)
	{
		m_iQueueScrollOffset = iQueueSize - m_iQueueWinClientHeight;
	}
	if (m_iQueueScrollOffset < 0)
	{
		m_iQueueScrollOffset = 0;
	}
	
	m_iSelectedQueueEntry = iEntry;
}


/*
 * Process keystrokes starting with the initialKey, which must not be
 * READKEY_EMPTY but has alread been set via ReadConsoleKey.
 */
void NCursesFrontend::UpdateInput(int initialKey)
{
	int iKey = initialKey;
	while (iKey != READKEY_EMPTY)
    {
		int iQueueSize = CalcQueueSize();

		// Normal or edit queue mode
		if (m_eInputMode == eNormal || m_eInputMode == eEditQueue)
		{
			switch (iKey)
			{
			case 'q':
				// Key 'q' for quit
				ExitProc();
				break;
			case 'z':
				// show/hide NZBFilename
				m_bShowNZBname = !m_bShowNZBname;
				break;
			case 'w':
				// swicth window sizes
				if (m_QueueWindowPercentage == 0.5)
				{
					m_QueueWindowPercentage = 1;
				}
				else if (m_QueueWindowPercentage == 1 && m_eInputMode != eEditQueue)
				{
					m_QueueWindowPercentage = 0;
				}
				else 
				{
					m_QueueWindowPercentage = 0.5;
				}
				CalcWindowSizes();
				SetCurrentQueueEntry(m_iSelectedQueueEntry);
				break;
			case 'g':
				// group/ungroup files
				m_bGroupFiles = !m_bGroupFiles;
				SetCurrentQueueEntry(m_iSelectedQueueEntry);
				NeedUpdateData();
				break;
			}
		}
		
		// Normal mode
		if (m_eInputMode == eNormal)
		{
			switch (iKey)
			{
			case 'p':
				// Key 'p' for pause
				if (!IsRemoteMode())
				{
					info(m_bPauseDownload ? "Unpausing download" : "Pausing download");
				}
				ServerPauseUnpause(!m_bPauseDownload);
				break;
			case 'e':
			case 10: // return
			case 13: // enter
				if (iQueueSize > 0)
				{
					m_eInputMode = eEditQueue;
					if (m_QueueWindowPercentage == 0)
					{
						m_QueueWindowPercentage = 0.5;
					}
					return;
				}
				break;
			case 'r':
				// Download rate
				m_eInputMode = eDownloadRate;
				m_iInputNumberIndex = 0;
				m_iInputValue = 0;
				return;
			case 't':
				// show/hide Timestamps
				m_bShowTimestamp = !m_bShowTimestamp;
				break;
			}
		}

		// Edit Queue mode
		if (m_eInputMode == eEditQueue)
		{
			switch (iKey)
			{
			case 'e':
			case 10: // return
			case 13: // enter
				m_eInputMode = eNormal;
				return;
			case KEY_DOWN:
				if (m_iSelectedQueueEntry < iQueueSize - 1)
				{
					SetCurrentQueueEntry(m_iSelectedQueueEntry + 1);
				}
				break;
			case KEY_UP:
				if (m_iSelectedQueueEntry > 0)
				{
					SetCurrentQueueEntry(m_iSelectedQueueEntry - 1);
				}
				break;
			case KEY_PPAGE:
				if (m_iSelectedQueueEntry > 0)
				{
					if (m_iSelectedQueueEntry == m_iQueueScrollOffset)
					{
						m_iQueueScrollOffset -= m_iQueueWinClientHeight;
						SetCurrentQueueEntry(m_iSelectedQueueEntry - m_iQueueWinClientHeight);
					}
					else
					{
						SetCurrentQueueEntry(m_iQueueScrollOffset);
					}
				}
				break;
			case KEY_NPAGE:
				if (m_iSelectedQueueEntry < iQueueSize - 1)
				{
					if (m_iSelectedQueueEntry == m_iQueueScrollOffset + m_iQueueWinClientHeight - 1)
					{
						m_iQueueScrollOffset += m_iQueueWinClientHeight;
						SetCurrentQueueEntry(m_iSelectedQueueEntry + m_iQueueWinClientHeight);
					}
					else
					{
						SetCurrentQueueEntry(m_iQueueScrollOffset + m_iQueueWinClientHeight - 1);
					}
				}
				break;
			case KEY_HOME:
				SetCurrentQueueEntry(0);
				break;
			case KEY_END:
				SetCurrentQueueEntry(iQueueSize > 0 ? iQueueSize - 1 : 0);
				break;
			case 'p':
				// Key 'p' for pause
				EditQueue(DownloadQueue::eaFilePause, 0);
				break;
			case 'd':
				SetHint(" Use Uppercase \"D\" for delete");
				break;
			case 'D':
				// Delete entry
				if (EditQueue(DownloadQueue::eaFileDelete, 0))
				{
					SetCurrentQueueEntry(m_iSelectedQueueEntry);
				}
				break;
			case 'u':
				if (EditQueue(DownloadQueue::eaFileMoveOffset, -1))
				{
					SetCurrentQueueEntry(m_iSelectedQueueEntry - 1);
				}
				break;
			case 'n':
				if (EditQueue(DownloadQueue::eaFileMoveOffset, +1))
				{
					SetCurrentQueueEntry(m_iSelectedQueueEntry + 1);
				}
				break;
			case 't':
				if (EditQueue(DownloadQueue::eaFileMoveTop, 0))
				{
					SetCurrentQueueEntry(0);
				}
				break;
			case 'b':
				if (EditQueue(DownloadQueue::eaFileMoveBottom, 0))
				{
					SetCurrentQueueEntry(iQueueSize > 0 ? iQueueSize - 1 : 0);
				}
				break;
			}
		}

		// Edit download rate input mode
		if (m_eInputMode == eDownloadRate)
		{
			// Numbers
			if (m_iInputNumberIndex < 5 && iKey >= '0' && iKey <= '9')
			{
				m_iInputValue = (m_iInputValue * 10) + (iKey - '0');
				m_iInputNumberIndex++;
			}
			// Enter
			else if (iKey == 10 || iKey == 13)
			{
				ServerSetDownloadRate(m_iInputValue * 1024);
				m_eInputMode = eNormal;
				return;
			}
			// Escape
			else if (iKey == 27)
			{
				m_eInputMode = eNormal;
				return;
			}
			// Backspace
			else if (m_iInputNumberIndex > 0 && iKey == KEY_BACKSPACE)
			{
				int iRemain = m_iInputValue % 10;

				m_iInputValue = (m_iInputValue - iRemain) / 10;
				m_iInputNumberIndex--;
			}
		}

		iKey = ReadConsoleKey();
	}
}

int NCursesFrontend::ReadConsoleKey()
{
#ifdef WIN32
	HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
	DWORD NumberOfEvents;
	BOOL bOK = GetNumberOfConsoleInputEvents(hConsole, &NumberOfEvents);
	if (bOK && NumberOfEvents > 0)
	{
		while (NumberOfEvents--)
		{
			INPUT_RECORD InputRecord;
			DWORD NumberOfEventsRead;
			if (ReadConsoleInput(hConsole, &InputRecord, 1, &NumberOfEventsRead) && 
				NumberOfEventsRead > 0 &&
				InputRecord.EventType == KEY_EVENT &&
				InputRecord.Event.KeyEvent.bKeyDown)
			{
				char c = tolower(InputRecord.Event.KeyEvent.wVirtualKeyCode);
				if (bool(InputRecord.Event.KeyEvent.dwControlKeyState & CAPSLOCK_ON) ^
					bool(InputRecord.Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED))
				{
					c = toupper(c);
				}
				return c;
			}
		}
	}
	return READKEY_EMPTY;
#else
	return getch();
#endif
}

#endif
