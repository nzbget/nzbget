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


FeedFilter::Term::Term()
{
	m_szField = NULL;
	m_szParam = NULL;
	m_iIntParam = 0;
	m_pRegEx = NULL;
	m_pRefValues = NULL;
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

bool FeedFilter::Term::Match(FeedItemInfo* pFeedItemInfo)
{
	const char* szStrValue = NULL;
	long long iIntValue = 0;
	EFieldType FieldType;

	if (!GetFieldData(m_szField, pFeedItemInfo, &FieldType, &szStrValue, &iIntValue))
	{
		return false;
	}

	bool bMatch = MatchValue(szStrValue, iIntValue);

	if (m_bPositive != bMatch)
	{
		return false;
	}

	return true;
}

bool FeedFilter::Term::MatchValue(const char* szStrValue, const long long iIntValue)
{
	switch (m_eCommand)
	{
		case fcText:
			return MatchText(szStrValue);

		case fcRegex:
			return MatchRegex(szStrValue);

		case fcEqual:
			return iIntValue == m_iIntParam;

		case fcLess:
			return iIntValue < m_iIntParam;

		case fcLessEqual:
			return iIntValue <= m_iIntParam;

		case fcGreater:
			return iIntValue > m_iIntParam;

		case fcGreaterEqual:
			return iIntValue >= m_iIntParam;

		default:
			return false;
	}
}

bool FeedFilter::Term::MatchText(const char* szStrValue)
{
	const char* WORD_SEPARATORS = " !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

	// first check if we should make word-search or substring-search
	int iParamLen = strlen(m_szParam);
	bool bSubstr = iParamLen >= 2 && m_szParam[0] == '*' && m_szParam[iParamLen-1] == '*';
	if (!bSubstr)
	{
		for (const char* p = m_szParam; *p; p++)
		{
			char ch = *p;
			if (strchr(WORD_SEPARATORS, ch) && ch != '*' && ch != '?' && ch != '#')
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

		// split szStrValue into tokens
		char* szStrValue2 = strdup(szStrValue);
		char* saveptr;
		char* szWord = strtok_r(szStrValue2, WORD_SEPARATORS, &saveptr);
		while (szWord)
		{
			szWord = Util::Trim(szWord);
			WildMask mask(m_szParam, m_pRefValues != NULL);
			bMatch = *szWord && mask.Match(szWord);
			if (bMatch)
			{
				FillWildMaskRefValues(szWord, &mask);
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

		WildMask mask(szMask, m_pRefValues != NULL);
		bMatch = mask.Match(szStrValue);

		if (bMatch)
		{
			FillWildMaskRefValues(szStrValue, &mask);
		}

		free(szMask);
	}

	return bMatch;
}

bool FeedFilter::Term::MatchRegex(const char* szStrValue)
{
	if (!m_pRegEx)
	{
		m_pRegEx = new RegEx(m_szParam, m_pRefValues == NULL ? 0 : 100);
	}

	bool bFound = m_pRegEx->Match(szStrValue);
	if (bFound)
	{
		FillRegExRefValues(szStrValue, m_pRegEx);
	}
	return bFound;
}

bool FeedFilter::Term::Compile(char* szToken)
{
	debug("Token: %s", szToken);

	char ch = szToken[0];

	m_bPositive = ch != '-';
	if (ch == '-' || ch == '+')
	{
		szToken++;
		ch = szToken[0];
	}

	char ch2= szToken[1];
	if ((ch == '(' || ch == ')' || ch == '|') && (ch2 == ' ' || ch2 == '\0'))
	{
		switch (ch)
		{
			case '(':
				m_eCommand = fcOpeningBrace;
				return true;
			case ')':
				m_eCommand = fcClosingBrace;
				return true;
			case '|':
				m_eCommand = fcOrOperator;
				return true;
		}
	}

	char *szField = NULL;
	m_eCommand = fcText;

	char* szColon = NULL;
	if (ch != '@' && ch != '$' && ch != '<' && ch != '>' && ch != '=')
	{
		szColon = strchr(szToken, ':');
	}
	if (szColon)
	{
		szField = szToken;
		szColon[0] = '\0';
		szToken = szColon + 1;
		ch = szToken[0];
	}

	if (ch == '\0')
	{
		return false;
	}

	ch2= szToken[1];

	if (ch == '@')
	{
		m_eCommand = fcText;
		szToken++;
	}
	else if (ch == '$')
	{
		m_eCommand = fcRegex;
		szToken++;
	}
	else if (ch == '=')
	{
		m_eCommand = fcEqual;
		szToken++;
	}
	else if (ch == '<' && ch2 == '=')
	{
		m_eCommand = fcLessEqual;
		szToken += 2;
	}
	else if (ch == '>' && ch2 == '=')
	{
		m_eCommand = fcGreaterEqual;
		szToken += 2;
	}
	else if (ch == '<')
	{
		m_eCommand = fcLess;
		szToken++;
	}
	else if (ch == '>')
	{
		m_eCommand = fcGreater;
		szToken++;
	}

	debug("%s, Field: %s, Command: %i, Param: %s", (m_bPositive ? "Positive" : "Negative"), szField, m_eCommand, szToken);

	const char* szStrValue;
	long long iIntValue;
	EFieldType eFieldType;
	if (!GetFieldData(szField, NULL, &eFieldType, &szStrValue, &iIntValue) ||
		(m_eCommand < fcEqual && eFieldType != ftString) ||
		(m_eCommand >= fcEqual && eFieldType != ftNumeric))
	{
		return false;
	}

	if ((szField && !strcasecmp(szField, "size") && !ParseSizeParam(szToken, &m_iIntParam)) ||
		(szField && !strcasecmp(szField, "age") && !ParseAgeParam(szToken, &m_iIntParam)) ||
		(szField && !strcasecmp(szField, "rating") && !ParseRatingParam(szToken, &m_iIntParam)) ||
		(szField && !strcasecmp(szField, "imdbid") && !ParseIntParam(szToken, &m_iIntParam)) ||
		(szField && !strcasecmp(szField, "rageid") && !ParseIntParam(szToken, &m_iIntParam)) ||
		(szField && !strcasecmp(szField, "season") && !ParseIntParam(szToken, &m_iIntParam)) ||
		(szField && !strcasecmp(szField, "episode") && !ParseIntParam(szToken, &m_iIntParam)) ||
		(szField && !strcasecmp(szField, "dupescore") && !ParseIntParam(szToken, &m_iIntParam)) ||
		(szField && !strcasecmp(szField, "priority") && !ParseIntParam(szToken, &m_iIntParam)))
	{
		return false;
	}

	m_szField = szField ? strdup(szField) : NULL;
	m_szParam = strdup(szToken);

	return true;
}

/*
 * If pFeedItemInfo is NULL, only field type info is returned
 */
bool FeedFilter::Term::GetFieldData(const char* szField, FeedItemInfo* pFeedItemInfo,
	EFieldType* FieldType, const char** StrValue, long long* IntValue)
{
	*StrValue = NULL;
	*IntValue = 0;

	if (!szField || !strcasecmp(szField, "title"))
	{
		*StrValue = pFeedItemInfo ? pFeedItemInfo->GetTitle() : NULL;
		*FieldType = ftString;
		return true;
	}
	else if (!strcasecmp(szField, "filename"))
	{
		*StrValue = pFeedItemInfo ? pFeedItemInfo->GetFilename() : NULL;
		*FieldType = ftString;
		return true;
	}
	else if (!strcasecmp(szField, "category"))
	{
		*StrValue = pFeedItemInfo ? pFeedItemInfo->GetCategory() : NULL;
		*FieldType = ftString;
		return true;
	}
	else if (!strcasecmp(szField, "link") || !strcasecmp(szField, "url"))
	{
		*StrValue = pFeedItemInfo ? pFeedItemInfo->GetUrl() : NULL;
		*FieldType = ftString;
		return true;
	}
	else if (!strcasecmp(szField, "genre"))
	{
		*StrValue = pFeedItemInfo ? pFeedItemInfo->GetGenre() : NULL;
		*FieldType = ftString;
		return true;
	}
	else if (!strcasecmp(szField, "size"))
	{
		*IntValue = pFeedItemInfo ? pFeedItemInfo->GetSize() : 0;
		*FieldType = ftNumeric;
		return true;
	}
	else if (!strcasecmp(szField, "age"))
	{
		*IntValue = pFeedItemInfo ? time(NULL) - pFeedItemInfo->GetTime() : 0;
		*FieldType = ftNumeric;
		return true;
	}
	else if (!strcasecmp(szField, "rating"))
	{
		*IntValue = pFeedItemInfo ? pFeedItemInfo->GetRating() : 0;
		*FieldType = ftNumeric;
		return true;
	}
	else if (!strcasecmp(szField, "imdbid"))
	{
		*IntValue = pFeedItemInfo ? pFeedItemInfo->GetImdbId() : 0;
		*FieldType = ftNumeric;
		return true;
	}
	else if (!strcasecmp(szField, "rageid"))
	{
		*IntValue = pFeedItemInfo ? pFeedItemInfo->GetRageId() : 0;
		*FieldType = ftNumeric;
		return true;
	}
	else if (!strcasecmp(szField, "season"))
	{
		*IntValue = pFeedItemInfo ? pFeedItemInfo->GetSeasonNum() : 0;
		*FieldType = ftNumeric;
		return true;
	}
	else if (!strcasecmp(szField, "episode"))
	{
		*IntValue = pFeedItemInfo ? pFeedItemInfo->GetEpisodeNum() : 0;
		*FieldType = ftNumeric;
		return true;
	}
	else if (!strcasecmp(szField, "priority"))
	{
		*IntValue = pFeedItemInfo ? pFeedItemInfo->GetPriority() : 0;
		*FieldType = ftNumeric;
		return true;
	}
	else if (!strcasecmp(szField, "dupekey"))
	{
		*StrValue = pFeedItemInfo ? pFeedItemInfo->GetDupeKey() : NULL;
		*FieldType = ftString;
		return true;
	}
	else if (!strcasecmp(szField, "dupescore"))
	{
		*IntValue = pFeedItemInfo ? pFeedItemInfo->GetDupeScore() : 0;
		*FieldType = ftNumeric;
		return true;
	}

	return false;
}

bool FeedFilter::Term::ParseSizeParam(const char* szParam, long long* pIntValue)
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

bool FeedFilter::Term::ParseAgeParam(const char* szParam, long long* pIntValue)
{
	*pIntValue = atoll(szParam);

	const char* p;
	for (p = szParam; *p && (*p >= '0' && *p <='9'); p++) ;
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

bool FeedFilter::Term::ParseRatingParam(const char* szParam, long long* pIntValue)
{
	*pIntValue = 0;
	
	double fParam = atof(szParam);
	
	const char* p;
	for (p = szParam; *p && ((*p >= '0' && *p <='9') || *p == '.'); p++) ;
	if (*p)
	{
		return false;
	}
	else if (strchr(szParam, '.'))
	{
		*pIntValue = (long long)(fParam * 10);
	}
	else
	{
		*pIntValue = (long long)fParam;
	}
	
	return true;
}

bool FeedFilter::Term::ParseIntParam(const char* szParam, long long* pIntValue)
{
	*pIntValue = atoi(szParam);
	
	const char* p;
	for (p = szParam; *p && *p >= '0' && *p <='9'; p++) ;
	if (*p)
	{
		return false;
	}
	
	return true;
}

void FeedFilter::Term::FillWildMaskRefValues(const char* szStrValue, WildMask* pMask)
{
	if (!m_pRefValues)
	{
		return;
	}

	for (int i = 0; i < pMask->GetMatchCount(); i++)
	{
		int iLen = pMask->GetMatchLen(i);
		char* szValue = (char*)malloc(iLen + 1);
		strncpy(szValue, szStrValue + pMask->GetMatchStart(i), iLen);
		szValue[iLen] = '\0';

		m_pRefValues->push_back(szValue);
	}
}

void FeedFilter::Term::FillRegExRefValues(const char* szStrValue, RegEx* pRegEx)
{
	if (!m_pRefValues)
	{
		return;
	}

	for (int i = 1; i < pRegEx->GetMatchCount(); i++)
	{
		int iLen = pRegEx->GetMatchLen(i);
		char* szValue = (char*)malloc(iLen + 1);
		strncpy(szValue, szStrValue + pRegEx->GetMatchStart(i), iLen);
		szValue[iLen] = '\0';

		m_pRefValues->push_back(szValue);
	}
}


FeedFilter::Rule::Rule()
{
	m_eCommand = frAccept;
	m_bIsValid = false;
	m_szCategory = NULL;
	m_iPriority = 0;
	m_iAddPriority = 0;
	m_bPause = false;
	m_szDupeKey = NULL;
	m_szAddDupeKey = NULL;
	m_iDupeScore = 0;
	m_iAddDupeScore = 0;
	m_eDupeMode = dmScore;
	m_bHasCategory = false;
	m_bHasPriority = false;
	m_bHasAddPriority = false;
	m_bHasPause = false;
	m_bHasDupeScore = false;
	m_bHasAddDupeScore = false;
	m_bHasDupeKey = false;
	m_bHasAddDupeKey = false;
	m_bHasDupeMode = false;
	m_bPatCategory = false;
	m_bPatDupeKey = false;
	m_bPatAddDupeKey = false;
	m_szPatCategory = NULL;
	m_szPatDupeKey = NULL;
	m_szPatAddDupeKey = NULL;
}

FeedFilter::Rule::~Rule()
{
	if (m_szCategory)
	{
		free(m_szCategory);
	}
	if (m_szDupeKey)
	{
		free(m_szDupeKey);
	}
	if (m_szAddDupeKey)
	{
		free(m_szAddDupeKey);
	}
	if (m_szPatCategory)
	{
		free(m_szPatCategory);
	}
	if (m_szPatDupeKey)
	{
		free(m_szPatDupeKey);
	}
	if (m_szPatAddDupeKey)
	{
		free(m_szPatAddDupeKey);
	}

	for (TermList::iterator it = m_Terms.begin(); it != m_Terms.end(); it++)
	{
		delete *it;
	}

	for (RefValues::iterator it = m_RefValues.begin(); it != m_RefValues.end(); it++)
	{
		delete *it;
	}
}

void FeedFilter::Rule::Compile(char* szRule)
{
	debug("Compiling rule: %s", szRule);

	m_bIsValid = true;

	char* szFilter3 = Util::Trim(szRule);

	char* szTerm = CompileCommand(szFilter3);
	if (!szTerm)
	{
		m_bIsValid = false;
		return;
	}
	if (m_eCommand == frComment)
	{
		return;
	}

	szTerm = Util::Trim(szTerm);

	for (char* p = szTerm; *p && m_bIsValid; p++)
	{
		char ch = *p;
		if (ch == ' ')
		{
			*p = '\0';
			m_bIsValid = CompileTerm(szTerm);
			szTerm = p + 1;
			while (*szTerm == ' ') szTerm++;
		}
	}

	m_bIsValid = m_bIsValid && CompileTerm(szTerm);

	if (m_bIsValid && m_bPatCategory)
	{
		m_szPatCategory = m_szCategory;
		m_szCategory = NULL;
	}
	if (m_bIsValid && m_bPatDupeKey)
	{
		m_szPatDupeKey = m_szDupeKey;
		m_szDupeKey = NULL;
	}
	if (m_bIsValid && m_bPatAddDupeKey)
	{
		m_szPatAddDupeKey = m_szAddDupeKey;
		m_szAddDupeKey = NULL;
	}
}

/* Checks if the rule starts with command and compiles it.
 * Returns a pointer to the next (first) term or NULL in a case of compilation error.
 */
char* FeedFilter::Rule::CompileCommand(char* szRule)
{
	if (!strncasecmp(szRule, "A:", 2) || !strncasecmp(szRule, "Accept:", 7) ||
		!strncasecmp(szRule, "A(", 2) || !strncasecmp(szRule, "Accept(", 7))
	{
		m_eCommand = frAccept;
		szRule += szRule[1] == ':' || szRule[1] == '(' ? 2 : 7;
	}
	else if (!strncasecmp(szRule, "O(", 2) || !strncasecmp(szRule, "Options(", 8))
	{
		m_eCommand = frOptions;
		szRule += szRule[1] == ':' || szRule[1] == '(' ? 2 : 8;
	}
	else if (!strncasecmp(szRule, "R:", 2) || !strncasecmp(szRule, "Reject:", 7))
	{
		m_eCommand = frReject;
		szRule += szRule[1] == ':' || szRule[1] == '(' ? 2 : 7;
	}
	else if (!strncasecmp(szRule, "Q:", 2) || !strncasecmp(szRule, "Require:", 8))
	{
		m_eCommand = frRequire;
		szRule += szRule[1] == ':' || szRule[1] == '(' ? 2 : 8;
	}
	else if (*szRule == '#')
	{
		m_eCommand = frComment;
		return szRule;
	}
	else
	{
		// not a command
		return szRule;
	}

	if ((m_eCommand == frAccept || m_eCommand == frOptions) && szRule[-1] == '(')
	{
		szRule = CompileOptions(szRule);
	}

	return szRule;
}

char* FeedFilter::Rule::CompileOptions(char* szRule)
{
	char* p = strchr(szRule, ')');
	if (!p)
	{
		// error
		return NULL;
	}

	// split command into tokens
	*p = '\0';
	char* saveptr;
	char* szToken = strtok_r(szRule, ",", &saveptr);
	while (szToken)
	{
		szToken = Util::Trim(szToken);
		if (*szToken)
		{
			char* szOption = szToken;
			const char* szValue = "";
			char* szColon = strchr(szToken, ':');
			if (szColon)
			{
				*szColon = '\0';
				szValue = Util::Trim(szColon + 1);
			}

			if (!strcasecmp(szOption, "category") || !strcasecmp(szOption, "cat") || !strcasecmp(szOption, "c"))
			{
				m_bHasCategory = true;
				if (m_szCategory)
				{
					free(m_szCategory);
				}
				m_szCategory = strdup(szValue);
				m_bPatCategory = strstr(szValue, "${");
			}
			else if (!strcasecmp(szOption, "pause") || !strcasecmp(szOption, "p"))
			{
				m_bHasPause = true;
				m_bPause = !*szValue || !strcasecmp(szValue, "yes") || !strcasecmp(szValue, "y");
				if (!m_bPause && !(!strcasecmp(szValue, "no") || !strcasecmp(szValue, "n")))
				{
					// error
					return NULL;
				}
			}
			else if (!strcasecmp(szOption, "priority") || !strcasecmp(szOption, "pr") || !strcasecmp(szOption, "r"))
			{
				if (!strchr("0123456789-+", *szValue))
				{
					// error
					return NULL;
				}
				m_bHasPriority = true;
				m_iPriority = atoi(szValue);
			}
			else if (!strcasecmp(szOption, "priority+") || !strcasecmp(szOption, "pr+") || !strcasecmp(szOption, "r+"))
			{
				if (!strchr("0123456789-+", *szValue))
				{
					// error
					return NULL;
				}
				m_bHasAddPriority = true;
				m_iAddPriority = atoi(szValue);
			}
			else if (!strcasecmp(szOption, "dupescore") || !strcasecmp(szOption, "ds") || !strcasecmp(szOption, "s"))
			{
				if (!strchr("0123456789-+", *szValue))
				{
					// error
					return NULL;
				}
				m_bHasDupeScore = true;
				m_iDupeScore = atoi(szValue);
			}
			else if (!strcasecmp(szOption, "dupescore+") || !strcasecmp(szOption, "ds+") || !strcasecmp(szOption, "s+"))
			{
				if (!strchr("0123456789-+", *szValue))
				{
					// error
					return NULL;
				}
				m_bHasAddDupeScore = true;
				m_iAddDupeScore = atoi(szValue);
			}
			else if (!strcasecmp(szOption, "dupekey") || !strcasecmp(szOption, "dk") || !strcasecmp(szOption, "k"))
			{
				m_bHasDupeKey = true;
				if (m_szDupeKey)
				{
					free(m_szDupeKey);
				}
				m_szDupeKey = strdup(szValue);
				m_bPatDupeKey = strstr(szValue, "${");
			}
			else if (!strcasecmp(szOption, "dupekey+") || !strcasecmp(szOption, "dk+") || !strcasecmp(szOption, "k+"))
			{
				m_bHasAddDupeKey = true;
				if (m_szAddDupeKey)
				{
					free(m_szAddDupeKey);
				}
				m_szAddDupeKey = strdup(szValue);
				m_bPatAddDupeKey = strstr(szValue, "${");
			}
			else if (!strcasecmp(szOption, "dupemode") || !strcasecmp(szOption, "dm") || !strcasecmp(szOption, "m"))
			{
				m_bHasDupeMode = true;
				if (!strcasecmp(szValue, "score") || !strcasecmp(szValue, "s"))
				{
					m_eDupeMode = dmScore;
				}
				else if (!strcasecmp(szValue, "all") || !strcasecmp(szValue, "a"))
				{
					m_eDupeMode = dmAll;
				}
				else if (!strcasecmp(szValue, "force") || !strcasecmp(szValue, "f"))
				{
					m_eDupeMode = dmForce;
				}
				else
				{
					// error
					return NULL;
				}
			}

			// for compatibility with older version we support old commands too
			else if (!strcasecmp(szOption, "paused") || !strcasecmp(szOption, "unpaused"))
			{
				m_bHasPause = true;
				m_bPause = !strcasecmp(szOption, "paused");
			}
			else if (strchr("0123456789-+", *szOption))
			{
				m_bHasPriority = true;
				m_iPriority = atoi(szOption);
			}
			else
			{
				m_bHasCategory = true;
				if (m_szCategory)
				{
					free(m_szCategory);
				}
				m_szCategory = strdup(szOption);
			}
		}
		szToken = strtok_r(NULL, ",", &saveptr);
	}

	szRule = p + 1;
	if (*szRule == ':')
	{
		szRule++;
	}

	return szRule;
}

bool FeedFilter::Rule::CompileTerm(char* szTerm)
{
	Term* pTerm = new Term();
	pTerm->SetRefValues(m_bPatCategory || m_bPatDupeKey || m_bPatAddDupeKey ? &m_RefValues : NULL);
	if (pTerm->Compile(szTerm))
	{
		m_Terms.push_back(pTerm);
		return true;
	}
	else
	{
		delete pTerm;
		return false;
	}
}

bool FeedFilter::Rule::Match(FeedItemInfo* pFeedItemInfo)
{
	for (RefValues::iterator it = m_RefValues.begin(); it != m_RefValues.end(); it++)
	{
		delete *it;
	}
	m_RefValues.clear();

	if (!MatchExpression(pFeedItemInfo))
	{
		return false;
	}

	if (m_bPatCategory)
	{
		ExpandRefValues(pFeedItemInfo, &m_szCategory, m_szPatCategory);
	}
	if (m_bPatDupeKey)
	{
		ExpandRefValues(pFeedItemInfo, &m_szDupeKey, m_szPatDupeKey);
	}
	if (m_bPatAddDupeKey)
	{
		ExpandRefValues(pFeedItemInfo, &m_szAddDupeKey, m_szPatAddDupeKey);
	}

	return true;
}

bool FeedFilter::Rule::MatchExpression(FeedItemInfo* pFeedItemInfo)
{
	char* expr = (char*)malloc(m_Terms.size() + 1);

	int index = 0;
	for (TermList::iterator it = m_Terms.begin(); it != m_Terms.end(); it++, index++)
	{
		Term* pTerm = *it;
		switch (pTerm->GetCommand())
		{
			case fcOpeningBrace:
				expr[index] = '(';
				break;

			case fcClosingBrace:
				expr[index] = ')';
				break;

			case fcOrOperator:
				expr[index] = '|';
				break;

			default:
				expr[index] = pTerm->Match(pFeedItemInfo) ? 'T' : 'F';
				break;
		}
	}
	expr[index] = '\0';

	// reduce result tree to one element (may be longer if expression has syntax errors)
	for (int iOldLen = 0, iNewLen = strlen(expr); iNewLen != iOldLen; iOldLen = iNewLen, iNewLen = strlen(expr))
	{
		// NOTE: there are no operator priorities.
		// the order of operators "OR" and "AND" is not defined, they can be checked in any order.
		// "OR" and "AND" should not be mixed in one group; instead braces should be used to define priorities.
		ReduceExpr(expr, "TT", "T");
		ReduceExpr(expr, "TF", "F");
		ReduceExpr(expr, "FT", "F");
		ReduceExpr(expr, "FF", "F");
		ReduceExpr(expr, "||", "|");
		ReduceExpr(expr, "(|", "(");
		ReduceExpr(expr, "|)", ")");
		ReduceExpr(expr, "T|T", "T");
		ReduceExpr(expr, "T|F", "T");
		ReduceExpr(expr, "F|T", "T");
		ReduceExpr(expr, "F|F", "F");
		ReduceExpr(expr, "(T)", "T");
		ReduceExpr(expr, "(F)", "F");
	}

	bool bMatch = *expr && *expr == 'T' && expr[1] == '\0';
	free(expr);
	return bMatch;
}

void FeedFilter::Rule::ReduceExpr(char* szExpr, const char* szFrom, const char* szTo)
{
	int iLenFrom = strlen(szFrom);
	int iLenTo = strlen(szTo);
	// assert(iLenTo < iLenFrom);

	while (char* p = strstr(szExpr, szFrom))
	{
		strcpy(p, szTo);
		strcpy(p + iLenTo, p + iLenFrom);
	}

	return;
}

void FeedFilter::Rule::ExpandRefValues(FeedItemInfo* pFeedItemInfo, char** pDestStr, char* pPatStr)
{
	if (*pDestStr)
	{
		free(*pDestStr);
	}

	*pDestStr = strdup(pPatStr);
	char* curvalue = *pDestStr;

	int iAttempts = 0;
	while (char* dollar = strstr(curvalue, "${"))
	{
		iAttempts++;
		if (iAttempts > 100)
		{
			break; // error
		}

		char* end = strchr(dollar, '}');
		if (!end)
		{
			break; // error
		}

		int varlen = (int)(end - dollar - 2);
		char variable[101];
		int maxlen = varlen < 100 ? varlen : 100;
		strncpy(variable, dollar + 2, maxlen);
		variable[maxlen] = '\0';

		const char* varvalue = GetRefValue(pFeedItemInfo, variable);
		if (!varvalue)
		{
			break; // error
		}

		int newlen = strlen(varvalue);
		char* newvalue = (char*)malloc(strlen(curvalue) - varlen - 3 + newlen + 1);
		strncpy(newvalue, curvalue, dollar - curvalue);
		strncpy(newvalue + (dollar - curvalue), varvalue, newlen);
		strcpy(newvalue + (dollar - curvalue) + newlen, end + 1);
		free(curvalue);
		curvalue = newvalue;
		*pDestStr = curvalue;
	}
}

const char* FeedFilter::Rule::GetRefValue(FeedItemInfo* pFeedItemInfo, const char* szVarName)
{
	if (!strcasecmp(szVarName, "season"))
	{
		pFeedItemInfo->GetSeasonNum(); // needed to parse title
		return pFeedItemInfo->GetSeason() ? pFeedItemInfo->GetSeason() : "";
	}
	else if (!strcasecmp(szVarName, "episode"))
	{
		pFeedItemInfo->GetEpisodeNum(); // needed to parse title
		return pFeedItemInfo->GetEpisode() ? pFeedItemInfo->GetEpisode() : "";
	}

	int iIndex = atoi(szVarName) - 1;
	if (iIndex >= 0 && iIndex < (int)m_RefValues.size())
	{
		return m_RefValues[iIndex];
	}

	return NULL;
}

FeedFilter::FeedFilter(const char* szFilter)
{
	Compile(szFilter);
}

FeedFilter::~FeedFilter()
{
	for (RuleList::iterator it = m_Rules.begin(); it != m_Rules.end(); it++)
	{
		delete *it;
	}
}

void FeedFilter::Compile(const char* szFilter)
{
	debug("Compiling filter: %s", szFilter);

	char* szFilter2 = strdup(szFilter);
	char* szRule = szFilter2;

	for (char* p = szRule; *p; p++)
	{
		char ch = *p;
		if (ch == '%')
		{
			*p = '\0';
			CompileRule(szRule);
			szRule = p + 1;
		}
	}

	CompileRule(szRule);

	free(szFilter2);
}

void FeedFilter::CompileRule(char* szRule)
{
	Rule* pRule = new Rule();
	m_Rules.push_back(pRule);
	pRule->Compile(szRule);
}

void FeedFilter::Match(FeedItemInfo* pFeedItemInfo)
{
	int index = 0;
	for (RuleList::iterator it = m_Rules.begin(); it != m_Rules.end(); it++)
	{
		Rule* pRule = *it;
		index++;
		if (pRule->IsValid())
		{
			bool bMatch = pRule->Match(pFeedItemInfo);
			switch (pRule->GetCommand())
			{
				case frAccept:
				case frOptions:
					if (bMatch)
					{
						pFeedItemInfo->SetMatchStatus(FeedItemInfo::msAccepted);
						pFeedItemInfo->SetMatchRule(index);
						ApplyOptions(pRule, pFeedItemInfo);
						if (pRule->GetCommand() == frAccept)
						{
							return;
						}
					}
					break;

				case frReject:
					if (bMatch)
					{
						pFeedItemInfo->SetMatchStatus(FeedItemInfo::msRejected);
						pFeedItemInfo->SetMatchRule(index);
						return;
					}
					break;

				case frRequire:
					if (!bMatch)
					{
						pFeedItemInfo->SetMatchStatus(FeedItemInfo::msRejected);
						pFeedItemInfo->SetMatchRule(index);
						return;
					}
					break;

				case frComment:
					break;
			}
		}
	}

	pFeedItemInfo->SetMatchStatus(FeedItemInfo::msIgnored);
	pFeedItemInfo->SetMatchRule(0);
}

void FeedFilter::ApplyOptions(Rule* pRule, FeedItemInfo* pFeedItemInfo)
{
	if (pRule->HasPause())
	{
		pFeedItemInfo->SetPauseNzb(pRule->GetPause());
	}
	if (pRule->HasCategory())
	{
		pFeedItemInfo->SetAddCategory(pRule->GetCategory());
	}
	if (pRule->HasPriority())
	{
		pFeedItemInfo->SetPriority(pRule->GetPriority());
	}
	if (pRule->HasAddPriority())
	{
		pFeedItemInfo->SetPriority(pFeedItemInfo->GetPriority() + pRule->GetAddPriority());
	}
	if (pRule->HasDupeScore())
	{
		pFeedItemInfo->SetDupeScore(pRule->GetDupeScore());
	}
	if (pRule->HasAddDupeScore())
	{
		pFeedItemInfo->SetDupeScore(pFeedItemInfo->GetDupeScore() + pRule->GetAddDupeScore());
	}
	if (pRule->HasDupeKey())
	{
		pFeedItemInfo->SetDupeKey(pRule->GetDupeKey());
	}
	if (pRule->HasAddDupeKey())
	{
		pFeedItemInfo->AppendDupeKey(pRule->GetAddDupeKey());
	}
	if (pRule->HasDupeMode())
	{
		pFeedItemInfo->SetDupeMode(pRule->GetDupeMode());
	}
}
