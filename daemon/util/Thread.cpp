/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "Log.h"
#include "Thread.h"

int Thread::m_threadCount = 1; // take the main program thread into account
std::unique_ptr<Mutex> Thread::m_threadMutex;


Mutex::Mutex()
{
#ifdef WIN32
#else
	pthread_mutex_init(&m_mutexObj, nullptr);
#endif
}

Mutex::~Mutex()
{
#ifdef WIN32
#else
	pthread_mutex_destroy(&m_mutexObj);
#endif
}

void Mutex::Lock()
{
#ifdef WIN32
	m_mutexObj.lock();
#else
	pthread_mutex_lock(&m_mutexObj);
#endif
}

void Mutex::Unlock()
{
#ifdef WIN32
	m_mutexObj.unlock();
#else
	pthread_mutex_unlock(&m_mutexObj);
#endif
}


ConditionVar::ConditionVar()
{
#ifdef WIN32
#else
	pthread_cond_init(&m_condObj, nullptr);
#endif
}

ConditionVar::~ConditionVar()
{
#ifdef WIN32
#else
	pthread_cond_destroy(&m_condObj);
#endif
}

void ConditionVar::Wait(Mutex& mutex)
{
#ifdef WIN32
	std::unique_lock<std::mutex> lock(mutex.m_mutexObj, std::adopt_lock);
	m_condObj.wait(lock);
	lock.release();
#else
	pthread_cond_wait(&m_condObj, &mutex.m_mutexObj);
#endif
}

void ConditionVar::Wait(Mutex& mutex, WaitFunc pred)
{
	while (!pred())
	{
		Wait(mutex);
	}
}

#ifndef WIN32
timespec ConditionVar::UntilTime(int msec)
{
	timeval tm;
	gettimeofday(&tm, nullptr);

	timespec ts;
	ts.tv_sec = tm.tv_sec + msec / 1000;
	ts.tv_nsec = tm.tv_usec * 1000 + 1000 * 1000 * (msec % 1000);
	ts.tv_sec += ts.tv_nsec / (1000 * 1000 * 1000);
	ts.tv_nsec %= 1000 * 1000 * 1000;

	return ts;
}
#endif

void ConditionVar::WaitFor(Mutex& mutex, int msec)
{
#ifdef WIN32
	std::unique_lock<std::mutex> lock(mutex.m_mutexObj, std::adopt_lock);
	m_condObj.wait_for(lock, std::chrono::milliseconds(msec));
	lock.release();
#else
	timespec ts = UntilTime(msec);
	pthread_cond_timedwait(&m_condObj, &mutex.m_mutexObj, &ts);
#endif
}

void ConditionVar::WaitFor(Mutex& mutex, int msec, WaitFunc pred)
{
#ifdef WIN32
	std::unique_lock<std::mutex> lock(mutex.m_mutexObj, std::adopt_lock);
	m_condObj.wait_for(lock, std::chrono::milliseconds(msec), pred);
	lock.release();
#else
	timespec ts = UntilTime(msec);
	int rc = 0;
	while (rc == 0 && !pred())
	{
		rc = pthread_cond_timedwait(&m_condObj, &mutex.m_mutexObj, &ts);
	}
#endif
}

void ConditionVar::NotifyOne()
{
#ifdef WIN32
	m_condObj.notify_one();
#else
	pthread_cond_signal(&m_condObj);
#endif
}

void ConditionVar::NotifyAll()
{
#ifdef WIN32
	m_condObj.notify_all();
#else
	pthread_cond_broadcast(&m_condObj);
#endif
}


void Thread::Init()
{
	debug("Initializing global thread data");

	m_threadMutex = std::make_unique<Mutex>();
}

Thread::Thread()
{
	debug("Creating Thread");
}

Thread::~Thread()
{
	debug("Destroying Thread");
}

void Thread::Start()
{
	debug("Starting Thread");

	m_running = true;

	// NOTE: we must guarantee, that in a time we set m_running
	// to value returned from pthread_create, the thread-object still exists.
	// This is not obvious!
	// pthread_create could wait long enough before returning result
	// back to allow the started thread to complete its job and terminate.
	// We lock mutex m_threadMutex on calling pthread_create; the started thread
	// then also try to lock the mutex (see thread_handler) and therefore
	// must wait until we unlock it
	Guard guard(m_threadMutex);

#ifdef WIN32
	m_threadObj = (HANDLE)_beginthread(Thread::thread_handler, 0, (void*)this);
	m_running = m_threadObj != 0;
#else
	pthread_attr_t m_attr;
	pthread_attr_init(&m_attr);
	pthread_attr_setdetachstate(&m_attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setinheritsched(&m_attr, PTHREAD_INHERIT_SCHED);
	m_running = !pthread_create(&m_threadObj, &m_attr, Thread::thread_handler, (void *) this);
	pthread_attr_destroy(&m_attr);
#endif
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

	Guard guard(m_threadMutex);

#ifdef WIN32
	bool terminated = TerminateThread(m_threadObj, 0) != 0;
#else
#ifdef HAVE_PTHREAD_CANCEL
	bool terminated = pthread_cancel(m_threadObj) == 0;
#else
	bool terminated = false;
	warn("Could not kill thread: thread cancelling isn't supported on this platform");
#endif
#endif

	if (terminated)
	{
		m_threadCount--;
	}
	return terminated;
}

#ifdef WIN32
void __cdecl Thread::thread_handler(void* object)
#else
void* Thread::thread_handler(void* object)
#endif
{
	{
		Guard guard(m_threadMutex);
		m_threadCount++;
	}

	debug("Entering Thread-func");

	Thread* thread = (Thread*)object;

	thread->Run();

	debug("Thread-func exited");

	if (thread->m_autoDestroy)
	{
		debug("Autodestroying Thread-object");
		delete thread;
	}
	else
	{
		thread->m_running = false;
	}

	{
		Guard guard(m_threadMutex);
		m_threadCount--;
	}

#ifndef WIN32
	return nullptr;
#endif
}

int Thread::GetThreadCount()
{
	Guard guard(m_threadMutex);
	return m_threadCount;
}
