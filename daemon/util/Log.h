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


#ifndef LOG_H
#define LOG_H

#include "NString.h"
#include "Thread.h"

void error(const char* msg, ...) PRINTF_SYNTAX(1);
void warn(const char* msg, ...) PRINTF_SYNTAX(1);
void info(const char* msg, ...) PRINTF_SYNTAX(1);
void detail(const char* msg, ...) PRINTF_SYNTAX(1);

#ifdef DEBUG
#ifdef HAVE_VARIADIC_MACROS
void debug(const char* filename, const char* funcname, int lineNr, const char* msg, ...) PRINTF_SYNTAX(4);
#else
void debug(const char* msg, ...) PRINTF_SYNTAX(1);
#endif
#endif

class Message
{
public:
	enum EKind
	{
		mkInfo,
		mkWarning,
		mkError,
		mkDebug,
		mkDetail
	};

	Message(uint32 id, EKind kind, time_t time, const char* text) :
		m_id(id), m_kind(kind), m_time(time), m_text(text) {}
	uint32 GetId() { return m_id; }
	EKind GetKind() { return m_kind; }
	time_t GetTime() { return m_time; }
	const char* GetText() { return m_text; }

private:
	uint32 m_id;
	EKind m_kind;
	time_t m_time;
	CString m_text;

	friend class Log;
};

typedef std::deque<Message> MessageList;
typedef GuardedPtr<MessageList> GuardedMessageList;

class Debuggable;
class DiskFile;

class Log
{
public:
	Log();
	~Log();
	GuardedMessageList GuardMessages() { return GuardedMessageList(&m_messages, &m_logMutex); }
	void Clear();
	void ResetLog();
	void InitOptions();
	void RegisterDebuggable(Debuggable* debuggable);
	void UnregisterDebuggable(Debuggable* debuggable);
	void LogDebugInfo();
	void IntervalCheck();

private:
	typedef std::list<Debuggable*> Debuggables;

	Mutex m_logMutex;
	MessageList m_messages;
	Debuggables m_debuggables;
	Mutex m_debugMutex;
	CString m_logFilename;
	std::unique_ptr<DiskFile> m_logFile;
	uint32 m_idGen = 0;
	time_t m_lastWritten = 0;
	bool m_optInit = false;
#ifdef DEBUG
	bool m_extraDebug;
#endif

	void Filelog(const char* msg, ...) PRINTF_SYNTAX(2);
	void AddMessage(Message::EKind kind, const char* text);
	void RotateLog();

	friend void error(const char* msg, ...);
	friend void warn(const char* msg, ...);
	friend void info(const char* msg, ...);
	friend void detail(const char* msg, ...);
#ifdef DEBUG
#ifdef HAVE_VARIADIC_MACROS
	friend void debug(const char* filename, const char* funcname, int lineNr, const char* msg, ...);
#else
	friend void debug(const char* msg, ...);
#endif
#endif
};

#ifdef DEBUG
#ifdef HAVE_VARIADIC_MACROS
#define debug(...) debug(__FILE__, FUNCTION_MACRO_NAME, __LINE__, __VA_ARGS__)
#endif
#else
#define debug(...) do { } while(0)
#endif

extern Log* g_Log;

class Debuggable
{
public:
	Debuggable() { g_Log->RegisterDebuggable(this); }
	virtual ~Debuggable() { g_Log->UnregisterDebuggable(this); }
protected:
	virtual void LogDebugInfo() = 0;
	friend class Log;
};

#endif
