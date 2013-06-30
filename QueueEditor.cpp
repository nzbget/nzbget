/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2011 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include <cctype>
#include <cstdio>
#include <sys/stat.h>
#include <set>
#ifndef WIN32
#include <unistd.h>
#include <sys/time.h>
#endif

#include "nzbget.h"
#include "DownloadInfo.h"
#include "QueueEditor.h"
#include "QueueCoordinator.h"
#include "DiskState.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"

extern QueueCoordinator* g_pQueueCoordinator;
extern Options* g_pOptions;
extern DiskState* g_pDiskState;

const int MAX_ID = 100000000;

QueueEditor::EditItem::EditItem(FileInfo* pFileInfo, int iOffset)
{
	m_pFileInfo = pFileInfo;
	m_iOffset = iOffset;
}

QueueEditor::QueueEditor()
{
	debug("Creating QueueEditor");
}

QueueEditor::~QueueEditor()
{
	debug("Destroying QueueEditor");
}

FileInfo* QueueEditor::FindFileInfo(DownloadQueue* pDownloadQueue, int iID)
{
	for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (pFileInfo->GetID() == iID)
		{
			return pFileInfo;
		}
	}
	return NULL;
}

int QueueEditor::FindFileInfoEntry(DownloadQueue* pDownloadQueue, FileInfo* pFileInfo)
{
	int iEntry = 0;
	for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
	{
		FileInfo* pFileInfo2 = *it;
		if (pFileInfo2 == pFileInfo)
		{
			return iEntry;
		}
		iEntry ++;
	}
	return -1;
}

/*
 * Set the pause flag of the specific entry in the queue
 * returns true if successful, false if operation is not possible
 */
void QueueEditor::PauseUnpauseEntry(FileInfo* pFileInfo, bool bPause)
{
	pFileInfo->SetPaused(bPause);
}

/*
 * Removes entry
 * returns true if successful, false if operation is not possible
 */
void QueueEditor::DeleteEntry(FileInfo* pFileInfo)
{
	info("Deleting file %s from download queue", pFileInfo->GetFilename());
	g_pQueueCoordinator->DeleteQueueEntry(pFileInfo);
}

/*
 * Moves entry in the queue
 * returns true if successful, false if operation is not possible
 */
void QueueEditor::MoveEntry(DownloadQueue* pDownloadQueue, FileInfo* pFileInfo, int iOffset)
{
	int iEntry = FindFileInfoEntry(pDownloadQueue, pFileInfo);
	if (iEntry > -1)
	{
		int iNewEntry = iEntry + iOffset;

		if (iNewEntry < 0)
		{
			iNewEntry = 0;
		}
		if ((unsigned int)iNewEntry > pDownloadQueue->GetFileQueue()->size() - 1)
		{
			iNewEntry = (int)pDownloadQueue->GetFileQueue()->size() - 1;
		}

		if (iNewEntry >= 0 && (unsigned int)iNewEntry <= pDownloadQueue->GetFileQueue()->size() - 1)
		{
			FileInfo* fi = pDownloadQueue->GetFileQueue()->at(iEntry);
			pDownloadQueue->GetFileQueue()->erase(pDownloadQueue->GetFileQueue()->begin() + iEntry);
			pDownloadQueue->GetFileQueue()->insert(pDownloadQueue->GetFileQueue()->begin() + iNewEntry, fi);
		}
	}
}

/*
 * Set priority for entry
 * returns true if successful, false if operation is not possible
 */
void QueueEditor::SetPriorityEntry(FileInfo* pFileInfo, const char* szPriority)
{
	debug("Setting priority %s for file %s", szPriority, pFileInfo->GetFilename());
	int iPriority = atoi(szPriority);
	pFileInfo->SetPriority(iPriority);
}

bool QueueEditor::EditEntry(int ID, bool bSmartOrder, EEditAction eAction, int iOffset, const char* szText)
{
	IDList cIDList;
	cIDList.clear();
	cIDList.push_back(ID);
	return EditList(&cIDList, NULL, mmID, bSmartOrder, eAction, iOffset, szText);
}

bool QueueEditor::LockedEditEntry(DownloadQueue* pDownloadQueue, int ID, bool bSmartOrder, EEditAction eAction, int iOffset, const char* szText)
{
	IDList cIDList;
	cIDList.clear();
	cIDList.push_back(ID);
	return InternEditList(pDownloadQueue, &cIDList, bSmartOrder, eAction, iOffset, szText);
}

bool QueueEditor::EditList(IDList* pIDList, NameList* pNameList, EMatchMode eMatchMode, bool bSmartOrder, 
	EEditAction eAction, int iOffset, const char* szText)
{
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	bool bOK = true;

	if (pNameList)
	{
		pIDList = new IDList();
		bOK = BuildIDListFromNameList(pDownloadQueue, pIDList, pNameList, eMatchMode, eAction);
	}

	bOK = bOK && (InternEditList(pDownloadQueue, pIDList, bSmartOrder, eAction, iOffset, szText) || eMatchMode == mmRegEx);

	if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
	{
		g_pDiskState->SaveDownloadQueue(pDownloadQueue);
	}

	g_pQueueCoordinator->UnlockQueue();

	if (pNameList)
	{
		delete pIDList;
	}

	return bOK;
}

bool QueueEditor::LockedEditList(DownloadQueue* pDownloadQueue, IDList* pIDList, bool bSmartOrder, EEditAction eAction, int iOffset, const char* szText)
{
	return InternEditList(pDownloadQueue, pIDList, bSmartOrder, eAction, iOffset, szText);
}

bool QueueEditor::InternEditList(DownloadQueue* pDownloadQueue, IDList* pIDList, bool bSmartOrder, EEditAction eAction, int iOffset, const char* szText)
{
	if (eAction == eaGroupMoveOffset)
	{
		AlignAffectedGroups(pDownloadQueue, pIDList, bSmartOrder, iOffset);
	}

	ItemList cItemList;
	PrepareList(pDownloadQueue, &cItemList, pIDList, bSmartOrder, eAction, iOffset);

	if (eAction == eaFilePauseAllPars || eAction == eaFilePauseExtraPars)
	{
		PauseParsInGroups(&cItemList, eAction == eaFilePauseExtraPars);
	}
	else if (eAction == eaGroupMerge)
	{
		MergeGroups(pDownloadQueue, &cItemList);
	}
	else if (eAction == eaFileReorder)
	{
		ReorderFiles(pDownloadQueue, &cItemList);
	}
	else
	{
		for (ItemList::iterator it = cItemList.begin(); it != cItemList.end(); it++)
		{
			EditItem* pItem = *it;
			switch (eAction)
			{
				case eaFilePause:
					PauseUnpauseEntry(pItem->m_pFileInfo, true);
					break;

				case eaFileResume:
					PauseUnpauseEntry(pItem->m_pFileInfo, false);
					break;

				case eaFileMoveOffset:
				case eaFileMoveTop:
				case eaFileMoveBottom:
					MoveEntry(pDownloadQueue, pItem->m_pFileInfo, pItem->m_iOffset);
					break;

				case eaFileDelete:
					DeleteEntry(pItem->m_pFileInfo);
					break;

				case eaFileSetPriority:
					SetPriorityEntry(pItem->m_pFileInfo, szText);
					break;

				case eaGroupSetCategory:
					SetNZBCategory(pItem->m_pFileInfo->GetNZBInfo(), szText);
					break;

				case eaGroupSetName:
					SetNZBName(pItem->m_pFileInfo->GetNZBInfo(), szText);
					break;

				case eaGroupSetParameter:
					SetNZBParameter(pItem->m_pFileInfo->GetNZBInfo(), szText);
					break;

				case eaGroupPause:
				case eaGroupResume:
				case eaGroupDelete:
				case eaGroupMoveTop:
				case eaGroupMoveBottom:
				case eaGroupMoveOffset:
				case eaGroupPauseAllPars:
				case eaGroupPauseExtraPars:
				case eaGroupSetPriority:
					EditGroup(pDownloadQueue, pItem->m_pFileInfo, eAction, iOffset, szText);
					break;

				case eaFilePauseAllPars:
				case eaFilePauseExtraPars:
				case eaGroupMerge:
				case eaFileReorder:
					// remove compiler warning "enumeration not handled in switch"
					break;
			}
			delete pItem;
		}
	}

	return cItemList.size() > 0;
}

void QueueEditor::PrepareList(DownloadQueue* pDownloadQueue, ItemList* pItemList, IDList* pIDList, bool bSmartOrder, 
	EEditAction EEditAction, int iOffset)
{
	if (EEditAction == eaFileMoveTop)
	{
		iOffset = -MAX_ID;
	}
	else if (EEditAction == eaFileMoveBottom)
	{
		iOffset = MAX_ID;
	}

	pItemList->reserve(pIDList->size());
	if (bSmartOrder && iOffset != 0 && 
		(EEditAction == eaFileMoveOffset || EEditAction == eaFileMoveTop || EEditAction == eaFileMoveBottom))
	{
		//add IDs to list in order they currently have in download queue
		int iLastDestPos = -1;
		int iStart, iEnd, iStep;
		if (iOffset < 0)
		{
			iStart = 0;
			iEnd = pDownloadQueue->GetFileQueue()->size();
			iStep = 1;
		}
		else
		{
			iStart = pDownloadQueue->GetFileQueue()->size() - 1;
			iEnd = -1;
			iStep = -1;
		}
		for (int iIndex = iStart; iIndex != iEnd; iIndex += iStep)
		{
			FileInfo* pFileInfo = pDownloadQueue->GetFileQueue()->at(iIndex);
			int iID = pFileInfo->GetID();
			for (IDList::iterator it = pIDList->begin(); it != pIDList->end(); it++)
			{
				if (iID == *it)
				{
					int iWorkOffset = iOffset;
					int iDestPos = iIndex + iWorkOffset;
					if (iLastDestPos == -1)
					{
						if (iDestPos < 0)
						{
							iWorkOffset = -iIndex;
						}
						else if (iDestPos > int(pDownloadQueue->GetFileQueue()->size()) - 1)
						{
							iWorkOffset = int(pDownloadQueue->GetFileQueue()->size()) - 1 - iIndex;
						}
					}
					else
					{
						if (iWorkOffset < 0 && iDestPos <= iLastDestPos)
						{
							iWorkOffset = iLastDestPos - iIndex + 1;
						}
						else if (iWorkOffset > 0 && iDestPos >= iLastDestPos)
						{
							iWorkOffset = iLastDestPos - iIndex - 1;
						}
					}
					iLastDestPos = iIndex + iWorkOffset;
					pItemList->push_back(new EditItem(pFileInfo, iWorkOffset));
					break;
				}
			}
		}
	}
	else
	{
		// check ID range
		int iMaxID = 0;
		int iMinID = MAX_ID;
		for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
		{
			FileInfo* pFileInfo = *it;
			int ID = pFileInfo->GetID();
			if (ID > iMaxID)
			{
				iMaxID = ID;
			}
			if (ID < iMinID)
			{
				iMinID = ID;
			}
		}

		//add IDs to list in order they were transmitted in command
		for (IDList::iterator it = pIDList->begin(); it != pIDList->end(); it++)
		{
			int iID = *it;
			if (iMinID <= iID && iID <= iMaxID)
			{
				FileInfo* pFileInfo = FindFileInfo(pDownloadQueue, iID);
				if (pFileInfo)
				{
					pItemList->push_back(new EditItem(pFileInfo, iOffset));
				}
			}
		}
	}
}

bool QueueEditor::BuildIDListFromNameList(DownloadQueue* pDownloadQueue, IDList* pIDList, NameList* pNameList, EMatchMode eMatchMode, EEditAction eAction)
{
#ifndef HAVE_REGEX_H
	if (eMatchMode == mmRegEx)
	{
		return false;
	}
#endif

	std::set<int> uniqueIDs;

	for (NameList::iterator it = pNameList->begin(); it != pNameList->end(); it++)
	{
		const char* szName = *it;

		RegEx *pRegEx = NULL;
		if (eMatchMode == mmRegEx)
		{
			pRegEx = new RegEx(szName);
			if (!pRegEx->IsValid())
			{
				delete pRegEx;
				return false;
			}
		}

		bool bFound = false;

		for (FileQueue::iterator it2 = pDownloadQueue->GetFileQueue()->begin(); it2 != pDownloadQueue->GetFileQueue()->end(); it2++)
		{
			FileInfo* pFileInfo = *it2;
			if (eAction < eaGroupMoveOffset)
			{
				// file action
				char szFilename[MAX_PATH];
				snprintf(szFilename, sizeof(szFilename) - 1, "%s/%s", pFileInfo->GetNZBInfo()->GetName(), Util::BaseFileName(pFileInfo->GetFilename()));
				if (((!pRegEx && !strcmp(szFilename, szName)) || (pRegEx && pRegEx->Match(szFilename))) &&
					(uniqueIDs.find(pFileInfo->GetID()) == uniqueIDs.end()))
				{
					uniqueIDs.insert(pFileInfo->GetID());
					pIDList->push_back(pFileInfo->GetID());
					bFound = true;
				}
			}
			else
			{
				// group action
				const char *szFilename = pFileInfo->GetNZBInfo()->GetName();
				if (((!pRegEx && !strcmp(szFilename, szName)) || (pRegEx && pRegEx->Match(szFilename))) &&
					(uniqueIDs.find(pFileInfo->GetNZBInfo()->GetID()) == uniqueIDs.end()))
				{
					uniqueIDs.insert(pFileInfo->GetNZBInfo()->GetID());
					pIDList->push_back(pFileInfo->GetID());
					bFound = true;
				}
			}
		}

		if (pRegEx)
		{
			delete pRegEx;
		}

		if (!bFound && (eMatchMode == mmName))
		{
			return false;
		}
	}

	return true;
}

bool QueueEditor::EditGroup(DownloadQueue* pDownloadQueue, FileInfo* pFileInfo, EEditAction eAction, int iOffset, const char* szText)
{
	IDList cIDList;
	cIDList.clear();

	// collecting files belonging to group
	for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
	{
		FileInfo* pFileInfo2 = *it;
		if (pFileInfo2->GetNZBInfo() == pFileInfo->GetNZBInfo())
		{
			cIDList.push_back(pFileInfo2->GetID());
		}
	}

	if (eAction == eaGroupMoveOffset)
	{
		// calculating offset in terms of files
		FileList cGroupList;
		BuildGroupList(pDownloadQueue, &cGroupList);
		unsigned int iNum = 0;
		for (FileList::iterator it = cGroupList.begin(); it != cGroupList.end(); it++, iNum++)
		{
			FileInfo* pGroupInfo = *it;
			if (pGroupInfo->GetNZBInfo() == pFileInfo->GetNZBInfo())
			{
				break;
			}
		}
		int iFileOffset = 0;
		if (iOffset > 0)
		{
			if (iNum + iOffset >= cGroupList.size() - 1)
			{
				eAction = eaGroupMoveBottom;
			}
			else
			{
				for (unsigned int i = iNum + 2; i < cGroupList.size() && iOffset > 0; i++, iOffset--)
				{
					iFileOffset += FindFileInfoEntry(pDownloadQueue, cGroupList[i]) - FindFileInfoEntry(pDownloadQueue, cGroupList[i-1]);
				}
			}
		}
		else
		{
			if (iNum + iOffset <= 0)
			{
				eAction = eaGroupMoveTop;
			}
			else
			{
				for (unsigned int i = iNum; i > 0 && iOffset < 0; i--, iOffset++)
				{
					iFileOffset -= FindFileInfoEntry(pDownloadQueue, cGroupList[i]) - FindFileInfoEntry(pDownloadQueue, cGroupList[i-1]);
				}
			}
		}
		iOffset = iFileOffset;
	}
	else if (eAction == eaGroupDelete)
	{
		pFileInfo->GetNZBInfo()->SetDeleted(true);
		pFileInfo->GetNZBInfo()->SetCleanupDisk(CanCleanupDisk(pDownloadQueue, pFileInfo->GetNZBInfo()));
	}

	EEditAction GroupToFileMap[] = { (EEditAction)0, eaFileMoveOffset, eaFileMoveTop, eaFileMoveBottom, 
		eaFilePause, eaFileResume, eaFileDelete, eaFilePauseAllPars, eaFilePauseExtraPars, eaFileSetPriority, eaFileReorder,
		eaFileMoveOffset, eaFileMoveTop, eaFileMoveBottom, eaFilePause, eaFileResume, eaFileDelete, 
		eaFilePauseAllPars, eaFilePauseExtraPars, eaFileSetPriority,
		(EEditAction)0, (EEditAction)0, (EEditAction)0 };

	return InternEditList(pDownloadQueue, &cIDList, true, GroupToFileMap[eAction], iOffset, szText);
}

void QueueEditor::BuildGroupList(DownloadQueue* pDownloadQueue, FileList* pGroupList)
{
	pGroupList->clear();
    for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
    {
        FileInfo* pFileInfo = *it;
		FileInfo* pGroupInfo = NULL;
		for (FileList::iterator itg = pGroupList->begin(); itg != pGroupList->end(); itg++)
		{
			FileInfo* pGroupInfo1 = *itg;
			if (pGroupInfo1->GetNZBInfo() == pFileInfo->GetNZBInfo())
			{
				pGroupInfo = pGroupInfo1;
				break;
			}
		}
		if (!pGroupInfo)
		{
			pGroupList->push_back(pFileInfo);
		}
	}
}

bool QueueEditor::ItemExists(FileList* pFileList, FileInfo* pFileInfo)
{
	for (FileList::iterator it = pFileList->begin(); it != pFileList->end(); it++)
	{
		if (*it == pFileInfo)
		{
			return true;
		}
	}
	return false;
}

void QueueEditor::AlignAffectedGroups(DownloadQueue* pDownloadQueue, IDList* pIDList, bool bSmartOrder, int iOffset)
{
	// Build list of all groups; List contains first file of each group
	FileList cGroupList;
	BuildGroupList(pDownloadQueue, &cGroupList);

	// Find affected groups. It includes groups being moved and groups directly
	// above or under of these groups (those order is also changed)
	FileList cAffectedGroupList;
	cAffectedGroupList.clear();
	ItemList cItemList;
	PrepareList(pDownloadQueue, &cItemList, pIDList, bSmartOrder, eaFileMoveOffset, iOffset);
	for (ItemList::iterator it = cItemList.begin(); it != cItemList.end(); it++)
	{
		EditItem* pItem = *it;
		unsigned int iNum = 0;
		for (FileList::iterator it = cGroupList.begin(); it != cGroupList.end(); it++, iNum++)
		{
			FileInfo* pFileInfo = *it;
			if (pItem->m_pFileInfo->GetNZBInfo() == pFileInfo->GetNZBInfo())
			{
				if (!ItemExists(&cAffectedGroupList, pFileInfo))
				{
					cAffectedGroupList.push_back(pFileInfo);
				}
				if (iOffset < 0)
				{
					for (int i = iNum - 1; i >= -iOffset-1; i--)
					{
						if (!ItemExists(&cAffectedGroupList, cGroupList[i]))
						{
							cAffectedGroupList.push_back(cGroupList[i]);
						}
					}
				}
				if (iOffset > 0)
				{
					for (unsigned int i = iNum + 1; i <= cGroupList.size() - iOffset; i++)
					{
						if (!ItemExists(&cAffectedGroupList, cGroupList[i]))
						{
							cAffectedGroupList.push_back(cGroupList[i]);
						}
					}

					if (iNum + 1 < cGroupList.size())
					{
						cAffectedGroupList.push_back(cGroupList[iNum + 1]);
					}
				}
				break;
			}
		}
		delete pItem;
	}
	cGroupList.clear();

	// Aligning groups
	for (FileList::iterator it = cAffectedGroupList.begin(); it != cAffectedGroupList.end(); it++)
	{
		FileInfo* pFileInfo = *it;
		AlignGroup(pDownloadQueue, pFileInfo->GetNZBInfo());
	}
}

void QueueEditor::AlignGroup(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
	FileInfo* pLastFileInfo = NULL;
	unsigned int iLastNum = 0;
	unsigned int iNum = 0;
	while (iNum < pDownloadQueue->GetFileQueue()->size())
	{
		FileInfo* pFileInfo = pDownloadQueue->GetFileQueue()->at(iNum);
		if (pFileInfo->GetNZBInfo() == pNZBInfo)
		{
			if (pLastFileInfo && iNum - iLastNum > 1)
			{
				pDownloadQueue->GetFileQueue()->erase(pDownloadQueue->GetFileQueue()->begin() + iNum);
				pDownloadQueue->GetFileQueue()->insert(pDownloadQueue->GetFileQueue()->begin() + iLastNum + 1, pFileInfo);
				iLastNum++;
			}
			else
			{
				iLastNum = iNum;
			}
			pLastFileInfo = pFileInfo;
		}
		iNum++;
	}
}

void QueueEditor::PauseParsInGroups(ItemList* pItemList, bool bExtraParsOnly)
{
	while (true)
	{
		FileList GroupFileList;
		GroupFileList.clear();
		FileInfo* pFirstFileInfo = NULL;

		for (ItemList::iterator it = pItemList->begin(); it != pItemList->end(); )
		{
			EditItem* pItem = *it;
			if (!pFirstFileInfo || 
				(pFirstFileInfo->GetNZBInfo() == pItem->m_pFileInfo->GetNZBInfo()))
			{
				GroupFileList.push_back(pItem->m_pFileInfo);
				if (!pFirstFileInfo)
				{
					pFirstFileInfo = pItem->m_pFileInfo;
				}
				delete pItem;
				pItemList->erase(it);
				it = pItemList->begin();
				continue;
			}
			it++;
		}

		if (!GroupFileList.empty())
		{
			PausePars(&GroupFileList, bExtraParsOnly);
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
void QueueEditor::PausePars(FileList* pFileList, bool bExtraParsOnly)
{
	debug("QueueEditor: Pausing pars");
	
	FileList Pars, Vols;
	Pars.clear();
	Vols.clear();
			
	for (FileList::iterator it = pFileList->begin(); it != pFileList->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		char szLoFileName[1024];
		strncpy(szLoFileName, pFileInfo->GetFilename(), 1024);
		szLoFileName[1024-1] = '\0';
		for (char* p = szLoFileName; *p; p++) *p = tolower(*p); // convert string to lowercase
		
		if (strstr(szLoFileName, ".par2"))
		{
			if (!bExtraParsOnly)
			{
				pFileInfo->SetPaused(true);
			}
			else
			{
				if (strstr(szLoFileName, ".vol"))
				{
					Vols.push_back(pFileInfo);
				}
				else
				{
					Pars.push_back(pFileInfo);
				}
			}
		}
	}
	
	if (bExtraParsOnly)
	{
		if (!Pars.empty())
		{
			for (FileList::iterator it = Vols.begin(); it != Vols.end(); it++)
			{
				FileInfo* pFileInfo = *it;
				pFileInfo->SetPaused(true);
			}
		}
		else
		{
			// pausing all Vol-files except the smallest one
			FileInfo* pSmallest = NULL;
			for (FileList::iterator it = Vols.begin(); it != Vols.end(); it++)
			{
				FileInfo* pFileInfo = *it;
				if (!pSmallest)
				{
					pSmallest = pFileInfo;
				}
				else if (pSmallest->GetSize() > pFileInfo->GetSize())
				{
					pSmallest->SetPaused(true);
					pSmallest = pFileInfo;
				}
				else 
				{
					pFileInfo->SetPaused(true);
				}
			}
		}
	}
}

void QueueEditor::SetNZBCategory(NZBInfo* pNZBInfo, const char* szCategory)
{
	debug("QueueEditor: setting category '%s' for '%s'", szCategory, Util::BaseFileName(pNZBInfo->GetFilename()));

	g_pQueueCoordinator->SetQueueEntryNZBCategory(pNZBInfo, szCategory);
}

void QueueEditor::SetNZBName(NZBInfo* pNZBInfo, const char* szName)
{
	debug("QueueEditor: renaming '%s' to '%s'", Util::BaseFileName(pNZBInfo->GetFilename()), szName);

	g_pQueueCoordinator->SetQueueEntryNZBName(pNZBInfo, szName);
}

/**
* Check if deletion of already downloaded files is possible (when nzb id deleted from queue).
* The deletion is most always possible, except the case if all remaining files in queue 
* (belonging to this nzb-file) are PARS.
*/
bool QueueEditor::CanCleanupDisk(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
    for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
    {
        FileInfo* pFileInfo = *it;
		char szLoFileName[1024];
		strncpy(szLoFileName, pFileInfo->GetFilename(), 1024);
		szLoFileName[1024-1] = '\0';
		for (char* p = szLoFileName; *p; p++) *p = tolower(*p); // convert string to lowercase

		if (!strstr(szLoFileName, ".par2"))
		{
			// non-par file found
			return true;
		}
	}

	return false;
}

void QueueEditor::MergeGroups(DownloadQueue* pDownloadQueue, ItemList* pItemList)
{
	if (pItemList->size() == 0)
	{
		return;
	}

	EditItem* pDestItem = pItemList->front();

	for (ItemList::iterator it = pItemList->begin() + 1; it != pItemList->end(); it++)
	{
		EditItem* pItem = *it;
		if (pItem->m_pFileInfo->GetNZBInfo() != pDestItem->m_pFileInfo->GetNZBInfo())
		{
			debug("merge %s to %s", pItem->m_pFileInfo->GetNZBInfo()->GetFilename(), pDestItem->m_pFileInfo->GetNZBInfo()->GetFilename());
			g_pQueueCoordinator->MergeQueueEntries(pDestItem->m_pFileInfo->GetNZBInfo(), pItem->m_pFileInfo->GetNZBInfo());
		}
		delete pItem;
	}

	// align group
	AlignGroup(pDownloadQueue, pDestItem->m_pFileInfo->GetNZBInfo());

	delete pDestItem;
}

void QueueEditor::ReorderFiles(DownloadQueue* pDownloadQueue, ItemList* pItemList)
{
	if (pItemList->size() == 0)
	{
		return;
	}

	EditItem* pFirstItem = pItemList->front();
	NZBInfo* pNZBInfo = pFirstItem->m_pFileInfo->GetNZBInfo();
	unsigned int iInsertPos = 0;

	// find first file of the group
    for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
    {
        FileInfo* pFileInfo = *it;
		if (pFileInfo->GetNZBInfo() == pNZBInfo)
		{
			break;
		}
		iInsertPos++;
	}

	// now can reorder
	for (ItemList::iterator it = pItemList->begin(); it != pItemList->end(); it++)
	{
		EditItem* pItem = *it;
		FileInfo* pFileInfo = pItem->m_pFileInfo;

		// move file item
		for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
		{
			FileInfo* pFileInfo1 = *it;
			if (pFileInfo1 == pFileInfo)
			{
				pDownloadQueue->GetFileQueue()->erase(it);
				pDownloadQueue->GetFileQueue()->insert(pDownloadQueue->GetFileQueue()->begin() + iInsertPos, pFileInfo);
				iInsertPos++;				
				break;
			}
		}

		delete pItem;
	}
}

void QueueEditor::SetNZBParameter(NZBInfo* pNZBInfo, const char* szParamString)
{
	debug("QueueEditor: setting nzb parameter '%s' for '%s'", szParamString, Util::BaseFileName(pNZBInfo->GetFilename()));

	char* szStr = strdup(szParamString);

	char* szValue = strchr(szStr, '=');
	if (szValue)
	{
		*szValue = '\0';
		szValue++;
		pNZBInfo->SetParameter(szStr, szValue);
	}
	else
	{
		error("Could not set nzb parameter for %s: invalid argument: %s", pNZBInfo->GetName(), szParamString);
	}

	free(szStr);
}
