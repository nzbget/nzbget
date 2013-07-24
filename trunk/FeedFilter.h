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
	enum ECommand
	{
		fcWord,
		fcSubstr,
		fcRegex,
		fcLess,
		fcLessEqual,
		fcGreater,
		fcGreaterEqual,
	};

	class Term
	{
	private:
		bool			m_bPositive;
		char*			m_szField;
		ECommand		m_eCommand;
		char*			m_szParam;
		long long		m_iIntParam;
		RegEx*			m_pRegEx;

		bool			MatchWord(const char* szStrValue);
		bool			MatchSubstr(const char* szStrValue);
		bool			MatchRegex(const char* szStrValue);

	public:
						Term(bool bPositive, const char* szField, ECommand eCommand, const char* szParam, long long iIntParam);
						~Term();
		bool			GetPositive() { return m_bPositive; }
		const char*		GetField() { return m_szField; }
		ECommand		GetCommand() { return m_eCommand; }
		const char*		GetParam() { return m_szParam; }
		bool			Match(const char* szStrValue, const long long iIntValue);
	};

	typedef std::deque<Term*> TermList;

private:
	bool				m_bValid;
	TermList			m_Terms;
	FeedItemInfo*		m_pFeedItemInfo;

	bool				Compile(const char* szFilter);
	bool				CompileToken(char* szToken);
	bool				GetFieldValue(const char* szField, FeedItemInfo* pFeedItemInfo, const char** StrValue, long long* IntValue);
	bool				ValidateFieldName(const char* szField);
	bool				ParseSizeParam(const char* szParam, long long* pIntValue);
	bool				ParseAgeParam(const char* szParam, long long* pIntValue);

public:
						FeedFilter(const char* szFilter);
						~FeedFilter();
	bool				IsValid() { return m_bValid; }
	bool				Match(FeedItemInfo* pFeedItemInfo);
};

#endif
