/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2012-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "WebServer.h"
#include "XmlRpc.h"
#include "Log.h"
#include "Options.h"
#include "Util.h"
#include "FileSystem.h"
#ifndef DISABLE_PARCHECK
	#include "par2cmdline.h"
	#include "md5.h"
#endif

static const char* ERR_HTTP_BAD_REQUEST = "400 Bad Request";
static const char* ERR_HTTP_NOT_FOUND = "404 Not Found";
static const char* ERR_HTTP_SERVICE_UNAVAILABLE = "503 Service Unavailable";

static const int MAX_UNCOMPRESSED_SIZE = 500;
char WebProcessor::m_serverAuthToken[3][49];

//*****************************************************************
// WebProcessor

void WebProcessor::Init()
{
	if (m_serverAuthToken[0][0] != 0)
	{
		// already initialized
		return;
	}

	for (int j = uaControl; j <= uaAdd; j++)
	{
		for (int i = 0; i < sizeof(m_serverAuthToken[j]) - 1; i++)
		{
			int ch = rand() % (10 + 26 + 26);
			if (0 <= ch && ch < 10)
			{
				m_serverAuthToken[j][i] = '0' + ch;
			}
			else if (10 <= ch && ch < 10 + 26)
			{
				m_serverAuthToken[j][i] = 'a' + ch - 10;
			}
			else
			{
				m_serverAuthToken[j][i] = 'A' + ch - 10 - 26;
			}
		}
		m_serverAuthToken[j][sizeof(m_serverAuthToken[j]) - 1] = '\0';
		debug("X-Auth-Token[%i]: %s", j, m_serverAuthToken[j]);
	}
}

void WebProcessor::Execute()
{
	m_gzip =false;
	m_userAccess = uaControl;
	m_authInfo[0] = '\0';
	m_authToken[0] = '\0';

	ParseHeaders();

	if (m_httpMethod == hmPost && m_contentLen <= 0)
	{
		error("Invalid-request: content length is 0");
		return;
	}

	if (m_httpMethod == hmOptions)
	{
		SendOptionsResponse();
		return;
	}

	ParseUrl();

	m_rpcRequest = XmlRpcProcessor::IsRpcRequest(m_url);
	m_authorized = CheckCredentials();

	if ((!g_Options->GetFormAuth() || m_rpcRequest) && !m_authorized)
	{
		SendAuthResponse();
		return;
	}

	if (m_httpMethod == hmPost)
	{
		// reading http body (request content)
		m_request.Reserve(m_contentLen);
		m_request[m_contentLen] = '\0';

		if (!m_connection->Recv(m_request, m_contentLen))
		{
			error("Invalid-request: could not read data");
			return;
		}
		debug("Request=%s", *m_request);
	}

	debug("request received from %s", m_connection->GetRemoteAddr());

	Dispatch();
}

void WebProcessor::ParseHeaders()
{
	// reading http header
	char buffer[1024];
	m_contentLen = 0;
	while (char* p = m_connection->ReadLine(buffer, sizeof(buffer), nullptr))
	{
		if (char* pe = strrchr(p, '\r')) *pe = '\0';
		debug("header=%s", p);

		if (!strncasecmp(p, "Content-Length: ", 16))
		{
			m_contentLen = atoi(p + 16);
		}
		else if (!strncasecmp(p, "Authorization: Basic ", 21) && Util::EmptyStr(m_authInfo))
		{
			char* authInfo64 = p + 21;
			if (strlen(authInfo64) > sizeof(m_authInfo))
			{
				error("Invalid-request: auth-info too big");
				return;
			}
			m_authInfo[WebUtil::DecodeBase64(authInfo64, 0, m_authInfo)] = '\0';
		}
		else if (!strncasecmp(p, "X-Authorization: Basic ", 23))
		{
			char* authInfo64 = p + 23;
			if (strlen(authInfo64) > sizeof(m_authInfo))
			{
				error("Invalid-request: auth-info too big");
				return;
			}
			m_authInfo[WebUtil::DecodeBase64(authInfo64, 0, m_authInfo)] = '\0';
		}
		else if (!strncasecmp(p, "Accept-Encoding: ", 17))
		{
			m_gzip = strstr(p, "gzip");
		}
		else if (!strncasecmp(p, "Origin: ", 8))
		{
			m_origin = p + 8;
		}
		else if (!strncasecmp(p, "Cookie: ", 8))
		{
			debug("%s", p);
			const char* tok = strstr(p, "Auth-Token=");
			if (tok && tok[11] != ';' && tok[11] != '\0')
			{
				strncpy(m_authToken, tok + 11, sizeof(m_authToken)-1);
				m_authToken[sizeof(m_authToken)-1] = '\0';
			}
		}
		else if (!strncasecmp(p, "X-Forwarded-For: ", 17))
		{
			m_forwardedFor = p + 17;
		}
		else if (!strncasecmp(p, "If-None-Match: ", 15))
		{
			m_oldetag = p + 15;
		}
		else if (*p == '\0')
		{
			break;
		}
	}

	debug("URL=%s", *m_url);
	debug("Authorization=%s", m_authInfo);
	debug("Auth-Token=%s", m_authToken);
}

void WebProcessor::ParseUrl()
{
	// remove subfolder "nzbget" from the path (if exists)
	// http://localhost:6789/nzbget/username:password/jsonrpc -> http://localhost:6789/username:password/jsonrpc
	if (!strncmp(m_url, "/nzbget/", 8))
	{
		m_url = CString(m_url + 7);
	}
	// http://localhost:6789/nzbget -> http://localhost:6789
	if (!strcmp(m_url, "/nzbget"))
	{
		SendRedirectResponse(BString<1024>("%s/", *m_url));
		return;
	}

	// authorization via URL in format:
	// http://localhost:6789/username:password/jsonrpc
	char* pauth1 = strchr(m_url + 1, ':');
	char* pauth2 = strchr(m_url + 1, '/');
	if (pauth1 && pauth1 < pauth2)
	{
		char* pstart = m_url + 1;
		int len = 0;
		char* pend = strchr(pstart + 1, '/');
		if (pend)
		{
			len = (int)(pend - pstart < (int)sizeof(m_authInfo) - 1 ? pend - pstart : (int)sizeof(m_authInfo) - 1);
		}
		else
		{
			len = strlen(pstart);
		}
		strncpy(m_authInfo, pstart, len);
		m_authInfo[len] = '\0';
		m_url = CString(pend);
	}

	debug("Final URL=%s", *m_url);
}

bool WebProcessor::CheckCredentials()
{
	if (!Util::EmptyStr(g_Options->GetControlPassword()) &&
		!(!Util::EmptyStr(g_Options->GetAuthorizedIp()) && IsAuthorizedIp(m_connection->GetRemoteAddr())))
	{
		if (Util::EmptyStr(m_authInfo))
		{
			// Authorization via X-Auth-Token
			for (int j = uaControl; j <= uaAdd; j++)
			{
				if (!strcmp(m_authToken, m_serverAuthToken[j]))
				{
					m_userAccess = (EUserAccess)j;
					return true;
				}
			}
			return false;
		}

		// Authorization via username:password
		char* pw = strchr(m_authInfo, ':');
		if (pw) *pw++ = '\0';

		if ((Util::EmptyStr(g_Options->GetControlUsername()) ||
			 !strcmp(m_authInfo, g_Options->GetControlUsername())) &&
			pw && !strcmp(pw, g_Options->GetControlPassword()))
		{
			m_userAccess = uaControl;
		}
		else if (!Util::EmptyStr(g_Options->GetRestrictedUsername()) &&
			!strcmp(m_authInfo, g_Options->GetRestrictedUsername()) &&
			pw && !strcmp(pw, g_Options->GetRestrictedPassword()))
		{
			m_userAccess = uaRestricted;
		}
		else if (!Util::EmptyStr(g_Options->GetAddUsername()) &&
			!strcmp(m_authInfo, g_Options->GetAddUsername()) &&
			pw && !strcmp(pw, g_Options->GetAddPassword()))
		{
			m_userAccess = uaAdd;
		}
		else
		{
			warn("Request received on port %i from %s%s, but username or password invalid (%s:%s)",
				g_Options->GetControlPort(), m_connection->GetRemoteAddr(),
				!m_forwardedFor.Empty() ? (char*)BString<1024>(" (forwarded for: %s)", *m_forwardedFor) : "",
				m_authInfo, pw);
			return false;
		}
	}

	return true;
}

bool WebProcessor::IsAuthorizedIp(const char* remoteAddr)
{
	const char* remoteIp = m_connection->GetRemoteAddr();

	// split option AuthorizedIP into tokens and check each token
	bool authorized = false;
	Tokenizer tok(g_Options->GetAuthorizedIp(), ",;");
	while (const char* iP = tok.Next())
	{
		if (!strcmp(iP, remoteIp))
		{
			authorized = true;
			break;
		}
	}

	return authorized;
}

void WebProcessor::Dispatch()
{
	if (m_url[0] != '/')
	{
		SendErrorResponse(ERR_HTTP_BAD_REQUEST, true);
		return;
	}

	if (m_rpcRequest)
	{
		XmlRpcProcessor processor;
		processor.SetRequest(m_request);
		processor.SetHttpMethod(m_httpMethod == hmGet ? XmlRpcProcessor::hmGet : XmlRpcProcessor::hmPost);
		processor.SetUserAccess((XmlRpcProcessor::EUserAccess)m_userAccess);
		processor.SetUrl(m_url);
		processor.Execute();
		SendBodyResponse(processor.GetResponse(), strlen(processor.GetResponse()), processor.GetContentType(), processor.IsCachable());
		return;
	}

	if (Util::EmptyStr(g_Options->GetWebDir()))
	{
		SendErrorResponse(ERR_HTTP_SERVICE_UNAVAILABLE, true);
		return;
	}

	if (m_httpMethod != hmGet)
	{
		SendErrorResponse(ERR_HTTP_BAD_REQUEST, true);
		return;
	}

	// for security reasons we allow only characters "0..9 A..Z a..z . - + _ / ?" in the URLs
	// we also don't allow ".." in the URLs
	for (char *p = m_url; *p; p++)
	{
		if (!((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
			*p == '.' || *p == '-' || *p == '+' || *p == '?' || *p == '_' || *p == '/') || (*p == '.' && p[1] == '.'))
		{
			SendErrorResponse(ERR_HTTP_NOT_FOUND, true);
			return;
		}
	}

	if (!strncmp(m_url, "/combined.", 10) && strchr(m_url, '?'))
	{
		SendMultiFileResponse();
	}
	else
	{
		SendSingleFileResponse();
	}
}

void WebProcessor::SendAuthResponse()
{
	const char* AUTH_RESPONSE_HEADER =
		"HTTP/1.0 401 Unauthorized\r\n"
		"%s"
		"Connection: close\r\n"
		"Content-Type: text/plain\r\n"
		"Server: nzbget-%s\r\n"
		"\r\n";

	BString<1024> responseHeader(AUTH_RESPONSE_HEADER,
		g_Options->GetFormAuth() ? "" : "WWW-Authenticate: Basic realm=\"NZBGet\"\r\n",
		Util::VersionRevision());

	// Send the response answer
	debug("ResponseHeader=%s", *responseHeader);
	m_connection->Send(responseHeader, responseHeader.Length());
}

void WebProcessor::SendOptionsResponse()
{
	const char* OPTIONS_RESPONSE_HEADER =
		"HTTP/1.1 200 OK\r\n"
		"Connection: close\r\n"
		//"Content-Type: plain/text\r\n"
		"Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
		"Access-Control-Allow-Origin: %s\r\n"
		"Access-Control-Allow-Credentials: true\r\n"
		"Access-Control-Max-Age: 86400\r\n"
		"Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
		"Server: nzbget-%s\r\n"
		"\r\n";
	BString<1024> responseHeader(OPTIONS_RESPONSE_HEADER,
		m_origin.Str(), Util::VersionRevision());

	// Send the response answer
	debug("ResponseHeader=%s", *responseHeader);
	m_connection->Send(responseHeader, responseHeader.Length());
}

void WebProcessor::SendErrorResponse(const char* errCode, bool printWarning)
{
	const char* RESPONSE_HEADER =
		"HTTP/1.0 %s\r\n"
		"Connection: close\r\n"
		"Content-Length: %i\r\n"
		"Content-Type: text/html\r\n"
		"Server: nzbget-%s\r\n"
		"\r\n";

	if (printWarning)
	{
		warn("Web-Server: %s, Resource: %s", errCode, *m_url);
	}

	BString<1024> responseBody("<html><head><title>%s</title></head><body>Error: %s</body></html>",
		errCode, errCode);
	int pageContentLen = responseBody.Length();

	BString<1024> responseHeader(RESPONSE_HEADER, errCode, pageContentLen, Util::VersionRevision());

	// Send the response answer
	m_connection->Send(responseHeader, responseHeader.Length());
	m_connection->Send(responseBody, pageContentLen);
}

void WebProcessor::SendRedirectResponse(const char* url)
{
	const char* REDIRECT_RESPONSE_HEADER =
		"HTTP/1.0 301 Moved Permanently\r\n"
		"Location: %s\r\n"
		"Connection: close\r\n"
		"Server: nzbget-%s\r\n"
		"\r\n";
	BString<1024> responseHeader(REDIRECT_RESPONSE_HEADER, url, Util::VersionRevision());

	// Send the response answer
	debug("ResponseHeader=%s", *responseHeader);
	m_connection->Send(responseHeader, responseHeader.Length());
}

#ifndef DISABLE_PARCHECK
static void base64_encode(char* dst, const unsigned char* src, size_t len)
{
	static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	for (; len >= 3; src += 3, len -= 3)
	{
		*dst++ = b64[src[0] >> 2];
		*dst++ = b64[(src[0] << 4 | src[1] >> 4) & 0x3f];
		*dst++ = b64[(src[1] << 2 | src[2] >> 6) & 0x3f];
		*dst++ = b64[src[2] & 0x3f];
	}
	if (len > 0)
	{
		*dst++ = b64[src[0] >> 2];
		if (len == 2)
		{
			*dst++ = b64[(src[0] << 4 | src[1] >> 4) & 0x3f];
			*dst++ = b64[(src[1] << 2) & 0x3f];
		}
		else
		{
			*dst++ = b64[(src[0] << 4) & 0x3f];
			*dst++ = '=';
		}
		*dst++ = '=';
	}
	*dst = '\0';
}
#endif

void WebProcessor::SendBodyResponse(const char* body, int bodyLen, const char* contentType, bool cachable)
{
	const char* RESPONSE_HEADER =
		"HTTP/1.1 %s\r\n"
		"Connection: close\r\n"
		"Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
		"Access-Control-Allow-Origin: %s\r\n"
		"Access-Control-Allow-Credentials: true\r\n"
		"Access-Control-Max-Age: 86400\r\n"
		"Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
		"Set-Cookie: Auth-Type=%s\r\n"
		"Set-Cookie: Auth-Token=%s\r\n"
		"Content-Length: %i\r\n"
		"%s"					// Content-Type: xxx
		"%s"					// Content-Encoding: gzip
		"%s"					// Etag
		"Server: nzbget-%s\r\n"
		"\r\n";

	char newetag[11 + 24];
	bool unchanged = false;
#ifndef DISABLE_PARCHECK
	if (cachable)
	{
		strcpy(newetag, "Etag: \"");

		Par2::MD5Hash hash;
		Par2::MD5Context md5;
		md5.Update(body, bodyLen);
		md5.Final(hash);

		base64_encode(newetag + strlen(newetag), hash.hash, sizeof(hash.hash));
		strcat(newetag, "\"");
		unchanged = m_oldetag && !strcmp(newetag + 6, m_oldetag);

		strcat(newetag, "\r\n");
	}
#endif

#ifndef DISABLE_GZIP
	CharBuffer gbuf;
	bool gzip = m_gzip && bodyLen > MAX_UNCOMPRESSED_SIZE;
	if (gzip)
	{
		uint32 outLen = ZLib::GZipLen(bodyLen);
		gbuf.Reserve(outLen);
		int gzippedLen = ZLib::GZip(body, bodyLen, *gbuf, outLen);
		if (gzippedLen > 0 && gzippedLen < bodyLen)
		{
			body = gbuf;
			bodyLen = gzippedLen;
		}
		else
		{
			gzip = false;
		}
	}
#else
	bool gzip = false;
#endif

	BString<1024> contentTypeHeader;
	if (contentType)
	{
		contentTypeHeader.Format("Content-Type: %s\r\n", contentType);
	}

	BString<1024> responseHeader(RESPONSE_HEADER,
		unchanged ? (m_httpMethod == hmGet ? "304 Not Modified" : "412 Precondition Failed") : "200 OK",
		m_origin.Str(),
		g_Options->GetFormAuth() ? "form" : "http",
		m_authorized ? m_serverAuthToken[m_userAccess] : "",
		unchanged ? 0 : bodyLen,
		*contentTypeHeader,
		gzip ? "Content-Encoding: gzip\r\n" : "",
		cachable ? newetag : "",
		Util::VersionRevision());

	// Send the request answer
	m_connection->Send(responseHeader, responseHeader.Length());
	if (!unchanged)
	{
		m_connection->Send(body, bodyLen);
	}
}

void WebProcessor::SendSingleFileResponse()
{
	const char *defRes = "";
	if (m_url[strlen(m_url)-1] == '/')
	{
		// default file in directory (if not specified) is "index.html"
		defRes = "index.html";
	}

	BString<1024> filename("%s%s%s", g_Options->GetWebDir(), *m_url, defRes);

	debug("serving file: %s", *filename);

	CharBuffer body;
	if (!FileSystem::LoadFileIntoBuffer(filename, body, true))
	{
		// do not print warnings "404 not found" for certain files
		bool ignorable = !strcmp(filename, "package-info.json") ||
			!strcmp(filename, "favicon.ico") ||
			!strncmp(filename, "apple-touch-icon", 16);

		SendErrorResponse(ERR_HTTP_NOT_FOUND, ignorable);
		return;
	}

	const char* contentType = DetectContentType(filename);
	int len = body.Size() - 1;

#ifdef DEBUG
	if (contentType && !strcmp(contentType, "text/html"))
	{
		Util::ReduceStr(body, "<!-- %if-debug%", "");
		Util::ReduceStr(body, "<!-- %if-not-debug% -->", "<!--");
		Util::ReduceStr(body, "<!-- %end% -->", "-->");
		Util::ReduceStr(body, "%end% -->", "");
		len = strlen(body);
	}
#endif

	SendBodyResponse(body, len, contentType, true);
}

void WebProcessor::SendMultiFileResponse()
{
	debug("serving multiple files: %s", *m_url);

	StringBuilder response;
	char* filelist = strchr(m_url, '?');
	*filelist++ = '\0';

	Tokenizer tok(filelist, "+");
	while (const char* filename = tok.Next())
	{
		BString<1024> diskFilename("%s%c%s", g_Options->GetWebDir(), PATH_SEPARATOR, filename);

		CharBuffer body;
		if (!FileSystem::LoadFileIntoBuffer(diskFilename, body, true))
		{
			warn("Web-Server: %s, Resource: /%s", ERR_HTTP_NOT_FOUND, filename);
			SendErrorResponse(ERR_HTTP_NOT_FOUND, false);
			return;
		}

		response.Append(body);
	}

	SendBodyResponse(response, response.Length(), DetectContentType(m_url), true);
}

const char* WebProcessor::DetectContentType(const char* filename)
{
	if (const char *ext = strrchr(filename, '.'))
	{
		if (!strcasecmp(ext, ".css"))
		{
			return "text/css";
		}
		else if (!strcasecmp(ext, ".html"))
		{
			return "text/html";
		}
		else if (!strcasecmp(ext, ".js"))
		{
			return "application/javascript";
		}
		else if (!strcasecmp(ext, ".png"))
		{
			return "image/png";
		}
		else if (!strcasecmp(ext, ".jpeg"))
		{
			return "image/jpeg";
		}
		else if (!strcasecmp(ext, ".gif"))
		{
			return "image/gif";
		}
	}
	return nullptr;
}
