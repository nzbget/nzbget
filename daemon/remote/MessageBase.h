/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2005 Bo Cordes Petersen <placebodk@users.sourceforge.net>
 *  Copyright (C) 2007-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef MESSAGEBASE_H
#define MESSAGEBASE_H

static const int32_t NZBMESSAGE_SIGNATURE = 0x6E7A6228; // = "nzb-XX" (protocol version)
static const int NZBREQUESTFILENAMESIZE = 512;
static const int NZBREQUESTPASSWORDSIZE = 32;

/**
 * NZBGet communication protocol uses only two basic data types: integer and char.
 * Integer values are passed using network byte order (Big-Endian).
 * Use function "htonl" and "ntohl" to convert integers to/from machine
 * (host) byte order.
 * All char-strings ends with NULL-char.
 *
 * NOTE:
 * NZBGet communication protocol is intended for usage only by NZBGet itself.
 * The communication works only if server and client has the same version.
 * The compatibility with previous program versions is not provided.
 * Third-party programs should use JSON-RPC or XML-RPC to communicate with NZBGet.
 */

// Possible values for field "m_iType" of struct "SNZBRequestBase":
enum ERemoteRequest
{
	rrDownload = 1,
	rrPauseUnpause,
	rrList,
	rrSetDownloadRate,
	rrDumpDebug,
	rrEditQueue,
	rrLog,
	rrShutdown,
	rrReload,
	rrVersion,
	rrPostQueue,
	rrWriteLog,
	rrScan,
	rrHistory
};

// Possible values for field "m_iAction" of struct "SNZBPauseUnpauseRequest":
enum ERemotePauseUnpauseAction
{
	rpDownload = 1,			// pause/unpause download queue
	rpPostProcess,			// pause/unpause post-processor queue
	rpScan					// pause/unpause scan of incoming nzb-directory
};

// Possible values for field "m_iMatchMode" of struct "SNZBEditQueueRequest":
enum ERemoteMatchMode
{
	rmId = 1,				// ID
	rmName,					// Name
	rmRegEx,				// RegEx
};

// The basic SNZBRequestBase struct, used in all requests
struct SNzbRequestBase
{
	int32_t					m_signature;			// Signature must be NZBMESSAGE_SIGNATURE in integer-value
	int32_t					m_structSize;			// Size of the entire struct
	int32_t					m_type;					// Message type, see enum in NZBMessageRequest-namespace
	char					m_username[NZBREQUESTPASSWORDSIZE];	// User name
	char					m_password[NZBREQUESTPASSWORDSIZE];	// Password
};

// The basic SNZBResposneBase struct, used in all responses
struct SNzbResponseBase
{
	int32_t					m_signature;			// Signature must be NZBMESSAGE_SIGNATURE in integer-value
	int32_t					m_structSize;			// Size of the entire struct
};

// A download request
struct SNzbDownloadRequest
{
	SNzbRequestBase			m_messageBase;			// Must be the first in the struct
	char					m_nzbFilename[NZBREQUESTFILENAMESIZE];// Name of nzb-file. For URLs can be empty, then the filename is read from URL download response
	char					m_category[NZBREQUESTFILENAMESIZE];	// Category, can be empty
	int32_t					m_addFirst;				// 1 - add file to the top of download queue
	int32_t					m_addPaused;			// 1 - pause added files
	int32_t					m_priority;				// Priority for files (0 - default)
	int32_t					m_dupeScore;			// Duplicate score
	int32_t					m_dupeMode;				// Duplicate mode (EDupeMode)
	char					m_dupeKey[NZBREQUESTFILENAMESIZE];	// Duplicate key
	int32_t					m_trailingDataLength;	// Length of nzb-file in bytes
	//char					m_content[m_trailingDataLength];	// variable sized
};

// A download response
struct SNzbDownloadResponse
{
	SNzbResponseBase		m_messageBase;			// Must be the first in the struct
	int32_t					m_success;				// 0 - command failed, 1 - command executed successfully
	int32_t					m_trailingDataLength;	// Length of Text-string (m_szText), following to this record
	//char					m_text[m_trailingDataLength];	// variable sized
};

// A list and status request
struct SNzbListRequest
{
	SNzbRequestBase			m_messageBase;			// Must be the first in the struct
	int32_t					m_fileList;				// 1 - return file list
	int32_t					m_serverState;			// 1 - return server state
	int32_t					m_matchMode;			// File/Group match mode, see enum eRemoteMatchMode (only values eRemoteMatchModeID (no filter) and eRemoteMatchModeRegEx are allowed)
	int32_t					m_matchGroup;			// 0 - match files; 1 - match nzbs (when m_iMatchMode == eRemoteMatchModeRegEx)
	char					m_pattern[NZBREQUESTFILENAMESIZE];	// RegEx Pattern (when m_iMatchMode == eRemoteMatchModeRegEx)
};

// A list response
struct SNzbListResponse
{
	SNzbResponseBase		m_messageBase;			// Must be the first in the struct
	int32_t					m_entrySize;			// Size of the SNZBListResponseEntry-struct
	int32_t 				m_remainingSizeLo;		// Remaining size in bytes, Low 32-bits of 64-bit value
	int32_t 				m_remainingSizeHi;		// Remaining size in bytes, High 32-bits of 64-bit value
	int32_t					m_downloadRate;			// Current download speed, in Bytes pro Second
	int32_t					m_downloadLimit;		// Current download limit, in Bytes pro Second
	int32_t					m_downloadPaused;		// 1 - download queue is currently in paused-state
	int32_t					m_download2Paused;		// 1 - download queue is currently in paused-state (second pause-register)
	int32_t					m_downloadStandBy;		// 0 - there are currently downloads running, 1 - no downloads in progress (download queue paused or all download jobs completed)
	int32_t					m_postPaused;			// 1 - post-processor queue is currently in paused-state
	int32_t					m_scanPaused;			// 1 - scaning of incoming directory is currently in paused-state
	int32_t					m_threadCount;			// Number of threads running
	int32_t					m_postJobCount;			// Number of jobs in post-processor queue (including current job)
	int32_t					m_upTimeSec;			// Server up time in seconds
	int32_t					m_downloadTimeSec;		// Server download time in seconds (up_time - standby_time)
	int32_t					m_downloadedBytesLo;	// Amount of data downloaded since server start, Low 32-bits of 64-bit value
	int32_t					m_downloadedBytesHi;	// Amount of data downloaded since server start, High 32-bits of 64-bit value
	int32_t					m_regExValid;			// 0 - error in RegEx-pattern, 1 - RegEx-pattern is valid (only when Request has eRemoteMatchModeRegEx)
	int32_t					m_nrTrailingNzbEntries;	// Number of List-NZB-entries, following to this structure
	int32_t					m_nrTrailingPPPEntries;	// Number of List-PPP-entries, following to this structure
	int32_t					m_nrTrailingFileEntries;	// Number of List-File-entries, following to this structure
	int32_t					m_trailingDataLength;		// Length of all List-entries, following to this structure
	// SNzbListResponseEntry m_nzbEntries[m_nrTrailingNZBEntries]			// variable sized
	// SNzbListResponseEntry m_pppEntries[m_nrTrailingPPPEntries]			// variable sized
	// SNzbListResponseEntry m_fileEntries[m_nrTrailingFileEntries]		// variable sized
};

// A list response nzb entry
struct SNzbListResponseNzbEntry
{
	int32_t					m_id;					// NZB-ID
	int32_t					m_kind;					// Item Kind (see NZBInfo::Kind)
	int32_t					m_sizeLo;				// Size of all files in bytes, Low 32-bits of 64-bit value
	int32_t					m_sizeHi;				// Size of all files in bytes, High 32-bits of 64-bit value
	int32_t					m_remainingSizeLo;		// Size of remaining (unpaused) files in bytes, Low 32-bits of 64-bit value
	int32_t					m_remainingSizeHi;		// Size of remaining (unpaused) files in bytes, High 32-bits of 64-bit value
	int32_t					m_pausedSizeLo;			// Size of npaused files in bytes, Low 32-bits of 64-bit value
	int32_t					m_pausedSizeHi;			// Size of paused files in bytes, High 32-bits of 64-bit value
	int32_t					m_pausedCount;			// Number of paused files
	int32_t					m_remainingParCount;	// Number of remaining par-files
	int32_t					m_priority;				// Download priority
	int32_t					m_match;				// 1 - group matches the pattern (only when Request has eRemoteMatchModeRegEx)
	int32_t					m_filenameLen;			// Length of Filename-string (m_szFilename), following to this record
	int32_t					m_nameLen;				// Length of Name-string (m_szName), following to this record
	int32_t					m_destDirLen;			// Length of DestDir-string (m_szDestDir), following to this record
	int32_t					m_categoryLen;			// Length of Category-string (m_szCategory), following to this record
	int32_t					m_queuedFilenameLen;	// Length of queued file name (m_szQueuedFilename), following to this record
	//char					m_filename[m_filenameLen];				// variable sized
	//char					m_name[m_nameLen];						// variable sized
	//char					m_destDir[m_destDirLen];				// variable sized
	//char					m_dategory[m_categoryLen];				// variable sized
	//char					m_queuedFilename[m_queuedFilenameLen];	// variable sized
};

// A list response pp-parameter entry
struct SNzbListResponsePPPEntry
{
	int32_t					m_nzbIndex;				// Index of NZB-Entry in m_NZBEntries-list
	int32_t					m_nameLen;				// Length of Name-string (m_szName), following to this record
	int32_t					m_valueLen;				// Length of Value-string (m_szValue), following to this record
	//char					m_name[m_nameLen];		// variable sized
	//char					m_value[m_valueLen];	// variable sized
};

// A list response file entry
struct SNzbListResponseFileEntry
{
	int32_t					m_id;					// Entry-ID
	int32_t					m_nzbIndex;				// Index of NZB-Entry in m_NZBEntries-list
	int32_t					m_fileSizeLo;			// Filesize in bytes, Low 32-bits of 64-bit value
	int32_t					m_fileSizeHi;			// Filesize in bytes, High 32-bits of 64-bit value
	int32_t					m_remainingSizeLo;		// Remaining size in bytes, Low 32-bits of 64-bit value
	int32_t					m_remainingSizeHi;		// Remaining size in bytes, High 32-bits of 64-bit value
	int32_t					m_paused;				// 1 - file is paused
	int32_t					m_filenameConfirmed;	// 1 - Filename confirmed (read from article body), 0 - Filename parsed from subject (can be changed after reading of article)
	int32_t					m_activeDownloads;		// Number of active downloads for this file
	int32_t					m_match;				// 1 - file matches the pattern (only when Request has eRemoteMatchModeRegEx)
	int32_t					m_subjectLen;			// Length of Subject-string (m_szSubject), following to this record
	int32_t					m_filenameLen;			// Length of Filename-string (m_szFilename), following to this record
	//char					m_subject[m_subjectLen];		// variable sized
	//char					m_filename[m_filenameLen];		// variable sized
};

// A log request
struct SNzbLogRequest
{
	SNzbRequestBase			m_messageBase;			// Must be the first in the struct
	int32_t					m_idFrom;				// Only one of these two parameters
	int32_t					m_lines;				// can be set. The another one must be set to "0".
};

// A log response
struct SNzbLogResponse
{
	SNzbResponseBase		m_messageBase;			// Must be the first in the struct
	int32_t					m_entrySize;			// Size of the SNZBLogResponseEntry-struct
	int32_t					m_nrTrailingEntries;	// Number of Log-entries, following to this structure
	int32_t					m_trailingDataLength;	// Length of all Log-entries, following to this structure
	// SNZBLogResponseEntry m_entries[m_trailingEntries]	// variable sized
};

// A log response entry
struct SNzbLogResponseEntry
{
	int32_t					m_id;					// ID of Log-entry
	int32_t					m_kind;					// see Message::Kind in "Log.h"
	int32_t					m_time;					// time since the Epoch (00:00:00 UTC, January 1, 1970), measured in seconds.
	int32_t					m_textLen;				// Length of Text-string (m_szText), following to this record
	//char					m_text[m_textLen];		// variable sized
};

// A Pause/Unpause request
struct SNzbPauseUnpauseRequest
{
	SNzbRequestBase			m_messageBase;			// Must be the first in the struct
	int32_t					m_pause;				// 1 - server must be paused, 0 - server must be unpaused
	int32_t					m_action;				// Action to be executed, see enum ERemotePauseUnpauseAction
};

// A Pause/Unpause response
struct SNzbPauseUnpauseResponse
{
	SNzbResponseBase		m_messageBase;			// Must be the first in the struct
	int32_t					m_success;				// 0 - command failed, 1 - command executed successfully
	int32_t					m_trailingDataLength;	// Length of Text-string (m_szText), following to this record
	//char					m_text[m_trailingDataLength];	// variable sized
};

// Request setting the download rate
struct SNzbSetDownloadRateRequest
{
	SNzbRequestBase			m_messageBase;			// Must be the first in the struct
	int32_t					m_downloadRate;			// Speed limit, in Bytes pro Second
};

// A setting download rate response
struct SNzbSetDownloadRateResponse
{
	SNzbResponseBase		m_messageBase;			// Must be the first in the struct
	int32_t					m_success;				// 0 - command failed, 1 - command executed successfully
	int32_t					m_trailingDataLength;	// Length of Text-string (m_szText), following to this record
	//char					m_text[m_trailingDataLength];	// variable sized
};

// edit queue request
struct SNzbEditQueueRequest
{
	SNzbRequestBase			m_messageBase;				// Must be the first in the struct
	int32_t					m_action;					// Action to be executed, see enum DownloadQueue::EEditAction
	int32_t					m_offset;					// Offset to move (for m_iAction = 0)
	int32_t					m_matchMode;				// File/Group match mode, see enum eRemoteMatchMode
	int32_t					m_nrTrailingIdEntries;		// Number of ID-entries, following to this structure
	int32_t					m_nrTrailingNameEntries;	// Number of Name-entries, following to this structure
	int32_t					m_trailingNameEntriesLen;	// Length of all Name-entries, following to this structure
	int32_t					m_textLen;					// Length of Text-string (m_szText), following to this record
	int32_t					m_trailingDataLength;		// Length of Text-string and all ID-entries, following to this structure
	//char					m_text[m_textLen];			// variable sized
	//int32_t				m_ids[m_nrTrailingIdEntries];		// variable sized array of IDs. For File-Actions - ID of file, for Group-Actions - ID of any file belonging to group
	//char*					m_names[m_nrTrailingNameEntries];	// variable sized array of strings. For File-Actions - name of file incl. nzb-name as path, for Group-Actions - name of group
};

// An edit queue response
struct SNzbEditQueueResponse
{
	SNzbResponseBase		m_messageBase;			// Must be the first in the struct
	int32_t					m_success;				// 0 - command failed, 1 - command executed successfully
	int32_t					m_trailingDataLength;	// Length of Text-string (m_szText), following to this record
	//char					m_text[m_trailingDataLength];	// variable sized
};

// Request dumping of debug info
struct SNzbDumpDebugRequest
{
	SNzbRequestBase			m_messageBase;			// Must be the first in the struct
};

// Dumping of debug response
struct SNzbDumpDebugResponse
{
	SNzbResponseBase		m_messageBase;			// Must be the first in the struct
	int32_t					m_success;				// 0 - command failed, 1 - command executed successfully
	int32_t					m_trailingDataLength;	// Length of Text-string (m_szText), following to this record
	//char					m_text[m_trailingDataLength];	// variable sized
};

// Shutdown server request
struct SNzbShutdownRequest
{
	SNzbRequestBase			m_messageBase;			// Must be the first in the struct
};

// Shutdown server response
struct SNzbShutdownResponse
{
	SNzbResponseBase		m_messageBase;			// Must be the first in the struct
	int32_t					m_success;				// 0 - command failed, 1 - command executed successfully
	int32_t					m_trailingDataLength;	// Length of Text-string (m_szText), following to this record
	//char					m_text[m_trailingDataLength];	// variable sized
};

// Reload server request
struct SNzbReloadRequest
{
	SNzbRequestBase			m_messageBase;			// Must be the first in the struct
};

// Reload server response
struct SNzbReloadResponse
{
	SNzbResponseBase		m_messageBase;			// Must be the first in the struct
	int32_t					m_success;				// 0 - command failed, 1 - command executed successfully
	int32_t					m_trailingDataLength;	// Length of Text-string (m_szText), following to this record
	//char					m_text[m_trailingDataLength];	// variable sized
};

// Server version request
struct SNzbVersionRequest
{
	SNzbRequestBase			m_messageBase;			// Must be the first in the struct
};

// Server version  response
struct SNzbVersionResponse
{
	SNzbResponseBase		m_messageBase;			// Must be the first in the struct
	int32_t					m_success;				// 0 - command failed, 1 - command executed successfully
	int32_t					m_trailingDataLength;	// Length of Text-string (m_szText), following to this record
	//char					m_text[m_trailingDataLength];	// variable sized
};

// PostQueue request
struct SNzbPostQueueRequest
{
	SNzbRequestBase			m_messageBase;			// Must be the first in the struct
};

// A PostQueue response
struct SNzbPostQueueResponse
{
	SNzbResponseBase		m_messageBase;			// Must be the first in the struct
	int32_t					m_entrySize;			// Size of the SNZBPostQueueResponseEntry-struct
	int32_t					m_nrTrailingEntries;	// Number of PostQueue-entries, following to this structure
	int32_t					m_trailingDataLength;	// Length of all PostQueue-entries, following to this structure
	// SNZBPostQueueResponseEntry m_entries[m_nrTrailingEntries]		// variable sized
};

// A PostQueue response entry
struct SNzbPostQueueResponseEntry
{
	int32_t					m_id;					// ID of Post-entry
	int32_t					m_stage;				// See PrePostProcessor::EPostJobStage
	int32_t					m_stageProgress;		// Progress of current stage, value in range 0..1000
	int32_t					m_fileProgress;			// Progress of current file, value in range 0..1000
	int32_t					m_totalTimeSec;			// Number of seconds this post-job is beeing processed (after it first changed the state from QUEUED).
	int32_t					m_stageTimeSec;			// Number of seconds the current stage is beeing processed.
	int32_t					m_nzbFilenameLen;		// Length of NZBFileName-string (m_szNZBFilename), following to this record
	int32_t					m_infoNameLen;			// Length of Filename-string (m_szFilename), following to this record
	int32_t					m_destDirLen;			// Length of DestDir-string (m_szDestDir), following to this record
	int32_t					m_progressLabelLen;		// Length of ProgressLabel-string (m_szProgressLabel), following to this record
	//char					m_nzbFilename[m_nzbFilenameLen];		// variable sized, may contain full path (local path on client) or only filename
	//char					m_infoName[m_infoNameLen];				// variable sized
	//char					m_destDir[m_destDirLen];				// variable sized
	//char					m_progressLabel[m_progressLabelLen];	// variable sized
};

// Write log request
struct SNzbWriteLogRequest
{
	SNzbRequestBase			m_messageBase;			// Must be the first in the struct
	int32_t					m_kind;					// see Message::Kind in "Log.h"
	int32_t					m_trailingDataLength;	// Length of nzb-file in bytes
	//char					m_text[m_trailingDataLength];	// variable sized
};

// Write log response
struct SNzbWriteLogResponse
{
	SNzbResponseBase		m_messageBase;			// Must be the first in the struct
	int32_t					m_success;				// 0 - command failed, 1 - command executed successfully
	int32_t					m_trailingDataLength;	// Length of Text-string (m_szText), following to this record
	//char					m_text[m_trailingDataLength];	// variable sized
};

// Scan nzb directory request
struct SNzbScanRequest
{
	SNzbRequestBase			m_messageBase;			// Must be the first in the struct
	int32_t					m_syncMode;				// 0 - asynchronous Scan (the command returns immediately), 1 - synchronous Scan (the command returns when the scan is completed)
};

// Scan nzb directory response
struct SNzbScanResponse
{
	SNzbResponseBase		m_messageBase;			// Must be the first in the struct
	int32_t					m_success;				// 0 - command failed, 1 - command executed successfully
	int32_t					m_trailingDataLength;	// Length of Text-string (m_szText), following to this record
	//char					m_text[m_trailingDataLength];	// variable sized
};

// A history request
struct SNzbHistoryRequest
{
	SNzbRequestBase			m_messageBase;			// Must be the first in the struct
	int32_t					m_hidden;				// 0 - only return visible records, 1 - also return hidden records
};

// history response
struct SNzbHistoryResponse
{
	SNzbResponseBase		m_messageBase;			// Must be the first in the struct
	int32_t					m_entrySize;			// Size of the SNZBHistoryResponseEntry-struct
	int32_t					m_nrTrailingEntries;	// Number of History-entries, following to this structure
	int32_t					m_trailingDataLength;	// Length of all History-entries, following to this structure
	// SNZBHistoryResponseEntry m_entries[m_nrTrailingEntries]			// variable sized
};

// history entry
struct SNzbHistoryResponseEntry
{
	int32_t					m_id;					// History-ID
	int32_t					m_kind;					// Kind of Item: 1 - Collection (NZB), 2 - URL, 3 - DUP (hidden record)
	int32_t					m_time;					// When the item was added to history. time since the Epoch (00:00:00 UTC, January 1, 1970), measured in seconds.
	int32_t					m_nicenameLen;			// Length of Nicename-string (m_szNicename), following to this record
	// for Collection and Dup items (m_iKind = 1 or 2)
	int32_t					m_sizeLo;				// Size of all files in bytes, Low 32-bits of 64-bit value
	int32_t					m_sizeHi;				// Size of all files in bytes, High 32-bits of 64-bit value
	// for Collection items (m_iKind = 1)
	int32_t					m_fileCount;			// Initial number of files included in NZB-file
	int32_t					m_parStatus;			// See NZBInfo::EParStatus
	int32_t					m_scriptStatus;			// See NZBInfo::EScriptStatus
	// for URL items (m_iKind = 2)
	int32_t					m_urlStatus;			// See NZBInfo::EUrlStatus
	// trailing data
	//char					m_nicename[m_nicenameLen];				// variable sized
};

#endif
