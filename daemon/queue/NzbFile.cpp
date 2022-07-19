/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "NzbFile.h"
#include "Log.h"
#include "DownloadInfo.h"
#include "Options.h"
#include "DiskState.h"
#include "Util.h"
#include "FileSystem.h"

NzbFile::NzbFile(const char* fileName, const char* category) :
	m_fileName(fileName)
{
	debug("Creating NZBFile");

	m_nzbInfo = std::make_unique<NzbInfo>();
	m_nzbInfo->SetFilename(fileName);
	m_nzbInfo->SetCategory(category);
	m_nzbInfo->BuildDestDirName();
}

void NzbFile::LogDebugInfo()
{
	info(" NZBFile %s", *m_fileName);
}

void NzbFile::AddArticle(FileInfo* fileInfo, std::unique_ptr<ArticleInfo> articleInfo)
{
	int index = articleInfo->GetPartNumber() - 1;

	// make Article-List big enough
	if (index >= (int)fileInfo->GetArticles()->size())
	{
		fileInfo->GetArticles()->resize(index + 1);
	}

	(*fileInfo->GetArticles())[index] = std::move(articleInfo);
}

void NzbFile::AddFileInfo(std::unique_ptr<FileInfo> fileInfo)
{
	// calculate file size and delete empty articles

	int64 size = 0;
	int64 missedSize = 0;
	int64 oneSize = 0;
	int uncountedArticles = 0;
	int missedArticles = 0;
	int totalArticles = (int)fileInfo->GetArticles()->size();
	int i = 0;
	for (ArticleList::iterator it = fileInfo->GetArticles()->begin(); it != fileInfo->GetArticles()->end(); )
	{
		ArticleInfo* article = (*it).get();
		if (!article)
		{
			fileInfo->GetArticles()->erase(it);
			it = fileInfo->GetArticles()->begin() + i;
			missedArticles++;
			if (oneSize > 0)
			{
				missedSize += oneSize;
			}
			else
			{
				uncountedArticles++;
			}
		}
		else
		{
			size += article->GetSize();
			if (oneSize == 0)
			{
				oneSize = article->GetSize();
			}
			it++;
			i++;
		}
	}

	if (fileInfo->GetArticles()->empty())
	{
		return;
	}

	missedSize += uncountedArticles * oneSize;
	size += missedSize;
	fileInfo->SetNzbInfo(m_nzbInfo.get());
	fileInfo->SetSize(size);
	fileInfo->SetRemainingSize(size - missedSize);
	fileInfo->SetMissedSize(missedSize);
	fileInfo->SetTotalArticles(totalArticles);
	fileInfo->SetMissedArticles(missedArticles);
	m_nzbInfo->GetFileList()->Add(std::move(fileInfo));
}

void NzbFile::ParseSubject(FileInfo* fileInfo, bool TryQuotes)
{
	// Example subject: some garbage "title" yEnc (10/99)

	if (!fileInfo->GetSubject())
	{
		// Malformed file element without subject. We generate subject using internal element id.
		fileInfo->SetSubject(CString::FormatStr("%d", fileInfo->GetId()));
        return;
	}

	// strip the "yEnc (10/99)"-suffix
	BString<1024> subject = fileInfo->GetSubject();
	char* end = subject + strlen(subject) - 1;
	if (*end == ')')
	{
		end--;
		while (strchr("0123456789", *end) && end > subject) end--;
		if (*end == '/')
		{
			end--;
			while (strchr("0123456789", *end) && end > subject) end--;
			if (end - 6 > subject && !strncmp(end - 6, " yEnc (", 7))
			{
				end[-6] = '\0';
			}
		}
	}

	if (TryQuotes)
	{
        // first, try to use the filename in quotation marks
        char* p = subject;
        char* start = strchr(p, '\"');
        if (start)
        {
            start++;
            char* end = strchr(start + 1, '\"');
            /* found a non-empty quoted string */
            if ( end )
            {
                int len = (int)(end - start);
                char* point = strchr(start + 1, '.');
                if (point && point < end)
                {
                    BString<1024> filename;
                    filename.Set(start, len);
                    fileInfo->SetFilename(filename);
                    return;
                }
            }
        }
        // second, sometimes we encounter deliberately malformed subjects
        // e.g. ...[PRiVATE]-[WtFnZb]-[24]-[12/filename.ext] - "" yEnc (...
        //   or ...[PRiVATE]-[WtFnZb]-[00101.mpls]-[163/591] - "" yEnc (...
        const char * const signature = "[PRiVATE]-[WtFnZb]-[";
        p = strstr( subject, signature );
        if ( p )
        {
            p += strlen( signature );
            // remember the spot of the final open bracket. at the end
            //  of the signature, so we don't scan backwards too far
            char * sigEnd = p - 1;

            p = strchr( p, '.' );
            // if there's no period at all, assume it's not a filename
            if ( p )
            {
                // scan forward to find the end of the filename, even if
                // we picked up an earlier period used as a separator
                end = strchr( p, ']' );
                if ( end )
                {
                    int depth = 0;
                    // scan backwards for the start of the filename
                    for ( start = NULL; p >= sigEnd && start == NULL; --p )
                    {
                        switch ( *p )
                        {
                            // yes, they sometimes nest square brackets,
                            // just to make things more interesting...
                        case ']':
                            ++depth;
                            break;

                        case '[':
                            if ( depth > 0 ) --depth;
                            else start = p + 1;
                            break;

                        case '/':
                            start = p + 1;
                            break;
                        }
                    }

                    if ( start )
                    {
                        BString<1024> filename;
                        filename.Set( start, (end - start) );
                        fileInfo->SetFilename( filename );
                        return;
                    }
                }
            }
        }
    }


	// tokenize subject, considering spaces as separators and quotation
	// marks as non separable token delimiters.
	// then take the last token containing dot (".") as a filename

	typedef std::vector<CString> TokenList;
	TokenList tokens;

	// tokenizing
	char* p = subject;
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
				tokens.emplace_back(start, len);
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
		fileInfo->SetFilename(besttoken);
	}
	else
	{
		// subject is empty or contains only separators?
		debug("Could not extract Filename from Subject: %s. Using Subject as Filename", fileInfo->GetSubject());
		fileInfo->SetFilename(fileInfo->GetSubject());
	}
}

bool NzbFile::HasDuplicateFilenames()
{
    // It's Ok if just two files to have the same filename, this is
    // a frequent occurrence from posting-errors to repost bad files
    int count = m_nzbInfo->GetFileList()->size();
    if ( count <= 2 ) return false;

    int percent;
    int totaldupes = 0;
    for (FileList::iterator it = m_nzbInfo->GetFileList()->begin(); it != m_nzbInfo->GetFileList()->end(); it++)
	{
        FileInfo* fileInfo1 = (*it).get();
		int dupes = 0;
		for (FileList::iterator it2 = it + 1; it2 != m_nzbInfo->GetFileList()->end(); it2++)
		{
			FileInfo* fileInfo2 = (*it2).get();
			if (!strcmp(fileInfo1->GetFilename(), fileInfo2->GetFilename()) &&
				strcmp(fileInfo1->GetSubject(), fileInfo2->GetSubject()))
			{
				++dupes;
			}
		}

		// If more than 5% of the files have the same parsed filename but
        // different subjects, this implies that the parsing was incorrect.
		// In this case, we take subjects as filenames to prevent
		// false "duplicate files"-alarm.
        percent = (dupes * 100)/count;
		if ( percent > 5 )
		{
            warn( "more than %d%% of the files appear to have the name \'%s\' - trying alternate names",
                  percent, fileInfo1->GetFilename() );
			return true;
		}
        totaldupes += dupes;
	}
    percent = (totaldupes * 100)/count;
    if ( percent > 25 )
    {
        warn( "more than %d%% of the files appear to have duplicate names - trying alternate names", percent );
        return true;
    }

	return false;
}

/**
 * Generate filenames from subjects and check if the parsing of subject was correct
 */
void NzbFile::BuildFilenames()
{
	for (FileInfo* fileInfo : m_nzbInfo->GetFileList())
	{
		ParseSubject(fileInfo, true);
	}

	if (HasDuplicateFilenames())
	{
		for (FileInfo* fileInfo : m_nzbInfo->GetFileList())
		{
			ParseSubject(fileInfo, false);
		}
	}

	if (HasDuplicateFilenames())
	{
		m_nzbInfo->SetManyDupeFiles(true);
		for (FileInfo* fileInfo : m_nzbInfo->GetFileList())
		{
			fileInfo->SetFilename(fileInfo->GetSubject());
		}
	}
}

void NzbFile::CalcHashes()
{
	RawFileList sortedFiles;

	for (FileInfo* fileInfo : m_nzbInfo->GetFileList())
	{
		sortedFiles.push_back(fileInfo);
	}

	std::sort(sortedFiles.begin(), sortedFiles.end(),
		[](FileInfo* first, FileInfo* second)
		{
			return strcmp(first->GetFilename(), second->GetFilename()) > 0;
		});

	uint32 fullContentHash = 0;
	uint32 filteredContentHash = 0;
	int useForFilteredCount = 0;

	for (FileInfo* fileInfo : sortedFiles)
	{
		// check file extension
		bool skip = !fileInfo->GetParFile() &&
			Util::MatchFileExt(fileInfo->GetFilename(), g_Options->GetParIgnoreExt(), ",;");

		for (ArticleInfo* article: fileInfo->GetArticles())
		{
			int len = strlen(article->GetMessageId());
			fullContentHash = Util::HashBJ96(article->GetMessageId(), len, fullContentHash);
			if (!skip)
			{
				filteredContentHash = Util::HashBJ96(article->GetMessageId(), len, filteredContentHash);
				useForFilteredCount++;
			}
		}
	}

	// if filtered hash is based on less than a half of files - do not use filtered hash at all
	if (useForFilteredCount < (int)sortedFiles.size() / 2)
	{
		filteredContentHash = 0;
	}

	m_nzbInfo->SetFullContentHash(fullContentHash);
	m_nzbInfo->SetFilteredContentHash(filteredContentHash);
}

void NzbFile::ProcessFiles()
{
	BuildFilenames();

	for (FileInfo* fileInfo : m_nzbInfo->GetFileList())
	{
		fileInfo->MakeValidFilename();

		BString<1024> loFileName = fileInfo->GetFilename();
		for (char* p = loFileName; *p; p++) *p = tolower(*p); // convert string to lowercase
		bool parFile = strstr(loFileName, ".par2");

		m_nzbInfo->SetFileCount(m_nzbInfo->GetFileCount() + 1);
		m_nzbInfo->SetTotalArticles(m_nzbInfo->GetTotalArticles() + fileInfo->GetTotalArticles());
		m_nzbInfo->SetFailedArticles(m_nzbInfo->GetFailedArticles() + fileInfo->GetMissedArticles());
		m_nzbInfo->SetCurrentFailedArticles(m_nzbInfo->GetCurrentFailedArticles() + fileInfo->GetMissedArticles());
		m_nzbInfo->SetSize(m_nzbInfo->GetSize() + fileInfo->GetSize());
		m_nzbInfo->SetRemainingSize(m_nzbInfo->GetRemainingSize() + fileInfo->GetRemainingSize());
		m_nzbInfo->SetFailedSize(m_nzbInfo->GetFailedSize() + fileInfo->GetMissedSize());
		m_nzbInfo->SetCurrentFailedSize(m_nzbInfo->GetFailedSize());

		fileInfo->SetParFile(parFile);
		if (parFile)
		{
			m_nzbInfo->SetParSize(m_nzbInfo->GetParSize() + fileInfo->GetSize());
			m_nzbInfo->SetParFailedSize(m_nzbInfo->GetParFailedSize() + fileInfo->GetMissedSize());
			m_nzbInfo->SetParCurrentFailedSize(m_nzbInfo->GetParFailedSize());
			m_nzbInfo->SetRemainingParCount(m_nzbInfo->GetRemainingParCount() + 1);
		}
	}

	m_nzbInfo->UpdateMinMaxTime();

	CalcHashes();

	if (g_Options->GetServerMode())
	{
		for (FileInfo* fileInfo : m_nzbInfo->GetFileList())
		{
			g_DiskState->SaveFile(fileInfo);
			fileInfo->GetArticles()->clear();
		}
	}

	if (m_password)
	{
		ReadPassword();
	}
}

/**
 * Password read using XML-parser may have special characters (such as TAB) stripped.
 * This function rereads password directly from file to keep all characters intact.
 */
void NzbFile::ReadPassword()
{
	DiskFile file;
	if (!file.Open(m_fileName, DiskFile::omRead))
	{
		return;
	}

	// obtain file size.
	file.Seek(0, DiskFile::soEnd);
	int size  = (int)file.Position();
	file.Seek(0, DiskFile::soSet);

	// reading first 4KB of the file

	CharBuffer buf(4096);

	size = size < 4096 ? size : 4096;

	// copy the file into the buffer.
	file.Read(buf, size);

	file.Close();

	buf[size-1] = '\0';

	char* metaPassword = strstr(buf, "<meta type=\"password\">");
	if (metaPassword)
	{
		metaPassword += 22; // length of '<meta type="password">'
		char* end = strstr(metaPassword, "</meta>");
		if (end)
		{
			*end = '\0';
			WebUtil::XmlDecode(metaPassword);
			m_password = metaPassword;
		}
	}
}

#ifdef WIN32
bool NzbFile::Parse()
{
	CoInitialize(nullptr);

	HRESULT hr;

	MSXML::IXMLDOMDocumentPtr doc;
	hr = doc.CreateInstance(MSXML::CLSID_DOMDocument);
	if (FAILED(hr))
	{
		return false;
	}

	// Load the XML document file...
	doc->put_resolveExternals(VARIANT_FALSE);
	doc->put_validateOnParse(VARIANT_FALSE);
	doc->put_async(VARIANT_FALSE);

	_variant_t vFilename(*WString(*m_fileName));

	// 1. first trying to load via filename without URL-encoding (certain charaters doesn't work when encoded)
	VARIANT_BOOL success = doc->load(vFilename);
	if (success == VARIANT_FALSE)
	{
		// 2. now trying filename encoded as URL
		char url[2048];
		EncodeUrl(m_fileName, url, 2048);
		debug("url=\"%s\"", url);
		_variant_t vUrl(url);

		success = doc->load(vUrl);
	}

	if (success == VARIANT_FALSE)
	{
		_bstr_t r(doc->GetparseError()->reason);
		const char* errMsg = r;
		m_nzbInfo->AddMessage(Message::mkError, BString<1024>("Error parsing nzb-file %s: %s",
			FileSystem::BaseFileName(m_fileName), errMsg));
		return false;
	}

	if (!ParseNzb(doc))
	{
		return false;
	}

	if (m_nzbInfo->GetFileList()->empty())
	{
		m_nzbInfo->AddMessage(Message::mkError, BString<1024>(
			"Error parsing nzb-file %s: file has no content", FileSystem::BaseFileName(m_fileName)));
		return false;
	}

	ProcessFiles();

	return true;
}

void NzbFile::EncodeUrl(const char* filename, char* url, int bufLen)
{
	WString widefilename(filename);

	char* end = url + bufLen;
	for (wchar_t* p = widefilename; *p && url < end - 3; p++)
	{
		wchar_t ch = *p;
		if (('0' <= ch && ch <= '9') ||
			('a' <= ch && ch <= 'z') ||
			('A' <= ch && ch <= 'Z') ||
			ch == '-' || ch == '.' || ch == '_' || ch == '~')
		{
			*url++ = (char)ch;
		}
		else
		{
			*url++ = '%';
			uint32 a = (uint32)ch >> 4;
			*url++ = a > 9 ? a - 10 + 'A' : a + '0';
			a = ch & 0xF;
			*url++ = a > 9 ? a - 10 + 'A' : a + '0';
		}
	}
	*url = '\0';
}

bool NzbFile::ParseNzb(IUnknown* nzb)
{
	MSXML::IXMLDOMDocumentPtr doc = nzb;
	MSXML::IXMLDOMNodePtr root = doc->documentElement;

	MSXML::IXMLDOMNodePtr node = root->selectSingleNode("/nzb/head/meta[@type='password']");
	if (node)
	{
		_bstr_t password(node->Gettext());
		m_password = password;
	}

	MSXML::IXMLDOMNodeListPtr fileList = root->selectNodes("/nzb/file");
	for (int i = 0; i < fileList->Getlength(); i++)
	{
		node = fileList->Getitem(i);
		MSXML::IXMLDOMNodePtr attribute = node->Getattributes()->getNamedItem("subject");
		if (!attribute) return false;
		_bstr_t subject(attribute->Gettext());

		std::unique_ptr<FileInfo> fileInfo = std::make_unique<FileInfo>();
		fileInfo->SetSubject(subject);

		attribute = node->Getattributes()->getNamedItem("date");
		if (attribute)
		{
			_bstr_t date(attribute->Gettext());
			fileInfo->SetTime(atoi(date));
		}

		MSXML::IXMLDOMNodeListPtr groupList = node->selectNodes("groups/group");
		for (int g = 0; g < groupList->Getlength(); g++)
		{
			MSXML::IXMLDOMNodePtr node = groupList->Getitem(g);
			_bstr_t group = node->Gettext();
			fileInfo->GetGroups()->push_back((const char*)group);
		}

		MSXML::IXMLDOMNodeListPtr segmentList = node->selectNodes("segments/segment");
		for (int g = 0; g < segmentList->Getlength(); g++)
		{
			MSXML::IXMLDOMNodePtr node = segmentList->Getitem(g);
			_bstr_t bid = node->Gettext();
			BString<1024> id("<%s>", (const char*)bid);

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
				std::unique_ptr<ArticleInfo> article = std::make_unique<ArticleInfo>();
				article->SetPartNumber(partNumber);
				article->SetMessageId(id);
				article->SetSize(lsize);
				AddArticle(fileInfo.get(), std::move(article));
			}
		}

		AddFileInfo(std::move(fileInfo));
	}
	return true;
}

#else

bool NzbFile::Parse()
{
#ifdef DISABLE_LIBXML2
	error("Could not parse rss feed, program was compiled without libxml2 support");
	return false;
#else
	xmlSAXHandler SAX_handler = {0};
	SAX_handler.startElement = reinterpret_cast<startElementSAXFunc>(SAX_StartElement);
	SAX_handler.endElement = reinterpret_cast<endElementSAXFunc>(SAX_EndElement);
	SAX_handler.characters = reinterpret_cast<charactersSAXFunc>(SAX_characters);
	SAX_handler.error = reinterpret_cast<errorSAXFunc>(SAX_error);
	SAX_handler.getEntity = reinterpret_cast<getEntitySAXFunc>(SAX_getEntity);

	m_ignoreNextError = false;

	int ret = xmlSAXUserParseFile(&SAX_handler, this, m_fileName);

	if (ret != 0)
	{
		m_nzbInfo->AddMessage(Message::mkError, BString<1024>(
			"Error parsing nzb-file %s", FileSystem::BaseFileName(m_fileName)));
		return false;
	}

	if (m_nzbInfo->GetFileList()->empty())
	{
		m_nzbInfo->AddMessage(Message::mkError, BString<1024>(
			"Error parsing nzb-file %s: file has no content", FileSystem::BaseFileName(m_fileName)));
		return false;
	}

	ProcessFiles();

	return true;
#endif
}

void NzbFile::Parse_StartElement(const char *name, const char **atts)
{
	BString<1024> tagAttrMessage("Malformed nzb-file, tag <%s> must have attributes", name);

	m_tagContent.Clear();

	if (!strcmp("file", name))
	{
		m_fileInfo = std::make_unique<FileInfo>();
		m_fileInfo->SetFilename(m_fileName);

		if (!atts)
		{
			m_nzbInfo->AddMessage(Message::mkWarning, tagAttrMessage);
			return;
		}

		for (int i = 0; atts[i]; i += 2)
		{
			const char* attrname = atts[i];
			const char* attrvalue = atts[i + 1];
			if (!strcmp("subject", attrname))
			{
				m_fileInfo->SetSubject(attrvalue);
			}
			if (!strcmp("date", attrname))
			{
				m_fileInfo->SetTime(atoi(attrvalue));
			}
		}
	}
	else if (!strcmp("segment", name))
	{
		if (!m_fileInfo)
		{
			m_nzbInfo->AddMessage(Message::mkWarning, "Malformed nzb-file, tag <segment> without tag <file>");
			return;
		}

		if (!atts)
		{
			m_nzbInfo->AddMessage(Message::mkWarning, tagAttrMessage);
			return;
		}

		int64 lsize = -1;
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

		if (partNumber > 0)
		{
			// new segment, add it!
			std::unique_ptr<ArticleInfo> article = std::make_unique<ArticleInfo>();
			article->SetPartNumber(partNumber);
			article->SetSize(lsize);
			m_article = article.get();
			AddArticle(m_fileInfo.get(), std::move(article));
		}
	}
	else if (!strcmp("meta", name))
	{
		if (!atts)
		{
			m_nzbInfo->AddMessage(Message::mkWarning, tagAttrMessage);
			return;
		}
		m_hasPassword = atts[0] && atts[1] && !strcmp("type", atts[0]) && !strcmp("password", atts[1]);
	}
}

void NzbFile::Parse_EndElement(const char *name)
{
	if (!strcmp("file", name))
	{
		// Close the file element, add the new file to file-list
		AddFileInfo(std::move(m_fileInfo));
		m_article = nullptr;
	}
	else if (!strcmp("group", name))
	{
		if (!m_fileInfo)
		{
			// error: bad nzb-file
			return;
		}

		m_fileInfo->GetGroups()->push_back(*m_tagContent);
		m_tagContent.Clear();
	}
	else if (!strcmp("segment", name))
	{
		if (!m_fileInfo || !m_article)
		{
			// error: bad nzb-file
			return;
		}

		// Get the #text part
		BString<1024> id("<%s>", *m_tagContent);
		m_article->SetMessageId(id);
		m_article = nullptr;
	}
	else if (!strcmp("meta", name) && m_hasPassword)
	{
		m_password = m_tagContent;
	}
}

void NzbFile::Parse_Content(const char *buf, int len)
{
	m_tagContent.Append(buf, len);
}

void NzbFile::SAX_StartElement(NzbFile* file, const char *name, const char **atts)
{
	file->Parse_StartElement(name, atts);
}

void NzbFile::SAX_EndElement(NzbFile* file, const char *name)
{
	file->Parse_EndElement(name);
}

void NzbFile::SAX_characters(NzbFile* file, const char * xmlstr, int len)
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
		file->Parse_Content(str + off, newlen);
	}
}

void* NzbFile::SAX_getEntity(NzbFile* file, const char * name)
{
#ifdef DISABLE_LIBXML2
	void* e = nullptr;
#else
	xmlEntityPtr e = xmlGetPredefinedEntity((xmlChar* )name);
#endif
	if (!e)
	{
		file->m_nzbInfo->AddMessage(Message::mkWarning, "entity not found");
		file->m_ignoreNextError = true;
	}

	return e;
}

void NzbFile::SAX_error(NzbFile* file, const char *msg, ...)
{
	if (file->m_ignoreNextError)
	{
		file->m_ignoreNextError = false;
		return;
	}

	va_list argp;
	va_start(argp, msg);
	char errMsg[1024];
	vsnprintf(errMsg, sizeof(errMsg), msg, argp);
	errMsg[1024-1] = '\0';
	va_end(argp);

	// remove trailing CRLF
	for (char* pend = errMsg + strlen(errMsg) - 1; pend >= errMsg && (*pend == '\n' || *pend == '\r' || *pend == ' '); pend--) *pend = '\0';

	file->m_nzbInfo->AddMessage(Message::mkError, BString<1024>("Error parsing nzb-file: %s", errMsg));
}
#endif
