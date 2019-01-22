/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "DirectUnpack.h"
#include "Log.h"
#include "Util.h"
#include "FileSystem.h"
#include "Options.h"

void DirectUnpack::StartJob(NzbInfo* nzbInfo)
{
	DirectUnpack* directUnpack = new DirectUnpack();
	directUnpack->m_nzbId = nzbInfo->GetId();
	directUnpack->SetAutoDestroy(true);

	nzbInfo->SetUnpackThread(directUnpack);
	nzbInfo->SetDirectUnpackStatus(NzbInfo::nsRunning);

	directUnpack->Start();
}

void DirectUnpack::Run()
{
	debug("Entering DirectUnpack-loop for %i", m_nzbId);

	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

		NzbInfo* nzbInfo = downloadQueue->GetQueue()->Find(m_nzbId);
		if (!nzbInfo)
		{
			debug("Could not find NzbInfo for %i", m_nzbId);
			return;
		}

		m_name = nzbInfo->GetName();
		m_destDir = nzbInfo->GetDestDir();
		m_finalDir = nzbInfo->BuildFinalDirName();

		NzbParameter* parameter = nzbInfo->GetParameters()->Find("*Unpack:Password");
		if (parameter)
		{
			m_password = parameter->GetValue();
		}
	}

	m_infoName.Format("direct unpack for %s", *m_name);
	m_infoNameUp.Format("Direct unpack for %s", *m_name); // first letter in upper case

	FindArchiveFiles();

	SetInfoName(m_infoName);
	SetWorkingDir(m_destDir);

	while (!IsStopped())
	{
		CString archive;
		{
			Guard guard(m_volumeMutex);
			if (!m_archives.empty())
			{
				archive = std::move(m_archives.front());
				m_archives.pop_front();
			}
		}

		if (archive)
		{
			if (!m_processed)
			{
				PrintMessage(Message::mkInfo, "Directly unpacking %s", *m_name);
				Cleanup();
				CreateUnpackDir();
				m_processed = true;
			}

			ExecuteUnrar(archive);

			if (!m_unpackOk)
			{
				break;
			}
		}
		else
		{
			if (m_nzbCompleted)
			{
				break;
			}
			Util::Sleep(100);
		}
	}

	if (!m_unpackOk)
	{
		Cleanup();
	}

	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

		NzbInfo* nzbInfo = downloadQueue->GetQueue()->Find(m_nzbId);
		if (!nzbInfo || nzbInfo->GetUnpackThread() != this)
		{
			debug("Could not find NzbInfo for %s", *m_infoName);
			return;
		}

		nzbInfo->SetUnpackThread(nullptr);
		nzbInfo->SetDirectUnpackStatus(m_processed ? m_unpackOk && !IsStopped() ? NzbInfo::nsSuccess : NzbInfo::nsFailure : NzbInfo::nsNone);

		if (nzbInfo->GetDirectUnpackStatus() == NzbInfo::nsSuccess)
		{
			nzbInfo->AddMessage(Message::mkInfo, BString<1024>("%s successful", *m_infoNameUp));
		}
		else if (nzbInfo->GetDirectUnpackStatus() == NzbInfo::nsFailure)
		{
			nzbInfo->AddMessage(Message::mkWarning, BString<1024>("%s failed", *m_infoNameUp));
		}

		if (nzbInfo->GetPostInfo() && nzbInfo->GetDirectUnpackStatus() == NzbInfo::nsSuccess)
		{
			std::move(m_extractedArchives.begin(), m_extractedArchives.end(),
				std::back_inserter(*nzbInfo->GetPostInfo()->GetExtractedArchives()));
		}

		AddExtraTime(nzbInfo);

		if (nzbInfo->GetPostInfo())
		{
			nzbInfo->GetPostInfo()->SetWorking(false);
		}
	}

	debug("Exiting DirectUnpack-loop for %i", m_nzbId);
}

void DirectUnpack::ExecuteUnrar(const char* archiveName)
{
	// Format:
	//   unrar x -y -p- -o+ -vp <archive.part0001.rar> <dest-dir>/_unpack/

	ParamList params;
	if (!PrepareCmdParams(g_Options->GetUnrarCmd(), &params, "unrar"))
	{
		return;
	}

	if (!params.Exists("x") && !params.Exists("e"))
	{
		params.emplace_back("x");
	}

	params.emplace_back("-y");

	if (!m_password.Empty())
	{
		params.push_back(CString::FormatStr("-p%s", *m_password));
	}
	else
	{
		params.emplace_back("-p-");
	}

	if (!params.Exists("-o+") && !params.Exists("-o-"))
	{
		params.emplace_back("-o+");
	}

	params.emplace_back("-vp");

	params.emplace_back(archiveName);
	m_unpackExtendedDir = FileSystem::MakeExtendedPath(m_unpackDir, true);
	params.push_back(*BString<1024>("%s%c", *m_unpackExtendedDir, PATH_SEPARATOR));
	SetArgs(std::move(params));
	SetLogPrefix("Unrar");
	ResetEnv();

	m_allOkMessageReceived = false;

	SetNeedWrite(true);
	m_unpacking = true;
	int exitCode = Execute();
	m_unpacking = false;
	SetLogPrefix(nullptr);

	m_unpackOk = exitCode == 0 && m_allOkMessageReceived && !GetTerminated();

	if (!m_unpackOk && exitCode > 0)
	{
		PrintMessage(Message::mkError, "Unrar error code: %i", exitCode);
	}
}

bool DirectUnpack::PrepareCmdParams(const char* command, ParamList* params, const char* infoName)
{
	if (FileSystem::FileExists(command))
	{
		params->emplace_back(command);
		return true;
	}

	std::vector<CString> cmdArgs = Util::SplitCommandLine(command);
	if (cmdArgs.empty())
	{
		PrintMessage(Message::mkError, "Could not start %s, failed to parse command line: %s", infoName, command);
		m_unpackOk = false;
		return false;
	}

	std::move(cmdArgs.begin(), cmdArgs.end(), std::back_inserter(*params));

	return true;
}

void DirectUnpack::CreateUnpackDir()
{
	bool useInterDir = !Util::EmptyStr(g_Options->GetInterDir()) &&
		!strncmp(m_destDir, g_Options->GetInterDir(), strlen(g_Options->GetInterDir())) &&
		m_destDir[strlen(g_Options->GetInterDir())] == PATH_SEPARATOR;

	m_finalDirCreated = useInterDir && !FileSystem::DirectoryExists(m_finalDir);

	const char* destDir = useInterDir && !m_finalDir.Empty() ? *m_finalDir : *m_destDir;

	m_unpackDir.Format("%s%c%s", destDir, PATH_SEPARATOR, "_unpack");

	detail("Unpacking into %s", *m_unpackDir);

	CString errmsg;
	if (!FileSystem::ForceDirectories(m_unpackDir, errmsg))
	{
		PrintMessage(Message::mkError, "Could not create directory %s: %s", *m_unpackDir, *errmsg);
	}
}

void DirectUnpack::FindArchiveFiles()
{
	Guard guard(m_volumeMutex);

	DirBrowser dir(m_destDir);
	while (const char* filename = dir.Next())
	{
		if (IsMainArchive(filename))
		{
			BString<1024> fullFilename("%s%c%s", *m_destDir, PATH_SEPARATOR, filename);
			if (!FileSystem::DirectoryExists(fullFilename))
			{
				m_archives.emplace_back(filename);
			}
		}
	}
}

bool DirectUnpack::IsMainArchive(const char* filename)
{
	RegEx regExRarPart(".*\\.part([0-9]+)\\.rar$");
	bool mainPart = Util::EndsWith(filename, ".rar", false) &&
		(!regExRarPart.Match(filename) || atoi(filename + regExRarPart.GetMatchStart(1)) == 1);
	return mainPart;
}

/**
 * Unrar prints "Insert disk"-message without new line terminator.
 * In order to become the message we analyze the output after every char.
 */
bool DirectUnpack::ReadLine(char* buf, int bufSize, FILE* stream)
{
	int i = 0;
	bool backspace = false;

	for (; i < bufSize - 1; i++)
	{
		int ch = fgetc(stream);

		if (ch == '\b')
		{
			backspace = true;
		}
		if (!backspace)
		{
			buf[i] = ch;
			buf[i+1] = '\0';
		}
		if (ch == EOF)
		{
			break;
		}
		if (ch == '\n')
		{
			i++;
			break;
		}
		if (i > 35 && ch == ' ' &&
			!strncmp(buf, "Insert disk with", 16) && strstr(buf, " [C]ontinue, [Q]uit "))
		{
			i++;
			break;
		}
	}

	// skip unnecessary progress messages
	if (!strncmp(buf, "...", 3))
	{
		buf[0] = '\0';
	}

	buf[i] = '\0';

	return i > 0;
}

void DirectUnpack::AddMessage(Message::EKind kind, const char* text)
{
	debug("%s", text);

	BString<1024> msgText = text;
	int len = strlen(text);

	// Modify unrar messages for better readability:
	// remove the destination path part from message "Extracting file.xxx"
	if (!strncmp(text, "Unrar: Extracting  ", 19) &&
		!strncmp(text + 19, m_unpackExtendedDir, strlen(m_unpackExtendedDir)))
	{
		msgText.Format("Unrar: Extracting %s", text + 19 + strlen(m_unpackExtendedDir) + 1);
	}

	if (!strncmp(text, "Unrar: Insert disk with", 23) && strstr(text, " [C]ontinue, [Q]uit"))
	{
		BString<1024> filename;
		filename.Set(text + 24, (int)(strstr(text, " [C]ontinue, [Q]uit") - text - 24));
		WaitNextVolume(filename);
		return;
	}

	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
	NzbInfo* nzbInfo = downloadQueue->GetQueue()->Find(m_nzbId);
	if (nzbInfo)
	{
		nzbInfo->AddMessage(kind, msgText);
	}
	else
	{
		ScriptController::AddMessage(kind, msgText);
	}

	if (!strncmp(msgText, "Unrar: Extracting ", 18) && nzbInfo)
	{
		SetProgressLabel(nzbInfo, msgText + 7);
	}

	if (!strncmp(text, "Unrar: Extracting from ", 23) && nzbInfo)
	{
		SetProgressLabel(nzbInfo, text + 7);
		m_extractedArchives.emplace_back(text + 23);
	}

	if (!IsStopped() && (
		!strncmp(text, "Unrar: Checksum error in the encrypted file", 42) ||
		!strncmp(text, "Unrar: CRC failed in the encrypted file", 39) ||
		!strncmp(text, "Unrar: The specified password is incorrect.", 43) ||
		strstr(text, " : packed data CRC failed in volume") ||
		strstr(text, " : packed data checksum error in volume") ||
		(len > 13 && !strncmp(text + len - 13, " - CRC failed", 13)) ||
		(len > 18 && !strncmp(text + len - 18, " - checksum failed", 18)) ||
		!strncmp(text, "Unrar: WARNING: You need to start extraction from a previous volume", 67)))
	{
		Stop(downloadQueue, nzbInfo);
	}

	if (!strncmp(text, "Unrar: All OK", 13))
	{
		m_allOkMessageReceived = true;
	}
}

void DirectUnpack::Stop(DownloadQueue* downloadQueue, NzbInfo* nzbInfo)
{
	debug("Stopping direct unpack for %s", *m_infoName);
	if (m_processed)
	{
		if (nzbInfo)
		{
			nzbInfo->AddMessage(Message::mkWarning, BString<1024>("Cancelling %s", *m_infoName));
		}
		else
		{
			warn("Cancelling %s", *m_infoName);
		}
	}
	AddExtraTime(nzbInfo);
	if (nzbInfo->GetPostInfo())
	{
		nzbInfo->GetPostInfo()->SetWorking(false);
	}
	Thread::Stop();
	if (m_unpacking)
	{
		Terminate();
	}
}

void DirectUnpack::WaitNextVolume(const char* filename)
{
	debug("WaitNextVolume for %s", filename);

	// Stop direct unpack if destination directory was changed during unpack
	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		NzbInfo* nzbInfo = downloadQueue->GetQueue()->Find(m_nzbId);
		if (nzbInfo && (strcmp(m_destDir, nzbInfo->GetDestDir()) ||
			strcmp(m_finalDir, nzbInfo->BuildFinalDirName())))
		{
			nzbInfo->AddMessage(Message::mkWarning, BString<1024>("Destination directory changed for %s", nzbInfo->GetName()));
			Stop(downloadQueue, nzbInfo);
		}
	}

	BString<1024> fullFilename("%s%c%s", *m_destDir, PATH_SEPARATOR, filename);
	if (FileSystem::FileExists(fullFilename))
	{
		Write("\n"); // emulating click on Enter-key for "continue"
	}
	else
	{
		Guard guard(m_volumeMutex);
		m_waitingFile = filename;
		if (m_nzbCompleted)
		{
			// nzb completed but unrar waits for another volume
			PrintMessage(Message::mkWarning, "Could not find volume %s", filename);
			GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
			NzbInfo* nzbInfo = downloadQueue->GetQueue()->Find(m_nzbId);
			Stop(downloadQueue, nzbInfo);
		}
	}
}

void DirectUnpack::FileDownloaded(DownloadQueue* downloadQueue, FileInfo* fileInfo)
{
	debug("FileDownloaded for %s/%s", fileInfo->GetNzbInfo()->GetName(), fileInfo->GetFilename());

	if (fileInfo->GetNzbInfo()->GetFailedArticles() > 0)
	{
		Stop(downloadQueue, fileInfo->GetNzbInfo());
		return;
	}

	Guard guard(m_volumeMutex);
	if (m_waitingFile && !strcasecmp(fileInfo->GetFilename(), m_waitingFile))
	{
		m_waitingFile = nullptr;
		Write("\n"); // emulating click on Enter-key for "continue"
	}

	if (IsMainArchive(fileInfo->GetFilename()))
	{
		m_archives.emplace_back(fileInfo->GetFilename());
	}
}

void DirectUnpack::NzbDownloaded(DownloadQueue* downloadQueue, NzbInfo* nzbInfo)
{
	debug("NzbDownloaded for %s", nzbInfo->GetName());

	Guard guard(m_volumeMutex);
	m_nzbCompleted = true;
	if (m_waitingFile)
	{
		// nzb completed but unrar waits for another volume
		nzbInfo->AddMessage(Message::mkWarning, BString<1024>("Unrar: Could not find volume %s", *m_waitingFile));
		Stop(downloadQueue, nzbInfo);
		return;
	}

	m_extraStartTime = Util::CurrentTime();

	if (nzbInfo->GetPostInfo())
	{
		nzbInfo->GetPostInfo()->SetProgressLabel(m_progressLabel);
	}
}

void DirectUnpack::NzbDeleted(DownloadQueue* downloadQueue, NzbInfo* nzbInfo)
{
	debug("NzbDeleted for %s", nzbInfo->GetName());

	nzbInfo->SetUnpackThread(nullptr);
	nzbInfo->SetDirectUnpackStatus(NzbInfo::nsFailure);
	Stop(downloadQueue, nzbInfo);
}

// Remove _unpack-dir
void DirectUnpack::Cleanup()
{
	debug("Cleanup for %s", *m_infoName);

	CString errmsg;
	if (FileSystem::DirectoryExists(m_unpackDir) &&
		!FileSystem::DeleteDirectoryWithContent(m_unpackDir, errmsg))
	{
		PrintMessage(Message::mkError, "Could not delete temporary directory %s: %s", *m_unpackDir, *errmsg);
	}

	if (m_finalDirCreated)
	{
		FileSystem::DeleteDirectory(m_finalDir);
	}
}

void DirectUnpack::SetProgressLabel(NzbInfo* nzbInfo, const char* progressLabel)
{
	m_progressLabel = progressLabel;
	if (nzbInfo->GetPostInfo())
	{
		nzbInfo->GetPostInfo()->SetProgressLabel(progressLabel);
	}
}

void DirectUnpack::AddExtraTime(NzbInfo* nzbInfo)
{
	if (m_extraStartTime)
	{
		int extraTime = (int)(Util::CurrentTime() - m_extraStartTime);
		nzbInfo->SetUnpackSec(nzbInfo->GetUnpackSec() + extraTime);
		nzbInfo->SetPostTotalSec(nzbInfo->GetPostTotalSec() + extraTime);
		m_extraStartTime = 0;
	}
}

bool DirectUnpack::IsArchiveFilename(const char* filename)
{
	if (Util::EndsWith(filename, ".rar", false))
	{
		return true;
	}

	RegEx regExRarMultiSeq(".*\\.[r-z][0-9][0-9]$");
	return regExRarMultiSeq.Match(filename);
}
