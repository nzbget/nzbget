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


#ifndef QUEUEEDITOR_H
#define QUEUEEDITOR_H

#include <vector>

#include "DownloadInfo.h"

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
		eaFilePauseAllPars,
		eaFilePauseExtraPars,
		eaGroupMoveOffset,				// move to m_iOffset relative to the current position in queue
		eaGroupMoveTop,
		eaGroupMoveBottom,
		eaGroupPause,
		eaGroupResume,
		eaGroupDelete,
		eaGroupPauseAllPars,
		eaGroupPauseExtraPars,
		eaGroupSetCategory
	};

private:
	class EditItem
	{
	public:
		int			m_iOffset;
		FileInfo*	m_pFileInfo;

		EditItem(FileInfo* pFileInfo, int iOffset);
	};

	typedef std::vector<EditItem*> ItemList;
	typedef std::vector<FileInfo*> FileList;

private:
	FileInfo*				FindFileInfo(DownloadQueue* pDownloadQueue, int iID);
	int						FindFileInfoEntry(DownloadQueue* pDownloadQueue, FileInfo* pFileInfo);
	bool					InternEditList(DownloadQueue* pDownloadQueue, IDList* pIDList, bool bSmartOrder, EEditAction eAction, int iOffset, const char* szText);
	void					PrepareList(DownloadQueue* pDownloadQueue, ItemList* pItemList, IDList* pIDList, bool bSmartOrder, EEditAction eAction, int iOffset);
	bool					EditGroup(DownloadQueue* pDownloadQueue, FileInfo* pFileInfo, EEditAction eAction, int iOffset);
	void					BuildGroupList(DownloadQueue* pDownloadQueue, FileList* pGroupList);
	void					AlignAffectedGroups(DownloadQueue* pDownloadQueue, IDList* pIDList, bool bSmartOrder, int iOffset);
	bool					ItemExists(FileList* pFileList, FileInfo* pFileInfo);
	void					AlignGroup(DownloadQueue* pDownloadQueue, FileInfo* pFirstFileInfo);
	void					PauseParsInGroups(ItemList* pItemList, bool bExtraParsOnly);
	void					PausePars(FileList* pFileList, bool bExtraParsOnly);
	void					SetNZBCategory(NZBInfo* pNZBInfo, const char* szCategory);
	bool					CanCleanupDisk(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);

	void					PauseUnpauseEntry(FileInfo* pFileInfo, bool bPause);
	void					DeleteEntry(FileInfo* pFileInfo);
	void					MoveEntry(DownloadQueue* pDownloadQueue, FileInfo* pFileInfo, int iOffset);

public:
							QueueEditor();                
							~QueueEditor();

	bool					EditEntry(int ID, bool bSmartOrder, EEditAction eAction, int iOffset, const char* szText);
	bool					EditList(IDList* pIDList, bool bSmartOrder, EEditAction eAction, int iOffset, const char* szText);

	bool					LockedEditEntry(DownloadQueue* pDownloadQueue, int ID, bool bSmartOrder, EEditAction eAction, int iOffset, const char* szText);
};

#endif
