/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2007-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "NString.h"
#include "QueueScript.h"
#include "NzbScript.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"
#include "FileSystem.h"

static const char* QUEUE_EVENT_NAMES[] = {
	"FILE_DOWNLOADED",
	"URL_COMPLETED",
	"NZB_MARKED",
	"NZB_ADDED",
	"NZB_NAMED",
	"NZB_DOWNLOADED",
	"NZB_DELETED" };

class QueueScriptController : public Thread, public NzbScriptController
{
public:
	virtual void Run();
	static void StartScript(NzbInfo* nzbInfo, ScriptConfig::Script* script, QueueScriptCoordinator::EEvent event);

protected:
	virtual void ExecuteScript(ScriptConfig::Script* script);
	virtual void AddMessage(Message::EKind kind, const char* text);

private:
	CString m_nzbName;
	CString m_nzbFilename;
	CString m_url;
	CString m_category;
	CString m_destDir;
	CString m_queuedFilename;
	int m_id;
	int m_priority;
	CString m_dupeKey;
	EDupeMode m_dupeMode;
	int m_dupeScore;
	NzbParameterList m_parameters;
	int m_prefixLen;
	ScriptConfig::Script* m_script;
	QueueScriptCoordinator::EEvent m_event;
	bool m_markBad;
	NzbInfo::EDeleteStatus m_deleteStatus;
	NzbInfo::EUrlStatus m_urlStatus;
	NzbInfo::EMarkStatus m_markStatus;

	void PrepareParams(const char* scriptName);
};


void QueueScriptController::StartScript(NzbInfo* nzbInfo, ScriptConfig::Script* script, QueueScriptCoordinator::EEvent event)
{
	QueueScriptController* scriptController = new QueueScriptController();

	scriptController->m_nzbName = nzbInfo->GetName();
	scriptController->m_nzbFilename = nzbInfo->GetFilename();
	scriptController->m_url = nzbInfo->GetUrl();
	scriptController->m_category = nzbInfo->GetCategory();
	scriptController->m_destDir = nzbInfo->GetDestDir();
	scriptController->m_queuedFilename = nzbInfo->GetQueuedFilename();
	scriptController->m_id = nzbInfo->GetId();
	scriptController->m_priority = nzbInfo->GetPriority();
	scriptController->m_dupeKey = nzbInfo->GetDupeKey();
	scriptController->m_dupeMode = nzbInfo->GetDupeMode();
	scriptController->m_dupeScore = nzbInfo->GetDupeScore();
	scriptController->m_parameters.CopyFrom(nzbInfo->GetParameters());
	scriptController->m_script = script;
	scriptController->m_event = event;
	scriptController->m_prefixLen = 0;
	scriptController->m_markBad = false;
	scriptController->m_deleteStatus = nzbInfo->GetDeleteStatus();
	scriptController->m_urlStatus = nzbInfo->GetUrlStatus();
	scriptController->m_markStatus = nzbInfo->GetMarkStatus();
	scriptController->SetAutoDestroy(true);

	scriptController->Start();
}

void QueueScriptController::Run()
{
	ExecuteScript(m_script);

	SetLogPrefix(nullptr);

	if (m_markBad)
	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		NzbInfo* nzbInfo = downloadQueue->GetQueue()->Find(m_id);
		if (nzbInfo)
		{
			nzbInfo->PrintMessage(Message::mkWarning, "Cancelling download and deleting %s", *m_nzbName);
			nzbInfo->SetDeleteStatus(NzbInfo::dsBad);
			downloadQueue->EditEntry(m_id, DownloadQueue::eaGroupDelete, nullptr);
		}
	}

	g_QueueScriptCoordinator->CheckQueue();
}

void QueueScriptController::ExecuteScript(ScriptConfig::Script* script)
{
	PrintMessage(m_event == QueueScriptCoordinator::qeFileDownloaded ? Message::mkDetail : Message::mkInfo,
		"Executing queue-script %s for %s", script->GetName(), FileSystem::BaseFileName(m_nzbName));

	SetArgs({script->GetLocation()});

	BString<1024> infoName("queue-script %s for %s", script->GetName(), FileSystem::BaseFileName(m_nzbName));
	SetInfoName(infoName);

	SetLogPrefix(script->GetDisplayName());
	m_prefixLen = strlen(script->GetDisplayName()) + 2; // 2 = strlen(": ");
	PrepareParams(script->GetName());

	Execute();

	SetLogPrefix(nullptr);
}

void QueueScriptController::PrepareParams(const char* scriptName)
{
	ResetEnv();

	SetEnvVar("NZBNA_NZBNAME", m_nzbName);
	SetIntEnvVar("NZBNA_NZBID", m_id);
	SetEnvVar("NZBNA_FILENAME", m_nzbFilename);
	SetEnvVar("NZBNA_DIRECTORY", m_destDir);
	SetEnvVar("NZBNA_QUEUEDFILE", m_queuedFilename);
	SetEnvVar("NZBNA_URL", m_url);
	SetEnvVar("NZBNA_CATEGORY", m_category);
	SetIntEnvVar("NZBNA_PRIORITY", m_priority);
	SetIntEnvVar("NZBNA_LASTID", m_id);	// deprecated

	SetEnvVar("NZBNA_DUPEKEY", m_dupeKey);
	SetIntEnvVar("NZBNA_DUPESCORE", m_dupeScore);

	const char* dupeModeName[] = { "SCORE", "ALL", "FORCE" };
	SetEnvVar("NZBNA_DUPEMODE", dupeModeName[m_dupeMode]);

	SetEnvVar("NZBNA_EVENT", QUEUE_EVENT_NAMES[m_event]);

	const char* deleteStatusName[] = { "NONE", "MANUAL", "HEALTH", "DUPE", "BAD", "GOOD", "COPY", "SCAN" };
	SetEnvVar("NZBNA_DELETESTATUS", deleteStatusName[m_deleteStatus]);

	const char* urlStatusName[] = { "NONE", "UNKNOWN", "SUCCESS", "FAILURE", "UNKNOWN", "SCAN_SKIPPED", "SCAN_FAILURE" };
	SetEnvVar("NZBNA_URLSTATUS", urlStatusName[m_urlStatus]);

	const char* markStatusName[] = { "NONE", "BAD", "GOOD", "SUCCESS" };
	SetEnvVar("NZBNA_MARKSTATUS", markStatusName[m_markStatus]);

	PrepareEnvScript(&m_parameters, scriptName);
}

void QueueScriptController::AddMessage(Message::EKind kind, const char* text)
{
	const char* msgText = text + m_prefixLen;

	if (!strncmp(msgText, "[NZB] ", 6))
	{
		debug("Command %s detected", msgText + 6);
		if (!strncmp(msgText + 6, "NZBPR_", 6))
		{
			CString param = msgText + 6 + 6;
			char* value = strchr(param, '=');
			if (value)
			{
				*value = '\0';
				GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
				NzbInfo* nzbInfo = QueueScriptCoordinator::FindNzbInfo(downloadQueue, m_id);
				if (nzbInfo)
				{
					nzbInfo->GetParameters()->SetParameter(param, value + 1);
				}
			}
			else
			{
				error("Invalid command \"%s\" received from %s", msgText, GetInfoName());
			}
		}
		else if (!strncmp(msgText + 6, "DIRECTORY=", 10) &&
			m_event == QueueScriptCoordinator::qeNzbDownloaded)
		{
			GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
			NzbInfo* nzbInfo = QueueScriptCoordinator::FindNzbInfo(downloadQueue, m_id);
			if (nzbInfo)
			{
				nzbInfo->SetFinalDir(msgText + 6 + 10);
			}
		}
		else if (!strncmp(msgText + 6, "MARK=BAD", 8))
		{
			m_markBad = true;
			GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
			NzbInfo* nzbInfo = QueueScriptCoordinator::FindNzbInfo(downloadQueue, m_id);
			if (nzbInfo)
			{
				nzbInfo->PrintMessage(Message::mkWarning, "Marking %s as bad", *m_nzbName);
				nzbInfo->SetMarkStatus(NzbInfo::ksBad);
			}
		}
		else
		{
			error("Invalid command \"%s\" received from %s", msgText, GetInfoName());
		}
	}
	else
	{
		NzbInfo* nzbInfo = nullptr;
		{
			GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
			nzbInfo = QueueScriptCoordinator::FindNzbInfo(downloadQueue, m_id);
			if (nzbInfo)
			{
				nzbInfo->AddMessage(kind, text);
			}
		}

		if (!nzbInfo)
		{
			ScriptController::AddMessage(kind, text);
		}
	}
}


void QueueScriptCoordinator::InitOptions()
{
	m_hasQueueScripts = false;
	for (ScriptConfig::Script& script : g_ScriptConfig->GetScripts())
	{
		if (script.GetQueueScript())
		{
			m_hasQueueScripts = true;
			break;
		}
	}
}

void QueueScriptCoordinator::EnqueueScript(NzbInfo* nzbInfo, EEvent event)
{
	if (!m_hasQueueScripts)
	{
		return;
	}

	Guard guard(m_queueMutex);

	if (event == qeNzbDownloaded)
	{
		// delete all other queued scripts for this nzb
		m_queue.erase(std::remove_if(m_queue.begin(), m_queue.end(),
			[nzbInfo](std::unique_ptr<QueueItem>& queueItem)
			{
				return queueItem->GetNzbId() == nzbInfo->GetId();
			}),
			m_queue.end());
	}

	// respect option "EventInterval"
	time_t curTime = Util::CurrentTime();
	if (event == qeFileDownloaded &&
		(g_Options->GetEventInterval() == -1 ||
		 (g_Options->GetEventInterval() > 0 && curTime - nzbInfo->GetQueueScriptTime() > 0 &&
		 (int)(curTime - nzbInfo->GetQueueScriptTime()) < g_Options->GetEventInterval())))
	{
		return;
	}

	for (ScriptConfig::Script& script : g_ScriptConfig->GetScripts())
	{
		if (UsableScript(script, nzbInfo, event))
		{
			bool alreadyQueued = false;
			if (event == qeFileDownloaded)
			{
				// check if this script is already queued for this nzb
				for (QueueItem* queueItem : &m_queue)
				{
					if (queueItem->GetNzbId() == nzbInfo->GetId() && queueItem->GetScript() == &script)
					{
						alreadyQueued = true;
						break;
					}
				}
			}

			if (!alreadyQueued)
			{
				std::unique_ptr<QueueItem> queueItem = std::make_unique<QueueItem>(nzbInfo->GetId(), &script, event);
				if (m_curItem)
				{
					m_queue.push_back(std::move(queueItem));
				}
				else
				{
					m_curItem = std::move(queueItem);
					QueueScriptController::StartScript(nzbInfo, m_curItem->GetScript(), m_curItem->GetEvent());
				}
			}

			nzbInfo->SetQueueScriptTime(Util::CurrentTime());
		}
	}
}

bool QueueScriptCoordinator::UsableScript(ScriptConfig::Script& script, NzbInfo* nzbInfo, EEvent event)
{
	if (!script.GetQueueScript())
	{
		return false;
	}

	if (!Util::EmptyStr(script.GetQueueEvents()) && !strstr(script.GetQueueEvents(), QUEUE_EVENT_NAMES[event]))
	{
		return false;
	}

	// check extension scripts assigned for that nzb
	for (NzbParameter& parameter : nzbInfo->GetParameters())
	{
		const char* varname = parameter.GetName();
		if (strlen(varname) > 0 && varname[0] != '*' && varname[strlen(varname)-1] == ':' &&
			(!strcasecmp(parameter.GetValue(), "yes") ||
			 !strcasecmp(parameter.GetValue(), "on") ||
			 !strcasecmp(parameter.GetValue(), "1")))
		{
			BString<1024> scriptName = varname;
			scriptName[strlen(scriptName)-1] = '\0'; // remove trailing ':'
			if (FileSystem::SameFilename(scriptName, script.GetName()))
			{
				return true;
			}
		}
	}

	// for URL-events the extension scripts are not assigned yet;
	// instead we take the default extension scripts for the category (or global)
	if (event == qeUrlCompleted)
	{
		const char* postScript = g_Options->GetExtensions();
		if (!Util::EmptyStr(nzbInfo->GetCategory()))
		{
			Options::Category* categoryObj = g_Options->FindCategory(nzbInfo->GetCategory(), false);
			if (categoryObj && !Util::EmptyStr(categoryObj->GetExtensions()))
			{
				postScript = categoryObj->GetExtensions();
			}
		}

		if (!Util::EmptyStr(postScript))
		{
			Tokenizer tok(postScript, ",;");
			while (const char* scriptName = tok.Next())
			{
				if (FileSystem::SameFilename(scriptName, script.GetName()))
				{
					return true;
				}
			}
		}
	}

	return false;
}

NzbInfo* QueueScriptCoordinator::FindNzbInfo(DownloadQueue* downloadQueue, int nzbId)
{
	NzbInfo* nzbInfo = downloadQueue->GetQueue()->Find(nzbId);
	if (nzbInfo)
	{
		return nzbInfo;
	}

	HistoryInfo* historyInfo = downloadQueue->GetHistory()->Find(nzbId);
	if (historyInfo)
	{
		return historyInfo->GetNzbInfo();
	}

	return nullptr;
}

void QueueScriptCoordinator::CheckQueue()
{
	if (m_stopped)
	{
		return;
	}

	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
	Guard guard(m_queueMutex);

	m_curItem.reset();
	NzbInfo* curNzbInfo = nullptr;
	Queue::iterator itCurItem;

	for (Queue::iterator it = m_queue.begin(); it != m_queue.end(); )
	{
		std::unique_ptr<QueueItem>& queueItem = *it;

		NzbInfo* nzbInfo = FindNzbInfo(downloadQueue, queueItem->GetNzbId());

		// in a case this nzb must not be processed further - delete queue script from queue
		EEvent event = queueItem->GetEvent();
		bool ignoreEvent = !nzbInfo ||
			(nzbInfo->GetDeleteStatus() != NzbInfo::dsNone && event != qeNzbDeleted && event != qeNzbMarked) ||
			(nzbInfo->GetMarkStatus() == NzbInfo::ksBad && event != qeNzbMarked);

		if (ignoreEvent)
		{
			it = m_queue.erase(it);
			if (curNzbInfo)
			{
				// process from the beginning, while "erase" invalidated "itCurItem"
				curNzbInfo = nullptr;
				it = m_queue.begin();
			}
			continue;
		}

		if (!m_curItem || queueItem->GetEvent() > m_curItem->GetEvent())
		{
			itCurItem = it;
			curNzbInfo = nzbInfo;
		}

		it++;
	}

	if (curNzbInfo)
	{
		m_curItem = std::move(*itCurItem);
		m_queue.erase(itCurItem);
		QueueScriptController::StartScript(curNzbInfo, m_curItem->GetScript(), m_curItem->GetEvent());
	}
}

bool QueueScriptCoordinator::HasJob(int nzbId, bool* active)
{
	Guard guard(m_queueMutex);

	bool working = m_curItem && m_curItem->GetNzbId() == nzbId;
	if (active)
	{
		*active = working;
	}
	if (!working)
	{
		for (QueueItem* queueItem : &m_queue)
		{
			working = queueItem->GetNzbId() == nzbId;
			if (working)
			{
				break;
			}
		}
	}

	return working;
}

int QueueScriptCoordinator::GetQueueSize()
{
	Guard guard(m_queueMutex);

	int queuedCount = m_queue.size();
	if (m_curItem)
	{
		queuedCount++;
	}

	return queuedCount;
}
