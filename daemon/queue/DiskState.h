/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
	void SaveNzbQueue(DownloadQueue* downloadQueue, DiskFile& outfile);
	bool LoadNzbList(NzbList* nzbList, Servers* servers, DiskFile& infile, int formatVersion);
	void SaveNzbInfo(NzbInfo* nzbInfo, DiskFile& outfile);
	bool LoadNzbInfo(NzbInfo* nzbInfo, Servers* servers, DiskFile& infile, int formatVersion);
	void SavePostQueue(DownloadQueue* downloadQueue, DiskFile& outfile);
	void SaveDupInfo(DupInfo* dupInfo, DiskFile& outfile);
	bool LoadDupInfo(DupInfo* dupInfo, DiskFile& infile, int formatVersion);
	void SaveHistory(DownloadQueue* downloadQueue, DiskFile& outfile);
	bool LoadHistory(DownloadQueue* downloadQueue, NzbList* nzbList, Servers* servers, DiskFile& infile, int formatVersion);
	NzbInfo* FindNzbInfo(DownloadQueue* downloadQueue, int id);
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
	bool FinishWriteTransaction(const char* newFileName, const char* destFileName);

	// backward compatibility functions (conversions from older formats)
	bool LoadPostQueue12(DownloadQueue* downloadQueue, NzbList* nzbList, DiskFile& infile, int formatVersion);
	bool LoadPostQueue5(DownloadQueue* downloadQueue, NzbList* nzbList);
	bool LoadUrlQueue12(DownloadQueue* downloadQueue, DiskFile& infile, int formatVersion);
	bool LoadUrlInfo12(NzbInfo* nzbInfo, DiskFile& infile, int formatVersion);
	int FindNzbInfoIndex(NzbList* nzbList, NzbInfo* nzbInfo);
	void ConvertDupeKey(char* buf, int bufsize);
	bool LoadFileQueue12(NzbList* nzbList, NzbList* sortList, DiskFile& infile, int formatVersion);
	void CompleteNzbList12(DownloadQueue* downloadQueue, NzbList* nzbList, int formatVersion);
	void CompleteDupList12(DownloadQueue* downloadQueue, int formatVersion);
	void CalcCriticalHealth(NzbList* nzbList);
};

extern DiskState* g_DiskState;

#endif
