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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#ifdef WIN32
#include <process.h>
#else
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#endif

#include "Log.h"
#include "Thread.h"

int Thread::m_iThreadCount = 1; // take the main program thread into account
Mutex* Thread::m_pMutexThread;

#ifndef WIN32
#ifndef HAVE_UNNAMED_SEMAPHORES	
int Semaphore::m_iID = 1;
Mutex Semaphore::m_mutexID;
#endif
#endif

Mutex::Mutex()
{
#ifdef WIN32
	m_pMutexObj = (CRITICAL_SECTION*)malloc(sizeof(CRITICAL_SECTION));
	InitializeCriticalSection((CRITICAL_SECTION*)m_pMutexObj);
#else
	m_pMutexObj = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init((pthread_mutex_t*)m_pMutexObj, NULL);
#endif
}

Mutex::~ Mutex()
{
#ifdef WIN32
	DeleteCriticalSection((CRITICAL_SECTION*)m_pMutexObj);
#else
	pthread_mutex_destroy((pthread_mutex_t*)m_pMutexObj);
#endif
	free(m_pMutexObj);
}

void Mutex::Lock()
{
#ifdef WIN32
	EnterCriticalSection((CRITICAL_SECTION*)m_pMutexObj);
#ifdef DEBUG
	// CriticalSections on Windows can be locked many times from the same thread,
	// but we do not want this and must treat such situations as errors and detect them.
	if (((CRITICAL_SECTION*)m_pMutexObj)->RecursionCount > 1)
	{
		error("Internal program error: inconsistent thread-lock detected");
	}
#endif
#else
	pthread_mutex_lock((pthread_mutex_t*)m_pMutexObj);
#endif
}

void Mutex::Unlock()
{
#ifdef WIN32
	LeaveCriticalSection((CRITICAL_SECTION*)m_pMutexObj);
#else
	pthread_mutex_unlock((pthread_mutex_t*)m_pMutexObj);
#endif
}


Semaphore::Semaphore()
{
#ifdef WIN32
	m_pSemObj = CreateSemaphore(NULL, 0, 1, NULL);
#else
	CreateSemObj(0);
#endif
}

Semaphore::Semaphore(int iValue)
{
#ifdef WIN32
	m_pSemObj = CreateSemaphore(NULL, iValue, iValue, NULL);
#else
	CreateSemObj(iValue);
#endif
}

#ifndef WIN32
void Semaphore::CreateSemObj(int iValue)
{
#ifdef HAVE_UNNAMED_SEMAPHORES
	m_pSemObj = (sem_t*)malloc(sizeof(sem_t));
	int iRet = sem_init((sem_t*)m_pSemObj, 0, iValue);
	if (iRet == -1)
	{
		error("sem_init failed: error %i", errno);
	}
#else
	m_mutexID.Lock();
	int iID = m_iID;
	m_iID++;
	m_mutexID.Unlock();
	snprintf(m_szName, sizeof(m_szName), "/NZBGET-%i", iID);
	m_pSemObj = sem_open(m_szName, O_CREAT, S_IRWXU, iValue);
	if (m_pSemObj == SEM_FAILED)
	{
		error("sem_open failed: error %i", errno);
	}
#endif
}
#endif

Semaphore::~ Semaphore()
{
#ifdef WIN32
	CloseHandle((HANDLE)m_pSemObj);
#else
#ifdef HAVE_UNNAMED_SEMAPHORES	
	sem_destroy((sem_t*)m_pSemObj);
	free(m_pSemObj);
#else
	sem_close((sem_t*)m_pSemObj);
	sem_unlink(m_szName);
#endif
#endif
}

void Semaphore::Post()
{
#ifdef WIN32
	ReleaseSemaphore((HANDLE)m_pSemObj, 1, NULL);
#else
	sem_post((sem_t*)m_pSemObj);
#endif
}

bool Semaphore::Wait()
{
#ifdef WIN32
	return WaitForSingleObject((HANDLE)m_pSemObj, INFINITE) == WAIT_OBJECT_0;
#else
	return sem_wait((sem_t*)m_pSemObj) == 0;
#endif
}

bool Semaphore::TryWait()
{
#ifdef WIN32
	return WaitForSingleObject((HANDLE)m_pSemObj, 0) == WAIT_OBJECT_0;
#else
	return sem_trywait((sem_t*)m_pSemObj) == 0;
#endif
}


void Thread::Init()
{
	debug("Initializing global thread data");

	m_pMutexThread = new Mutex();
}

void Thread::Final()
{
	debug("Finalizing global thread data");

	delete m_pMutexThread;
}

Thread::Thread()
{
	debug("Creating Thread");

#ifdef WIN32
	m_pThreadObj = NULL;
#else
	m_pThreadObj = (pthread_t*)malloc(sizeof(pthread_t));
	*((pthread_t*)m_pThreadObj) = 0;
#endif
	m_bRunning = false;
	m_bStopped = false;
	m_bAutoDestroy = false;
}

Thread::~Thread()
{
	debug("Destroying Thread");
#ifndef WIN32
	free(m_pThreadObj);
#endif
}

void Thread::Start()
{
	debug("Starting Thread");

	m_bRunning = true;
	
	// NOTE: we must guarantee, that in a time we set m_bRunning
	// to value returned from pthread_create, the thread-object still exists.
	// This is not obviously!
	// pthread_create could wait long enough before returning result
	// back to allow the started thread to complete its job and terminate.
	// We lock mutex m_pMutexThread on calling pthread_create; the started thread
	// then also try to lock the mutex (see thread_handler) and therefore
	// must wait until we unlock it
	m_pMutexThread->Lock();

#ifdef WIN32
	m_pThreadObj = (HANDLE)_beginthread(Thread::thread_handler, 0, (void *)this);
	m_bRunning = m_pThreadObj != NULL;
#else
	pthread_attr_t m_Attr;
	pthread_attr_init(&m_Attr);
	pthread_attr_setdetachstate(&m_Attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setinheritsched(&m_Attr , PTHREAD_INHERIT_SCHED);
	m_bRunning = !pthread_create((pthread_t*)m_pThreadObj, &m_Attr, Thread::thread_handler, (void *) this);
	pthread_attr_destroy(&m_Attr);
#endif

	m_pMutexThread->Unlock();
}

void Thread::Stop()
{
	debug("Stopping Thread");

	m_bStopped = true;
} 

bool Thread::Kill()
{
	debug("Killing Thread");

	m_pMutexThread->Lock();

#ifdef WIN32
	bool terminated = TerminateThread((HANDLE)m_pThreadObj, 0) != 0;
#else
	bool terminated = pthread_cancel(*(pthread_t*)m_pThreadObj) == 0;
#endif

	if (terminated)
	{
		m_iThreadCount--;
	}
	m_pMutexThread->Unlock();
	return terminated;
}

#ifdef WIN32
void __cdecl Thread::thread_handler(void* pObject)
#else
void* Thread::thread_handler(void* pObject)
#endif
{
	m_pMutexThread->Lock();
	m_iThreadCount++;
	m_pMutexThread->Unlock();

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

	m_pMutexThread->Lock();
	m_iThreadCount--;
	m_pMutexThread->Unlock();

#ifndef WIN32
	return NULL;
#endif
}

int Thread::GetThreadCount()
{
	m_pMutexThread->Lock();
	int iThreadCount = m_iThreadCount;
	m_pMutexThread->Unlock();
	return iThreadCount;
}
