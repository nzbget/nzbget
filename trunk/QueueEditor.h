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


#ifndef QUEUEEDITOR_H
#define QUEUEEDITOR_H

#include <vector>

#include "DownloadInfo.h"
#include "DiskState.h"
                                            
class QueueEditor
{
public:
	typedef std::vector<int> IDList;

	enum EEditAction
	{
		eaFileMoveOffset = 1,			// move to m_iOffset relative to the current position in queue
		eaFileMoveTop,
		eaFileMoveBottom,
		eaFilePause,
		eaFileResume,
		eaFileDelete,
		eaGroupMoveOffset,				// move to m_iOffset relative to the current position in queue
		eaGroupMoveTop,
		eaGroupMoveBottom,
		eaGroupPause,
		eaGroupPausePars,
		eaGroupResume,
		eaGroupDelete
	};

private:
	class EditItem
	{
	public:
		int		m_iID;
		int		m_iOffset;

		EditItem(int iID, int iOffset);
	};

	typedef std::vector<EditItem*> ItemList;

private:
	DiskState*				m_pDiskState;

	FileInfo*				FindFileInfo(DownloadQueue* pDownloadQueue, int iID);
	int						FindFileInfoEntry(DownloadQueue* pDownloadQueue, int iID);
	void					PrepareList(ItemList* pItemList, IDList* pIDList, bool bSmartOrder, EEditAction eAction, int iOffset);
	bool					EditGroup(int iID, EEditAction eAction, int iOffset);

	bool					PauseUnpauseEntry(int iID, bool bPause);
	bool					DeleteEntry(int iID);
	bool					MoveEntry(int iID, int iOffset);

public:
							QueueEditor();                
							~QueueEditor();
	void					SetDiskState(DiskState* diskState) { m_pDiskState = diskState; };

	bool					EditEntry(int ID, bool bSmartOrder, EEditAction eAction, int iOffset);
	bool					EditList(IDList* pIDList, bool bSmartOrder, EEditAction eAction, int iOffset);
};

#endif
