/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef STATMETER_H
#define STATMETER_H

#include <vector>
#include <time.h>

#include "Log.h"
#include "Thread.h"

class StatMeter : public Debuggable
{
private:
	// general
	Mutex				m_mutexStat;

	// speed meter
	static const int	SPEEDMETER_SLOTS = 30;	  
	static const int	SPEEDMETER_SLOTSIZE = 1;  //Split elapsed time into this number of secs.
	int					m_iSpeedBytes[SPEEDMETER_SLOTS];
	int					m_iSpeedTotalBytes;
	int					m_iSpeedTime[SPEEDMETER_SLOTS];
	int					m_iSpeedStartTime; 
	time_t				m_tSpeedCorrection;
#ifdef HAVE_SPINLOCK
	SpinLock			m_spinlockSpeed;
#else
	Mutex				m_mutexSpeed;
#endif

	int					m_iSpeedBytesIndex;
	long long			m_iAllBytes;
	time_t				m_tStartServer;
	time_t				m_tLastCheck;
	time_t				m_tStartDownload;
	time_t				m_tPausedFrom;
	bool				m_bStandBy;

	void				ResetSpeedStat();
	void				AdjustTimeOffset();

protected:
	virtual void		LogDebugInfo();

public:
						StatMeter();
						~StatMeter();
	int					CalcCurrentDownloadSpeed();
	void				AddSpeedReading(int iBytes);
	void				CalcTotalStat(int* iUpTimeSec, int* iDnTimeSec, long long* iAllBytes, bool* bStandBy);
	bool				GetStandBy() { return m_bStandBy; }
	void				CheckTime();
	void				EnterLeaveStandBy(bool bEnter);
};

#endif
