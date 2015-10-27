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


#ifndef FEEDFILE_H
#define FEEDFILE_H

#include <vector>

#include "FeedInfo.h"

class FeedFile
{
private:
	FeedItemInfos*		m_feedItemInfos;
	char*				m_fileName;

						FeedFile(const char* fileName);
	void				AddItem(FeedItemInfo* feedItemInfo);
	void				ParseSubject(FeedItemInfo* feedItemInfo);
#ifdef WIN32
    bool 				ParseFeed(IUnknown* nzb);
	static void			EncodeURL(const char* filename, char* url);
#else
	FeedItemInfo*		m_feedItemInfo;
	char*				m_tagContent;
	int					m_tagContentLen;
	bool				m_ignoreNextError;

	static void			SAX_StartElement(FeedFile* file, const char *name, const char **atts);
	static void			SAX_EndElement(FeedFile* file, const char *name);
	static void			SAX_characters(FeedFile* file, const char * xmlstr, int len);
	static void*		SAX_getEntity(FeedFile* file, const char * name);
	static void			SAX_error(FeedFile* file, const char *msg, ...);
	void				Parse_StartElement(const char *name, const char **atts);
	void				Parse_EndElement(const char *name);
	void				Parse_Content(const char *buf, int len);
	void				ResetTagContent();
#endif

public:
	virtual 			~FeedFile();
	static FeedFile*	Create(const char* fileName);
	FeedItemInfos*		GetFeedItemInfos() { return m_feedItemInfos; }

	void				LogDebugInfo();
};

#endif
