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
#include "DownloadInfo.h"
#include "QueueEditor.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"
#include "FileSystem.h"
#include "QueueCoordinator.h"
#include "PrePostProcessor.h"
#include "HistoryCoordinator.h"
#include "UrlCoordinator.h"
#include "ParParser.h"

const int MAX_ID = 1000000000;

class GroupSorter
{
public:
	GroupSorter(NzbList* nzbList, QueueEditor::ItemList* sortItemList) :
		m_nzbList(nzbList), m_sortItemList(sortItemList) {}
	bool Execute(const char* sort);
	bool operator()(const std::unique_ptr<NzbInfo>& refNzbInfo1, const std::unique_ptr<NzbInfo>& refNzbInfo2) const;

private:
	enum ESortCriteria
	{
		scName,
		scSize,
		scRemainingSize,
		scAge,
		scCategory,
		scPriority
	};

	enum ESortOrder
	{
		soAscending,
		soDescending,
		soAuto
	};

	NzbList* m_nzbList;
	QueueEditor::ItemList* m_sortItemList;
	ESortCriteria m_sortCriteria;
	ESortOrder m_sortOrder;
};

bool GroupSorter::Execute(const char* sort)
{
	if (!strcasecmp(sort, "name") || !strcasecmp(sort, "name+") || !strcasecmp(sort, "name-"))
	{
		m_sortCriteria = scName;
	}
	else if (!strcasecmp(sort, "size") || !strcasecmp(sort, "size+") || !strcasecmp(sort, "size-"))
	{
		m_sortCriteria = scSize;
	}
	else if (!strcasecmp(sort, "left") || !strcasecmp(sort, "left+") || !strcasecmp(sort, "left-"))
	{
		m_sortCriteria = scRemainingSize;
	}
	else if (!strcasecmp(sort, "age") || !strcasecmp(sort, "age+") || !strcasecmp(sort, "age-"))
	{
		m_sortCriteria = scAge;
	}
	else if (!strcasecmp(sort, "category") || !strcasecmp(sort, "category+") || !strcasecmp(sort, "category-"))
	{
		m_sortCriteria = scCategory;
	}
	else if (!strcasecmp(sort, "priority") || !strcasecmp(sort, "priority+") || !strcasecmp(sort, "priority-"))
	{
		m_sortCriteria = scPriority;
	}
	else
	{
		error("Could not sort groups: incorrect sort order (%s)", sort);
		return false;
	}

	char lastCh = sort[strlen(sort) - 1];
	if (lastCh == '+')
	{
		m_sortOrder = soAscending;
	}
	else if (lastCh == '-')
	{
		m_sortOrder = soDescending;
	}
	else
	{
		m_sortOrder = soAuto;
	}

	RawNzbList tempList;
	for (NzbInfo* nzbInfo : m_nzbList)
	{
		tempList.push_back(nzbInfo);
	}

	ESortOrder origSortOrder = m_sortOrder;
	if (m_sortOrder == soAuto && m_sortCriteria == scPriority)
	{
		m_sortOrder = soDescending;
	}

	std::stable_sort(m_nzbList->begin(), m_nzbList->end(), *this);

	if (origSortOrder == soAuto &&
		std::equal(tempList.begin(), tempList.end(), m_nzbList->begin(),
		[](NzbInfo* nzbInfo1, std::unique_ptr<NzbInfo>& nzbInfo2)
		{
			return nzbInfo1 == nzbInfo2.get();
		}))
	{
		m_sortOrder = m_sortOrder == soDescending ? soAscending : soDescending;
		std::stable_sort(m_nzbList->begin(), m_nzbList->end(), *this);
	} 

	return true;
}

bool GroupSorter::operator()(const std::unique_ptr<NzbInfo>& refNzbInfo1, const std::unique_ptr<NzbInfo>& refNzbInfo2) const
{
	NzbInfo* nzbInfo1 = refNzbInfo1.get();
	NzbInfo* nzbInfo2 = refNzbInfo2.get();

	// if list of ID is empty - sort all items
	bool sortItem1 = m_sortItemList->empty();
	bool sortItem2 = m_sortItemList->empty();

	for (QueueEditor::EditItem& item : m_sortItemList)
	{
		sortItem1 |= item.m_nzbInfo == nzbInfo1;
		sortItem2 |= item.m_nzbInfo == nzbInfo2;
	}

	if (!sortItem1 || !sortItem2)
	{
		return false;
	}

	bool ret = false;

	if (m_sortOrder == soDescending)
	{
		std::swap(nzbInfo1, nzbInfo2);
	}

	switch (m_sortCriteria)
	{
		case scName:
			ret = strcmp(nzbInfo1->GetName(), nzbInfo2->GetName()) < 0;
			break;

		case scSize:
			ret = nzbInfo1->GetSize() < nzbInfo2->GetSize();
			break;

		case scRemainingSize:
			ret = nzbInfo1->GetRemainingSize() - nzbInfo1->GetPausedSize() <
				nzbInfo2->GetRemainingSize() - nzbInfo2->GetPausedSize();
			break;

		case scAge:
			ret = nzbInfo1->GetMinTime() > nzbInfo2->GetMinTime();
			break;

		case scCategory:
			ret = strcmp(nzbInfo1->GetCategory(), nzbInfo2->GetCategory()) < 0;
			break;

		case scPriority:
			ret = nzbInfo1->GetPriority() < nzbInfo2->GetPriority();
			break;
	}

	return ret;
}


FileInfo* QueueEditor::FindFileInfo(int id)
{
	for (NzbInfo* nzbInfo : m_downloadQueue->GetQueue())
	{
		FileInfo* fileInfo = nzbInfo->GetFileList()->Find(id);
		if (fileInfo)
		{
			return fileInfo;
		}
	}
	return nullptr;
}

void QueueEditor::PauseUnpauseEntry(FileInfo* fileInfo, bool pause)
{
	fileInfo->SetPaused(pause);
}

void QueueEditor::DeleteEntry(FileInfo* fileInfo)
{
	if (!fileInfo->GetDeleted())
	{
		fileInfo->GetNzbInfo()->PrintMessage(
			fileInfo->GetNzbInfo()->GetDeleting() ? Message::mkDetail : Message::mkInfo,
			"Deleting file %s from download queue", fileInfo->GetFilename());
		g_QueueCoordinator->DeleteQueueEntry(m_downloadQueue, fileInfo);
	}
}

void QueueEditor::MoveEntry(FileInfo* fileInfo, int offset)
{
	int entry = 0;
	for (FileInfo* fileInfo2 : fileInfo->GetNzbInfo()->GetFileList())
	{
		if (fileInfo2 == fileInfo)
		{
			break;
		}
		entry++;
	}

	int newEntry = entry + offset;
	int size = (int)fileInfo->GetNzbInfo()->GetFileList()->size();

	if (newEntry < 0)
	{
		newEntry = 0;
	}
	if (newEntry > size - 1)
	{
		newEntry = (int)size - 1;
	}

	if (newEntry >= 0 && newEntry <= size - 1)
	{
		std::unique_ptr<FileInfo> movedFileInfo = std::move(*(fileInfo->GetNzbInfo()->GetFileList()->begin() + entry));
		fileInfo->GetNzbInfo()->GetFileList()->erase(fileInfo->GetNzbInfo()->GetFileList()->begin() + entry);
		fileInfo->GetNzbInfo()->GetFileList()->insert(fileInfo->GetNzbInfo()->GetFileList()->begin() + newEntry, std::move(movedFileInfo));
	}
}

void QueueEditor::MoveGroup(NzbInfo* nzbInfo, int offset)
{
	int entry = 0;
	for (NzbInfo* nzbInfo2 : m_downloadQueue->GetQueue())
	{
		if (nzbInfo2 == nzbInfo)
		{
			break;
		}
		entry++;
	}

	int newEntry = entry + offset;
	int size = (int)m_downloadQueue->GetQueue()->size();

	if (newEntry < 0)
	{
		newEntry = 0;
	}
	if (newEntry > size - 1)
	{
		newEntry = (int)size - 1;
	}

	if (newEntry >= 0 && newEntry <= size - 1)
	{
		std::unique_ptr<NzbInfo> movedNzbInfo = std::move(*(m_downloadQueue->GetQueue()->begin() + entry));
		m_downloadQueue->GetQueue()->erase(m_downloadQueue->GetQueue()->begin() + entry);
		m_downloadQueue->GetQueue()->insert(m_downloadQueue->GetQueue()->begin() + newEntry, std::move(movedNzbInfo));
	}
}

bool QueueEditor::EditEntry(DownloadQueue* downloadQueue, int ID, DownloadQueue::EEditAction action, const char* args)
{
	m_downloadQueue = downloadQueue;
	IdList cIdList;
	cIdList.push_back(ID);
	return InternEditList(nullptr, &cIdList, action, args);
}

bool QueueEditor::EditList(DownloadQueue* downloadQueue, IdList* idList, NameList* nameList, DownloadQueue::EMatchMode matchMode,
	DownloadQueue::EEditAction action, const char* args)
{
	if (action == DownloadQueue::eaPostDelete)
	{
		return g_PrePostProcessor->EditList(downloadQueue, idList, action, args);
	}
	else if (DownloadQueue::eaHistoryDelete <= action && action <= DownloadQueue::eaHistorySetName)
	{
		return g_HistoryCoordinator->EditList(downloadQueue, idList, action, args);
	}

	m_downloadQueue = downloadQueue;
	bool ok = true;

	std::unique_ptr<IdList> nameIdList;

	if (nameList)
	{
		nameIdList = std::make_unique<IdList>();
		idList = nameIdList.get();
		ok = BuildIdListFromNameList(idList, nameList, matchMode, action);
	}

	ok = ok && (InternEditList(nullptr, idList, action, args) || matchMode == DownloadQueue::mmRegEx);

	m_downloadQueue->Save();

	return ok;
}

bool QueueEditor::InternEditList(ItemList* itemList,
	IdList* idList, DownloadQueue::EEditAction action, const char* args)
{
	ItemList workItems;
	if (!itemList)
	{
		itemList = &workItems;
		int offset = args && (action == DownloadQueue::eaFileMoveOffset ||
			action == DownloadQueue::eaGroupMoveOffset) ? atoi(args) : 0;
		PrepareList(itemList, idList, action, offset);
	}

	switch (action)
	{
		case DownloadQueue::eaFilePauseAllPars:
		case DownloadQueue::eaFilePauseExtraPars:
			PauseParsInGroups(itemList, action == DownloadQueue::eaFilePauseExtraPars);
			break;

		case DownloadQueue::eaGroupMerge:
			return MergeGroups(itemList);

		case DownloadQueue::eaGroupSort:
			return SortGroups(itemList, args);

		case DownloadQueue::eaGroupMoveAfter:
		case DownloadQueue::eaGroupMoveBefore:
			return MoveGroupsTo(itemList, idList, action == DownloadQueue::eaGroupMoveBefore, args);

		case DownloadQueue::eaFileSplit:
			return SplitGroup(itemList, args);

		case DownloadQueue::eaFileReorder:
			ReorderFiles(itemList);
			break;

		default:
			for (EditItem& item : itemList)
			{
				switch (action)
				{
					case DownloadQueue::eaFilePause:
						PauseUnpauseEntry(item.m_fileInfo, true);
						break;

					case DownloadQueue::eaFileResume:
						PauseUnpauseEntry(item.m_fileInfo, false);
						break;

					case DownloadQueue::eaFileMoveOffset:
					case DownloadQueue::eaFileMoveTop:
					case DownloadQueue::eaFileMoveBottom:
						MoveEntry(item.m_fileInfo, item.m_offset);
						break;

					case DownloadQueue::eaFileDelete:
						DeleteEntry(item.m_fileInfo);
						break;

					case DownloadQueue::eaGroupSetPriority:
						SetNzbPriority(item.m_nzbInfo, args);
						break;

					case DownloadQueue::eaGroupSetCategory:
					case DownloadQueue::eaGroupApplyCategory:
						SetNzbCategory(item.m_nzbInfo, args, action == DownloadQueue::eaGroupApplyCategory);
						break;

					case DownloadQueue::eaGroupSetName:
						SetNzbName(item.m_nzbInfo, args);
						break;

					case DownloadQueue::eaGroupSetDupeKey:
					case DownloadQueue::eaGroupSetDupeScore:
					case DownloadQueue::eaGroupSetDupeMode:
						SetNzbDupeParam(item.m_nzbInfo, action, args);
						break;

					case DownloadQueue::eaGroupSetParameter:
						SetNzbParameter(item.m_nzbInfo, args);
						break;

					case DownloadQueue::eaGroupMoveTop:
					case DownloadQueue::eaGroupMoveBottom:
					case DownloadQueue::eaGroupMoveOffset:
						MoveGroup(item.m_nzbInfo, item.m_offset);
						break;

					case DownloadQueue::eaGroupPause:
					case DownloadQueue::eaGroupResume:
					case DownloadQueue::eaGroupPauseAllPars:
					case DownloadQueue::eaGroupPauseExtraPars:
						EditGroup(item.m_nzbInfo, action, args);
						break;

					case DownloadQueue::eaGroupDelete:
					case DownloadQueue::eaGroupParkDelete:
					case DownloadQueue::eaGroupDupeDelete:
					case DownloadQueue::eaGroupFinalDelete:
						if (item.m_nzbInfo->GetKind() == NzbInfo::nkUrl)
						{
							DeleteUrl(item.m_nzbInfo, action);
						}
						else
						{
							EditGroup(item.m_nzbInfo, action, args);
						}
						break;

					case DownloadQueue::eaGroupSortFiles:
						SortGroupFiles(item.m_nzbInfo);
						break;

					default:
						// suppress compiler warning "enumeration not handled in switch"
						break;
				}
			}
	}

	return itemList->size() > 0;
}

void QueueEditor::PrepareList(ItemList* itemList, IdList* idList,
	DownloadQueue::EEditAction action, int offset)
{
	if (action == DownloadQueue::eaFileMoveTop || action == DownloadQueue::eaGroupMoveTop)
	{
		offset = -MAX_ID;
	}
	else if (action == DownloadQueue::eaFileMoveBottom || action == DownloadQueue::eaGroupMoveBottom)
	{
		offset = MAX_ID;
	}

	itemList->reserve(idList->size());
	if ((offset != 0) &&
		(action == DownloadQueue::eaFileMoveOffset || action == DownloadQueue::eaFileMoveTop || action == DownloadQueue::eaFileMoveBottom))
	{
		// add IDs to list in order they currently have in download queue
		for (NzbInfo* nzbInfo : m_downloadQueue->GetQueue())
		{
			int nrEntries = (int)nzbInfo->GetFileList()->size();
			int lastDestPos = -1;
			int start, end, step;
			if (offset < 0)
			{
				start = 0;
				end = nrEntries;
				step = 1;
			}
			else
			{
				start = nrEntries - 1;
				end = -1;
				step = -1;
			}
			for (int index = start; index != end; index += step)
			{
				std::unique_ptr<FileInfo>& fileInfo = nzbInfo->GetFileList()->at(index);
				IdList::iterator it2 = std::find(idList->begin(), idList->end(), fileInfo->GetId());
				if (it2 != idList->end())
				{
					int workOffset = offset;
					int destPos = index + workOffset;
					if (lastDestPos == -1)
					{
						if (destPos < 0)
						{
							workOffset = -index;
						}
						else if (destPos > nrEntries - 1)
						{
							workOffset = nrEntries - 1 - index;
						}
					}
					else
					{
						if (workOffset < 0 && destPos <= lastDestPos)
						{
							workOffset = lastDestPos - index + 1;
						}
						else if (workOffset > 0 && destPos >= lastDestPos)
						{
							workOffset = lastDestPos - index - 1;
						}
					}
					lastDestPos = index + workOffset;
					itemList->emplace_back(fileInfo.get(), nullptr, workOffset);
				}
			}
		}
	}
	else if (((offset != 0) &&
		(action == DownloadQueue::eaGroupMoveOffset || action == DownloadQueue::eaGroupMoveTop || action == DownloadQueue::eaGroupMoveBottom)) ||
		action == DownloadQueue::eaGroupMoveBefore || action == DownloadQueue::eaGroupMoveAfter)
	{
		// add IDs to list in order they currently have in download queue
		int nrEntries = (int)m_downloadQueue->GetQueue()->size();
		int lastDestPos = -1;
		int start, end, step;
		if (offset <= 0)
		{
			start = 0;
			end = nrEntries;
			step = 1;
		}
		else
		{
			start = nrEntries - 1;
			end = -1;
			step = -1;
		}
		for (int index = start; index != end; index += step)
		{
			std::unique_ptr<NzbInfo>& nzbInfo = m_downloadQueue->GetQueue()->at(index);
			IdList::iterator it2 = std::find(idList->begin(), idList->end(), nzbInfo->GetId());
			if (it2 != idList->end())
			{
				int workOffset = offset;
				int destPos = index + workOffset;
				if (lastDestPos == -1)
				{
					if (destPos < 0)
					{
						workOffset = -index;
					}
					else if (destPos > nrEntries - 1)
					{
						workOffset = nrEntries - 1 - index;
					}
				}
				else
				{
					if (workOffset < 0 && destPos <= lastDestPos)
					{
						workOffset = lastDestPos - index + 1;
					}
					else if (workOffset > 0 && destPos >= lastDestPos)
					{
						workOffset = lastDestPos - index - 1;
					}
				}
				lastDestPos = index + workOffset;
				itemList->emplace_back(nullptr, nzbInfo.get(), workOffset);
			}
		}
	}
	else if (action < DownloadQueue::eaGroupMoveOffset)
	{
		// check ID range
		int maxId = 0;
		int minId = MAX_ID;
		for (NzbInfo* nzbInfo : m_downloadQueue->GetQueue())
		{
			for (FileInfo* fileInfo : nzbInfo->GetFileList())
			{
				int ID = fileInfo->GetId();
				if (ID > maxId)
				{
					maxId = ID;
				}
				if (ID < minId)
				{
					minId = ID;
				}
			}
		}

		//add IDs to list in order they were transmitted in command
		for (int id : *idList)
		{
			if (minId <= id && id <= maxId)
			{
				FileInfo* fileInfo = FindFileInfo(id);
				if (fileInfo)
				{
					itemList->emplace_back(fileInfo, nullptr, offset);
				}
			}
		}
	}
	else
	{
		// check ID range
		int maxId = 0;
		int minId = MAX_ID;
		for (NzbInfo* nzbInfo : m_downloadQueue->GetQueue())
		{
			int ID = nzbInfo->GetId();
			if (ID > maxId)
			{
				maxId = ID;
			}
			if (ID < minId)
			{
				minId = ID;
			}
		}

		//add IDs to list in order they were transmitted in command
		for (int id : *idList)
		{
			if (minId <= id && id <= maxId)
			{
				for (NzbInfo* nzbInfo : m_downloadQueue->GetQueue())
				{
					if (id == nzbInfo->GetId())
					{
						itemList->emplace_back(nullptr, nzbInfo, offset);
					}
				}
			}
		}
	}
}

bool QueueEditor::BuildIdListFromNameList(IdList* idList, NameList* nameList, DownloadQueue::EMatchMode matchMode, DownloadQueue::EEditAction action)
{
#ifndef HAVE_REGEX_H
	if (matchMode == DownloadQueue::mmRegEx)
	{
		return false;
	}
#endif

	std::set<int> uniqueIds;

	for (CString& name : nameList)
	{
		std::unique_ptr<RegEx> regEx;
		if (matchMode == DownloadQueue::mmRegEx)
		{
			regEx = std::make_unique<RegEx>(name);
			if (!regEx->IsValid())
			{
				return false;
			}
		}

		bool found = false;

		for (NzbInfo* nzbInfo : m_downloadQueue->GetQueue())
		{
			for (FileInfo* fileInfo : nzbInfo->GetFileList())
			{
				if (action < DownloadQueue::eaGroupMoveOffset)
				{
					// file action
					BString<1024> filename("%s/%s", fileInfo->GetNzbInfo()->GetName(), FileSystem::BaseFileName(fileInfo->GetFilename()));
					if (((!regEx && !strcmp(filename, name)) || (regEx && regEx->Match(filename))) &&
						(uniqueIds.find(fileInfo->GetId()) == uniqueIds.end()))
					{
						uniqueIds.insert(fileInfo->GetId());
						idList->push_back(fileInfo->GetId());
						found = true;
					}
				}
			}

			if (action >= DownloadQueue::eaGroupMoveOffset)
			{
				// group action
				const char *filename = nzbInfo->GetName();
				if (((!regEx && !strcmp(filename, name)) || (regEx && regEx->Match(filename))) &&
					(uniqueIds.find(nzbInfo->GetId()) == uniqueIds.end()))
				{
					uniqueIds.insert(nzbInfo->GetId());
					idList->push_back(nzbInfo->GetId());
					found = true;
				}
			}
		}

		if (!found && (matchMode == DownloadQueue::mmName))
		{
			return false;
		}
	}

	return true;
}

bool QueueEditor::EditGroup(NzbInfo* nzbInfo, DownloadQueue::EEditAction action, const char* args)
{
	ItemList itemList;
	bool allPaused = true;
	int id = nzbInfo->GetId();

	// collecting files belonging to group
	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
		itemList.emplace_back(fileInfo, nullptr, 0);
		allPaused &= fileInfo->GetPaused();
	}

	if (action == DownloadQueue::eaGroupDelete ||
		action == DownloadQueue::eaGroupParkDelete ||
		action == DownloadQueue::eaGroupDupeDelete ||
		action == DownloadQueue::eaGroupFinalDelete)
	{
		nzbInfo->SetDeleting(true);
		nzbInfo->SetParking(action == DownloadQueue::eaGroupParkDelete &&
			g_Options->GetKeepHistory() > 0 &&
			!nzbInfo->GetUnpackCleanedUpDisk() &&
			nzbInfo->GetCurrentSuccessArticles() > 0);
		nzbInfo->SetAvoidHistory(action == DownloadQueue::eaGroupFinalDelete);
		nzbInfo->SetDeletePaused(allPaused);
		if (action == DownloadQueue::eaGroupDupeDelete)
		{
			nzbInfo->SetDeleteStatus(NzbInfo::dsDupe);
		}
		nzbInfo->SetCleanupDisk(action != DownloadQueue::eaGroupParkDelete);
	}

	DownloadQueue::EEditAction GroupToFileMap[] = {
		(DownloadQueue::EEditAction)0,
		DownloadQueue::eaFileMoveOffset,
		DownloadQueue::eaFileMoveTop,
		DownloadQueue::eaFileMoveBottom,
		DownloadQueue::eaFilePause,
		DownloadQueue::eaFileResume,
		DownloadQueue::eaFileDelete,
		DownloadQueue::eaFilePauseAllPars,
		DownloadQueue::eaFilePauseExtraPars,
		DownloadQueue::eaFileReorder,
		DownloadQueue::eaFileSplit,
		DownloadQueue::eaFileMoveOffset,
		DownloadQueue::eaFileMoveTop,
		DownloadQueue::eaFileMoveBottom,
		(DownloadQueue::EEditAction)0,
		(DownloadQueue::EEditAction)0,
		DownloadQueue::eaFilePause,
		DownloadQueue::eaFileResume,
		DownloadQueue::eaFileDelete,
		DownloadQueue::eaFileDelete,
		DownloadQueue::eaFileDelete,
		DownloadQueue::eaFileDelete,
		DownloadQueue::eaFilePauseAllPars,
		DownloadQueue::eaFilePauseExtraPars,
		(DownloadQueue::EEditAction)0,
		(DownloadQueue::EEditAction)0,
		(DownloadQueue::EEditAction)0,
		(DownloadQueue::EEditAction)0 };

	bool ok = InternEditList(&itemList, nullptr, GroupToFileMap[action], args);

	if ((action == DownloadQueue::eaGroupDelete || action == DownloadQueue::eaGroupDupeDelete || action == DownloadQueue::eaGroupFinalDelete) &&
		// NZBInfo could have been destroyed already
		m_downloadQueue->GetQueue()->Find(id))
	{
		DownloadQueue::Aspect deleteAspect = { DownloadQueue::eaNzbDeleted, m_downloadQueue, nzbInfo, nullptr };
		m_downloadQueue->Notify(&deleteAspect);
	}

	return ok;
}

void QueueEditor::PauseParsInGroups(ItemList* itemList, bool extraParsOnly)
{
	while (true)
	{
		RawFileList GroupFileList;
		FileInfo* firstFileInfo = nullptr;

		for (ItemList::iterator it = itemList->begin(); it != itemList->end(); )
		{
			EditItem& item = *it;
			if (!firstFileInfo ||
				(firstFileInfo->GetNzbInfo() == item.m_fileInfo->GetNzbInfo()))
			{
				GroupFileList.push_back(item.m_fileInfo);
				if (!firstFileInfo)
				{
					firstFileInfo = item.m_fileInfo;
				}
				itemList->erase(it);
				it = itemList->begin();
				continue;
			}
			it++;
		}

		if (!GroupFileList.empty())
		{
			PausePars(&GroupFileList, extraParsOnly);
		}
		else
		{
			break;
		}
	}
}

/**
* If the parameter "bExtraParsOnly" is set to "false", then we pause all par2-files.
* If the parameter "bExtraParsOnly" is set to "true", we use the following strategy:
* At first we find all par-files, which do not have "vol" in their names, then we pause
* all vols and do not affect all just-pars.
* In a case, if there are no just-pars, but only vols, we find the smallest vol-file
* and do not affect it, but pause all other pars.
*/
void QueueEditor::PausePars(RawFileList* fileList, bool extraParsOnly)
{
	debug("QueueEditor: Pausing pars");

	RawFileList Pars, Vols;

	for (FileInfo* fileInfo : fileList)
	{
		if (fileInfo->GetParFile())
		{
			if (!extraParsOnly)
			{
				fileInfo->SetPaused(true);
			}
			else
			{
				BString<1024> loFileName = fileInfo->GetFilename();
				for (char* p = loFileName; *p; p++) *p = tolower(*p); // convert string to lowercase
				if (strstr(loFileName, ".vol"))
				{
					Vols.push_back(fileInfo);
				}
				else
				{
					Pars.push_back(fileInfo);
				}
			}
		}
	}

	if (extraParsOnly)
	{
		if (!Pars.empty())
		{
			for (FileInfo* fileInfo : Vols)
			{
				fileInfo->SetPaused(true);
			}
		}
		else
		{
			// pausing all Vol-files except the smallest one
			FileInfo* smallest = nullptr;
			for (FileInfo* fileInfo : Vols)
			{
				if (!smallest)
				{
					smallest = fileInfo;
				}
				else if (smallest->GetSize() > fileInfo->GetSize())
				{
					smallest->SetPaused(true);
					smallest = fileInfo;
				}
				else
				{
					fileInfo->SetPaused(true);
				}
			}
		}
	}
}

void QueueEditor::SetNzbPriority(NzbInfo* nzbInfo, const char* priority)
{
	debug("Setting priority %s for %s", priority, nzbInfo->GetName());

	int priorityVal = atoi(priority);
	nzbInfo->SetPriority(priorityVal);
}

void QueueEditor::SetNzbCategory(NzbInfo* nzbInfo, const char* category, bool applyParams)
{
	debug("QueueEditor: setting category '%s' for '%s'", category, nzbInfo->GetName());

	bool oldUnpack = g_Options->GetUnpack();
	const char* oldExtensions = g_Options->GetExtensions();
	if (applyParams && !Util::EmptyStr(nzbInfo->GetCategory()))
	{
		Options::Category* categoryObj = g_Options->FindCategory(nzbInfo->GetCategory(), false);
		if (categoryObj)
		{
			oldUnpack = categoryObj->GetUnpack();
			if (!Util::EmptyStr(categoryObj->GetExtensions()))
			{
				oldExtensions = categoryObj->GetExtensions();
			}
		}
	}

	g_QueueCoordinator->SetQueueEntryCategory(m_downloadQueue, nzbInfo, category);

	if (!applyParams)
	{
		return;
	}

	bool newUnpack = g_Options->GetUnpack();
	const char* newExtensions = g_Options->GetExtensions();
	if (!Util::EmptyStr(nzbInfo->GetCategory()))
	{
		Options::Category* categoryObj = g_Options->FindCategory(nzbInfo->GetCategory(), false);
		if (categoryObj)
		{
			newUnpack = categoryObj->GetUnpack();
			if (!Util::EmptyStr(categoryObj->GetExtensions()))
			{
				newExtensions = categoryObj->GetExtensions();
			}
		}
	}

	if (oldUnpack != newUnpack)
	{
		nzbInfo->GetParameters()->SetParameter("*Unpack:", newUnpack ? "yes" : "no");
	}

	if (strcasecmp(oldExtensions, newExtensions))
	{
		// add new params not existed in old category
		Tokenizer tokNew(newExtensions, ",;");
		while (const char* newScriptName = tokNew.Next())
		{
			bool found = false;
			const char* oldScriptName;
			Tokenizer tokOld(oldExtensions, ",;");
			while ((oldScriptName = tokOld.Next()) && !found)
			{
				found = !strcasecmp(newScriptName, oldScriptName);
			}
			if (!found)
			{
				nzbInfo->GetParameters()->SetParameter(BString<1024>("%s:", newScriptName), "yes");
			}
		}

		// remove old params not existed in new category
		Tokenizer tokOld(oldExtensions, ",;");
		while (const char* oldScriptName = tokOld.Next())
		{
			bool found = false;
			const char* newScriptName;
			Tokenizer tokNew(newExtensions, ",;");
			while ((newScriptName = tokNew.Next()) && !found)
			{
				found = !strcasecmp(newScriptName, oldScriptName);
			}
			if (!found)
			{
				nzbInfo->GetParameters()->SetParameter(BString<1024>("%s:", oldScriptName), "no");
			}
		}
	}
}

void QueueEditor::SetNzbName(NzbInfo* nzbInfo, const char* name)
{
	debug("QueueEditor: renaming '%s' to '%s'", nzbInfo->GetName(), name);

	g_QueueCoordinator->SetQueueEntryName(m_downloadQueue, nzbInfo, name);
}

bool QueueEditor::MergeGroups(ItemList* itemList)
{
	if (itemList->size() == 0)
	{
		return false;
	}

	bool ok = true;

	EditItem& destItem = itemList->front();

	for (EditItem& item : itemList) 
	{
		if (item.m_nzbInfo != destItem.m_nzbInfo)
		{
			debug("merge %s to %s", item.m_nzbInfo->GetFilename(), destItem.m_nzbInfo->GetFilename());
			if (g_QueueCoordinator->MergeQueueEntries(m_downloadQueue, destItem.m_nzbInfo, item.m_nzbInfo))
			{
				ok = false;
			}
		}
	}

	return ok;
}

bool QueueEditor::SplitGroup(ItemList* itemList, const char* name)
{
	if (itemList->size() == 0)
	{
		return false;
	}

	RawFileList fileList;

	for (EditItem& item : itemList)
	{
		fileList.push_back(item.m_fileInfo);
	}

	NzbInfo* newNzbInfo = nullptr;
	bool ok = g_QueueCoordinator->SplitQueueEntries(m_downloadQueue, &fileList, name, &newNzbInfo);

	return ok;
}

bool QueueEditor::SortGroups(ItemList* itemList, const char* sort)
{
	AlignGroups(itemList);
	GroupSorter sorter(m_downloadQueue->GetQueue(), itemList);
	return sorter.Execute(sort);
}

void QueueEditor::AlignGroups(ItemList* itemList)
{
	NzbList* nzbList = m_downloadQueue->GetQueue();
	NzbInfo* lastNzbInfo = nullptr;
	uint32 lastNum = 0;
	uint32 num = 0;
	while (num < nzbList->size())
	{
		std::unique_ptr<NzbInfo>& nzbInfo = nzbList->at(num);

		bool selected = false;
		for (QueueEditor::EditItem& item : itemList)
		{
			if (item.m_nzbInfo == nzbInfo.get())
			{
				selected = true;
				break;
			}
		}

		if (selected)
		{
			if (lastNzbInfo && num - lastNum > 1)
			{
				std::unique_ptr<NzbInfo> movedNzbInfo = std::move(*(nzbList->begin() + num));
				nzbList->erase(nzbList->begin() + num);
				nzbList->insert(nzbList->begin() + lastNum + 1, std::move(movedNzbInfo));
				lastNum++;
			}
			else
			{
				lastNum = num;
			}
			lastNzbInfo = nzbInfo.get();
		}
		num++;
	}
}

bool QueueEditor::ItemListContainsItem(ItemList* itemList, int id)
{
	return std::find_if(itemList->begin(), itemList->end(),
		[id](const EditItem& item)
		{
			return item.m_nzbInfo->GetId() == id;
		}) != itemList->end();
};

bool QueueEditor::MoveGroupsTo(ItemList* itemList, IdList* idList, bool before, const char* args)
{
	if (itemList->size() == 0 || Util::EmptyStr(args))
	{
		return false;
	}

	int targetId = atoi(args);
	int offset = 0;

	// check if target is in list of moved items
	if (ItemListContainsItem(itemList, targetId))
	{
		// find the next item to use as target-before
		bool found = false;
		bool targetSet = false;

		for (NzbInfo* nzbInfo : m_downloadQueue->GetQueue())
		{
			if (found)
			{
				if (!ItemListContainsItem(itemList, nzbInfo->GetId()))
				{
					targetId = nzbInfo->GetId();
					before = true;
					targetSet = true;
					break;
				}
			}
			else if (targetId == nzbInfo->GetId())
			{
				found = true;
			}
		}

		if (!targetSet)
		{
			// there are no next item; move to the bottom then
			offset = MAX_ID;
		}
	}

	AlignGroups(itemList);

	if (offset == 0)
	{
		// calculate offset between first moving item and target
		int moveId = itemList->at(0).m_nzbInfo->GetId();
		bool progress = false;
		int step = 0;
		for (NzbInfo* nzbInfo : m_downloadQueue->GetQueue())
		{
			int id = nzbInfo->GetId();
			if (id == targetId || id == moveId)
			{
				if (!progress)
				{
					step = id == targetId ? -1 : 1;
					offset = (before ? 0 : 1) - (step > 0 ? itemList->size() : 0);
					progress = true;
				}
				else
				{
					break;
				}
			}		

			if (progress)
			{
				offset += step;
			}
		}
	}

	return InternEditList(nullptr, idList, DownloadQueue::eaGroupMoveOffset,
		CString::FormatStr("%i", offset));
}

void QueueEditor::ReorderFiles(ItemList* itemList)
{
	if (itemList->size() == 0)
	{
		return;
	}

	EditItem& firstItem = itemList->front();
	NzbInfo* nzbInfo = firstItem.m_fileInfo->GetNzbInfo();
	uint32 insertPos = 0;

	// now can reorder
	for (EditItem& item : itemList)
	{
		FileInfo* fileInfo = item.m_fileInfo;

		// move file item
		FileList::iterator it2 = nzbInfo->GetFileList()->Find(fileInfo);
		if (it2 != nzbInfo->GetFileList()->end())
		{
			std::unique_ptr<FileInfo> movedFileInfo = std::move(*it2);
			nzbInfo->GetFileList()->erase(it2);
			nzbInfo->GetFileList()->insert(nzbInfo->GetFileList()->begin() + insertPos, std::move(movedFileInfo));
			insertPos++;
		}
	}
}

void QueueEditor::SetNzbParameter(NzbInfo* nzbInfo, const char* paramString)
{
	debug("QueueEditor: setting nzb parameter '%s' for '%s'", paramString, FileSystem::BaseFileName(nzbInfo->GetFilename()));

	CString str = paramString;
	char* value = strchr(str, '=');
	if (value)
	{
		*value = '\0';
		value++;
		nzbInfo->GetParameters()->SetParameter(str, value);
	}
	else
	{
		error("Could not set nzb parameter for %s: invalid argument: %s", nzbInfo->GetName(), paramString);
	}
}

void QueueEditor::SetNzbDupeParam(NzbInfo* nzbInfo, DownloadQueue::EEditAction action, const char* text)
{
	debug("QueueEditor: setting dupe parameter %i='%s' for '%s'", (int)action, text, nzbInfo->GetName());

	switch (action)
	{
		case DownloadQueue::eaGroupSetDupeKey:
			nzbInfo->SetDupeKey(text);
			break;

		case DownloadQueue::eaGroupSetDupeScore:
			nzbInfo->SetDupeScore(atoi(text));
			break;

		case DownloadQueue::eaGroupSetDupeMode:
			{
				EDupeMode mode = dmScore;
				if (!strcasecmp(text, "SCORE"))
				{
					mode = dmScore;
				}
				else if (!strcasecmp(text, "ALL"))
				{
					mode = dmAll;
				}
				else if (!strcasecmp(text, "FORCE"))
				{
					mode = dmForce;
				}
				else
				{
					error("Could not set duplicate mode for %s: incorrect mode (%s)", nzbInfo->GetName(), text);
					return;
				}
				nzbInfo->SetDupeMode(mode);
				break;
			}

		default:
			// suppress compiler warning
			break;
	}
}

void QueueEditor::SortGroupFiles(NzbInfo* nzbInfo)
{
	debug("QueueEditor: sorting inner files for '%s'", nzbInfo->GetName());

	std::sort(nzbInfo->GetFileList()->begin(), nzbInfo->GetFileList()->end(),
		[](const std::unique_ptr<FileInfo>& fileInfo1, const std::unique_ptr<FileInfo>& fileInfo2)
		{
			if (!fileInfo1->GetParFile() && !fileInfo2->GetParFile())
			{
				// ".rar"-files are ordered before ".r01", etc. files
				int len1 = strlen(fileInfo1->GetFilename());
				int len2 = strlen(fileInfo2->GetFilename());
				const char* ext1 = strrchr(fileInfo1->GetFilename(), '.');
				const char* ext2 = strrchr(fileInfo2->GetFilename(), '.');
				int ext1len = ext1 ? strlen(ext1) : 0;
				int ext2len = ext2 ? strlen(ext2) : 0;
				bool sameBaseName = len1 == len2 && ext1len == 4 && ext2len == 4 &&
					!strncmp(fileInfo1->GetFilename(), fileInfo2->GetFilename(), len1 - 4);
				if (sameBaseName && !strcmp(ext1, ".rar") && ext2[1] == 'r' &&
					isdigit(ext2[2]) && isdigit(ext2[3]))
				{
					return true;
				}
				else if (sameBaseName && !strcmp(ext2, ".rar") && ext1[1] == 'r' &&
					isdigit(ext1[2]) && isdigit(ext1[3]))
				{
					return false;
				}
			}
			else if (fileInfo1->GetParFile() && fileInfo2->GetParFile() &&
				ParParser::SameParCollection(fileInfo1->GetFilename(), fileInfo2->GetFilename(),
					fileInfo1->GetFilenameConfirmed() && fileInfo2->GetFilenameConfirmed()))
			{
				return fileInfo1->GetSize() < fileInfo2->GetSize();
			}
			else if (!fileInfo1->GetParFile() && fileInfo2->GetParFile())
			{
				return true;
			}
			else if (fileInfo1->GetParFile() && !fileInfo2->GetParFile())
			{
				return false;
			}

			return strcmp(fileInfo1->GetFilename(), fileInfo2->GetFilename()) < 0;
		});
}

bool QueueEditor::DeleteUrl(NzbInfo* nzbInfo, DownloadQueue::EEditAction action)
{
	return g_UrlCoordinator->DeleteQueueEntry(m_downloadQueue, nzbInfo, action == DownloadQueue::eaGroupFinalDelete);
}
