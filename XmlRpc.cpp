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
			szAuthInfo[DecodeBase64(szAuthInfo64, 0, szAuthInfo)] = '\0';
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

void XmlRpcProcessor::Dispatch()
{
	XmlCommand* command = NULL;

	char szMethodName[100];
	if (!ParseTagValue(m_szRequest, "methodName", szMethodName, sizeof(szMethodName), NULL))
	{
		command = new ErrorXmlCommand(1, "Invalid request");
	}
	else if (!strcasecmp(szMethodName, "pause"))
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

	if (command)
	{
		debug("MethodName=%s", szMethodName);
		command->SetSocket(m_iSocket);
		command->SetRequest(m_szRequest);
		command->Execute();
		delete command;
	}
}


//*****************************************************************
// Commands

XmlCommand::XmlCommand()
{
	m_iSocket = INVALID_SOCKET;
	m_szRequest = NULL;
	m_szRequestPtr = NULL;
	m_szResponse = NULL;
	m_iResponseBufSize = 0;
	m_iResponseLen = 0;
}

XmlCommand::~XmlCommand()
{
	if (m_szResponse)
	{
		free(m_szResponse);
	}
}

void XmlCommand::AppendResponse(const char* szPart)
{
	int iPartLen = strlen(szPart);
	if (m_iResponseLen + iPartLen + 1 > m_iResponseBufSize)
	{
		m_iResponseBufSize += iPartLen + 10240;
		m_szResponse = (char*)realloc(m_szResponse, m_iResponseBufSize);
	}
	strcpy(m_szResponse + m_iResponseLen, szPart);
	m_iResponseLen += iPartLen;
	m_szResponse[m_iResponseLen] = '\0';
}

void XmlCommand::SendResponse(const char* szResponse)
{
	const char* RESPONSE_HEADER = 
		"HTTP/1.0 200 OK\n"
		"Connection: close\n"
		"Content-Length: %i\n"
		"Content-Type: text/xml\n"
		"Server: nzbget-%s\n"
		"\n";

	char szHeader[1024];
	int iLen = strlen(szResponse);
	snprintf(szHeader, 1024, RESPONSE_HEADER, iLen, VERSION);

	// Send the request answer
	send(m_iSocket, szHeader, strlen(szHeader), 0);
	send(m_iSocket, szResponse, iLen, 0);
}

void XmlCommand::SendErrorResponse(int iErrCode, const char* szErrText)
{
	const char* RESPONSE_ERROR_BODY = 
		"<?xml version=\"1.0\"?>\n"
		"<methodResponse>\n"
		"<fault><value><struct>\n"
		"<member><name>faultCode</name><value><int>%i</int></value></member>\n"
		"<member><name>faultString</name><value><string>%s</string></value></member>\n"
		"</struct></value></fault>\n"
		"</methodResponse>";

	char szContent[1024];
	snprintf(szContent, 1024, RESPONSE_ERROR_BODY, iErrCode, szErrText);
	szContent[1024-1] = '\0';

	SendResponse(szContent);
}

void XmlCommand::SendBoolResponse(bool bOK)
{
	const char* RESPONSE_BOOL_BODY = 
		"<?xml version=\"1.0\"?>\n"
		"<methodResponse>\n"
		"<params><param><value><boolean>%i</boolean></value></param></params>\n"
		"</methodResponse>";

	char szContent[1024];
	snprintf(szContent, 1024, RESPONSE_BOOL_BODY, (int)bOK);
	szContent[1024-1] = '\0';

	SendResponse(szContent);
}

bool XmlCommand::NextIntParam(int* iValue)
{
	int iLen = 0;
	const char* szParam = FindTag(m_szRequestPtr, "i4", &iLen);
	if (!szParam)
	{
		szParam = FindTag(m_szRequestPtr, "int", &iLen);
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
	SendErrorResponse(m_iErrCode, m_szErrText);
}

void PauseXmlCommand::Execute()
{
	g_pOptions->SetPause(true);
	SendBoolResponse(true);
}

void UnPauseXmlCommand::Execute()
{
	g_pOptions->SetPause(false);
	SendBoolResponse(true);
}

void ShutdownXmlCommand::Execute()
{
	SendBoolResponse(true);
	ExitProc();
}

void VersionXmlCommand::Execute()
{
	const char* RESPONSE_STRING_BODY = 
		"<?xml version=\"1.0\"?>\n"
		"<methodResponse>\n"
		"<params><param><value><string>%s</string></value></param></params>\n"
		"</methodResponse>";

	char szContent[1024];
	snprintf(szContent, 1024, RESPONSE_STRING_BODY, VERSION);
	szContent[1024-1] = '\0';

	SendResponse(szContent);
}

void DumpDebugXmlCommand::Execute()
{
	g_pQueueCoordinator->LogDebugInfo();
	SendBoolResponse(true);
}

void SetDownloadRateXmlCommand::Execute()
{
	int iRate = 0;
	if (!NextIntParam(&iRate) || iRate < 0)
	{
		SendErrorResponse(2, "Invalid parameter");
		return;
	}

	g_pOptions->SetDownloadRate(iRate);
	SendBoolResponse(true);
}

void StatusXmlCommand::Execute()
{
	const char* RESPONSE_STATUS_BODY = 
		"<?xml version=\"1.0\"?>\n"
		"<methodResponse>\n"
		"<params><param><value><struct>\n"
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
		"</struct></value></param></params>\n"
		"</methodResponse>";

	unsigned int iRemainingSizeHi, iRemainingSizeLo;
	int iDownloadRate = (int)(g_pQueueCoordinator->CalcCurrentDownloadSpeed() * 1024);
	long long iRemainingSize = g_pQueueCoordinator->CalcRemainingSize();
	SplitInt64(iRemainingSize, &iRemainingSizeHi, &iRemainingSizeLo);
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
	SplitInt64(iAllBytes, &iDownloadedSizeHi, &iDownloadedSizeLo);
	int iAverageDownloadRate = iDownloadTimeSec > 0 ? iAllBytes / iDownloadTimeSec : 0;

	char szContent[2048];
	snprintf(szContent, 2048, RESPONSE_STATUS_BODY, iRemainingSizeLo, iRemainingSizeHi,	iRemainingMBytes,
		iDownloadedSizeLo, iDownloadedSizeHi, iDownloadedMBytes, iDownloadRate, iAverageDownloadRate, iDownloadLimit,
		iThreadCount, iParJobCount, iUpTimeSec, iDownloadTimeSec, (int)bServerPaused, (int)bServerStandBy);
	szContent[2048-1] = '\0';

	SendResponse(szContent);
}

void LogXmlCommand::Execute()
{
	int iIDFrom = 0;
	int iNrEntries = 0;
	if (!NextIntParam(&iIDFrom) || !NextIntParam(&iNrEntries) || (iNrEntries > 0 && iIDFrom > 0))
	{
		SendErrorResponse(2, "Invalid parameter");
		return;
	}

	debug("iIDFrom=%i", iIDFrom);
	debug("iNrEntries=%i", iNrEntries);

	AppendResponse("<?xml version=\"1.0\"?>\n<methodResponse>\n<params><param><value><array><data>\n");
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

    char* szMessageType[] = { "INFO", "WARNING", "ERROR", "DEBUG"};
	int szItemBufSize = 10240;
	char* szItemBuf = (char*)malloc(szItemBufSize);

	for (unsigned int i = (unsigned int)iStart; i < pMessages->size(); i++)
	{
		Message* pMessage = (*pMessages)[i];
		char* xmltext = XmlEncode(pMessage->GetText());
		snprintf(szItemBuf, szItemBufSize, LOG_ITEM, pMessage->GetID(), szMessageType[pMessage->GetKind()], pMessage->GetTime(), xmltext);
		szItemBuf[szItemBufSize-1] = '\0';
		free(xmltext);
		AppendResponse(szItemBuf);
	}

	free(szItemBuf);

	g_pLog->UnlockMessages();
	AppendResponse("</data></array></value></param></params>\n</methodResponse>");

	debug("m_szResponse=%s", m_szResponse);
	SendResponse(m_szResponse);
}

void ListFilesXmlCommand::Execute()
{
	AppendResponse("<?xml version=\"1.0\"?>\n<methodResponse>\n<params><param><value><array><data>\n");

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
		SplitInt64(pFileInfo->GetSize(), &iFileSizeHi, &iFileSizeLo);
		SplitInt64(pFileInfo->GetRemainingSize(), &iRemainingSizeHi, &iRemainingSizeLo);
		char* xmlNZBFilename = XmlEncode(pFileInfo->GetNZBFilename());
		char* xmlSubject = XmlEncode(pFileInfo->GetSubject());
		char* xmlFilename = XmlEncode(pFileInfo->GetFilename());
		char* xmlDestDir = XmlEncode(pFileInfo->GetDestDir());

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
	AppendResponse("</data></array></value></param></params>\n</methodResponse>");

	debug("m_szResponse=%s", m_szResponse);
	SendResponse(m_szResponse);
}

void ListGroupsXmlCommand::Execute()
{
	AppendResponse("<?xml version=\"1.0\"?>\n<methodResponse>\n<params><param><value><array><data>\n");

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
		"<member><name>ParCount</name><value><i4>%i</i4></value></member>\n"
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
		SplitInt64(pGroupInfo->GetSize(), &iFileSizeHi, &iFileSizeLo);
		iFileSizeMB = pGroupInfo->GetSize() / 1024 / 1024;
		SplitInt64(pGroupInfo->GetRemainingSize(), &iRemainingSizeHi, &iRemainingSizeLo);
		iRemainingSizeMB = pGroupInfo->GetRemainingSize() / 1024 / 1024;
		SplitInt64(pGroupInfo->GetPausedSize(), &iPausedSizeHi, &iPausedSizeLo);
		iPausedSizeMB = pGroupInfo->GetPausedSize() / 1024 / 1024;
		FileInfo::MakeNiceNZBName(pGroupInfo->GetNZBFilename(), szNZBNicename, sizeof(szNZBNicename));

		char* xmlNZBNicename = XmlEncode(szNZBNicename);
		char* xmlNZBFilename = XmlEncode(pGroupInfo->GetNZBFilename());
		char* xmlDestDir = XmlEncode(pGroupInfo->GetDestDir());

		snprintf(szItemBuf, szItemBufSize, LIST_ITEM, pGroupInfo->GetFirstID(), pGroupInfo->GetLastID(),
			iFileSizeLo, iFileSizeHi, iFileSizeMB, iRemainingSizeLo, iRemainingSizeHi, iRemainingSizeMB,
			iPausedSizeLo, iPausedSizeHi, iPausedSizeMB, pGroupInfo->GetFileCount(), pGroupInfo->GetRemainingFileCount(), 
			pGroupInfo->GetParCount(), szNZBNicename, xmlNZBFilename, xmlDestDir);
		szItemBuf[szItemBufSize-1] = '\0';

		free(xmlNZBNicename);
		free(xmlNZBFilename);
		free(xmlDestDir);

		AppendResponse(szItemBuf);
	}
	free(szItemBuf);

	AppendResponse("</data></array></value></param></params>\n</methodResponse>");

	for (GroupQueue::iterator it = groupQueue.begin(); it != groupQueue.end(); it++)
	{
		delete *it;
	}
	groupQueue.clear();

	debug("m_szResponse=%s", m_szResponse);
	SendResponse(m_szResponse);
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
	if (!ParseTagValue(m_szRequest, "string", szEditCommand, sizeof(szEditCommand), NULL))
	{
		SendErrorResponse(2, "Invalid parameter");
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
		SendErrorResponse(3, "Invalid action");
		return;
	}

	int iOffset = 0;
	if (!NextIntParam(&iOffset))
	{
		SendErrorResponse(2, "Invalid parameter");
		return;
	}

	QueueEditor::IDList cIDList;
	int iID = 0;
	while (NextIntParam(&iID))
	{
		cIDList.push_back(iID);
	}

	bool bOK = g_pQueueCoordinator->GetQueueEditor()->EditList(&cIDList, true, (QueueEditor::EEditAction)iAction, iOffset);

	SendBoolResponse(bOK);
}

void DownloadXmlCommand::Execute()
{
	char szFileName[1024];
	if (!ParseTagValue(m_szRequest, "string", szFileName, sizeof(szFileName), NULL))
	{
		SendErrorResponse(2, "Invalid parameter");
		return;
	}

	XmlDecode(szFileName);
	debug("FileName=%s", szFileName);

	const char* pTagEnd;
	char szAddTop[10];
	if (!ParseTagValue(m_szRequest, "boolean", szAddTop, sizeof(szAddTop), &pTagEnd))
	{
		SendErrorResponse(2, "Invalid parameter");
		return;
	}

	bool bAddTop = !strcmp(szAddTop, "1");

	int iLen = 0;
	char* szFileContent = (char*)FindTag(pTagEnd, "string", &iLen);
	if (!szFileContent)
	{
		SendErrorResponse(2, "Invalid parameter");
		return;
	}

	szFileContent[DecodeBase64(szFileContent, iLen, szFileContent)] = '\0';
	//debug("FileContent=%s", szFileContent);

	NZBFile* pNZBFile = NZBFile::CreateFromBuffer(szFileName, szFileContent, iLen);

	if (pNZBFile)
	{
		info("Request: Queue collection %s", szFileName);
		g_pQueueCoordinator->AddNZBFileToQueue(pNZBFile, bAddTop);
		delete pNZBFile;
		SendBoolResponse(true);
	}
	else
	{
		SendBoolResponse(false);
	}
}

void PostQueueXmlCommand::Execute()
{
	AppendResponse("<?xml version=\"1.0\"?>\n<methodResponse>\n<params><param><value><array><data>\n");

	const char* POSTQUEUE_ITEM = 
		"<value><struct>\n"
		"<member><name>NZBNicename</name><value><string>%s</string></value></member>\n"
		"<member><name>NZBFilename</name><value><string>%s</string></value></member>\n"
		"<member><name>ParFilename</name><value><string>%s</string></value></member>\n"
		"<member><name>InfoName</name><value><string>%s</string></value></member>\n"
		"</struct></value>\n";

	PrePostProcessor::ParQueue* pParQueue = g_pPrePostProcessor->LockParQueue();

	int szItemBufSize = 10240;
	char* szItemBuf = (char*)malloc(szItemBufSize);

	for (PrePostProcessor::ParQueue::iterator it = pParQueue->begin(); it != pParQueue->end(); it++)
	{
		PrePostProcessor::ParJob* pParJob = *it;
		char szNZBNicename[1024];
		FileInfo::MakeNiceNZBName(pParJob->GetNZBFilename(), szNZBNicename, sizeof(szNZBNicename));

		char* xmlNZBNicename = XmlEncode(szNZBNicename);
		char* xmlNZBFilename = XmlEncode(pParJob->GetNZBFilename());
		char* xmlParFilename = XmlEncode(pParJob->GetParFilename());
		char* xmlInfoName = XmlEncode(pParJob->GetInfoName());

		snprintf(szItemBuf, szItemBufSize, POSTQUEUE_ITEM, szNZBNicename, xmlNZBFilename, xmlParFilename, xmlInfoName);
		szItemBuf[szItemBufSize-1] = '\0';

		free(xmlNZBNicename);
		free(xmlNZBFilename);
		free(xmlParFilename);
		free(xmlInfoName);

		AppendResponse(szItemBuf);
	}
	free(szItemBuf);

	g_pPrePostProcessor->UnlockParQueue();

	AppendResponse("</data></array></value></param></params>\n</methodResponse>");

	debug("m_szResponse=%s", m_szResponse);
	SendResponse(m_szResponse);
}
