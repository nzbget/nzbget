/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2016-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

				CheckOneFile(fullFilename);
			}
		}
	}

	if (!m_volumes.empty())
	{
		RenameFiles(destDir);
	}
}

void RarRenamer::CheckOneFile(const char* filename)
{
	if (m_ignoreExt && Util::MatchFileExt(FileSystem::BaseFileName(filename), m_ignoreExt, ",;"))
	{
		return;
	}
	
	RarVolume volume(filename);
	volume.SetPassword(m_password);
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
	MakeSets();

	for (RarVolumeSet& set : m_sets)
	{
		if (!IsSetProperlyNamed(set))
		{
			RarFile* mainFile = FindMainFile(set);
			BString<1024> mainBasename = FileSystem::BaseFileName(mainFile->GetFilename());
			char* ext = strrchr(mainBasename, '.');
			// strip extension if its length is 3 chars
			if (ext && strlen(ext) == 4)
			{
				*ext = '\0';
			}

			BString<1024> newBasename = *mainBasename;
			int num = 0;
			bool willOverwrite = true;
			while (willOverwrite)
			{
				if (num++)
				{
					newBasename.Format("%s-%i", *mainBasename, num);
				}

				for (RarVolume* volume : set)
				{
					CString destfilename = GenNewVolumeFilename(destDir, newBasename, volume);
					willOverwrite = strcmp(volume->GetFilename(), destfilename) && FileSystem::FileExists(destfilename);
					if (willOverwrite)
					{
						break;
					}
				}
			}

			for (RarVolume* volume : set)
			{
				CString destfilename = GenNewVolumeFilename(destDir, newBasename, volume);
				if (strcmp(volume->GetFilename(), destfilename))
				{
					RenameFile(volume->GetFilename(), destfilename);
				}
			}
		}
	}
}

CString RarRenamer::GenNewVolumeFilename(const char* destDir, const char* newBasename, RarVolume* volume)
{
	CString extension = volume->GetNewNaming() ? GenNewExtension(volume->GetVolumeNo()) : GenOldExtension(volume->GetVolumeNo());
	return CString::FormatStr("%s%c%s.%s", destDir, PATH_SEPARATOR, newBasename, *extension);
}

CString RarRenamer::GenNewExtension(int volumeNo)
{
	return CString::FormatStr("part%04i.rar", volumeNo + 1);
}

CString RarRenamer::GenOldExtension(int volumeNo)
{
	if (volumeNo == 0)
	{
		return "rar";
	}
	else
	{
		unsigned char ch = 'r' + (volumeNo - 1) / 100;
		return CString::FormatStr("%c%02d", ch, (volumeNo - 1) % 100);
	}
}

void RarRenamer::MakeSets()
{
	m_sets.clear();

	// find first volumes and create initial incomplete sets
	for (RarVolume& volume : m_volumes)
	{
		if (!volume.GetFiles()->empty() && volume.GetVolumeNo() == 0 &&
			!volume.GetFiles()->front().GetSplitBefore())
		{
			m_sets.push_back({&volume});
		}
	}

	// complete sets, discard sets which cannot be completed
	m_sets.erase(std::remove_if(m_sets.begin(), m_sets.end(),
		[volumes = &m_volumes](RarVolumeSet& set)
		{
			debug("*** Building set %s", FileSystem::BaseFileName(set[0]->GetFilename()));
			bool found = true;
			while (found)
			{
				found = false;
				std::vector<RarVolume*> candidates;

				RarVolume* lastVolume = set.back();
				for (RarVolume& volume : *volumes)
				{
					if (!volume.GetFiles()->empty() && volume.GetMultiVolume() &&
						volume.GetVolumeNo() == lastVolume->GetVolumeNo() + 1 &&
						volume.GetVersion() == lastVolume->GetVersion() &&
						lastVolume->GetHasNextVolume() &&
						((volume.GetFiles()->front().GetSplitBefore() &&
						 lastVolume->GetFiles()->back().GetSplitAfter() &&
						 !strcmp(volume.GetFiles()->front().GetFilename(), lastVolume->GetFiles()->back().GetFilename())) ||
						 (!volume.GetFiles()->front().GetSplitBefore() && !lastVolume->GetFiles()->back().GetSplitAfter())))
					{
						debug("   found candidate %s", FileSystem::BaseFileName(volume.GetFilename()));
						candidates.push_back(&volume);
					}
				}

				RarVolume* nextVolume = nullptr;

				if (candidates.size() > 1)
				{
					for (RarVolume* volume : candidates)
					{
						if (SameArchiveName(FileSystem::BaseFileName(set[0]->GetFilename()),
							FileSystem::BaseFileName(volume->GetFilename()), set[0]->GetNewNaming()))
						{
							nextVolume = volume;
							break;
						}
					}
				}

				if (!nextVolume && !candidates.empty())
				{
					nextVolume = candidates.front();
				}

				if (nextVolume)
				{
					debug("   adding %s", FileSystem::BaseFileName(nextVolume->GetFilename()));
					set.push_back(nextVolume);
					found = true;
				}
			}

			RarVolume* lastVolume = set.back();
			bool completed = !lastVolume->GetHasNextVolume() &&
				(lastVolume->GetFiles()->empty() || !lastVolume->GetFiles()->back().GetSplitAfter());

			return !completed;
		}),
		m_sets.end());

#ifdef DEBUG
	// debug log
	for (RarVolumeSet& set : m_sets)
	{
		debug("*** Set ***");
		for (RarVolume* volume : set)
		{
			debug("   %s", FileSystem::BaseFileName(volume->GetFilename()));
		}
	}
#endif
}

bool RarRenamer::SameArchiveName(const char* filename1, const char* filename2, bool newNaming)
{
	if (strlen(filename1) != strlen(filename2))
	{
		return false;
	}

	const char* ext1 = strrchr(filename1, '.');
	const char* ext2 = strrchr(filename2, '.');

	if (!(ext1 && ext2 && strlen(ext1) == strlen(ext2)))
	{
		return false;
	}

	if (newNaming)
	{
		if (ext1 == filename1 || ext2 == filename2)
		{
			return false;
		}
		BString<1024> name1, name2;
		name1.Set(filename1, (int)(ext1 - filename1));
		name2.Set(filename2, (int)(ext2 - filename2));
		ext1 = strrchr(name1, '.');
		ext2 = strrchr(name2, '.');
		return ext1 && ext2 && strlen(ext1) == strlen(ext2) &&
			!strncmp(ext1, ".part", 5) && !strncmp(ext2, ".part", 5) &&
			!strncmp(name1, name2, ext1 - name1);
	}
	else
	{
		return !strncmp(filename1, filename2, ext1 - filename1);
	}
}

bool RarRenamer::IsSetProperlyNamed(RarVolumeSet& set)
{
	RegEx regExPart(".*.part([0-9]+)\\.rar$");

	const char* setBasename = FileSystem::BaseFileName(set[0]->GetFilename());
	int setPartLen = 0;
	for (RarVolume* volume : set)
	{
		const char* filename = FileSystem::BaseFileName(volume->GetFilename());

		if (strlen(setBasename) != strlen(filename))
		{
			return false;
		}

		if (volume->GetNewNaming())
		{
			if (!regExPart.Match(filename))
			{
				return false;
			}
			BString<1024> partNo(filename + regExPart.GetMatchStart(1), regExPart.GetMatchLen(1));
			if (setPartLen == 0)
			{
				setPartLen = partNo.Length();
			}
			bool ok = (uint32)atoi(partNo) == volume->GetVolumeNo() + 1 &&
				partNo.Length() == setPartLen &&
				!strncmp(setBasename, filename, regExPart.GetMatchStart(1));
			if (!ok)
			{
				return false;
			}
		}
		else
		{
			const char* ext = strrchr(filename, '.');
			if (!ext || strcmp(ext + 1, GenOldExtension(volume->GetVolumeNo())) ||
				strncmp(setBasename, filename, ext - filename))
			{
				return false;
			}
		}
	}

	return true;
}

RarFile* RarRenamer::FindMainFile(RarVolumeSet& set)
{
	std::deque<RarFile*> allFiles;

	for (RarVolume* volume : set)
	{
		for (RarFile& file : *volume->GetFiles())
		{
			allFiles.push_back(&file);
		}

	}

	std::deque<RarFile*>::iterator it = std::max_element(allFiles.begin(), allFiles.end(),
		[](RarFile* file1, RarFile* file2)
		{
			return file1->GetSize() < file2->GetSize();
		});

	return *it;
}
