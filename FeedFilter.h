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

	public:
						Term(bool bPositive, const char* szField, ECommand eCommand, const char* szParam);
						~Term();
		bool			GetPositive() { return m_bPositive; }
		const char*		GetField() { return m_szField; }
		ECommand		GetCommand() { return m_eCommand; }
		const char*		GetParam() { return m_szParam; }
	};

	typedef std::deque<Term*> TermList;

private:
	bool				m_bValid;
	TermList			m_Terms;
	FeedItemInfo*		m_pFeedItemInfo;

	bool				Compile(const char* szFilter);
	bool				CompileToken(char* szToken);
	bool				GetValueForTerm(Term* pTerm, FeedItemInfo* pFeedItemInfo, const char** StrValue, long long* IntValue);
	bool				MatchTermWord(Term* pTerm, FeedItemInfo* pFeedItemInfo, const char* szStrValue);
	bool				MatchTermSubstr(Term* pTerm, FeedItemInfo* pFeedItemInfo, const char* szStrValue);
	bool				ValidateFieldName(const char* szField);

public:
						FeedFilter(const char* szFilter);
						~FeedFilter();
	bool				IsValid() { return m_bValid; }
	bool				Match(FeedItemInfo* pFeedItemInfo);
};

#endif
