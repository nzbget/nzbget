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
	typedef std::deque<char*> RefValues;

	enum ETermCommand
	{
		fcText,
		fcRegex,
		fcEqual,
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
		RefValues*		m_pRefValues;

		bool			GetFieldData(const char* szField, FeedItemInfo* pFeedItemInfo,
							EFieldType* FieldType, const char** StrValue, long long* IntValue);
		bool			ParseSizeParam(const char* szParam, long long* pIntValue);
		bool			ParseAgeParam(const char* szParam, long long* pIntValue);
		bool			ParseRatingParam(const char* szParam, long long* pIntValue);
		bool			ParseIntParam(const char* szParam, long long* pIntValue);
		bool			MatchValue(const char* szStrValue, const long long iIntValue);
		bool			MatchText(const char* szStrValue);
		bool			MatchRegex(const char* szStrValue);
		void			FillWildMaskRefValues(const char* szStrValue, WildMask* pMask);
		void			FillRegExRefValues(const char* szStrValue, RegEx* pRegEx);

	public:
						Term();
						~Term();
		void			SetRefValues(RefValues* pRefValues) { m_pRefValues = pRefValues; }
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
		int				m_iAddPriority;
		bool			m_bPause;
		int				m_iDupeScore;
		int				m_iAddDupeScore;
		char*			m_szDupeKey;
		char*			m_szAddDupeKey;
		bool			m_bNoDupeCheck;
		bool			m_bHasCategory;
		bool			m_bHasPriority;
		bool			m_bHasAddPriority;
		bool			m_bHasPause;
		bool			m_bHasDupeScore;
		bool			m_bHasAddDupeScore;
		bool			m_bHasDupeKey;
		bool			m_bHasAddDupeKey;
		bool			m_bHasNoDupeCheck;
		bool			m_bPatCategory;
		bool			m_bPatDupeKey;
		bool			m_bPatAddDupeKey;
		char*			m_szPatCategory;
		char*			m_szPatDupeKey;
		char*			m_szPatAddDupeKey;
		TermList		m_Terms;
		RefValues		m_RefValues;

		char*			CompileCommand(char* szRule);
		char*			CompileOptions(char* szRule);
		bool			CompileTerm(char* szTerm);

	public:
						Rule();
						~Rule();
		void			Compile(char* szRule);
		bool			IsValid() { return m_bIsValid; }
		ERuleCommand	GetCommand() { return m_eCommand; }
		const char*		GetCategory() { return m_szCategory; }
		int				GetPriority() { return m_iPriority; }
		int				GetAddPriority() { return m_iAddPriority; }
		bool			GetPause() { return m_bPause; }
		const char*		GetDupeKey() { return m_szDupeKey; }
		const char*		GetAddDupeKey() { return m_szAddDupeKey; }
		int				GetDupeScore() { return m_iDupeScore; }
		int				GetAddDupeScore() { return m_iAddDupeScore; }
		bool			GetNoDupeCheck() { return m_bNoDupeCheck; }
		bool			HasCategory() { return m_bHasCategory; }
		bool			HasPriority() { return m_bHasPriority; }
		bool			HasAddPriority() { return m_bHasAddPriority; }
		bool			HasPause() { return m_bHasPause; }
		bool			HasDupeScore() { return m_bHasDupeScore; }
		bool			HasAddDupeScore() { return m_bHasAddDupeScore; }
		bool			HasDupeKey() { return m_bHasDupeKey; }
		bool			HasAddDupeKey() { return m_bHasAddDupeKey; }
		bool			HasNoDupeCheck() { return m_bHasNoDupeCheck; }
		bool			Match(FeedItemInfo* pFeedItemInfo);
		RefValues*		GetRefValues() { return &m_RefValues; }
		void			ExpandRefValues(char** pDestStr, char* pPatStr);
	};

	typedef std::deque<Rule*> RuleList;

private:
	RuleList			m_Rules;

	void				Compile(const char* szFilter);
	void				CompileRule(char* szRule);
	void				ApplyOptions(Rule* pRule, FeedItemInfo* pFeedItemInfo);

public:
						FeedFilter(const char* szFilter);
						~FeedFilter();
	void				Match(FeedItemInfo* pFeedItemInfo);
};

#endif
