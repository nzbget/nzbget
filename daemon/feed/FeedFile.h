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


#ifndef FEEDFILE_H
#define FEEDFILE_H

#include "NString.h"
#include "FeedInfo.h"

class FeedFile
{
public:
	FeedFile(const char* fileName, const char* infoName);
	bool Parse();
	std::unique_ptr<FeedItemList> DetachFeedItems() { return std::move(m_feedItems); }

	void LogDebugInfo();

private:
	std::unique_ptr<FeedItemList> m_feedItems;
	CString m_fileName;
	CString m_infoName;

	void ParseSubject(FeedItemInfo& feedItemInfo);
#ifdef WIN32
	bool ParseFeed(IUnknown* nzb);
	static void EncodeUrl(const char* filename, char* url, int bufLen);
#else
	FeedItemInfo* m_feedItemInfo;
	StringBuilder m_tagContent;
	bool m_ignoreNextError;

	static void SAX_StartElement(FeedFile* file, const char *name, const char **atts);
	static void SAX_EndElement(FeedFile* file, const char *name);
	static void SAX_characters(FeedFile* file, const char *  xmlstr, int len);
	static void* SAX_getEntity(FeedFile* file, const char *  name);
	static void SAX_error(FeedFile* file, const char *msg, ...);
	void Parse_StartElement(const char *name, const char **atts);
	void Parse_EndElement(const char *name);
	void Parse_Content(const char *buf, int len);
	void ResetTagContent();
#endif
};

#endif
