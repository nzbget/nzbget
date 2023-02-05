/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "nzbget.h"
#include "Util.h"
#include "FileSystem.h"
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
static const char* OPTION_FORMAUTH				= "FormAuth";
static const char* OPTION_SECURECONTROL			= "SecureControl";
static const char* OPTION_SECUREPORT			= "SecurePort";
static const char* OPTION_SECURECERT			= "SecureCert";
static const char* OPTION_SECUREKEY				= "SecureKey";
static const char* OPTION_CERTSTORE				= "CertStore";
static const char* OPTION_CERTCHECK				= "CertCheck";
static const char* OPTION_AUTHORIZEDIP			= "AuthorizedIP";
static const char* OPTION_ARTICLETIMEOUT		= "ArticleTimeout";
static const char* OPTION_URLTIMEOUT			= "UrlTimeout";
static const char* OPTION_REMOTETIMEOUT			= "RemoteTimeout";
static const char* OPTION_FLUSHQUEUE			= "FlushQueue";
static const char* OPTION_NZBLOG				= "NzbLog";
static const char* OPTION_RAWARTICLE			= "RawArticle";
static const char* OPTION_SKIPWRITE				= "SkipWrite";
static const char* OPTION_ARTICLERETRIES		= "ArticleRetries";
static const char* OPTION_ARTICLEINTERVAL		= "ArticleInterval";
static const char* OPTION_URLRETRIES			= "UrlRetries";
static const char* OPTION_URLINTERVAL			= "UrlInterval";
static const char* OPTION_CONTINUEPARTIAL		= "ContinuePartial";
static const char* OPTION_URLCONNECTIONS		= "UrlConnections";
static const char* OPTION_LOGBUFFER				= "LogBuffer";
static const char* OPTION_INFOTARGET			= "InfoTarget";
static const char* OPTION_WARNINGTARGET			= "WarningTarget";
static const char* OPTION_ERRORTARGET			= "ErrorTarget";
static const char* OPTION_DEBUGTARGET			= "DebugTarget";
static const char* OPTION_DETAILTARGET			= "DetailTarget";
static const char* OPTION_PARCHECK				= "ParCheck";
static const char* OPTION_PARREPAIR				= "ParRepair";
static const char* OPTION_PARSCAN				= "ParScan";
static const char* OPTION_PARQUICK				= "ParQuick";
static const char* OPTION_POSTSTRATEGY			= "PostStrategy";
static const char* OPTION_FILENAMING			= "FileNaming";
static const char* OPTION_PARRENAME				= "ParRename";
static const char* OPTION_PARBUFFER				= "ParBuffer";
static const char* OPTION_PARTHREADS			= "ParThreads";
static const char* OPTION_RARRENAME				= "RarRename";
static const char* OPTION_HEALTHCHECK			= "HealthCheck";
static const char* OPTION_DIRECTRENAME			= "DirectRename";
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
static const char* OPTION_DISKSPACE				= "DiskSpace";
static const char* OPTION_CRASHTRACE			= "CrashTrace";
static const char* OPTION_CRASHDUMP				= "CrashDump";
static const char* OPTION_PARPAUSEQUEUE			= "ParPauseQueue";
static const char* OPTION_SCRIPTPAUSEQUEUE		= "ScriptPauseQueue";
static const char* OPTION_NZBCLEANUPDISK		= "NzbCleanupDisk";
static const char* OPTION_PARTIMELIMIT			= "ParTimeLimit";
static const char* OPTION_KEEPHISTORY			= "KeepHistory";
static const char* OPTION_UNPACK				= "Unpack";
static const char* OPTION_DIRECTUNPACK			= "DirectUnpack";
static const char* OPTION_UNPACKCLEANUPDISK		= "UnpackCleanupDisk";
static const char* OPTION_UNRARCMD				= "UnrarCmd";
static const char* OPTION_SEVENZIPCMD			= "SevenZipCmd";
static const char* OPTION_UNPACKPASSFILE		= "UnpackPassFile";
static const char* OPTION_UNPACKPAUSEQUEUE		= "UnpackPauseQueue";
static const char* OPTION_SCRIPTORDER			= "ScriptOrder";
static const char* OPTION_EXTENSIONS			= "Extensions";
static const char* OPTION_EXTCLEANUPDISK		= "ExtCleanupDisk";
static const char* OPTION_PARIGNOREEXT			= "ParIgnoreExt";
static const char* OPTION_UNPACKIGNOREEXT		= "UnpackIgnoreExt";
static const char* OPTION_FEEDHISTORY			= "FeedHistory";
static const char* OPTION_URLFORCE				= "UrlForce";
static const char* OPTION_TIMECORRECTION		= "TimeCorrection";
static const char* OPTION_PROPAGATIONDELAY		= "PropagationDelay";
static const char* OPTION_ARTICLECACHE			= "ArticleCache";
static const char* OPTION_EVENTINTERVAL			= "EventInterval";
static const char* OPTION_SHELLOVERRIDE			= "ShellOverride";
static const char* OPTION_MONTHLYQUOTA			= "MonthlyQuota";
static const char* OPTION_QUOTASTARTDAY			= "QuotaStartDay";
static const char* OPTION_DAILYQUOTA			= "DailyQuota";
static const char* OPTION_REORDERFILES			= "ReorderFiles";
static const char* OPTION_UPDATECHECK			= "UpdateCheck";
static const char* OPTION_ARTICLEREADCHUNKSIZE		= "ArticleReadChunkSize";

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
static const char* OPTION_PARCLEANUPQUEUE		= "ParCleanupQueue";
static const char* OPTION_DELETECLEANUPDISK		= "DeleteCleanupDisk";
static const char* OPTION_HISTORYCLEANUPDISK	= "HistoryCleanupDisk";
static const char* OPTION_SCANSCRIPT			= "ScanScript";
static const char* OPTION_QUEUESCRIPT			= "QueueScript";
static const char* OPTION_FEEDSCRIPT			= "FeedScript";
static const char* OPTION_DECODE				= "Decode";
static const char* OPTION_SAVEQUEUE				= "SaveQueue";
static const char* OPTION_RELOADQUEUE			= "ReloadQueue";
static const char* OPTION_TERMINATETIMEOUT		= "TerminateTimeout";
static const char* OPTION_ACCURATERATE			= "AccurateRate";
static const char* OPTION_CREATEBROKENLOG		= "CreateBrokenLog";
static const char* OPTION_BROKENLOG				= "BrokenLog";

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
		nullptr
	};
#endif

void Options::OptEntry::SetValue(const char* value)
{
	m_value = value;
	if (!m_defValue)
	{
		m_defValue = value;
	}
}

bool Options::OptEntry::Restricted()
{
	BString<1024> loName = *m_name;
	for (char* p = loName; *p; p++) *p = tolower(*p); // convert string to lowercase

	bool restricted = !strcasecmp(m_name, OPTION_CONTROLIP) ||
		!strcasecmp(m_name, OPTION_CONTROLPORT) ||
		!strcasecmp(m_name, OPTION_FORMAUTH) ||
		!strcasecmp(m_name, OPTION_SECURECONTROL) ||
		!strcasecmp(m_name, OPTION_SECUREPORT) ||
		!strcasecmp(m_name, OPTION_SECURECERT) ||
		!strcasecmp(m_name, OPTION_SECUREKEY) ||
		!strcasecmp(m_name, OPTION_CERTSTORE) ||
		!strcasecmp(m_name, OPTION_CERTCHECK) ||
		!strcasecmp(m_name, OPTION_AUTHORIZEDIP) ||
		!strcasecmp(m_name, OPTION_DAEMONUSERNAME) ||
		!strcasecmp(m_name, OPTION_UMASK) ||
		strchr(m_name, ':') ||			// All extension script options
		strstr(loName, "username") ||	// ServerX.Username, ControlUsername, etc.
		strstr(loName, "password");		// ServerX.Password, ControlPassword, etc.

	return restricted;
}

Options::OptEntry* Options::OptEntries::FindOption(const char* name)
{
	if (!name)
	{
		return nullptr;
	}

	for (OptEntry& optEntry : this)
	{
		if (!strcasecmp(optEntry.GetName(), name))
		{
			return &optEntry;
		}
	}

	return nullptr;
}


Options::Category* Options::Categories::FindCategory(const char* name, bool searchAliases)
{
	if (!name)
	{
		return nullptr;
	}

	for (Category& category : this)
	{
		if (!strcasecmp(category.GetName(), name))
		{
			return &category;
		}
	}

	if (searchAliases)
	{
		for (Category& category : this)
		{
			for (CString& alias : category.GetAliases())
			{
				WildMask mask(alias);
				if (mask.Match(name))
				{
					return &category;
				}
			}
		}
	}

	return nullptr;
}


Options::Options(const char* exeName, const char* configFilename, bool noConfig,
	CmdOptList* commandLineOptions, Extender* extender)
{
	Init(exeName, configFilename, noConfig, commandLineOptions, false, extender);
}

Options::Options(CmdOptList* commandLineOptions, Extender* extender)
{
	Init("nzbget/nzbget", nullptr, true, commandLineOptions, true, extender);
}

void Options::Init(const char* exeName, const char* configFilename, bool noConfig,
	CmdOptList* commandLineOptions, bool noDiskAccess, Extender* extender)
{
	g_Options = this;
	m_extender = extender;
	m_noDiskAccess = noDiskAccess;
	m_noConfig = noConfig;
	m_configFilename = configFilename;

	SetOption(OPTION_CONFIGFILE, "");

	CString filename;
	if (m_noDiskAccess)
	{
		filename = exeName;
	}
	else
	{
		filename = FileSystem::GetExeFileName(exeName);
	}
	FileSystem::NormalizePathSeparators(filename);
	SetOption(OPTION_APPBIN, filename);
	char* end = strrchr(filename, PATH_SEPARATOR);
	if (end) *end = '\0';
	SetOption(OPTION_APPDIR, filename);
	m_appDir = *filename;

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

	ConvertOldOptions(&m_optEntries);
	InitOptions();
	CheckOptions();

	InitServers();
	InitCategories();
	InitScheduler();
	InitFeeds();
}

Options::~Options()
{
	g_Options = nullptr;
}

void Options::ConfigError(const char* msg, ...)
{
	char tmp2[1024];

	va_list ap;
	va_start(ap, msg);
	vsnprintf(tmp2, 1024, msg, ap);
	tmp2[1024-1] = '\0';
	va_end(ap);

	printf("%s(%i): %s\n", m_configFilename ? FileSystem::BaseFileName(m_configFilename) : "<noconfig>", m_configLine, tmp2);
	error("%s(%i): %s", m_configFilename ? FileSystem::BaseFileName(m_configFilename) : "<noconfig>", m_configLine, tmp2);

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

	printf("%s(%i): %s\n", FileSystem::BaseFileName(m_configFilename), m_configLine, tmp2);
	warn("%s(%i): %s", FileSystem::BaseFileName(m_configFilename), m_configLine, tmp2);
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
	SetOption(OPTION_LOGFILE, "${MainDir}/nzbget.log");
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
	SetOption(OPTION_FORMAUTH, "no");
	SetOption(OPTION_SECURECONTROL, "no");
	SetOption(OPTION_SECUREPORT, "6791");
	SetOption(OPTION_SECURECERT, "");
	SetOption(OPTION_SECUREKEY, "");
	SetOption(OPTION_CERTSTORE, "");
	SetOption(OPTION_CERTCHECK, "no");
	SetOption(OPTION_AUTHORIZEDIP, "");
	SetOption(OPTION_ARTICLETIMEOUT, "60");
	SetOption(OPTION_URLTIMEOUT, "60");
	SetOption(OPTION_REMOTETIMEOUT, "90");
	SetOption(OPTION_FLUSHQUEUE, "yes");
	SetOption(OPTION_NZBLOG, "yes");
	SetOption(OPTION_RAWARTICLE, "no");
	SetOption(OPTION_SKIPWRITE, "no");
	SetOption(OPTION_ARTICLERETRIES, "3");
	SetOption(OPTION_ARTICLEINTERVAL, "10");
	SetOption(OPTION_URLRETRIES, "3");
	SetOption(OPTION_URLINTERVAL, "10");
	SetOption(OPTION_CONTINUEPARTIAL, "no");
	SetOption(OPTION_URLCONNECTIONS, "4");
	SetOption(OPTION_LOGBUFFER, "1000");
	SetOption(OPTION_INFOTARGET, "both");
	SetOption(OPTION_WARNINGTARGET, "both");
	SetOption(OPTION_ERRORTARGET, "both");
	SetOption(OPTION_DEBUGTARGET, "none");
	SetOption(OPTION_DETAILTARGET, "both");
	SetOption(OPTION_PARCHECK, "auto");
	SetOption(OPTION_PARREPAIR, "yes");
	SetOption(OPTION_PARSCAN, "extended");
	SetOption(OPTION_PARQUICK, "yes");
	SetOption(OPTION_POSTSTRATEGY, "sequential");
	SetOption(OPTION_FILENAMING, "article");
	SetOption(OPTION_PARRENAME, "yes");
	SetOption(OPTION_PARBUFFER, "16");
	SetOption(OPTION_PARTHREADS, "1");
	SetOption(OPTION_RARRENAME, "yes");
	SetOption(OPTION_HEALTHCHECK, "none");
	SetOption(OPTION_DIRECTRENAME, "no");
	SetOption(OPTION_SCRIPTORDER, "");
	SetOption(OPTION_EXTENSIONS, "");
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
	SetOption(OPTION_DISKSPACE, "250");
	SetOption(OPTION_CRASHTRACE, "no");
	SetOption(OPTION_CRASHDUMP, "no");
	SetOption(OPTION_PARPAUSEQUEUE, "no");
	SetOption(OPTION_SCRIPTPAUSEQUEUE, "no");
	SetOption(OPTION_NZBCLEANUPDISK, "no");
	SetOption(OPTION_PARTIMELIMIT, "0");
	SetOption(OPTION_KEEPHISTORY, "7");
	SetOption(OPTION_UNPACK, "no");
	SetOption(OPTION_DIRECTUNPACK, "no");
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
	SetOption(OPTION_UNPACKIGNOREEXT, "");
	SetOption(OPTION_FEEDHISTORY, "7");
	SetOption(OPTION_URLFORCE, "yes");
	SetOption(OPTION_TIMECORRECTION, "0");
	SetOption(OPTION_PROPAGATIONDELAY, "0");
	SetOption(OPTION_ARTICLECACHE, "0");
	SetOption(OPTION_EVENTINTERVAL, "0");
	SetOption(OPTION_SHELLOVERRIDE, "");
	SetOption(OPTION_MONTHLYQUOTA, "0");
	SetOption(OPTION_QUOTASTARTDAY, "1");
	SetOption(OPTION_DAILYQUOTA, "0");
	SetOption(OPTION_REORDERFILES, "no");
	SetOption(OPTION_UPDATECHECK, "none");
	SetOption(OPTION_ARTICLEREADCHUNKSIZE, "4");
}

void Options::InitOptFile()
{
	if (!m_configFilename && !m_noConfig)
	{
		// search for config file in default locations
#ifdef WIN32
		BString<1024> filename("%s\\nzbget.conf", *m_appDir);

		if (!FileSystem::FileExists(filename))
		{
			char appDataPath[MAX_PATH];
			SHGetFolderPath(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, appDataPath);
			filename.Format("%s\\NZBGet\\nzbget.conf", appDataPath);

			if (m_extender && !FileSystem::FileExists(filename))
			{
				m_extender->SetupFirstStart();
			}
		}

		if (FileSystem::FileExists(filename))
		{
			m_configFilename = filename;
		}
#else
		// look in the exe-directory first
		BString<1024> filename("%s/nzbget.conf", *m_appDir);

		if (FileSystem::FileExists(filename))
		{
			m_configFilename = filename;
		}
		else
		{
			int p = 0;
			while (const char* altfilename = PossibleConfigLocations[p++])
			{
				// substitute HOME-variable
				filename = FileSystem::ExpandHomePath(altfilename);

				if (FileSystem::FileExists(filename))
				{
					m_configFilename = *filename;
					break;
				}
			}
		}
#endif
	}

	if (m_configFilename)
	{
		// normalize path in filename
		CString filename = FileSystem::ExpandFileName(m_configFilename);

#ifndef WIN32
		// substitute HOME-variable
		filename = FileSystem::ExpandHomePath(filename);
#endif

		m_configFilename = *filename;

		SetOption(OPTION_CONFIGFILE, m_configFilename);
		LoadConfigFile();
	}
}

void Options::CheckDir(CString& dir, const char* optionName,
	const char* parentDir, bool allowEmpty, bool create)
{
	const char* tempdir = GetOption(optionName);

	if (m_noDiskAccess)
	{
		dir = tempdir;
		return;
	}

	if (Util::EmptyStr(tempdir))
	{
		if (!allowEmpty)
		{
			ConfigError("Invalid value for option \"%s\": <empty>", optionName);
		}
		dir = "";
		return;
	}

	dir = tempdir;
	FileSystem::NormalizePathSeparators((char*)dir);
	if (!Util::EmptyStr(dir) && dir[dir.Length() - 1] == PATH_SEPARATOR)
	{
		// remove trailing slash
		dir[dir.Length() - 1] = '\0';
	}

	if (!(dir[0] == PATH_SEPARATOR || dir[0] == ALT_PATH_SEPARATOR ||
		(dir[0] && dir[1] == ':')) &&
		!Util::EmptyStr(parentDir))
	{
		// convert relative path to absolute path
		int plen = strlen(parentDir);

		BString<1024> usedir2;
		if (parentDir[plen-1] == PATH_SEPARATOR || parentDir[plen-1] == ALT_PATH_SEPARATOR)
		{
			usedir2.Format("%s%s", parentDir, *dir);
		}
		else
		{
			usedir2.Format("%s%c%s", parentDir, PATH_SEPARATOR, *dir);
		}

		FileSystem::NormalizePathSeparators((char*)usedir2);
		dir = usedir2;
		SetOption(optionName, usedir2);
	}

	// Ensure the dir is created
	CString errmsg;
	if (create && !FileSystem::ForceDirectories(dir, errmsg))
	{
		ConfigError("Invalid value for option \"%s\" (%s): %s", optionName, *dir, *errmsg);
	}
}

void Options::InitOptions()
{
	const char* mainDir = GetOption(OPTION_MAINDIR);

	CheckDir(m_destDir, OPTION_DESTDIR, mainDir, false, false);
	CheckDir(m_interDir, OPTION_INTERDIR, mainDir, true, false);
	CheckDir(m_tempDir, OPTION_TEMPDIR, mainDir, false, true);
	CheckDir(m_queueDir, OPTION_QUEUEDIR, mainDir, false, true);
	CheckDir(m_webDir, OPTION_WEBDIR, nullptr, true, false);
	CheckDir(m_scriptDir, OPTION_SCRIPTDIR, mainDir, true, false);
	CheckDir(m_nzbDir, OPTION_NZBDIR, mainDir, false, true);

	m_requiredDir = GetOption(OPTION_REQUIREDDIR);

	m_configTemplate		= GetOption(OPTION_CONFIGTEMPLATE);
	m_scriptOrder			= GetOption(OPTION_SCRIPTORDER);
	m_extensions			= GetOption(OPTION_EXTENSIONS);
	m_controlIp				= GetOption(OPTION_CONTROLIP);
	m_controlUsername		= GetOption(OPTION_CONTROLUSERNAME);
	m_controlPassword		= GetOption(OPTION_CONTROLPASSWORD);
	m_restrictedUsername	= GetOption(OPTION_RESTRICTEDUSERNAME);
	m_restrictedPassword	= GetOption(OPTION_RESTRICTEDPASSWORD);
	m_addUsername			= GetOption(OPTION_ADDUSERNAME);
	m_addPassword			= GetOption(OPTION_ADDPASSWORD);
	m_secureCert			= GetOption(OPTION_SECURECERT);
	m_secureKey				= GetOption(OPTION_SECUREKEY);
	m_certStore				= GetOption(OPTION_CERTSTORE);
	m_authorizedIp			= GetOption(OPTION_AUTHORIZEDIP);
	m_lockFile				= GetOption(OPTION_LOCKFILE);
	m_daemonUsername		= GetOption(OPTION_DAEMONUSERNAME);
	m_logFile				= GetOption(OPTION_LOGFILE);
	m_unrarCmd				= GetOption(OPTION_UNRARCMD);
	m_sevenZipCmd			= GetOption(OPTION_SEVENZIPCMD);
	m_unpackPassFile		= GetOption(OPTION_UNPACKPASSFILE);
	m_extCleanupDisk		= GetOption(OPTION_EXTCLEANUPDISK);
	m_parIgnoreExt			= GetOption(OPTION_PARIGNOREEXT);
	m_unpackIgnoreExt		= GetOption(OPTION_UNPACKIGNOREEXT);
	m_shellOverride			= GetOption(OPTION_SHELLOVERRIDE);

	m_downloadRate			= ParseIntValue(OPTION_DOWNLOADRATE, 10) * 1024;
	m_articleTimeout		= ParseIntValue(OPTION_ARTICLETIMEOUT, 10);
	m_urlTimeout			= ParseIntValue(OPTION_URLTIMEOUT, 10);
	m_remoteTimeout			= ParseIntValue(OPTION_REMOTETIMEOUT, 10);
	m_articleRetries		= ParseIntValue(OPTION_ARTICLERETRIES, 10);
	m_articleInterval		= ParseIntValue(OPTION_ARTICLEINTERVAL, 10);
	m_urlRetries			= ParseIntValue(OPTION_URLRETRIES, 10);
	m_urlInterval			= ParseIntValue(OPTION_URLINTERVAL, 10);
	m_controlPort			= ParseIntValue(OPTION_CONTROLPORT, 10);
	m_securePort			= ParseIntValue(OPTION_SECUREPORT, 10);
	m_urlConnections		= ParseIntValue(OPTION_URLCONNECTIONS, 10);
	m_logBuffer				= ParseIntValue(OPTION_LOGBUFFER, 10);
	m_rotateLog				= ParseIntValue(OPTION_ROTATELOG, 10);
	m_umask					= ParseIntValue(OPTION_UMASK, 8);
	m_updateInterval		= ParseIntValue(OPTION_UPDATEINTERVAL, 10);
	m_writeBuffer			= ParseIntValue(OPTION_WRITEBUFFER, 10);
	m_nzbDirInterval		= ParseIntValue(OPTION_NZBDIRINTERVAL, 10);
	m_nzbDirFileAge			= ParseIntValue(OPTION_NZBDIRFILEAGE, 10);
	m_diskSpace				= ParseIntValue(OPTION_DISKSPACE, 10);
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
	m_eventInterval			= ParseIntValue(OPTION_EVENTINTERVAL, 10);
	m_parBuffer				= ParseIntValue(OPTION_PARBUFFER, 10);
	m_parThreads			= ParseIntValue(OPTION_PARTHREADS, 10);
	m_monthlyQuota			= ParseIntValue(OPTION_MONTHLYQUOTA, 10);
	m_quotaStartDay			= ParseIntValue(OPTION_QUOTASTARTDAY, 10);
	m_dailyQuota			= ParseIntValue(OPTION_DAILYQUOTA, 10);
	m_articleReadChunkSize		= ParseIntValue(OPTION_ARTICLEREADCHUNKSIZE, 10) * 1024;

	m_nzbLog				= (bool)ParseEnumValue(OPTION_NZBLOG, BoolCount, BoolNames, BoolValues);
	m_appendCategoryDir		= (bool)ParseEnumValue(OPTION_APPENDCATEGORYDIR, BoolCount, BoolNames, BoolValues);
	m_continuePartial		= (bool)ParseEnumValue(OPTION_CONTINUEPARTIAL, BoolCount, BoolNames, BoolValues);
	m_flushQueue			= (bool)ParseEnumValue(OPTION_FLUSHQUEUE, BoolCount, BoolNames, BoolValues);
	m_dupeCheck				= (bool)ParseEnumValue(OPTION_DUPECHECK, BoolCount, BoolNames, BoolValues);
	m_parRepair				= (bool)ParseEnumValue(OPTION_PARREPAIR, BoolCount, BoolNames, BoolValues);
	m_parQuick				= (bool)ParseEnumValue(OPTION_PARQUICK, BoolCount, BoolNames, BoolValues);
	m_parRename				= (bool)ParseEnumValue(OPTION_PARRENAME, BoolCount, BoolNames, BoolValues);
	m_rarRename				= (bool)ParseEnumValue(OPTION_RARRENAME, BoolCount, BoolNames, BoolValues);
	m_directRename			= (bool)ParseEnumValue(OPTION_DIRECTRENAME, BoolCount, BoolNames, BoolValues);
	m_cursesNzbName			= (bool)ParseEnumValue(OPTION_CURSESNZBNAME, BoolCount, BoolNames, BoolValues);
	m_cursesTime			= (bool)ParseEnumValue(OPTION_CURSESTIME, BoolCount, BoolNames, BoolValues);
	m_cursesGroup			= (bool)ParseEnumValue(OPTION_CURSESGROUP, BoolCount, BoolNames, BoolValues);
	m_crcCheck				= (bool)ParseEnumValue(OPTION_CRCCHECK, BoolCount, BoolNames, BoolValues);
	m_directWrite			= (bool)ParseEnumValue(OPTION_DIRECTWRITE, BoolCount, BoolNames, BoolValues);
	m_rawArticle			= (bool)ParseEnumValue(OPTION_RAWARTICLE, BoolCount, BoolNames, BoolValues);
	m_skipWrite				= (bool)ParseEnumValue(OPTION_SKIPWRITE, BoolCount, BoolNames, BoolValues);
	m_crashTrace			= (bool)ParseEnumValue(OPTION_CRASHTRACE, BoolCount, BoolNames, BoolValues);
	m_crashDump				= (bool)ParseEnumValue(OPTION_CRASHDUMP, BoolCount, BoolNames, BoolValues);
	m_parPauseQueue			= (bool)ParseEnumValue(OPTION_PARPAUSEQUEUE, BoolCount, BoolNames, BoolValues);
	m_scriptPauseQueue		= (bool)ParseEnumValue(OPTION_SCRIPTPAUSEQUEUE, BoolCount, BoolNames, BoolValues);
	m_nzbCleanupDisk		= (bool)ParseEnumValue(OPTION_NZBCLEANUPDISK, BoolCount, BoolNames, BoolValues);
	m_formAuth				= (bool)ParseEnumValue(OPTION_FORMAUTH, BoolCount, BoolNames, BoolValues);
	m_secureControl			= (bool)ParseEnumValue(OPTION_SECURECONTROL, BoolCount, BoolNames, BoolValues);
	m_unpack				= (bool)ParseEnumValue(OPTION_UNPACK, BoolCount, BoolNames, BoolValues);
	m_directUnpack			= (bool)ParseEnumValue(OPTION_DIRECTUNPACK, BoolCount, BoolNames, BoolValues);
	m_unpackCleanupDisk		= (bool)ParseEnumValue(OPTION_UNPACKCLEANUPDISK, BoolCount, BoolNames, BoolValues);
	m_unpackPauseQueue		= (bool)ParseEnumValue(OPTION_UNPACKPAUSEQUEUE, BoolCount, BoolNames, BoolValues);
	m_urlForce				= (bool)ParseEnumValue(OPTION_URLFORCE, BoolCount, BoolNames, BoolValues);
	m_certCheck				= (bool)ParseEnumValue(OPTION_CERTCHECK, BoolCount, BoolNames, BoolValues);
	m_reorderFiles			= (bool)ParseEnumValue(OPTION_REORDERFILES, BoolCount, BoolNames, BoolValues);

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

	const char* PostStrategyNames[] = { "sequential", "balanced", "aggressive", "rocket" };
	const int PostStrategyValues[] = { ppSequential, ppBalanced, ppAggressive, ppRocket };
	const int PostStrategyCount = 4;
	m_postStrategy = (EPostStrategy)ParseEnumValue(OPTION_POSTSTRATEGY, PostStrategyCount, PostStrategyNames, PostStrategyValues);

	const char* FileNamingNames[] = { "auto", "article", "nzb" };
	const int FileNamingValues[] = { nfAuto, nfArticle, nfNzb };
	const int FileNamingCount = 4;
	m_fileNaming = (EFileNaming)ParseEnumValue(OPTION_FILENAMING, FileNamingCount, FileNamingNames, FileNamingValues);

	const char* HealthCheckNames[] = { "pause", "delete", "park", "none" };
	const int HealthCheckValues[] = { hcPause, hcDelete, hcPark, hcNone };
	const int HealthCheckCount = 4;
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
		val = strtol(optEntry->GetDefValue(), nullptr, base);
	}

	return val;
}

void Options::SetOption(const char* optname, const char* value)
{
	OptEntry* optEntry = FindOption(optname);
	if (!optEntry)
	{
		m_optEntries.emplace_back(optname, nullptr);
		optEntry = &m_optEntries.back();
	}

	CString curvalue;

#ifndef WIN32
	if (value && (value[0] == '~') && (value[1] == '/'))
	{
		if (m_noDiskAccess)
		{
			curvalue = value;
		}
		else
		{
			curvalue = FileSystem::ExpandHomePath(value);
		}
	}
	else
#endif
	{
		curvalue = value;
	}

	optEntry->SetLineNo(m_configLine);

	// expand variables
	while (const char* dollar = strstr(curvalue, "${"))
	{
		const char* end = strchr(dollar, '}');
		if (end)
		{
			int varlen = (int)(end - dollar - 2);
			BString<100> variable;
			variable.Set(dollar + 2, varlen);
			const char* varvalue = GetOption(variable);
			if (varvalue)
			{
				curvalue.Replace((int)(dollar - curvalue), 2 + varlen + 1, varvalue);
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
	return nullptr;
}

void Options::InitServers()
{
	int n = 1;
	while (true)
	{
		const char* nactive = GetOption(BString<100>("Server%i.Active", n));
		bool active = true;
		if (nactive)
		{
			active = (bool)ParseEnumValue(BString<100>("Server%i.Active", n), BoolCount, BoolNames, BoolValues);
		}

		const char* nname = GetOption(BString<100>("Server%i.Name", n));
		const char* nlevel = GetOption(BString<100>("Server%i.Level", n));
		const char* ngroup = GetOption(BString<100>("Server%i.Group", n));
		const char* nhost = GetOption(BString<100>("Server%i.Host", n));
		const char* nport = GetOption(BString<100>("Server%i.Port", n));
		const char* nusername = GetOption(BString<100>("Server%i.Username", n));
		const char* npassword = GetOption(BString<100>("Server%i.Password", n));

		const char* njoingroup = GetOption(BString<100>("Server%i.JoinGroup", n));
		bool joinGroup = false;
		if (njoingroup)
		{
			joinGroup = (bool)ParseEnumValue(BString<100>("Server%i.JoinGroup", n), BoolCount, BoolNames, BoolValues);
		}

		const char* noptional = GetOption(BString<100>("Server%i.Optional", n));
		bool optional = false;
		if (noptional)
		{
			optional = (bool)ParseEnumValue(BString<100>("Server%i.Optional", n), BoolCount, BoolNames, BoolValues);
		}

		const char* ntls = GetOption(BString<100>("Server%i.Encryption", n));
		bool tls = false;
		if (ntls)
		{
			tls = (bool)ParseEnumValue(BString<100>("Server%i.Encryption", n), BoolCount, BoolNames, BoolValues);
#ifdef DISABLE_TLS
			if (tls)
			{
				ConfigError("Invalid value for option \"%s\": program was compiled without TLS/SSL-support",
					*BString<100>("Server%i.Encryption", n));
				tls = false;
			}
#endif
			m_tls |= tls;
		}

		const char* nipversion = GetOption(BString<100>("Server%i.IpVersion", n));
		int ipversion = 0;
		if (nipversion)
		{
			const char* IpVersionNames[] = {"auto", "ipv4", "ipv6"};
			const int IpVersionValues[] = {0, 4, 6};
			const int IpVersionCount = 3;
			ipversion = ParseEnumValue(BString<100>("Server%i.IpVersion", n), IpVersionCount, IpVersionNames, IpVersionValues);
		}

		const char* ncipher = GetOption(BString<100>("Server%i.Cipher", n));
		const char* nconnections = GetOption(BString<100>("Server%i.Connections", n));
		const char* nretention = GetOption(BString<100>("Server%i.Retention", n));

		bool definition = nactive || nname || nlevel || ngroup || nhost || nport || noptional ||
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
					ipversion,
					nusername, npassword,
					joinGroup, tls, ncipher,
					nconnections ? atoi(nconnections) : 1,
					nretention ? atoi(nretention) : 0,
					nlevel ? atoi(nlevel) : 0,
					ngroup ? atoi(ngroup) : 0,
					optional);
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
		const char* nname = GetOption(BString<100>("Category%i.Name", n));
		const char* ndestdir = GetOption(BString<100>("Category%i.DestDir", n));

		const char* nunpack = GetOption(BString<100>("Category%i.Unpack", n));
		bool unpack = true;
		if (nunpack)
		{
			unpack = (bool)ParseEnumValue(BString<100>("Category%i.Unpack", n), BoolCount, BoolNames, BoolValues);
		}

		const char* nextensions = GetOption(BString<100>("Category%i.Extensions", n));
		const char* naliases = GetOption(BString<100>("Category%i.Aliases", n));

		bool definition = nname || ndestdir || nunpack || nextensions || naliases;
		bool completed = nname && strlen(nname) > 0;

		if (!definition)
		{
			break;
		}

		if (completed)
		{
			CString destDir;
			if (ndestdir && ndestdir[0] != '\0')
			{
				CheckDir(destDir, BString<100>("Category%i.DestDir", n), m_destDir, false, false);
			}

			m_categories.emplace_back(nname, destDir, unpack, nextensions);
			Category& category = m_categories.back();

			// split Aliases into tokens and create items for each token
			if (naliases)
			{
				Tokenizer tok(naliases, ",;");
				while (const char* aliasName = tok.Next())
				{
					category.GetAliases()->push_back(aliasName);
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
		const char* nname = GetOption(BString<100>("Feed%i.Name", n));
		const char* nurl = GetOption(BString<100>("Feed%i.URL", n));
		const char* nfilter = GetOption(BString<100>("Feed%i.Filter", n));
		const char* ncategory = GetOption(BString<100>("Feed%i.Category", n));
		const char* nextensions = GetOption(BString<100>("Feed%i.Extensions", n));

		const char* nbacklog = GetOption(BString<100>("Feed%i.Backlog", n));
		bool backlog = true;
		if (nbacklog)
		{
			backlog = (bool)ParseEnumValue(BString<100>("Feed%i.Backlog", n), BoolCount, BoolNames, BoolValues);
		}

		const char* npausenzb = GetOption(BString<100>("Feed%i.PauseNzb", n));
		bool pauseNzb = false;
		if (npausenzb)
		{
			pauseNzb = (bool)ParseEnumValue(BString<100>("Feed%i.PauseNzb", n), BoolCount, BoolNames, BoolValues);
		}

		const char* ninterval = GetOption(BString<100>("Feed%i.Interval", n));
		const char* npriority = GetOption(BString<100>("Feed%i.Priority", n));

		bool definition = nname || nurl || nfilter || ncategory || nbacklog || npausenzb ||
			ninterval || npriority || nextensions;
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
					backlog, pauseNzb, ncategory, npriority ? atoi(npriority) : 0, nextensions);
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
		const char* time = GetOption(BString<100>("Task%i.Time", n));
		const char* weekDays = GetOption(BString<100>("Task%i.WeekDays", n));
		const char* command = GetOption(BString<100>("Task%i.Command", n));
		const char* downloadRate = GetOption(BString<100>("Task%i.DownloadRate", n));
		const char* process = GetOption(BString<100>("Task%i.Process", n));
		const char* param = GetOption(BString<100>("Task%i.Param", n));

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
		ESchedulerCommand taskCommand = (ESchedulerCommand)ParseEnumValue(
			BString<100>("Task%i.Command", n), CommandCount, CommandNames, CommandValues);

		if (param && strlen(param) > 0 && taskCommand == scProcess &&
			Util::SplitCommandLine(param).empty())
		{
			ConfigError("Invalid value for option \"Task%i.Param\"", n);
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

		CreateSchedulerTask(n, time, weekDays, taskCommand, param);
	}
}

void Options::CreateSchedulerTask(int id, const char* time, const char* weekDays,
	ESchedulerCommand command, const char* param)
{
	if (!id)
	{
		m_configLine = 0;
	}

	int weekDaysVal = 0;
	if (weekDays && !ParseWeekDays(weekDays, &weekDaysVal))
	{
		ConfigError("Invalid value for option \"Task%i.WeekDays\": \"%s\"", id, weekDays);
		return;
	}

	int hours, minutes;
	Tokenizer tok(time, ";,");
	while (const char* oneTime = tok.Next())
	{
		if (!ParseTime(oneTime, &hours, &minutes))
		{
			ConfigError("Invalid value for option \"Task%i.Time\": \"%s\"", id, oneTime);
			return;
		}

		if (m_extender)
		{
			if (hours == -2)
			{
				for (int everyHour = 0; everyHour < 24; everyHour++)
				{
					m_extender->AddTask(id, everyHour, minutes, weekDaysVal, command, param);
				}
			}
			else
			{
				m_extender->AddTask(id, hours, minutes, weekDaysVal, command, param);
			}
		}
	}
}

bool Options::ParseTime(const char* time, int* hours, int* minutes)
{
	if (!strcmp(time, "*"))
	{
		*hours = -1;
		return true;
	}

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
		*hours = -2;
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

	DiskFile infile;

	if (!infile.Open(m_configFilename, DiskFile::omRead))
	{
		ConfigError("Could not open file %s", *m_configFilename);
		m_fatalError = true;
		return;
	}

	m_configLine = 0;
	int bufLen = (int)FileSystem::FileSize(m_configFilename) + 1;
	CharBuffer buf(bufLen);

	int line = 0;
	while (infile.ReadLine(buf, buf.Size() - 1))
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

	infile.Close();

	m_configLine = 0;
}

void Options::InitCommandLineOptions(CmdOptList* commandLineOptions)
{
	for (const char* option : *commandLineOptions)
	{
		SetOptionString(option);
	}
}

bool Options::SetOptionString(const char* option)
{
	CString optname;
	CString optvalue;

	if (!SplitOptionString(option, optname, optvalue))
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
		ConfigError("Invalid option \"%s\"", *optname);
	}

	return ok;
}

/*
 * Splits option string into name and value;
 * Converts old names and values if necessary;
 * Returns true if the option string has name and value;
 */
bool Options::SplitOptionString(const char* option, CString& optName, CString& optValue)
{
	const char* eq = strchr(option, '=');
	if (!eq || eq == option)
	{
		return false;
	}

	optName.Set(option, (int)(eq - option));
	optValue.Set(eq + 1);

	ConvertOldOption(optName, optValue);

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
			!strcasecmp(p, ".retention") || !strcasecmp(p, ".optional") ||
			!strcasecmp(p, ".notes") || !strcasecmp(p, ".ipversion")))
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
		if (p && (!strcasecmp(p, ".name") || !strcasecmp(p, ".destdir") || !strcasecmp(p, ".extensions") ||
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
			 !strcasecmp(p, ".category") || !strcasecmp(p, ".priority") || !strcasecmp(p, ".extensions")))
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
		!strcasecmp(optname, OPTION_RELOADPOSTQUEUE) ||
		!strcasecmp(optname, OPTION_PARCLEANUPQUEUE) ||
		!strcasecmp(optname, OPTION_DELETECLEANUPDISK) ||
		!strcasecmp(optname, OPTION_HISTORYCLEANUPDISK) ||
		!strcasecmp(optname, OPTION_SAVEQUEUE) ||
		!strcasecmp(optname, OPTION_RELOADQUEUE) ||
		!strcasecmp(optname, OPTION_TERMINATETIMEOUT) ||
		!strcasecmp(optname, OPTION_ACCURATERATE) ||
		!strcasecmp(optname, OPTION_CREATEBROKENLOG) ||
		!strcasecmp(optname, OPTION_BROKENLOG))
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
			ConfigError("Option \"%s\" is obsolete, ignored, use \"%s\" and \"%s\" instead",
				optname, OPTION_SCRIPTDIR, OPTION_EXTENSIONS);
		}
		return true;
	}

	if (!strcasecmp(optname, OPTION_SCANSCRIPT) ||
		!strcasecmp(optname, OPTION_QUEUESCRIPT) ||
		!strcasecmp(optname, OPTION_FEEDSCRIPT))
	{
		// will be automatically converted into "Extensions"
		return true;
	}

	if (!strcasecmp(optname, OPTION_CREATELOG) || !strcasecmp(optname, OPTION_RESETLOG))
	{
		ConfigWarn("Option \"%s\" is obsolete, ignored, use \"%s\" instead", optname, OPTION_WRITELOG);
		return true;
	}

	return false;
}

void Options::ConvertOldOption(CString& option, CString& value)
{
	// for compatibility with older versions accept old option names

	if (!strcasecmp(option, "$MAINDIR"))
	{
		option = "MainDir";
	}

	if (!strcasecmp(option, "ServerIP"))
	{
		option = "ControlIP";
	}

	if (!strcasecmp(option, "ServerPort"))
	{
		option = "ControlPort";
	}

	if (!strcasecmp(option, "ServerPassword"))
	{
		option = "ControlPassword";
	}

	if (!strcasecmp(option, "PostPauseQueue"))
	{
		option = "ScriptPauseQueue";
	}

	if (!strcasecmp(option, "ParCheck") && !strcasecmp(value, "yes"))
	{
		value = "always";
	}

	if (!strcasecmp(option, "ParCheck") && !strcasecmp(value, "no"))
	{
		value = "auto";
	}

	if (!strcasecmp(option, "ParScan") && !strcasecmp(value, "auto"))
	{
		value = "extended";
	}

	if (!strcasecmp(option, "DefScript") || !strcasecmp(option, "PostScript"))
	{
		option = "Extensions";
	}

	int nameLen = strlen(option);
	if (!strncasecmp(option, "Category", 8) &&
		((nameLen > 10 && !strcasecmp(option + nameLen - 10, ".DefScript")) ||
		 (nameLen > 11 && !strcasecmp(option + nameLen - 11, ".PostScript"))))
	{
		option.Replace(".DefScript", ".Extensions");
		option.Replace(".PostScript", ".Extensions");
	}
	if (!strncasecmp(option, "Feed", 4) && nameLen > 11 && !strcasecmp(option + nameLen - 11, ".FeedScript"))
	{
		option.Replace(".FeedScript", ".Extensions");
	}

	if (!strcasecmp(option, "WriteBufferSize"))
	{
		option = "WriteBuffer";
		int val = strtol(value, nullptr, 10);
		val = val == -1 ? 1024 : val / 1024;
		value.Format("%i", val);
	}

	if (!strcasecmp(option, "ConnectionTimeout"))
	{
		option = "ArticleTimeout";
	}

	if (!strcasecmp(option, "Retries"))
	{
		option = "ArticleRetries";
	}

	if (!strcasecmp(option, "RetryInterval"))
	{
		option = "ArticleInterval";
	}

	if (!strcasecmp(option, "DumpCore"))
	{
		option = OPTION_CRASHDUMP;
	}

	if (!strcasecmp(option, OPTION_DECODE))
	{
		option = OPTION_RAWARTICLE;
		value = !strcasecmp(value, "no") ? "yes" : "no";
	}

	if (!strcasecmp(option, "LogBufferSize"))
	{
		option = OPTION_LOGBUFFER;
	}
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
	if (m_directRename)
	{
		LocateOptionSrcPos(OPTION_DIRECTRENAME);
		ConfigError("Invalid value for option \"%s\": program was compiled without parcheck-support", OPTION_DIRECTRENAME);
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

	if (m_certCheck)
	{
		LocateOptionSrcPos(OPTION_CERTCHECK);
		ConfigError("Invalid value for option \"%s\": program was compiled without TLS/SSL-support", OPTION_CERTCHECK);
	}
#endif

#ifdef HAVE_OPENSSL
#ifndef HAVE_X509_CHECK_HOST
	if (m_certCheck)
	{
		LocateOptionSrcPos(OPTION_CERTCHECK);
		ConfigWarn("TLS certificate verification (option \"%s\") is limited because the program "
			"was compiled with older OpenSSL version not supporting hostname validation (at least OpenSSL 1.0.2d is required)", OPTION_CERTCHECK);
	}
#endif
#endif

#ifdef HAVE_LIBGNUTLS
#if	GNUTLS_VERSION_NUMBER < 0x030104
	if (m_certCheck)
	{
		LocateOptionSrcPos(OPTION_CERTCHECK);
		ConfigWarn("TLS certificate verification (option \"%s\") is disabled because the program "
			"was compiled with older GnuTLS version not supporting certificate validation (at least GnuTLS 3.1.4 is required)", OPTION_CERTCHECK);
	}
#endif
#endif

	if (m_certCheck && m_certStore.Empty())
	{
		LocateOptionSrcPos(OPTION_CERTCHECK);
		ConfigError("Option \"%s\" requires proper configuration of option \"%s\"", OPTION_CERTCHECK, OPTION_CERTSTORE);
		m_certCheck = false;
	}

	if (m_rawArticle)
	{
		m_directWrite = false;
	}

	if (m_skipWrite)
	{
		m_directRename = false;
	}

	// if option "ConfigTemplate" is not set, use "WebDir" as default location for template
	// (for compatibility with versions 9 and 10).
	if (m_configTemplate.Empty() && !m_noDiskAccess)
	{
		m_configTemplate.Format("%s%s", *m_webDir, "nzbget.conf");
		if (!FileSystem::FileExists(m_configTemplate))
		{
			m_configTemplate = "";
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
		m_articleCache = 1500;
		m_parBuffer = 400;
	}

	if (!m_unpackPassFile.Empty() && !FileSystem::FileExists(m_unpackPassFile))
	{
		ConfigError("Invalid value for option \"UnpackPassFile\": %s. File not found", *m_unpackPassFile);
	}
}

void Options::ConvertOldOptions(OptEntries* optEntries)
{
	MergeOldScriptOption(optEntries, OPTION_SCANSCRIPT, true);
	MergeOldScriptOption(optEntries, OPTION_QUEUESCRIPT, true);
	MergeOldScriptOption(optEntries, OPTION_FEEDSCRIPT, false);
}

void Options::MergeOldScriptOption(OptEntries* optEntries, const char* optname, bool mergeCategories)
{
	OptEntry* optEntry = optEntries->FindOption(optname);
	if (!optEntry || Util::EmptyStr(optEntry->GetValue()))
	{
		return;
	}

	OptEntry* extensionsOpt = optEntries->FindOption(OPTION_EXTENSIONS);
	if (!extensionsOpt)
	{
		optEntries->emplace_back(OPTION_EXTENSIONS, "");
		extensionsOpt = optEntries->FindOption(OPTION_EXTENSIONS);
	}

	const char* scriptList = optEntry->GetValue();

	Tokenizer tok(scriptList, ",;");
	while (const char* scriptName = tok.Next())
	{
		// merge into global "Extensions"
		if (!HasScript(extensionsOpt->m_value, scriptName))
		{
			if (!extensionsOpt->m_value.Empty())
			{
				extensionsOpt->m_value.Append(",");
			}
			extensionsOpt->m_value.Append(scriptName);
		}

		// merge into categories' "Extensions" (if not empty)
		if (mergeCategories)
		{
			for (OptEntry& opt : optEntries)
			{
				const char* catoptname = opt.GetName();
				if (!strncasecmp(catoptname, "category", 8))
				{
					char* p = (char*)catoptname + 8;
					while (*p >= '0' && *p <= '9') p++;
					if (p && (!strcasecmp(p, ".extensions")))
					{
						if (!opt.m_value.Empty() && !HasScript(opt.m_value, scriptName))
						{
							opt.m_value.Append(",");
							opt.m_value.Append(scriptName);
						}
					}
				}
			}
		}
	}
}

bool Options::HasScript(const char* scriptList, const char* scriptName)
{
	Tokenizer tok(scriptList, ",;");
	while (const char* scriptName2 = tok.Next())
	{
		if (!strcasecmp(scriptName2, scriptName))
		{
			return true;
		}
	}
	return false;
};
