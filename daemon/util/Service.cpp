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
	m_iLastTick = 0;
	g_pServiceCoordinator->RegisterService(this);
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

	const int iStepMSec = 100;
	int iCurTick = 0;

	while (!IsStopped())
	{
		for (ServiceList::iterator it = m_Services.begin(); it != m_Services.end(); it++)
		{
			Service* pService = *it;
			if (iCurTick >= pService->m_iLastTick + pService->ServiceInterval() ||	// interval expired
				iCurTick == 0 ||													// first start
				iCurTick + 10000 < pService->m_iLastTick)							// int overflow
			{
				pService->ServiceWork();
				pService->m_iLastTick = iCurTick;
			}
		}

		iCurTick += iStepMSec;
		usleep(iStepMSec * 1000);
	}

	debug("Exiting ServiceCoordinator-loop");
}

void ServiceCoordinator::RegisterService(Service* pService)
{
	m_Services.push_back(pService);
}
