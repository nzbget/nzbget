/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef FEEDFILTER_H
#define FEEDFILTER_H

#include "DownloadInfo.h"
#include "Util.h"

class FeedFilter
{
private:
	enum ETermCommand
	{
		fcText,
		fcRegex,
		fcLess,
		fcLessEqual,
		fcGreater,
		fcGreaterEqual,
	};
	
	enum EFieldType
	{
		ftString,
		ftNumeric
	};

	class Term
	{
	private:
		bool			m_bPositive;
		char*			m_szField;
		ETermCommand	m_eCommand;
		char*			m_szParam;
		long long		m_iIntParam;
		RegEx*			m_pRegEx;

		bool			GetFieldData(const char* szField, FeedItemInfo* pFeedItemInfo,
							EFieldType* FieldType, const char** StrValue, long long* IntValue);
		bool			ParseSizeParam(const char* szParam, long long* pIntValue);
		bool			ParseAgeParam(const char* szParam, long long* pIntValue);
		bool			MatchValue(const char* szStrValue, const long long iIntValue);
		bool			MatchText(const char* szStrValue);
		bool			MatchRegex(const char* szStrValue);

	public:
						Term();
						~Term();
		bool			Compile(char* szToken);
		bool			Match(FeedItemInfo* pFeedItemInfo);
	};

	typedef std::deque<Term*> TermList;

	enum ERuleCommand
	{
		frAccept,
		frReject,
		frRequire,
		frOptions,
		frComment
	};

	class Rule
	{
	private:
		bool			m_bIsValid;
		ERuleCommand	m_eCommand;
		char*			m_szCategory;
		int				m_iPriority;
		bool			m_bPause;
		bool			m_bHasCategory;
		bool			m_bHasPriority;
		bool			m_bHasPause;
		TermList		m_Terms;

		char*			CompileCommand(char* szRule);
		bool			CompileTerm(char* szTerm);

	public:
						Rule();
						~Rule();
		void			Compile(char* szRule);
		bool			IsValid() { return m_bIsValid; }
		ERuleCommand	GetCommand() { return m_eCommand; }
		const char*		GetCategory() { return m_szCategory; }
		int				GetPriority() { return m_iPriority; }
		bool			GetPause() { return m_bPause; }
		bool			HasCategory() { return m_bHasCategory; }
		bool			HasPriority() { return m_bHasPriority; }
		bool			HasPause() { return m_bHasPause; }
		bool			Match(FeedItemInfo* pFeedItemInfo);
	};

	typedef std::deque<Rule*> RuleList;

private:
	RuleList			m_Rules;
	FeedItemInfo*		m_pFeedItemInfo;

	void				Compile(const char* szFilter);
	void				CompileRule(char* szRule);

public:
						FeedFilter(const char* szFilter);
						~FeedFilter();
	void				Match(FeedItemInfo* pFeedItemInfo);
};

#endif
