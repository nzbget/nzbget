/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2010 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifndef WIN32
#include <unistd.h>
#endif

#include "nzbget.h"
#include "ColoredFrontend.h"
#include "Util.h"

ColoredFrontend::ColoredFrontend()
{
	m_summary = true;
	m_needGoBack = false;
#ifdef WIN32
	m_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
}

void ColoredFrontend::BeforePrint()
{
	if (m_needGoBack)
	{
		// go back one line
#ifdef WIN32
		CONSOLE_SCREEN_BUFFER_INFO BufInfo;
		GetConsoleScreenBufferInfo(m_hConsole, &BufInfo);
		BufInfo.dwCursorPosition.Y--;
		SetConsoleCursorPosition(m_hConsole, BufInfo.dwCursorPosition);
#else
		printf("\r\033[1A");
#endif
		m_needGoBack = false;
	}
}

void ColoredFrontend::PrintStatus()
{
	char tmp[1024];
	char timeString[100];
	timeString[0] = '\0';
	int currentDownloadSpeed = m_standBy ? 0 : m_currentDownloadSpeed;

	if (currentDownloadSpeed > 0 && !m_pauseDownload)
	{
		long long remain_sec = (long long)(m_remainingSize / currentDownloadSpeed);
		int h = (int)(remain_sec / 3600);
		int m = (int)((remain_sec % 3600) / 60);
		int s = (int)(remain_sec % 60);
		sprintf(timeString, " (~ %.2d:%.2d:%.2d)", h, m, s);
	}

	char downloadLimit[128];
	if (m_downloadLimit > 0)
	{
		sprintf(downloadLimit, ", Limit %i KB/s", m_downloadLimit / 1024);
	}
	else
	{
		downloadLimit[0] = 0;
	}

    char postStatus[128];
    if (m_postJobCount > 0)
    {
        sprintf(postStatus, ", %i post-job%s", m_postJobCount, m_postJobCount > 1 ? "s" : "");
    }
    else
    {
        postStatus[0] = 0;
    }

#ifdef WIN32
	char* controlSeq = "";
#else
	printf("\033[s");
	const char* controlSeq = "\033[K";
#endif

	char fileSize[20];
	char currendSpeed[20];
	snprintf(tmp, 1024, " %d threads, %s, %s remaining%s%s%s%s%s\n",
		m_threadCount, Util::FormatSpeed(currendSpeed, sizeof(currendSpeed), currentDownloadSpeed),
		Util::FormatSize(fileSize, sizeof(fileSize), m_remainingSize),
		timeString, postStatus, m_pauseDownload ? (m_standBy ? ", Paused" : ", Pausing") : "",
		downloadLimit, controlSeq);
	tmp[1024-1] = '\0';
	printf("%s", tmp);
	m_needGoBack = true;
} 

void ColoredFrontend::PrintMessage(Message * message)
{
#ifdef WIN32
	switch (message->GetKind())
	{
		case Message::mkDebug:
			SetConsoleTextAttribute(m_hConsole, 8);
			printf("[DEBUG]");
			break;
		case Message::mkError:
			SetConsoleTextAttribute(m_hConsole, 4);
			printf("[ERROR]");
			break; 
		case Message::mkWarning:
			SetConsoleTextAttribute(m_hConsole, 5);
			printf("[WARNING]");
			break;
		case Message::mkInfo:
			SetConsoleTextAttribute(m_hConsole, 2);
			printf("[INFO]");
			break;
		case Message::mkDetail:
			SetConsoleTextAttribute(m_hConsole, 2);
			printf("[DETAIL]");
			break;
	}
	SetConsoleTextAttribute(m_hConsole, 7);
	char* msg = strdup(message->GetText());
	CharToOem(msg, msg);
	printf(" %s\n", msg);
	free(msg);
#else
	const char* msg = message->GetText();
	switch (message->GetKind())
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
