/*
 * This file is part of nzbget. See <http://nzbget.net>.
 *
 * Copyright (C) 2012-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * In this module:
 *   1) Loading of program options and post-processing scripts options;
 *   2) Settings tab;
 *   3) Function "Reload".
 */

/*** OPTIONS AND CONFIGS (FROM CONFIG FILES) **************************************/

var Options = (new function($)
{
	'use strict';

	// Properties (public)
	this.options;
	this.postParamConfig = [];
	this.categories = [];
	this.restricted = false;

	// State
	var _this = this;
	var serverTemplateData = null;
	var serverValues;
	var loadComplete;
	var loadConfigError;
	var loadServerTemplateError;
	var shortScriptNames = [];

	var HIDDEN_SECTIONS = ['DISPLAY (TERMINAL)', 'POSTPROCESSING-PARAMETERS', 'POST-PROCESSING-PARAMETERS', 'POST-PROCESSING PARAMETERS'];
	var POSTPARAM_SECTIONS = ['POSTPROCESSING-PARAMETERS', 'POST-PROCESSING-PARAMETERS', 'POST-PROCESSING PARAMETERS'];

	this.init = function()
	{
	}

	this.update = function()
	{
		// RPC-function "config" returns CURRENT configuration settings loaded in NZBGet
		RPC.call('config', [], function(_options)
			{
				_this.options = _options;
				initCategories();
				_this.restricted = _this.option('ControlPort') === '***';
				RPC.next();
			});

		// loading config templates and build list of post-processing parameters
		_this.postParamConfig = [];
		RPC.call('configtemplates', [false], function(data)
			{
				initPostParamConfig(data);
				RPC.next();
			});
	}

	this.cleanup = function()
	{
		serverTemplateData = null;
		serverValues = null;
	}

	this.option = function(name)
	{
		var opt = findOption(this.options, name);
		return opt ? opt.Value : null;
	}

	function initCategories()
	{
		_this.categories = [];
		for (var i=0; i < _this.options.length; i++)
		{
			var option = _this.options[i];
			if ((option.Name.toLowerCase().substring(0, 8) === 'category') && (option.Name.toLowerCase().indexOf('.name') > -1))
			{
				_this.categories.push(option.Value);
			}
		}
	}

	/*** LOADING CONFIG ********************************************************************/

	this.loadConfig = function(callbacks)
	{
		loadComplete = callbacks.complete;
		loadConfigError = callbacks.configError;
		loadServerTemplateError = callbacks.serverTemplateError;

		// RPC-function "loadconfig" reads the configuration settings from NZBGet configuration file.
		// that's not neccessary the same settings returned by RPC-function "config". This could be the case,
		// for example, if the settings were modified but NZBGet was not restarted.
		RPC.call('loadconfig', [], serverValuesLoaded, loadConfigError);
	}

	function serverValuesLoaded(data)
	{
		serverValues = data;
		RPC.call('configtemplates', [true], serverTemplateLoaded, loadServerTemplateError);
	}

	function serverTemplateLoaded(data)
	{
		serverTemplateData = data;
		complete();
	}

	function complete()
	{
		initShortScriptNames(serverTemplateData);

		if (serverTemplateData === null)
		{
			// the loading was cancelled and the data was discarded (via method "cleanup()")
			return;
		}

		var config = [];
		config.values = serverValues;

		readWebSettings(config);

		var serverConfig = readConfigTemplate(serverTemplateData[0].Template, undefined, HIDDEN_SECTIONS, '');
		mergeValues(serverConfig.sections, serverValues);
		config.push(serverConfig);

		// read scripts configs
		for (var i=1; i < serverTemplateData.length; i++)
		{
			var scriptName = serverTemplateData[i].Name;
			var scriptConfig = readConfigTemplate(serverTemplateData[i].Template, undefined, HIDDEN_SECTIONS, scriptName + ':');
			scriptConfig.scriptName = scriptName;
			scriptConfig.id = Util.makeId(scriptName);
			scriptConfig.name = scriptName.substr(0, scriptName.lastIndexOf('.')) || scriptName; // remove file extension
			scriptConfig.name = scriptConfig.name.replace(/\\/, ' \\ ').replace(/\//, ' / ');
			scriptConfig.shortName = shortScriptName(scriptName);
			scriptConfig.shortName = scriptConfig.shortName.replace(/\\/, ' \\ ').replace(/\//, ' / ');
			scriptConfig.post = serverTemplateData[i].PostScript;
			scriptConfig.scan = serverTemplateData[i].ScanScript;
			scriptConfig.queue = serverTemplateData[i].QueueScript;
			scriptConfig.scheduler = serverTemplateData[i].SchedulerScript;
			scriptConfig.defscheduler = serverTemplateData[i].TaskTime !== '';
			scriptConfig.feed = serverTemplateData[i].FeedScript;
			mergeValues(scriptConfig.sections, serverValues);
			config.push(scriptConfig);
		}

		serverValues = null;
		loadComplete(config);
	}

	function readWebSettings(config)
	{
		var webTemplate = '### WEB-INTERFACE ###\n\n';
		var webValues = [];

		for (var optname in UISettings.description)
		{
			var descript = UISettings.description[optname];
			var value = UISettings[optname];
			optname = optname[0].toUpperCase() + optname.substring(1);
			if (value === true) value = 'yes';
			if (value === false) value = 'no';

			descript = descript.replace(/\n/g, '\n# ').replace(/\n# \n/g, '\n#\n');
			webTemplate += '# ' + descript + '\n' + optname + '=' + value + '\n\n';

			webValues.push({Name: optname, Value: value.toString()});
		}

		var webConfig = readConfigTemplate(webTemplate, undefined, '', '');
		mergeValues(webConfig.sections, webValues);
		config.push(webConfig);
	}

	this.reloadConfig = function(_serverValues, _complete)
	{
		loadComplete = _complete;
		serverValues = _serverValues;
		complete();
	}

	/*** PARSE CONFIG AND BUILD INTERNAL STRUCTURES **********************************************/

	function readConfigTemplate(filedata, visiblesections, hiddensections, nameprefix)
	{
		var config = { description: '', nameprefix: nameprefix, sections: [] };
		var section = null;
		var description = '';
		var firstdescrline = '';

		var data = filedata.split('\n');
		for (var i=0, len=data.length; i < len; i++)
		{
			var line = data[i].replace(/\r+$/,''); // remove possible trailing CR-characters

			if (line.substring(0, 4) === '### ')
			{
				var section = {};
				section.name = line.substr(4, line.length - 8).trim();
				section.id = Util.makeId(nameprefix + section.name);
				section.options = [];
				description = '';
				section.hidden = !(hiddensections === undefined || (hiddensections.indexOf(section.name) == -1)) ||
					(visiblesections !== undefined && (visiblesections.indexOf(section.name) == -1));
				section.postparam = POSTPARAM_SECTIONS.indexOf(section.name) > -1;
				config.sections.push(section);
			}
			else if (line.substring(0, 2) === '# ' || line === '#')
			{
				if (description !== '')
				{
					description += ' ';
				}
				if (line[2] === ' ' && line[3] !== ' ' && description.substring(description.length-4, 4) != '\n \n ')
				{
					description += '\n';
				}
				description += line.substr(1, 10000).trim();
				var lastchar = description.substr(description.length - 1, 1);
				if (lastchar === '.' && firstdescrline === '')
				{
					firstdescrline = description;
					description = '';
				}
				if ('.;:'.indexOf(lastchar) > -1 || line === '#')
				{
					description += '\n';
				}
			}
			else if (line.indexOf('=') > -1 || line.indexOf('@') > -1)
			{
				if (!section)
				{
					// bad template file; create default section.
					section = {};
					section.name = 'OPTIONS';
					section.id = Util.makeId(nameprefix + section.name);
					section.options = [];
					description = '';
					config.sections.push(section);
				}

				var option = {};
				var enabled = line.substr(0, 1) !== '#';
				var command = line.indexOf('=') === -1 && line.indexOf('@') > -1;
				option.caption = line.substr(enabled ? 0 : 1, line.indexOf(command ? '@' : '=') - (enabled ? 0 : 1)).trim();
				if (command)
				{
					var optpos = option.caption.indexOf('[');
					option.commandopts = optpos > -1 ? option.caption.substring(optpos + 1, option.caption.indexOf(']')).toLowerCase() : 'settings';
					option.caption = optpos > -1 ? option.caption.substring(0, optpos) : option.caption;
				}
				option.name = (nameprefix != '' ? nameprefix : '') + option.caption;
				option.defvalue = line.substr(line.indexOf(command ? '@' : '=') + 1, 1000).trim();
				option.value = null;
				option.sectionId = section.id;
				option.select = [];

				var pstart = firstdescrline.lastIndexOf('(');
				var pend = firstdescrline.lastIndexOf(')');
				if (pstart > -1 && pend > -1 && pend === firstdescrline.length - 2)
				{
					var paramstr = firstdescrline.substr(pstart + 1, pend - pstart - 1);
					var params = paramstr.split(',');
					for (var pj=0; pj < params.length; pj++)
					{
						option.select.push(params[pj].trim());
					}
					firstdescrline = firstdescrline.substr(0, pstart).trim() + '.';
				}

				if (option.name.substr(nameprefix.length, 1000).indexOf('1.') > -1)
				{
					section.multi = true;
					section.multiprefix = option.name.substr(0, option.name.indexOf('1.'));
				}

				if (!section.multi || option.name.indexOf('1.') > -1)
				{
					section.options.push(option);
				}

				if (section.multi)
				{
					option.template = true;
				}

				option.description = firstdescrline + description;
				description = '';
				firstdescrline = '';
			}
			else
			{
				if (!section && firstdescrline !== '')
				{
					config.description = firstdescrline + description;
				}
				else if (section && section.options.length === 0)
				{
					section.description = firstdescrline + description;
				}
				description = '';
				firstdescrline = '';
			}
		}

		return config;
	}

	function findOption(options, name)
	{
		if (!options)
		{
			return null;
		}

		name = name.toLowerCase();

		for (var i=0; i < options.length; i++)
		{
			var option = options[i];
			if ((option.Name && option.Name.toLowerCase() === name) ||
				(option.name && option.name.toLowerCase() === name))
			{
				return option;
			}
		}

		return null;
	}
	this.findOption = findOption;

	function mergeValues(config, values)
	{
		// copy values
		for (var i=0; i < config.length; i++)
		{
			var section = config[i];
			if (section.multi)
			{
				// multi sections (news-servers, scheduler)

				var subexists = true;
				for (var k=1; subexists; k++)
				{
					subexists = false;
					for (var m=0; m < section.options.length; m++)
					{
						var option = section.options[m];
						if (option.name.indexOf('1.') > -1)
						{
							var name = option.name.replace(/1/, k);
							var val = findOption(values, name);
							if (val)
							{
								subexists = true;
								break;
							}
						}
					}
					if (subexists)
					{
						for (var m=0; m < section.options.length; m++)
						{
							var option = section.options[m];
							if (option.template)
							{
								var name = option.name.replace(/1/, k);
								// copy option
								var newoption = $.extend({}, option);
								newoption.name = name;
								newoption.caption = option.caption.replace(/1/, k);
								newoption.template = false;
								newoption.multiid = k;
								newoption.value = null;
								section.options.push(newoption);
								var val = findOption(values, name);
								if (val)
								{
									newoption.value = val.Value;
								}
							}
						}
					}
				}
			}
			else
			{
				// simple sections

				for (var j=0; j < section.options.length; j++)
				{
					var option = section.options[j];
					option.value = null;
					var val = findOption(values, option.name);
					if (val && !option.commandopts)
					{
						option.value = val.Value;
					}
				}
			}
		}
	}
	this.mergeValues = mergeValues;

	function initShortScriptNames(configTemplatesData)
	{
		for (var i=1; i < configTemplatesData.length; i++)
		{
			shortScriptNames[configTemplatesData[i].Name] = configTemplatesData[i].DisplayName;
		}
	}

	function shortScriptName(scriptName)
	{
		var shortName = shortScriptNames[scriptName];
		return shortName ? shortName : scriptName;
	}
	this.shortScriptName = shortScriptName;

	function initPostParamConfig(data)
	{
		initShortScriptNames(data);

		// Create one big post-param section. It consists of one item for every post-processing script
		// and additionally includes all post-param options from post-param section of each script.

		var section = {};
		section.id = 'PP-Parameters';
		section.options = [];
		section.description = '';
		section.hidden = false;
		section.postparam = true;
		_this.postParamConfig = [section];

		for (var i=1; i < data.length; i++)
		{
			if (data[i].PostScript || data[i].QueueScript)
			{
				var scriptName = data[i].Name;
				var sectionId = Util.makeId(scriptName + ':');
				var option = {};
				option.name = scriptName + ':';
				option.caption = shortScriptName(scriptName);
				option.caption = option.caption.replace(/\\/, ' \\ ').replace(/\//, ' / ');

				option.defvalue = 'no';
				option.description = (data[i].Template.trim().split('\n')[0].substr(1, 1000).trim() || 'Extension script ' + scriptName + '.');
				option.value = null;
				option.sectionId = sectionId;
				option.select = ['yes', 'no'];
				section.options.push(option);

				var templateData = data[i].Template;
				var postConfig = readConfigTemplate(templateData, POSTPARAM_SECTIONS, undefined, scriptName + ':');
				for (var j=0; j < postConfig.sections.length; j++)
				{
					var sec = postConfig.sections[j];
					if (!sec.hidden)
					{
						for (var n=0; n < sec.options.length; n++)
						{
							var option = sec.options[n];
							option.sectionId = sectionId;
							section.options.push(option);
						}
					}
				}
			}
		}
	}
}(jQuery));


/*** SETTINGS TAB (UI) *********************************************************/

var Config = (new function($)
{
	'use strict';

	// Controls
	var $ConfigNav;
	var $ConfigData;
	var $ConfigTabBadge;
	var $ConfigTabBadgeEmpty;
	var $ConfigContent;
	var $ConfigInfo;
	var $ConfigTitle;
	var $ConfigTable;
	var $ViewButton;
	var $LeaveConfigDialog;
	var $Body;

	// State
	var config = null;
	var filterText = '';
	var lastSection;
	var reloadTime;
	var updateTabInfo;
	var restored = false;
	var compactMode = false;
	var configSaved = false;
	var leaveTarget;

	this.init = function(options)
	{
		updateTabInfo = options.updateTabInfo;

		$Body = $('html, body');
		$ConfigNav = $('#ConfigNav');
		$ConfigData = $('#ConfigData');
		$ConfigTabBadge = $('#ConfigTabBadge');
		$ConfigTabBadgeEmpty = $('#ConfigTabBadgeEmpty');
		$ConfigContent = $('#ConfigContent');
		$ConfigInfo = $('#ConfigInfo');
		$ConfigTitle = $('#ConfigTitle');
		$ViewButton = $('#Config_ViewButton');
		$LeaveConfigDialog = $('#LeaveConfigDialog');
		$('#ConfigTable_filter').val('');

		Util.show('#ConfigBackupSafariNote', $.browser.safari);
		$('#ConfigTable_filter').val('');
		compactMode = UISettings.read('$Config_ViewCompact', 'no') == 'yes';
		setViewMode();

		$(window).bind('beforeunload', userLeavesPage);

		$ConfigNav.on('click', 'li > a', navClick);

		$ConfigTable = $('#ConfigTable');
		$ConfigTable.fasttable(
			{
				filterInput: $('#ConfigTable_filter'),
				filterClearButton: $("#ConfigTable_clearfilter"),
				filterInputCallback: filterInput,
				filterClearCallback: filterClear
			});
	}

	this.config = function()
	{
		return config;
	}

	this.show = function()
	{
		removeSaveBanner();
		$('#ConfigSaved').hide();
		$('#ConfigLoadInfo').show();
		$('#ConfigLoadServerTemplateError').hide();
		$('#ConfigLoadError').hide();
		$ConfigContent.hide();
		configSaved = false;
	}

	this.shown = function()
	{
		Options.loadConfig({
			complete: buildPage,
			configError: loadConfigError,
			serverTemplateError: loadServerTemplateError
			});
	}

	this.hide = function()
	{
		Options.cleanup();
		config = null;
		$ConfigNav.children().not('.config-static').remove();
		$ConfigData.children().not('.config-static').remove();
	}

	function loadConfigError(message, resultObj)
	{
		$('#ConfigLoadInfo').hide();
		$('#ConfigLoadError').show();
		if (resultObj && resultObj.error && resultObj.error.message)
		{
			message = resultObj.error.message;
		}
		$('#ConfigLoadErrorText').text(message);
	}

	function loadServerTemplateError()
	{
		$('#ConfigLoadInfo').hide();
		$('#ConfigLoadServerTemplateError').show();
		var optConfigTemplate = Options.option('ConfigTemplate');
		$('#ConfigLoadServerTemplateErrorEmpty').toggle(optConfigTemplate === '');
		$('#ConfigLoadServerTemplateErrorNotFound').toggle(optConfigTemplate !== '');
		$('#ConfigLoadServerTemplateErrorWebDir').text(Options.option('WebDir'));
		$('#ConfigLoadServerTemplateErrorConfigFile').text(Options.option('ConfigFile'));
	}

	function findOptionByName(name)
	{
		name = name.toLowerCase();

		for (var k=0; k < config.length; k++)
		{
			var sections = config[k].sections;
			for (var i=0; i < sections.length; i++)
			{
				var section = sections[i];
				for (var j=0; j < section.options.length; j++)
				{
					var option = section.options[j];
					if (!option.template &&
						((option.Name && option.Name.toLowerCase() === name) ||
						 (option.name && option.name.toLowerCase() === name)))
					{
						return option;
					}
				}
			}
		}
		return null;
	}
	this.findOptionByName = findOptionByName;

	function findOptionById(formId)
	{
		for (var k=0; k < config.length; k++)
		{
			var sections = config[k].sections;
			for (var i=0; i < sections.length; i++)
			{
				var section = sections[i];
				for (var j=0; j < section.options.length; j++)
				{
					var option = section.options[j];
					if (option.formId === formId)
					{
						return option;
					}
				}
			}
		}
		return null;
	}

	function findSectionById(sectionId)
	{
		for (var k=0; k < config.length; k++)
		{
			var sections = config[k].sections;
			for (var i=0; i < sections.length; i++)
			{
				var section = sections[i];
				if (section.id === sectionId)
				{
					return section;
				}
			}
		}
		return null;
	}

	this.processShortcut = function(key)
	{
		switch (key)
		{
			case 'Shift+F': $('#ConfigTable_filter').focus(); return true;
			case 'Shift+C': $('#ConfigTable_clearfilter').click(); return true;
		}
	}

	/*** GENERATE HTML PAGE *****************************************************************/

	function buildOptionsContent(section, extensionsec)
	{
		var html = '';

		var lastmultiid = 1;
		var firstmultioption = true;
		var hasoptions = false;

		for (var i=0, op=0; i < section.options.length; i++)
		{
			if (i > 0 && extensionsec && Options.restricted)
			{
				// in restricted mode don't show any options for extension scripts,
				// option's content is hidden content anyway (***)
				break;
			}

			var option = section.options[i];
			if (!option.template)
			{
				if (section.multi && option.multiid !== lastmultiid)
				{
					// new set in multi section
					html += buildMultiRowEnd(section, lastmultiid, true, true);
					lastmultiid = option.multiid;
					firstmultioption = true;
				}
				if (section.multi && firstmultioption)
				{
					html += buildMultiRowStart(section, option.multiid, option);
					firstmultioption = false;
				}
				html += buildOptionRow(option, section);
				hasoptions = true;
				op++;
			}
		}

		if (section.multi)
		{
			html += buildMultiRowEnd(section, lastmultiid, false, hasoptions);
		}

		return html;
	}
	this.buildOptionsContent = buildOptionsContent;

	function buildMultiSetContent(section, multiid)
	{
		var html = '';
		var firstmultioption = true;
		var hasoptions = false;

		for (var i=0, op=0; i < section.options.length; i++)
		{
			var option = section.options[i];
			if (!option.template && option.multiid === multiid)
			{
				if (firstmultioption)
				{
					html += buildMultiRowStart(section, multiid, option);
					firstmultioption = false;
				}
				html += buildOptionRow(option, section);
				hasoptions = true;
				op++;
			}
		}
		html += buildMultiRowEnd(section, multiid, true, hasoptions);

		return html;
	}

	function buildOptionRow(option, section)
	{
		var value = option.value;
		if (option.value === null)
		{
			value = option.defvalue;
		}

		option.formId = (option.name.indexOf(':') == -1 ? 'S_' : '') + Util.makeId(option.name);

		var caption = option.caption;
		if (section.multi)
		{
			caption = '<span class="config-multicaption">' + caption.substring(0, caption.indexOf('.') + 1) + '</span>' + caption.substring(caption.indexOf('.') + 1);
		}

		var html =
			'<div class="control-group ' + option.sectionId + (section.multi ? ' multiid' + option.multiid + ' multiset' : '') + '">'+
				'<label class="control-label">' +
				'<a class="option-name" href="#" data-optid="' + option.formId + '" '+
				'onclick="Config.scrollToOption(event, this)">' + caption + '</a>' +
				(option.value === null && !section.postparam && !option.commandopts ?
					' <a data-toggle="modal" href="#ConfigNewOptionHelp" class="label label-info">new</a>' : '') + '</label>'+
				'<div class="controls">';

		if (option.nocontent)
		{
			option.type = 'info';
			html +=	'<div class="" id="' + option.formId + '"/>';
		}
		else if (option.select.length > 1)
		{
			option.type = 'switch';
			html +=	'<div class="btn-group btn-switch" id="' + option.formId + '">';

			var valfound = false;
			for (var j=0; j < option.select.length; j++)
			{
				var pvalue = option.select[j];
				if (value && pvalue.toLowerCase() === value.toLowerCase())
				{
					html += '<input type="button" class="btn btn-primary" value="' + Util.textToAttr(pvalue) + '" onclick="Config.switchClick(this)">';
					valfound = true;
				}
				else
				{
					html += '<input type="button" class="btn" value="' + Util.textToAttr(pvalue) + '" onclick="Config.switchClick(this)">';
				}
			}
			if (!valfound)
			{
				html += '<input type="button" class="btn btn-primary" value="' + Util.textToAttr(value) + '" onclick="Config.switchClick(this)">';
			}

			html +='</div>';

		}
		else if (option.select.length === 1)
		{
			option.type = 'numeric';
			html += '<div class="input-append">'+
				'<input type="text" id="' + option.formId + '" value="' + Util.textToAttr(value) + '" class="editnumeric">'+
				'<span class="add-on">'+ option.select[0] +'</span>'+
				'</div>';
		}
		else if (option.caption.toLowerCase().indexOf('password') > -1 &&
			option.name.toLowerCase() !== '*unpack:password')
		{
			option.type = 'password';
			html += '<div class="password-field input-append">' +
				'<input type="password" id="' + option.formId + '" value="' + Util.textToAttr(value) + '" class="editsmall">'+
				'<span class="add-on">'+
				'<label class="checkbox">'+
				'<input type="checkbox" onclick="Config.togglePassword(this, \'' + option.formId + '\')" /> Show'+
				'</label>'+
				'</span>'+
				'</div>';
		}
		else if (option.caption.toLowerCase().indexOf('username') > -1 ||
			(option.caption.indexOf('IP') > -1 && option.name.toLowerCase() !== 'authorizedip'))
		{
			option.type = 'text';
			html += '<input type="text" id="' + option.formId + '" value="' + Util.textToAttr(value) + '" class="editsmall">';
		}
		else if (option.editor)
		{
			option.type = 'text';
			html += '<table class="editor"><tr><td>';
			html += '<input type="text" id="' + option.formId + '" value="' + Util.textToAttr(value) + '">';
			html += '</td><td>';
			html += '<button type="button" id="' + option.formId + '_Editor" class="btn" onclick="' + option.editor.click + '($(\'input\', $(this).closest(\'table\')).attr(\'id\'))">' + option.editor.caption + '</button>';
			html += '</td></tr></table>';
		}
		else if (option.commandopts)
		{
			option.type = 'command';
			html += '<button type="button" id="' + option.formId + '" class="btn ' + 
				(option.commandopts.indexOf('danger') > -1 ? 'btn-danger' : 'btn-inverse') + 
				'" onclick="Config.commandClick(this)">' + value +  '</button>';
		}
		else
		{
			option.type = 'text';
			html += '<input type="text" id="' + option.formId + '" value="' + Util.textToAttr(value) + '" class="editlarge">';
		}

		if (option.description !== '')
		{
			var htmldescr = option.description;
			htmldescr = htmldescr.replace(/NOTE: do not forget to uncomment the next line.\n/, '');

			// replace option references
			var exp = /\<([A-Z0-9\.]*)\>/ig;
			htmldescr = htmldescr.replace(exp, '<a class="option" href="#" onclick="Config.scrollToOption(event, this)">$1</a>');

			htmldescr = htmldescr.replace(/&/g, '&amp;');

			// add extra new line after Examples not ended with dot
			htmldescr = htmldescr.replace(/Example:.*/g, function (match) {
				return match + (Util.endsWith(match, '.') ? '' : '\n');
			});

			// replace URLs
			exp = /(http:\/\/[-A-Z0-9+&@#\/%?=~_|!:,.;]*[-A-Z0-9+&@#\/%=~_|])/ig;
			htmldescr = htmldescr.replace(exp, "<a href='$1'>$1</a>");

			// highlight first line
			htmldescr = htmldescr.replace(/\n/, '</span>\n');
			htmldescr = '<span class="help-option-title">' + htmldescr;

			htmldescr = htmldescr.replace(/\n/g, '<br>');
			htmldescr = htmldescr.replace(/NOTE: /g, '<span class="label label-warning">NOTE:</span> ');
			htmldescr = htmldescr.replace(/INFO: /g, '<span class="label label-info">INFO:</span> ');

			if (htmldescr.indexOf('INFO FOR DEVELOPERS:') > -1)
			{
				htmldescr = htmldescr.replace(/INFO FOR DEVELOPERS:<br>/g, '<input class="btn btn-mini" value="Show more info for developers" type="button" onclick="Config.showSpoiler(this)"><span class="hide">');
				htmldescr += '</span>';
			}

			if (htmldescr.indexOf('MORE INFO:') > -1)
			{
				htmldescr = htmldescr.replace(/MORE INFO:<br>/g, '<input class="btn btn-mini" value="Show more info" type="button" onclick="Config.showSpoiler(this)"><span class="hide">');
				htmldescr += '</span>';
			}

			if (section.multi)
			{
				// replace strings like "TaskX.Command" and "Task1.Command"
				htmldescr = htmldescr.replace(new RegExp(section.multiprefix + '[X|1]\.', 'g'), '');
			}

			html += '<p class="help-block">' + htmldescr + '</p>';
		}

		html += '</div>';
		html += '</div>';

		return html;
	}

	function buildMultiRowStart(section, multiid, option)
	{
		var name = option.caption;
		var setname = name.substr(0, name.indexOf('.'));
		var html = '<div class="config-settitle ' + section.id + ' multiid' + multiid + ' multiset">' + setname + '</div>';
		return html;
	}

	function buildMultiRowEnd(section, multiid, hasmore, hasoptions)
	{
		var name = section.options[0].caption;
		var setname = name.substr(0, name.indexOf('1'));
		var html = '';

		if (hasoptions)
		{
			html += '<div class="' + section.id + ' multiid' + multiid + ' multiset multiset-toolbar">';
			html += '<button type="button" class="btn config-button config-delete" data-multiid="' + multiid + '" ' +
				'onclick="Config.deleteSet(this, \'' + setname + '\',\'' + section.id + '\')">Delete ' + setname + multiid + '</button>';
			html += ' <button type="button" class="btn config-button" data-multiid="' + multiid + '" ' +
				'onclick="Config.moveSet(this, \'' + setname + '\',\'' + section.id + '\', \'up\')">Move Up</button>';
			html += ' <button type="button" class="btn config-button" data-multiid="' + multiid + '" ' +
				'onclick="Config.moveSet(this, \'' + setname + '\',\'' + section.id + '\', \'down\')">Move Down</button>';
			if (setname.toLowerCase() === 'feed')
			{
				html += ' <button type="button" class="btn config-button" data-multiid="' + multiid + '" ' +
					'onclick="Config.previewFeed(this, \'' + setname + '\',\'' + section.id + '\')">Preview Feed</button>';
			}
			if (setname.toLowerCase() === 'server')
			{
				html += ' <button type="button" class="btn config-button" data-multiid="' + multiid + '" ' +
					'onclick="Config.testConnection(this, \'' + setname + '\',\'' + section.id + '\')">Test Connection</button>';
				html += ' <button type="button" class="btn config-button" data-multiid="' + multiid + '" ' +
					'onclick="Config.serverStats(this, \'' + setname + '\',\'' + section.id + '\')">Volume Statistics</button>';
			}
			html += '<hr>';
			html += '</div>';
		}

		if (!hasmore)
		{
			html += '<div class="' + section.id + '">';
			html += '<button type="button" class="btn config-add ' + section.id + ' multiset" onclick="Config.addSet(\'' + setname + '\',\'' + section.id +
			  '\')">Add ' + (hasoptions ? 'another ' : '') + setname + '</button>';
			html += '</div>';
		}

		return html;
	}

	function buildPage(_config)
	{
		config = _config;

		extendConfig();

		$ConfigNav.children().not('.config-static').remove();
		$ConfigData.children().not('.config-static').remove();

		for (var k=0; k < config.length; k++)
		{
			if (k == 1 || k == 2)
			{
				$ConfigNav.append('<li class="divider"></li>');
			}
			var conf = config[k];
			var added = false;
			for (var i=0; i < conf.sections.length; i++)
			{
				var section = conf.sections[i];
				if (!section.hidden)
				{
					var html = $('<li><a href="#' + section.id + '">' + section.name + '</a></li>');
					$ConfigNav.append(html);
					var content = buildOptionsContent(section, k > 1);
					$ConfigData.append(content);
					added = true;
				}
			}
			if (!added)
			{
				var html = $('<li><a href="#' + conf.id + '">' + conf.name + '</a></li>');
				$ConfigNav.append(html);
			}
		}

		notifyChanges();

		$ConfigNav.append('<li class="divider hide ConfigSearch"></li>');
		$ConfigNav.append('<li class="hide ConfigSearch"><a href="#Search">SEARCH RESULTS</a></li>');

		$ConfigNav.toggleClass('long-list', $ConfigNav.children().length > 20);

		showSection('Config-Info', false);

		if (filterText !== '')
		{
			filterInput(filterText);
		}

		$('#ConfigLoadInfo').hide();
		$ConfigContent.show();
	}

	function extendConfig()
	{
		for (var i=2; i < config.length; i++)
		{
			var conf = config[i];

			var firstVisibleSection = null;
			var visibleSections = 0;
			for (var j=0; j < conf.sections.length; j++)
			{
				if (!conf.sections[j].hidden)
				{
					if (!firstVisibleSection)
					{
						firstVisibleSection = conf.sections[j];
					}
					visibleSections++;
				}
			}

			// rename sections
			for (var j=0; j < conf.sections.length; j++)
			{
				var section = conf.sections[j];
				section.name = conf.shortName.toUpperCase() + (visibleSections > 1 ? ' - ' + section.name.toUpperCase() + '' : '');
				section.caption = conf.name.toUpperCase() + (visibleSections > 1 ? ' - ' + section.name.toUpperCase() + '' : '');
			}

			if (!firstVisibleSection)
			{
				// create new section for virtual option "About".
				var section = {};
				section.name = conf.shortName.toUpperCase();
				section.caption = conf.name.toUpperCase();
				section.id = conf.id + '_';
				section.options = [];
				firstVisibleSection = section;
				conf.sections.push(section);
			}

			// create virtual option "About" with scripts description.
			var option = {};
			var shortName = conf.scriptName.replace(/^.*[\\\/]/, ''); // leave only file name (remove path)
			shortName = shortName.substr(0, shortName.lastIndexOf('.')) || shortName; // remove file extension
			option.caption = 'About ' + shortName;
			option.name = conf.nameprefix + option.caption;
			option.value = '';
			option.defvalue = '';
			option.sectionId = firstVisibleSection.id;
			option.select = [];
			var description = conf.description;
			option.description = description !== '' ? description : 'No description available.';
			option.nocontent = true;
			firstVisibleSection.options.unshift(option);
		}

		// register editors for certain options
		var conf = config[1];
		for (var j=0; j < conf.sections.length; j++)
		{
			var section = conf.sections[j];
			for (var k=0; k < section.options.length; k++)
			{
				var option = section.options[k];
				var optname = option.name.toLowerCase();
				if (optname === 'scriptorder')
				{
					option.editor = { caption: 'Reorder', click: 'Config.editScriptOrder' };
				}
				if (optname === 'extensions')
				{
					option.editor = { caption: 'Choose', click: 'Config.editExtensions' };
				}
				if (optname.indexOf('category') > -1 && optname.indexOf('.extensions') > -1)
				{
					option.editor = { caption: 'Choose', click: 'Config.editCategoryExtensions' };
				}
				if (optname.indexOf('feed') > -1 && optname.indexOf('.extensions') > -1)
				{
					option.editor = { caption: 'Choose', click: 'Config.editFeedExtensions' };
				}
				if (optname.indexOf('task') > -1 && optname.indexOf('.param') > -1)
				{
					option.editor = { caption: 'Choose', click: 'Config.editSchedulerScript' };
				}
				if (optname.indexOf('task') > -1 && optname.indexOf('.command') > -1)
				{
					option.onchange = Config.schedulerCommandChanged;
				}
				if (optname.indexOf('.filter') > -1)
				{
					option.editor = { caption: 'Change', click: 'Config.editFilter' };
				}
			}
		}
	}

	function notifyChanges()
	{
		for (var k=0; k < config.length; k++)
		{
			var sections = config[k].sections;
			for (var i=0; i < sections.length; i++)
			{
				var section = sections[i];
				for (var j=0; j < section.options.length; j++)
				{
					var option = section.options[j];
					if (option.onchange && !option.template)
					{
						option.onchange(option);
					}
				}
			}
		}
	}

	function scrollOptionIntoView(optFormId)
	{
		var option = findOptionById(optFormId);

		// switch to tab and scroll the option into view
		showSection(option.sectionId, false);

		var element = $('#' + option.formId);
		var parent = $('html,body');

		parent[0].scrollIntoView(true);
		var offsetY = 15;
		if ($('body').hasClass('navfixed')) {
			offsetY = 55;
		}
		parent.animate({ scrollTop: parent.scrollTop() + element.offset().top - parent.offset().top - offsetY }, { duration: 'slow', easing: 'swing' });
	}

	this.switchClick = function(control)
	{
		$('.btn', $(control).parent()).removeClass('btn-primary');
		$(control).addClass('btn-primary');

		// not for page Postprocess in download details
		if (config)
		{
			var optFormId = $(control).parent().attr('id');
			var option = findOptionById(optFormId);
			if (option.onchange)
			{
				option.onchange(option);
			}
		}
	}

	function switchGetValue(control)
	{
		var state = $('.btn-primary', control).val();
		return state;
	}

	function switchSetValue(control, value)
	{
		$('.btn', control).removeClass('btn-primary');
		$('.btn@[value=' + value + ']', control).addClass('btn-primary');
	}

	this.togglePassword = function(control, target)
	{
		var checked = $(control).is(':checked');
		$('#'+target).prop('type', checked ? 'text' : 'password');
	}

	/*** CHANGE/ADD/REMOVE OPTIONS *************************************************************/

	function navClick(event)
	{
		event.preventDefault();
		var sectionId = $(this).attr('href').substr(1);
		showSection(sectionId, true);
	}

	function showSection(sectionId, animateScroll)
	{
		var link = $('a[href="#' + sectionId + '"]', $ConfigNav);
		$('li', $ConfigNav).removeClass('active');
		link.closest('li').addClass('active');
		$ConfigContent.removeClass('search');
		Util.show($ViewButton, sectionId !== 'Config-Info');

		$ConfigInfo.hide();

		if (sectionId === 'Search')
		{
			search();
			return;
		}

		lastSection = sectionId;

		if (sectionId === 'Config-Info')
		{
			$ConfigInfo.show();
			$ConfigData.children().hide();
			$ConfigTitle.text('INFO');
			return;
		}

		if (sectionId === 'Config-System')
		{
			$ConfigData.children().hide();
			$('.config-system', $ConfigData).show();
			markLastControlGroup();
			$ConfigTitle.text('SYSTEM');
			return;
		}

		$ConfigData.children().hide();
		var opts = $('.' + sectionId, $ConfigData);
		opts.show();
		markLastControlGroup();

		var section = findSectionById(sectionId);
		$ConfigTitle.text(section.caption ? section.caption : section.name);

		$Body.animate({ scrollTop: 0 }, { duration: animateScroll ? 'slow' : 0, easing: 'swing' });
	}

	this.deleteSet = function(control, setname, sectionId)
	{
		var multiid = parseInt($(control).attr('data-multiid'));
		$('#ConfigDeleteConfirmDialog_Option').text(setname + multiid);
		ConfirmDialog.showModal('ConfigDeleteConfirmDialog', function()
		{
			deleteOptionSet(setname, multiid, sectionId);
		});
	}

	function deleteOptionSet(setname, multiid, sectionId)
	{
		// remove options from page, using a temporary div for slide effect
		var opts = $('.' + sectionId + '.multiid' + multiid, $ConfigData);
		var div = $('<div></div>');
		opts.first().before(div);
		div.append(opts);
		div.slideUp('normal', function()
		{
			div.remove();
		});

		// remove option set from config
		var section = findSectionById(sectionId);
		for (var j=0; j < section.options.length; j++)
		{
			var option = section.options[j];
			if (!option.template && option.multiid === multiid)
			{
				section.options.splice(j, 1);
				j--;
			}
		}

		// reformat remaining sets (captions, input IDs, etc.)
		reformatSection(section, setname);

		section.modified = true;
	}

	function reformatSection(section, setname)
	{
		var hasOptions = false;
		var lastMultiId = 0;
		for (var j=0; j < section.options.length; j++)
		{
			var option = section.options[j];
			if (!option.template)
			{
				if (option.multiid !== lastMultiId && option.multiid !== lastMultiId + 1)
				{
					reformatSet(section, setname, option.multiid, lastMultiId + 1);
				}
				lastMultiId = option.multiid;
				hasOptions = true;
			}
		}

		// update add-button
		var addButton = $('.config-add.' + section.id, $ConfigData);
		addButton.text('Add ' + (hasOptions ? 'another ' : '') + setname);
	}

	function reformatSet(section, setname, oldMultiId, newMultiId)
	{
		for (var j=0; j < section.options.length; j++)
		{
			var option = section.options[j];
			if (!option.template && option.multiid == oldMultiId)
			{
				// reformat multiid
				var div = $('#' + setname + oldMultiId);
				div.attr('id', setname + newMultiId);

				// update captions
				$('.config-settitle.' + section.id + '.multiid' + oldMultiId, $ConfigData).text(setname + newMultiId);
				$('.' + section.id + '.multiid' + oldMultiId + ' .config-multicaption', $ConfigData).text(setname + newMultiId + '.');
				$('.' + section.id + '.multiid' + oldMultiId + ' .config-delete', $ConfigData).text('Delete ' + setname + newMultiId);

				//update data id
				$('.' + section.id + '.multiid' + oldMultiId + ' .config-button', $ConfigData).attr('data-multiid', newMultiId);

				//update class
				$('.' + section.id + '.multiid' + oldMultiId, $ConfigData).removeClass('multiid' + oldMultiId).addClass('multiid' + newMultiId);

				// update input id
				var oldFormId = option.formId;
				option.formId = option.formId.replace(new RegExp(option.multiid), newMultiId);
				$('#' + oldFormId).attr('id', option.formId);

				// update label data-optid
				$('a[data-optid=' + oldFormId + ']').attr('data-optid', option.formId);

				// update editor id
				$('#' + oldFormId + '_Editor').attr('id', option.formId + '_Editor');

				// update name
				option.name = option.name.replace(new RegExp(option.multiid), newMultiId);

				option.multiid = newMultiId;
			}
		}
	}

	this.addSet = function(setname, sectionId)
	{
		// find section
		var section = findSectionById(sectionId);

		// find max multiid
		var multiid = 0;
		for (var j=0; j < section.options.length; j++)
		{
			var option = section.options[j];
			if (!option.template && option.multiid > multiid)
			{
				multiid = option.multiid;
			}
		}
		multiid++;

		// create new multi set
		var addedOptions = [];
		for (var j=0; j < section.options.length; j++)
		{
			var option = section.options[j];
			if (option.template)
			{
				var name = option.name.replace(/1/, multiid);
				// copy option
				var newoption = $.extend({}, option);
				newoption.name = name;
				newoption.caption = option.caption.replace(/1/, multiid);
				newoption.template = false;
				newoption.multiid = multiid;
				section.options.push(newoption);
				addedOptions.push(newoption);
			}
		}

		section.modified = true;

		// visualize new multi set
		var html = buildMultiSetContent(section, multiid);

		var addButton = $('.config-add.' + section.id, $ConfigData);
		addButton.text('Add another ' + setname);

		// insert before add-button, using a temporary div for slide effect
		var div = $('<div>' + html + '</div>');
		div.hide();
		addButton.parent().before(div);

		for (var j=0; j < addedOptions.length; j++)
		{
			var option = addedOptions[j];
			if (option.onchange)
			{
				option.onchange(option);
			}
		}

		div.slideDown('normal', function()
		{
			var opts = div.children();
			opts.detach();
			div.after(opts);
			div.remove();
		});
	}

	this.moveSet = function(control, setname, sectionId, direction)
	{
		var id1 = parseInt($(control).attr('data-multiid'));
		var id2 = direction === 'down' ? id1 + 1 : id1 - 1;

		// swap options in two sets
		var opts1 = $('.' + sectionId + '.multiid' + (direction === 'down' ? id1 : id2), $ConfigData);
		var opts2 = $('.' + sectionId + '.multiid' + (direction === 'down' ? id2 : id1), $ConfigData);

		if (opts1.length === 0 || opts2.length === 0)
		{
			return;
		}

		opts1.first().before(opts2);

		// reformat remaining sets (captions, input IDs, etc.)
		var section = findSectionById(sectionId);
		reformatSet(section, setname, id2, 10000 + id2);
		reformatSet(section, setname, id1, id2);
		reformatSet(section, setname, 10000 + id2, id1);

		section.modified = true;
	}

	this.viewMode = function()
	{
		compactMode = !compactMode;
		UISettings.write('$Config_ViewCompact', compactMode ? 'yes' : 'no');
		setViewMode();
	}

	function setViewMode()
	{
		$('#Config_ViewCompact i').toggleClass('icon-ok', compactMode).toggleClass('icon-empty', !compactMode);
		$ConfigContent.toggleClass('hide-help-block', compactMode);
	}

	/*** OPTION SPECIFIC EDITORS *************************************************/

	this.editScriptOrder = function(optFormId)
	{
		var option = findOptionById(optFormId);
		ScriptListDialog.showModal(option, config, null);
	}

	this.editExtensions = function(optFormId)
	{
		var option = findOptionById(optFormId);
		ScriptListDialog.showModal(option, config, ['post', 'scan', 'queue', 'defscheduler']);
	}

	this.editCategoryExtensions = function(optFormId)
	{
		var option = findOptionById(optFormId);
		ScriptListDialog.showModal(option, config, ['post', 'scan', 'queue']);
	}

	this.editFeedExtensions = function(optFormId)
	{
		var option = findOptionById(optFormId);
		ScriptListDialog.showModal(option, config, ['feed']);
	}

	this.editSchedulerScript = function(optFormId)
	{
		var option = findOptionById(optFormId);
		var command = getOptionValue(findOptionById(optFormId.replace(/Param/, 'Command')));
		if (command !== 'Script')
		{
			alert('This button is to choose scheduler scripts when option TaskX.Command is set to "Script".');
			return;
		}
		ScriptListDialog.showModal(option, config, ['scheduler']);
	}

	this.schedulerCommandChanged = function(option)
	{
		var command = getOptionValue(option);
		var btnId = option.formId.replace(/Command/, 'Param_Editor');
		Util.show('#' + btnId, command === 'Script');
	}

	this.commandClick = function(button)
	{
		var optFormId = $(button).attr('id');
		var option = findOptionById(optFormId);
		var script = option.name.substr(0, option.name.indexOf(':'));
		var command = option.name.substr(option.name.indexOf(':') + 1, 1000);
		var changedOptions = prepareSaveRequest(true, false, true);

		function execScript()
		{
			ExecScriptDialog.showModal(script, command, 'SETTINGS', changedOptions);
		}

		if (option.commandopts.indexOf('danger') > -1)
		{
			$('#DangerScriptConfirmDialog_OK').text(option.defvalue);
			$('#DangerScriptConfirmDialog_Command').text(command);
			ConfirmDialog.showModal('DangerScriptConfirmDialog', execScript);
		}
		else
		{
			execScript();
		}
	}

	/*** RSS FEEDS ********************************************************************/

	this.editFilter = function(optFormId)
	{
		var option = findOptionById(optFormId);
		FeedFilterDialog.showModal(
			option.multiid,
			getOptionValue(findOptionByName('Feed' + option.multiid + '.Name')),
			getOptionValue(findOptionByName('Feed' + option.multiid + '.URL')),
			getOptionValue(findOptionByName('Feed' + option.multiid + '.Filter')),
			getOptionValue(findOptionByName('Feed' + option.multiid + '.Backlog')),
			getOptionValue(findOptionByName('Feed' + option.multiid + '.PauseNzb')),
			getOptionValue(findOptionByName('Feed' + option.multiid + '.Category')),
			getOptionValue(findOptionByName('Feed' + option.multiid + '.Priority')),
			getOptionValue(findOptionByName('Feed' + option.multiid + '.Interval')),
			getOptionValue(findOptionByName('Feed' + option.multiid + '.Extensions')),
			function(filter)
				{
					var control = $('#' + option.formId);
					control.val(filter);
				});
	}

	this.previewFeed = function(control, setname, sectionId)
	{
		var multiid = parseInt($(control).attr('data-multiid'));
		FeedDialog.showModal(multiid,
			getOptionValue(findOptionByName('Feed' + multiid + '.Name')),
			getOptionValue(findOptionByName('Feed' + multiid + '.URL')),
			getOptionValue(findOptionByName('Feed' + multiid + '.Filter')),
			getOptionValue(findOptionByName('Feed' + multiid + '.Backlog')),
			getOptionValue(findOptionByName('Feed' + multiid + '.PauseNzb')),
			getOptionValue(findOptionByName('Feed' + multiid + '.Category')),
			getOptionValue(findOptionByName('Feed' + multiid + '.Priority')),
			getOptionValue(findOptionByName('Feed' + multiid + '.Interval')),
			getOptionValue(findOptionByName('Feed' + multiid + '.Extensions')));
	}

	/*** TEST SERVER ********************************************************************/

	var connecting = false;

	this.testConnection = function(control, setname, sectionId)
	{
		if (connecting)
		{
			return;
		}

		connecting = true;
		$('#Notif_Config_TestConnectionProgress').fadeIn(function() {
			var multiid = parseInt($(control).attr('data-multiid'));
			var timeout = Math.min(parseInt(getOptionValue(findOptionByName('ArticleTimeout'))), 10);
			RPC.call('testserver', [
				getOptionValue(findOptionByName('Server' + multiid + '.Host')),
				parseInt(getOptionValue(findOptionByName('Server' + multiid + '.Port'))),
				getOptionValue(findOptionByName('Server' + multiid + '.Username')),
				getOptionValue(findOptionByName('Server' + multiid + '.Password')),
				getOptionValue(findOptionByName('Server' + multiid + '.Encryption')) === 'yes',
				getOptionValue(findOptionByName('Server' + multiid + '.Cipher')),
				timeout
				],
				function(errtext) {
					$('#Notif_Config_TestConnectionProgress').fadeOut(function() {
						if (errtext == '')
						{
							PopupNotification.show('#Notif_Config_TestConnectionOK');
						}
						else
						{
							AlertDialog.showModal('Connection test failed', errtext);
						}
					});
					connecting = false;
				},
				function(message, resultObj) {
					$('#Notif_Config_TestConnectionProgress').fadeOut(function() {
						if (resultObj && resultObj.error && resultObj.error.message)
						{
							message = resultObj.error.message;
						}
						AlertDialog.showModal('Connection test failed', message);
						connecting = false;
					});
				});
		});
	}

	/*** DOWNLOADED VOLUMES FOR A SERVER *******************************************************/

	this.serverStats = function(control, setname, sectionId)
	{
		var multiid = parseInt($(control).attr('data-multiid'));

		// Because the settings page isn't saved yet and user can reorder servers
		// we cannot just use the server-id in teh call to "StatDialog".
		// Instead we need to find the server in loaded options and get its ID from there.

		var serverName = getOptionValue(findOptionByName('Server' + multiid + '.Name'));
		var serverHost = getOptionValue(findOptionByName('Server' + multiid + '.Host'));
		var serverPort = getOptionValue(findOptionByName('Server' + multiid + '.Port'));
		var serverId = 0;

		// searching by name
		var opt = undefined;
		for (var i = 1; opt !== null; i++)
		{
			var opt = Options.option('Server' + i + '.Name');
			if (opt === serverName)
			{
				serverId = i;
				break;
			}
		}
		if (serverId === 0)
		{
			// searching by host:port
			var host = undefined;
			for (var i = 1; host !== null; i++)
			{
				host = Options.option('Server' + i + '.Host');
				var port = Options.option('Server' + i + '.Port');
				if (host === serverHost && port === serverPort)
				{
					serverId = i;
					break;
				}
			}
		}

		if (serverId === 0)
		{
			AlertDialog.showModal('Downloaded volumes', 'No statistics available for that server yet.');
			return;
		}

		StatDialog.showModal(serverId);
	}

	/*** SAVE ********************************************************************/

	function getOptionValue(option)
	{
		var control = $('#' + option.formId);
		if (option.type === 'switch')
		{
			return switchGetValue(control);
		}
		else
		{
			return control.val();
		}
	}
	this.getOptionValue = getOptionValue;

	function setOptionValue(option, value)
	{
		var control = $('#' + option.formId);
		if (option.type === 'switch')
		{
			switchSetValue(control, value);
		}
		else
		{
			control.val(value);
		}
	}
	this.setOptionValue = setOptionValue;

	// Checks if there are obsolete or invalid options
	function invalidOptionsExist()
	{
		var hiddenOptions = ['ConfigFile', 'AppBin', 'AppDir', 'Version'];

		for (var i=0; i < Options.options.length; i++)
		{
			var option = Options.options[i];
			var confOpt = findOptionByName(option.Name);
			if (!confOpt && hiddenOptions.indexOf(option.Name) === -1)
			{
				return true;
			}
		}

		return false;
	}

	function prepareSaveRequest(onlyUserChanges, webSettings, onlyChangedOptions)
	{
		var modified = false;
		var request = [];
		for (var k = (webSettings ? 0 : 1); k < (webSettings ? 1 : config.length); k++)
		{
			var sections = config[k].sections;
			for (var i=0; i < sections.length; i++)
			{
				var section = sections[i];
				if (!section.postparam)
				{
					for (var j=0; j < section.options.length; j++)
					{
						var option = section.options[j];
						if (!option.template && !(option.type === 'info') && !option.commandopts)
						{
							var oldValue = option.value;
							var newValue = getOptionValue(option);
							if (section.hidden)
							{
								newValue = oldValue;
							}
							if (newValue != null)
							{
								if (onlyUserChanges)
								{
									var optmodified = oldValue != newValue && oldValue !== null;
								}
								else
								{
									var optmodified = (oldValue != newValue) || (option.value === null);
								}
								modified = modified || optmodified;
								if (optmodified || !onlyChangedOptions)
								{
									var opt = {Name: option.name, Value: newValue};
									request.push(opt);
								}
							}
						}
						modified = modified || section.modified;
					}
				}
			}
		}

		return modified || (!onlyUserChanges && invalidOptionsExist()) || restored ? request : [];
	}

	this.saveChanges = function()
	{
		$LeaveConfigDialog.modal('hide');

		var serverSaveRequest = prepareSaveRequest(false, false);
		var webSaveRequest = prepareSaveRequest(false, true);

		if (serverSaveRequest.length === 0 && webSaveRequest.length === 0)
		{
			PopupNotification.show('#Notif_Config_Unchanged');
			return;
		}

		showSaveBanner();

		Util.show('#ConfigSaved_Reload, #ConfigReload', serverSaveRequest.length > 0);

		if (webSaveRequest.length > 0)
		{
			saveWebSettings(webSaveRequest);
		}

		if (serverSaveRequest.length > 0)
		{
			$('#Notif_Config_Failed_Filename').text(Options.option('ConfigFile'));
			RPC.call('saveconfig', [serverSaveRequest], saveCompleted);
		}
		else
		{
			// only web-settings were changed, refresh page
			document.location.reload(true);
		}
	}

	function showSaveBanner()
	{
		$('#Config_Save').attr('disabled', 'disabled');
	}

	function removeSaveBanner()
	{
		$('#Config_Save').removeAttr('disabled');
	}

	function saveCompleted(result)
	{
		removeSaveBanner();
		if (result)
		{
			$ConfigContent.fadeOut(function() { $('#ConfigSaved').fadeIn(); });
		}
		else
		{
			PopupNotification.show('#Notif_Config_Failed');
		}
		configSaved = true;
	}

	function saveWebSettings(values)
	{
		for (var i=0; i < values.length; i++)
		{
			var option = values[i];
			var optname = option.Name;
			var optvalue = option.Value;
			optname = optname[0].toLowerCase() + optname.substring(1);
			if (optvalue === 'yes') optvalue = true;
			if (optvalue === 'no') optvalue = false;
			UISettings[optname] = optvalue;
		}
		UISettings.save();
	}

	this.canLeaveTab = function(target)
	{
		if (!config || prepareSaveRequest(true).length === 0 || configSaved)
		{
			return true;
		}

		leaveTarget = target;
		$LeaveConfigDialog.modal({backdrop: 'static'});
		return false;
	}

	function userLeavesPage(e)
	{
		if (config && !configSaved && !UISettings.connectionError && prepareSaveRequest(true).length > 0)
		{
			return "Discard changes?";
		}
	}

	this.discardChanges = function()
	{
		configSaved = true;
		$LeaveConfigDialog.modal('hide');
		leaveTarget.click();
	}

	this.scrollToOption = function(event, control)
	{
		event.preventDefault();

		if ($(control).hasClass('option-name') && !$ConfigContent.hasClass('search'))
		{
			// Click on option title scrolls only from Search-page, not from regual pages
			return;
		}

		var optid = $(control).attr('data-optid');
		if (!optid)
		{
			var optname = $(control).text();
			var option = findOptionByName(optname);
			if (option)
			{
				optid = option.formId;
			}
		}
		if (optid)
		{
			scrollOptionIntoView(optid);
		}
	}

	this.showSpoiler = function(control)
	{
		$(control).hide();
		$(control).next().show();
	}

	function filterInput(value)
	{
		filterText = value;
		if (filterText.trim() !== '')
		{
			$('.ConfigSearch').show();
			showSection('Search', true);
		}
		else
		{
			filterClear();
		}
	}

	function filterClear()
	{
		filterText = '';
		showSection(lastSection, true);
		$('.ConfigSearch').hide();
		$ConfigTabBadge.hide();
		$ConfigTabBadgeEmpty.show();
	}

	var searcher = new FastSearcher();

	function search()
	{
		$ConfigTabBadge.show();
		$ConfigTabBadgeEmpty.hide();
		$ConfigContent.addClass('search');

		$ConfigData.children().hide();

		searcher.compile(filterText);

		var total = 0;
		var available = 0;

		for (var k=0; k < config.length; k++)
		{
			var sections = config[k].sections;
			for (var i=0; i < sections.length; i++)
			{
				var section = sections[i];
				if (!section.hidden)
				{
					for (var j=0; j < section.options.length; j++)
					{
						var option = section.options[j];
						if (!option.template)
						{
							total++;
							if (filterOption(option))
							{
								available++;
								var opt = $('#' + option.formId).closest('.control-group');
								opt.show();
							}
						}
					}
				}
			}
		}

		filterStaticPages();

		markLastControlGroup();

		$ConfigTitle.text('SEARCH RESULTS');
		$Body.animate({ scrollTop: 0 }, { duration: 0 });

		updateTabInfo($ConfigTabBadge, { filter: true, available: available, total: total});
	}

	function filterOption(option)
	{
		return searcher.exec({ name: option.caption, description: option.description, value: (option.value === null ? '' : option.value), _search: ['name', 'description', 'value'] });
	}

	function filterStaticPages()
	{
		$ConfigData.children().filter('.config-static').each(function(index, element)
			{
				var name = $('.control-label', element).text();
				var description = $('.controls', element).text();
				var found = searcher.exec({ name: name, description: description, value: '', _search: ['name', 'description'] });
				Util.show(element, found);
			});
	}

	function markLastControlGroup()
	{
		$ConfigData.children().removeClass('last-group');
		$ConfigData.children().filter(':visible').last().addClass('last-group');
	}

	/*** RELOAD ********************************************************************/

	function restart(callback)
	{
		Refresher.pause();
		$('#ConfigReloadInfoNotes').hide();

		$('body').fadeOut(function()
		{
			$('#Navbar, #MainContent').hide();
			$('#ConfigSaved').hide();
			$('body').toggleClass('navfixed', false);
			$('body').show();
			$('#ConfigReloadInfo').fadeIn();
			reloadTime = new Date();
			callback();
		});
	}

	this.reloadConfirm = function()
	{
		ConfirmDialog.showModal('ReloadConfirmDialog', Config.reload);
	}

	this.reload = function()
	{
		$('#ConfigReloadAction').text('Stopping all activities and reloading...');
		restart(function() { RPC.call('reload', [], reloadCheckStatus); });
	}

	function reloadCheckStatus()
	{
		RPC.call('status', [], function(status)
			{
				// OK, checking if it is a restarted instance
				if (status.UpTimeSec >= Status.status.UpTimeSec)
				{
					// the old instance is not restarted yet
					// waiting 0.5 sec. and retrying
					setTimeout(reloadCheckStatus, 500);
					reloadCheckNotes();
				}
				else
				{
					// restarted successfully
					$('#ConfigReloadAction').text('Reloaded successfully. Refreshing the page...');
					// refresh page
					document.location.reload(true);
				}
			},
			function()
			{
				// Failure, waiting 0.5 sec. and retrying
				setTimeout(reloadCheckStatus, 500);
				reloadCheckNotes();
			});
	}

	function reloadCheckNotes()
	{
		// if reload takes more than 30 sec. show additional tips
		if (new Date() - reloadTime > 30000)
		{
			$('#ConfigReloadInfoNotes').show(1000);
		}
	}

	this.applyReloadedValues = function(values)
	{
		Options.reloadConfig(values, buildPage);
		restored = true;
	}

	/*** SHUTDOWN ********************************************************************/

	this.shutdownConfirm = function()
	{
		ConfirmDialog.showModal('ShutdownConfirmDialog', Config.shutdown);
	}

	this.shutdown = function()
	{
		$('#ConfigReloadTitle').text('Shutdown NZBGet');
		$('#ConfigReloadAction').text('Stopping all activities...');
		restart(function() { RPC.call('shutdown', [], shutdownCheckStatus); });
	}

	function shutdownCheckStatus()
	{
		RPC.call('version', [], function(version)
			{
				// the program still runs, waiting 0.5 sec. and retrying
				setTimeout(shutdownCheckStatus, 500);
			},
			function()
			{
				// the program has been stopped
				$('#ConfigReloadTransmit').hide();
				$('#ConfigReloadAction').text('The program has been stopped.');
			});
	}

	/*** UPDATE ********************************************************************/

	this.checkUpdates = function()
	{
		UpdateDialog.showModal();
	}
}(jQuery));


/*** CHOOSE SCRIPT DIALOG *******************************************************/

var ScriptListDialog = (new function($)
{
	'use strict'

	// Controls
	var $ScriptListDialog;
	var $ScriptTable;
	var option;
	var config;
	var kind;
	var scriptList;
	var allScripts;
	var orderChanged;
	var orderMode;

	this.init = function()
	{
		$ScriptListDialog = $('#ScriptListDialog');
		$('#ScriptListDialog_Save').click(save);

		$ScriptTable = $('#ScriptListDialog_ScriptTable');

		$ScriptTable.fasttable(
			{
				pagerContainer: '#ScriptListDialog_ScriptTable_pager',
				infoEmpty: 'No scripts found. If you just changed option "ScriptDir", save settings and reload NZBGet.',
				pageSize: 1000
			});

		$ScriptListDialog.on('hidden', function()
		{
			// cleanup
			$ScriptTable.fasttable('update', []);
		});
	}

	this.showModal = function(_option, _config, _kind)
	{
		option = _option;
		config = _config;
		kind = _kind;
		orderChanged = false;
		orderMode = option.name === 'ScriptOrder';

		if (orderMode)
		{
			$('#ScriptListDialog_Title').text('Reorder extensions');
			$('#ScriptListDialog_Instruction').text('Hover mouse over table elements for reorder buttons to appear.');
		}
		else
		{
			$('#ScriptListDialog_Title').text('Choose extensions');
			$('#ScriptListDialog_Instruction').html('Select extension scripts for option <strong>' + option.name + '</strong>.');
		}

		$ScriptTable.toggleClass('table-hidecheck', orderMode);
		$ScriptTable.toggleClass('table-check table-cancheck', !orderMode);
		$('#ScriptListDialog_OrderInfo').toggleClass('alert alert-info', !orderMode);
		Util.show('#ScriptListDialog_OrderInfo', orderMode, 'inline-block');

		buildScriptList();
		var selectedList = Util.parseCommaList(Config.getOptionValue(option));
		updateTable(selectedList);

		$ScriptListDialog.modal({backdrop: 'static'});
	}

	function updateTable(selectedList)
	{
		var reorderButtons = '<div class="btn-row-order-block"><div class="btn-row-order icon-top" onclick="ScriptListDialog.move(this, \'top\')"></div><div class="btn-row-order icon-up" onclick="ScriptListDialog.move(this, \'up\')"></div><div class="btn-row-order icon-down" onclick="ScriptListDialog.move(this, \'down\')"></div><div class="btn-row-order icon-bottom" onclick="ScriptListDialog.move(this, \'bottom\')"></div></div>';
		var data = [];
		for (var i=0; i < scriptList.length; i++)
		{
			var scriptName = scriptList[i];
			var scriptShortName = Options.shortScriptName(scriptName);

			var fields = ['<div class="check img-check"></div>', '<span data-index="' + i + '">' + scriptShortName + '</span>' + reorderButtons];
			var item =
			{
				id: scriptName,
				fields: fields,
			};
			data.push(item);

			if (!orderMode && selectedList && selectedList.indexOf(scriptName) > -1)
			{
				$ScriptTable.fasttable('checkRow', scriptName, true);
			}
		}
		$ScriptTable.fasttable('update', data);
	}

	function buildScriptList()
	{
		var orderList = Util.parseCommaList(Config.getOptionValue(Config.findOptionByName('ScriptOrder')));

		var availableScripts = [];
		var availableAllScripts = [];
		for (var i=2; i < config.length; i++)
		{
			availableAllScripts.push(config[i].scriptName);
			var accept = !kind;
			if (!accept)
			{
				for (var j=0; j < kind.length; j++)
				{
					accept = accept || config[i][kind[j]];
				}
			}
			if (accept)
			{
				availableScripts.push(config[i].scriptName);
			}
		}
		availableScripts.sort();
		availableAllScripts.sort();

		scriptList = [];
		allScripts = [];

		// first add all scripts from orderList
		for (var i=0; i < orderList.length; i++)
		{
			var scriptName = orderList[i];
			if (availableScripts.indexOf(scriptName) > -1)
			{
				scriptList.push(scriptName);
			}
			if (availableAllScripts.indexOf(scriptName) > -1)
			{
				allScripts.push(scriptName);
			}
		}

		// add all other scripts of this kind from script list
		for (var i=0; i < availableScripts.length; i++)
		{
			var scriptName = availableScripts[i];
			if (scriptList.indexOf(scriptName) === -1)
			{
				scriptList.push(scriptName);
			}
		}

		// add all other scripts of other kinds from script list
		for (var i=0; i < availableAllScripts.length; i++)
		{
			var scriptName = availableAllScripts[i];
			if (allScripts.indexOf(scriptName) === -1)
			{
				allScripts.push(scriptName);
			}
		}

		return scriptList;
	}

	function save(e)
	{
		e.preventDefault();

		if (!orderMode)
		{
			var	 selectedList = '';
			var checkedRows = $ScriptTable.fasttable('checkedRows');

			for (var i=0; i < scriptList.length; i++)
			{
				var scriptName = scriptList[i];
				if (checkedRows[scriptName])
				{
					selectedList += (selectedList == '' ? '' : ', ') + scriptName;
				}
			}

			var control = $('#' + option.formId);
			control.val(selectedList);
		}

		if (orderChanged)
		{
			var scriptOrderOption = Config.findOptionByName('ScriptOrder');
			var control = $('#' + scriptOrderOption.formId);

			// preserving order of scripts of other kinds which were not visible in the dialog
			var orderList = [];
			for (var i=0; i < allScripts.length; i++)
			{
				var scriptName = allScripts[i];
				if (orderList.indexOf(scriptName) === -1)
				{
					if (scriptList.indexOf(scriptName) > -1)
					{
						orderList = orderList.concat(scriptList);
					}
					else
					{
						orderList.push(scriptName);
					}
				}
			}

			control.val(orderList.join(', '));
		}

		$ScriptListDialog.modal('hide');
	}

	this.move = function(control, direction)
	{
		var index = parseInt($('span', $(control).closest('tr')).attr('data-index'));
		if ((index === 0 && (direction === 'up' || direction === 'top')) ||
			(index === scriptList.length-1 && (direction === 'down' || direction === 'bottom')))
		{
			return;
		}

		switch (direction)
		{
			case 'up':
			case 'down':
				{
					var newIndex = direction === 'up' ? index - 1 : index + 1;
					var tmp = scriptList[newIndex];
					scriptList[newIndex] = scriptList[index];
					scriptList[index] = tmp;
					break;
				}
			case 'top':
			case 'bottom':
				{
					var tmp = scriptList[index];
					scriptList.splice(index, 1);
					if (direction === 'top')
					{
						scriptList.unshift(tmp);
					}
					else
					{
						scriptList.push(tmp);
					}
					break;
				}
		}

		if (!orderChanged && !orderMode)
		{
			$('#ScriptListDialog_OrderInfo').fadeIn(500);
		}

		orderChanged = true;
		updateTable();
	}

}(jQuery));


/*** BACKUP/RESTORE SETTINGS *******************************************************/

var ConfigBackupRestore = (new function($)
{
	'use strict'

	// State
	var settings;
	var filename;

	this.init = function(options)
	{
		$('#Config_RestoreInput')[0].addEventListener('change', restoreSelectHandler, false);
	}

	/*** BACKUP ********************************************************************/

	this.backupSettings = function()
	{
		var settings = '';
		for (var i=0; i < Config.config().values.length; i++)
		{
			var option = Config.config().values[i];
			if (option.Value !== null)
			{
				settings += settings==='' ? '' : '\n';
				settings += option.Name + '=' + option.Value;
			}
		}

		var pad = function(arg) { return (arg < 10 ? '0' : '') + arg }
		var dt = new Date();
		var datestr = dt.getFullYear() + pad(dt.getMonth() + 1) + pad(dt.getDate()) + '-' + pad(dt.getHours()) + pad(dt.getMinutes()) + pad(dt.getSeconds());

		var filename = 'nzbget-' + datestr + '.conf';

		if (!Util.saveToLocalFile(settings, "text/plain;charset=utf-8", filename))
		{
			alert('Unfortunately your browser doesn\'t support access to local file system.\n\n'+
				'To backup settings you can manually save file "nzbget.conf" (' +
				Options.option('ConfigFile')+ ').');
		}
	}

	/*** RESTORE ********************************************************************/

	this.restoreSettings = function()
	{
		if (!window.FileReader)
		{
			alert("Unfortunately your browser doesn't support FileReader API.");
			return;
		}

		var testreader = new FileReader();
		if (!testreader.readAsBinaryString && !testreader.readAsDataURL)
		{
			alert("Unfortunately your browser doesn't support neither \"readAsBinaryString\" nor \"readAsDataURL\" functions of FileReader API.");
			return;
		}

		var inp = $('#Config_RestoreInput');

		// Reset file input control (needed for IE10)
		inp.wrap('<form>').closest('form').get(0).reset();
		inp.unwrap();

		inp.click();
	}

	function restoreSelectHandler(event)
	{
		if (!event.target.files)
		{
			alert("Unfortunately your browser doesn't support direct access to local files.");
			return;
		}
		if (event.target.files.length > 0)
		{
			restoreFromFile(event.target.files[0]);
		}
	}

	function restoreFromFile(file)
	{
		var reader = new FileReader();
		reader.onload = function (event)
		{
			if (reader.readAsBinaryString)
			{
				settings = event.target.result;
			}
			else
			{
				var base64str = event.target.result.replace(/^data:[^,]+,/, '');
				settings = atob(base64str);
			}
			filename = file.name;

			if (settings.indexOf('MainDir=') < 0)
			{
				alert('File ' + filename + ' is not a valid NZBGet backup.');
				return;
			}

			RestoreSettingsDialog.showModal(Config.config(), restoreExecute);
		};

		if (reader.readAsBinaryString)
		{
			reader.readAsBinaryString(file);
		}
		else
		{
			reader.readAsDataURL(file);
		}
	}

	function restoreExecute(selectedSections)
	{
		$('#Notif_Config_Restoring').show();
		setTimeout(function() {
			var values = restoreValues(selectedSections);
			Config.applyReloadedValues(values);
			$('#Notif_Config_Restoring').hide();
			$('#SettingsRestoredDialog').modal({backdrop: 'static'});
		}, 50);
	}

	function restoreValues(selectedSections)
	{
		var config = Config.config();
		var values = config.values;

		settings = settings.split('\n');

		for (var i=0; i < settings.length; i++)
		{
			var optstr = settings[i];
			var ind = optstr.indexOf('=');
			var option = { Name: optstr.substr(0, ind).trim(), Value: optstr.substr(ind+1, 100000).trim() };
			settings[i] = option;
		}

		function removeValue(name)
		{
			var name = name.toLowerCase();
			for (var i=0; i < values.length; i++)
			{
				if (values[i].Name.toLowerCase() === name)
				{
					values.splice(i, 1);
					return true;
				}
			}
			return false;
		}

		function addValue(name)
		{
			var name = name.toLowerCase();
			for (var i=0; i < settings.length; i++)
			{
				if (settings[i].Name.toLowerCase() === name)
				{
					values.push(settings[i]);
					return true;
				}
			}
			return false;
		}

		function restoreOption(option)
		{
			if (!option.template && !option.multiid)
			{
				removeValue(option.name);
				addValue(option.name);
			}
			else if (option.template)
			{
				// delete all multi-options
				for (var j=1; ; j++)
				{
					var optname = option.name.replace('1', j);
					if (!removeValue(optname))
					{
						break;
					}
				}
				// add all multi-options
				for (var j=1; ; j++)
				{
					var optname = option.name.replace('1', j);
					if (!addValue(optname))
					{
						break;
					}
				}
			}
		}

		for (var k=0; k < config.length; k++)
		{
			var conf = config[k];
			for (var i=0; i < conf.sections.length; i++)
			{
				var section = conf.sections[i];
				if (!section.hidden && selectedSections[section.id])
				{
					for (var m=0; m < section.options.length; m++)
					{
						restoreOption(section.options[m]);
					}
				}
			}
		}

		return values;
	}
}(jQuery));


/*** RESTORE SETTINGS DIALOG *******************************************************/

var RestoreSettingsDialog = (new function($)
{
	'use strict'

	// Controls
	var $RestoreSettingsDialog;
	var $SectionTable;

	// State
	var config;
	var restoreClick;

	this.init = function()
	{
		$RestoreSettingsDialog = $('#RestoreSettingsDialog');
		$('#RestoreSettingsDialog_Restore').click(restore);

		$SectionTable = $('#RestoreSettingsDialog_SectionTable');

		$SectionTable.fasttable(
			{
				pagerContainer: $('#RestoreSettingsDialog_SectionTable_pager'),
				rowSelect: UISettings.rowSelect,
				infoEmpty: 'No sections found.',
				pageSize: 1000
			});

		$RestoreSettingsDialog.on('hidden', function()
		{
			// cleanup
			$SectionTable.fasttable('update', []);
		});
	}

	this.showModal = function(_config, _restoreClick)
	{
		config = _config;
		restoreClick = _restoreClick;
		updateTable();
		$RestoreSettingsDialog.modal({backdrop: 'static'});
	}

	function updateTable()
	{
		var data = [];
		for (var k=0; k < config.length; k++)
		{
			var conf = config[k];
			for (var i=0; i < conf.sections.length; i++)
			{
				var section = conf.sections[i];
				if (!section.hidden)
				{
					var fields = ['<div class="check img-check"></div>', '<span data-section="' + section.id + '">' + section.name + '</span>'];
					var item =
					{
						id: section.id,
						fields: fields,
					};
					data.push(item);
				}
			}
		}
		$SectionTable.fasttable('update', data);
	}

	function restore(e)
	{
		e.preventDefault();

		var checkedRows = $SectionTable.fasttable('checkedRows');
		var checkedCount = $SectionTable.fasttable('checkedCount');
		if (checkedCount === 0)
		{
			PopupNotification.show('#Notif_Config_RestoreSections');
			return;
		}

		checkedRows = $.extend({}, checkedRows); // clone
		$RestoreSettingsDialog.modal('hide');

		setTimeout(function() { restoreClick(checkedRows); }, 0);
	}

}(jQuery));


/*** UPDATE DIALOG *******************************************************/

var UpdateDialog = (new function($)
{
	'use strict'

	// Controls
	var $UpdateDialog;
	var $UpdateProgressDialog;
	var $UpdateProgressDialog_Log;
	var $UpdateDialog_Close;

	// State
	var VersionInfo;
	var PackageInfo;
	var UpdateInfo;
	var lastUpTimeSec;
	var installing = false;
	var logReceived = false;
	var foreground = false;

	this.init = function()
	{
		$UpdateDialog = $('#UpdateDialog');
		$('#UpdateDialog_InstallStable,#UpdateDialog_InstallTesting,#UpdateDialog_InstallDevel').click(install);
		$UpdateProgressDialog = $('#UpdateProgressDialog');
		$UpdateProgressDialog_Log = $('#UpdateProgressDialog_Log');
		$UpdateDialog_Close = $('#UpdateDialog_Close');

		$UpdateDialog.on('hidden', resumeRefresher);
		$UpdateProgressDialog.on('hidden', resumeRefresher);
	}

	function resumeRefresher()
	{
		if (!installing && foreground)
		{
			Refresher.resume();
		}
	}

	this.showModal = function()
	{
		foreground = true;
		this.performCheck();
		$UpdateDialog.modal({backdrop: 'static'});
	}

	this.checkUpdate = function()
	{
		var lastCheck = new Date(parseInt(UISettings.read('LastUpdateCheck', '0')));
		var hoursSinceLastCheck = Math.abs(new Date() - lastCheck) / (60*60*1000);
		if (Options.option('UpdateCheck') !== 'none' && hoursSinceLastCheck > 12)
		{
			UISettings.write('LastUpdateCheck', new Date() / 1);
			this.performCheck();
		}
	}

	this.performCheck = function()
	{
		$('#UpdateDialog_Install').hide();
		$('#UpdateDialog_CheckProgress').show();
		$('#UpdateDialog_CheckFailed').hide();
		$('#UpdateDialog_Versions').hide();
		$('#UpdateDialog_UpdateAvail').hide();
		$('#UpdateDialog_DownloadAvail').hide();
		$('#UpdateDialog_UpdateNotAvail').hide();
		$('#UpdateDialog_InstalledInfo').show();
		$UpdateDialog_Close.text(foreground ? 'Close' : 'Remind Me Later');
		$('#UpdateDialog_VerInstalled').text(Options.option('Version'));

		PackageInfo = {};
		VersionInfo = {};
		UpdateInfo = {};

		installing = false;
		if (foreground)
		{
			Refresher.pause();
		}

		RPC.call('readurl', ['http://nzbget.net/info/nzbget-version.json?nocache=' + new Date().getTime(), 'nzbget version info'], loadedUpstreamInfo, error);
	}

	function error(e)
	{
		$('#UpdateDialog_CheckProgress').hide();
		$('#UpdateDialog_CheckFailed').show();
	}

	function parseJsonP(jsonp)
	{
		var p = jsonp.indexOf('{');
		var obj = JSON.parse(jsonp.substr(p, 10000));
		return obj;
	}

	function loadedUpstreamInfo(data)
	{
		VersionInfo = parseJsonP(data);
		if (VersionInfo['devel-version'] || !foreground)
		{
			loadPackageInfo();
		}
		else
		{
			loadGitVerData(loadPackageInfo);
		}
	}

	function loadGitVerData(callback)
	{
		// fetching devel version number from github web-site
		RPC.call('readurl', ['https://github.com/nzbget/nzbget', 'nzbget git revision info'],
			function(gitRevData)
			{
				RPC.call('readurl', ['https://raw.githubusercontent.com/nzbget/nzbget/develop/configure.ac', 'nzbget git branch info'],
					function(gitBranchData)
					{
						var html = document.createElement('DIV');
						html.innerHTML = gitRevData;
						html = html.textContent || html.innerText || '';
						html = html.replace(/(?:\r\n|\r|\n)/g, ' ');
						var rev = html.match(/([0-9\,]*)\s*commits/);

						if (rev && rev.length > 1)
						{
							rev = rev[1].replace(',', '');
							var ver = gitBranchData.match(/AC_INIT\(nzbget, (.*), .*/);
							if (ver && ver.length > 1)
							{
								VersionInfo['devel-version'] = ver[1] + '-r' + rev;
							}
						}

						callback();
					}, callback);
			}, callback);
	}

	function loadPackageInfo()
	{
		$.get('package-info.json', loadedPackageInfo, 'html').fail(loadedAll);
	}

	function loadedPackageInfo(data)
	{
		PackageInfo = parseJsonP(data);
		if (PackageInfo['update-info-link'])
		{
			RPC.call('readurl', [PackageInfo['update-info-link'], 'nzbget update info'], loadedUpdateInfo, loadedAll);
		}
		else if (PackageInfo['update-info-script'])
		{
			RPC.call('checkupdates', [], loadedUpdateInfo, loadedAll);
		}
		else
		{
			loadedAll();
		}
	}

	function loadedUpdateInfo(data)
	{
		UpdateInfo = parseJsonP(data);
		loadedAll();
	}

	function formatTesting(str)
	{
		return str.replace('-testing-', '-');
	}

	function revision(version)
	{
		var rev = version.match(/.*r(\d+)/);
		return rev && rev.length > 1 ? parseInt(rev[1]) : 0;
	}

	function vernumber(version)
	{
		var ver = version.match(/([\d.]+).*/);
		return ver && ver.length > 1 ? parseFloat(ver[1]) : 0;
	}

	function loadedAll()
	{
		var installedVersion = Options.option('Version');

		$('#UpdateDialog_CheckProgress').hide();
		$('#UpdateDialog_Versions').show();
		$('#UpdateDialog_InstalledInfo').show();

		$('#UpdateDialog_CurStable').text(VersionInfo['stable-version'] ? VersionInfo['stable-version'] : 'no data');
		$('#UpdateDialog_CurTesting').text(VersionInfo['testing-version'] ? formatTesting(VersionInfo['testing-version']) : 'no data');
		$('#UpdateDialog_CurDevel').text(VersionInfo['devel-version'] ? formatTesting(VersionInfo['devel-version']) : 'no data');

		$('#UpdateDialog_CurNotesStable').attr('href', VersionInfo['stable-release-notes']);
		$('#UpdateDialog_CurNotesTesting').attr('href', VersionInfo['testing-release-notes']);
		$('#UpdateDialog_CurNotesDevel').attr('href', VersionInfo['devel-release-notes']);
		$('#UpdateDialog_DownloadStable').attr('href', VersionInfo['stable-download']);
		$('#UpdateDialog_DownloadTesting').attr('href', VersionInfo['testing-download']);
		Util.show('#UpdateDialog_CurNotesStable', VersionInfo['stable-release-notes']);
		Util.show('#UpdateDialog_CurNotesTesting', VersionInfo['testing-release-notes']);
		Util.show('#UpdateDialog_CurNotesDevel', VersionInfo['devel-release-notes']);

		$('#UpdateDialog_AvailStable').text(UpdateInfo['stable-version'] ? UpdateInfo['stable-version'] : 'not available');
		$('#UpdateDialog_AvailTesting').text(UpdateInfo['testing-version'] ? formatTesting(UpdateInfo['testing-version']) : 'not available');
		$('#UpdateDialog_AvailDevel').text(UpdateInfo['devel-version'] ? formatTesting(UpdateInfo['devel-version']) : 'not available');

		if (UpdateInfo['stable-version'] === VersionInfo['stable-version'] &&
			UpdateInfo['testing-version'] === VersionInfo['testing-version'])
		{
			$('#UpdateDialog_AvailStableBlock,#UpdateDialog_AvailTestingBlock,#UpdateDialog_AvailDevelBlock').hide();
			$('#UpdateDialog_AvailRow .update-row-name').text('');
			$('#UpdateDialog_AvailRow td').css('border-style', 'none');
		}

		$('#UpdateDialog_DownloadRow td').css('border-style', 'none');
		
		$('#UpdateDialog_AvailNotesStable').attr('href', UpdateInfo['stable-package-info']);
		$('#UpdateDialog_AvailNotesTesting').attr('href', UpdateInfo['testing-package-info']);
		$('#UpdateDialog_AvailNotesDevel').attr('href', UpdateInfo['devel-package-info']);
		Util.show('#UpdateDialog_AvailNotesStableBlock', UpdateInfo['stable-package-info']);
		Util.show('#UpdateDialog_AvailNotesTestingBlock', UpdateInfo['testing-package-info']);
		Util.show('#UpdateDialog_AvailNotesDevelBlock', UpdateInfo['devel-package-info']);

		var installedVer = vernumber(installedVersion);
		var installedRev = revision(installedVersion);
		var installedTesting = installedRev > 0 || installedVersion.indexOf('testing') > -1;

		var canInstallStable = UpdateInfo['stable-version'] &&
			((installedVer < vernumber(UpdateInfo['stable-version'])) ||
			 (installedTesting && installedVer === vernumber(UpdateInfo['stable-version'])));
		var canInstallTesting = UpdateInfo['testing-version'] &&
			((installedVer < vernumber(UpdateInfo['testing-version'])) ||
			 (installedTesting && installedVer === vernumber(UpdateInfo['testing-version'])) &&
			  installedRev < revision(UpdateInfo['testing-version']));
		var canInstallDevel = UpdateInfo['devel-version'] &&
			((installedVer < vernumber(UpdateInfo['devel-version'])) ||
			 (installedTesting && installedVer === vernumber(UpdateInfo['devel-version'])) &&
			  installedRev < revision(UpdateInfo['devel-version']));
		Util.show('#UpdateDialog_InstallStable', canInstallStable);
		Util.show('#UpdateDialog_InstallTesting', canInstallTesting);
		Util.show('#UpdateDialog_InstallDevel', canInstallDevel);

		var canDownloadStable = 
			((installedVer < vernumber(VersionInfo['stable-version'])) ||
			 (installedTesting && installedVer === vernumber(VersionInfo['stable-version'])));
		var canDownloadTesting = 
			((installedVer < vernumber(VersionInfo['testing-version'])) ||
			 (installedTesting && installedVer === vernumber(VersionInfo['testing-version']) &&
			  installedRev < revision(VersionInfo['testing-version'])));
		Util.show('#UpdateDialog_DownloadStable', canDownloadStable);
		Util.show('#UpdateDialog_DownloadTesting', canDownloadTesting);

		var hasUpdateSource = PackageInfo['update-info-link'] || PackageInfo['update-info-script'];
		var hasUpdateInfo = UpdateInfo['stable-version'] || UpdateInfo['testing-version'] || UpdateInfo['devel-version'];
		var canUpdate = canInstallStable || canInstallTesting || canInstallDevel;
		var canDownload = canDownloadStable || canDownloadTesting;
		Util.show('#UpdateDialog_UpdateAvail', canUpdate);
		Util.show('#UpdateDialog_UpdateNotAvail', !canUpdate && !canDownload);
		Util.show('#UpdateDialog_CheckFailed', hasUpdateSource && !hasUpdateInfo);
		Util.show('#UpdateDialog_DownloadRow,#UpdateDialog_DownloadAvail', canDownload && !canUpdate);
		$('#UpdateDialog_AvailRow').toggleClass('hide', !hasUpdateInfo);

		if (!foreground &&
			(((canInstallStable || canDownloadStable) && notificationAllowed('stable')) ||
			 (Options.option('UpdateCheck') === 'testing' && installedRev > 0 &&
			  (canInstallTesting || canDownloadTesting) && notificationAllowed('testing'))))
		{
			$UpdateDialog.modal({backdrop: 'static'});
			loadDevelVersionInfo();
		}
	}

	function loadDevelVersionInfo()
	{
		if (!VersionInfo['devel-version'])
		{
			$('#UpdateDialog_CurDevel').text('loading...');
			loadGitVerData(function()
				{
					$('#UpdateDialog_CurDevel').text(VersionInfo['devel-version'] ? formatTesting(VersionInfo['devel-version']) : 'no data');
				});
		}
	}

	function notificationAllowed(branch)
	{
		// We don't want to update all users on release day.
		// Parameter "update-rate" controls the spreading speed of the release.
		// It contains comma-separated list of percentages of users, which should get
		// notification about new release at a given day after release.

		var rateCurve = UpdateInfo[branch + '-update-rate'] ? UpdateInfo[branch + '-update-rate'] : VersionInfo[branch + '-update-rate'];
		if (!rateCurve)
		{
			return true;
		}

		var rates = rateCurve.split(',');
		var releaseDate = new Date(UpdateInfo[branch + '-date'] ? UpdateInfo[branch + '-date'] : VersionInfo[branch + '-date']);
		var daysSinceRelease = Math.floor((new Date() - releaseDate) / (1000*60*60*24));
		var coverage = rates[Math.min(daysSinceRelease, rates.length-1)];
		var dice = Math.floor(Math.random() * 100);

		return dice <= coverage;
	}

	function install(e)
	{
		e.preventDefault();
		var kind = $(this).attr('data-kind');
		var script = PackageInfo['install-script'];

		if (!script)
		{
			alert('Something is wrong with the package configuration file "package-info.json".');
			return;
		}

		RPC.call('status', [], function(status)
			{
				lastUpTimeSec = status.UpTimeSec;
				RPC.call('startupdate', [kind], updateStarted);
			});
	}

	function updateStarted(started)
	{
		if (!started)
		{
			PopupNotification.show('#Notif_StartUpdate_Failed');
			return;
		}

		installing = true;
		$UpdateDialog.fadeOut(250, function()
			{
				$UpdateProgressDialog_Log.text('');
				$UpdateProgressDialog.fadeIn(250, function()
					{
						$UpdateDialog.modal('hide');
						$UpdateProgressDialog.modal({backdrop: 'static'});
						updateLog();
					});
			});
	}

	function updateLog()
	{
		RPC.call('logupdate', [0, 100], function(data)
			{
				logReceived = logReceived || data.length > 0;
				if (logReceived && data.length === 0)
				{
					terminated();
				}
				else
				{
					updateLogTable(data);
					setTimeout(updateLog, 500);
				}
			}, terminated);
	}

	function terminated()
	{
		// rpc-failure: the program has been terminated. Waiting for new instance.
		setLogContentAndScroll($UpdateProgressDialog_Log.html() + '\n' + 'NZBGet has been terminated. Waiting for restart...');
		setTimeout(checkStatus, 500);
	}

	function setLogContentAndScroll(html)
	{
		var scroll = $UpdateProgressDialog_Log.prop('scrollHeight') - $UpdateProgressDialog_Log.prop('scrollTop') === $UpdateProgressDialog_Log.prop('clientHeight');
		$UpdateProgressDialog_Log.html(html);
		if (scroll)
		{
			$UpdateProgressDialog_Log.scrollTop($UpdateProgressDialog_Log.prop('scrollHeight'));
		}
	}

	function updateLogTable(messages)
	{
		var html = '';
		for (var i=0; i < messages.length; i++)
		{
			var message = messages[i];
			var text = Util.textToHtml(message.Text);
			if (message.Kind === 'ERROR')
			{
				text = '<span class="update-log-error">' + text + '</span>';
			}
			html = html + text + '\n';
		}
		setLogContentAndScroll(html);
	}

	function checkStatus()
	{
		RPC.call('status', [], function(status)
			{
				// OK, checking if it is a restarted instance
				if (status.UpTimeSec >= lastUpTimeSec)
				{
					// the old instance is not restarted yet
					// waiting 0.5 sec. and retrying
					if ($('#UpdateProgressDialog').is(':visible'))
					{
						setTimeout(checkStatus, 500);
					}
				}
				else
				{
					// restarted successfully, refresh page
					setLogContentAndScroll($UpdateProgressDialog_Log.html() + '\n' + 'Successfully started. Refreshing the page...');
					setTimeout(function()
						{
							document.location.reload(true);
						}, 1000);
				}
			},
			function()
			{
				// Failure, waiting 0.5 sec. and retrying
				if ($('#UpdateProgressDialog').is(':visible'))
				{
					setTimeout(checkStatus, 500);
				}
			});
	}

}(jQuery));


/*** EXEC SCRIPT DIALOG *******************************************************/

var ExecScriptDialog = (new function($)
{
	'use strict'

	// Controls
	var $ExecScriptDialog;
	var $ExecScriptDialog_Log;
	var $ExecScriptDialog_Title;
	var $ExecScriptDialog_Status;

	// State
	var visible = false;

	this.init = function()
	{
		$ExecScriptDialog = $('#ExecScriptDialog');
		$ExecScriptDialog_Log = $('#ExecScriptDialog_Log');
		$ExecScriptDialog_Title = $('#ExecScriptDialog_Title');
		$ExecScriptDialog_Status = $('#ExecScriptDialog_Status');
		$ExecScriptDialog.on('hidden', function() { visible = false; });
	}

	this.showModal = function(script, command, context, changedOptions)
	{
		$ExecScriptDialog_Title.text('Executing script ' + script);
		$ExecScriptDialog_Log.text('');
		$ExecScriptDialog_Status.show();
		$ExecScriptDialog.modal({backdrop: 'static'});
		visible = true;

		RPC.call('startscript', [script, command, context, changedOptions],
			function (result)
			{
				if (result)
				{
					updateLog();
				}
				else
				{
					setLogContentAndScroll('<span class="script-log-error">Script start failed</span>');
					$ExecScriptDialog_Status.hide();
				}
			}
		);
	}

	function updateLog()
	{
		RPC.call('logscript', [0, 100], function(data)
			{
				updateLogTable(data);
				if (visible)
				{
					setTimeout(updateLog, 500);
				}
			});
	}

	function setLogContentAndScroll(html)
	{
		var scroll = $ExecScriptDialog_Log.prop('scrollHeight') - $ExecScriptDialog_Log.prop('scrollTop') === $ExecScriptDialog_Log.prop('clientHeight');
		$ExecScriptDialog_Log.html(html);
		if (scroll)
		{
			$ExecScriptDialog_Log.scrollTop($ExecScriptDialog_Log.prop('scrollHeight'));
		}
	}

	function updateLogTable(messages)
	{
		var html = '';
		for (var i=0; i < messages.length; i++)
		{
			var message = messages[i];
			var text = Util.textToHtml(message.Text);
			if (text.substr(0, 7) === 'Script ')
			{
				$ExecScriptDialog_Status.fadeOut(500);
				if (message.Kind === 'INFO')
				{
					text = '<span class="script-log-success">' + text + '</span>';
				}
			}
			if (message.Kind === 'ERROR')
			{
				text = '<span class="script-log-error">' + text + '</span>';
			}
			html = html + text + '\n';
		}
		setLogContentAndScroll(html);
	}

}(jQuery));
