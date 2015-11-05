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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>

#include "nzbget.h"
#include "FeedInfo.h"
#include "Util.h"

FeedInfo::FeedInfo(int id, const char* name, const char* url, bool backlog, int interval,
	const char* filter, bool pauseNzb, const char* category, int priority, const char* feedScript)
{
	m_id = id;
	m_name = strdup(name ? name : "");
	m_url = strdup(url ? url : "");
	m_filter = strdup(filter ? filter : "");
	m_backlog = backlog;
	m_filterHash = Util::HashBJ96(m_filter, strlen(m_filter), 0);
	m_category = strdup(category ? category : "");
	m_interval = interval;
	m_feedScript = strdup(feedScript ? feedScript : "");
	m_pauseNzb = pauseNzb;
	m_priority = priority;
	m_lastUpdate = 0;
	m_preview = false;
	m_status = fsUndefined;
	m_outputFilename = NULL;
	m_fetch = false;
	m_force = false;
}

FeedInfo::~FeedInfo()
{
	free(m_name);
	free(m_url);
	free(m_filter);
	free(m_category);
	free(m_outputFilename);
	free(m_feedScript);
}

void FeedInfo::SetOutputFilename(const char* outputFilename)
{
	free(m_outputFilename);
	m_outputFilename = strdup(outputFilename);
}


FeedItemInfo::Attr::Attr(const char* name, const char* value)
{
	m_name = strdup(name ? name : "");
	m_value = strdup(value ? value : "");
}

FeedItemInfo::Attr::~Attr()
{
	free(m_name);
	free(m_value);
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
	m_title = NULL;
	m_filename = NULL;
	m_url = NULL;
	m_category = strdup("");
	m_size = 0;
	m_time = 0;
	m_imdbId = 0;
	m_rageId = 0;
	m_description = strdup("");
	m_season = NULL;
	m_episode = NULL;
	m_seasonNum = 0;
	m_episodeNum = 0;
	m_seasonEpisodeParsed = false;
	m_addCategory = strdup("");
	m_pauseNzb = false;
	m_priority = 0;
	m_status = isUnknown;
	m_matchStatus = msIgnored;
	m_matchRule = 0;
	m_dupeKey = NULL;
	m_dupeScore = 0;
	m_dupeMode = dmScore;
	m_dupeStatus = NULL;
}

FeedItemInfo::~FeedItemInfo()
{
	free(m_title);
	free(m_filename);
	free(m_url);
	free(m_category);
	free(m_description);
	free(m_season);
	free(m_episode);
	free(m_addCategory);
	free(m_dupeKey);
	free(m_dupeStatus);
}

void FeedItemInfo::SetTitle(const char* title)
{
	free(m_title);
	m_title = title ? strdup(title) : NULL;
}

void FeedItemInfo::SetFilename(const char* filename)
{
	free(m_filename);
	m_filename = filename ? strdup(filename) : NULL;
}

void FeedItemInfo::SetUrl(const char* url)
{
	free(m_url);
	m_url = url ? strdup(url) : NULL;
}

void FeedItemInfo::SetCategory(const char* category)
{
	free(m_category);
	m_category = strdup(category ? category: "");
}

void FeedItemInfo::SetDescription(const char* description)
{
	free(m_description);
	m_description = strdup(description ? description: "");
}

void FeedItemInfo::SetSeason(const char* season)
{
	free(m_season);
	m_season = season ? strdup(season) : NULL;
	m_seasonNum = season ? ParsePrefixedInt(season) : 0;
}

void FeedItemInfo::SetEpisode(const char* episode)
{
	free(m_episode);
	m_episode = episode ? strdup(episode) : NULL;
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

void FeedItemInfo::SetAddCategory(const char* addCategory)
{
	free(m_addCategory);
	m_addCategory = strdup(addCategory ? addCategory : "");
}

void FeedItemInfo::SetDupeKey(const char* dupeKey)
{
	free(m_dupeKey);
	m_dupeKey = strdup(dupeKey ? dupeKey : "");
}

void FeedItemInfo::AppendDupeKey(const char* extraDupeKey)
{
	if (!m_dupeKey || *m_dupeKey == '\0' || !extraDupeKey || *extraDupeKey == '\0')
	{
		return;
	}

	int len = (m_dupeKey ? strlen(m_dupeKey) : 0) + 1 + strlen(extraDupeKey) + 1;
	char* newKey = (char*)malloc(len);
	snprintf(newKey, len, "%s-%s", m_dupeKey, extraDupeKey);
	newKey[len - 1] = '\0';

	free(m_dupeKey);
	m_dupeKey = newKey;
}

void FeedItemInfo::BuildDupeKey(const char* rageId, const char* series)
{
	int rageIdVal = rageId && *rageId ? atoi(rageId) : m_rageId;

	free(m_dupeKey);

	if (m_imdbId != 0)
	{
		m_dupeKey = (char*)malloc(20);
		snprintf(m_dupeKey, 20, "imdb=%i", m_imdbId);
	}
	else if (series && *series && GetSeasonNum() != 0 && GetEpisodeNum() != 0)
	{
		int len = strlen(series) + 50;
		m_dupeKey = (char*)malloc(len);
		snprintf(m_dupeKey, len, "series=%s-%s-%s", series, m_season, m_episode);
		m_dupeKey[len-1] = '\0';
	}
	else if (rageIdVal != 0 && GetSeasonNum() != 0 && GetEpisodeNum() != 0)
	{
		m_dupeKey = (char*)malloc(100);
		snprintf(m_dupeKey, 100, "rageid=%i-%s-%s", rageIdVal, m_season, m_episode);
		m_dupeKey[100-1] = '\0';
	}
	else
	{
		m_dupeKey = strdup("");
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
		char regValue[100];
		char value[100];

		snprintf(value, 100, "S%02d", atoi(m_title + (*ppRegEx)->GetMatchStart(1)));
		value[100-1] = '\0';
		SetSeason(value);

		int len = (*ppRegEx)->GetMatchLen(2);
		len = len < 99 ? len : 99;
		strncpy(regValue, m_title + (*ppRegEx)->GetMatchStart(2), (*ppRegEx)->GetMatchLen(2));
		regValue[len] = '\0';
		snprintf(value, 100, "E%s", regValue);
		value[100-1] = '\0';
		Util::ReduceStr(value, "-", "");
		for (char* p = value; *p; p++) *p = toupper(*p); // convert string to uppercase e02 -> E02
		SetEpisode(value);
	}
}

const char* FeedItemInfo::GetDupeStatus()
{
	if (!m_dupeStatus)
	{
		char statuses[200];
		statuses[0] = '\0';
		m_feedFilterHelper->CalcDupeStatus(m_title, m_dupeKey, statuses, sizeof(statuses));
		m_dupeStatus = strdup(statuses);
	}

	return m_dupeStatus;
}


FeedHistoryInfo::FeedHistoryInfo(const char* url, FeedHistoryInfo::EStatus status, time_t lastSeen)
{
	m_url = url ? strdup(url) : NULL;
	m_status = status;
	m_lastSeen = lastSeen;
}

FeedHistoryInfo::~FeedHistoryInfo()
{
	free(m_url);
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
