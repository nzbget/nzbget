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

bool DirectUnpack::ParamList::Exists(const char* param)
{
	return std::find(begin(), end(), param) != end();
}


void DirectUnpack::StartJob(NzbInfo* nzbInfo)
{
	DirectUnpack* directUnpack = new DirectUnpack();
	directUnpack->m_nzbInfo = nzbInfo;
	directUnpack->SetAutoDestroy(false);

	nzbInfo->SetUnpackThread(directUnpack);
	nzbInfo->SetDirectUnpackStatus(NzbInfo::nsRunning);

	directUnpack->Start();
}

void DirectUnpack::Run()
{
	{
		GuardedDownloadQueue guard = DownloadQueue::Guard();

		m_destDir = m_nzbInfo->GetDestDir();
		m_name = m_nzbInfo->GetName();

		NzbParameter* parameter = m_nzbInfo->GetParameters()->Find("*Unpack:Password", false);
		if (parameter)
		{
			m_password = parameter->GetValue();
		}
	}

	m_infoName.Format("direct unpack for %s", *m_name);
	m_infoNameUp.Format("Direct unpack for %s", *m_name); // first letter in upper case

	CheckArchiveFiles();

	SetInfoName(m_infoName);
	SetWorkingDir(m_destDir);

	if (m_hasRarFiles)
	{
		PrintMessage(Message::mkInfo, "Direct unpacking %s", *m_name);
		CreateUnpackDir();
		ExecuteUnrar();
	}

	{
		GuardedDownloadQueue guard = DownloadQueue::Guard();
		m_nzbInfo->SetUnpackThread(nullptr);
		m_nzbInfo->SetDirectUnpackStatus(m_hasRarFiles ? m_unpackOk ? NzbInfo::nsSuccess : NzbInfo::nsFailure : NzbInfo::nsNone);
		SetAutoDestroy(true);
	}
}

void DirectUnpack::ExecuteUnrar()
{
	// Format:
	//   unrar x -y -p- -o+ *.rar ./_unpack/

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

	params.emplace_back("*.rar");
	params.push_back(FileSystem::MakeExtendedPath(BString<1024>("%s%c", *m_unpackDir, PATH_SEPARATOR), true));
	SetArgs(std::move(params));
	SetLogPrefix("Unrar");
	ResetEnv();

	m_allOkMessageReceived = false;

	SetNeedWrite(true);
	int exitCode = Execute();
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
		!strncmp(m_nzbInfo->GetDestDir(), g_Options->GetInterDir(), strlen(g_Options->GetInterDir())) &&
		m_nzbInfo->GetDestDir()[strlen(g_Options->GetInterDir())] == PATH_SEPARATOR;

	if (useInterDir)
	{
		m_finalDir = m_nzbInfo->BuildFinalDirName();
		m_finalDirCreated = !FileSystem::DirectoryExists(m_finalDir);
	}

	const char* destDir = !m_finalDir.Empty() ? *m_finalDir : *m_destDir;

	m_unpackDir.Format("%s%c%s", destDir, PATH_SEPARATOR, "_unpack");

	detail("Unpacking into %s", *m_unpackDir);

	CString errmsg;
	if (!FileSystem::ForceDirectories(m_unpackDir, errmsg))
	{
		PrintMessage(Message::mkError, "Could not create directory %s: %s", *m_unpackDir, *errmsg);
	}
}

void DirectUnpack::CheckArchiveFiles()
{
	m_hasRarFiles = false;

	RegEx regExRar(".*\\.rar$");
	RegEx regExRarMultiSeq(".*\\.[r-z][0-9][0-9]$");

	DirBrowser dir(m_destDir);
	while (const char* filename = dir.Next())
	{
		BString<1024> fullFilename("%s%c%s", *m_destDir, PATH_SEPARATOR, filename);

		if (!FileSystem::DirectoryExists(fullFilename) && regExRar.Match(filename))
		{
			m_hasRarFiles = true;
		}
	}
}

/**
 * Unrar prints progress information into the same line using backspace control character.
 * In order to print progress continuously we analyze the output after every char
 * and update post-job progress information.
 */
bool DirectUnpack::ReadLine(char* buf, int bufSize, FILE* stream)
{
	bool printed = false;

	int i = 0;

	for (; i < bufSize - 1; i++)
	{
		int ch = fgetc(stream);

		buf[i] = ch;
		buf[i+1] = '\0';
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

		char* backspace = strrchr(buf, '\b');
		if (backspace)
		{
			if (!printed)
			{
				BString<1024> tmp = buf;
				char* tmpPercent = strrchr(tmp, '\b');
				if (tmpPercent)
				{
					*tmpPercent = '\0';
				}
				if (strncmp(buf, "...", 3))
				{
					ProcessOutput(tmp);
				}
				printed = true;
			}
		}
	}

	buf[i] = '\0';

	if (printed)
	{
		buf[0] = '\0';
	}

	return i > 0;
}

void DirectUnpack::AddMessage(Message::EKind kind, const char* text)
{
	BString<1024> msgText = text;
	int len = strlen(text);

	// Modify unrar messages for better readability:
	// remove the destination path part from message "Extracting file.xxx"
	if (!strncmp(text, "Unrar: Extracting  ", 19) &&
		!strncmp(text + 19, m_unpackDir, strlen(m_unpackDir)))
	{
		msgText.Format("Unrar: Extracting %s", text + 19 + strlen(m_unpackDir) + 1);
	}

	if (!strncmp(text, "Unrar: Insert disk with", 23) && strstr(text, " [C]ontinue, [Q]uit"))
	{
		BString<1024> filename;
		filename.Set(text + 24, strstr(text, " [C]ontinue, [Q]uit") - text - 24);
		WaitNextVolume(filename);
		return;
	}

	m_nzbInfo->AddMessage(kind, msgText);

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
		m_nzbInfo->AddMessage(Message::mkWarning,
			BString<1024>("Cancelling %s due to errors", *m_infoName));
		Stop();
	}

	if (!strncmp(text, "Unrar: All OK", 13))
	{
		m_allOkMessageReceived = true;
	}
}

void DirectUnpack::Stop()
{
	debug("Stopping direct unpack");
	Thread::Stop();
	Terminate();
}

void DirectUnpack::FileDownloaded(FileInfo* fileInfo)
{
	if (m_waitingFile && !strcasecmp(fileInfo->GetFilename(), m_waitingFile))
	{
		debug("File %s just arrived", *m_waitingFile);
		m_waitingFile = nullptr;
		Write("\n"); // emulating click on Enter-key for "continue"
	}
}

void DirectUnpack::WaitNextVolume(const char* filename)
{
	debug("Looking for %s", filename);
	BString<1024> fullFilename("%s%c%s", *m_destDir, PATH_SEPARATOR, filename);
	if (FileSystem::FileExists(fullFilename))
	{
		debug("File %s already there", filename);
		Write("\n"); // emulating click on Enter-key for "continue"
	}
	else
	{
		debug("Waiting for %s", filename);
		m_waitingFile = filename;
	}
}
