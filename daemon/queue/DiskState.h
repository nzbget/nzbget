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
#include "Log.h"

class DiskState
{
private:
	int					fscanf(FILE* infile, const char* Format, ...);
	bool				SaveFileInfo(FileInfo* fileInfo, const char* filename);
	bool				LoadFileInfo(FileInfo* fileInfo, const char* filename, bool fileSummary, bool articles);
	void				SaveNZBQueue(DownloadQueue* downloadQueue, FILE* outfile);
	bool				LoadNZBList(NZBList* nzbList, Servers* servers, FILE* infile, int formatVersion);
	void				SaveNZBInfo(NZBInfo* nzbInfo, FILE* outfile);
	bool				LoadNZBInfo(NZBInfo* nzbInfo, Servers* servers, FILE* infile, int formatVersion);
	void				SavePostQueue(DownloadQueue* downloadQueue, FILE* outfile);
	void				SaveDupInfo(DupInfo* dupInfo, FILE* outfile);
	bool				LoadDupInfo(DupInfo* dupInfo, FILE* infile, int formatVersion);
	void				SaveHistory(DownloadQueue* downloadQueue, FILE* outfile);
	bool				LoadHistory(DownloadQueue* downloadQueue, NZBList* nzbList, Servers* servers, FILE* infile, int formatVersion);
	NZBInfo*			FindNZBInfo(DownloadQueue* downloadQueue, int id);
	bool				SaveFeedStatus(Feeds* feeds, FILE* outfile);
	bool				LoadFeedStatus(Feeds* feeds, FILE* infile, int formatVersion);
	bool				SaveFeedHistory(FeedHistory* feedHistory, FILE* outfile);
	bool				LoadFeedHistory(FeedHistory* feedHistory, FILE* infile, int formatVersion);
	bool				SaveServerInfo(Servers* servers, FILE* outfile);
	bool				LoadServerInfo(Servers* servers, FILE* infile, int formatVersion, bool* perfectMatch);
	bool				SaveVolumeStat(ServerVolumes* serverVolumes, FILE* outfile);
	bool				LoadVolumeStat(Servers* servers, ServerVolumes* serverVolumes, FILE* infile, int formatVersion);
	void				CalcFileStats(DownloadQueue* downloadQueue, int formatVersion);
	void				CalcNZBFileStats(NZBInfo* nzbInfo, int formatVersion);
	bool				LoadAllFileStates(DownloadQueue* downloadQueue, Servers* servers);
	void				SaveServerStats(ServerStatList* serverStatList, FILE* outfile);
	bool				LoadServerStats(ServerStatList* serverStatList, Servers* servers, FILE* infile);
	bool				FinishWriteTransaction(const char* newFileName, const char* destFileName);

	// backward compatibility functions (conversions from older formats)
	bool				LoadPostQueue12(DownloadQueue* downloadQueue, NZBList* nzbList, FILE* infile, int formatVersion);
	bool				LoadPostQueue5(DownloadQueue* downloadQueue, NZBList* nzbList);
	bool				LoadUrlQueue12(DownloadQueue* downloadQueue, FILE* infile, int formatVersion);
	bool				LoadUrlInfo12(NZBInfo* nzbInfo, FILE* infile, int formatVersion);
	int					FindNZBInfoIndex(NZBList* nzbList, NZBInfo* nzbInfo);
	void				ConvertDupeKey(char* buf, int bufsize);
	bool				LoadFileQueue12(NZBList* nzbList, NZBList* sortList, FILE* infile, int formatVersion);
	void				CompleteNZBList12(DownloadQueue* downloadQueue, NZBList* nzbList, int formatVersion);
	void				CompleteDupList12(DownloadQueue* downloadQueue, int formatVersion);
	void				CalcCriticalHealth(NZBList* nzbList);

public:
	bool				DownloadQueueExists();
	bool				SaveDownloadQueue(DownloadQueue* downloadQueue);
	bool				LoadDownloadQueue(DownloadQueue* downloadQueue, Servers* servers);
	bool				SaveFile(FileInfo* fileInfo);
	bool				SaveFileState(FileInfo* fileInfo, bool completed);
	bool				LoadFileState(FileInfo* fileInfo, Servers* servers, bool completed);
	bool				LoadArticles(FileInfo* fileInfo);
	void				DiscardDownloadQueue();
	void				DiscardFile(FileInfo* fileInfo, bool deleteData, bool deletePartialState, bool deleteCompletedState);
	void				DiscardFiles(NZBInfo* nzbInfo);
	bool				SaveFeeds(Feeds* feeds, FeedHistory* feedHistory);
	bool				LoadFeeds(Feeds* feeds, FeedHistory* feedHistory);
	bool				SaveStats(Servers* servers, ServerVolumes* serverVolumes);
	bool				LoadStats(Servers* servers, ServerVolumes* serverVolumes, bool* perfectMatch);
	void				CleanupTempDir(DownloadQueue* downloadQueue);
	void				WriteCacheFlag();
	void				DeleteCacheFlag();
	void				AppendNZBMessage(int nzbId, Message::EKind kind, const char* text);
	void				LoadNZBMessages(int nzbId, MessageList* messages);
};

extern DiskState* g_pDiskState;

#endif
