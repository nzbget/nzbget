/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include "RarRenamer.h"
#include "Log.h"
#include "Options.h"
#include "Util.h"
#include "FileSystem.h"

void RarRenamer::Execute()
{
	m_progressLabel.Format("Checking renamed rar-files for %s", *m_infoName);
	m_stageProgress = 0;
	UpdateProgress();

	BuildDirList(m_destDir);

	for (CString& destDir : m_dirList)
	{
		debug("Checking %s", *destDir);
		CheckFiles(destDir);
	}
}

void RarRenamer::BuildDirList(const char* destDir)
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

void RarRenamer::CheckFiles(const char* destDir)
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
				m_stageProgress = m_fileCount > 0 ? m_curFile * 1000 / m_fileCount : 1000;
				UpdateProgress();
				m_curFile++;

				CheckRegularFile(destDir, fullFilename);
			}
		}
	}

	if (!m_volumes.empty())
	{
		RenameFiles(destDir);
	}
}

void RarRenamer::CheckRegularFile(const char* destDir, const char* filename)
{
	RarVolume volume(filename);
	if (volume.Read())
	{
		m_volumes.push_back(std::move(volume));
	}
}

void RarRenamer::RenameFile(const char* srcFilename, const char* destFileName)
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

void RarRenamer::RenameFiles(const char* destDir)
{
	BString<1024> newBasename;
	for (RarVolume& volume : m_volumes)
	{
		if (!volume.GetFiles()->empty())
		{
			newBasename = FileSystem::BaseFileName(volume.GetFiles()->at(0).GetFilename());
			break;
		}
	}

	if (newBasename.Empty())
	{
		PrintMessage(Message::mkError, "Could not determine base archive name for %s", *m_infoName);
		return;
	}

	RegEx regExRar(".*\\.rar$");
	RegEx regExRarMultiSeq(".*\\.[r-z][0-9][0-9]$");

	for (RarVolume& volume : m_volumes)
	{
		const char* baseFilename = FileSystem::BaseFileName(volume.GetFilename());
		if (!regExRar.Match(baseFilename) && !regExRarMultiSeq.Match(baseFilename))
		{
			BString<1024> newname("%s%c%s.part%03i.rar", destDir, PATH_SEPARATOR, *newBasename, volume.GetVolumeNo() + 1);
			RenameFile(volume.GetFilename(), newname);
		}
	}
}
