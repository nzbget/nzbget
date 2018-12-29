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
#include "Options.h"
#include "Log.h"
#include "Util.h"
#include "FileSystem.h"

Log::Log()
{
	g_Log = this;

#ifdef DEBUG
	m_extraDebug = FileSystem::FileExists("extradebug");
#endif
}

Log::~Log()
{
	g_Log = nullptr;
}

void Log::LogDebugInfo()
{
	info("--------------------------------------------");
	info("Dumping debug info to log");
	info("--------------------------------------------");

	Guard guard(m_debugMutex);
	for (Debuggable* debuggable : m_debuggables)
	{
		debuggable->LogDebugInfo();
	}

	info("--------------------------------------------");
}

void Log::Filelog(const char* msg, ...)
{
	if (m_logFilename.Empty())
	{
		return;
	}

	char tmp2[1024];

	va_list ap;
	va_start(ap, msg);
	vsnprintf(tmp2, 1024, msg, ap);
	tmp2[1024-1] = '\0';
	va_end(ap);

	time_t rawtime = Util::CurrentTime() + g_Options->GetTimeCorrection();

	char time[50];
	Util::FormatTime(rawtime, time, 50);

	if ((int)rawtime/86400 != (int)m_lastWritten/86400 && g_Options->GetWriteLog() == Options::wlRotate)
	{
		if (m_logFile)
		{
			m_logFile.reset();
		}
		RotateLog();
	}

	m_lastWritten = rawtime;

	if (!m_logFile)
	{
		m_logFile = std::make_unique<DiskFile>();
		if (!m_logFile->Open(m_logFilename, DiskFile::omAppend))
		{
			perror(m_logFilename);
			m_logFile.reset();
			return;
		}
	}

	m_logFile->Seek(0, DiskFile::soEnd);

#ifdef DEBUG
#ifdef WIN32
	uint64 processId = GetCurrentProcessId();
	uint64 threadId = GetCurrentThreadId();
#else
	uint64 processId = (uint64)getpid();
	uint64 threadId = (uint64)pthread_self();
#endif
	m_logFile->Print("%s\t%" PRIu64 "\t%" PRIu64 "\t%s%s", time, processId, threadId, tmp2, LINE_ENDING);
#else
	m_logFile->Print("%s\t%s%s", time, tmp2, LINE_ENDING);
#endif

	m_logFile->Flush();
}

void Log::IntervalCheck()
{
	// Close log-file on idle (if last write into log was more than a second ago)
	if (m_logFile)
	{
		time_t curTime = Util::CurrentTime() + g_Options->GetTimeCorrection();
		if (std::abs(curTime - m_lastWritten) > 1)
		{
			Guard guard(m_logMutex);
			m_logFile.reset();
		}
	}
}

#ifdef DEBUG
#undef debug
#ifdef HAVE_VARIADIC_MACROS
void debug(const char* filename, const char* funcname, int lineNr, const char* msg, ...)
#else
void debug(const char* msg, ...)
#endif
{
	if (!g_Log)
	{
		return;
	}

	char tmp1[1024];

	va_list ap;
	va_start(ap, msg);
	vsnprintf(tmp1, 1024, msg, ap);
	tmp1[1024-1] = '\0';
	va_end(ap);

	BString<1024> tmp2;
#ifdef HAVE_VARIADIC_MACROS
	if (funcname)
	{
		tmp2.Format("%s (%s:%i:%s)", tmp1, FileSystem::BaseFileName(filename), lineNr, funcname);
	}
	else
	{
		tmp2.Format("%s (%s:%i)", tmp1, FileSystem::BaseFileName(filename), lineNr);
	}
#else
	tmp2.Format("%s", tmp1);
#endif

	Guard guard(g_Log->m_logMutex);

	if (!g_Options && g_Log->m_extraDebug)
	{
		printf("%s\n", *tmp2);
	}

	Options::EMessageTarget messageTarget = g_Options ? g_Options->GetDebugTarget() : Options::mtScreen;
	if (messageTarget == Options::mtScreen || messageTarget == Options::mtBoth)
	{
		g_Log->AddMessage(Message::mkDebug, tmp2);
	}
	if (messageTarget == Options::mtLog || messageTarget == Options::mtBoth)
	{
		g_Log->Filelog("DEBUG\t%s", *tmp2);
	}
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

	Guard guard(g_Log->m_logMutex);

	Options::EMessageTarget messageTarget = g_Options ? g_Options->GetErrorTarget() : Options::mtBoth;
	if (messageTarget == Options::mtScreen || messageTarget == Options::mtBoth)
	{
		g_Log->AddMessage(Message::mkError, tmp2);
	}
	if (messageTarget == Options::mtLog || messageTarget == Options::mtBoth)
	{
		g_Log->Filelog("ERROR\t%s", tmp2);
	}
}

void warn(const char* msg, ...)
{
	char tmp2[1024];

	va_list ap;
	va_start(ap, msg);
	vsnprintf(tmp2, 1024, msg, ap);
	tmp2[1024-1] = '\0';
	va_end(ap);

	Guard guard(g_Log->m_logMutex);

	Options::EMessageTarget messageTarget = g_Options ? g_Options->GetWarningTarget() : Options::mtScreen;
	if (messageTarget == Options::mtScreen || messageTarget == Options::mtBoth)
	{
		g_Log->AddMessage(Message::mkWarning, tmp2);
	}
	if (messageTarget == Options::mtLog || messageTarget == Options::mtBoth)
	{
		g_Log->Filelog("WARNING\t%s", tmp2);
	}
}

void info(const char* msg, ...)
{
	char tmp2[1024];

	va_list ap;
	va_start(ap, msg);
	vsnprintf(tmp2, 1024, msg, ap);
	tmp2[1024-1] = '\0';
	va_end(ap);

	Guard guard(g_Log->m_logMutex);

	Options::EMessageTarget messageTarget = g_Options ? g_Options->GetInfoTarget() : Options::mtScreen;
	if (messageTarget == Options::mtScreen || messageTarget == Options::mtBoth)
	{
		g_Log->AddMessage(Message::mkInfo, tmp2);
	}
	if (messageTarget == Options::mtLog || messageTarget == Options::mtBoth)
	{
		g_Log->Filelog("INFO\t%s", tmp2);
	}
}

void detail(const char* msg, ...)
{
	char tmp2[1024];

	va_list ap;
	va_start(ap, msg);
	vsnprintf(tmp2, 1024, msg, ap);
	tmp2[1024-1] = '\0';
	va_end(ap);

	Guard guard(g_Log->m_logMutex);

	Options::EMessageTarget messageTarget = g_Options ? g_Options->GetDetailTarget() : Options::mtScreen;
	if (messageTarget == Options::mtScreen || messageTarget == Options::mtBoth)
	{
		g_Log->AddMessage(Message::mkDetail, tmp2);
	}
	if (messageTarget == Options::mtLog || messageTarget == Options::mtBoth)
	{
		g_Log->Filelog("DETAIL\t%s", tmp2);
	}
}


void Log::Clear()
{
	Guard guard(m_logMutex);
	m_messages.clear();
}

void Log::AddMessage(Message::EKind kind, const char * text)
{
	m_messages.emplace_back(++m_idGen, kind, Util::CurrentTime(), text);

	if (m_optInit && g_Options)
	{
		while (m_messages.size() > (uint32)g_Options->GetLogBuffer())
		{
			m_messages.pop_front();
		}
	}
}

void Log::ResetLog()
{
	FileSystem::DeleteFile(g_Options->GetLogFile());
}

void Log::RotateLog()
{
	BString<1024> directory = g_Options->GetLogFile();

	// split the full filename into path, basename and extension
	char* baseName = FileSystem::BaseFileName(directory);
	if (baseName > directory)
	{
		baseName[-1] = '\0';
	}

	BString<1024> baseExt;
	char* ext = strrchr(baseName, '.');
	if (ext && ext > baseName)
	{
		baseExt = ext;
		ext[0] = '\0';
	}

	BString<1024> fileMask("%s-####-##-##%s", baseName, *baseExt);

	time_t curTime = Util::CurrentTime() + g_Options->GetTimeCorrection();
	int curDay = (int)curTime / 86400;
	BString<1024> fullFilename;

	WildMask mask(fileMask, true);
	DirBrowser dir(directory);
	while (const char* filename = dir.Next())
	{
		if (mask.Match(filename))
		{
			fullFilename.Format("%s%c%s", *directory, PATH_SEPARATOR, filename);

			struct tm tm;
			memset(&tm, 0, sizeof(tm));
			tm.tm_year = atoi(filename + mask.GetMatchStart(0)) - 1900;
			tm.tm_mon = atoi(filename + mask.GetMatchStart(1)) - 1;
			tm.tm_mday = atoi(filename + mask.GetMatchStart(2));
			time_t fileTime = Util::Timegm(&tm);
			int fileDay = (int)fileTime / 86400;

			if (fileDay <= curDay - g_Options->GetRotateLog())
			{
				BString<1024> message("Deleting old log-file %s\n", filename);
				g_Log->AddMessage(Message::mkInfo, message);

				FileSystem::DeleteFile(fullFilename);
			}
		}
	}

	struct tm tm;
	gmtime_r(&curTime, &tm);
	fullFilename.Format("%s%c%s-%i-%.2i-%.2i%s", *directory, PATH_SEPARATOR,
		baseName, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, *baseExt);

	m_logFilename = fullFilename;
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
	const char* messageType[] = { "INFO", "WARNING", "ERROR", "DEBUG", "DETAIL"};

	if (g_Options->GetWriteLog() != Options::wlNone && g_Options->GetLogFile())
	{
		m_logFilename = g_Options->GetLogFile();
		if (g_Options->GetServerMode() && g_Options->GetWriteLog() == Options::wlReset)
		{
			g_Log->ResetLog();
		}
	}

	m_idGen = 0;

	for (uint32 i = 0; i < m_messages.size(); )
	{
		Message& message = m_messages.at(i);
		Options::EMessageTarget target = Options::mtNone;
		switch (message.GetKind())
		{
			case Message::mkDebug:
				target = g_Options->GetDebugTarget();
				break;
			case Message::mkDetail:
				target = g_Options->GetDetailTarget();
				break;
			case Message::mkInfo:
				target = g_Options->GetInfoTarget();
				break;
			case Message::mkWarning:
				target = g_Options->GetWarningTarget();
				break;
			case Message::mkError:
				target = g_Options->GetErrorTarget();
				break;
		}

		if (target == Options::mtLog || target == Options::mtBoth)
		{
			Filelog("%s\t%s", messageType[message.GetKind()], message.GetText());
		}

		if (target == Options::mtLog || target == Options::mtNone)
		{
			m_messages.erase(m_messages.begin() + i);
		}
		else
		{
			message.m_id = ++m_idGen;
			i++;
		}
	}

	m_optInit = true;
}

void Log::RegisterDebuggable(Debuggable* debuggable)
{
	Guard guard(m_debugMutex);
	m_debuggables.push_back(debuggable);
}

void Log::UnregisterDebuggable(Debuggable* debuggable)
{
	Guard guard(m_debugMutex);
	m_debuggables.remove(debuggable);
}
