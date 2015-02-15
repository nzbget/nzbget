/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2013-2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
	int					m_iID;
	char*				m_szName;
	char*				m_szUrl;
	int					m_iInterval;
	char*				m_szFilter;
	unsigned int		m_iFilterHash;
	bool				m_bPauseNzb;
	char*				m_szCategory;
	int					m_iPriority;
	time_t				m_tLastUpdate;
	bool				m_bPreview;
	EStatus				m_eStatus;
	char*				m_szOutputFilename;
	bool				m_bFetch;
	bool				m_bForce;

public:
						FeedInfo(int iID, const char* szName, const char* szUrl, int iInterval,
							const char* szFilter, bool bPauseNzb, const char* szCategory, int iPriority);
						~FeedInfo();
	int					GetID() { return m_iID; }
	const char*			GetName() { return m_szName; }
	const char*			GetUrl() { return m_szUrl; }
	int					GetInterval() { return m_iInterval; }
	const char*			GetFilter() { return m_szFilter; }
	unsigned int		GetFilterHash() { return m_iFilterHash; }
	bool				GetPauseNzb() { return m_bPauseNzb; }
	const char*			GetCategory() { return m_szCategory; }
	int					GetPriority() { return m_iPriority; }
	time_t				GetLastUpdate() { return m_tLastUpdate; }
	void				SetLastUpdate(time_t tLastUpdate) { m_tLastUpdate = tLastUpdate; }
	bool				GetPreview() { return m_bPreview; }
	void				SetPreview(bool bPreview) { m_bPreview = bPreview; }
	EStatus				GetStatus() { return m_eStatus; }
	void				SetStatus(EStatus Status) { m_eStatus = Status; }
	const char*			GetOutputFilename() { return m_szOutputFilename; }
	void 				SetOutputFilename(const char* szOutputFilename);
	bool				GetFetch() { return m_bFetch; }
	void				SetFetch(bool bFetch) { m_bFetch = bFetch; }
	bool				GetForce() { return m_bForce; }
	void				SetForce(bool bForce) { m_bForce = bForce; }
};

typedef std::deque<FeedInfo*> Feeds;

class SharedFeedData
{
private:
	RegEx*				m_pSeasonEpisodeRegEx;

public:
						SharedFeedData();
						~SharedFeedData();
	RegEx*				GetSeasonEpisodeRegEx();
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
		char*			m_szName;
		char*			m_szValue;
	public:
						Attr(const char* szName, const char* szValue);
						~Attr();
		const char*		GetName() { return m_szName; }
		const char*		GetValue() { return m_szValue; }
	};
	
	typedef std::deque<Attr*>  AttributesBase;

	class Attributes: public AttributesBase
	{
	public:
						~Attributes();
		void			Add(const char* szName, const char* szValue);
		Attr*			Find(const char* szName);
	};

private:
	char*				m_szTitle;
	char*				m_szFilename;
	char*				m_szUrl;
	time_t				m_tTime;
	long long			m_lSize;
	char*				m_szCategory;
	int					m_iImdbId;
	int					m_iRageId;
	char*				m_szDescription;
	char*				m_szSeason;
	char*				m_szEpisode;
	int					m_iSeasonNum;
	int					m_iEpisodeNum;
	bool				m_bSeasonEpisodeParsed;
	char*				m_szAddCategory;
	bool				m_bPauseNzb;
	int					m_iPriority;
	EStatus				m_eStatus;
	EMatchStatus		m_eMatchStatus;
	int					m_iMatchRule;
	char*				m_szDupeKey;
	int					m_iDupeScore;
	EDupeMode			m_eDupeMode;
	char*				m_szDupeStatus;
	SharedFeedData*		m_pSharedFeedData;
	Attributes			m_Attributes;

	int					ParsePrefixedInt(const char *szValue);
	void				ParseSeasonEpisode();

public:
						FeedItemInfo();
						~FeedItemInfo();
	void				SetSharedFeedData(SharedFeedData* pSharedFeedData) { m_pSharedFeedData = pSharedFeedData; }
	const char*			GetTitle() { return m_szTitle; }
	void				SetTitle(const char* szTitle);
	const char*			GetFilename() { return m_szFilename; }
	void				SetFilename(const char* szFilename);
	const char*			GetUrl() { return m_szUrl; }
	void				SetUrl(const char* szUrl);
	long long			GetSize() { return m_lSize; }
	void				SetSize(long long lSize) { m_lSize = lSize; }
	const char*			GetCategory() { return m_szCategory; }
	void				SetCategory(const char* szCategory);
	int					GetImdbId() { return m_iImdbId; }
	void				SetImdbId(int iImdbId) { m_iImdbId = iImdbId; }
	int					GetRageId() { return m_iRageId; }
	void				SetRageId(int iRageId) { m_iRageId = iRageId; }
	const char*			GetDescription() { return m_szDescription; }
	void				SetDescription(const char* szDescription);
	const char*			GetSeason() { return m_szSeason; }
	void				SetSeason(const char* szSeason);
	const char*			GetEpisode() { return m_szEpisode; }
	void				SetEpisode(const char* szEpisode);
	int					GetSeasonNum();
	int					GetEpisodeNum();
	const char*			GetAddCategory() { return m_szAddCategory; }
	void				SetAddCategory(const char* szAddCategory);
	bool				GetPauseNzb() { return m_bPauseNzb; }
	void				SetPauseNzb(bool bPauseNzb) { m_bPauseNzb = bPauseNzb; }
	int					GetPriority() { return m_iPriority; }
	void				SetPriority(int iPriority) { m_iPriority = iPriority; }
	time_t				GetTime() { return m_tTime; }
	void				SetTime(time_t tTime) { m_tTime = tTime; }
	EStatus				GetStatus() { return m_eStatus; }
	void				SetStatus(EStatus eStatus) { m_eStatus = eStatus; }
	EMatchStatus		GetMatchStatus() { return m_eMatchStatus; }
	void				SetMatchStatus(EMatchStatus eMatchStatus) { m_eMatchStatus = eMatchStatus; }
	int					GetMatchRule() { return m_iMatchRule; }
	void				SetMatchRule(int iMatchRule) { m_iMatchRule = iMatchRule; }
	const char*			GetDupeKey() { return m_szDupeKey; }
	void				SetDupeKey(const char* szDupeKey);
	void				AppendDupeKey(const char* szExtraDupeKey);
	void				BuildDupeKey(const char* szRageId, const char* szSeries);
	int					GetDupeScore() { return m_iDupeScore; }
	void				SetDupeScore(int iDupeScore) { m_iDupeScore = iDupeScore; }
	EDupeMode			GetDupeMode() { return m_eDupeMode; }
	void				SetDupeMode(EDupeMode eDupeMode) { m_eDupeMode = eDupeMode; }
	const char*			GetDupeStatus();
	Attributes*			GetAttributes() { return &m_Attributes; }
};

typedef std::deque<FeedItemInfo*>	FeedItemInfosBase;

class FeedItemInfos : public FeedItemInfosBase
{
private:
	int					m_iRefCount;
	SharedFeedData		m_SharedFeedData;

public:
						FeedItemInfos();
						~FeedItemInfos();
	void				Retain();
	void				Release();
	void				Add(FeedItemInfo* pFeedItemInfo);
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
	char*				m_szUrl;
	EStatus				m_eStatus;
	time_t				m_tLastSeen;

public:
						FeedHistoryInfo(const char* szUrl, EStatus eStatus, time_t tLastSeen);
						~FeedHistoryInfo();
	const char*			GetUrl() { return m_szUrl; }
	EStatus				GetStatus() { return m_eStatus; }
	void				SetStatus(EStatus Status) { m_eStatus = Status; }
	time_t				GetLastSeen() { return m_tLastSeen; }
	void				SetLastSeen(time_t tLastSeen) { m_tLastSeen = tLastSeen; }
};

typedef std::deque<FeedHistoryInfo*> FeedHistoryBase;

class FeedHistory : public FeedHistoryBase
{
public:
						~FeedHistory();
	void				Clear();
	void				Add(const char* szUrl, FeedHistoryInfo::EStatus eStatus, time_t tLastSeen);
	void				Remove(const char* szUrl);
	FeedHistoryInfo*	Find(const char* szUrl);
};

#endif
