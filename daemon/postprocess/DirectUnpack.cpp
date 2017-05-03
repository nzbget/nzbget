/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "DirectUnpack.h"
#include "Log.h"
#include "Util.h"
#include "FileSystem.h"
#include "Options.h"

void DirectUnpack::StartJob(NzbInfo* nzbInfo)
{
	DirectUnpack* directUnpack = new DirectUnpack();
	directUnpack->m_nzbInfo = nzbInfo;
	directUnpack->SetAutoDestroy(false);

	nzbInfo->SetUnpackThread(directUnpack);
	nzbInfo->SetDirectUnpackStatus(NzbInfo::nsRunning);

	directUnpack->Start();
}

void DirectUnpack::Run()
{
	{
		GuardedDownloadQueue guard = DownloadQueue::Guard();
		m_nzbInfo->SetUnpackThread(nullptr);
		m_nzbInfo->SetDirectUnpackStatus(NzbInfo::nsFailure);
		SetAutoDestroy(true);
	}
}

void DirectUnpack::Stop()
{
	debug("Stopping direct unpack");
	Thread::Stop();
	Terminate();
}

void DirectUnpack::FileDownloaded(FileInfo* fileInfo)
{
}
