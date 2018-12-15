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


#include "nzbget.h"
#include "Log.h"
#include "DownloadInfo.h"
#include "Util.h"
#include "FeedFilter.h"

bool FeedFilter::Term::Match(FeedItemInfo& feedItemInfo)
{
	const char* strValue = nullptr;
	int64 intValue = 0;

	if (!GetFieldData(m_field, &feedItemInfo, &strValue, &intValue))
	{
		return false;
	}

	bool match = MatchValue(strValue, intValue);

	if (m_positive != match)
	{
		return false;
	}

	return true;
}

bool FeedFilter::Term::MatchValue(const char* strValue, int64 intValue)
{
	double fFloatValue = (double)intValue;
	BString<100> intBuf;

	if (m_command < fcEqual && !strValue)
	{
		intBuf.Format("%" PRId64, intValue);
		strValue = intBuf;
	}

	else if (m_command >= fcEqual && strValue)
	{
		fFloatValue = atof(strValue);
		intValue = (int64)fFloatValue;
	}

	switch (m_command)
	{
		case fcText:
			return MatchText(strValue);

		case fcRegex:
			return MatchRegex(strValue);

		case fcEqual:
			return m_float ? fFloatValue == m_floatParam : intValue == m_intParam;

		case fcLess:
			return m_float ? fFloatValue < m_floatParam : intValue < m_intParam;

		case fcLessEqual:
			return m_float ? fFloatValue <= m_floatParam : intValue <= m_intParam;

		case fcGreater:
			return m_float ? fFloatValue > m_floatParam : intValue > m_intParam;

		case fcGreaterEqual:
			return m_float ? fFloatValue >= m_floatParam : intValue >= m_intParam;

		default:
			return false;
	}
}

bool FeedFilter::Term::MatchText(const char* strValue)
{
	const char* WORD_SEPARATORS = " !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

	// first check if we should make word-search or substring-search
	int paramLen = strlen(m_param);
	bool substr = paramLen >= 2 && m_param[0] == '*' && m_param[paramLen-1] == '*';
	if (!substr)
	{
		for (const char* p = m_param; *p; p++)
		{
			char ch = *p;
			if (strchr(WORD_SEPARATORS, ch) && ch != '*' && ch != '?' && ch != '#')
			{
				substr = true;
				break;
			}
		}
	}

	bool match = false;

	if (!substr)
	{
		// Word-search

		// split szStrValue into tokens
		Tokenizer tok(strValue, WORD_SEPARATORS);
		while (const char* word = tok.Next())
		{
			WildMask mask(m_param, m_refValues != nullptr);
			match = mask.Match(word);
			if (match)
			{
				FillWildMaskRefValues(word, &mask, 0);
				break;
			}
		}
	}
	else
	{
		// Substring-search

		int refOffset = 1;
		const char* format = "*%s*";
		if (paramLen >= 2 && m_param[0] == '*' && m_param[paramLen-1] == '*')
		{
			format = "%s";
			refOffset = 0;
		}
		else if (paramLen >= 1 && m_param[0] == '*')
		{
			format = "%s*";
			refOffset = 0;
		}
		else if (paramLen >= 1 && m_param[paramLen-1] == '*')
		{
			format = "*%s";
		}

		WildMask mask(CString::FormatStr(format, *m_param), m_refValues != nullptr);
		match = mask.Match(strValue);

		if (match)
		{
			FillWildMaskRefValues(strValue, &mask, refOffset);
		}
	}

	return match;
}

bool FeedFilter::Term::MatchRegex(const char* strValue)
{
	if (!m_regEx)
	{
		m_regEx = std::make_unique<RegEx>(m_param, m_refValues == nullptr ? 0 : 100);
	}

	bool found = m_regEx->Match(strValue);
	if (found)
	{
		FillRegExRefValues(strValue, m_regEx.get());
	}
	return found;
}

bool FeedFilter::Term::Compile(char* token)
{
	debug("Token: %s", token);

	char ch = token[0];

	m_positive = ch != '-';
	if (ch == '-' || ch == '+')
	{
		token++;
		ch = token[0];
	}

	char ch2= token[1];
	if ((ch == '(' || ch == ')' || ch == '|') && (ch2 == ' ' || ch2 == '\0'))
	{
		switch (ch)
		{
			case '(':
				m_command = fcOpeningBrace;
				return true;
			case ')':
				m_command = fcClosingBrace;
				return true;
			case '|':
				m_command = fcOrOperator;
				return true;
		}
	}

	char *field = nullptr;
	m_command = fcText;

	char* colon = nullptr;
	if (ch != '@' && ch != '$' && ch != '<' && ch != '>' && ch != '=')
	{
		colon = strchr(token, ':');
	}
	if (colon)
	{
		field = token;
		colon[0] = '\0';
		token = colon + 1;
		ch = token[0];
	}

	if (ch == '\0')
	{
		return false;
	}

	ch2= token[1];

	if (ch == '@')
	{
		m_command = fcText;
		token++;
	}
	else if (ch == '$')
	{
		m_command = fcRegex;
		token++;
	}
	else if (ch == '=')
	{
		m_command = fcEqual;
		token++;
	}
	else if (ch == '<' && ch2 == '=')
	{
		m_command = fcLessEqual;
		token += 2;
	}
	else if (ch == '>' && ch2 == '=')
	{
		m_command = fcGreaterEqual;
		token += 2;
	}
	else if (ch == '<')
	{
		m_command = fcLess;
		token++;
	}
	else if (ch == '>')
	{
		m_command = fcGreater;
		token++;
	}

	debug("%s, Field: %s, Command: %i, Param: %s", (m_positive ? "Positive" : "Negative"), field, m_command, token);

	const char* strValue;
	int64 intValue;
	if (!GetFieldData(field, nullptr, &strValue, &intValue))
	{
		return false;
	}

	if (field && !ParseParam(field, token))
	{
		return false;
	}

	m_field = field;
	m_param = token;

	return true;
}

/*
 * If pFeedItemInfo is nullptr, only field name is validated
 */
bool FeedFilter::Term::GetFieldData(const char* field, FeedItemInfo* feedItemInfo,
	const char** StrValue, int64* IntValue)
{
	*StrValue = nullptr;
	*IntValue = 0;

	if (!field || !strcasecmp(field, "title"))
	{
		*StrValue = feedItemInfo ? feedItemInfo->GetTitle() : nullptr;
		return true;
	}
	else if (!strcasecmp(field, "filename"))
	{
		*StrValue = feedItemInfo ? feedItemInfo->GetFilename() : nullptr;
		return true;
	}
	else if (!strcasecmp(field, "category"))
	{
		*StrValue = feedItemInfo ? feedItemInfo->GetCategory() : nullptr;
		return true;
	}
	else if (!strcasecmp(field, "link") || !strcasecmp(field, "url"))
	{
		*StrValue = feedItemInfo ? feedItemInfo->GetUrl() : nullptr;
		return true;
	}
	else if (!strcasecmp(field, "size"))
	{
		*IntValue = feedItemInfo ? feedItemInfo->GetSize() : 0;
		return true;
	}
	else if (!strcasecmp(field, "age"))
	{
		*IntValue = feedItemInfo ? Util::CurrentTime() - feedItemInfo->GetTime() : 0;
		return true;
	}
	else if (!strcasecmp(field, "imdbid"))
	{
		*IntValue = feedItemInfo ? feedItemInfo->GetImdbId() : 0;
		return true;
	}
	else if (!strcasecmp(field, "rageid"))
	{
		*IntValue = feedItemInfo ? feedItemInfo->GetRageId() : 0;
		return true;
	}
	else if (!strcasecmp(field, "tvdbid"))
	{
		*IntValue = feedItemInfo ? feedItemInfo->GetTvdbId() : 0;
		return true;
	}
	else if (!strcasecmp(field, "tvmazeid"))
	{
		*IntValue = feedItemInfo ? feedItemInfo->GetTvmazeId() : 0;
		return true;
	}
	else if (!strcasecmp(field, "description"))
	{
		*StrValue = feedItemInfo ? feedItemInfo->GetDescription() : nullptr;
		return true;
	}
	else if (!strcasecmp(field, "season"))
	{
		*IntValue = feedItemInfo ? feedItemInfo->GetSeasonNum() : 0;
		return true;
	}
	else if (!strcasecmp(field, "episode"))
	{
		*IntValue = feedItemInfo ? feedItemInfo->GetEpisodeNum() : 0;
		return true;
	}
	else if (!strcasecmp(field, "priority"))
	{
		*IntValue = feedItemInfo ? feedItemInfo->GetPriority() : 0;
		return true;
	}
	else if (!strcasecmp(field, "dupekey"))
	{
		*StrValue = feedItemInfo ? feedItemInfo->GetDupeKey() : nullptr;
		return true;
	}
	else if (!strcasecmp(field, "dupescore"))
	{
		*IntValue = feedItemInfo ? feedItemInfo->GetDupeScore() : 0;
		return true;
	}
	else if (!strcasecmp(field, "dupestatus"))
	{
		*StrValue = feedItemInfo ? feedItemInfo->GetDupeStatus() : nullptr;
		return true;
	}
	else if (!strncasecmp(field, "attr-", 5))
	{
		if (feedItemInfo)
		{
			FeedItemInfo::Attr* attr = feedItemInfo->GetAttributes()->Find(field + 5);
			*StrValue = attr ? attr->GetValue() : nullptr;
		}
		return true;
	}

	return false;
}

bool FeedFilter::Term::ParseParam(const char* field, const char* param)
{
	if (!strcasecmp(field, "size"))
	{
		return ParseSizeParam(param);
	}
	else if (!strcasecmp(field, "age"))
	{
		return ParseAgeParam(param);
	}
	else if (m_command >= fcEqual)
	{
		return ParseNumericParam(param);
	}

	return true;
}

bool FeedFilter::Term::ParseSizeParam(const char* param)
{
	double fParam = atof(param);

	const char* p;
	for (p = param; *p && ((*p >= '0' && *p <='9') || *p == '.'); p++) ;
	if (*p)
	{
		if (!strcasecmp(p, "K") || !strcasecmp(p, "KB"))
		{
			m_intParam = (int64)(fParam*1024);
		}
		else if (!strcasecmp(p, "M") || !strcasecmp(p, "MB"))
		{
			m_intParam = (int64)(fParam*1024*1024);
		}
		else if (!strcasecmp(p, "G") || !strcasecmp(p, "GB"))
		{
			m_intParam = (int64)(fParam*1024*1024*1024);
		}
		else
		{
			return false;
		}
	}
	else
	{
		m_intParam = (int64)fParam;
	}

	return true;
}

bool FeedFilter::Term::ParseAgeParam(const char* param)
{
	double fParam = atof(param);

	const char* p;
	for (p = param; *p && ((*p >= '0' && *p <='9') || *p == '.'); p++) ;
	if (*p)
	{
		if (!strcasecmp(p, "m"))
		{
			// minutes
			m_intParam = (int64)(fParam*60);
		}
		else if (!strcasecmp(p, "h"))
		{
			// hours
			m_intParam = (int64)(fParam*60*60);
		}
		else if (!strcasecmp(p, "d"))
		{
			// days
			m_intParam = (int64)(fParam*60*60*24);
		}
		else
		{
			return false;
		}
	}
	else
	{
		// days by default
		m_intParam = (int64)(fParam*60*60*24);
	}

	return true;
}

bool FeedFilter::Term::ParseNumericParam(const char* param)
{
	m_floatParam = atof(param);
	m_intParam = (int64)m_floatParam;
	m_float = strchr(param, '.');

	const char* p;
	for (p = param; *p && ((*p >= '0' && *p <='9') || *p == '.' || *p == '-') ; p++) ;
	if (*p)
	{
		return false;
	}

	return true;
}

void FeedFilter::Term::FillWildMaskRefValues(const char* strValue, WildMask* mask, int refOffset)
{
	if (!m_refValues)
	{
		return;
	}

	for (int i = refOffset; i < mask->GetMatchCount(); i++)
	{
		m_refValues->emplace_back(strValue + mask->GetMatchStart(i), mask->GetMatchLen(i));
	}
}

void FeedFilter::Term::FillRegExRefValues(const char* strValue, RegEx* regEx)
{
	if (!m_refValues)
	{
		return;
	}

	for (int i = 1; i < regEx->GetMatchCount(); i++)
	{
		m_refValues->emplace_back(strValue + regEx->GetMatchStart(i), regEx->GetMatchLen(i));
	}
}


void FeedFilter::Rule::Compile(char* rule)
{
	debug("Compiling rule: %s", rule);

	m_isValid = true;

	char* filter3 = Util::Trim(rule);

	char* term = CompileCommand(filter3);
	if (!term)
	{
		m_isValid = false;
		return;
	}
	if (m_command == frComment)
	{
		return;
	}

	term = Util::Trim(term);

	for (char* p = term; *p && m_isValid; p++)
	{
		char ch = *p;
		if (ch == ' ')
		{
			*p = '\0';
			m_isValid = CompileTerm(term);
			term = p + 1;
			while (*term == ' ') term++;
			p = term;
		}
	}

	m_isValid = m_isValid && CompileTerm(term);

	if (m_isValid && m_hasPatCategory)
	{
		m_patCategory.Bind(m_category.Unbind());
	}
	if (m_isValid && m_hasPatDupeKey)
	{
		m_patDupeKey.Bind(m_dupeKey.Unbind());
	}
	if (m_isValid && m_hasPatAddDupeKey)
	{
		m_patAddDupeKey.Bind(m_addDupeKey.Unbind());
	}
}

/* Checks if the rule starts with command and compiles it.
 * Returns a pointer to the next (first) term or nullptr in a case of compilation error.
 */
char* FeedFilter::Rule::CompileCommand(char* rule)
{
	if (!strncasecmp(rule, "A:", 2) || !strncasecmp(rule, "Accept:", 7) ||
		!strncasecmp(rule, "A(", 2) || !strncasecmp(rule, "Accept(", 7))
	{
		m_command = frAccept;
		rule += rule[1] == ':' || rule[1] == '(' ? 2 : 7;
	}
	else if (!strncasecmp(rule, "O(", 2) || !strncasecmp(rule, "Options(", 8))
	{
		m_command = frOptions;
		rule += rule[1] == ':' || rule[1] == '(' ? 2 : 8;
	}
	else if (!strncasecmp(rule, "R:", 2) || !strncasecmp(rule, "Reject:", 7))
	{
		m_command = frReject;
		rule += rule[1] == ':' || rule[1] == '(' ? 2 : 7;
	}
	else if (!strncasecmp(rule, "Q:", 2) || !strncasecmp(rule, "Require:", 8))
	{
		m_command = frRequire;
		rule += rule[1] == ':' || rule[1] == '(' ? 2 : 8;
	}
	else if (*rule == '#')
	{
		m_command = frComment;
		return rule;
	}
	else
	{
		// not a command
		return rule;
	}

	if ((m_command == frAccept || m_command == frOptions) && rule[-1] == '(')
	{
		rule = CompileOptions(rule);
	}

	return rule;
}

char* FeedFilter::Rule::CompileOptions(char* rule)
{
	char* p = strchr(rule, ')');
	if (!p)
	{
		// error
		return nullptr;
	}

	// split command into tokens
	*p = '\0';
	Tokenizer tok(rule, ",", true);
	while (char* option = tok.Next())
	{
		const char* value = "";
		char* colon = strchr(option, ':');
		if (colon)
		{
			*colon = '\0';
			value = Util::Trim(colon + 1);
		}

		if (!strcasecmp(option, "category") || !strcasecmp(option, "cat") || !strcasecmp(option, "c"))
		{
			m_hasCategory = true;
			m_category = value;
			m_hasPatCategory = strstr(value, "${");
		}
		else if (!strcasecmp(option, "pause") || !strcasecmp(option, "p"))
		{
			m_hasPause = true;
			m_pause = !*value || !strcasecmp(value, "yes") || !strcasecmp(value, "y");
			if (!m_pause && !(!strcasecmp(value, "no") || !strcasecmp(value, "n")))
			{
				// error
				return nullptr;
			}
		}
		else if (!strcasecmp(option, "priority") || !strcasecmp(option, "pr") || !strcasecmp(option, "r"))
		{
			if (!strchr("0123456789-+", *value))
			{
				// error
				return nullptr;
			}
			m_hasPriority = true;
			m_priority = atoi(value);
		}
		else if (!strcasecmp(option, "priority+") || !strcasecmp(option, "pr+") || !strcasecmp(option, "r+"))
		{
			if (!strchr("0123456789-+", *value))
			{
				// error
				return nullptr;
			}
			m_hasAddPriority = true;
			m_addPriority = atoi(value);
		}
		else if (!strcasecmp(option, "dupescore") || !strcasecmp(option, "ds") || !strcasecmp(option, "s"))
		{
			if (!strchr("0123456789-+", *value))
			{
				// error
				return nullptr;
			}
			m_hasDupeScore = true;
			m_dupeScore = atoi(value);
		}
		else if (!strcasecmp(option, "dupescore+") || !strcasecmp(option, "ds+") || !strcasecmp(option, "s+"))
		{
			if (!strchr("0123456789-+", *value))
			{
				// error
				return nullptr;
			}
			m_hasAddDupeScore = true;
			m_addDupeScore = atoi(value);
		}
		else if (!strcasecmp(option, "dupekey") || !strcasecmp(option, "dk") || !strcasecmp(option, "k"))
		{
			m_hasDupeKey = true;
			m_dupeKey = value;
			m_hasPatDupeKey = strstr(value, "${");
		}
		else if (!strcasecmp(option, "dupekey+") || !strcasecmp(option, "dk+") || !strcasecmp(option, "k+"))
		{
			m_hasAddDupeKey = true;
			m_addDupeKey = value;
			m_hasPatAddDupeKey = strstr(value, "${");
		}
		else if (!strcasecmp(option, "dupemode") || !strcasecmp(option, "dm") || !strcasecmp(option, "m"))
		{
			m_hasDupeMode = true;
			if (!strcasecmp(value, "score") || !strcasecmp(value, "s"))
			{
				m_dupeMode = dmScore;
			}
			else if (!strcasecmp(value, "all") || !strcasecmp(value, "a"))
			{
				m_dupeMode = dmAll;
			}
			else if (!strcasecmp(value, "force") || !strcasecmp(value, "f"))
			{
				m_dupeMode = dmForce;
			}
			else
			{
				// error
				return nullptr;
			}
		}
		else if (!strcasecmp(option, "rageid"))
		{
			m_hasRageId = true;
			m_rageId = value;
		}
		else if (!strcasecmp(option, "tvdbid"))
		{
			m_hasTvdbId = true;
			m_tvdbId = value;
		}
		else if (!strcasecmp(option, "tvmazeid"))
		{
			m_hasTvmazeId = true;
			m_tvmazeId = value;
		}
		else if (!strcasecmp(option, "series"))
		{
			m_hasSeries = true;
			m_series = value;
		}

		// for compatibility with older version we support old commands too
		else if (!strcasecmp(option, "paused") || !strcasecmp(option, "unpaused"))
		{
			m_hasPause = true;
			m_pause = !strcasecmp(option, "paused");
		}
		else if (strchr("0123456789-+", *option))
		{
			m_hasPriority = true;
			m_priority = atoi(option);
		}
		else
		{
			m_hasCategory = true;
			m_category = option;
		}
	}

	rule = p + 1;
	if (*rule == ':')
	{
		rule++;
	}

	return rule;
}

bool FeedFilter::Rule::CompileTerm(char* termstr)
{
	m_terms.emplace_back();
	m_terms.back().SetRefValues(m_hasPatCategory || m_hasPatDupeKey || m_hasPatAddDupeKey ? &m_refValues : nullptr);
	bool ok = m_terms.back().Compile(termstr);
	if (!ok)
	{
		m_terms.pop_back();
	}
	return ok;
}

bool FeedFilter::Rule::Match(FeedItemInfo& feedItemInfo)
{
	m_refValues.clear();

	if (!MatchExpression(feedItemInfo))
	{
		return false;
	}

	if (m_hasPatCategory)
	{
		ExpandRefValues(feedItemInfo, &m_category, m_patCategory);
	}
	if (m_hasPatDupeKey)
	{
		ExpandRefValues(feedItemInfo, &m_dupeKey, m_patDupeKey);
	}
	if (m_hasPatAddDupeKey)
	{
		ExpandRefValues(feedItemInfo, &m_addDupeKey, m_patAddDupeKey);
	}

	return true;
}

bool FeedFilter::Rule::MatchExpression(FeedItemInfo& feedItemInfo)
{
	CString expr;
	expr.Reserve(m_terms.size());

	int index = 0;
	for (Term& term : m_terms)
	{
		switch (term.GetCommand())
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
				expr[index] = term.Match(feedItemInfo) ? 'T' : 'F';
				break;
		}
		index++;
	}
	expr[index] = '\0';

	// reduce result tree to one element (may be longer if expression has syntax errors)
	for (int oldLen = 0, newLen = strlen(expr); newLen != oldLen; oldLen = newLen, newLen = strlen(expr))
	{
		// NOTE: there are no operator priorities.
		// the order of operators "OR" and "AND" is not defined, they can be checked in any order.
		// "OR" and "AND" should not be mixed in one group; instead braces should be used to define priorities.
		Util::ReduceStr(expr, "TT", "T");
		Util::ReduceStr(expr, "TF", "F");
		Util::ReduceStr(expr, "FT", "F");
		Util::ReduceStr(expr, "FF", "F");
		Util::ReduceStr(expr, "||", "|");
		Util::ReduceStr(expr, "(|", "(");
		Util::ReduceStr(expr, "|)", ")");
		Util::ReduceStr(expr, "T|T", "T");
		Util::ReduceStr(expr, "T|F", "T");
		Util::ReduceStr(expr, "F|T", "T");
		Util::ReduceStr(expr, "F|F", "F");
		Util::ReduceStr(expr, "(T)", "T");
		Util::ReduceStr(expr, "(F)", "F");
	}

	bool match = expr.Length() == 1 && expr[0] == 'T';
	return match;
}

void FeedFilter::Rule::ExpandRefValues(FeedItemInfo& feedItemInfo, CString* destStr, const char* patStr)
{
	CString curvalue = patStr;

	int attempts = 0;
	while (const char* dollar = strstr(curvalue, "${"))
	{
		attempts++;
		if (attempts > 100)
		{
			break; // error
		}

		const char* end = strchr(dollar, '}');
		if (!end)
		{
			break; // error
		}

		int varlen = (int)(end - dollar - 2);
		BString<100> variable;
		variable.Set(dollar + 2, varlen);
		const char* varvalue = GetRefValue(feedItemInfo, variable);
		if (!varvalue)
		{
			break; // error
		}

		curvalue.Replace((int)(dollar - curvalue), 2 + varlen + 1, varvalue);
	}

	*destStr = std::move(curvalue);
}

const char* FeedFilter::Rule::GetRefValue(FeedItemInfo& feedItemInfo, const char* varName)
{
	if (!strcasecmp(varName, "season"))
	{
		feedItemInfo.GetSeasonNum(); // needed to parse title
		return feedItemInfo.GetSeason() ? feedItemInfo.GetSeason() : "";
	}
	else if (!strcasecmp(varName, "episode"))
	{
		feedItemInfo.GetEpisodeNum(); // needed to parse title
		return feedItemInfo.GetEpisode() ? feedItemInfo.GetEpisode() : "";
	}

	int index = atoi(varName) - 1;
	if (index >= 0 && index < (int)m_refValues.size())
	{
		return m_refValues[index];
	}

	return nullptr;
}

FeedFilter::FeedFilter(const char* filter)
{
	Compile(filter);
}

void FeedFilter::Compile(const char* filter)
{
	debug("Compiling filter: %s", filter);

	CString filter2 = filter;
	char* rule = filter2;

	for (char* p = rule; *p; p++)
	{
		char ch = *p;
		if (ch == '%')
		{
			*p = '\0';
			CompileRule(rule);
			rule = p + 1;
		}
	}

	CompileRule(rule);
}

void FeedFilter::CompileRule(char* rulestr)
{
	m_rules.emplace_back();
	m_rules.back().Compile(rulestr);
}

void FeedFilter::Match(FeedItemInfo& feedItemInfo)
{
	int index = 0;
	for (Rule& rule : m_rules)
	{
		index++;
		if (rule.IsValid())
		{
			bool match = rule.Match(feedItemInfo);
			switch (rule.GetCommand())
			{
				case frAccept:
				case frOptions:
					if (match)
					{
						feedItemInfo.SetMatchStatus(FeedItemInfo::msAccepted);
						feedItemInfo.SetMatchRule(index);
						ApplyOptions(rule, feedItemInfo);
						if (rule.GetCommand() == frAccept)
						{
							return;
						}
					}
					break;

				case frReject:
					if (match)
					{
						feedItemInfo.SetMatchStatus(FeedItemInfo::msRejected);
						feedItemInfo.SetMatchRule(index);
						return;
					}
					break;

				case frRequire:
					if (!match)
					{
						feedItemInfo.SetMatchStatus(FeedItemInfo::msRejected);
						feedItemInfo.SetMatchRule(index);
						return;
					}
					break;

				case frComment:
					break;
			}
		}
	}

	feedItemInfo.SetMatchStatus(FeedItemInfo::msIgnored);
	feedItemInfo.SetMatchRule(0);
}

void FeedFilter::ApplyOptions(Rule& rule, FeedItemInfo& feedItemInfo)
{
	if (rule.HasPause())
	{
		feedItemInfo.SetPauseNzb(rule.GetPause());
	}
	if (rule.HasCategory())
	{
		feedItemInfo.SetAddCategory(rule.GetCategory());
	}
	if (rule.HasPriority())
	{
		feedItemInfo.SetPriority(rule.GetPriority());
	}
	if (rule.HasAddPriority())
	{
		feedItemInfo.SetPriority(feedItemInfo.GetPriority() + rule.GetAddPriority());
	}
	if (rule.HasDupeScore())
	{
		feedItemInfo.SetDupeScore(rule.GetDupeScore());
	}
	if (rule.HasAddDupeScore())
	{
		feedItemInfo.SetDupeScore(feedItemInfo.GetDupeScore() + rule.GetAddDupeScore());
	}
	if (rule.HasRageId() || rule.HasTvdbId() || rule.HasTvmazeId() || rule.HasSeries())
	{
		feedItemInfo.BuildDupeKey(rule.GetRageId(), rule.GetTvdbId(), rule.GetTvmazeId(), rule.GetSeries());
	}
	if (rule.HasDupeKey())
	{
		feedItemInfo.SetDupeKey(rule.GetDupeKey());
	}
	if (rule.HasAddDupeKey())
	{
		feedItemInfo.AppendDupeKey(rule.GetAddDupeKey());
	}
	if (rule.HasDupeMode())
	{
		feedItemInfo.SetDupeMode(rule.GetDupeMode());
	}
}
