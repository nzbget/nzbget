/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#include "nzbget.h"
#include "Scanner.h"
#include "Options.h"
#include "Log.h"
#include "QueueCoordinator.h"
#include "HistoryCoordinator.h"
#include "ScanScript.h"
#include "Util.h"

Scanner::FileData::FileData(const char* filename)
{
	m_filename = strdup(filename);
	m_size = 0;
	m_lastChange = 0;
}

Scanner::FileData::~FileData()
{
	free(m_filename);
}


Scanner::QueueData::QueueData(const char* filename, const char* nzbName, const char* category,
	int priority, const char* dupeKey, int dupeScore, EDupeMode dupeMode,
	NzbParameterList* parameters, bool addTop, bool addPaused, NzbInfo* urlInfo,
	EAddStatus* addStatus, int* nzbId)
{
	m_filename = strdup(filename);
	m_nzbName = strdup(nzbName);
	m_category = strdup(category ? category : "");
	m_priority = priority;
	m_dupeKey = strdup(dupeKey ? dupeKey : "");
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

Scanner::QueueData::~QueueData()
{
	free(m_filename);
	free(m_nzbName);
	free(m_category);
	free(m_dupeKey);
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


Scanner::Scanner()
{
	debug("Creating Scanner");

	m_requestedNzbDirScan = false;
	m_scanning = false;
	m_nzbDirInterval = 0;
	m_pass = 0;
	m_scanScript = false;
}

Scanner::~Scanner()
{
	debug("Destroying Scanner");

	for (FileList::iterator it = m_fileList.begin(); it != m_fileList.end(); it++)
	{
		delete *it;
	}
	m_fileList.clear();

	ClearQueueList();
}

void Scanner::InitOptions()
{
	m_nzbDirInterval = g_Options->GetNzbDirInterval() * 1000;
	const char* scanScript = g_Options->GetScanScript();
	m_scanScript = scanScript && strlen(scanScript) > 0;
}

void Scanner::ClearQueueList()
{
	for (QueueList::iterator it = m_queueList.begin(); it != m_queueList.end(); it++)
	{
		delete *it;
	}
	m_queueList.clear();
}

void Scanner::ServiceWork()
{
	if (!DownloadQueue::IsLoaded())
	{
		return;
	}

	m_scanMutex.Lock();

	if (m_requestedNzbDirScan ||
		(!g_Options->GetPauseScan() && g_Options->GetNzbDirInterval() > 0 &&
		 m_nzbDirInterval >= g_Options->GetNzbDirInterval() * 1000))
	{
		// check nzbdir every g_pOptions->GetNzbDirInterval() seconds or if requested
		bool checkStat = !m_requestedNzbDirScan;
		m_requestedNzbDirScan = false;
		m_scanning = true;
		CheckIncomingNzbs(g_Options->GetNzbDir(), "", checkStat);
		if (!checkStat && m_scanScript)
		{
			// if immediate scan requested, we need second scan to process files extracted by NzbProcess-script
			CheckIncomingNzbs(g_Options->GetNzbDir(), "", checkStat);
		}
		m_scanning = false;
		m_nzbDirInterval = 0;

		// if NzbDirFileAge is less than NzbDirInterval (that can happen if NzbDirInterval
		// is set for rare scans like once per hour) we make 4 scans:
		//   - one additional scan is neccessary to check sizes of detected files;
		//   - another scan is required to check files which were extracted by NzbProcess-script;
		//   - third scan is needed to check sizes of extracted files.
		if (g_Options->GetNzbDirInterval() > 0 && g_Options->GetNzbDirFileAge() < g_Options->GetNzbDirInterval())
		{
			int maxPass = m_scanScript ? 3 : 1;
			if (m_pass < maxPass)
			{
				// scheduling another scan of incoming directory in NzbDirFileAge seconds.
				m_nzbDirInterval = (g_Options->GetNzbDirInterval() - g_Options->GetNzbDirFileAge()) * 1000;
				m_pass++;
			}
			else
			{
				m_pass = 0;
			}
		}

		DropOldFiles();
		ClearQueueList();
	}
	m_nzbDirInterval += 200;

	m_scanMutex.Unlock();
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
		char fullfilename[1023 + 1]; // one char reserved for the trailing slash (if needed)
		snprintf(fullfilename, 1023, "%s%s", directory, filename);
		fullfilename[1023 - 1] = '\0';
		bool isDirectory = Util::DirectoryExists(fullfilename);
		// check subfolders
		if (isDirectory && strcmp(filename, ".") && strcmp(filename, ".."))
		{
			fullfilename[strlen(fullfilename) + 1] = '\0';
			fullfilename[strlen(fullfilename)] = PATH_SEPARATOR;
			const char* useCategory = filename;
			char subCategory[1024];
			if (strlen(category) > 0)
			{
				snprintf(subCategory, 1023, "%s%c%s", category, PATH_SEPARATOR, filename);
				subCategory[1024 - 1] = '\0';
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

	long long size = Util::FileSize(fullFilename);
	time_t current = time(NULL);
	bool canProcess = false;
	bool inList = false;

	for (FileList::iterator it = m_fileList.begin(); it != m_fileList.end(); it++)
	{
		FileData* fileData = *it;
		if (!strcmp(fileData->GetFilename(), fullFilename))
		{
			inList = true;
			if (fileData->GetSize() == size &&
				current - fileData->GetLastChange() >= g_Options->GetNzbDirFileAge())
			{
				canProcess = true;
				delete fileData;
				m_fileList.erase(it);
			}
			else
			{
				fileData->SetSize(size);
				if (fileData->GetSize() != size)
				{
					fileData->SetLastChange(current);
				}
			}
			break;
		}
	}

	if (!inList)
	{
		FileData* fileData = new FileData(fullFilename);
		fileData->SetSize(size);
		fileData->SetLastChange(current);
		m_fileList.push_back(fileData);
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
	time_t current = time(NULL);

	int i = 0;
	for (FileList::iterator it = m_fileList.begin(); it != m_fileList.end(); )
	{
		FileData* fileData = *it;
		if ((current - fileData->GetLastChange() >=
			(g_Options->GetNzbDirInterval() + g_Options->GetNzbDirFileAge()) * 2) ||
			// can occur if the system clock was adjusted
			current < fileData->GetLastChange())
		{
			debug("Removing file %s from scan file list", fileData->GetFilename());

			delete fileData;
			m_fileList.erase(it);
			it = m_fileList.begin() + i;
		}
		else
		{
			it++;
			i++;
		}
	}
}

void Scanner::ProcessIncomingFile(const char* directory, const char* baseFilename,
	const char* fullFilename, const char* category)
{
	const char* extension = strrchr(baseFilename, '.');
	if (!extension)
	{
		return;
	}

	char* nzbName = strdup("");
	char* nzbCategory = strdup(category);
	NzbParameterList* parameters = new NzbParameterList();
	int priority = 0;
	bool addTop = false;
	bool addPaused = false;
	char* dupeKey = strdup("");
	int dupeScore = 0;
	EDupeMode dupeMode = dmScore;
	EAddStatus addStatus = asSkipped;
	QueueData* queueData = NULL;
	NzbInfo* urlInfo = NULL;
	int nzbId = 0;

	for (QueueList::iterator it = m_queueList.begin(); it != m_queueList.end(); it++)
	{
		QueueData* queueData1 = *it;
		if (Util::SameFilename(queueData1->GetFilename(), fullFilename))
		{
			queueData = queueData1;
			free(nzbName);
			nzbName = strdup(queueData->GetNzbName());
			free(nzbCategory);
			nzbCategory = strdup(queueData->GetCategory());
			priority = queueData->GetPriority();
			free(dupeKey);
			dupeKey = strdup(queueData->GetDupeKey());
			dupeScore = queueData->GetDupeScore();
			dupeMode = queueData->GetDupeMode();
			addTop = queueData->GetAddTop();
			addPaused = queueData->GetAddPaused();
			parameters->CopyFrom(queueData->GetParameters());
			urlInfo = queueData->GetUrlInfo();
		}
	}

	InitPPParameters(nzbCategory, parameters, false);

	bool exists = true;

	if (m_scanScript && strcasecmp(extension, ".nzb_processed"))
	{
		ScanScriptController::ExecuteScripts(fullFilename,
			urlInfo ? urlInfo->GetUrl() : "", directory,
			&nzbName, &nzbCategory, &priority, parameters, &addTop,
			&addPaused, &dupeKey, &dupeScore, &dupeMode);
		exists = Util::FileExists(fullFilename);
		if (exists && strcasecmp(extension, ".nzb"))
		{
			char bakname2[1024];
			bool renameOK = Util::RenameBak(fullFilename, "processed", false, bakname2, 1024);
			if (!renameOK)
			{
				char sysErrStr[256];
				error("Could not rename file %s to %s: %s", fullFilename, bakname2, Util::GetLastErrorMessage(sysErrStr, sizeof(sysErrStr)));
			}
		}
	}

	if (!strcasecmp(extension, ".nzb_processed"))
	{
		char renamedName[1024];
		bool renameOK = Util::RenameBak(fullFilename, "nzb", true, renamedName, 1024);
		if (renameOK)
		{
			bool added = AddFileToQueue(renamedName, nzbName, nzbCategory, priority,
				dupeKey, dupeScore, dupeMode, parameters, addTop, addPaused, urlInfo, &nzbId);
			addStatus = added ? asSuccess : asFailed;
		}
		else
		{
			char sysErrStr[256];
			error("Could not rename file %s to %s: %s", fullFilename, renamedName, Util::GetLastErrorMessage(sysErrStr, sizeof(sysErrStr)));
			addStatus = asFailed;
		}
	}
	else if (exists && !strcasecmp(extension, ".nzb"))
	{
		bool added = AddFileToQueue(fullFilename, nzbName, nzbCategory, priority,
			dupeKey, dupeScore, dupeMode, parameters, addTop, addPaused, urlInfo, &nzbId);
		addStatus = added ? asSuccess : asFailed;
	}

	delete parameters;

	free(nzbName);
	free(nzbCategory);
	free(dupeKey);

	if (queueData)
	{
		queueData->SetAddStatus(addStatus);
		queueData->SetNzbId(nzbId);
	}
}

void Scanner::InitPPParameters(const char* category, NzbParameterList* parameters, bool reset)
{
	bool unpack = g_Options->GetUnpack();
	const char* postScript = g_Options->GetPostScript();

	if (!Util::EmptyStr(category))
	{
		Options::Category* categoryObj = g_Options->FindCategory(category, false);
		if (categoryObj)
		{
			unpack = categoryObj->GetUnpack();
			if (!Util::EmptyStr(categoryObj->GetPostScript()))
			{
				postScript = categoryObj->GetPostScript();
			}
		}
	}

	if (reset)
	{
		for (ScriptConfig::Scripts::iterator it = g_ScriptConfig->GetScripts()->begin(); it != g_ScriptConfig->GetScripts()->end(); it++)
		{
			ScriptConfig::Script* script = *it;
			char param[1024];
			snprintf(param, 1024, "%s:", script->GetName());
			param[1024-1] = '\0';
			parameters->SetParameter(param, NULL);
		}
	}

	parameters->SetParameter("*Unpack:", unpack ? "yes" : "no");

	if (!Util::EmptyStr(postScript))
	{
		// split szPostScript into tokens and create pp-parameter for each token
		Tokenizer tok(postScript, ",;");
		while (const char* scriptName = tok.Next())
		{
			char param[1024];
			snprintf(param, 1024, "%s:", scriptName);
			param[1024-1] = '\0';
			parameters->SetParameter(param, "yes");
		}
	}
}

bool Scanner::AddFileToQueue(const char* filename, const char* nzbName, const char* category,
	int priority, const char* dupeKey, int dupeScore, EDupeMode dupeMode,
	NzbParameterList* parameters, bool addTop, bool addPaused, NzbInfo* urlInfo, int* nzbId)
{
	const char* basename = Util::BaseFileName(filename);

	info("Adding collection %s to queue", basename);

	NzbFile* nzbFile = new NzbFile(filename, category);
	bool ok = nzbFile->Parse();
	if (!ok)
	{
		error("Could not add collection %s to queue", basename);
	}

	char bakname2[1024];
	if (!Util::RenameBak(filename, nzbFile ? "queued" : "error", false, bakname2, 1024))
	{
		ok = false;
		char sysErrStr[256];
		error("Could not rename file %s to %s: %s", filename, bakname2, Util::GetLastErrorMessage(sysErrStr, sizeof(sysErrStr)));
	}

	NzbInfo* nzbInfo = nzbFile->GetNzbInfo();
	nzbInfo->SetQueuedFilename(bakname2);

	if (nzbName && strlen(nzbName) > 0)
	{
		nzbInfo->SetName(NULL);
#ifdef WIN32
		char* ansiFilename = strdup(nzbName);
		WebUtil::Utf8ToAnsi(ansiFilename, strlen(ansiFilename) + 1);
		nzbInfo->SetFilename(ansiFilename);
		free(ansiFilename);
#else
		nzbInfo->SetFilename(nzbName);
#endif
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
	}

	if (nzbFile->GetPassword())
	{
		nzbInfo->GetParameters()->SetParameter("*Unpack:Password", nzbFile->GetPassword());
	}

	nzbInfo->GetParameters()->CopyFrom(parameters);

	for (::FileList::iterator it = nzbInfo->GetFileList()->begin(); it != nzbInfo->GetFileList()->end(); it++)
	{
		FileInfo* fileInfo = *it;
		fileInfo->SetPaused(addPaused);
	}

	if (ok)
	{
		g_QueueCoordinator->AddNzbFileToQueue(nzbFile, urlInfo, addTop);
	}
	else if (!urlInfo)
	{
		nzbFile->GetNzbInfo()->SetDeleteStatus(NzbInfo::dsScan);
		g_QueueCoordinator->AddNzbFileToQueue(nzbFile, urlInfo, addTop);
	}

	if (nzbId)
	{
		*nzbId = nzbInfo->GetId();
	}

	delete nzbFile;

	return ok;
}

void Scanner::ScanNzbDir(bool syncMode)
{
	m_scanMutex.Lock();
	m_scanning = true;
	m_requestedNzbDirScan = true;
	m_scanMutex.Unlock();

	while (syncMode && (m_scanning || m_requestedNzbDirScan))
	{
		usleep(100 * 1000);
	}
}

Scanner::EAddStatus Scanner::AddExternalFile(const char* nzbName, const char* category,
	int priority, const char* dupeKey, int dupeScore,  EDupeMode dupeMode,
	NzbParameterList* parameters, bool addTop, bool addPaused, NzbInfo* urlInfo,
	const char* fileName, const char* buffer, int bufSize, int* nzbId)
{
	bool nzb = false;
	char tempFileName[1024];

	if (fileName)
	{
		strncpy(tempFileName, fileName, 1024);
		tempFileName[1024-1] = '\0';
	}
	else
	{
		int num = 1;
		while (num == 1 || Util::FileExists(tempFileName))
		{
			snprintf(tempFileName, 1024, "%snzb-%i.tmp", g_Options->GetTempDir(), num);
			tempFileName[1024-1] = '\0';
			num++;
		}

		if (!Util::SaveBufferIntoFile(tempFileName, buffer, bufSize))
		{
			error("Could not create file %s", tempFileName);
			return asFailed;
		}

		char buf[1024];
		strncpy(buf, buffer, 1024);
		buf[1024-1] = '\0';
		nzb = !strncmp(buf, "<?xml", 5) && strstr(buf, "<nzb");
	}

	// move file into NzbDir, make sure the file name is unique
	char validNzbName[1024];
	strncpy(validNzbName, Util::BaseFileName(nzbName), 1024);
	validNzbName[1024-1] = '\0';
	Util::MakeValidFilename(validNzbName, '_', false);

#ifdef WIN32
	WebUtil::Utf8ToAnsi(validNzbName, 1024);
#endif

	const char* extension = strrchr(nzbName, '.');
	if (nzb && (!extension || strcasecmp(extension, ".nzb")))
	{
		strncat(validNzbName, ".nzb", 1024 - strlen(validNzbName) - 1);
	}

	char scanFileName[1024];
	snprintf(scanFileName, 1024, "%s%s", g_Options->GetNzbDir(), validNzbName);

	char *ext = strrchr(validNzbName, '.');
	if (ext)
	{
		*ext = '\0';
		ext++;
	}

	int num = 2;
	while (Util::FileExists(scanFileName))
	{
		if (ext)
		{
			snprintf(scanFileName, 1024, "%s%s_%i.%s", g_Options->GetNzbDir(), validNzbName, num, ext);
		}
		else
		{
			snprintf(scanFileName, 1024, "%s%s_%i", g_Options->GetNzbDir(), validNzbName, num);
		}
		scanFileName[1024-1] = '\0';
		num++;
	}

	m_scanMutex.Lock();

	if (!Util::MoveFile(tempFileName, scanFileName))
	{
		char sysErrStr[256];
		error("Could not move file %s to %s: %s", tempFileName, scanFileName, Util::GetLastErrorMessage(sysErrStr, sizeof(sysErrStr)));
		remove(tempFileName);
		m_scanMutex.Unlock(); // UNLOCK
		return asFailed;
	}

	char* useCategory = strdup(category ? category : "");
	Options::Category* categoryObj = g_Options->FindCategory(useCategory, true);
	if (categoryObj && strcmp(useCategory, categoryObj->GetName()))
	{
		free(useCategory);
		useCategory = strdup(categoryObj->GetName());
		detail("Category %s matched to %s for %s", category, useCategory, nzbName);
	}

	EAddStatus addStatus = asSkipped;
	QueueData* queueData = new QueueData(scanFileName, nzbName, useCategory, priority,
		dupeKey, dupeScore, dupeMode, parameters, addTop, addPaused, urlInfo,
		&addStatus, nzbId);
	free(useCategory);
	m_queueList.push_back(queueData);

	m_scanMutex.Unlock();

	ScanNzbDir(true);

	return addStatus;
}
