/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2004  Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007  Andrei Prygounkov <hugbug@users.sourceforge.net>
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


#ifndef THREAD_H
#define THREAD_H

#ifndef WIN32
#include <pthread.h>
#include <semaphore.h>
#endif

class Mutex
{
private:
#ifdef WIN32
	CRITICAL_SECTION		m_mutexObj;
#else
	pthread_mutex_t			m_mutexObj;
#endif
	
public:
							Mutex();
							~Mutex();
	void					Lock();
	void					Unlock();
};


class Semaphore
{
private:
#ifdef WIN32
	HANDLE					m_semObj;
#else
	sem_t					m_semObj;
#endif
	
public:
							Semaphore();
							Semaphore(int iValue);
							~Semaphore();
	void					Post();
	bool					Wait();
	bool					TryWait();
	bool					TimedWait(int iMSec);
	bool					IsLocked();
};


class Thread
{
private:
	static Mutex			m_mutexThread;
	static int				m_iThreadCount;
#ifdef WIN32
	HANDLE	 				m_Thread;
#else
	pthread_t 				m_Thread;
#endif
	bool 					m_bRunning;
	bool					m_bStopped;
	bool					m_bAutoDestroy;

#ifdef WIN32
	static void __cdecl 	thread_handler(void* pObject);
#else
	static void				*thread_handler(void* pObject);
#endif

public:
	Thread();
	virtual 				~Thread();

	virtual void 			Start();
	virtual void 			Stop();
	bool					Kill();

	bool 					IsStopped() { return m_bStopped; };
	bool 					IsRunning()	const { return m_bRunning; }
	void 					SetRunning(bool bOnOff) { m_bRunning = bOnOff; }
	bool					GetAutoDestroy() { return m_bAutoDestroy; }
	void					SetAutoDestroy(bool bAutoDestroy) { m_bAutoDestroy = bAutoDestroy; }
	static int				GetThreadCount();

	static void				Init();
	static void				Final();

protected:
	virtual void 			Run() {}; // Virtual function - override in derivatives
};

#endif
