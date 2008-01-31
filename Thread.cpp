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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <stdio.h>

#ifdef WIN32
#include <process.h>
#endif

#include "Log.h"
#include "Thread.h"

int Thread::m_iThreadCount = 1; // take the main program thread into account
Mutex Thread::m_mutexThread;

Mutex::Mutex()
{
#ifdef WIN32
	InitializeCriticalSection(&m_mutexObj);
#else
	pthread_mutex_init(&m_mutexObj, NULL);
#endif
}

Mutex::~ Mutex()
{
#ifdef WIN32
	DeleteCriticalSection(&m_mutexObj);
#else
	pthread_mutex_destroy(&m_mutexObj);
#endif
}

void Mutex::Lock()
{
#ifdef WIN32
	EnterCriticalSection(&m_mutexObj);
#ifdef DEBUG
	// CriticalSections on Windows can be locked many times from the same thread,
	// but we do not want this and must treat such situations as errors and detect them.
	if (m_mutexObj.RecursionCount > 1)
	{
		error("Internal program error: inconsistent thread-lock detected");
	}
#endif
#else
	pthread_mutex_lock(&m_mutexObj);
#endif
}

void Mutex::Unlock()
{
#ifdef WIN32
	LeaveCriticalSection(&m_mutexObj);
#else
	pthread_mutex_unlock(&m_mutexObj);
#endif
}


Semaphore::Semaphore()
{
#ifdef WIN32
	m_semObj = CreateSemaphore(NULL, 0, 1, NULL);
#else
	sem_init(&m_semObj, 0, 0);
#endif
}

Semaphore::Semaphore(int iValue)
{
#ifdef WIN32
	m_semObj = CreateSemaphore(NULL, iValue, iValue, NULL);
#else
	sem_init(&m_semObj, 0, iValue);
#endif
}

Semaphore::~ Semaphore()
{
#ifdef WIN32
	CloseHandle(m_semObj);
#else
	sem_destroy(&m_semObj);
#endif
}

void Semaphore::Post()
{
#ifdef WIN32
	ReleaseSemaphore(m_semObj, 1, NULL);
#else
	sem_post(&m_semObj);
#endif
}

bool Semaphore::Wait()
{
#ifdef WIN32
	return WaitForSingleObject(m_semObj, INFINITE) == WAIT_OBJECT_0;
#else
	return sem_wait(&m_semObj) == 0;
#endif
}

bool Semaphore::TryWait()
{
#ifdef WIN32
	return WaitForSingleObject(m_semObj, 0) == WAIT_OBJECT_0;
#else
	return sem_trywait(&m_semObj) == 0;
#endif
}

bool Semaphore::TimedWait(int iMSec)
{
#ifdef WIN32
	return WaitForSingleObject(m_semObj, iMSec) == WAIT_OBJECT_0;
#else
	struct timespec alarm;
	alarm.tv_sec = ::time(NULL) + iMSec / 1000;
	alarm.tv_nsec = (iMSec % 1000) * 1000;
	return sem_timedwait(&m_semObj, &alarm) == 0;
#endif
}

bool Semaphore::IsLocked()
{
#ifdef WIN32
	bool bCanLock = WaitForSingleObject(m_semObj, 0) == WAIT_OBJECT_0;
	if (bCanLock)
	{
		ReleaseSemaphore(m_semObj, 1, NULL);
	}
	return !bCanLock;
#else
	int iSemValue;
	sem_getvalue(&m_semObj, &iSemValue);
	return iSemValue <= 0;
#endif
}


void Thread::Init()
{
	debug("Initializing global thread data");
}

void Thread::Final()
{
	debug("Finalizing global thread data");
}

Thread::Thread()
{
	debug("Creating Thread");

	m_Thread = 0;
	m_bRunning = false;
	m_bStopped = false;
	m_bAutoDestroy = false;
}

Thread::~Thread()
{
	debug("Destroying Thread");
}

void Thread::Start()
{
	debug("Starting Thread");

	m_bRunning = true;
	
	// NOTE: we must garantee, that in a time we setting m_bRunning
	// to value returned from pthread_create, the thread-object still exists.
	// This is not obviously!
	// pthread_create could wait long enough before returning result
	// back to allow the started thread to complete it job
	// and terminate.
	// We lock mutex m_mutexThread on calling pthread_create; the started thread
	// then also try to lock the mutex (see thread_handler) and therefore
	// must wait until we unlock it
	m_mutexThread.Lock();

#ifdef WIN32
	m_Thread = (HANDLE)_beginthread(Thread::thread_handler, 0, (void *)this);
	m_bRunning = m_Thread != NULL;
#else
	pthread_attr_t m_Attr;
	pthread_attr_init(&m_Attr);
	pthread_attr_setdetachstate(&m_Attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setinheritsched(&m_Attr , PTHREAD_INHERIT_SCHED);
	m_bRunning = !pthread_create(&m_Thread, &m_Attr, Thread::thread_handler, (void *) this);
	pthread_attr_destroy(&m_Attr);
#endif

	m_mutexThread.Unlock();
}

void Thread::Stop()
{
	debug("Stopping Thread");

	m_bStopped = true;
} 

bool Thread::Kill()
{
	debug("Killing Thread");

	m_mutexThread.Lock();

#ifdef WIN32
	bool terminated = TerminateThread(m_Thread, 0) != 0;
#else
	bool terminated = pthread_cancel(m_Thread) == 0;
#endif

	if (terminated)
	{
		m_iThreadCount--;
	}
	m_mutexThread.Unlock();
	return terminated;
}

#ifdef WIN32
void __cdecl Thread::thread_handler(void* pObject)
#else
void* Thread::thread_handler(void* pObject)
#endif
{
	m_mutexThread.Lock();
	m_iThreadCount++;
	m_mutexThread.Unlock();

	debug("Entering Thread-func");

	Thread* pThread = (Thread*)pObject;

	pThread->Run();

	debug("Thread-func exited");
	
	pThread->m_bRunning = false;
	
	if (pThread->m_bAutoDestroy)
	{
		debug("Autodestroying Thread-object");
		delete pThread;
	}

	m_mutexThread.Lock();
	m_iThreadCount--;
	m_mutexThread.Unlock();

#ifndef WIN32
	return NULL;
#endif
}

int Thread::GetThreadCount()
{
	m_mutexThread.Lock();
	int iThreadCount = m_iThreadCount;
	m_mutexThread.Unlock();
	return iThreadCount;
}

