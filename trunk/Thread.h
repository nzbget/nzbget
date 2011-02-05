/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2010 Andrei Prygounkov <hugbug@users.sourceforge.net>
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

class Mutex
{
private:
	void*					m_pMutexObj;
	
public:
							Mutex();
							~Mutex();
	void					Lock();
	void					Unlock();
};

#ifdef HAVE_SPINLOCK
class SpinLock
{
private:
	volatile void			*m_pSpinLockObj;
	
public:
							SpinLock();
							~SpinLock();
	void					Lock();
	void					Unlock();
};
#endif

class Thread
{
private:
	static Mutex*			m_pMutexThread;
	static int				m_iThreadCount;
	void*	 				m_pThreadObj;
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
	static void				Init();
	static void				Final();

	virtual void 			Start();
	virtual void 			Stop();
	bool					Kill();

	bool 					IsStopped() { return m_bStopped; };
	bool 					IsRunning()	const { return m_bRunning; }
	void 					SetRunning(bool bOnOff) { m_bRunning = bOnOff; }
	bool					GetAutoDestroy() { return m_bAutoDestroy; }
	void					SetAutoDestroy(bool bAutoDestroy) { m_bAutoDestroy = bAutoDestroy; }
	static int				GetThreadCount();

protected:
	virtual void 			Run() {}; // Virtual function - override in derivatives
};

#endif
