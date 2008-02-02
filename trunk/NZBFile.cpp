/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2004  Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007  Andrei Prygounkov <hugbug@users.sourceforge.net>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Revision$
 * $Date$
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <string.h>
#include <list>
#ifdef WIN32
#include <comutil.h>
#import "MSXML.dll" named_guids 
using namespace MSXML;
#else
#include <libxml/parser.h>
#include <libxml/xmlreader.h>
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

NZBFile::NZBFile(const char* szFileName)
{
    debug("Creating NZBFile");

    m_szFileName = strdup(szFileName);

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
}

void NZBFile::LogDebugInfo()
{
    debug(" NZBFile %s", m_szFileName);
}

void NZBFile::DetachFileInfos()
{
    m_FileInfos.clear();
}

NZBFile* NZBFile::CreateFromBuffer(const char* szFileName, const char* szBuffer, int iSize)
{
	return Create(szFileName, szBuffer, iSize, true);
}

NZBFile* NZBFile::CreateFromFile(const char* szFileName)
{
	return Create(szFileName, NULL, 0, false);
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
		ParseSubject(pFileInfo);
		BuildDestDirName(pFileInfo);
		m_FileInfos.push_back(pFileInfo);

		if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
		{
			g_pDiskState->SaveFile(pFileInfo);
			pFileInfo->ClearArticles();
		}
	}
	else
	{
		delete pFileInfo; 
	}
}

void NZBFile::ParseSubject(FileInfo* pFileInfo)
{
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
			int len = p - start;
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

	pFileInfo->MakeValidFilename();
}

void NZBFile::BuildDestDirName(FileInfo* pFileInfo)
{
	char szBuffer[1024];

	if (g_pOptions->GetAppendNZBDir())
	{
		char szNiceNZBName[1024];
		pFileInfo->GetNiceNZBName(szNiceNZBName, 1024);
		snprintf(szBuffer, 1024, "%s%s", g_pOptions->GetDestDir(), szNiceNZBName);
		szBuffer[1024-1] = '\0';
	}
	else
	{
		strncpy(szBuffer, g_pOptions->GetDestDir(), 1024);
		szBuffer[1024-1] = '\0'; // trim the last slash, always returned by GetDestDir()
	}

	pFileInfo->SetDestDir(szBuffer);
}

/**
 * Check if the parsing of subject was correct
 */
void NZBFile::CheckFilenames()
{
    for (FileInfos::iterator it = m_FileInfos.begin(); it != m_FileInfos.end(); it++)
    {
        FileInfo* pFileInfo1 = *it;
		int iDupe = 0;
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
			for (FileInfos::iterator it2 = it; it2 != m_FileInfos.end(); it2++)
			{
				FileInfo* pFileInfo2 = *it2;
				pFileInfo2->SetFilename(pFileInfo2->GetSubject());
				pFileInfo2->MakeValidFilename();
			}
		}
    }
}

void NZBFile::UpdateGroupInfo()
{
	long long lSize = 0;
    for (FileInfos::iterator it = m_FileInfos.begin(); it != m_FileInfos.end(); it++)
    {
        FileInfo* pFileInfo = *it;
		lSize += pFileInfo->GetSize();
	}

    for (FileInfos::iterator it = m_FileInfos.begin(); it != m_FileInfos.end(); it++)
    {
        FileInfo* pFileInfo = *it;
		pFileInfo->SetNZBSize(lSize);
		pFileInfo->SetNZBFileCount(m_FileInfos.size());
		if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
		{
			g_pDiskState->LoadArticles(pFileInfo);
			g_pDiskState->SaveFile(pFileInfo);
			pFileInfo->ClearArticles();
		}
	}
}

#ifdef WIN32
NZBFile* NZBFile::Create(const char* szFileName, const char* szBuffer, int iSize, bool bFromBuffer)
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
	VARIANT_BOOL success;
	if (bFromBuffer)
	{
		success = doc->loadXML(szBuffer);
	}
	else
	{
		// filename needs to be properly encoded
		char* szURL = (char*)malloc(strlen(szFileName)*3 + 1);
		EncodeURL(szFileName, szURL);
		debug("url=\"%s\"", szURL);
		_variant_t v(szURL);
		free(szURL);
		success = doc->load(v);
	} 
	if (success == VARIANT_FALSE)
	{
		_bstr_t r(doc->GetparseError()->reason);
		const char* szErrMsg = r;
		error("Error parsing nzb-file: %s", szErrMsg);
		return NULL;
	}

    NZBFile* pFile = new NZBFile(szFileName);
    if (pFile->ParseNZB(doc))
	{
		pFile->CheckFilenames();
		pFile->UpdateGroupInfo();
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
        pFileInfo->SetNZBFilename(m_szFileName);
		pFileInfo->SetSubject(subject);

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

			ArticleInfo* pArticle = new ArticleInfo();
			pArticle->SetPartNumber(partNumber);
			pArticle->SetMessageID(szId);
			pArticle->SetSize(lsize);
			AddArticle(pFileInfo, pArticle);

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

NZBFile* NZBFile::Create(const char* szFileName, const char* szBuffer, int iSize, bool bFromBuffer)
{
    xmlTextReaderPtr doc;
	if (bFromBuffer)
	{
		doc = xmlReaderForMemory(szBuffer, iSize-1, "", NULL, 0);
	}
	else
	{
		doc = xmlReaderForFile(szFileName, NULL, 0);
	}
    if (!doc)
    {
        return NULL;
    }

    NZBFile* pFile = new NZBFile(szFileName);
    if (pFile->ParseNZB(doc))
	{
		pFile->CheckFilenames();
		pFile->UpdateGroupInfo();
	}
	else
	{
		delete pFile;
		pFile = NULL;
	}

    xmlFreeTextReader(doc);

    return pFile;
}

bool NZBFile::ParseNZB(void* nzb)
{
	FileInfo* pFileInfo = NULL;
	xmlTextReaderPtr node = (xmlTextReaderPtr)nzb;
    // walk through whole doc and search for segments-tags
    int ret = xmlTextReaderRead(node);
    while (ret == 1)
    {
        if (node)
        {
            xmlChar *name, *value;

            name = xmlTextReaderName(node);
            if (name == NULL)
            {
                name = xmlStrdup(BAD_CAST "--");
            }
            value = xmlTextReaderValue(node);

            if (xmlTextReaderNodeType(node) == 1)
            {
                if (!strcmp("file", (char*)name))
                {
                    pFileInfo = new FileInfo();
                    pFileInfo->SetNZBFilename(m_szFileName);

                    while (xmlTextReaderMoveToNextAttribute(node))
                    {
						xmlFree(name);
            			name = xmlTextReaderName(node);
                        if (!strcmp("subject",(char*)name))
                        {
							xmlFree(value);
							value = xmlTextReaderValue(node);
                            pFileInfo->SetSubject((char*)value);
                        }
                    }
                }
                else if (!strcmp("segment",(char*)name))
                {
                    long long lsize = -1;
                    int partNumber = -1;

                    while (xmlTextReaderMoveToNextAttribute(node))
                    {
						xmlFree(name);
						name = xmlTextReaderName(node);
						xmlFree(value);
						value = xmlTextReaderValue(node);
                        if (!strcmp("bytes",(char*)name))
                        {
                            lsize = atol((char*)value);
                        }
                        if (!strcmp("number",(char*)name))
                        {
                            partNumber = atol((char*)value);
                        }
                    }
                    if (lsize > 0)
                    {
                        pFileInfo->SetSize(pFileInfo->GetSize() + lsize);
                    }
                    
					/* Get the #text part */
                    ret = xmlTextReaderRead(node);

                    if (partNumber > 0)
                    {
                        // new segment, add it!
						xmlFree(value);
						value = xmlTextReaderValue(node);
                        char tmp[2048];
                        snprintf(tmp, 2048, "<%s>", (char*)value);
                        ArticleInfo* pArticle = new ArticleInfo();
                        pArticle->SetPartNumber(partNumber);
                        pArticle->SetMessageID(tmp);
                        pArticle->SetSize(lsize);
						AddArticle(pFileInfo, pArticle);
                    }
                }
                else if (!strcmp("group",(char*)name))
                {
                    ret = xmlTextReaderRead(node);
					xmlFree(value);
					value = xmlTextReaderValue(node);
                    pFileInfo->GetGroups()->push_back(strdup((char*)value));
                }
            }

            if (xmlTextReaderNodeType(node) == 15)
            {
                /* Close the file element, add the new file to file-list */
                if (!strcmp("file",(char*)name))
                {
					AddFileInfo(pFileInfo);
                }
            }

            xmlFree(name);
            xmlFree(value);
        }
        ret = xmlTextReaderRead(node);
    }
    if (ret != 0)
    {
        error("Failed to parse nzb-file\n");
		return false;
    }
	return true;
}
#endif
