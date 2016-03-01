/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2013-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#ifndef DISABLE_PARCHECK

#include "par2cmdline.h"
#include "par2repairer.h"
#include "md5.h"

#include "ParRenamer.h"
#include "ParParser.h"
#include "Log.h"
#include "Options.h"
#include "Util.h"
#include "FileSystem.h"

class ParRenamerRepairer : public Par2::Par2Repairer
{
public:
	friend class ParRenamer;
};


void ParRenamer::Cleanup()
{
	m_dirList.clear();
	m_fileHashList.clear();
}

void ParRenamer::Cancel()
{
	m_cancelled = true;
}

void ParRenamer::Run()
{
	Cleanup();
	m_cancelled = false;
	m_fileCount = 0;
	m_curFile = 0;
	m_renamedCount = 0;
	m_hasMissedFiles = false;
	m_status = psFailed;

	m_progressLabel.Format("Checking renamed files for %s", *m_infoName);
	m_stageProgress = 0;
	UpdateProgress();

	BuildDirList(m_destDir);

	for (CString& destDir : m_dirList)
	{
		debug("Checking %s", *destDir);
		m_fileHashList.clear();
		LoadParFiles(destDir);

		if (m_fileHashList.empty())
		{
			int savedCurFile = m_curFile;
			CheckFiles(destDir, true);
			m_curFile = savedCurFile; // restore progress indicator
			LoadParFiles(destDir);
		}

		CheckFiles(destDir, false);

		if (m_detectMissing)
		{
			CheckMissing();
		}
	}

	if (m_cancelled)
	{
		PrintMessage(Message::mkWarning, "Renaming cancelled for %s", *m_infoName);
	}
	else if (m_renamedCount > 0)
	{
		PrintMessage(Message::mkInfo, "Successfully renamed %i file(s) for %s", m_renamedCount, *m_infoName);
		m_status = psSuccess;
	}
	else
	{
		PrintMessage(Message::mkInfo, "No renamed files found for %s", *m_infoName);
	}

	Cleanup();
	Completed();
}

void ParRenamer::BuildDirList(const char* destDir)
{
	m_dirList.push_back(destDir);

	DirBrowser dirBrowser(destDir);

	while (const char* filename = dirBrowser.Next())
	{
		if (!m_cancelled)
		{
			BString<1024> fullFilename("%s%c%s", destDir, PATH_SEPARATOR, filename);
			if (FileSystem::DirectoryExists(fullFilename))
			{
				BuildDirList(fullFilename);
			}
			else
			{
				m_fileCount++;
			}
		}
	}
}

void ParRenamer::LoadParFiles(const char* destDir)
{
	ParParser::ParFileList parFileList;
	ParParser::FindMainPars(destDir, &parFileList);

	for (CString& parFilename : parFileList)
	{
		BString<1024> fullParFilename("%s%c%s", destDir, PATH_SEPARATOR, *parFilename);
		LoadParFile(fullParFilename);
	}
}

void ParRenamer::LoadParFile(const char* parFilename)
{
	ParRenamerRepairer repairer;

	if (!repairer.LoadPacketsFromFile(parFilename))
	{
		PrintMessage(Message::mkWarning, "Could not load par2-file %s", parFilename);
		return;
	}

	for (std::pair<const Par2::MD5Hash, Par2::Par2RepairerSourceFile*>& entry : repairer.sourcefilemap)
	{
		if (m_cancelled)
		{
			break;
		}

		Par2::Par2RepairerSourceFile* sourceFile = entry.second;
		if (!sourceFile || !sourceFile->GetDescriptionPacket())
		{
			PrintMessage(Message::mkWarning, "Damaged par2-file detected: %s", parFilename);
			continue;
		}
		m_fileHashList.emplace_back(sourceFile->GetDescriptionPacket()->FileName().c_str(),
			sourceFile->GetDescriptionPacket()->Hash16k().print().c_str());
		RegisterParredFile(sourceFile->GetDescriptionPacket()->FileName().c_str());
	}
}

void ParRenamer::CheckFiles(const char* destDir, bool renamePars)
{
	DirBrowser dir(destDir);
	while (const char* filename = dir.Next())
	{
		if (!m_cancelled)
		{
			BString<1024> fullFilename("%s%c%s", destDir, PATH_SEPARATOR, filename);

			if (!FileSystem::DirectoryExists(fullFilename))
			{
				m_progressLabel.Format("Checking file %s", filename);
				m_stageProgress = m_fileCount > 0 ? m_curFile * 1000 / m_fileCount : 1000;
				UpdateProgress();
				m_curFile++;

				if (renamePars)
				{
					CheckParFile(destDir, fullFilename);
				}
				else
				{
					CheckRegularFile(destDir, fullFilename);
				}
			}
		}
	}
}

void ParRenamer::CheckMissing()
{
	for (FileHash& fileHash : m_fileHashList)
	{
		if (!fileHash.GetFileExists())
		{
			if (Util::MatchFileExt(fileHash.GetFilename(), g_Options->GetParIgnoreExt(), ",;"))
			{
				PrintMessage(Message::mkInfo, "File %s is missing, ignoring", fileHash.GetFilename());
			}
			else
			{
				PrintMessage(Message::mkInfo, "File %s is missing", fileHash.GetFilename());
				m_hasMissedFiles = true;
			}
		}
	}
}

bool ParRenamer::IsSplittedFragment(const char* filename, const char* correctName)
{
	bool splittedFragement = false;
	const char* diskBasename = FileSystem::BaseFileName(filename);
	const char* extension = strrchr(diskBasename, '.');
	int baseLen = strlen(correctName);
	if (extension && !strncasecmp(diskBasename, correctName, baseLen))
	{
		const char* p = diskBasename + baseLen;
		if (*p == '.')
		{
			for (p++; *p && strchr("0123456789", *p); p++) ;
			splittedFragement = !*p;
			splittedFragement = splittedFragement && atoi(diskBasename + baseLen + 1) <= 1; // .000 or .001
		}
	}

	return splittedFragement;
}

void ParRenamer::CheckRegularFile(const char* destDir, const char* filename)
{
	debug("Computing hash for %s", filename);

	DiskFile file;
	if (!file.Open(filename, DiskFile::omRead))
	{
		PrintMessage(Message::mkError, "Could not open file %s", filename);
		return;
	}

	// load first 16K of the file into buffer
	static const int blockSize = 16*1024;
	CharBuffer buffer(blockSize);

	int readBytes = (int)file.Read(buffer, buffer.Size());
	if (readBytes != buffer.Size() && file.Error())
	{
		PrintMessage(Message::mkError, "Could not read file %s", filename);
		return;
	}

	file.Close();

	Par2::MD5Hash hash16k;
	Par2::MD5Context context;
	context.Update(buffer, readBytes);
	context.Final(hash16k);

	debug("file: %s; hash16k: %s", FileSystem::BaseFileName(filename), hash16k.print().c_str());

	for (FileHash& fileHash : m_fileHashList)
	{
		if (!strcmp(fileHash.GetHash(), hash16k.print().c_str()))
		{
			debug("Found correct filename: %s", fileHash.GetFilename());
			fileHash.SetFileExists(true);

			BString<1024> dstFilename("%s%c%s", destDir, PATH_SEPARATOR, fileHash.GetFilename());

			if (!FileSystem::FileExists(dstFilename) && !IsSplittedFragment(filename, fileHash.GetFilename()))
			{
				RenameFile(filename, dstFilename);
			}

			break;
		}
	}
}

/*
* For files not having par2-extensions: checks if the file is a par2-file and renames
* it according to its set-id.
*/
void ParRenamer::CheckParFile(const char* destDir, const char* filename)
{
	debug("Checking par2-header for %s", filename);

	const char* basename = FileSystem::BaseFileName(filename);
	const char* extension = strrchr(basename, '.');
	if (extension && !strcasecmp(extension, ".par2"))
	{
		// do not process files already having par2-extension
		return;
	}

	DiskFile file;
	if (!file.Open(filename, DiskFile::omRead))
	{
		PrintMessage(Message::mkError, "Could not open file %s", filename);
		return;
	}

	// load par2-header
	Par2::PACKET_HEADER header;

	int readBytes = (int)file.Read(&header, sizeof(header));
	if (readBytes != sizeof(header) && file.Error())
	{
		PrintMessage(Message::mkError, "Could not read file %s", filename);
		return;
	}

	file.Close();

	// Check the packet header
	if (Par2::packet_magic != header.magic ||          // not par2-file
		sizeof(Par2::PACKET_HEADER) > header.length || // packet length is too small
		0 != (header.length & 3) ||              // packet length is not a multiple of 4
		FileSystem::FileSize(filename) < (int)header.length)       // packet would extend beyond the end of the file
	{
		// not par2-file or damaged header, ignoring the file
		return;
	}

	BString<100> setId = header.setid.print().c_str();
	for (char* p = setId; *p; p++) *p = tolower(*p); // convert string to lowercase

	debug("Renaming: %s; setid: %s", FileSystem::BaseFileName(filename), *setId);

	BString<1024> destFileName;
	int num = 1;
	while (num == 1 || FileSystem::FileExists(destFileName))
	{
		destFileName.Format("%s%c%s.vol%03i+01.PAR2", destDir, PATH_SEPARATOR, *setId, num);
		num++;
	}

	RenameFile(filename, destFileName);
}

void ParRenamer::RenameFile(const char* srcFilename, const char* destFileName)
{
	PrintMessage(Message::mkInfo, "Renaming %s to %s", FileSystem::BaseFileName(srcFilename), FileSystem::BaseFileName(destFileName));
	if (!FileSystem::MoveFile(srcFilename, destFileName))
	{
		PrintMessage(Message::mkError, "Could not rename %s to %s: %s", srcFilename, destFileName,
			*FileSystem::GetLastErrorMessage());
		return;
	}

	m_renamedCount++;

	// notify about new file name
	RegisterRenamedFile(FileSystem::BaseFileName(srcFilename), FileSystem::BaseFileName(destFileName));
}

#endif
