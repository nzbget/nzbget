/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
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


#ifndef NCURSESFRONTEND_H
#define NCURSESFRONTEND_H

#ifndef DISABLE_CURSES

#include <vector>
#include <time.h>

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

	bool				m_bUseColor;
	int					m_iDataUpdatePos;
    bool				m_bUpdateNextTime;
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
	char*				m_szHint;
	time_t				m_tStartHint;
	int					m_iColWidthFiles;
	int					m_iColWidthTotal;
	int					m_iColWidthLeft;

	// Inputting numbers
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
	void			ResetColWidths();
	void			PrintGroupname(NZBInfo* pNZBInfo, int iRow, bool bSelected, bool bCalcColWidth);
	void			PrintTopHeader(char* szHeader, int iLineNr, bool bUpTime);
	int				PrintMessage(Message* Msg, int iRow, int iMaxLines);
	void			PrintKeyInputBar();
	void 			PrintStatus();
	void			UpdateInput(int initialKey);
	void			Update(int iKey);
	void			SetCurrentQueueEntry(int iEntry);
	void			CalcWindowSizes();
	void			RefreshScreen();
	int				ReadConsoleKey();
	int				CalcQueueSize();
	void			NeedUpdateData();
	bool			EditQueue(DownloadQueue::EEditAction eAction, int iOffset);
	void			SetHint(const char* szHint);

protected:
	virtual void 	Run();

public:
					NCursesFrontend();
	virtual			~NCursesFrontend();
};

#endif

#endif
