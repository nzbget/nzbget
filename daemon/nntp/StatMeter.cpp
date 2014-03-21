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

#include "nzbget.h"
#include "StatMeter.h"
#include "Options.h"

extern Options* g_pOptions;

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

	g_pLog->RegisterDebuggable(this);
}

StatMeter::~StatMeter()
{
	debug("Destroying StatMeter");
	// Cleanup

	g_pLog->UnregisterDebuggable(this);

	debug("StatMeter destroyed");
}

void StatMeter::AdjustTimeOffset()
{
	time_t tUtcTime = time(NULL);
	tm tmSplittedTime;
	gmtime_r(&tUtcTime, &tmSplittedTime);
	time_t tLocTime = mktime(&tmSplittedTime);
	time_t tLocalTimeDelta = tUtcTime - tLocTime;
	g_pOptions->SetLocalTimeOffset((int)tLocalTimeDelta + g_pOptions->GetTimeCorrection());

	debug("UTC delta: %i (%i+%i)", g_pOptions->GetLocalTimeOffset(), (int)tLocalTimeDelta, g_pOptions->GetTimeCorrection());
}

/*
 * Detects large step changes of system time and adjust statistics.
 */
void StatMeter::CheckTime()
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
}
