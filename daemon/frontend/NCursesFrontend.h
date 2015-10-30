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
	    normal,
	    editQueue,
	    downloadRate
	};

	bool				m_useColor;
	int					m_dataUpdatePos;
    bool				m_updateNextTime;
	int					m_screenHeight;
	int					m_screenWidth;
	int					m_queueWinTop;
	int					m_queueWinHeight;
	int					m_queueWinClientHeight;
	int					m_messagesWinTop;
	int					m_messagesWinHeight;
	int					m_messagesWinClientHeight;
	int					m_selectedQueueEntry;
	int					m_lastEditEntry;
	bool				m_lastPausePars;
	int					m_queueScrollOffset;
	char*				m_hint;
	time_t				m_startHint;
	int					m_colWidthFiles;
	int					m_colWidthTotal;
	int					m_colWidthLeft;

	// Inputting numbers
	int					m_inputNumberIndex;
	int					m_inputValue;

#ifdef WIN32
	CHAR_INFO*			m_screenBuffer;
	CHAR_INFO*			m_oldScreenBuffer;
	int					m_screenBufferSize;
	std::vector<WORD>	m_colorAttr;
#else
	void* 				m_window;  //  WINDOW*
#endif

	EInputMode			m_inputMode;
	bool				m_showNzbname;
	bool				m_showTimestamp;
	bool				m_groupFiles;
	int					m_queueWindowPercentage;

#ifdef WIN32
	void			init_pair(int colorNumber, WORD wForeColor, WORD wBackColor);
#endif
	void			PlotLine(const char * string, int row, int pos, int colorPair);
	void			PlotText(const char * string, int row, int pos, int colorPair, bool blink);
	void			PrintMessages();
	void			PrintQueue();
	void			PrintFileQueue();
	void			PrintFilename(FileInfo* fileInfo, int row, bool selected);
	void			PrintGroupQueue();
	void			ResetColWidths();
	void			PrintGroupname(NzbInfo* nzbInfo, int row, bool selected, bool calcColWidth);
	void			PrintTopHeader(char* header, int lineNr, bool upTime);
	int				PrintMessage(Message* Msg, int row, int maxLines);
	void			PrintKeyInputBar();
	void 			PrintStatus();
	void			UpdateInput(int initialKey);
	void			Update(int key);
	void			SetCurrentQueueEntry(int entry);
	void			CalcWindowSizes();
	void			RefreshScreen();
	int				ReadConsoleKey();
	int				CalcQueueSize();
	void			NeedUpdateData();
	bool			EditQueue(DownloadQueue::EEditAction action, int offset);
	void			SetHint(const char* hint);

protected:
	virtual void 	Run();

public:
					NCursesFrontend();
	virtual			~NCursesFrontend();
};

#endif

#endif
