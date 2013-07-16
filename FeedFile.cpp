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
 * $Revision: 0 $
 * $Date: 2013-06-24 00:00:00 +0200 (Mo, 24 Jun 2013) $
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
#include "FeedFile.h"
#include "Log.h"
#include "DownloadInfo.h"
#include "Options.h"
#include "Util.h"

extern Options* g_pOptions;

FeedFile::FeedFile(const char* szFileName)
{
    debug("Creating FeedFile");

    m_szFileName = strdup(szFileName);

#ifndef WIN32
	m_pFeedItemInfo = NULL;
	m_szTagContent = NULL;
	m_iTagContentLen = 0;
#endif
}

FeedFile::~FeedFile()
{
    debug("Destroying FeedFile");

    // Cleanup
    if (m_szFileName)
    {
        free(m_szFileName);
    }

    for (FeedItemInfos::iterator it = m_FeedItemInfos.begin(); it != m_FeedItemInfos.end(); it++)
    {
        delete *it;
    }
    m_FeedItemInfos.clear();

#ifndef WIN32
	if (m_pFeedItemInfo)
	{
		delete m_pFeedItemInfo;
	}
	
	if (m_szTagContent)
	{
		free(m_szTagContent);
	}
#endif
}

void FeedFile::LogDebugInfo()
{
    debug(" FeedFile %s", m_szFileName);
}

void FeedFile::AddItem(FeedItemInfo* pFeedItemInfo)
{
	m_FeedItemInfos.push_back(pFeedItemInfo);
}

void FeedFile::DetachFeedItemInfos()
{
    m_FeedItemInfos.clear();
}

void FeedFile::ParseSubject(FeedItemInfo* pFeedItemInfo)
{
	// if title has quatation marks we use only part within quatation marks 
	char* p = (char*)pFeedItemInfo->GetName();
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

				char* ext = strrchr(filename, '.');
				if (ext && !strcasecmp(ext, ".par2"))
				{
					*ext = '\0';
				}

				pFeedItemInfo->SetName(filename);
				free(filename);
				return;
			}
		}
	}
}

#ifdef WIN32
FeedFile* FeedFile::Create(const char* szFileName)
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
		error("Error parsing rss feed: %s", szErrMsg);
		return NULL;
	}

    FeedFile* pFile = new FeedFile(szFileName);
    if (!pFile->ParseFeed(doc))
	{
		delete pFile;
		pFile = NULL;
	}

    return pFile;
}

void FeedFile::EncodeURL(const char* szFilename, char* szURL)
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

bool FeedFile::ParseFeed(IUnknown* nzb)
{
	MSXML::IXMLDOMDocumentPtr doc = nzb;
	MSXML::IXMLDOMNodePtr root = doc->documentElement;

	MSXML::IXMLDOMNodeListPtr itemList = root->selectNodes("/rss/channel/item");
	for (int i = 0; i < itemList->Getlength(); i++)
	{
		MSXML::IXMLDOMNodePtr node = itemList->Getitem(i);

		FeedItemInfo* pFeedItemInfo = new FeedItemInfo();
		AddItem(pFeedItemInfo);

		MSXML::IXMLDOMNodePtr tag;
		MSXML::IXMLDOMNodePtr attr;
		
		// <title>Debian 6</title> 
		tag = node->selectSingleNode("title");
		if (!tag) return false;
		_bstr_t title(tag->Gettext());
		pFeedItemInfo->SetName(title);
		ParseSubject(pFeedItemInfo);

		// <link>https://nzb.org/fetch/334534ce/4364564564</link>
		tag = node->selectSingleNode("link");
		if (!tag) return false;
		_bstr_t link(tag->Gettext());
		pFeedItemInfo->SetUrl(link);

		// <pubDate>Wed, 26 Jun 2013 00:02:54 -0600</pubDate>
		tag = node->selectSingleNode("pubDate");
		if (tag)
		{
			_bstr_t time(tag->Gettext());
			time_t unixtime = Util::ParseRfc822DateTime(time);
			if (unixtime > 0)
			{
				pFeedItemInfo->SetTime(unixtime);
			}
		}

		// <category>Movies &gt; HD</category>
		tag = node->selectSingleNode("category");
		if (tag)
		{
			_bstr_t category(tag->Gettext());
			pFeedItemInfo->SetCategory(category);
		}

		// newznab special

		//<newznab:attr name="size" value="5423523453534" />
		tag = node->selectSingleNode("newznab:attr[@name='size']");
		if (tag)
		{
			attr = tag->Getattributes()->getNamedItem("value");
			if (attr)
			{
				_bstr_t size(attr->Gettext());
				long long lSize = atoll(size);
				pFeedItemInfo->SetSize(lSize);
			}
		}

	}
	return true;
}

#else

FeedFile* FeedFile::Create(const char* szFileName)
{
    FeedFile* pFile = new FeedFile(szFileName);

	xmlSAXHandler SAX_handler = {0};
	SAX_handler.startElement = reinterpret_cast<startElementSAXFunc>(SAX_StartElement);
	SAX_handler.endElement = reinterpret_cast<endElementSAXFunc>(SAX_EndElement);
	SAX_handler.characters = reinterpret_cast<charactersSAXFunc>(SAX_characters);
	SAX_handler.error = reinterpret_cast<errorSAXFunc>(SAX_error);
	SAX_handler.getEntity = reinterpret_cast<getEntitySAXFunc>(SAX_getEntity);

	pFile->m_bIgnoreNextError = false;

	int ret = xmlSAXUserParseFile(&SAX_handler, pFile, szFileName);
    
    if (ret != 0)
	{
        error("Failed to parse rss feed");
		delete pFile;
		pFile = NULL;
	}
	
	return pFile;
}

void FeedFile::Parse_StartElement(const char *name, const char **atts)
{
	if (m_szTagContent)
	{
		free(m_szTagContent);
		m_szTagContent = NULL;
		m_iTagContentLen = 0;
	}
	
	if (!strcmp("item", name))
	{
		m_pFeedItemInfo = new FeedItemInfo();
	}
	else if (!strcmp("enclosure", name) && m_pFeedItemInfo)
	{
		//<enclosure url="http://myindexer.com/fetch/9eeb264aecce961a6e0d" length="150263340" type="application/x-nzb" />
		for (; *atts; atts+=2)
		{
			if (!strcmp("url", atts[0]))
			{
				m_pFeedItemInfo->SetUrl(atts[1]);
			}
			else if (!strcmp("length", atts[0]))
			{
				long long lSize = atoll(atts[1]);
				m_pFeedItemInfo->SetSize(lSize);
			}
		}
	}
	else if (!strcmp("newznab:attr", name) && m_pFeedItemInfo && m_pFeedItemInfo->GetSize() == 0)
	{
		//<newznab:attr name="size" value="5423523453534" />
		if (atts[0] && atts[1] && atts[2] && atts[3] &&
			!strcmp("name", atts[0]) && !strcmp("size", atts[1]) && !strcmp("value", atts[2]))
		{
			long long lSize = atoll(atts[3]);
			m_pFeedItemInfo->SetSize(lSize);
		}
	}
}

void FeedFile::Parse_EndElement(const char *name)
{
	if (!strcmp("item", name))
	{
		// Close the file element, add the new file to file-list
		AddItem(m_pFeedItemInfo);
		m_pFeedItemInfo = NULL;
	}
	else if (!strcmp("title", name) && m_pFeedItemInfo)
	{
		m_pFeedItemInfo->SetName(m_szTagContent);
		ParseSubject(m_pFeedItemInfo);
		m_szTagContent = NULL;
		m_iTagContentLen = 0;
	}
	else if (!strcmp("link", name) && m_pFeedItemInfo &&
		(!m_pFeedItemInfo->GetUrl() || strlen(m_pFeedItemInfo->GetUrl()) == 0))
	{
		m_pFeedItemInfo->SetUrl(m_szTagContent);
		m_szTagContent = NULL;
		m_iTagContentLen = 0;
	}
	else if (!strcmp("category", name) && m_pFeedItemInfo)
	{
		m_pFeedItemInfo->SetCategory(m_szTagContent);
		m_szTagContent = NULL;
		m_iTagContentLen = 0;
	}
	else if (!strcmp("pubDate", name) && m_pFeedItemInfo)
	{
		time_t unixtime = Util::ParseRfc822DateTime(m_szTagContent);
		if (unixtime > 0)
		{
			m_pFeedItemInfo->SetTime(unixtime);
		}
		
		m_szTagContent = NULL;
		m_iTagContentLen = 0;
	}
}

void FeedFile::Parse_Content(const char *buf, int len)
{
	m_szTagContent = (char*)realloc(m_szTagContent, m_iTagContentLen + len + 1);
	strncpy(m_szTagContent + m_iTagContentLen, buf, len);
	m_iTagContentLen += len;
	m_szTagContent[m_iTagContentLen] = '\0';
}

void FeedFile::SAX_StartElement(FeedFile* pFile, const char *name, const char **atts)
{
	pFile->Parse_StartElement(name, atts);
}

void FeedFile::SAX_EndElement(FeedFile* pFile, const char *name)
{
	pFile->Parse_EndElement(name);
}

void FeedFile::SAX_characters(FeedFile* pFile, const char * xmlstr, int len)
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

void* FeedFile::SAX_getEntity(FeedFile* pFile, const char * name)
{
	xmlEntityPtr e = xmlGetPredefinedEntity((xmlChar* )name);
	if (!e)
	{
		warn("entity not found");
		pFile->m_bIgnoreNextError = true;
	}

	return e;
}

void FeedFile::SAX_error(FeedFile* pFile, const char *msg, ...)
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
    error("Error parsing rss feed: %s", szErrMsg);
}
#endif
