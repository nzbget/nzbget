/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "NServFrontend.h"
#include "Util.h"

NServFrontend::NServFrontend()
{
#ifdef WIN32
	m_console = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
}

void NServFrontend::Run()
{
	while (!IsStopped())
	{
		Update();
		Util::Sleep(100);
	}
	// Printing the last messages
	Update();
}

void NServFrontend::Update()
{
	BeforePrint();

	{
		GuardedMessageList messages = g_Log->GuardMessages();
		if (!messages->empty())
		{
			Message& firstMessage = messages->front();
			int start = m_neededLogFirstId - firstMessage.GetId() + 1;
			if (start < 0)
			{
				PrintSkip();
				start = 0;
			}
			for (uint32 i = (uint32)start; i < messages->size(); i++)
			{
				PrintMessage(messages->at(i));
				m_neededLogFirstId = messages->at(i).GetId();
			}
		}
	}

	fflush(stdout);
}

void NServFrontend::BeforePrint()
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

void NServFrontend::PrintMessage(Message& message)
{
#ifdef WIN32
	switch (message.GetKind())
	{
		case Message::mkDebug:
			SetConsoleTextAttribute(m_console, 8);
			printf("[DEBUG]  ");
			break;
		case Message::mkError:
			SetConsoleTextAttribute(m_console, 4);
			printf("[ERROR] ");
			break;
		case Message::mkWarning:
			SetConsoleTextAttribute(m_console, 5);
			printf("[WARNING]");
			break;
		case Message::mkInfo:
			SetConsoleTextAttribute(m_console, 2);
			printf("[INFO]  ");
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

void NServFrontend::PrintSkip()
{
#ifdef WIN32
	printf(".....\n");
#else
	printf(".....\033[K\n");
#endif
}
