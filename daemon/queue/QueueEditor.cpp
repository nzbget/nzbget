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

const int MAX_ID = 1000000000;

class GroupSorter
{
public:
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

private:
	NzbList*				m_nzbList;
	QueueEditor::ItemList*	m_sortItemList;
	ESortCriteria			m_sortCriteria;
	ESortOrder				m_sortOrder;

	void					AlignSelectedGroups();

public:
							GroupSorter(NzbList* nzbList, QueueEditor::ItemList* sortItemList) :
								m_nzbList(nzbList), m_sortItemList(sortItemList) {}
	bool					Execute(const char* sort);
	bool					operator()(NzbInfo* nzbInfo1, NzbInfo* nzbInfo2) const;
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

	AlignSelectedGroups();

	NzbList tempList = *m_nzbList;

	ESortOrder origSortOrder = m_sortOrder;
	if (m_sortOrder == soAuto && m_sortCriteria == scPriority)
	{
		m_sortOrder = soDescending;
	}

	std::sort(m_nzbList->begin(), m_nzbList->end(), *this);

	if (origSortOrder == soAuto && tempList == *m_nzbList)
	{
		m_sortOrder = m_sortOrder == soDescending ? soAscending : soDescending;
		std::sort(m_nzbList->begin(), m_nzbList->end(), *this);
	}

	tempList.clear(); // prevent destroying of elements

	return true;
}

bool GroupSorter::operator()(NzbInfo* nzbInfo1, NzbInfo* nzbInfo2) const
{
	// if list of ID is empty - sort all items
	bool sortItem1 = m_sortItemList->empty();
	bool sortItem2 = m_sortItemList->empty();

	for (QueueEditor::ItemList::iterator it = m_sortItemList->begin(); it != m_sortItemList->end(); it++)
	{
		QueueEditor::EditItem* item = *it;
		sortItem1 |= item->m_nzbInfo == nzbInfo1;
		sortItem2 |= item->m_nzbInfo == nzbInfo2;
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

void GroupSorter::AlignSelectedGroups()
{
	NzbInfo* lastNzbInfo = nullptr;
	uint32 lastNum = 0;
	uint32 num = 0;
	while (num < m_nzbList->size())
	{
		NzbInfo* nzbInfo = m_nzbList->at(num);

		bool selected = false;
		for (QueueEditor::ItemList::iterator it = m_sortItemList->begin(); it != m_sortItemList->end(); it++)
		{
			QueueEditor::EditItem* item = *it;
			if (item->m_nzbInfo == nzbInfo)
			{
				selected = true;
				break;
			}
		}

		if (selected)
		{
			if (lastNzbInfo && num - lastNum > 1)
			{
				m_nzbList->erase(m_nzbList->begin() + num);
				m_nzbList->insert(m_nzbList->begin() + lastNum + 1, nzbInfo);
				lastNum++;
			}
			else
			{
				lastNum = num;
			}
			lastNzbInfo = nzbInfo;
		}
		num++;
	}
}


QueueEditor::EditItem::EditItem(FileInfo* fileInfo, NzbInfo* nzbInfo, int offset)
{
	m_fileInfo = fileInfo;
	m_nzbInfo = nzbInfo;
	m_offset = offset;
}

QueueEditor::QueueEditor()
{
	debug("Creating QueueEditor");
}

QueueEditor::~QueueEditor()
{
	debug("Destroying QueueEditor");
}

FileInfo* QueueEditor::FindFileInfo(int id)
{
	for (NzbList::iterator it = m_downloadQueue->GetQueue()->begin(); it != m_downloadQueue->GetQueue()->end(); it++)
	{
		NzbInfo* nzbInfo = *it;
		for (FileList::iterator it2 = nzbInfo->GetFileList()->begin(); it2 != nzbInfo->GetFileList()->end(); it2++)
		{
			FileInfo* fileInfo = *it2;
			if (fileInfo->GetId() == id)
			{
				return fileInfo;
			}
		}
	}
	return nullptr;
}

/*
 * Set the pause flag of the specific entry in the queue
 */
void QueueEditor::PauseUnpauseEntry(FileInfo* fileInfo, bool pause)
{
	fileInfo->SetPaused(pause);
}

/*
 * Removes entry
 */
void QueueEditor::DeleteEntry(FileInfo* fileInfo)
{
	fileInfo->GetNzbInfo()->PrintMessage(
		fileInfo->GetNzbInfo()->GetDeleting() ? Message::mkDetail : Message::mkInfo,
		"Deleting file %s from download queue", fileInfo->GetFilename());
	g_QueueCoordinator->DeleteQueueEntry(m_downloadQueue, fileInfo);
}

/*
 * Moves entry in the queue
 */
void QueueEditor::MoveEntry(FileInfo* fileInfo, int offset)
{
	int entry = 0;
	for (FileList::iterator it = fileInfo->GetNzbInfo()->GetFileList()->begin(); it != fileInfo->GetNzbInfo()->GetFileList()->end(); it++)
	{
		FileInfo* fileInfo2 = *it;
		if (fileInfo2 == fileInfo)
		{
			break;
		}
		entry ++;
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
		fileInfo->GetNzbInfo()->GetFileList()->erase(fileInfo->GetNzbInfo()->GetFileList()->begin() + entry);
		fileInfo->GetNzbInfo()->GetFileList()->insert(fileInfo->GetNzbInfo()->GetFileList()->begin() + newEntry, fileInfo);
	}
}

/*
 * Moves group in the queue
 */
void QueueEditor::MoveGroup(NzbInfo* nzbInfo, int offset)
{
	int entry = 0;
	for (NzbList::iterator it = m_downloadQueue->GetQueue()->begin(); it != m_downloadQueue->GetQueue()->end(); it++)
	{
		NzbInfo* nzbInfo2 = *it;
		if (nzbInfo2 == nzbInfo)
		{
			break;
		}
		entry ++;
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
		m_downloadQueue->GetQueue()->erase(m_downloadQueue->GetQueue()->begin() + entry);
		m_downloadQueue->GetQueue()->insert(m_downloadQueue->GetQueue()->begin() + newEntry, nzbInfo);
	}
}

bool QueueEditor::EditEntry(DownloadQueue* downloadQueue, int ID, DownloadQueue::EEditAction action, int offset, const char* text)
{
	m_downloadQueue = downloadQueue;
	IdList cIdList;
	cIdList.push_back(ID);
	return InternEditList(nullptr, &cIdList, action, offset, text);
}

bool QueueEditor::EditList(DownloadQueue* downloadQueue, IdList* idList, NameList* nameList, DownloadQueue::EMatchMode matchMode,
	DownloadQueue::EEditAction action, int offset, const char* text)
{
	if (action == DownloadQueue::eaPostDelete)
	{
		return g_PrePostProcessor->EditList(downloadQueue, idList, action, offset, text);
	}
	else if (DownloadQueue::eaHistoryDelete <= action && action <= DownloadQueue::eaHistorySetName)
	{
		return g_HistoryCoordinator->EditList(downloadQueue, idList, action, offset, text);
	}

	m_downloadQueue = downloadQueue;
	bool ok = true;

	if (nameList)
	{
		idList = new IdList();
		ok = BuildIdListFromNameList(idList, nameList, matchMode, action);
	}

	ok = ok && (InternEditList(nullptr, idList, action, offset, text) || matchMode == DownloadQueue::mmRegEx);

	m_downloadQueue->Save();

	if (nameList)
	{
		delete idList;
	}

	return ok;
}

bool QueueEditor::InternEditList(ItemList* itemList,
	IdList* idList, DownloadQueue::EEditAction action, int offset, const char* text)
{
	ItemList workItems;
	if (!itemList)
	{
		itemList = &workItems;
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
			return SortGroups(itemList, text);

		case DownloadQueue::eaFileSplit:
			return SplitGroup(itemList, text);

		case DownloadQueue::eaFileReorder:
			ReorderFiles(itemList);
			break;

		default:
			for (ItemList::iterator it = itemList->begin(); it != itemList->end(); it++)
			{
				EditItem* item = *it;
				switch (action)
				{
					case DownloadQueue::eaFilePause:
						PauseUnpauseEntry(item->m_fileInfo, true);
						break;

					case DownloadQueue::eaFileResume:
						PauseUnpauseEntry(item->m_fileInfo, false);
						break;

					case DownloadQueue::eaFileMoveOffset:
					case DownloadQueue::eaFileMoveTop:
					case DownloadQueue::eaFileMoveBottom:
						MoveEntry(item->m_fileInfo, item->m_offset);
						break;

					case DownloadQueue::eaFileDelete:
						DeleteEntry(item->m_fileInfo);
						break;

					case DownloadQueue::eaGroupSetPriority:
						SetNzbPriority(item->m_nzbInfo, text);
						break;

					case DownloadQueue::eaGroupSetCategory:
					case DownloadQueue::eaGroupApplyCategory:
						SetNzbCategory(item->m_nzbInfo, text, action == DownloadQueue::eaGroupApplyCategory);
						break;

					case DownloadQueue::eaGroupSetName:
						SetNzbName(item->m_nzbInfo, text);
						break;

					case DownloadQueue::eaGroupSetDupeKey:
					case DownloadQueue::eaGroupSetDupeScore:
					case DownloadQueue::eaGroupSetDupeMode:
						SetNzbDupeParam(item->m_nzbInfo, action, text);
						break;

					case DownloadQueue::eaGroupSetParameter:
						SetNzbParameter(item->m_nzbInfo, text);
						break;

					case DownloadQueue::eaGroupMoveTop:
					case DownloadQueue::eaGroupMoveBottom:
					case DownloadQueue::eaGroupMoveOffset:
						MoveGroup(item->m_nzbInfo, item->m_offset);
						break;

					case DownloadQueue::eaGroupPause:
					case DownloadQueue::eaGroupResume:
					case DownloadQueue::eaGroupPauseAllPars:
					case DownloadQueue::eaGroupPauseExtraPars:
						EditGroup(item->m_nzbInfo, action, offset, text);
						break;

					case DownloadQueue::eaGroupDelete:
					case DownloadQueue::eaGroupDupeDelete:
					case DownloadQueue::eaGroupFinalDelete:
						if (item->m_nzbInfo->GetKind() == NzbInfo::nkUrl)
						{
							DeleteUrl(item->m_nzbInfo, action);
						}
						else
						{
							EditGroup(item->m_nzbInfo, action, offset, text);
						}


					default:
						// suppress compiler warning "enumeration not handled in switch"
						break;
				}
				delete item;
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
		for (NzbList::iterator it = m_downloadQueue->GetQueue()->begin(); it != m_downloadQueue->GetQueue()->end(); it++)
		{
			NzbInfo* nzbInfo = *it;
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
				FileInfo* fileInfo = nzbInfo->GetFileList()->at(index);
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
					itemList->push_back(new EditItem(fileInfo, nullptr, workOffset));
				}
			}
		}
	}
	else if ((offset != 0) &&
		(action == DownloadQueue::eaGroupMoveOffset || action == DownloadQueue::eaGroupMoveTop || action == DownloadQueue::eaGroupMoveBottom))
	{
		// add IDs to list in order they currently have in download queue
		// per group only one FileInfo is added to the list
		int nrEntries = (int)m_downloadQueue->GetQueue()->size();
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
			NzbInfo* nzbInfo = m_downloadQueue->GetQueue()->at(index);
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
				itemList->push_back(new EditItem(nullptr, nzbInfo, workOffset));
			}
		}
	}
	else if (action < DownloadQueue::eaGroupMoveOffset)
	{
		// check ID range
		int maxId = 0;
		int minId = MAX_ID;
		for (NzbList::iterator it = m_downloadQueue->GetQueue()->begin(); it != m_downloadQueue->GetQueue()->end(); it++)
		{
			NzbInfo* nzbInfo = *it;
			for (FileList::iterator it2 = nzbInfo->GetFileList()->begin(); it2 != nzbInfo->GetFileList()->end(); it2++)
			{
				FileInfo* fileInfo = *it2;
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
		for (IdList::iterator it = idList->begin(); it != idList->end(); it++)
		{
			int id = *it;
			if (minId <= id && id <= maxId)
			{
				FileInfo* fileInfo = FindFileInfo(id);
				if (fileInfo)
				{
					itemList->push_back(new EditItem(fileInfo, nullptr, offset));
				}
			}
		}
	}
	else
	{
		// check ID range
		int maxId = 0;
		int minId = MAX_ID;
		for (NzbList::iterator it = m_downloadQueue->GetQueue()->begin(); it != m_downloadQueue->GetQueue()->end(); it++)
		{
			NzbInfo* nzbInfo = *it;
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
		for (IdList::iterator it = idList->begin(); it != idList->end(); it++)
		{
			int id = *it;
			if (minId <= id && id <= maxId)
			{
				for (NzbList::iterator it2 = m_downloadQueue->GetQueue()->begin(); it2 != m_downloadQueue->GetQueue()->end(); it2++)
				{
					NzbInfo* nzbInfo = *it2;
					if (id == nzbInfo->GetId())
					{
						itemList->push_back(new EditItem(nullptr, nzbInfo, offset));
					}
				}
			}
		}
	}
}

bool QueueEditor::BuildIdListFromNameList(IdList* idList, NameList* nameList, DownloadQueue::EMatchMode matchMode, DownloadQueue::EEditAction action)
{
#ifndef HAVE_REGEX_H
	if (matchMode == mmRegEx)
	{
		return false;
	}
#endif

	std::set<int> uniqueIds;

	for (NameList::iterator it = nameList->begin(); it != nameList->end(); it++)
	{
		const char* name = *it;

		RegEx *regEx = nullptr;
		if (matchMode == DownloadQueue::mmRegEx)
		{
			regEx = new RegEx(name);
			if (!regEx->IsValid())
			{
				delete regEx;
				return false;
			}
		}

		bool found = false;

		for (NzbList::iterator it3 = m_downloadQueue->GetQueue()->begin(); it3 != m_downloadQueue->GetQueue()->end(); it3++)
		{
			NzbInfo* nzbInfo = *it3;

			for (FileList::iterator it2 = nzbInfo->GetFileList()->begin(); it2 != nzbInfo->GetFileList()->end(); it2++)
			{
				FileInfo* fileInfo = *it2;
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

		delete regEx;

		if (!found && (matchMode == DownloadQueue::mmName))
		{
			return false;
		}
	}

	return true;
}

bool QueueEditor::EditGroup(NzbInfo* nzbInfo, DownloadQueue::EEditAction action, int offset, const char* text)
{
	ItemList itemList;
	bool allPaused = true;
	int id = nzbInfo->GetId();

	// collecting files belonging to group
	for (FileList::iterator it = nzbInfo->GetFileList()->begin(); it != nzbInfo->GetFileList()->end(); it++)
	{
		FileInfo* fileInfo = *it;
		itemList.push_back(new EditItem(fileInfo, nullptr, 0));
		allPaused &= fileInfo->GetPaused();
	}

	if (action == DownloadQueue::eaGroupDelete || action == DownloadQueue::eaGroupDupeDelete || action == DownloadQueue::eaGroupFinalDelete)
	{
		nzbInfo->SetDeleting(true);
		nzbInfo->SetAvoidHistory(action == DownloadQueue::eaGroupFinalDelete);
		nzbInfo->SetDeletePaused(allPaused);
		if (action == DownloadQueue::eaGroupDupeDelete)
		{
			nzbInfo->SetDeleteStatus(NzbInfo::dsDupe);
		}
		nzbInfo->SetCleanupDisk(CanCleanupDisk(nzbInfo));
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
		DownloadQueue::eaFilePause,
		DownloadQueue::eaFileResume,
		DownloadQueue::eaFileDelete,
		DownloadQueue::eaFileDelete,
		DownloadQueue::eaFileDelete,
		DownloadQueue::eaFilePauseAllPars,
		DownloadQueue::eaFilePauseExtraPars,
		(DownloadQueue::EEditAction)0,
		(DownloadQueue::EEditAction)0,
		(DownloadQueue::EEditAction)0,
		(DownloadQueue::EEditAction)0 };

	bool ok = InternEditList(&itemList, nullptr, GroupToFileMap[action], offset, text);

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
		FileList GroupFileList;
		FileInfo* firstFileInfo = nullptr;

		for (ItemList::iterator it = itemList->begin(); it != itemList->end(); )
		{
			EditItem* item = *it;
			if (!firstFileInfo ||
				(firstFileInfo->GetNzbInfo() == item->m_fileInfo->GetNzbInfo()))
			{
				GroupFileList.push_back(item->m_fileInfo);
				if (!firstFileInfo)
				{
					firstFileInfo = item->m_fileInfo;
				}
				delete item;
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
void QueueEditor::PausePars(FileList* fileList, bool extraParsOnly)
{
	debug("QueueEditor: Pausing pars");

	FileList Pars, Vols;

	for (FileList::iterator it = fileList->begin(); it != fileList->end(); it++)
	{
		FileInfo* fileInfo = *it;
		BString<1024> loFileName = fileInfo->GetFilename();
		for (char* p = loFileName; *p; p++) *p = tolower(*p); // convert string to lowercase

		if (strstr(loFileName, ".par2"))
		{
			if (!extraParsOnly)
			{
				fileInfo->SetPaused(true);
			}
			else
			{
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
			for (FileList::iterator it = Vols.begin(); it != Vols.end(); it++)
			{
				FileInfo* fileInfo = *it;
				fileInfo->SetPaused(true);
			}
		}
		else
		{
			// pausing all Vol-files except the smallest one
			FileInfo* smallest = nullptr;
			for (FileList::iterator it = Vols.begin(); it != Vols.end(); it++)
			{
				FileInfo* fileInfo = *it;
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
	const char* oldPostScript = g_Options->GetPostScript();
	if (applyParams && !Util::EmptyStr(nzbInfo->GetCategory()))
	{
		Options::Category* categoryObj = g_Options->FindCategory(nzbInfo->GetCategory(), false);
		if (categoryObj)
		{
			oldUnpack = categoryObj->GetUnpack();
			if (!Util::EmptyStr(categoryObj->GetPostScript()))
			{
				oldPostScript = categoryObj->GetPostScript();
			}
		}
	}

	g_QueueCoordinator->SetQueueEntryCategory(m_downloadQueue, nzbInfo, category);

	if (!applyParams)
	{
		return;
	}

	bool newUnpack = g_Options->GetUnpack();
	const char* newPostScript = g_Options->GetPostScript();
	if (!Util::EmptyStr(nzbInfo->GetCategory()))
	{
		Options::Category* categoryObj = g_Options->FindCategory(nzbInfo->GetCategory(), false);
		if (categoryObj)
		{
			newUnpack = categoryObj->GetUnpack();
			if (!Util::EmptyStr(categoryObj->GetPostScript()))
			{
				newPostScript = categoryObj->GetPostScript();
			}
		}
	}

	if (oldUnpack != newUnpack)
	{
		nzbInfo->GetParameters()->SetParameter("*Unpack:", newUnpack ? "yes" : "no");
	}

	if (strcasecmp(oldPostScript, newPostScript))
	{
		// add new params not existed in old category
		Tokenizer tokNew(newPostScript, ",;");
		while (const char* newScriptName = tokNew.Next())
		{
			bool found = false;
			const char* oldScriptName;
			Tokenizer tokOld(oldPostScript, ",;");
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
		Tokenizer tokOld(oldPostScript, ",;");
		while (const char* oldScriptName = tokOld.Next())
		{
			bool found = false;
			const char* newScriptName;
			Tokenizer tokNew(newPostScript, ",;");
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

/**
* Check if deletion of already downloaded files is possible (when nzb id deleted from queue).
* The deletion is most always possible, except the case if all remaining files in queue
* (belonging to this nzb-file) are PARS.
*/
bool QueueEditor::CanCleanupDisk(NzbInfo* nzbInfo)
{
	if (nzbInfo->GetDeleteStatus() != NzbInfo::dsNone)
	{
		return true;
	}

	for (FileList::iterator it = nzbInfo->GetFileList()->begin(); it != nzbInfo->GetFileList()->end(); it++)
	{
		FileInfo* fileInfo = *it;
		BString<1024> loFileName = fileInfo->GetFilename();
		for (char* p = loFileName; *p; p++) *p = tolower(*p); // convert string to lowercase

		if (!strstr(loFileName, ".par2"))
		{
			// non-par file found
			return true;
		}
	}

	return false;
}

bool QueueEditor::MergeGroups(ItemList* itemList)
{
	if (itemList->size() == 0)
	{
		return false;
	}

	bool ok = true;

	EditItem* destItem = itemList->front();

	for (ItemList::iterator it = itemList->begin() + 1; it != itemList->end(); it++)
	{
		EditItem* item = *it;
		if (item->m_nzbInfo != destItem->m_nzbInfo)
		{
			debug("merge %s to %s", item->m_nzbInfo->GetFilename(), destItem->m_nzbInfo->GetFilename());
			if (g_QueueCoordinator->MergeQueueEntries(m_downloadQueue, destItem->m_nzbInfo, item->m_nzbInfo))
			{
				ok = false;
			}
		}
		delete item;
	}

	delete destItem;
	return ok;
}

bool QueueEditor::SplitGroup(ItemList* itemList, const char* name)
{
	if (itemList->size() == 0)
	{
		return false;
	}

	FileList fileList(false);

	for (ItemList::iterator it = itemList->begin(); it != itemList->end(); it++)
	{
		EditItem* item = *it;
		fileList.push_back(item->m_fileInfo);
		delete item;
	}

	NzbInfo* newNzbInfo = nullptr;
	bool ok = g_QueueCoordinator->SplitQueueEntries(m_downloadQueue, &fileList, name, &newNzbInfo);

	return ok;
}

bool QueueEditor::SortGroups(ItemList* itemList, const char* sort)
{
	GroupSorter sorter(m_downloadQueue->GetQueue(), itemList);
	return sorter.Execute(sort);
}

void QueueEditor::ReorderFiles(ItemList* itemList)
{
	if (itemList->size() == 0)
	{
		return;
	}

	EditItem* firstItem = itemList->front();
	NzbInfo* nzbInfo = firstItem->m_fileInfo->GetNzbInfo();
	uint32 insertPos = 0;

	// now can reorder
	for (ItemList::iterator it = itemList->begin(); it != itemList->end(); it++)
	{
		EditItem* item = *it;
		FileInfo* fileInfo = item->m_fileInfo;

		// move file item
		FileList::iterator it2 = std::find(nzbInfo->GetFileList()->begin(), nzbInfo->GetFileList()->end(), fileInfo);
		if (it2 != nzbInfo->GetFileList()->end())
		{
			nzbInfo->GetFileList()->erase(it2);
			nzbInfo->GetFileList()->insert(nzbInfo->GetFileList()->begin() + insertPos, fileInfo);
			insertPos++;
		}

		delete item;
	}
}

void QueueEditor::SetNzbParameter(NzbInfo* nzbInfo, const char* paramString)
{
	debug("QueueEditor: setting nzb parameter '%s' for '%s'", paramString, FileSystem::BaseFileName(nzbInfo->GetFilename()));

	char* str = strdup(paramString);

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

	free(str);
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

bool QueueEditor::DeleteUrl(NzbInfo* nzbInfo, DownloadQueue::EEditAction action)
{
	return g_UrlCoordinator->DeleteQueueEntry(m_downloadQueue, nzbInfo, action == DownloadQueue::eaGroupFinalDelete);
}
