/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007  Andrei Prygounkov <hugbug@users.sourceforge.net>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Revision$
 * $Date$
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <cctype>
#include <cstdio>
#include <sys/stat.h>
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
	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
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
	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
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
 * Removes entry with index iEntry
 * returns true if successful, false if operation is not possible
 */
void QueueEditor::DeleteEntry(FileInfo* pFileInfo)
{
	info("Deleting file %s from download queue", pFileInfo->GetFilename());
	g_pQueueCoordinator->DeleteQueueEntry(pFileInfo);
}

/*
 * Moves entry identified with iID in the queue
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
		if ((unsigned int)iNewEntry > pDownloadQueue->size() - 1)
		{
			iNewEntry = (int)pDownloadQueue->size() - 1;
		}

		if (iNewEntry >= 0 && (unsigned int)iNewEntry <= pDownloadQueue->size() - 1)
		{
			FileInfo* fi = (*pDownloadQueue)[iEntry];
			pDownloadQueue->erase(pDownloadQueue->begin() + iEntry);
			pDownloadQueue->insert(pDownloadQueue->begin() + iNewEntry, fi);
		}
	}
}

bool QueueEditor::EditEntry(int ID, bool bSmartOrder, EEditAction eAction, int iOffset, const char* szText)
{
	IDList cIDList;
	cIDList.clear();
	cIDList.push_back(ID);
	return EditList(&cIDList, bSmartOrder, eAction, iOffset, szText);
}

bool QueueEditor::LockedEditEntry(DownloadQueue* pDownloadQueue, int ID, bool bSmartOrder, EEditAction eAction, int iOffset, const char* szText)
{
	IDList cIDList;
	cIDList.clear();
	cIDList.push_back(ID);
	return InternEditList(pDownloadQueue, &cIDList, bSmartOrder, eAction, iOffset, szText);
}

bool QueueEditor::EditList(IDList* pIDList, bool bSmartOrder, EEditAction eAction, int iOffset, const char* szText)
{
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	bool bOK = InternEditList(pDownloadQueue, pIDList, bSmartOrder, eAction, iOffset, szText);

	if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
	{
		g_pDiskState->SaveDownloadQueue(pDownloadQueue);
	}

	g_pQueueCoordinator->UnlockQueue();

	return bOK;
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

				case eaFilePauseAllPars:
				case eaFilePauseExtraPars:
					// remove compiler warning "enumeration not handled in switch"
					break;

				case eaGroupSetCategory:
					SetNZBCategory(pItem->m_pFileInfo->GetNZBInfo(), szText);
					break;

				case eaGroupPause:
				case eaGroupResume:
				case eaGroupDelete:
				case eaGroupMoveTop:
				case eaGroupMoveBottom:
				case eaGroupMoveOffset:
				case eaGroupPauseAllPars:
				case eaGroupPauseExtraPars:
					EditGroup(pDownloadQueue, pItem->m_pFileInfo, eAction, iOffset);
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
			iEnd = pDownloadQueue->size();
			iStep = 1;
		}
		else
		{
			iStart = pDownloadQueue->size() - 1;
			iEnd = -1;
			iStep = -1;
		}
		for (int iIndex = iStart; iIndex != iEnd; iIndex += iStep)
		{
			FileInfo* pFileInfo = (*pDownloadQueue)[iIndex];
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
						else if (iDestPos > int(pDownloadQueue->size()) - 1)
						{
							iWorkOffset = int(pDownloadQueue->size()) - 1 - iIndex;
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
		for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
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

bool QueueEditor::EditGroup(DownloadQueue* pDownloadQueue, FileInfo* pFileInfo, EEditAction eAction, int iOffset)
{
	IDList cIDList;
	cIDList.clear();

	// collecting files belonging to group
	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
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

	EEditAction GroupToFileMap[] = { (EEditAction)0, eaFileMoveOffset, eaFileMoveTop, eaFileMoveBottom, eaFilePause, eaFileResume, eaFileDelete, eaFilePauseAllPars, eaFilePauseExtraPars,
		eaFileMoveOffset, eaFileMoveTop, eaFileMoveBottom, eaFilePause, eaFileResume, eaFileDelete, eaFilePauseAllPars, eaFilePauseExtraPars };

	return InternEditList(pDownloadQueue, &cIDList, true, GroupToFileMap[eAction], iOffset, NULL);
}

void QueueEditor::BuildGroupList(DownloadQueue* pDownloadQueue, FileList* pGroupList)
{
	pGroupList->clear();
    for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
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
		AlignGroup(pDownloadQueue, pFileInfo);
	}
}

void QueueEditor::AlignGroup(DownloadQueue* pDownloadQueue, FileInfo* pFirstFileInfo)
{
	FileInfo* pLastFileInfo = NULL;
	unsigned int iLastNum = 0;
	unsigned int iNum = 0;
	while (iNum < pDownloadQueue->size())
	{
		FileInfo* pFileInfo = (*pDownloadQueue)[iNum];
		if (pFirstFileInfo->GetNZBInfo() == pFileInfo->GetNZBInfo())
		{
			if (pLastFileInfo && iNum - iLastNum > 1)
			{
				pDownloadQueue->erase(pDownloadQueue->begin() + iNum);
				pDownloadQueue->insert(pDownloadQueue->begin() + iLastNum + 1, pFileInfo);
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

/**
* Check if deletion of already downloaded files is possible (when nzb id deleted from queue).
* The deletion is most always possible, except the case if all remaining files in queue 
* (belonging to this nzb-file) are PARS.
*/
bool QueueEditor::CanCleanupDisk(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo)
{
    for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
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
