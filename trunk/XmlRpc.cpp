/*
 *  This file is part of nzbget
 *
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#ifndef WIN32
#include <unistd.h>
#endif

#include "nzbget.h"
#include "XmlRpc.h"
#include "Log.h"
#include "Options.h"
#include "QueueCoordinator.h"
#include "UrlCoordinator.h"
#include "QueueEditor.h"
#include "PrePostProcessor.h"
#include "Scanner.h"
#include "FeedCoordinator.h"
#include "ServerPool.h"
#include "Util.h"

extern Options* g_pOptions;
extern QueueCoordinator* g_pQueueCoordinator;
extern UrlCoordinator* g_pUrlCoordinator;
extern PrePostProcessor* g_pPrePostProcessor;
extern Scanner* g_pScanner;
extern FeedCoordinator* g_pFeedCoordinator;
extern ServerPool* g_pServerPool;
extern void ExitProc();
extern void Reload();


//*****************************************************************
// XmlRpcProcessor

XmlRpcProcessor::XmlRpcProcessor()
{
	m_szRequest = NULL;
	m_eProtocol = rpUndefined;
	m_eHttpMethod = hmPost;
	m_szUrl = NULL;
	m_szContentType = NULL;
}

XmlRpcProcessor::~XmlRpcProcessor()
{
	if (m_szUrl)
	{
		free(m_szUrl);
	}
}

void XmlRpcProcessor::SetUrl(const char* szUrl)
{
	m_szUrl = strdup(szUrl);
}


bool XmlRpcProcessor::IsRpcRequest(const char* szUrl)
{
	return !strcmp(szUrl, "/xmlrpc") || !strncmp(szUrl, "/xmlrpc/", 8) ||
		!strcmp(szUrl, "/jsonrpc") || !strncmp(szUrl, "/jsonrpc/", 9) ||
		!strcmp(szUrl, "/jsonprpc") || !strncmp(szUrl, "/jsonprpc/", 10);
}

void XmlRpcProcessor::Execute()
{
	m_eProtocol = rpUndefined;
	if (!strcmp(m_szUrl, "/xmlrpc") || !strncmp(m_szUrl, "/xmlrpc/", 8))
	{
		m_eProtocol = XmlRpcProcessor::rpXmlRpc;
	}
	else if (!strcmp(m_szUrl, "/jsonrpc") || !strncmp(m_szUrl, "/jsonrpc/", 9))
	{
		m_eProtocol = rpJsonRpc;
	}
	else if (!strcmp(m_szUrl, "/jsonprpc") || !strncmp(m_szUrl, "/jsonprpc/", 10))
	{
		m_eProtocol = rpJsonPRpc;
	}
	else
	{
		error("internal error: invalid rpc-request: %s", m_szUrl);
		return;
	}

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
		WebUtil::XmlParseTagValue(m_szRequest, "methodName", szMethodName, sizeof(szMethodName), NULL);
	} 
	else if (m_eProtocol == rpJsonRpc) 
	{
		int iValueLen = 0;
		if (const char* szMethodPtr = WebUtil::JsonFindField(m_szRequest, "method", &iValueLen))
		{
			strncpy(szMethodName, szMethodPtr + 1, iValueLen - 2);
			szMethodName[iValueLen - 2] = '\0';
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
		BuildResponse(command->GetResponse(), command->GetCallbackFunc(), command->GetFault());
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
		WebUtil::XmlParseTagValue(szNameEnd, "string", szMethodName, sizeof(szMethodName), NULL);
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
		BuildResponse(command->GetResponse(), "", command->GetFault());
		delete command;
	}
	else
	{
		cStringBuilder.Append("</data></array>");
		BuildResponse(cStringBuilder.GetBuffer(), "", false);
	}
}

void XmlRpcProcessor::BuildResponse(const char* szResponse, const char* szCallbackFunc, bool bFault)
{
	const char XML_HEADER[] = "<?xml version=\"1.0\"?>\n<methodResponse>\n";
	const char XML_FOOTER[] = "</methodResponse>";
	const char XML_OK_OPEN[] = "<params><param><value>";
	const char XML_OK_CLOSE[] = "</value></param></params>\n";
	const char XML_FAULT_OPEN[] = "<fault><value>";
	const char XML_FAULT_CLOSE[] = "</value></fault>\n";

	const char JSON_HEADER[] = "{\n\"version\" : \"1.1\",\n";
	const char JSON_FOOTER[] = "\n}";
	const char JSON_OK_OPEN[] = "\"result\" : ";
	const char JSON_OK_CLOSE[] = "";
	const char JSON_FAULT_OPEN[] = "\"error\" : ";
	const char JSON_FAULT_CLOSE[] = "";

	const char JSONP_CALLBACK_HEADER[] = "(";
	const char JSONP_CALLBACK_FOOTER[] = ")";

	bool bXmlRpc = m_eProtocol == rpXmlRpc;

	const char* szCallbackHeader = m_eProtocol == rpJsonPRpc ? JSONP_CALLBACK_HEADER : "";
	const char* szHeader = bXmlRpc ? XML_HEADER : JSON_HEADER;
	const char* szFooter = bXmlRpc ? XML_FOOTER : JSON_FOOTER;
	const char* szOpenTag = bFault ? (bXmlRpc ? XML_FAULT_OPEN : JSON_FAULT_OPEN) : (bXmlRpc ? XML_OK_OPEN : JSON_OK_OPEN);
	const char* szCloseTag = bFault ? (bXmlRpc ? XML_FAULT_CLOSE : JSON_FAULT_CLOSE ) : (bXmlRpc ? XML_OK_CLOSE : JSON_OK_CLOSE);
	const char* szCallbackFooter = m_eProtocol == rpJsonPRpc ? JSONP_CALLBACK_FOOTER : "";

	debug("Response=%s", szResponse);

	if (szCallbackFunc)
	{
		m_cResponse.Append(szCallbackFunc);
	}
	m_cResponse.Append(szCallbackHeader);
	m_cResponse.Append(szHeader);
	m_cResponse.Append(szOpenTag);
	m_cResponse.Append(szResponse);
	m_cResponse.Append(szCloseTag);
	m_cResponse.Append(szFooter);
	m_cResponse.Append(szCallbackFooter);
	
	m_szContentType = bXmlRpc ? "text/xml" : "application/json";
}

XmlCommand* XmlRpcProcessor::CreateCommand(const char* szMethodName)
{
	XmlCommand* command = NULL;

	if (!strcasecmp(szMethodName, "pause") || !strcasecmp(szMethodName, "pausedownload"))
	{
		command = new PauseUnpauseXmlCommand(true, PauseUnpauseXmlCommand::paDownload);
	}
	else if (!strcasecmp(szMethodName, "resume") || !strcasecmp(szMethodName, "resumedownload"))
	{
		command = new PauseUnpauseXmlCommand(false, PauseUnpauseXmlCommand::paDownload);
	}
	else if (!strcasecmp(szMethodName, "pausedownload2"))
	{
		command = new PauseUnpauseXmlCommand(true, PauseUnpauseXmlCommand::paDownload2);
	}
	else if (!strcasecmp(szMethodName, "resumedownload2"))
	{
		command = new PauseUnpauseXmlCommand(false, PauseUnpauseXmlCommand::paDownload2);
	}
	else if (!strcasecmp(szMethodName, "shutdown"))
	{
		command = new ShutdownXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "reload"))
	{
		command = new ReloadXmlCommand();
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
	else if (!strcasecmp(szMethodName, "clearlog"))
	{
		command = new ClearLogXmlCommand();
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
	else if (!strcasecmp(szMethodName, "scheduleresume"))
	{
		command = new ScheduleResumeXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "history"))
	{
		command = new HistoryXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "appendurl"))
	{
		command = new DownloadUrlXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "urlqueue"))
	{
		command = new UrlQueueXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "config"))
	{
		command = new ConfigXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "loadconfig"))
	{
		command = new LoadConfigXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "saveconfig"))
	{
		command = new SaveConfigXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "configtemplates"))
	{
		command = new ConfigTemplatesXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "viewfeed"))
	{
		command = new ViewFeedXmlCommand(false);
	}
	else if (!strcasecmp(szMethodName, "previewfeed"))
	{
		command = new ViewFeedXmlCommand(true);
	}
	else if (!strcasecmp(szMethodName, "fetchfeeds"))
	{
		command = new FetchFeedsXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "editserver"))
	{
		command = new EditServerXmlCommand();
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
	m_szCallbackFunc = NULL;
	m_bFault = false;
	m_eProtocol = XmlRpcProcessor::rpUndefined;
}

bool XmlCommand::IsJson()
{ 
	return m_eProtocol == XmlRpcProcessor::rpJsonRpc || m_eProtocol == XmlRpcProcessor::rpJsonPRpc;
}

void XmlCommand::AppendResponse(const char* szPart)
{
	m_StringBuilder.Append(szPart);
}

void XmlCommand::BuildErrorResponse(int iErrCode, const char* szErrText, ...)
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

	char szFullText[1024];

	va_list ap;
	va_start(ap, szErrText);
	vsnprintf(szFullText, 1024, szErrText, ap);
	szFullText[1024-1] = '\0';
	va_end(ap);

	char* xmlText = EncodeStr(szFullText);

	char szContent[1024];
	snprintf(szContent, 1024, IsJson() ? JSON_RESPONSE_ERROR_BODY : XML_RESPONSE_ERROR_BODY, iErrCode, xmlText);
	szContent[1024-1] = '\0';

	free(xmlText);

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

	if (m_eProtocol == XmlRpcProcessor::rpJsonPRpc)
	{
		NextParamAsStr(&m_szCallbackFunc);
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
		char* szParam = (char*)WebUtil::JsonNextValue(m_szRequestPtr, &iLen);
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
		char* szParam = (char*)WebUtil::XmlFindTag(m_szRequestPtr, "i4", &iLen);
		if (!szParam)
		{
			szParam = (char*)WebUtil::XmlFindTag(m_szRequestPtr, "int", &iLen);
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
		char* szParam;
		if (!NextParamAsStr(&szParam))
		{
			return false;
		}

		if (IsJson())
		{
			if (!strcmp(szParam, "true"))
			{
				*bValue = true;
				return true;
			}
			else if (!strcmp(szParam, "false"))
			{
				*bValue = false;
				return true;
			}
		}
		else
		{
			*bValue = szParam[0] == '1';
			return true;
		}
		return false;
	}
	else if (IsJson())
	{
		int iLen = 0;
		char* szParam = (char*)WebUtil::JsonNextValue(m_szRequestPtr, &iLen);
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
		char* szParam = (char*)WebUtil::XmlFindTag(m_szRequestPtr, "boolean", &iLen);
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
		char* szParam = strchr(m_szRequestPtr, '=');
		if (!szParam)
		{
			return false;
		}
		szParam++; // skip '='
		int iLen = 0;
		char* szParamEnd = strchr(m_szRequestPtr, '&');
		if (szParamEnd)
		{
			iLen = (int)(szParamEnd - szParam);
			szParam[iLen] = '\0';
		}
		else
		{
			iLen = strlen(szParam) - 1;
		}
		m_szRequestPtr = szParam + iLen + 1;
		*szValue = szParam;
		return true;
	}
	else if (IsJson())
	{
		int iLen = 0;
		char* szParam = (char*)WebUtil::JsonNextValue(m_szRequestPtr, &iLen);
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
		char* szParam = (char*)WebUtil::XmlFindTag(m_szRequestPtr, "string", &iLen);
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
		return WebUtil::JsonEncode(szStr);
	}
	else
	{
		return WebUtil::XmlEncode(szStr);
	}
}

void XmlCommand::DecodeStr(char* szStr)
{
	if (IsJson())
	{
		WebUtil::JsonDecode(szStr);
	}
	else
	{
		WebUtil::XmlDecode(szStr);
	}
}

bool XmlCommand::CheckSafeMethod()
{
	bool bSafe = m_eHttpMethod == XmlRpcProcessor::hmPost || m_eProtocol == XmlRpcProcessor::rpJsonPRpc;
	if (!bSafe)
	{
		BuildErrorResponse(4, "Not safe procedure for HTTP-Method GET. Use Method POST instead");
	}
	return bSafe;
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
	if (!CheckSafeMethod())
	{
		return;
	}

	bool bOK = true;

	g_pOptions->SetResumeTime(0);

	switch (m_eEPauseAction)
	{
		case paDownload:
			g_pOptions->SetPauseDownload(m_bPause);
			break;

		case paDownload2:
			g_pOptions->SetPauseDownload2(m_bPause);
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

// bool scheduleresume(int Seconds) 
void ScheduleResumeXmlCommand::Execute()
{
	if (!CheckSafeMethod())
	{
		return;
	}

	int iSeconds = 0;
	if (!NextParamAsInt(&iSeconds) || iSeconds < 0)
	{
		BuildErrorResponse(2, "Invalid parameter");
		return;
	}

	time_t tCurTime = time(NULL);

	g_pOptions->SetResumeTime(tCurTime + iSeconds);

	BuildBoolResponse(true);
}

void ShutdownXmlCommand::Execute()
{
	if (!CheckSafeMethod())
	{
		return;
	}

	BuildBoolResponse(true);
	ExitProc();
}

void ReloadXmlCommand::Execute()
{
	if (!CheckSafeMethod())
	{
		return;
	}

	BuildBoolResponse(true);
	Reload();
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
	g_pUrlCoordinator->LogDebugInfo();
	g_pFeedCoordinator->LogDebugInfo();
	BuildBoolResponse(true);
}

void SetDownloadRateXmlCommand::Execute()
{
	if (!CheckSafeMethod())
	{
		return;
	}

	int iRate = 0;
	if (!NextParamAsInt(&iRate) || iRate < 0)
	{
		BuildErrorResponse(2, "Invalid parameter");
		return;
	}

	g_pOptions->SetDownloadRate(iRate * 1024);
	BuildBoolResponse(true);
}

void StatusXmlCommand::Execute()
{
	const char* XML_STATUS_START = 
		"<struct>\n"
		"<member><name>RemainingSizeLo</name><value><i4>%u</i4></value></member>\n"
		"<member><name>RemainingSizeHi</name><value><i4>%u</i4></value></member>\n"
		"<member><name>RemainingSizeMB</name><value><i4>%i</i4></value></member>\n"
		"<member><name>DownloadedSizeLo</name><value><i4>%u</i4></value></member>\n"
		"<member><name>DownloadedSizeHi</name><value><i4>%u</i4></value></member>\n"
		"<member><name>DownloadedSizeMB</name><value><i4>%i</i4></value></member>\n"
		"<member><name>DownloadRate</name><value><i4>%i</i4></value></member>\n"
		"<member><name>AverageDownloadRate</name><value><i4>%i</i4></value></member>\n"
		"<member><name>DownloadLimit</name><value><i4>%i</i4></value></member>\n"
		"<member><name>ThreadCount</name><value><i4>%i</i4></value></member>\n"
		"<member><name>ParJobCount</name><value><i4>%i</i4></value></member>\n"					// deprecated (renamed to PostJobCount)
		"<member><name>PostJobCount</name><value><i4>%i</i4></value></member>\n"
		"<member><name>UrlCount</name><value><i4>%i</i4></value></member>\n"
		"<member><name>UpTimeSec</name><value><i4>%i</i4></value></member>\n"
		"<member><name>DownloadTimeSec</name><value><i4>%i</i4></value></member>\n"
		"<member><name>ServerPaused</name><value><boolean>%s</boolean></value></member>\n"		// deprecated (renamed to DownloadPaused)
		"<member><name>DownloadPaused</name><value><boolean>%s</boolean></value></member>\n"
		"<member><name>Download2Paused</name><value><boolean>%s</boolean></value></member>\n"
		"<member><name>ServerStandBy</name><value><boolean>%s</boolean></value></member>\n"
		"<member><name>PostPaused</name><value><boolean>%s</boolean></value></member>\n"
		"<member><name>ScanPaused</name><value><boolean>%s</boolean></value></member>\n"
		"<member><name>FreeDiskSpaceLo</name><value><i4>%u</i4></value></member>\n"
		"<member><name>FreeDiskSpaceHi</name><value><i4>%u</i4></value></member>\n"
		"<member><name>FreeDiskSpaceMB</name><value><i4>%i</i4></value></member>\n"
		"<member><name>ServerTime</name><value><i4>%i</i4></value></member>\n"
		"<member><name>ResumeTime</name><value><i4>%i</i4></value></member>\n"
		"<member><name>FeedActive</name><value><boolean>%s</boolean></value></member>\n"
		"<member><name>NewsServers</name><value><array><data>\n";

	const char* XML_STATUS_END =
		"</data></array></value></member>\n"
		"</struct>\n";

	const char* JSON_STATUS_START = 
		"{\n"
		"\"RemainingSizeLo\" : %u,\n"
		"\"RemainingSizeHi\" : %u,\n"
		"\"RemainingSizeMB\" : %i,\n"
		"\"DownloadedSizeLo\" : %u,\n"
		"\"DownloadedSizeHi\" : %u,\n"
		"\"DownloadedSizeMB\" : %i,\n"
		"\"DownloadRate\" : %i,\n"
		"\"AverageDownloadRate\" : %i,\n"
		"\"DownloadLimit\" : %i,\n"
		"\"ThreadCount\" : %i,\n"
		"\"ParJobCount\" : %i,\n"			// deprecated (renamed to PostJobCount)
		"\"PostJobCount\" : %i,\n"
		"\"UrlCount\" : %i,\n"
		"\"UpTimeSec\" : %i,\n"
		"\"DownloadTimeSec\" : %i,\n"
		"\"ServerPaused\" : %s,\n"			// deprecated (renamed to DownloadPaused)
		"\"DownloadPaused\" : %s,\n"
		"\"Download2Paused\" : %s,\n"
		"\"ServerStandBy\" : %s,\n"
		"\"PostPaused\" : %s,\n"
		"\"ScanPaused\" : %s,\n"
		"\"FreeDiskSpaceLo\" : %u,\n"
		"\"FreeDiskSpaceHi\" : %u,\n"
		"\"FreeDiskSpaceMB\" : %i,\n"
		"\"ServerTime\" : %i,\n"
		"\"ResumeTime\" : %i,\n"
		"\"FeedActive\" : %s,\n"
		"\"NewsServers\" : [\n";

	const char* JSON_STATUS_END = 
		"]\n"
		"}";

	const char* XML_NEWSSERVER_ITEM = 
		"<value><struct>\n"
		"<member><name>ID</name><value><i4>%i</i4></value></member>\n"
		"<member><name>Active</name><value><boolean>%s</boolean></value></member>\n"
		"</struct></value>\n";

	const char* JSON_NEWSSERVER_ITEM = 
		"{\n"
		"\"ID\" : %i,\n"
		"\"Active\" : %s\n"
		"}";

	unsigned long iRemainingSizeHi, iRemainingSizeLo;
	int iDownloadRate = (int)(g_pQueueCoordinator->CalcCurrentDownloadSpeed());
	long long iRemainingSize = g_pQueueCoordinator->CalcRemainingSize();
	Util::SplitInt64(iRemainingSize, &iRemainingSizeHi, &iRemainingSizeLo);
	int iRemainingMBytes = (int)(iRemainingSize / 1024 / 1024);
	int iDownloadLimit = (int)(g_pOptions->GetDownloadRate());
	bool bDownloadPaused = g_pOptions->GetPauseDownload();
	bool bDownload2Paused = g_pOptions->GetPauseDownload2();
	bool bPostPaused = g_pOptions->GetPausePostProcess();
	bool bScanPaused = g_pOptions->GetPauseScan();
	int iThreadCount = Thread::GetThreadCount() - 1; // not counting itself
	DownloadQueue *pDownloadQueue = g_pQueueCoordinator->LockQueue();
	int iPostJobCount = pDownloadQueue->GetPostQueue()->size();
	int iUrlCount = pDownloadQueue->GetUrlQueue()->size();
	g_pQueueCoordinator->UnlockQueue();
	unsigned long iDownloadedSizeHi, iDownloadedSizeLo;
	int iUpTimeSec, iDownloadTimeSec;
	long long iAllBytes;
	bool bServerStandBy;
	g_pQueueCoordinator->CalcStat(&iUpTimeSec, &iDownloadTimeSec, &iAllBytes, &bServerStandBy);
	int iDownloadedMBytes = (int)(iAllBytes / 1024 / 1024);
	Util::SplitInt64(iAllBytes, &iDownloadedSizeHi, &iDownloadedSizeLo);
	int iAverageDownloadRate = (int)(iDownloadTimeSec > 0 ? iAllBytes / iDownloadTimeSec : 0);
	unsigned long iFreeDiskSpaceHi, iFreeDiskSpaceLo;
	long long iFreeDiskSpace = Util::FreeDiskSize(g_pOptions->GetDestDir());
	Util::SplitInt64(iFreeDiskSpace, &iFreeDiskSpaceHi, &iFreeDiskSpaceLo);
	int iFreeDiskSpaceMB = (int)(iFreeDiskSpace / 1024 / 1024);
	int iServerTime = time(NULL);
	int iResumeTime = g_pOptions->GetResumeTime();
	bool bFeedActive = g_pFeedCoordinator->HasActiveDownloads();
	
	char szContent[3072];
	snprintf(szContent, 3072, IsJson() ? JSON_STATUS_START : XML_STATUS_START, 
		iRemainingSizeLo, iRemainingSizeHi,	iRemainingMBytes, iDownloadedSizeLo, iDownloadedSizeHi, 
		iDownloadedMBytes, iDownloadRate, iAverageDownloadRate, iDownloadLimit,	iThreadCount, 
		iPostJobCount, iPostJobCount, iUrlCount, iUpTimeSec, iDownloadTimeSec, 
		BoolToStr(bDownloadPaused), BoolToStr(bDownloadPaused), BoolToStr(bDownload2Paused), 
		BoolToStr(bServerStandBy), BoolToStr(bPostPaused), BoolToStr(bScanPaused),
		iFreeDiskSpaceLo, iFreeDiskSpaceHi,	iFreeDiskSpaceMB, iServerTime, iResumeTime,
		BoolToStr(bFeedActive));
	szContent[3072-1] = '\0';

	AppendResponse(szContent);

	int index = 0;
	for (Servers::iterator it = g_pServerPool->GetServers()->begin(); it != g_pServerPool->GetServers()->end(); it++)
	{
		NewsServer* pServer = *it;
		snprintf(szContent, sizeof(szContent), IsJson() ? JSON_NEWSSERVER_ITEM : XML_NEWSSERVER_ITEM,
			pServer->GetID(), BoolToStr(pServer->GetActive()));
		szContent[3072-1] = '\0';

		if (IsJson() && index++ > 0)
		{
			AppendResponse(",\n");
		}
		AppendResponse(szContent);
	}

	AppendResponse(IsJson() ? JSON_STATUS_END : XML_STATUS_END);
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
		iNrEntries = pMessages->size();
		iStart = iIDFrom - pMessages->front()->GetID();
		if (iStart < 0)
		{
			iStart = 0;
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

	int iItemBufSize = 10240;
	char* szItemBuf = (char*)malloc(iItemBufSize);
	int index = 0;

	for (unsigned int i = (unsigned int)iStart; i < pMessages->size(); i++)
	{
		Message* pMessage = (*pMessages)[i];
		char* xmltext = EncodeStr(pMessage->GetText());
		snprintf(szItemBuf, iItemBufSize, IsJson() ? JSON_LOG_ITEM : XML_LOG_ITEM,
			pMessage->GetID(), szMessageType[pMessage->GetKind()], pMessage->GetTime(), xmltext);
		szItemBuf[iItemBufSize-1] = '\0';
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

// struct[] listfiles(int IDFrom, int IDTo, int NZBID) 
// For backward compatibility with 0.8 parameter "NZBID" is optional
void ListFilesXmlCommand::Execute()
{
	int iIDStart = 0;
	int iIDEnd = 0;
	if (NextParamAsInt(&iIDStart) && (!NextParamAsInt(&iIDEnd) || iIDEnd < iIDStart))
	{
		BuildErrorResponse(2, "Invalid parameter");
		return;
	}

	// For backward compatibility with 0.8 parameter "NZBID" is optional (error checking omitted)
	int iNZBID = 0;
	NextParamAsInt(&iNZBID);

	if (iNZBID > 0 && (iIDStart != 0 || iIDEnd != 0))
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
		"<member><name>FileSizeLo</name><value><i4>%u</i4></value></member>\n"
		"<member><name>FileSizeHi</name><value><i4>%u</i4></value></member>\n"
		"<member><name>RemainingSizeLo</name><value><i4>%u</i4></value></member>\n"
		"<member><name>RemainingSizeHi</name><value><i4>%u</i4></value></member>\n"
		"<member><name>PostTime</name><value><i4>%i</i4></value></member>\n"
		"<member><name>FilenameConfirmed</name><value><boolean>%s</boolean></value></member>\n"
		"<member><name>Paused</name><value><boolean>%s</boolean></value></member>\n"
		"<member><name>NZBID</name><value><i4>%i</i4></value></member>\n"
		"<member><name>NZBName</name><value><string>%s</string></value></member>\n"
		"<member><name>NZBNicename</name><value><string>%s</string></value></member>\n"	// deprecated, use "NZBName" instead
		"<member><name>NZBFilename</name><value><string>%s</string></value></member>\n"
		"<member><name>Subject</name><value><string>%s</string></value></member>\n"
		"<member><name>Filename</name><value><string>%s</string></value></member>\n"
		"<member><name>DestDir</name><value><string>%s</string></value></member>\n"
		"<member><name>Category</name><value><string>%s</string></value></member>\n"
		"<member><name>Priority</name><value><i4>%i</i4></value></member>\n"
		"<member><name>ActiveDownloads</name><value><i4>%i</i4></value></member>\n"
		"</struct></value>\n";

	const char* JSON_LIST_ITEM = 
		"{\n"
		"\"ID\" : %i,\n"
		"\"FileSizeLo\" : %u,\n"
		"\"FileSizeHi\" : %u,\n"
		"\"RemainingSizeLo\" : %u,\n"
		"\"RemainingSizeHi\" : %u,\n"
		"\"PostTime\" : %i,\n"
		"\"FilenameConfirmed\" : %s,\n"
		"\"Paused\" : %s,\n"
		"\"NZBID\" : %i,\n"
		"\"NZBName\" : \"%s\",\n"
		"\"NZBNicename\" : \"%s\",\n" 		// deprecated, use "NZBName" instead
		"\"NZBFilename\" : \"%s\",\n"
		"\"Subject\" : \"%s\",\n"
		"\"Filename\" : \"%s\",\n"
		"\"DestDir\" : \"%s\",\n"
		"\"Category\" : \"%s\",\n"
		"\"Priority\" : %i,\n"
		"\"ActiveDownloads\" : %i\n"
		"}";

	int iItemBufSize = 10240;
	char* szItemBuf = (char*)malloc(iItemBufSize);
	int index = 0;

	for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if ((iNZBID > 0 && iNZBID == pFileInfo->GetNZBInfo()->GetID()) ||
			(iNZBID == 0 && (iIDStart == 0 || (iIDStart <= pFileInfo->GetID() && pFileInfo->GetID() <= iIDEnd))))
		{
			unsigned long iFileSizeHi, iFileSizeLo;
			unsigned long iRemainingSizeLo, iRemainingSizeHi;
			Util::SplitInt64(pFileInfo->GetSize(), &iFileSizeHi, &iFileSizeLo);
			Util::SplitInt64(pFileInfo->GetRemainingSize(), &iRemainingSizeHi, &iRemainingSizeLo);
			char* xmlNZBFilename = EncodeStr(pFileInfo->GetNZBInfo()->GetFilename());
			char* xmlSubject = EncodeStr(pFileInfo->GetSubject());
			char* xmlFilename = EncodeStr(pFileInfo->GetFilename());
			char* xmlDestDir = EncodeStr(pFileInfo->GetNZBInfo()->GetDestDir());
			char* xmlCategory = EncodeStr(pFileInfo->GetNZBInfo()->GetCategory());
			char* xmlNZBNicename = EncodeStr(pFileInfo->GetNZBInfo()->GetName());

			snprintf(szItemBuf, iItemBufSize, IsJson() ? JSON_LIST_ITEM : XML_LIST_ITEM,
				pFileInfo->GetID(), iFileSizeLo, iFileSizeHi, iRemainingSizeLo, iRemainingSizeHi, 
				pFileInfo->GetTime(), BoolToStr(pFileInfo->GetFilenameConfirmed()), 
				BoolToStr(pFileInfo->GetPaused()), pFileInfo->GetNZBInfo()->GetID(), xmlNZBNicename,
				xmlNZBNicename, xmlNZBFilename, xmlSubject, xmlFilename, xmlDestDir, xmlCategory,
				pFileInfo->GetPriority(), pFileInfo->GetActiveDownloads());
			szItemBuf[iItemBufSize-1] = '\0';

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

void NzbInfoXmlCommand::AppendNZBInfoFields(NZBInfo* pNZBInfo)
{
	const char* XML_HISTORY_ITEM_START =
	"<member><name>NZBID</name><value><i4>%i</i4></value></member>\n"
	"<member><name>NZBName</name><value><string>%s</string></value></member>\n"
	"<member><name>NZBNicename</name><value><string>%s</string></value></member>\n"	// deprecated, use "NZBName" instead
	"<member><name>NZBFilename</name><value><string>%s</string></value></member>\n"
	"<member><name>DestDir</name><value><string>%s</string></value></member>\n"
	"<member><name>FinalDir</name><value><string>%s</string></value></member>\n"
	"<member><name>Category</name><value><string>%s</string></value></member>\n"
	"<member><name>ParStatus</name><value><string>%s</string></value></member>\n"
	"<member><name>UnpackStatus</name><value><string>%s</string></value></member>\n"
	"<member><name>MoveStatus</name><value><string>%s</string></value></member>\n"
	"<member><name>ScriptStatus</name><value><string>%s</string></value></member>\n"
	"<member><name>DeleteStatus</name><value><string>%s</string></value></member>\n"
	"<member><name>MarkStatus</name><value><string>%s</string></value></member>\n"
	"<member><name>FileSizeLo</name><value><i4>%u</i4></value></member>\n"
	"<member><name>FileSizeHi</name><value><i4>%u</i4></value></member>\n"
	"<member><name>FileSizeMB</name><value><i4>%i</i4></value></member>\n"
	"<member><name>FileCount</name><value><i4>%i</i4></value></member>\n"
	"<member><name>TotalArticles</name><value><i4>%i</i4></value></member>\n"
	"<member><name>SuccessArticles</name><value><i4>%i</i4></value></member>\n"
	"<member><name>FailedArticles</name><value><i4>%i</i4></value></member>\n"
	"<member><name>Health</name><value><i4>%i</i4></value></member>\n"
	"<member><name>CriticalHealth</name><value><i4>%i</i4></value></member>\n"
	"<member><name>DupeKey</name><value><string>%s</string></value></member>\n"
	"<member><name>DupeScore</name><value><i4>%i</i4></value></member>\n"
	"<member><name>DupeMode</name><value><i4>%i</i4></value></member>\n"
	"<member><name>Deleted</name><value><boolean>%s</boolean></value></member>\n"	 // deprecated, use "DeleteStatus" instead
	"<member><name>Parameters</name><value><array><data>\n";
	
	const char* XML_HISTORY_ITEM_SCRIPT_START =
	"</data></array></value></member>\n"
	"<member><name>ScriptStatuses</name><value><array><data>\n";
	
	const char* XML_HISTORY_ITEM_STATS_START =
	"</data></array></value></member>\n"
	"<member><name>ServerStats</name><value><array><data>\n";
	
	const char* XML_HISTORY_ITEM_END =
	"</data></array></value></member>\n";
	
	const char* JSON_HISTORY_ITEM_START =
	"\"NZBID\" : %i,\n"
	"\"NZBName\" : \"%s\",\n"
	"\"NZBNicename\" : \"%s\",\n"		// deprecated, use NZBName instead
	"\"NZBFilename\" : \"%s\",\n"
	"\"DestDir\" : \"%s\",\n"
	"\"FinalDir\" : \"%s\",\n"
	"\"Category\" : \"%s\",\n"
	"\"ParStatus\" : \"%s\",\n"
	"\"UnpackStatus\" : \"%s\",\n"
	"\"MoveStatus\" : \"%s\",\n"
	"\"ScriptStatus\" : \"%s\",\n"
	"\"DeleteStatus\" : \"%s\",\n"
	"\"MarkStatus\" : \"%s\",\n"
	"\"FileSizeLo\" : %u,\n"
	"\"FileSizeHi\" : %u,\n"
	"\"FileSizeMB\" : %i,\n"
	"\"FileCount\" : %i,\n"
	"\"TotalArticles\" : %i,\n"
	"\"SuccessArticles\" : %i,\n"
	"\"FailedArticles\" : %i,\n"
	"\"Health\" : %i,\n"
	"\"CriticalHealth\" : %i,\n"
	"\"DupeKey\" : \"%s\",\n"
	"\"DupeScore\" : %i,\n"
	"\"DupeMode\" : \"%s\",\n"
	"\"Deleted\" : %s,\n"			  // deprecated, use "DeleteStatus" instead
	"\"Parameters\" : [\n";
	
	const char* JSON_HISTORY_ITEM_SCRIPT_START =
	"],\n"
	"\"ScriptStatuses\" : [\n";
	
	const char* JSON_HISTORY_ITEM_STATS_START =
	"],\n"
	"\"ServerStats\" : [\n";
	
	const char* JSON_HISTORY_ITEM_END =
	"]\n";
	
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
	
	const char* XML_SCRIPT_ITEM =
	"<value><struct>\n"
	"<member><name>Name</name><value><string>%s</string></value></member>\n"
	"<member><name>Status</name><value><string>%s</string></value></member>\n"
	"</struct></value>\n";
	
	const char* JSON_SCRIPT_ITEM =
	"{\n"
	"\"Name\" : \"%s\",\n"
	"\"Status\" : \"%s\"\n"
	"}";
	
	const char* XML_STAT_ITEM =
	"<value><struct>\n"
	"<member><name>ServerID</name><value><i4>%i</i4></value></member>\n"
	"<member><name>SuccessArticles</name><value><i4>%i</i4></value></member>\n"
	"<member><name>FailedArticles</name><value><i4>%i</i4></value></member>\n"
	"</struct></value>\n";
	
	const char* JSON_STAT_ITEM =
	"{\n"
	"\"ServerID\" : %i,\n"
	"\"SuccessArticles\" : %i,\n"
	"\"FailedArticles\" : %i\n"
	"}";
	
    const char* szParStatusName[] = { "NONE", "NONE", "FAILURE", "SUCCESS", "REPAIR_POSSIBLE", "MANUAL" };
    const char* szUnpackStatusName[] = { "NONE", "NONE", "FAILURE", "SUCCESS" };
    const char* szMoveStatusName[] = { "NONE", "FAILURE", "SUCCESS" };
    const char* szScriptStatusName[] = { "NONE", "FAILURE", "SUCCESS" };
    const char* szDeleteStatusName[] = { "NONE", "MANUAL", "HEALTH", "DUPE" };
    const char* szMarkStatusName[] = { "NONE", "BAD", "GOOD" };
    const char* szDupeModeName[] = { "SCORE", "ALL", "FORCE" };
	
	int iItemBufSize = 10240;
	char* szItemBuf = (char*)malloc(iItemBufSize);
	
	unsigned long iFileSizeHi, iFileSizeLo, iFileSizeMB;
	Util::SplitInt64(pNZBInfo->GetSize(), &iFileSizeHi, &iFileSizeLo);
	iFileSizeMB = (int)(pNZBInfo->GetSize() / 1024 / 1024);
	
	char* xmlNZBFilename = EncodeStr(pNZBInfo->GetFilename());
	char* xmlNZBNicename = EncodeStr(pNZBInfo->GetName());
	char* xmlDestDir = EncodeStr(pNZBInfo->GetDestDir());
	char* xmlFinalDir = EncodeStr(pNZBInfo->GetFinalDir());
	char* xmlCategory = EncodeStr(pNZBInfo->GetCategory());
	char* xmlDupeKey = EncodeStr(pNZBInfo->GetDupeKey());
	
	snprintf(szItemBuf, iItemBufSize, IsJson() ? JSON_HISTORY_ITEM_START : XML_HISTORY_ITEM_START,
			 pNZBInfo->GetID(), xmlNZBNicename, xmlNZBNicename, xmlNZBFilename,
			 xmlDestDir, xmlFinalDir, xmlCategory, szParStatusName[pNZBInfo->GetParStatus()],
			 szUnpackStatusName[pNZBInfo->GetUnpackStatus()], szMoveStatusName[pNZBInfo->GetMoveStatus()],
			 szScriptStatusName[pNZBInfo->GetScriptStatuses()->CalcTotalStatus()],
			 szDeleteStatusName[pNZBInfo->GetDeleteStatus()], szMarkStatusName[pNZBInfo->GetMarkStatus()],
			 iFileSizeLo, iFileSizeHi, iFileSizeMB, pNZBInfo->GetFileCount(),
			 pNZBInfo->GetTotalArticles(), pNZBInfo->GetSuccessArticles(), pNZBInfo->GetFailedArticles(),
			 pNZBInfo->CalcHealth(), pNZBInfo->CalcCriticalHealth(),
			 xmlDupeKey, pNZBInfo->GetDupeScore(), szDupeModeName[pNZBInfo->GetDupeMode()],
			 BoolToStr(pNZBInfo->GetDeleteStatus() != NZBInfo::dsNone));
	
	free(xmlNZBNicename);
	free(xmlNZBFilename);
	free(xmlCategory);
	free(xmlDestDir);
	free(xmlFinalDir);
	free(xmlDupeKey);
		
	szItemBuf[iItemBufSize-1] = '\0';

	AppendResponse(szItemBuf);
	
	// Post-processing parameters
	int iParamIndex = 0;
	for (NZBParameterList::iterator it = pNZBInfo->GetParameters()->begin(); it != pNZBInfo->GetParameters()->end(); it++)
	{
		NZBParameter* pParameter = *it;
		
		char* xmlName = EncodeStr(pParameter->GetName());
		char* xmlValue = EncodeStr(pParameter->GetValue());
		
		snprintf(szItemBuf, iItemBufSize, IsJson() ? JSON_PARAMETER_ITEM : XML_PARAMETER_ITEM, xmlName, xmlValue);
		szItemBuf[iItemBufSize-1] = '\0';
		
		free(xmlName);
		free(xmlValue);
		
		if (IsJson() && iParamIndex++ > 0)
		{
			AppendResponse(",\n");
		}
		AppendResponse(szItemBuf);
	}
	
	AppendResponse(IsJson() ? JSON_HISTORY_ITEM_SCRIPT_START : XML_HISTORY_ITEM_SCRIPT_START);
	
	// Script statuses
	int iScriptIndex = 0;
	for (ScriptStatusList::iterator it = pNZBInfo->GetScriptStatuses()->begin(); it != pNZBInfo->GetScriptStatuses()->end(); it++)
	{
		ScriptStatus* pScriptStatus = *it;
		
		char* xmlName = EncodeStr(pScriptStatus->GetName());
		char* xmlStatus = EncodeStr(szScriptStatusName[pScriptStatus->GetStatus()]);
		
		snprintf(szItemBuf, iItemBufSize, IsJson() ? JSON_SCRIPT_ITEM : XML_SCRIPT_ITEM, xmlName, xmlStatus);
		szItemBuf[iItemBufSize-1] = '\0';
		
		free(xmlName);
		free(xmlStatus);
		
		if (IsJson() && iScriptIndex++ > 0)
		{
			AppendResponse(",\n");
		}
		AppendResponse(szItemBuf);
	}
	
	AppendResponse(IsJson() ? JSON_HISTORY_ITEM_STATS_START : XML_HISTORY_ITEM_STATS_START);
	
	// Server stats
	int iStatIndex = 0;
	for (ServerStatList::iterator it = pNZBInfo->GetServerStats()->begin(); it != pNZBInfo->GetServerStats()->end(); it++)
	{
		ServerStat* pServerStat = *it;
		
		snprintf(szItemBuf, iItemBufSize, IsJson() ? JSON_STAT_ITEM : XML_STAT_ITEM,
				 pServerStat->GetServerID(), pServerStat->GetSuccessArticles(), pServerStat->GetFailedArticles());
		szItemBuf[iItemBufSize-1] = '\0';
		
		if (IsJson() && iStatIndex++ > 0)
		{
			AppendResponse(",\n");
		}
		AppendResponse(szItemBuf);
	}
	
	AppendResponse(IsJson() ? JSON_HISTORY_ITEM_END : XML_HISTORY_ITEM_END);

	free(szItemBuf);
}

void ListGroupsXmlCommand::Execute()
{
	AppendResponse(IsJson() ? "[\n" : "<array><data>\n");

	const char* XML_LIST_ITEM_START = 
		"<value><struct>\n"
		"<member><name>FirstID</name><value><i4>%i</i4></value></member>\n"
		"<member><name>LastID</name><value><i4>%i</i4></value></member>\n"
		"<member><name>RemainingSizeLo</name><value><i4>%u</i4></value></member>\n"
		"<member><name>RemainingSizeHi</name><value><i4>%u</i4></value></member>\n"
		"<member><name>RemainingSizeMB</name><value><i4>%i</i4></value></member>\n"
		"<member><name>PausedSizeLo</name><value><i4>%u</i4></value></member>\n"
		"<member><name>PausedSizeHi</name><value><i4>%u</i4></value></member>\n"
		"<member><name>PausedSizeMB</name><value><i4>%i</i4></value></member>\n"
		"<member><name>RemainingFileCount</name><value><i4>%i</i4></value></member>\n"
		"<member><name>RemainingParCount</name><value><i4>%i</i4></value></member>\n"
		"<member><name>MinPostTime</name><value><i4>%i</i4></value></member>\n"
		"<member><name>MaxPostTime</name><value><i4>%i</i4></value></member>\n"
		"<member><name>MinPriority</name><value><i4>%i</i4></value></member>\n"
		"<member><name>MaxPriority</name><value><i4>%i</i4></value></member>\n"
		"<member><name>ActiveDownloads</name><value><i4>%i</i4></value></member>\n";

	const char* XML_LIST_ITEM_END = 
		"</struct></value>\n";

	const char* JSON_LIST_ITEM_START = 
		"{\n"
		"\"FirstID\" : %i,\n"
		"\"LastID\" : %i,\n"
		"\"RemainingSizeLo\" : %u,\n"
		"\"RemainingSizeHi\" : %u,\n"
		"\"RemainingSizeMB\" : %i,\n"
		"\"PausedSizeLo\" : %u,\n"
		"\"PausedSizeHi\" : %u,\n"
		"\"PausedSizeMB\" : %i,\n"
		"\"RemainingFileCount\" : %i,\n"
		"\"RemainingParCount\" : %i,\n"
		"\"MinPostTime\" : %i,\n"
		"\"MaxPostTime\" : %i,\n"
		"\"MinPriority\" : %i,\n"
		"\"MaxPriority\" : %i,\n"
		"\"ActiveDownloads\" : %i,\n";
	
	const char* JSON_LIST_ITEM_END =
		"}";

	GroupQueue groupQueue;
	groupQueue.clear();
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();
	pDownloadQueue->BuildGroups(&groupQueue);
	g_pQueueCoordinator->UnlockQueue();

	int iItemBufSize = 10240;
	char* szItemBuf = (char*)malloc(iItemBufSize);
	int index = 0;

	for (GroupQueue::iterator it = groupQueue.begin(); it != groupQueue.end(); it++)
	{
		GroupInfo* pGroupInfo = *it;

		unsigned long iRemainingSizeLo, iRemainingSizeHi, iRemainingSizeMB;
		unsigned long iPausedSizeLo, iPausedSizeHi, iPausedSizeMB;
		Util::SplitInt64(pGroupInfo->GetRemainingSize(), &iRemainingSizeHi, &iRemainingSizeLo);
		iRemainingSizeMB = (int)(pGroupInfo->GetRemainingSize() / 1024 / 1024);
		Util::SplitInt64(pGroupInfo->GetPausedSize(), &iPausedSizeHi, &iPausedSizeLo);
		iPausedSizeMB = (int)(pGroupInfo->GetPausedSize() / 1024 / 1024);

		snprintf(szItemBuf, iItemBufSize, IsJson() ? JSON_LIST_ITEM_START : XML_LIST_ITEM_START,
			pGroupInfo->GetFirstID(), pGroupInfo->GetLastID(), iRemainingSizeLo, iRemainingSizeHi, iRemainingSizeMB,
			iPausedSizeLo, iPausedSizeHi, iPausedSizeMB, pGroupInfo->GetRemainingFileCount(),
			pGroupInfo->GetRemainingParCount(), pGroupInfo->GetMinTime(), pGroupInfo->GetMaxTime(),
			pGroupInfo->GetMinPriority(), pGroupInfo->GetMaxPriority(), pGroupInfo->GetActiveDownloads());
		szItemBuf[iItemBufSize-1] = '\0';

		if (IsJson() && index++ > 0)
		{
			AppendResponse(",\n");
		}
		AppendResponse(szItemBuf);
		
		AppendNZBInfoFields(pGroupInfo->GetNZBInfo());

		AppendResponse(IsJson() ? JSON_LIST_ITEM_END : XML_LIST_ITEM_END);
	}
	free(szItemBuf);

	AppendResponse(IsJson() ? "\n]" : "</data></array>\n");
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
	{ QueueEditor::eaFileSetPriority, "FileSetPriority" },
	{ QueueEditor::eaFileReorder, "FileReorder" },
	{ QueueEditor::eaFileSplit, "FileSplit" },
	{ QueueEditor::eaGroupMoveOffset, "GroupMoveOffset" },
	{ QueueEditor::eaGroupMoveTop, "GroupMoveTop" },
	{ QueueEditor::eaGroupMoveBottom, "GroupMoveBottom" },
	{ QueueEditor::eaGroupPause, "GroupPause" },
	{ QueueEditor::eaGroupResume, "GroupResume" },
	{ QueueEditor::eaGroupDelete, "GroupDelete" },
	{ QueueEditor::eaGroupPauseAllPars, "GroupPauseAllPars" },
	{ QueueEditor::eaGroupPauseExtraPars, "GroupPauseExtraPars" },
	{ QueueEditor::eaGroupSetPriority, "GroupSetPriority" },
	{ QueueEditor::eaGroupSetCategory, "GroupSetCategory" },
	{ QueueEditor::eaGroupMerge, "GroupMerge" },
	{ QueueEditor::eaGroupSetParameter, "GroupSetParameter" },
	{ QueueEditor::eaGroupSetName, "GroupSetName" },
	{ QueueEditor::eaGroupSetDupeKey, "GroupSetDupeKey" },
	{ QueueEditor::eaGroupSetDupeScore, "GroupSetDupeScore" },
	{ QueueEditor::eaGroupSetDupeMode, "GroupSetDupeMode" },
	{ QueueEditor::eaGroupMarkDupe, "GroupMarkDupe" },
	{ PrePostProcessor::eaPostMoveOffset, "PostMoveOffset" },
	{ PrePostProcessor::eaPostMoveTop, "PostMoveTop" },
	{ PrePostProcessor::eaPostMoveBottom, "PostMoveBottom" },
	{ PrePostProcessor::eaPostDelete, "PostDelete" },
	{ PrePostProcessor::eaHistoryDelete, "HistoryDelete" },
	{ PrePostProcessor::eaHistoryReturn, "HistoryReturn" },
	{ PrePostProcessor::eaHistoryProcess, "HistoryProcess" },
	{ PrePostProcessor::eaHistorySetParameter, "HistorySetParameter" },
	{ PrePostProcessor::eaHistorySetDupeKey, "HistorySetDupeKey" },
	{ PrePostProcessor::eaHistorySetDupeScore, "HistorySetDupeScore" },
	{ PrePostProcessor::eaHistorySetDupeMode, "HistorySetDupeMode" },
	{ PrePostProcessor::eaHistoryMarkBad, "HistoryMarkBad" },
	{ PrePostProcessor::eaHistoryMarkGood, "HistoryMarkGood" },
	{ 0, NULL }
};

void EditQueueXmlCommand::Execute()
{
	if (!CheckSafeMethod())
	{
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

	DecodeStr(szEditText);

	IDList cIDList;
	int iID = 0;
	while (NextParamAsInt(&iID))
	{
		cIDList.push_back(iID);
	}

	bool bOK = false;

	if (iAction < PrePostProcessor::eaPostMoveOffset)
	{
		bOK = g_pQueueCoordinator->GetQueueEditor()->EditList(&cIDList, NULL, QueueEditor::mmID, true, (QueueEditor::EEditAction)iAction, iOffset, szEditText);
	}
	else
	{
		bOK = g_pPrePostProcessor->QueueEditList(&cIDList, (PrePostProcessor::EEditAction)iAction, iOffset, szEditText);
	}

	BuildBoolResponse(bOK);
}

// bool append(string NZBFilename, string Category, int Priority, bool AddToTop, string Content, bool AddPaused, string DupeKey, int DupeScore, string DupeMode)
// For backward compatibility with 0.8 parameter "Priority" is optional
// Parameters starting from "AddPaused" are optional (added in v12)
void DownloadXmlCommand::Execute()
{
	if (!CheckSafeMethod())
	{
		return;
	}

	char* szFileName;
	if (!NextParamAsStr(&szFileName))
	{
		BuildErrorResponse(2, "Invalid parameter (FileName)");
		return;
	}

	char* szCategory;
	if (!NextParamAsStr(&szCategory))
	{
		BuildErrorResponse(2, "Invalid parameter (Category)");
		return;
	}

	DecodeStr(szFileName);
	DecodeStr(szCategory);

	debug("FileName=%s", szFileName);

	// For backward compatibility with 0.8 parameter "Priority" is optional (error checking omitted)
	int iPriority = 0;
	NextParamAsInt(&iPriority);

	bool bAddTop;
	if (!NextParamAsBool(&bAddTop))
	{
		BuildErrorResponse(2, "Invalid parameter (AddTop)");
		return;
	}

	char* szFileContent;
	if (!NextParamAsStr(&szFileContent))
	{
		BuildErrorResponse(2, "Invalid parameter (FileContent)");
		return;
	}

	bool bAddPaused = false;
	char* szDupeKey = NULL;
	int iDupeScore = 0;
	EDupeMode eDupeMode = dmScore;
	if (NextParamAsBool(&bAddPaused))
	{
		if (!NextParamAsStr(&szDupeKey))
		{
			BuildErrorResponse(2, "Invalid parameter (DupeKey)");
			return;
		}
		if (!NextParamAsInt(&iDupeScore))
		{
			BuildErrorResponse(2, "Invalid parameter (DupeScore)");
			return;
		}
		char* szDupeMode = NULL;
		if (!NextParamAsStr(&szDupeMode) ||
			(strcasecmp(szDupeMode, "score") && strcasecmp(szDupeMode, "all") && strcasecmp(szDupeMode, "force")))
		{
			BuildErrorResponse(2, "Invalid parameter (DupeMode)");
			return;
		}
		eDupeMode = !strcasecmp(szDupeMode, "all") ? dmAll :
			!strcasecmp(szDupeMode, "force") ? dmForce : dmScore;
	}

	if (IsJson())
	{
		// JSON-string may contain '/'-character used in Base64, which must be escaped in JSON
		WebUtil::JsonDecode(szFileContent);
	}

	int iLen = WebUtil::DecodeBase64(szFileContent, 0, szFileContent);
	szFileContent[iLen] = '\0';
	//debug("FileContent=%s", szFileContent);

	bool bOK = g_pScanner->AddExternalFile(szFileName, szCategory, iPriority,
		szDupeKey, iDupeScore, eDupeMode, NULL, bAddTop, bAddPaused, NULL, szFileContent, iLen) != Scanner::asFailed;

	BuildBoolResponse(bOK);
}

void PostQueueXmlCommand::Execute()
{
	int iNrEntries = 0;
	NextParamAsInt(&iNrEntries);

	AppendResponse(IsJson() ? "[\n" : "<array><data>\n");

	const char* XML_POSTQUEUE_ITEM_START = 
		"<value><struct>\n"
		"<member><name>ID</name><value><i4>%i</i4></value></member>\n"
		"<member><name>InfoName</name><value><string>%s</string></value></member>\n"
		"<member><name>ParFilename</name><value><string></string></value></member>\n"		// deprecated, always empty
		"<member><name>Stage</name><value><string>%s</string></value></member>\n"
		"<member><name>ProgressLabel</name><value><string>%s</string></value></member>\n"
		"<member><name>FileProgress</name><value><i4>%i</i4></value></member>\n"
		"<member><name>StageProgress</name><value><i4>%i</i4></value></member>\n"
		"<member><name>TotalTimeSec</name><value><i4>%i</i4></value></member>\n"
		"<member><name>StageTimeSec</name><value><i4>%i</i4></value></member>\n";

	const char* XML_LOG_START =
		"<member><name>Log</name><value><array><data>\n";

	const char* XML_POSTQUEUE_ITEM_END =
		"</data></array></value></member>\n"
		"</struct></value>\n";
		
	const char* JSON_POSTQUEUE_ITEM_START =
		"{\n"
		"\"ID\" : %i,\n"
		"\"InfoName\" : \"%s\",\n"
		"\"ParFilename\" : \"\",\n"	// deprecated, always empty
		"\"Stage\" : \"%s\",\n"
		"\"ProgressLabel\" : \"%s\",\n"
		"\"FileProgress\" : %i,\n"
		"\"StageProgress\" : %i,\n"
		"\"TotalTimeSec\" : %i,\n"
		"\"StageTimeSec\" : %i,\n";

	const char* JSON_LOG_START =
		",\n"
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
	int iItemBufSize = 10240;
	char* szItemBuf = (char*)malloc(iItemBufSize);
	int index = 0;

	for (PostQueue::iterator it = pPostQueue->begin(); it != pPostQueue->end(); it++)
	{
		PostInfo* pPostInfo = *it;

	    const char* szPostStageName[] = { "QUEUED", "LOADING_PARS", "VERIFYING_SOURCES", "REPAIRING", "VERIFYING_REPAIRED", "RENAMING", "UNPACKING", "MOVING", "EXECUTING_SCRIPT", "FINISHED" };

		char* xmlInfoName = EncodeStr(pPostInfo->GetInfoName());
		char* xmlProgressLabel = EncodeStr(pPostInfo->GetProgressLabel());

		snprintf(szItemBuf, iItemBufSize, IsJson() ? JSON_POSTQUEUE_ITEM_START : XML_POSTQUEUE_ITEM_START,
			pPostInfo->GetID(), xmlInfoName, szPostStageName[pPostInfo->GetStage()], xmlProgressLabel,
			pPostInfo->GetFileProgress(), pPostInfo->GetStageProgress(),
			pPostInfo->GetStartTime() ? tCurTime - pPostInfo->GetStartTime() : 0,
			pPostInfo->GetStageTime() ? tCurTime - pPostInfo->GetStageTime() : 0);
		szItemBuf[iItemBufSize-1] = '\0';

		free(xmlInfoName);
		free(xmlProgressLabel);

		if (IsJson() && index++ > 0)
		{
			AppendResponse(",\n");
		}
		AppendResponse(szItemBuf);
		
		AppendNZBInfoFields(pPostInfo->GetNZBInfo());

		AppendResponse(IsJson() ? JSON_LOG_START : XML_LOG_START);

		if (iNrEntries > 0)
		{
			PostInfo::Messages* pMessages = pPostInfo->LockMessages();
			if (!pMessages->empty())
			{
				if (iNrEntries > (int)pMessages->size())
				{
					iNrEntries = pMessages->size();
				}
				int iStart = pMessages->size() - iNrEntries;

				int index = 0;
				for (unsigned int i = (unsigned int)iStart; i < pMessages->size(); i++)
				{
					Message* pMessage = (*pMessages)[i];
					char* xmltext = EncodeStr(pMessage->GetText());
					snprintf(szItemBuf, iItemBufSize, IsJson() ? JSON_LOG_ITEM : XML_LOG_ITEM,
						pMessage->GetID(), szMessageType[pMessage->GetKind()], pMessage->GetTime(), xmltext);
					szItemBuf[iItemBufSize-1] = '\0';
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
	if (!CheckSafeMethod())
	{
		return;
	}

	char* szKind;
	char* szText;
	if (!NextParamAsStr(&szKind) || !NextParamAsStr(&szText))
	{
		BuildErrorResponse(2, "Invalid parameter");
		return;
	}

	DecodeStr(szText);

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

void ClearLogXmlCommand::Execute()
{
	if (!CheckSafeMethod())
	{
		return;
	}

	g_pLog->Clear();

	BuildBoolResponse(true);
}

void ScanXmlCommand::Execute()
{
	if (!CheckSafeMethod())
	{
		return;
	}

	bool bSyncMode = false;
	// optional parameter "SyncMode"
	NextParamAsBool(&bSyncMode);

	g_pScanner->ScanNZBDir(bSyncMode);
	BuildBoolResponse(true);
}

// struct[] history(bool Dup)
// Parameter "Dup" is optional (new in v12)
void HistoryXmlCommand::Execute()
{
	AppendResponse(IsJson() ? "[\n" : "<array><data>\n");

	const char* XML_HISTORY_ITEM_START =
		"<value><struct>\n"
		"<member><name>ID</name><value><i4>%i</i4></value></member>\n"
		"<member><name>Kind</name><value><string>%s</string></value></member>\n"
		"<member><name>Name</name><value><string>%s</string></value></member>\n"
		"<member><name>RemainingFileCount</name><value><i4>%i</i4></value></member>\n"
		"<member><name>HistoryTime</name><value><i4>%i</i4></value></member>\n"
		"<member><name>URL</name><value><string>%s</string></value></member>\n"
		"<member><name>UrlStatus</name><value><string>%s</string></value></member>\n";

	const char* XML_HISTORY_ITEM_LOG_START =
		"<member><name>Log</name><value><array><data>\n";

	const char* XML_HISTORY_ITEM_END =
		"</data></array></value></member>\n"
		"</struct></value>\n";

	const char* JSON_HISTORY_ITEM_START =
		"{\n"
		"\"ID\" : %i,\n"
		"\"Kind\" : \"%s\",\n"
		"\"Name\" : \"%s\",\n"
		"\"RemainingFileCount\" : %i,\n"
		"\"HistoryTime\" : %i,\n"
		"\"URL\" : \"%s\",\n"
		"\"UrlStatus\" : \"%s\",\n";
	
	const char* JSON_HISTORY_ITEM_LOG_START =
		"\"Log\" : [\n";

	const char* JSON_HISTORY_ITEM_END = 
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

	const char* XML_HISTORY_DUP_ITEM =
		"<value><struct>\n"
		"<member><name>ID</name><value><i4>%i</i4></value></member>\n"
		"<member><name>Kind</name><value><string>%s</string></value></member>\n"
		"<member><name>Name</name><value><string>%s</string></value></member>\n"
		"<member><name>HistoryTime</name><value><i4>%i</i4></value></member>\n"
		"<member><name>FileSizeLo</name><value><i4>%u</i4></value></member>\n"
		"<member><name>FileSizeHi</name><value><i4>%u</i4></value></member>\n"
		"<member><name>FileSizeMB</name><value><i4>%i</i4></value></member>\n"
		"<member><name>DupeKey</name><value><string>%s</string></value></member>\n"
		"<member><name>DupeScore</name><value><i4>%i</i4></value></member>\n"
		"<member><name>DupeMode</name><value><string>%s</string></value></member>\n"
		"<member><name>DupStatus</name><value><string>%s</string></value></member>\n"
		"</struct></value>\n";

	const char* JSON_HISTORY_DUP_ITEM =
		"{\n"
		"\"ID\" : %i,\n"
		"\"Kind\" : \"%s\",\n"
		"\"Name\" : \"%s\",\n"
		"\"HistoryTime\" : %i,\n"
		"\"FileSizeLo\" : %i,\n"
		"\"FileSizeHi\" : %i,\n"
		"\"FileSizeMB\" : %i,\n"
		"\"DupeKey\" : \"%s\",\n"
		"\"DupeScore\" : %i,\n"
		"\"DupeMode\" : \"%s\",\n"
		"\"DupStatus\" : \"%s\",\n";

	const char* szUrlStatusName[] = { "UNKNOWN", "UNKNOWN", "SUCCESS", "FAILURE", "UNKNOWN", "SCAN_SKIPPED", "SCAN_FAILURE" };
	const char* szDupStatusName[] = { "UNKNOWN", "SUCCESS", "FAILURE", "DELETED", "DUPE", "BAD", "GOOD" };
	const char* szMessageType[] = { "INFO", "WARNING", "ERROR", "DEBUG", "DETAIL"};
    const char* szDupeModeName[] = { "SCORE", "ALL", "FORCE" };

	bool bDup = false;
	NextParamAsBool(&bDup);

	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	int iItemBufSize = 10240;
	char* szItemBuf = (char*)malloc(iItemBufSize);
	NZBInfo* pUrlNZBInfo = new NZBInfo(); // fake NZB-Info for Urls
	int index = 0;

	for (HistoryList::iterator it = pDownloadQueue->GetHistoryList()->begin(); it != pDownloadQueue->GetHistoryList()->end(); it++)
	{
		HistoryInfo* pHistoryInfo = *it;

		if (pHistoryInfo->GetKind() == HistoryInfo::hkDupInfo && !bDup)
		{
			continue;
		}

		NZBInfo* pNZBInfo = NULL;
		char szNicename[1024];
		pHistoryInfo->GetName(szNicename, sizeof(szNicename));

		char *xmlNicename = EncodeStr(szNicename);

		if (pHistoryInfo->GetKind() == HistoryInfo::hkNZBInfo)
		{
			pNZBInfo = pHistoryInfo->GetNZBInfo();

			snprintf(szItemBuf, iItemBufSize, IsJson() ? JSON_HISTORY_ITEM_START : XML_HISTORY_ITEM_START,
				pHistoryInfo->GetID(), "NZB", xmlNicename, pNZBInfo->GetParkedFileCount(),
				pHistoryInfo->GetTime(), "", "");
		}
		else if (pHistoryInfo->GetKind() == HistoryInfo::hkUrlInfo)
		{
			UrlInfo* pUrlInfo = pHistoryInfo->GetUrlInfo();

			char* xmlURL = EncodeStr(pUrlInfo->GetURL());

			snprintf(szItemBuf, iItemBufSize, IsJson() ? JSON_HISTORY_ITEM_START : XML_HISTORY_ITEM_START,
				pHistoryInfo->GetID(), "URL", xmlNicename, 0, pHistoryInfo->GetTime(), xmlURL,
				szUrlStatusName[pUrlInfo->GetStatus()]);

			free(xmlURL);

			pUrlNZBInfo->SetCategory(pUrlInfo->GetCategory());
			pUrlNZBInfo->SetFilename(pUrlInfo->GetNZBFilename());
			pNZBInfo = pUrlNZBInfo;
		}
		else if (pHistoryInfo->GetKind() == HistoryInfo::hkDupInfo)
		{
			DupInfo* pDupInfo = pHistoryInfo->GetDupInfo();

			unsigned long iFileSizeHi, iFileSizeLo, iFileSizeMB;
			Util::SplitInt64(pDupInfo->GetSize(), &iFileSizeHi, &iFileSizeLo);
			iFileSizeMB = (int)(pDupInfo->GetSize() / 1024 / 1024);

			char* xmlDupeKey = EncodeStr(pDupInfo->GetDupeKey());

			snprintf(szItemBuf, iItemBufSize, IsJson() ? JSON_HISTORY_DUP_ITEM : XML_HISTORY_DUP_ITEM,
				pHistoryInfo->GetID(), "DUP", xmlNicename, pHistoryInfo->GetTime(),
				iFileSizeLo, iFileSizeHi, iFileSizeMB, xmlDupeKey, pDupInfo->GetDupeScore(),
				szDupeModeName[pDupInfo->GetDupeMode()], szDupStatusName[pDupInfo->GetStatus()]);

			free(xmlDupeKey);
		}

		szItemBuf[iItemBufSize-1] = '\0';

		free(xmlNicename);

		if (IsJson() && index++ > 0)
		{
			AppendResponse(",\n");
		}
		AppendResponse(szItemBuf);

		if (pNZBInfo)
		{
			AppendNZBInfoFields(pNZBInfo);
			if (IsJson())
			{
				AppendResponse(",\n");
			}
		}
		
		AppendResponse(IsJson() ? JSON_HISTORY_ITEM_LOG_START : XML_HISTORY_ITEM_LOG_START);

		if (pNZBInfo)
		{
			// Log-Messages
			NZBInfo::Messages* pMessages = pNZBInfo->LockMessages();
			if (!pMessages->empty())
			{
				int iLogIndex = 0;
				for (NZBInfo::Messages::iterator it = pMessages->begin(); it != pMessages->end(); it++)
				{
					Message* pMessage = *it;
					char* xmltext = EncodeStr(pMessage->GetText());
					snprintf(szItemBuf, iItemBufSize, IsJson() ? JSON_LOG_ITEM : XML_LOG_ITEM,
						pMessage->GetID(), szMessageType[pMessage->GetKind()], pMessage->GetTime(), xmltext);
					szItemBuf[iItemBufSize-1] = '\0';
					free(xmltext);

					if (IsJson() && iLogIndex++ > 0)
					{
						AppendResponse(",\n");
					}
					AppendResponse(szItemBuf);
				}
			}
			pNZBInfo->UnlockMessages();
		}

		AppendResponse(IsJson() ? JSON_HISTORY_ITEM_END : XML_HISTORY_ITEM_END);
	}
	free(szItemBuf);
	delete pUrlNZBInfo;

	AppendResponse(IsJson() ? "\n]" : "</data></array>\n");

	g_pQueueCoordinator->UnlockQueue();
}

// bool appendurl(string NZBFilename, string Category, int Priority, bool AddToTop, string URL, bool AddPaused, string DupeKey, int DupeScore, string DupeMode)
// Parameters starting from "AddPaused" are optional (added in v12)
void DownloadUrlXmlCommand::Execute()
{
	if (!CheckSafeMethod())
	{
		return;
	}

	char* szNZBFileName;
	if (!NextParamAsStr(&szNZBFileName))
	{
		BuildErrorResponse(2, "Invalid parameter (NZBFilename)");
		return;
	}

	char* szCategory;
	if (!NextParamAsStr(&szCategory))
	{
		BuildErrorResponse(2, "Invalid parameter (Category)");
		return;
	}

	int iPriority = 0;
	if (!NextParamAsInt(&iPriority))
	{
		BuildErrorResponse(2, "Invalid parameter (Priority)");
		return;
	}

	bool bAddTop;
	if (!NextParamAsBool(&bAddTop))
	{
		BuildErrorResponse(2, "Invalid parameter (AddTop)");
		return;
	}

	char* szURL;
	if (!NextParamAsStr(&szURL))
	{
		BuildErrorResponse(2, "Invalid parameter (URL)");
		return;
	}

	bool bAddPaused = false;
	char* szDupeKey = NULL;
	int iDupeScore = 0;
	EDupeMode eDupeMode = dmScore;
	if (NextParamAsBool(&bAddPaused))
	{
		if (!NextParamAsStr(&szDupeKey))
		{
			BuildErrorResponse(2, "Invalid parameter (DupeKey)");
			return;
		}
		if (!NextParamAsInt(&iDupeScore))
		{
			BuildErrorResponse(2, "Invalid parameter (DupeScore)");
			return;
		}
		char* szDupeMode = NULL;
		if (!NextParamAsStr(&szDupeMode) ||
			(strcasecmp(szDupeMode, "score") && strcasecmp(szDupeMode, "all") && strcasecmp(szDupeMode, "force")))
		{
			BuildErrorResponse(2, "Invalid parameter (DupeMode)");
			return;
		}
		eDupeMode = !strcasecmp(szDupeMode, "all") ? dmAll :
			!strcasecmp(szDupeMode, "force") ? dmForce : dmScore;
	}

	DecodeStr(szNZBFileName);
	DecodeStr(szCategory);
	DecodeStr(szURL);
	if (szDupeKey)
	{
		DecodeStr(szDupeKey);
	}

	debug("URL=%s", szURL);

	UrlInfo* pUrlInfo = new UrlInfo();
	pUrlInfo->SetURL(szURL);
	pUrlInfo->SetNZBFilename(szNZBFileName);
	pUrlInfo->SetCategory(szCategory);
	pUrlInfo->SetPriority(iPriority);
	pUrlInfo->SetAddTop(bAddTop);
	pUrlInfo->SetAddPaused(bAddPaused);
	pUrlInfo->SetDupeKey(szDupeKey ? szDupeKey : "");
	pUrlInfo->SetDupeScore(iDupeScore);
	pUrlInfo->SetDupeMode(eDupeMode);

	char szNicename[1024];
	pUrlInfo->GetName(szNicename, sizeof(szNicename));
	info("Queue %s", szNicename);

	g_pUrlCoordinator->AddUrlToQueue(pUrlInfo, bAddTop);

	BuildBoolResponse(true);
}

void UrlQueueXmlCommand::Execute()
{
	AppendResponse(IsJson() ? "[\n" : "<array><data>\n");

	const char* XML_URLQUEUE_ITEM = 
		"<value><struct>\n"
		"<member><name>ID</name><value><i4>%i</i4></value></member>\n"
		"<member><name>NZBFilename</name><value><string>%s</string></value></member>\n"
		"<member><name>URL</name><value><string>%s</string></value></member>\n"
		"<member><name>Name</name><value><string>%s</string></value></member>\n"
		"<member><name>Category</name><value><string>%s</string></value></member>\n"
		"<member><name>Priority</name><value><i4>%i</i4></value></member>\n"
		"</struct></value>\n";

	const char* JSON_URLQUEUE_ITEM = 
		"{\n"
		"\"ID\" : %i,\n"
		"\"NZBFilename\" : \"%s\",\n"
		"\"URL\" : \"%s\",\n"
		"\"Name\" : \"%s\",\n"
		"\"Category\" : \"%s\",\n"
		"\"Priority\" : %i\n"
		"}";

	UrlQueue* pUrlQueue = g_pQueueCoordinator->LockQueue()->GetUrlQueue();

	int iItemBufSize = 10240;
	char* szItemBuf = (char*)malloc(iItemBufSize);
	int index = 0;

	for (UrlQueue::iterator it = pUrlQueue->begin(); it != pUrlQueue->end(); it++)
	{
		UrlInfo* pUrlInfo = *it;
		char szNicename[1024];
		pUrlInfo->GetName(szNicename, sizeof(szNicename));

		char* xmlNicename = EncodeStr(szNicename);
		char* xmlNZBFilename = EncodeStr(pUrlInfo->GetNZBFilename());
		char* xmlURL = EncodeStr(pUrlInfo->GetURL());
		char* xmlCategory = EncodeStr(pUrlInfo->GetCategory());

		snprintf(szItemBuf, iItemBufSize, IsJson() ? JSON_URLQUEUE_ITEM : XML_URLQUEUE_ITEM,
			pUrlInfo->GetID(), xmlNZBFilename, xmlURL, xmlNicename, xmlCategory, pUrlInfo->GetPriority());
		szItemBuf[iItemBufSize-1] = '\0';

		free(xmlNicename);
		free(xmlNZBFilename);
		free(xmlURL);
		free(xmlCategory);

		if (IsJson() && index++ > 0)
		{
			AppendResponse(",\n");
		}
		AppendResponse(szItemBuf);
	}
	free(szItemBuf);

	g_pQueueCoordinator->UnlockQueue();

	AppendResponse(IsJson() ? "\n]" : "</data></array>\n");
}

// struct[] config()
void ConfigXmlCommand::Execute()
{
	const char* XML_CONFIG_ITEM = 
		"<value><struct>\n"
		"<member><name>Name</name><value><string>%s</string></value></member>\n"
		"<member><name>Value</name><value><string>%s</string></value></member>\n"
		"</struct></value>\n";

	const char* JSON_CONFIG_ITEM = 
		"{\n"
		"\"Name\" : \"%s\",\n"
		"\"Value\" : \"%s\"\n"
		"}";

	AppendResponse(IsJson() ? "[\n" : "<array><data>\n");

	int iItemBufSize = 1024;
	char* szItemBuf = (char*)malloc(iItemBufSize);
	int index = 0;

	Options::OptEntries* pOptEntries = g_pOptions->LockOptEntries();

	for (Options::OptEntries::iterator it = pOptEntries->begin(); it != pOptEntries->end(); it++)
	{
		Options::OptEntry* pOptEntry = *it;

		char* xmlName = EncodeStr(pOptEntry->GetName());
		char* xmlValue = EncodeStr(pOptEntry->GetValue());

		// option values can sometimes have unlimited length
		int iValLen = strlen(xmlValue);
		if (iValLen > iItemBufSize - 500)
		{
			iItemBufSize = iValLen + 500;
			szItemBuf = (char*)realloc(szItemBuf, iItemBufSize);
		}

		snprintf(szItemBuf, iItemBufSize, IsJson() ? JSON_CONFIG_ITEM : XML_CONFIG_ITEM, xmlName, xmlValue);
		szItemBuf[iItemBufSize-1] = '\0';

		free(xmlName);
		free(xmlValue);

		if (IsJson() && index++ > 0)
		{
			AppendResponse(",\n");
		}
		AppendResponse(szItemBuf);
	}

	g_pOptions->UnlockOptEntries();

	free(szItemBuf);

	AppendResponse(IsJson() ? "\n]" : "</data></array>\n");
}

// struct[] loadconfig()
void LoadConfigXmlCommand::Execute()
{
	const char* XML_CONFIG_ITEM = 
		"<value><struct>\n"
		"<member><name>Name</name><value><string>%s</string></value></member>\n"
		"<member><name>Value</name><value><string>%s</string></value></member>\n"
		"</struct></value>\n";

	const char* JSON_CONFIG_ITEM = 
		"{\n"
		"\"Name\" : \"%s\",\n"
		"\"Value\" : \"%s\"\n"
		"}";

	Options::OptEntries* pOptEntries = new Options::OptEntries();
	if (!g_pOptions->LoadConfig(pOptEntries))
	{
		BuildErrorResponse(3, "Could not read configuration file");
		delete pOptEntries;
		return;
	}

	AppendResponse(IsJson() ? "[\n" : "<array><data>\n");

	int iItemBufSize = 1024;
	char* szItemBuf = (char*)malloc(iItemBufSize);
	int index = 0;

	for (Options::OptEntries::iterator it = pOptEntries->begin(); it != pOptEntries->end(); it++)
	{
		Options::OptEntry* pOptEntry = *it;

		char* xmlName = EncodeStr(pOptEntry->GetName());
		char* xmlValue = EncodeStr(pOptEntry->GetValue());

		// option values can sometimes have unlimited length
		int iValLen = strlen(xmlValue);
		if (iValLen > iItemBufSize - 500)
		{
			iItemBufSize = iValLen + 500;
			szItemBuf = (char*)realloc(szItemBuf, iItemBufSize);
		}

		snprintf(szItemBuf, iItemBufSize, IsJson() ? JSON_CONFIG_ITEM : XML_CONFIG_ITEM, xmlName, xmlValue);
		szItemBuf[iItemBufSize-1] = '\0';

		free(xmlName);
		free(xmlValue);

		if (IsJson() && index++ > 0)
		{
			AppendResponse(",\n");
		}
		AppendResponse(szItemBuf);
	}

	delete pOptEntries;

	free(szItemBuf);

	AppendResponse(IsJson() ? "\n]" : "</data></array>\n");
}

// bool saveconfig(struct[] data)
void SaveConfigXmlCommand::Execute()
{
	Options::OptEntries* pOptEntries = new Options::OptEntries();

	char* szName;
	char* szValue;
	char* szDummy;
	while (NextParamAsStr(&szDummy) && NextParamAsStr(&szName) && NextParamAsStr(&szDummy) && NextParamAsStr(&szValue))
	{
		DecodeStr(szName);
		DecodeStr(szValue);
		pOptEntries->push_back(new Options::OptEntry(szName, szValue));
	}

	// save to config file
	bool bOK = g_pOptions->SaveConfig(pOptEntries);

	delete pOptEntries;

	BuildBoolResponse(bOK);
}

// struct[] configtemplates()
void ConfigTemplatesXmlCommand::Execute()
{
	const char* XML_CONFIG_ITEM = 
		"<value><struct>\n"
		"<member><name>Name</name><value><string>%s</string></value></member>\n"
		"<member><name>DisplayName</name><value><string>%s</string></value></member>\n"
		"<member><name>Template</name><value><string>%s</string></value></member>\n"
		"</struct></value>\n";

	const char* JSON_CONFIG_ITEM = 
		"{\n"
		"\"Name\" : \"%s\",\n"
		"\"DisplayName\" : \"%s\",\n"
		"\"Template\" : \"%s\"\n"
		"}";

	Options::ConfigTemplates* pConfigTemplates = new Options::ConfigTemplates();

	if (!g_pOptions->LoadConfigTemplates(pConfigTemplates))
	{
		BuildErrorResponse(3, "Could not read configuration templates");
		delete pConfigTemplates;
		return;
	}

	AppendResponse(IsJson() ? "[\n" : "<array><data>\n");

	int index = 0;

	for (Options::ConfigTemplates::iterator it = pConfigTemplates->begin(); it != pConfigTemplates->end(); it++)
	{
		Options::ConfigTemplate* pConfigTemplate = *it;

		char* xmlName = EncodeStr(pConfigTemplate->GetName());
		char* xmlDisplayName = EncodeStr(pConfigTemplate->GetDisplayName());
		char* xmlTemplate = EncodeStr(pConfigTemplate->GetTemplate());

		int iItemBufSize = strlen(xmlName) + strlen(xmlTemplate) + 1024;
		char* szItemBuf = (char*)malloc(iItemBufSize);

		snprintf(szItemBuf, iItemBufSize, IsJson() ? JSON_CONFIG_ITEM : XML_CONFIG_ITEM, xmlName, xmlDisplayName, xmlTemplate);
		szItemBuf[iItemBufSize-1] = '\0';

		free(xmlName);
		free(xmlDisplayName);
		free(xmlTemplate);

		if (IsJson() && index++ > 0)
		{
			AppendResponse(",\n");
		}
		AppendResponse(szItemBuf);

		free(szItemBuf);
	}

	delete pConfigTemplates;

	AppendResponse(IsJson() ? "\n]" : "</data></array>\n");
}

ViewFeedXmlCommand::ViewFeedXmlCommand(bool bPreview)
{
	m_bPreview = bPreview;
}

// struct[] viewfeed(int id)
// struct[] previewfeed(string name, string url, string filter, bool includeNonMatching)
// struct[] previewfeed(string name, string url, string filter, bool pauseNzb, string category, int priority,
//		bool includeNonMatching, int cacheTimeSec, string cacheId)
void ViewFeedXmlCommand::Execute()
{
	bool bOK = false;
	bool bIncludeNonMatching = false;
	FeedItemInfos* pFeedItemInfos = NULL;

	if (m_bPreview)
	{
		char* szName;
		char* szUrl;
		char* szFilter;
		bool bPauseNzb;
		char* szCategory;
		int iPriority;
		char* szCacheId;
		int iCacheTimeSec;
		if (!NextParamAsStr(&szName) || !NextParamAsStr(&szUrl) || !NextParamAsStr(&szFilter) ||
			!NextParamAsBool(&bPauseNzb) || !NextParamAsStr(&szCategory) || !NextParamAsInt(&iPriority) ||
			!NextParamAsBool(&bIncludeNonMatching) || !NextParamAsInt(&iCacheTimeSec) ||
			!NextParamAsStr(&szCacheId))
		{
			BuildErrorResponse(2, "Invalid parameter");
			return;
		}

		DecodeStr(szName);
		DecodeStr(szUrl);
		DecodeStr(szFilter);
		DecodeStr(szCacheId);
		DecodeStr(szCategory);

		debug("Url=%s", szUrl);
		debug("Filter=%s", szFilter);

		bOK = g_pFeedCoordinator->PreviewFeed(szName, szUrl, szFilter,
			bPauseNzb, szCategory, iPriority, iCacheTimeSec, szCacheId, &pFeedItemInfos);
	}
	else
	{
		int iID = 0;
		if (!NextParamAsInt(&iID) || !NextParamAsBool(&bIncludeNonMatching))
		{
			BuildErrorResponse(2, "Invalid parameter");
			return;
		}

		debug("ID=%i", iID);

		bOK = g_pFeedCoordinator->ViewFeed(iID, &pFeedItemInfos);
	}

	if (!bOK)
	{
		BuildErrorResponse(3, "Could not read feed");
		return;
	}

	const char* XML_FEED_ITEM = 
		"<value><struct>\n"
		"<member><name>Title</name><value><string>%s</string></value></member>\n"
		"<member><name>Filename</name><value><string>%s</string></value></member>\n"
		"<member><name>URL</name><value><string>%s</string></value></member>\n"
		"<member><name>SizeLo</name><value><i4>%i</i4></value></member>\n"
		"<member><name>SizeHi</name><value><i4>%i</i4></value></member>\n"
		"<member><name>SizeMB</name><value><i4>%i</i4></value></member>\n"
		"<member><name>Category</name><value><string>%s</string></value></member>\n"
		"<member><name>AddCategory</name><value><string>%s</string></value></member>\n"
		"<member><name>PauseNzb</name><value><boolean>%s</boolean></value></member>\n"
		"<member><name>Priority</name><value><i4>%i</i4></value></member>\n"
		"<member><name>Time</name><value><i4>%i</i4></value></member>\n"
		"<member><name>Match</name><value><string>%s</string></value></member>\n"
		"<member><name>Rule</name><value><i4>%i</i4></value></member>\n"
		"<member><name>DupeKey</name><value><string>%s</string></value></member>\n"
		"<member><name>DupeScore</name><value><i4>%i</i4></value></member>\n"
		"<member><name>DupeMode</name><value><string>%s</string></value></member>\n"
		"<member><name>Status</name><value><string>%s</string></value></member>\n"
		"</struct></value>\n";

	const char* JSON_FEED_ITEM = 
		"{\n"
		"\"Title\" : \"%s\",\n"
		"\"Filename\" : \"%s\",\n"
		"\"URL\" : \"%s\",\n"
		"\"SizeLo\" : %i,\n"
		"\"SizeHi\" : %i,\n"
		"\"SizeMB\" : %i,\n"
		"\"Category\" : \"%s\",\n"
		"\"AddCategory\" : \"%s\",\n"
		"\"PauseNzb\" : %s,\n"
		"\"Priority\" : %i,\n"
		"\"Time\" : %i,\n"
		"\"Match\" : \"%s\",\n"
		"\"Rule\" : %i,\n"
		"\"DupeKey\" : \"%s\",\n"
		"\"DupeScore\" : %i,\n"
		"\"DupeMode\" : \"%s\",\n"
		"\"Status\" : \"%s\"\n"
		"}";

    const char* szStatusType[] = { "UNKNOWN", "BACKLOG", "FETCHED", "NEW" };
    const char* szMatchStatusType[] = { "IGNORED", "ACCEPTED", "REJECTED" };
    const char* szDupeModeType[] = { "SCORE", "ALL", "FORCE" };

	int iItemBufSize = 10240;
	char* szItemBuf = (char*)malloc(iItemBufSize);

	AppendResponse(IsJson() ? "[\n" : "<array><data>\n");
	int index = 0;

    for (FeedItemInfos::iterator it = pFeedItemInfos->begin(); it != pFeedItemInfos->end(); it++)
    {
        FeedItemInfo* pFeedItemInfo = *it;

		if (bIncludeNonMatching || pFeedItemInfo->GetMatchStatus() == FeedItemInfo::msAccepted)
		{
			unsigned long iSizeHi, iSizeLo;
			Util::SplitInt64(pFeedItemInfo->GetSize(), &iSizeHi, &iSizeLo);
			int iSizeMB = (int)(pFeedItemInfo->GetSize() / 1024 / 1024);

			char* xmltitle = EncodeStr(pFeedItemInfo->GetTitle());
			char* xmlfilename = EncodeStr(pFeedItemInfo->GetFilename());
			char* xmlurl = EncodeStr(pFeedItemInfo->GetUrl());
			char* xmlcategory = EncodeStr(pFeedItemInfo->GetCategory());
			char* xmladdcategory = EncodeStr(pFeedItemInfo->GetAddCategory());
			char* xmldupekey = EncodeStr(pFeedItemInfo->GetDupeKey());

			snprintf(szItemBuf, iItemBufSize, IsJson() ? JSON_FEED_ITEM : XML_FEED_ITEM,
				xmltitle, xmlfilename, xmlurl, iSizeLo, iSizeHi, iSizeMB, xmlcategory, xmladdcategory,
				BoolToStr(pFeedItemInfo->GetPauseNzb()), pFeedItemInfo->GetPriority(), pFeedItemInfo->GetTime(),
				szMatchStatusType[pFeedItemInfo->GetMatchStatus()], pFeedItemInfo->GetMatchRule(),
				xmldupekey, pFeedItemInfo->GetDupeScore(), szDupeModeType[pFeedItemInfo->GetDupeMode()],
				szStatusType[pFeedItemInfo->GetStatus()]);
			szItemBuf[iItemBufSize-1] = '\0';

			free(xmltitle);
			free(xmlfilename);
			free(xmlurl);
			free(xmlcategory);
			free(xmladdcategory);
			free(xmldupekey);

			if (IsJson() && index++ > 0)
			{
				AppendResponse(",\n");
			}
			AppendResponse(szItemBuf);
		}
    }

	free(szItemBuf);

	pFeedItemInfos->Release();

	AppendResponse(IsJson() ? "\n]" : "</data></array>\n");
}

void FetchFeedsXmlCommand::Execute()
{
	if (!CheckSafeMethod())
	{
		return;
	}

	g_pFeedCoordinator->FetchAllFeeds();

	BuildBoolResponse(true);
}

// bool editserver(int ID, bool Active)
void EditServerXmlCommand::Execute()
{
	if (!CheckSafeMethod())
	{
		return;
	}

	bool bOK = false;
	int bFirst = true;

	int iID;
	while (NextParamAsInt(&iID))
	{
		bFirst = false;

		bool bActive;
		if (!NextParamAsBool(&bActive))
		{
			BuildErrorResponse(2, "Invalid parameter");
			return;
		}

		for (Servers::iterator it = g_pServerPool->GetServers()->begin(); it != g_pServerPool->GetServers()->end(); it++)
		{
			NewsServer* pServer = *it;
			if (pServer->GetID() == iID)
			{
				pServer->SetActive(bActive);
				bOK = true;
			}
		}
	}

	if (bFirst)
	{
		BuildErrorResponse(2, "Invalid parameter");
		return;
	}

	if (bOK)
	{
		g_pServerPool->Changed();
	}

	BuildBoolResponse(bOK);
}
