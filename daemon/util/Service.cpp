/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2015-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

	const int stepMSec = 100;
	int curTick = 0;

	while (!IsStopped())
	{
		for (Service* service : m_services)
		{
			if (curTick >= service->m_lastTick + service->ServiceInterval() ||	// interval expired
				curTick == 0 ||													// first start
				curTick + 10000 < service->m_lastTick)							// int overflow
			{
				service->ServiceWork();
				service->m_lastTick = curTick;
			}
		}

		curTick += stepMSec;
		usleep(stepMSec * 1000);
	}

	debug("Exiting ServiceCoordinator-loop");
}

void ServiceCoordinator::RegisterService(Service* service)
{
	m_services.push_back(service);
}
