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
 * $Revision: 1 $
 * $Date: 2007-11-29 15:19:01 +0100 (Do, 29 Nov 2007) $
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

QueueEditor::EditItem::EditItem(int iID, int iOffset)
{
	m_iID = iID;
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

int QueueEditor::FindFileInfoEntry(DownloadQueue* pDownloadQueue, int iID)
{
	int iEntry = 0;
	for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
	{
		FileInfo* pFileInfo = *it;
		if (pFileInfo->GetID() == iID)
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
bool QueueEditor::PauseUnpauseEntry(int iID, bool bPause)
{
	bool bOK = false;
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	FileInfo* pFileInfo = FindFileInfo(pDownloadQueue, iID);

	if (pFileInfo)
	{
		pFileInfo->SetPaused(bPause);
		bOK = true;
	}

	if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
	{
		m_pDiskState->Save(pDownloadQueue, true);
	}

	g_pQueueCoordinator->UnlockQueue();
	return bOK;
}

/*
 * Removes entry with index iEntry
 * returns true if successful, false if operation is not possible
 */
bool QueueEditor::DeleteEntry(int iID)
{
	bool bOK = false;
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	FileInfo* pFileInfo = FindFileInfo(pDownloadQueue, iID);
	if (pFileInfo)
	{
		info("Deleting file %s from download queue", pFileInfo->GetFilename());
		g_pQueueCoordinator->DeleteQueueEntry(pFileInfo);
		bOK = true;
	}

	g_pQueueCoordinator->UnlockQueue();
	return bOK;
}

/*
 * Moves entry identified with iID in the queue
 * returns true if successful, false if operation is not possible
 */
bool QueueEditor::MoveEntry(int iID, int iOffset)
{
	bool bOK = false;
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();

	int iEntry = FindFileInfoEntry(pDownloadQueue, iID);
	if (iEntry >= 0)
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
			bOK = true;
		}
	}

	if (g_pOptions->GetSaveQueue() && g_pOptions->GetServerMode())
	{
		m_pDiskState->Save(pDownloadQueue, true);
	}

	g_pQueueCoordinator->UnlockQueue();
	return bOK;
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
	ItemList cItemList;
	PrepareList(&cItemList, pIDList, bSmartOrder, eAction, iOffset);

	for (ItemList::iterator it = cItemList.begin(); it != cItemList.end(); it++)
	{
		EditItem* pItem = *it;
		switch (eAction)
		{
			case eaFilePause:
				PauseUnpauseEntry(pItem->m_iID, true);
				break;

			case eaFileResume:
				PauseUnpauseEntry(pItem->m_iID, false);
				break;

			case eaFileMoveOffset:
				MoveEntry(pItem->m_iID, pItem->m_iOffset);
				break;

			case eaFileMoveTop:
				MoveEntry(pItem->m_iID, -MAX_ID);
				break;

			case eaFileMoveBottom:
				MoveEntry(pItem->m_iID, +MAX_ID);
				break;

			case eaFileDelete:
				DeleteEntry(pItem->m_iID);
				break;

			case eaGroupPause:
			case eaGroupResume:
			case eaGroupDelete:
				EditGroup(pItem->m_iID, eAction, 0);
				break;

			case eaGroupMoveOffset:
				//MoveGroup(pItem->m_iID, iOffset); // not yet implemented
				return false;
				break;

			case eaGroupMoveTop:
				EditGroup(pItem->m_iID, eaGroupMoveOffset, -MAX_ID);
				break;

			case eaGroupMoveBottom:
				EditGroup(pItem->m_iID, eaGroupMoveOffset, +MAX_ID);
				break;
		}
		delete pItem;
	}

	return cItemList.size() > 0;
}

void QueueEditor::PrepareList(ItemList* pItemList, IDList* pIDList, bool bSmartOrder, EEditAction EEditAction, int iOffset)
{
	pItemList->reserve(pIDList->size());
	if (bSmartOrder && iOffset != 0 && EEditAction == eaFileMoveOffset)
	{
		//add IDs to list in order they currently have in download queue
		int iLastDestPos = -1;
		DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();
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
					pItemList->push_back(new EditItem(iID, iWorkOffset));
					break;
				}
			}
		}
		g_pQueueCoordinator->UnlockQueue();
	}
	else
	{
		// check ID range
		int iMaxID = 0;
		int iMinID = MAX_ID;
		DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();
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
		g_pQueueCoordinator->UnlockQueue();

		//add IDs to list in order they were transmitted in command
		for (IDList::iterator it = pIDList->begin(); it != pIDList->end(); it++)
		{
			int iID = *it;
			if (iMinID <= iID && iID <= iMaxID)
			{
				pItemList->push_back(new EditItem(iID, iOffset));
			}
		}
	}
}

bool QueueEditor::EditGroup(int iID, EEditAction eAction, int iOffset)
{
	IDList cIDList;
	cIDList.clear();

	// collecting files belonging to group
	DownloadQueue* pDownloadQueue = g_pQueueCoordinator->LockQueue();
	int iQueueSize = pDownloadQueue->size();
	FileInfo* pFirstFileInfo = FindFileInfo(pDownloadQueue, iID);
	if (pFirstFileInfo)
	{
		for (DownloadQueue::iterator it = pDownloadQueue->begin(); it != pDownloadQueue->end(); it++)
		{
			FileInfo* pFileInfo = *it;
			if (!strcmp(pFirstFileInfo->GetNZBFilename(), pFileInfo->GetNZBFilename()))
			{
				cIDList.push_back(pFileInfo->GetID());
			}
		}
	}
	g_pQueueCoordinator->UnlockQueue();

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

	bool bOK = EditList(&cIDList, true, GroupToFileMap[eAction], iOffset);

	return bOK;
}
