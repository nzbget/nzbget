/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2014-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include "Log.h"
#include "Thread.h"

class ServerVolume
{
public:
	typedef std::vector<long long>	VolumeArray;

private:
	VolumeArray			m_bytesPerSeconds;
	VolumeArray			m_bytesPerMinutes;
	VolumeArray			m_bytesPerHours;
	VolumeArray			m_bytesPerDays;
	int					m_firstDay;
	long long			m_totalBytes;
	long long			m_customBytes;
	time_t				m_dataTime;
	time_t				m_customTime;
	int					m_secSlot;
	int					m_minSlot;
	int					m_hourSlot;
	int					m_daySlot;

public:
						ServerVolume();
	VolumeArray*		BytesPerSeconds() { return &m_bytesPerSeconds; }
	VolumeArray*		BytesPerMinutes() { return &m_bytesPerMinutes; }
	VolumeArray*		BytesPerHours() { return &m_bytesPerHours; }
	VolumeArray*		BytesPerDays() { return &m_bytesPerDays; }
	void				SetFirstDay(int firstDay) { m_firstDay = firstDay; }
	int					GetFirstDay() { return m_firstDay; }
	void				SetTotalBytes(long long totalBytes) { m_totalBytes = totalBytes; }
	long long			GetTotalBytes() { return m_totalBytes; }
	void				SetCustomBytes(long long customBytes) { m_customBytes = customBytes; }
	long long			GetCustomBytes() { return m_customBytes; }
	int					GetSecSlot() { return m_secSlot; }
	int					GetMinSlot() { return m_minSlot; }
	int					GetHourSlot() { return m_hourSlot; }
	int					GetDaySlot() { return m_daySlot; }
	time_t				GetDataTime() { return m_dataTime; }
	void				SetDataTime(time_t dataTime) { m_dataTime = dataTime; }
	time_t				GetCustomTime() { return m_customTime; }
	void				SetCustomTime(time_t customTime) { m_customTime = customTime; }

	void				AddData(int bytes);
	void				CalcSlots(time_t locCurTime);
	void				ResetCustom();
	void				LogDebugInfo();
};

typedef std::vector<ServerVolume*>	ServerVolumes;

class StatMeter : public Debuggable
{
private:
	// speed meter
	static const int	SPEEDMETER_SLOTS = 30;
	static const int	SPEEDMETER_SLOTSIZE = 1;  //Split elapsed time into this number of secs.
	int					m_speedBytes[SPEEDMETER_SLOTS];
	long long			m_speedTotalBytes;
	int					m_speedTime[SPEEDMETER_SLOTS];
	int					m_speedStartTime;
	time_t				m_speedCorrection;
	int					m_speedBytesIndex;
	int					m_curSecBytes;
	time_t				m_curSecTime;
	Mutex				m_speedMutex;

	// time
	long long			m_allBytes;
	time_t				m_startServer;
	time_t				m_lastCheck;
	time_t				m_lastTimeOffset;
	time_t				m_startDownload;
	time_t				m_pausedFrom;
	bool				m_standBy;
	Mutex				m_statMutex;

	// data volume
	bool				m_statChanged;
	ServerVolumes		m_serverVolumes;
	Mutex				m_volumeMutex;

	void				ResetSpeedStat();
	void				AdjustTimeOffset();

protected:
	virtual void		LogDebugInfo();

public:
						StatMeter();
						~StatMeter();
	void				Init();
	int					CalcCurrentDownloadSpeed();
	int					CalcMomentaryDownloadSpeed();
	void				AddSpeedReading(int bytes);
	void				AddServerData(int bytes, int serverId);
	void				CalcTotalStat(int* upTimeSec, int* dnTimeSec, long long* allBytes, bool* standBy);
	bool				GetStandBy() { return m_standBy; }
	void				IntervalCheck();
	void				EnterLeaveStandBy(bool enter);
	ServerVolumes*		LockServerVolumes();
	void				UnlockServerVolumes();
	void				Save();
	bool				Load(bool* perfectServerMatch);
};

extern StatMeter* g_StatMeter;

#endif
