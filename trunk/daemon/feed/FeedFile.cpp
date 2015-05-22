/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2013-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "FeedFile.h"
#include "Log.h"
#include "DownloadInfo.h"
#include "Options.h"
#include "Util.h"

FeedFile::FeedFile(const char* szFileName)
{
    debug("Creating FeedFile");

    m_szFileName = strdup(szFileName);
	m_pFeedItemInfos = new FeedItemInfos();
	m_pFeedItemInfos->Retain();

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
	free(m_szFileName);
	m_pFeedItemInfos->Release();

#ifndef WIN32
	delete m_pFeedItemInfo;
	free(m_szTagContent);
#endif
}

void FeedFile::LogDebugInfo()
{
    info(" FeedFile %s", m_szFileName);
}

void FeedFile::AddItem(FeedItemInfo* pFeedItemInfo)
{
	m_pFeedItemInfos->Add(pFeedItemInfo);
}

void FeedFile::ParseSubject(FeedItemInfo* pFeedItemInfo)
{
	// if title has quatation marks we use only part within quatation marks 
	char* p = (char*)pFeedItemInfo->GetTitle();
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

				pFeedItemInfo->SetFilename(filename);
				free(filename);
				return;
			}
		}
	}

	pFeedItemInfo->SetFilename(pFeedItemInfo->GetTitle());
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
		if (!tag)
		{
			// bad rss feed
			return false;
		}
		_bstr_t title(tag->Gettext());
		pFeedItemInfo->SetTitle(title);
		ParseSubject(pFeedItemInfo);

		// <pubDate>Wed, 26 Jun 2013 00:02:54 -0600</pubDate>
		tag = node->selectSingleNode("pubDate");
		if (tag)
		{
			_bstr_t time(tag->Gettext());
			time_t unixtime = WebUtil::ParseRfc822DateTime(time);
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

		// <description>long text</description>
		tag = node->selectSingleNode("description");
		if (tag)
		{
			_bstr_t description(tag->Gettext());
			pFeedItemInfo->SetDescription(description);
		}

		//<enclosure url="http://myindexer.com/fetch/9eeb264aecce961a6e0d" length="150263340" type="application/x-nzb" />
		tag = node->selectSingleNode("enclosure");
		if (tag)
		{
			attr = tag->Getattributes()->getNamedItem("url");
			if (attr)
			{
				_bstr_t url(attr->Gettext());
				pFeedItemInfo->SetUrl(url);
			}

			attr = tag->Getattributes()->getNamedItem("length");
			if (attr)
			{
				_bstr_t size(attr->Gettext());
				long long lSize = atoll(size);
				pFeedItemInfo->SetSize(lSize);
			}
		}

		if (!pFeedItemInfo->GetUrl())
		{
			// <link>https://nzb.org/fetch/334534ce/4364564564</link>
			tag = node->selectSingleNode("link");
			if (!tag)
			{
				// bad rss feed
				return false;
			}
			_bstr_t link(tag->Gettext());
			pFeedItemInfo->SetUrl(link);
		}


		// newznab special

		//<newznab:attr name="size" value="5423523453534" />
		if (pFeedItemInfo->GetSize() == 0)
		{
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

		//<newznab:attr name="imdb" value="1588173"/>
		tag = node->selectSingleNode("newznab:attr[@name='imdb']");
		if (tag)
		{
			attr = tag->Getattributes()->getNamedItem("value");
			if (attr)
			{
				_bstr_t val(attr->Gettext());
				int iVal = atoi(val);
				pFeedItemInfo->SetImdbId(iVal);
			}
		}

		//<newznab:attr name="rageid" value="33877"/>
		tag = node->selectSingleNode("newznab:attr[@name='rageid']");
		if (tag)
		{
			attr = tag->Getattributes()->getNamedItem("value");
			if (attr)
			{
				_bstr_t val(attr->Gettext());
				int iVal = atoi(val);
				pFeedItemInfo->SetRageId(iVal);
			}
		}

		//<newznab:attr name="episode" value="E09"/>
		//<newznab:attr name="episode" value="9"/>
		tag = node->selectSingleNode("newznab:attr[@name='episode']");
		if (tag)
		{
			attr = tag->Getattributes()->getNamedItem("value");
			if (attr)
			{
				_bstr_t val(attr->Gettext());
				pFeedItemInfo->SetEpisode(val);
			}
		}

		//<newznab:attr name="season" value="S03"/>
		//<newznab:attr name="season" value="3"/>
		tag = node->selectSingleNode("newznab:attr[@name='season']");
		if (tag)
		{
			attr = tag->Getattributes()->getNamedItem("value");
			if (attr)
			{
				_bstr_t val(attr->Gettext());
				pFeedItemInfo->SetSeason(val);
			}
		}

		MSXML::IXMLDOMNodeListPtr itemList = node->selectNodes("newznab:attr");
		for (int i = 0; i < itemList->Getlength(); i++)
		{
			MSXML::IXMLDOMNodePtr node = itemList->Getitem(i);
			MSXML::IXMLDOMNodePtr name = node->Getattributes()->getNamedItem("name");
			MSXML::IXMLDOMNodePtr value = node->Getattributes()->getNamedItem("value");
			if (name && value)
			{
				_bstr_t name(name->Gettext());
				_bstr_t val(value->Gettext());
				pFeedItemInfo->GetAttributes()->Add(name, val);
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
	ResetTagContent();
	
	if (!strcmp("item", name))
	{
		delete m_pFeedItemInfo;
		m_pFeedItemInfo = new FeedItemInfo();
	}
	else if (!strcmp("enclosure", name) && m_pFeedItemInfo)
	{
		//<enclosure url="http://myindexer.com/fetch/9eeb264aecce961a6e0d" length="150263340" type="application/x-nzb" />
		for (; *atts; atts+=2)
		{
			if (!strcmp("url", atts[0]))
			{
				char* szUrl = strdup(atts[1]);
				WebUtil::XmlDecode(szUrl);
				m_pFeedItemInfo->SetUrl(szUrl);
				free(szUrl);
			}
			else if (!strcmp("length", atts[0]))
			{
				long long lSize = atoll(atts[1]);
				m_pFeedItemInfo->SetSize(lSize);
			}
		}
	}
	else if (m_pFeedItemInfo && !strcmp("newznab:attr", name) &&
		atts[0] && atts[1] && atts[2] && atts[3] &&
		!strcmp("name", atts[0]) && !strcmp("value", atts[2]))
	{
		m_pFeedItemInfo->GetAttributes()->Add(atts[1], atts[3]);

		//<newznab:attr name="size" value="5423523453534" />
		if (m_pFeedItemInfo->GetSize() == 0 &&
			!strcmp("size", atts[1]))
		{
			long long lSize = atoll(atts[3]);
			m_pFeedItemInfo->SetSize(lSize);
		}

		//<newznab:attr name="imdb" value="1588173"/>
		else if (!strcmp("imdb", atts[1]))
		{
			m_pFeedItemInfo->SetImdbId(atoi(atts[3]));
		}

		//<newznab:attr name="rageid" value="33877"/>
		else if (!strcmp("rageid", atts[1]))
		{
			m_pFeedItemInfo->SetRageId(atoi(atts[3]));
		}

		//<newznab:attr name="episode" value="E09"/>
		//<newznab:attr name="episode" value="9"/>
		else if (!strcmp("episode", atts[1]))
		{
			m_pFeedItemInfo->SetEpisode(atts[3]);
		}

		//<newznab:attr name="season" value="S03"/>
		//<newznab:attr name="season" value="3"/>
		else if (!strcmp("season", atts[1]))
		{
			m_pFeedItemInfo->SetSeason(atts[3]);
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
		m_pFeedItemInfo->SetTitle(m_szTagContent);
		ResetTagContent();
		ParseSubject(m_pFeedItemInfo);
	}
	else if (!strcmp("link", name) && m_pFeedItemInfo &&
		(!m_pFeedItemInfo->GetUrl() || strlen(m_pFeedItemInfo->GetUrl()) == 0))
	{
		m_pFeedItemInfo->SetUrl(m_szTagContent);
		ResetTagContent();
	}
	else if (!strcmp("category", name) && m_pFeedItemInfo)
	{
		m_pFeedItemInfo->SetCategory(m_szTagContent);
		ResetTagContent();
	}
	else if (!strcmp("description", name) && m_pFeedItemInfo)
	{
		m_pFeedItemInfo->SetDescription(m_szTagContent);
		ResetTagContent();
	}
	else if (!strcmp("pubDate", name) && m_pFeedItemInfo)
	{
		time_t unixtime = WebUtil::ParseRfc822DateTime(m_szTagContent);
		if (unixtime > 0)
		{
			m_pFeedItemInfo->SetTime(unixtime);
		}
		ResetTagContent();
	}
}

void FeedFile::Parse_Content(const char *buf, int len)
{
	m_szTagContent = (char*)realloc(m_szTagContent, m_iTagContentLen + len + 1);
	strncpy(m_szTagContent + m_iTagContentLen, buf, len);
	m_iTagContentLen += len;
	m_szTagContent[m_iTagContentLen] = '\0';
}

void FeedFile::ResetTagContent()
{
	free(m_szTagContent);
	m_szTagContent = NULL;
	m_iTagContentLen = 0;
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
