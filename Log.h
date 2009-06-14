/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2009 Andrei Prygounkov <hugbug@users.sourceforge.net>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Revision$
 * $Date$
 *
 */


#ifndef LOG_H
#define LOG_H

#include <deque>
#include <time.h>

#include "Thread.h"

void error(const char* msg, ...);
void warn(const char* msg, ...);
void info(const char* msg, ...);
void detail(const char* msg, ...);
void abort(const char* msg, ...);

#ifdef DEBUG
#ifdef HAVE_VARIADIC_MACROS
	void debug(const char* szFilename, const char* szFuncname, int iLineNr, const char* msg, ...);
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
	unsigned int		m_iID;
	EKind				m_eKind;
	time_t				m_tTime;
	char*				m_szText;

public:
	Message(unsigned int iID, EKind eKind, time_t tTime, const char* szText);
	~Message();
	unsigned int		GetID() { return m_iID; }
	EKind				GetKind() { return m_eKind; }
	time_t				GetTime() { return m_tTime; }
	const char*			GetText() { return m_szText; }
};

class Log
{
public:
	typedef std::deque<Message*>	Messages;

private:
	Mutex				m_mutexLog;
	Messages			m_Messages;
	char*				m_szLogFilename;
	unsigned int		m_iIDGen;
#ifdef DEBUG
	bool				m_bExtraDebug;
#endif

	void				Filelog(const char* msg, ...);
	void				AppendMessage(Message::EKind eKind, const char* szText);

	friend void error(const char* msg, ...);
	friend void warn(const char* msg, ...);
	friend void info(const char* msg, ...);
	friend void abort(const char* msg, ...);
	friend void detail(const char* msg, ...);
#ifdef DEBUG
#ifdef HAVE_VARIADIC_MACROS
	friend void debug(const char* szFilename, const char* szFuncname, int iLineNr, const char* msg, ...);
#else	
	friend void debug(const char* msg, ...);
#endif
#endif
	
public:
						Log();
						~Log();
	Messages*			LockMessages();
	void				UnlockMessages();
	void				ResetLog();
	void				InitOptions();
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
