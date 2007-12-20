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

#include <list>

#include "DownloadInfo.h"
#include "DiskState.h"
                                            
class QueueEditor
{
private:
	class EditItem
	{
	public:
		int		m_iID;
		int		m_iOffset;

		EditItem(int iID, int iOffset);
	};

	typedef std::list<EditItem*> ItemList;

	enum EAction
	{
		eaMove,				// move to m_iOffset relative to the current position in queue
		eaPause,			// pause
		eaResume,			// resume (unpause)
		eaDelete			// delete
	};

private:
	DiskState*				m_pDiskState;

	FileInfo*				FindFileInfo(DownloadQueue* pDownloadQueue, int iID);
	int						FindFileInfoEntry(DownloadQueue* pDownloadQueue, int iID);
	bool					EditList(int* pIDs, int iCount, bool bSmartOrder, EAction eAction, int iOffset);
	void					PrepareList(ItemList* pItemList, int* pIDs, int iCount, bool bSmartOrder, EAction eAction, int iOffset);

public:
							QueueEditor();                
							~QueueEditor();
	void					SetDiskState(DiskState* diskState) { m_pDiskState = diskState; };

	bool					PauseUnpauseEntry(int iID, bool bPause);
	bool					DeleteEntry(int iID);
	bool					MoveEntry(int iID, int iOffset, bool bAutoCorrection);

	bool					PauseUnpauseList(int* pIDs, int iCount, bool bPause);
	bool					DeleteList(int* pIDs, int iCount);
	bool					MoveList(int* pIDs, int iCount, bool SmartOrder, int iOffset);
};

#endif
