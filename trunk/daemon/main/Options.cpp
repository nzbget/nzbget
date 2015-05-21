/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
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


#ifdef HAVE_CONFIG_H
#include "config.h"					  
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#include <set>
#ifdef WIN32
#include <Shlobj.h>
#else
#include <unistd.h>
#endif

#include "nzbget.h"
#include "Util.h"
#include "Options.h"
#include "Log.h"
#include "MessageBase.h"
#include "DownloadInfo.h"

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
static const char* OPTION_LOGFILE				= "LogFile";
static const char* OPTION_WRITELOG				= "WriteLog";
static const char* OPTION_ROTATELOG				= "RotateLog";
static const char* OPTION_APPENDCATEGORYDIR		= "AppendCategoryDir";
static const char* OPTION_LOCKFILE				= "LockFile";
static const char* OPTION_DAEMONUSERNAME		= "DaemonUsername";
static const char* OPTION_OUTPUTMODE			= "OutputMode";
static const char* OPTION_DUPECHECK				= "DupeCheck";
static const char* OPTION_DOWNLOADRATE			= "DownloadRate";
static const char* OPTION_CONTROLIP				= "ControlIp";
static const char* OPTION_CONTROLPORT			= "ControlPort";
static const char* OPTION_CONTROLUSERNAME		= "ControlUsername";
static const char* OPTION_CONTROLPASSWORD		= "ControlPassword";
static const char* OPTION_RESTRICTEDUSERNAME	= "RestrictedUsername";
static const char* OPTION_RESTRICTEDPASSWORD	= "RestrictedPassword";
static const char* OPTION_ADDUSERNAME			= "AddUsername";
static const char* OPTION_ADDPASSWORD			= "AddPassword";
static const char* OPTION_SECURECONTROL			= "SecureControl";
static const char* OPTION_SECUREPORT			= "SecurePort";
static const char* OPTION_SECURECERT			= "SecureCert";
static const char* OPTION_SECUREKEY				= "SecureKey";
static const char* OPTION_AUTHORIZEDIP			= "AuthorizedIP";
static const char* OPTION_ARTICLETIMEOUT		= "ArticleTimeout";
static const char* OPTION_URLTIMEOUT			= "UrlTimeout";
static const char* OPTION_SAVEQUEUE				= "SaveQueue";
static const char* OPTION_RELOADQUEUE			= "ReloadQueue";
static const char* OPTION_BROKENLOG				= "BrokenLog";
static const char* OPTION_NZBLOG				= "NzbLog";
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
static const char* OPTION_PARQUICK				= "ParQuick";
static const char* OPTION_PARRENAME				= "ParRename";
static const char* OPTION_PARBUFFER				= "ParBuffer";
static const char* OPTION_PARTHREADS			= "ParThreads";
static const char* OPTION_HEALTHCHECK			= "HealthCheck";
static const char* OPTION_SCANSCRIPT			= "ScanScript";
static const char* OPTION_QUEUESCRIPT			= "QueueScript";
static const char* OPTION_UMASK					= "UMask";
static const char* OPTION_UPDATEINTERVAL		= "UpdateInterval";
static const char* OPTION_CURSESNZBNAME			= "CursesNzbName";
static const char* OPTION_CURSESTIME			= "CursesTime";
static const char* OPTION_CURSESGROUP			= "CursesGroup";
static const char* OPTION_CRCCHECK				= "CrcCheck";
static const char* OPTION_DIRECTWRITE			= "DirectWrite";
static const char* OPTION_WRITEBUFFER			= "WriteBuffer";
static const char* OPTION_NZBDIRINTERVAL		= "NzbDirInterval";
static const char* OPTION_NZBDIRFILEAGE			= "NzbDirFileAge";
static const char* OPTION_PARCLEANUPQUEUE		= "ParCleanupQueue";
static const char* OPTION_DISKSPACE				= "DiskSpace";
static const char* OPTION_DUMPCORE				= "DumpCore";
static const char* OPTION_PARPAUSEQUEUE			= "ParPauseQueue";
static const char* OPTION_SCRIPTPAUSEQUEUE		= "ScriptPauseQueue";
static const char* OPTION_NZBCLEANUPDISK		= "NzbCleanupDisk";
static const char* OPTION_DELETECLEANUPDISK		= "DeleteCleanupDisk";
static const char* OPTION_PARTIMELIMIT			= "ParTimeLimit";
static const char* OPTION_KEEPHISTORY			= "KeepHistory";
static const char* OPTION_ACCURATERATE			= "AccurateRate";
static const char* OPTION_UNPACK				= "Unpack";
static const char* OPTION_UNPACKCLEANUPDISK		= "UnpackCleanupDisk";
static const char* OPTION_UNRARCMD				= "UnrarCmd";
static const char* OPTION_SEVENZIPCMD			= "SevenZipCmd";
static const char* OPTION_UNPACKPASSFILE		= "UnpackPassFile";
static const char* OPTION_UNPACKPAUSEQUEUE		= "UnpackPauseQueue";
static const char* OPTION_SCRIPTORDER			= "ScriptOrder";
static const char* OPTION_POSTSCRIPT			= "PostScript";
static const char* OPTION_EXTCLEANUPDISK		= "ExtCleanupDisk";
static const char* OPTION_PARIGNOREEXT			= "ParIgnoreExt";
static const char* OPTION_FEEDHISTORY			= "FeedHistory";
static const char* OPTION_URLFORCE				= "UrlForce";
static const char* OPTION_TIMECORRECTION		= "TimeCorrection";
static const char* OPTION_PROPAGATIONDELAY		= "PropagationDelay";
static const char* OPTION_ARTICLECACHE			= "ArticleCache";
static const char* OPTION_EVENTINTERVAL			= "EventInterval";

// obsolete options
static const char* OPTION_POSTLOGKIND			= "PostLogKind";
static const char* OPTION_NZBLOGKIND			= "NZBLogKind";
static const char* OPTION_RETRYONCRCERROR		= "RetryOnCrcError";
static const char* OPTION_ALLOWREPROCESS		= "AllowReProcess";
static const char* OPTION_POSTPROCESS			= "PostProcess";
static const char* OPTION_LOADPARS				= "LoadPars";
static const char* OPTION_THREADLIMIT			= "ThreadLimit";
static const char* OPTION_PROCESSLOGKIND		= "ProcessLogKind";
static const char* OPTION_APPENDNZBDIR			= "AppendNzbDir";
static const char* OPTION_RENAMEBROKEN			= "RenameBroken";
static const char* OPTION_MERGENZB				= "MergeNzb";
static const char* OPTION_STRICTPARNAME			= "StrictParName";
static const char* OPTION_RELOADURLQUEUE		= "ReloadUrlQueue";
static const char* OPTION_RELOADPOSTQUEUE		= "ReloadPostQueue";
static const char* OPTION_NZBPROCESS			= "NZBProcess";
static const char* OPTION_NZBADDEDPROCESS		= "NZBAddedProcess";
static const char* OPTION_CREATELOG				= "CreateLog";
static const char* OPTION_RESETLOG				= "ResetLog";

const char* BoolNames[] = { "yes", "no", "true", "false", "1", "0", "on", "off", "enable", "disable", "enabled", "disabled" };
const int BoolValues[] = { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };
const int BoolCount = 12;

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

Options* g_pOptions = NULL;

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
	free(m_szName);
	free(m_szValue);
	free(m_szDefValue);
}

void Options::OptEntry::SetName(const char* szName)
{
	free(m_szName);
	m_szName = strdup(szName);
}

void Options::OptEntry::SetValue(const char* szValue)
{
	free(m_szValue);
	m_szValue = strdup(szValue);

	if (!m_szDefValue)
	{
		m_szDefValue = strdup(szValue);
	}
}

bool Options::OptEntry::Restricted()
{
	char szLoName[256];
	strncpy(szLoName, m_szName, sizeof(szLoName));
	szLoName[256-1] = '\0';
	for (char* p = szLoName; *p; p++) *p = tolower(*p); // convert string to lowercase

	bool bRestricted = !strcasecmp(m_szName, OPTION_CONTROLIP) ||
		!strcasecmp(m_szName, OPTION_CONTROLPORT) ||
		!strcasecmp(m_szName, OPTION_SECURECONTROL) ||
		!strcasecmp(m_szName, OPTION_SECUREPORT) ||
		!strcasecmp(m_szName, OPTION_SECURECERT) ||
		!strcasecmp(m_szName, OPTION_SECUREKEY) ||
		!strcasecmp(m_szName, OPTION_AUTHORIZEDIP) ||
		!strcasecmp(m_szName, OPTION_DAEMONUSERNAME) ||
		!strcasecmp(m_szName, OPTION_UMASK) ||
		strchr(m_szName, ':') ||			// All extension script options
		strstr(szLoName, "username") ||		// ServerX.Username, ControlUsername, etc.
		strstr(szLoName, "password");		// ServerX.Password, ControlPassword, etc.

	return bRestricted;
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


Options::Category::Category(const char* szName, const char* szDestDir, bool bUnpack, const char* szPostScript)
{
	m_szName = strdup(szName);
	m_szDestDir = szDestDir ? strdup(szDestDir) : NULL;
	m_bUnpack = bUnpack;
	m_szPostScript = szPostScript ? strdup(szPostScript) : NULL;
}

Options::Category::~Category()
{
	free(m_szName);
	free(m_szDestDir);
	free(m_szPostScript);

	for (NameList::iterator it = m_Aliases.begin(); it != m_Aliases.end(); it++)
	{
		free(*it);
	}
}

Options::Categories::~Categories()
{
	for (iterator it = begin(); it != end(); it++)
	{
		delete *it;
	}
}

Options::Category* Options::Categories::FindCategory(const char* szName, bool bSearchAliases)
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

	if (bSearchAliases)
	{
		for (iterator it = begin(); it != end(); it++)
		{
			Category* pCategory = *it;
			for (NameList::iterator it2 = pCategory->GetAliases()->begin(); it2 != pCategory->GetAliases()->end(); it2++)
			{
				const char* szAlias = *it2;
				WildMask mask(szAlias);
				if (mask.Match(szName))
				{
					return pCategory;
				}
			}
		}
	}

	return NULL;
}


Options::Options(const char* szExeName, const char* szConfigFilename, bool bNoConfig,
	CmdOptList* pCommandLineOptions, Extender* pExtender)
{
	Init(szExeName, szConfigFilename, bNoConfig, pCommandLineOptions, false, pExtender);
}

Options::Options(CmdOptList* pCommandLineOptions, Extender* pExtender)
{
	Init("nzbget/nzbget", NULL, true, pCommandLineOptions, true, pExtender);
}

void Options::Init(const char* szExeName, const char* szConfigFilename, bool bNoConfig,
	CmdOptList* pCommandLineOptions, bool bNoDiskAccess, Extender* pExtender)
{
	g_pOptions = this;
	m_pExtender = pExtender;
	m_bNoDiskAccess = bNoDiskAccess;
	m_bNoConfig = bNoConfig;
	m_bConfigErrors = false;
	m_iConfigLine = 0;
	m_bServerMode = false;
	m_bRemoteClientMode = false;
	m_bFatalError = false;

	// initialize options with default values
	m_szConfigFilename		= NULL;
	m_szAppDir				= NULL;
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
	m_bPausePostProcess		= false;
	m_bPauseScan			= false;
	m_bTempPauseDownload	= false;
	m_bBrokenLog			= false;
	m_bNzbLog				= false;
	m_iDownloadRate			= 0;
	m_iArticleTimeout		= 0;
	m_iUrlTimeout			= 0;
	m_iTerminateTimeout		= 0;
	m_bAppendCategoryDir	= false;
	m_bContinuePartial		= false;
	m_bSaveQueue			= false;
	m_bDupeCheck			= false;
	m_iRetries				= 0;
	m_iRetryInterval		= 0;
	m_iControlPort			= 0;
	m_szControlIP			= NULL;
	m_szControlUsername		= NULL;
	m_szControlPassword		= NULL;
	m_szRestrictedUsername	= NULL;
	m_szRestrictedPassword	= NULL;
	m_szAddUsername			= NULL;
	m_szAddPassword			= NULL;
	m_bSecureControl		= false;
	m_iSecurePort			= 0;
	m_szSecureCert			= NULL;
	m_szSecureKey			= NULL;
	m_szAuthorizedIP		= NULL;
	m_szLockFile			= NULL;
	m_szDaemonUsername		= NULL;
	m_eOutputMode			= omLoggable;
	m_bReloadQueue			= false;
	m_iUrlConnections		= 0;
	m_iLogBufferSize		= 0;
	m_eWriteLog				= wlAppend;
	m_iRotateLog			= 0;
	m_szLogFile				= NULL;
	m_eParCheck				= pcManual;
	m_bParRepair			= false;
	m_eParScan				= psLimited;
	m_bParQuick				= true;
	m_bParRename			= false;
	m_iParBuffer			= 0;
	m_iParThreads			= 0;
	m_eHealthCheck			= hcNone;
	m_szScriptOrder			= NULL;
	m_szPostScript			= NULL;
	m_szScanScript			= NULL;
	m_szQueueScript			= NULL;
	m_iUMask				= 0;
	m_iUpdateInterval		= 0;
	m_bCursesNZBName		= false;
	m_bCursesTime			= false;
	m_bCursesGroup			= false;
	m_bCrcCheck				= false;
	m_bDirectWrite			= false;
	m_iWriteBuffer			= 0;
	m_iNzbDirInterval		= 0;
	m_iNzbDirFileAge		= 0;
	m_bParCleanupQueue		= false;
	m_iDiskSpace			= 0;
	m_bTLS					= false;
	m_bDumpCore				= false;
	m_bParPauseQueue		= false;
	m_bScriptPauseQueue		= false;
	m_bNzbCleanupDisk		= false;
	m_bDeleteCleanupDisk	= false;
	m_iParTimeLimit			= 0;
	m_iKeepHistory			= 0;
	m_bAccurateRate			= false;
	m_tResumeTime			= 0;
	m_bUnpack				= false;
	m_bUnpackCleanupDisk	= false;
	m_szUnrarCmd			= NULL;
	m_szSevenZipCmd			= NULL;
	m_szUnpackPassFile		= NULL;
	m_bUnpackPauseQueue		= false;
	m_szExtCleanupDisk		= NULL;
	m_szParIgnoreExt		= NULL;
	m_iFeedHistory			= 0;
	m_bUrlForce				= false;
	m_iTimeCorrection		= 0;
	m_iLocalTimeOffset		= 0;
	m_iPropagationDelay		= 0;
	m_iArticleCache			= 0;
	m_iEventInterval		= 0;

	m_bNoDiskAccess = bNoDiskAccess;

	m_szConfigFilename = szConfigFilename ? strdup(szConfigFilename) : NULL;
	SetOption(OPTION_CONFIGFILE, "");

	char szFilename[MAX_PATH + 1];
	if (m_bNoDiskAccess)
	{
		strncpy(szFilename, szExeName, sizeof(szFilename));
		szFilename[sizeof(szFilename)-1] = '\0';
	}
	else
	{
		Util::GetExeFileName(szExeName, szFilename, sizeof(szFilename));
	}
	Util::NormalizePathSeparators(szFilename);
	SetOption(OPTION_APPBIN, szFilename);
	char* end = strrchr(szFilename, PATH_SEPARATOR);
	if (end) *end = '\0';
	SetOption(OPTION_APPDIR, szFilename);
	m_szAppDir = strdup(szFilename);

	SetOption(OPTION_VERSION, Util::VersionRevision());

	InitDefaults();

	InitOptFile();
	if (m_bFatalError)
	{
		return;
	}

	if (pCommandLineOptions)
	{
		InitCommandLineOptions(pCommandLineOptions);
	}

	if (!m_szConfigFilename && !bNoConfig)
	{
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
		m_bFatalError = true;
		return;
	}

	InitOptions();
	CheckOptions();

	InitServers();
	InitCategories();
	InitScheduler();
	InitFeeds();
}

Options::~Options()
{
	g_pOptions = NULL;
	free(m_szConfigFilename);
	free(m_szAppDir);
	free(m_szDestDir);
	free(m_szInterDir);
	free(m_szTempDir);
	free(m_szQueueDir);
	free(m_szNzbDir);
	free(m_szWebDir);
	free(m_szConfigTemplate);
	free(m_szScriptDir);
	free(m_szControlIP);
	free(m_szControlUsername);
	free(m_szControlPassword);
	free(m_szRestrictedUsername);
	free(m_szRestrictedPassword);
	free(m_szAddUsername);
	free(m_szAddPassword);
	free(m_szSecureCert);
	free(m_szSecureKey);
	free(m_szAuthorizedIP);
	free(m_szLogFile);
	free(m_szLockFile);
	free(m_szDaemonUsername);
	free(m_szScriptOrder);
	free(m_szPostScript);
	free(m_szScanScript);
	free(m_szQueueScript);
	free(m_szUnrarCmd);
	free(m_szSevenZipCmd);
	free(m_szUnpackPassFile);
	free(m_szExtCleanupDisk);
	free(m_szParIgnoreExt);
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

	printf("%s(%i): %s\n", m_szConfigFilename ? Util::BaseFileName(m_szConfigFilename) : "<noconfig>", m_iConfigLine, tmp2);
	error("%s(%i): %s", m_szConfigFilename ? Util::BaseFileName(m_szConfigFilename) : "<noconfig>", m_iConfigLine, tmp2);

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

void Options::InitDefaults()
{
#ifdef WIN32
	SetOption(OPTION_MAINDIR, "${AppDir}");
	SetOption(OPTION_WEBDIR, "${AppDir}\\webui");
	SetOption(OPTION_CONFIGTEMPLATE, "${AppDir}\\nzbget.conf.template");
#else
	SetOption(OPTION_MAINDIR, "~/downloads");
	SetOption(OPTION_WEBDIR, "");
	SetOption(OPTION_CONFIGTEMPLATE, "");
#endif
	SetOption(OPTION_TEMPDIR, "${MainDir}/tmp");
	SetOption(OPTION_DESTDIR, "${MainDir}/dst");
	SetOption(OPTION_INTERDIR, "");
	SetOption(OPTION_QUEUEDIR, "${MainDir}/queue");
	SetOption(OPTION_NZBDIR, "${MainDir}/nzb");
	SetOption(OPTION_LOCKFILE, "${MainDir}/nzbget.lock");
	SetOption(OPTION_LOGFILE, "${DestDir}/nzbget.log");
	SetOption(OPTION_SCRIPTDIR, "${MainDir}/scripts");
	SetOption(OPTION_WRITELOG, "append");
	SetOption(OPTION_ROTATELOG, "3");
	SetOption(OPTION_APPENDCATEGORYDIR, "yes");
	SetOption(OPTION_OUTPUTMODE, "curses");
	SetOption(OPTION_DUPECHECK, "yes");
	SetOption(OPTION_DOWNLOADRATE, "0");
	SetOption(OPTION_CONTROLIP, "0.0.0.0");
	SetOption(OPTION_CONTROLUSERNAME, "nzbget");
	SetOption(OPTION_CONTROLPASSWORD, "tegbzn6789");
	SetOption(OPTION_RESTRICTEDUSERNAME, "");
	SetOption(OPTION_RESTRICTEDPASSWORD, "");
	SetOption(OPTION_ADDUSERNAME, "");
	SetOption(OPTION_ADDPASSWORD, "");
	SetOption(OPTION_CONTROLPORT, "6789");
	SetOption(OPTION_SECURECONTROL, "no");
	SetOption(OPTION_SECUREPORT, "6791");
	SetOption(OPTION_SECURECERT, "");
	SetOption(OPTION_SECUREKEY, "");
	SetOption(OPTION_AUTHORIZEDIP, "");
	SetOption(OPTION_ARTICLETIMEOUT, "60");
	SetOption(OPTION_URLTIMEOUT, "60");
	SetOption(OPTION_SAVEQUEUE, "yes");
	SetOption(OPTION_RELOADQUEUE, "yes");
	SetOption(OPTION_BROKENLOG, "yes");
	SetOption(OPTION_NZBLOG, "yes");
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
	SetOption(OPTION_PARQUICK, "yes");
	SetOption(OPTION_PARRENAME, "yes");
	SetOption(OPTION_PARBUFFER, "16");
	SetOption(OPTION_PARTHREADS, "1");
	SetOption(OPTION_HEALTHCHECK, "none");
	SetOption(OPTION_SCRIPTORDER, "");
	SetOption(OPTION_POSTSCRIPT, "");
	SetOption(OPTION_SCANSCRIPT, "");
	SetOption(OPTION_QUEUESCRIPT, "");
	SetOption(OPTION_DAEMONUSERNAME, "root");
	SetOption(OPTION_UMASK, "1000");
	SetOption(OPTION_UPDATEINTERVAL, "200");
	SetOption(OPTION_CURSESNZBNAME, "yes");
	SetOption(OPTION_CURSESTIME, "no");
	SetOption(OPTION_CURSESGROUP, "no");
	SetOption(OPTION_CRCCHECK, "yes");
	SetOption(OPTION_DIRECTWRITE, "yes");
	SetOption(OPTION_WRITEBUFFER, "0");
	SetOption(OPTION_NZBDIRINTERVAL, "5");
	SetOption(OPTION_NZBDIRFILEAGE, "60");
	SetOption(OPTION_PARCLEANUPQUEUE, "yes");
	SetOption(OPTION_DISKSPACE, "250");
	SetOption(OPTION_DUMPCORE, "no");
	SetOption(OPTION_PARPAUSEQUEUE, "no");
	SetOption(OPTION_SCRIPTPAUSEQUEUE, "no");
	SetOption(OPTION_NZBCLEANUPDISK, "no");
	SetOption(OPTION_DELETECLEANUPDISK, "no");
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
	SetOption(OPTION_UNPACKPASSFILE, "");
	SetOption(OPTION_UNPACKPAUSEQUEUE, "no");
	SetOption(OPTION_EXTCLEANUPDISK, "");
	SetOption(OPTION_PARIGNOREEXT, "");
	SetOption(OPTION_FEEDHISTORY, "7");
	SetOption(OPTION_URLFORCE, "yes");
	SetOption(OPTION_TIMECORRECTION, "0");
	SetOption(OPTION_PROPAGATIONDELAY, "0");
	SetOption(OPTION_ARTICLECACHE, "0");
	SetOption(OPTION_EVENTINTERVAL, "0");
}

void Options::InitOptFile()
{
	if (!m_szConfigFilename && !m_bNoConfig)
	{
		// search for config file in default locations
#ifdef WIN32
		char szFilename[MAX_PATH + 20];
		snprintf(szFilename, sizeof(szFilename), "%s\\nzbget.conf", m_szAppDir);

		if (!Util::FileExists(szFilename))
		{
			char szAppDataPath[MAX_PATH];
			SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szAppDataPath);
			snprintf(szFilename, sizeof(szFilename), "%s\\NZBGet\\nzbget.conf", szAppDataPath);
			szFilename[sizeof(szFilename)-1] = '\0';

			if (m_pExtender && !Util::FileExists(szFilename))
			{
				m_pExtender->SetupFirstStart();
			}
		}

		if (Util::FileExists(szFilename))
		{
			m_szConfigFilename = strdup(szFilename);
		}
#else
		// look in the exe-directory first
		char szFilename[1024];
		snprintf(szFilename, sizeof(szFilename), "%s/nzbget.conf", m_szAppDir);
		szFilename[1024-1] = '\0';

		if (Util::FileExists(szFilename))
		{
			m_szConfigFilename = strdup(szFilename);
		}
		else
		{
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
}

void Options::CheckDir(char** dir, const char* szOptionName,
	const char* szParentDir, bool bAllowEmpty, bool bCreate)
{
	char* usedir = NULL;
	const char* tempdir = GetOption(szOptionName);

	if (m_bNoDiskAccess)
	{
		*dir = strdup(tempdir);
		return;
	}

	if (Util::EmptyStr(tempdir))
	{
		if (!bAllowEmpty)
		{
			ConfigError("Invalid value for option \"%s\": <empty>", szOptionName);
		}
		*dir = strdup("");
		return;
	}

	int len = strlen(tempdir);
	usedir = (char*)malloc(len + 2);
	strcpy(usedir, tempdir);
	char ch = usedir[len-1];
	Util::NormalizePathSeparators(usedir);
	if (ch != PATH_SEPARATOR)
	{
		usedir[len] = PATH_SEPARATOR;
		usedir[len + 1] = '\0';
	}

	if (!(usedir[0] == PATH_SEPARATOR || usedir[0] == ALT_PATH_SEPARATOR ||
		(usedir[0] && usedir[1] == ':')) &&
		!Util::EmptyStr(szParentDir))
	{
		// convert relative path to absolute path
		int plen = strlen(szParentDir);
		int len2 = len + plen + 4;
		char* usedir2 = (char*)malloc(len2);
		if (szParentDir[plen-1] == PATH_SEPARATOR || szParentDir[plen-1] == ALT_PATH_SEPARATOR)
		{
			snprintf(usedir2, len2, "%s%s", szParentDir, usedir);
		}
		else
		{
			snprintf(usedir2, len2, "%s%c%s", szParentDir, PATH_SEPARATOR, usedir);
		}
		usedir2[len2-1] = '\0';
		free(usedir);

		usedir = usedir2;
		Util::NormalizePathSeparators(usedir);

		int ulen = strlen(usedir);
		usedir[ulen-1] = '\0';
		SetOption(szOptionName, usedir);
		usedir[ulen-1] = PATH_SEPARATOR;
	}

	// Ensure the dir is created
	char szErrBuf[1024];
	if (bCreate && !Util::ForceDirectories(usedir, szErrBuf, sizeof(szErrBuf)))
	{
		ConfigError("Invalid value for option \"%s\" (%s): %s", szOptionName, usedir, szErrBuf);
	}
	*dir = usedir;
}

void Options::InitOptions()
{
	const char* szMainDir = GetOption(OPTION_MAINDIR);

	CheckDir(&m_szDestDir, OPTION_DESTDIR, szMainDir, false, false);
	CheckDir(&m_szInterDir, OPTION_INTERDIR, szMainDir, true, true);
	CheckDir(&m_szTempDir, OPTION_TEMPDIR, szMainDir, false, true);
	CheckDir(&m_szQueueDir, OPTION_QUEUEDIR, szMainDir, false, true);
	CheckDir(&m_szWebDir, OPTION_WEBDIR, NULL, true, false);
	CheckDir(&m_szScriptDir, OPTION_SCRIPTDIR, szMainDir, true, false);

	m_szConfigTemplate		= strdup(GetOption(OPTION_CONFIGTEMPLATE));
	m_szScriptOrder			= strdup(GetOption(OPTION_SCRIPTORDER));
	m_szPostScript			= strdup(GetOption(OPTION_POSTSCRIPT));
	m_szScanScript			= strdup(GetOption(OPTION_SCANSCRIPT));
	m_szQueueScript			= strdup(GetOption(OPTION_QUEUESCRIPT));
	m_szControlIP			= strdup(GetOption(OPTION_CONTROLIP));
	m_szControlUsername		= strdup(GetOption(OPTION_CONTROLUSERNAME));
	m_szControlPassword		= strdup(GetOption(OPTION_CONTROLPASSWORD));
	m_szRestrictedUsername	= strdup(GetOption(OPTION_RESTRICTEDUSERNAME));
	m_szRestrictedPassword	= strdup(GetOption(OPTION_RESTRICTEDPASSWORD));
	m_szAddUsername			= strdup(GetOption(OPTION_ADDUSERNAME));
	m_szAddPassword			= strdup(GetOption(OPTION_ADDPASSWORD));
	m_szSecureCert			= strdup(GetOption(OPTION_SECURECERT));
	m_szSecureKey			= strdup(GetOption(OPTION_SECUREKEY));
	m_szAuthorizedIP		= strdup(GetOption(OPTION_AUTHORIZEDIP));
	m_szLockFile			= strdup(GetOption(OPTION_LOCKFILE));
	m_szDaemonUsername		= strdup(GetOption(OPTION_DAEMONUSERNAME));
	m_szLogFile				= strdup(GetOption(OPTION_LOGFILE));
	m_szUnrarCmd			= strdup(GetOption(OPTION_UNRARCMD));
	m_szSevenZipCmd			= strdup(GetOption(OPTION_SEVENZIPCMD));
	m_szUnpackPassFile		= strdup(GetOption(OPTION_UNPACKPASSFILE));
	m_szExtCleanupDisk		= strdup(GetOption(OPTION_EXTCLEANUPDISK));
	m_szParIgnoreExt		= strdup(GetOption(OPTION_PARIGNOREEXT));

	m_iDownloadRate			= (int)(ParseFloatValue(OPTION_DOWNLOADRATE) * 1024);
	m_iArticleTimeout		= ParseIntValue(OPTION_ARTICLETIMEOUT, 10);
	m_iUrlTimeout			= ParseIntValue(OPTION_URLTIMEOUT, 10);
	m_iTerminateTimeout		= ParseIntValue(OPTION_TERMINATETIMEOUT, 10);
	m_iRetries				= ParseIntValue(OPTION_RETRIES, 10);
	m_iRetryInterval		= ParseIntValue(OPTION_RETRYINTERVAL, 10);
	m_iControlPort			= ParseIntValue(OPTION_CONTROLPORT, 10);
	m_iSecurePort			= ParseIntValue(OPTION_SECUREPORT, 10);
	m_iUrlConnections		= ParseIntValue(OPTION_URLCONNECTIONS, 10);
	m_iLogBufferSize		= ParseIntValue(OPTION_LOGBUFFERSIZE, 10);
	m_iRotateLog			= ParseIntValue(OPTION_ROTATELOG, 10);
	m_iUMask				= ParseIntValue(OPTION_UMASK, 8);
	m_iUpdateInterval		= ParseIntValue(OPTION_UPDATEINTERVAL, 10);
	m_iWriteBuffer			= ParseIntValue(OPTION_WRITEBUFFER, 10);
	m_iNzbDirInterval		= ParseIntValue(OPTION_NZBDIRINTERVAL, 10);
	m_iNzbDirFileAge		= ParseIntValue(OPTION_NZBDIRFILEAGE, 10);
	m_iDiskSpace			= ParseIntValue(OPTION_DISKSPACE, 10);
	m_iParTimeLimit			= ParseIntValue(OPTION_PARTIMELIMIT, 10);
	m_iKeepHistory			= ParseIntValue(OPTION_KEEPHISTORY, 10);
	m_iFeedHistory			= ParseIntValue(OPTION_FEEDHISTORY, 10);
	m_iTimeCorrection		= ParseIntValue(OPTION_TIMECORRECTION, 10);
	if (-24 <= m_iTimeCorrection && m_iTimeCorrection <= 24)
	{
		m_iTimeCorrection *= 60;
	}
	m_iTimeCorrection *= 60;
	m_iPropagationDelay		= ParseIntValue(OPTION_PROPAGATIONDELAY, 10) * 60;
	m_iArticleCache			= ParseIntValue(OPTION_ARTICLECACHE, 10);
	m_iEventInterval		= ParseIntValue(OPTION_EVENTINTERVAL, 10);
	m_iParBuffer			= ParseIntValue(OPTION_PARBUFFER, 10);
	m_iParThreads			= ParseIntValue(OPTION_PARTHREADS, 10);

	CheckDir(&m_szNzbDir, OPTION_NZBDIR, szMainDir, m_iNzbDirInterval == 0, true);

	m_bBrokenLog			= (bool)ParseEnumValue(OPTION_BROKENLOG, BoolCount, BoolNames, BoolValues);
	m_bNzbLog				= (bool)ParseEnumValue(OPTION_NZBLOG, BoolCount, BoolNames, BoolValues);
	m_bAppendCategoryDir	= (bool)ParseEnumValue(OPTION_APPENDCATEGORYDIR, BoolCount, BoolNames, BoolValues);
	m_bContinuePartial		= (bool)ParseEnumValue(OPTION_CONTINUEPARTIAL, BoolCount, BoolNames, BoolValues);
	m_bSaveQueue			= (bool)ParseEnumValue(OPTION_SAVEQUEUE, BoolCount, BoolNames, BoolValues);
	m_bDupeCheck			= (bool)ParseEnumValue(OPTION_DUPECHECK, BoolCount, BoolNames, BoolValues);
	m_bParRepair			= (bool)ParseEnumValue(OPTION_PARREPAIR, BoolCount, BoolNames, BoolValues);
	m_bParQuick				= (bool)ParseEnumValue(OPTION_PARQUICK, BoolCount, BoolNames, BoolValues);
	m_bParRename			= (bool)ParseEnumValue(OPTION_PARRENAME, BoolCount, BoolNames, BoolValues);
	m_bReloadQueue			= (bool)ParseEnumValue(OPTION_RELOADQUEUE, BoolCount, BoolNames, BoolValues);
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
	m_bAccurateRate			= (bool)ParseEnumValue(OPTION_ACCURATERATE, BoolCount, BoolNames, BoolValues);
	m_bSecureControl		= (bool)ParseEnumValue(OPTION_SECURECONTROL, BoolCount, BoolNames, BoolValues);
	m_bUnpack				= (bool)ParseEnumValue(OPTION_UNPACK, BoolCount, BoolNames, BoolValues);
	m_bUnpackCleanupDisk	= (bool)ParseEnumValue(OPTION_UNPACKCLEANUPDISK, BoolCount, BoolNames, BoolValues);
	m_bUnpackPauseQueue		= (bool)ParseEnumValue(OPTION_UNPACKPAUSEQUEUE, BoolCount, BoolNames, BoolValues);
	m_bUrlForce				= (bool)ParseEnumValue(OPTION_URLFORCE, BoolCount, BoolNames, BoolValues);

	const char* OutputModeNames[] = { "loggable", "logable", "log", "colored", "color", "ncurses", "curses" };
	const int OutputModeValues[] = { omLoggable, omLoggable, omLoggable, omColored, omColored, omNCurses, omNCurses };
	const int OutputModeCount = 7;
	m_eOutputMode = (EOutputMode)ParseEnumValue(OPTION_OUTPUTMODE, OutputModeCount, OutputModeNames, OutputModeValues);

	const char* ParCheckNames[] = { "auto", "always", "force", "manual", "yes", "no" }; // yes/no for compatibility with older versions
	const int ParCheckValues[] = { pcAuto, pcAlways, pcForce, pcManual, pcAlways, pcAuto };
	const int ParCheckCount = 6;
	m_eParCheck = (EParCheck)ParseEnumValue(OPTION_PARCHECK, ParCheckCount, ParCheckNames, ParCheckValues);

	const char* ParScanNames[] = { "limited", "full", "auto" };
	const int ParScanValues[] = { psLimited, psFull, psAuto };
	const int ParScanCount = 3;
	m_eParScan = (EParScan)ParseEnumValue(OPTION_PARSCAN, ParScanCount, ParScanNames, ParScanValues);

	const char* HealthCheckNames[] = { "pause", "delete", "none" };
	const int HealthCheckValues[] = { hcPause, hcDelete, hcNone };
	const int HealthCheckCount = 3;
	m_eHealthCheck = (EHealthCheck)ParseEnumValue(OPTION_HEALTHCHECK, HealthCheckCount, HealthCheckNames, HealthCheckValues);

	const char* TargetNames[] = { "screen", "log", "both", "none" };
	const int TargetValues[] = { mtScreen, mtLog, mtBoth, mtNone };
	const int TargetCount = 4;
	m_eInfoTarget = (EMessageTarget)ParseEnumValue(OPTION_INFOTARGET, TargetCount, TargetNames, TargetValues);
	m_eWarningTarget = (EMessageTarget)ParseEnumValue(OPTION_WARNINGTARGET, TargetCount, TargetNames, TargetValues);
	m_eErrorTarget = (EMessageTarget)ParseEnumValue(OPTION_ERRORTARGET, TargetCount, TargetNames, TargetValues);
	m_eDebugTarget = (EMessageTarget)ParseEnumValue(OPTION_DEBUGTARGET, TargetCount, TargetNames, TargetValues);
	m_eDetailTarget = (EMessageTarget)ParseEnumValue(OPTION_DETAILTARGET, TargetCount, TargetNames, TargetValues);

	const char* WriteLogNames[] = { "none", "append", "reset", "rotate" };
	const int WriteLogValues[] = { wlNone, wlAppend, wlReset, wlRotate };
	const int WriteLogCount = 4;
	m_eWriteLog = (EWriteLog)ParseEnumValue(OPTION_WRITELOG, WriteLogCount, WriteLogNames, WriteLogValues);
}

int Options::ParseEnumValue(const char* OptName, int argc, const char * argn[], const int argv[])
{
	OptEntry* pOptEntry = FindOption(OptName);
	if (!pOptEntry)
	{
		ConfigError("Undefined value for option \"%s\"", OptName);
		return argv[0];
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
		ConfigError("Undefined value for option \"%s\"", OptName);
		return 0;
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
		ConfigError("Undefined value for option \"%s\"\n", OptName);
		return 0;
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
		if (m_bNoDiskAccess)
		{
			strncpy(szExpandedPath, value, 1024);
			szExpandedPath[1024-1] = '\0';
		}
		else if (!Util::ExpandHomePath(value, szExpandedPath, sizeof(szExpandedPath)))
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
				break;
			}
		}
		else
		{
			break;
		}
	}

	pOptEntry->SetValue(curvalue);

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

		sprintf(optname, "Server%i.Active", n);
		const char* nactive = GetOption(optname);
		bool bActive = true;
		if (nactive)
		{
			bActive = (bool)ParseEnumValue(optname, BoolCount, BoolNames, BoolValues);
		}

		sprintf(optname, "Server%i.Name", n);
		const char* nname = GetOption(optname);

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

		sprintf(optname, "Server%i.Retention", n);
		const char* nretention = GetOption(optname);

		bool definition = nactive || nname || nlevel || ngroup || nhost || nport ||
			nusername || npassword || nconnections || njoingroup || ntls || ncipher || nretention;
		bool completed = nhost && nport && nconnections;

		if (!definition)
		{
			break;
		}

		if (completed)
		{
			if (m_pExtender)
			{
				m_pExtender->AddNewsServer(n, bActive, nname,
					nhost,
					nport ? atoi(nport) : 119,
					nusername, npassword,
					bJoinGroup, bTLS, ncipher,
					nconnections ? atoi(nconnections) : 1,
					nretention ? atoi(nretention) : 0,
					nlevel ? atoi(nlevel) : 0,
					ngroup ? atoi(ngroup) : 0);
			}
		}
		else
		{
			ConfigError("Server definition not complete for \"Server%i\"", n);
		}

		n++;
	}
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

		sprintf(optname, "Category%i.Unpack", n);
		const char* nunpack = GetOption(optname);
		bool bUnpack = true;
		if (nunpack)
		{
			bUnpack = (bool)ParseEnumValue(optname, BoolCount, BoolNames, BoolValues);
		}

		sprintf(optname, "Category%i.PostScript", n);
		const char* npostscript = GetOption(optname);

		sprintf(optname, "Category%i.Aliases", n);
		const char* naliases = GetOption(optname);

		bool definition = nname || ndestdir || nunpack || npostscript || naliases;
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
				CheckDir(&szDestDir, destdiroptname, m_szDestDir, false, false);
			}

			Category* pCategory = new Category(nname, szDestDir, bUnpack, npostscript);
			m_Categories.push_back(pCategory);

			free(szDestDir);
			
			// split Aliases into tokens and create items for each token
			if (naliases)
			{
				Tokenizer tok(naliases, ",;");
				while (const char* szAliasName = tok.Next())
				{
					pCategory->GetAliases()->push_back(strdup(szAliasName));
				}
			}
		}
		else
		{
			ConfigError("Category definition not complete for \"Category%i\"", n);
		}

		n++;
	}
}

void Options::InitFeeds()
{
	int n = 1;
	while (true)
	{
		char optname[128];

		sprintf(optname, "Feed%i.Name", n);
		const char* nname = GetOption(optname);

		sprintf(optname, "Feed%i.URL", n);
		const char* nurl = GetOption(optname);
		
		sprintf(optname, "Feed%i.Filter", n);
		const char* nfilter = GetOption(optname);

		sprintf(optname, "Feed%i.Category", n);
		const char* ncategory = GetOption(optname);

		sprintf(optname, "Feed%i.PauseNzb", n);
		const char* npausenzb = GetOption(optname);
		bool bPauseNzb = false;
		if (npausenzb)
		{
			bPauseNzb = (bool)ParseEnumValue(optname, BoolCount, BoolNames, BoolValues);
		}

		sprintf(optname, "Feed%i.Interval", n);
		const char* ninterval = GetOption(optname);

		sprintf(optname, "Feed%i.Priority", n);
		const char* npriority = GetOption(optname);

		bool definition = nname || nurl || nfilter || ncategory || npausenzb || ninterval || npriority;
		bool completed = nurl;

		if (!definition)
		{
			break;
		}

		if (completed)
		{
			if (m_pExtender)
			{
				m_pExtender->AddFeed(n, nname, nurl, ninterval ? atoi(ninterval) : 0, nfilter,
					bPauseNzb, ncategory, npriority ? atoi(npriority) : 0);
			}
		}
		else
		{
			ConfigError("Feed definition not complete for \"Feed%i\"", n);
		}

		n++;
	}
}

void Options::InitScheduler()
{
	for (int n = 1; ; n++)
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

		sprintf(optname, "Task%i.Param", n);
		const char* szParam = GetOption(optname);

		if (Util::EmptyStr(szParam) && !Util::EmptyStr(szProcess))
		{
			szParam = szProcess;
		}
		if (Util::EmptyStr(szParam) && !Util::EmptyStr(szDownloadRate))
		{
			szParam = szDownloadRate;
		}

		bool definition = szTime || szWeekDays || szCommand || szDownloadRate || szParam;
		bool completed = szTime && szCommand;

		if (!definition)
		{
			break;
		}

		if (!completed)
		{
			ConfigError("Task definition not complete for \"Task%i\"", n);
			continue;
		}

		snprintf(optname, sizeof(optname), "Task%i.Command", n);
		optname[sizeof(optname)-1] = '\0';

		const char* CommandNames[] = { "pausedownload", "pause", "unpausedownload", "resumedownload", "unpause", "resume",
			"pausepostprocess", "unpausepostprocess", "resumepostprocess", "pausepost", "unpausepost", "resumepost",
			"downloadrate", "setdownloadrate", "rate", "speed", "script", "process", "pausescan", "unpausescan", "resumescan",
			"activateserver", "activateservers", "deactivateserver", "deactivateservers", "fetchfeed", "fetchfeeds" };
		const int CommandValues[] = { scPauseDownload, scPauseDownload, scUnpauseDownload,
			scUnpauseDownload, scUnpauseDownload, scUnpauseDownload,
			scPausePostProcess, scUnpausePostProcess, scUnpausePostProcess,
			scPausePostProcess, scUnpausePostProcess, scUnpausePostProcess,
			scDownloadRate, scDownloadRate, scDownloadRate, scDownloadRate,
			scScript, scProcess, scPauseScan, scUnpauseScan, scUnpauseScan,
			scActivateServer, scActivateServer, scDeactivateServer,
			scDeactivateServer, scFetchFeed, scFetchFeed };
		const int CommandCount = 27;
		ESchedulerCommand eCommand = (ESchedulerCommand)ParseEnumValue(optname, CommandCount, CommandNames, CommandValues);

		if (szParam && strlen(szParam) > 0 && eCommand == scProcess &&
			!Util::SplitCommandLine(szParam, NULL))
		{
			ConfigError("Invalid value for option \"Task%i.Param\"", n);
			continue;
		}

		int iWeekDays = 0;
		if (szWeekDays && !ParseWeekDays(szWeekDays, &iWeekDays))
		{
			ConfigError("Invalid value for option \"Task%i.WeekDays\": \"%s\"", n, szWeekDays);
			continue;
		}

		if (eCommand == scDownloadRate)
		{
			if (szParam)
			{
				char* szErr;
				int iDownloadRate = strtol(szParam, &szErr, 10);
				if (!szErr || *szErr != '\0' || iDownloadRate < 0)
				{
					ConfigError("Invalid value for option \"Task%i.Param\": \"%s\"", n, szDownloadRate);
					continue;
				}
			}
			else
			{
				ConfigError("Task definition not complete for \"Task%i\". Option \"Task%i.Param\" is missing", n, n);
				continue;
			}
		}

		if ((eCommand == scScript || 
			 eCommand == scProcess || 
			 eCommand == scActivateServer ||
			 eCommand == scDeactivateServer ||
			 eCommand == scFetchFeed) && 
			Util::EmptyStr(szParam))
		{
			ConfigError("Task definition not complete for \"Task%i\". Option \"Task%i.Param\" is missing", n, n);
			continue;
		}

		int iHours, iMinutes;
		Tokenizer tok(szTime, ";,");
		while (const char* szOneTime = tok.Next())
		{
			if (!ParseTime(szOneTime, &iHours, &iMinutes))
			{
				ConfigError("Invalid value for option \"Task%i.Time\": \"%s\"", n, szOneTime);
				break;
			}

			if (m_pExtender)
			{
				if (iHours == -1)
				{
					for (int iEveryHour = 0; iEveryHour < 24; iEveryHour++)
					{
						m_pExtender->AddTask(n, iEveryHour, iMinutes, iWeekDays, eCommand, szParam);
					}
				}
				else
				{
					m_pExtender->AddTask(n, iHours, iMinutes, iWeekDays, eCommand, szParam);
				}
			}
		}
	}
}

bool Options::ParseTime(const char* szTime, int* pHours, int* pMinutes)
{
	int iColons = 0;
	const char* p = szTime;
	while (*p)
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
	SetOption(OPTION_CONFIGFILE, m_szConfigFilename);

	FILE* infile = fopen(m_szConfigFilename, FOPEN_RB);

	if (!infile)
	{
		ConfigError("Could not open file %s", m_szConfigFilename);
		m_bFatalError = true;
		return;
	}

	m_iConfigLine = 0;
	int iBufLen = (int)Util::FileSize(m_szConfigFilename) + 1;
	char* buf = (char*)malloc(iBufLen);

	int iLine = 0;
	while (fgets(buf, iBufLen - 1, infile))
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
	free(buf);

	m_iConfigLine = 0;
}

void Options::InitCommandLineOptions(CmdOptList* pCommandLineOptions)
{
	for (CmdOptList::iterator it = pCommandLineOptions->begin(); it != pCommandLineOptions->end(); it++)
	{
		const char* option = *it;
		SetOptionString(option);
	}
}

bool Options::SetOptionString(const char* option)
{
	char* optname;
	char* optvalue;

	if (!SplitOptionString(option, &optname, &optvalue))
	{
		ConfigError("Invalid option \"%s\"", option);
		return false;
	}

	bool bOK = ValidateOptionName(optname, optvalue);
	if (bOK)
	{
		SetOption(optname, optvalue);
	}
	else
	{
		ConfigError("Invalid option \"%s\"", optname);
	}

	free(optname);
	free(optvalue);

	return bOK;
}

/*
 * Splits option string into name and value;
 * Converts old names and values if necessary;
 * Allocates buffers for name and value;
 * Returns true if the option string has name and value;
 * If "true" is returned the caller is responsible for freeing optname and optvalue.
 */
bool Options::SplitOptionString(const char* option, char** pOptName, char** pOptValue)
{
	const char* eq = strchr(option, '=');
	if (!eq)
	{
		return false;
	}

	const char* value = eq + 1;

	char optname[1001];
	char optvalue[1001];
	int maxlen = (int)(eq - option < 1000 ? eq - option : 1000);
	strncpy(optname, option, maxlen);
	optname[maxlen] = '\0';
	strncpy(optvalue, eq + 1, 1000);
	optvalue[1000]  = '\0';
	if (strlen(optname) == 0)
	{
		return false;
	}

	ConvertOldOption(optname, sizeof(optname), optvalue, sizeof(optvalue));

	// if value was (old-)converted use the new value, which is linited to 1000 characters,
	// otherwise use original (length-unlimited) value
	if (strncmp(value, optvalue, 1000))
	{
		value = optvalue;
	}

	*pOptName = strdup(optname);
	*pOptValue = strdup(value);

	return true;
}

bool Options::ValidateOptionName(const char* optname, const char* optvalue)
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
			(!strcasecmp(p, ".active") || !strcasecmp(p, ".name") ||
			!strcasecmp(p, ".level") || !strcasecmp(p, ".host") ||
			!strcasecmp(p, ".port") || !strcasecmp(p, ".username") ||
			!strcasecmp(p, ".password") || !strcasecmp(p, ".joingroup") ||
			!strcasecmp(p, ".encryption") || !strcasecmp(p, ".connections") ||
			!strcasecmp(p, ".cipher") || !strcasecmp(p, ".group") ||
			!strcasecmp(p, ".retention")))
		{
			return true;
		}
	}

	if (!strncasecmp(optname, "task", 4))
	{
		char* p = (char*)optname + 4;
		while (*p >= '0' && *p <= '9') p++;
		if (p && (!strcasecmp(p, ".time") || !strcasecmp(p, ".weekdays") ||
			!strcasecmp(p, ".command") || !strcasecmp(p, ".param") ||
			!strcasecmp(p, ".downloadrate") || !strcasecmp(p, ".process")))
		{
			return true;
		}
	}
	
	if (!strncasecmp(optname, "category", 8))
	{
		char* p = (char*)optname + 8;
		while (*p >= '0' && *p <= '9') p++;
		if (p && (!strcasecmp(p, ".name") || !strcasecmp(p, ".destdir") || !strcasecmp(p, ".postscript") ||
			!strcasecmp(p, ".unpack") || !strcasecmp(p, ".aliases")))
		{
			return true;
		}
	}

	if (!strncasecmp(optname, "feed", 4))
	{
		char* p = (char*)optname + 4;
		while (*p >= '0' && *p <= '9') p++;
		if (p && (!strcasecmp(p, ".name") || !strcasecmp(p, ".url") || !strcasecmp(p, ".interval") ||
			 !strcasecmp(p, ".filter") || !strcasecmp(p, ".pausenzb") || !strcasecmp(p, ".category") ||
			 !strcasecmp(p, ".priority")))
		{
			return true;
		}
	}

	// scripts options
	if (strchr(optname, ':'))
	{
		return true;
	}

	// print warning messages for obsolete options
	if (!strcasecmp(optname, OPTION_RETRYONCRCERROR) ||
		!strcasecmp(optname, OPTION_ALLOWREPROCESS) ||
		!strcasecmp(optname, OPTION_LOADPARS) ||
		!strcasecmp(optname, OPTION_THREADLIMIT) ||
		!strcasecmp(optname, OPTION_POSTLOGKIND) ||
		!strcasecmp(optname, OPTION_NZBLOGKIND) ||
		!strcasecmp(optname, OPTION_PROCESSLOGKIND) ||
		!strcasecmp(optname, OPTION_APPENDNZBDIR) ||
		!strcasecmp(optname, OPTION_RENAMEBROKEN) ||
		!strcasecmp(optname, OPTION_MERGENZB) ||
		!strcasecmp(optname, OPTION_STRICTPARNAME) ||
		!strcasecmp(optname, OPTION_RELOADURLQUEUE) ||
		!strcasecmp(optname, OPTION_RELOADPOSTQUEUE))
	{
		ConfigWarn("Option \"%s\" is obsolete, ignored", optname);
		return true;
	}
	if (!strcasecmp(optname, OPTION_POSTPROCESS) ||
		!strcasecmp(optname, OPTION_NZBPROCESS) ||
		!strcasecmp(optname, OPTION_NZBADDEDPROCESS))
	{
		if (optvalue && strlen(optvalue) > 0)
		{
			ConfigError("Option \"%s\" is obsolete, ignored, use \"%s\" and \"%s\" instead", optname, OPTION_SCRIPTDIR,
				!strcasecmp(optname, OPTION_POSTPROCESS) ? OPTION_POSTSCRIPT :
				!strcasecmp(optname, OPTION_NZBPROCESS) ? OPTION_SCANSCRIPT :
				!strcasecmp(optname, OPTION_NZBADDEDPROCESS) ? OPTION_QUEUESCRIPT :
				"ERROR");
		}
		return true;
	}

	if (!strcasecmp(optname, OPTION_CREATELOG) || !strcasecmp(optname, OPTION_RESETLOG))
	{
		ConfigWarn("Option \"%s\" is obsolete, ignored, use \"%s\" instead", optname, OPTION_WRITELOG);
		return true;
	}

	return false;
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
		strncpy(szValue, "always", iValueBufLen);
	}

	if (!strcasecmp(szOption, "ParCheck") && !strcasecmp(szValue, "no"))
	{
		strncpy(szValue, "auto", iValueBufLen);
	}

	if (!strcasecmp(szOption, "DefScript"))
	{
		strncpy(szOption, "PostScript", iOptionBufLen);
	}

	int iNameLen = strlen(szOption);
	if (!strncasecmp(szOption, "Category", 8) && iNameLen > 10 &&
		!strcasecmp(szOption + iNameLen - 10, ".DefScript"))
	{
		strncpy(szOption + iNameLen - 10, ".PostScript", iOptionBufLen - 9 /* strlen("Category.") */);
	}

	if (!strcasecmp(szOption, "WriteBufferSize"))
	{
		strncpy(szOption, "WriteBuffer", iOptionBufLen);
		int val = strtol(szValue, NULL, 10);
		val = val == -1 ? 1024 : val / 1024;
		snprintf(szValue, iValueBufLen, "%i", val);
	}	

	if (!strcasecmp(szOption, "ConnectionTimeout"))
	{
		strncpy(szOption, "ArticleTimeout", iOptionBufLen);
	}

	if (!strcasecmp(szOption, "CreateBrokenLog"))
	{
		strncpy(szOption, "BrokenLog", iOptionBufLen);
	}

	szOption[iOptionBufLen-1] = '\0';
	szOption[iValueBufLen-1] = '\0';
}

void Options::CheckOptions()
{
#ifdef DISABLE_PARCHECK
	if (m_eParCheck != pcManual)
	{
		LocateOptionSrcPos(OPTION_PARCHECK);
		ConfigError("Invalid value for option \"%s\": program was compiled without parcheck-support", OPTION_PARCHECK);
	}
	if (m_bParRename)
	{
		LocateOptionSrcPos(OPTION_PARRENAME);
		ConfigError("Invalid value for option \"%s\": program was compiled without parcheck-support", OPTION_PARRENAME);
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
	if (Util::EmptyStr(m_szConfigTemplate) && !m_bNoDiskAccess)
	{
		free(m_szConfigTemplate);
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

	if (m_iArticleCache < 0)
	{
		m_iArticleCache = 0;
	}
	else if (sizeof(void*) == 4 && m_iArticleCache > 1900)
	{
		ConfigError("Invalid value for option \"ArticleCache\": %i. Changed to 1900", m_iArticleCache);
		m_iArticleCache = 1900;
	}
	else if (sizeof(void*) == 4 && m_iParBuffer > 1900)
	{
		ConfigError("Invalid value for option \"ParBuffer\": %i. Changed to 1900", m_iParBuffer);
		m_iParBuffer = 1900;
	}

	if (sizeof(void*) == 4 && m_iParBuffer + m_iArticleCache > 1900)
	{
		ConfigError("Options \"ArticleCache\" and \"ParBuffer\" in total cannot use more than 1900MB of memory in 32-Bit mode. Changed to 1500 and 400");
		m_iArticleCache = 1900;
		m_iParBuffer = 400;
	}

	if (!Util::EmptyStr(m_szUnpackPassFile) && !Util::FileExists(m_szUnpackPassFile))
	{
		ConfigError("Invalid value for option \"UnpackPassFile\": %s. File not found", m_szUnpackPassFile);
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
