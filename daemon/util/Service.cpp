/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2015-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "Service.h"
#include "DownloadInfo.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"

Service::Service()
{
	g_ServiceCoordinator->RegisterService(this);
}

void Service::WakeUp()
{
	g_ServiceCoordinator->WakeUp();
};


ServiceCoordinator::ServiceCoordinator()
{
	debug("Creating ServiceCoordinator");

}

ServiceCoordinator::~ServiceCoordinator()
{
	debug("Destroying ServiceCoordinator");
}

void ServiceCoordinator::Run()
{
	debug("Entering ServiceCoordinator-loop");

	while (!IsStopped())
	{
		// perform service work
		for (Service* service : m_services)
		{
			int serviceInterval = service->ServiceInterval();
			if (serviceInterval >= Service::Now &&
				abs(Util::CurrentTime() - service->m_lastWork) >= serviceInterval)
			{
				service->ServiceWork();
				service->m_lastWork = Util::CurrentTime();
			}
		}

		// calculate wait interval
		int waitInterval = 60 * 60 * 24; // something very large
		time_t curTime = Util::CurrentTime();
		for (Service* service : m_services)
		{
			int serviceInterval = service->ServiceInterval();
			int remaining = Service::Sleep;
			if (serviceInterval >= Service::Now)
			{
				remaining = serviceInterval - (curTime - service->m_lastWork);
				waitInterval = std::min(waitInterval, remaining);
			}
			debug("serviceInterval: %i, remaining: %i", serviceInterval, remaining);
		}

		debug("Waiting in ServiceCoordinator: %i", waitInterval);
		if (waitInterval > 0)
		{
			Guard guard(m_waitMutex);
			m_waitCond.WaitFor(m_waitMutex, waitInterval * 1000, [&] { return m_workenUp || IsStopped(); });
			m_workenUp = false;
		}
	}

	debug("Exiting ServiceCoordinator-loop");
}

void ServiceCoordinator::Stop()
{
	Thread::Stop();
	
	// Resume Run() to exit it
	Guard guard(m_waitMutex);
	m_waitCond.NotifyAll();
}

void ServiceCoordinator::WakeUp()
{
	debug("Waking up ServiceCoordinator");
	// Resume Run()
	Guard guard(m_waitMutex);
	m_workenUp = true;
	m_waitCond.NotifyAll();
}

void ServiceCoordinator::RegisterService(Service* service)
{
	m_services.push_back(service);
}
