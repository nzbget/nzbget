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
#include "FeedInfo.h"
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
		fcOpeningBrace,
		fcClosingBrace,
		fcOrOperator
	};
	
	class Term
	{
	private:
		bool			m_bPositive;
		char*			m_szField;
		ETermCommand	m_eCommand;
		char*			m_szParam;
		long long		m_iIntParam;
		double			m_fFloatParam;
		bool			m_bFloat;
		RegEx*			m_pRegEx;
		RefValues*		m_pRefValues;

		bool			GetFieldData(const char* szField, FeedItemInfo* pFeedItemInfo,
							const char** StrValue, long long* IntValue);
		bool			ParseSizeParam(const char* szParam);
		bool			ParseAgeParam(const char* szParam);
		bool			ParseNumericParam(const char* szParam);
		bool			MatchValue(const char* szStrValue, long long iIntValue);
		bool			MatchText(const char* szStrValue);
		bool			MatchRegex(const char* szStrValue);
		void			FillWildMaskRefValues(const char* szStrValue, WildMask* pMask, int iRefOffset);
		void			FillRegExRefValues(const char* szStrValue, RegEx* pRegEx);

	public:
						Term();
						~Term();
		void			SetRefValues(RefValues* pRefValues) { m_pRefValues = pRefValues; }
		bool			Compile(char* szToken);
		bool			Match(FeedItemInfo* pFeedItemInfo);
		ETermCommand	GetCommand() { return m_eCommand; }
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
		EDupeMode		m_eDupeMode;
		char*			m_szSeries;
		char*			m_szRageId;
		bool			m_bHasCategory;
		bool			m_bHasPriority;
		bool			m_bHasAddPriority;
		bool			m_bHasPause;
		bool			m_bHasDupeScore;
		bool			m_bHasAddDupeScore;
		bool			m_bHasDupeKey;
		bool			m_bHasAddDupeKey;
		bool			m_bHasDupeMode;
		bool			m_bPatCategory;
		bool			m_bPatDupeKey;
		bool			m_bPatAddDupeKey;
		bool			m_bHasSeries;
		bool			m_bHasRageId;
		char*			m_szPatCategory;
		char*			m_szPatDupeKey;
		char*			m_szPatAddDupeKey;
		TermList		m_Terms;
		RefValues		m_RefValues;

		char*			CompileCommand(char* szRule);
		char*			CompileOptions(char* szRule);
		bool			CompileTerm(char* szTerm);
		bool			MatchExpression(FeedItemInfo* pFeedItemInfo);

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
		EDupeMode		GetDupeMode() { return m_eDupeMode; }
		const char*		GetRageId() { return m_szRageId; }
		const char*		GetSeries() { return m_szSeries; }
		bool			HasCategory() { return m_bHasCategory; }
		bool			HasPriority() { return m_bHasPriority; }
		bool			HasAddPriority() { return m_bHasAddPriority; }
		bool			HasPause() { return m_bHasPause; }
		bool			HasDupeScore() { return m_bHasDupeScore; }
		bool			HasAddDupeScore() { return m_bHasAddDupeScore; }
		bool			HasDupeKey() { return m_bHasDupeKey; }
		bool			HasAddDupeKey() { return m_bHasAddDupeKey; }
		bool			HasDupeMode() { return m_bHasDupeMode; }
		bool			HasRageId() { return m_bHasRageId; }
		bool			HasSeries() { return m_bHasSeries; }
		bool			Match(FeedItemInfo* pFeedItemInfo);
		void			ExpandRefValues(FeedItemInfo* pFeedItemInfo, char** pDestStr, char* pPatStr);
		const char*		GetRefValue(FeedItemInfo* pFeedItemInfo, const char* szVarName);
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
