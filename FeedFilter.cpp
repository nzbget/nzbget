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


FeedFilter::Term::Term(bool bPositive, const char* szField, ECommand eCommand, const char* szParam, long long iIntParam)
{
	m_bPositive = bPositive;
	m_szField = szField ? strdup(szField) : NULL;
	m_eCommand = eCommand;
	m_szParam = strdup(szParam);
	m_iIntParam = iIntParam;
	m_pRegEx = NULL;
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
	if (m_pRegEx)
	{
		delete m_pRegEx;
	}
}

bool FeedFilter::Term::Match(const char* szStrValue, const long long iIntValue)
{
	switch (m_eCommand)
	{
		case fcText:
			return MatchText(szStrValue);

		case fcRegex:
			return MatchRegex(szStrValue);

		case fcLess:
			return iIntValue < m_iIntParam;

		case fcLessEqual:
			return iIntValue <= m_iIntParam;

		case fcGreater:
			return iIntValue > m_iIntParam;

		case fcGreaterEqual:
			return iIntValue >= m_iIntParam;
	}

	return false;
}

bool FeedFilter::Term::MatchText(const char* szStrValue)
{
	const char* WORD_SEPARATORS = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

	// first check if we should make word-search or substring-search
	int iParamLen = strlen(m_szParam);
	bool bSubstr = iParamLen >= 2 && m_szParam[0] == '*' && m_szParam[iParamLen-1] == '*';
	if (!bSubstr)
	{
		for (const char* p = m_szParam; *p; p++)
		{
			char ch = *p;
			if (strchr(WORD_SEPARATORS, ch) && ch != '*' && ch != '?')
			{
				bSubstr = true;
				break;
			}
		}
	}

	bool bMatch = false;

	if (!bSubstr)
	{
		// Word-search

		// split szStrValue into tokens and create pp-parameter for each token
		char* szStrValue2 = strdup(szStrValue);
		char* saveptr;
		char* szWord = strtok_r(szStrValue2, WORD_SEPARATORS, &saveptr);
		while (szWord)
		{
			szWord = Util::Trim(szWord);
			bMatch = *szWord && Util::MatchMask(szWord, m_szParam, false);
			if (bMatch)
			{
				break;
			}
			szWord = strtok_r(NULL, WORD_SEPARATORS, &saveptr);
		}
		free(szStrValue2);
	}
	else
	{
		// Substring-search

		const char* szFormat = "*%s*";
		if (iParamLen >= 2 && m_szParam[0] == '*' && m_szParam[iParamLen-1] == '*')
		{
			szFormat = "%s";
		}
		else if (iParamLen >= 1 && m_szParam[0] == '*')
		{
			szFormat = "%s*";
		}
		else if (iParamLen >= 1 && m_szParam[iParamLen-1] == '*')
		{
			szFormat = "*%s";
		}

		int iMaskLen = strlen(m_szParam) + 2 + 1;
		char* szMask = (char*)malloc(iMaskLen);
		snprintf(szMask, iMaskLen, szFormat, m_szParam);
		szMask[iMaskLen-1] = '\0';

		bMatch = Util::MatchMask(szStrValue, szMask, false);

		free(szMask);
	}

	return bMatch;
}

bool FeedFilter::Term::MatchRegex(const char* szStrValue)
{
	if (!m_pRegEx)
	{
		m_pRegEx = new RegEx(m_szParam);
	}

	bool bFound = m_pRegEx->Match(szStrValue);
	return bFound;
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

	for (char* p = szFilter2; *p && bOK; p++)
	{
		char ch = *p;
		if (ch == ' ')
		{
			*p = '\0';
			bOK = CompileToken(szToken);
			szToken = p + 1;
		}
	}

	bOK = bOK && CompileToken(szToken);

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
	ECommand eCommand = fcText;

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
		eCommand = fcText;
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

	long long iIntValue = 0;

	if ((szField && !strcasecmp(szField, "size") && !ParseSizeParam(szToken, &iIntValue)) ||
		(szField && !strcasecmp(szField, "age") && !ParseAgeParam(szToken, &iIntValue)))
	{
		return false;
	}

	m_Terms.push_back(new Term(bPositive, szField, eCommand, szToken, iIntValue));

	return true;
}

bool FeedFilter::ValidateFieldName(const char* szField)
{
	return !szField || !strcasecmp(szField, "title") || !strcasecmp(szField, "filename") ||
		!strcasecmp(szField, "category") || !strcasecmp(szField, "size") || !strcasecmp(szField, "age");
}

bool FeedFilter::Match(FeedItemInfo* pFeedItemInfo)
{
	if (!m_bValid)
	{
		return false;
	}

	for (TermList::iterator it = m_Terms.begin(); it != m_Terms.end(); it++)
	{
		Term* pTerm = *it;

		const char* szStrValue = NULL;
		long long iIntValue = 0;
		if (!GetFieldValue(pTerm->GetField(), pFeedItemInfo, &szStrValue, &iIntValue))
		{
			return false;
		}

		bool bMatch = pTerm->Match(szStrValue, iIntValue);

		if (pTerm->GetPositive() != bMatch)
		{
			return false;
		}
	}

	return true;
}

bool FeedFilter::GetFieldValue(const char* szField, FeedItemInfo* pFeedItemInfo, const char** StrValue, long long* IntValue)
{
	*StrValue = NULL;
	*IntValue = 0;

	if (!szField || !strcasecmp(szField, "title"))
	{
		*StrValue = pFeedItemInfo->GetTitle();
		return true;
	}
	else if (!strcasecmp(szField, "filename"))
	{
		*StrValue = pFeedItemInfo->GetFilename();
		return true;
	}
	else if (!strcasecmp(szField, "category"))
	{
		*StrValue = pFeedItemInfo->GetCategory();
		return true;
	}
	else if (!strcasecmp(szField, "link") || !strcasecmp(szField, "url"))
	{
		*StrValue = pFeedItemInfo->GetUrl();
		return true;
	}
	else if (!strcasecmp(szField, "size"))
	{
		*IntValue = pFeedItemInfo->GetSize();
		return true;
	}
	else if (!strcasecmp(szField, "age"))
	{
		*IntValue = time(NULL) - pFeedItemInfo->GetTime();
		return true;
	}

	return false;
}

bool FeedFilter::ParseSizeParam(const char* szParam, long long* pIntValue)
{
	*pIntValue = 0;

	double fParam = atof(szParam);

	const char* p;
	for (p = szParam; *p && ((*p >= '0' && *p <='9') || *p == '.'); p++) ;
	if (*p)
	{
		if (!strcasecmp(p, "K") || !strcasecmp(p, "KB"))
		{
			*pIntValue = (long long)(fParam*1024);
		}
		else if (!strcasecmp(p, "M") || !strcasecmp(p, "MB"))
		{
			*pIntValue = (long long)(fParam*1024*1024);
		}
		else if (!strcasecmp(p, "G") || !strcasecmp(p, "GB"))
		{
			*pIntValue = (long long)(fParam*1024*1024*1024);
		}
		else
		{
			return false;
		}
	}
	else
	{
		*pIntValue = (long long)fParam;
	}

	return true;
}

bool FeedFilter::ParseAgeParam(const char* szParam, long long* pIntValue)
{
	*pIntValue = atoll(szParam);

	const char* p;
	for (p = szParam; *p && ((*p >= '0' && *p <='9') || *p == '.'); p++) ;
	if (*p)
	{
		if (!strcasecmp(p, "m"))
		{
			// minutes
			*pIntValue *= 60;
		}
		else if (!strcasecmp(p, "h"))
		{
			// hours
			*pIntValue *= 60 * 60;
		}
		else if (!strcasecmp(p, "d"))
		{
			// days
			*pIntValue *= 60 * 60 * 24;
		}
		else
		{
			return false;
		}
	}
	else
	{
		// days by default
		*pIntValue *= 60 * 60 * 24;
	}

	return true;
}
