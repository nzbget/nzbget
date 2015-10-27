/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2013-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 * $Revision: 0 $
 * $Date$
 *
 */


#ifndef FEEDINFO_H
#define FEEDINFO_H

#include <deque>
#include <time.h>

#include "Util.h"
#include "DownloadInfo.h"


class FeedInfo
{
public:
	enum EStatus
	{
		fsUndefined,
		fsRunning,
		fsFinished,
		fsFailed
	};

private:
	int					m_id;
	char*				m_name;
	char*				m_url;
	int					m_interval;
	char*				m_filter;
	unsigned int		m_filterHash;
	bool				m_pauseNzb;
	char*				m_category;
	char*				m_feedScript;
	int					m_priority;
	time_t				m_lastUpdate;
	bool				m_preview;
	EStatus				m_status;
	char*				m_outputFilename;
	bool				m_fetch;
	bool				m_force;
	bool				m_backlog;

public:
						FeedInfo(int id, const char* name, const char* url, bool backlog, int interval,
							const char* filter, bool pauseNzb, const char* category, int priority,
							const char* feedScript);
						~FeedInfo();
	int					GetID() { return m_id; }
	const char*			GetName() { return m_name; }
	const char*			GetUrl() { return m_url; }
	int					GetInterval() { return m_interval; }
	const char*			GetFilter() { return m_filter; }
	unsigned int		GetFilterHash() { return m_filterHash; }
	bool				GetPauseNzb() { return m_pauseNzb; }
	const char*			GetCategory() { return m_category; }
	int					GetPriority() { return m_priority; }
	const char*			GetFeedScript() { return m_feedScript; }
	time_t				GetLastUpdate() { return m_lastUpdate; }
	void				SetLastUpdate(time_t lastUpdate) { m_lastUpdate = lastUpdate; }
	bool				GetPreview() { return m_preview; }
	void				SetPreview(bool preview) { m_preview = preview; }
	EStatus				GetStatus() { return m_status; }
	void				SetStatus(EStatus Status) { m_status = Status; }
	const char*			GetOutputFilename() { return m_outputFilename; }
	void 				SetOutputFilename(const char* outputFilename);
	bool				GetFetch() { return m_fetch; }
	void				SetFetch(bool fetch) { m_fetch = fetch; }
	bool				GetForce() { return m_force; }
	void				SetForce(bool force) { m_force = force; }
	bool				GetBacklog() { return m_backlog; }
	void				SetBacklog(bool backlog) { m_backlog = backlog; }
};

typedef std::deque<FeedInfo*> Feeds;

class FeedFilterHelper
{
public:
	virtual RegEx**		GetSeasonEpisodeRegEx() = 0;
	virtual void		CalcDupeStatus(const char* title, const char* dupeKey, char* statusBuf, int bufLen) = 0;
};

class FeedItemInfo
{
public:
	enum EStatus
	{
		isUnknown,
		isBacklog,
		isFetched,
		isNew
	};

	enum EMatchStatus
	{
		msIgnored,
		msAccepted,
		msRejected
	};

	class Attr
	{
	private:
		char*			m_name;
		char*			m_value;
	public:
						Attr(const char* name, const char* value);
						~Attr();
		const char*		GetName() { return m_name; }
		const char*		GetValue() { return m_value; }
	};
	
	typedef std::deque<Attr*>  AttributesBase;

	class Attributes: public AttributesBase
	{
	public:
						~Attributes();
		void			Add(const char* name, const char* value);
		Attr*			Find(const char* name);
	};

private:
	char*				m_title;
	char*				m_filename;
	char*				m_url;
	time_t				m_time;
	long long			m_size;
	char*				m_category;
	int					m_imdbId;
	int					m_rageId;
	char*				m_description;
	char*				m_season;
	char*				m_episode;
	int					m_seasonNum;
	int					m_episodeNum;
	bool				m_seasonEpisodeParsed;
	char*				m_addCategory;
	bool				m_pauseNzb;
	int					m_priority;
	EStatus				m_status;
	EMatchStatus		m_matchStatus;
	int					m_matchRule;
	char*				m_dupeKey;
	int					m_dupeScore;
	EDupeMode			m_dupeMode;
	char*				m_dupeStatus;
	FeedFilterHelper*	m_feedFilterHelper;
	Attributes			m_attributes;

	int					ParsePrefixedInt(const char *value);
	void				ParseSeasonEpisode();

public:
						FeedItemInfo();
						~FeedItemInfo();
	void				SetFeedFilterHelper(FeedFilterHelper* feedFilterHelper) { m_feedFilterHelper = feedFilterHelper; }
	const char*			GetTitle() { return m_title; }
	void				SetTitle(const char* title);
	const char*			GetFilename() { return m_filename; }
	void				SetFilename(const char* filename);
	const char*			GetUrl() { return m_url; }
	void				SetUrl(const char* url);
	long long			GetSize() { return m_size; }
	void				SetSize(long long size) { m_size = size; }
	const char*			GetCategory() { return m_category; }
	void				SetCategory(const char* category);
	int					GetImdbId() { return m_imdbId; }
	void				SetImdbId(int imdbId) { m_imdbId = imdbId; }
	int					GetRageId() { return m_rageId; }
	void				SetRageId(int rageId) { m_rageId = rageId; }
	const char*			GetDescription() { return m_description; }
	void				SetDescription(const char* description);
	const char*			GetSeason() { return m_season; }
	void				SetSeason(const char* season);
	const char*			GetEpisode() { return m_episode; }
	void				SetEpisode(const char* episode);
	int					GetSeasonNum();
	int					GetEpisodeNum();
	const char*			GetAddCategory() { return m_addCategory; }
	void				SetAddCategory(const char* addCategory);
	bool				GetPauseNzb() { return m_pauseNzb; }
	void				SetPauseNzb(bool pauseNzb) { m_pauseNzb = pauseNzb; }
	int					GetPriority() { return m_priority; }
	void				SetPriority(int priority) { m_priority = priority; }
	time_t				GetTime() { return m_time; }
	void				SetTime(time_t time) { m_time = time; }
	EStatus				GetStatus() { return m_status; }
	void				SetStatus(EStatus status) { m_status = status; }
	EMatchStatus		GetMatchStatus() { return m_matchStatus; }
	void				SetMatchStatus(EMatchStatus matchStatus) { m_matchStatus = matchStatus; }
	int					GetMatchRule() { return m_matchRule; }
	void				SetMatchRule(int matchRule) { m_matchRule = matchRule; }
	const char*			GetDupeKey() { return m_dupeKey; }
	void				SetDupeKey(const char* dupeKey);
	void				AppendDupeKey(const char* extraDupeKey);
	void				BuildDupeKey(const char* rageId, const char* series);
	int					GetDupeScore() { return m_dupeScore; }
	void				SetDupeScore(int dupeScore) { m_dupeScore = dupeScore; }
	EDupeMode			GetDupeMode() { return m_dupeMode; }
	void				SetDupeMode(EDupeMode dupeMode) { m_dupeMode = dupeMode; }
	const char*			GetDupeStatus();
	Attributes*			GetAttributes() { return &m_attributes; }
};

typedef std::deque<FeedItemInfo*>	FeedItemInfosBase;

class FeedItemInfos : public FeedItemInfosBase
{
private:
	int					m_refCount;

public:
						FeedItemInfos();
						~FeedItemInfos();
	void				Retain();
	void				Release();
	void				Add(FeedItemInfo* feedItemInfo);
};

class FeedHistoryInfo
{
public:
	enum EStatus
	{
		hsUnknown,
		hsBacklog,
		hsFetched
	};

private:
	char*				m_url;
	EStatus				m_status;
	time_t				m_lastSeen;

public:
						FeedHistoryInfo(const char* url, EStatus status, time_t lastSeen);
						~FeedHistoryInfo();
	const char*			GetUrl() { return m_url; }
	EStatus				GetStatus() { return m_status; }
	void				SetStatus(EStatus Status) { m_status = Status; }
	time_t				GetLastSeen() { return m_lastSeen; }
	void				SetLastSeen(time_t lastSeen) { m_lastSeen = lastSeen; }
};

typedef std::deque<FeedHistoryInfo*> FeedHistoryBase;

class FeedHistory : public FeedHistoryBase
{
public:
						~FeedHistory();
	void				Clear();
	void				Add(const char* url, FeedHistoryInfo::EStatus status, time_t lastSeen);
	void				Remove(const char* url);
	FeedHistoryInfo*	Find(const char* url);
};

#endif
