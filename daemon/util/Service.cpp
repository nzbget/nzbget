/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#ifdef WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include "nzbget.h"
#include "Service.h"
#include "DownloadInfo.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"

Service::Service()
{
	m_lastTick = 0;
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
		for (ServiceList::iterator it = m_services.begin(); it != m_services.end(); it++)
		{
			Service* service = *it;
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
