/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include <sys/stat.h>
#include <set>
#ifndef WIN32
#include <unistd.h>
#include <sys/time.h>
#endif
#include <algorithm>

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

const int MAX_ID = 1000000000;

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
	for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
	{
		NZBInfo* pNZBInfo = *it;
		for (FileList::iterator it2 = pNZBInfo->GetFileList()->begin(); it2 != pNZBInfo->GetFileList()->end(); it2++)
		{
			FileInfo* pFileInfo = *it2;
			if (pFileInfo->GetID() == iID)
			{
				return pFileInfo;
			}
		}
	}
	return NULL;
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
	if (pFileInfo->GetNZBInfo()->GetDeleting())
	{
		detail("Deleting file %s from download queue", pFileInfo->GetFilename());
	}
	else
	{
		info("Deleting file %s from download queue", pFileInfo->GetFilename());
	}
	g_pQueueCoordinator->DeleteQueueEntry(pFileInfo);
}

/*
 * Moves entry in the queue
 * returns true if successful, false if operation is not possible
 */
void QueueEditor::MoveEntry(DownloadQueue* pDownloadQueue, FileInfo* pFileInfo, int iOffset)
{
	int iEntry = 0;
	for (FileList::iterator it = pFileInfo->GetNZBInfo()->GetFileList()->begin(); it != pFileInfo->GetNZBInfo()->GetFileList()->end(); it++)
	{
		FileInfo* pFileInfo2 = *it;
		if (pFileInfo2 == pFileInfo)
		{
			break;
		}
		iEntry ++;
	}

	int iNewEntry = iEntry + iOffset;
	int iSize = (int)pFileInfo->GetNZBInfo()->GetFileList()->size();

	if (iNewEntry < 0)
	{
		iNewEntry = 0;
	}
	if (iNewEntry > iSize - 1)
	{
		iNewEntry = (int)iSize - 1;
	}

	if (iNewEntry >= 0 && iNewEntry <= iSize - 1)
	{
		pFileInfo->GetNZBInfo()->GetFileList()->erase(pFileInfo->GetNZBInfo()->GetFileList()->begin() + iEntry);
		pFileInfo->GetNZBInfo()->GetFileList()->insert(pFileInfo->GetNZBInfo()->GetFileList()->begin() + iNewEntry, pFileInfo);
	}
}

/*
 * Moves group in the queue
 * returns true if successful, false if operation is not possible
 */
void QueueEditor::MoveGroup(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, int iOffset)
{
	int iEntry = 0;
	for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
	{
		NZBInfo* pNZBInfo2 = *it;
		if (pNZBInfo2 == pNZBInfo)
		{
			break;
		}
		iEntry ++;
	}

	int iNewEntry = iEntry + iOffset;
	int iSize = (int)pDownloadQueue->GetQueue()->size();

	if (iNewEntry < 0)
	{
		iNewEntry = 0;
	}
	if (iNewEntry > iSize - 1)
	{
		iNewEntry = (int)iSize - 1;
	}

	if (iNewEntry >= 0 && iNewEntry <= iSize - 1)
	{
		pDownloadQueue->GetQueue()->erase(pDownloadQueue->GetQueue()->begin() + iEntry);
		pDownloadQueue->GetQueue()->insert(pDownloadQueue->GetQueue()->begin() + iNewEntry, pNZBInfo);
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

bool QueueEditor::EditEntry(int ID, EEditAction eAction, int iOffset, const char* szText)
{
	IDList cIDList;
	cIDList.push_back(ID);
	return EditList(&cIDList, NULL, mmID, eAction, iOffset, szText);
}

bool QueueEditor::LockedEditEntry(DownloadQueue* pDownloadQueue, int ID, EEditAction eAction, int iOffset, const char* szText)
{
	IDList cIDList;
	cIDList.push_back(ID);
	return InternEditList(pDownloadQueue, NULL, &cIDList, eAction, iOffset, szText);
}

bool QueueEditor::EditList(IDList* pIDList, NameList* pNameList, EMatchMode eMatchMode, 
	EEditAction eAction, int iOffset, const char* szText)
{
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	bool bOK = true;

	if (pNameList)
	{
		pIDList = new IDList();
		bOK = BuildIDListFromNameList(pDownloadQueue, pIDList, pNameList, eMatchMode, eAction);
	}

	bOK = bOK && (InternEditList(pDownloadQueue, NULL, pIDList, eAction, iOffset, szText) || eMatchMode == mmRegEx);

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

bool QueueEditor::InternEditList(DownloadQueue* pDownloadQueue, ItemList* pItemList, 
	IDList* pIDList, EEditAction eAction, int iOffset, const char* szText)
{
	std::set<NZBInfo*> uniqueNzbs;

	ItemList itemList;
	if (!pItemList)
	{
		pItemList = &itemList;
		PrepareList(pDownloadQueue, pItemList, pIDList, eAction, iOffset);
	}

	switch (eAction)
	{
		case eaFilePauseAllPars:
		case eaFilePauseExtraPars:
			PauseParsInGroups(pItemList, eAction == eaFilePauseExtraPars);	
			break;

		case eaGroupMerge:
			return MergeGroups(pDownloadQueue, pItemList);

		case eaFileSplit:
			return SplitGroup(pDownloadQueue, pItemList, szText);

		case eaFileReorder:
			ReorderFiles(pDownloadQueue, pItemList);
			break;
		
		default:
			for (ItemList::iterator it = pItemList->begin(); it != pItemList->end(); it++)
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
						uniqueNzbs.insert(pItem->m_pFileInfo->GetNZBInfo());
						SetPriorityEntry(pItem->m_pFileInfo, szText);
						break;

					case eaGroupSetCategory:
						SetNZBCategory(pItem->m_pFileInfo->GetNZBInfo(), szText);
						break;

					case eaGroupSetName:
						SetNZBName(pItem->m_pFileInfo->GetNZBInfo(), szText);
						break;

					case eaGroupSetDupeKey:
					case eaGroupSetDupeScore:
					case eaGroupSetDupeMode:
						SetNZBDupeParam(pItem->m_pFileInfo->GetNZBInfo(), eAction, szText);
						break;

					case eaGroupSetParameter:
						SetNZBParameter(pItem->m_pFileInfo->GetNZBInfo(), szText);
						break;

					case eaGroupMoveTop:
					case eaGroupMoveBottom:
					case eaGroupMoveOffset:
						MoveGroup(pDownloadQueue, pItem->m_pFileInfo->GetNZBInfo(), pItem->m_iOffset);
						break;

					case eaGroupPause:
					case eaGroupResume:
					case eaGroupDelete:
					case eaGroupDupeDelete:
					case eaGroupFinalDelete:
					case eaGroupPauseAllPars:
					case eaGroupPauseExtraPars:
					case eaGroupSetPriority:
						EditGroup(pDownloadQueue, pItem->m_pFileInfo, eAction, iOffset, szText);
						break;

					case eaFilePauseAllPars:
					case eaFilePauseExtraPars:
					case eaGroupMerge:
					case eaFileReorder:
					case eaFileSplit:
						// remove compiler warning "enumeration not handled in switch"
						break;
				}
				delete pItem;
			}
	}

	if (eAction == eaFileSetPriority)
	{
		for (std::set<NZBInfo*>::iterator it = uniqueNzbs.begin(); it != uniqueNzbs.end(); it++)
		{
			NZBInfo* pNZBInfo = *it;
			pNZBInfo->CalcFileStats();
		}
	}

	return pItemList->size() > 0;
}

void QueueEditor::PrepareList(DownloadQueue* pDownloadQueue, ItemList* pItemList, IDList* pIDList,
	EEditAction eAction, int iOffset)
{
	if (eAction == eaFileMoveTop || eAction == eaGroupMoveTop)
	{
		iOffset = -MAX_ID;
	}
	else if (eAction == eaFileMoveBottom || eAction == eaGroupMoveBottom)
	{
		iOffset = MAX_ID;
	}

	pItemList->reserve(pIDList->size());
	if ((iOffset != 0) && 
		(eAction == eaFileMoveOffset || eAction == eaFileMoveTop || eAction == eaFileMoveBottom))
	{
		// add IDs to list in order they currently have in download queue
		for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
		{
			NZBInfo* pNZBInfo = *it;
			int iNrEntries = (int)pNZBInfo->GetFileList()->size();
			int iLastDestPos = -1;
			int iStart, iEnd, iStep;
			if (iOffset < 0)
			{
				iStart = 0;
				iEnd = iNrEntries;
				iStep = 1;
			}
			else
			{
				iStart = iNrEntries - 1;
				iEnd = -1;
				iStep = -1;
			}
			for (int iIndex = iStart; iIndex != iEnd; iIndex += iStep)
			{
				FileInfo* pFileInfo = pNZBInfo->GetFileList()->at(iIndex);
				IDList::iterator it2 = std::find(pIDList->begin(), pIDList->end(), pFileInfo->GetID());
				if (it2 != pIDList->end())
				{
					int iWorkOffset = iOffset;
					int iDestPos = iIndex + iWorkOffset;
					if (iLastDestPos == -1)
					{
						if (iDestPos < 0)
						{
							iWorkOffset = -iIndex;
						}
						else if (iDestPos > iNrEntries - 1)
						{
							iWorkOffset = iNrEntries - 1 - iIndex;
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
				}
			}
		}
	}
	else if ((iOffset != 0) && 
		(eAction == eaGroupMoveOffset || eAction == eaGroupMoveTop || eAction == eaGroupMoveBottom))
	{
		// add IDs to list in order they currently have in download queue
		// per group only one FileInfo is added to the list
		int iNrEntries = (int)pDownloadQueue->GetQueue()->size();
		int iLastDestPos = -1;
		int iStart, iEnd, iStep;
		if (iOffset < 0)
		{
			iStart = 0;
			iEnd = iNrEntries;
			iStep = 1;
		}
		else
		{
			iStart = iNrEntries - 1;
			iEnd = -1;
			iStep = -1;
		}
		for (int iIndex = iStart; iIndex != iEnd; iIndex += iStep)
		{
			NZBInfo* pNZBInfo = pDownloadQueue->GetQueue()->at(iIndex);
			for (FileList::iterator it = pNZBInfo->GetFileList()->begin(); it != pNZBInfo->GetFileList()->end(); it++)
			{
				FileInfo* pFileInfo = *it;
				IDList::iterator it2 = std::find(pIDList->begin(), pIDList->end(), pFileInfo->GetID());
				if (it2 != pIDList->end())
				{
					int iWorkOffset = iOffset;
					int iDestPos = iIndex + iWorkOffset;
					if (iLastDestPos == -1)
					{
						if (iDestPos < 0)
						{
							iWorkOffset = -iIndex;
						}
						else if (iDestPos > iNrEntries - 1)
						{
							iWorkOffset = iNrEntries - 1 - iIndex;
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
				}
			}
		}
	}
	else
	{
		// check ID range
		int iMaxID = 0;
		int iMinID = MAX_ID;
		for (NZBList::iterator it = pDownloadQueue->GetQueue()->begin(); it != pDownloadQueue->GetQueue()->end(); it++)
		{
			NZBInfo* pNZBInfo = *it;
			for (FileList::iterator it2 = pNZBInfo->GetFileList()->begin(); it2 != pNZBInfo->GetFileList()->end(); it2++)
			{
				FileInfo* pFileInfo = *it2;
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

		for (NZBList::iterator it3 = pDownloadQueue->GetQueue()->begin(); it3 != pDownloadQueue->GetQueue()->end(); it3++)
		{
			NZBInfo* pNZBInfo = *it3;
			for (FileList::iterator it2 = pNZBInfo->GetFileList()->begin(); it2 != pNZBInfo->GetFileList()->end(); it2++)
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
		}

		delete pRegEx;

		if (!bFound && (eMatchMode == mmName))
		{
			return false;
		}
	}

	return true;
}

bool QueueEditor::EditGroup(DownloadQueue* pDownloadQueue, FileInfo* pFileInfo, EEditAction eAction, int iOffset, const char* szText)
{
	ItemList itemList;
	bool bAllPaused = true;

	// collecting files belonging to group
	for (FileList::iterator it = pFileInfo->GetNZBInfo()->GetFileList()->begin(); it != pFileInfo->GetNZBInfo()->GetFileList()->end(); it++)
	{
		FileInfo* pFileInfo2 = *it;
		itemList.push_back(new EditItem(pFileInfo2, 0));
		bAllPaused &= pFileInfo2->GetPaused();
	}

	if (eAction == eaGroupDelete || eAction == eaGroupDupeDelete || eAction == eaGroupFinalDelete)
	{
		pFileInfo->GetNZBInfo()->SetDeleting(true);
		pFileInfo->GetNZBInfo()->SetAvoidHistory(eAction == eaGroupFinalDelete);
		pFileInfo->GetNZBInfo()->SetDeletePaused(bAllPaused);
		if (eAction == eaGroupDupeDelete)
		{
			pFileInfo->GetNZBInfo()->SetDeleteStatus(NZBInfo::dsDupe);
		}
		pFileInfo->GetNZBInfo()->SetCleanupDisk(CanCleanupDisk(pDownloadQueue, pFileInfo->GetNZBInfo()));
	}

	EEditAction GroupToFileMap[] = { (EEditAction)0, eaFileMoveOffset, eaFileMoveTop, eaFileMoveBottom, eaFilePause,
		eaFileResume, eaFileDelete, eaFilePauseAllPars, eaFilePauseExtraPars, eaFileSetPriority, eaFileReorder, eaFileSplit,
		eaFileMoveOffset, eaFileMoveTop, eaFileMoveBottom, eaFilePause, eaFileResume, eaFileDelete, eaFileDelete, eaFileDelete,
		eaFilePauseAllPars, eaFilePauseExtraPars, eaFileSetPriority,
		(EEditAction)0, (EEditAction)0, (EEditAction)0 };

	return InternEditList(pDownloadQueue, &itemList, NULL, GroupToFileMap[eAction], iOffset, szText);
}

void QueueEditor::PauseParsInGroups(ItemList* pItemList, bool bExtraParsOnly)
{
	while (true)
	{
		FileList GroupFileList;
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
    for (FileList::iterator it = pNZBInfo->GetFileList()->begin(); it != pNZBInfo->GetFileList()->end(); it++)
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

bool QueueEditor::MergeGroups(DownloadQueue* pDownloadQueue, ItemList* pItemList)
{
	if (pItemList->size() == 0)
	{
		return false;
	}

	bool bOK = true;

	EditItem* pDestItem = pItemList->front();

	for (ItemList::iterator it = pItemList->begin() + 1; it != pItemList->end(); it++)
	{
		EditItem* pItem = *it;
		if (pItem->m_pFileInfo->GetNZBInfo() != pDestItem->m_pFileInfo->GetNZBInfo())
		{
			debug("merge %s to %s", pItem->m_pFileInfo->GetNZBInfo()->GetFilename(), pDestItem->m_pFileInfo->GetNZBInfo()->GetFilename());
			if (g_pQueueCoordinator->MergeQueueEntries(pDestItem->m_pFileInfo->GetNZBInfo(), pItem->m_pFileInfo->GetNZBInfo()))
			{
				bOK = false;
			}
		}
		delete pItem;
	}

	delete pDestItem;
	return bOK;
}

bool QueueEditor::SplitGroup(DownloadQueue* pDownloadQueue, ItemList* pItemList, const char* szName)
{
	if (pItemList->size() == 0)
	{
		return false;
	}

	FileList fileList(false);

	for (ItemList::iterator it = pItemList->begin(); it != pItemList->end(); it++)
	{
		EditItem* pItem = *it;
		fileList.push_back(pItem->m_pFileInfo);
		delete pItem;
	}

	NZBInfo* pNewNZBInfo = NULL;
	bool bOK = g_pQueueCoordinator->SplitQueueEntries(&fileList, szName, &pNewNZBInfo);

	return bOK;
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

	// now can reorder
	for (ItemList::iterator it = pItemList->begin(); it != pItemList->end(); it++)
	{
		EditItem* pItem = *it;
		FileInfo* pFileInfo = pItem->m_pFileInfo;

		// move file item
		FileList::iterator it2 = std::find(pNZBInfo->GetFileList()->begin(), pNZBInfo->GetFileList()->end(), pFileInfo);
		if (it2 != pNZBInfo->GetFileList()->end())
		{
			pNZBInfo->GetFileList()->erase(it2);
			pNZBInfo->GetFileList()->insert(pNZBInfo->GetFileList()->begin() + iInsertPos, pFileInfo);
			iInsertPos++;				
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
		pNZBInfo->GetParameters()->SetParameter(szStr, szValue);
	}
	else
	{
		error("Could not set nzb parameter for %s: invalid argument: %s", pNZBInfo->GetName(), szParamString);
	}

	free(szStr);
}

void QueueEditor::SetNZBDupeParam(NZBInfo* pNZBInfo, EEditAction eAction, const char* szText)
{
	debug("QueueEditor: setting dupe parameter %i='%s' for '%s'", (int)eAction, szText, pNZBInfo->GetName());

	switch (eAction) 
	{
		case eaGroupSetDupeKey:
			pNZBInfo->SetDupeKey(szText);
			break;

		case eaGroupSetDupeScore:
			pNZBInfo->SetDupeScore(atoi(szText));
			break;

		case eaGroupSetDupeMode:
			{
				EDupeMode eMode = dmScore;
				if (!strcasecmp(szText, "SCORE"))
				{
					eMode = dmScore;
				}
				else if (!strcasecmp(szText, "ALL"))
				{
					eMode = dmAll;
				}
				else if (!strcasecmp(szText, "FORCE"))
				{
					eMode = dmForce;
				}
				else
				{
					error("Could not set duplicate mode for %s: incorrect mode (%s)", pNZBInfo->GetName(), szText);
					return;
				}
				pNZBInfo->SetDupeMode(eMode);
				break;
			}

		default:
			// suppress compiler warning
			break;
	}
}
