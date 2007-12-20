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

bool QueueEditor::EditList(int* pIDs, int iCount, bool bSmartOrder, EEditAction EEditAction, int iOffset)
{
	ItemList cItemList;
	PrepareList(&cItemList, pIDs, iCount, bSmartOrder, EEditAction, iOffset);

	for (ItemList::iterator it = cItemList.begin(); it != cItemList.end(); it++)
	{
		EditItem* pItem = *it;
		switch (EEditAction)
		{
			case eaPause:
			case eaResume:
				PauseUnpauseEntry(pItem->m_iID, EEditAction == eaPause);
				break;

			case eaMove:
				MoveEntry(pItem->m_iID, pItem->m_iOffset);
				break;

			case eaDelete:
				DeleteEntry(pItem->m_iID);
				break;
		}
		delete pItem;
	}

	return cItemList.size() > 0;
}

void QueueEditor::PrepareList(ItemList* pItemList, int* pIDs, int iCount, bool bSmartOrder, EEditAction EEditAction, int iOffset)
{
	if (bSmartOrder && iOffset != 0 && EEditAction == eaMove)
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
			for (int i = 0; i < iCount; i++)
			{
				if (pFileInfo->GetID() == (int)pIDs[i])
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
					pItemList->push_back(new EditItem(pIDs[i], iWorkOffset));
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
		for (int i = 0; i < iCount; i++)
		{
			int ID = pIDs[i];
			if (iMinID <= ID && ID <= iMaxID)
			{
				pItemList->push_back(new EditItem(pIDs[i], iOffset));
			}
		}
	}
}

bool QueueEditor::EditGroup(int iID, EEditAction eAction, int iOffset)
{
	std::list<int> IDList;
	IDList.clear();

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
				IDList.push_back(pFileInfo->GetID());
			}
		}
	}
	g_pQueueCoordinator->UnlockQueue();

	if (eAction == eaMove && !(iOffset > iQueueSize || iOffset < -iQueueSize))
	{
		// currently Move-command can move only to Top or to Bottom, other offsets not supported
		return false;
	}

	int* pIDs = (int*)malloc(IDList.size() * sizeof(int));

	int* pPtr = pIDs;
	for (std::list<int>::iterator it = IDList.begin(); it != IDList.end(); it++)
	{
		*pPtr++ = *it;
	}

	bool bOK = EditList(pIDs, IDList.size(), true, eAction, iOffset);

	free(pIDs);
	return bOK;
}

bool QueueEditor::PauseUnpauseList(int* pIDs, int iCount, bool bPause)
{
	return EditList(pIDs, iCount, false, bPause ? eaPause : eaResume, 0);
}

bool QueueEditor::DeleteList(int* pIDs, int iCount)
{
	return EditList(pIDs, iCount, false, eaDelete, 0);
}

bool QueueEditor::MoveList(int* pIDs, int iCount, bool SmartOrder, int iOffset)
{
	return EditList(pIDs, iCount, SmartOrder, eaMove, iOffset);
}

bool QueueEditor::PauseUnpauseGroup(int iID, bool bPause)
{
	return EditGroup(iID, bPause ? eaPause : eaResume, 0);
}

bool QueueEditor::DeleteGroup(int iID)
{
	return EditGroup(iID, eaDelete, 0);
}

bool QueueEditor::MoveGroup(int iID, int iOffset)
{
	return EditGroup(iID, eaMove, iOffset);
}
