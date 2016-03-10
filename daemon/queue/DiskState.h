/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
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


#ifndef DISKSTATE_H
#define DISKSTATE_H

#include "DownloadInfo.h"
#include "FeedInfo.h"
#include "NewsServer.h"
#include "StatMeter.h"
#include "FileSystem.h"
#include "Log.h"

class DiskState
{
public:
	bool DownloadQueueExists();
	bool SaveDownloadQueue(DownloadQueue* downloadQueue);
	bool LoadDownloadQueue(DownloadQueue* downloadQueue, Servers* servers);
	bool SaveFile(FileInfo* fileInfo);
	bool SaveFileState(FileInfo* fileInfo, bool completed);
	bool LoadFileState(FileInfo* fileInfo, Servers* servers, bool completed);
	bool LoadArticles(FileInfo* fileInfo);
	void DiscardDownloadQueue();
	void DiscardFile(FileInfo* fileInfo, bool deleteData, bool deletePartialState, bool deleteCompletedState);
	void DiscardFiles(NzbInfo* nzbInfo);
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
	int fscanf(DiskFile& infile, const char* format, ...);
	bool SaveFileInfo(FileInfo* fileInfo, const char* filename);
	bool LoadFileInfo(FileInfo* fileInfo, const char* filename, bool fileSummary, bool articles);
	void SaveQueue(NzbList* queue, DiskFile& outfile);
	bool LoadQueue(NzbList* queue, Servers* servers, DiskFile& infile, int formatVersion);
	void SaveNzbInfo(NzbInfo* nzbInfo, DiskFile& outfile);
	bool LoadNzbInfo(NzbInfo* nzbInfo, Servers* servers, DiskFile& infile, int formatVersion);
	void SaveDupInfo(DupInfo* dupInfo, DiskFile& outfile);
	bool LoadDupInfo(DupInfo* dupInfo, DiskFile& infile, int formatVersion);
	void SaveHistory(HistoryList* history, DiskFile& outfile);
	bool LoadHistory(HistoryList* history, Servers* servers, DiskFile& infile, int formatVersion);
	bool SaveFeedStatus(Feeds* feeds, DiskFile& outfile);
	bool LoadFeedStatus(Feeds* feeds, DiskFile& infile, int formatVersion);
	bool SaveFeedHistory(FeedHistory* feedHistory, DiskFile& outfile);
	bool LoadFeedHistory(FeedHistory* feedHistory, DiskFile& infile, int formatVersion);
	bool SaveServerInfo(Servers* servers, DiskFile& outfile);
	bool LoadServerInfo(Servers* servers, DiskFile& infile, int formatVersion, bool* perfectMatch);
	bool SaveVolumeStat(ServerVolumes* serverVolumes, DiskFile& outfile);
	bool LoadVolumeStat(Servers* servers, ServerVolumes* serverVolumes, DiskFile& infile, int formatVersion);
	void CalcFileStats(DownloadQueue* downloadQueue, int formatVersion);
	void CalcNzbFileStats(NzbInfo* nzbInfo, int formatVersion);
	bool LoadAllFileStates(DownloadQueue* downloadQueue, Servers* servers);
	void SaveServerStats(ServerStatList* serverStatList, DiskFile& outfile);
	bool LoadServerStats(ServerStatList* serverStatList, Servers* servers, DiskFile& infile);
};

extern DiskState* g_DiskState;

#endif
