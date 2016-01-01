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


#include "nzbget.h"
#include "StatMeter.h"
#include "Options.h"
#include "ServerPool.h"
#include "DiskState.h"
#include "Util.h"

static const int DAYS_UP_TO_2013_JAN_1 = 15706;
static const int DAYS_IN_TWENTY_YEARS = 366*20;

ServerVolume::ServerVolume()
{
	m_bytesPerSeconds.resize(60);
	m_bytesPerMinutes.resize(60);
	m_bytesPerHours.resize(24);
	m_bytesPerDays.resize(0);
	m_firstDay = 0;
	m_dataTime = 0;
	m_totalBytes = 0;
	m_customBytes = 0;
	m_customTime = Util::CurrentTime();
	m_secSlot = 0;
	m_minSlot = 0;
	m_hourSlot = 0;
	m_daySlot = 0;
}

void ServerVolume::CalcSlots(time_t locCurTime)
{
	m_secSlot = (int)locCurTime % 60;
	m_minSlot = ((int)locCurTime / 60) % 60;
	m_hourSlot = ((int)locCurTime % 86400) / 3600;
	int daysSince1970 = (int)locCurTime / 86400;
	m_daySlot = daysSince1970 - DAYS_UP_TO_2013_JAN_1 + 1;
	if (0 <= m_daySlot && m_daySlot < DAYS_IN_TWENTY_YEARS)
	{
		int curDay = daysSince1970;
		if (m_firstDay == 0 || m_firstDay > curDay)
		{
			m_firstDay = curDay;
		}
		m_daySlot = curDay - m_firstDay;
		if (m_daySlot + 1 > (int)m_bytesPerDays.size())
		{
			m_bytesPerDays.resize(m_daySlot + 1);
		}
	}
	else
	{
		m_daySlot = -1;
	}
}

void ServerVolume::AddData(int bytes)
{
	time_t curTime = Util::CurrentTime();
	time_t locCurTime = curTime + g_Options->GetLocalTimeOffset();
	time_t locDataTime = m_dataTime + g_Options->GetLocalTimeOffset();

	int lastMinSlot = m_minSlot;
	int lastHourSlot = m_hourSlot;

	CalcSlots(locCurTime);

	if (locCurTime != locDataTime)
	{
		// clear seconds/minutes/hours slots if necessary
		// also handle the backwards changes of system clock

		int totalDelta = (int)(locCurTime - locDataTime);
		int deltaSign = totalDelta >= 0 ? 1 : -1;
		totalDelta = abs(totalDelta);

		int secDelta = totalDelta;
		if (deltaSign < 0) secDelta++;
		if (secDelta >= 60) secDelta = 60;
		for (int i = 0; i < secDelta; i++)
		{
			int nulSlot = m_secSlot - i * deltaSign;
			if (nulSlot < 0) nulSlot += 60;
			if (nulSlot >= 60) nulSlot -= 60;
			m_bytesPerSeconds[nulSlot] = 0;
		}

		int minDelta = totalDelta / 60;
		if (deltaSign < 0) minDelta++;
		if (abs(minDelta) >= 60) minDelta = 60;
		if (minDelta == 0 && m_minSlot != lastMinSlot) minDelta = 1;
		for (int i = 0; i < minDelta; i++)
		{
			int nulSlot = m_minSlot - i * deltaSign;
			if (nulSlot < 0) nulSlot += 60;
			if (nulSlot >= 60) nulSlot -= 60;
			m_bytesPerMinutes[nulSlot] = 0;
		}

		int hourDelta = totalDelta / (60 * 60);
		if (deltaSign < 0) hourDelta++;
		if (hourDelta >= 24) hourDelta = 24;
		if (hourDelta == 0 && m_hourSlot != lastHourSlot) hourDelta = 1;
		for (int i = 0; i < hourDelta; i++)
		{
			int nulSlot = m_hourSlot - i * deltaSign;
			if (nulSlot < 0) nulSlot += 24;
			if (nulSlot >= 24) nulSlot -= 24;
			m_bytesPerHours[nulSlot] = 0;
		}
	}

	// add bytes to every slot
	m_bytesPerSeconds[m_secSlot] += bytes;
	m_bytesPerMinutes[m_minSlot] += bytes;
	m_bytesPerHours[m_hourSlot] += bytes;
	if (m_daySlot >= 0)
	{
		m_bytesPerDays[m_daySlot] += bytes;
	}
	m_totalBytes += bytes;
	m_customBytes += bytes;

	m_dataTime = curTime;
}

void ServerVolume::ResetCustom()
{
	m_customBytes = 0;
	m_customTime = Util::CurrentTime();
}

void ServerVolume::LogDebugInfo()
{
	info("   ---------- ServerVolume");

	StringBuilder msg;

	for (int i = 0; i < 60; i++)
	{
		msg.AppendFmt("[%i]=%lli ", i, m_bytesPerSeconds[i]);
	}
	info("Secs: %s", *msg);

	msg.Clear();
	for (int i = 0; i < 60; i++)
	{
		msg.AppendFmt("[%i]=%lli ", i, m_bytesPerMinutes[i]);
	}
	info("Mins: %s", *msg);

	msg.Clear();
	for (int i = 0; i < 24; i++)
	{
		msg.AppendFmt("[%i]=%lli ", i, m_bytesPerHours[i]);
	}
	info("Hours: %s", *msg);

	msg.Clear();
	for (int i = 0; i < (int)m_bytesPerDays.size(); i++)
	{
		msg.AppendFmt("[%i]=%lli ", m_firstDay + i, m_bytesPerDays[i]);
	}
	info("Days: %s", *msg);
}

StatMeter::StatMeter()
{
	debug("Creating StatMeter");

	ResetSpeedStat();

	m_allBytes = 0;
	m_startDownload = 0;
	m_pausedFrom = 0;
	m_standBy = true;
	m_startServer = 0;
	m_lastCheck = 0;
	m_lastTimeOffset = 0;
	m_statChanged = false;

	g_Log->RegisterDebuggable(this);
}

StatMeter::~StatMeter()
{
	debug("Destroying StatMeter");
	// Cleanup

	g_Log->UnregisterDebuggable(this);

	for (ServerVolumes::iterator it = m_serverVolumes.begin(); it != m_serverVolumes.end(); it++)
	{
		delete *it;
	}

	debug("StatMeter destroyed");
}

void StatMeter::Init()
{
	m_startServer = Util::CurrentTime();
	m_lastCheck = m_startServer;
	AdjustTimeOffset();

	m_serverVolumes.resize(1 + g_ServerPool->GetServers()->size());
	m_serverVolumes[0] = new ServerVolume();
	for (Servers::iterator it = g_ServerPool->GetServers()->begin(); it != g_ServerPool->GetServers()->end(); it++)
	{
		NewsServer* server = *it;
		m_serverVolumes[server->GetId()] = new ServerVolume();
	}
}

void StatMeter::AdjustTimeOffset()
{
	time_t utcTime = Util::CurrentTime();
	tm tmSplittedTime;
	gmtime_r(&utcTime, &tmSplittedTime);
	tmSplittedTime.tm_isdst = -1;
	time_t locTime = mktime(&tmSplittedTime);
	time_t localTimeDelta = utcTime - locTime;
	g_Options->SetLocalTimeOffset((int)localTimeDelta + g_Options->GetTimeCorrection());
	m_lastTimeOffset = utcTime;

	debug("UTC delta: %i (%i+%i)", g_Options->GetLocalTimeOffset(), (int)localTimeDelta, g_Options->GetTimeCorrection());
}

/*
 * Called once per second.
 *  - detect large step changes of system time and adjust statistics;
 *  - save volume stats (if changed).
 */
void StatMeter::IntervalCheck()
{
	time_t m_curTime = Util::CurrentTime();
	time_t diff = m_curTime - m_lastCheck;
	if (diff > 60 || diff < 0)
	{
		m_startServer += diff + 1; // "1" because the method is called once per second
		if (m_startDownload != 0 && !m_standBy)
		{
			m_startDownload += diff + 1;
		}
		AdjustTimeOffset();
	}
	else if (m_lastTimeOffset > m_curTime ||
		m_curTime - m_lastTimeOffset > 60 * 60 * 3 ||
		(m_curTime - m_lastTimeOffset > 60 && !m_standBy))
	{
		// checking time zone settings may prevent the device from entering sleep/hibernate mode
		// check every minute if not in standby
		// check at least every 3 hours even in standby
		AdjustTimeOffset();
	}

	m_lastCheck = m_curTime;

	if (m_statChanged)
	{
		Save();
	}
}

void StatMeter::EnterLeaveStandBy(bool enter)
{
	m_statMutex.Lock();
	m_standBy = enter;
	if (enter)
	{
		m_pausedFrom = Util::CurrentTime();
	}
	else
	{
		if (m_startDownload == 0)
		{
			m_startDownload = Util::CurrentTime();
		}
		else
		{
			m_startDownload += Util::CurrentTime() - m_pausedFrom;
		}
		m_pausedFrom = 0;
		ResetSpeedStat();
	}
	m_statMutex.Unlock();
}

void StatMeter::CalcTotalStat(int* upTimeSec, int* dnTimeSec, int64* allBytes, bool* standBy)
{
	m_statMutex.Lock();
	if (m_startServer > 0)
	{
		*upTimeSec = (int)(Util::CurrentTime() - m_startServer);
	}
	else
	{
		*upTimeSec = 0;
	}
	*standBy = m_standBy;
	if (m_standBy)
	{
		*dnTimeSec = (int)(m_pausedFrom - m_startDownload);
	}
	else
	{
		*dnTimeSec = (int)(Util::CurrentTime() - m_startDownload);
	}
	*allBytes = m_allBytes;
	m_statMutex.Unlock();
}

// Average speed in last 30 seconds
int StatMeter::CalcCurrentDownloadSpeed()
{
	if (m_standBy)
	{
		return 0;
	}

	int timeDiff = (int)Util::CurrentTime() - m_speedStartTime * SPEEDMETER_SLOTSIZE;
	if (timeDiff == 0)
	{
		return 0;
	}

	return (int)(m_speedTotalBytes / timeDiff);
}

// Amount of data downloaded in current second
int StatMeter::CalcMomentaryDownloadSpeed()
{
	time_t curTime = Util::CurrentTime();
	int speed = curTime == m_curSecTime ? m_curSecBytes : 0;
	return speed;
}

void StatMeter::AddSpeedReading(int bytes)
{
	time_t curTime = Util::CurrentTime();
	int nowSlot = (int)curTime / SPEEDMETER_SLOTSIZE;

	if (g_Options->GetAccurateRate())
	{
		m_speedMutex.Lock();
	}

	if (curTime != m_curSecTime)
	{
		m_curSecTime =	curTime;
		m_curSecBytes = 0;
	}
	m_curSecBytes += bytes;

	while (nowSlot > m_speedTime[m_speedBytesIndex])
	{
		//record bytes in next slot
		m_speedBytesIndex++;
		if (m_speedBytesIndex >= SPEEDMETER_SLOTS)
		{
			m_speedBytesIndex = 0;
		}
		//Adjust counters with outgoing information.
		m_speedTotalBytes = m_speedTotalBytes - (int64)m_speedBytes[m_speedBytesIndex];

		//Note we should really use the start time of the next slot
		//but its easier to just use the outgoing slot time. This
		//will result in a small error.
		m_speedStartTime = m_speedTime[m_speedBytesIndex];

		//Now reset.
		m_speedBytes[m_speedBytesIndex] = 0;
		m_speedTime[m_speedBytesIndex] = nowSlot;
	}

	// Once per second recalculate summary field "m_iSpeedTotalBytes" to recover from possible synchronisation errors
	if (curTime > m_speedCorrection)
	{
		int64 speedTotalBytes = 0;
		for (int i = 0; i < SPEEDMETER_SLOTS; i++)
		{
			speedTotalBytes += m_speedBytes[i];
		}
		m_speedTotalBytes = speedTotalBytes;
		m_speedCorrection = curTime;
	}

	if (m_speedTotalBytes == 0)
	{
		m_speedStartTime = nowSlot;
	}
	m_speedBytes[m_speedBytesIndex] += bytes;
	m_speedTotalBytes += bytes;
	m_allBytes += bytes;

	if (g_Options->GetAccurateRate())
	{
		m_speedMutex.Unlock();
	}
}

void StatMeter::ResetSpeedStat()
{
	time_t curTime = Util::CurrentTime();
	m_speedStartTime = (int)curTime / SPEEDMETER_SLOTSIZE;
	for (int i = 0; i < SPEEDMETER_SLOTS; i++)
	{
		m_speedBytes[i] = 0;
		m_speedTime[i] = m_speedStartTime;
	}
	m_speedBytesIndex = 0;
	m_speedTotalBytes = 0;
	m_speedCorrection = curTime;
	m_curSecTime =	0;
	m_curSecBytes = 0;
}

void StatMeter::LogDebugInfo()
{
	info("   ---------- SpeedMeter");
	int speed = CalcCurrentDownloadSpeed() / 1024;
	int timeDiff = (int)Util::CurrentTime() - m_speedStartTime * SPEEDMETER_SLOTSIZE;
	info("      Speed: %i", speed);
	info("      SpeedStartTime: %i", m_speedStartTime);
	info("      SpeedTotalBytes: %lli", m_speedTotalBytes);
	info("      SpeedBytesIndex: %i", m_speedBytesIndex);
	info("      AllBytes: %lli", m_allBytes);
	info("      Time: %i", (int)Util::CurrentTime());
	info("      TimeDiff: %i", timeDiff);
	for (int i=0; i < SPEEDMETER_SLOTS; i++)
	{
		info("      Bytes[%i]: %i, Time[%i]: %i", i, m_speedBytes[i], i, m_speedTime[i]);
	}

	m_volumeMutex.Lock();
	int index = 0;
	for (ServerVolumes::iterator it = m_serverVolumes.begin(); it != m_serverVolumes.end(); it++, index++)
	{
		ServerVolume* serverVolume = *it;
		info("      ServerVolume %i", index);
		serverVolume->LogDebugInfo();
	}
	m_volumeMutex.Unlock();
}

void StatMeter::AddServerData(int bytes, int serverId)
{
	if (bytes == 0)
	{
		return;
	}

	m_volumeMutex.Lock();
	m_serverVolumes[0]->AddData(bytes);
	m_serverVolumes[serverId]->AddData(bytes);
	m_statChanged = true;
	m_volumeMutex.Unlock();
}

ServerVolumes* StatMeter::LockServerVolumes()
{
	m_volumeMutex.Lock();

	// update slots
	for (ServerVolumes::iterator it = m_serverVolumes.begin(); it != m_serverVolumes.end(); it++)
	{
		ServerVolume* serverVolume = *it;
		serverVolume->AddData(0);
	}

	return &m_serverVolumes;
}

void StatMeter::UnlockServerVolumes()
{
	m_volumeMutex.Unlock();
}

void StatMeter::Save()
{
	if (!g_Options->GetServerMode())
	{
		return;
	}

	m_volumeMutex.Lock();
	g_DiskState->SaveStats(g_ServerPool->GetServers(), &m_serverVolumes);
	m_statChanged = false;
	m_volumeMutex.Unlock();
}

bool StatMeter::Load(bool* perfectServerMatch)
{
	m_volumeMutex.Lock();

	bool ok = g_DiskState->LoadStats(g_ServerPool->GetServers(), &m_serverVolumes, perfectServerMatch);

	for (ServerVolumes::iterator it = m_serverVolumes.begin(); it != m_serverVolumes.end(); it++)
	{
		ServerVolume* serverVolume = *it;
		serverVolume->CalcSlots(serverVolume->GetDataTime() + g_Options->GetLocalTimeOffset());
	}

	m_volumeMutex.Unlock();

	return ok;
}
