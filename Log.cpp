/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#else
#include <pthread.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <stdio.h>

#include "nzbget.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"

extern Options* g_pOptions;

Log::Log()
{
	m_Messages.clear();
	m_iIDGen = 0;
	m_szLogFilename = NULL;
#ifdef DEBUG
	m_bExtraDebug = Util::FileExists("extradebug");
#endif
}

Log::~Log()
{
	Clear();
	if (m_szLogFilename)
	{
		free(m_szLogFilename);
	}
}

void Log::Filelog(const char* msg, ...)
{
	if (m_szLogFilename)
	{
		char tmp2[1024];

		va_list ap;
		va_start(ap, msg);
		vsnprintf(tmp2, 1024, msg, ap);
		tmp2[1024-1] = '\0';
		va_end(ap);

		time_t rawtime;
		time(&rawtime);
		
		char szTime[50];
#ifdef HAVE_CTIME_R_3
		ctime_r(&rawtime, szTime, 50);
#else
		ctime_r(&rawtime, szTime);
#endif
		szTime[50-1] = '\0';
		szTime[strlen(szTime) - 1] = '\0'; // trim LF

		FILE* file = fopen(m_szLogFilename, "ab+");
		if (file)
		{
#ifdef WIN32
			unsigned long iThreadId = GetCurrentThreadId();
#else
			unsigned long iThreadId = (unsigned long)pthread_self();
#endif
#ifdef DEBUG
			fprintf(file, "%s\t%lu\t%s%s", szTime, iThreadId, tmp2, LINE_ENDING);
#else
			fprintf(file, "%s\t%s%s", szTime, tmp2, LINE_ENDING);
#endif
			fclose(file);
		}
		else
		{
			perror(m_szLogFilename);
		}
	}
}

#ifdef DEBUG
#undef debug
#ifdef HAVE_VARIADIC_MACROS
void debug(const char* szFilename, const char* szFuncname, int iLineNr, const char* msg, ...)
#else
void debug(const char* msg, ...)
#endif
{
	char tmp1[1024];

	va_list ap;
	va_start(ap, msg);
	vsnprintf(tmp1, 1024, msg, ap);
	tmp1[1024-1] = '\0';
	va_end(ap);

	char tmp2[1024];
#ifdef HAVE_VARIADIC_MACROS
	if (szFuncname)
	{
		snprintf(tmp2, 1024, "%s (%s:%i:%s)", tmp1, Util::BaseFileName(szFilename), iLineNr, szFuncname);
	}
	else
	{
		snprintf(tmp2, 1024, "%s (%s:%i)", tmp1, Util::BaseFileName(szFilename), iLineNr);
	}
#else
	snprintf(tmp2, 1024, "%s", tmp1);
#endif
	tmp2[1024-1] = '\0';

	g_pLog->m_mutexLog.Lock();

	if (!g_pOptions && g_pLog->m_bExtraDebug)
	{
		printf("%s\n", tmp2);
	}

	Options::EMessageTarget eMessageTarget = g_pOptions ? g_pOptions->GetDebugTarget() : Options::mtScreen;
	if (eMessageTarget == Options::mtLog || eMessageTarget == Options::mtBoth)
	{
		g_pLog->Filelog("DEBUG\t%s", tmp2);
	}
	if (eMessageTarget == Options::mtScreen || eMessageTarget == Options::mtBoth)
	{
		g_pLog->AppendMessage(Message::mkDebug, tmp2);
	}

	g_pLog->m_mutexLog.Unlock();
}
#endif

void error(const char* msg, ...)
{
	char tmp2[1024];

	va_list ap;
	va_start(ap, msg);
	vsnprintf(tmp2, 1024, msg, ap);
	tmp2[1024-1] = '\0';
	va_end(ap);

	g_pLog->m_mutexLog.Lock();

	Options::EMessageTarget eMessageTarget = g_pOptions ? g_pOptions->GetErrorTarget() : Options::mtBoth;
	if (eMessageTarget == Options::mtLog || eMessageTarget == Options::mtBoth)
	{
		g_pLog->Filelog("ERROR\t%s", tmp2);
	}
	if (eMessageTarget == Options::mtScreen || eMessageTarget == Options::mtBoth)
	{
		g_pLog->AppendMessage(Message::mkError, tmp2);
	}

	g_pLog->m_mutexLog.Unlock();
}

void warn(const char* msg, ...)
{
	char tmp2[1024];

	va_list ap;
	va_start(ap, msg);
	vsnprintf(tmp2, 1024, msg, ap);
	tmp2[1024-1] = '\0';
	va_end(ap);

	g_pLog->m_mutexLog.Lock();

	Options::EMessageTarget eMessageTarget = g_pOptions ? g_pOptions->GetWarningTarget() : Options::mtScreen;
	if (eMessageTarget == Options::mtLog || eMessageTarget == Options::mtBoth)
	{
		g_pLog->Filelog("WARNING\t%s", tmp2);
	}
	if (eMessageTarget == Options::mtScreen || eMessageTarget == Options::mtBoth)
	{
		g_pLog->AppendMessage(Message::mkWarning, tmp2);
	}

	g_pLog->m_mutexLog.Unlock();
}

void info(const char* msg, ...)
{
	char tmp2[1024];

	va_list ap;
	va_start(ap, msg);
	vsnprintf(tmp2, 1024, msg, ap);
	tmp2[1024-1] = '\0';
	va_end(ap);

	g_pLog->m_mutexLog.Lock();

	Options::EMessageTarget eMessageTarget = g_pOptions ? g_pOptions->GetInfoTarget() : Options::mtScreen;
	if (eMessageTarget == Options::mtLog || eMessageTarget == Options::mtBoth)
	{
		g_pLog->Filelog("INFO\t%s", tmp2);
	}
	if (eMessageTarget == Options::mtScreen || eMessageTarget == Options::mtBoth)
	{
		g_pLog->AppendMessage(Message::mkInfo, tmp2);
	}

	g_pLog->m_mutexLog.Unlock();
}

void detail(const char* msg, ...)
{
	char tmp2[1024];

	va_list ap;
	va_start(ap, msg);
	vsnprintf(tmp2, 1024, msg, ap);
	tmp2[1024-1] = '\0';
	va_end(ap);

	g_pLog->m_mutexLog.Lock();

	Options::EMessageTarget eMessageTarget = g_pOptions ? g_pOptions->GetDetailTarget() : Options::mtScreen;
	if (eMessageTarget == Options::mtLog || eMessageTarget == Options::mtBoth)
	{
		g_pLog->Filelog("DETAIL\t%s", tmp2);
	}
	if (eMessageTarget == Options::mtScreen || eMessageTarget == Options::mtBoth)
	{
		g_pLog->AppendMessage(Message::mkDetail, tmp2);
	}

	g_pLog->m_mutexLog.Unlock();
}

void abort(const char* msg, ...)
{
	char tmp2[1024];

	va_list ap;
	va_start(ap, msg);
	vsnprintf(tmp2, 1024, msg, ap);
	tmp2[1024-1] = '\0';
	va_end(ap);

	g_pLog->m_mutexLog.Lock();

	printf("\n%s", tmp2);

	g_pLog->Filelog(tmp2);

	g_pLog->m_mutexLog.Unlock();

	exit(-1);
}

//************************************************************
// Message

Message::Message(unsigned int iID, EKind eKind, time_t tTime, const char* szText)
{
	m_iID = iID;
	m_eKind = eKind;
	m_tTime = tTime;
	if (szText)
	{
		m_szText = strdup(szText);
	}
	else
	{
		m_szText = NULL;
	}
}

Message::~ Message()
{
	if (m_szText)
	{
		free(m_szText);
	}
}

void Log::Clear()
{
	m_mutexLog.Lock();
	for (Messages::iterator it = m_Messages.begin(); it != m_Messages.end(); it++)
	{
		delete *it;
	}
	m_Messages.clear();
	m_mutexLog.Unlock();
}

void Log::AppendMessage(Message::EKind eKind, const char * szText)
{
	Message* pMessage = new Message(++m_iIDGen, eKind, time(NULL), szText);
	m_Messages.push_back(pMessage);

	if (g_pOptions)
	{
		while (m_Messages.size() > (unsigned int)g_pOptions->GetLogBufferSize())
		{
			Message* pMessage = m_Messages.front();
			delete pMessage;
			m_Messages.pop_front();
		}
	}
}

Log::Messages* Log::LockMessages()
{
	m_mutexLog.Lock();
	return &m_Messages;
}

void Log::UnlockMessages()
{
	m_mutexLog.Unlock();
}

void Log::ResetLog()
{
	remove(g_pOptions->GetLogFile());
}

/*
* During intializing stage (when options were not read yet) all messages
* are saved in screen log, even if they shouldn't (according to options).
* Method "InitOptions()" check all messages added to screen log during
* intializing stage and does three things:
* 1) save the messages to log-file (if they should according to options);
* 2) delete messages from screen log (if they should not be saved in screen log).
* 3) renumerate IDs
*/
void Log::InitOptions()
{
	const char* szMessageType[] = { "INFO", "WARNING", "ERROR", "DEBUG", "DETAIL"};

	if (g_pOptions->GetCreateLog() && g_pOptions->GetLogFile())
	{
		m_szLogFilename = strdup(g_pOptions->GetLogFile());
	}

	m_iIDGen = 0;

	for (unsigned int i = 0; i < m_Messages.size(); )
	{
		Message* pMessage = m_Messages.at(i);
		Options::EMessageTarget eTarget = Options::mtNone;
		switch (pMessage->GetKind())
		{
			case Message::mkDebug:
				eTarget = g_pOptions->GetDebugTarget();
				break;
			case Message::mkDetail:
				eTarget = g_pOptions->GetDetailTarget();
				break;
			case Message::mkInfo:
				eTarget = g_pOptions->GetInfoTarget();
				break;
			case Message::mkWarning:
				eTarget = g_pOptions->GetWarningTarget();
				break;
			case Message::mkError:
				eTarget = g_pOptions->GetErrorTarget();
				break;
		}

		if (eTarget == Options::mtLog || eTarget == Options::mtBoth)
		{
			Filelog("%s\t%s", szMessageType[pMessage->GetKind()], pMessage->GetText());
		}

		if (eTarget == Options::mtLog || eTarget == Options::mtNone)
		{
			delete pMessage;
			m_Messages.erase(m_Messages.begin() + i);
		}
		else
		{
			pMessage->m_iID = ++m_iIDGen;
			i++;
		}
	}
}
