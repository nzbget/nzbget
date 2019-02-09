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
#include "Util.h"
#include "LoggableFrontend.h"
#include "Log.h"

void LoggableFrontend::Run()
{
	debug("Entering LoggableFrontend-loop");

	while (!IsStopped())
	{
		Update();
		Wait(m_updateInterval);
	}
	// Printing the last messages
	Update();

	BeforeExit();

	debug("Exiting LoggableFrontend-loop");
}

void LoggableFrontend::Update()
{
	if (!PrepareData())
	{
		FreeData();
		return;
	}

	BeforePrint();

	{
		GuardedMessageList messages = GuardMessages();
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

	PrintStatus();

	FreeData();

	fflush(stdout);
}

void LoggableFrontend::PrintMessage(Message& message)
{
#ifdef WIN32
	CString cmsg = message.GetText();
	CharToOem(cmsg, cmsg);
	const char* msg = cmsg;
#else
	const char* msg = message.GetText();
#endif
	switch (message.GetKind())
	{
		case Message::mkDebug:
			printf("[DEBUG] %s\n", msg);
			break;
		case Message::mkError:
			printf("[ERROR] %s\n", msg);
			break;
		case Message::mkWarning:
			printf("[WARNING] %s\n", msg);
			break;
		case Message::mkInfo:
			printf("[INFO] %s\n", msg);
			break;
		case Message::mkDetail:
			printf("[DETAIL] %s\n", msg);
			break;
	}
}

void LoggableFrontend::PrintSkip()
{
	printf(".....\n");
}
