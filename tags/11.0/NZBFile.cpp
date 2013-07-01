/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include <string.h>
#include <list>
#ifdef WIN32
#include <comutil.h>
#import <msxml.tlb> named_guids 
using namespace MSXML;
#else
#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlerror.h>
#include <libxml/entities.h>
#endif

#include "nzbget.h"
#include "NZBFile.h"
#include "Log.h"
#include "DownloadInfo.h"
#include "Options.h"
#include "DiskState.h"
#include "Util.h"

extern Options* g_pOptions;
extern DiskState* g_pDiskState;

NZBFile::NZBFile(const char* szFileName, const char* szCategory)
{
    debug("Creating NZBFile");

    m_szFileName = strdup(szFileName);
	m_pNZBInfo = new NZBInfo();
	m_pNZBInfo->AddReference();
	m_pNZBInfo->SetFilename(szFileName);
	m_pNZBInfo->SetCategory(szCategory);
	m_pNZBInfo->BuildDestDirName();

#ifndef WIN32
	m_pFileInfo = NULL;
	m_pArticle = NULL;
	m_szTagContent = NULL;
	m_iTagContentLen = 0;
#endif

    m_FileInfos.clear();
}

NZBFile::~NZBFile()
{
    debug("Destroying NZBFile");

    // Cleanup
    if (m_szFileName)
    {
        free(m_szFileName);
    }

    for (FileInfos::iterator it = m_FileInfos.begin(); it != m_FileInfos.end(); it++)
    {
        delete *it;
    }
    m_FileInfos.clear();

	if (m_pNZBInfo)
	{
		m_pNZBInfo->Release();
	}

#ifndef WIN32
	if (m_pFileInfo)
	{
		delete m_pFileInfo;
	}
	
	if (m_szTagContent)
	{
		free(m_szTagContent);
	}
#endif
}

void NZBFile::LogDebugInfo()
{
    debug(" NZBFile %s", m_szFileName);
}

void NZBFile::DetachFileInfos()
{
    m_FileInfos.clear();
}

void NZBFile::AddArticle(FileInfo* pFileInfo, ArticleInfo* pArticleInfo)
{
	// make Article-List big enough
	while ((int)pFileInfo->GetArticles()->size() < pArticleInfo->GetPartNumber())
		pFileInfo->GetArticles()->push_back(NULL);

	(*pFileInfo->GetArticles())[pArticleInfo->GetPartNumber() - 1] = pArticleInfo;
}

void NZBFile::AddFileInfo(FileInfo* pFileInfo)
{
	// deleting empty articles
	FileInfo::Articles* pArticles = pFileInfo->GetArticles();
	int i = 0;
	for (FileInfo::Articles::iterator it = pArticles->begin(); it != pArticles->end();)
	{
		if (*it == NULL)
		{
			pArticles->erase(it);
			it = pArticles->begin() + i;
		}
		else
		{
			it++;
			i++;
		}
	}

	if (!pArticles->empty())
	{
		m_FileInfos.push_back(pFileInfo);
		pFileInfo->SetNZBInfo(m_pNZBInfo);
		m_pNZBInfo->SetSize(m_pNZBInfo->GetSize() + pFileInfo->GetSize());
		m_pNZBInfo->SetFileCount(m_pNZBInfo->GetFileCount() + 1);
	}
	else
	{
		delete pFileInfo; 
	}
}

void NZBFile::ParseSubject(FileInfo* pFileInfo, bool TryQuotes)
{
	if (TryQuotes)
	{
		// try to use the filename in quatation marks 
		char* p = (char*)pFileInfo->GetSubject();
		char* start = strchr(p, '\"');
		if (start)
		{
			start++;
			char* end = strchr(start + 1, '\"');
			if (end)
			{
				int len = (int)(end - start);
				char* point = strchr(start + 1, '.');
				if (point && point < end)
				{
					char* filename = (char*)malloc(len + 1);
					strncpy(filename, start, len);
					filename[len] = '\0';
					pFileInfo->SetFilename(filename);
					free(filename);
					return;
				}
			}
		}
	}

	// tokenize subject, considering spaces as separators and quotation 
	// marks as non separatable token delimiters.
	// then take the last token containing dot (".") as a filename

	typedef std::list<char*> TokenList;
	TokenList tokens;
	tokens.clear();

	// tokenizing
	char* p = (char*)pFileInfo->GetSubject();
	char* start = p;
	bool quot = false;
	while (true)
	{
		char ch = *p;
		bool sep = (ch == '\"') || (!quot && ch == ' ') || (ch == '\0');
		if (sep)
		{
			// end of token
			int len = (int)(p - start);
			if (len > 0)
			{
				char* token = (char*)malloc(len + 1);
				strncpy(token, start, len);
				token[len] = '\0';
				tokens.push_back(token);
			}
			start = p;
			if (ch != '\"' || quot)
			{
				start++;
			}
			quot = *start == '\"';
			if (quot)
			{
				start++;
				char* q = strchr(start, '\"');
				if (q)
				{
					p = q - 1;
				}
				else
				{
					quot = false;
				}
			}
		}
		if (ch == '\0')
		{
			break;
		}
		p++;
	}

	if (!tokens.empty())
	{
		// finding the best candidate for being a filename
		char* besttoken = tokens.back();
		for (TokenList::reverse_iterator it = tokens.rbegin(); it != tokens.rend(); it++)
		{
			char* s = *it;
			char* p = strchr(s, '.');
			if (p && (p[1] != '\0'))
			{
				besttoken = s;
				break;
			}
		}
		pFileInfo->SetFilename(besttoken);

		// free mem
		for (TokenList::iterator it = tokens.begin(); it != tokens.end(); it++)
		{
			free(*it);
		}
	}
	else
	{
		// subject is empty or contains only separators?
		debug("Could not extract Filename from Subject: %s. Using Subject as Filename", pFileInfo->GetSubject());
		pFileInfo->SetFilename(pFileInfo->GetSubject());
	}
}

bool NZBFile::HasDuplicateFilenames()
{
	for (FileInfos::iterator it = m_FileInfos.begin(); it != m_FileInfos.end(); it++)
    {
        FileInfo* pFileInfo1 = *it;
		int iDupe = 1;
		for (FileInfos::iterator it2 = it + 1; it2 != m_FileInfos.end(); it2++)
		{
			FileInfo* pFileInfo2 = *it2;
			if (!strcmp(pFileInfo1->GetFilename(), pFileInfo2->GetFilename()) &&
				strcmp(pFileInfo1->GetSubject(), pFileInfo2->GetSubject()))
			{
				iDupe++;
			}
		}

		// If more than two files have the same parsed filename but different subjects,
		// this means, that the parsing was not correct.
		// in this case we take subjects as filenames to prevent 
		// false "duplicate files"-alarm.
		// It's Ok for just two files to have the same filename, this is 
		// an often case by posting-errors to repost bad files
		if (iDupe > 2 || (iDupe == 2 && m_FileInfos.size() == 2))
		{
			return true;
		}
    }

	return false;
}

/**
 * Generate filenames from subjects and check if the parsing of subject was correct
 */
void NZBFile::ProcessFilenames()
{
	for (FileInfos::iterator it = m_FileInfos.begin(); it != m_FileInfos.end(); it++)
    {
        FileInfo* pFileInfo = *it;
		ParseSubject(pFileInfo, true);
	}

	if (HasDuplicateFilenames())
    {
		for (FileInfos::iterator it = m_FileInfos.begin(); it != m_FileInfos.end(); it++)
		{
			FileInfo* pFileInfo = *it;
			ParseSubject(pFileInfo, false);
		}
	}

	if (HasDuplicateFilenames())
    {
		for (FileInfos::iterator it = m_FileInfos.begin(); it != m_FileInfos.end(); it++)
		{
			FileInfo* pFileInfo = *it;
			pFileInfo->SetFilename(pFileInfo->GetSubject());
		}
    }

	for (FileInfos::iterator it = m_FileInfos.begin(); it != m_FileInfos.end(); it++)
    {
        FileInfo* pFileInfo = *it;
		pFileInfo->MakeValidFilename();
		if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
		{
			g_pDiskState->SaveFile(pFileInfo);
			pFileInfo->ClearArticles();
		}
	}
}

#ifdef WIN32
NZBFile* NZBFile::Create(const char* szFileName, const char* szCategory)
{
    CoInitialize(NULL);

	HRESULT hr;

	MSXML::IXMLDOMDocumentPtr doc;
	hr = doc.CreateInstance(MSXML::CLSID_DOMDocument);
    if (FAILED(hr))
    {
        return NULL;
    }

    // Load the XML document file...
	doc->put_resolveExternals(VARIANT_FALSE);
	doc->put_validateOnParse(VARIANT_FALSE);
	doc->put_async(VARIANT_FALSE);

	// filename needs to be properly encoded
	char* szURL = (char*)malloc(strlen(szFileName)*3 + 1);
	EncodeURL(szFileName, szURL);
	debug("url=\"%s\"", szURL);
	_variant_t v(szURL);
	free(szURL);

	VARIANT_BOOL success = doc->load(v);
	if (success == VARIANT_FALSE)
	{
		_bstr_t r(doc->GetparseError()->reason);
		const char* szErrMsg = r;
		error("Error parsing nzb-file: %s", szErrMsg);
		return NULL;
	}

    NZBFile* pFile = new NZBFile(szFileName, szCategory);
    if (pFile->ParseNZB(doc))
	{
		pFile->ProcessFilenames();
	}
	else
	{
		delete pFile;
		pFile = NULL;
	}

    return pFile;
}

void NZBFile::EncodeURL(const char* szFilename, char* szURL)
{
	while (char ch = *szFilename++)
	{
		if (('0' <= ch && ch <= '9') ||
			('a' <= ch && ch <= 'z') ||
			('A' <= ch && ch <= 'Z') )
		{
			*szURL++ = ch;
		}
		else
		{
			*szURL++ = '%';
			int a = ch >> 4;
			*szURL++ = a > 9 ? a - 10 + 'a' : a + '0';
			a = ch & 0xF;
			*szURL++ = a > 9 ? a - 10 + 'a' : a + '0';
		}
	}
	*szURL = NULL;
}

bool NZBFile::ParseNZB(IUnknown* nzb)
{
	MSXML::IXMLDOMDocumentPtr doc = nzb;
	MSXML::IXMLDOMNodePtr root = doc->documentElement;

	MSXML::IXMLDOMNodeListPtr fileList = root->selectNodes("/nzb/file");
	for (int i = 0; i < fileList->Getlength(); i++)
	{
		MSXML::IXMLDOMNodePtr node = fileList->Getitem(i);
		MSXML::IXMLDOMNodePtr attribute = node->Getattributes()->getNamedItem("subject");
		if (!attribute) return false;
		_bstr_t subject(attribute->Gettext());
        FileInfo* pFileInfo = new FileInfo();
		pFileInfo->SetSubject(subject);

		attribute = node->Getattributes()->getNamedItem("date");
		if (attribute)
		{
			_bstr_t date(attribute->Gettext());
			pFileInfo->SetTime(atoi(date));
		}

		MSXML::IXMLDOMNodeListPtr groupList = node->selectNodes("groups/group");
		for (int g = 0; g < groupList->Getlength(); g++)
		{
			MSXML::IXMLDOMNodePtr node = groupList->Getitem(g);
			_bstr_t group = node->Gettext();
			pFileInfo->GetGroups()->push_back(strdup((const char*)group));
		}

		MSXML::IXMLDOMNodeListPtr segmentList = node->selectNodes("segments/segment");
		for (int g = 0; g < segmentList->Getlength(); g++)
		{
			MSXML::IXMLDOMNodePtr node = segmentList->Getitem(g);
			_bstr_t id = node->Gettext();
            char szId[2048];
            snprintf(szId, 2048, "<%s>", (const char*)id);

			MSXML::IXMLDOMNodePtr attribute = node->Getattributes()->getNamedItem("number");
			if (!attribute) return false;
			_bstr_t number(attribute->Gettext());

			attribute = node->Getattributes()->getNamedItem("bytes");
			if (!attribute) return false;
			_bstr_t bytes(attribute->Gettext());

			int partNumber = atoi(number);
			int lsize = atoi(bytes);

			if (partNumber > 0)
			{
				ArticleInfo* pArticle = new ArticleInfo();
				pArticle->SetPartNumber(partNumber);
				pArticle->SetMessageID(szId);
				pArticle->SetSize(lsize);
				AddArticle(pFileInfo, pArticle);
			}

            if (lsize > 0)
            {
                pFileInfo->SetSize(pFileInfo->GetSize() + lsize);
            }
		}

		AddFileInfo(pFileInfo);
	}
	return true;
}

#else

NZBFile* NZBFile::Create(const char* szFileName, const char* szCategory)
{
    NZBFile* pFile = new NZBFile(szFileName, szCategory);

	xmlSAXHandler SAX_handler = {0};
	SAX_handler.startElement = reinterpret_cast<startElementSAXFunc>(SAX_StartElement);
	SAX_handler.endElement = reinterpret_cast<endElementSAXFunc>(SAX_EndElement);
	SAX_handler.characters = reinterpret_cast<charactersSAXFunc>(SAX_characters);
	SAX_handler.error = reinterpret_cast<errorSAXFunc>(SAX_error);
	SAX_handler.getEntity = reinterpret_cast<getEntitySAXFunc>(SAX_getEntity);

	pFile->m_bIgnoreNextError = false;

	int ret = xmlSAXUserParseFile(&SAX_handler, pFile, szFileName);
    
    if (ret == 0)
	{
		pFile->ProcessFilenames();
	}
	else
	{
        error("Failed to parse nzb-file");
		delete pFile;
		pFile = NULL;
	}
	
	return pFile;
}

void NZBFile::Parse_StartElement(const char *name, const char **atts)
{
	if (m_szTagContent)
	{
		free(m_szTagContent);
		m_szTagContent = NULL;
		m_iTagContentLen = 0;
	}
	
	if (!strcmp("file", name))
	{
		m_pFileInfo = new FileInfo();
		m_pFileInfo->SetFilename(m_szFileName);

    	for (int i = 0; atts[i]; i += 2)
    	{
    		const char* attrname = atts[i];
    		const char* attrvalue = atts[i + 1];
			if (!strcmp("subject", attrname))
			{
				m_pFileInfo->SetSubject(attrvalue);
			}
			if (!strcmp("date", attrname))
			{
				m_pFileInfo->SetTime(atoi(attrvalue));
			}
		}
	}
	else if (!strcmp("segment", name))
	{
		if (!m_pFileInfo)
		{
			// error: bad nzb-file
			return;
		}
	
		long long lsize = -1;
		int partNumber = -1;

    	for (int i = 0; atts[i]; i += 2)
    	{
    		const char* attrname = atts[i];
    		const char* attrvalue = atts[i + 1];
			if (!strcmp("bytes", attrname))
			{
				lsize = atol(attrvalue);
			}
			if (!strcmp("number", attrname))
			{
				partNumber = atol(attrvalue);
			}
		}
		if (lsize > 0)
		{
			m_pFileInfo->SetSize(m_pFileInfo->GetSize() + lsize);
		}

		if (partNumber > 0)
		{
			// new segment, add it!
			m_pArticle = new ArticleInfo();
			m_pArticle->SetPartNumber(partNumber);
			m_pArticle->SetSize(lsize);
			AddArticle(m_pFileInfo, m_pArticle);
		}
	}
}

void NZBFile::Parse_EndElement(const char *name)
{
	if (!strcmp("file", name))
	{
		// Close the file element, add the new file to file-list
		AddFileInfo(m_pFileInfo);
		m_pFileInfo = NULL;
		m_pArticle = NULL;
	}
	else if (!strcmp("group", name))
	{
		if (!m_pFileInfo)
		{
			// error: bad nzb-file
			return;
		}
		
		m_pFileInfo->GetGroups()->push_back(m_szTagContent);
		m_szTagContent = NULL;
		m_iTagContentLen = 0;
	}
	else if (!strcmp("segment", name))
	{
		if (!m_pFileInfo || !m_pArticle)
		{
			// error: bad nzb-file
			return;
		}

		// Get the #text part
		char ID[2048];
		snprintf(ID, 2048, "<%s>", m_szTagContent);
		m_pArticle->SetMessageID(ID);
		m_pArticle = NULL;
	}
}

void NZBFile::Parse_Content(const char *buf, int len)
{
	m_szTagContent = (char*)realloc(m_szTagContent, m_iTagContentLen + len + 1);
	strncpy(m_szTagContent + m_iTagContentLen, buf, len);
	m_iTagContentLen += len;
	m_szTagContent[m_iTagContentLen] = '\0';
}

void NZBFile::SAX_StartElement(NZBFile* pFile, const char *name, const char **atts)
{
	pFile->Parse_StartElement(name, atts);
}

void NZBFile::SAX_EndElement(NZBFile* pFile, const char *name)
{
	pFile->Parse_EndElement(name);
}

void NZBFile::SAX_characters(NZBFile* pFile, const char * xmlstr, int len)
{
	char* str = (char*)xmlstr;
	
	// trim starting blanks
	int off = 0;
	for (int i = 0; i < len; i++)
	{
		char ch = str[i];
		if (ch == ' ' || ch == 10 || ch == 13 || ch == 9)
		{
			off++;
		}
		else
		{
			break;
		}
	}
	
	int newlen = len - off;
	
	// trim ending blanks
	for (int i = len - 1; i >= off; i--)
	{
		char ch = str[i];
		if (ch == ' ' || ch == 10 || ch == 13 || ch == 9)
		{
			newlen--;
		}
		else
		{
			break;
		}
	}
	
	if (newlen > 0)
	{
		// interpret tag content
		pFile->Parse_Content(str + off, newlen);
	}
}

void* NZBFile::SAX_getEntity(NZBFile* pFile, const char * name)
{
	xmlEntityPtr e = xmlGetPredefinedEntity((xmlChar* )name);
	if (!e)
	{
		warn("entity not found");
		pFile->m_bIgnoreNextError = true;
	}

	return e;
}

void NZBFile::SAX_error(NZBFile* pFile, const char *msg, ...)
{
	if (pFile->m_bIgnoreNextError)
	{
		pFile->m_bIgnoreNextError = false;
		return;
	}
	
    va_list argp;
    va_start(argp, msg);
    char szErrMsg[1024];
    vsnprintf(szErrMsg, sizeof(szErrMsg), msg, argp);
    szErrMsg[1024-1] = '\0';
    va_end(argp);

	// remove trailing CRLF
	for (char* pend = szErrMsg + strlen(szErrMsg) - 1; pend >= szErrMsg && (*pend == '\n' || *pend == '\r' || *pend == ' '); pend--) *pend = '\0';
    error("Error parsing nzb-file: %s", szErrMsg);
}
#endif
