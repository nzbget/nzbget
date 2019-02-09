/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#ifndef DISABLE_CURSES

// "ncurses.h" contains many global defines such as for "OK" or "clear" which we sure don't want
// everywhere in the project. For that reason we include "ncurses.h" directly here instead of
// putting it into global header file "nzbget.h".

#ifdef HAVE_NCURSES_H
#include <ncurses.h>
#endif
#ifdef HAVE_NCURSES_NCURSES_H
#include <ncurses/ncurses.h>
#endif

#include "NCursesFrontend.h"
#include "Options.h"
#include "Util.h"

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
	m_summary = true;
	m_fileList = true;

	m_showNzbname = g_Options->GetCursesNzbName();
	m_showTimestamp = g_Options->GetCursesTime();
	m_groupFiles = g_Options->GetCursesGroup();

	// Setup curses
#ifdef WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

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
#else
	m_window = initscr();
	if (m_window == nullptr)
	{
		printf("ERROR: m_pWindow == nullptr\n");
		exit(-1);
	}
	keypad(stdscr, true);
	nodelay((WINDOW*)m_window, true);
	noecho();
	curs_set(0);
	m_useColor = has_colors();
#endif

	if (m_useColor)
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
	SetHint(nullptr);
}

void NCursesFrontend::Run()
{
	debug("Entering NCursesFrontend-loop");

	m_dataUpdatePos = 0;

	while (!IsStopped())
	{
		// The data (queue and log) is updated each m_iUpdateInterval msec,
		// but the window is updated more often for better reaction on user's input

		bool updateNow = false;
		int key = ReadConsoleKey();

		if (key != READKEY_EMPTY)
		{
			// Update now and next if a key is pressed.
			updateNow = true;
			m_updateNextTime = true;
		}
		else if (m_updateNextTime)
		{
			// Update due to key being pressed during previous call.
			updateNow = true;
			m_updateNextTime = false;
		}
		else if (m_dataUpdatePos <= 0)
		{
			updateNow = true;
			m_updateNextTime = false;
		}

		if (updateNow)
		{
			Update(key);
		}

		if (m_dataUpdatePos <= 0)
		{
			m_dataUpdatePos = m_updateInterval;
		}

		// update more often (sleep shorter) if need faster reaction on user input
		int sleepInterval = m_inputMode == normal ? 100 : 10;
		Wait(sleepInterval);
		m_dataUpdatePos -= sleepInterval;
	}

	FreeData();

	debug("Exiting NCursesFrontend-loop");
}

void NCursesFrontend::NeedUpdateData()
{
	m_dataUpdatePos = 10;
	m_updateNextTime = true;
}

void NCursesFrontend::Update(int key)
{
	// Figure out how big the screen is
	CalcWindowSizes();

	if (m_dataUpdatePos <= 0)
	{
		FreeData();
		m_neededLogEntries = m_messagesWinClientHeight;
		if (!PrepareData())
		{
			return;
		}

		// recalculate frame sizes
		CalcWindowSizes();
	}

	if (m_inputMode == editQueue)
	{
		int queueSize = CalcQueueSize();
		if (queueSize == 0)
		{
			m_selectedQueueEntry = 0;
			m_inputMode = normal;
		}
	}

	//------------------------------------------
	// Print Current NZBInfoList
	//------------------------------------------
	if (m_queueWinHeight > 0)
	{
		PrintQueue();
	}

	//------------------------------------------
	// Print Messages
	//------------------------------------------
	if (m_messagesWinHeight > 0)
	{
		PrintMessages();
	}

	PrintStatus();

	PrintKeyInputBar();

	UpdateInput(key);

	RefreshScreen();
}

void NCursesFrontend::CalcWindowSizes()
{
	int nrRows, nrColumns;
#ifdef WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO BufInfo;
	GetConsoleScreenBufferInfo(hConsole, &BufInfo);
	nrRows = BufInfo.srWindow.Bottom - BufInfo.srWindow.Top + 1;
	nrColumns = BufInfo.srWindow.Right - BufInfo.srWindow.Left + 1;
#else
	getmaxyx(stdscr, nrRows, nrColumns);
#endif
	if (nrRows != m_screenHeight || nrColumns != m_screenWidth)
	{
#ifdef WIN32
		int screenAreaSize = nrRows * nrColumns;
		m_screenBuffer.resize(screenAreaSize);
		m_oldScreenBuffer.resize(screenAreaSize);
#else
		curses_clear();
#endif
		m_screenHeight = nrRows;
		m_screenWidth = nrColumns;
	}

	int queueSize = CalcQueueSize();

	m_queueWinTop = 0;
	m_queueWinHeight = (m_screenHeight - 2) * m_queueWindowPercentage / 100;
	if (m_queueWinHeight - 1 > queueSize)
	{
		m_queueWinHeight = queueSize > 0 ? queueSize + 1 : 1 + 1;
	}
	m_queueWinClientHeight = m_queueWinHeight - 1;
	if (m_queueWinClientHeight < 0)
	{
		m_queueWinClientHeight = 0;
	}

	m_messagesWinTop = m_queueWinTop + m_queueWinHeight;
	m_messagesWinHeight = m_screenHeight - m_queueWinHeight - 2;
	m_messagesWinClientHeight = m_messagesWinHeight - 1;
	if (m_messagesWinClientHeight < 0)
	{
		m_messagesWinClientHeight = 0;
	}
}

int NCursesFrontend::CalcQueueSize()
{
	int queueSize = 0;
	if (m_groupFiles)
	{
		queueSize = DownloadQueue::Guard()->GetQueue()->size();
	}
	else
	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
		{
			queueSize += nzbInfo->GetFileList()->size();
		}
	}
	return queueSize;
}

void NCursesFrontend::PlotLine(const char * string, int row, int pos, int colorPair)
{
	BString<1024> buffer("%-*s", m_screenWidth, string);
	int len = buffer.Length();
	if (len > m_screenWidth - pos && m_screenWidth - pos < MAX_SCREEN_WIDTH)
	{
		buffer[m_screenWidth - pos] = '\0';
	}

	PlotText(buffer, row, pos, colorPair, false);
}

void NCursesFrontend::PlotText(const char * string, int row, int pos, int colorPair, bool blink)
{
#ifdef WIN32
	int bufPos = row * m_screenWidth + pos;
	int len = strlen(string);
	for (int i = 0; i < len; i++)
	{
		char c = string[i];
		CharToOemBuff(&c, &c, 1);
		m_screenBuffer[bufPos + i].Char.AsciiChar = c;
		m_screenBuffer[bufPos + i].Attributes = m_colorAttr[colorPair];
	}
#else
	if( m_useColor )
	{
		attron(COLOR_PAIR(colorPair));
		if (blink)
		{
			attron(A_BLINK);
		}
	}
	mvaddstr(row, pos, (char*)string);
	if( m_useColor )
	{
		attroff(COLOR_PAIR(colorPair));
		if (blink)
		{
			attroff(A_BLINK);
		}
	}
#endif
}

void NCursesFrontend::RefreshScreen()
{
#ifdef WIN32
	bool bufChanged = !std::equal(m_screenBuffer.begin(), m_screenBuffer.end(), m_oldScreenBuffer.begin(), m_oldScreenBuffer.end(),
		[](CHAR_INFO& a, CHAR_INFO& b)
		{
			return a.Char.AsciiChar == b.Char.AsciiChar && a.Attributes == b.Attributes;
		});

	if (bufChanged)
	{
		COORD BufSize;
		BufSize.X = m_screenWidth;
		BufSize.Y = m_screenHeight;

		COORD BufCoord;
		BufCoord.X = 0;
		BufCoord.Y = 0;

		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		CONSOLE_SCREEN_BUFFER_INFO BufInfo;
		GetConsoleScreenBufferInfo(hConsole, &BufInfo);
		WriteConsoleOutput(hConsole, m_screenBuffer.data(), BufSize, BufCoord, &BufInfo.srWindow);

		BufInfo.dwCursorPosition.X = BufInfo.srWindow.Right;
		BufInfo.dwCursorPosition.Y = BufInfo.srWindow.Bottom;
		SetConsoleCursorPosition(hConsole, BufInfo.dwCursorPosition);

		m_oldScreenBuffer = m_screenBuffer;
	}
#else
	// Cursor placement
	wmove((WINDOW*)m_window, m_screenHeight, m_screenWidth);

	// NCurses refresh
	refresh();
#endif
}

#ifdef WIN32
void NCursesFrontend::init_pair(int colorNumber, WORD wForeColor, WORD wBackColor)
{
	m_colorAttr.resize(colorNumber + 1);
	m_colorAttr[colorNumber] = wForeColor | (wBackColor << 4);
}
#endif

void NCursesFrontend::PrintMessages()
{
	int lineNr = m_messagesWinTop;

	BString<1024> buffer("%s Messages", m_useColor ? "" : "*** ");
	PlotLine(buffer, lineNr++, 0, NCURSES_COLORPAIR_INFOLINE);

	int line = lineNr + m_messagesWinClientHeight - 1;
	int linesToPrint = m_messagesWinClientHeight;

	GuardedMessageList messages = GuardMessages();

	// print messages from bottom
	for (int i = (int)messages->size() - 1; i >= 0 && linesToPrint > 0; i--)
	{
		int printedLines = PrintMessage(messages->at(i), line, linesToPrint);
		line -= printedLines;
		linesToPrint -= printedLines;
	}

	if (linesToPrint > 0)
	{
		// too few messages, print them again from top
		line = lineNr + m_messagesWinClientHeight - 1;
		while (linesToPrint-- > 0)
		{
			PlotLine("", line--, 0, NCURSES_COLORPAIR_TEXT);
		}
		int linesToPrint2 = m_messagesWinClientHeight;
		for (int i = (int)messages->size() - 1; i >= 0 && linesToPrint2 > 0; i--)
		{
			int printedLines = PrintMessage(messages->at(i), line, linesToPrint2);
			line -= printedLines;
			linesToPrint2 -= printedLines;
		}
	}
}

int NCursesFrontend::PrintMessage(Message& msg, int row, int maxLines)
{
	const char* messageType[] = { "INFO    ", "WARNING ", "ERROR   ", "DEBUG   ", "DETAIL  "};
	const int messageTypeColor[] = { NCURSES_COLORPAIR_INFO, NCURSES_COLORPAIR_WARNING,
		NCURSES_COLORPAIR_ERROR, NCURSES_COLORPAIR_DEBUG, NCURSES_COLORPAIR_DETAIL };

	CString text;

	if (m_showTimestamp)
	{
		time_t rawtime = msg.GetTime() + g_Options->GetTimeCorrection();
		text.Format("%s - %s", *Util::FormatTime(rawtime), msg.GetText());
	}
	else
	{
		text = msg.GetText();
	}

	// replace some special characters with spaces
	for (char* p = (char*)text; *p; p++)
	{
		if (*p == '\n' || *p == '\r' || *p == '\b')
		{
			*p = ' ';
		}
	}

	int len = strlen(text);
	int winWidth = m_screenWidth - 8;
	int msgLines = len / winWidth;
	if (len % winWidth > 0)
	{
		msgLines++;
	}

	int lines = 0;
	for (int i = msgLines - 1; i >= 0 && lines < maxLines; i--)
	{
		int r = row - msgLines + i + 1;
		PlotLine(text + winWidth * i, r, 8, NCURSES_COLORPAIR_TEXT);
		if (i == 0)
		{
			PlotText(messageType[msg.GetKind()], r, 0, messageTypeColor[msg.GetKind()], false);
		}
		else
		{
			PlotText("        ", r, 0, messageTypeColor[msg.GetKind()], false);
		}
		lines++;
	}

	return lines;
}

void NCursesFrontend::PrintStatus()
{
	int statusRow = m_screenHeight - 2;

	BString<100> timeString;

	int currentDownloadSpeed = m_standBy ? 0 : m_currentDownloadSpeed;
	if (currentDownloadSpeed > 0 && !m_pauseDownload)
	{
		int64 remain_sec = (int64)(m_remainingSize / currentDownloadSpeed);
		int h = (int)(remain_sec / 3600);
		int m = (int)((remain_sec % 3600) / 60);
		int s = (int)(remain_sec % 60);
		timeString.Format(" (~ %.2d:%.2d:%.2d)", h, m, s);
	}

	BString<100> downloadLimit;
	if (m_downloadLimit > 0)
	{
		downloadLimit.Format(", Limit %i KB/s", m_downloadLimit / 1024);
	}

	BString<100> postStatus;
	if (m_postJobCount > 0)
	{
		postStatus.Format(", %i post-job%s", m_postJobCount, m_postJobCount > 1 ? "s" : "");
	}

	int averageSpeed = (int)(m_dnTimeSec > 0 ? m_allBytes / m_dnTimeSec : 0);

	BString<1024> status(" %d threads, %s, %s remaining%s%s%s%s, Avg. %s",
		m_threadCount, *Util::FormatSpeed(currentDownloadSpeed),
		*Util::FormatSize(m_remainingSize),
		*timeString, *postStatus, m_pauseDownload ? (m_standBy ? ", Paused" : ", Pausing") : "",
		*downloadLimit, *Util::FormatSpeed(averageSpeed));
	PlotLine(status, statusRow, 0, NCURSES_COLORPAIR_STATUS);
}

void NCursesFrontend::PrintKeyInputBar()
{
	int queueSize = CalcQueueSize();
	int inputBarRow = m_screenHeight - 1;

	if (!m_hint.Empty())
	{
		time_t time = Util::CurrentTime();
		if (time - m_startHint < 5)
		{
			PlotLine(m_hint, inputBarRow, 0, NCURSES_COLORPAIR_HINT);
			return;
		}
		else
		{
			SetHint(nullptr);
		}
	}

	switch (m_inputMode)
	{
	case normal:
		if (m_groupFiles)
		{
			PlotLine("(Q)uit | (E)dit | (P)ause | (R)ate | (W)indow | (G)roup | (T)ime", inputBarRow, 0, NCURSES_COLORPAIR_KEYBAR);
		}
		else
		{
			PlotLine("(Q)uit | (E)dit | (P)ause | (R)ate | (W)indow | (G)roup | (T)ime | n(Z)b", inputBarRow, 0, NCURSES_COLORPAIR_KEYBAR);
		}
		break;
	case editQueue:
	{
		const char* status = nullptr;
		if (m_selectedQueueEntry > 0 && queueSize > 1 && m_selectedQueueEntry == queueSize - 1)
		{
			status = "(Q)uit | (E)xit | (P)ause | (D)elete | (U)p/(T)op";
		}
		else if (queueSize > 1 && m_selectedQueueEntry == 0)
		{
			status = "(Q)uit | (E)xit | (P)ause | (D)elete | dow(N)/(B)ottom";
		}
		else if (queueSize > 1)
		{
			status = "(Q)uit | (E)xit | (P)ause | (D)elete | (U)p/dow(N)/(T)op/(B)ottom";
		}
		else
		{
			status = "(Q)uit | (E)xit | (P)ause | (D)elete";
		}

		PlotLine(status, inputBarRow, 0, NCURSES_COLORPAIR_KEYBAR);
		break;
	}
	case downloadRate:
		BString<100> hint("Download rate: %i", m_inputValue);
		PlotLine(hint, inputBarRow, 0, NCURSES_COLORPAIR_KEYBAR);
		// Print the cursor
		PlotText(" ", inputBarRow, 15 + m_inputNumberIndex, NCURSES_COLORPAIR_CURSOR, true);
		break;
	}
}

void NCursesFrontend::SetHint(const char* hint)
{
	m_hint = hint;
	if (!m_hint.Empty())
	{
		m_startHint = Util::CurrentTime();
	}
}

void NCursesFrontend::PrintQueue()
{
	if (m_groupFiles)
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
	int lineNr = m_queueWinTop + 1;
	int64 remaining = 0;
	int64 paused = 0;
	int pausedFiles = 0;
	int fileNum = 0;

	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
	for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
	{
		for (FileInfo* fileInfo : nzbInfo->GetFileList())
		{
			if (fileNum >= m_queueScrollOffset && fileNum < m_queueScrollOffset + m_queueWinHeight -1)
			{
				PrintFilename(fileInfo, lineNr++, fileNum == m_selectedQueueEntry);
			}
			fileNum++;

			if (fileInfo->GetPaused())
			{
				pausedFiles++;
				paused += fileInfo->GetRemainingSize();
			}
			remaining += fileInfo->GetRemainingSize();
		}
	}

	if (fileNum > 0)
	{
		BString<1024> header(" %sFiles for downloading - %i / %i files in queue - %s / %s",
			m_useColor ? "" : "*** ", fileNum,
			fileNum - pausedFiles,
			*Util::FormatSize(remaining), *Util::FormatSize(remaining - paused));
		PrintTopHeader(header, m_queueWinTop, true);
	}
	else
	{
		lineNr--;
		BString<1024> header("%s Files for downloading", m_useColor ? "" : "*** ");
		PrintTopHeader(header, lineNr++, true);
		PlotLine("Ready to receive nzb-job", lineNr++, 0, NCURSES_COLORPAIR_TEXT);
	}
}

void NCursesFrontend::PrintFilename(FileInfo * fileInfo, int row, bool selected)
{
	int color = 0;
	const char* Brace1 = "[";
	const char* Brace2 = "]";
	if (m_inputMode == editQueue && selected)
	{
		color = NCURSES_COLORPAIR_TEXTHIGHL;
		if (!m_useColor)
		{
			Brace1 = "<";
			Brace2 = ">";
		}
	}
	else
	{
		color = NCURSES_COLORPAIR_TEXT;
	}

	const char* downloading = "";
	if (fileInfo->GetActiveDownloads() > 0)
	{
		downloading = " *";
	}

	BString<100> priority;
	if (fileInfo->GetNzbInfo()->GetPriority() != 0)
	{
		priority.Format(" [%+i]", fileInfo->GetNzbInfo()->GetPriority());
	}

	BString<100> completed;
	if (fileInfo->GetRemainingSize() < fileInfo->GetSize())
	{
		completed.Format(", %i%%", (int)(100 - fileInfo->GetRemainingSize() * 100 / fileInfo->GetSize()));
	}

	BString<1024> nzbNiceName;
	if (m_showNzbname)
	{
		nzbNiceName.Format("%s%c", fileInfo->GetNzbInfo()->GetName(), PATH_SEPARATOR);
	}

	BString<1024> text("%s%i%s%s%s %s%s (%s%s)%s", Brace1, fileInfo->GetId(),
		Brace2, *priority, downloading, *nzbNiceName, fileInfo->GetFilename(),
		*Util::FormatSize(fileInfo->GetSize()),
		*completed, fileInfo->GetPaused() ? " (paused)" : "");

	PlotLine(text, row, 0, color);
}

void NCursesFrontend::PrintTopHeader(char* header, int lineNr, bool upTime)
{
	BString<1024> buffer("%-*s", m_screenWidth, header);
	int headerLen = strlen(header);
	int charsLeft = m_screenWidth - headerLen - 2;

	int time = upTime ? m_upTimeSec : m_dnTimeSec;
	int d = time / 3600 / 24;
	int h = (time % (3600 * 24)) / 3600;
	int m = (time % 3600) / 60;
	int s = time % 60;
	BString<100> timeStr;

	if (d == 0)
	{
		timeStr.Format("%.2d:%.2d:%.2d", h, m, s);
		if ((int)strlen(timeStr) > charsLeft)
		{
			timeStr.Format("%.2d:%.2d", h, m);
		}
	}
	else
	{
		timeStr.Format("%i %s %.2d:%.2d:%.2d", d, (d == 1 ? "day" : "days"), h, m, s);
		if ((int)strlen(timeStr) > charsLeft)
		{
			timeStr.Format("%id %.2d:%.2d:%.2d", d, h, m, s);
		}
		if ((int)strlen(timeStr) > charsLeft)
		{
			timeStr.Format("%id %.2d:%.2d", d, h, m);
		}
	}

	const char* shortCap = upTime ? " Up " : "Dn ";
	const char* longCap = upTime ? " Uptime " : " Download-time ";

	int timeLen = strlen(timeStr);
	int shortCapLen = strlen(shortCap);
	int longCapLen = strlen(longCap);

	if (charsLeft - timeLen - longCapLen >= 0)
	{
		snprintf(buffer + m_screenWidth - timeLen - longCapLen, MAX_SCREEN_WIDTH - (m_screenWidth - timeLen - longCapLen), "%s%s", longCap, *timeStr);
	}
	else if (charsLeft - timeLen - shortCapLen >= 0)
	{
		snprintf(buffer + m_screenWidth - timeLen - shortCapLen, MAX_SCREEN_WIDTH - (m_screenWidth - timeLen - shortCapLen), "%s%s", shortCap, *timeStr);
	}
	else if (charsLeft - timeLen >= 0)
	{
		snprintf(buffer + m_screenWidth - timeLen, MAX_SCREEN_WIDTH - (m_screenWidth - timeLen), "%s", *timeStr);
	}

	PlotLine(buffer, lineNr, 0, NCURSES_COLORPAIR_INFOLINE);
}

void NCursesFrontend::PrintGroupQueue()
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	int lineNr = m_queueWinTop;

	if (downloadQueue->GetQueue()->empty())
	{
		BString<1024> buffer("%s NZBs for downloading", m_useColor ? "" : "*** ");
		PrintTopHeader(buffer, lineNr++, false);
		PlotLine("Ready to receive nzb-job", lineNr++, 0, NCURSES_COLORPAIR_TEXT);
	}
	else
	{
		lineNr++;

		ResetColWidths();
		int calcLineNr = lineNr;
		int i = 0;
		for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
		{
			if (i >= m_queueScrollOffset && i < m_queueScrollOffset + m_queueWinHeight -1)
			{
				PrintGroupname(nzbInfo, calcLineNr++, false, true);
			}
			i++;
		}

		int64 remaining = 0;
		int64 paused = 0;
		i = 0;
		for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
		{
			if (i >= m_queueScrollOffset && i < m_queueScrollOffset + m_queueWinHeight -1)
			{
				PrintGroupname(nzbInfo, lineNr++, i == m_selectedQueueEntry, false);
			}
			i++;
			remaining += nzbInfo->GetRemainingSize();
			paused += nzbInfo->GetPausedSize();
		}

		BString<1024> buffer(" %sNZBs for downloading - %i NZBs in queue - %s / %s",
			m_useColor ? "" : "*** ", (int)downloadQueue->GetQueue()->size(),
			*Util::FormatSize(remaining), *Util::FormatSize(remaining - paused));
		PrintTopHeader(buffer, m_queueWinTop, false);
	}
}

void NCursesFrontend::ResetColWidths()
{
	m_colWidthFiles = 0;
	m_colWidthTotal = 0;
	m_colWidthLeft = 0;
}

void NCursesFrontend::PrintGroupname(NzbInfo* nzbInfo, int row, bool selected, bool calcColWidth)
{
	int color = NCURSES_COLORPAIR_TEXT;
	char chBrace1 = '[';
	char chBrace2 = ']';
	if (m_inputMode == editQueue && selected)
	{
		color = NCURSES_COLORPAIR_TEXTHIGHL;
		if (!m_useColor)
		{
			chBrace1 = '<';
			chBrace2 = '>';
		}
	}

	const char* downloading = "";
	if (nzbInfo->GetActiveDownloads() > 0)
	{
		downloading = " *";
	}

	BString<100> priority;
	if (nzbInfo->GetPriority() != 0)
	{
		priority.Format(" [%+i]", nzbInfo->GetPriority());
	}

	// Format:
	// [id - id] Name   Left-Files/Paused     Total      Left     Time
	// [1-2] Nzb-name             999/999 999.99 MB 999.99 MB 00:00:00

	int nameLen = 0;
	if (calcColWidth)
	{
		nameLen = m_screenWidth - 1 - 9 - 11 - 11 - 9;
	}
	else
	{
		nameLen = m_screenWidth - 1 - m_colWidthFiles - 2 - m_colWidthTotal - 2 - m_colWidthLeft - 2 - 9;
	}

	BString<1024> buffer;

	bool printFormatted = nameLen > 20;

	if (printFormatted)
	{
		BString<100> files("%i/%i", (int)nzbInfo->GetFileList()->size(), nzbInfo->GetPausedFileCount());
		BString<1024> nameWithIds("%c%i%c%s%s %s", chBrace1, nzbInfo->GetId(), chBrace2,
			*priority, downloading, nzbInfo->GetName());

		int64 unpausedRemainingSize = nzbInfo->GetRemainingSize() - nzbInfo->GetPausedSize();
		CString remaining = Util::FormatSize(unpausedRemainingSize);
		CString total = Util::FormatSize(nzbInfo->GetSize());

		BString<100> time;
		int currentDownloadSpeed = m_standBy ? 0 : m_currentDownloadSpeed;
		if (nzbInfo->GetPausedSize() > 0 && unpausedRemainingSize == 0)
		{
			time = "[paused]";
			remaining = Util::FormatSize(nzbInfo->GetRemainingSize());
		}
		else if (currentDownloadSpeed > 0 && !m_pauseDownload)
		{
			int64 remain_sec = (int64)(unpausedRemainingSize / currentDownloadSpeed);
			int h = (int)(remain_sec / 3600);
			int m = (int)((remain_sec % 3600) / 60);
			int s = (int)(remain_sec % 60);
			if (h < 100)
			{
				time.Format("%.2d:%.2d:%.2d", h, m, s);
			}
			else
			{
				time.Format("99:99:99");
			}
		}

		if (calcColWidth)
		{
			int colWidthFiles = strlen(files);
			m_colWidthFiles = colWidthFiles > m_colWidthFiles ? colWidthFiles : m_colWidthFiles;

			int colWidthTotal = strlen(total);
			m_colWidthTotal = colWidthTotal > m_colWidthTotal ? colWidthTotal : m_colWidthTotal;

			int colWidthLeft = strlen(remaining);
			m_colWidthLeft = colWidthLeft > m_colWidthLeft ? colWidthLeft : m_colWidthLeft;
		}
		else
		{
			buffer.Format("%-*s  %*s  %*s  %*s  %8s", nameLen, *nameWithIds,
				m_colWidthFiles, *files, m_colWidthTotal, *total, m_colWidthLeft, *remaining, *time);
		}
	}
	else
	{
		buffer.Format("%c%i%c%s %s", chBrace1, nzbInfo->GetId(),
			chBrace2, downloading, nzbInfo->GetName());
	}

	if (!calcColWidth)
	{
		PlotLine(buffer, row, 0, color);
	}
}

bool NCursesFrontend::EditQueue(DownloadQueue::EEditAction action, int offset)
{
	int ID = 0;

	if (m_groupFiles)
	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		if (m_selectedQueueEntry >= 0 && m_selectedQueueEntry < (int)downloadQueue->GetQueue()->size())
		{
			std::unique_ptr<NzbInfo>& nzbInfo = downloadQueue->GetQueue()->at(m_selectedQueueEntry);
			ID = nzbInfo->GetId();
			if (action == DownloadQueue::eaFilePause)
			{
				if (nzbInfo->GetRemainingSize() == nzbInfo->GetPausedSize())
				{
					action = DownloadQueue::eaFileResume;
				}
				else if (nzbInfo->GetPausedSize() == 0 && (nzbInfo->GetRemainingParCount() > 0) &&
					!(m_lastPausePars && m_lastEditEntry == m_selectedQueueEntry))
				{
					action = DownloadQueue::eaFilePauseExtraPars;
					m_lastPausePars = true;
				}
				else
				{
					action = DownloadQueue::eaFilePause;
					m_lastPausePars = false;
				}
			}
		}

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
		action = FileToGroupMap[action];
	}
	else
	{
		int fileNum = 0;
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
		{
			for (FileInfo* fileInfo : nzbInfo->GetFileList())
			{
				if (m_selectedQueueEntry == fileNum)
				{
					ID = fileInfo->GetId();
					if (action == DownloadQueue::eaFilePause)
					{
						action = !fileInfo->GetPaused() ? DownloadQueue::eaFilePause : DownloadQueue::eaFileResume;
					}
				}
				fileNum++;
			}
		}
	}

	m_lastEditEntry = m_selectedQueueEntry;

	NeedUpdateData();

	if (ID != 0)
	{
		return ServerEditQueue(action, offset, ID);
	}
	else
	{
		return false;
	}
}

void NCursesFrontend::SetCurrentQueueEntry(int entry)
{
	int queueSize = CalcQueueSize();

	if (entry < 0)
	{
		entry = 0;
	}
	else if (entry > queueSize - 1)
	{
		entry = queueSize - 1;
	}

	if (entry > m_queueScrollOffset + m_queueWinClientHeight ||
	   entry < m_queueScrollOffset - m_queueWinClientHeight)
	{
		m_queueScrollOffset = entry - m_queueWinClientHeight / 2;
	}
	else if (entry < m_queueScrollOffset)
	{
		m_queueScrollOffset -= m_queueWinClientHeight;
	}
	else if (entry >= m_queueScrollOffset + m_queueWinClientHeight)
	{
		m_queueScrollOffset += m_queueWinClientHeight;
	}

	if (m_queueScrollOffset > queueSize - m_queueWinClientHeight)
	{
		m_queueScrollOffset = queueSize - m_queueWinClientHeight;
	}
	if (m_queueScrollOffset < 0)
	{
		m_queueScrollOffset = 0;
	}

	m_selectedQueueEntry = entry;
}


/*
 * Process keystrokes starting with the initialKey, which must not be
 * READKEY_EMPTY but has alread been set via ReadConsoleKey.
 */
void NCursesFrontend::UpdateInput(int initialKey)
{
	int key = initialKey;
	while (key != READKEY_EMPTY)
	{
		int queueSize = CalcQueueSize();

		// Normal or edit queue mode
		if (m_inputMode == normal || m_inputMode == editQueue)
		{
			switch (key)
			{
			case 'q':
				// Key 'q' for quit
				ExitProc();
				break;
			case 'z':
				// show/hide NZBFilename
				m_showNzbname = !m_showNzbname;
				break;
			case 'w':
				// swicth window sizes
				if (m_queueWindowPercentage == 50)
				{
					m_queueWindowPercentage = 100;
				}
				else if (m_queueWindowPercentage == 100 && m_inputMode != editQueue)
				{
					m_queueWindowPercentage = 0;
				}
				else
				{
					m_queueWindowPercentage = 50;
				}
				CalcWindowSizes();
				SetCurrentQueueEntry(m_selectedQueueEntry);
				break;
			case 'g':
				// group/ungroup files
				m_groupFiles = !m_groupFiles;
				SetCurrentQueueEntry(m_selectedQueueEntry);
				NeedUpdateData();
				break;
			}
		}

		// Normal mode
		if (m_inputMode == normal)
		{
			switch (key)
			{
			case 'p':
				// Key 'p' for pause
				if (!IsRemoteMode())
				{
					info(m_pauseDownload ? "Unpausing download" : "Pausing download");
				}
				ServerPauseUnpause(!m_pauseDownload);
				break;
			case 'e':
			case 10: // return
			case 13: // enter
				if (queueSize > 0)
				{
					m_inputMode = editQueue;
					if (m_queueWindowPercentage == 0)
					{
						m_queueWindowPercentage = 50;
					}
					return;
				}
				break;
			case 'r':
				// Download rate
				m_inputMode = downloadRate;
				m_inputNumberIndex = 0;
				m_inputValue = 0;
				return;
			case 't':
				// show/hide Timestamps
				m_showTimestamp = !m_showTimestamp;
				break;
			}
		}

		// Edit Queue mode
		if (m_inputMode == editQueue)
		{
			switch (key)
			{
			case 'e':
			case 10: // return
			case 13: // enter
				m_inputMode = normal;
				return;
			case KEY_DOWN:
				if (m_selectedQueueEntry < queueSize - 1)
				{
					SetCurrentQueueEntry(m_selectedQueueEntry + 1);
				}
				break;
			case KEY_UP:
				if (m_selectedQueueEntry > 0)
				{
					SetCurrentQueueEntry(m_selectedQueueEntry - 1);
				}
				break;
			case KEY_PPAGE:
				if (m_selectedQueueEntry > 0)
				{
					if (m_selectedQueueEntry == m_queueScrollOffset)
					{
						m_queueScrollOffset -= m_queueWinClientHeight;
						SetCurrentQueueEntry(m_selectedQueueEntry - m_queueWinClientHeight);
					}
					else
					{
						SetCurrentQueueEntry(m_queueScrollOffset);
					}
				}
				break;
			case KEY_NPAGE:
				if (m_selectedQueueEntry < queueSize - 1)
				{
					if (m_selectedQueueEntry == m_queueScrollOffset + m_queueWinClientHeight - 1)
					{
						m_queueScrollOffset += m_queueWinClientHeight;
						SetCurrentQueueEntry(m_selectedQueueEntry + m_queueWinClientHeight);
					}
					else
					{
						SetCurrentQueueEntry(m_queueScrollOffset + m_queueWinClientHeight - 1);
					}
				}
				break;
			case KEY_HOME:
				SetCurrentQueueEntry(0);
				break;
			case KEY_END:
				SetCurrentQueueEntry(queueSize > 0 ? queueSize - 1 : 0);
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
					SetCurrentQueueEntry(m_selectedQueueEntry);
				}
				break;
			case 'u':
				if (EditQueue(DownloadQueue::eaFileMoveOffset, -1))
				{
					SetCurrentQueueEntry(m_selectedQueueEntry - 1);
				}
				break;
			case 'n':
				if (EditQueue(DownloadQueue::eaFileMoveOffset, +1))
				{
					SetCurrentQueueEntry(m_selectedQueueEntry + 1);
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
					SetCurrentQueueEntry(queueSize > 0 ? queueSize - 1 : 0);
				}
				break;
			}
		}

		// Edit download rate input mode
		if (m_inputMode == downloadRate)
		{
			// Numbers
			if (m_inputNumberIndex < 5 && key >= '0' && key <= '9')
			{
				m_inputValue = (m_inputValue * 10) + (key - '0');
				m_inputNumberIndex++;
			}
			// Enter
			else if (key == 10 || key == 13)
			{
				ServerSetDownloadRate(m_inputValue * 1024);
				m_inputMode = normal;
				return;
			}
			// Escape
			else if (key == 27)
			{
				m_inputMode = normal;
				return;
			}
			// Backspace
			else if (m_inputNumberIndex > 0 && key == KEY_BACKSPACE)
			{
				int remain = m_inputValue % 10;

				m_inputValue = (m_inputValue - remain) / 10;
				m_inputNumberIndex--;
			}
		}

		key = ReadConsoleKey();
	}
}

int NCursesFrontend::ReadConsoleKey()
{
#ifdef WIN32
	HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
	DWORD NumberOfEvents;
	BOOL ok = GetNumberOfConsoleInputEvents(hConsole, &NumberOfEvents);
	if (ok && NumberOfEvents > 0)
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
