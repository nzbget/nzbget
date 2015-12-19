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


#include "nzbget.h"
#include "FeedInfo.h"
#include "Util.h"

FeedInfo::FeedInfo(int id, const char* name, const char* url, bool backlog, int interval,
	const char* filter, bool pauseNzb, const char* category, int priority, const char* feedScript)
{
	m_id = id;
	m_name = name ? name : "";
	m_url = url ? url : "";
	m_filter = filter ? filter : "";
	m_backlog = backlog;
	m_filterHash = Util::HashBJ96(m_filter, strlen(m_filter), 0);
	m_category = category ? category : "";
	m_interval = interval;
	m_feedScript = feedScript ? feedScript : "";
	m_pauseNzb = pauseNzb;
	m_priority = priority;
	m_lastUpdate = 0;
	m_preview = false;
	m_status = fsUndefined;
	m_fetch = false;
	m_force = false;
}


FeedItemInfo::Attr::Attr(const char* name, const char* value)
{
	m_name = name ? name : "";
	m_value = value ? value : "";
}


FeedItemInfo::Attributes::~Attributes()
{
	for (iterator it = begin(); it != end(); it++)
	{
		delete *it;
	}
}

void FeedItemInfo::Attributes::Add(const char* name, const char* value)
{
	push_back(new Attr(name, value));
}

FeedItemInfo::Attr* FeedItemInfo::Attributes::Find(const char* name)
{
	for (iterator it = begin(); it != end(); it++)
	{
		Attr* attr = *it;
		if (!strcasecmp(attr->GetName(), name))
		{
			return attr;
		}
	}

	return NULL;
}


FeedItemInfo::FeedItemInfo()
{
	m_feedFilterHelper = NULL;
	m_category = "";
	m_size = 0;
	m_time = 0;
	m_imdbId = 0;
	m_rageId = 0;
	m_description = "";
	m_seasonNum = 0;
	m_episodeNum = 0;
	m_seasonEpisodeParsed = false;
	m_addCategory = "";
	m_pauseNzb = false;
	m_priority = 0;
	m_status = isUnknown;
	m_matchStatus = msIgnored;
	m_matchRule = 0;
	m_dupeScore = 0;
	m_dupeMode = dmScore;
}

void FeedItemInfo::SetSeason(const char* season)
{
	m_season = season;
	m_seasonNum = season ? ParsePrefixedInt(season) : 0;
}

void FeedItemInfo::SetEpisode(const char* episode)
{
	m_episode = episode;
	m_episodeNum = episode ? ParsePrefixedInt(episode) : 0;
}

int FeedItemInfo::ParsePrefixedInt(const char *value)
{
	const char* val = value;
	if (!strchr("0123456789", *val))
	{
		val++;
	}
	return atoi(val);
}

void FeedItemInfo::AppendDupeKey(const char* extraDupeKey)
{
	if (!m_dupeKey.Empty() && !Util::EmptyStr(extraDupeKey))
	{
		m_dupeKey.AppendFmt("-%s", extraDupeKey);
	}
}

void FeedItemInfo::BuildDupeKey(const char* rageId, const char* series)
{
	int rageIdVal = rageId && *rageId ? atoi(rageId) : m_rageId;

	if (m_imdbId != 0)
	{
		m_dupeKey.Format("imdb=%i", m_imdbId);
	}
	else if (series && *series && GetSeasonNum() != 0 && GetEpisodeNum() != 0)
	{
		m_dupeKey.Format("series=%s-%s-%s", series, *m_season, *m_episode);
	}
	else if (rageIdVal != 0 && GetSeasonNum() != 0 && GetEpisodeNum() != 0)
	{
		m_dupeKey.Format("rageid=%i-%s-%s", rageIdVal, *m_season, *m_episode);
	}
	else
	{
		m_dupeKey = "";
	}
}

int FeedItemInfo::GetSeasonNum()
{
	if (!m_season && !m_seasonEpisodeParsed)
	{
		ParseSeasonEpisode();
	}

	return m_seasonNum;
}

int FeedItemInfo::GetEpisodeNum()
{
	if (!m_episode && !m_seasonEpisodeParsed)
	{
		ParseSeasonEpisode();
	}

	return m_episodeNum;
}

void FeedItemInfo::ParseSeasonEpisode()
{
	m_seasonEpisodeParsed = true;

	RegEx** ppRegEx = m_feedFilterHelper->GetSeasonEpisodeRegEx();
	if (!*ppRegEx)
	{
		*ppRegEx = new RegEx("[^[:alnum:]]s?([0-9]+)[ex]([0-9]+(-?e[0-9]+)?)[^[:alnum:]]", 10);
	}

	if ((*ppRegEx)->Match(m_title))
	{
		SetSeason(BString<100>("S%02d", atoi(m_title + (*ppRegEx)->GetMatchStart(1))));

		BString<100> regValue;
		regValue.Set(m_title + (*ppRegEx)->GetMatchStart(2), (*ppRegEx)->GetMatchLen(2));

		BString<100> episode("E%s", *regValue);
		Util::ReduceStr(episode, "-", "");
		for (char* p = episode; *p; p++) *p = toupper(*p); // convert string to uppercase e02 -> E02
		SetEpisode(episode);
	}
}

const char* FeedItemInfo::GetDupeStatus()
{
	if (!m_dupeStatus)
	{
		BString<1024> statuses;
		m_feedFilterHelper->CalcDupeStatus(m_title, m_dupeKey, statuses, statuses.Capacity());
		m_dupeStatus = statuses;
	}

	return m_dupeStatus;
}


FeedHistoryInfo::FeedHistoryInfo(const char* url, FeedHistoryInfo::EStatus status, time_t lastSeen)
{
	m_url = url;
	m_status = status;
	m_lastSeen = lastSeen;
}


FeedHistory::~FeedHistory()
{
	Clear();
}

void FeedHistory::Clear()
{
	for (iterator it = begin(); it != end(); it++)
	{
		delete *it;
	}
	clear();
}

void FeedHistory::Add(const char* url, FeedHistoryInfo::EStatus status, time_t lastSeen)
{
	push_back(new FeedHistoryInfo(url, status, lastSeen));
}

void FeedHistory::Remove(const char* url)
{
	for (iterator it = begin(); it != end(); it++)
	{
		FeedHistoryInfo* feedHistoryInfo = *it;
		if (!strcmp(feedHistoryInfo->GetUrl(), url))
		{
			delete feedHistoryInfo;
			erase(it);
			break;
		}
	}
}

FeedHistoryInfo* FeedHistory::Find(const char* url)
{
	for (iterator it = begin(); it != end(); it++)
	{
		FeedHistoryInfo* feedHistoryInfo = *it;
		if (!strcmp(feedHistoryInfo->GetUrl(), url))
		{
			return feedHistoryInfo;
		}
	}

	return NULL;
}


FeedItemInfos::FeedItemInfos()
{
	debug("Creating FeedItemInfos");

	m_refCount = 0;
}

FeedItemInfos::~FeedItemInfos()
{
	debug("Destroing FeedItemInfos");

	for (iterator it = begin(); it != end(); it++)
	{
		delete *it;
	}
}

void FeedItemInfos::Retain()
{
	m_refCount++;
}

void FeedItemInfos::Release()
{
	m_refCount--;
	if (m_refCount <= 0)
	{
		delete this;
	}
}

void FeedItemInfos::Add(FeedItemInfo* feedItemInfo)
{
	push_back(feedItemInfo);
}
