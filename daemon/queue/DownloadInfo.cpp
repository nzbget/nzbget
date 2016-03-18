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
#include "DownloadInfo.h"
#include "DiskState.h"
#include "Options.h"
#include "Util.h"
#include "FileSystem.h"

int FileInfo::m_idGen = 0;
int FileInfo::m_idMax = 0;
int NzbInfo::m_idGen = 0;
int NzbInfo::m_idMax = 0;
DownloadQueue* DownloadQueue::g_DownloadQueue = nullptr;
bool DownloadQueue::g_Loaded = false;

void NzbParameterList::SetParameter(const char* name, const char* value)
{
	bool emptyVal = Util::EmptyStr(value);

	iterator pos = std::find_if(begin(), end(),
		[name](NzbParameter& parameter)
		{
			return !strcmp(parameter.GetName(), name);
		});

	if (emptyVal && pos != end())
	{
		erase(pos);
	}
	else if (pos != end())
	{
		pos->SetValue(value);
	}
	else if (!emptyVal)
	{
		emplace_back(name, value);
	}
}

NzbParameter* NzbParameterList::Find(const char* name, bool caseSensitive)
{
	for (NzbParameter& parameter : this)
	{
		if ((caseSensitive && !strcmp(parameter.GetName(), name)) ||
			(!caseSensitive && !strcasecmp(parameter.GetName(), name)))
		{
			return &parameter;
		}
	}

	return nullptr;
}

void NzbParameterList::CopyFrom(NzbParameterList* sourceParameters)
{
	for (NzbParameter& parameter : sourceParameters)
	{
		SetParameter(parameter.GetName(), parameter.GetValue());
	}
}


ScriptStatus::EStatus ScriptStatusList::CalcTotalStatus()
{
	ScriptStatus::EStatus status = ScriptStatus::srNone;

	for (ScriptStatus& scriptStatus : this)
	{
		// Failure-Status overrides Success-Status
		if ((scriptStatus.GetStatus() == ScriptStatus::srSuccess && status == ScriptStatus::srNone) ||
			(scriptStatus.GetStatus() == ScriptStatus::srFailure))
		{
			status = scriptStatus.GetStatus();
		}
	}

	return status;
}


void ServerStatList::StatOp(int serverId, int successArticles, int failedArticles, EStatOperation statOperation)
{
	ServerStat* serverStat = nullptr;
	for (ServerStat& serverStat1 : this)
	{
		if (serverStat1.GetServerId() == serverId)
		{
			serverStat = &serverStat1;
			break;
		}
	}

	if (!serverStat)
	{
		emplace_back(serverId);
		serverStat = &back();
	}

	switch (statOperation)
	{
		case soSet:
			serverStat->SetSuccessArticles(successArticles);
			serverStat->SetFailedArticles(failedArticles);
			break;

		case soAdd:
			serverStat->SetSuccessArticles(serverStat->GetSuccessArticles() + successArticles);
			serverStat->SetFailedArticles(serverStat->GetFailedArticles() + failedArticles);
			break;

		case soSubtract:
			serverStat->SetSuccessArticles(serverStat->GetSuccessArticles() - successArticles);
			serverStat->SetFailedArticles(serverStat->GetFailedArticles() - failedArticles);
			break;
	}
}

void ServerStatList::ListOp(ServerStatList* serverStats, EStatOperation statOperation)
{
	for (ServerStat& serverStat : serverStats)
	{
		StatOp(serverStat.GetServerId(), serverStat.GetSuccessArticles(), serverStat.GetFailedArticles(), statOperation);
	}
}


void NzbInfo::SetId(int id)
{
	m_id = id;
	if (m_idMax < m_id)
	{
		m_idMax = m_id;
	}
}

void NzbInfo::ResetGenId(bool max)
{
	if (max)
	{
		m_idGen = m_idMax;
	}
	else
	{
		m_idGen = 0;
		m_idMax = 0;
	}
}

int NzbInfo::GenerateId()
{
	return ++m_idGen;
}

void NzbInfo::SetUrl(const char* url)
{
	m_url = url;

	if (!m_name)
	{
		CString nzbNicename = MakeNiceUrlName(url, m_filename);
		SetName(nzbNicename);
	}
}

void NzbInfo::SetFilename(const char* filename)
{
	bool hadFilename = !Util::EmptyStr(m_filename);
	m_filename = filename;

	if ((!m_name || !hadFilename) && !Util::EmptyStr(filename))
	{
		CString nzbNicename = MakeNiceNzbName(m_filename, true);
		SetName(nzbNicename);
	}
}

CString NzbInfo::MakeNiceNzbName(const char * nzbFilename, bool removeExt)
{
	CString nicename = FileSystem::BaseFileName(nzbFilename);
	if (removeExt)
	{
		// wipe out ".nzb"
		char* p = strrchr(nicename, '.');
		if (p && !strcasecmp(p, ".nzb")) *p = '\0';
	}
	FileSystem::MakeValidFilename(nicename, '_', false);
	return nicename;
}

CString NzbInfo::MakeNiceUrlName(const char* urlStr, const char* nzbFilename)
{
	CString urlNicename;
	URL url(urlStr);

	if (!Util::EmptyStr(nzbFilename))
	{
		CString nzbNicename = MakeNiceNzbName(nzbFilename, true);
		urlNicename.Format("%s @ %s", *nzbNicename, url.GetHost());
	}
	else if (url.IsValid())
	{
		urlNicename.Format("%s%s", url.GetHost(), url.GetResource());
	}
	else
	{
		urlNicename = urlStr;
	}

	return urlNicename;
}

void NzbInfo::BuildDestDirName()
{
	if (Util::EmptyStr(g_Options->GetInterDir()))
	{
		m_destDir = BuildFinalDirName();
	}
	else
	{
		m_destDir.Format("%s%c%s.#%i", g_Options->GetInterDir(), PATH_SEPARATOR, GetName(), GetId());
	}
}

CString NzbInfo::BuildFinalDirName()
{
	CString finalDir = g_Options->GetDestDir();
	bool useCategory = !m_category.Empty();

	if (useCategory)
	{
		Options::Category* category = g_Options->FindCategory(m_category, false);
		if (category && !Util::EmptyStr(category->GetDestDir()))
		{
			finalDir = category->GetDestDir();
			useCategory = false;
		}
	}

	if (g_Options->GetAppendCategoryDir() && useCategory)
	{
		BString<1024> categoryDir;
		categoryDir = m_category;
		FileSystem::MakeValidFilename(categoryDir, '_', true);
		// we can't format with "finalDir.Format" because one of the parameter is "finalDir" itself.
		finalDir = CString::FormatStr("%s%c%s", *finalDir, PATH_SEPARATOR, *categoryDir);
	}

	finalDir.AppendFmt("%c%s", PATH_SEPARATOR, GetName());

	return finalDir;
}

int NzbInfo::CalcHealth()
{
	if (m_currentFailedSize == 0 || m_size == m_parSize)
	{
		return 1000;
	}

	int health = (int)((m_size - m_parSize -
		(m_currentFailedSize - m_parCurrentFailedSize)) * 1000 / (m_size - m_parSize));

	if (health == 1000 && m_currentFailedSize - m_parCurrentFailedSize > 0)
	{
		health = 999;
	}

	return health;
}

int NzbInfo::CalcCriticalHealth(bool allowEstimation)
{
	if (m_size == 0)
	{
		return 1000;
	}

	if (m_size == m_parSize)
	{
		return 0;
	}

	int64 goodParSize = m_parSize - m_parCurrentFailedSize;
	int criticalHealth = (int)((m_size - goodParSize*2) * 1000 / (m_size - goodParSize));

	if (goodParSize*2 > m_size)
	{
		criticalHealth = 0;
	}
	else if (criticalHealth == 1000 && m_parSize > 0)
	{
		criticalHealth = 999;
	}

	if (criticalHealth == 1000 && allowEstimation)
	{
		// using empirical critical health 85%, to avoid false alarms for downloads with renamed par-files
		criticalHealth = 850;
	}

	return criticalHealth;
}

void NzbInfo::UpdateMinMaxTime()
{
	m_minTime = 0;
	m_maxTime = 0;

	bool first = true;
	for (FileInfo* fileInfo : &m_fileList)
	{
		if (first)
		{
			m_minTime = fileInfo->GetTime();
			m_maxTime = fileInfo->GetTime();
			first = false;
		}
		if (fileInfo->GetTime() > 0)
		{
			if (fileInfo->GetTime() < m_minTime)
			{
				m_minTime = fileInfo->GetTime();
			}
			if (fileInfo->GetTime() > m_maxTime)
			{
				m_maxTime = fileInfo->GetTime();
			}
		}
	}
}

void NzbInfo::AddMessage(Message::EKind kind, const char * text)
{
	switch (kind)
	{
		case Message::mkDetail:
			detail("%s", text);
			break;

		case Message::mkInfo:
			info("%s", text);
			break;

		case Message::mkWarning:
			warn("%s", text);
			break;

		case Message::mkError:
			error("%s", text);
			break;

		case Message::mkDebug:
			debug("%s", text);
			break;
	}

	Guard guard(m_logMutex);

	m_messages.emplace_back(++m_idMessageGen, kind, Util::CurrentTime(), text);

	if (g_Options->GetSaveQueue() && g_Options->GetServerMode() && g_Options->GetNzbLog())
	{
		g_DiskState->AppendNzbMessage(m_id, kind, text);
		m_messageCount++;
	}

	while (m_messages.size() > (uint32)g_Options->GetLogBufferSize())
	{
		m_messages.pop_front();
	}

	m_cachedMessageCount = m_messages.size();
}

void NzbInfo::PrintMessage(Message::EKind kind, const char* format, ...)
{
	char tmp2[1024];

	va_list ap;
	va_start(ap, format);
	vsnprintf(tmp2, 1024, format, ap);
	tmp2[1024-1] = '\0';
	va_end(ap);

	AddMessage(kind, tmp2);
}

void NzbInfo::ClearMessages()
{
	Guard guard(m_logMutex);
	m_messages.clear();
	m_cachedMessageCount = 0;
}

void NzbInfo::MoveFileList(NzbInfo* srcNzbInfo)
{
	m_fileList = std::move(*srcNzbInfo->GetFileList());
	for (FileInfo* fileInfo : &m_fileList)
	{
		fileInfo->SetNzbInfo(this);
	}

	SetFullContentHash(srcNzbInfo->GetFullContentHash());
	SetFilteredContentHash(srcNzbInfo->GetFilteredContentHash());

	SetFileCount(srcNzbInfo->GetFileCount());
	SetPausedFileCount(srcNzbInfo->GetPausedFileCount());
	SetRemainingParCount(srcNzbInfo->GetRemainingParCount());

	SetSize(srcNzbInfo->GetSize());
	SetRemainingSize(srcNzbInfo->GetRemainingSize());
	SetPausedSize(srcNzbInfo->GetPausedSize());
	SetSuccessSize(srcNzbInfo->GetSuccessSize());
	SetCurrentSuccessSize(srcNzbInfo->GetCurrentSuccessSize());
	SetFailedSize(srcNzbInfo->GetFailedSize());
	SetCurrentFailedSize(srcNzbInfo->GetCurrentFailedSize());

	SetParSize(srcNzbInfo->GetParSize());
	SetParSuccessSize(srcNzbInfo->GetParSuccessSize());
	SetParCurrentSuccessSize(srcNzbInfo->GetParCurrentSuccessSize());
	SetParFailedSize(srcNzbInfo->GetParFailedSize());
	SetParCurrentFailedSize(srcNzbInfo->GetParCurrentFailedSize());

	SetTotalArticles(srcNzbInfo->GetTotalArticles());
	SetSuccessArticles(srcNzbInfo->GetSuccessArticles());
	SetFailedArticles(srcNzbInfo->GetFailedArticles());
	SetCurrentSuccessArticles(srcNzbInfo->GetSuccessArticles());
	SetCurrentFailedArticles(srcNzbInfo->GetFailedArticles());

	SetMinTime(srcNzbInfo->GetMinTime());
	SetMaxTime(srcNzbInfo->GetMaxTime());
}

void NzbInfo::EnterPostProcess()
{
	m_postInfo = std::make_unique<PostInfo>();
	m_postInfo->SetNzbInfo(this);
}

void NzbInfo::LeavePostProcess()
{
	m_postInfo.reset();
	ClearMessages();
}

void NzbInfo::SetActiveDownloads(int activeDownloads)
{
	if (((m_activeDownloads == 0 && activeDownloads > 0) ||
		 (m_activeDownloads > 0 && activeDownloads == 0)) &&
		m_kind == NzbInfo::nkNzb)
	{
		if (activeDownloads > 0)
		{
			m_downloadStartTime = Util::CurrentTime();
		}
		else
		{
			m_downloadSec += Util::CurrentTime() - m_downloadStartTime;
			m_downloadStartTime = 0;
		}
	}
	m_activeDownloads = activeDownloads;
}

bool NzbInfo::IsDupeSuccess()
{
	bool failure =
		m_markStatus != NzbInfo::ksSuccess &&
		m_markStatus != NzbInfo::ksGood &&
		(m_deleteStatus != NzbInfo::dsNone ||
		m_markStatus == NzbInfo::ksBad ||
		m_parStatus == NzbInfo::psFailure ||
		m_unpackStatus == NzbInfo::usFailure ||
		m_unpackStatus == NzbInfo::usPassword ||
		(m_parStatus == NzbInfo::psSkipped &&
		 m_unpackStatus == NzbInfo::usSkipped &&
		 CalcHealth() < CalcCriticalHealth(true)));
	return !failure;
}

const char* NzbInfo::MakeTextStatus(bool ignoreScriptStatus)
{
	const char* status = "FAILURE/INTERNAL_ERROR";

	if (m_kind == NzbInfo::nkNzb)
	{
		int health = CalcHealth();
		int criticalHealth = CalcCriticalHealth(false);
		ScriptStatus::EStatus scriptStatus = ignoreScriptStatus ? ScriptStatus::srSuccess : m_scriptStatuses.CalcTotalStatus();

		if (m_markStatus == NzbInfo::ksBad)
		{
			status = "FAILURE/BAD";
		}
		else if (m_markStatus == NzbInfo::ksGood)
		{
			status = "SUCCESS/GOOD";
		}
		else if (m_markStatus == NzbInfo::ksSuccess)
		{
			status = "SUCCESS/MARK";
		}
		else if (m_deleteStatus == NzbInfo::dsHealth)
		{
			status = "FAILURE/HEALTH";
		}
		else if (m_deleteStatus == NzbInfo::dsManual)
		{
			status = "DELETED/MANUAL";
		}
		else if (m_deleteStatus == NzbInfo::dsDupe)
		{
			status = "DELETED/DUPE";
		}
		else if (m_deleteStatus == NzbInfo::dsBad)
		{
			status = "FAILURE/BAD";
		}
		else if (m_deleteStatus == NzbInfo::dsGood)
		{
			status = "DELETED/GOOD";
		}
		else if (m_deleteStatus == NzbInfo::dsCopy)
		{
			status = "DELETED/COPY";
		}
		else if (m_deleteStatus == NzbInfo::dsScan)
		{
			status = "FAILURE/SCAN";
		}
		else if (m_parStatus == NzbInfo::psFailure)
		{
			status = "FAILURE/PAR";
		}
		else if (m_unpackStatus == NzbInfo::usFailure)
		{
			status = "FAILURE/UNPACK";
		}
		else if (m_moveStatus == NzbInfo::msFailure)
		{
			status = "FAILURE/MOVE";
		}
		else if (m_parStatus == NzbInfo::psManual)
		{
			status = "WARNING/DAMAGED";
		}
		else if (m_parStatus == NzbInfo::psRepairPossible)
		{
			status = "WARNING/REPAIRABLE";
		}
		else if ((m_parStatus == NzbInfo::psNone || m_parStatus == NzbInfo::psSkipped) &&
				 (m_unpackStatus == NzbInfo::usNone || m_unpackStatus == NzbInfo::usSkipped) &&
				 health < criticalHealth)
		{
			status = "FAILURE/HEALTH";
		}
		else if ((m_parStatus == NzbInfo::psNone || m_parStatus == NzbInfo::psSkipped) &&
				 (m_unpackStatus == NzbInfo::usNone || m_unpackStatus == NzbInfo::usSkipped) &&
				 health < 1000 && health >= criticalHealth)
		{
			status = "WARNING/HEALTH";
		}
		else if ((m_parStatus == NzbInfo::psNone || m_parStatus == NzbInfo::psSkipped) &&
				 (m_unpackStatus == NzbInfo::usNone || m_unpackStatus == NzbInfo::usSkipped) &&
				 scriptStatus != ScriptStatus::srFailure && health == 1000)
		{
			status = "SUCCESS/HEALTH";
		}
		else if (m_unpackStatus == NzbInfo::usSpace)
		{
			status = "WARNING/SPACE";
		}
		else if (m_unpackStatus == NzbInfo::usPassword)
		{
			status = "WARNING/PASSWORD";
		}
		else if ((m_unpackStatus == NzbInfo::usSuccess ||
				  ((m_unpackStatus == NzbInfo::usNone || m_unpackStatus == NzbInfo::usSkipped) &&
				   m_parStatus == NzbInfo::psSuccess)) &&
				 scriptStatus == ScriptStatus::srSuccess)
		{
			status = "SUCCESS/ALL";
		}
		else if (m_unpackStatus == NzbInfo::usSuccess && scriptStatus == ScriptStatus::srNone)
		{
			status = "SUCCESS/UNPACK";
		}
		else if (m_parStatus == NzbInfo::psSuccess && scriptStatus == ScriptStatus::srNone)
		{
			status = "SUCCESS/PAR";
		}
		else if (scriptStatus == ScriptStatus::srFailure)
		{
			status = "WARNING/SCRIPT";
		}
	}
	else if (m_kind == NzbInfo::nkUrl)
	{
		if (m_deleteStatus == NzbInfo::dsManual)
		{
			status = "DELETED/MANUAL";
		}
		else if (m_deleteStatus == NzbInfo::dsDupe)
		{
			status = "DELETED/DUPE";
		}
		else
		{
			const char* urlStatusName[] = { "FAILURE/INTERNAL_ERROR", "FAILURE/INTERNAL_ERROR", "FAILURE/INTERNAL_ERROR",
				"FAILURE/FETCH", "FAILURE/INTERNAL_ERROR", "WARNING/SKIPPED", "FAILURE/SCAN" };
			status = urlStatusName[m_urlStatus];
		}
	}

	return status;
}


void ArticleInfo::AttachSegment(std::unique_ptr<SegmentData> content, int64 offset, int size)
{
	m_segmentContent = std::move(content);
	m_segmentOffset = offset;
	m_segmentSize = size;
}

void ArticleInfo::DiscardSegment()
{
	m_segmentContent.reset();
}


void FileInfo::SetId(int id)
{
	m_id = id;
	if (m_idMax < m_id)
	{
		m_idMax = m_id;
	}
}

void FileInfo::ResetGenId(bool max)
{
	if (max)
	{
		m_idGen = m_idMax;
	}
	else
	{
		m_idGen = 0;
		m_idMax = 0;
	}
}

void FileInfo::SetPaused(bool paused)
{
	if (m_paused != paused && m_nzbInfo)
	{
		m_nzbInfo->SetPausedFileCount(m_nzbInfo->GetPausedFileCount() + (paused ? 1 : -1));
		m_nzbInfo->SetPausedSize(m_nzbInfo->GetPausedSize() + (paused ? m_remainingSize : - m_remainingSize));
	}
	m_paused = paused;
}

void FileInfo::MakeValidFilename()
{
	FileSystem::MakeValidFilename(m_filename, '_', false);
}

void FileInfo::SetActiveDownloads(int activeDownloads)
{
	m_activeDownloads = activeDownloads;

	if (m_activeDownloads > 0 && !m_outputFileMutex)
	{
		m_outputFileMutex = std::make_unique<Mutex>();
	}
	else if (m_activeDownloads == 0)
	{
		m_outputFileMutex.reset();
	}
}


CompletedFile::CompletedFile(int id, const char* fileName, EStatus status, uint32 crc) :
	m_id(id), m_fileName(fileName), m_status(status), m_crc(crc)

{
	if (FileInfo::m_idMax < m_id)
	{
		FileInfo::m_idMax = m_id;
	}
}


void DupInfo::SetId(int id)
{
	m_id = id;
	if (NzbInfo::m_idMax < m_id)
	{
		NzbInfo::m_idMax = m_id;
	}
}


HistoryInfo::~HistoryInfo()
{
	if ((m_kind == hkNzb || m_kind == hkUrl) && m_info)
	{
		delete (NzbInfo*)m_info;
	}
	else if (m_kind == hkDup && m_info)
	{
		delete (DupInfo*)m_info;
	}
}

int HistoryInfo::GetId()
{
	if ((m_kind == hkNzb || m_kind == hkUrl))
	{
		return ((NzbInfo*)m_info)->GetId();
	}
	else // if (m_eKind == hkDup)
	{
		return ((DupInfo*)m_info)->GetId();
	}
}

const char* HistoryInfo::GetName()
{
	if (m_kind == hkNzb || m_kind == hkUrl)
	{
		return GetNzbInfo()->GetName();
	}
	else if (m_kind == hkDup)
	{
		return GetDupInfo()->GetName();
	}
	else
	{
		return "<unknown>";
	}
}


void DownloadQueue::CalcRemainingSize(int64* remaining, int64* remainingForced)
{
	int64 remainingSize = 0;
	int64 remainingForcedSize = 0;

	for (NzbInfo* nzbInfo : &m_queue)
	{
		for (FileInfo* fileInfo : nzbInfo->GetFileList())
		{
			if (!fileInfo->GetPaused() && !fileInfo->GetDeleted())
			{
				remainingSize += fileInfo->GetRemainingSize();
				if (nzbInfo->GetForcePriority())
				{
					remainingForcedSize += fileInfo->GetRemainingSize();
				}
			}
		}
	}

	*remaining = remainingSize;

	if (remainingForced)
	{
		*remainingForced = remainingForcedSize;
	}
}
