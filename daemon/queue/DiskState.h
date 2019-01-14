/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2007-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef DISKSTATE_H
#define DISKSTATE_H

#include "DownloadInfo.h"
#include "FeedInfo.h"
#include "NewsServer.h"
#include "StatMeter.h"
#include "FileSystem.h"
#include "Log.h"

class StateDiskFile;

class DiskState
{
public:
	bool DownloadQueueExists();
	bool SaveDownloadQueue(DownloadQueue* downloadQueue, bool saveHistory);
	bool LoadDownloadQueue(DownloadQueue* downloadQueue, Servers* servers);
	bool SaveDownloadProgress(DownloadQueue* downloadQueue);
	bool SaveFile(FileInfo* fileInfo);
	bool LoadFile(FileInfo* fileInfo, bool fileSummary, bool articles);
	bool SaveAllFileInfos(DownloadQueue* downloadQueue);
	void DiscardQuickFileInfos();
	bool SaveFileState(FileInfo* fileInfo, bool completed);
	bool LoadFileState(FileInfo* fileInfo, Servers* servers, bool completed);
	bool LoadArticles(FileInfo* fileInfo);
	void DiscardDownloadQueue();
	void DiscardFile(int fileId, bool deleteData, bool deletePartialState, bool deleteCompletedState);
	void DiscardFiles(NzbInfo* nzbInfo, bool deleteLog = true);
	bool SaveFeeds(Feeds* feeds, FeedHistory* feedHistory);
	bool LoadFeeds(Feeds* feeds, FeedHistory* feedHistory);
	bool SaveStats(Servers* servers, ServerVolumes* serverVolumes);
	bool LoadStats(Servers* servers, ServerVolumes* serverVolumes, bool* perfectMatch);
	void CleanupTempDir(DownloadQueue* downloadQueue);
	void WriteCacheFlag();
	void DeleteCacheFlag();
	void AppendNzbMessage(int nzbId, Message::EKind kind, const char* text);
	void LoadNzbMessages(int nzbId, MessageList* messages);

private:
	bool SaveFileInfo(FileInfo* fileInfo, StateDiskFile& outfile, bool articles);
	bool LoadFileInfo(FileInfo* fileInfo, StateDiskFile& outfile, int formatVersion, bool fileSummary, bool articles);
	bool SaveFileState(FileInfo* fileInfo, StateDiskFile& outfile, bool completed);
	bool LoadFileState(FileInfo* fileInfo, Servers* servers, StateDiskFile& infile, int formatVersion, bool completed);
	void SaveQueue(NzbList* queue, StateDiskFile& outfile);
	bool LoadQueue(NzbList* queue, Servers* servers, StateDiskFile& infile, int formatVersion);
	void SaveProgress(NzbList* queue, StateDiskFile& outfile, int changedCount);
	bool LoadProgress(NzbList* queue, Servers* servers, StateDiskFile& infile, int formatVersion);
	void SaveNzbInfo(NzbInfo* nzbInfo, StateDiskFile& outfile);
	bool LoadNzbInfo(NzbInfo* nzbInfo, Servers* servers, StateDiskFile& infile, int formatVersion);
	void SaveDupInfo(DupInfo* dupInfo, StateDiskFile& outfile);
	bool LoadDupInfo(DupInfo* dupInfo, StateDiskFile& infile, int formatVersion);
	void SaveHistory(HistoryList* history, StateDiskFile& outfile);
	bool LoadHistory(HistoryList* history, Servers* servers, StateDiskFile& infile, int formatVersion);
	bool SaveFeedStatus(Feeds* feeds, StateDiskFile& outfile);
	bool LoadFeedStatus(Feeds* feeds, StateDiskFile& infile, int formatVersion);
	bool SaveFeedHistory(FeedHistory* feedHistory, StateDiskFile& outfile);
	bool LoadFeedHistory(FeedHistory* feedHistory, StateDiskFile& infile, int formatVersion);
	bool SaveServerInfo(Servers* servers, StateDiskFile& outfile);
	bool LoadServerInfo(Servers* servers, StateDiskFile& infile, int formatVersion, bool* perfectMatch);
	bool SaveVolumeStat(ServerVolumes* serverVolumes, StateDiskFile& outfile);
	bool LoadVolumeStat(Servers* servers, ServerVolumes* serverVolumes, StateDiskFile& infile, int formatVersion);
	void CalcFileStats(DownloadQueue* downloadQueue, int formatVersion);
	bool LoadAllFileInfos(DownloadQueue* downloadQueue);
	bool LoadAllFileStates(DownloadQueue* downloadQueue, Servers* servers);
	void SaveServerStats(ServerStatList* serverStatList, StateDiskFile& outfile);
	bool LoadServerStats(ServerStatList* serverStatList, Servers* servers, StateDiskFile& infile);
	void CleanupQueueDir(DownloadQueue* downloadQueue);
};

extern DiskState* g_DiskState;

#endif
