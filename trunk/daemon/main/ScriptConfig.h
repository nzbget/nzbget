/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2013-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef SCRIPTCONFIG_H
#define SCRIPTCONFIG_H

#include <vector>
#include <list>
#include <time.h>

#include "Options.h"

class ScriptConfig
{
public:
	class Script
	{
	private:
		char*			m_szName;
		char*			m_szLocation;
		char*			m_szDisplayName;
		bool			m_bPostScript;
		bool			m_bScanScript;
		bool			m_bQueueScript;
		bool			m_bSchedulerScript;
		char*			m_szQueueEvents;

	public:
						Script(const char* szName, const char* szLocation);
						~Script();
		const char*		GetName() { return m_szName; }
		const char*		GetLocation() { return m_szLocation; }
		void			SetDisplayName(const char* szDisplayName);
		const char*		GetDisplayName() { return m_szDisplayName; }
		bool			GetPostScript() { return m_bPostScript; }
		void			SetPostScript(bool bPostScript) { m_bPostScript = bPostScript; }
		bool			GetScanScript() { return m_bScanScript; }
		void			SetScanScript(bool bScanScript) { m_bScanScript = bScanScript; }
		bool			GetQueueScript() { return m_bQueueScript; }
		void			SetQueueScript(bool bQueueScript) { m_bQueueScript = bQueueScript; }
		bool			GetSchedulerScript() { return m_bSchedulerScript; }
		void			SetSchedulerScript(bool bSchedulerScript) { m_bSchedulerScript = bSchedulerScript; }
		void			SetQueueEvents(const char* szQueueEvents);
		const char*		GetQueueEvents() { return m_szQueueEvents; }
	};

	typedef std::list<Script*>  ScriptsBase;

	class Scripts: public ScriptsBase
	{
	public:
						~Scripts();
		void			Clear();
		Script*			Find(const char* szName);	
	};

	class ConfigTemplate
	{
	private:
		Script*			m_pScript;
		char*			m_szTemplate;

		friend class Options;

	public:
						ConfigTemplate(Script* pScript, const char* szTemplate);
						~ConfigTemplate();
		Script*			GetScript() { return m_pScript; }
		const char*		GetTemplate() { return m_szTemplate; }
	};
	
	typedef std::vector<ConfigTemplate*>  ConfigTemplatesBase;

	class ConfigTemplates: public ConfigTemplatesBase
	{
	public:
						~ConfigTemplates();
	};

private:
	Scripts				m_Scripts;
	ConfigTemplates		m_ConfigTemplates;

	void				InitScripts();
	void				InitConfigTemplates();
	static bool			CompareScripts(Script* pScript1, Script* pScript2);
	void				LoadScriptDir(Scripts* pScripts, const char* szDirectory, bool bIsSubDir);
	void				BuildScriptDisplayNames(Scripts* pScripts);
	void				LoadScripts(Scripts* pScripts);

public:
						ScriptConfig();
						~ScriptConfig();
	Scripts*			GetScripts() { return &m_Scripts; }
	bool				LoadConfig(Options::OptEntries* pOptEntries);
	bool				SaveConfig(Options::OptEntries* pOptEntries);
	bool				LoadConfigTemplates(ConfigTemplates* pConfigTemplates);
	ConfigTemplates*	GetConfigTemplates() { return &m_ConfigTemplates; }
};

extern ScriptConfig* g_pScriptConfig;

#endif
