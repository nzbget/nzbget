/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2014-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef STATMETER_H
#define STATMETER_H

#include "Log.h"
#include "Thread.h"
#include "Util.h"

class ServerVolume
{
public:
	typedef std::vector<int64> VolumeArray;

	VolumeArray* BytesPerSeconds() { return &m_bytesPerSeconds; }
	VolumeArray* BytesPerMinutes() { return &m_bytesPerMinutes; }
	VolumeArray* BytesPerHours() { return &m_bytesPerHours; }
	VolumeArray* BytesPerDays() { return &m_bytesPerDays; }
	void SetFirstDay(int firstDay) { m_firstDay = firstDay; }
	int GetFirstDay() { return m_firstDay; }
	void SetTotalBytes(int64 totalBytes) { m_totalBytes = totalBytes; }
	int64 GetTotalBytes() { return m_totalBytes; }
	void SetCustomBytes(int64 customBytes) { m_customBytes = customBytes; }
	int64 GetCustomBytes() { return m_customBytes; }
	int GetSecSlot() { return m_secSlot; }
	int GetMinSlot() { return m_minSlot; }
	int GetHourSlot() { return m_hourSlot; }
	int GetDaySlot() { return m_daySlot; }
	time_t GetDataTime() { return m_dataTime; }
	void SetDataTime(time_t dataTime) { m_dataTime = dataTime; }
	time_t GetCustomTime() { return m_customTime; }
	void SetCustomTime(time_t customTime) { m_customTime = customTime; }

	void AddData(int bytes);
	void CalcSlots(time_t locCurTime);
	void ResetCustom();
	void LogDebugInfo();

private:
	VolumeArray m_bytesPerSeconds = VolumeArray(60);
	VolumeArray m_bytesPerMinutes = VolumeArray(60);
	VolumeArray m_bytesPerHours = VolumeArray(24);
	VolumeArray m_bytesPerDays;
	int m_firstDay = 0;
	int64 m_totalBytes = 0;
	int64 m_customBytes = 0;
	time_t m_dataTime = 0;
	time_t m_customTime = Util::CurrentTime();
	int m_secSlot = 0;
	int m_minSlot = 0;
	int m_hourSlot = 0;
	int m_daySlot = 0;
};

typedef std::vector<ServerVolume> ServerVolumes;
typedef GuardedPtr<ServerVolumes> GuardedServerVolumes;

class StatMeter : public Debuggable
{
public:
	StatMeter();
	void Init();
	int CalcCurrentDownloadSpeed();
	int CalcMomentaryDownloadSpeed();
	void AddSpeedReading(int bytes);
	void AddServerData(int bytes, int serverId);
	void CalcTotalStat(int* upTimeSec, int* dnTimeSec, int64* allBytes, bool* standBy);
	void CalcQuotaUsage(int64& monthBytes, int64& dayBytes);
	void IntervalCheck();
	void EnterLeaveStandBy(bool enter);
	GuardedServerVolumes GuardServerVolumes();
	void Save();
	bool Load(bool* perfectServerMatch);

protected:
	virtual void LogDebugInfo();

private:
	// speed meter
	static const int SPEEDMETER_SLOTS = 30;
	static const int SPEEDMETER_SLOTSIZE = 1; //Split elapsed time into this number of secs.
	int m_speedBytes[SPEEDMETER_SLOTS];
	int64 m_speedTotalBytes;
	int m_speedTime[SPEEDMETER_SLOTS];
	int m_speedStartTime;
	time_t m_speedCorrection;
	int m_speedBytesIndex;
	int m_curSecBytes;
	time_t m_curSecTime;

	// time
	int64 m_allBytes = 0;
	time_t m_startServer = 0;
	time_t m_lastCheck = 0;
	time_t m_lastTimeOffset = 0;
	time_t m_startDownload = 0;
	time_t m_pausedFrom = 0;
	bool m_standBy = true;
	Mutex m_statMutex;

	// data volume
	bool m_statChanged = false;
	ServerVolumes m_serverVolumes;
	Mutex m_volumeMutex;

	void ResetSpeedStat();
	void AdjustTimeOffset();
	void CheckQuota();
	int CalcMonthSlots(ServerVolume& volume);
};

extern StatMeter* g_StatMeter;

#endif
