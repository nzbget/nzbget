/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2013-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef FEEDFILTER_H
#define FEEDFILTER_H

#include "NString.h"
#include "DownloadInfo.h"
#include "FeedInfo.h"
#include "Util.h"

class FeedFilter
{
public:
	FeedFilter(const char* filter);
	void Match(FeedItemInfo& feedItemInfo);

private:
	typedef std::vector<CString> RefValues;

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
	public:
		Term() {}
		Term(Term&&) = delete; // catch performance issues
		void SetRefValues(RefValues* refValues) { m_refValues = refValues; }
		bool Compile(char* token);
		bool Match(FeedItemInfo& feedItemInfo);
		ETermCommand GetCommand() { return m_command; }

	private:
		bool m_positive;
		CString m_field;
		ETermCommand m_command;
		CString m_param;
		int64 m_intParam = 0;
		double m_floatParam = 0.0;
		bool m_float = false;
		std::unique_ptr<RegEx> m_regEx;
		RefValues* m_refValues = nullptr;

		bool GetFieldData(const char* field, FeedItemInfo* feedItemInfo,
			const char** StrValue, int64* IntValue);
		bool ParseParam(const char* field, const char* param);
		bool ParseSizeParam(const char* param);
		bool ParseAgeParam(const char* param);
		bool ParseNumericParam(const char* param);
		bool MatchValue(const char* strValue, int64 intValue);
		bool MatchText(const char* strValue);
		bool MatchRegex(const char* strValue);
		void FillWildMaskRefValues(const char* strValue, WildMask* mask, int refOffset);
		void FillRegExRefValues(const char* strValue, RegEx* regEx);
	};

	typedef std::deque<Term> TermList;

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
	public:
		Rule() {}
		Rule(Rule&&) = delete; // catch performance issues
		void Compile(char* rule);
		bool IsValid() { return m_isValid; }
		ERuleCommand GetCommand() { return m_command; }
		const char* GetCategory() { return m_category; }
		int GetPriority() { return m_priority; }
		int GetAddPriority() { return m_addPriority; }
		bool GetPause() { return m_pause; }
		const char* GetDupeKey() { return m_dupeKey; }
		const char* GetAddDupeKey() { return m_addDupeKey; }
		int GetDupeScore() { return m_dupeScore; }
		int GetAddDupeScore() { return m_addDupeScore; }
		EDupeMode GetDupeMode() { return m_dupeMode; }
		const char* GetRageId() { return m_rageId; }
		const char* GetTvdbId() { return m_tvdbId; }
		const char* GetTvmazeId() { return m_tvmazeId; }
		const char* GetSeries() { return m_series; }
		bool HasCategory() { return m_hasCategory; }
		bool HasPriority() { return m_hasPriority; }
		bool HasAddPriority() { return m_hasAddPriority; }
		bool HasPause() { return m_hasPause; }
		bool HasDupeScore() { return m_hasDupeScore; }
		bool HasAddDupeScore() { return m_hasAddDupeScore; }
		bool HasDupeKey() { return m_hasDupeKey; }
		bool HasAddDupeKey() { return m_hasAddDupeKey; }
		bool HasDupeMode() { return m_hasDupeMode; }
		bool HasRageId() { return m_hasRageId; }
		bool HasTvdbId() { return m_hasTvdbId; }
		bool HasTvmazeId() { return m_hasTvmazeId; }
		bool HasSeries() { return m_hasSeries; }
		bool Match(FeedItemInfo& feedItemInfo);
		void ExpandRefValues(FeedItemInfo& feedItemInfo, CString* destStr, const char* patStr);
		const char* GetRefValue(FeedItemInfo& feedItemInfo, const char* varName);

	private:
		bool m_isValid = false;
		ERuleCommand m_command = frAccept;
		CString m_category;
		int m_priority = 0;
		int m_addPriority = 0;
		bool m_pause = false;
		int m_dupeScore;
		int m_addDupeScore = 0;
		CString m_dupeKey;
		CString m_addDupeKey;
		EDupeMode m_dupeMode = dmScore;
		CString m_series;
		CString m_rageId;
		CString m_tvdbId;
		CString m_tvmazeId;
		bool m_hasCategory = false;
		bool m_hasPriority = false;
		bool m_hasAddPriority = false;
		bool m_hasPause = false;
		bool m_hasDupeScore = false;
		bool m_hasAddDupeScore = false;
		bool m_hasDupeKey = false;
		bool m_hasAddDupeKey = false;
		bool m_hasDupeMode = false;
		bool m_hasPatCategory = false;
		bool m_hasPatDupeKey = false;
		bool m_hasPatAddDupeKey = false;
		bool m_hasSeries = false;
		bool m_hasRageId = false;
		bool m_hasTvdbId = false;
		bool m_hasTvmazeId = false;
		CString m_patCategory;
		CString m_patDupeKey;
		CString m_patAddDupeKey;
		TermList m_terms;
		RefValues m_refValues;

		char* CompileCommand(char* rule);
		char* CompileOptions(char* rule);
		bool CompileTerm(char* term);
		bool MatchExpression(FeedItemInfo& feedItemInfo);
	};

	typedef std::deque<Rule> RuleList;

	RuleList m_rules;

	void Compile(const char* filter);
	void CompileRule(char* rule);
	void ApplyOptions(Rule& rule, FeedItemInfo& feedItemInfo);
};

#endif
