/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef COLOREDFRONTEND_H
#define COLOREDFRONTEND_H

#include "LoggableFrontend.h"
#include "Log.h"

class ColoredFrontend : public LoggableFrontend
{
public:
	ColoredFrontend();

protected:
	virtual void BeforePrint();
	virtual void PrintMessage(Message& message);
	virtual void PrintStatus();
	virtual void PrintSkip();
	virtual void BeforeExit();

private:
	bool m_needGoBack = false;

#ifdef WIN32
	HANDLE m_console;
#endif
};

#endif
