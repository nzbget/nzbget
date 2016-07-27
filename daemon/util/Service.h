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


#ifndef SERVICE_H
#define SERVICE_H

#include "Thread.h"

class Service
{
public:
	Service();

protected:
	virtual int ServiceInterval() = 0;
	virtual void ServiceWork() = 0;

private:
	int m_lastTick = 0;

	friend class ServiceCoordinator;
};

class ServiceCoordinator : public Thread
{
public:
	typedef std::vector<Service*> ServiceList;

	ServiceCoordinator();
	virtual ~ServiceCoordinator();
	virtual void Run();

private:
	ServiceList m_services;

	void RegisterService(Service* service);

	friend class Service;
};

extern ServiceCoordinator* g_ServiceCoordinator;

#endif
