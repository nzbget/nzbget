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

	// NOTE: "m_threadMutex" ensures that "t" lives until the very end of the function
	Guard guard(m_threadMutex);

	// start the new thread
	std::thread t([&]{
		{
			// trying to lock "m_threadMutex", this will wait until function "Start()" is completed
			// and "t" is detached.
			Guard guard(m_threadMutex);
		}

		thread_handler();
	});

	// save the native handle to be able to cancel (Kill) the thread and then detach
	m_threadObj = t.native_handle();
	t.detach();
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

void Thread::thread_handler()
{
	m_threadCount++;

	debug("Entering Thread-func");

	Run();

	debug("Thread-func exited");

	m_running = false;

	m_threadCount--;

	if (m_autoDestroy)
	{
		debug("Autodestroying Thread-object");
		delete this;
	}
}

int Thread::GetThreadCount()
{
	Guard guard(m_threadMutex);
	return m_threadCount;
}
