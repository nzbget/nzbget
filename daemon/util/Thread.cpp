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

#include "nzbget.h"
#include "Log.h"
#include "Thread.h"

int Thread::m_threadCount = 1; // take the main program thread into account
std::unique_ptr<Mutex> Thread::m_threadMutex;


Mutex::Mutex()
{
#ifdef WIN32
	m_mutexObj = (CRITICAL_SECTION*)malloc(sizeof(CRITICAL_SECTION));
	InitializeCriticalSection((CRITICAL_SECTION*)m_mutexObj);
#else
	m_mutexObj = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init((pthread_mutex_t*)m_mutexObj, nullptr);
#endif
}

Mutex::~ Mutex()
{
#ifdef WIN32
	DeleteCriticalSection((CRITICAL_SECTION*)m_mutexObj);
#else
	pthread_mutex_destroy((pthread_mutex_t*)m_mutexObj);
#endif
	free(m_mutexObj);
}

void Mutex::Lock()
{
#ifdef WIN32
	EnterCriticalSection((CRITICAL_SECTION*)m_mutexObj);
#ifdef DEBUG
	// CriticalSections on Windows can be locked many times from the same thread,
	// but we do not want this and must treat such situations as errors and detect them.
	if (((CRITICAL_SECTION*)m_mutexObj)->RecursionCount > 1)
	{
		error("Internal program error: inconsistent thread-lock detected");
	}
#endif
#else
	pthread_mutex_lock((pthread_mutex_t*)m_mutexObj);
#endif
}

void Mutex::Unlock()
{
#ifdef WIN32
	LeaveCriticalSection((CRITICAL_SECTION*)m_mutexObj);
#else
	pthread_mutex_unlock((pthread_mutex_t*)m_mutexObj);
#endif
}


void Thread::Init()
{
	debug("Initializing global thread data");

	m_threadMutex = std::make_unique<Mutex>();
}

void Thread::Final()
{
	debug("Finalizing global thread data");

	m_threadMutex.reset();
}

Thread::Thread()
{
	debug("Creating Thread");

#ifdef WIN32
	m_threadObj = nullptr;
#else
	m_threadObj = (pthread_t*)malloc(sizeof(pthread_t));
	*((pthread_t*)m_threadObj) = 0;
#endif
	m_running = false;
	m_stopped = false;
	m_autoDestroy = false;
}

Thread::~Thread()
{
	debug("Destroying Thread");
#ifndef WIN32
	free(m_threadObj);
#endif
}

void Thread::Start()
{
	debug("Starting Thread");

	m_running = true;

	// NOTE: we must guarantee, that in a time we set m_bRunning
	// to value returned from pthread_create, the thread-object still exists.
	// This is not obviously!
	// pthread_create could wait long enough before returning result
	// back to allow the started thread to complete its job and terminate.
	// We lock mutex m_pMutexThread on calling pthread_create; the started thread
	// then also try to lock the mutex (see thread_handler) and therefore
	// must wait until we unlock it
	m_threadMutex->Lock();

#ifdef WIN32
	m_threadObj = (HANDLE)_beginthread(Thread::thread_handler, 0, (void *)this);
	m_running = m_threadObj != nullptr;
#else
	pthread_attr_t m_attr;
	pthread_attr_init(&m_attr);
	pthread_attr_setdetachstate(&m_attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setinheritsched(&m_attr , PTHREAD_INHERIT_SCHED);
	m_running = !pthread_create((pthread_t*)m_threadObj, &m_attr, Thread::thread_handler, (void *) this);
	pthread_attr_destroy(&m_attr);
#endif

	m_threadMutex->Unlock();
}

void Thread::Stop()
{
	debug("Stopping Thread");

	m_stopped = true;
}

void Thread::Resume()
{
	debug("Resuming Thread");

	m_stopped = false;
}

bool Thread::Kill()
{
	debug("Killing Thread");

	m_threadMutex->Lock();

#ifdef WIN32
	bool terminated = TerminateThread((HANDLE)m_threadObj, 0) != 0;
#else
	bool terminated = pthread_cancel(*(pthread_t*)m_threadObj) == 0;
#endif

	if (terminated)
	{
		m_threadCount--;
	}
	m_threadMutex->Unlock();
	return terminated;
}

#ifdef WIN32
void __cdecl Thread::thread_handler(void* object)
#else
void* Thread::thread_handler(void* object)
#endif
{
	m_threadMutex->Lock();
	m_threadCount++;
	m_threadMutex->Unlock();

	debug("Entering Thread-func");

	Thread* thread = (Thread*)object;

	thread->Run();

	debug("Thread-func exited");

	thread->m_running = false;

	if (thread->m_autoDestroy)
	{
		debug("Autodestroying Thread-object");
		delete thread;
	}

	m_threadMutex->Lock();
	m_threadCount--;
	m_threadMutex->Unlock();

#ifndef WIN32
	return nullptr;
#endif
}

int Thread::GetThreadCount()
{
	m_threadMutex->Lock();
	int threadCount = m_threadCount;
	m_threadMutex->Unlock();
	return threadCount;
}
