/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2004  Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2014  Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include "Observer.h"
#include "Log.h"

Subject::Subject()
{
	m_Observers.clear();
}

void Subject::Attach(Observer* pObserver)
{
	m_Observers.push_back(pObserver);
}

void Subject::Detach(Observer* pObserver)
{
	m_Observers.remove(pObserver);
}

void Subject::Notify(void* pAspect)
{
	debug("Notifying observers");
	
	for (std::list<Observer*>::iterator it = m_Observers.begin(); it != m_Observers.end(); it++)
	{
        Observer* Observer = *it;
		Observer->Update(this, pAspect);
	}
}
