/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2004  Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007  Andrei Prygounkov <hugbug@users.sourceforge.net>
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

#ifndef DISABLE_CURSES

#ifdef HAVE_NCURSES_H
#include <ncurses.h>
#endif
#ifdef HAVE_NCURSES_NCURSES_H
#include <ncurses/ncurses.h>
#endif

#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <unistd.h>
#endif

#include "nzbget.h"
#include "NCursesFrontend.h"
#include "Options.h"

#ifdef HAVE_CURSES_H
// curses.h header must be included last to avoid problems on Solaris
// (and possibly other systems, that uses curses.h (not ncurses.h)
#include <curses.h>
// "#undef erase" is neccessary on Solaris
#undef erase
#endif

extern Options* g_pOptions;
extern void ExitProc();

static const int NCURSES_COLORPAIR_TEXT			= 1;
static const int NCURSES_COLORPAIR_INFO			= 2;
static const int NCURSES_COLORPAIR_WARNING		= 3;
static const int NCURSES_COLORPAIR_ERROR		= 4;
static const int NCURSES_COLORPAIR_DEBUG		= 5;
static const int NCURSES_COLORPAIR_STATUS		= 6;
static const int NCURSES_COLORPAIR_KEYBAR		= 7;
static const int NCURSES_COLORPAIR_INFOLINE		= 8;
static const int NCURSES_COLORPAIR_TEXTHIGHL	= 9;
static const int NCURSES_COLORPAIR_CURSOR		= 10;

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

NCursesFrontend::GroupInfo::GroupInfo(int iID, const char* szNZBFilename)
{
	m_iID = iID;
	m_szNZBFilename = strdup(szNZBFilename);
	m_iFileCount = 0;
	m_lSize = 0;
	m_lRemainingSize = 0;
	m_lPausedSize = 0;
}

NCursesFrontend::GroupInfo::~GroupInfo()
{
	if (m_szNZBFilename)
	{
		free(m_szNZBFilename);
	}
}

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

	m_groupQueue.clear();

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
        init_pair(NCURSES_COLORPAIR_STATUS,		COLOR_BLUE,		COLOR_WHITE);
        init_pair(NCURSES_COLORPAIR_KEYBAR,		COLOR_WHITE,	COLOR_BLUE);
        init_pair(NCURSES_COLORPAIR_INFOLINE,	COLOR_WHITE,	COLOR_BLUE);
        init_pair(NCURSES_COLORPAIR_TEXTHIGHL,	COLOR_BLACK,	COLOR_CYAN);
        init_pair(NCURSES_COLORPAIR_CURSOR,		COLOR_BLACK,	COLOR_YELLOW);
    }
}

NCursesFrontend::~NCursesFrontend()
{
#ifdef WIN32
	if (m_pScreenBuffer)
	{
		free(m_pScreenBuffer);
	}
	if (m_pOldScreenBuffer)
	{
		free(m_pOldScreenBuffer);
	}
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
}

void NCursesFrontend::Run()
{
    debug("Entering NCursesFrontend-loop");

	int iScreenUpdateInterval = 25;
	int iScreenUpdatePos = 0;
	m_iDataUpdatePos = 0;

    while (!IsStopped())
    {
        // The data (queue and log) is updated each m_iUpdateInterval msec,
        // but the window is updated more often for better reaction on user's input
		if (iScreenUpdatePos <= 0)
		{
			iScreenUpdatePos = iScreenUpdateInterval;
			Update();
			if (m_iDataUpdatePos <= 0)
			{
				m_iDataUpdatePos = m_iUpdateInterval;
			}
		}

		usleep(10 * 1000);
		iScreenUpdatePos -= 10;
		m_iDataUpdatePos -= 10;
    }

    FreeData();
	ClearGroupQueue();
	
    debug("Exiting NCursesFrontend-loop");
}

void NCursesFrontend::NeedUpdateData()
{
	m_iDataUpdatePos = 10;
}

void NCursesFrontend::Update()
{
    // Figure out how big the screen is
	CalcWindowSizes();

    if (m_iDataUpdatePos <= 0)
    {
        FreeData();
		ClearGroupQueue();
		m_iNeededLogEntries = m_iMessagesWinClientHeight;
        if (!PrepareData())
        {
            return;
        }
		PrepareGroupQueue();
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
    // Print Current NZBQueue
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

    // Update the input
    UpdateInput();

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
		m_pScreenBuffer = (CHAR_INFO*)malloc(m_iScreenBufferSize);
		memset(m_pScreenBuffer, 0, m_iScreenBufferSize);
		m_pOldScreenBuffer = (CHAR_INFO*)malloc(m_iScreenBufferSize);
		memset(m_pOldScreenBuffer, 0, m_iScreenBufferSize);
#else
        clear();
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
	if (m_bGroupFiles)
	{
		return m_groupQueue.size();
	}
	else
	{
		DownloadQueue* pDownloadQueue = LockQueue();
		int iQueueSize = pDownloadQueue->size();
		UnlockQueue();
		return iQueueSize;
	}
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
		m_pScreenBuffer[iBufPos + i].Char.AsciiChar = szString[i];
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

    Log::Messages* pMessages = LockMessages();
    
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
    char* szMessageType[] = { "INFO    ", "WARNING ", "ERROR   ", "DEBUG   "};
    const int iMessageTypeColor[] = { NCURSES_COLORPAIR_INFO, NCURSES_COLORPAIR_WARNING,
    	NCURSES_COLORPAIR_ERROR, NCURSES_COLORPAIR_DEBUG };

	char* szText = (char*)Msg->GetText();

	if (m_bShowTimestamp)
	{
		int iLen = strlen(szText) + 50;
		szText = (char*)malloc(iLen);

		time_t rawtime = Msg->GetTime();
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

	if (m_bShowTimestamp)
	{
		free(szText);
	}

	return iLines;
}

void NCursesFrontend::PrintStatus()
{
    char tmp[MAX_SCREEN_WIDTH];
    int iStatusRow = m_iScreenHeight - 2;

    char timeString[100];
    timeString[0] = '\0';

    if (m_fCurrentDownloadSpeed > 0.0)
    {
        long long remain_sec = (long long)(m_lRemainingSize / (m_fCurrentDownloadSpeed * 1024));
        int h = 0;
        int m = 0;
        int s = 0;
        while (remain_sec > 3600)
        {
            h++;
            remain_sec -= 3600;
        }
        while (remain_sec > 60)
        {
            m++;
            remain_sec -= 60;
        }
        s = remain_sec;

        sprintf(timeString, " (~ %.2d:%.2d:%.2d)", h, m, s);
    }

    char szDownloadLimit[128];
    if (m_fDownloadLimit > 0.0f)
    {
        sprintf(szDownloadLimit, ", Limit %.0f KB/s", m_fDownloadLimit);
    }
    else
    {
        szDownloadLimit[0] = 0;
    }

    char szParStatus[128];
    if (m_iParJobCount > 0)
    {
        sprintf(szParStatus, ", processing %i par%s", m_iParJobCount, m_iParJobCount > 1 ? "s" : "");
    }
    else
    {
        szParStatus[0] = 0;
    }

    snprintf(tmp, MAX_SCREEN_WIDTH, " %d threads, %.0f KB/s, %.2f MB remaining%s%s%s%s", m_iThreadCount, m_fCurrentDownloadSpeed, (float)(m_lRemainingSize / 1024.0 / 1024.0), timeString, szParStatus, m_bPause ? ", Paused" : "", szDownloadLimit);
	tmp[MAX_SCREEN_WIDTH - 1] = '\0';
    PlotLine(tmp, iStatusRow, 0, NCURSES_COLORPAIR_STATUS);
}

void NCursesFrontend::PrintKeyInputBar()
{
    int iQueueSize = CalcQueueSize();
	int iInputBarRow = m_iScreenHeight - 1;
	
    switch (m_eInputMode)
    {
    case eNormal:
        PlotLine("(Q)uit | (E)dit | (P)ause | (R)ate | n(Z)b | (W)indow | (T)ime | (G)roup", iInputBarRow, 0, NCURSES_COLORPAIR_KEYBAR);
        break;
    case eEditQueue:
    {
		char* szStatus = NULL;
		if (m_iSelectedQueueEntry > 0 && iQueueSize > 1 && m_iSelectedQueueEntry == iQueueSize - 1)
		{
			if (m_bGroupFiles)
			{
				// Up-/Down-commands for groups not supported yet
				szStatus = "(Q)uit | (E)xit | (P)ause | (D)elete | (T)op";
			}
			else
			{
				szStatus = "(Q)uit | (E)xit | (P)ause | (D)elete | (U)p/(T)op";
			}
		}
		else if (iQueueSize > 1 && m_iSelectedQueueEntry == 0)
		{
			if (m_bGroupFiles)
			{
				// Up-/Down-commands for groups not supported yet
				szStatus = "(Q)uit | (E)xit | (P)ause | (D)elete | (B)ottom";
			}
			else
			{
				szStatus = "(Q)uit | (E)xit | (P)ause | (D)elete | dow(N)/(B)ottom";
			}
		}
		else if (iQueueSize > 1)
		{
			if (m_bGroupFiles)
			{
				// Up-/Down-commands for groups not supported yet
				szStatus = "(Q)uit | (E)xit | (P)ause | (D)elete | (T)op/(B)ottom";
			}
			else
			{
				szStatus = "(Q)uit | (E)xit | (P)ause | (D)elete | (U)p/dow(N)/(T)op/(B)ottom";
			}
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
	int iLineNr = m_iQueueWinTop;
			
    DownloadQueue* pDownloadQueue = LockQueue();
	if (pDownloadQueue->empty())
    {
		char szBuffer[MAX_SCREEN_WIDTH];
		snprintf(szBuffer, sizeof(szBuffer), "%s Files for downloading", m_bUseColor ? "" : "*** ");
		szBuffer[MAX_SCREEN_WIDTH - 1] = '\0';
	    PlotLine(szBuffer, iLineNr++, 0, NCURSES_COLORPAIR_INFOLINE);
        PlotLine("Ready to receive nzb-job", iLineNr++, 0, NCURSES_COLORPAIR_TEXT);
    }
    else
    {
		iLineNr++;
		long long lRemaining = 0;
		long long lPaused = 0;
		int iPausedFiles = 0;
		int i = 0;
        for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++, i++)
        {
            FileInfo* pFileInfo = *it;

			if (i >= m_iQueueScrollOffset && i < m_iQueueScrollOffset + m_iQueueWinHeight -1)
			{
				PrintFilename(pFileInfo, iLineNr++, i == m_iSelectedQueueEntry);
			}

			if (pFileInfo->GetPaused())
			{
				iPausedFiles++;
				lPaused += pFileInfo->GetRemainingSize();
			}
			lRemaining += pFileInfo->GetRemainingSize();
        }
		
		char szRemaining[20];
		FormatFileSize(szRemaining, sizeof(szRemaining), lRemaining);

		char szUnpaused[20];
		FormatFileSize(szUnpaused, sizeof(szUnpaused), lRemaining - lPaused);
		
		char szBuffer[MAX_SCREEN_WIDTH];
		snprintf(szBuffer, sizeof(szBuffer), " %sFiles for downloading - %i / %i files in queue - %s / %s", 
			m_bUseColor ? "" : "*** ", pDownloadQueue->size(), pDownloadQueue->size() - iPausedFiles, szRemaining, szUnpaused);
		szBuffer[MAX_SCREEN_WIDTH - 1] = '\0';
		PlotLine(szBuffer, m_iQueueWinTop, 0, NCURSES_COLORPAIR_INFOLINE);
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

	char szCompleted[20];
	szCompleted[0] = '\0';
	if (pFileInfo->GetRemainingSize() < pFileInfo->GetSize())
	{
		sprintf(szCompleted, ", %i%%", (int)(100 - pFileInfo->GetRemainingSize() * 100.0 / pFileInfo->GetSize()));
	}

	char szNZBNiceName[1024];
	if (m_bShowNZBname)
	{
		pFileInfo->GetNiceNZBName(szNZBNiceName, 1023);
		int len = strlen(szNZBNiceName);
		szNZBNiceName[len] = PATH_SEPARATOR;
		szNZBNiceName[len + 1] = '\0';
	}
	else
	{
		szNZBNiceName[0] = '\0';
	}

	char szBuffer[MAX_SCREEN_WIDTH];
	snprintf(szBuffer, MAX_SCREEN_WIDTH, "%s%i%s %s%s (%.2f MB%s)%s", Brace1, pFileInfo->GetID(), Brace2, szNZBNiceName, pFileInfo->GetFilename(), pFileInfo->GetSize() / 1024.0 / 1024.0, szCompleted, pFileInfo->GetPaused() ? " (paused)" : "");
	szBuffer[MAX_SCREEN_WIDTH - 1] = '\0';

	PlotLine(szBuffer, iRow, 0, color);
}

void NCursesFrontend::FormatFileSize(char * szBuffer, int iBufLen, long long lFileSize)
{
	if (lFileSize > 1024 * 1024 * 1024)
	{
		snprintf(szBuffer, iBufLen, "%.2f GB", (float)lFileSize / 1024 / 1024 / 1024);
	}
	else if (lFileSize > 1024 * 1024)
	{
		snprintf(szBuffer, iBufLen, "%.2f MB", (float)lFileSize / 1024 / 1024);
	}
	else if (lFileSize > 1024)
	{
		snprintf(szBuffer, iBufLen, "%.2f KB", (float)lFileSize / 1024);
	}
	else 
	{
		snprintf(szBuffer, iBufLen, "%i", (int)lFileSize);
	}
	szBuffer[iBufLen - 1] = '\0';
}

void NCursesFrontend::PrintGroupQueue()
{
	int iLineNr = m_iQueueWinTop;

	GroupQueue* pGroupQueue = &m_groupQueue;
	if (pGroupQueue->empty())
    {
		char szBuffer[MAX_SCREEN_WIDTH];
		snprintf(szBuffer, sizeof(szBuffer), "%s NZBs for downloading", m_bUseColor ? "" : "*** ");
		szBuffer[MAX_SCREEN_WIDTH - 1] = '\0';
	    PlotLine(szBuffer, iLineNr++, 0, NCURSES_COLORPAIR_INFOLINE);
        PlotLine("Ready to receive nzb-job", iLineNr++, 0, NCURSES_COLORPAIR_TEXT);
    }
    else
    {
		iLineNr++;
		long long lRemaining = 0;
		long long lPaused = 0;
		int i = 0;
        for (GroupQueue::iterator it = pGroupQueue->begin(); it != pGroupQueue->end(); it++, i++)
        {
            GroupInfo* pGroupInfo = *it;

			if (i >= m_iQueueScrollOffset && i < m_iQueueScrollOffset + m_iQueueWinHeight -1)
			{
				PrintGroupname(pGroupInfo, iLineNr++, i == m_iSelectedQueueEntry);
			}

			lRemaining += pGroupInfo->GetRemainingSize();
			lPaused += pGroupInfo->GetPausedSize();
        }
		
		char szRemaining[20];
		FormatFileSize(szRemaining, sizeof(szRemaining), lRemaining);

		char szUnpaused[20];
		FormatFileSize(szUnpaused, sizeof(szUnpaused), lRemaining - lPaused);
		
		char szBuffer[MAX_SCREEN_WIDTH];
		snprintf(szBuffer, sizeof(szBuffer), " %sNZBs for downloading - %i NZBs in queue - %s / %s", 
			m_bUseColor ? "" : "*** ", pGroupQueue->size(), szRemaining, szUnpaused);
		szBuffer[MAX_SCREEN_WIDTH - 1] = '\0';
		PlotLine(szBuffer, m_iQueueWinTop, 0, NCURSES_COLORPAIR_INFOLINE);
    }
}

void NCursesFrontend::PrintGroupname(GroupInfo * pGroupInfo, int iRow, bool bSelected)
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

	long long lUnpausedRemainingSize = pGroupInfo->GetRemainingSize() - pGroupInfo->GetPausedSize();

	char szRemaining[20];
	FormatFileSize(szRemaining, sizeof(szRemaining), lUnpausedRemainingSize);

	char szPaused[20];
	szPaused[0] = '\0';
	if (pGroupInfo->GetPausedSize() > 0)
	{
		char szPausedSize[20];
		FormatFileSize(szPausedSize, sizeof(szPausedSize), pGroupInfo->GetPausedSize());
		sprintf(szPaused, " + %s paused", szPausedSize);
	}

	char szNZBNiceName[1024];
	FileInfo::MakeNiceNZBName(pGroupInfo->GetNZBFilename(), szNZBNiceName, 1023);

	char szBuffer[MAX_SCREEN_WIDTH];
	snprintf(szBuffer, MAX_SCREEN_WIDTH, "%s%i%s %s (%i file%s, %s%s)", Brace1, pGroupInfo->GetID(), Brace2, szNZBNiceName, 
		pGroupInfo->m_iFileCount, pGroupInfo->m_iFileCount > 1 ? "s" : "", szRemaining, szPaused);
	szBuffer[MAX_SCREEN_WIDTH - 1] = '\0';

	PlotLine(szBuffer, iRow, 0, color);
}

void NCursesFrontend::PrepareGroupQueue()
{
	m_groupQueue.clear();

    DownloadQueue* pDownloadQueue = LockQueue();
    for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
    {
        FileInfo* pFileInfo = *it;
		GroupInfo* pGroupInfo = NULL;
		for (GroupQueue::iterator itg = m_groupQueue.begin(); itg != m_groupQueue.end(); itg++)
		{
			GroupInfo* pGroupInfo1 = *itg;
			if (!strcmp(pGroupInfo1->GetNZBFilename(), pFileInfo->GetNZBFilename()))
			{
				pGroupInfo = pGroupInfo1;
				break;
			}
		}
		if (!pGroupInfo)
		{
			pGroupInfo = new GroupInfo(pFileInfo->GetID(), pFileInfo->GetNZBFilename());
			m_groupQueue.push_back(pGroupInfo);
		}
		pGroupInfo->m_iFileCount++;
		pGroupInfo->m_lSize += pFileInfo->GetSize();
		pGroupInfo->m_lRemainingSize += pFileInfo->GetRemainingSize();
		if (pFileInfo->GetPaused())
		{
			pGroupInfo->m_lPausedSize += pFileInfo->GetRemainingSize();
		}
	}
	UnlockQueue();
}

void NCursesFrontend::ClearGroupQueue()
{
	for (GroupQueue::iterator it = m_groupQueue.begin(); it != m_groupQueue.end(); it++)
	{
		delete *it;
	}
	m_groupQueue.clear();
}

bool NCursesFrontend::EditQueue(QueueEditor::EEditAction eAction, int iOffset)
{
	int ID = 0;
	bool bPause = false;

	if (m_bGroupFiles)
	{
		if (m_iSelectedQueueEntry >= 0 && m_iSelectedQueueEntry < (int)m_groupQueue.size())
		{
			GroupInfo* pGroupInfo = m_groupQueue[m_iSelectedQueueEntry];
			ID = pGroupInfo->GetID();
			bPause = pGroupInfo->GetRemainingSize() > pGroupInfo->GetPausedSize();
		}
		if (eAction == QueueEditor::eaFilePause)
		{
			eAction = bPause ? QueueEditor::eaFilePause : QueueEditor::eaFileResume;
		}

		// map file-edit-actions to group-edit-actions
		QueueEditor::EEditAction FileToGroupMap[] = {
			(QueueEditor::EEditAction)0,
			QueueEditor::eaGroupMoveOffset, 
			QueueEditor::eaGroupMoveTop, 
			QueueEditor::eaGroupMoveBottom, 
			QueueEditor::eaGroupPause, 
			QueueEditor::eaGroupResume, 
			QueueEditor::eaGroupDelete };
		eAction = FileToGroupMap[eAction];
	}
	else
	{
		DownloadQueue* pDownloadQueue = LockQueue();
		if (m_iSelectedQueueEntry >= 0 && m_iSelectedQueueEntry < (int)pDownloadQueue->size())
		{
			FileInfo* pFileInfo = (*pDownloadQueue)[m_iSelectedQueueEntry];
			ID = pFileInfo->GetID();
			bPause = !pFileInfo->GetPaused();
		}
		UnlockQueue();
		if (eAction == QueueEditor::eaFilePause)
		{
			eAction = bPause ? QueueEditor::eaFilePause : QueueEditor::eaFileResume;
		}
	}

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

void NCursesFrontend::UpdateInput()
{
    int iKey;
    while ((iKey = ReadConsoleKey()) != READKEY_EMPTY)
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
					info(m_bPause ? "Unpausing download" : "Pausing download");
				}
				ServerPauseUnpause(!m_bPause);
				break;
			case '\'':
				ServerDumpDebug();
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
				EditQueue(QueueEditor::eaFilePause, 0);
				break;
			case 'd':
				// Delete entry
				if (EditQueue(QueueEditor::eaFileDelete, 0))
				{
					SetCurrentQueueEntry(m_iSelectedQueueEntry);
				}
				break;
			case 'u':
				if (EditQueue(QueueEditor::eaFileMoveOffset, -1))
				{
					SetCurrentQueueEntry(m_iSelectedQueueEntry - 1);
				}
				break;
			case 'n':
				if (EditQueue(QueueEditor::eaFileMoveOffset, +1))
				{
					SetCurrentQueueEntry(m_iSelectedQueueEntry + 1);
				}
				break;
			case 't':
				if (EditQueue(QueueEditor::eaFileMoveTop, 0))
				{
					SetCurrentQueueEntry(0);
				}
				break;
			case 'b':
				if (EditQueue(QueueEditor::eaFileMoveBottom, 0))
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
				ServerSetDownloadRate((float)m_iInputValue);
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
				return tolower(InputRecord.Event.KeyEvent.wVirtualKeyCode);
			}
		}
	}
	return READKEY_EMPTY;
#else
	return getch();
#endif
}

#endif
