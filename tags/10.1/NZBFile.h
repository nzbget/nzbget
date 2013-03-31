/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2010 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef NZBFILE_H
#define NZBFILE_H

#include <vector>

#include "DownloadInfo.h"

class NZBFile
{
public:
	typedef std::vector<FileInfo*>	FileInfos;

private:
	FileInfos			m_FileInfos;
	NZBInfo*			m_pNZBInfo;
	char*				m_szFileName;

						NZBFile(const char* szFileName, const char* szCategory);
	void				AddArticle(FileInfo* pFileInfo, ArticleInfo* pArticleInfo);
	void				AddFileInfo(FileInfo* pFileInfo);
	void				ParseSubject(FileInfo* pFileInfo, bool TryQuotes);
	void				ProcessFilenames();
	bool				HasDuplicateFilenames();
#ifdef WIN32
    bool 				ParseNZB(IUnknown* nzb);
	static void			EncodeURL(const char* szFilename, char* szURL);
#else
	FileInfo*			m_pFileInfo;
	ArticleInfo*		m_pArticle;
	char*				m_szTagContent;
	int					m_iTagContentLen;
	bool				m_bIgnoreNextError;

	static void			SAX_StartElement(NZBFile* pFile, const char *name, const char **atts);
	static void			SAX_EndElement(NZBFile* pFile, const char *name);
	static void			SAX_characters(NZBFile* pFile, const char * xmlstr, int len);
	static void*		SAX_getEntity(NZBFile* pFile, const char * name);
	static void			SAX_error(NZBFile* pFile, const char *msg, ...);
	void				Parse_StartElement(const char *name, const char **atts);
	void				Parse_EndElement(const char *name);
	void				Parse_Content(const char *buf, int len);
#endif
	static NZBFile*		Create(const char* szFileName, const char* szCategory, const char* szBuffer, int iSize, bool bFromBuffer);

public:
	virtual 			~NZBFile();
	static NZBFile*		CreateFromBuffer(const char* szFileName, const char* szCategory, const char* szBuffer, int iSize);
	static NZBFile*		CreateFromFile(const char* szFileName, const char* szCategory);
	const char* 		GetFileName() const { return m_szFileName; }
	FileInfos*			GetFileInfos() { return &m_FileInfos; }
	NZBInfo*			GetNZBInfo() { return m_pNZBInfo; }
	void				DetachFileInfos();

	void				LogDebugInfo();
};

#endif
