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

#include "DownloadInfo.h"

class FeedFile
{
private:
	FeedInfo*			m_pFeedInfo;
	FeedItemInfos		m_FeedItemInfos;
	char*				m_szFileName;

						FeedFile(const char* szFileName);
	void				AddItem(FeedItemInfo* pFeedItemInfo);
	void				ParseSubject(FeedItemInfo* pFeedItemInfo);
#ifdef WIN32
    bool 				ParseFeed(IUnknown* nzb);
	static void			EncodeURL(const char* szFilename, char* szURL);
#else
	FeedItemInfo*		m_pFeedItemInfo;
	char*				m_szTagContent;
	int					m_iTagContentLen;
	bool				m_bIgnoreNextError;

	static void			SAX_StartElement(FeedFile* pFile, const char *name, const char **atts);
	static void			SAX_EndElement(FeedFile* pFile, const char *name);
	static void			SAX_characters(FeedFile* pFile, const char * xmlstr, int len);
	static void*		SAX_getEntity(FeedFile* pFile, const char * name);
	static void			SAX_error(FeedFile* pFile, const char *msg, ...);
	void				Parse_StartElement(const char *name, const char **atts);
	void				Parse_EndElement(const char *name);
	void				Parse_Content(const char *buf, int len);
#endif

public:
	virtual 			~FeedFile();
	static FeedFile*	Create(const char* szFileName);
	FeedItemInfos*		GetFeedItemInfos() { return &m_FeedItemInfos; }
	void				DetachFeedItemInfos();

	void				LogDebugInfo();
};

#endif
