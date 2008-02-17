/*
 *  This file is part of nzbget
 *
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
#include "config.h"
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
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

void XmlRpcProcessor::Execute()
{
	char szAuthInfo[1024];
	szAuthInfo[0] = '\0';

	Connection con(m_iSocket, false);

	// reading http header
	char szBuffer[1024];
	bool bBody = false;
	int iContentLen = 0;
	while (char* p = con.ReadLine(szBuffer, sizeof(szBuffer), NULL))
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
				error("invalid-request: auth-info to big");
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

	debug("Content-Length=%i", iContentLen);
	debug("Authorization=%s", szAuthInfo);

	if (iContentLen <= 0)
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
		warn("xml-rpc request received on port %i from %s, but password invalid", g_pOptions->GetServerPort(), m_szClientIP);
		return;
	}

	// reading http body (request content)
	m_szRequest = (char*)malloc(iContentLen + 1);
	m_szRequest[iContentLen] = '\0';

	if (!con.RecvAll(m_szRequest, iContentLen))
	{
		free(m_szRequest);
		error("invalid-request: could not read data");
		return;
	}

	debug("Request received from %s", m_szClientIP);
	debug("Request=%s", m_szRequest);

	Dispatch();

	free(m_szRequest);
}

XmlCommand* XmlRpcProcessor::CreateCommand(const char* szMethodName)
{
	XmlCommand* command = NULL;

	if (!strcasecmp(szMethodName, "pause"))
	{
		command = new PauseXmlCommand();
	}
	else if (!strcasecmp(szMethodName, "resume"))
	{
		command = new UnPauseXmlCommand();
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
	else 
	{
		command = new ErrorXmlCommand(1, "Invalid method");
	}

	return command;
}

void XmlRpcProcessor::Dispatch()
{
	char szMethodName[100];
	szMethodName[0] = '\0';
	Util::ParseTagValue(m_szRequest, "methodName", szMethodName, sizeof(szMethodName), NULL);

	debug("MethodName=%s", szMethodName);

	if (!strcasecmp(szMethodName, "system.multicall"))
	{
		MutliCall();
	}
	else
	{
		XmlCommand* command = CreateCommand(szMethodName);
		command->SetRequest(m_szRequest);
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
		Util::ParseTagValue(szNameEnd, "string", szMethodName, sizeof(szMethodName), NULL);
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
	const char* RESPONSE_HEADER = 
		"HTTP/1.0 200 OK\n"
		"Connection: close\n"
		"Content-Length: %i\n"
		"Content-Type: text/xml\n"
		"Server: nzbget-%s\n"
		"\n";
	const char XML_HEADER[] = "<?xml version=\"1.0\"?>\n<methodResponse>\n";
	const char XML_FOOTER[] = "</methodResponse>";
	const char XML_OK_OPEN[] = "<params><param><value>";
	const char XML_OK_CLOSE[] = "</value></param></params>\n";
	const char XML_FAULT_OPEN[] = "<fault><value>";
	const char XML_FAULT_CLOSE[] = "</value></fault>\n";

	debug("Response=%s", szResponse);

	const char* xmlOpenTag = bFault ? XML_FAULT_OPEN : XML_OK_OPEN;
	const char* xmlCloseTag = bFault ? XML_FAULT_CLOSE : XML_OK_CLOSE;
	int iOpenTagLen = (bFault ? sizeof(XML_FAULT_OPEN) : sizeof(XML_OK_OPEN)) - 1;
	int iCloseTagLen = (bFault ? sizeof(XML_FAULT_CLOSE) : sizeof(XML_OK_CLOSE)) - 1;
	int iResponseLen = strlen(szResponse);

	char szHeader[1024];
	int iBodyLen = iResponseLen + sizeof(XML_HEADER) - 1 + sizeof(XML_FOOTER) - 1 + iOpenTagLen + iCloseTagLen;
	snprintf(szHeader, 1024, RESPONSE_HEADER, iBodyLen, VERSION);

	// Send the request answer
	send(m_iSocket, szHeader, strlen(szHeader), 0);
	send(m_iSocket, XML_HEADER, sizeof(XML_HEADER) - 1, 0);
	send(m_iSocket, xmlOpenTag, iOpenTagLen, 0);
	send(m_iSocket, szResponse, iResponseLen, 0);
	send(m_iSocket, xmlCloseTag, iCloseTagLen, 0);
	send(m_iSocket, XML_FOOTER, sizeof(XML_FOOTER) - 1, 0);
}


//*****************************************************************
// Commands

XmlCommand::XmlCommand()
{
	m_szRequest = NULL;
	m_szRequestPtr = NULL;
	m_bFault = false;
}

void XmlCommand::AppendResponse(const char* szPart)
{
	m_StringBuilder.Append(szPart);
}

void XmlCommand::BuildErrorResponse(int iErrCode, const char* szErrText)
{
	const char* RESPONSE_ERROR_BODY = 
		"<struct>\n"
		"<member><name>faultCode</name><value><int>%i</int></value></member>\n"
		"<member><name>faultString</name><value><string>%s</string></value></member>\n"
		"</struct>\n";

	char szContent[1024];
	snprintf(szContent, 1024, RESPONSE_ERROR_BODY, iErrCode, szErrText);
	szContent[1024-1] = '\0';

	AppendResponse(szContent);

	m_bFault = true;
}

void XmlCommand::BuildBoolResponse(bool bOK)
{
	const char* RESPONSE_BOOL_BODY = "<boolean>%i</boolean>";

	char szContent[1024];
	snprintf(szContent, 1024, RESPONSE_BOOL_BODY, (int)bOK);
	szContent[1024-1] = '\0';

	AppendResponse(szContent);
}

bool XmlCommand::NextIntParam(int* iValue)
{
	int iLen = 0;
	const char* szParam = Util::FindTag(m_szRequestPtr, "i4", &iLen);
	if (!szParam)
	{
		szParam = Util::FindTag(m_szRequestPtr, "int", &iLen);
	}
	if (szParam)
	{
		*iValue = atoi(szParam);
		m_szRequestPtr = szParam + iLen;
	}
	return szParam != NULL;
}

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

void PauseXmlCommand::Execute()
{
	g_pOptions->SetPause(true);
	BuildBoolResponse(true);
}

void UnPauseXmlCommand::Execute()
{
	g_pOptions->SetPause(false);
	BuildBoolResponse(true);
}

void ShutdownXmlCommand::Execute()
{
	BuildBoolResponse(true);
	ExitProc();
}

void VersionXmlCommand::Execute()
{
	const char* RESPONSE_STRING_BODY = "<string>%s</string>";

	char szContent[1024];
	snprintf(szContent, 1024, RESPONSE_STRING_BODY, VERSION);
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
	int iRate = 0;
	if (!NextIntParam(&iRate) || iRate < 0)
	{
		BuildErrorResponse(2, "Invalid parameter");
		return;
	}

	g_pOptions->SetDownloadRate(iRate);
	BuildBoolResponse(true);
}

void StatusXmlCommand::Execute()
{
	const char* RESPONSE_STATUS_BODY = 
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
		"<member><name>ServerPaused</name><value><boolean>%i</boolean></value></member>\n"
		"<member><name>ServerStandBy</name><value><boolean>%i</boolean></value></member>\n"
		"</struct>\n";

	unsigned int iRemainingSizeHi, iRemainingSizeLo;
	int iDownloadRate = (int)(g_pQueueCoordinator->CalcCurrentDownloadSpeed() * 1024);
	long long iRemainingSize = g_pQueueCoordinator->CalcRemainingSize();
	Util::SplitInt64(iRemainingSize, &iRemainingSizeHi, &iRemainingSizeLo);
	int iRemainingMBytes = iRemainingSize / 1024 / 1024;
	int iDownloadLimit = (int)(g_pOptions->GetDownloadRate() * 1024);
	bool bServerPaused = g_pOptions->GetPause();
	int iThreadCount = Thread::GetThreadCount() - 1; // not counting itself
	PrePostProcessor::ParQueue* pParQueue = g_pPrePostProcessor->LockParQueue();
	int iParJobCount = pParQueue->size();
	g_pPrePostProcessor->UnlockParQueue();
	unsigned int iDownloadedSizeHi, iDownloadedSizeLo;
	int iUpTimeSec, iDownloadTimeSec;
	long long iAllBytes;
	bool bServerStandBy;
	g_pQueueCoordinator->CalcStat(&iUpTimeSec, &iDownloadTimeSec, &iAllBytes, &bServerStandBy);
	int iDownloadedMBytes = iAllBytes / 1024 / 1024;
	Util::SplitInt64(iAllBytes, &iDownloadedSizeHi, &iDownloadedSizeLo);
	int iAverageDownloadRate = iDownloadTimeSec > 0 ? iAllBytes / iDownloadTimeSec : 0;

	char szContent[2048];
	snprintf(szContent, 2048, RESPONSE_STATUS_BODY, iRemainingSizeLo, iRemainingSizeHi,	iRemainingMBytes,
		iDownloadedSizeLo, iDownloadedSizeHi, iDownloadedMBytes, iDownloadRate, iAverageDownloadRate, iDownloadLimit,
		iThreadCount, iParJobCount, iUpTimeSec, iDownloadTimeSec, (int)bServerPaused, (int)bServerStandBy);
	szContent[2048-1] = '\0';

	AppendResponse(szContent);
}

void LogXmlCommand::Execute()
{
	int iIDFrom = 0;
	int iNrEntries = 0;
	if (!NextIntParam(&iIDFrom) || !NextIntParam(&iNrEntries) || (iNrEntries > 0 && iIDFrom > 0))
	{
		BuildErrorResponse(2, "Invalid parameter");
		return;
	}

	debug("iIDFrom=%i", iIDFrom);
	debug("iNrEntries=%i", iNrEntries);

	AppendResponse("<array><data>\n");
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

	const char* LOG_ITEM = 
		"<value><struct>\n"
		"<member><name>ID</name><value><i4>%i</i4></value></member>\n"
		"<member><name>Kind</name><value><string>%s</string></value></member>\n"
		"<member><name>Time</name><value><i4>%i</i4></value></member>\n"
		"<member><name>Text</name><value><string>%s</string></value></member>\n"
		"</struct></value>\n";

    char* szMessageType[] = { "INFO", "WARNING", "ERROR", "DEBUG", "DETAIL"};
	int szItemBufSize = 10240;
	char* szItemBuf = (char*)malloc(szItemBufSize);

	for (unsigned int i = (unsigned int)iStart; i < pMessages->size(); i++)
	{
		Message* pMessage = (*pMessages)[i];
		char* xmltext = Util::XmlEncode(pMessage->GetText());
		snprintf(szItemBuf, szItemBufSize, LOG_ITEM, pMessage->GetID(), szMessageType[pMessage->GetKind()], pMessage->GetTime(), xmltext);
		szItemBuf[szItemBufSize-1] = '\0';
		free(xmltext);
		AppendResponse(szItemBuf);
	}

	free(szItemBuf);

	g_pLog->UnlockMessages();
	AppendResponse("</data></array>\n");
}

void ListFilesXmlCommand::Execute()
{
	AppendResponse("<array><data>\n");

	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	const char* LIST_ITEM = 
		"<value><struct>\n"
		"<member><name>ID</name><value><i4>%i</i4></value></member>\n"
		"<member><name>FileSizeLo</name><value><i4>%i</i4></value></member>\n"
		"<member><name>FileSizeHi</name><value><i4>%i</i4></value></member>\n"
		"<member><name>RemainingSizeLo</name><value><i4>%i</i4></value></member>\n"
		"<member><name>RemainingSizeHi</name><value><i4>%i</i4></value></member>\n"
		"<member><name>FilenameConfirmed</name><value><boolean>%i</boolean></value></member>\n"
		"<member><name>Paused</name><value><boolean>%i</boolean></value></member>\n"
		"<member><name>NZBFilename</name><value><string>%s</string></value></member>\n"
		"<member><name>Subject</name><value><string>%s</string></value></member>\n"
		"<member><name>Filename</name><value><string>%s</string></value></member>\n"
		"<member><name>DestDir</name><value><string>%s</string></value></member>\n"
		"</struct></value>\n";

	int szItemBufSize = 10240;
	char* szItemBuf = (char*)malloc(szItemBufSize);

	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		unsigned int iFileSizeHi, iFileSizeLo;
		unsigned int iRemainingSizeLo, iRemainingSizeHi;
		Util::SplitInt64(pFileInfo->GetSize(), &iFileSizeHi, &iFileSizeLo);
		Util::SplitInt64(pFileInfo->GetRemainingSize(), &iRemainingSizeHi, &iRemainingSizeLo);
		char* xmlNZBFilename = Util::XmlEncode(pFileInfo->GetNZBInfo()->GetFilename());
		char* xmlSubject = Util::XmlEncode(pFileInfo->GetSubject());
		char* xmlFilename = Util::XmlEncode(pFileInfo->GetFilename());
		char* xmlDestDir = Util::XmlEncode(pFileInfo->GetNZBInfo()->GetDestDir());

		snprintf(szItemBuf, szItemBufSize, LIST_ITEM, pFileInfo->GetID(), iFileSizeLo, iFileSizeHi, 
			iRemainingSizeLo, iRemainingSizeHi, (int)pFileInfo->GetFilenameConfirmed(), (int)pFileInfo->GetPaused(),
			xmlNZBFilename, xmlSubject, xmlFilename, xmlDestDir);
		szItemBuf[szItemBufSize-1] = '\0';

		free(xmlNZBFilename);
		free(xmlSubject);
		free(xmlFilename);
		free(xmlDestDir);

		AppendResponse(szItemBuf);
	}
	free(szItemBuf);

	g_pQueueCoordinator->UnlockQueue();
	AppendResponse("</data></array>\n");
}

void ListGroupsXmlCommand::Execute()
{
	AppendResponse("<array><data>\n");

	const char* LIST_ITEM = 
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
		"<member><name>NZBNicename</name><value><string>%s</string></value></member>\n"
		"<member><name>NZBFilename</name><value><string>%s</string></value></member>\n"
		"<member><name>DestDir</name><value><string>%s</string></value></member>\n"
		"</struct></value>\n";

	GroupQueue groupQueue;
	groupQueue.clear();
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();
	GroupInfo::BuildGroups(pDownloadQueue, &groupQueue);
	g_pQueueCoordinator->UnlockQueue();

	int szItemBufSize = 10240;
	char* szItemBuf = (char*)malloc(szItemBufSize);

	for (GroupQueue::iterator it = groupQueue.begin(); it != groupQueue.end(); it++)
	{
		GroupInfo* pGroupInfo = *it;
		unsigned int iFileSizeHi, iFileSizeLo, iFileSizeMB;
		unsigned int iRemainingSizeLo, iRemainingSizeHi, iRemainingSizeMB;
		unsigned int iPausedSizeLo, iPausedSizeHi, iPausedSizeMB;
		char szNZBNicename[1024];
		Util::SplitInt64(pGroupInfo->GetNZBInfo()->GetSize(), &iFileSizeHi, &iFileSizeLo);
		iFileSizeMB = pGroupInfo->GetNZBInfo()->GetSize() / 1024 / 1024;
		Util::SplitInt64(pGroupInfo->GetRemainingSize(), &iRemainingSizeHi, &iRemainingSizeLo);
		iRemainingSizeMB = pGroupInfo->GetRemainingSize() / 1024 / 1024;
		Util::SplitInt64(pGroupInfo->GetPausedSize(), &iPausedSizeHi, &iPausedSizeLo);
		iPausedSizeMB = pGroupInfo->GetPausedSize() / 1024 / 1024;
		pGroupInfo->GetNZBInfo()->GetNiceNZBName(szNZBNicename, sizeof(szNZBNicename));

		char* xmlNZBNicename = Util::XmlEncode(szNZBNicename);
		char* xmlNZBFilename = Util::XmlEncode(pGroupInfo->GetNZBInfo()->GetFilename());
		char* xmlDestDir = Util::XmlEncode(pGroupInfo->GetNZBInfo()->GetDestDir());

		snprintf(szItemBuf, szItemBufSize, LIST_ITEM, pGroupInfo->GetFirstID(), pGroupInfo->GetLastID(),
			iFileSizeLo, iFileSizeHi, iFileSizeMB, iRemainingSizeLo, iRemainingSizeHi, iRemainingSizeMB,
			iPausedSizeLo, iPausedSizeHi, iPausedSizeMB, pGroupInfo->GetNZBInfo()->GetFileCount(), 
			pGroupInfo->GetRemainingFileCount(), pGroupInfo->GetRemainingParCount(), szNZBNicename, xmlNZBFilename, xmlDestDir);
		szItemBuf[szItemBufSize-1] = '\0';

		free(xmlNZBNicename);
		free(xmlNZBFilename);
		free(xmlDestDir);

		AppendResponse(szItemBuf);
	}
	free(szItemBuf);

	AppendResponse("</data></array>\n");

	for (GroupQueue::iterator it = groupQueue.begin(); it != groupQueue.end(); it++)
	{
		delete *it;
	}
	groupQueue.clear();
}

typedef struct 
{
	QueueEditor::EEditAction	eActionID;
	const char*					szActionName;
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
	{ QueueEditor::eaFileMoveOffset, NULL }
};

void EditQueueXmlCommand::Execute()
{
	char szEditCommand[100];
	if (!Util::ParseTagValue(m_szRequest, "string", szEditCommand, sizeof(szEditCommand), NULL))
	{
		BuildErrorResponse(2, "Invalid parameter");
		return;
	}

	debug("EditCommand=%s", szEditCommand);
	int iAction = -1;
	for (int i = 0; ; i++)
	{
		const char* szName = EditCommandNameMap[i].szActionName;
		QueueEditor::EEditAction eID = EditCommandNameMap[i].eActionID;
		if (!szName)
		{
			break;
		}
		if (!strcasecmp(szEditCommand, szName))
		{
			iAction = (int)eID;
			break;
		}
	}

	if (iAction == -1)
	{
		BuildErrorResponse(3, "Invalid action");
		return;
	}

	int iOffset = 0;
	if (!NextIntParam(&iOffset))
	{
		BuildErrorResponse(2, "Invalid parameter");
		return;
	}

	QueueEditor::IDList cIDList;
	int iID = 0;
	while (NextIntParam(&iID))
	{
		cIDList.push_back(iID);
	}

	bool bOK = g_pQueueCoordinator->GetQueueEditor()->EditList(&cIDList, true, (QueueEditor::EEditAction)iAction, iOffset);

	BuildBoolResponse(bOK);
}

void DownloadXmlCommand::Execute()
{
	char szFileName[1024];
	if (!Util::ParseTagValue(m_szRequest, "string", szFileName, sizeof(szFileName), NULL))
	{
		BuildErrorResponse(2, "Invalid parameter");
		return;
	}

	Util::XmlDecode(szFileName);
	debug("FileName=%s", szFileName);

	const char* pTagEnd;
	char szAddTop[10];
	if (!Util::ParseTagValue(m_szRequest, "boolean", szAddTop, sizeof(szAddTop), &pTagEnd))
	{
		BuildErrorResponse(2, "Invalid parameter");
		return;
	}

	bool bAddTop = !strcmp(szAddTop, "1");

	int iLen = 0;
	char* szFileContent = (char*)Util::FindTag(pTagEnd, "string", &iLen);
	if (!szFileContent)
	{
		BuildErrorResponse(2, "Invalid parameter");
		return;
	}

	szFileContent[Util::DecodeBase64(szFileContent, iLen, szFileContent)] = '\0';
	//debug("FileContent=%s", szFileContent);

	NZBFile* pNZBFile = NZBFile::CreateFromBuffer(szFileName, szFileContent, iLen);

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
	AppendResponse("<array><data>\n");

	const char* POSTQUEUE_ITEM = 
		"<value><struct>\n"
		"<member><name>NZBNicename</name><value><string>%s</string></value></member>\n"
		"<member><name>NZBFilename</name><value><string>%s</string></value></member>\n"
		"<member><name>ParFilename</name><value><string>%s</string></value></member>\n"
		"<member><name>InfoName</name><value><string>%s</string></value></member>\n"
		"<member><name>Action</name><value><string>%s</string></value></member>\n"
		"<member><name>Stage</name><value><string>%s</string></value></member>\n"
		"<member><name>ProgressLabel</name><value><string>%s</string></value></member>\n"
		"<member><name>FileProgress</name><value><int>%i</int></value></member>\n"
		"<member><name>StageProgress</name><value><int>%i</int></value></member>\n"
		"<member><name>TotalTimeSec</name><value><int>%i</int></value></member>\n"
		"<member><name>StageTimeSec</name><value><int>%i</int></value></member>\n"
		"</struct></value>\n";

	PrePostProcessor::ParQueue* pParQueue = g_pPrePostProcessor->LockParQueue();

	time_t tCurTime = time(NULL);
	int szItemBufSize = 10240;
	char* szItemBuf = (char*)malloc(szItemBufSize);

	for (PrePostProcessor::ParQueue::iterator it = pParQueue->begin(); it != pParQueue->end(); it++)
	{
		PrePostProcessor::ParJob* pParJob = *it;
		char szNZBNicename[1024];
		NZBInfo::MakeNiceNZBName(pParJob->GetNZBFilename(), szNZBNicename, sizeof(szNZBNicename));

	    char* szParStageKind[] = { "QUEUED", "LOADING_PARS", "VERIFYING_SOURCES", "REPAIRING", "VERIFYING_REPAIRED" };

		char* xmlNZBNicename = Util::XmlEncode(szNZBNicename);
		char* xmlNZBFilename = Util::XmlEncode(pParJob->GetNZBFilename());
		char* xmlParFilename = Util::XmlEncode(pParJob->GetParFilename());
		char* xmlInfoName = Util::XmlEncode(pParJob->GetInfoName());
		char* xmlProgressLabel = pParJob->GetProgressLabel() ? Util::XmlEncode(pParJob->GetProgressLabel()) : NULL;

		snprintf(szItemBuf, szItemBufSize, POSTQUEUE_ITEM, szNZBNicename, xmlNZBFilename, xmlParFilename, 
			xmlInfoName, "par", szParStageKind[pParJob->GetStage()], pParJob->GetProgressLabel() ? xmlProgressLabel : "", 
			pParJob->GetFileProgress(), pParJob->GetStageProgress(), 
			pParJob->GetStartTime() ? tCurTime - pParJob->GetStartTime() : 0, 
			pParJob->GetStageTime() ? tCurTime - pParJob->GetStageTime() : 0);
		szItemBuf[szItemBufSize-1] = '\0';

		free(xmlNZBNicename);
		free(xmlNZBFilename);
		free(xmlParFilename);
		free(xmlInfoName);
		if (xmlProgressLabel)
		{
			free(xmlProgressLabel);
		}

		AppendResponse(szItemBuf);
	}
	free(szItemBuf);

	g_pPrePostProcessor->UnlockParQueue();

	AppendResponse("</data></array>\n");
}
