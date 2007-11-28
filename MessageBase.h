/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2005  Bo Cordes Petersen <placebodk@users.sourceforge.net>
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


#ifndef MESSAGEBASE_H
#define MESSAGEBASE_H

static const int32_t NZBMESSAGE_SIGNATURE = 0x6E7A6201; // = "nzb"-version-1
static const int NZBREQUESTFILENAMESIZE = 512;
static const int NZBREQUESTPASSWORDSIZE = 32;

// The pack-directive prevents aligning of structs.
// This makes them more portable and allows to use together servers and clients
// compiled on different cpu architectures
#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif

namespace NZBMessageRequest
{
enum
{
    eRequestDownload = 1,
    eRequestPauseUnpause,
    eRequestList,
    eRequestSetDownloadRate,
    eRequestDumpDebug,
    eRequestEditQueue,
    eRequestLog,
    eRequestShutdown
};

// Possible values for field "m_iAction" of struct "SNZBEditQueueRequest":
enum
{
    eActionMoveOffset = 1,	// move to m_iOffset relative to the current position in queue
    eActionMoveTop,			// move to top of queue
    eActionMoveBottom,		// move to bottom of queue
    eActionPause,			// pause
    eActionResume,			// resume (unpause)
    eActionDelete			// delete
};
}

// The basic NZBMessageBase struct
struct SNZBMessageBase
{
	int32_t					m_iId;		// Id must be 'nzbg' in integer-value
	int32_t					m_iType;	// message type, must be > 0
	int32_t					m_iSize;	// Size of the entire struct
	char					m_szPassword[ NZBREQUESTPASSWORDSIZE ];	// Password needs to be in every request
};

// A download request
struct SNZBDownloadRequest
{
	SNZBMessageBase			m_MessageBase;	// Must be the first in the struct
	char					m_szFilename[ NZBREQUESTFILENAMESIZE ];
	int32_t					m_bAddFirst;
	int32_t					m_iTrailingDataLength;
};

// A list request
struct SNZBListRequest
{
	SNZBMessageBase			m_MessageBase;	// Must be the first in the struct
	int32_t					m_bFileList;
	int32_t					m_bServerState;
};

// A list request-answer
struct SNZBListRequestAnswer
{
	int32_t					m_iSize;	// Size of the entire struct
	int32_t					m_iEntrySize;	// Size of the SNZBListRequestAnswerEntry-struct
	long long 				m_lRemainingSize;
	float					m_fDownloadRate;
	float					m_fDownloadLimit;
	int32_t					m_bServerPaused;
	int32_t					m_iThreadCount;
	int32_t					m_iNrTrailingEntries;
	int32_t					m_iTrailingDataLength;
};

// A list request-answer entry
struct SNZBListRequestAnswerEntry
{
	int32_t					m_iNZBFilenameLen;
	int32_t					m_iSubjectLen;
	int32_t					m_iFilenameLen;
	int32_t					m_iDestDirLen;
	int32_t					m_iFileSize;
	int32_t					m_bFilenameConfirmed;
	int32_t					m_iRemainingSize;
	int32_t					m_iID;
	int32_t					m_bPaused;
	//char					m_szNZBFilename[0]; // variable sized
	//char					m_szSubject[0]; // variable sized
	//char					m_szFilename[0]; // variable sized
	//char					m_szDestDir[0]; // variable sized
};

// A log request
struct SNZBLogRequest
{
	SNZBMessageBase			m_MessageBase;	// Must be the first in the struct
	int32_t					m_iIDFrom;      // Only one of these two parameters
	int32_t					m_iLines;		// can be set. The another one must be set to "0".
};

// A log request-answer
struct SNZBLogRequestAnswer
{
	int32_t					m_iSize;	// Size of the entire struct
	int32_t					m_iEntrySize;	// Size of the SNZBLogRequestAnswerEntry-struct
	int32_t					m_iNrTrailingEntries;
	int32_t					m_iTrailingDataLength;
};

// A log request-answer entry
struct SNZBLogRequestAnswerEntry
{
	int32_t					m_iTextLen;
	int32_t					m_iID;
	int32_t					m_iKind;     // see Message::Kind in "Log.h"
	time_t					m_tTime;
	//char					m_szText[0]; // variable sized
};

// A Pause/Unpause request
struct SNZBPauseUnpauseRequest
{
	SNZBMessageBase			m_MessageBase;	// Must be the first in the struct
	int32_t					m_bPause;		// The value g_bPause should be set to
};

// Request setting the download rate
struct SNZBSetDownloadRateRequest
{
	SNZBMessageBase			m_MessageBase;		// Must be the first in the struct
	float					m_fDownloadRate;
};

// A download request
struct SNZBEditQueueRequest
{
	SNZBMessageBase			m_MessageBase;	// Must be the first in the struct
	int32_t					m_iIDFrom;			// ID of the first file in the range
	int32_t					m_iIDTo;			// ID of the last file in the range, not used yet, must be same as m_iIDFrom
	int32_t					m_iAction;			// action to be done, see later
	int32_t					m_iOffset;			// Offset to move (for m_iAction = 0)
};

// Request dumping of debug info
struct SNZBDumpDebugRequest
{
	SNZBMessageBase			m_MessageBase;		// Must be the first in the struct
	int32_t					m_iLevel;			// Future use
};

#ifdef HAVE_PRAGMA_PACK
#pragma pack()
#endif

#endif
