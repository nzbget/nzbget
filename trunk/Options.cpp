/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <set>
#ifdef WIN32
#include <direct.h>
#else
#include <unistd.h>
#include <getopt.h>
#endif

#include "nzbget.h"
#include "Util.h"
#include "Options.h"
#include "Log.h"
#include "ServerPool.h"
#include "NewsServer.h"
#include "MessageBase.h"
#include "Scheduler.h"

extern ServerPool* g_pServerPool;
extern Scheduler* g_pScheduler;

#ifdef HAVE_GETOPT_LONG
static struct option long_options[] =
    {
	    {"help", no_argument, 0, 'h'},
	    {"configfile", required_argument, 0, 'c'},
	    {"noconfigfile", no_argument, 0, 'n'},
	    {"printconfig", no_argument, 0, 'p'},
	    {"server", no_argument, 0, 's' },
	    {"daemon", no_argument, 0, 'D' },
	    {"version", no_argument, 0, 'v'},
	    {"serverversion", no_argument, 0, 'V'},
	    {"option", required_argument, 0, 'o'},
	    {"append", no_argument, 0, 'A'},
	    {"list", no_argument, 0, 'L'},
	    {"pause", no_argument, 0, 'P'},
	    {"unpause", no_argument, 0, 'U'},
	    {"rate", required_argument, 0, 'R'},
	    {"debug", no_argument, 0, 'B'},
	    {"log", required_argument, 0, 'G'},
	    {"top", no_argument, 0, 'T'},
	    {"edit", required_argument, 0, 'E'},
	    {"connect", no_argument, 0, 'C'},
	    {"quit", no_argument, 0, 'Q'},
	    {"reload", no_argument, 0, 'O'},
	    {"write", required_argument, 0, 'W'},
	    {"category", required_argument, 0, 'K'},
	    {"scan", no_argument, 0, 'S'},
	    {0, 0, 0, 0}
    };
#endif

static char short_options[] = "c:hno:psvAB:DCE:G:K:LPR:STUQOVW:";

// Program options
static const char* OPTION_CONFIGFILE			= "ConfigFile";
static const char* OPTION_APPBIN				= "AppBin";
static const char* OPTION_APPDIR				= "AppDir";
static const char* OPTION_VERSION				= "Version";
static const char* OPTION_MAINDIR				= "MainDir";
static const char* OPTION_DESTDIR				= "DestDir";
static const char* OPTION_INTERDIR				= "InterDir";
static const char* OPTION_TEMPDIR				= "TempDir";
static const char* OPTION_QUEUEDIR				= "QueueDir";
static const char* OPTION_NZBDIR				= "NzbDir";
static const char* OPTION_WEBDIR				= "WebDir";
static const char* OPTION_CONFIGTEMPLATE		= "ConfigTemplate";
static const char* OPTION_SCRIPTDIR				= "ScriptDir";
static const char* OPTION_CREATELOG				= "CreateLog";
static const char* OPTION_LOGFILE				= "LogFile";
static const char* OPTION_APPENDNZBDIR			= "AppendNzbDir";
static const char* OPTION_APPENDCATEGORYDIR		= "AppendCategoryDir";
static const char* OPTION_LOCKFILE				= "LockFile";
static const char* OPTION_DAEMONUSERNAME		= "DaemonUserName";
static const char* OPTION_OUTPUTMODE			= "OutputMode";
static const char* OPTION_DUPECHECK				= "DupeCheck";
static const char* OPTION_DOWNLOADRATE			= "DownloadRate";
static const char* OPTION_RENAMEBROKEN			= "RenameBroken";
static const char* OPTION_CONTROLIP				= "ControlIp";
static const char* OPTION_CONTROLPORT			= "ControlPort";
static const char* OPTION_CONTROLPASSWORD		= "ControlPassword";
static const char* OPTION_SECURECONTROL			= "SecureControl";
static const char* OPTION_SECUREPORT			= "SecurePort";
static const char* OPTION_SECURECERT			= "SecureCert";
static const char* OPTION_SECUREKEY				= "SecureKey";
static const char* OPTION_CONNECTIONTIMEOUT		= "ConnectionTimeout";
static const char* OPTION_SAVEQUEUE				= "SaveQueue";
static const char* OPTION_RELOADQUEUE			= "ReloadQueue";
static const char* OPTION_RELOADURLQUEUE		= "ReloadUrlQueue";
static const char* OPTION_RELOADPOSTQUEUE		= "ReloadPostQueue";
static const char* OPTION_CREATEBROKENLOG		= "CreateBrokenLog";
static const char* OPTION_RESETLOG				= "ResetLog";
static const char* OPTION_DECODE				= "Decode";
static const char* OPTION_RETRIES				= "Retries";
static const char* OPTION_RETRYINTERVAL			= "RetryInterval";
static const char* OPTION_TERMINATETIMEOUT		= "TerminateTimeout";
static const char* OPTION_CONTINUEPARTIAL		= "ContinuePartial";
static const char* OPTION_URLCONNECTIONS		= "UrlConnections";
static const char* OPTION_LOGBUFFERSIZE			= "LogBufferSize";
static const char* OPTION_INFOTARGET			= "InfoTarget";
static const char* OPTION_WARNINGTARGET			= "WarningTarget";
static const char* OPTION_ERRORTARGET			= "ErrorTarget";
static const char* OPTION_DEBUGTARGET			= "DebugTarget";
static const char* OPTION_DETAILTARGET			= "DetailTarget";
static const char* OPTION_PARCHECK				= "ParCheck";
static const char* OPTION_PARREPAIR				= "ParRepair";
static const char* OPTION_PARSCAN				= "ParScan";
static const char* OPTION_NZBPROCESS			= "NZBProcess";
static const char* OPTION_NZBADDEDPROCESS		= "NZBAddedProcess";
static const char* OPTION_STRICTPARNAME			= "StrictParName";
static const char* OPTION_UMASK					= "UMask";
static const char* OPTION_UPDATEINTERVAL		= "UpdateInterval";
static const char* OPTION_CURSESNZBNAME			= "CursesNzbName";
static const char* OPTION_CURSESTIME			= "CursesTime";
static const char* OPTION_CURSESGROUP			= "CursesGroup";
static const char* OPTION_CRCCHECK				= "CrcCheck";
static const char* OPTION_THREADLIMIT			= "ThreadLimit";
static const char* OPTION_DIRECTWRITE			= "DirectWrite";
static const char* OPTION_WRITEBUFFERSIZE		= "WriteBufferSize";
static const char* OPTION_NZBDIRINTERVAL		= "NzbDirInterval";
static const char* OPTION_NZBDIRFILEAGE			= "NzbDirFileAge";
static const char* OPTION_PARCLEANUPQUEUE		= "ParCleanupQueue";
static const char* OPTION_DISKSPACE				= "DiskSpace";
static const char* OPTION_PROCESSLOGKIND		= "ProcessLogKind";
static const char* OPTION_DUMPCORE				= "DumpCore";
static const char* OPTION_PARPAUSEQUEUE			= "ParPauseQueue";
static const char* OPTION_SCRIPTPAUSEQUEUE		= "ScriptPauseQueue";
static const char* OPTION_NZBCLEANUPDISK		= "NzbCleanupDisk";
static const char* OPTION_DELETECLEANUPDISK		= "DeleteCleanupDisk";
static const char* OPTION_MERGENZB				= "MergeNzb";
static const char* OPTION_PARTIMELIMIT			= "ParTimeLimit";
static const char* OPTION_KEEPHISTORY			= "KeepHistory";
static const char* OPTION_ACCURATERATE			= "AccurateRate";
static const char* OPTION_UNPACK				= "Unpack";
static const char* OPTION_UNPACKCLEANUPDISK		= "UnpackCleanupDisk";
static const char* OPTION_UNRARCMD				= "UnrarCmd";
static const char* OPTION_SEVENZIPCMD			= "SevenZipCmd";
static const char* OPTION_UNPACKPAUSEQUEUE		= "UnpackPauseQueue";
static const char* OPTION_SCRIPTORDER			= "ScriptOrder";
static const char* OPTION_DEFSCRIPT				= "DefScript";
static const char* OPTION_EXTCLEANUPDISK		= "ExtCleanupDisk";

// obsolete options
static const char* OPTION_POSTLOGKIND			= "PostLogKind";
static const char* OPTION_NZBLOGKIND			= "NZBLogKind";
static const char* OPTION_RETRYONCRCERROR		= "RetryOnCrcError";
static const char* OPTION_ALLOWREPROCESS		= "AllowReProcess";
static const char* OPTION_POSTPROCESS			= "PostProcess";
static const char* OPTION_LOADPARS				= "LoadPars";

const char* BoolNames[] = { "yes", "no", "true", "false", "1", "0", "on", "off", "enable", "disable", "enabled", "disabled" };
const int BoolValues[] = { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };
const int BoolCount = 12;

static const char* PPSCRIPT_SIGNATURE = "### NZBGET POST-PROCESSING SCRIPT";

#ifndef WIN32
const char* PossibleConfigLocations[] =
	{
		"~/.nzbget",
		"/etc/nzbget.conf",
		"/usr/etc/nzbget.conf",
		"/usr/local/etc/nzbget.conf",
		"/opt/etc/nzbget.conf",
		NULL
	};
#endif

Options::OptEntry::OptEntry()
{
	m_szName = NULL;
	m_szValue = NULL;
	m_szDefValue = NULL;
	m_iLineNo = 0;
}

Options::OptEntry::OptEntry(const char* szName, const char* szValue)
{
	m_szName = strdup(szName);
	m_szValue = strdup(szValue);
	m_szDefValue = NULL;
	m_iLineNo = 0;
}

Options::OptEntry::~OptEntry()
{
	if (m_szName)
	{
		free(m_szName);
	}
	if (m_szValue)
	{
		free(m_szValue);
	}
	if (m_szDefValue)
	{
		free(m_szDefValue);
	}
}

void Options::OptEntry::SetName(const char* szName)
{
	if (m_szName)
	{
		free(m_szName);
	}
	m_szName = strdup(szName);
}

void Options::OptEntry::SetValue(const char* szValue)
{
	if (m_szValue)
	{
		free(m_szValue);
	}
	m_szValue = strdup(szValue);

	if (!m_szDefValue)
	{
		m_szDefValue = strdup(szValue);
	}
}

Options::OptEntries::~OptEntries()
{
	for (iterator it = begin(); it != end(); it++)
	{
		delete *it;
	}
}

Options::OptEntry* Options::OptEntries::FindOption(const char* szName)
{
	if (!szName)
	{
		return NULL;
	}

	for (iterator it = begin(); it != end(); it++)
	{
		OptEntry* pOptEntry = *it;
		if (!strcasecmp(pOptEntry->GetName(), szName))
		{
			return pOptEntry;
		}
	}

	return NULL;
}


Options::ConfigTemplate::ConfigTemplate(const char* szName, const char* szDisplayName, const char* szTemplate)
{
	m_szName = strdup(szName);
	m_szDisplayName = strdup(szDisplayName);
	m_szTemplate = strdup(szTemplate ? szTemplate : "");
}

Options::ConfigTemplate::~ConfigTemplate()
{
	if (m_szName)
	{
		free(m_szName);
	}
	if (m_szDisplayName)
	{
		free(m_szDisplayName);
	}
	if (m_szTemplate)
	{
		free(m_szTemplate);
	}
}

Options::ConfigTemplates::~ConfigTemplates()
{
	for (iterator it = begin(); it != end(); it++)
	{
		delete *it;
	}
}


Options::Category::Category(const char* szName, const char* szDestDir, const char* szDefScript)
{
	m_szName = strdup(szName);
	m_szDestDir = szDestDir ? strdup(szDestDir) : NULL;
	m_szDefScript = szDefScript ? strdup(szDefScript) : NULL;
}

Options::Category::~Category()
{
	if (m_szName)
	{
		free(m_szName);
	}
	if (m_szDestDir)
	{
		free(m_szDestDir);
	}
	if (m_szDefScript)
	{
		free(m_szDefScript);
	}
}

Options::Categories::~Categories()
{
	for (iterator it = begin(); it != end(); it++)
	{
		delete *it;
	}
}

Options::Category* Options::Categories::FindCategory(const char* szName)
{
	if (!szName)
	{
		return NULL;
	}

	for (iterator it = begin(); it != end(); it++)
	{
		Category* pCategory = *it;
		if (!strcasecmp(pCategory->GetName(), szName))
		{
			return pCategory;
		}
	}

	return NULL;
}


Options::Script::Script(const char* szName, const char* szLocation)
{
	m_szName = strdup(szName);
	m_szLocation = strdup(szLocation);
	m_szDisplayName = strdup(szName);
}

Options::Script::~Script()
{
	if (m_szName)
	{
		free(m_szName);
	}
	if (m_szLocation)
	{
		free(m_szLocation);
	}
	if (m_szDisplayName)
	{
		free(m_szDisplayName);
	}
}

void Options::Script::SetDisplayName(const char* szDisplayName)
{
	if (m_szDisplayName)
	{
		free(m_szDisplayName);
	}
	m_szDisplayName = strdup(szDisplayName);
}


Options::ScriptList::~ScriptList()
{
	for (iterator it = begin(); it != end(); it++)
	{
		delete *it;
	}
}

Options::Script* Options::ScriptList::Find(const char* szName)
{
	for (iterator it = begin(); it != end(); it++)
	{
		Script* pScript = *it;
		if (!strcmp(pScript->GetName(), szName))
		{
			return pScript;
		}
	}

	return NULL;
}


Options::Options(int argc, char* argv[])
{
	m_bConfigErrors = false;
	m_iConfigLine = 0;

	// initialize options with default values
	m_bConfigInitialized	= false;
	m_szConfigFilename		= NULL;
	m_szDestDir				= NULL;
	m_szInterDir			= NULL;
	m_szTempDir				= NULL;
	m_szQueueDir			= NULL;
	m_szNzbDir				= NULL;
	m_szWebDir				= NULL;
	m_szConfigTemplate		= NULL;
	m_szScriptDir			= NULL;
	m_eInfoTarget			= mtScreen;
	m_eWarningTarget		= mtScreen;
	m_eErrorTarget			= mtScreen;
	m_eDebugTarget			= mtScreen;
	m_eDetailTarget			= mtScreen;
	m_bDecode				= true;
	m_bPauseDownload		= false;
	m_bPauseDownload2		= false;
	m_bPausePostProcess		= false;
	m_bPauseScan			= false;
	m_bCreateBrokenLog		= false;
	m_bResetLog				= false;
	m_iDownloadRate			= 0;
	m_iEditQueueAction		= 0;
	m_pEditQueueIDList		= NULL;
	m_iEditQueueIDCount		= 0;
	m_iEditQueueOffset		= 0;
	m_szEditQueueText		= NULL;
	m_szArgFilename			= NULL;
	m_szLastArg				= NULL;
	m_szAddCategory			= NULL;
	m_iAddPriority			= 0;
	m_szAddNZBFilename		= NULL;
	m_bAddPaused			= false;
	m_iConnectionTimeout	= 0;
	m_iTerminateTimeout		= 0;
	m_bServerMode			= false;
	m_bDaemonMode			= false;
	m_bRemoteClientMode		= false;
	m_bPrintOptions			= false;
	m_bAddTop				= false;
	m_bAppendNZBDir			= false;
	m_bAppendCategoryDir	= false;
	m_bContinuePartial		= false;
	m_bRenameBroken			= false;
	m_bSaveQueue			= false;
	m_bDupeCheck			= false;
	m_iRetries				= 0;
	m_iRetryInterval		= 0;
	m_iControlPort			= 0;
	m_szControlIP			= NULL;
	m_szControlPassword		= NULL;
	m_bSecureControl		= false;
	m_iSecurePort			= 0;
	m_szSecureCert			= NULL;
	m_szSecureKey			= NULL;
	m_szLockFile			= NULL;
	m_szDaemonUserName		= NULL;
	m_eOutputMode			= omLoggable;
	m_bReloadQueue			= false;
	m_bReloadUrlQueue		= false;
	m_bReloadPostQueue		= false;
	m_iUrlConnections		= 0;
	m_iLogBufferSize		= 0;
	m_iLogLines				= 0;
	m_iWriteLogKind			= 0;
	m_bCreateLog			= false;
	m_szLogFile				= NULL;
	m_eParCheck				= pcManual;
	m_bParRepair			= false;
	m_eParScan				= psLimited;
	m_szScriptOrder			= NULL;
	m_szDefScript			= NULL;
	m_szNZBProcess			= NULL;
	m_szNZBAddedProcess		= NULL;
	m_bStrictParName		= false;
	m_bNoConfig				= false;
	m_iUMask				= 0;
	m_iUpdateInterval		= 0;
	m_bCursesNZBName		= false;
	m_bCursesTime			= false;
	m_bCursesGroup			= false;
	m_bCrcCheck				= false;
	m_bDirectWrite			= false;
	m_iThreadLimit			= 0;
	m_iWriteBufferSize		= 0;
	m_iNzbDirInterval		= 0;
	m_iNzbDirFileAge		= 0;
	m_bParCleanupQueue		= false;
	m_iDiskSpace			= 0;
	m_eProcessLogKind		= slNone;
	m_bTestBacktrace		= false;
	m_bTLS					= false;
	m_bDumpCore				= false;
	m_bParPauseQueue		= false;
	m_bScriptPauseQueue		= false;
	m_bNzbCleanupDisk		= false;
	m_bDeleteCleanupDisk	= false;
	m_bMergeNzb				= false;
	m_iParTimeLimit			= 0;
	m_iKeepHistory			= 0;
	m_bAccurateRate			= false;
	m_EMatchMode			= mmID;
	m_tResumeTime			= 0;
	m_bUnpack				= false;
	m_bUnpackCleanupDisk	= false;
	m_szUnrarCmd			= NULL;
	m_szSevenZipCmd			= NULL;
	m_bUnpackPauseQueue		= false;
	m_szExtCleanupDisk		= NULL;

	// Option "ConfigFile" will be initialized later, but we want
	// to see it at the top of option list, so we add it first
	SetOption(OPTION_CONFIGFILE, "");

	char szFilename[MAX_PATH + 1];
#ifdef WIN32
	GetModuleFileName(NULL, szFilename, sizeof(szFilename));
#else
	Util::ExpandFileName(argv[0], szFilename, sizeof(szFilename));
#endif
	Util::NormalizePathSeparators(szFilename);
	SetOption(OPTION_APPBIN, szFilename);
	char* end = strrchr(szFilename, PATH_SEPARATOR);
	if (end) *end = '\0';
	SetOption(OPTION_APPDIR, szFilename);

	SetOption(OPTION_VERSION, Util::VersionRevision());

	InitDefault();
	InitCommandLine(argc, argv);

	if (argc == 1)
	{
		PrintUsage(argv[0]);
	}
	if (!m_szConfigFilename && !m_bNoConfig)
	{
		if (argc == 1)
		{
			printf("\n");
		}
		printf("No configuration-file found\n");
#ifdef WIN32
		printf("Please put configuration-file \"nzbget.conf\" into the directory with exe-file\n");
#else
		printf("Please use option \"-c\" or put configuration-file in one of the following locations:\n");
		int p = 0;
		while (const char* szFilename = PossibleConfigLocations[p++])
		{
			printf("%s\n", szFilename);
		}
#endif
		exit(-1);
	}
	if (argc == 1)
	{
		exit(-1);
	}

	InitOptions();
	CheckOptions();

	if (!m_bPrintOptions)
	{
		InitFileArg(argc, argv);
	}

	InitServers();
	InitCategories();
	InitScheduler();

	if (m_bPrintOptions)
	{
		Dump();
		exit(-1);
	}

	if (m_bConfigErrors && m_eClientOperation == opClientNoOperation)
	{
		info("Pausing all activities due to errors in configuration");
		m_bPauseDownload = true;
		m_bPauseDownload2= true;
		m_bPausePostProcess = true;
		m_bPauseScan = true;
	}
}

Options::~Options()
{
	if (m_szConfigFilename)
	{
		free(m_szConfigFilename);
	}
	if (m_szDestDir)
	{
		free(m_szDestDir);
	}
	if (m_szInterDir)
	{
		free(m_szInterDir);
	}
	if (m_szTempDir)
	{
		free(m_szTempDir);
	}
	if (m_szQueueDir)
	{
		free(m_szQueueDir);
	}
	if (m_szNzbDir)
	{
		free(m_szNzbDir);
	}
	if (m_szWebDir)
	{
		free(m_szWebDir);
	}
	if (m_szConfigTemplate)
	{
		free(m_szConfigTemplate);
	}
	if (m_szScriptDir)
	{
		free(m_szScriptDir);
	}
	if (m_szArgFilename)
	{
		free(m_szArgFilename);
	}
	if (m_szAddCategory)
	{
		free(m_szAddCategory);
	}
	if (m_szEditQueueText)
	{
		free(m_szEditQueueText);
	}
	if (m_szLastArg)
	{
		free(m_szLastArg);
	}
	if (m_szControlIP)
	{
		free(m_szControlIP);
	}
	if (m_szControlPassword)
	{
		free(m_szControlPassword);
	}
	if (m_szSecureCert)
	{
		free(m_szSecureCert);
	}
	if (m_szSecureKey)
	{
		free(m_szSecureKey);
	}
	if (m_szLogFile)
	{
		free(m_szLogFile);
	}
	if (m_szLockFile)
	{
		free(m_szLockFile);
	}
	if (m_szDaemonUserName)
	{
		free(m_szDaemonUserName);
	}
	if (m_szScriptOrder)
	{
		free(m_szScriptOrder);
	}
	if (m_szDefScript)
	{
		free(m_szDefScript);
	}
	if (m_szNZBProcess)
	{
		free(m_szNZBProcess);
	}
	if (m_szNZBAddedProcess)
	{
		free(m_szNZBAddedProcess);
	}
	if (m_pEditQueueIDList)
	{
		free(m_pEditQueueIDList);
	}
	if (m_szAddNZBFilename)
	{
		free(m_szAddNZBFilename);
	}
	if (m_szUnrarCmd)
	{
		free(m_szUnrarCmd);
	}
	if (m_szSevenZipCmd)
	{
		free(m_szSevenZipCmd);
	}
	if (m_szExtCleanupDisk)
	{
		free(m_szExtCleanupDisk);
	}

	for (NameList::iterator it = m_EditQueueNameList.begin(); it != m_EditQueueNameList.end(); it++)
	{
		free(*it);
	}
	m_EditQueueNameList.clear();
}

void Options::Dump()
{
	for (OptEntries::iterator it = m_OptEntries.begin(); it != m_OptEntries.end(); it++)
	{
		OptEntry* pOptEntry = *it;
		printf("%s = \"%s\"\n", pOptEntry->GetName(), pOptEntry->GetValue());
	}
}

void Options::ConfigError(const char* msg, ...)
{
	char tmp2[1024];

	va_list ap;
	va_start(ap, msg);
	vsnprintf(tmp2, 1024, msg, ap);
	tmp2[1024-1] = '\0';
	va_end(ap);

	printf("%s(%i): %s\n", Util::BaseFileName(m_szConfigFilename), m_iConfigLine, tmp2);
	error("%s(%i): %s", Util::BaseFileName(m_szConfigFilename), m_iConfigLine, tmp2);

	m_bConfigErrors = true;
}

void Options::ConfigWarn(const char* msg, ...)
{
	char tmp2[1024];
	
	va_list ap;
	va_start(ap, msg);
	vsnprintf(tmp2, 1024, msg, ap);
	tmp2[1024-1] = '\0';
	va_end(ap);
	
	printf("%s(%i): %s\n", Util::BaseFileName(m_szConfigFilename), m_iConfigLine, tmp2);
	warn("%s(%i): %s", Util::BaseFileName(m_szConfigFilename), m_iConfigLine, tmp2);
}

void Options::LocateOptionSrcPos(const char *szOptionName)
{
	OptEntry* pOptEntry = FindOption(szOptionName);
	if (pOptEntry)
	{
		m_iConfigLine = pOptEntry->GetLineNo();
	}
	else
	{
		m_iConfigLine = 0;
	}
}

void Options::InitDefault()
{
#ifdef WIN32
	SetOption(OPTION_MAINDIR, "${AppDir}");
#else
	SetOption(OPTION_MAINDIR, "~/downloads");
#endif
	SetOption(OPTION_TEMPDIR, "${MainDir}/tmp");
	SetOption(OPTION_DESTDIR, "${MainDir}/dst");
	SetOption(OPTION_INTERDIR, "");
	SetOption(OPTION_QUEUEDIR, "${MainDir}/queue");
	SetOption(OPTION_NZBDIR, "${MainDir}/nzb");
	SetOption(OPTION_LOCKFILE, "${MainDir}/nzbget.lock");
	SetOption(OPTION_LOGFILE, "${DestDir}/nzbget.log");
	SetOption(OPTION_WEBDIR, "");
	SetOption(OPTION_CONFIGTEMPLATE, "");
	SetOption(OPTION_SCRIPTDIR, "${MainDir}/ppscripts");
	SetOption(OPTION_CREATELOG, "yes");
	SetOption(OPTION_APPENDNZBDIR, "yes");
	SetOption(OPTION_APPENDCATEGORYDIR, "yes");
	SetOption(OPTION_OUTPUTMODE, "curses");
	SetOption(OPTION_DUPECHECK, "yes");
	SetOption(OPTION_DOWNLOADRATE, "0");
	SetOption(OPTION_RENAMEBROKEN, "no");
	SetOption(OPTION_CONTROLIP, "0.0.0.0");
	SetOption(OPTION_CONTROLPASSWORD, "tegbzn6789");
	SetOption(OPTION_CONTROLPORT, "6789");
	SetOption(OPTION_SECURECONTROL, "no");
	SetOption(OPTION_SECUREPORT, "6791");
	SetOption(OPTION_SECURECERT, "");
	SetOption(OPTION_SECUREKEY, "");
	SetOption(OPTION_CONNECTIONTIMEOUT, "60");
	SetOption(OPTION_SAVEQUEUE, "yes");
	SetOption(OPTION_RELOADQUEUE, "yes");
	SetOption(OPTION_RELOADURLQUEUE, "yes");
	SetOption(OPTION_RELOADPOSTQUEUE, "yes");
	SetOption(OPTION_CREATEBROKENLOG, "yes");
	SetOption(OPTION_RESETLOG, "no");
	SetOption(OPTION_DECODE, "yes");
	SetOption(OPTION_RETRIES, "3");
	SetOption(OPTION_RETRYINTERVAL, "10");
	SetOption(OPTION_TERMINATETIMEOUT, "600");
	SetOption(OPTION_CONTINUEPARTIAL, "no");
	SetOption(OPTION_URLCONNECTIONS, "4");
	SetOption(OPTION_LOGBUFFERSIZE, "1000");
	SetOption(OPTION_INFOTARGET, "both");
	SetOption(OPTION_WARNINGTARGET, "both");
	SetOption(OPTION_ERRORTARGET, "both");
	SetOption(OPTION_DEBUGTARGET, "none");
	SetOption(OPTION_DETAILTARGET, "both");
	SetOption(OPTION_PARCHECK, "auto");
	SetOption(OPTION_PARREPAIR, "yes");
	SetOption(OPTION_PARSCAN, "limited");
	SetOption(OPTION_SCRIPTORDER, "");
	SetOption(OPTION_DEFSCRIPT, "");
	SetOption(OPTION_NZBPROCESS, "");
	SetOption(OPTION_NZBADDEDPROCESS, "");
	SetOption(OPTION_STRICTPARNAME, "yes");
	SetOption(OPTION_DAEMONUSERNAME, "root");
	SetOption(OPTION_UMASK, "1000");
	SetOption(OPTION_UPDATEINTERVAL, "200");
	SetOption(OPTION_CURSESNZBNAME, "yes");
	SetOption(OPTION_CURSESTIME, "no");
	SetOption(OPTION_CURSESGROUP, "no");
	SetOption(OPTION_CRCCHECK, "yes");
	SetOption(OPTION_THREADLIMIT, "100");
	SetOption(OPTION_DIRECTWRITE, "yes");
	SetOption(OPTION_WRITEBUFFERSIZE, "0");
	SetOption(OPTION_NZBDIRINTERVAL, "5");
	SetOption(OPTION_NZBDIRFILEAGE, "60");
	SetOption(OPTION_PARCLEANUPQUEUE, "yes");
	SetOption(OPTION_DISKSPACE, "250");
	SetOption(OPTION_PROCESSLOGKIND, "detail");
	SetOption(OPTION_DUMPCORE, "no");
	SetOption(OPTION_PARPAUSEQUEUE, "no");
	SetOption(OPTION_SCRIPTPAUSEQUEUE, "no");
	SetOption(OPTION_NZBCLEANUPDISK, "no");
	SetOption(OPTION_DELETECLEANUPDISK, "no");
	SetOption(OPTION_MERGENZB, "no");
	SetOption(OPTION_PARTIMELIMIT, "0");
	SetOption(OPTION_KEEPHISTORY, "7");
	SetOption(OPTION_ACCURATERATE, "no");
	SetOption(OPTION_UNPACK, "no");
	SetOption(OPTION_UNPACKCLEANUPDISK, "no");
#ifdef WIN32
	SetOption(OPTION_UNRARCMD, "unrar.exe");
	SetOption(OPTION_SEVENZIPCMD, "7z.exe");
#else
	SetOption(OPTION_UNRARCMD, "unrar");
	SetOption(OPTION_SEVENZIPCMD, "7z");
#endif
	SetOption(OPTION_UNPACKPAUSEQUEUE, "no");
	SetOption(OPTION_EXTCLEANUPDISK, "");
}

void Options::InitOptFile()
{
	if (m_bConfigInitialized)
	{
		return;
	}

	if (!m_szConfigFilename && !m_bNoConfig)
	{
		// search for config file in default locations
#ifdef WIN32
		char szFilename[MAX_PATH + 1];
		GetModuleFileName(NULL, szFilename, MAX_PATH + 1);
		szFilename[MAX_PATH] = '\0';
		Util::NormalizePathSeparators(szFilename);
		char* end = strrchr(szFilename, PATH_SEPARATOR);
		if (end) end[1] = '\0';
		strcat(szFilename, "nzbget.conf");

		if (Util::FileExists(szFilename))
		{
			m_szConfigFilename = strdup(szFilename);
		}
#else
		int p = 0;
		while (const char* szFilename = PossibleConfigLocations[p++])
		{
			// substitute HOME-variable
			char szExpandedFilename[1024];
			if (Util::ExpandHomePath(szFilename, szExpandedFilename, sizeof(szExpandedFilename)))
			{
				szFilename = szExpandedFilename;
			}

			if (Util::FileExists(szFilename))
			{
				m_szConfigFilename = strdup(szFilename);
				break;
			}
		}
#endif
	}

	if (m_szConfigFilename)
	{
		// normalize path in filename
		char szFilename[MAX_PATH + 1];
		Util::ExpandFileName(m_szConfigFilename, szFilename, sizeof(szFilename));
		szFilename[MAX_PATH] = '\0';

#ifndef WIN32
		// substitute HOME-variable
		char szExpandedFilename[1024];
		if (Util::ExpandHomePath(szFilename, szExpandedFilename, sizeof(szExpandedFilename)))
		{
			strncpy(szFilename, szExpandedFilename, sizeof(szFilename));
		}
#endif
		
		free(m_szConfigFilename);
		m_szConfigFilename = strdup(szFilename);

		SetOption(OPTION_CONFIGFILE, m_szConfigFilename);
		LoadConfigFile();
	}

	m_bConfigInitialized = true;
}

void Options::CheckDir(char** dir, const char* szOptionName, bool bAllowEmpty, bool bCreate)
{
	char* usedir = NULL;
	const char* tempdir = GetOption(szOptionName);
	if (tempdir && strlen(tempdir) > 0)
	{
		int len = strlen(tempdir);
		usedir = (char*) malloc(len + 2);
		strcpy(usedir, tempdir);
		char ch = usedir[len-1];
		if (ch == ALT_PATH_SEPARATOR)
		{
			usedir[len-1] = PATH_SEPARATOR;
		}
		else if (ch != PATH_SEPARATOR)
		{
			usedir[len] = PATH_SEPARATOR;
			usedir[len + 1] = '\0';
		}
		Util::NormalizePathSeparators(usedir);
	}
	else
	{
		if (!bAllowEmpty)
		{
			ConfigError("Invalid value for option \"%s\": <empty>", szOptionName);
		}
		*dir = strdup("");
		return;
	}

	// Ensure the dir is created
	if (bCreate && !Util::ForceDirectories(usedir))
	{
		ConfigError("Invalid value for option \"%s\": could not create directory \"%s\"", szOptionName, usedir);
	}
	*dir = usedir;
}

void Options::InitOptions()
{
	CheckDir(&m_szDestDir, OPTION_DESTDIR, false, true);
	CheckDir(&m_szInterDir, OPTION_INTERDIR, true, true);
	CheckDir(&m_szTempDir, OPTION_TEMPDIR, false, true);
	CheckDir(&m_szQueueDir, OPTION_QUEUEDIR, false, true);
	CheckDir(&m_szWebDir, OPTION_WEBDIR, true, false);
	CheckDir(&m_szScriptDir, OPTION_SCRIPTDIR, true, false);

	m_szConfigTemplate		= strdup(GetOption(OPTION_CONFIGTEMPLATE));
	m_szScriptOrder			= strdup(GetOption(OPTION_SCRIPTORDER));
	m_szDefScript			= strdup(GetOption(OPTION_DEFSCRIPT));
	m_szNZBProcess			= strdup(GetOption(OPTION_NZBPROCESS));
	m_szNZBAddedProcess		= strdup(GetOption(OPTION_NZBADDEDPROCESS));
	m_szControlIP			= strdup(GetOption(OPTION_CONTROLIP));
	m_szControlPassword		= strdup(GetOption(OPTION_CONTROLPASSWORD));
	m_szSecureCert			= strdup(GetOption(OPTION_SECURECERT));
	m_szSecureKey			= strdup(GetOption(OPTION_SECUREKEY));
	m_szLockFile			= strdup(GetOption(OPTION_LOCKFILE));
	m_szDaemonUserName		= strdup(GetOption(OPTION_DAEMONUSERNAME));
	m_szLogFile				= strdup(GetOption(OPTION_LOGFILE));
	m_szUnrarCmd			= strdup(GetOption(OPTION_UNRARCMD));
	m_szSevenZipCmd			= strdup(GetOption(OPTION_SEVENZIPCMD));
	m_szExtCleanupDisk		= strdup(GetOption(OPTION_EXTCLEANUPDISK));

	m_iDownloadRate			= (int)(ParseFloatValue(OPTION_DOWNLOADRATE) * 1024);
	m_iConnectionTimeout	= ParseIntValue(OPTION_CONNECTIONTIMEOUT, 10);
	m_iTerminateTimeout		= ParseIntValue(OPTION_TERMINATETIMEOUT, 10);
	m_iRetries				= ParseIntValue(OPTION_RETRIES, 10);
	m_iRetryInterval		= ParseIntValue(OPTION_RETRYINTERVAL, 10);
	m_iControlPort			= ParseIntValue(OPTION_CONTROLPORT, 10);
	m_iSecurePort			= ParseIntValue(OPTION_SECUREPORT, 10);
	m_iUrlConnections		= ParseIntValue(OPTION_URLCONNECTIONS, 10);
	m_iLogBufferSize		= ParseIntValue(OPTION_LOGBUFFERSIZE, 10);
	m_iUMask				= ParseIntValue(OPTION_UMASK, 8);
	m_iUpdateInterval		= ParseIntValue(OPTION_UPDATEINTERVAL, 10);
	m_iThreadLimit			= ParseIntValue(OPTION_THREADLIMIT, 10);
	m_iWriteBufferSize		= ParseIntValue(OPTION_WRITEBUFFERSIZE, 10);
	m_iNzbDirInterval		= ParseIntValue(OPTION_NZBDIRINTERVAL, 10);
	m_iNzbDirFileAge		= ParseIntValue(OPTION_NZBDIRFILEAGE, 10);
	m_iDiskSpace			= ParseIntValue(OPTION_DISKSPACE, 10);
	m_iParTimeLimit			= ParseIntValue(OPTION_PARTIMELIMIT, 10);
	m_iKeepHistory			= ParseIntValue(OPTION_KEEPHISTORY, 10);

	CheckDir(&m_szNzbDir, OPTION_NZBDIR, m_iNzbDirInterval == 0, true);

	m_bCreateBrokenLog		= (bool)ParseEnumValue(OPTION_CREATEBROKENLOG, BoolCount, BoolNames, BoolValues);
	m_bResetLog				= (bool)ParseEnumValue(OPTION_RESETLOG, BoolCount, BoolNames, BoolValues);
	m_bAppendNZBDir			= (bool)ParseEnumValue(OPTION_APPENDNZBDIR, BoolCount, BoolNames, BoolValues);
	m_bAppendCategoryDir	= (bool)ParseEnumValue(OPTION_APPENDCATEGORYDIR, BoolCount, BoolNames, BoolValues);
	m_bContinuePartial		= (bool)ParseEnumValue(OPTION_CONTINUEPARTIAL, BoolCount, BoolNames, BoolValues);
	m_bRenameBroken			= (bool)ParseEnumValue(OPTION_RENAMEBROKEN, BoolCount, BoolNames, BoolValues);
	m_bSaveQueue			= (bool)ParseEnumValue(OPTION_SAVEQUEUE, BoolCount, BoolNames, BoolValues);
	m_bDupeCheck			= (bool)ParseEnumValue(OPTION_DUPECHECK, BoolCount, BoolNames, BoolValues);
	m_bCreateLog			= (bool)ParseEnumValue(OPTION_CREATELOG, BoolCount, BoolNames, BoolValues);
	m_bParRepair			= (bool)ParseEnumValue(OPTION_PARREPAIR, BoolCount, BoolNames, BoolValues);
	m_bStrictParName		= (bool)ParseEnumValue(OPTION_STRICTPARNAME, BoolCount, BoolNames, BoolValues);
	m_bReloadQueue			= (bool)ParseEnumValue(OPTION_RELOADQUEUE, BoolCount, BoolNames, BoolValues);
	m_bReloadUrlQueue		= (bool)ParseEnumValue(OPTION_RELOADURLQUEUE, BoolCount, BoolNames, BoolValues);
	m_bReloadPostQueue		= (bool)ParseEnumValue(OPTION_RELOADPOSTQUEUE, BoolCount, BoolNames, BoolValues);
	m_bCursesNZBName		= (bool)ParseEnumValue(OPTION_CURSESNZBNAME, BoolCount, BoolNames, BoolValues);
	m_bCursesTime			= (bool)ParseEnumValue(OPTION_CURSESTIME, BoolCount, BoolNames, BoolValues);
	m_bCursesGroup			= (bool)ParseEnumValue(OPTION_CURSESGROUP, BoolCount, BoolNames, BoolValues);
	m_bCrcCheck				= (bool)ParseEnumValue(OPTION_CRCCHECK, BoolCount, BoolNames, BoolValues);
	m_bDirectWrite			= (bool)ParseEnumValue(OPTION_DIRECTWRITE, BoolCount, BoolNames, BoolValues);
	m_bParCleanupQueue		= (bool)ParseEnumValue(OPTION_PARCLEANUPQUEUE, BoolCount, BoolNames, BoolValues);
	m_bDecode				= (bool)ParseEnumValue(OPTION_DECODE, BoolCount, BoolNames, BoolValues);
	m_bDumpCore				= (bool)ParseEnumValue(OPTION_DUMPCORE, BoolCount, BoolNames, BoolValues);
	m_bParPauseQueue		= (bool)ParseEnumValue(OPTION_PARPAUSEQUEUE, BoolCount, BoolNames, BoolValues);
	m_bScriptPauseQueue		= (bool)ParseEnumValue(OPTION_SCRIPTPAUSEQUEUE, BoolCount, BoolNames, BoolValues);
	m_bNzbCleanupDisk		= (bool)ParseEnumValue(OPTION_NZBCLEANUPDISK, BoolCount, BoolNames, BoolValues);
	m_bDeleteCleanupDisk	= (bool)ParseEnumValue(OPTION_DELETECLEANUPDISK, BoolCount, BoolNames, BoolValues);
	m_bMergeNzb				= (bool)ParseEnumValue(OPTION_MERGENZB, BoolCount, BoolNames, BoolValues);
	m_bAccurateRate			= (bool)ParseEnumValue(OPTION_ACCURATERATE, BoolCount, BoolNames, BoolValues);
	m_bSecureControl		= (bool)ParseEnumValue(OPTION_SECURECONTROL, BoolCount, BoolNames, BoolValues);
	m_bUnpack				= (bool)ParseEnumValue(OPTION_UNPACK, BoolCount, BoolNames, BoolValues);
	m_bUnpackCleanupDisk	= (bool)ParseEnumValue(OPTION_UNPACKCLEANUPDISK, BoolCount, BoolNames, BoolValues);
	m_bUnpackPauseQueue		= (bool)ParseEnumValue(OPTION_UNPACKPAUSEQUEUE, BoolCount, BoolNames, BoolValues);

	const char* OutputModeNames[] = { "loggable", "logable", "log", "colored", "color", "ncurses", "curses" };
	const int OutputModeValues[] = { omLoggable, omLoggable, omLoggable, omColored, omColored, omNCurses, omNCurses };
	const int OutputModeCount = 7;
	m_eOutputMode = (EOutputMode)ParseEnumValue(OPTION_OUTPUTMODE, OutputModeCount, OutputModeNames, OutputModeValues);

	const char* ParCheckNames[] = { "auto", "force", "manual", "yes", "no" }; // yes/no for compatibility with older versions
	const int ParCheckValues[] = { pcAuto, pcForce, pcManual, pcForce, pcAuto };
	const int ParCheckCount = 5;
	m_eParCheck = (EParCheck)ParseEnumValue(OPTION_PARCHECK, ParCheckCount, ParCheckNames, ParCheckValues);

	const char* ParScanNames[] = { "limited", "full", "auto" };
	const int ParScanValues[] = { psLimited, psFull, psAuto };
	const int ParScanCount = 3;
	m_eParScan = (EParScan)ParseEnumValue(OPTION_PARSCAN, ParScanCount, ParScanNames, ParScanValues);

	const char* TargetNames[] = { "screen", "log", "both", "none" };
	const int TargetValues[] = { mtScreen, mtLog, mtBoth, mtNone };
	const int TargetCount = 4;
	m_eInfoTarget = (EMessageTarget)ParseEnumValue(OPTION_INFOTARGET, TargetCount, TargetNames, TargetValues);
	m_eWarningTarget = (EMessageTarget)ParseEnumValue(OPTION_WARNINGTARGET, TargetCount, TargetNames, TargetValues);
	m_eErrorTarget = (EMessageTarget)ParseEnumValue(OPTION_ERRORTARGET, TargetCount, TargetNames, TargetValues);
	m_eDebugTarget = (EMessageTarget)ParseEnumValue(OPTION_DEBUGTARGET, TargetCount, TargetNames, TargetValues);
	m_eDetailTarget = (EMessageTarget)ParseEnumValue(OPTION_DETAILTARGET, TargetCount, TargetNames, TargetValues);

	const char* ScriptLogKindNames[] = { "none", "detail", "info", "warning", "error", "debug" };
	const int ScriptLogKindValues[] = { slNone, slDetail, slInfo, slWarning, slError, slDebug };
	const int ScriptLogKindCount = 6;
	m_eProcessLogKind = (EScriptLogKind)ParseEnumValue(OPTION_PROCESSLOGKIND, ScriptLogKindCount, ScriptLogKindNames, ScriptLogKindValues);
}

int Options::ParseEnumValue(const char* OptName, int argc, const char * argn[], const int argv[])
{
	OptEntry* pOptEntry = FindOption(OptName);
	if (!pOptEntry)
	{
		ConfigError("Undefined value for option \"%s\"", OptName);
		return argv[0];
		//abort("FATAL ERROR: Undefined value for option \"%s\"\n", OptName);
	}

	int iDefNum = 0;

	for (int i = 0; i < argc; i++)
	{
		if (!strcasecmp(pOptEntry->GetValue(), argn[i]))
		{
			// normalizing option value in option list, for example "NO" -> "no"
			for (int j = 0; j < argc; j++)
			{
				if (argv[j] == argv[i])
				{
					if (strcmp(argn[j], pOptEntry->GetValue()))
					{
						pOptEntry->SetValue(argn[j]);
					}
					break;
				}
			}

			return argv[i];
		}

		if (!strcasecmp(pOptEntry->GetDefValue(), argn[i]))
		{
			iDefNum = i;
		}
	}

	m_iConfigLine = pOptEntry->GetLineNo();
	ConfigError("Invalid value for option \"%s\": \"%s\"", OptName, pOptEntry->GetValue());
	pOptEntry->SetValue(argn[iDefNum]);
	return argv[iDefNum];
}

int Options::ParseIntValue(const char* OptName, int iBase)
{
	OptEntry* pOptEntry = FindOption(OptName);
	if (!pOptEntry)
	{
		abort("FATAL ERROR: Undefined value for option \"%s\"\n", OptName);
	}

	char *endptr;
	int val = strtol(pOptEntry->GetValue(), &endptr, iBase);

	if (endptr && *endptr != '\0')
	{
		m_iConfigLine = pOptEntry->GetLineNo();
		ConfigError("Invalid value for option \"%s\": \"%s\"", OptName, pOptEntry->GetValue());
		pOptEntry->SetValue(pOptEntry->GetDefValue());
		val = strtol(pOptEntry->GetDefValue(), NULL, iBase);
	}

	return val;
}

float Options::ParseFloatValue(const char* OptName)
{
	OptEntry* pOptEntry = FindOption(OptName);
	if (!pOptEntry)
	{
		abort("FATAL ERROR: Undefined value for option \"%s\"\n", OptName);
	}

	char *endptr;
	float val = (float)strtod(pOptEntry->GetValue(), &endptr);

	if (endptr && *endptr != '\0')
	{
		m_iConfigLine = pOptEntry->GetLineNo();
		ConfigError("Invalid value for option \"%s\": \"%s\"", OptName, pOptEntry->GetValue());
		pOptEntry->SetValue(pOptEntry->GetDefValue());
		val = (float)strtod(pOptEntry->GetDefValue(), NULL);
	}

	return val;
}

void Options::InitCommandLine(int argc, char* argv[])
{
	m_eClientOperation = opClientNoOperation; // default

	// reset getopt
	optind = 1;

	while (true)
	{
		int c;

#ifdef HAVE_GETOPT_LONG
		int option_index  = 0;
		c = getopt_long(argc, argv, short_options, long_options, &option_index);
#else
		c = getopt(argc, argv, short_options);
#endif

		if (c == -1) break;

		switch (c)
		{
			case 'c':
				m_szConfigFilename = strdup(optarg);
				break;
			case 'n':
				m_szConfigFilename = NULL;
				m_bNoConfig = true;
				break;
			case 'h':
				PrintUsage(argv[0]);
				exit(0);
				break;
			case 'v':
				printf("nzbget version: %s\n", Util::VersionRevision());
				exit(1);
				break;
			case 'p':
				m_bPrintOptions = true;
				break;
			case 'o':
				InitOptFile();
				if (!SetOptionString(optarg))
				{
					abort("FATAL ERROR: error in option \"%s\"\n", optarg);
				}
				break;
			case 's':
				m_bServerMode = true;
				break;
			case 'D':
				m_bServerMode = true;
				m_bDaemonMode = true;
				break;
			case 'A':
				m_eClientOperation = opClientRequestDownload; // default

				while (true)
				{
					optind++;
					optarg = optind > argc ? NULL : argv[optind-1];
					if (optarg && !strcasecmp(optarg, "F"))
					{
						m_eClientOperation = opClientRequestDownload;
					}
					else if (optarg && !strcasecmp(optarg, "U"))
					{
						m_eClientOperation = opClientRequestDownloadUrl;
					}
					else if (optarg && !strcasecmp(optarg, "T"))
					{
						m_bAddTop = true;
					}
					else if (optarg && !strcasecmp(optarg, "P"))
					{
						m_bAddPaused = true;
					}
					else if (optarg && !strcasecmp(optarg, "I"))
					{
						optind++;
						if (optind > argc)
						{
							abort("FATAL ERROR: Could not parse value of option 'A'\n");
						}
						m_iAddPriority = atoi(argv[optind-1]);
					}
					else if (optarg && !strcasecmp(optarg, "C"))
					{
						optind++;
						if (optind > argc)
						{
							abort("FATAL ERROR: Could not parse value of option 'A'\n");
						}
						if (m_szAddCategory)
						{
							free(m_szAddCategory);
						}
						m_szAddCategory = strdup(argv[optind-1]);
					}
					else if (optarg && !strcasecmp(optarg, "N"))
					{
						optind++;
						if (optind > argc)
						{
							abort("FATAL ERROR: Could not parse value of option 'A'\n");
						}
						if (m_szAddNZBFilename)
						{
							free(m_szAddNZBFilename);
						}
						m_szAddNZBFilename = strdup(argv[optind-1]);
					}
					else
					{
						optind--;
						break;
					}
				}
				break;
			case 'L':
				optind++;
				optarg = optind > argc ? NULL : argv[optind-1];
				if (!optarg || !strncmp(optarg, "-", 1))
				{
					m_eClientOperation = opClientRequestListFiles;
					optind--;
				}
				else if (!strcasecmp(optarg, "F") || !strcasecmp(optarg, "FR"))
				{
					m_eClientOperation = opClientRequestListFiles;
				}
				else if (!strcasecmp(optarg, "G") || !strcasecmp(optarg, "GR"))
				{
					m_eClientOperation = opClientRequestListGroups;
				}
				else if (!strcasecmp(optarg, "O"))
				{
					m_eClientOperation = opClientRequestPostQueue;
				}
				else if (!strcasecmp(optarg, "S"))
				{
					m_eClientOperation = opClientRequestListStatus;
				}
				else if (!strcasecmp(optarg, "H"))
				{
					m_eClientOperation = opClientRequestHistory;
				}
				else if (!strcasecmp(optarg, "U"))
				{
					m_eClientOperation = opClientRequestUrlQueue;
				}
				else
				{
					abort("FATAL ERROR: Could not parse value of option 'L'\n");
				}

				if (optarg && (!strcasecmp(optarg, "FR") || !strcasecmp(optarg, "GR")))
				{
					m_EMatchMode = mmRegEx;

					optind++;
					if (optind > argc)
					{
						abort("FATAL ERROR: Could not parse value of option 'L'\n");
					}
					m_szEditQueueText = strdup(argv[optind-1]);
				}
				break;
			case 'P':
			case 'U':
				optind++;
				optarg = optind > argc ? NULL : argv[optind-1];
				if (!optarg || !strncmp(optarg, "-", 1))
				{
					m_eClientOperation = c == 'P' ? opClientRequestDownloadPause : opClientRequestDownloadUnpause;
					optind--;
				}
				else if (!strcasecmp(optarg, "D"))
				{
					m_eClientOperation = c == 'P' ? opClientRequestDownloadPause : opClientRequestDownloadUnpause;
				}
				else if (!strcasecmp(optarg, "D2"))
				{
					m_eClientOperation = c == 'P' ? opClientRequestDownload2Pause : opClientRequestDownload2Unpause;
				}
				else if (!strcasecmp(optarg, "O"))
				{
					m_eClientOperation = c == 'P' ? opClientRequestPostPause : opClientRequestPostUnpause;
				}
				else if (!strcasecmp(optarg, "S"))
				{
					m_eClientOperation = c == 'P' ? opClientRequestScanPause : opClientRequestScanUnpause;
				}
				else
				{
					abort("FATAL ERROR: Could not parse value of option '%c'\n", c);
				}
				break;
			case 'R':
				m_eClientOperation = opClientRequestSetRate;
				m_iSetRate = (int)(atof(optarg)*1024);
				break;
			case 'B':
				if (!strcasecmp(optarg, "dump"))
				{
					m_eClientOperation = opClientRequestDumpDebug;
				}
				else if (!strcasecmp(optarg, "trace"))
				{
					m_bTestBacktrace = true;
				}
				else
				{
					abort("FATAL ERROR: Could not parse value of option 'B'\n");
				}
				break;
			case 'G':
				m_eClientOperation = opClientRequestLog;
				m_iLogLines = atoi(optarg);
				if (m_iLogLines == 0)
				{
					abort("FATAL ERROR: Could not parse value of option 'G'\n");
				}
				break;
			case 'T':
				m_bAddTop = true;
				break;
			case 'C':
				m_bRemoteClientMode = true;
				break;
			case 'E':
			{
				m_eClientOperation = opClientRequestEditQueue;
				bool bGroup = !strcasecmp(optarg, "G") || !strcasecmp(optarg, "GN") || !strcasecmp(optarg, "GR");
				bool bFile = !strcasecmp(optarg, "F") || !strcasecmp(optarg, "FN") || !strcasecmp(optarg, "FR");
				if (!strcasecmp(optarg, "GN") || !strcasecmp(optarg, "FN"))
				{
					m_EMatchMode = mmName;
				}
				else if (!strcasecmp(optarg, "GR") || !strcasecmp(optarg, "FR"))
				{
					m_EMatchMode = mmRegEx;
				}
				else
				{
					m_EMatchMode = mmID;
				};
				bool bPost = !strcasecmp(optarg, "O");
				bool bHistory = !strcasecmp(optarg, "H");
				if (bGroup || bFile || bPost || bHistory)
				{
					optind++;
					if (optind > argc)
					{
						abort("FATAL ERROR: Could not parse value of option 'E'\n");
					}
					optarg = argv[optind-1];
				}

				if (bPost)
				{
					// edit-commands for post-processor-queue
					if (!strcasecmp(optarg, "T"))
					{
						m_iEditQueueAction = eRemoteEditActionPostMoveTop;
					}
					else if (!strcasecmp(optarg, "B"))
					{
						m_iEditQueueAction = eRemoteEditActionPostMoveBottom;
					}
					else if (!strcasecmp(optarg, "D"))
					{
						m_iEditQueueAction = eRemoteEditActionPostDelete;
					}
					else
					{
						m_iEditQueueOffset = atoi(optarg);
						if (m_iEditQueueOffset == 0)
						{
							abort("FATAL ERROR: Could not parse value of option 'E'\n");
						}
						m_iEditQueueAction = eRemoteEditActionPostMoveOffset;
					}
				}
				else if (bHistory)
				{
					// edit-commands for history
					if (!strcasecmp(optarg, "D"))
					{
						m_iEditQueueAction = eRemoteEditActionHistoryDelete;
					}
					else if (!strcasecmp(optarg, "R"))
					{
						m_iEditQueueAction = eRemoteEditActionHistoryReturn;
					}
					else if (!strcasecmp(optarg, "P"))
					{
						m_iEditQueueAction = eRemoteEditActionHistoryProcess;
					}
					else if (!strcasecmp(optarg, "O"))
					{
						m_iEditQueueAction = eRemoteEditActionHistorySetParameter;
						
						optind++;
						if (optind > argc)
						{
							abort("FATAL ERROR: Could not parse value of option 'E'\n");
						}
						m_szEditQueueText = strdup(argv[optind-1]);
						
						if (!strchr(m_szEditQueueText, '='))
						{
							abort("FATAL ERROR: Could not parse value of option 'E'\n");
						}
					}
					else
					{
						abort("FATAL ERROR: Could not parse value of option 'E'\n");
					}
				}
				else
				{
					// edit-commands for download-queue
					if (!strcasecmp(optarg, "T"))
					{
						m_iEditQueueAction = bGroup ? eRemoteEditActionGroupMoveTop : eRemoteEditActionFileMoveTop;
					}
					else if (!strcasecmp(optarg, "B"))
					{
						m_iEditQueueAction = bGroup ? eRemoteEditActionGroupMoveBottom : eRemoteEditActionFileMoveBottom;
					}
					else if (!strcasecmp(optarg, "P"))
					{
						m_iEditQueueAction = bGroup ? eRemoteEditActionGroupPause : eRemoteEditActionFilePause;
					}
					else if (!strcasecmp(optarg, "A"))
					{
						m_iEditQueueAction = bGroup ? eRemoteEditActionGroupPauseAllPars : eRemoteEditActionFilePauseAllPars;
					}
					else if (!strcasecmp(optarg, "R"))
					{
						m_iEditQueueAction = bGroup ? eRemoteEditActionGroupPauseExtraPars : eRemoteEditActionFilePauseExtraPars;
					}
					else if (!strcasecmp(optarg, "U"))
					{
						m_iEditQueueAction = bGroup ? eRemoteEditActionGroupResume : eRemoteEditActionFileResume;
					}
					else if (!strcasecmp(optarg, "D"))
					{
						m_iEditQueueAction = bGroup ? eRemoteEditActionGroupDelete : eRemoteEditActionFileDelete;
					}
					else if (!strcasecmp(optarg, "C") || !strcasecmp(optarg, "K"))
					{
						// switch "K" is provided for compatibility with v. 0.8.0 and can be removed in future versions
						if (!bGroup)
						{
							abort("FATAL ERROR: Category can be set only for groups\n");
						}
						m_iEditQueueAction = eRemoteEditActionGroupSetCategory;

						optind++;
						if (optind > argc)
						{
							abort("FATAL ERROR: Could not parse value of option 'E'\n");
						}
						m_szEditQueueText = strdup(argv[optind-1]);
					}
					else if (!strcasecmp(optarg, "N"))
					{
						if (!bGroup)
						{
							abort("FATAL ERROR: Only groups can be renamed\n");
						}
						m_iEditQueueAction = eRemoteEditActionGroupSetName;

						optind++;
						if (optind > argc)
						{
							abort("FATAL ERROR: Could not parse value of option 'E'\n");
						}
						m_szEditQueueText = strdup(argv[optind-1]);
					}
					else if (!strcasecmp(optarg, "M"))
					{
						if (!bGroup)
						{
							abort("FATAL ERROR: Only groups can be merged\n");
						}
						m_iEditQueueAction = eRemoteEditActionGroupMerge;
					}
					else if (!strcasecmp(optarg, "S"))
					{
						m_iEditQueueAction = eRemoteEditActionFileSplit;

						optind++;
						if (optind > argc)
						{
							abort("FATAL ERROR: Could not parse value of option 'E'\n");
						}
						m_szEditQueueText = strdup(argv[optind-1]);
					}
					else if (!strcasecmp(optarg, "O"))
					{
						if (!bGroup)
						{
							abort("FATAL ERROR: Post-process parameter can be set only for groups\n");
						}
						m_iEditQueueAction = eRemoteEditActionGroupSetParameter;

						optind++;
						if (optind > argc)
						{
							abort("FATAL ERROR: Could not parse value of option 'E'\n");
						}
						m_szEditQueueText = strdup(argv[optind-1]);

						if (!strchr(m_szEditQueueText, '='))
						{
							abort("FATAL ERROR: Could not parse value of option 'E'\n");
						}
					}
					else if (!strcasecmp(optarg, "I"))
					{
						m_iEditQueueAction = bGroup ? eRemoteEditActionGroupSetPriority : eRemoteEditActionFileSetPriority;

						optind++;
						if (optind > argc)
						{
							abort("FATAL ERROR: Could not parse value of option 'E'\n");
						}
						m_szEditQueueText = strdup(argv[optind-1]);

						if (atoi(m_szEditQueueText) == 0 && strcmp("0", m_szEditQueueText))
						{
							abort("FATAL ERROR: Could not parse value of option 'E'\n");
						}
					}
					else
					{
						m_iEditQueueOffset = atoi(optarg);
						if (m_iEditQueueOffset == 0)
						{
							abort("FATAL ERROR: Could not parse value of option 'E'\n");
						}
						m_iEditQueueAction = bGroup ? eRemoteEditActionGroupMoveOffset : eRemoteEditActionFileMoveOffset;
					}
				}
				break;
			}
			case 'Q':
				m_eClientOperation = opClientRequestShutdown;
				break;
			case 'O':
				m_eClientOperation = opClientRequestReload;
				break;
			case 'V':
				m_eClientOperation = opClientRequestVersion;
				break;
			case 'W':
				m_eClientOperation = opClientRequestWriteLog;
				if (!strcasecmp(optarg, "I")) {
					m_iWriteLogKind = (int)Message::mkInfo;
				}
				else if (!strcasecmp(optarg, "W")) {
					m_iWriteLogKind = (int)Message::mkWarning;
				}
				else if (!strcasecmp(optarg, "E")) {
					m_iWriteLogKind = (int)Message::mkError;
				}
				else if (!strcasecmp(optarg, "D")) {
					m_iWriteLogKind = (int)Message::mkDetail;
				}
				else if (!strcasecmp(optarg, "G")) {
					m_iWriteLogKind = (int)Message::mkDebug;
				}
				else
				{
					abort("FATAL ERROR: Could not parse value of option 'W'\n");
				}
				break;
			case 'K':
				// switch "K" is provided for compatibility with v. 0.8.0 and can be removed in future versions
				if (m_szAddCategory)
				{
					free(m_szAddCategory);
				}
				m_szAddCategory = strdup(optarg);
				break;
			case 'S':
				optind++;
				optarg = optind > argc ? NULL : argv[optind-1];
				if (!optarg || !strncmp(optarg, "-", 1))
				{
					m_eClientOperation = opClientRequestScanAsync;
					optind--;
				}
				else if (!strcasecmp(optarg, "W"))
				{
					m_eClientOperation = opClientRequestScanSync;
				}
				else
				{
					abort("FATAL ERROR: Could not parse value of option '%c'\n", c);
				}
				break;
			case '?':
				exit(-1);
				break;
		}
	}

	if (m_bServerMode && (m_eClientOperation == opClientRequestDownloadPause ||
		m_eClientOperation == opClientRequestDownload2Pause))
	{
		m_bPauseDownload = m_eClientOperation == opClientRequestDownloadPause;
		m_bPauseDownload2 = m_eClientOperation == opClientRequestDownload2Pause;
		m_eClientOperation = opClientNoOperation;
	}

	InitOptFile();
}

void Options::PrintUsage(char* com)
{
	printf("Usage:\n"
		"  %s [switches]\n\n"
		"Switches:\n"
	    "  -h, --help                Print this help-message\n"
	    "  -v, --version             Print version and exit\n"
		"  -c, --configfile <file>   Filename of configuration-file\n"
		"  -n, --noconfigfile        Prevent loading of configuration-file\n"
		"                            (required options must be passed with --option)\n"
		"  -p, --printconfig         Print configuration and exit\n"
		"  -o, --option <name=value> Set or override option in configuration-file\n"
		"  -s, --server              Start nzbget as a server in console-mode\n"
#ifndef WIN32
		"  -D, --daemon              Start nzbget as a server in daemon-mode\n"
#endif
	    "  -V, --serverversion       Print server's version and exit\n"
		"  -Q, --quit                Shutdown server\n"
		"  -O, --reload              Reload config and restart all services\n"
		"  -A, --append  [F|U] [<options>] <nzb-file/url> Send file/url to server's\n"
		"                            download queue\n"
		"                 F          Send file (default)\n"
		"                 U          Send url\n"
		"    <options> are (multiple options must be separated with space):\n"
		"       T                    Add file to the top (beginning) of queue\n"
		"       P                    Pause added files\n"
		"       C <name>             Assign category to nzb-file\n"
		"       N <name>             Use this name as nzb-filename (only for URLs)\n"
		"       I <priority>         Set priority (signed integer)\n"
		"  -C, --connect             Attach client to server\n"
		"  -L, --list    [F|FR|G|GR|O|U|H|S] [RegEx] Request list of items from server\n"
		"                 F          List individual files and server status (default)\n"
		"                 FR         Like \"F\" but apply regular expression filter\n"
		"                 G          List groups (nzb-files) and server status\n"
		"                 GR         Like \"G\" but apply regular expression filter\n"
		"                 O          List post-processor-queue\n"
		"                 U          List url-queue\n"
		"                 H          List history\n"
		"                 S          Print only server status\n"
		"    <RegEx>                 Regular expression (only with options \"FR\", \"GR\")\n"
		"                            using POSIX Extended Regular Expression Syntax\n"
		"  -P, --pause   [D|D2|O|S]  Pause server\n"
		"                 D          Pause download queue (default)\n"
		"                 D2         Pause download queue via second pause-register\n"
		"                 O          Pause post-processor queue\n"
		"                 S          Pause scan of incoming nzb-directory\n"
		"  -U, --unpause [D|D2|O|S]  Unpause server\n"
		"                 D          Unpause download queue (default)\n"
		"                 D2         Unpause download queue via second pause-register\n"
		"                 O          Unpause post-processor queue\n"
		"                 S          Unpause scan of incoming nzb-directory\n"
		"  -R, --rate <speed>        Set download rate on server, in KB/s\n"
		"  -G, --log <lines>         Request last <lines> lines from server's screen-log\n"
		"  -W, --write <D|I|W|E|G> \"Text\" Send text to server's log\n"
		"  -S, --scan    [W]         Scan incoming nzb-directory on server\n"
		"                 W          Wait until scan completes (synchronous mode)\n"
		"  -E, --edit [F|FN|FR|G|GN|GR|O|H] <action> <IDs/Names/RegExs> Edit items\n"
		"                            on server\n"
		"              F             Edit individual files (default)\n"
		"              FN            Like \"F\" but uses names (as \"group/file\")\n"
		"                            instead of IDs\n"
		"              FR            Like \"FN\" but with regular expressions\n"
		"              G             Edit all files in the group (same nzb-file)\n"
		"              GN            Like \"G\" but uses group names instead of IDs\n"
		"              GR            Like \"GN\" but with regular expressions\n"
		"              O             Edit post-processor-queue\n"
		"              H             Edit history\n"
		"    <action> is one of:\n"
		"       <+offset|-offset>    Move files/groups/post-job in queue relative to\n"
		"                            current position, offset is an integer value\n"
		"       T                    Move files/groups/post-job to top of queue\n"
		"       B                    Move files/groups/post-job to bottom of queue\n"
		"       P                    Pause file(s)/group(s)/\n"
		"                            Postprocess history-item(s) again\n"
		"       U                    Resume (unpause) files/groups\n"
		"       A                    Pause all pars (for groups)\n"
		"       R                    Pause extra pars (for groups)/\n"
		"                            Return history-items back to download queue\n"
		"       D                    Delete files/groups/post-jobs/history-items\n"
		"       C <name>             Set category (for groups)\n"
		"       N <name>             Rename (for groups)\n"
		"       M                    Merge (for groups)\n"
		"       S <name>             Split - create new group from selected files\n"
		"       O <name>=<value>     Set post-process parameter (for groups/history)\n"
		"       I <priority>         Set priority (signed integer) for files/groups\n"
		"    <IDs>                   Comma-separated list of file-ids or ranges\n"
		"                            of file-ids, e. g.: 1-5,3,10-22\n"
		"    <Names>                 List of names (with options \"FN\" and \"GN\"),\n"
		"                            e. g.: \"my nzb download%cmyfile.nfo\" \"another nzb\"\n"
		"    <RegExs>                List of regular expressions (options \"FR\", \"GR\")\n"
		"                            using POSIX Extended Regular Expression Syntax",
		Util::BaseFileName(com),
		PATH_SEPARATOR, PATH_SEPARATOR);
}

void Options::InitFileArg(int argc, char* argv[])
{
	if (optind >= argc)
	{
		// no nzb-file passed
		if (!m_bServerMode && !m_bRemoteClientMode &&
		        (m_eClientOperation == opClientNoOperation ||
		         m_eClientOperation == opClientRequestDownload ||
				 m_eClientOperation == opClientRequestWriteLog))
		{
			if (m_eClientOperation == opClientRequestWriteLog)
			{
				abort("FATAL ERROR: Log-text not specified\n");
			}
			else
			{
				abort("FATAL ERROR: Nzb-file not specified\n");
			}
		}
	}
	else if (m_eClientOperation == opClientRequestEditQueue)
	{
		if (m_EMatchMode == mmID)
		{
			ParseFileIDList(argc, argv, optind);
		}
		else
		{
			ParseFileNameList(argc, argv, optind);
		}
	}
	else
	{
		m_szLastArg = strdup(argv[optind]);

		// Check if the file-name is a relative path or an absolute path
		// If the path starts with '/' its an absolute, else relative
		const char* szFileName = argv[optind];

#ifdef WIN32
			m_szArgFilename = strdup(szFileName);
#else
		if (szFileName[0] == '/')
		{
			m_szArgFilename = strdup(szFileName);
		}
		else
		{
			// TEST
			char szFileNameWithPath[1024];
			getcwd(szFileNameWithPath, 1024);
			strcat(szFileNameWithPath, "/");
			strcat(szFileNameWithPath, szFileName);
			m_szArgFilename = strdup(szFileNameWithPath);
		}
#endif

		if (m_bServerMode || m_bRemoteClientMode ||
		        !(m_eClientOperation == opClientNoOperation ||
		          m_eClientOperation == opClientRequestDownload ||
		          m_eClientOperation == opClientRequestDownloadUrl ||
				  m_eClientOperation == opClientRequestWriteLog))
		{
			abort("FATAL ERROR: Too many arguments\n");
		}
	}
}

void Options::SetOption(const char* optname, const char* value)
{
	OptEntry* pOptEntry = FindOption(optname);
	if (!pOptEntry)
	{
		pOptEntry = new OptEntry();
		pOptEntry->SetName(optname);
		m_OptEntries.push_back(pOptEntry);
	}

	char* curvalue = NULL;

#ifndef WIN32
	if (value && (value[0] == '~') && (value[1] == '/'))
	{
		char szExpandedPath[1024];
		if (!Util::ExpandHomePath(value, szExpandedPath, sizeof(szExpandedPath)))
		{
			ConfigError("Invalid value for option\"%s\": unable to determine home-directory", optname);
			szExpandedPath[0] = '\0';
		}
		curvalue = strdup(szExpandedPath);
	}
	else
#endif
	{
		curvalue = strdup(value);
	}

	pOptEntry->SetLineNo(m_iConfigLine);
	bool bOK = true;

	// expand variables
	while (char* dollar = strstr(curvalue, "${"))
	{
		char* end = strchr(dollar, '}');
		if (end)
		{
			int varlen = (int)(end - dollar - 2);
			char variable[101];
			int maxlen = varlen < 100 ? varlen : 100;
			strncpy(variable, dollar + 2, maxlen);
			variable[maxlen] = '\0';
			const char* varvalue = GetOption(variable);
			if (varvalue)
			{
				int newlen = strlen(varvalue);
				char* newvalue = (char*)malloc(strlen(curvalue) - varlen - 3 + newlen + 1);
				strncpy(newvalue, curvalue, dollar - curvalue);
				strncpy(newvalue + (dollar - curvalue), varvalue, newlen);
				strcpy(newvalue + (dollar - curvalue) + newlen, end + 1);
				free(curvalue);
				curvalue = newvalue;
			}
			else
			{
				ConfigError("Invalid value for option \"%s\": variable \"%s\" not found", optname, variable);
				bOK = false;
				break;
			}
		}
		else
		{
			ConfigError("Invalid value for option \"%s\": syntax error in variable-substitution \"%s\"", optname, curvalue);
			bOK = false;
			break;
		}
	}

	if (bOK)
	{
		pOptEntry->SetValue(curvalue);
	}

	free(curvalue);
}

Options::OptEntry* Options::FindOption(const char* optname)
{
	OptEntry* pOptEntry = m_OptEntries.FindOption(optname);

	// normalize option name in option list; for example "server1.joingroup" -> "Server1.JoinGroup"
	if (pOptEntry && strcmp(pOptEntry->GetName(), optname))
	{
		pOptEntry->SetName(optname);
	}

	return pOptEntry;
}

const char* Options::GetOption(const char* optname)
{
	OptEntry* pOptEntry = FindOption(optname);
	if (pOptEntry)
	{
		if (pOptEntry->GetLineNo() > 0)
		{
			m_iConfigLine = pOptEntry->GetLineNo();
		}
		return pOptEntry->GetValue();
	}
	return NULL;
}

void Options::InitServers()
{
	int n = 1;
	while (true)
	{
		char optname[128];

		sprintf(optname, "Server%i.Level", n);
		const char* nlevel = GetOption(optname);

		sprintf(optname, "Server%i.Group", n);
		const char* ngroup = GetOption(optname);
		
		sprintf(optname, "Server%i.Host", n);
		const char* nhost = GetOption(optname);

		sprintf(optname, "Server%i.Port", n);
		const char* nport = GetOption(optname);

		sprintf(optname, "Server%i.Username", n);
		const char* nusername = GetOption(optname);

		sprintf(optname, "Server%i.Password", n);
		const char* npassword = GetOption(optname);

		sprintf(optname, "Server%i.JoinGroup", n);
		const char* njoingroup = GetOption(optname);
		bool bJoinGroup = false;
		if (njoingroup)
		{
			bJoinGroup = (bool)ParseEnumValue(optname, BoolCount, BoolNames, BoolValues);
		}

		sprintf(optname, "Server%i.Encryption", n);
		const char* ntls = GetOption(optname);
		bool bTLS = false;
		if (ntls)
		{
			bTLS = (bool)ParseEnumValue(optname, BoolCount, BoolNames, BoolValues);
#ifdef DISABLE_TLS
			if (bTLS)
			{
				ConfigError("Invalid value for option \"%s\": program was compiled without TLS/SSL-support", optname);
				bTLS = false;
			}
#endif
			m_bTLS |= bTLS;
		}

		sprintf(optname, "Server%i.Cipher", n);
		const char* ncipher = GetOption(optname);

		sprintf(optname, "Server%i.Connections", n);
		const char* nconnections = GetOption(optname);

		bool definition = nlevel || ngroup || nhost || nport || nusername || npassword || nconnections || njoingroup || ntls || ncipher;
		bool completed = nhost && nport && nconnections;

		if (!definition)
		{
			break;
		}

		if (completed)
		{
			NewsServer* pNewsServer = new NewsServer(n, nhost, atoi(nport), nusername, npassword,
				bJoinGroup, bTLS, ncipher, atoi((char*)nconnections),
				nlevel ? atoi((char*)nlevel) : 0,
				ngroup ? atoi((char*)ngroup) : 0);
			g_pServerPool->AddServer(pNewsServer);
		}
		else
		{
			ConfigError("Server definition not complete for \"Server%i\"", n);
		}

		n++;
	}

	g_pServerPool->SetTimeout(GetConnectionTimeout());
}

void Options::InitCategories()
{
	int n = 1;
	while (true)
	{
		char optname[128];

		sprintf(optname, "Category%i.Name", n);
		const char* nname = GetOption(optname);

		char destdiroptname[128];
		sprintf(destdiroptname, "Category%i.DestDir", n);
		const char* ndestdir = GetOption(destdiroptname);

		sprintf(optname, "Category%i.DefScript", n);
		const char* ndefscript = GetOption(optname);

		bool definition = nname || ndestdir || ndefscript;
		bool completed = nname && strlen(nname) > 0;

		if (!definition)
		{
			break;
		}

		if (completed)
		{
			char* szDestDir = NULL;
			if (ndestdir && ndestdir[0] != '\0')
			{
				CheckDir(&szDestDir, destdiroptname, false, true);
			}

			Category* pCategory = new Category(nname, szDestDir, ndefscript);
			m_Categories.push_back(pCategory);

			if (szDestDir)
			{
				free(szDestDir);
			}
		}
		else
		{
			ConfigError("Category definition not complete for \"Category%i\"", n);
		}

		n++;
	}
}

void Options::InitScheduler()
{
	int n = 1;
	while (true)
	{
		char optname[128];

		sprintf(optname, "Task%i.Time", n);
		const char* szTime = GetOption(optname);

		sprintf(optname, "Task%i.WeekDays", n);
		const char* szWeekDays = GetOption(optname);

		sprintf(optname, "Task%i.Command", n);
		const char* szCommand = GetOption(optname);

		sprintf(optname, "Task%i.DownloadRate", n);
		const char* szDownloadRate = GetOption(optname);

		sprintf(optname, "Task%i.Process", n);
		const char* szProcess = GetOption(optname);

		bool definition = szTime || szWeekDays || szCommand || szDownloadRate || szProcess;
		bool completed = szTime && szCommand;

		if (!definition)
		{
			break;
		}

		bool bOK = true;

		if (!completed)
		{
			ConfigError("Task definition not complete for \"Task%i\"", n);
			bOK = false;
		}

		if (szProcess && strlen(szProcess) > 0 && !Util::SplitCommandLine(szProcess, NULL))
		{
			ConfigError("Invalid value for option \"Task%i.Process\"", n);
			bOK = false;
		}

		snprintf(optname, sizeof(optname), "Task%i.Command", n);
		optname[sizeof(optname)-1] = '\0';

		const char* CommandNames[] = { "pausedownload", "pause", "unpausedownload", "resumedownload", "unpause", "resume", "downloadrate", "setdownloadrate",
			"rate", "speed", "script", "process", "pausescan", "unpausescan", "resumescan" };
		const int CommandValues[] = { Scheduler::scPauseDownload, Scheduler::scPauseDownload, Scheduler::scUnpauseDownload, Scheduler::scUnpauseDownload, Scheduler::scUnpauseDownload, Scheduler::scUnpauseDownload, Scheduler::scDownloadRate, Scheduler::scDownloadRate,
			Scheduler::scDownloadRate, Scheduler::scDownloadRate, Scheduler::scProcess, Scheduler::scProcess, Scheduler::scPauseScan, Scheduler::scUnpauseScan, Scheduler::scUnpauseScan };
		const int CommandCount = 15;
		Scheduler::ECommand eCommand = (Scheduler::ECommand)ParseEnumValue(optname, CommandCount, CommandNames, CommandValues);

		int iWeekDays = 0;
		if (szWeekDays && !ParseWeekDays(szWeekDays, &iWeekDays))
		{
			ConfigError("Invalid value for option \"Task%i.WeekDays\": \"%s\"", n, szWeekDays);
			bOK = false;
		}

		int iDownloadRate = 0;
		if (eCommand == Scheduler::scDownloadRate)
		{
			if (szDownloadRate)
			{
				char* szErr;
				iDownloadRate = strtol(szDownloadRate, &szErr, 10);
				if (!szErr || *szErr != '\0' || iDownloadRate < 0)
				{
					ConfigError("Invalid value for option \"Task%i.DownloadRate\": \"%s\"", n, szDownloadRate);
					bOK = false;
				}
			}
			else
			{
				ConfigError("Task definition not complete for \"Task%i\". Option \"Task%i.DownloadRate\" is missing", n, n);
				bOK = false;
			}
		}

		if (eCommand == Scheduler::scProcess && (!szProcess || strlen(szProcess) == 0))
		{
			ConfigError("Task definition not complete for \"Task%i\". Option \"Task%i.Process\" is missing", n, n);
			bOK = false;
		}

		int iHours, iMinutes;
		const char** pTime = &szTime;
		while (*pTime)
		{
			if (!ParseTime(pTime, &iHours, &iMinutes))
			{
				ConfigError("Invalid value for option \"Task%i.Time\": \"%s\"", n, pTime);
				break;
			}

			if (bOK)
			{
				if (iHours == -1)
				{
					for (int iEveryHour = 0; iEveryHour < 24; iEveryHour++)
					{
						Scheduler::Task* pTask = new Scheduler::Task(iEveryHour, iMinutes, iWeekDays, eCommand, iDownloadRate * 1024, szProcess);
						g_pScheduler->AddTask(pTask);
					}
				}
				else
				{
					Scheduler::Task* pTask = new Scheduler::Task(iHours, iMinutes, iWeekDays, eCommand, iDownloadRate * 1024, szProcess);
					g_pScheduler->AddTask(pTask);
				}
			}
		}

		n++;
	}
}

/*
* Parses Time string and moves current string pointer to the next time token.
*/
bool Options::ParseTime(const char** pTime, int* pHours, int* pMinutes)
{
	const char* szTime = *pTime;
	const char* szComma = strchr(szTime, ',');

	int iColons = 0;
	const char* p = szTime;
	while (*p && (!szComma || p != szComma))
	{
		if (!strchr("0123456789: *", *p))
		{
			return false;
		}
		if (*p == ':')
		{
			iColons++;
		}
		p++;
	}

	if (iColons != 1)
	{
		return false;
	}

	const char* szColon = strchr(szTime, ':');
	if (!szColon)
	{
		return false;
	}

	if (szTime[0] == '*')
	{
		*pHours = -1;
	}
	else
	{
		*pHours = atoi(szTime);
		if (*pHours < 0 || *pHours > 23)
		{
			return false;
		}
	}

	if (szColon[1] == '*')
	{
		return false;
	}
	*pMinutes = atoi(szColon + 1);
	if (*pMinutes < 0 || *pMinutes > 59)
	{
		return false;
	}

	if (szComma)
	{
		*pTime = szComma + 1;
	}
	else
	{
		*pTime = NULL;
	}

	return true;
}

bool Options::ParseWeekDays(const char* szWeekDays, int* pWeekDaysBits)
{
	*pWeekDaysBits = 0;
	const char* p = szWeekDays;
	int iFirstDay = 0;
	bool bRange = false;
	while (*p)
	{
		if (strchr("1234567", *p))
		{
			int iDay = *p - '0';
			if (bRange)
			{
				if (iDay <= iFirstDay || iFirstDay == 0)
				{
					return false;
				}
				for (int i = iFirstDay; i <= iDay; i++)
				{
					*pWeekDaysBits |= 1 << (i - 1);
				}
				iFirstDay = 0;
			}
			else
			{
				*pWeekDaysBits |= 1 << (iDay - 1);
				iFirstDay = iDay;
			}
			bRange = false;
		}
		else if (*p == ',')
		{
			bRange = false;
		}
		else if (*p == '-')
		{
			bRange = true;
		}
		else if (*p == ' ')
		{
			// skip spaces
		}
		else
		{
			return false;
		}
		p++;
	}
	return true;
}

void Options::LoadConfigFile()
{
	FILE* infile = fopen(m_szConfigFilename, "rb");

	if (!infile)
	{
		abort("FATAL ERROR: Could not open file %s\n", m_szConfigFilename);
	}

	m_iConfigLine = 0;
	int iLine = 0;
	char buf[1024];
	while (fgets(buf, sizeof(buf) - 1, infile))
	{
		m_iConfigLine = ++iLine;

		if (buf[0] != 0 && buf[strlen(buf)-1] == '\n')
		{
			buf[strlen(buf)-1] = 0; // remove traling '\n'
		}
		if (buf[0] != 0 && buf[strlen(buf)-1] == '\r')
		{
			buf[strlen(buf)-1] = 0; // remove traling '\r' (for windows line endings)
		}

		if (buf[0] == 0 || buf[0] == '#' || strspn(buf, " ") == strlen(buf))
		{
			continue;
		}

		SetOptionString(buf);
	}

	fclose(infile);

	m_iConfigLine = 0;
}

bool Options::SetOptionString(const char * option)
{
	const char* eq = strchr(option, '=');
	if (eq)
	{
		char optname[1001];
		char optvalue[1001];
		int maxlen = (int)(eq - option < 1000 ? eq - option : 1000);
		strncpy(optname, option, maxlen);
		optname[maxlen] = '\0';
		strncpy(optvalue, eq + 1, 1000);
		optvalue[1000]  = '\0';
		if (strlen(optname) > 0)
		{
			ConvertOldOption(optname, sizeof(optname), optvalue, sizeof(optvalue));

			if (!ValidateOptionName(optname))
			{
				ConfigError("Invalid option \"%s\"", optname);
				return false;
			}
			SetOption(optname, optvalue);
		}
		return true;
	}
	else
	{
		ConfigError("Invalid option \"%s\"", option);
		return false;
	}
}

bool Options::ValidateOptionName(const char * optname)
{
	if (!strcasecmp(optname, OPTION_CONFIGFILE) || !strcasecmp(optname, OPTION_APPBIN) ||
		!strcasecmp(optname, OPTION_APPDIR) || !strcasecmp(optname, OPTION_VERSION))
	{
		// read-only options
		return false;
	}

	const char* v = GetOption(optname);
	if (v)
	{
		// it's predefined option, OK
		return true;
	}

	if (!strncasecmp(optname, "server", 6))
	{
		char* p = (char*)optname + 6;
		while (*p >= '0' && *p <= '9') p++;
		if (p &&
			(!strcasecmp(p, ".level") || !strcasecmp(p, ".host") ||
			!strcasecmp(p, ".port") || !strcasecmp(p, ".username") ||
			!strcasecmp(p, ".password") || !strcasecmp(p, ".joingroup") ||
			!strcasecmp(p, ".encryption") || !strcasecmp(p, ".connections") ||
			!strcasecmp(p, ".cipher") || !strcasecmp(p, ".group")))
		{
			return true;
		}
	}

	if (!strncasecmp(optname, "task", 4))
	{
		char* p = (char*)optname + 4;
		while (*p >= '0' && *p <= '9') p++;
		if (p && (!strcasecmp(p, ".time") || !strcasecmp(p, ".weekdays") ||
			!strcasecmp(p, ".command") || !strcasecmp(p, ".downloadrate") || !strcasecmp(p, ".process")))
		{
			return true;
		}
	}
	
	if (!strncasecmp(optname, "category", 8))
	{
		char* p = (char*)optname + 8;
		while (*p >= '0' && *p <= '9') p++;
		if (p && (!strcasecmp(p, ".name") || !strcasecmp(p, ".destdir") || !strcasecmp(p, ".defscript")))
		{
			return true;
		}
	}

	// post-processing scripts options
	if (strchr(optname, ':'))
	{
		return true;
	}

	// print a warning message for obsolete options
	if (!strcasecmp(optname, OPTION_POSTLOGKIND) || !strcasecmp(optname, OPTION_NZBLOGKIND))
	{
		ConfigError("Option \"%s\" is obsolete, ignored, use \"%s\" instead", optname, OPTION_PROCESSLOGKIND);
		return true;
	}
	if (!strcasecmp(optname, OPTION_RETRYONCRCERROR) ||
		!strcasecmp(optname, OPTION_ALLOWREPROCESS) ||
		!strcasecmp(optname, OPTION_LOADPARS))
	{
		ConfigWarn("Option \"%s\" is obsolete, ignored", optname);
		return true;
	}
	if (!strcasecmp(optname, OPTION_POSTPROCESS))
	{
		ConfigError("Option \"%s\" is obsolete, ignored, use \"%s\" and \"%s\" instead", optname, OPTION_SCRIPTDIR, OPTION_DEFSCRIPT);
		return true;
	}

	return false;
}

void Options::CheckOptions()
{
#ifdef DISABLE_PARCHECK
	if (m_eParCheck != pcManual)
	{
		LocateOptionSrcPos(OPTION_PARCHECK);
		ConfigError("Invalid value for option \"%s\": program was compiled without parcheck-support", OPTION_PARCHECK);
	}
#endif

#ifdef DISABLE_CURSES
	if (m_eOutputMode == omNCurses)
	{
		LocateOptionSrcPos(OPTION_OUTPUTMODE);
		ConfigError("Invalid value for option \"%s\": program was compiled without curses-support", OPTION_OUTPUTMODE);
	}
#endif

#ifdef DISABLE_TLS
	if (m_bSecureControl)
	{
		LocateOptionSrcPos(OPTION_SECURECONTROL);
		ConfigError("Invalid value for option \"%s\": program was compiled without TLS/SSL-support", OPTION_SECURECONTROL);
	}
#endif

	if (!m_bDecode)
	{
		m_bDirectWrite = false;
	}

	// if option "ConfigTemplate" is not set, use "WebDir" as default location for template
	// (for compatibility with versions 9 and 10).
	if (!m_szConfigTemplate || m_szConfigTemplate[0] == '\0')
	{
		if (m_szConfigTemplate)
		{
			free(m_szConfigTemplate);
		}
		int iLen = strlen(m_szWebDir) + 15;
		m_szConfigTemplate = (char*)malloc(iLen);
		snprintf(m_szConfigTemplate, iLen, "%s%s", m_szWebDir, "nzbget.conf");
		m_szConfigTemplate[iLen-1] = '\0';
		if (!Util::FileExists(m_szConfigTemplate))
		{
			free(m_szConfigTemplate);
			m_szConfigTemplate = strdup("");
		}
	}
}

void Options::ParseFileIDList(int argc, char* argv[], int optind)
{
	std::vector<int> IDs;
	IDs.clear();

	while (optind < argc)
	{
		char* szWritableFileIDList = strdup(argv[optind++]);

		char* optarg = strtok(szWritableFileIDList, ", ");
		while (optarg)
		{
			int iEditQueueIDFrom = 0;
			int iEditQueueIDTo = 0;
			const char* p = strchr(optarg, '-');
			if (p)
			{
				char buf[101];
				int maxlen = (int)(p - optarg < 100 ? p - optarg : 100);
				strncpy(buf, optarg, maxlen);
				buf[maxlen] = '\0';
				iEditQueueIDFrom = atoi(buf);
				iEditQueueIDTo = atoi(p + 1);
				if (iEditQueueIDFrom <= 0 || iEditQueueIDTo <= 0)
				{
					abort("FATAL ERROR: invalid list of file IDs\n");
				}
			}
			else
			{
				iEditQueueIDFrom = atoi(optarg);
				if (iEditQueueIDFrom <= 0)
				{
					abort("FATAL ERROR: invalid list of file IDs\n");
				}
				iEditQueueIDTo = iEditQueueIDFrom;
			}

			int iEditQueueIDCount = 0;
			if (iEditQueueIDTo != 0)
			{
				if (iEditQueueIDFrom < iEditQueueIDTo)
				{
					iEditQueueIDCount = iEditQueueIDTo - iEditQueueIDFrom + 1;
				}
				else
				{
					iEditQueueIDCount = iEditQueueIDFrom - iEditQueueIDTo + 1;
				}
			}
			else
			{
				iEditQueueIDCount = 1;
			}

			for (int i = 0; i < iEditQueueIDCount; i++)
			{
				if (iEditQueueIDFrom < iEditQueueIDTo || iEditQueueIDTo == 0)
				{
					IDs.push_back(iEditQueueIDFrom + i);
				}
				else
				{
					IDs.push_back(iEditQueueIDFrom - i);
				}
			}

			optarg = strtok(NULL, ", ");
		}

		free(szWritableFileIDList);
	}

	m_iEditQueueIDCount = IDs.size();
	m_pEditQueueIDList = (int*)malloc(sizeof(int) * m_iEditQueueIDCount);
	for (int i = 0; i < m_iEditQueueIDCount; i++)
	{
		m_pEditQueueIDList[i] = IDs[i];
	}
}

void Options::ParseFileNameList(int argc, char* argv[], int optind)
{
	while (optind < argc)
	{
		m_EditQueueNameList.push_back(strdup(argv[optind++]));
	}
}

Options::OptEntries* Options::LockOptEntries()
{
	m_mutexOptEntries.Lock();
	return &m_OptEntries;
}

void Options::UnlockOptEntries()
{
	m_mutexOptEntries.Unlock();
}

bool Options::LoadConfig(OptEntries* pOptEntries)
{
	// read config file
	FILE* infile = fopen(m_szConfigFilename, "rb");

	if (!infile)
	{
		return false;
	}

	char buf[1024];
	while (fgets(buf, sizeof(buf) - 1, infile))
	{
		// remove trailing '\n' and '\r' and spaces
		Util::TrimRight(buf);

		// skip comments and empty lines
		if (buf[0] == 0 || buf[0] == '#' || strspn(buf, " ") == strlen(buf))
		{
			continue;
		}

		const char* eq = strchr(buf, '=');
		if (eq)
		{
			char optname[1024];
			char optvalue[1024];
			int len = (int)(eq - buf);
			strncpy(optname, buf, len);
			optname[len] = '\0';
			strncpy(optvalue, eq + 1, 1024);
			optvalue[1024-1]  = '\0';
			if (strlen(optname) > 0)
			{
				ConvertOldOption(optname, sizeof(optname), optvalue, sizeof(optvalue));

				OptEntry* pOptEntry = new OptEntry();
				pOptEntry->SetName(optname);
				pOptEntry->SetValue(optvalue);
				pOptEntries->push_back(pOptEntry);
			}
		}
	}

	fclose(infile);

	return true;
}

bool Options::SaveConfig(OptEntries* pOptEntries)
{
	// save to config file
	FILE* infile = fopen(m_szConfigFilename, "r+b");

	if (!infile)
	{
		return false;
	}

	std::vector<char*> config;
	std::set<OptEntry*> writtenOptions;

	// read config file into memory array
	char buf[1024];
	char val[1024];
	while (fgets(buf, sizeof(buf) - 1, infile))
	{
		config.push_back(strdup(buf));
	}

	// write config file back to disk, replace old values of existing options with new values
	rewind(infile);
	for (std::vector<char*>::iterator it = config.begin(); it != config.end(); it++)
    {
        char* buf = *it;

		const char* eq = strchr(buf, '=');
		if (eq && buf[0] != '#')
		{
			// remove trailing '\n' and '\r' and spaces
			Util::TrimRight(buf);

			char optname[1024];
			char optvalue[1024];
			int len = (int)(eq - buf);
			strncpy(optname, buf, len);
			optname[len] = '\0';
			strncpy(optvalue, eq + 1, 1024);
			optvalue[1024-1]  = '\0';
			if (strlen(optname) > 0)
			{
				ConvertOldOption(optname, sizeof(optname), optvalue, sizeof(optvalue));

				OptEntry *pOptEntry = pOptEntries->FindOption(optname);
				if (pOptEntry)
				{
					snprintf(val, sizeof(val) - 1, "%s=%s\n", pOptEntry->GetName(), pOptEntry->GetValue());
					val[sizeof(val) - 1] = '\0';
					fputs(val, infile);
					writtenOptions.insert(pOptEntry);
				}
			}
		}
		else
		{
			fputs(buf, infile);
		}

		free(buf);
	}

	// write new options
	for (Options::OptEntries::iterator it = pOptEntries->begin(); it != pOptEntries->end(); it++)
	{
		Options::OptEntry* pOptEntry = *it;
		std::set<OptEntry*>::iterator fit = writtenOptions.find(pOptEntry);
		if (fit == writtenOptions.end())
		{
			snprintf(val, sizeof(val) - 1, "%s=%s\n", pOptEntry->GetName(), pOptEntry->GetValue());
			val[sizeof(val) - 1] = '\0';
			fputs(val, infile);
		}
	}

	// close and truncate the file
	int pos = ftell(infile);
	fclose(infile);

	Util::TruncateFile(m_szConfigFilename, pos);

	return true;
}

void Options::ConvertOldOption(char *szOption, int iOptionBufLen, char *szValue, int iValueBufLen)
{
	// for compatibility with older versions accept old option names

	if (!strcasecmp(szOption, "$MAINDIR"))
	{
		strncpy(szOption, "MainDir", iOptionBufLen);
	}

	if (!strcasecmp(szOption, "ServerIP"))
	{
		strncpy(szOption, "ControlIP", iOptionBufLen);
	}

	if (!strcasecmp(szOption, "ServerPort"))
	{
		strncpy(szOption, "ControlPort", iOptionBufLen);
	}

	if (!strcasecmp(szOption, "ServerPassword"))
	{
		strncpy(szOption, "ControlPassword", iOptionBufLen);
	}

	if (!strcasecmp(szOption, "PostPauseQueue"))
	{
		strncpy(szOption, "ScriptPauseQueue", iOptionBufLen);
	}

	if (!strcasecmp(szOption, "ParCheck") && !strcasecmp(szValue, "yes"))
	{
		strncpy(szValue, "force", iValueBufLen);
	}

	if (!strcasecmp(szOption, "ParCheck") && !strcasecmp(szValue, "no"))
	{
		strncpy(szValue, "auto", iValueBufLen);
	}

	szOption[iOptionBufLen-1] = '\0';
	szOption[iValueBufLen-1] = '\0';
}

bool Options::LoadConfigTemplates(ConfigTemplates* pConfigTemplates)
{
	char* szBuffer;
	int iLength;
	if (!Util::LoadFileIntoBuffer(m_szConfigTemplate, &szBuffer, &iLength))
	{
		return false;
	}
	ConfigTemplate* pConfigTemplate = new ConfigTemplate("", "", szBuffer);
	pConfigTemplates->push_back(pConfigTemplate);
	free(szBuffer);

	if (!m_szScriptDir)
	{
		return true;
	}

	ScriptList scriptList;
	LoadScriptList(&scriptList);

	for (ScriptList::iterator it = scriptList.begin(); it != scriptList.end(); it++)
	{
		Script* pScript = *it;

		FILE* infile = fopen(pScript->GetLocation(), "rb");
		if (!infile)
		{
			ConfigTemplate* pConfigTemplate = new ConfigTemplate(pScript->GetName(), pScript->GetDisplayName(), "");
			pConfigTemplates->push_back(pConfigTemplate);
			continue;
		}

		const int iConfigSignatureLen = strlen(PPSCRIPT_SIGNATURE);
		StringBuilder stringBuilder;
		char buf[1024];
		bool bInConfig = false;

		while (fgets(buf, sizeof(buf) - 1, infile))
		{
			if (!strncmp(buf, PPSCRIPT_SIGNATURE, iConfigSignatureLen))
			{
				if (bInConfig)
				{
					break;
				}
				bInConfig = true;
				continue;
			}

			if (bInConfig)
			{
				stringBuilder.Append(buf);
			}
		}

		fclose(infile);

		ConfigTemplate* pConfigTemplate = new ConfigTemplate(pScript->GetName(), pScript->GetDisplayName(), stringBuilder.GetBuffer());
		pConfigTemplates->push_back(pConfigTemplate);
	}

	return true;
}

void Options::LoadScriptList(ScriptList* pScriptList)
{
	if (strlen(m_szScriptDir) == 0)
	{
		return;
	}

	ScriptList tmpScriptList;
	LoadScriptDir(&tmpScriptList, m_szScriptDir, false);
	tmpScriptList.sort(CompareScripts);

	// first add all scripts from m_szScriptOrder
	char* szScriptOrder = strdup(m_szScriptOrder);
	char* saveptr;
	char* szScriptName = strtok_r(szScriptOrder, ",;", &saveptr);
	while (szScriptName)
	{
		szScriptName = Util::Trim(szScriptName);
		if (szScriptName[0] != '\0')
		{
			Script* pScript = tmpScriptList.Find(szScriptName);
			if (pScript)
			{
				pScriptList->push_back(new Script(pScript->GetName(), pScript->GetLocation()));
			}
		}
		szScriptName = strtok_r(NULL, ",;", &saveptr);
	}
	free(szScriptOrder);

	// second add all other scripts from scripts directory
	for (ScriptList::iterator it = tmpScriptList.begin(); it != tmpScriptList.end(); it++)
	{
		Script* pScript = *it;
		if (!pScriptList->Find(pScript->GetName()))
		{
			pScriptList->push_back(new Script(pScript->GetName(), pScript->GetLocation()));
		}
	}

	BuildScriptDisplayNames(pScriptList);
}

void Options::LoadScriptDir(ScriptList* pScriptList, const char* szDirectory, bool bIsSubDir)
{
	int iBufSize = 1024*10;
	char* szBuffer = (char*)malloc(iBufSize+1);

	DirBrowser dir(szDirectory);
	while (const char* szFilename = dir.Next())
	{
		if (szFilename[0] != '.' && szFilename[0] != '_')
		{
			char szFullFilename[1024];
			snprintf(szFullFilename, 1024, "%s%s", szDirectory, szFilename);
			szFullFilename[1024-1] = '\0';

			if (!Util::DirectoryExists(szFullFilename))
			{
				// check if the file contains pp-script-signature
				FILE* infile = fopen(szFullFilename, "rb");
				if (infile)
				{
					// read first 10KB of the file and look for signature
					int iReadBytes = fread(szBuffer, 1, iBufSize, infile);
					fclose(infile);
					szBuffer[iReadBytes] = 0;
					if (strstr(szBuffer, PPSCRIPT_SIGNATURE))
					{
						char szScriptName[1024];
						if (bIsSubDir)
						{
							char szDirectory2[1024];
							snprintf(szDirectory2, 1024, "%s", szDirectory);
							szDirectory2[1024-1] = '\0';
							int iLen = strlen(szDirectory2);
							if (szDirectory2[iLen-1] == PATH_SEPARATOR || szDirectory2[iLen-1] == ALT_PATH_SEPARATOR)
							{
								// trim last path-separator
								szDirectory2[iLen-1] = '\0';
							}

							snprintf(szScriptName, 1024, "%s%c%s", Util::BaseFileName(szDirectory2), PATH_SEPARATOR, szFilename);
						}
						else
						{
							snprintf(szScriptName, 1024, "%s", szFilename);
						}
						szScriptName[1024-1] = '\0';

						Script* pScript = new Script(szScriptName, szFullFilename);
						pScriptList->push_back(pScript);
					}
				}
			}
			else if (!bIsSubDir)
			{
				snprintf(szFullFilename, 1024, "%s%s%c", szDirectory, szFilename, PATH_SEPARATOR);
				szFullFilename[1024-1] = '\0';

				LoadScriptDir(pScriptList, szFullFilename, true);
			}
		}
	}

	free(szBuffer);
}

bool Options::CompareScripts(Script* pScript1, Script* pScript2)
{
	return strcmp(pScript1->GetName(), pScript2->GetName()) < 0;
}

void Options::BuildScriptDisplayNames(ScriptList* pScriptList)
{
	// trying to use short name without path and extension.
	// if there are other scripts with the same short name - using a longer name instead (with ot without extension)

	for (ScriptList::iterator it = pScriptList->begin(); it != pScriptList->end(); it++)
	{
		Script* pScript = *it;

		char szShortName[256];
		strncpy(szShortName, pScript->GetName(), 256);
		szShortName[256-1] = '\0';
		if (char* ext = strrchr(szShortName, '.')) *ext = '\0'; // strip file extension

		const char* szDisplayName = Util::BaseFileName(szShortName);

		for (ScriptList::iterator it2 = pScriptList->begin(); it2 != pScriptList->end(); it2++)
		{
			Script* pScript2 = *it2;

			char szShortName2[256];
			strncpy(szShortName2, pScript2->GetName(), 256);
			szShortName2[256-1] = '\0';
			if (char* ext = strrchr(szShortName2, '.')) *ext = '\0'; // strip file extension

			const char* szDisplayName2 = Util::BaseFileName(szShortName2);

			if (!strcmp(szDisplayName, szDisplayName2) && pScript->GetName() != pScript2->GetName())
			{
				if (!strcmp(szShortName, szShortName2))
				{
					szDisplayName =	pScript->GetName();
				}
				else
				{
					szDisplayName =	szShortName;
				}
				break;
			}
		}

		pScript->SetDisplayName(szDisplayName);
	}
}
