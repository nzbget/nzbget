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


#ifndef QUEUEEDITOR_H
#define QUEUEEDITOR_H

#include <vector>

#include "DownloadInfo.h"

class QueueEditor
{
public:
	// NOTE: changes to this enum must be synced with "eRemoteEditAction" in unit "MessageBase.h"
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
		eaFileSetPriority,
		eaFileReorder,
		eaFileSplit,
		eaGroupMoveOffset,				// move to m_iOffset relative to the current position in queue
		eaGroupMoveTop,
		eaGroupMoveBottom,
		eaGroupPause,
		eaGroupResume,
		eaGroupDelete,
		eaGroupDupeDelete,
		eaGroupFinalDelete,
		eaGroupPauseAllPars,
		eaGroupPauseExtraPars,
		eaGroupSetPriority,
		eaGroupSetCategory,
		eaGroupMerge,
		eaGroupSetParameter,
		eaGroupSetName,
		eaGroupSetDupeKey,
		eaGroupSetDupeScore,
		eaGroupSetDupeMode
	};

	enum EMatchMode
	{
		mmID = 1,
		mmName,
		mmRegEx
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
	bool					BuildIDListFromNameList(DownloadQueue* pDownloadQueue, IDList* pIDList, NameList* pNameList, EMatchMode eMatchMode, EEditAction eAction);
	bool					EditGroup(DownloadQueue* pDownloadQueue, FileInfo* pFileInfo, EEditAction eAction, int iOffset, const char* szText);
	void					BuildGroupList(DownloadQueue* pDownloadQueue, FileList* pGroupList);
	void					AlignAffectedGroups(DownloadQueue* pDownloadQueue, IDList* pIDList, bool bSmartOrder, int iOffset);
	bool					ItemExists(FileList* pFileList, FileInfo* pFileInfo);
	void					AlignGroup(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	void					PauseParsInGroups(ItemList* pItemList, bool bExtraParsOnly);
	void					PausePars(FileList* pFileList, bool bExtraParsOnly);
	void					SetNZBCategory(NZBInfo* pNZBInfo, const char* szCategory);
	void					SetNZBName(NZBInfo* pNZBInfo, const char* szName);
	bool					CanCleanupDisk(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	bool					MergeGroups(DownloadQueue* pDownloadQueue, ItemList* pItemList);
	bool					SplitGroup(DownloadQueue* pDownloadQueue, ItemList* pItemList, const char* szName);
	void					ReorderFiles(DownloadQueue* pDownloadQueue, ItemList* pItemList);
	void					SetNZBParameter(NZBInfo* pNZBInfo, const char* szParamString);
	void					SetNZBDupeParam(NZBInfo* pNZBInfo, EEditAction eAction, const char* szText);

	void					PauseUnpauseEntry(FileInfo* pFileInfo, bool bPause);
	void					DeleteEntry(FileInfo* pFileInfo);
	void					MoveEntry(DownloadQueue* pDownloadQueue, FileInfo* pFileInfo, int iOffset);
	void					SetPriorityEntry(FileInfo* pFileInfo, const char* szPriority);

public:
							QueueEditor();                
							~QueueEditor();

	bool					EditEntry(int ID, bool bSmartOrder, EEditAction eAction, int iOffset, const char* szText);
	bool					EditList(IDList* pIDList, NameList* pNameList, EMatchMode eMatchMode, bool bSmartOrder, EEditAction eAction, int iOffset, const char* szText);

	bool					LockedEditEntry(DownloadQueue* pDownloadQueue, int ID, bool bSmartOrder, EEditAction eAction, int iOffset, const char* szText);
	bool					LockedEditList(DownloadQueue* pDownloadQueue, IDList* pIDList, bool bSmartOrder, EEditAction eAction, int iOffset, const char* szText);
};

#endif
