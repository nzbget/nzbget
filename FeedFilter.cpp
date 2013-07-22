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

#include "nzbget.h"
#include "Log.h"
#include "DownloadInfo.h"
#include "Util.h"
#include "FeedFilter.h"


/*
# Filter string is similar to used in search engines. It consists of
# search rules separated with spaces. Every rule is checked for a feed
# item and if they all success the feed item is considered good. If
# any of the rules fails the feed item is ignored (rejected).
#
# Definition of rules:
#  [+|-][field:][command]param
#
#  +       - declares a positive rule. Rules are positive by default,
#            the "+" can be omitted;
#  -       - declares a negative rule. If the rule succeed the feed
#            item is ignored;
#  field   - field to which apply the rule. Available fields: title,
#            filename, category, link, size, age, date. If
#            not specified the default field "title" is used;
#  command - one of the special characters defining how to interpret the
#            parameter (followed after the command):
#            @  - search for word "param" This is default command,
#                 the "@" can be omitted;
#            " (quotation mark) - search for substring "param". The parameter
#                  must end with quotation mark as well;
#            $  - "param" defines a regular expression (using POSIX Extended
#                 Regular Expressions syntax);
#            <  - less than;
#            <= - equal or less than;
#            >  - greater than;
#            >= - equal or greater than;
#  param   - parameter for command.
#
# Commands @, " and $ are for use with text fields (title, filename, category,
# link). Commands <, <=, > and >= are for use with numeric and date
# fields (size, age, date). Commands @ and " support wildcard characters * and ?.
*/

FeedFilter::Term::Term(bool bPositive, const char* szField, ECommand eCommand, const char* szParam)
{
	m_bPositive = bPositive;
	m_szField = szField ? strdup(szField) : NULL;
	m_eCommand = eCommand;
	m_szParam = strdup(szParam);
}

FeedFilter::Term::~Term()
{
	if (m_szField)
	{
		free(m_szField);
	}
	if (m_szParam)
	{
		free(m_szParam);
	}
}


FeedFilter::FeedFilter(const char* szFilter)
{
	m_bValid = Compile(szFilter);
}

FeedFilter::~FeedFilter()
{
	for (TermList::iterator it = m_Terms.begin(); it != m_Terms.end(); it++)
	{
		delete *it;
	}
}

bool FeedFilter::Compile(const char* szFilter)
{
	debug("Compiling filter: %s", szFilter);

	bool bOK = true;

	char* szFilter2 = strdup(szFilter);
	char* szToken = szFilter2;
	bool bQuote = false;

	for (char* p = szFilter2; *p; p++)
	{
		char ch = *p;
		if ((ch == ' ' && !bQuote) || (ch == '"' && bQuote))
		{
			*p = '\0';
			CompileToken(szToken);
			szToken = p + 1;
			bQuote = false;
		}
		else if (ch == '"')
		{
			bQuote = true;
		}
	}
	CompileToken(szToken);

	free(szFilter2);

	return bOK;
}

bool FeedFilter::CompileToken(char* szToken)
{
	debug("Token: %s", szToken);

	char ch = szToken[0];

	bool bPositive = ch != '-';
	if (ch == '-' || ch == '+')
	{
		szToken++;
	}

	char *szField = NULL;
	ECommand eCommand = fcWord;

	char* szColon = strchr(szToken, ':');
	if (szColon)
	{
		szField = szToken;
		szColon[0] = '\0';
		szToken = szColon + 1;
	}

	ch = szToken[0];
	if (ch == '\0')
	{
		return false;
	}

	char ch2= szToken[1];

	if (ch == '@')
	{
		eCommand = fcWord;
		szToken++;
	}
	else if (ch == '"')
	{
		eCommand = fcSubstr;
		char* szEnd = szToken + strlen(szToken);
		if (*szEnd == '"')
		{
			*szEnd = '\0';
		}
		szToken++;
	}
	else if (ch == '$')
	{
		eCommand = fcRegex;
		szToken++;
	}
	else if (ch == '<')
	{
		eCommand = fcLess;
		szToken++;
	}
	else if (ch == '<' && ch2 == '=')
	{
		eCommand = fcLessEqual;
		szToken += 2;
	}
	else if (ch == '>')
	{
		eCommand = fcGreater;
		szToken++;
	}
	else if (ch == '>' && ch2 == '=')
	{
		eCommand = fcGreaterEqual;
		szToken += 2;
	}

	debug("%s, Field: %s, Command: %i, Param: %s", (bPositive ? "Positive" : "Negative"), szField, eCommand, szToken);

	if (!ValidateFieldName(szField))
	{
		return false;
	}

	m_Terms.push_back(new Term(bPositive, szField, eCommand, szToken));

	return true;
}

bool FeedFilter::ValidateFieldName(const char* szField)
{
	return !szField || !strcasecmp(szField, "title") || !strcasecmp(szField, "filename") ||
		!strcasecmp(szField, "category") || !strcasecmp(szField, "size") ||
		!strcasecmp(szField, "date") || !strcasecmp(szField, "age");
}

bool FeedFilter::Match(FeedItemInfo* pFeedItemInfo)
{
	for (TermList::iterator it = m_Terms.begin(); it != m_Terms.end(); it++)
	{
		Term* pTerm = *it;

		const char* szStrValue = NULL;
		long long iIntValue = 0;
		if (!GetValueForTerm(pTerm, pFeedItemInfo, &szStrValue, &iIntValue))
		{
			return false;
		}

		bool bMatch = false;
		switch (pTerm->GetCommand())
		{
			case fcWord:
				bMatch = MatchTermWord(pTerm, pFeedItemInfo, szStrValue);
				break;

			case fcSubstr:
				bMatch = MatchTermSubstr(pTerm, pFeedItemInfo, szStrValue);
				break;

			case fcRegex:
			case fcLess:
			case fcLessEqual:
			case fcGreater:
			case fcGreaterEqual:
				bMatch = false; // not yet implemented
				break;
		}

		if (pTerm->GetPositive() != bMatch)
		{
			return false;
		}
	}

	return true;
}

bool FeedFilter::GetValueForTerm(Term* pTerm, FeedItemInfo* pFeedItemInfo, const char** StrValue, long long* IntValue)
{
	*StrValue = NULL;
	*IntValue = 0;

	if (!pTerm->GetField() || !strcasecmp(pTerm->GetField(), "title"))
	{
		*StrValue = pFeedItemInfo->GetTitle();
		return true;
	}
	else if (!strcasecmp(pTerm->GetField(), "filename"))
	{
		*StrValue = pFeedItemInfo->GetFilename();
		return true;
	}
	else if (!strcasecmp(pTerm->GetField(), "category"))
	{
		*StrValue = pFeedItemInfo->GetCategory();
		return true;
	}
	else if (!strcasecmp(pTerm->GetField(), "link") || !strcasecmp(pTerm->GetField(), "url"))
	{
		*StrValue = pFeedItemInfo->GetUrl();
		return true;
	}
	else if (!strcasecmp(pTerm->GetField(), "size"))
	{
		*IntValue = pFeedItemInfo->GetSize();
		return true;
	}
	else if (!strcasecmp(pTerm->GetField(), "date"))
	{
		*IntValue = pFeedItemInfo->GetTime();
		return true;
	}
	else if (!strcasecmp(pTerm->GetField(), "age"))
	{
		*IntValue = time(NULL) - pFeedItemInfo->GetTime();
		return true;
	}

	return false;
}

bool FeedFilter::MatchTermWord(Term* pTerm, FeedItemInfo* pFeedItemInfo, const char* szStrValue)
{
	const char* WORD_SEPARATORS = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

	// split szStrValue into tokens and create pp-parameter for each token
	char* szStrValue2 = strdup(szStrValue);
	char* saveptr;
	bool bFound = false;
	char* szWord = strtok_r(szStrValue2, WORD_SEPARATORS, &saveptr);
	while (szWord)
	{
		szWord = Util::Trim(szWord);
		bFound = *szWord && Util::MatchMask(szWord, pTerm->GetParam(), false);
		if (bFound)
		{
			break;
		}
		szWord = strtok_r(NULL, WORD_SEPARATORS, &saveptr);
	}
	free(szStrValue2);

	return bFound;
}

bool FeedFilter::MatchTermSubstr(Term* pTerm, FeedItemInfo* pFeedItemInfo, const char* szStrValue)
{
	int iLen = strlen(pTerm->GetParam()) + 2 + 1;
	char* szParam = (char*)malloc(iLen);
	snprintf(szParam, iLen, "*%s*", pTerm->GetParam());
	szParam[iLen-1] = '\0';

	bool bFound = Util::MatchMask(szStrValue, szParam, false);

	return bFound;
}
