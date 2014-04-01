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

class ServerVolume
{
public:
	typedef std::vector<long long>	VolumeArray;

private:
	VolumeArray			m_BytesPerSeconds;
	VolumeArray			m_BytesPerMinutes;
	VolumeArray			m_BytesPerHours;
	VolumeArray			m_BytesPerDays;
	int					m_iFirstDay;
	long long			m_lTotalBytes;
	time_t				m_tDataTime;
	int					m_iSecSlot;
	int					m_iMinSlot;
	int					m_iHourSlot;
	int					m_iDaySlot;

public:
						ServerVolume();
	VolumeArray*		BytesPerSeconds() { return &m_BytesPerSeconds; }
	VolumeArray*		BytesPerMinutes() { return &m_BytesPerMinutes; }
	VolumeArray*		BytesPerHours() { return &m_BytesPerHours; }
	VolumeArray*		BytesPerDays() { return &m_BytesPerDays; }
	void				SetFirstDay(int iFirstDay) { m_iFirstDay = iFirstDay; }
	int					GetFirstDay() { return m_iFirstDay; }
	void				SetTotalBytes(long long lTotalBytes) { m_lTotalBytes = lTotalBytes; }
	long long			GetTotalBytes() { return m_lTotalBytes; }
	int					GetSecSlot() { return m_iSecSlot; }
	int					GetMinSlot() { return m_iMinSlot; }
	int					GetHourSlot() { return m_iHourSlot; }
	int					GetDaySlot() { return m_iDaySlot; }
	void				CalcSlots(time_t tLocCurTime);

	time_t				GetDataTime() { return m_tDataTime; }
	void				SetDataTime(time_t tDataTime) { m_tDataTime = tDataTime; }

	void				AddData(int iBytes);
	void				LogDebugInfo();
};

typedef std::vector<ServerVolume*>	ServerVolumes;

class StatMeter : public Debuggable
{
private:
	// speed meter
	static const int	SPEEDMETER_SLOTS = 30;	  
	static const int	SPEEDMETER_SLOTSIZE = 1;  //Split elapsed time into this number of secs.
	int					m_iSpeedBytes[SPEEDMETER_SLOTS];
	int					m_iSpeedTotalBytes;
	int					m_iSpeedTime[SPEEDMETER_SLOTS];
	int					m_iSpeedStartTime; 
	time_t				m_tSpeedCorrection;
	int					m_iSpeedBytesIndex;
#ifdef HAVE_SPINLOCK
	SpinLock			m_spinlockSpeed;
#else
	Mutex				m_mutexSpeed;
#endif

	// time
	long long			m_iAllBytes;
	time_t				m_tStartServer;
	time_t				m_tLastCheck;
	time_t				m_tStartDownload;
	time_t				m_tPausedFrom;
	bool				m_bStandBy;
	Mutex				m_mutexStat;

	// data volume
	bool				m_bStatChanged;
	ServerVolumes		m_ServerVolumes;
	Mutex				m_mutexVolume;

	void				ResetSpeedStat();
	void				AdjustTimeOffset();

protected:
	virtual void		LogDebugInfo();

public:
						StatMeter();
						~StatMeter();
	void				InitVolumes();
	int					CalcCurrentDownloadSpeed();
	void				AddSpeedReading(int iBytes);
	void				AddServerData(int iBytes, int iServerID);
	void				CalcTotalStat(int* iUpTimeSec, int* iDnTimeSec, long long* iAllBytes, bool* bStandBy);
	bool				GetStandBy() { return m_bStandBy; }
	void				IntervalCheck();
	void				EnterLeaveStandBy(bool bEnter);
	ServerVolumes*		LockServerVolumes();
	void				UnlockServerVolumes();
	void				Save();
	void				Load();
};

#endif
