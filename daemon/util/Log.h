/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef LOG_H
#define LOG_H

#include <deque>
#include <list>
#include <time.h>

#include "Thread.h"

void error(const char* msg, ...);
void warn(const char* msg, ...);
void info(const char* msg, ...);
void detail(const char* msg, ...);

#ifdef DEBUG
#ifdef HAVE_VARIADIC_MACROS
	void debug(const char* filename, const char* funcname, int lineNr, const char* msg, ...);
#else
	void debug(const char* msg, ...);
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

private:
	unsigned int		m_id;
	EKind				m_kind;
	time_t				m_time;
	char*				m_text;

	friend class Log;

public:
						Message(unsigned int id, EKind kind, time_t time, const char* text);
						~Message();
	unsigned int		GetID() { return m_id; }
	EKind				GetKind() { return m_kind; }
	time_t				GetTime() { return m_time; }
	const char*			GetText() { return m_text; }
};

typedef std::deque<Message*> MessageListBase;

class MessageList: public MessageListBase
{
public:
						~MessageList();
	void				Clear();
};

class Debuggable
{
protected:
	virtual void		LogDebugInfo() = 0;
	friend class Log;
};

class Log
{
public:
	typedef std::list<Debuggable*>	Debuggables;

private:
	Mutex				m_logMutex;
	MessageList			m_messages;
	Debuggables			m_debuggables;
	Mutex				m_debugMutex;
	char*				m_logFilename;
	unsigned int		m_idGen;
	time_t				m_lastWritten;
	bool				m_optInit;
#ifdef DEBUG
	bool				m_extraDebug;
#endif

						Log();
						~Log();
	void				Filelog(const char* msg, ...);
	void				AddMessage(Message::EKind kind, const char* text);
	void				RotateLog();

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
	
public:
	static void			Init();
	static void			Final();
	MessageList*		LockMessages();
	void				UnlockMessages();
	void				Clear();
	void				ResetLog();
	void				InitOptions();
	void				RegisterDebuggable(Debuggable* debuggable);
	void				UnregisterDebuggable(Debuggable* debuggable);
	void				LogDebugInfo();
};

#ifdef DEBUG
#ifdef HAVE_VARIADIC_MACROS
#define debug(...)   debug(__FILE__, FUNCTION_MACRO_NAME, __LINE__, __VA_ARGS__)
#endif
#else
#define debug(...)   do { } while(0)
#endif

extern Log* g_pLog;

#endif
