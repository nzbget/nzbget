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
	ParRenamerRepairer() : Par2::Par2Repairer(m_nout, m_nout) {};
	friend class ParRenamer;
private:
	class NullStreamBuf : public std::streambuf {};
	NullStreamBuf m_nullbuf;
	std::ostream m_nout{&m_nullbuf};
};


void ParRenamer::Execute()
{
	m_progressLabel.Format("Checking renamed files for %s", *m_infoName);
	m_stageProgress = 0;
	UpdateProgress();

	BuildDirList(m_destDir);

	for (CString& destDir : m_dirList)
	{
		debug("Checking %s", *destDir);
		m_fileHashList.clear();
		m_parInfoList.clear();
		m_badParList.clear();
		m_loadedParList.clear();

		CheckFiles(destDir, true);
		RenameParFiles(destDir);

		LoadMainParFiles(destDir);
		if (m_hasDamagedParFiles)
		{
			LoadExtraParFiles(destDir);
		}

		CheckFiles(destDir, false);

		if (m_detectMissing)
		{
			CheckMissing();
		}

		if (m_renamedCount > 0 && !m_badParList.empty())
		{
			RenameBadParFiles();
		}
	}
}

void ParRenamer::BuildDirList(const char* destDir)
{
	m_dirList.push_back(destDir);

	DirBrowser dirBrowser(destDir);

	while (const char* filename = dirBrowser.Next())
	{
		if (!IsStopped())
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

void ParRenamer::LoadMainParFiles(const char* destDir)
{
	ParParser::ParFileList parFileList;
	ParParser::FindMainPars(destDir, &parFileList);

	for (CString& parFilename : parFileList)
	{
		BString<1024> fullParFilename("%s%c%s", destDir, PATH_SEPARATOR, *parFilename);
		LoadParFile(fullParFilename);
	}
}

void ParRenamer::LoadExtraParFiles(const char* destDir)
{
	DirBrowser dir(destDir);
	while (const char* filename = dir.Next())
	{
		BString<1024> fullParFilename("%s%c%s", destDir, PATH_SEPARATOR, filename);
		if (ParParser::ParseParFilename(fullParFilename, true, nullptr, nullptr))
		{
			bool knownBadParFile = std::find_if(m_badParList.begin(), m_badParList.end(),
				[&fullParFilename](CString& filename)
				{
					return !strcmp(filename, fullParFilename);
				}) != m_badParList.end();

			bool loadedParFile = std::find_if(m_loadedParList.begin(), m_loadedParList.end(),
				[&fullParFilename](CString& filename)
				{
					return !strcmp(filename, fullParFilename);
				}) != m_loadedParList.end();

			if (!knownBadParFile && !loadedParFile)
			{
				LoadParFile(fullParFilename);
			}
		}
	}
}

void ParRenamer::LoadParFile(const char* parFilename)
{
	ParRenamerRepairer repairer;

	if (!repairer.LoadPacketsFromFile(parFilename) || FileSystem::FileSize(parFilename) == 0)
	{
		PrintMessage(Message::mkWarning, "Could not load par2-file %s", parFilename);
		m_hasDamagedParFiles = true;
		m_badParList.emplace_back(parFilename);
		return;
	}

	m_loadedParList.emplace_back(parFilename);
	PrintMessage(Message::mkInfo, "Loaded par2-file %s for par-rename", FileSystem::BaseFileName(parFilename));

	for (std::pair<const Par2::MD5Hash, Par2::Par2RepairerSourceFile*>& entry : repairer.sourcefilemap)
	{
		if (IsStopped())
		{
			break;
		}

		Par2::Par2RepairerSourceFile* sourceFile = entry.second;
		if (!sourceFile || !sourceFile->GetDescriptionPacket())
		{
			PrintMessage(Message::mkWarning, "Damaged par2-file detected: %s", FileSystem::BaseFileName(parFilename));
			m_badParList.emplace_back(parFilename);
			m_hasDamagedParFiles = true;
			continue;
		}
		std::string filename = Par2::DiskFile::TranslateFilename(sourceFile->GetDescriptionPacket()->FileName());
		std::string hash = sourceFile->GetDescriptionPacket()->Hash16k().print();

		bool exists = std::find_if(m_fileHashList.begin(), m_fileHashList.end(),
			[&hash](FileHash& fileHash)
			{
				return !strcmp(fileHash.GetHash(), hash.c_str());
			})
			!= m_fileHashList.end();

		if (!exists)
		{
			m_fileHashList.emplace_back(filename.c_str(), hash.c_str());
			RegisterParredFile(filename.c_str());
		}
	}
}

void ParRenamer::CheckFiles(const char* destDir, bool checkPars)
{
	DirBrowser dir(destDir);
	while (const char* filename = dir.Next())
	{
		if (!IsStopped())
		{
			BString<1024> fullFilename("%s%c%s", destDir, PATH_SEPARATOR, filename);

			if (!FileSystem::DirectoryExists(fullFilename))
			{
				m_progressLabel.Format("Checking file %s", filename);
				m_stageProgress = m_fileCount > 0 ? m_curFile * 1000 / m_fileCount / 2 : 1000;
				UpdateProgress();
				m_curFile++;

				if (checkPars)
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

void ParRenamer::CheckParFile(const char* destDir, const char* filename)
{
	debug("Checking par2-header for %s", filename);

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

	debug("Storing: %s; setid: %s", FileSystem::BaseFileName(filename), *setId);

	m_parInfoList.emplace_back(filename, setId);
}

void ParRenamer::RenameParFiles(const char* destDir)
{
	if (NeedRenameParFiles())
	{
		for (ParInfo& parInfo : m_parInfoList)
		{
			RenameParFile(destDir, parInfo.GetFilename(), parInfo.GetSetId());
		}
	}
}

bool ParRenamer::NeedRenameParFiles()
{
	for (ParInfoList::iterator it1 = m_parInfoList.begin(); it1 != m_parInfoList.end(); it1++)
	{
		ParInfo& parInfo1 = *it1;

		const char* baseName1 = FileSystem::BaseFileName(parInfo1.GetFilename());

		const char* extension = strrchr(baseName1, '.');
		if (!extension || strcasecmp(extension, ".par2"))
		{
			// file doesn't have "par2" extension
			return true;
		}

		int baseLen1;
		ParParser::ParseParFilename(baseName1, true, &baseLen1, nullptr);

		for (ParInfoList::iterator it2 = it1 + 1; it2 != m_parInfoList.end(); it2++)
		{
			ParInfo& parInfo2 = *it2;

			if (!strcmp(parInfo1.GetSetId(), parInfo2.GetSetId()))
			{
				const char* baseName2 = FileSystem::BaseFileName(parInfo2.GetFilename());
				int baseLen2;
				ParParser::ParseParFilename(baseName2, true, &baseLen2, nullptr);
				if (baseLen1 != baseLen2 || strncasecmp(baseName1, baseName2, baseLen1))
				{
					// same setid but different base file names
					return true;
				}
			}
		}
	}

	return false;
}

void ParRenamer::RenameParFile(const char* destDir, const char* filename, const char* setId)
{
	debug("Renaming: %s; setid: %s", FileSystem::BaseFileName(filename), setId);

	BString<1024> destFileName;
	int num = 1;
	while (num == 1 || FileSystem::FileExists(destFileName))
	{
		destFileName.Format("%s%c%s.vol%03i+01.PAR2", destDir, PATH_SEPARATOR, setId, num);
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

void ParRenamer::RenameBadParFiles()
{
	for (CString& parFilename : m_badParList)
	{
		BString<1024> destFileName("%s.bad", *parFilename);
		RenameFile(parFilename, destFileName);
	}
}

#endif
