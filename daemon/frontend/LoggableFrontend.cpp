/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2004  Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2015  Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#include "nzbget.h"
#include "LoggableFrontend.h"
#include "Log.h"

LoggableFrontend::LoggableFrontend()
{
	debug("Creating LoggableFrontend");

	m_neededLogEntries = 0;
	m_summary = false;
	m_fileList = false;
}

void LoggableFrontend::Run()
{
	debug("Entering LoggableFrontend-loop");

	while (!IsStopped())
	{
		Update();
		usleep(m_updateInterval * 1000);
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

	MessageList* messages = LockMessages();
	if (!messages->empty())
	{
		Message* firstMessage = messages->front();
		int start = m_neededLogFirstId - firstMessage->GetId() + 1;
		if (start < 0)
		{
			PrintSkip();
			start = 0;
		}
		for (unsigned int i = (unsigned int)start; i < messages->size(); i++)
		{
			PrintMessage((*messages)[i]);
			m_neededLogFirstId = (*messages)[i]->GetId();
		}
	}
	UnlockMessages();

	PrintStatus();

	FreeData();

	fflush(stdout);
}

void LoggableFrontend::PrintMessage(Message * message)
{
#ifdef WIN32
	char* msg = strdup(message->GetText());
	CharToOem(msg, msg);
#else
	const char* msg = message->GetText();
#endif
	switch (message->GetKind())
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
#ifdef WIN32
	free(msg);
#endif
}

void LoggableFrontend::PrintSkip()
{
	printf(".....\n");
}
