/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2014-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "StatMeter.h"
#include "Options.h"
#include "WorkState.h"
#include "ServerPool.h"
#include "DiskState.h"
#include "Util.h"

static const int DAYS_UP_TO_2013_JAN_1 = 15706;
static const int DAYS_IN_TWENTY_YEARS = 366*20;

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
	time_t locCurTime = curTime + g_WorkState->GetLocalTimeOffset();
	time_t locDataTime = m_dataTime + g_WorkState->GetLocalTimeOffset();

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
		msg.AppendFmt("[%i]=%" PRIi64 " ", i, m_bytesPerSeconds[i]);
	}
	info("Secs: %s", *msg);

	msg.Clear();
	for (int i = 0; i < 60; i++)
	{
		msg.AppendFmt("[%i]=%" PRIi64 " ", i, m_bytesPerMinutes[i]);
	}
	info("Mins: %s", *msg);

	msg.Clear();
	for (int i = 0; i < 24; i++)
	{
		msg.AppendFmt("[%i]=%" PRIi64 " ", i, m_bytesPerHours[i]);
	}
	info("Hours: %s", *msg);

	msg.Clear();
	for (int i = 0; i < (int)m_bytesPerDays.size(); i++)
	{
		msg.AppendFmt("[%i]=%" PRIi64 " ", m_firstDay + i, m_bytesPerDays[i]);
	}
	info("Days: %s", *msg);
}

StatMeter::StatMeter()
{
	debug("Creating StatMeter");

	ResetSpeedStat();
}

void StatMeter::Init()
{
	m_startServer = Util::CurrentTime();
	m_lastCheck = m_startServer;
	AdjustTimeOffset();

	m_serverVolumes.resize(1 + g_ServerPool->GetServers()->size());
}

void StatMeter::AdjustTimeOffset()
{
	time_t utcTime = Util::CurrentTime();
	tm tmSplittedTime;
	gmtime_r(&utcTime, &tmSplittedTime);
	tmSplittedTime.tm_isdst = -1;
	time_t locTime = mktime(&tmSplittedTime);
	time_t localTimeDelta = utcTime - locTime;
	g_WorkState->SetLocalTimeOffset((int)localTimeDelta + g_Options->GetTimeCorrection());
	m_lastTimeOffset = utcTime;

	debug("UTC delta: %i (%i+%i)", g_WorkState->GetLocalTimeOffset(), (int)localTimeDelta, g_Options->GetTimeCorrection());
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

	CheckQuota();

	if (m_statChanged)
	{
		Save();
	}
}

void StatMeter::EnterLeaveStandBy(bool enter)
{
	Guard guard(m_statMutex);
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
}

void StatMeter::CalcTotalStat(int* upTimeSec, int* dnTimeSec, int64* allBytes, bool* standBy)
{
	Guard guard(m_statMutex);
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
	info("      SpeedTotalBytes: %" PRIi64, m_speedTotalBytes);
	info("      SpeedBytesIndex: %i", m_speedBytesIndex);
	info("      AllBytes: %" PRIi64, m_allBytes);
	info("      Time: %i", (int)Util::CurrentTime());
	info("      TimeDiff: %i", timeDiff);
	for (int i=0; i < SPEEDMETER_SLOTS; i++)
	{
		info("      Bytes[%i]: %i, Time[%i]: %i", i, m_speedBytes[i], i, m_speedTime[i]);
	}

	Guard guard(m_volumeMutex);
	int index = 0;
	for (ServerVolume& serverVolume : m_serverVolumes)
	{
		info("      ServerVolume %i", index);
		serverVolume.LogDebugInfo();
		index++;
	}
}

void StatMeter::AddServerData(int bytes, int serverId)
{
	if (bytes == 0)
	{
		return;
	}

	Guard guard(m_volumeMutex);
	m_serverVolumes[0].AddData(bytes);
	m_serverVolumes[serverId].AddData(bytes);
	m_statChanged = true;
}

GuardedServerVolumes StatMeter::GuardServerVolumes()
{
	GuardedServerVolumes serverVolumes(&m_serverVolumes, &m_volumeMutex);

	// update slots
	for (ServerVolume& serverVolume : m_serverVolumes)
	{
		serverVolume.AddData(0);
	}

	return serverVolumes;
}

void StatMeter::Save()
{
	if (!g_Options->GetServerMode())
	{
		return;
	}

	Guard guard(m_volumeMutex);
	g_DiskState->SaveStats(g_ServerPool->GetServers(), &m_serverVolumes);
	m_statChanged = false;
}

bool StatMeter::Load(bool* perfectServerMatch)
{
	Guard guard(m_volumeMutex);

	bool ok = g_DiskState->LoadStats(g_ServerPool->GetServers(), &m_serverVolumes, perfectServerMatch);

	for (ServerVolume& serverVolume : m_serverVolumes)
	{
		serverVolume.CalcSlots(serverVolume.GetDataTime() + g_WorkState->GetLocalTimeOffset());
	}

	return ok;
}

void StatMeter::CheckQuota()
{
	if ((g_Options->GetDailyQuota() == 0 && g_Options->GetMonthlyQuota() == 0))
	{
		return;
	}

	int64 monthBytes, dayBytes;
	CalcQuotaUsage(monthBytes, dayBytes);

	bool monthlyQuotaReached = g_Options->GetMonthlyQuota() > 0 && monthBytes >= (int64)g_Options->GetMonthlyQuota() * 1024 * 1024;
	bool dailyQuotaReached = g_Options->GetDailyQuota() > 0 && dayBytes >= (int64)g_Options->GetDailyQuota() * 1024 * 1024;

	if (monthlyQuotaReached && !g_WorkState->GetQuotaReached())
	{
		warn("Monthly quota reached at %s", *Util::FormatSize(monthBytes));
	}
	else if (dailyQuotaReached && !g_WorkState->GetQuotaReached())
	{
		warn("Daily quota reached at %s", *Util::FormatSize(dayBytes));
	}
	else if (!monthlyQuotaReached && !dailyQuotaReached && g_WorkState->GetQuotaReached())
	{
		info("Quota lifted");
	}

	g_WorkState->SetQuotaReached(monthlyQuotaReached || dailyQuotaReached);
}

void StatMeter::CalcQuotaUsage(int64& monthBytes, int64& dayBytes)
{
	Guard guard(m_volumeMutex);

	ServerVolume totalVolume = m_serverVolumes[0];

	time_t locTime = Util::CurrentTime() + g_WorkState->GetLocalTimeOffset();
	int daySlot = (int)(locTime / 86400) - totalVolume.GetFirstDay();

	dayBytes = 0;
	if (daySlot < (int)totalVolume.BytesPerDays()->size())
	{
		dayBytes = totalVolume.BytesPerDays()->at(daySlot);
	}

	int elapsedSlots = CalcMonthSlots(totalVolume);
	monthBytes = 0;
	int endSlot = std::max(daySlot - elapsedSlots, -1);
	for (int slot = daySlot; slot >= 0 && slot > endSlot; slot--)
	{
		if (slot < (int)totalVolume.BytesPerDays()->size())
		{
			monthBytes += totalVolume.BytesPerDays()->at(slot);
			debug("adding slot %i: %i", slot, (int)(totalVolume.BytesPerDays()->at(slot) / 1024 / 1024));
		}
	}

	debug("month volume: %i MB", (int)(monthBytes / 1024 / 1024));
}

int StatMeter::CalcMonthSlots(ServerVolume& volume)
{
	int elapsedDays;

	time_t locCurTime = Util::CurrentTime() + g_WorkState->GetLocalTimeOffset();
	tm dayparts;
	gmtime_r(&locCurTime, &dayparts);

	if (g_Options->GetQuotaStartDay() > dayparts.tm_mday)
	{
		dayparts.tm_mon--;
		dayparts.tm_mday = g_Options->GetQuotaStartDay();
		time_t prevMonth = Util::Timegm(&dayparts);
		tm prevparts;
		gmtime_r(&prevMonth, &prevparts);
		if (prevparts.tm_mday != g_Options->GetQuotaStartDay())
		{
			dayparts.tm_mday = 1;
			dayparts.tm_mon++;
			prevMonth = Util::Timegm(&dayparts);
		}
		elapsedDays = (int)(locCurTime - prevMonth) / 60 / 60 / 24 + 1;
	}
	else
	{
		elapsedDays = dayparts.tm_mday - g_Options->GetQuotaStartDay() + 1;
	}

	return elapsedDays;
}
