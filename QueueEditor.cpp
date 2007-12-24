/*
 *  This file if part of nzbget
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
#ifndef WIN32
#include <unistd.h>
#include <sys/time.h>
#endif

#include "nzbget.h"
#include "DownloadInfo.h"
#include "QueueEditor.h"
#include "QueueCoordinator.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"

extern QueueCoordinator* g_pQueueCoordinator;
extern Options* g_pOptions;

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
	bool bFound = false;
	int iEntry = 0;
	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
	{
		if (*it == pFileInfo)
		{
			bFound = true;
			break;
		}
		iEntry ++;
	}

	if (bFound)
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

bool QueueEditor::EditEntry(int ID, bool bSmartOrder, EEditAction eAction, int iOffset)
{
	IDList cIDList;
	cIDList.clear();
	cIDList.push_back(ID);
	return EditList(&cIDList, bSmartOrder, eAction, iOffset);
}

bool QueueEditor::EditList(IDList* pIDList, bool bSmartOrder, EEditAction eAction, int iOffset)
{
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	ItemList cItemList;
	PrepareList(pDownloadQueue, &cItemList, pIDList, bSmartOrder, eAction, iOffset);

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
				MoveEntry(pDownloadQueue, pItem->m_pFileInfo, pItem->m_iOffset);
				break;

			case eaFileMoveTop:
				MoveEntry(pDownloadQueue, pItem->m_pFileInfo, -MAX_ID);
				break;

			case eaFileMoveBottom:
				MoveEntry(pDownloadQueue, pItem->m_pFileInfo, +MAX_ID);
				break;

			case eaFileDelete:
				DeleteEntry(pItem->m_pFileInfo);
				break;

			case eaGroupPause:
			case eaGroupResume:
			case eaGroupDelete:
				EditGroup(pDownloadQueue, pItem->m_pFileInfo, eAction, 0);
				break;

			case eaGroupMoveOffset:
				//MoveGroup(pDownloadQueue, pItem->m_pFileInfo, iOffset); // not yet implemented
				return false;
				break;

			case eaGroupMoveTop:
				EditGroup(pDownloadQueue, pItem->m_pFileInfo, eaGroupMoveOffset, -MAX_ID);
				break;

			case eaGroupMoveBottom:
				EditGroup(pDownloadQueue, pItem->m_pFileInfo, eaGroupMoveOffset, +MAX_ID);
				break;
		}
		delete pItem;
	}

	if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
	{
		m_pDiskState->Save(pDownloadQueue, true);
	}

	g_pQueueCoordinator->UnlockQueue();

	return cItemList.size() > 0;
}

void QueueEditor::PrepareList(DownloadQueue* pDownloadQueue, ItemList* pItemList, IDList* pIDList, bool bSmartOrder, 
	EEditAction EEditAction, int iOffset)
{
	pItemList->reserve(pIDList->size());
	if (bSmartOrder && iOffset != 0 && EEditAction == eaFileMoveOffset)
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
	int iQueueSize = pDownloadQueue->size();
	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
	{
		FileInfo* pFileInfo2 = *it;
		if (!strcmp(pFileInfo2->GetNZBFilename(), pFileInfo->GetNZBFilename()))
		{
			cIDList.push_back(pFileInfo2->GetID());
		}
	}

	if (eAction == eaGroupMoveOffset && !(iOffset > iQueueSize || iOffset < -iQueueSize))
	{
		// currently Move-command can move only to Top or to Bottom, other offsets are not supported
		return false;
	}

	if (eAction == eaGroupPausePars)
	{
		// Pause-pars command not yet supported
		return false;
	}

	EEditAction GroupToFileMap[] = { (EEditAction)0, eaFileMoveOffset, eaFileMoveTop, eaFileMoveBottom, eaFilePause, eaFileResume, eaFileDelete,
		eaFileMoveOffset, eaFileMoveTop, eaFileMoveBottom, eaFilePause, eaFilePause, eaFileResume, eaFileDelete };

	return EditList(&cIDList, true, GroupToFileMap[eAction], iOffset);
}
