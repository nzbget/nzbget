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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "nzbget.h"
#include "StatMeter.h"
#include "Options.h"
#include "ServerPool.h"
#include "DiskState.h"

extern ServerPool* g_pServerPool;
extern Options* g_pOptions;
extern DiskState* g_pDiskState;

static const int DAYS_UP_TO_2013_JAN_1 = 15706;
static const int DAYS_IN_TWENTY_YEARS = 366*20;

ServerVolume::ServerVolume()
{
	m_BytesPerSeconds.resize(60);
	m_BytesPerMinutes.resize(60);
	m_BytesPerHours.resize(24);
	m_BytesPerDays.resize(0);
	m_iFirstDay = 0;
	m_tDataTime = 0;
	m_lTotalBytes = 0;
	m_iSecSlot = 0;
	m_iMinSlot = 0;
	m_iHourSlot = 0;
	m_iDaySlot = 0;
}

void ServerVolume::CalcSlots(time_t tLocCurTime)
{
	m_iSecSlot = (int)tLocCurTime % 60;
	m_iMinSlot = ((int)tLocCurTime / 60) % 60;
	m_iHourSlot = ((int)tLocCurTime % 86400) / 3600;
	int iDaysSince1970 = (int)tLocCurTime / 86400;
	m_iDaySlot = iDaysSince1970 - DAYS_UP_TO_2013_JAN_1 + 1;
	if (0 <= m_iDaySlot && m_iDaySlot < DAYS_IN_TWENTY_YEARS)
	{
		int iCurDay = iDaysSince1970;
		if (m_iFirstDay == 0 || m_iFirstDay > iCurDay)
		{
			m_iFirstDay = iCurDay;
		}
		m_iDaySlot = iCurDay - m_iFirstDay;
		if (m_iDaySlot + 1 > (int)m_BytesPerDays.size())
		{
			m_BytesPerDays.resize(m_iDaySlot + 1);
		}
	}
	else
	{
		m_iDaySlot = -1;
	}
}

void ServerVolume::AddData(int iBytes)
{
	time_t tCurTime = time(NULL);
	time_t tLocCurTime = tCurTime + g_pOptions->GetLocalTimeOffset();
	time_t tLocDataTime = m_tDataTime + g_pOptions->GetLocalTimeOffset();

	int iLastSecSlot = m_iSecSlot;
	int iLastMinSlot = m_iMinSlot;
	int iLastHourSlot = m_iHourSlot;

	CalcSlots(tLocCurTime);

	if (tLocCurTime != tLocDataTime)
	{
		// clear seconds/minutes/hours slots if necessary

		int iTotalDelta = (int)(tLocCurTime - tLocDataTime);

		int iSecDelta = iTotalDelta;
		if (iSecDelta >= 60 || iTotalDelta < 0) iSecDelta = 60;
		for (int i=0; i<iSecDelta; i++)
		{
			int iNulSlot = m_iSecSlot - i;
			if (iNulSlot < 0) iNulSlot += 60;
			m_BytesPerSeconds[iNulSlot] = 0;
		}

		int iMinDelta = iTotalDelta / 60;
		if (iMinDelta >= 60 || iTotalDelta < 0) iMinDelta = 60;
		if (iMinDelta == 0 && m_iMinSlot != iLastMinSlot) iMinDelta = 1;
		for (int i=0; i<iMinDelta; i++)
		{
			int iNulSlot = m_iMinSlot - i;
			if (iNulSlot < 0) iNulSlot += 60;
			m_BytesPerMinutes[iNulSlot] = 0;
		}

		int iHourDelta = iTotalDelta / (60 * 60);
		if (iHourDelta >= 24 || iTotalDelta < 0) iHourDelta = 24;
		if (iHourDelta == 0 && m_iHourSlot != iLastHourSlot) iHourDelta = 1;
		for (int i=0; i<iHourDelta; i++)
		{
			int iNulSlot = m_iHourSlot - i;
			if (iNulSlot < 0) iNulSlot += 24;
			m_BytesPerHours[iNulSlot] = 0;
		}
	}

	// add bytes to every slot
	m_BytesPerSeconds[m_iSecSlot] += iBytes;
	m_BytesPerMinutes[m_iMinSlot] += iBytes;
	m_BytesPerHours[m_iHourSlot] += iBytes;
	if (m_iDaySlot >= 0)
	{
		m_BytesPerDays[m_iDaySlot] += iBytes;
	}
	m_lTotalBytes += iBytes;

	m_tDataTime = tCurTime;
}

void ServerVolume::LogDebugInfo()
{
	info("   ---------- ServerVolume");

	char szSec[4000];

	szSec[0] = '\0';
	for (int i = 0; i < 60; i++)
	{
		char szNum[20];
		snprintf(szNum, 20, "[%i]=%lli ", i, m_BytesPerSeconds[i]);
		strncat(szSec, szNum, 4000);
	}
	info("Secs: %s", szSec);

	szSec[0] = '\0';
	for (int i = 0; i < 60; i++)
	{
		char szNum[20];
		snprintf(szNum, 20, "[%i]=%lli ", i, m_BytesPerMinutes[i]);
		strncat(szSec, szNum, 4000);
	}
	info("Mins: %s", szSec);

	szSec[0] = '\0';
	for (int i = 0; i < 24; i++)
	{
		char szNum[20];
		snprintf(szNum, 20, "[%i]=%lli ", i, m_BytesPerHours[i]);
		strncat(szSec, szNum, 4000);
	}
	info("Hours: %s", szSec);

	szSec[0] = '\0';
	for (int i = 0; i < (int)m_BytesPerDays.size(); i++)
	{
		char szNum[20];
		snprintf(szNum, 20, "[%i]=%lli ", m_iFirstDay + i, m_BytesPerDays[i]);
		strncat(szSec, szNum, 4000);
	}
	info("Days: %s", szSec);
}

StatMeter::StatMeter()
{
	debug("Creating StatMeter");

	ResetSpeedStat();

	m_iAllBytes = 0;
	m_tStartDownload = 0;
	m_tPausedFrom = 0;
	m_bStandBy = true;
	m_tStartServer = 0;
	m_tLastCheck = 0;
	m_bStatChanged = false;

	g_pLog->RegisterDebuggable(this);
}

StatMeter::~StatMeter()
{
	debug("Destroying StatMeter");
	// Cleanup

	g_pLog->UnregisterDebuggable(this);

	for (ServerVolumes::iterator it = m_ServerVolumes.begin(); it != m_ServerVolumes.end(); it++)
	{
		delete *it;
	}

	debug("StatMeter destroyed");
}

void StatMeter::AdjustTimeOffset()
{
	time_t tUtcTime = time(NULL);
	tm tmSplittedTime;
	gmtime_r(&tUtcTime, &tmSplittedTime);
	tmSplittedTime.tm_isdst = -1;
	time_t tLocTime = mktime(&tmSplittedTime);
	time_t tLocalTimeDelta = tUtcTime - tLocTime;
	g_pOptions->SetLocalTimeOffset((int)tLocalTimeDelta + g_pOptions->GetTimeCorrection());

	debug("UTC delta: %i (%i+%i)", g_pOptions->GetLocalTimeOffset(), (int)tLocalTimeDelta, g_pOptions->GetTimeCorrection());
}

/*
 * Called once per second.
 *  - detect large step changes of system time and adjust statistics;
 *  - save volume stats (if changed).
 */
void StatMeter::IntervalCheck()
{
	if (!m_tLastCheck)
	{
		m_tStartServer = time(NULL);
		m_tLastCheck = m_tStartServer;
		AdjustTimeOffset();
	}

	time_t m_tCurTime = time(NULL);
	time_t tDiff = m_tCurTime - m_tLastCheck;
	if (tDiff > 60 || tDiff < 0)
	{
		m_tStartServer += tDiff + 1; // "1" because the method is called once per second
		if (m_tStartDownload != 0 && !m_bStandBy)
		{
			m_tStartDownload += tDiff + 1;
		}
		AdjustTimeOffset();
	}		
	m_tLastCheck = m_tCurTime;

	if (m_bStatChanged)
	{
		Save();
	}
}

void StatMeter::EnterLeaveStandBy(bool bEnter)
{
	m_mutexStat.Lock();
	m_bStandBy = bEnter;
	if (bEnter)
	{
		m_tPausedFrom = time(NULL);
	}
	else
	{
		if (m_tStartDownload == 0)
		{
			m_tStartDownload = time(NULL);
		}
		else
		{
			m_tStartDownload += time(NULL) - m_tPausedFrom;
		}
		m_tPausedFrom = 0;
		ResetSpeedStat();
	}
	m_mutexStat.Unlock();
}

void StatMeter::CalcTotalStat(int* iUpTimeSec, int* iDnTimeSec, long long* iAllBytes, bool* bStandBy)
{
	m_mutexStat.Lock();
	if (m_tStartServer > 0)
	{
		*iUpTimeSec = (int)(time(NULL) - m_tStartServer);
	}
	else
	{
		*iUpTimeSec = 0;
	}
	*bStandBy = m_bStandBy;
	if (m_bStandBy)
	{
		*iDnTimeSec = (int)(m_tPausedFrom - m_tStartDownload);
	}
	else
	{
		*iDnTimeSec = (int)(time(NULL) - m_tStartDownload);
	}
	*iAllBytes = m_iAllBytes;
	m_mutexStat.Unlock();
}

/*
 * NOTE: see note to "AddSpeedReading"
 */
int StatMeter::CalcCurrentDownloadSpeed()
{
	if (m_bStandBy)
	{
		return 0;
	}

	int iTimeDiff = (int)time(NULL) - m_iSpeedStartTime * SPEEDMETER_SLOTSIZE;
	if (iTimeDiff == 0)
	{
		return 0;
	}

	return m_iSpeedTotalBytes / iTimeDiff;
}

void StatMeter::AddSpeedReading(int iBytes)
{
	time_t tCurTime = time(NULL);
	int iNowSlot = (int)tCurTime / SPEEDMETER_SLOTSIZE;

	if (g_pOptions->GetAccurateRate())
	{
#ifdef HAVE_SPINLOCK
		m_spinlockSpeed.Lock();
#else
		m_mutexSpeed.Lock();
#endif
	}

	while (iNowSlot > m_iSpeedTime[m_iSpeedBytesIndex])
	{
		//record bytes in next slot
		m_iSpeedBytesIndex++;
		if (m_iSpeedBytesIndex >= SPEEDMETER_SLOTS)
		{
			m_iSpeedBytesIndex = 0;
		}
		//Adjust counters with outgoing information.
		m_iSpeedTotalBytes -= m_iSpeedBytes[m_iSpeedBytesIndex];

		//Note we should really use the start time of the next slot
		//but its easier to just use the outgoing slot time. This
		//will result in a small error.
		m_iSpeedStartTime = m_iSpeedTime[m_iSpeedBytesIndex];

		//Now reset.
		m_iSpeedBytes[m_iSpeedBytesIndex] = 0;
		m_iSpeedTime[m_iSpeedBytesIndex] = iNowSlot;
	}

	// Once per second recalculate summary field "m_iSpeedTotalBytes" to recover from possible synchronisation errors
	if (tCurTime > m_tSpeedCorrection)
	{
		int iSpeedTotalBytes = 0;
		for (int i = 0; i < SPEEDMETER_SLOTS; i++)
		{
			iSpeedTotalBytes += m_iSpeedBytes[i];
		}
		m_iSpeedTotalBytes = iSpeedTotalBytes;
		m_tSpeedCorrection = tCurTime;
	}

	if (m_iSpeedTotalBytes == 0)
	{
		m_iSpeedStartTime = iNowSlot;
	}
	m_iSpeedBytes[m_iSpeedBytesIndex] += iBytes;
	m_iSpeedTotalBytes += iBytes;
	m_iAllBytes += iBytes;

	if (g_pOptions->GetAccurateRate())
	{
#ifdef HAVE_SPINLOCK
		m_spinlockSpeed.Unlock();
#else
		m_mutexSpeed.Unlock();
#endif
	}
}

void StatMeter::ResetSpeedStat()
{
	time_t tCurTime = time(NULL);
	m_iSpeedStartTime = (int)tCurTime / SPEEDMETER_SLOTSIZE;
	for (int i = 0; i < SPEEDMETER_SLOTS; i++)
	{
		m_iSpeedBytes[i] = 0;
		m_iSpeedTime[i] = m_iSpeedStartTime;
	}
	m_iSpeedBytesIndex = 0;
	m_iSpeedTotalBytes = 0;
	m_tSpeedCorrection = tCurTime;
}

void StatMeter::LogDebugInfo()
{
	info("   ---------- SpeedMeter");
	float fSpeed = (float)(CalcCurrentDownloadSpeed() / 1024.0);
	int iTimeDiff = (int)time(NULL) - m_iSpeedStartTime * SPEEDMETER_SLOTSIZE;
	info("      Speed: %f", fSpeed);
	info("      SpeedStartTime: %i", m_iSpeedStartTime);
	info("      SpeedTotalBytes: %i", m_iSpeedTotalBytes);
	info("      SpeedBytesIndex: %i", m_iSpeedBytesIndex);
	info("      AllBytes: %i", m_iAllBytes);
	info("      Time: %i", (int)time(NULL));
	info("      TimeDiff: %i", iTimeDiff);
	for (int i=0; i < SPEEDMETER_SLOTS; i++)
	{
		info("      Bytes[%i]: %i, Time[%i]: %i", i, m_iSpeedBytes[i], i, m_iSpeedTime[i]);
	}

	m_mutexVolume.Lock();
	int index = 0;
	for (ServerVolumes::iterator it = m_ServerVolumes.begin(); it != m_ServerVolumes.end(); it++, index++)
	{
		ServerVolume* pServerVolume = *it;
		info("      ServerVolume %i", index);
		pServerVolume->LogDebugInfo();
	}
	m_mutexVolume.Unlock();
}

void StatMeter::InitVolumes()
{
	m_ServerVolumes.resize(1 + g_pServerPool->GetServers()->size());
	m_ServerVolumes[0] = new ServerVolume();
	for (Servers::iterator it = g_pServerPool->GetServers()->begin(); it != g_pServerPool->GetServers()->end(); it++)
	{
		NewsServer* pServer = *it;
		m_ServerVolumes[pServer->GetID()] = new ServerVolume();
	}
}

void StatMeter::AddServerData(int iBytes, int iServerID)
{
	if (iBytes == 0)
	{
		return;
	}

	m_mutexVolume.Lock();
	m_ServerVolumes[0]->AddData(iBytes);
	m_ServerVolumes[iServerID]->AddData(iBytes);
	m_bStatChanged = true;
	m_mutexVolume.Unlock();
}

ServerVolumes* StatMeter::LockServerVolumes()
{
	m_mutexVolume.Lock();

	// update slots
	for (ServerVolumes::iterator it = m_ServerVolumes.begin(); it != m_ServerVolumes.end(); it++)
	{
		ServerVolume* pServerVolume = *it;
		pServerVolume->AddData(0);
	}

	return &m_ServerVolumes;
}

void StatMeter::UnlockServerVolumes()
{
	m_mutexVolume.Unlock();
}

void StatMeter::Save()
{
	if (!g_pOptions->GetServerMode())
	{
		return;
	}

	m_mutexVolume.Lock();
	g_pDiskState->SaveStats(g_pServerPool->GetServers(), &m_ServerVolumes);
	m_bStatChanged = false;
	m_mutexVolume.Unlock();
}

void StatMeter::Load()
{
	if (!g_pOptions->GetServerMode())
	{
		return;
	}

	m_mutexVolume.Lock();
	g_pDiskState->LoadStats(g_pServerPool->GetServers(), &m_ServerVolumes);

	for (ServerVolumes::iterator it = m_ServerVolumes.begin(); it != m_ServerVolumes.end(); it++)
	{
		ServerVolume* pServerVolume = *it;
		pServerVolume->CalcSlots(pServerVolume->GetDataTime() + g_pOptions->GetLocalTimeOffset());
	}

	m_mutexVolume.Unlock();
}
