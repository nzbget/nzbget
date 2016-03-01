/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
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
#include "ColoredFrontend.h"
#include "Util.h"

ColoredFrontend::ColoredFrontend()
{
	m_summary = true;
#ifdef WIN32
	m_console = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
}

void ColoredFrontend::BeforePrint()
{
	if (m_needGoBack)
	{
		// go back one line
#ifdef WIN32
		CONSOLE_SCREEN_BUFFER_INFO BufInfo;
		GetConsoleScreenBufferInfo(m_console, &BufInfo);
		BufInfo.dwCursorPosition.Y--;
		SetConsoleCursorPosition(m_console, BufInfo.dwCursorPosition);
#else
		printf("\r\033[1A");
#endif
		m_needGoBack = false;
	}
}

void ColoredFrontend::PrintStatus()
{
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

#ifdef WIN32
	const char* controlSeq = "";
#else
	printf("\033[s");
	const char* controlSeq = "\033[K";
#endif

	BString<1024> status(" %d threads, %s, %s remaining%s%s%s%s%s\n",
		m_threadCount, *Util::FormatSpeed(currentDownloadSpeed),
		*Util::FormatSize(m_remainingSize), *timeString, *postStatus,
		m_pauseDownload ? (m_standBy ? ", Paused" : ", Pausing") : "",
		*downloadLimit, controlSeq);
	printf("%s", *status);
	m_needGoBack = true;
}

void ColoredFrontend::PrintMessage(Message& message)
{
#ifdef WIN32
	switch (message.GetKind())
	{
		case Message::mkDebug:
			SetConsoleTextAttribute(m_console, 8);
			printf("[DEBUG]");
			break;
		case Message::mkError:
			SetConsoleTextAttribute(m_console, 4);
			printf("[ERROR]");
			break;
		case Message::mkWarning:
			SetConsoleTextAttribute(m_console, 5);
			printf("[WARNING]");
			break;
		case Message::mkInfo:
			SetConsoleTextAttribute(m_console, 2);
			printf("[INFO]");
			break;
		case Message::mkDetail:
			SetConsoleTextAttribute(m_console, 2);
			printf("[DETAIL]");
			break;
	}
	SetConsoleTextAttribute(m_console, 7);
	CString msg = message.GetText();
	CharToOem(msg, msg);
	printf(" %s\n", *msg);
#else
	const char* msg = message.GetText();
	switch (message.GetKind())
	{
		case Message::mkDebug:
			printf("[DEBUG] %s\033[K\n", msg);
			break;
		case Message::mkError:
			printf("\033[31m[ERROR]\033[39m %s\033[K\n", msg);
			break;
		case Message::mkWarning:
			printf("\033[35m[WARNING]\033[39m %s\033[K\n", msg);
			break;
		case Message::mkInfo:
			printf("\033[32m[INFO]\033[39m %s\033[K\n", msg);
			break;
		case Message::mkDetail:
			printf("\033[32m[DETAIL]\033[39m %s\033[K\n", msg);
			break;
	}
#endif
}

void ColoredFrontend::PrintSkip()
{
#ifdef WIN32
	printf(".....\n");
#else
	printf(".....\033[K\n");
#endif
}

void ColoredFrontend::BeforeExit()
{
	if (IsRemoteMode())
	{
		printf("\n");
	}
}
