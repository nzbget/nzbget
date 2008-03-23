/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007  Andrei Prygounkov <hugbug@users.sourceforge.net>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Revision$
 * $Date$
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "nzbget.h"
#include "PostInfo.h"
#include "Options.h"

extern Options* g_pOptions;

int PostInfo::m_iIDGen = 0;

PostInfo::PostInfo()
{
	debug("Creating PostInfo");

	m_szNZBFilename = NULL;
	m_szDestDir = NULL;
	m_szParFilename = NULL;
	m_szInfoName = NULL;
	m_bWorking = false;
	m_bParCheck = false;
	m_iParStatus = 0;
	m_bParFailed = false;
	m_szProgressLabel = strdup("");
	m_iFileProgress = 0;
	m_iStageProgress = 0;
	m_tStartTime = 0;
	m_tStageTime = 0;
	m_eStage = ptQueued;
	m_Messages.clear();
	m_iIDGen++;
	m_iID = m_iIDGen;
}

PostInfo::~ PostInfo()
{
	debug("Destroying PostInfo");

	if (m_szNZBFilename)
	{
		free(m_szNZBFilename);
	}
	if (m_szDestDir)
	{
		free(m_szDestDir);
	}
	if (m_szParFilename)
	{
		free(m_szParFilename);
	}
	if (m_szInfoName)
	{
		free(m_szInfoName);
	}
	if (m_szProgressLabel)
	{
		free(m_szProgressLabel);
	}
	for (Messages::iterator it = m_Messages.begin(); it != m_Messages.end(); it++)
	{
		delete *it;
	}
	m_Messages.clear();
}

void PostInfo::SetNZBFilename(const char* szNZBFilename)
{
	m_szNZBFilename = strdup(szNZBFilename);
}

void PostInfo::SetDestDir(const char* szDestDir)
{
	m_szDestDir = strdup(szDestDir);
}

void PostInfo::SetParFilename(const char* szParFilename)
{
	m_szParFilename = strdup(szParFilename);
}

void PostInfo::SetInfoName(const char* szInfoName)
{
	m_szInfoName = strdup(szInfoName);
}

void PostInfo::SetProgressLabel(const char* szProgressLabel)
{
	if (m_szProgressLabel)
	{
		free(m_szProgressLabel);
	}
	m_szProgressLabel = strdup(szProgressLabel);
}

PostInfo::Messages* PostInfo::LockMessages()
{
	m_mutexLog.Lock();
	return &m_Messages;
}

void PostInfo::UnlockMessages()
{
	m_mutexLog.Unlock();
}

void PostInfo::AppendMessage(Message::EKind eKind, const char * szText)
{
	Message* pMessage = new Message(++m_iIDGen, eKind, time(NULL), szText);

	m_mutexLog.Lock();
	m_Messages.push_back(pMessage);

	while (m_Messages.size() > (unsigned int)g_pOptions->GetLogBufferSize())
	{
		Message* pMessage = m_Messages.front();
		delete pMessage;
		m_Messages.pop_front();
	}
	m_mutexLog.Unlock();
}
