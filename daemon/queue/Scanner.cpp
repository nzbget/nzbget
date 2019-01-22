/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2007-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "Scanner.h"
#include "Options.h"
#include "WorkState.h"
#include "Log.h"
#include "QueueCoordinator.h"
#include "HistoryCoordinator.h"
#include "ScanScript.h"
#include "Util.h"
#include "FileSystem.h"

int Scanner::m_idGen = 0;

Scanner::QueueData::QueueData(const char* filename, const char* nzbName, const char* category,
	int priority, const char* dupeKey, int dupeScore, EDupeMode dupeMode,
	NzbParameterList* parameters, bool addTop, bool addPaused, NzbInfo* urlInfo,
	EAddStatus* addStatus, int* nzbId)
{
	m_filename = filename;
	m_nzbName = nzbName;
	m_category = category ? category : "";
	m_priority = priority;
	m_dupeKey = dupeKey ? dupeKey : "";
	m_dupeScore = dupeScore;
	m_dupeMode = dupeMode;
	m_addTop = addTop;
	m_addPaused = addPaused;
	m_urlInfo = urlInfo;
	m_addStatus = addStatus;
	m_nzbId = nzbId;

	if (parameters)
	{
		m_parameters.CopyFrom(parameters);
	}
}

void Scanner::QueueData::SetAddStatus(EAddStatus addStatus)
{
	if (m_addStatus)
	{
		*m_addStatus = addStatus;
	}
}

void Scanner::QueueData::SetNzbId(int nzbId)
{
	if (m_nzbId)
	{
		*m_nzbId = nzbId;
	}
}


void Scanner::InitOptions()
{
	m_nzbDirInterval = 1;
	m_scanScript = ScanScriptController::HasScripts();
}

int Scanner::ServiceInterval()
{
	return m_requestedNzbDirScan ? Service::Now :
		g_Options->GetNzbDirInterval() <= 0 ? Service::Sleep :
		// g_Options->GetPauseScan() ? Service::Sleep :   // for that to work we need to react on changing of pause-state
		m_nzbDirInterval;
}

void Scanner::ServiceWork()
{
	debug("Scanner service work");

	if (!DownloadQueue::IsLoaded())
	{
		return;
	}

	m_nzbDirInterval = g_Options->GetNzbDirInterval();

	if (g_WorkState->GetPauseScan() && !m_requestedNzbDirScan)
	{
		return;
	}

	debug("Scanner service work: doing work");

	Guard guard(m_scanMutex);

	// check nzbdir every g_pOptions->GetNzbDirInterval() seconds or if requested
	bool checkStat = !m_requestedNzbDirScan;
	m_requestedNzbDirScan = false;
	m_scanning = true;
	CheckIncomingNzbs(g_Options->GetNzbDir(), "", checkStat);
	if (!checkStat && m_scanScript)
	{
		// if immediate scan requested, we need second scan to process files extracted by scan-scripts
		CheckIncomingNzbs(g_Options->GetNzbDir(), "", checkStat);
	}
	m_scanning = false;

	// if NzbDirFileAge is less than NzbDirInterval (that can happen if NzbDirInterval
	// is set for rare scans like once per hour) we make 4 scans:
	//   - one additional scan is neccessary to check sizes of detected files;
	//   - another scan is required to check files which were extracted by scan-scripts;
	//   - third scan is needed to check sizes of extracted files.
	if (g_Options->GetNzbDirInterval() > 0 && g_Options->GetNzbDirFileAge() < g_Options->GetNzbDirInterval())
	{
		int maxPass = m_scanScript ? 3 : 1;
		if (m_pass < maxPass)
		{
			// scheduling another scan of incoming directory in NzbDirFileAge seconds.
			m_nzbDirInterval = g_Options->GetNzbDirFileAge();
			m_pass++;
		}
		else
		{
			m_pass = 0;
		}
	}

	DropOldFiles();
	m_queueList.clear();
}

/**
* Check if there are files in directory for incoming nzb-files
* and add them to download queue
*/
void Scanner::CheckIncomingNzbs(const char* directory, const char* category, bool checkStat)
{
	DirBrowser dir(directory);
	while (const char* filename = dir.Next())
	{
		if (filename[0] == '.')
		{
			// skip hidden files
			continue;
		}

		BString<1024> fullfilename("%s%c%s", directory, PATH_SEPARATOR, filename);
		bool isDirectory = FileSystem::DirectoryExists(fullfilename);
		// check subfolders
		if (isDirectory)
		{
			const char* useCategory = filename;
			BString<1024> subCategory;
			if (strlen(category) > 0)
			{
				subCategory.Format("%s%c%s", category, PATH_SEPARATOR, filename);
				useCategory = subCategory;
			}
			CheckIncomingNzbs(fullfilename, useCategory, checkStat);
		}
		else if (!isDirectory && CanProcessFile(fullfilename, checkStat))
		{
			ProcessIncomingFile(directory, filename, fullfilename, category);
		}
	}
}

/**
 * Only files which were not changed during last g_pOptions->GetNzbDirFileAge() seconds
 * can be processed. That prevents the processing of files, which are currently being
 * copied into nzb-directory (eg. being downloaded in web-browser).
 */
bool Scanner::CanProcessFile(const char* fullFilename, bool checkStat)
{
	const char* extension = strrchr(fullFilename, '.');
	if (!extension ||
		!strcasecmp(extension, ".queued") ||
		!strcasecmp(extension, ".error") ||
		!strcasecmp(extension, ".processed"))
	{
		return false;
	}

	if (!checkStat)
	{
		return true;
	}

	int64 size = FileSystem::FileSize(fullFilename);
	time_t current = Util::CurrentTime();
	bool canProcess = false;
	bool inList = false;

	for (FileList::iterator it = m_fileList.begin(); it != m_fileList.end(); it++)
	{
		FileData& fileData = *it;
		if (!strcmp(fileData.GetFilename(), fullFilename))
		{
			inList = true;
			if (fileData.GetSize() == size &&
				current - fileData.GetLastChange() >= g_Options->GetNzbDirFileAge())
			{
				canProcess = true;
				m_fileList.erase(it);
			}
			else
			{
				fileData.SetSize(size);
				if (fileData.GetSize() != size)
				{
					fileData.SetLastChange(current);
				}
			}
			break;
		}
	}

	if (!inList)
	{
		m_fileList.emplace_back(fullFilename, size, current);
	}

	return canProcess;
}

/**
 * Remove old files from the list of monitored files.
 * Normally these files are deleted from the list when they are processed.
 * However if a file was detected by function "CanProcessFile" once but wasn't
 * processed later (for example if the user deleted it), it will stay in the list,
 * until we remove it here.
 */
void Scanner::DropOldFiles()
{
	time_t current = Util::CurrentTime();

	m_fileList.erase(std::remove_if(m_fileList.begin(), m_fileList.end(),
		[current](FileData& fileData)
		{
			if ((current - fileData.GetLastChange() >=
				(g_Options->GetNzbDirInterval() + g_Options->GetNzbDirFileAge()) * 2) ||
				// can occur if the system clock was adjusted
				current < fileData.GetLastChange())
			{
				debug("Removing file %s from scan file list", fileData.GetFilename());
				return true;
			}
			return false;
		}),
		m_fileList.end());
}

void Scanner::ProcessIncomingFile(const char* directory, const char* baseFilename,
	const char* fullFilename, const char* category)
{
	const char* extension = strrchr(baseFilename, '.');
	if (!extension)
	{
		return;
	}

	CString nzbName = "";
	CString nzbCategory = category;
	NzbParameterList parameters;
	int priority = 0;
	bool addTop = false;
	bool addPaused = false;
	CString dupeKey = "";
	int dupeScore = 0;
	EDupeMode dupeMode = dmScore;
	EAddStatus addStatus = asSkipped;
	QueueData* queueData = nullptr;
	NzbInfo* urlInfo = nullptr;
	int nzbId = 0;

	for (QueueData& queueData1 : m_queueList)
	{
		if (FileSystem::SameFilename(queueData1.GetFilename(), fullFilename))
		{
			queueData = &queueData1;
			nzbName = queueData->GetNzbName();
			nzbCategory = queueData->GetCategory();
			priority = queueData->GetPriority();
			dupeKey = queueData->GetDupeKey();
			dupeScore = queueData->GetDupeScore();
			dupeMode = queueData->GetDupeMode();
			addTop = queueData->GetAddTop();
			addPaused = queueData->GetAddPaused();
			parameters.CopyFrom(queueData->GetParameters());
			urlInfo = queueData->GetUrlInfo();
		}
	}

	InitPPParameters(nzbCategory, &parameters, false);

	bool exists = true;

	if (m_scanScript && strcasecmp(extension, ".nzb_processed"))
	{
		ScanScriptController::ExecuteScripts(fullFilename,
			urlInfo ? urlInfo->GetUrl() : "", directory,
			&nzbName, &nzbCategory, &priority, &parameters, &addTop,
			&addPaused, &dupeKey, &dupeScore, &dupeMode);
		exists = FileSystem::FileExists(fullFilename);
		if (exists && strcasecmp(extension, ".nzb"))
		{
			CString bakname2;
			bool renameOK = FileSystem::RenameBak(fullFilename, "processed", false, bakname2);
			if (!renameOK)
			{
				error("Could not rename file %s to %s: %s", fullFilename, *bakname2,
					*FileSystem::GetLastErrorMessage());
			}
		}
	}

	if (!strcasecmp(extension, ".nzb_processed"))
	{
		CString renamedName;
		bool renameOK = FileSystem::RenameBak(fullFilename, "nzb", true, renamedName);
		if (renameOK)
		{
			bool added = AddFileToQueue(renamedName, nzbName, nzbCategory, priority,
				dupeKey, dupeScore, dupeMode, &parameters, addTop, addPaused, urlInfo, &nzbId);
			addStatus = added ? asSuccess : asFailed;
		}
		else
		{
			error("Could not rename file %s to %s: %s", fullFilename, *renamedName,
				*FileSystem::GetLastErrorMessage());
			addStatus = asFailed;
		}
	}
	else if (exists && !strcasecmp(extension, ".nzb"))
	{
		bool added = AddFileToQueue(fullFilename, nzbName, nzbCategory, priority,
			dupeKey, dupeScore, dupeMode, &parameters, addTop, addPaused, urlInfo, &nzbId);
		addStatus = added ? asSuccess : asFailed;
	}

	if (queueData)
	{
		queueData->SetAddStatus(addStatus);
		queueData->SetNzbId(nzbId);
	}
}

void Scanner::InitPPParameters(const char* category, NzbParameterList* parameters, bool reset)
{
	bool unpack = g_Options->GetUnpack();
	const char* extensions = g_Options->GetExtensions();

	if (!Util::EmptyStr(category))
	{
		Options::Category* categoryObj = g_Options->FindCategory(category, false);
		if (categoryObj)
		{
			unpack = categoryObj->GetUnpack();
			if (!Util::EmptyStr(categoryObj->GetExtensions()))
			{
				extensions = categoryObj->GetExtensions();
			}
		}
	}

	if (reset)
	{
		for (ScriptConfig::Script& script : g_ScriptConfig->GetScripts())
		{
			parameters->SetParameter(BString<1024>("%s:", script.GetName()), nullptr);
		}
	}

	if (!parameters->Find("*Unpack:"))
	{
		parameters->SetParameter("*Unpack:", unpack ? "yes" : "no");
	}

	if (!Util::EmptyStr(extensions))
	{
		// create pp-parameter for each post-processing or queue- script
		Tokenizer tok(extensions, ",;");
		while (const char* scriptName = tok.Next())
		{
			for (ScriptConfig::Script& script : g_ScriptConfig->GetScripts())
			{
				BString<1024> paramName("%s:", scriptName);
				if ((script.GetPostScript() || script.GetQueueScript()) &&
					!parameters->Find(paramName) &&
					FileSystem::SameFilename(scriptName, script.GetName()))
				{
					parameters->SetParameter(paramName, "yes");
				}
			}
		}
	}
}

bool Scanner::AddFileToQueue(const char* filename, const char* nzbName, const char* category,
	int priority, const char* dupeKey, int dupeScore, EDupeMode dupeMode,
	NzbParameterList* parameters, bool addTop, bool addPaused, NzbInfo* urlInfo, int* nzbId)
{
	const char* basename = FileSystem::BaseFileName(filename);

	info("Adding collection %s to queue", basename);

	NzbFile nzbFile(filename, category);
	bool ok = nzbFile.Parse();
	if (!ok)
	{
		error("Could not add collection %s to queue", basename);
	}

	CString bakname2;
	if (!FileSystem::RenameBak(filename, ok ? "queued" : "error", false, bakname2))
	{
		ok = false;
		error("Could not rename file %s to %s: %s", filename, *bakname2,
			*FileSystem::GetLastErrorMessage());
	}

	std::unique_ptr<NzbInfo> nzbInfo = nzbFile.DetachNzbInfo();
	nzbInfo->SetQueuedFilename(bakname2);

	if (nzbName && strlen(nzbName) > 0)
	{
		nzbInfo->SetName(nullptr);
		nzbInfo->SetFilename(nzbName);
		nzbInfo->BuildDestDirName();
	}

	nzbInfo->SetDupeKey(dupeKey);
	nzbInfo->SetDupeScore(dupeScore);
	nzbInfo->SetDupeMode(dupeMode);
	nzbInfo->SetPriority(priority);
	if (urlInfo)
	{
		nzbInfo->SetUrl(urlInfo->GetUrl());
		nzbInfo->SetUrlStatus(urlInfo->GetUrlStatus());
		nzbInfo->SetFeedId(urlInfo->GetFeedId());
		nzbInfo->SetDupeHint(urlInfo->GetDupeHint());
	}

	if (nzbFile.GetPassword())
	{
		nzbInfo->GetParameters()->SetParameter("*Unpack:Password", nzbFile.GetPassword());
	}

	nzbInfo->GetParameters()->CopyFrom(parameters);

	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
		fileInfo->SetPaused(addPaused);
	}

	NzbInfo* addedNzb = nullptr;

	if (ok)
	{
		addedNzb = g_QueueCoordinator->AddNzbFileToQueue(std::move(nzbInfo), std::move(urlInfo), addTop);
	}
	else if (urlInfo)
	{
		for (Message& message : nzbInfo->GuardCachedMessages())
		{
			urlInfo->AddMessage(message.GetKind(), message.GetText(), false);
		}
	}
	else
	{
		nzbInfo->SetDeleteStatus(NzbInfo::dsScan);
		addedNzb = g_QueueCoordinator->AddNzbFileToQueue(std::move(nzbInfo), std::move(urlInfo), addTop);
	}

	if (nzbId)
	{
		*nzbId = addedNzb ? addedNzb->GetId() : 0;
	}

	return ok;
}

void Scanner::ScanNzbDir(bool syncMode)
{
	{
		Guard guard(m_scanMutex);
		m_scanning = true;
		m_requestedNzbDirScan = true;
		WakeUp();
	}

	while (syncMode && (m_scanning || m_requestedNzbDirScan))
	{
		Util::Sleep(100);
	}
}

Scanner::EAddStatus Scanner::AddExternalFile(const char* nzbName, const char* category,
	int priority, const char* dupeKey, int dupeScore, EDupeMode dupeMode,
	NzbParameterList* parameters, bool addTop, bool addPaused, NzbInfo* urlInfo,
	const char* fileName, const char* buffer, int bufSize, int* nzbId)
{
	bool nzb = false;
	BString<1024> tempFileName;

	if (fileName)
	{
		tempFileName = fileName;
	}
	else
	{
		int num = ++m_idGen;
		while (tempFileName.Empty() || FileSystem::FileExists(tempFileName))
		{
			tempFileName.Format("%s%cnzb-%i.tmp", g_Options->GetTempDir(), PATH_SEPARATOR, num);
			num++;
		}

		if (!FileSystem::SaveBufferIntoFile(tempFileName, buffer, bufSize))
		{
			error("Could not create file %s", *tempFileName);
			return asFailed;
		}

		// "buffer" doesn't end with nullptr, therefore we can't search in it with "strstr"
		BString<1024> buf;
		buf.Set(buffer, bufSize);
		nzb = !strncmp(buf, "<?xml", 5) && strstr(buf, "<nzb");
	}

	// move file into NzbDir, make sure the file name is unique
	CString validNzbName = FileSystem::MakeValidFilename(FileSystem::BaseFileName(nzbName));

	const char* extension = strrchr(nzbName, '.');
	if (nzb && (!extension || strcasecmp(extension, ".nzb")))
	{
		validNzbName.Append(".nzb");
	}

	BString<1024> scanFileName("%s%c%s", g_Options->GetNzbDir(), PATH_SEPARATOR, *validNzbName);

	char *ext = strrchr(validNzbName, '.');
	if (ext)
	{
		*ext = '\0';
		ext++;
	}

	int num = 2;
	while (FileSystem::FileExists(scanFileName))
	{
		if (ext)
		{
			scanFileName.Format("%s%c%s_%i.%s", g_Options->GetNzbDir(),
				PATH_SEPARATOR, *validNzbName, num, ext);
		}
		else
		{
			scanFileName.Format("%s%c%s_%i", g_Options->GetNzbDir(),
				PATH_SEPARATOR, *validNzbName, num);
		}
		num++;
	}

	EAddStatus addStatus;

	{
		Guard guard(m_scanMutex);

		if (!FileSystem::MoveFile(tempFileName, scanFileName))
		{
			error("Could not move file %s to %s: %s", *tempFileName, *scanFileName,
				*FileSystem::GetLastErrorMessage());
			FileSystem::DeleteFile(tempFileName);
			return asFailed;
		}

		CString useCategory = category ? category : "";
		Options::Category* categoryObj = g_Options->FindCategory(useCategory, true);
		if (categoryObj && strcmp(useCategory, categoryObj->GetName()))
		{
			useCategory = categoryObj->GetName();
			detail("Category %s matched to %s for %s", category, *useCategory, nzbName);
		}

		addStatus = asSkipped;
		m_queueList.emplace_back(scanFileName, nzbName, useCategory, priority,
			dupeKey, dupeScore, dupeMode, parameters, addTop, addPaused, urlInfo,
			&addStatus, nzbId);
	}

	ScanNzbDir(true);

	return addStatus;
}
