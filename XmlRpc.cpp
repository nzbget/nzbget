/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2009 Andrei Prygounkov <hugbug@users.sourceforge.net>
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
#include "config.h"
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <cstdio>
#include <fstream>
#ifndef WIN32
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include "nzbget.h"
#include "XmlRpc.h"
#include "Log.h"
#include "Options.h"
#include "QueueCoordinator.h"
#include "QueueEditor.h"
#include "PrePostProcessor.h"
#include "Util.h"

extern Options* g_pOptions;
extern QueueCoordinator* g_pQueueCoordinator;
extern PrePostProcessor* g_pPrePostProcessor;
extern void ExitProc();


//*****************************************************************
// StringBuilder

StringBuilder::StringBuilder()
{
	m_szBuffer = NULL;
	m_iBufferSize = 0;
	m_iUsedSize = 0;
}

StringBuilder::~StringBuilder()
{
	if (m_szBuffer)
	{
		free(m_szBuffer);
	}
}

void StringBuilder::Append(const char* szStr)
{
	int iPartLen = strlen(szStr);
	if (m_iUsedSize + iPartLen + 1 > m_iBufferSize)
	{
		m_iBufferSize += iPartLen + 10240;
		m_szBuffer = (char*)realloc(m_szBuffer, m_iBufferSize);
	}
	strcpy(m_szBuffer + m_iUsedSize, szStr);
	m_iUsedSize += iPartLen;
	m_szBuffer[m_iUsedSize] = '\0';
}

//*****************************************************************
// XmlRpcProcessor

XmlRpcProcessor::XmlRpcProcessor()
{
	m_pConnection = NULL;
	m_szClientIP = NULL;
	m_szRequest = NULL;
	m_eProtocol = rpUndefined;
	m_eHttpMethod = hmPost;
	m_szUrl = NULL;
}

void XmlRpcProcessor::SetUrl(const char* szUrl)
{
	m_szUrl = strdup(szUrl);
}

XmlRpcProcessor::~XmlRpcProcessor()
{
	if (m_szRequest)
	{
		free(m_szRequest);
	}
	if (m_szUrl)
	{
		free(m_szUrl);
	}
}

void XmlRpcProcessor::Execute()
{
	char szAuthInfo[1024];
	szAuthInfo[0] = '\0';

	// reading http header
	char szBuffer[1024];
	bool bBody = false;
	int iContentLen = 0;
	while (char* p = m_pConnection->ReadLine(szBuffer, sizeof(szBuffer), NULL))
	{
		debug("header=%s", p);
		if (!strncasecmp(p, "Content-Length: ", 16))
		{
			iContentLen = atoi(p + 16);
		}
		if (!strncasecmp(p, "Authorization: Basic ", 21))
		{
			char* szAuthInfo64 = p + 21;
			if (strlen(szAuthInfo64) > sizeof(szAuthInfo))
			{
				error("invalid-request: auth-info too big");
				return;
			}
			if (char* pe = strrchr(szAuthInfo64, '\r')) *pe = '\0';
			szAuthInfo[Util::DecodeBase64(szAuthInfo64, 0, szAuthInfo)] = '\0';
		}
		if (!strncmp(p, "\r", 1))
		{
			bBody = true;
			break;
		}
	}

	debug("URL=%s", m_szUrl);
	debug("Content-Length=%i", iContentLen);
	debug("Authorization=%s", szAuthInfo);

	if (m_eHttpMethod == hmPost && iContentLen <= 0)
	{
		error("invalid-request: content length is 0");
		return;
	}

	if (strlen(szAuthInfo) == 0)
	{
		error("invalid-request: not authorized");
		return;
	}

	// Authorization
	char* pw = strchr(szAuthInfo, ':');
	if (pw) *pw++ = '\0';
	if (strcmp(szAuthInfo, "nzbget") || strcmp(pw, g_pOptions->GetServerPassword()))
	{
		warn("rpc-request received on port %i from %s, but password invalid", g_pOptions->GetServerPort(), m_szClientIP);
		return;
	}

	if (m_eHttpMethod == hmPost)
	{
		// reading http body (request content)
		m_szRequest = (char*)malloc(iContentLen + 1);
		m_szRequest[iContentLen] = '\0';

		if (!m_pConnection->RecvAll(m_szRequest, iContentLen))
		{
			free(m_szRequest);
			error("invalid-request: could not read data");
			return;
		}
		debug("Request=%s", m_szRequest);
	}

	debug("Request received from %s", m_szClientIP);

	Dispatch();
}

void XmlRpcProcessor::Dispatch()
{
	char* szRequest = m_szRequest;
	char szMethodName[100];
	szMethodName[0] = '\0';

	if (m_eHttpMethod == hmGet)
	{
		szRequest = m_szUrl + 1;
		char* pstart = strchr(szRequest, '/');
		if (pstart)
		{
			char* pend = strchr(pstart + 1, '?');
			if (pend) 
			{
				int iLen = (int)(pend - pstart - 1 < (int)sizeof(szMethodName) - 1 ? pend - pstart - 1 : (int)sizeof(szMethodName) - 1);
				strncpy(szMethodName, pstart + 1, iLen);
				szMethodName[iLen] = '\0';
				szRequest = pend + 1;
			}
			else
			{
				strncpy(szMethodName, pstart + 1, sizeof(szMethodName));
				szMethodName[sizeof(szMethodName) - 1] = '\0';
				szRequest = szRequest + strlen(szRequest);
			}
		}
	}
	else if (m_eProtocol == rpXmlRpc)
	{
		Util::XmlParseTagValue(m_szRequest, "methodName", szMethodName, sizeof(szMethodName), NULL);
	} 
	else if (m_eProtocol == rpJsonRpc) 
	{
		int iValueLen = 0;
		if (const char* szMethodPtr = Util::JsonFindField(m_szRequest, "method", &iValueLen))
		{
			strncpy(szMethodName, szMethodPtr + 1, iValueLen - 2);
			szMethodName[iValueLen - 2] = '\0';
			szRequest = (char*)szMethodPtr + 1 + iValueLen;
		}
	}

	debug("MethodName=%s", szMethodName);

	if (!strcasecmp(szMethodName, "system.multicall") && m_eProtocol == rpXmlRpc && m_eHttpMethod == hmPost)
	{
		MutliCall();
	}
	else
	{
		XmlCommand* command = CreateCommand(szMethodName);
		command->SetRequest(szRequest);
		command->SetProtocol(m_eProtocol);
		command->SetHttpMethod(m_eHttpMethod);
		command->PrepareParams();
		command->Execute();
		SendResponse(command->GetResponse(), command->GetFault());
		delete command;
	}
}

void XmlRpcProcessor::MutliCall()
{
	bool bError = false;
	StringBuilder cStringBuilder;

	cStringBuilder.Append("<array><data>");

	char* szRequestPtr = m_szRequest;
	char* szCallEnd = strstr(szRequestPtr, "</struct>");
	while (szCallEnd)
	{
		*szCallEnd = '\0';
		debug("MutliCall, request=%s", szRequestPtr);
		char* szNameEnd = strstr(szRequestPtr, "</name>");
		if (!szNameEnd)
		{
			bError = true;
			break;
		}

		char szMethodName[100];
		szMethodName[0] = '\0';
		Util::XmlParseTagValue(szNameEnd, "string", szMethodName, sizeof(szMethodName), NULL);
		debug("MutliCall, MethodName=%s", szMethodName);

		XmlCommand* command = CreateCommand(szMethodName);
		command->SetRequest(szRequestPtr);
		command->Execute();

		debug("MutliCall, Response=%s", command->GetResponse());

		bool bFault = !strncmp(command->GetResponse(), "<fault>", 7);
		bool bArray = !bFault && !strncmp(command->GetResponse(), "<array>", 7);
		if (!bFault && !bArray)
		{
			cStringBuilder.Append("<array><data>");
		}
		cStringBuilder.Append("<value>");
		cStringBuilder.Append(command->GetResponse());
		cStringBuilder.Append("</value>");
		if (!bFault && !bArray)
		{
			cStringBuilder.Append("</data></array>");
		}

		delete command;

		szRequestPtr = szCallEnd + 9; //strlen("</struct>")
		szCallEnd = strstr(szRequestPtr, "</struct>");
	}

	if (bError)
	{
		XmlCommand* command = new ErrorXmlCommand(4, "Parse error");
		command->SetRequest(m_szRequest);
		command->SetProtocol(rpXmlRpc);
		command->PrepareParams();
		command->Execute();
		SendResponse(command->GetResponse(), command->GetFault());
		delete command;
	}
	else
	{
		cStringBuilder.Append("</data></array>");
		SendResponse(cStringBuilder.GetBuffer(), false);
	}
}

void XmlRpcProcessor::SendResponse(const char* szResponse, bool bFault)
{
	const char* XML_RESPONSE_HEADER = 
		"HTTP/1.0 200 OK\r\n"
		"Connection: close\r\n"
		"Content-Length: %i\r\n"
		"Content-Type: text/xml\r\n"
		"Server: nzbget-%s\r\n"
		"\r\n";
	const char XML_HEADER[] = "<?xml version=\"1.0\"?>\n<methodResponse>\n";
	const char XML_FOOTER[] = "</methodResponse>";
	const char XML_OK_OPEN[] = "<params><param><value>";
	const char XML_OK_CLOSE[] = "</value></param></params>\n";
	const char XML_FAULT_OPEN[] = "<fault><value>";
	const char XML_FAULT_CLOSE[] = "</value></fault>\n";

	const char* JSON_RESPONSE_HEADER = 
		"HTTP/1.0 200 OK\r\n"
		"Connection: close\r\n"
		"Content-Length: %i\r\n"
		"Content-Type: application/json\r\n"
		"Server: nzbget-%s\r\n"
		"\r\n";
	const char JSON_HEADER[] = "{\n\"version\" : \"1.1\",\n";
	const char JSON_FOOTER[] = "\n}";
	const char JSON_OK_OPEN[] = "\"result\" : ";
	const char JSON_OK_CLOSE[] = "";
	const char JSON_FAULT_OPEN[] = "\"error\" : ";
	const char JSON_FAULT_CLOSE[] = "";

	bool bXmlRpc = m_eProtocol == rpXmlRpc;

	const char* szHeader = bXmlRpc ? XML_HEADER : JSON_HEADER;
	const char* szFooter = bXmlRpc ? XML_FOOTER : JSON_FOOTER;
	const char* szOpenTag = bFault ? (bXmlRpc ? XML_FAULT_OPEN : JSON_FAULT_OPEN) : (bXmlRpc ? XML_OK_OPEN : JSON_OK_OPEN);
	const char* szCloseTag = bFault ? (bXmlRpc ? XML_FAULT_CLOSE : JSON_FAULT_CLOSE ) : (bXmlRpc ? XML_OK_CLOSE : JSON_OK_CLOSE);
	int iHeaderLen = (bXmlRpc ? sizeof(XML_HEADER) : sizeof(JSON_HEADER)) - 1;
	int iFooterLen = (bXmlRpc ? sizeof(XML_FOOTER) : sizeof(JSON_FOOTER)) - 1;
	int iOpenTagLen = (bFault ? (bXmlRpc ? sizeof(XML_FAULT_OPEN) : sizeof(JSON_FAULT_OPEN)) : (bXmlRpc ? sizeof(XML_OK_OPEN) : sizeof(JSON_OK_OPEN))) - 1;
	int iCloseTagLen = (bFault ? (bXmlRpc ? sizeof(XML_FAULT_CLOSE) : sizeof(JSON_FAULT_CLOSE)) : (bXmlRpc ? sizeof(XML_OK_CLOSE) : sizeof(JSON_OK_CLOSE))) - 1;

	debug("Response=%s", szResponse);
	int iResponseLen = strlen(szResponse);

	char szResponseHeader[1024];
	int iBodyLen = iResponseLen + iHeaderLen + iFooterLen + iOpenTagLen + iCloseTagLen;
	snprintf(szResponseHeader, 1024, bXmlRpc ? XML_RESPONSE_HEADER : JSON_RESPONSE_HEADER, iBodyLen, Util::VersionRevision());

	// Send the request answer
	m_pConnection->Send(szResponseHeader, strlen(szResponseHeader));
	m_pConnection->Send(szHeader, iHeaderLen);
	m_pConnection->Send(szOpenTag, iOpenTagLen);
	m_pConnection->Send(szResponse, iResponseLen);
	m_pConnection->Send(szCloseTag, iCloseTagLen);
	m_pConnection->Send(szFooter, iFooterLen);
}

XmlCommand* XmlRpcProcessor::CreateCommand(const char* szMethodName)
{
	XmlCommand* command = NULL;

	if (!strcasecmp(szMethodName, "pause"))
	{
		command = new PauseUnpauseXmlCommand(true, PauseUnpauseXmlCommand::paDownload);
	}
	else if (!strcasecmp(szMethodName, "resume"))
	{
		command = new PauseUnpauseXmlCommand(false, PauseUnpauseXmlCommand::paDownload);
	}
	else if (!strcasecmp(szMethodName, "shutdown"))
	{
		command = new ShutdownXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "version"))
	{
		command = new VersionXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "dump"))
	{
		command = new DumpDebugXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "rate"))
	{
		command = new SetDownloadRateXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "status"))
	{
		command = new StatusXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "log"))
	{
		command = new LogXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "listfiles"))
	{
		command = new ListFilesXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "listgroups"))
	{
		command = new ListGroupsXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "editqueue"))
	{
		command = new EditQueueXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "append"))
	{
		command = new DownloadXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "postqueue"))
	{
		command = new PostQueueXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "writelog"))
	{
		command = new WriteLogXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "scan"))
	{
		command = new ScanXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "pausepost"))
	{
		command = new PauseUnpauseXmlCommand(true, PauseUnpauseXmlCommand::paPostProcess);
	}
	else if (!strcasecmp(szMethodName, "resumepost"))
	{
		command = new PauseUnpauseXmlCommand(false, PauseUnpauseXmlCommand::paPostProcess);
	}
	else if (!strcasecmp(szMethodName, "pausescan"))
	{
		command = new PauseUnpauseXmlCommand(true, PauseUnpauseXmlCommand::paScan);
	}
	else if (!strcasecmp(szMethodName, "resumescan"))
	{
		command = new PauseUnpauseXmlCommand(false, PauseUnpauseXmlCommand::paScan);
	}
	else if (!strcasecmp(szMethodName, "history"))
	{
		command = new HistoryXmlCommand();
	}
	else 
	{
		command = new ErrorXmlCommand(1, "Invalid procedure");
	}

	return command;
}


//*****************************************************************
// Base command

XmlCommand::XmlCommand()
{
	m_szRequest = NULL;
	m_szRequestPtr = NULL;
	m_bFault = false;
	m_eProtocol = XmlRpcProcessor::rpUndefined;
}

void XmlCommand::AppendResponse(const char* szPart)
{
	m_StringBuilder.Append(szPart);
}

void XmlCommand::BuildErrorResponse(int iErrCode, const char* szErrText)
{
	const char* XML_RESPONSE_ERROR_BODY = 
		"<struct>\n"
		"<member><name>faultCode</name><value><i4>%i</i4></value></member>\n"
		"<member><name>faultString</name><value><string>%s</string></value></member>\n"
		"</struct>\n";

	const char* JSON_RESPONSE_ERROR_BODY = 
		"{\n"
        "\"name\" : \"JSONRPCError\",\n"
        "\"code\" : %i,\n"
        "\"message\" : \"%s\"\n"
        "}";

	char szContent[1024];
	snprintf(szContent, 1024, IsJson() ? JSON_RESPONSE_ERROR_BODY : XML_RESPONSE_ERROR_BODY,
		iErrCode, szErrText);
	szContent[1024-1] = '\0';

	AppendResponse(szContent);

	m_bFault = true;
}

void XmlCommand::BuildBoolResponse(bool bOK)
{
	const char* XML_RESPONSE_BOOL_BODY = "<boolean>%s</boolean>";
	const char* JSON_RESPONSE_BOOL_BODY = "%s";

	char szContent[1024];
	snprintf(szContent, 1024, IsJson() ? JSON_RESPONSE_BOOL_BODY : XML_RESPONSE_BOOL_BODY,
		BoolToStr(bOK));
	szContent[1024-1] = '\0';

	AppendResponse(szContent);
}

void XmlCommand::PrepareParams()
{
	if (IsJson() && m_eHttpMethod == XmlRpcProcessor::hmPost)
	{
		char* szParams = strstr(m_szRequestPtr, "\"params\"");
		if (!szParams)
		{
			m_szRequestPtr[0] = '\0';
			return;
		}
		m_szRequestPtr = szParams + 8; // strlen("\"params\"")
	}
}

bool XmlCommand::NextParamAsInt(int* iValue)
{
	if (m_eHttpMethod == XmlRpcProcessor::hmGet)
	{
		char* szParam = strchr(m_szRequestPtr, '=');
		if (!szParam)
		{
			return false;
		}
		*iValue = atoi(szParam + 1);
		m_szRequestPtr = szParam + 1;
		return true;
	}
	else if (IsJson())
	{
		int iLen = 0;
		char* szParam = (char*)Util::JsonNextValue(m_szRequestPtr, &iLen);
		if (!szParam || !strchr("-+0123456789", *szParam))
		{
			return false;
		}
		*iValue = atoi(szParam);
		m_szRequestPtr = szParam + iLen + 1;
		return true;
	}
	else
	{
		int iLen = 0;
		int iTagLen = 4; //strlen("<i4>");
		char* szParam = (char*)Util::XmlFindTag(m_szRequestPtr, "i4", &iLen);
		if (!szParam)
		{
			szParam = (char*)Util::XmlFindTag(m_szRequestPtr, "int", &iLen);
			iTagLen = 5; //strlen("<int>");
		}
		if (!szParam || !strchr("-+0123456789", *szParam))
		{
			return false;
		}
		*iValue = atoi(szParam);
		m_szRequestPtr = szParam + iLen + iTagLen;
		return true;
	}
}

bool XmlCommand::NextParamAsBool(bool* bValue)
{
	if (m_eHttpMethod == XmlRpcProcessor::hmGet)
	{
		return false;
	}
	else if (IsJson())
	{
		int iLen = 0;
		char* szParam = (char*)Util::JsonNextValue(m_szRequestPtr, &iLen);
		if (!szParam)
		{
			return false;
		}
		if (iLen == 4 && !strncmp(szParam, "true", 4))
		{
			*bValue = true;
			m_szRequestPtr = szParam + iLen + 1;
			return true;
		}
		else if (iLen == 5 && !strncmp(szParam, "false", 5))
		{
			*bValue = false;
			m_szRequestPtr = szParam + iLen + 1;
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		int iLen = 0;
		char* szParam = (char*)Util::XmlFindTag(m_szRequestPtr, "boolean", &iLen);
		if (!szParam)
		{
			return false;
		}
		*bValue = szParam[0] == '1';
		m_szRequestPtr = szParam + iLen + 9; //strlen("<boolean>");
		return true;
	}
}

bool XmlCommand::NextParamAsStr(char** szValue)
{
	if (m_eHttpMethod == XmlRpcProcessor::hmGet)
	{
		return false;
	}
	else if (IsJson())
	{
		int iLen = 0;
		char* szParam = (char*)Util::JsonNextValue(m_szRequestPtr, &iLen);
		if (!szParam || iLen < 2 || szParam[0] != '"' || szParam[iLen - 1] != '"')
		{
			return false;
		}
		szParam++; // skip first '"'
		szParam[iLen - 2] = '\0'; // skip last '"'
		m_szRequestPtr = szParam + iLen;
		*szValue = szParam;
		return true;
	}
	else
	{
		int iLen = 0;
		char* szParam = (char*)Util::XmlFindTag(m_szRequestPtr, "string", &iLen);
		if (!szParam)
		{
			return false;
		}
		szParam[iLen] = '\0';
		m_szRequestPtr = szParam + iLen + 8; //strlen("<string>")
		*szValue = szParam;
		return true;
	}
}

const char* XmlCommand::BoolToStr(bool bValue)
{
	return IsJson() ? (bValue ? "true" : "false") : (bValue ? "1" : "0");
}

char* XmlCommand::EncodeStr(const char* szStr)
{
	if (!szStr)
	{
		return strdup("");
	}

	if (IsJson()) 
	{
		return Util::JsonEncode(szStr);
	}
	else
	{
		return Util::XmlEncode(szStr);
	}
}

//*****************************************************************
// Commands

ErrorXmlCommand::ErrorXmlCommand(int iErrCode, const char* szErrText)
{
	m_iErrCode = iErrCode;
	m_szErrText = szErrText;
}

void ErrorXmlCommand::Execute()
{
	error("Received unsupported request: %s", m_szErrText);
	BuildErrorResponse(m_iErrCode, m_szErrText);
}

PauseUnpauseXmlCommand::PauseUnpauseXmlCommand(bool bPause, EPauseAction eEPauseAction)
{
	m_bPause = bPause;
	m_eEPauseAction = eEPauseAction;
}

void PauseUnpauseXmlCommand::Execute()
{
	if (m_eHttpMethod == XmlRpcProcessor::hmGet)
	{
		BuildErrorResponse(4, "Not safe procedure for HTTP-Method GET. Use Method POST instead");
		return;
	}

	bool bOK = true;

	switch (m_eEPauseAction)
	{
		case paDownload:
			g_pOptions->SetPauseDownload(m_bPause);
			break;

		case paPostProcess:
			g_pOptions->SetPausePostProcess(m_bPause);
			break;

		case paScan:
			g_pOptions->SetPauseScan(m_bPause);
			break;

		default:
			bOK = false;
	}

	BuildBoolResponse(bOK);
}

void ShutdownXmlCommand::Execute()
{
	if (m_eHttpMethod == XmlRpcProcessor::hmGet)
	{
		BuildErrorResponse(4, "Not safe procedure for HTTP-Method GET. Use Method POST instead");
		return;
	}

	BuildBoolResponse(true);
	ExitProc();
}

void VersionXmlCommand::Execute()
{
	const char* XML_RESPONSE_STRING_BODY = "<string>%s</string>";
	const char* JSON_RESPONSE_STRING_BODY = "\"%s\"";

	char szContent[1024];
	snprintf(szContent, 1024, IsJson() ? JSON_RESPONSE_STRING_BODY : XML_RESPONSE_STRING_BODY, Util::VersionRevision());
	szContent[1024-1] = '\0';

	AppendResponse(szContent);
}

void DumpDebugXmlCommand::Execute()
{
	g_pQueueCoordinator->LogDebugInfo();
	BuildBoolResponse(true);
}

void SetDownloadRateXmlCommand::Execute()
{
	if (m_eHttpMethod == XmlRpcProcessor::hmGet)
	{
		BuildErrorResponse(4, "Not safe procedure for HTTP-Method GET. Use Method POST instead");
		return;
	}

	int iRate = 0;
	if (!NextParamAsInt(&iRate) || iRate < 0)
	{
		BuildErrorResponse(2, "Invalid parameter");
		return;
	}

	g_pOptions->SetDownloadRate((float)iRate);
	BuildBoolResponse(true);
}

void StatusXmlCommand::Execute()
{
	const char* XML_RESPONSE_STATUS_BODY = 
		"<struct>\n"
		"<member><name>RemainingSizeLo</name><value><i4>%i</i4></value></member>\n"
		"<member><name>RemainingSizeHi</name><value><i4>%i</i4></value></member>\n"
		"<member><name>RemainingSizeMB</name><value><i4>%i</i4></value></member>\n"
		"<member><name>DownloadedSizeLo</name><value><i4>%i</i4></value></member>\n"
		"<member><name>DownloadedSizeHi</name><value><i4>%i</i4></value></member>\n"
		"<member><name>DownloadedSizeMB</name><value><i4>%i</i4></value></member>\n"
		"<member><name>DownloadRate</name><value><i4>%i</i4></value></member>\n"
		"<member><name>AverageDownloadRate</name><value><i4>%i</i4></value></member>\n"
		"<member><name>DownloadLimit</name><value><i4>%i</i4></value></member>\n"
		"<member><name>ThreadCount</name><value><i4>%i</i4></value></member>\n"
		"<member><name>ParJobCount</name><value><i4>%i</i4></value></member>\n"
		"<member><name>UpTimeSec</name><value><i4>%i</i4></value></member>\n"
		"<member><name>DownloadTimeSec</name><value><i4>%i</i4></value></member>\n"
		"<member><name>ServerPaused</name><value><boolean>%s</boolean></value></member>\n"
		"<member><name>ServerStandBy</name><value><boolean>%s</boolean></value></member>\n"
		"<member><name>PostPaused</name><value><boolean>%s</boolean></value></member>\n"
		"<member><name>ScanPaused</name><value><boolean>%s</boolean></value></member>\n"
		"</struct>\n";

	const char* JSON_RESPONSE_STATUS_BODY = 
		"{\n"
		"\"RemainingSizeLo\" : %i,\n"
		"\"RemainingSizeHi\" : %i,\n"
		"\"RemainingSizeMB\" : %i,\n"
		"\"DownloadedSizeLo\" : %i,\n"
		"\"DownloadedSizeHi\" : %i,\n"
		"\"DownloadedSizeMB\" : %i,\n"
		"\"DownloadRate\" : %i,\n"
		"\"AverageDownloadRate\" : %i,\n"
		"\"DownloadLimit\" : %i,\n"
		"\"ThreadCount\" : %i,\n"
		"\"ParJobCount\" : %i,\n"
		"\"UpTimeSec\" : %i,\n"
		"\"DownloadTimeSec\" : %i,\n"
		"\"ServerPaused\" : %s,\n"
		"\"ServerStandBy\" : %s,\n"
		"\"PostPaused\" : %s,\n"
		"\"ScanPaused\" : %s\n"
		"}\n";

	unsigned long iRemainingSizeHi, iRemainingSizeLo;
	int iDownloadRate = (int)(g_pQueueCoordinator->CalcCurrentDownloadSpeed() * 1024);
	long long iRemainingSize = g_pQueueCoordinator->CalcRemainingSize();
	Util::SplitInt64(iRemainingSize, &iRemainingSizeHi, &iRemainingSizeLo);
	int iRemainingMBytes = (int)(iRemainingSize / 1024 / 1024);
	int iDownloadLimit = (int)(g_pOptions->GetDownloadRate() * 1024);
	bool bServerPaused = g_pOptions->GetPauseDownload();
	bool bPostPaused = g_pOptions->GetPausePostProcess();
	bool bScanPaused = g_pOptions->GetPauseScan();
	int iThreadCount = Thread::GetThreadCount() - 1; // not counting itself
	PostQueue* pPostQueue = g_pQueueCoordinator->LockQueue()->GetPostQueue();
	int iParJobCount = pPostQueue->size();
	g_pQueueCoordinator->UnlockQueue();
	unsigned long iDownloadedSizeHi, iDownloadedSizeLo;
	int iUpTimeSec, iDownloadTimeSec;
	long long iAllBytes;
	bool bServerStandBy;
	g_pQueueCoordinator->CalcStat(&iUpTimeSec, &iDownloadTimeSec, &iAllBytes, &bServerStandBy);
	int iDownloadedMBytes = (int)(iAllBytes / 1024 / 1024);
	Util::SplitInt64(iAllBytes, &iDownloadedSizeHi, &iDownloadedSizeLo);
	int iAverageDownloadRate = (int)(iDownloadTimeSec > 0 ? iAllBytes / iDownloadTimeSec : 0);

	char szContent[2048];
	snprintf(szContent, 2048, IsJson() ? JSON_RESPONSE_STATUS_BODY : XML_RESPONSE_STATUS_BODY, 
		iRemainingSizeLo, iRemainingSizeHi,	iRemainingMBytes, iDownloadedSizeLo, iDownloadedSizeHi, 
		iDownloadedMBytes, iDownloadRate, iAverageDownloadRate, iDownloadLimit,	iThreadCount, 
		iParJobCount, iUpTimeSec, iDownloadTimeSec, BoolToStr(bServerPaused), BoolToStr(bServerStandBy),
		BoolToStr(bPostPaused), BoolToStr(bScanPaused));
	szContent[2048-1] = '\0';

	AppendResponse(szContent);
}

void LogXmlCommand::Execute()
{
	int iIDFrom = 0;
	int iNrEntries = 0;
	if (!NextParamAsInt(&iIDFrom) || !NextParamAsInt(&iNrEntries) || (iNrEntries > 0 && iIDFrom > 0))
	{
		BuildErrorResponse(2, "Invalid parameter");
		return;
	}

	debug("iIDFrom=%i", iIDFrom);
	debug("iNrEntries=%i", iNrEntries);

	AppendResponse(IsJson() ? "[\n" : "<array><data>\n");
	Log::Messages* pMessages = g_pLog->LockMessages();

	int iStart = pMessages->size();
	if (iNrEntries > 0)
	{
		if (iNrEntries > (int)pMessages->size())
		{
			iNrEntries = pMessages->size();
		}
		iStart = pMessages->size() - iNrEntries;
	}
	if (iIDFrom > 0 && !pMessages->empty())
	{
		iStart = iIDFrom - pMessages->front()->GetID();
		if (iStart < 0)
		{
			iStart = 0;
		}
		iNrEntries = pMessages->size() - iStart;
		if (iNrEntries < 0)
		{
			iNrEntries = 0;
		}
	}

	const char* XML_LOG_ITEM = 
		"<value><struct>\n"
		"<member><name>ID</name><value><i4>%i</i4></value></member>\n"
		"<member><name>Kind</name><value><string>%s</string></value></member>\n"
		"<member><name>Time</name><value><i4>%i</i4></value></member>\n"
		"<member><name>Text</name><value><string>%s</string></value></member>\n"
		"</struct></value>\n";

	const char* JSON_LOG_ITEM = 
		"{\n"
		"\"ID\" : %i,\n"
		"\"Kind\" : \"%s\",\n"
		"\"Time\" : %i,\n"
		"\"Text\" : \"%s\"\n"
		"}";

    const char* szMessageType[] = { "INFO", "WARNING", "ERROR", "DEBUG", "DETAIL" };

	int szItemBufSize = 10240;
	char* szItemBuf = (char*)malloc(szItemBufSize);
	int index = 0;

	for (unsigned int i = (unsigned int)iStart; i < pMessages->size(); i++)
	{
		Message* pMessage = (*pMessages)[i];
		char* xmltext = EncodeStr(pMessage->GetText());
		snprintf(szItemBuf, szItemBufSize, IsJson() ? JSON_LOG_ITEM : XML_LOG_ITEM,
			pMessage->GetID(), szMessageType[pMessage->GetKind()], pMessage->GetTime(), xmltext);
		szItemBuf[szItemBufSize-1] = '\0';
		free(xmltext);

		if (IsJson() && index++ > 0)
		{
			AppendResponse(",\n");
		}
		AppendResponse(szItemBuf);
	}

	free(szItemBuf);

	g_pLog->UnlockMessages();
	AppendResponse(IsJson() ? "\n]" : "</data></array>\n");
}

void ListFilesXmlCommand::Execute()
{
	int iIDStart = 0;
	int iIDEnd = 0;
	if (NextParamAsInt(&iIDStart) && (!NextParamAsInt(&iIDEnd) || iIDEnd < iIDStart))
	{
		BuildErrorResponse(2, "Invalid parameter");
		return;
	}

	debug("iIDStart=%i", iIDStart);
	debug("iIDEnd=%i", iIDEnd);

	AppendResponse(IsJson() ? "[\n" : "<array><data>\n");
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	const char* XML_LIST_ITEM = 
		"<value><struct>\n"
		"<member><name>ID</name><value><i4>%i</i4></value></member>\n"
		"<member><name>FileSizeLo</name><value><i4>%i</i4></value></member>\n"
		"<member><name>FileSizeHi</name><value><i4>%i</i4></value></member>\n"
		"<member><name>RemainingSizeLo</name><value><i4>%i</i4></value></member>\n"
		"<member><name>RemainingSizeHi</name><value><i4>%i</i4></value></member>\n"
		"<member><name>PostTime</name><value><i4>%i</i4></value></member>\n"
		"<member><name>FilenameConfirmed</name><value><boolean>%s</boolean></value></member>\n"
		"<member><name>Paused</name><value><boolean>%s</boolean></value></member>\n"
		"<member><name>NZBID</name><value><i4>%i</i4></value></member>\n"
		"<member><name>NZBNicename</name><value><string>%s</string></value></member>\n"
		"<member><name>NZBFilename</name><value><string>%s</string></value></member>\n"
		"<member><name>Subject</name><value><string>%s</string></value></member>\n"
		"<member><name>Filename</name><value><string>%s</string></value></member>\n"
		"<member><name>DestDir</name><value><string>%s</string></value></member>\n"
		"<member><name>Category</name><value><string>%s</string></value></member>\n"
		"</struct></value>\n";

	const char* JSON_LIST_ITEM = 
		"{\n"
		"\"ID\" : %i,\n"
		"\"FileSizeLo\" : %i,\n"
		"\"FileSizeHi\" : %i,\n"
		"\"RemainingSizeLo\" : %i,\n"
		"\"RemainingSizeHi\" : %i,\n"
		"\"PostTime\" : %i,\n"
		"\"FilenameConfirmed\" : %s,\n"
		"\"Paused\" : %s,\n"
		"\"NZBID\" : %i,\n"
		"\"NZBNicename\" : \"%s\",\n"
		"\"NZBFilename\" : \"%s\",\n"
		"\"Subject\" : \"%s\",\n"
		"\"Filename\" : \"%s\",\n"
		"\"DestDir\" : \"%s\",\n"
		"\"Category\" : \"%s\"\n"
		"}";

	int szItemBufSize = 10240;
	char* szItemBuf = (char*)malloc(szItemBufSize);
	int index = 0;

	for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (iIDStart == 0 || (iIDStart <= pFileInfo->GetID() && pFileInfo->GetID() <= iIDEnd))
		{
			unsigned long iFileSizeHi, iFileSizeLo;
			unsigned long iRemainingSizeLo, iRemainingSizeHi;
			char szNZBNicename[1024];
			Util::SplitInt64(pFileInfo->GetSize(), &iFileSizeHi, &iFileSizeLo);
			Util::SplitInt64(pFileInfo->GetRemainingSize(), &iRemainingSizeHi, &iRemainingSizeLo);
			pFileInfo->GetNZBInfo()->GetNiceNZBName(szNZBNicename, sizeof(szNZBNicename));
			char* xmlNZBFilename = EncodeStr(pFileInfo->GetNZBInfo()->GetFilename());
			char* xmlSubject = EncodeStr(pFileInfo->GetSubject());
			char* xmlFilename = EncodeStr(pFileInfo->GetFilename());
			char* xmlDestDir = EncodeStr(pFileInfo->GetNZBInfo()->GetDestDir());
			char* xmlCategory = EncodeStr(pFileInfo->GetNZBInfo()->GetCategory());
			char* xmlNZBNicename = EncodeStr(szNZBNicename);

			snprintf(szItemBuf, szItemBufSize, IsJson() ? JSON_LIST_ITEM : XML_LIST_ITEM,
				pFileInfo->GetID(), iFileSizeLo, iFileSizeHi, iRemainingSizeLo, iRemainingSizeHi, 
				pFileInfo->GetTime(), BoolToStr(pFileInfo->GetFilenameConfirmed()), 
				BoolToStr(pFileInfo->GetPaused()), pFileInfo->GetNZBInfo()->GetID(), 
				xmlNZBNicename, xmlNZBFilename, xmlSubject, xmlFilename, xmlDestDir, xmlCategory);
			szItemBuf[szItemBufSize-1] = '\0';

			free(xmlNZBFilename);
			free(xmlSubject);
			free(xmlFilename);
			free(xmlDestDir);
			free(xmlCategory);
			free(xmlNZBNicename);

			if (IsJson() && index++ > 0)
			{
				AppendResponse(",\n");
			}
			AppendResponse(szItemBuf);
		}
	}
	free(szItemBuf);

	g_pQueueCoordinator->UnlockQueue();
	AppendResponse(IsJson() ? "\n]" : "</data></array>\n");
}

void ListGroupsXmlCommand::Execute()
{
	AppendResponse(IsJson() ? "[\n" : "<array><data>\n");

	const char* XML_LIST_ITEM_START = 
		"<value><struct>\n"
		"<member><name>FirstID</name><value><i4>%i</i4></value></member>\n"
		"<member><name>LastID</name><value><i4>%i</i4></value></member>\n"
		"<member><name>FileSizeLo</name><value><i4>%i</i4></value></member>\n"
		"<member><name>FileSizeHi</name><value><i4>%i</i4></value></member>\n"
		"<member><name>FileSizeMB</name><value><i4>%i</i4></value></member>\n"
		"<member><name>RemainingSizeLo</name><value><i4>%i</i4></value></member>\n"
		"<member><name>RemainingSizeHi</name><value><i4>%i</i4></value></member>\n"
		"<member><name>RemainingSizeMB</name><value><i4>%i</i4></value></member>\n"
		"<member><name>PausedSizeLo</name><value><i4>%i</i4></value></member>\n"
		"<member><name>PausedSizeHi</name><value><i4>%i</i4></value></member>\n"
		"<member><name>PausedSizeMB</name><value><i4>%i</i4></value></member>\n"
		"<member><name>FileCount</name><value><i4>%i</i4></value></member>\n"
		"<member><name>RemainingFileCount</name><value><i4>%i</i4></value></member>\n"
		"<member><name>RemainingParCount</name><value><i4>%i</i4></value></member>\n"
		"<member><name>MinPostTime</name><value><i4>%i</i4></value></member>\n"
		"<member><name>MaxPostTime</name><value><i4>%i</i4></value></member>\n"
		"<member><name>NZBID</name><value><i4>%i</i4></value></member>\n"
		"<member><name>NZBNicename</name><value><string>%s</string></value></member>\n"
		"<member><name>NZBFilename</name><value><string>%s</string></value></member>\n"
		"<member><name>DestDir</name><value><string>%s</string></value></member>\n"
		"<member><name>Category</name><value><string>%s</string></value></member>\n"
		"<member><name>Parameters</name><value><array><data>\n";

	const char* XML_LIST_ITEM_END = 
		"</data></array></value></member>\n"
		"</struct></value>\n";

	const char* JSON_LIST_ITEM_START = 
		"{\n"
		"\"FirstID\" : %i,\n"
		"\"LastID\" : %i,\n"
		"\"FileSizeLo\" : %i,\n"
		"\"FileSizeHi\" : %i,\n"
		"\"FileSizeMB\" : %i,\n"
		"\"RemainingSizeLo\" : %i,\n"
		"\"RemainingSizeHi\" : %i,\n"
		"\"RemainingSizeMB\" : %i,\n"
		"\"PausedSizeLo\" : %i,\n"
		"\"PausedSizeHi\" : %i,\n"
		"\"PausedSizeMB\" : %i,\n"
		"\"FileCount\" : %i,\n"
		"\"RemainingFileCount\" : %i,\n"
		"\"RemainingParCount\" : %i,\n"
		"\"MinPostTime\" : %i,\n"
		"\"MaxPostTime\" : %i,\n"
		"\"NZBID\" : %i,\n"
		"\"NZBNicename\" : \"%s\",\n"
		"\"NZBFilename\" : \"%s\",\n"
		"\"DestDir\" : \"%s\",\n"
		"\"Category\" : \"%s\",\n"
		"\"Parameters\" : [\n";

	const char* JSON_LIST_ITEM_END = 
		"]\n"
		"}";

	const char* XML_PARAMETER_ITEM = 
		"<value><struct>\n"
		"<member><name>Name</name><value><string>%s</string></value></member>\n"
		"<member><name>Value</name><value><string>%s</string></value></member>\n"
		"</struct></value>\n";

	const char* JSON_PARAMETER_ITEM = 
		"{\n"
		"\"Name\" : \"%s\",\n"
		"\"Value\" : \"%s\"\n"
		"}";

	GroupQueue groupQueue;
	groupQueue.clear();
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();
	pDownloadQueue->BuildGroups(&groupQueue);
	g_pQueueCoordinator->UnlockQueue();

	int szItemBufSize = 10240;
	char* szItemBuf = (char*)malloc(szItemBufSize);
	int index = 0;

	for (GroupQueue::iterator it = groupQueue.begin(); it != groupQueue.end(); it++)
	{
		GroupInfo* pGroupInfo = *it;
		unsigned long iFileSizeHi, iFileSizeLo, iFileSizeMB;
		unsigned long iRemainingSizeLo, iRemainingSizeHi, iRemainingSizeMB;
		unsigned long iPausedSizeLo, iPausedSizeHi, iPausedSizeMB;
		char szNZBNicename[1024];
		Util::SplitInt64(pGroupInfo->GetNZBInfo()->GetSize(), &iFileSizeHi, &iFileSizeLo);
		iFileSizeMB = (int)(pGroupInfo->GetNZBInfo()->GetSize() / 1024 / 1024);
		Util::SplitInt64(pGroupInfo->GetRemainingSize(), &iRemainingSizeHi, &iRemainingSizeLo);
		iRemainingSizeMB = (int)(pGroupInfo->GetRemainingSize() / 1024 / 1024);
		Util::SplitInt64(pGroupInfo->GetPausedSize(), &iPausedSizeHi, &iPausedSizeLo);
		iPausedSizeMB = (int)(pGroupInfo->GetPausedSize() / 1024 / 1024);
		pGroupInfo->GetNZBInfo()->GetNiceNZBName(szNZBNicename, sizeof(szNZBNicename));

		char* xmlNZBNicename = EncodeStr(szNZBNicename);
		char* xmlNZBFilename = EncodeStr(pGroupInfo->GetNZBInfo()->GetFilename());
		char* xmlDestDir = EncodeStr(pGroupInfo->GetNZBInfo()->GetDestDir());
		char* xmlCategory = EncodeStr(pGroupInfo->GetNZBInfo()->GetCategory());

		snprintf(szItemBuf, szItemBufSize, IsJson() ? JSON_LIST_ITEM_START : XML_LIST_ITEM_START,
			pGroupInfo->GetFirstID(), pGroupInfo->GetLastID(), iFileSizeLo, iFileSizeHi, iFileSizeMB, 
			iRemainingSizeLo, iRemainingSizeHi, iRemainingSizeMB, iPausedSizeLo, iPausedSizeHi, iPausedSizeMB, 
			pGroupInfo->GetNZBInfo()->GetFileCount(), pGroupInfo->GetRemainingFileCount(), 
			pGroupInfo->GetRemainingParCount(), pGroupInfo->GetMinTime(), pGroupInfo->GetMaxTime(),
			pGroupInfo->GetNZBInfo()->GetID(), xmlNZBNicename, xmlNZBFilename, xmlDestDir, xmlCategory);
		szItemBuf[szItemBufSize-1] = '\0';

		free(xmlNZBNicename);
		free(xmlNZBFilename);
		free(xmlDestDir);
		free(xmlCategory);

		if (IsJson() && index++ > 0)
		{
			AppendResponse(",\n");
		}
		AppendResponse(szItemBuf);

		int iParamIndex = 0;

		for (NZBParameterList::iterator it = pGroupInfo->GetNZBInfo()->GetParameters()->begin(); it != pGroupInfo->GetNZBInfo()->GetParameters()->end(); it++)
		{
			NZBParameter* pParameter = *it;

			char* xmlName = EncodeStr(pParameter->GetName());
			char* xmlValue = EncodeStr(pParameter->GetValue());

			snprintf(szItemBuf, szItemBufSize, IsJson() ? JSON_PARAMETER_ITEM : XML_PARAMETER_ITEM, xmlName, xmlValue);
			szItemBuf[szItemBufSize-1] = '\0';

			free(xmlName);
			free(xmlValue);

			if (IsJson() && iParamIndex++ > 0)
			{
				AppendResponse(",\n");
			}
			AppendResponse(szItemBuf);
		}

		AppendResponse(IsJson() ? JSON_LIST_ITEM_END : XML_LIST_ITEM_END);
	}
	free(szItemBuf);

	AppendResponse(IsJson() ? "\n]" : "</data></array>\n");

	for (GroupQueue::iterator it = groupQueue.begin(); it != groupQueue.end(); it++)
	{
		delete *it;
	}
	groupQueue.clear();
}

typedef struct 
{
	int				iActionID;
	const char*		szActionName;
} EditCommandEntry;

EditCommandEntry EditCommandNameMap[] = { 
	{ QueueEditor::eaFileMoveOffset, "FileMoveOffset" },
	{ QueueEditor::eaFileMoveTop, "FileMoveTop" },
	{ QueueEditor::eaFileMoveBottom, "FileMoveBottom" },
	{ QueueEditor::eaFilePause, "FilePause" },
	{ QueueEditor::eaFileResume, "FileResume" },
	{ QueueEditor::eaFileDelete, "FileDelete" },
	{ QueueEditor::eaFilePauseAllPars, "FilePauseAllPars" },
	{ QueueEditor::eaFilePauseExtraPars, "FilePauseExtraPars" },
	{ QueueEditor::eaGroupMoveOffset, "GroupMoveOffset" },
	{ QueueEditor::eaGroupMoveTop, "GroupMoveTop" },
	{ QueueEditor::eaGroupMoveBottom, "GroupMoveBottom" },
	{ QueueEditor::eaGroupPause, "GroupPause" },
	{ QueueEditor::eaGroupResume, "GroupResume" },
	{ QueueEditor::eaGroupDelete, "GroupDelete" },
	{ QueueEditor::eaGroupPauseAllPars, "GroupPauseAllPars" },
	{ QueueEditor::eaGroupPauseExtraPars, "GroupPauseExtraPars" },
	{ QueueEditor::eaGroupSetCategory, "GroupSetCategory" },
	{ QueueEditor::eaGroupMerge, "GroupMerge" },
	{ QueueEditor::eaGroupSetParameter, "GroupSetParameter" },
	{ PrePostProcessor::eaPostMoveOffset, "PostMoveOffset" },
	{ PrePostProcessor::eaPostMoveTop, "PostMoveTop" },
	{ PrePostProcessor::eaPostMoveBottom, "PostMoveBottom" },
	{ PrePostProcessor::eaPostDelete, "PostDelete" },
	{ PrePostProcessor::eaHistoryDelete, "HistoryDelete" },
	{ PrePostProcessor::eaHistoryDelete, "HistoryReturn" },
	{ PrePostProcessor::eaHistoryDelete, "HistoryProcess" },
	{ 0, NULL }
};

void EditQueueXmlCommand::Execute()
{
	if (m_eHttpMethod == XmlRpcProcessor::hmGet)
	{
		BuildErrorResponse(4, "Not safe procedure for HTTP-Method GET. Use Method POST instead");
		return;
	}

	char* szEditCommand;
	if (!NextParamAsStr(&szEditCommand))
	{
		BuildErrorResponse(2, "Invalid parameter");
		return;
	}
	debug("EditCommand=%s", szEditCommand);

	int iAction = -1;
	for (int i = 0; const char* szName = EditCommandNameMap[i].szActionName; i++)
	{
		if (!strcasecmp(szEditCommand, szName))
		{
			iAction = EditCommandNameMap[i].iActionID;
			break;
		}
	}

	if (iAction == -1)
	{
		BuildErrorResponse(3, "Invalid action");
		return;
	}

	int iOffset = 0;
	if (!NextParamAsInt(&iOffset))
	{
		BuildErrorResponse(2, "Invalid parameter");
		return;
	}

	char* szEditText;
	if (!NextParamAsStr(&szEditText))
	{
		BuildErrorResponse(2, "Invalid parameter");
		return;
	}
	debug("EditText=%s", szEditText);

	if (IsJson())
	{
		Util::JsonDecode(szEditText);
	}
	else
	{
		Util::XmlDecode(szEditText);
	}

	IDList cIDList;
	int iID = 0;
	while (NextParamAsInt(&iID))
	{
		cIDList.push_back(iID);
	}

	bool bOK = false;

	if (iAction < PrePostProcessor::eaPostMoveOffset)
	{
		bOK = g_pQueueCoordinator->GetQueueEditor()->EditList(&cIDList, true, (QueueEditor::EEditAction)iAction, iOffset, szEditText);
	}
	else
	{
		bOK = g_pPrePostProcessor->QueueEditList(&cIDList, (PrePostProcessor::EEditAction)iAction, iOffset);
	}

	BuildBoolResponse(bOK);
}

void DownloadXmlCommand::Execute()
{
	if (m_eHttpMethod == XmlRpcProcessor::hmGet)
	{
		BuildErrorResponse(4, "Not safe procedure for HTTP-Method GET. Use Method POST instead");
		return;
	}

	char* szFileName;
	if (!NextParamAsStr(&szFileName))
	{
		BuildErrorResponse(2, "Invalid parameter");
		return;
	}

	char* szCategory;
	if (!NextParamAsStr(&szCategory))
	{
		BuildErrorResponse(2, "Invalid parameter");
		return;
	}

	if (IsJson())
	{
		Util::JsonDecode(szFileName);
		Util::JsonDecode(szCategory);
	}
	else
	{
		Util::XmlDecode(szFileName);
		Util::XmlDecode(szCategory);
	}

	debug("FileName=%s", szFileName);

	bool bAddTop;
	if (!NextParamAsBool(&bAddTop))
	{
		BuildErrorResponse(2, "Invalid parameter");
		return;
	}

	char* szFileContent;
	if (!NextParamAsStr(&szFileContent))
	{
		BuildErrorResponse(2, "Invalid parameter");
		return;
	}

	if (IsJson())
	{
		// JSON-string may contain '/'-character used in Base64, which must be escaped in JSON
		Util::JsonDecode(szFileContent);
	}

	int iLen = Util::DecodeBase64(szFileContent, 0, szFileContent);
	szFileContent[iLen] = '\0';
	//debug("FileContent=%s", szFileContent);

	NZBFile* pNZBFile = NZBFile::CreateFromBuffer(szFileName, szCategory, szFileContent, iLen);

	if (pNZBFile)
	{
		info("Request: Queue collection %s", szFileName);
		g_pQueueCoordinator->AddNZBFileToQueue(pNZBFile, bAddTop);
		delete pNZBFile;
		BuildBoolResponse(true);
	}
	else
	{
		BuildBoolResponse(false);
	}
}

void PostQueueXmlCommand::Execute()
{
	int iNrEntries = 0;
	NextParamAsInt(&iNrEntries);

	AppendResponse(IsJson() ? "[\n" : "<array><data>\n");

	const char* XML_POSTQUEUE_ITEM_START = 
		"<value><struct>\n"
		"<member><name>ID</name><value><i4>%i</i4></value></member>\n"
		"<member><name>NZBID</name><value><i4>%i</i4></value></member>\n"
		"<member><name>NZBNicename</name><value><string>%s</string></value></member>\n"
		"<member><name>NZBFilename</name><value><string>%s</string></value></member>\n"
		"<member><name>DestDir</name><value><string>%s</string></value></member>\n"
		"<member><name>ParFilename</name><value><string>%s</string></value></member>\n"
		"<member><name>InfoName</name><value><string>%s</string></value></member>\n"
		"<member><name>Stage</name><value><string>%s</string></value></member>\n"
		"<member><name>ProgressLabel</name><value><string>%s</string></value></member>\n"
		"<member><name>FileProgress</name><value><i4>%i</i4></value></member>\n"
		"<member><name>StageProgress</name><value><i4>%i</i4></value></member>\n"
		"<member><name>TotalTimeSec</name><value><i4>%i</i4></value></member>\n"
		"<member><name>StageTimeSec</name><value><i4>%i</i4></value></member>\n"
		"<member><name>Log</name><value><array><data>\n";

	const char* XML_POSTQUEUE_ITEM_END = 
		"</data></array></value></member>\n"
		"</struct></value>\n";

	const char* JSON_POSTQUEUE_ITEM_START = 
		"{\n"
		"\"ID\" : %i,\n"
		"\"NZBID\" : %i,\n"
		"\"NZBNicename\" : \"%s\",\n"
		"\"NZBFilename\" : \"%s\",\n"
		"\"DestDir\" : \"%s\",\n"
		"\"ParFilename\" : \"%s\",\n"
		"\"InfoName\" : \"%s\",\n"
		"\"Stage\" : \"%s\",\n"
		"\"ProgressLabel\" : \"%s\",\n"
		"\"FileProgress\" : %i,\n"
		"\"StageProgress\" : %i,\n"
		"\"TotalTimeSec\" : %i,\n"
		"\"StageTimeSec\" : %i,\n"
		"\"Log\" : [\n";

	const char* JSON_POSTQUEUE_ITEM_END = 
		"]\n"
		"}";

	const char* XML_LOG_ITEM = 
		"<value><struct>\n"
		"<member><name>ID</name><value><i4>%i</i4></value></member>\n"
		"<member><name>Kind</name><value><string>%s</string></value></member>\n"
		"<member><name>Time</name><value><i4>%i</i4></value></member>\n"
		"<member><name>Text</name><value><string>%s</string></value></member>\n"
		"</struct></value>\n";

	const char* JSON_LOG_ITEM = 
		"{\n"
		"\"ID\" : %i,\n"
		"\"Kind\" : \"%s\",\n"
		"\"Time\" : %i,\n"
		"\"Text\" : \"%s\"\n"
		"}";

	const char* szMessageType[] = { "INFO", "WARNING", "ERROR", "DEBUG", "DETAIL"};

	PostQueue* pPostQueue = g_pQueueCoordinator->LockQueue()->GetPostQueue();

	time_t tCurTime = time(NULL);
	int szItemBufSize = 10240;
	char* szItemBuf = (char*)malloc(szItemBufSize);
	int index = 0;

	for (PostQueue::iterator it = pPostQueue->begin(); it != pPostQueue->end(); it++)
	{
		PostInfo* pPostInfo = *it;
		char szNZBNicename[1024];
		pPostInfo->GetNZBInfo()->GetNiceNZBName(szNZBNicename, sizeof(szNZBNicename));

	    const char* szPostStageName[] = { "QUEUED", "LOADING_PARS", "VERIFYING_SOURCES", "REPAIRING", "VERIFYING_REPAIRED", "EXECUTING_SCRIPT", "FINISHED" };

		char* xmlNZBNicename = EncodeStr(szNZBNicename);
		char* xmlNZBFilename = EncodeStr(pPostInfo->GetNZBInfo()->GetFilename());
		char* xmlDestDir = EncodeStr(pPostInfo->GetNZBInfo()->GetDestDir());
		char* xmlParFilename = EncodeStr(pPostInfo->GetParFilename());
		char* xmlInfoName = EncodeStr(pPostInfo->GetInfoName());
		char* xmlProgressLabel = EncodeStr(pPostInfo->GetProgressLabel());

		snprintf(szItemBuf, szItemBufSize, IsJson() ? JSON_POSTQUEUE_ITEM_START : XML_POSTQUEUE_ITEM_START,
			pPostInfo->GetID(), pPostInfo->GetNZBInfo()->GetID(),
			xmlNZBNicename, xmlNZBFilename, xmlDestDir, xmlParFilename,
			xmlInfoName, szPostStageName[pPostInfo->GetStage()], xmlProgressLabel,
			pPostInfo->GetFileProgress(), pPostInfo->GetStageProgress(),
			pPostInfo->GetStartTime() ? tCurTime - pPostInfo->GetStartTime() : 0,
			pPostInfo->GetStageTime() ? tCurTime - pPostInfo->GetStageTime() : 0);
		szItemBuf[szItemBufSize-1] = '\0';

		free(xmlNZBNicename);
		free(xmlNZBFilename);
		free(xmlDestDir);
		free(xmlParFilename);
		free(xmlInfoName);
		free(xmlProgressLabel);

		if (IsJson() && index++ > 0)
		{
			AppendResponse(",\n");
		}
		AppendResponse(szItemBuf);

		if (iNrEntries > 0)
		{
			PostInfo::Messages* pMessages = pPostInfo->LockMessages();
			if (!pMessages->empty())
			{
				int iStart = pMessages->size();
				if (iNrEntries > (int)pMessages->size())
				{
					iNrEntries = pMessages->size();
				}
				iStart = pMessages->size() - iNrEntries;

				int index = 0;
				for (unsigned int i = (unsigned int)iStart; i < pMessages->size(); i++)
				{
					Message* pMessage = (*pMessages)[i];
					char* xmltext = EncodeStr(pMessage->GetText());
					snprintf(szItemBuf, szItemBufSize, IsJson() ? JSON_LOG_ITEM : XML_LOG_ITEM,
						pMessage->GetID(), szMessageType[pMessage->GetKind()], pMessage->GetTime(), xmltext);
					szItemBuf[szItemBufSize-1] = '\0';
					free(xmltext);

					if (IsJson() && index++ > 0)
					{
						AppendResponse(",\n");
					}
					AppendResponse(szItemBuf);
				}
			}
			pPostInfo->UnlockMessages();
		}

		AppendResponse(IsJson() ? JSON_POSTQUEUE_ITEM_END : XML_POSTQUEUE_ITEM_END);
	}
	free(szItemBuf);

	g_pQueueCoordinator->UnlockQueue();

	AppendResponse(IsJson() ? "\n]" : "</data></array>\n");
}

void WriteLogXmlCommand::Execute()
{
	if (m_eHttpMethod == XmlRpcProcessor::hmGet)
	{
		BuildErrorResponse(4, "Not safe procedure for HTTP-Method GET. Use Method POST instead");
		return;
	}

	char* szKind;
	char* szText;
	if (!NextParamAsStr(&szKind) || !NextParamAsStr(&szText))
	{
		BuildErrorResponse(2, "Invalid parameter");
		return;
	}

	if (IsJson())
	{
		Util::JsonDecode(szText);
	}
	else
	{
		Util::XmlDecode(szText);
	}

	debug("Kind=%s, Text=%s", szKind, szText);

	if (!strcmp(szKind, "INFO")) {
		info(szText);
	}
	else if (!strcmp(szKind, "WARNING")) {
		warn(szText);
	}
	else if (!strcmp(szKind, "ERROR")) {
		error(szText);
	}
	else if (!strcmp(szKind, "DETAIL")) {
		detail(szText);
	}
	else if (!strcmp(szKind, "DEBUG")) {
		debug(szText);
	} 
	else
	{
		BuildErrorResponse(3, "Invalid Kind");
		return;
	}

	BuildBoolResponse(true);
}

void ScanXmlCommand::Execute()
{
	if (m_eHttpMethod == XmlRpcProcessor::hmGet)
	{
		BuildErrorResponse(4, "Not safe procedure for HTTP-Method GET. Use Method POST instead");
		return;
	}

	g_pPrePostProcessor->ScanNZBDir();
	BuildBoolResponse(true);
}

void HistoryXmlCommand::Execute()
{
	AppendResponse(IsJson() ? "[\n" : "<array><data>\n");

	const char* XML_HISTORY_ITEM_START = 
		"<value><struct>\n"
		"<member><name>NZBID</name><value><i4>%i</i4></value></member>\n"
		"<member><name>NZBNicename</name><value><string>%s</string></value></member>\n"
		"<member><name>NZBFilename</name><value><string>%s</string></value></member>\n"
		"<member><name>DestDir</name><value><string>%s</string></value></member>\n"
		"<member><name>Category</name><value><string>%s</string></value></member>\n"
		"<member><name>ParStatus</name><value><string>%s</string></value></member>\n"
		"<member><name>ScriptStatus</name><value><string>%s</string></value></member>\n"
		"<member><name>FileSizeLo</name><value><i4>%i</i4></value></member>\n"
		"<member><name>FileSizeHi</name><value><i4>%i</i4></value></member>\n"
		"<member><name>FileSizeMB</name><value><i4>%i</i4></value></member>\n"
		"<member><name>FileCount</name><value><i4>%i</i4></value></member>\n"
		"<member><name>RemainingFileCount</name><value><i4>%i</i4></value></member>\n"
		"<member><name>HistoryTime</name><value><i4>%i</i4></value></member>\n"
		"<member><name>Parameters</name><value><array><data>\n";

	const char* XML_HISTORY_ITEM_LOG_START = 
		"</data></array></value></member>\n"
		"<member><name>Log</name><value><array><data>\n";

	const char* XML_HISTORY_ITEM_END = 
		"</data></array></value></member>\n"
		"</struct></value>\n";

	const char* JSON_HISTORY_ITEM_START = 
		"{\n"
		"\"NZBID\" : %i,\n"
		"\"NZBNicename\" : \"%s\",\n"
		"\"NZBFilename\" : \"%s\",\n"
		"\"DestDir\" : \"%s\",\n"
		"\"Category\" : \"%s\",\n"
		"\"ParStatus\" : \"%s\",\n"
		"\"ScriptStatus\" : \"%s\",\n"
		"\"FileSizeLo\" : %i,\n"
		"\"FileSizeHi\" : %i,\n"
		"\"FileSizeMB\" : %i,\n"
		"\"FileCount\" : %i,\n"
		"\"RemainingFileCount\" : %i,\n"
		"\"HistoryTime\" : %i,\n"
		"\"Parameters\" : [\n";

	const char* JSON_HISTORY_ITEM_LOG_START = 
		"],\n"
		"\"Log\" : [\n";

	const char* JSON_HISTORY_ITEM_END = 
		"]\n"
		"}";

	const char* XML_PARAMETER_ITEM = 
		"<value><struct>\n"
		"<member><name>Name</name><value><string>%s</string></value></member>\n"
		"<member><name>Value</name><value><string>%s</string></value></member>\n"
		"</struct></value>\n";

	const char* JSON_PARAMETER_ITEM = 
		"{\n"
		"\"Name\" : \"%s\",\n"
		"\"Value\" : \"%s\"\n"
		"}";

	const char* XML_LOG_ITEM = 
		"<value><struct>\n"
		"<member><name>ID</name><value><i4>%i</i4></value></member>\n"
		"<member><name>Kind</name><value><string>%s</string></value></member>\n"
		"<member><name>Time</name><value><i4>%i</i4></value></member>\n"
		"<member><name>Text</name><value><string>%s</string></value></member>\n"
		"</struct></value>\n";

	const char* JSON_LOG_ITEM = 
		"{\n"
		"\"ID\" : %i,\n"
		"\"Kind\" : \"%s\",\n"
		"\"Time\" : %i,\n"
		"\"Text\" : \"%s\"\n"
		"}";

    const char* szParStatusName[] = { "NONE", "FAILURE", "REPAIR_POSSIBLE", "SUCCESS" };
    const char* szScriptStatusName[] = { "NONE", "UNKNOWN", "FAILURE", "SUCCESS" };
	const char* szMessageType[] = { "INFO", "WARNING", "ERROR", "DEBUG", "DETAIL"};

	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	int szItemBufSize = 10240;
	char* szItemBuf = (char*)malloc(szItemBufSize);
	int index = 0;

	for (HistoryList::iterator it = pDownloadQueue->GetHistoryList()->begin(); it != pDownloadQueue->GetHistoryList()->end(); it++)
	{
		NZBInfo* pNZBInfo = *it;
		unsigned long iFileSizeHi, iFileSizeLo, iFileSizeMB;
		char szNZBNicename[1024];
		Util::SplitInt64(pNZBInfo->GetSize(), &iFileSizeHi, &iFileSizeLo);
		iFileSizeMB = (int)(pNZBInfo->GetSize() / 1024 / 1024);
		pNZBInfo->GetNiceNZBName(szNZBNicename, sizeof(szNZBNicename));

		char* xmlNZBNicename = EncodeStr(szNZBNicename);
		char* xmlNZBFilename = EncodeStr(pNZBInfo->GetFilename());
		char* xmlDestDir = EncodeStr(pNZBInfo->GetDestDir());
		char* xmlCategory = EncodeStr(pNZBInfo->GetCategory());

		snprintf(szItemBuf, szItemBufSize, IsJson() ? JSON_HISTORY_ITEM_START : XML_HISTORY_ITEM_START,
			pNZBInfo->GetID(), xmlNZBNicename, xmlNZBFilename, xmlDestDir, xmlCategory,
			szParStatusName[pNZBInfo->GetParStatus()], szScriptStatusName[pNZBInfo->GetScriptStatus()],
			iFileSizeLo, iFileSizeHi, iFileSizeMB, pNZBInfo->GetFileCount(), pNZBInfo->GetParkedFileCount(),
			pNZBInfo->GetHistoryTime());
		szItemBuf[szItemBufSize-1] = '\0';

		free(xmlNZBNicename);
		free(xmlNZBFilename);
		free(xmlDestDir);
		free(xmlCategory);

		if (IsJson() && index++ > 0)
		{
			AppendResponse(",\n");
		}
		AppendResponse(szItemBuf);

		// Post-processing parameters
		int iParamIndex = 0;
		for (NZBParameterList::iterator it = pNZBInfo->GetParameters()->begin(); it != pNZBInfo->GetParameters()->end(); it++)
		{
			NZBParameter* pParameter = *it;

			char* xmlName = EncodeStr(pParameter->GetName());
			char* xmlValue = EncodeStr(pParameter->GetValue());

			snprintf(szItemBuf, szItemBufSize, IsJson() ? JSON_PARAMETER_ITEM : XML_PARAMETER_ITEM, xmlName, xmlValue);
			szItemBuf[szItemBufSize-1] = '\0';

			free(xmlName);
			free(xmlValue);

			if (IsJson() && iParamIndex++ > 0)
			{
				AppendResponse(",\n");
			}
			AppendResponse(szItemBuf);
		}

		AppendResponse(IsJson() ? JSON_HISTORY_ITEM_LOG_START : XML_HISTORY_ITEM_LOG_START);

		// Log-Messages
		NZBInfo::Messages* pMessages = pNZBInfo->LockMessages();
		if (!pMessages->empty())
		{
			int iLogIndex = 0;
			for (NZBInfo::Messages::iterator it = pMessages->begin(); it != pMessages->end(); it++)
			{
				Message* pMessage = *it;
				char* xmltext = EncodeStr(pMessage->GetText());
				snprintf(szItemBuf, szItemBufSize, IsJson() ? JSON_LOG_ITEM : XML_LOG_ITEM,
					pMessage->GetID(), szMessageType[pMessage->GetKind()], pMessage->GetTime(), xmltext);
				szItemBuf[szItemBufSize-1] = '\0';
				free(xmltext);

				if (IsJson() && iLogIndex++ > 0)
				{
					AppendResponse(",\n");
				}
				AppendResponse(szItemBuf);
			}
		}
		pNZBInfo->UnlockMessages();

		AppendResponse(IsJson() ? JSON_HISTORY_ITEM_END : XML_HISTORY_ITEM_END);
	}
	free(szItemBuf);

	AppendResponse(IsJson() ? "\n]" : "</data></array>\n");

	g_pQueueCoordinator->UnlockQueue();
}
