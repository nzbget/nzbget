/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef NSERVFRONTEND_H
#define NSERVFRONTEND_H

#include "Thread.h"
#include "Log.h"

class NServFrontend : public Thread
{
public:
	NServFrontend();

private:
	uint32 m_neededLogEntries = 0;
	uint32 m_neededLogFirstId = 0;
	bool m_needGoBack = false;

#ifdef WIN32
	HANDLE m_console;
#endif

	void Run();
	void Update();
	void BeforePrint();
	void PrintMessage(Message& message);
	void PrintSkip();
};

#endif
