/*
 *  This file is part of nzbget
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

static const int32_t NZBMESSAGE_SIGNATURE = 0x6E7A6202; // = "nzb2" (nzb version 2)
static const int NZBREQUESTFILENAMESIZE = 512;
static const int NZBREQUESTPASSWORDSIZE = 32;

/**
 * NZBGet communication protocol uses only two basic data types: integer and char.
 * Integer values are passed using network byte order (Big-Endian).
 * Use function "htonl" and "ntohl" to convert integers to/from machine 
 ' (host) byte order.
 * All char-strings ends with NULL-char.
 */

// Possible values for field "m_iType" of struct "SNZBRequestBase":
enum eRemoteRequest
{
	eRemoteRequestDownload = 1,
	eRemoteRequestPauseUnpause,
	eRemoteRequestList,
	eRemoteRequestSetDownloadRate,
	eRemoteRequestDumpDebug,
	eRemoteRequestEditQueue,
	eRemoteRequestLog,
	eRemoteRequestShutdown,
	eRemoteRequestVersion,
	eRemoteRequestPostQueue,
	eRemoteRequestWriteLog
};

// Possible values for field "m_iAction" of struct "SNZBEditQueueRequest":
// File-Actions affect one file, Group-Actions affect all files in group.
// Group is a list of files, added to queue from one NZB-File.
enum eRemoteEditAction
{
	eRemoteEditActionFileMoveOffset = 1,	// move to m_iOffset relative to the current position in queue
	eRemoteEditActionFileMoveTop,			// move to top of queue
	eRemoteEditActionFileMoveBottom,		// move to bottom of queue
	eRemoteEditActionFilePause,				// pause
	eRemoteEditActionFileResume,			// resume (unpause)
	eRemoteEditActionFileDelete,			// delete
	eRemoteEditActionFilePauseAllPars,		// pause only (all) pars (does not affect other files)
	eRemoteEditActionFilePauseExtraPars,	// pause only (almost all) pars, except main par-file (does not affect other files)
	eRemoteEditActionGroupMoveOffset,		// move to m_iOffset relative to the current position in queue
	eRemoteEditActionGroupMoveTop,			// move to top of queue
	eRemoteEditActionGroupMoveBottom,		// move to bottom of queue
	eRemoteEditActionGroupPause,			// pause
	eRemoteEditActionGroupResume,			// resume (unpause)
	eRemoteEditActionGroupDelete,			// delete
	eRemoteEditActionGroupPauseAllPars,		// pause only (all) pars (does not affect other files)
	eRemoteEditActionGroupPauseExtraPars	// pause only (almost all) pars, except main par-file (does not affect other files)
};

// The basic SNZBRequestBase struct, used in all requests
struct SNZBRequestBase
{
	int32_t					m_iSignature;			// Signature must be NZBMESSAGE_SIGNATURE in integer-value
	int32_t					m_iStructSize;			// Size of the entire struct
	int32_t					m_iType;				// Message type, see enum in NZBMessageRequest-namespace
	char					m_szPassword[NZBREQUESTPASSWORDSIZE];	// Password needs to be in every request
};

// The basic SNZBResposneBase struct, used in all responses
struct SNZBResponseBase
{
	int32_t					m_iSignature;			// Signature must be NZBMESSAGE_SIGNATURE in integer-value
	int32_t					m_iStructSize;			// Size of the entire struct
};

// A download request
struct SNZBDownloadRequest
{
	SNZBRequestBase			m_MessageBase;			// Must be the first in the struct
	char					m_szFilename[NZBREQUESTFILENAMESIZE];	// Name of nzb-file, may contain full path (local path on client) or only filename
	int32_t					m_bAddFirst;			// 1 - add file to the top of download queue
	int32_t					m_iTrailingDataLength;	// Length of nzb-file in bytes
	//char					m_szContent[m_iTrailingDataLength];	// variable sized
};

// A download response
struct SNZBDownloadResponse
{
	SNZBResponseBase		m_MessageBase;			// Must be the first in the struct
	int32_t					m_bSuccess;				// 0 - command failed, 1 - command executed successfully
	int32_t					m_iTrailingDataLength;	// Length of Text-string (m_szText), following to this record
	//char					m_szText[m_iTrailingDataLength];	// variable sized
};

// A list and status request
struct SNZBListRequest
{
	SNZBRequestBase			m_MessageBase;			// Must be the first in the struct
	int32_t					m_bFileList;			// 1 - return file list
	int32_t					m_bServerState;			// 1 - return server state
};

// A list response
struct SNZBListResponse
{
	SNZBResponseBase		m_MessageBase;			// Must be the first in the struct
	int32_t					m_iEntrySize;			// Size of the SNZBListResponseEntry-struct
	int32_t 				m_iRemainingSizeLo;		// Remaining size in bytes, Low 32-bits of 64-bit value
	int32_t 				m_iRemainingSizeHi;		// Remaining size in bytes, High 32-bits of 64-bit value
	int32_t					m_iDownloadRate;		// Current download speed, in Bytes pro Second
	int32_t					m_iDownloadLimit;		// Current download limit, in Bytes pro Second
	int32_t					m_bServerPaused;		// 1 - server is currently in paused-state
	int32_t					m_iThreadCount;			// Number of threads running
	int32_t					m_iPostJobCount;		// Number of jobs in post-processor queue (including current job)
	int32_t					m_iUpTimeSec;			// Server up time in seconds
	int32_t					m_iDownloadTimeSec;		// Server download time in seconds (up_time - standby_time)
	int32_t					m_iDownloadedBytesLo;	// Amount of data downloaded since server start, Low 32-bits of 64-bit value
	int32_t					m_iDownloadedBytesHi;	// Amount of data downloaded since server start, High 32-bits of 64-bit value
	int32_t					m_bServerStandBy;		// 0 - there are currently downloads running, 1 - no downloads in progress (server paused or all jobs completed)
	int32_t					m_iNrTrailingEntries;	// Number of List-entries, following to this structure
	int32_t					m_iTrailingDataLength;	// Length of all List-entries, following to this structure
	// SNZBListResponseEntry m_Entries[m_iNrTrailingEntries]		// variable sized
};

// A list response entry
struct SNZBListResponseEntry
{
	int32_t					m_iID;					// Entry-ID
	int32_t					m_iFileSizeLo;			// Filesize in bytes, Low 32-bits of 64-bit value
	int32_t					m_iFileSizeHi;			// Filesize in bytes, High 32-bits of 64-bit value
	int32_t					m_iRemainingSizeLo;		// Remaining size in bytes, Low 32-bits of 64-bit value
	int32_t					m_iRemainingSizeHi;		// Remaining size in bytes, High 32-bits of 64-bit value
	int32_t					m_bPaused;				// 1 - file is paused
	int32_t					m_bFilenameConfirmed;	// 1 - Filename confirmed (read from article body), 0 - Filename parsed from subject (can be changed after reading of article)
	int32_t					m_iNZBFilenameLen;		// Length of NZBFileName-string (m_szNZBFilename), following to this record
	int32_t					m_iSubjectLen;			// Length of Subject-string (m_szSubject), following to this record
	int32_t					m_iFilenameLen;			// Length of Filename-string (m_szFilename), following to this record
	int32_t					m_iDestDirLen;			// Length of DestDir-string (m_szDestDir), following to this record
	//char					m_szNZBFilename[m_iNZBFilenameLen];	// variable sized, may contain full path (local path on client) or only filename
	//char					m_szSubject[m_iSubjectLen];			// variable sized
	//char					m_szFilename[m_iFilenameLen];		// variable sized
	//char					m_szDestDir[m_iDestDirLen];			// variable sized
};

// A log request
struct SNZBLogRequest
{
	SNZBRequestBase			m_MessageBase;			// Must be the first in the struct
	int32_t					m_iIDFrom;				// Only one of these two parameters
	int32_t					m_iLines;				// can be set. The another one must be set to "0".
};

// A log response
struct SNZBLogResponse
{
	SNZBResponseBase		m_MessageBase;			// Must be the first in the struct
	int32_t					m_iEntrySize;			// Size of the SNZBLogResponseEntry-struct
	int32_t					m_iNrTrailingEntries;	// Number of Log-entries, following to this structure
	int32_t					m_iTrailingDataLength;	// Length of all Log-entries, following to this structure
	// SNZBLogResponseEntry m_Entries[m_iNrTrailingEntries]	// variable sized
};

// A log response entry
struct SNZBLogResponseEntry
{
	int32_t					m_iID;					// ID of Log-entry
	int32_t					m_iKind;				// see Message::Kind in "Log.h"
	int32_t					m_tTime;				// time since the Epoch (00:00:00 UTC, January 1, 1970), measured in seconds.
	int32_t					m_iTextLen;				// Length of Text-string (m_szText), following to this record
	//char					m_szText[m_iTextLen];	// variable sized
};

// A Pause/Unpause request
struct SNZBPauseUnpauseRequest
{
	SNZBRequestBase			m_MessageBase;			// Must be the first in the struct
	int32_t					m_bPause;				// 1 - server must be paused, 0 - server must be unpaused
};

// A Pause/Unpause response
struct SNZBPauseUnpauseResponse
{
	SNZBResponseBase		m_MessageBase;			// Must be the first in the struct
	int32_t					m_bSuccess;				// 0 - command failed, 1 - command executed successfully
	int32_t					m_iTrailingDataLength;	// Length of Text-string (m_szText), following to this record
	//char					m_szText[m_iTrailingDataLength];	// variable sized
};

// Request setting the download rate
struct SNZBSetDownloadRateRequest
{
	SNZBRequestBase			m_MessageBase;			// Must be the first in the struct
	int32_t					m_iDownloadRate;		// Speed limit, in Bytes pro Second
};

// A setting download rate response
struct SNZBSetDownloadRateResponse
{
	SNZBResponseBase		m_MessageBase;			// Must be the first in the struct
	int32_t					m_bSuccess;				// 0 - command failed, 1 - command executed successfully
	int32_t					m_iTrailingDataLength;	// Length of Text-string (m_szText), following to this record
	//char					m_szText[m_iTrailingDataLength];	// variable sized
};

// An edit queue request
struct SNZBEditQueueRequest
{
	SNZBRequestBase			m_MessageBase;			// Must be the first in the struct
	int32_t					m_iAction;				// Action to be executed, see enum in NZBMessageRequest-namespace
	int32_t					m_iOffset;				// Offset to move (for m_iAction = 0)
	int32_t					m_bSmartOrder;			// For Move-Actions: 0 - execute action for each ID in order they are placed in array;
													// 1 - smart execute to ensure that the relative order of all affected IDs are not changed.
	int32_t					m_iNrTrailingEntries;	// Number of ID-entries, following to this structure
	int32_t					m_iTrailingDataLength;	// Length of all ID-entries, following to this structure
	//int32_t				m_iIDs[m_iNrTrailingEntries];	// variable sized array of IDs. For File-Actions - ID of file, for Group-Actions - ID of any file belonging to group
};

// An edit queue response
struct SNZBEditQueueResponse
{
	SNZBResponseBase		m_MessageBase;			// Must be the first in the struct
	int32_t					m_bSuccess;				// 0 - command failed, 1 - command executed successfully
	int32_t					m_iTrailingDataLength;	// Length of Text-string (m_szText), following to this record
	//char					m_szText[m_iTrailingDataLength];	// variable sized
};

// Request dumping of debug info
struct SNZBDumpDebugRequest
{
	SNZBRequestBase			m_MessageBase;			// Must be the first in the struct
};

// Dumping of debug response
struct SNZBDumpDebugResponse
{
	SNZBResponseBase		m_MessageBase;			// Must be the first in the struct
	int32_t					m_bSuccess;				// 0 - command failed, 1 - command executed successfully
	int32_t					m_iTrailingDataLength;	// Length of Text-string (m_szText), following to this record
	//char					m_szText[m_iTrailingDataLength];	// variable sized
};

// Shutdown server request
struct SNZBShutdownRequest
{
	SNZBRequestBase			m_MessageBase;			// Must be the first in the struct
};

// Shutdown server response
struct SNZBShutdownResponse
{
	SNZBResponseBase		m_MessageBase;			// Must be the first in the struct
	int32_t					m_bSuccess;				// 0 - command failed, 1 - command executed successfully
	int32_t					m_iTrailingDataLength;	// Length of Text-string (m_szText), following to this record
	//char					m_szText[m_iTrailingDataLength];	// variable sized
};

// Server version request
struct SNZBVersionRequest
{
	SNZBRequestBase			m_MessageBase;			// Must be the first in the struct
};

// Server version  response
struct SNZBVersionResponse
{
	SNZBResponseBase		m_MessageBase;			// Must be the first in the struct
	int32_t					m_bSuccess;				// 0 - command failed, 1 - command executed successfully
	int32_t					m_iTrailingDataLength;	// Length of Text-string (m_szText), following to this record
	//char					m_szText[m_iTrailingDataLength];	// variable sized
};

// PostQueue request
struct SNZBPostQueueRequest
{
	SNZBRequestBase			m_MessageBase;			// Must be the first in the struct
};

// A PostQueue response
struct SNZBPostQueueResponse
{
	SNZBResponseBase		m_MessageBase;			// Must be the first in the struct
	int32_t					m_iEntrySize;			// Size of the SNZBPostQueueResponseEntry-struct
	int32_t					m_iNrTrailingEntries;	// Number of PostQueue-entries, following to this structure
	int32_t					m_iTrailingDataLength;	// Length of all PostQueue-entries, following to this structure
	// SNZBPostQueueResponseEntry m_Entries[m_iNrTrailingEntries]		// variable sized
};

// A PostQueue response entry
struct SNZBPostQueueResponseEntry
{
	int32_t					m_iID;					// ID of Post-entry
	int32_t					m_iStage;				// See PrePostProcessor::EPostJobStage
	int32_t					m_iStageProgress;		// Progress of current stage, value in range 0..1000
	int32_t					m_iFileProgress;		// Progress of current file, value in range 0..1000
	int32_t					m_iTotalTimeSec;		// Number of seconds this post-job is beeing processed (after it first changed the state from QUEUED).
	int32_t					m_iStageTimeSec;		// Number of seconds the current stage is beeing processed.
	int32_t					m_iNZBFilenameLen;		// Length of NZBFileName-string (m_szNZBFilename), following to this record
	int32_t					m_iParFilename;			// Length of ParFilename-string (m_szParFilename), following to this record
	int32_t					m_iInfoNameLen;			// Length of Filename-string (m_szFilename), following to this record
	int32_t					m_iDestDirLen;			// Length of DestDir-string (m_szDestDir), following to this record
	int32_t					m_iProgressLabelLen;	// Length of ProgressLabel-string (m_szProgressLabel), following to this record
	//char					m_szNZBFilename[m_iNZBFilenameLen];		// variable sized, may contain full path (local path on client) or only filename
	//char					m_szParFilename[m_iParFilename];		// variable sized
	//char					m_szInfoName[m_iInfoNameLen];			// variable sized
	//char					m_szDestDir[m_iDestDirLen];				// variable sized
	//char					m_szProgressLabel[m_iProgressLabelLen];	// variable sized
};

// Write log request
struct SNZBWriteLogRequest
{
	SNZBRequestBase			m_MessageBase;			// Must be the first in the struct
	int32_t					m_iKind;				// see Message::Kind in "Log.h"
	int32_t					m_iTrailingDataLength;	// Length of nzb-file in bytes
	//char					m_szText[m_iTrailingDataLength];	// variable sized
};

// Write log  response
struct SNZBWriteLogResponse
{
	SNZBResponseBase		m_MessageBase;			// Must be the first in the struct
	int32_t					m_bSuccess;				// 0 - command failed, 1 - command executed successfully
	int32_t					m_iTrailingDataLength;	// Length of Text-string (m_szText), following to this record
	//char					m_szText[m_iTrailingDataLength];	// variable sized
};

#endif
