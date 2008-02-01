/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2004  Sven Henkel <sidddy@users.sourceforge.net>
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


#ifndef NCURSESFRONTEND_H
#define NCURSESFRONTEND_H

#ifndef DISABLE_CURSES

#include <vector>

#include "Frontend.h"
#include "Log.h"
#include "DownloadInfo.h"

class NCursesFrontend : public Frontend
{
private:

	enum EInputMode
	{
	    eNormal,
	    eEditQueue,
	    eDownloadRate
	};

	class GroupInfo
	{
	private:
		int				m_iID;
		char* 			m_szNZBFilename;

	public:
		int		 		m_iFileCount;
		long long 		m_lSize;
		long long 		m_lRemainingSize;
		long long 		m_lPausedSize;
		int				m_iParCount;

	public:
						GroupInfo(int iID, const char* szNZBFilename);
						~GroupInfo();
		int				GetID() { return m_iID; }
		const char*		GetNZBFilename() { return m_szNZBFilename; }
		long long 		GetSize() { return m_lSize; }
		long long 		GetRemainingSize() { return m_lRemainingSize; }
		long long 		GetPausedSize() { return m_lPausedSize; }
	};

	typedef std::deque<GroupInfo*> GroupQueue;

	bool				m_bUseColor;
	int					m_iDataUpdatePos;
	int					m_iScreenHeight;
	int					m_iScreenWidth;
	int					m_iQueueWinTop;
	int					m_iQueueWinHeight;
	int					m_iQueueWinClientHeight;
	int					m_iMessagesWinTop;
	int					m_iMessagesWinHeight;
	int					m_iMessagesWinClientHeight;
	int					m_iSelectedQueueEntry;
	int					m_iLastEditEntry;
	bool				m_bLastPausePars;
	int					m_iQueueScrollOffset;
	GroupQueue			m_groupQueue;

	// Inputting numbres
	int					m_iInputNumberIndex;
	int					m_iInputValue;

#ifdef WIN32
	CHAR_INFO*			m_pScreenBuffer;
	CHAR_INFO*			m_pOldScreenBuffer;
	int					m_iScreenBufferSize;
	std::vector<WORD>	m_ColorAttr;
#else
	void* 				m_pWindow;  //  WINDOW*
#endif

	EInputMode			m_eInputMode;
	bool				m_bShowNZBname;
	bool				m_bShowTimestamp;
	bool				m_bGroupFiles;
	float				m_QueueWindowPercentage;

#ifdef WIN32
	void			init_pair(int iColorNumber, WORD wForeColor, WORD wBackColor);
#endif
	void			PlotLine(const char * szString, int iRow, int iPos, int iColorPair);
	void			PlotText(const char * szString, int iRow, int iPos, int iColorPair, bool bBlink);
	void			PrintMessages();
	void			PrintQueue();
	void			PrintFileQueue();
	void			PrintFilename(FileInfo* pFileInfo, int iRow, bool bSelected);
	void			PrintGroupQueue();
	void			PrintGroupname(GroupInfo * pGroupInfo, int iRow, bool bSelected);
	void			PrepareGroupQueue();
	void			PrintTopHeader(char* szHeader, int iLineNr, bool bUpTime);
	void			ClearGroupQueue();
	int				PrintMessage(Message* Msg, int iRow, int iMaxLines);
	void			PrintKeyInputBar();
	void 			PrintStatus();
	void			UpdateInput();
	void			Update();
	void			SetCurrentQueueEntry(int iEntry);
	void			CalcWindowSizes();
	void			FormatFileSize(char* szBuffer, int iBufLen, long long lFileSize);
	void			RefreshScreen();
	int				ReadConsoleKey();
	int				CalcQueueSize();
	void			NeedUpdateData();
	bool			EditQueue(QueueEditor::EEditAction eAction, int iOffset);

protected:
	virtual void 	Run();

public:
					NCursesFrontend();
	virtual			~NCursesFrontend();
};

#endif

#endif
