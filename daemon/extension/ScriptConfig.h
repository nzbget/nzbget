/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2013-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef SCRIPTCONFIG_H
#define SCRIPTCONFIG_H

#include "NString.h"
#include "Container.h"
#include "Options.h"

class ScriptConfig
{
public:
	class Script
	{
	public:
		Script(const char* name, const char* location) :
			m_name(name), m_location(location), m_displayName(name) {};
		Script(Script&&) = default;
		const char* GetName() { return m_name; }
		const char* GetLocation() { return m_location; }
		void SetDisplayName(const char* displayName) { m_displayName = displayName; }
		const char* GetDisplayName() { return m_displayName; }
		bool GetPostScript() { return m_postScript; }
		void SetPostScript(bool postScript) { m_postScript = postScript; }
		bool GetScanScript() { return m_scanScript; }
		void SetScanScript(bool scanScript) { m_scanScript = scanScript; }
		bool GetQueueScript() { return m_queueScript; }
		void SetQueueScript(bool queueScript) { m_queueScript = queueScript; }
		bool GetSchedulerScript() { return m_schedulerScript; }
		void SetSchedulerScript(bool schedulerScript) { m_schedulerScript = schedulerScript; }
		bool GetFeedScript() { return m_feedScript; }
		void SetFeedScript(bool feedScript) { m_feedScript = feedScript; }
		void SetQueueEvents(const char* queueEvents) { m_queueEvents = queueEvents; }
		const char* GetQueueEvents() { return m_queueEvents; }
		void SetTaskTime(const char* taskTime) { m_taskTime = taskTime; }
		const char* GetTaskTime() { return m_taskTime; }

	private:
		CString m_name;
		CString m_location;
		CString m_displayName;
		bool m_postScript = false;
		bool m_scanScript = false;
		bool m_queueScript = false;
		bool m_schedulerScript = false;
		bool m_feedScript = false;
		CString m_queueEvents;
		CString m_taskTime;
	};

	typedef std::list<Script> Scripts;

	class ConfigTemplate
	{
	public:
		ConfigTemplate(Script&& script, const char* templ) :
			m_script(std::move(script)), m_template(templ) {}
		Script* GetScript() { return &m_script; }
		const char* GetTemplate() { return m_template; }

	private:
		Script m_script;
		CString m_template;
	};

	typedef std::deque<ConfigTemplate> ConfigTemplates;

	void InitOptions();
	Scripts* GetScripts() { return &m_scripts; }
	bool LoadConfig(Options::OptEntries* optEntries);
	bool SaveConfig(Options::OptEntries* optEntries);
	bool LoadConfigTemplates(ConfigTemplates* configTemplates);
	ConfigTemplates* GetConfigTemplates() { return &m_configTemplates; }

private:
	Scripts m_scripts;
	ConfigTemplates m_configTemplates;

	void InitScripts();
	void InitConfigTemplates();
	void CreateTasks();
	void LoadScriptDir(Scripts* scripts, const char* directory, bool isSubDir);
	void BuildScriptDisplayNames(Scripts* scripts);
	void LoadScripts(Scripts* scripts);
	bool LoadScriptFile(Script* script);
	BString<1024>BuildScriptName(const char* directory, const char* filename, bool isSubDir);
	bool ScriptExists(Scripts* scripts, const char* scriptName);
};

extern ScriptConfig* g_ScriptConfig;

#endif
