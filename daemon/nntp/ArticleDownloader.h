/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef ARTICLEDOWNLOADER_H
#define ARTICLEDOWNLOADER_H

#include "NString.h"
#include "Observer.h"
#include "DownloadInfo.h"
#include "Thread.h"
#include "NntpConnection.h"
#include "Decoder.h"
#include "ArticleWriter.h"
#include "Util.h"

class ArticleContentAnalyzer
{
public:
	virtual ~ArticleContentAnalyzer() {};
	virtual void Reset() = 0;
	virtual void Append(const void* buffer, int len) = 0;
};

class ArticleDownloader : public Thread, public Subject
{
public:
	enum EStatus
	{
		adUndefined,
		adRunning,
		adWaiting,
		adFinished,
		adFailed,
		adRetry,
		adCrcError,
		adNotFound,
		adConnectError,
		adFatalError
	};

	ArticleDownloader();
	virtual ~ArticleDownloader();
	void SetFileInfo(FileInfo* fileInfo) { m_fileInfo = fileInfo; }
	FileInfo* GetFileInfo() { return m_fileInfo; }
	void SetArticleInfo(ArticleInfo* articleInfo) { m_articleInfo = articleInfo; }
	ArticleInfo* GetArticleInfo() { return m_articleInfo; }
	EStatus GetStatus() { return m_status; }
	ServerStatList* GetServerStats() { return &m_serverStats; }
	virtual void Run();
	virtual void Stop();
	time_t GetLastUpdateTime() { return m_lastUpdateTime; }
	void SetLastUpdateTimeNow();
	const char* GetArticleFilename() { return m_articleFilename; }
	void SetInfoName(const char* infoName);
	const char* GetInfoName() { return m_infoName; }
	const char* GetConnectionName() { return m_connectionName; }
	void SetConnection(NntpConnection* connection) { m_connection = connection; }
	void CompleteFileParts() { m_articleWriter.CompleteFileParts(); }
	int GetDownloadedSize() { return m_downloadedSize; }
	void SetContentAnalyzer(std::unique_ptr<ArticleContentAnalyzer> contentAnalyzer) { m_contentAnalyzer = std::move(contentAnalyzer); }
	ArticleContentAnalyzer* GetContentAnalyzer() { return m_contentAnalyzer.get(); }

	void LogDebugInfo();

private:
	FileInfo* m_fileInfo;
	ArticleInfo* m_articleInfo;
	NntpConnection* m_connection = nullptr;
	EStatus m_status = adUndefined;
	Mutex m_connectionMutex;
	CString m_infoName;
	CString m_connectionName;
	CString m_articleFilename;
	time_t m_lastUpdateTime;
	Decoder m_decoder;
	ArticleWriter m_articleWriter;
	ServerStatList m_serverStats;
	bool m_writingStarted;
	int m_downloadedSize = 0;
	std::unique_ptr<ArticleContentAnalyzer> m_contentAnalyzer;

	EStatus Download();
	EStatus DecodeCheck();
	void FreeConnection(bool keepConnected);
	EStatus CheckResponse(const char* response, const char* comment);
	void SetStatus(EStatus status) { m_status = status; }
	bool Write(char* buffer, int len);
	void AddServerData();
};

#endif
