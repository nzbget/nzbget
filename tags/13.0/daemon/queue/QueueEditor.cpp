/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "Options.h"
#include "Log.h"
#include "Util.h"
#include "QueueCoordinator.h"
#include "PrePostProcessor.h"
#include "HistoryCoordinator.h"
#include "UrlCoordinator.h"

extern QueueCoordinator* g_pQueueCoordinator;
extern HistoryCoordinator* g_pHistoryCoordinator;
extern UrlCoordinator* g_pUrlCoordinator;
extern PrePostProcessor* g_pPrePostProcessor;
extern Options* g_pOptions;

const int MAX_ID = 1000000000;

QueueEditor::EditItem::EditItem(FileInfo* pFileInfo, NZBInfo* pNZBInfo, int iOffset)
{
	m_pFileInfo = pFileInfo;
	m_pNZBInfo = pNZBInfo;
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

FileInfo* QueueEditor::FindFileInfo(int iID)
{
	for (NZBList::iterator it = m_pDownloadQueue->GetQueue()->begin(); it != m_pDownloadQueue->GetQueue()->end(); it++)
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
 */
void QueueEditor::PauseUnpauseEntry(FileInfo* pFileInfo, bool bPause)
{
	pFileInfo->SetPaused(bPause);
}

/*
 * Removes entry
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
	g_pQueueCoordinator->DeleteQueueEntry(m_pDownloadQueue, pFileInfo);
}

/*
 * Moves entry in the queue
 */
void QueueEditor::MoveEntry(FileInfo* pFileInfo, int iOffset)
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
 */
void QueueEditor::MoveGroup(NZBInfo* pNZBInfo, int iOffset)
{
	int iEntry = 0;
	for (NZBList::iterator it = m_pDownloadQueue->GetQueue()->begin(); it != m_pDownloadQueue->GetQueue()->end(); it++)
	{
		NZBInfo* pNZBInfo2 = *it;
		if (pNZBInfo2 == pNZBInfo)
		{
			break;
		}
		iEntry ++;
	}

	int iNewEntry = iEntry + iOffset;
	int iSize = (int)m_pDownloadQueue->GetQueue()->size();

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
		m_pDownloadQueue->GetQueue()->erase(m_pDownloadQueue->GetQueue()->begin() + iEntry);
		m_pDownloadQueue->GetQueue()->insert(m_pDownloadQueue->GetQueue()->begin() + iNewEntry, pNZBInfo);
	}
}

bool QueueEditor::EditEntry(DownloadQueue* pDownloadQueue, int ID, DownloadQueue::EEditAction eAction, int iOffset, const char* szText)
{
	m_pDownloadQueue = pDownloadQueue;
	IDList cIDList;
	cIDList.push_back(ID);
	return InternEditList(NULL, &cIDList, eAction, iOffset, szText);
}

bool QueueEditor::EditList(DownloadQueue* pDownloadQueue, IDList* pIDList, NameList* pNameList, DownloadQueue::EMatchMode eMatchMode,
	DownloadQueue::EEditAction eAction, int iOffset, const char* szText)
{
	if (eAction == DownloadQueue::eaPostDelete)
	{
		return g_pPrePostProcessor->EditList(pDownloadQueue, pIDList, eAction, iOffset, szText);
	}
	else if (DownloadQueue::eaHistoryDelete <= eAction && eAction <= DownloadQueue::eaHistoryMarkGood)
	{
		return g_pHistoryCoordinator->EditList(pDownloadQueue, pIDList, eAction, iOffset, szText);
	}

	m_pDownloadQueue = pDownloadQueue;
	bool bOK = true;

	if (pNameList)
	{
		pIDList = new IDList();
		bOK = BuildIDListFromNameList(pIDList, pNameList, eMatchMode, eAction);
	}

	bOK = bOK && (InternEditList(NULL, pIDList, eAction, iOffset, szText) || eMatchMode == DownloadQueue::mmRegEx);

	m_pDownloadQueue->Save();

	if (pNameList)
	{
		delete pIDList;
	}

	return bOK;
}

bool QueueEditor::InternEditList(ItemList* pItemList, 
	IDList* pIDList, DownloadQueue::EEditAction eAction, int iOffset, const char* szText)
{
	std::set<NZBInfo*> uniqueNzbs;

	ItemList itemList;
	if (!pItemList)
	{
		pItemList = &itemList;
		PrepareList(pItemList, pIDList, eAction, iOffset);
	}

	switch (eAction)
	{
		case DownloadQueue::eaFilePauseAllPars:
		case DownloadQueue::eaFilePauseExtraPars:
			PauseParsInGroups(pItemList, eAction == DownloadQueue::eaFilePauseExtraPars);	
			break;

		case DownloadQueue::eaGroupMerge:
			return MergeGroups(pItemList);

		case DownloadQueue::eaFileSplit:
			return SplitGroup(pItemList, szText);

		case DownloadQueue::eaFileReorder:
			ReorderFiles(pItemList);
			break;
		
		default:
			for (ItemList::iterator it = pItemList->begin(); it != pItemList->end(); it++)
			{
				EditItem* pItem = *it;
				switch (eAction)
				{
					case DownloadQueue::eaFilePause:
						PauseUnpauseEntry(pItem->m_pFileInfo, true);
						break;

					case DownloadQueue::eaFileResume:
						PauseUnpauseEntry(pItem->m_pFileInfo, false);
						break;

					case DownloadQueue::eaFileMoveOffset:
					case DownloadQueue::eaFileMoveTop:
					case DownloadQueue::eaFileMoveBottom:
						MoveEntry(pItem->m_pFileInfo, pItem->m_iOffset);
						break;

					case DownloadQueue::eaFileDelete:
						DeleteEntry(pItem->m_pFileInfo);
						break;

					case DownloadQueue::eaGroupSetPriority:
						SetNZBPriority(pItem->m_pNZBInfo, szText);
						break;

					case DownloadQueue::eaGroupSetCategory:
					case DownloadQueue::eaGroupApplyCategory:
						SetNZBCategory(pItem->m_pNZBInfo, szText, eAction == DownloadQueue::eaGroupApplyCategory);
						break;

					case DownloadQueue::eaGroupSetName:
						SetNZBName(pItem->m_pNZBInfo, szText);
						break;

					case DownloadQueue::eaGroupSetDupeKey:
					case DownloadQueue::eaGroupSetDupeScore:
					case DownloadQueue::eaGroupSetDupeMode:
						SetNZBDupeParam(pItem->m_pNZBInfo, eAction, szText);
						break;

					case DownloadQueue::eaGroupSetParameter:
						SetNZBParameter(pItem->m_pNZBInfo, szText);
						break;

					case DownloadQueue::eaGroupMoveTop:
					case DownloadQueue::eaGroupMoveBottom:
					case DownloadQueue::eaGroupMoveOffset:
						MoveGroup(pItem->m_pNZBInfo, pItem->m_iOffset);
						break;

					case DownloadQueue::eaGroupPause:
					case DownloadQueue::eaGroupResume:
					case DownloadQueue::eaGroupPauseAllPars:
					case DownloadQueue::eaGroupPauseExtraPars:
						EditGroup(pItem->m_pNZBInfo, eAction, iOffset, szText);
						break;

					case DownloadQueue::eaGroupDelete:
					case DownloadQueue::eaGroupDupeDelete:
					case DownloadQueue::eaGroupFinalDelete:
						if (pItem->m_pNZBInfo->GetKind() == NZBInfo::nkUrl)
						{
							DeleteUrl(pItem->m_pNZBInfo, eAction);
						}
						else
						{
							EditGroup(pItem->m_pNZBInfo, eAction, iOffset, szText);
						}


					default:
						// suppress compiler warning "enumeration not handled in switch"
						break;
				}
				delete pItem;
			}
	}

	return pItemList->size() > 0;
}

void QueueEditor::PrepareList(ItemList* pItemList, IDList* pIDList,
	DownloadQueue::EEditAction eAction, int iOffset)
{
	if (eAction == DownloadQueue::eaFileMoveTop || eAction == DownloadQueue::eaGroupMoveTop)
	{
		iOffset = -MAX_ID;
	}
	else if (eAction == DownloadQueue::eaFileMoveBottom || eAction == DownloadQueue::eaGroupMoveBottom)
	{
		iOffset = MAX_ID;
	}

	pItemList->reserve(pIDList->size());
	if ((iOffset != 0) && 
		(eAction == DownloadQueue::eaFileMoveOffset || eAction == DownloadQueue::eaFileMoveTop || eAction == DownloadQueue::eaFileMoveBottom))
	{
		// add IDs to list in order they currently have in download queue
		for (NZBList::iterator it = m_pDownloadQueue->GetQueue()->begin(); it != m_pDownloadQueue->GetQueue()->end(); it++)
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
					pItemList->push_back(new EditItem(pFileInfo, NULL, iWorkOffset));
				}
			}
		}
	}
	else if ((iOffset != 0) && 
		(eAction == DownloadQueue::eaGroupMoveOffset || eAction == DownloadQueue::eaGroupMoveTop || eAction == DownloadQueue::eaGroupMoveBottom))
	{
		// add IDs to list in order they currently have in download queue
		// per group only one FileInfo is added to the list
		int iNrEntries = (int)m_pDownloadQueue->GetQueue()->size();
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
			NZBInfo* pNZBInfo = m_pDownloadQueue->GetQueue()->at(iIndex);
			IDList::iterator it2 = std::find(pIDList->begin(), pIDList->end(), pNZBInfo->GetID());
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
				pItemList->push_back(new EditItem(NULL, pNZBInfo, iWorkOffset));
			}
		}
	}
	else if (eAction < DownloadQueue::eaGroupMoveOffset)
	{
		// check ID range
		int iMaxID = 0;
		int iMinID = MAX_ID;
		for (NZBList::iterator it = m_pDownloadQueue->GetQueue()->begin(); it != m_pDownloadQueue->GetQueue()->end(); it++)
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
				FileInfo* pFileInfo = FindFileInfo(iID);
				if (pFileInfo)
				{
					pItemList->push_back(new EditItem(pFileInfo, NULL, iOffset));
				}
			}
		}
	}
	else 
	{
		// check ID range
		int iMaxID = 0;
		int iMinID = MAX_ID;
		for (NZBList::iterator it = m_pDownloadQueue->GetQueue()->begin(); it != m_pDownloadQueue->GetQueue()->end(); it++)
		{
			NZBInfo* pNZBInfo = *it;
			int ID = pNZBInfo->GetID();
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
				for (NZBList::iterator it2 = m_pDownloadQueue->GetQueue()->begin(); it2 != m_pDownloadQueue->GetQueue()->end(); it2++)
				{
					NZBInfo* pNZBInfo = *it2;
					if (iID == pNZBInfo->GetID())
					{
						pItemList->push_back(new EditItem(NULL, pNZBInfo, iOffset));
					}
				}
			}
		}
	}
}

bool QueueEditor::BuildIDListFromNameList(IDList* pIDList, NameList* pNameList, DownloadQueue::EMatchMode eMatchMode, DownloadQueue::EEditAction eAction)
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
		if (eMatchMode == DownloadQueue::mmRegEx)
		{
			pRegEx = new RegEx(szName);
			if (!pRegEx->IsValid())
			{
				delete pRegEx;
				return false;
			}
		}

		bool bFound = false;

		for (NZBList::iterator it3 = m_pDownloadQueue->GetQueue()->begin(); it3 != m_pDownloadQueue->GetQueue()->end(); it3++)
		{
			NZBInfo* pNZBInfo = *it3;

			for (FileList::iterator it2 = pNZBInfo->GetFileList()->begin(); it2 != pNZBInfo->GetFileList()->end(); it2++)
			{
				FileInfo* pFileInfo = *it2;
				if (eAction < DownloadQueue::eaGroupMoveOffset)
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
			}

			if (eAction >= DownloadQueue::eaGroupMoveOffset)
			{
				// group action
				const char *szFilename = pNZBInfo->GetName();
				if (((!pRegEx && !strcmp(szFilename, szName)) || (pRegEx && pRegEx->Match(szFilename))) &&
					(uniqueIDs.find(pNZBInfo->GetID()) == uniqueIDs.end()))
				{
					uniqueIDs.insert(pNZBInfo->GetID());
					pIDList->push_back(pNZBInfo->GetID());
					bFound = true;
				}
			}
		}

		delete pRegEx;

		if (!bFound && (eMatchMode == DownloadQueue::mmName))
		{
			return false;
		}
	}

	return true;
}

bool QueueEditor::EditGroup(NZBInfo* pNZBInfo, DownloadQueue::EEditAction eAction, int iOffset, const char* szText)
{
	ItemList itemList;
	bool bAllPaused = true;

	// collecting files belonging to group
	for (FileList::iterator it = pNZBInfo->GetFileList()->begin(); it != pNZBInfo->GetFileList()->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		itemList.push_back(new EditItem(pFileInfo, NULL, 0));
		bAllPaused &= pFileInfo->GetPaused();
	}

	if (eAction == DownloadQueue::eaGroupDelete || eAction == DownloadQueue::eaGroupDupeDelete || eAction == DownloadQueue::eaGroupFinalDelete)
	{
		pNZBInfo->SetDeleting(true);
		pNZBInfo->SetAvoidHistory(eAction == DownloadQueue::eaGroupFinalDelete);
		pNZBInfo->SetDeletePaused(bAllPaused);
		if (eAction == DownloadQueue::eaGroupDupeDelete)
		{
			pNZBInfo->SetDeleteStatus(NZBInfo::dsDupe);
		}
		pNZBInfo->SetCleanupDisk(CanCleanupDisk(pNZBInfo));
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

	return InternEditList(&itemList, NULL, GroupToFileMap[eAction], iOffset, szText);
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

void QueueEditor::SetNZBPriority(NZBInfo* pNZBInfo, const char* szPriority)
{
	debug("Setting priority %s for %s", szPriority, pNZBInfo->GetName());

	int iPriority = atoi(szPriority);
	pNZBInfo->SetPriority(iPriority);
}

void QueueEditor::SetNZBCategory(NZBInfo* pNZBInfo, const char* szCategory, bool bApplyParams)
{
	debug("QueueEditor: setting category '%s' for '%s'", szCategory, pNZBInfo->GetName());

	bool bOldUnpack = g_pOptions->GetUnpack();
	const char* szOldPostScript = g_pOptions->GetPostScript();
	if (bApplyParams && !Util::EmptyStr(pNZBInfo->GetCategory()))
	{
		Options::Category* pCategory = g_pOptions->FindCategory(pNZBInfo->GetCategory(), false);
		if (pCategory)
		{
			bOldUnpack = pCategory->GetUnpack();
			if (!Util::EmptyStr(pCategory->GetPostScript()))
			{
				szOldPostScript = pCategory->GetPostScript();
			}
		}
	}

	g_pQueueCoordinator->SetQueueEntryCategory(m_pDownloadQueue, pNZBInfo, szCategory);

	if (!bApplyParams)
	{
		return;
	}

	bool bNewUnpack = g_pOptions->GetUnpack();
	const char* szNewPostScript = g_pOptions->GetPostScript();
	if (!Util::EmptyStr(pNZBInfo->GetCategory()))
	{
		Options::Category* pCategory = g_pOptions->FindCategory(pNZBInfo->GetCategory(), false);
		if (pCategory)
		{
			bNewUnpack = pCategory->GetUnpack();
			if (!Util::EmptyStr(pCategory->GetPostScript()))
			{
				szNewPostScript = pCategory->GetPostScript();
			}
		}
	}

	if (bOldUnpack != bNewUnpack)
	{
		pNZBInfo->GetParameters()->SetParameter("*Unpack:", bNewUnpack ? "yes" : "no");
	}
	
	if (strcasecmp(szOldPostScript, szNewPostScript))
	{
		// add new params not existed in old category
		Tokenizer tokNew(szNewPostScript, ",;");
		while (const char* szNewScriptName = tokNew.Next())
		{
			bool bFound = false;
			const char* szOldScriptName;
			Tokenizer tokOld(szOldPostScript, ",;");
			while ((szOldScriptName = tokOld.Next()) && !bFound)
			{
				bFound = !strcasecmp(szNewScriptName, szOldScriptName);
			}
			if (!bFound)
			{
				char szParam[1024];
				snprintf(szParam, 1024, "%s:", szNewScriptName);
				szParam[1024-1] = '\0';
				pNZBInfo->GetParameters()->SetParameter(szParam, "yes");
			}
		}

		// remove old params not existed in new category
		Tokenizer tokOld(szOldPostScript, ",;");
		while (const char* szOldScriptName = tokOld.Next())
		{
			bool bFound = false;
			const char* szNewScriptName;
			Tokenizer tokNew(szNewPostScript, ",;");
			while ((szNewScriptName = tokNew.Next()) && !bFound)
			{
				bFound = !strcasecmp(szNewScriptName, szOldScriptName);
			}
			if (!bFound)
			{
				char szParam[1024];
				snprintf(szParam, 1024, "%s:", szOldScriptName);
				szParam[1024-1] = '\0';
				pNZBInfo->GetParameters()->SetParameter(szParam, "no");
			}
		}
	}
}

void QueueEditor::SetNZBName(NZBInfo* pNZBInfo, const char* szName)
{
	debug("QueueEditor: renaming '%s' to '%s'", pNZBInfo->GetName(), szName);

	g_pQueueCoordinator->SetQueueEntryName(m_pDownloadQueue, pNZBInfo, szName);
}

/**
* Check if deletion of already downloaded files is possible (when nzb id deleted from queue).
* The deletion is most always possible, except the case if all remaining files in queue 
* (belonging to this nzb-file) are PARS.
*/
bool QueueEditor::CanCleanupDisk(NZBInfo* pNZBInfo)
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

bool QueueEditor::MergeGroups(ItemList* pItemList)
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
		if (pItem->m_pNZBInfo != pDestItem->m_pNZBInfo)
		{
			debug("merge %s to %s", pItem->m_pNZBInfo->GetFilename(), pDestItem->m_pNZBInfo->GetFilename());
			if (g_pQueueCoordinator->MergeQueueEntries(m_pDownloadQueue, pDestItem->m_pNZBInfo, pItem->m_pNZBInfo))
			{
				bOK = false;
			}
		}
		delete pItem;
	}

	delete pDestItem;
	return bOK;
}

bool QueueEditor::SplitGroup(ItemList* pItemList, const char* szName)
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
	bool bOK = g_pQueueCoordinator->SplitQueueEntries(m_pDownloadQueue, &fileList, szName, &pNewNZBInfo);

	return bOK;
}

void QueueEditor::ReorderFiles(ItemList* pItemList)
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

void QueueEditor::SetNZBDupeParam(NZBInfo* pNZBInfo, DownloadQueue::EEditAction eAction, const char* szText)
{
	debug("QueueEditor: setting dupe parameter %i='%s' for '%s'", (int)eAction, szText, pNZBInfo->GetName());

	switch (eAction) 
	{
		case DownloadQueue::eaGroupSetDupeKey:
			pNZBInfo->SetDupeKey(szText);
			break;

		case DownloadQueue::eaGroupSetDupeScore:
			pNZBInfo->SetDupeScore(atoi(szText));
			break;

		case DownloadQueue::eaGroupSetDupeMode:
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

bool QueueEditor::DeleteUrl(NZBInfo* pNZBInfo, DownloadQueue::EEditAction eAction)
{
	return g_pUrlCoordinator->DeleteQueueEntry(m_pDownloadQueue, pNZBInfo, eAction == DownloadQueue::eaGroupFinalDelete);
}
