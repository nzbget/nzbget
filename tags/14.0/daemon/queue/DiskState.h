/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

class DiskState
{
private:
	int					fscanf(FILE* infile, const char* Format, ...);
	int					ParseFormatVersion(const char* szFormatSignature);
	bool				SaveFileInfo(FileInfo* pFileInfo, const char* szFilename);
	bool				LoadFileInfo(FileInfo* pFileInfo, const char* szFilename, bool bFileSummary, bool bArticles);
	void				SaveNZBQueue(DownloadQueue* pDownloadQueue, FILE* outfile);
	bool				LoadNZBList(NZBList* pNZBList, Servers* pServers, FILE* infile, int iFormatVersion);
	void				SaveNZBInfo(NZBInfo* pNZBInfo, FILE* outfile);
	bool				LoadNZBInfo(NZBInfo* pNZBInfo, Servers* pServers, FILE* infile, int iFormatVersion);
	void				SavePostQueue(DownloadQueue* pDownloadQueue, FILE* outfile);
	void				SaveDupInfo(DupInfo* pDupInfo, FILE* outfile);
	bool				LoadDupInfo(DupInfo* pDupInfo, FILE* infile, int iFormatVersion);
	void				SaveHistory(DownloadQueue* pDownloadQueue, FILE* outfile);
	bool				LoadHistory(DownloadQueue* pDownloadQueue, NZBList* pNZBList, Servers* pServers, FILE* infile, int iFormatVersion);
	NZBInfo*			FindNZBInfo(DownloadQueue* pDownloadQueue, int iID);
	bool				SaveFeedStatus(Feeds* pFeeds, FILE* outfile);
	bool				LoadFeedStatus(Feeds* pFeeds, FILE* infile, int iFormatVersion);
	bool				SaveFeedHistory(FeedHistory* pFeedHistory, FILE* outfile);
	bool				LoadFeedHistory(FeedHistory* pFeedHistory, FILE* infile, int iFormatVersion);
	bool				SaveServerInfo(Servers* pServers, FILE* outfile);
	bool				LoadServerInfo(Servers* pServers, FILE* infile, int iFormatVersion, bool* pPerfectMatch);
	bool				SaveVolumeStat(ServerVolumes* pServerVolumes, FILE* outfile);
	bool				LoadVolumeStat(Servers* pServers, ServerVolumes* pServerVolumes, FILE* infile, int iFormatVersion);
	void				CalcFileStats(DownloadQueue* pDownloadQueue, int iFormatVersion);
	void				CalcNZBFileStats(NZBInfo* pNZBInfo, int iFormatVersion);
	bool				LoadAllFileStates(DownloadQueue* pDownloadQueue, Servers* pServers);
	void				SaveServerStats(ServerStatList* pServerStatList, FILE* outfile);
	bool				LoadServerStats(ServerStatList* pServerStatList, Servers* pServers, FILE* infile);

	// backward compatibility functions (conversions from older formats)
	bool				LoadPostQueue12(DownloadQueue* pDownloadQueue, NZBList* pNZBList, FILE* infile, int iFormatVersion);
	bool				LoadPostQueue5(DownloadQueue* pDownloadQueue, NZBList* pNZBList);
	bool				LoadUrlQueue12(DownloadQueue* pDownloadQueue, FILE* infile, int iFormatVersion);
	bool				LoadUrlInfo12(NZBInfo* pNZBInfo, FILE* infile, int iFormatVersion);
	int					FindNZBInfoIndex(NZBList* pNZBList, NZBInfo* pNZBInfo);
	void				ConvertDupeKey(char* buf, int bufsize);
	bool				LoadFileQueue12(NZBList* pNZBList, NZBList* pSortList, FILE* infile, int iFormatVersion);
	void				CompleteNZBList12(DownloadQueue* pDownloadQueue, NZBList* pNZBList, int iFormatVersion);
	void				CompleteDupList12(DownloadQueue* pDownloadQueue, int iFormatVersion);
	void				CalcCriticalHealth(NZBList* pNZBList);

public:
	bool				DownloadQueueExists();
	bool				SaveDownloadQueue(DownloadQueue* pDownloadQueue);
	bool				LoadDownloadQueue(DownloadQueue* pDownloadQueue, Servers* pServers);
	bool				SaveFile(FileInfo* pFileInfo);
	bool				SaveFileState(FileInfo* pFileInfo, bool bCompleted);
	bool				LoadFileState(FileInfo* pFileInfo, Servers* pServers, bool bCompleted);
	bool				LoadArticles(FileInfo* pFileInfo);
	void				DiscardDownloadQueue();
	bool				DiscardFile(FileInfo* pFileInfo, bool bDeleteData, bool bDeletePartialState, bool bDeleteCompletedState);
	void				DiscardFiles(NZBInfo* pNZBInfo);
	bool				SaveFeeds(Feeds* pFeeds, FeedHistory* pFeedHistory);
	bool				LoadFeeds(Feeds* pFeeds, FeedHistory* pFeedHistory);
	bool				SaveStats(Servers* pServers, ServerVolumes* pServerVolumes);
	bool				LoadStats(Servers* pServers, ServerVolumes* pServerVolumes, bool* pPerfectMatch);
	void				CleanupTempDir(DownloadQueue* pDownloadQueue);
	void				WriteCacheFlag();
	void				DeleteCacheFlag();
};

#endif
