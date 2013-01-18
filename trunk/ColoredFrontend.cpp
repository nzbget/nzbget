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
	m_bSummary = true;
	m_bNeedGoBack = false;
#ifdef WIN32
	m_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
}

void ColoredFrontend::BeforePrint()
{
	if (m_bNeedGoBack)
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
		m_bNeedGoBack = false;
	}
}

void ColoredFrontend::PrintStatus()
{
	char tmp[1024];
	char timeString[100];
	timeString[0] = '\0';
	int iCurrentDownloadSpeed = m_bStandBy ? 0 : m_iCurrentDownloadSpeed;

	if (iCurrentDownloadSpeed > 0 && !(m_bPauseDownload || m_bPauseDownload2))
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

#ifdef WIN32
	char* szControlSeq = "";
#else
	printf("\033[s");
	const char* szControlSeq = "\033[K";
#endif

	snprintf(tmp, 1024, " %d threads, %.*f KB/s, %.2f MB remaining%s%s%s%s%s\n", 
		m_iThreadCount, (iCurrentDownloadSpeed >= 10*1024 ? 0 : 1), (float)iCurrentDownloadSpeed / 1024.0, 
		(float)(Util::Int64ToFloat(m_lRemainingSize) / 1024.0 / 1024.0), timeString, szPostStatus, 
		m_bPauseDownload || m_bPauseDownload2 ? (m_bStandBy ? ", Paused" : ", Pausing") : "",
		szDownloadLimit, szControlSeq);
	tmp[1024-1] = '\0';
	printf("%s", tmp);
	m_bNeedGoBack = true;
} 

void ColoredFrontend::PrintMessage(Message * pMessage)
{
#ifdef WIN32
	switch (pMessage->GetKind())
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
	char* msg = strdup(pMessage->GetText());
	CharToOem(msg, msg);
	printf(" %s\n", msg);
	free(msg);
#else
	const char* msg = pMessage->GetText();
	switch (pMessage->GetKind())
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
