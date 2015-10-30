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
#include <stdarg.h>
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
static const char* OPTION_REQUIREDDIR			= "RequiredDir";
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
static const char* OPTION_FLUSHQUEUE			= "FlushQueue";
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
static const char* OPTION_FEEDSCRIPT			= "FeedScript";
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
	m_name = NULL;
	m_value = NULL;
	m_defValue = NULL;
	m_lineNo = 0;
}

Options::OptEntry::OptEntry(const char* name, const char* value)
{
	m_name = strdup(name);
	m_value = strdup(value);
	m_defValue = NULL;
	m_lineNo = 0;
}

Options::OptEntry::~OptEntry()
{
	free(m_name);
	free(m_value);
	free(m_defValue);
}

void Options::OptEntry::SetName(const char* name)
{
	free(m_name);
	m_name = strdup(name);
}

void Options::OptEntry::SetValue(const char* value)
{
	free(m_value);
	m_value = strdup(value);

	if (!m_defValue)
	{
		m_defValue = strdup(value);
	}
}

bool Options::OptEntry::Restricted()
{
	char loName[256];
	strncpy(loName, m_name, sizeof(loName));
	loName[256-1] = '\0';
	for (char* p = loName; *p; p++) *p = tolower(*p); // convert string to lowercase

	bool restricted = !strcasecmp(m_name, OPTION_CONTROLIP) ||
		!strcasecmp(m_name, OPTION_CONTROLPORT) ||
		!strcasecmp(m_name, OPTION_SECURECONTROL) ||
		!strcasecmp(m_name, OPTION_SECUREPORT) ||
		!strcasecmp(m_name, OPTION_SECURECERT) ||
		!strcasecmp(m_name, OPTION_SECUREKEY) ||
		!strcasecmp(m_name, OPTION_AUTHORIZEDIP) ||
		!strcasecmp(m_name, OPTION_DAEMONUSERNAME) ||
		!strcasecmp(m_name, OPTION_UMASK) ||
		strchr(m_name, ':') ||			// All extension script options
		strstr(loName, "username") ||		// ServerX.Username, ControlUsername, etc.
		strstr(loName, "password");		// ServerX.Password, ControlPassword, etc.

	return restricted;
}

Options::OptEntries::~OptEntries()
{
	for (iterator it = begin(); it != end(); it++)
	{
		delete *it;
	}
}

Options::OptEntry* Options::OptEntries::FindOption(const char* name)
{
	if (!name)
	{
		return NULL;
	}

	for (iterator it = begin(); it != end(); it++)
	{
		OptEntry* optEntry = *it;
		if (!strcasecmp(optEntry->GetName(), name))
		{
			return optEntry;
		}
	}

	return NULL;
}


Options::Category::Category(const char* name, const char* destDir, bool unpack, const char* postScript)
{
	m_name = strdup(name);
	m_destDir = destDir ? strdup(destDir) : NULL;
	m_unpack = unpack;
	m_postScript = postScript ? strdup(postScript) : NULL;
}

Options::Category::~Category()
{
	free(m_name);
	free(m_destDir);
	free(m_postScript);

	for (NameList::iterator it = m_aliases.begin(); it != m_aliases.end(); it++)
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

Options::Category* Options::Categories::FindCategory(const char* name, bool searchAliases)
{
	if (!name)
	{
		return NULL;
	}

	for (iterator it = begin(); it != end(); it++)
	{
		Category* category = *it;
		if (!strcasecmp(category->GetName(), name))
		{
			return category;
		}
	}

	if (searchAliases)
	{
		for (iterator it = begin(); it != end(); it++)
		{
			Category* category = *it;
			for (NameList::iterator it2 = category->GetAliases()->begin(); it2 != category->GetAliases()->end(); it2++)
			{
				const char* alias = *it2;
				WildMask mask(alias);
				if (mask.Match(name))
				{
					return category;
				}
			}
		}
	}

	return NULL;
}


Options::Options(const char* exeName, const char* configFilename, bool noConfig,
	CmdOptList* commandLineOptions, Extender* extender)
{
	Init(exeName, configFilename, noConfig, commandLineOptions, false, extender);
}

Options::Options(CmdOptList* commandLineOptions, Extender* extender)
{
	Init("nzbget/nzbget", NULL, true, commandLineOptions, true, extender);
}

void Options::Init(const char* exeName, const char* configFilename, bool noConfig,
	CmdOptList* commandLineOptions, bool noDiskAccess, Extender* extender)
{
	g_pOptions = this;
	m_extender = extender;
	m_noDiskAccess = noDiskAccess;
	m_noConfig = noConfig;
	m_configErrors = false;
	m_configLine = 0;
	m_serverMode = false;
	m_remoteClientMode = false;
	m_fatalError = false;

	// initialize options with default values
	m_configFilename		= NULL;
	m_appDir				= NULL;
	m_destDir				= NULL;
	m_interDir			= NULL;
	m_tempDir				= NULL;
	m_queueDir			= NULL;
	m_nzbDir				= NULL;
	m_webDir				= NULL;
	m_configTemplate		= NULL;
	m_scriptDir			= NULL;
	m_requiredDir			= NULL;
	m_infoTarget			= mtScreen;
	m_warningTarget		= mtScreen;
	m_errorTarget			= mtScreen;
	m_debugTarget			= mtScreen;
	m_detailTarget			= mtScreen;
	m_decode				= true;
	m_pauseDownload		= false;
	m_pausePostProcess		= false;
	m_pauseScan			= false;
	m_tempPauseDownload	= true;
	m_tempPausePostprocess	= true;
	m_brokenLog			= false;
	m_nzbLog				= false;
	m_downloadRate			= 0;
	m_articleTimeout		= 0;
	m_urlTimeout			= 0;
	m_terminateTimeout		= 0;
	m_appendCategoryDir	= false;
	m_continuePartial		= false;
	m_saveQueue			= false;
	m_flushQueue			= false;
	m_dupeCheck			= false;
	m_retries				= 0;
	m_retryInterval		= 0;
	m_controlPort			= 0;
	m_controlIp			= NULL;
	m_controlUsername		= NULL;
	m_controlPassword		= NULL;
	m_restrictedUsername	= NULL;
	m_restrictedPassword	= NULL;
	m_addUsername			= NULL;
	m_addPassword			= NULL;
	m_secureControl		= false;
	m_securePort			= 0;
	m_secureCert			= NULL;
	m_secureKey			= NULL;
	m_authorizedIp		= NULL;
	m_lockFile			= NULL;
	m_daemonUsername		= NULL;
	m_outputMode			= omLoggable;
	m_reloadQueue			= false;
	m_urlConnections		= 0;
	m_logBufferSize		= 0;
	m_writeLog				= wlAppend;
	m_rotateLog			= 0;
	m_logFile				= NULL;
	m_parCheck				= pcManual;
	m_parRepair			= false;
	m_parScan				= psLimited;
	m_parQuick				= true;
	m_parRename			= false;
	m_parBuffer			= 0;
	m_parThreads			= 0;
	m_healthCheck			= hcNone;
	m_scriptOrder			= NULL;
	m_postScript			= NULL;
	m_scanScript			= NULL;
	m_queueScript			= NULL;
	m_feedScript			= NULL;
	m_uMask				= 0;
	m_updateInterval		= 0;
	m_cursesNzbName		= false;
	m_cursesTime			= false;
	m_cursesGroup			= false;
	m_crcCheck				= false;
	m_directWrite			= false;
	m_writeBuffer			= 0;
	m_nzbDirInterval		= 0;
	m_nzbDirFileAge		= 0;
	m_parCleanupQueue		= false;
	m_diskSpace			= 0;
	m_tls					= false;
	m_dumpCore				= false;
	m_parPauseQueue		= false;
	m_scriptPauseQueue		= false;
	m_nzbCleanupDisk		= false;
	m_deleteCleanupDisk	= false;
	m_parTimeLimit			= 0;
	m_keepHistory			= 0;
	m_accurateRate			= false;
	m_resumeTime			= 0;
	m_unpack				= false;
	m_unpackCleanupDisk	= false;
	m_unrarCmd			= NULL;
	m_sevenZipCmd			= NULL;
	m_unpackPassFile		= NULL;
	m_unpackPauseQueue		= false;
	m_extCleanupDisk		= NULL;
	m_parIgnoreExt		= NULL;
	m_feedHistory			= 0;
	m_urlForce				= false;
	m_timeCorrection		= 0;
	m_localTimeOffset		= 0;
	m_propagationDelay		= 0;
	m_articleCache			= 0;
	m_eventInterval		= 0;

	m_noDiskAccess = noDiskAccess;

	m_configFilename = configFilename ? strdup(configFilename) : NULL;
	SetOption(OPTION_CONFIGFILE, "");

	char filename[MAX_PATH + 1];
	if (m_noDiskAccess)
	{
		strncpy(filename, exeName, sizeof(filename));
		filename[sizeof(filename)-1] = '\0';
	}
	else
	{
		Util::GetExeFileName(exeName, filename, sizeof(filename));
	}
	Util::NormalizePathSeparators(filename);
	SetOption(OPTION_APPBIN, filename);
	char* end = strrchr(filename, PATH_SEPARATOR);
	if (end) *end = '\0';
	SetOption(OPTION_APPDIR, filename);
	m_appDir = strdup(filename);

	SetOption(OPTION_VERSION, Util::VersionRevision());

	InitDefaults();

	InitOptFile();
	if (m_fatalError)
	{
		return;
	}

	if (commandLineOptions)
	{
		InitCommandLineOptions(commandLineOptions);
	}

	if (!m_configFilename && !noConfig)
	{
		printf("No configuration-file found\n");
#ifdef WIN32
		printf("Please put configuration-file \"nzbget.conf\" into the directory with exe-file\n");
#else
		printf("Please use option \"-c\" or put configuration-file in one of the following locations:\n");
		int p = 0;
		while (const char* filename = PossibleConfigLocations[p++])
		{
			printf("%s\n", filename);
		}
#endif
		m_fatalError = true;
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
	free(m_configFilename);
	free(m_appDir);
	free(m_destDir);
	free(m_interDir);
	free(m_tempDir);
	free(m_queueDir);
	free(m_nzbDir);
	free(m_webDir);
	free(m_configTemplate);
	free(m_scriptDir);
	free(m_requiredDir);
	free(m_controlIp);
	free(m_controlUsername);
	free(m_controlPassword);
	free(m_restrictedUsername);
	free(m_restrictedPassword);
	free(m_addUsername);
	free(m_addPassword);
	free(m_secureCert);
	free(m_secureKey);
	free(m_authorizedIp);
	free(m_logFile);
	free(m_lockFile);
	free(m_daemonUsername);
	free(m_scriptOrder);
	free(m_postScript);
	free(m_scanScript);
	free(m_queueScript);
	free(m_feedScript);
	free(m_unrarCmd);
	free(m_sevenZipCmd);
	free(m_unpackPassFile);
	free(m_extCleanupDisk);
	free(m_parIgnoreExt);
}

void Options::Dump()
{
	for (OptEntries::iterator it = m_optEntries.begin(); it != m_optEntries.end(); it++)
	{
		OptEntry* optEntry = *it;
		printf("%s = \"%s\"\n", optEntry->GetName(), optEntry->GetValue());
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

	printf("%s(%i): %s\n", m_configFilename ? Util::BaseFileName(m_configFilename) : "<noconfig>", m_configLine, tmp2);
	error("%s(%i): %s", m_configFilename ? Util::BaseFileName(m_configFilename) : "<noconfig>", m_configLine, tmp2);

	m_configErrors = true;
}

void Options::ConfigWarn(const char* msg, ...)
{
	char tmp2[1024];
	
	va_list ap;
	va_start(ap, msg);
	vsnprintf(tmp2, 1024, msg, ap);
	tmp2[1024-1] = '\0';
	va_end(ap);
	
	printf("%s(%i): %s\n", Util::BaseFileName(m_configFilename), m_configLine, tmp2);
	warn("%s(%i): %s", Util::BaseFileName(m_configFilename), m_configLine, tmp2);
}

void Options::LocateOptionSrcPos(const char *optionName)
{
	OptEntry* optEntry = FindOption(optionName);
	if (optEntry)
	{
		m_configLine = optEntry->GetLineNo();
	}
	else
	{
		m_configLine = 0;
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
	SetOption(OPTION_REQUIREDDIR, "");
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
	SetOption(OPTION_FLUSHQUEUE, "yes");
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
	SetOption(OPTION_PARSCAN, "extended");
	SetOption(OPTION_PARQUICK, "yes");
	SetOption(OPTION_PARRENAME, "yes");
	SetOption(OPTION_PARBUFFER, "16");
	SetOption(OPTION_PARTHREADS, "1");
	SetOption(OPTION_HEALTHCHECK, "none");
	SetOption(OPTION_SCRIPTORDER, "");
	SetOption(OPTION_POSTSCRIPT, "");
	SetOption(OPTION_SCANSCRIPT, "");
	SetOption(OPTION_QUEUESCRIPT, "");
	SetOption(OPTION_FEEDSCRIPT, "");
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
	if (!m_configFilename && !m_noConfig)
	{
		// search for config file in default locations
#ifdef WIN32
		char filename[MAX_PATH + 20];
		snprintf(filename, sizeof(filename), "%s\\nzbget.conf", m_appDir);

		if (!Util::FileExists(filename))
		{
			char appDataPath[MAX_PATH];
			SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, appDataPath);
			snprintf(filename, sizeof(filename), "%s\\NZBGet\\nzbget.conf", appDataPath);
			filename[sizeof(filename)-1] = '\0';

			if (m_extender && !Util::FileExists(filename))
			{
				m_extender->SetupFirstStart();
			}
		}

		if (Util::FileExists(filename))
		{
			m_configFilename = strdup(filename);
		}
#else
		// look in the exe-directory first
		char filename[1024];
		snprintf(filename, sizeof(filename), "%s/nzbget.conf", m_appDir);
		filename[1024-1] = '\0';

		if (Util::FileExists(filename))
		{
			m_configFilename = strdup(filename);
		}
		else
		{
			int p = 0;
			while (const char* filename = PossibleConfigLocations[p++])
			{
				// substitute HOME-variable
				char expandedFilename[1024];
				if (Util::ExpandHomePath(filename, expandedFilename, sizeof(expandedFilename)))
				{
					filename = expandedFilename;
				}

				if (Util::FileExists(filename))
				{
					m_configFilename = strdup(filename);
					break;
				}
			}
		}
#endif
	}

	if (m_configFilename)
	{
		// normalize path in filename
		char filename[MAX_PATH + 1];
		Util::ExpandFileName(m_configFilename, filename, sizeof(filename));
		filename[MAX_PATH] = '\0';

#ifndef WIN32
		// substitute HOME-variable
		char expandedFilename[1024];
		if (Util::ExpandHomePath(filename, expandedFilename, sizeof(expandedFilename)))
		{
			strncpy(filename, expandedFilename, sizeof(filename));
		}
#endif

		free(m_configFilename);
		m_configFilename = strdup(filename);

		SetOption(OPTION_CONFIGFILE, m_configFilename);
		LoadConfigFile();
	}
}

void Options::CheckDir(char** dir, const char* optionName,
	const char* parentDir, bool allowEmpty, bool create)
{
	char* usedir = NULL;
	const char* tempdir = GetOption(optionName);

	if (m_noDiskAccess)
	{
		*dir = strdup(tempdir);
		return;
	}

	if (Util::EmptyStr(tempdir))
	{
		if (!allowEmpty)
		{
			ConfigError("Invalid value for option \"%s\": <empty>", optionName);
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
		!Util::EmptyStr(parentDir))
	{
		// convert relative path to absolute path
		int plen = strlen(parentDir);
		int len2 = len + plen + 4;
		char* usedir2 = (char*)malloc(len2);
		if (parentDir[plen-1] == PATH_SEPARATOR || parentDir[plen-1] == ALT_PATH_SEPARATOR)
		{
			snprintf(usedir2, len2, "%s%s", parentDir, usedir);
		}
		else
		{
			snprintf(usedir2, len2, "%s%c%s", parentDir, PATH_SEPARATOR, usedir);
		}
		usedir2[len2-1] = '\0';
		free(usedir);

		usedir = usedir2;
		Util::NormalizePathSeparators(usedir);

		int ulen = strlen(usedir);
		usedir[ulen-1] = '\0';
		SetOption(optionName, usedir);
		usedir[ulen-1] = PATH_SEPARATOR;
	}

	// Ensure the dir is created
	char errBuf[1024];
	if (create && !Util::ForceDirectories(usedir, errBuf, sizeof(errBuf)))
	{
		ConfigError("Invalid value for option \"%s\" (%s): %s", optionName, usedir, errBuf);
	}
	*dir = usedir;
}

void Options::InitOptions()
{
	const char* mainDir = GetOption(OPTION_MAINDIR);

	CheckDir(&m_destDir, OPTION_DESTDIR, mainDir, false, false);
	CheckDir(&m_interDir, OPTION_INTERDIR, mainDir, true, false);
	CheckDir(&m_tempDir, OPTION_TEMPDIR, mainDir, false, true);
	CheckDir(&m_queueDir, OPTION_QUEUEDIR, mainDir, false, true);
	CheckDir(&m_webDir, OPTION_WEBDIR, NULL, true, false);
	CheckDir(&m_scriptDir, OPTION_SCRIPTDIR, mainDir, true, false);
	CheckDir(&m_nzbDir, OPTION_NZBDIR, mainDir, false, true);

	m_requiredDir = strdup(GetOption(OPTION_REQUIREDDIR));

	m_configTemplate		= strdup(GetOption(OPTION_CONFIGTEMPLATE));
	m_scriptOrder			= strdup(GetOption(OPTION_SCRIPTORDER));
	m_postScript			= strdup(GetOption(OPTION_POSTSCRIPT));
	m_scanScript			= strdup(GetOption(OPTION_SCANSCRIPT));
	m_queueScript			= strdup(GetOption(OPTION_QUEUESCRIPT));
	m_feedScript			= strdup(GetOption(OPTION_FEEDSCRIPT));
	m_controlIp			= strdup(GetOption(OPTION_CONTROLIP));
	m_controlUsername		= strdup(GetOption(OPTION_CONTROLUSERNAME));
	m_controlPassword		= strdup(GetOption(OPTION_CONTROLPASSWORD));
	m_restrictedUsername	= strdup(GetOption(OPTION_RESTRICTEDUSERNAME));
	m_restrictedPassword	= strdup(GetOption(OPTION_RESTRICTEDPASSWORD));
	m_addUsername			= strdup(GetOption(OPTION_ADDUSERNAME));
	m_addPassword			= strdup(GetOption(OPTION_ADDPASSWORD));
	m_secureCert			= strdup(GetOption(OPTION_SECURECERT));
	m_secureKey			= strdup(GetOption(OPTION_SECUREKEY));
	m_authorizedIp		= strdup(GetOption(OPTION_AUTHORIZEDIP));
	m_lockFile			= strdup(GetOption(OPTION_LOCKFILE));
	m_daemonUsername		= strdup(GetOption(OPTION_DAEMONUSERNAME));
	m_logFile				= strdup(GetOption(OPTION_LOGFILE));
	m_unrarCmd			= strdup(GetOption(OPTION_UNRARCMD));
	m_sevenZipCmd			= strdup(GetOption(OPTION_SEVENZIPCMD));
	m_unpackPassFile		= strdup(GetOption(OPTION_UNPACKPASSFILE));
	m_extCleanupDisk		= strdup(GetOption(OPTION_EXTCLEANUPDISK));
	m_parIgnoreExt		= strdup(GetOption(OPTION_PARIGNOREEXT));

	m_downloadRate			= ParseIntValue(OPTION_DOWNLOADRATE, 10) * 1024;
	m_articleTimeout		= ParseIntValue(OPTION_ARTICLETIMEOUT, 10);
	m_urlTimeout			= ParseIntValue(OPTION_URLTIMEOUT, 10);
	m_terminateTimeout		= ParseIntValue(OPTION_TERMINATETIMEOUT, 10);
	m_retries				= ParseIntValue(OPTION_RETRIES, 10);
	m_retryInterval		= ParseIntValue(OPTION_RETRYINTERVAL, 10);
	m_controlPort			= ParseIntValue(OPTION_CONTROLPORT, 10);
	m_securePort			= ParseIntValue(OPTION_SECUREPORT, 10);
	m_urlConnections		= ParseIntValue(OPTION_URLCONNECTIONS, 10);
	m_logBufferSize		= ParseIntValue(OPTION_LOGBUFFERSIZE, 10);
	m_rotateLog			= ParseIntValue(OPTION_ROTATELOG, 10);
	m_uMask				= ParseIntValue(OPTION_UMASK, 8);
	m_updateInterval		= ParseIntValue(OPTION_UPDATEINTERVAL, 10);
	m_writeBuffer			= ParseIntValue(OPTION_WRITEBUFFER, 10);
	m_nzbDirInterval		= ParseIntValue(OPTION_NZBDIRINTERVAL, 10);
	m_nzbDirFileAge		= ParseIntValue(OPTION_NZBDIRFILEAGE, 10);
	m_diskSpace			= ParseIntValue(OPTION_DISKSPACE, 10);
	m_parTimeLimit			= ParseIntValue(OPTION_PARTIMELIMIT, 10);
	m_keepHistory			= ParseIntValue(OPTION_KEEPHISTORY, 10);
	m_feedHistory			= ParseIntValue(OPTION_FEEDHISTORY, 10);
	m_timeCorrection		= ParseIntValue(OPTION_TIMECORRECTION, 10);
	if (-24 <= m_timeCorrection && m_timeCorrection <= 24)
	{
		m_timeCorrection *= 60;
	}
	m_timeCorrection *= 60;
	m_propagationDelay		= ParseIntValue(OPTION_PROPAGATIONDELAY, 10) * 60;
	m_articleCache			= ParseIntValue(OPTION_ARTICLECACHE, 10);
	m_eventInterval		= ParseIntValue(OPTION_EVENTINTERVAL, 10);
	m_parBuffer			= ParseIntValue(OPTION_PARBUFFER, 10);
	m_parThreads			= ParseIntValue(OPTION_PARTHREADS, 10);

	m_brokenLog			= (bool)ParseEnumValue(OPTION_BROKENLOG, BoolCount, BoolNames, BoolValues);
	m_nzbLog				= (bool)ParseEnumValue(OPTION_NZBLOG, BoolCount, BoolNames, BoolValues);
	m_appendCategoryDir	= (bool)ParseEnumValue(OPTION_APPENDCATEGORYDIR, BoolCount, BoolNames, BoolValues);
	m_continuePartial		= (bool)ParseEnumValue(OPTION_CONTINUEPARTIAL, BoolCount, BoolNames, BoolValues);
	m_saveQueue			= (bool)ParseEnumValue(OPTION_SAVEQUEUE, BoolCount, BoolNames, BoolValues);
	m_flushQueue			= (bool)ParseEnumValue(OPTION_FLUSHQUEUE, BoolCount, BoolNames, BoolValues);
	m_dupeCheck			= (bool)ParseEnumValue(OPTION_DUPECHECK, BoolCount, BoolNames, BoolValues);
	m_parRepair			= (bool)ParseEnumValue(OPTION_PARREPAIR, BoolCount, BoolNames, BoolValues);
	m_parQuick				= (bool)ParseEnumValue(OPTION_PARQUICK, BoolCount, BoolNames, BoolValues);
	m_parRename			= (bool)ParseEnumValue(OPTION_PARRENAME, BoolCount, BoolNames, BoolValues);
	m_reloadQueue			= (bool)ParseEnumValue(OPTION_RELOADQUEUE, BoolCount, BoolNames, BoolValues);
	m_cursesNzbName		= (bool)ParseEnumValue(OPTION_CURSESNZBNAME, BoolCount, BoolNames, BoolValues);
	m_cursesTime			= (bool)ParseEnumValue(OPTION_CURSESTIME, BoolCount, BoolNames, BoolValues);
	m_cursesGroup			= (bool)ParseEnumValue(OPTION_CURSESGROUP, BoolCount, BoolNames, BoolValues);
	m_crcCheck				= (bool)ParseEnumValue(OPTION_CRCCHECK, BoolCount, BoolNames, BoolValues);
	m_directWrite			= (bool)ParseEnumValue(OPTION_DIRECTWRITE, BoolCount, BoolNames, BoolValues);
	m_parCleanupQueue		= (bool)ParseEnumValue(OPTION_PARCLEANUPQUEUE, BoolCount, BoolNames, BoolValues);
	m_decode				= (bool)ParseEnumValue(OPTION_DECODE, BoolCount, BoolNames, BoolValues);
	m_dumpCore				= (bool)ParseEnumValue(OPTION_DUMPCORE, BoolCount, BoolNames, BoolValues);
	m_parPauseQueue		= (bool)ParseEnumValue(OPTION_PARPAUSEQUEUE, BoolCount, BoolNames, BoolValues);
	m_scriptPauseQueue		= (bool)ParseEnumValue(OPTION_SCRIPTPAUSEQUEUE, BoolCount, BoolNames, BoolValues);
	m_nzbCleanupDisk		= (bool)ParseEnumValue(OPTION_NZBCLEANUPDISK, BoolCount, BoolNames, BoolValues);
	m_deleteCleanupDisk	= (bool)ParseEnumValue(OPTION_DELETECLEANUPDISK, BoolCount, BoolNames, BoolValues);
	m_accurateRate			= (bool)ParseEnumValue(OPTION_ACCURATERATE, BoolCount, BoolNames, BoolValues);
	m_secureControl		= (bool)ParseEnumValue(OPTION_SECURECONTROL, BoolCount, BoolNames, BoolValues);
	m_unpack				= (bool)ParseEnumValue(OPTION_UNPACK, BoolCount, BoolNames, BoolValues);
	m_unpackCleanupDisk	= (bool)ParseEnumValue(OPTION_UNPACKCLEANUPDISK, BoolCount, BoolNames, BoolValues);
	m_unpackPauseQueue		= (bool)ParseEnumValue(OPTION_UNPACKPAUSEQUEUE, BoolCount, BoolNames, BoolValues);
	m_urlForce				= (bool)ParseEnumValue(OPTION_URLFORCE, BoolCount, BoolNames, BoolValues);

	const char* OutputModeNames[] = { "loggable", "logable", "log", "colored", "color", "ncurses", "curses" };
	const int OutputModeValues[] = { omLoggable, omLoggable, omLoggable, omColored, omColored, omNCurses, omNCurses };
	const int OutputModeCount = 7;
	m_outputMode = (EOutputMode)ParseEnumValue(OPTION_OUTPUTMODE, OutputModeCount, OutputModeNames, OutputModeValues);

	const char* ParCheckNames[] = { "auto", "always", "force", "manual" };
	const int ParCheckValues[] = { pcAuto, pcAlways, pcForce, pcManual };
	const int ParCheckCount = 6;
	m_parCheck = (EParCheck)ParseEnumValue(OPTION_PARCHECK, ParCheckCount, ParCheckNames, ParCheckValues);

	const char* ParScanNames[] = { "limited", "extended", "full", "dupe" };
	const int ParScanValues[] = { psLimited, psExtended, psFull, psDupe };
	const int ParScanCount = 4;
	m_parScan = (EParScan)ParseEnumValue(OPTION_PARSCAN, ParScanCount, ParScanNames, ParScanValues);

	const char* HealthCheckNames[] = { "pause", "delete", "none" };
	const int HealthCheckValues[] = { hcPause, hcDelete, hcNone };
	const int HealthCheckCount = 3;
	m_healthCheck = (EHealthCheck)ParseEnumValue(OPTION_HEALTHCHECK, HealthCheckCount, HealthCheckNames, HealthCheckValues);

	const char* TargetNames[] = { "screen", "log", "both", "none" };
	const int TargetValues[] = { mtScreen, mtLog, mtBoth, mtNone };
	const int TargetCount = 4;
	m_infoTarget = (EMessageTarget)ParseEnumValue(OPTION_INFOTARGET, TargetCount, TargetNames, TargetValues);
	m_warningTarget = (EMessageTarget)ParseEnumValue(OPTION_WARNINGTARGET, TargetCount, TargetNames, TargetValues);
	m_errorTarget = (EMessageTarget)ParseEnumValue(OPTION_ERRORTARGET, TargetCount, TargetNames, TargetValues);
	m_debugTarget = (EMessageTarget)ParseEnumValue(OPTION_DEBUGTARGET, TargetCount, TargetNames, TargetValues);
	m_detailTarget = (EMessageTarget)ParseEnumValue(OPTION_DETAILTARGET, TargetCount, TargetNames, TargetValues);

	const char* WriteLogNames[] = { "none", "append", "reset", "rotate" };
	const int WriteLogValues[] = { wlNone, wlAppend, wlReset, wlRotate };
	const int WriteLogCount = 4;
	m_writeLog = (EWriteLog)ParseEnumValue(OPTION_WRITELOG, WriteLogCount, WriteLogNames, WriteLogValues);
}

int Options::ParseEnumValue(const char* OptName, int argc, const char * argn[], const int argv[])
{
	OptEntry* optEntry = FindOption(OptName);
	if (!optEntry)
	{
		ConfigError("Undefined value for option \"%s\"", OptName);
		return argv[0];
	}

	int defNum = 0;

	for (int i = 0; i < argc; i++)
	{
		if (!strcasecmp(optEntry->GetValue(), argn[i]))
		{
			// normalizing option value in option list, for example "NO" -> "no"
			for (int j = 0; j < argc; j++)
			{
				if (argv[j] == argv[i])
				{
					if (strcmp(argn[j], optEntry->GetValue()))
					{
						optEntry->SetValue(argn[j]);
					}
					break;
				}
			}

			return argv[i];
		}

		if (!strcasecmp(optEntry->GetDefValue(), argn[i]))
		{
			defNum = i;
		}
	}

	m_configLine = optEntry->GetLineNo();
	ConfigError("Invalid value for option \"%s\": \"%s\"", OptName, optEntry->GetValue());
	optEntry->SetValue(argn[defNum]);
	return argv[defNum];
}

int Options::ParseIntValue(const char* OptName, int base)
{
	OptEntry* optEntry = FindOption(OptName);
	if (!optEntry)
	{
		ConfigError("Undefined value for option \"%s\"", OptName);
		return 0;
	}

	char *endptr;
	int val = strtol(optEntry->GetValue(), &endptr, base);

	if (endptr && *endptr != '\0')
	{
		m_configLine = optEntry->GetLineNo();
		ConfigError("Invalid value for option \"%s\": \"%s\"", OptName, optEntry->GetValue());
		optEntry->SetValue(optEntry->GetDefValue());
		val = strtol(optEntry->GetDefValue(), NULL, base);
	}

	return val;
}

void Options::SetOption(const char* optname, const char* value)
{
	OptEntry* optEntry = FindOption(optname);
	if (!optEntry)
	{
		optEntry = new OptEntry();
		optEntry->SetName(optname);
		m_optEntries.push_back(optEntry);
	}

	char* curvalue = NULL;

#ifndef WIN32
	if (value && (value[0] == '~') && (value[1] == '/'))
	{
		char expandedPath[1024];
		if (m_noDiskAccess)
		{
			strncpy(expandedPath, value, 1024);
			expandedPath[1024-1] = '\0';
		}
		else if (!Util::ExpandHomePath(value, expandedPath, sizeof(expandedPath)))
		{
			ConfigError("Invalid value for option\"%s\": unable to determine home-directory", optname);
			expandedPath[0] = '\0';
		}
		curvalue = strdup(expandedPath);
	}
	else
#endif
	{
		curvalue = strdup(value);
	}

	optEntry->SetLineNo(m_configLine);

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

	optEntry->SetValue(curvalue);

	free(curvalue);
}

Options::OptEntry* Options::FindOption(const char* optname)
{
	OptEntry* optEntry = m_optEntries.FindOption(optname);

	// normalize option name in option list; for example "server1.joingroup" -> "Server1.JoinGroup"
	if (optEntry && strcmp(optEntry->GetName(), optname))
	{
		optEntry->SetName(optname);
	}

	return optEntry;
}

const char* Options::GetOption(const char* optname)
{
	OptEntry* optEntry = FindOption(optname);
	if (optEntry)
	{
		if (optEntry->GetLineNo() > 0)
		{
			m_configLine = optEntry->GetLineNo();
		}
		return optEntry->GetValue();
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
		bool active = true;
		if (nactive)
		{
			active = (bool)ParseEnumValue(optname, BoolCount, BoolNames, BoolValues);
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
		bool joinGroup = false;
		if (njoingroup)
		{
			joinGroup = (bool)ParseEnumValue(optname, BoolCount, BoolNames, BoolValues);
		}

		sprintf(optname, "Server%i.Encryption", n);
		const char* ntls = GetOption(optname);
		bool tls = false;
		if (ntls)
		{
			tls = (bool)ParseEnumValue(optname, BoolCount, BoolNames, BoolValues);
#ifdef DISABLE_TLS
			if (tls)
			{
				ConfigError("Invalid value for option \"%s\": program was compiled without TLS/SSL-support", optname);
				tls = false;
			}
#endif
			m_tls |= tls;
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
			if (m_extender)
			{
				m_extender->AddNewsServer(n, active, nname,
					nhost,
					nport ? atoi(nport) : 119,
					nusername, npassword,
					joinGroup, tls, ncipher,
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
		bool unpack = true;
		if (nunpack)
		{
			unpack = (bool)ParseEnumValue(optname, BoolCount, BoolNames, BoolValues);
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
			char* destDir = NULL;
			if (ndestdir && ndestdir[0] != '\0')
			{
				CheckDir(&destDir, destdiroptname, m_destDir, false, false);
			}

			Category* category = new Category(nname, destDir, unpack, npostscript);
			m_categories.push_back(category);

			free(destDir);
			
			// split Aliases into tokens and create items for each token
			if (naliases)
			{
				Tokenizer tok(naliases, ",;");
				while (const char* aliasName = tok.Next())
				{
					category->GetAliases()->push_back(strdup(aliasName));
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

		sprintf(optname, "Feed%i.FeedScript", n);
		const char* nfeedscript = GetOption(optname);

		sprintf(optname, "Feed%i.Backlog", n);
		const char* nbacklog = GetOption(optname);
		bool backlog = true;
		if (nbacklog)
		{
			backlog = (bool)ParseEnumValue(optname, BoolCount, BoolNames, BoolValues);
		}

		sprintf(optname, "Feed%i.PauseNzb", n);
		const char* npausenzb = GetOption(optname);
		bool pauseNzb = false;
		if (npausenzb)
		{
			pauseNzb = (bool)ParseEnumValue(optname, BoolCount, BoolNames, BoolValues);
		}

		sprintf(optname, "Feed%i.Interval", n);
		const char* ninterval = GetOption(optname);

		sprintf(optname, "Feed%i.Priority", n);
		const char* npriority = GetOption(optname);

		bool definition = nname || nurl || nfilter || ncategory || nbacklog || npausenzb ||
			ninterval || npriority || nfeedscript;
		bool completed = nurl;

		if (!definition)
		{
			break;
		}

		if (completed)
		{
			if (m_extender)
			{
				m_extender->AddFeed(n, nname, nurl, ninterval ? atoi(ninterval) : 0, nfilter,
					backlog, pauseNzb, ncategory, npriority ? atoi(npriority) : 0, nfeedscript);
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
		const char* time = GetOption(optname);

		sprintf(optname, "Task%i.WeekDays", n);
		const char* weekDays = GetOption(optname);

		sprintf(optname, "Task%i.Command", n);
		const char* command = GetOption(optname);

		sprintf(optname, "Task%i.DownloadRate", n);
		const char* downloadRate = GetOption(optname);

		sprintf(optname, "Task%i.Process", n);
		const char* process = GetOption(optname);

		sprintf(optname, "Task%i.Param", n);
		const char* param = GetOption(optname);

		if (Util::EmptyStr(param) && !Util::EmptyStr(process))
		{
			param = process;
		}
		if (Util::EmptyStr(param) && !Util::EmptyStr(downloadRate))
		{
			param = downloadRate;
		}

		bool definition = time || weekDays || command || downloadRate || param;
		bool completed = time && command;

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
		ESchedulerCommand taskCommand = (ESchedulerCommand)ParseEnumValue(optname, CommandCount, CommandNames, CommandValues);

		if (param && strlen(param) > 0 && taskCommand == scProcess &&
			!Util::SplitCommandLine(param, NULL))
		{
			ConfigError("Invalid value for option \"Task%i.Param\"", n);
			continue;
		}

		int weekDaysVal = 0;
		if (weekDays && !ParseWeekDays(weekDays, &weekDaysVal))
		{
			ConfigError("Invalid value for option \"Task%i.WeekDays\": \"%s\"", n, weekDays);
			continue;
		}

		if (taskCommand == scDownloadRate)
		{
			if (param)
			{
				char* err;
				int downloadRateVal = strtol(param, &err, 10);
				if (!err || *err != '\0' || downloadRateVal < 0)
				{
					ConfigError("Invalid value for option \"Task%i.Param\": \"%s\"", n, downloadRate);
					continue;
				}
			}
			else
			{
				ConfigError("Task definition not complete for \"Task%i\". Option \"Task%i.Param\" is missing", n, n);
				continue;
			}
		}

		if ((taskCommand == scScript || 
			 taskCommand == scProcess || 
			 taskCommand == scActivateServer ||
			 taskCommand == scDeactivateServer ||
			 taskCommand == scFetchFeed) && 
			Util::EmptyStr(param))
		{
			ConfigError("Task definition not complete for \"Task%i\". Option \"Task%i.Param\" is missing", n, n);
			continue;
		}

		int hours, minutes;
		Tokenizer tok(time, ";,");
		while (const char* oneTime = tok.Next())
		{
			if (!ParseTime(oneTime, &hours, &minutes))
			{
				ConfigError("Invalid value for option \"Task%i.Time\": \"%s\"", n, oneTime);
				break;
			}

			if (m_extender)
			{
				if (hours == -1)
				{
					for (int everyHour = 0; everyHour < 24; everyHour++)
					{
						m_extender->AddTask(n, everyHour, minutes, weekDaysVal, taskCommand, param);
					}
				}
				else
				{
					m_extender->AddTask(n, hours, minutes, weekDaysVal, taskCommand, param);
				}
			}
		}
	}
}

bool Options::ParseTime(const char* time, int* hours, int* minutes)
{
	int colons = 0;
	const char* p = time;
	while (*p)
	{
		if (!strchr("0123456789: *", *p))
		{
			return false;
		}
		if (*p == ':')
		{
			colons++;
		}
		p++;
	}

	if (colons != 1)
	{
		return false;
	}

	const char* colon = strchr(time, ':');
	if (!colon)
	{
		return false;
	}

	if (time[0] == '*')
	{
		*hours = -1;
	}
	else
	{
		*hours = atoi(time);
		if (*hours < 0 || *hours > 23)
		{
			return false;
		}
	}

	if (colon[1] == '*')
	{
		return false;
	}
	*minutes = atoi(colon + 1);
	if (*minutes < 0 || *minutes > 59)
	{
		return false;
	}

	return true;
}

bool Options::ParseWeekDays(const char* weekDays, int* weekDaysBits)
{
	*weekDaysBits = 0;
	const char* p = weekDays;
	int firstDay = 0;
	bool range = false;
	while (*p)
	{
		if (strchr("1234567", *p))
		{
			int day = *p - '0';
			if (range)
			{
				if (day <= firstDay || firstDay == 0)
				{
					return false;
				}
				for (int i = firstDay; i <= day; i++)
				{
					*weekDaysBits |= 1 << (i - 1);
				}
				firstDay = 0;
			}
			else
			{
				*weekDaysBits |= 1 << (day - 1);
				firstDay = day;
			}
			range = false;
		}
		else if (*p == ',')
		{
			range = false;
		}
		else if (*p == '-')
		{
			range = true;
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
	SetOption(OPTION_CONFIGFILE, m_configFilename);

	FILE* infile = fopen(m_configFilename, FOPEN_RB);

	if (!infile)
	{
		ConfigError("Could not open file %s", m_configFilename);
		m_fatalError = true;
		return;
	}

	m_configLine = 0;
	int bufLen = (int)Util::FileSize(m_configFilename) + 1;
	char* buf = (char*)malloc(bufLen);

	int line = 0;
	while (fgets(buf, bufLen - 1, infile))
	{
		m_configLine = ++line;

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

	m_configLine = 0;
}

void Options::InitCommandLineOptions(CmdOptList* commandLineOptions)
{
	for (CmdOptList::iterator it = commandLineOptions->begin(); it != commandLineOptions->end(); it++)
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

	bool ok = ValidateOptionName(optname, optvalue);
	if (ok)
	{
		SetOption(optname, optvalue);
	}
	else
	{
		ConfigError("Invalid option \"%s\"", optname);
	}

	free(optname);
	free(optvalue);

	return ok;
}

/*
 * Splits option string into name and value;
 * Converts old names and values if necessary;
 * Allocates buffers for name and value;
 * Returns true if the option string has name and value;
 * If "true" is returned the caller is responsible for freeing optname and optvalue.
 */
bool Options::SplitOptionString(const char* option, char** optName, char** optValue)
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

	*optName = strdup(optname);
	*optValue = strdup(value);

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
			 !strcasecmp(p, ".filter") || !strcasecmp(p, ".backlog") || !strcasecmp(p, ".pausenzb") ||
			 !strcasecmp(p, ".category") || !strcasecmp(p, ".priority") || !strcasecmp(p, ".feedscript")))
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

void Options::ConvertOldOption(char *option, int optionBufLen, char *value, int valueBufLen)
{
	// for compatibility with older versions accept old option names

	if (!strcasecmp(option, "$MAINDIR"))
	{
		strncpy(option, "MainDir", optionBufLen);
	}

	if (!strcasecmp(option, "ServerIP"))
	{
		strncpy(option, "ControlIP", optionBufLen);
	}

	if (!strcasecmp(option, "ServerPort"))
	{
		strncpy(option, "ControlPort", optionBufLen);
	}

	if (!strcasecmp(option, "ServerPassword"))
	{
		strncpy(option, "ControlPassword", optionBufLen);
	}

	if (!strcasecmp(option, "PostPauseQueue"))
	{
		strncpy(option, "ScriptPauseQueue", optionBufLen);
	}

	if (!strcasecmp(option, "ParCheck") && !strcasecmp(value, "yes"))
	{
		strncpy(value, "always", valueBufLen);
	}

	if (!strcasecmp(option, "ParCheck") && !strcasecmp(value, "no"))
	{
		strncpy(value, "auto", valueBufLen);
	}

	if (!strcasecmp(option, "ParScan") && !strcasecmp(value, "auto"))
	{
		strncpy(value, "extended", valueBufLen);
	}

	if (!strcasecmp(option, "DefScript"))
	{
		strncpy(option, "PostScript", optionBufLen);
	}

	int nameLen = strlen(option);
	if (!strncasecmp(option, "Category", 8) && nameLen > 10 &&
		!strcasecmp(option + nameLen - 10, ".DefScript"))
	{
		strncpy(option + nameLen - 10, ".PostScript", optionBufLen - 9 /* strlen("Category.") */);
	}

	if (!strcasecmp(option, "WriteBufferSize"))
	{
		strncpy(option, "WriteBuffer", optionBufLen);
		int val = strtol(value, NULL, 10);
		val = val == -1 ? 1024 : val / 1024;
		snprintf(value, valueBufLen, "%i", val);
	}	

	if (!strcasecmp(option, "ConnectionTimeout"))
	{
		strncpy(option, "ArticleTimeout", optionBufLen);
	}

	if (!strcasecmp(option, "CreateBrokenLog"))
	{
		strncpy(option, "BrokenLog", optionBufLen);
	}

	option[optionBufLen-1] = '\0';
	option[valueBufLen-1] = '\0';
}

void Options::CheckOptions()
{
#ifdef DISABLE_PARCHECK
	if (m_parCheck != pcManual)
	{
		LocateOptionSrcPos(OPTION_PARCHECK);
		ConfigError("Invalid value for option \"%s\": program was compiled without parcheck-support", OPTION_PARCHECK);
	}
	if (m_parRename)
	{
		LocateOptionSrcPos(OPTION_PARRENAME);
		ConfigError("Invalid value for option \"%s\": program was compiled without parcheck-support", OPTION_PARRENAME);
	}
#endif

#ifdef DISABLE_CURSES
	if (m_outputMode == omNCurses)
	{
		LocateOptionSrcPos(OPTION_OUTPUTMODE);
		ConfigError("Invalid value for option \"%s\": program was compiled without curses-support", OPTION_OUTPUTMODE);
	}
#endif

#ifdef DISABLE_TLS
	if (m_secureControl)
	{
		LocateOptionSrcPos(OPTION_SECURECONTROL);
		ConfigError("Invalid value for option \"%s\": program was compiled without TLS/SSL-support", OPTION_SECURECONTROL);
	}
#endif

	if (!m_decode)
	{
		m_directWrite = false;
	}

	// if option "ConfigTemplate" is not set, use "WebDir" as default location for template
	// (for compatibility with versions 9 and 10).
	if (Util::EmptyStr(m_configTemplate) && !m_noDiskAccess)
	{
		free(m_configTemplate);
		int len = strlen(m_webDir) + 15;
		m_configTemplate = (char*)malloc(len);
		snprintf(m_configTemplate, len, "%s%s", m_webDir, "nzbget.conf");
		m_configTemplate[len-1] = '\0';
		if (!Util::FileExists(m_configTemplate))
		{
			free(m_configTemplate);
			m_configTemplate = strdup("");
		}
	}

	if (m_articleCache < 0)
	{
		m_articleCache = 0;
	}
	else if (sizeof(void*) == 4 && m_articleCache > 1900)
	{
		ConfigError("Invalid value for option \"ArticleCache\": %i. Changed to 1900", m_articleCache);
		m_articleCache = 1900;
	}
	else if (sizeof(void*) == 4 && m_parBuffer > 1900)
	{
		ConfigError("Invalid value for option \"ParBuffer\": %i. Changed to 1900", m_parBuffer);
		m_parBuffer = 1900;
	}

	if (sizeof(void*) == 4 && m_parBuffer + m_articleCache > 1900)
	{
		ConfigError("Options \"ArticleCache\" and \"ParBuffer\" in total cannot use more than 1900MB of memory in 32-Bit mode. Changed to 1500 and 400");
		m_articleCache = 1900;
		m_parBuffer = 400;
	}

	if (!Util::EmptyStr(m_unpackPassFile) && !Util::FileExists(m_unpackPassFile))
	{
		ConfigError("Invalid value for option \"UnpackPassFile\": %s. File not found", m_unpackPassFile);
	}
}

Options::OptEntries* Options::LockOptEntries()
{
	m_optEntriesMutex.Lock();
	return &m_optEntries;
}

void Options::UnlockOptEntries()
{
	m_optEntriesMutex.Unlock();
}
