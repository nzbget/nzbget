/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef WORKSTATE_H
#define WORKSTATE_H

#include "Observer.h"

// WorkState is observable but notifications are not 100% reliable.
// The changes via Set-methods and readings via Get-methods are not synchronized throughout the program.
// As result race conditions may occur and some changes may go unnoticed.
// When waiting for changes don't wait too long to avoid lock ups.

class WorkState : public Subject
{
public:
	void SetPauseDownload(bool pauseDownload) { m_pauseDownload = pauseDownload; Changed(); }
	bool GetPauseDownload() const { return m_pauseDownload; }
	void SetPausePostProcess(bool pausePostProcess) { m_pausePostProcess = pausePostProcess; Changed(); }
	bool GetPausePostProcess() const { return m_pausePostProcess; }
	void SetPauseScan(bool pauseScan) { m_pauseScan = pauseScan; Changed(); }
	bool GetPauseScan() const { return m_pauseScan; }
	void SetTempPauseDownload(bool tempPauseDownload) { m_tempPauseDownload = tempPauseDownload; Changed(); }
	bool GetTempPauseDownload() const { return m_tempPauseDownload; }
	void SetTempPausePostprocess(bool tempPausePostprocess) { m_tempPausePostprocess = tempPausePostprocess; Changed(); }
	bool GetTempPausePostprocess() const { return m_tempPausePostprocess; }
	void SetPauseFrontend(bool pauseFrontend) { m_pauseFrontend = pauseFrontend; Changed(); }
	bool GetPauseFrontend() const { return m_pauseFrontend; }
	void SetSpeedLimit(int speedLimit) { m_speedLimit = speedLimit; Changed(); }
	int GetSpeedLimit() const { return m_speedLimit; }
	void SetResumeTime(time_t resumeTime) { m_resumeTime = resumeTime; Changed(); }
	time_t GetResumeTime() const { return m_resumeTime; }
	void SetLocalTimeOffset(int localTimeOffset) { m_localTimeOffset = localTimeOffset; Changed(); }
	int GetLocalTimeOffset() { return m_localTimeOffset; }
	void SetQuotaReached(bool quotaReached) { m_quotaReached = quotaReached; Changed(); }
	bool GetQuotaReached() { return m_quotaReached; }
	void SetDownloading(bool downloading) { m_downloading = downloading; Changed(); }
	bool GetDownloading() { return m_downloading; }

private:
	bool m_pauseDownload = false;
	bool m_pausePostProcess = false;
	bool m_pauseScan = false;
	bool m_tempPauseDownload = true;
	bool m_tempPausePostprocess = true;
	bool m_pauseFrontend = false;
	int m_downloadRate = 0;
	time_t m_resumeTime = 0;
	int m_localTimeOffset = 0;
	bool m_quotaReached = false;
	int m_speedLimit = 0;
	bool m_downloading = false;

	void Changed();
};

extern WorkState* g_WorkState;

#endif
