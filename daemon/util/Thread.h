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


#ifndef THREAD_H
#define THREAD_H

class Mutex
{
private:
	void*					m_mutexObj;

public:
							Mutex();
							~Mutex();
	void					Lock();
	void					Unlock();
};

class Thread
{
private:
	static std::unique_ptr<Mutex>	m_threadMutex;
	static int				m_threadCount;
	void*	 				m_threadObj;
	bool 					m_running;
	bool					m_stopped;
	bool					m_autoDestroy;

#ifdef WIN32
	static void __cdecl 	thread_handler(void* object);
#else
	static void				*thread_handler(void* object);
#endif

public:
							Thread();
	virtual 				~Thread();
	static void				Init();

	virtual void 			Start();
	virtual void 			Stop();
	virtual void 			Resume();
	bool					Kill();

	bool 					IsStopped() { return m_stopped; };
	bool 					IsRunning()	const { return m_running; }
	void 					SetRunning(bool onOff) { m_running = onOff; }
	bool					GetAutoDestroy() { return m_autoDestroy; }
	void					SetAutoDestroy(bool autoDestroy) { m_autoDestroy = autoDestroy; }
	static int				GetThreadCount();

protected:
	virtual void 			Run() {}; // Virtual function - override in derivatives
};

#endif
