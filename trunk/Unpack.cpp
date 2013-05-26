/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include <ctype.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <errno.h>

#include "nzbget.h"
#include "Unpack.h"
#include "Log.h"
#include "Util.h"
#include "ParCoordinator.h"

extern Options* g_pOptions;
extern DownloadQueueHolder* g_pDownloadQueueHolder;

void UnpackController::FileList::Clear()
{
	for (iterator it = begin(); it != end(); it++)
	{
		free(*it);
	}
	clear();
}

bool UnpackController::FileList::Exists(const char* szFilename)
{
	for (iterator it = begin(); it != end(); it++)
	{
		char* szFilename1 = *it;
		if (!strcmp(szFilename1, szFilename))
		{
			return true;
		}
	}

	return false;
}

UnpackController::~UnpackController()
{
	m_archiveFiles.Clear();
}

void UnpackController::StartJob(PostInfo* pPostInfo)
{
	UnpackController* pUnpackController = new UnpackController();
	pUnpackController->m_pPostInfo = pPostInfo;
	pUnpackController->SetAutoDestroy(false);

	pPostInfo->SetPostThread(pUnpackController);

	pUnpackController->Start();
}

void UnpackController::Run()
{
	// the locking is needed for accessing the members of NZBInfo
	g_pDownloadQueueHolder->LockQueue();

	strncpy(m_szDestDir, m_pPostInfo->GetNZBInfo()->GetDestDir(), 1024);
	m_szDestDir[1024-1] = '\0';

	strncpy(m_szName, m_pPostInfo->GetNZBInfo()->GetName(), 1024);
	m_szName[1024-1] = '\0';

	m_bCleanedUpDisk = false;
	bool bUnpack = true;
	m_szPassword[0] = '\0';
	m_szFinalDir[0] = '\0';
	
	for (NZBParameterList::iterator it = m_pPostInfo->GetNZBInfo()->GetParameters()->begin(); it != m_pPostInfo->GetNZBInfo()->GetParameters()->end(); it++)
	{
		NZBParameter* pParameter = *it;
		if (!strcasecmp(pParameter->GetName(), "*Unpack:") && !strcasecmp(pParameter->GetValue(), "no"))
		{
			bUnpack = false;
		}
		if (!strcasecmp(pParameter->GetName(), "*Unpack:Password"))
		{
			strncpy(m_szPassword, pParameter->GetValue(), 1024-1);
			m_szPassword[1024-1] = '\0';
		}
	}
	
	g_pDownloadQueueHolder->UnlockQueue();

	snprintf(m_szInfoName, 1024, "unpack for %s", m_szName);
	m_szInfoName[1024-1] = '\0';

	snprintf(m_szInfoNameUp, 1024, "Unpack for %s", m_szName); // first letter in upper case
	m_szInfoNameUp[1024-1] = '\0';

	CheckStateFiles();

#ifndef DISABLE_PARCHECK
	if (bUnpack && m_bHasBrokenFiles && m_pPostInfo->GetNZBInfo()->GetParStatus() <= NZBInfo::psSkipped && m_bHasParFiles)
	{
		PrintMessage(Message::mkInfo, "%s has broken files", m_szName);
		RequestParCheck(false);
		m_pPostInfo->SetWorking(false);
		return;
	}
#endif

	if (bUnpack)
	{
		bool bScanNonStdFiles = m_pPostInfo->GetNZBInfo()->GetRenameStatus() > NZBInfo::rsSkipped ||
			m_pPostInfo->GetNZBInfo()->GetParStatus() == NZBInfo::psSuccess ||
			!m_bHasParFiles;
		CheckArchiveFiles(bScanNonStdFiles);
	}

	if (bUnpack && (m_bHasRarFiles || m_bHasNonStdRarFiles || m_bHasSevenZipFiles || m_bHasSevenZipMultiFiles))
	{
		SetInfoName(m_szInfoName);
		SetDefaultLogKind(g_pOptions->GetProcessLogKind());
		SetWorkingDir(m_szDestDir);

		PrintMessage(Message::mkInfo, "Unpacking %s", m_szName);

		CreateUnpackDir();

		m_bUnpackOK = true;
		m_bUnpackStartError = false;

		if (m_bHasRarFiles || m_bHasNonStdRarFiles)
		{
			ExecuteUnrar();
		}

		if (m_bHasSevenZipFiles && m_bUnpackOK)
		{
			ExecuteSevenZip(false);
		}

		if (m_bHasSevenZipMultiFiles && m_bUnpackOK)
		{
			ExecuteSevenZip(true);
		}

		Completed();
	}
	else
	{
		PrintMessage(Message::mkInfo, (bUnpack ? "Nothing to unpack for %s" : "Unpack for %s skipped"), m_szName);

#ifndef DISABLE_PARCHECK
		if (bUnpack && m_pPostInfo->GetNZBInfo()->GetParStatus() <= NZBInfo::psSkipped && m_bHasParFiles)
		{
			RequestParCheck(m_pPostInfo->GetNZBInfo()->GetRenameStatus() <= NZBInfo::rsSkipped);
		}
		else
#endif
		{
			m_pPostInfo->GetNZBInfo()->SetUnpackStatus(NZBInfo::usSkipped);
			m_pPostInfo->SetStage(PostInfo::ptQueued);
		}
	}

	m_pPostInfo->SetWorking(false);
}

void UnpackController::ExecuteUnrar()
{
	// Format: 
	//   unrar x -y -p- -o+ *.rar ./_unpack

	char szPasswordParam[1024];
	const char* szArgs[8];
	szArgs[0] = g_pOptions->GetUnrarCmd();
	szArgs[1] = "x";
	szArgs[2] = "-y";
	szArgs[3] = "-p-";
	if (strlen(m_szPassword) > 0)
	{
		snprintf(szPasswordParam, 1024, "-p%s", m_szPassword);
		szArgs[3] = szPasswordParam;
	}
	szArgs[4] = "-o+";
	szArgs[5] = m_bHasNonStdRarFiles ? "*.*" : "*.rar";
	szArgs[6] = m_szUnpackDir;
	szArgs[7] = NULL;
	SetArgs(szArgs, false);

	SetScript(g_pOptions->GetUnrarCmd());
	SetLogPrefix("Unrar");

	m_bAllOKMessageReceived = false;
	m_eUnpacker = upUnrar;
	
	SetProgressLabel("");
	int iExitCode = Execute();
	SetLogPrefix(NULL);
	SetProgressLabel("");

	m_bUnpackOK = iExitCode == 0 && m_bAllOKMessageReceived && !GetTerminated();
	m_bUnpackStartError = iExitCode == -1;

	if (!m_bUnpackOK && iExitCode > 0)
	{
		PrintMessage(Message::mkError, "Unrar error code: %i", iExitCode);
	}
}

void UnpackController::ExecuteSevenZip(bool bMultiVolumes)
{
	// Format: 
	//   7z x -y -p- -o./_unpack *.7z
	// OR
	//   7z x -y -p- -o./_unpack *.7z.001

	char szPasswordParam[1024];
	const char* szArgs[7];
	szArgs[0] = g_pOptions->GetSevenZipCmd();
	szArgs[1] = "x";
	szArgs[2] = "-y";

	szArgs[3] = "-p-";
	if (strlen(m_szPassword) > 0)
	{
		snprintf(szPasswordParam, 1024, "-p%s", m_szPassword);
		szArgs[3] = szPasswordParam;
	}

	char szUnpackDirParam[1024];
	snprintf(szUnpackDirParam, 1024, "-o%s", m_szUnpackDir);
	szArgs[4] = szUnpackDirParam;

	szArgs[5] = bMultiVolumes ? "*.7z.001" : "*.7z";
	szArgs[6] = NULL;
	SetArgs(szArgs, false);

	SetScript(g_pOptions->GetSevenZipCmd());

	m_bAllOKMessageReceived = false;
	m_eUnpacker = upSevenZip;

	PrintMessage(Message::mkInfo, "Executing 7-Zip");
	SetLogPrefix("7-Zip");
	SetProgressLabel("");
	int iExitCode = Execute();
	SetLogPrefix(NULL);
	SetProgressLabel("");

	m_bUnpackOK = iExitCode == 0 && m_bAllOKMessageReceived && !GetTerminated();
	m_bUnpackStartError = iExitCode == -1;

	if (!m_bUnpackOK && iExitCode > 0)
	{
		PrintMessage(Message::mkError, "7-Zip error code: %i", iExitCode);
	}
}

void UnpackController::Completed()
{
	bool bCleanupSuccess = Cleanup();

	if (m_bUnpackOK && bCleanupSuccess)
	{
		PrintMessage(Message::mkInfo, "%s %s", m_szInfoNameUp, "successful");
		m_pPostInfo->GetNZBInfo()->SetUnpackStatus(NZBInfo::usSuccess);
		m_pPostInfo->GetNZBInfo()->SetUnpackCleanedUpDisk(m_bCleanedUpDisk);
		m_pPostInfo->SetStage(PostInfo::ptQueued);
	}
	else
	{
#ifndef DISABLE_PARCHECK
		if (!m_bUnpackOK && m_pPostInfo->GetNZBInfo()->GetParStatus() <= NZBInfo::psSkipped && !m_bUnpackStartError && !GetTerminated() && m_bHasParFiles)
		{
			RequestParCheck(false);
		}
		else
#endif
		{
			PrintMessage(Message::mkError, "%s failed", m_szInfoNameUp);
			m_pPostInfo->GetNZBInfo()->SetUnpackStatus(NZBInfo::usFailure);
			m_pPostInfo->SetStage(PostInfo::ptQueued);
		}
	}
}

#ifndef DISABLE_PARCHECK
void UnpackController::RequestParCheck(bool bRename)
{
	PrintMessage(Message::mkInfo, "%s requested %s", m_szInfoNameUp, bRename ? "par-rename": "par-check/repair");
	if (bRename)
	{
		m_pPostInfo->SetRequestParRename(true);
	}
	else
	{
		m_pPostInfo->SetRequestParCheck(true);
	}
	m_pPostInfo->SetStage(PostInfo::ptFinished);
}
#endif

void UnpackController::CheckStateFiles()
{
	char szBrokenLog[1024];
	snprintf(szBrokenLog, 1024, "%s%c%s", m_szDestDir, PATH_SEPARATOR, "_brokenlog.txt");
	szBrokenLog[1024-1] = '\0';
	m_bHasBrokenFiles = Util::FileExists(szBrokenLog);

	m_bHasParFiles = ParCoordinator::FindMainPars(m_szDestDir, NULL);
}

void UnpackController::CreateUnpackDir()
{
	m_bInterDir = strlen(g_pOptions->GetInterDir()) > 0 &&
		!strncmp(m_szDestDir, g_pOptions->GetInterDir(), strlen(g_pOptions->GetInterDir()));
	if (m_bInterDir)
	{
		m_pPostInfo->GetNZBInfo()->BuildFinalDirName(m_szFinalDir, 1024);
		m_szFinalDir[1024-1] = '\0';
		snprintf(m_szUnpackDir, 1024, "%s%c%s", m_szFinalDir, PATH_SEPARATOR, "_unpack");
	}
	else
	{
		snprintf(m_szUnpackDir, 1024, "%s%c%s", m_szDestDir, PATH_SEPARATOR, "_unpack");
	}
	m_szUnpackDir[1024-1] = '\0';

	char szErrBuf[1024];
	if (!Util::ForceDirectories(m_szUnpackDir, szErrBuf, sizeof(szErrBuf)))
	{
		error("Could not create directory %s: %s", m_szUnpackDir, szErrBuf);
	}
}


void UnpackController::CheckArchiveFiles(bool bScanNonStdFiles)
{
	m_bHasRarFiles = false;
	m_bHasNonStdRarFiles = false;
	m_bHasSevenZipFiles = false;
	m_bHasSevenZipMultiFiles = false;

	RegEx regExRar(".*\\.rar$");
	RegEx regExRarMultiSeq(".*\\.(r|s)[0-9][0-9]$");
	RegEx regExSevenZip(".*\\.7z$");
	RegEx regExSevenZipMulti(".*\\.7z\\.[0-9]*$");

	DirBrowser dir(m_szDestDir);
	while (const char* filename = dir.Next())
	{
		char szFullFilename[1024];
		snprintf(szFullFilename, 1024, "%s%c%s", m_szDestDir, PATH_SEPARATOR, filename);
		szFullFilename[1024-1] = '\0';

		if (strcmp(filename, ".") && strcmp(filename, "..") && !Util::DirectoryExists(szFullFilename))
		{
			if (regExRar.Match(filename))
			{
				m_bHasRarFiles = true;
			}
			else if (regExSevenZip.Match(filename))
			{
				m_bHasSevenZipFiles = true;
			}
			else if (regExSevenZipMulti.Match(filename))
			{
				m_bHasSevenZipMultiFiles = true;
			}
			else if (bScanNonStdFiles && !m_bHasNonStdRarFiles && !regExRarMultiSeq.Match(filename))
			{
				// Check if file has RAR signature
				char rarSignature[] = {0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x00};
				char fileSignature[7];

				FILE* infile;
				infile = fopen(szFullFilename, "rb");
				if (infile)
				{
					int cnt = (int)fread(fileSignature, 1, sizeof(fileSignature), infile);
					fclose(infile);
					if (cnt == sizeof(fileSignature) && !strcmp(rarSignature, fileSignature))
					{
						m_bHasNonStdRarFiles = true;
					}
				}
			}
		}
	}
}

bool UnpackController::Cleanup()
{
	// By success:
	//   - move unpacked files to destination dir;
	//   - remove _unpack-dir;
	//   - delete archive-files.
	// By failure:
	//   - remove _unpack-dir.

	bool bOK = true;

	FileList extractedFiles;

	if (m_bUnpackOK)
	{
		// moving files back
		DirBrowser dir(m_szUnpackDir);
		while (const char* filename = dir.Next())
		{
			if (strcmp(filename, ".") && strcmp(filename, ".."))
			{
				char szSrcFile[1024];
				snprintf(szSrcFile, 1024, "%s%c%s", m_szUnpackDir, PATH_SEPARATOR, filename);
				szSrcFile[1024-1] = '\0';

				char szDstFile[1024];
				snprintf(szDstFile, 1024, "%s%c%s", m_szFinalDir[0] != '\0' ? m_szFinalDir : m_szDestDir, PATH_SEPARATOR, filename);
				szDstFile[1024-1] = '\0';

				// silently overwrite existing files
				remove(szDstFile);

				if (!Util::MoveFile(szSrcFile, szDstFile))
				{
					PrintMessage(Message::mkError, "Could not move file %s to %s", szSrcFile, szDstFile);
					bOK = false;
				}

				extractedFiles.push_back(strdup(filename));
			}
		}
	}

	if (bOK && !Util::DeleteDirectoryWithContent(m_szUnpackDir))
	{
		PrintMessage(Message::mkError, "Could not remove temporary directory %s", m_szUnpackDir);
	}

	if (m_bUnpackOK && bOK && g_pOptions->GetUnpackCleanupDisk())
	{
		PrintMessage(Message::mkInfo, "Deleting archive files");

		// Delete rar-files (only files which were used by unrar)
		for (FileList::iterator it = m_archiveFiles.begin(); it != m_archiveFiles.end(); it++)
		{
			char* szFilename = *it;

			if (m_bInterDir || !extractedFiles.Exists(szFilename))
			{
				char szFullFilename[1024];
				snprintf(szFullFilename, 1024, "%s%c%s", m_szDestDir, PATH_SEPARATOR, szFilename);
				szFullFilename[1024-1] = '\0';

				PrintMessage(Message::mkInfo, "Deleting file %s", szFilename);

				if (remove(szFullFilename) != 0)
				{
					PrintMessage(Message::mkError, "Could not delete file %s", szFullFilename);
				}
			}
		}

		// Unfortunately 7-Zip doesn't print the processed archive-files to the output.
		// Therefore we don't know for sure which files were extracted.
		// We just delete all 7z-files in the directory.

		RegEx regExSevenZip(".*\\.7z$|.*\\.7z\\.[0-9]*$");

		DirBrowser dir(m_szDestDir);
		while (const char* filename = dir.Next())
		{
			char szFullFilename[1024];
			snprintf(szFullFilename, 1024, "%s%c%s", m_szDestDir, PATH_SEPARATOR, filename);
			szFullFilename[1024-1] = '\0';

			if (strcmp(filename, ".") && strcmp(filename, "..") && !Util::DirectoryExists(szFullFilename) 
				&& regExSevenZip.Match(filename) && (m_bInterDir || !extractedFiles.Exists(filename)))
			{
				PrintMessage(Message::mkInfo, "Deleting file %s", filename);

				if (remove(szFullFilename) != 0)
				{
					PrintMessage(Message::mkError, "Could not delete file %s", szFullFilename);
				}
			}
		}

		m_bCleanedUpDisk = true;
	}

	extractedFiles.Clear();

	return bOK;
}

/**
 * Unrar prints progress information into the same line using backspace control character.
 * In order to print progress continuously we analyze the output after every char
 * and update post-job progress information.
 */
bool UnpackController::ReadLine(char* szBuf, int iBufSize, FILE* pStream)
{
	bool bPrinted = false;

	int i = 0;

	for (; i < iBufSize - 1; i++)
	{
		int ch = fgetc(pStream);
		szBuf[i] = ch;
		szBuf[i+1] = '\0';
		if (ch == EOF)
		{
			break;
		}
		if (ch == '\n')
		{
			i++;
			break;
		}

		char* szBackspace = strrchr(szBuf, '\b');
		if (szBackspace)
		{
			if (!bPrinted)
			{
				char tmp[1024];
				strncpy(tmp, szBuf, 1024);
				tmp[1024-1] = '\0';
				char* szTmpPercent = strrchr(tmp, '\b');
				if (szTmpPercent)
				{
					*szTmpPercent = '\0';
				}
				if (strncmp(szBuf, "...", 3))
				{
					ProcessOutput(tmp);
				}
				bPrinted = true;
			}
			if (strchr(szBackspace, '%'))
			{
				int iPercent = atoi(szBackspace + 1);
				m_pPostInfo->SetStageProgress(iPercent * 10);
			}
		}
	}

	szBuf[i] = '\0';

	if (bPrinted)
	{
		szBuf[0] = '\0';
	}

	return i > 0;
}

void UnpackController::AddMessage(Message::EKind eKind, const char* szText)
{
	char szMsgText[1024];
	strncpy(szMsgText, szText, 1024);
	szMsgText[1024-1] = '\0';
	
	// Modify unrar messages for better readability:
	// remove the destination path part from message "Extracting file.xxx"
	if (m_eUnpacker == upUnrar && !strncmp(szText, "Unrar: Extracting  ", 19) &&
		!strncmp(szText + 19, m_szUnpackDir, strlen(m_szUnpackDir)))
	{
		snprintf(szMsgText, 1024, "Unrar: Extracting %s", szText + 19 + strlen(m_szUnpackDir) + 1);
		szMsgText[1024-1] = '\0';
	}

	ScriptController::AddMessage(eKind, szMsgText);
	m_pPostInfo->AppendMessage(eKind, szMsgText);

	if (m_eUnpacker == upUnrar && !strncmp(szMsgText, "Unrar: UNRAR ", 6) &&
		strstr(szMsgText, " Copyright ") && strstr(szMsgText, " Alexander Roshal"))
	{
		// reset start time for a case if user uses unpack-script to do some things
		// (like sending Wake-On-Lan message) before executing unrar
		m_pPostInfo->SetStageTime(time(NULL));
	}

	if (m_eUnpacker == upUnrar && !strncmp(szMsgText, "Unrar: Extracting ", 18))
	{
		SetProgressLabel(szMsgText + 7);
	}

	if (m_eUnpacker == upUnrar && !strncmp(szText, "Unrar: Extracting from ", 23))
	{
		const char *szFilename = szText + 23;
		debug("Filename: %s", szFilename);
		m_archiveFiles.push_back(strdup(szFilename));
		SetProgressLabel(szText + 7);
	}

	if ((m_eUnpacker == upUnrar && !strncmp(szText, "Unrar: All OK", 13)) ||
		(m_eUnpacker == upSevenZip && !strncmp(szText, "7-Zip: Everything is Ok", 23)))
	{
		m_bAllOKMessageReceived = true;
	}
}

void UnpackController::Stop()
{
	debug("Stopping unpack");
	Thread::Stop();
	Terminate();
}

void UnpackController::SetProgressLabel(const char* szProgressLabel)
{
	g_pDownloadQueueHolder->LockQueue();
	m_pPostInfo->SetProgressLabel(szProgressLabel);
	g_pDownloadQueueHolder->UnlockQueue();
}


void MoveController::StartJob(PostInfo* pPostInfo)
{
	MoveController* pMoveController = new MoveController();
	pMoveController->m_pPostInfo = pPostInfo;
	pMoveController->SetAutoDestroy(false);

	pPostInfo->SetPostThread(pMoveController);

	pMoveController->Start();
}

void MoveController::Run()
{
	// the locking is needed for accessing the members of NZBInfo
	g_pDownloadQueueHolder->LockQueue();

	char szNZBName[1024];
	strncpy(szNZBName, m_pPostInfo->GetNZBInfo()->GetName(), 1024);
	szNZBName[1024-1] = '\0';

	char szInfoName[1024];
	snprintf(szInfoName, 1024, "move for %s", m_pPostInfo->GetNZBInfo()->GetName());
	szInfoName[1024-1] = '\0';
	SetInfoName(szInfoName);

	SetDefaultLogKind(g_pOptions->GetProcessLogKind());

	strncpy(m_szInterDir, m_pPostInfo->GetNZBInfo()->GetDestDir(), 1024);
	m_szInterDir[1024-1] = '\0';

	m_pPostInfo->GetNZBInfo()->BuildFinalDirName(m_szDestDir, 1024);
	m_szDestDir[1024-1] = '\0';

	g_pDownloadQueueHolder->UnlockQueue();

	info("Moving completed files for %s", szNZBName);

	bool bOK = MoveFiles();

	szInfoName[0] = 'M'; // uppercase

	if (bOK)
	{
		info("%s successful", szInfoName);
		// save new dest dir
		g_pDownloadQueueHolder->LockQueue();
		m_pPostInfo->GetNZBInfo()->SetDestDir(m_szDestDir);
		m_pPostInfo->GetNZBInfo()->SetMoveStatus(NZBInfo::msSuccess);
		g_pDownloadQueueHolder->UnlockQueue();
	}
	else
	{
		error("%s failed", szInfoName);
		m_pPostInfo->GetNZBInfo()->SetMoveStatus(NZBInfo::msFailure);
	}

	m_pPostInfo->SetStage(PostInfo::ptQueued);
	m_pPostInfo->SetWorking(false);
}

bool MoveController::MoveFiles()
{
	char szErrBuf[1024];
	if (!Util::ForceDirectories(m_szDestDir, szErrBuf, sizeof(szErrBuf)))
	{
		error("Could not create directory %s: %s", m_szDestDir, szErrBuf);
		return false;
	}

	bool bOK = true;
	DirBrowser dir(m_szInterDir);
	while (const char* filename = dir.Next())
	{
		if (strcmp(filename, ".") && strcmp(filename, ".."))
		{
			char szSrcFile[1024];
			snprintf(szSrcFile, 1024, "%s%c%s", m_szInterDir, PATH_SEPARATOR, filename);
			szSrcFile[1024-1] = '\0';

			char szDstFile[1024];
			snprintf(szDstFile, 1024, "%s%c%s", m_szDestDir, PATH_SEPARATOR, filename);
			szDstFile[1024-1] = '\0';

			// prevent overwriting of existing files
			int dupcount = 0;
			while (Util::FileExists(szDstFile))
			{
				dupcount++;
				snprintf(szDstFile, 1024, "%s%c%s_duplicate%d", m_szDestDir, PATH_SEPARATOR, filename, dupcount);
				szDstFile[1024-1] = '\0';
			}

			PrintMessage(Message::mkInfo, "Moving file %s to %s", Util::BaseFileName(szSrcFile), m_szDestDir);
			if (!Util::MoveFile(szSrcFile, szDstFile))
			{
				PrintMessage(Message::mkError, "Could not move file %s to %s! Errcode: %i", szSrcFile, szDstFile, errno);
				bOK = false;
			}
		}
	}

	Util::RemoveDirectory(m_szInterDir);

	return bOK;
}


void CleanupController::StartJob(PostInfo* pPostInfo)
{
	CleanupController* pCleanupController = new CleanupController();
	pCleanupController->m_pPostInfo = pPostInfo;
	pCleanupController->SetAutoDestroy(false);

	pPostInfo->SetPostThread(pCleanupController);

	pCleanupController->Start();
}

void CleanupController::Run()
{
	// the locking is needed for accessing the members of NZBInfo
	g_pDownloadQueueHolder->LockQueue();

	char szNZBName[1024];
	strncpy(szNZBName, m_pPostInfo->GetNZBInfo()->GetName(), 1024);
	szNZBName[1024-1] = '\0';

	char szInfoName[1024];
	snprintf(szInfoName, 1024, "cleanup for %s", m_pPostInfo->GetNZBInfo()->GetName());
	szInfoName[1024-1] = '\0';
	SetInfoName(szInfoName);

	SetDefaultLogKind(g_pOptions->GetProcessLogKind());

	strncpy(m_szDestDir, m_pPostInfo->GetNZBInfo()->GetDestDir(), 1024);
	m_szDestDir[1024-1] = '\0';

	bool bInterDir = strlen(g_pOptions->GetInterDir()) > 0 &&
		!strncmp(m_szDestDir, g_pOptions->GetInterDir(), strlen(g_pOptions->GetInterDir()));
	if (bInterDir)
	{
		m_pPostInfo->GetNZBInfo()->BuildFinalDirName(m_szFinalDir, 1024);
		m_szFinalDir[1024-1] = '\0';
	}
	else
	{
		m_szFinalDir[0] = '\0';
	}

	g_pDownloadQueueHolder->UnlockQueue();

	info("Cleaning up %s", szNZBName);

	bool bDeleted = false;
	bool bOK = Cleanup(m_szDestDir, &bDeleted);

	if (bOK && m_szFinalDir[0] != '\0')
	{
		bool bDeleted2 = false;
		bOK = Cleanup(m_szFinalDir, &bDeleted2);
		bDeleted = bDeleted || bDeleted2;
	}

	szInfoName[0] = 'C'; // uppercase

	if (bOK && bDeleted)
	{
		info("%s successful", szInfoName);
		m_pPostInfo->GetNZBInfo()->SetCleanupStatus(NZBInfo::csSuccess);
	}
	else if (bOK)
	{
		info("Nothing to cleanup for %s", szNZBName);
		m_pPostInfo->GetNZBInfo()->SetCleanupStatus(NZBInfo::csSuccess);
	}
	else
	{
		error("%s failed", szInfoName);
		m_pPostInfo->GetNZBInfo()->SetCleanupStatus(NZBInfo::csFailure);
	}

	m_pPostInfo->SetStage(PostInfo::ptQueued);
	m_pPostInfo->SetWorking(false);
}

bool CleanupController::Cleanup(const char* szDestDir, bool *bDeleted)
{
	*bDeleted = false;
	bool bOK = true;

	ExtList extList;
	
	// split ExtCleanupDisk into tokens and create a list
	char* szExtCleanupDisk = strdup(g_pOptions->GetExtCleanupDisk());
	
	char* saveptr;
	char* szExt = strtok_r(szExtCleanupDisk, ",; ", &saveptr);
	while (szExt)
	{
		extList.push_back(szExt);
		szExt = strtok_r(NULL, ",; ", &saveptr);
	}
	
	DirBrowser dir(szDestDir);
	while (const char* filename = dir.Next())
	{
		// check file extension
		
		int iFilenameLen = strlen(filename);
		bool bDeleteIt = false;
		for (ExtList::iterator it = extList.begin(); it != extList.end(); it++)
		{
			const char* szExt = *it;
			int iExtLen = strlen(szExt);
			if (iFilenameLen >= iExtLen && !strcasecmp(szExt, filename + iFilenameLen - iExtLen))
			{
				bDeleteIt = true;
				break;
			}
		}

		if (bDeleteIt)
		{
			char szFullFilename[1024];
			snprintf(szFullFilename, 1024, "%s%c%s", szDestDir, PATH_SEPARATOR, filename);
			szFullFilename[1024-1] = '\0';

			PrintMessage(Message::mkInfo, "Deleting file %s", filename);
			if (remove(szFullFilename) != 0)
			{
				PrintMessage(Message::mkError, "Could not delete file %s! Errcode: %i", szFullFilename, errno);
				bOK = false;
			}

			*bDeleted = true;
		}
	}

	free(szExtCleanupDisk);

	return bOK;
}
