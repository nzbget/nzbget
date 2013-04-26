/*
 * This file is part of nzbget
 *
 * Copyright (C) 2012-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * $Revision$
 * $Date$
 *
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
	this.postParamConfig;
	this.categories = [];

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
		// RPC-function "config" returns CURRENT configurations settings loaded in NZBGet
		RPC.call('config', [], function(_options) {
			_this.options = _options;
			initCategories();

			// loading config templates and build list of post-processing parameters
			_this.postParamConfig = [];
			RPC.call('configtemplates', [], function(data)
				{
					initPostParamConfig(data);
					RPC.next();
				},
				RPC.next);
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
		RPC.call('configtemplates', [], serverTemplateLoaded, loadServerTemplateError);
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
		var serverConfig = readConfigTemplate(serverTemplateData[0].Template, undefined, HIDDEN_SECTIONS, '', '');
		mergeValues(serverConfig.sections, serverValues);
		config.push(serverConfig);

		// read scripts configs
		for (var i=1; i < serverTemplateData.length; i++)
		{
			var scriptName = serverTemplateData[i].Name;
			var scriptConfig = readConfigTemplate(serverTemplateData[i].Template, undefined, HIDDEN_SECTIONS, scriptName + ':');
			scriptConfig.scriptName = scriptName;
			scriptConfig.id = scriptName.replace(/ |\/|\\|[\.|$|\:|\*]/g, '_');
			scriptConfig.name = scriptName.substr(0, scriptName.lastIndexOf('.')) || scriptName; // remove file extension
			scriptConfig.name = scriptConfig.name.replace(/\\/, ' \\ ').replace(/\//, ' / ');
			scriptConfig.shortName = shortScriptName(scriptName);
			scriptConfig.shortName = scriptConfig.shortName.replace(/\\/, ' \\ ').replace(/\//, ' / ');
			mergeValues(scriptConfig.sections, serverValues);
			config.push(scriptConfig);
		}

		serverValues = null;
		loadComplete(config);
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
			var line = data[i];

			if (line.substring(0, 4) === '### ')
			{
				var section = {};
				section.name = line.substr(4, line.length - 8).trim();
				section.id = (nameprefix + section.name).replace(/ |\/|\\|[\.|$|\:|\*]/g, '_');
				section.options = [];
				description = '';
				section.hidden = !(hiddensections === undefined || (hiddensections.indexOf(section.name) == -1)) ||
					(visiblesections !== undefined && (visiblesections.indexOf(section.name) == -1));
				config.sections.push(section);
			}
			else if (line.substring(0, 2) === '# ' || line === '#')
			{
				if (description !== '')
				{
					description += ' ';
				}
				description += line.substr(1, 10000).trim();
				var lastchar = description.substr(description.length - 1, 1);
				if (lastchar === '.' && firstdescrline === '')
				{
					firstdescrline = description;
					description = '';
				}
				if (".;:".indexOf(lastchar) > -1 || line === '#')
				{
					description += "\n";
				}
			}
			else if (line.indexOf('=') > -1)
			{
				if (!section)
				{
					// bad template file; create default section.
					section = {};
					section.name = 'OPTIONS';
					section.id = (nameprefix + section.name).replace(/ |\/|[\.|$|\:|\*]/g, '_');
					section.options = [];
					description = '';
					config.sections.push(section);
				}

				var option = {};
				var enabled = line.substr(0, 1) !== '#';
				option.caption = line.substr(enabled ? 0 : 1, line.indexOf('=') - (enabled ? 0 : 1)).trim();
				option.name = (nameprefix != '' ? nameprefix : '') + option.caption;
				option.defvalue = line.substr(line.indexOf('=') + 1, 1000).trim();
				option.value = '';
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
								section.options.push(newoption);
								var val = findOption(values, name);
								if (val)
								{
									newoption.value = val.Value;
								}
								newoption.exists = !!val;
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
					var val = findOption(values, option.name);
					if (val)
					{
						option.value = val.Value;
					}
					option.exists = !!val;
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
			var scriptName = data[i].Name;
			var sectionId = (scriptName + ':').replace(/ |\/|[\.|$|\:|\*]/g, '_');
			var option = {};
			option.name = scriptName + ':';
			option.caption = shortScriptName(scriptName);
			option.caption = option.caption.replace(/\\/, ' \\ ').replace(/\//, ' / ');

			option.defvalue = 'no';
			option.description = (data[i].Template.trim().split('\n')[0].substr(1, 1000).trim() || 'Post-processing script ' + scriptName + '.');
			option.value = '';
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
	var $Body;

	// State
	var config;
	var filterText = '';
	var lastSection;
	var reloadTime;
	var updateTabInfo;

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

		$('#ConfigTable_filter').val('');

		$('#ConfigTabLink').on('show', show);
		$('#ConfigTabLink').on('shown', shown);

		$ConfigNav.on('click', 'li > a', navClick);

		$ConfigTable = $({});
		$ConfigTable.fasttable(
			{
				filterInput: $('#ConfigTable_filter'),
				filterClearButton: $("#ConfigTable_clearfilter"),
				filterInputCallback: filterInput,
				filterClearCallback: filterClear
			});
	}

	this.cleanup = function()
	{
		Options.cleanup();
		config = null;
		$ConfigNav.children().not('.config-static').remove();
		$ConfigData.children().not('.config-static').remove();
	}

	function show()
	{
		removeSaveBanner();
		$('#ConfigSaved').hide();
		$('#ConfigLoadInfo').show();
		$('#ConfigLoadServerTemplateError').hide();
		$('#ConfigLoadError').hide();
		$ConfigContent.hide();
	}

	function shown()
	{
		Options.loadConfig({
			complete: buildPage,
			configError: loadConfigError,
			serverTemplateError: loadServerTemplateError
			});
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
					if ((option.Name && option.Name.toLowerCase() === name) ||
						(option.name && option.name.toLowerCase() === name))
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

	/*** GENERATE HTML PAGE *****************************************************************/

	function buildOptionsContent(section)
	{
		var html = '';

		var lastmultiid = 1;
		var firstmultioption = true;
		var hasoptions = false;

		for (var i=0, op=0; i < section.options.length; i++)
		{
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
		if (!option.exists)
		{
			value = option.defvalue;
		}

		option.formId = (option.name.indexOf(':') == -1 ? 'S_' : '') + option.name.replace(/ |\/|\\|[\.|$|\:|\*]/g, '_');

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
				(!option.exists && !section.postparam ?
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
		else if (option.name.toLowerCase() === 'serverpassword')
		{
			option.type = 'password';
			html += '<input type="password" id="' + option.formId + '" value="' + Util.textToAttr(value) + '" class="editsmall">';
		}
		else if (option.name.toLowerCase().indexOf('username') > -1 ||
				option.name.toLowerCase().indexOf('password') > -1 ||
				   option.name.indexOf('IP') > -1)
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
			html += '<button class="btn" onclick="' + option.editor.click + '($(\'input\', $(this).closest(\'table\')).attr(\'id\'))">' + option.editor.caption + '</button>';
			html += '</td></tr></table>';
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
			htmldescr = htmldescr.replace(/\</g, 'OPENTAG');
			htmldescr = htmldescr.replace(/\>/g, 'CLOSETAG');
			htmldescr = htmldescr.replace(/OPENTAG/g, '<a class="option" href="#" onclick="Config.scrollToOption(event, this)">');
			htmldescr = htmldescr.replace(/CLOSETAG/g, '</a>');
			htmldescr = htmldescr.replace(/&/g, '&amp;');

			// replace URLs
			var exp = /(http:\/\/[-A-Z0-9+&@#\/%?=~_|!:,.;]*[-A-Z0-9+&@#\/%=~_|])/ig;
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
			html += '<div class="' + section.id + ' multiid' + multiid + ' multiset">';
			html += '<button type="button" class="btn config-delete" data-multiid="' + multiid + ' multiset" ' +
				'onclick="Config.deleteSet(this, \'' + setname + '\',\'' + section.id + '\')">Delete ' + setname + multiid + '</button>';
			html += '<hr>';
			html += '</div>';
		}

		if (!hasmore)
		{
			var nextid = hasoptions ? multiid + 1 : 1;
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

		$ConfigNav.append('<li class="divider"></li>');

		for (var k=0; k < config.length; k++)
		{
			if (k == 1)
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
					var content = buildOptionsContent(section);
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

		$ConfigNav.append('<li class="divider hide ConfigSearch"></li>');
		$ConfigNav.append('<li class="hide ConfigSearch"><a href="#Search">SEARCH RESULTS</a></li>');

		$ConfigNav.toggleClass('long-list', $ConfigNav.children().length > 20);

		showSection('Config-Info');

		if (filterText !== '')
		{
			filterInput(filterText);
		}

		$('#ConfigLoadInfo').hide();
		$ConfigContent.show();
	}

	function extendConfig()
	{
		for (var i=1; i < config.length; i++)
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
			option.description = description !== '' ? description : 'No description available.\n\nNOTE: The script doesn\'t have a description section. '+
				'It\'s either not NZBGet script or a script created for an older NZBGet version and might not work properly.';
			option.exists = true;
			option.nocontent = true;
			firstVisibleSection.options.unshift(option);
		}

		// register editors for options "DefScript" and "ScriptOrder"
		var conf = config[0];
		for (var j=0; j < conf.sections.length; j++)
		{
			var section = conf.sections[j];
			for (var k=0; k < section.options.length; k++)
			{
				var option = section.options[k];
				var optname = option.name.toLowerCase();
				if (optname.indexOf('scriptorder') > -1)
				{
					option.editor = { caption: 'Reorder', click: 'Config.editScriptOrder' };
				}
				if (optname.indexOf('defscript') > -1)
				{
					option.editor = { caption: 'Choose', click: 'Config.editDefScript' };
				}
			}
		}
	}

	function scrollOptionIntoView(optFormId)
	{
		var option = findOptionById(optFormId);

		// switch to tab and scroll the option into view
		showSection(option.sectionId);

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
		var state = $(control).val().toLowerCase();
		$('.btn', $(control).parent()).removeClass('btn-primary');
		$(control).addClass('btn-primary');
	}

	function switchGetValue(control)
	{
		var state = $('.btn-primary', $(control).parent()).val();
		return state;
	}

	/*** CHANGE/ADD/REMOVE OPTIONS *************************************************************/

	function navClick(event)
	{
		event.preventDefault();
		var sectionId = $(this).attr('href').substr(1);
		showSection(sectionId);
	}

	function showSection(sectionId)
	{
		var link = $('a[href="#' + sectionId + '"]', $ConfigNav);
		$('li', $ConfigNav).removeClass('active');
		link.closest('li').addClass('active');
		$ConfigContent.removeClass('search');

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
			$ConfigTitle.text('INFO: SETTINGS');
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

		$Body.animate({ scrollTop: 0 }, { duration: 'slow', easing: 'swing' });
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
		var oldMultiId = -1;
		var newMultiId = 0;
		for (var j=0; j < section.options.length; j++)
		{
			var option = section.options[j];
			if (!option.template)
			{
				if (option.multiid !== oldMultiId)
				{
					oldMultiId = option.multiid;
					newMultiId++;

					// reformat multiid
					var div = $('#' + setname + oldMultiId);
					div.attr('id', setname + newMultiId);

					// update captions
					$('.config-settitle.' + section.id + '.multiid' + oldMultiId, $ConfigData).text(setname + newMultiId);
					$('.' + section.id + '.multiid' + oldMultiId + ' .config-multicaption', $ConfigData).text(setname + newMultiId + '.');
					$('.' + section.id + '.multiid' + oldMultiId + ' .config-delete', $ConfigData).text('Delete ' + setname + newMultiId).attr('data-multiid', newMultiId);

					//update class
					$('.' + section.id + '.multiid' + oldMultiId, $ConfigData).removeClass('multiid' + oldMultiId).addClass('multiid' + newMultiId);
				}

				// update input id
				var oldFormId = option.formId;
				option.formId = option.formId.replace(new RegExp(option.multiid), newMultiId);
				$('#' + oldFormId).attr('id', option.formId);

				// update name
				option.name = option.name.replace(new RegExp(option.multiid), newMultiId);

				option.multiid = newMultiId;
			}
		}

		// update add-button
		var addButton = $('.config-add.' + section.id, $ConfigData);
		addButton.text('Add ' + (newMultiId > 0 ? 'another ' : '') + setname);
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

		div.slideDown('normal', function()
		{
			var opts = div.children();
			opts.detach();
			div.after(opts);
			div.remove();
		});
	}

	/*** OPTION SPECIFIC EDITORS *************************************************/
	this.editScriptOrder = function(optFormId)
	{
		var option = findOptionById(optFormId);
		ScriptListDialog.showModal(option, config);
	}

	this.editDefScript = function(optFormId)
	{
		var option = findOptionById(optFormId);
		ScriptListDialog.showModal(option, config);
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

	function prepareSaveRequest()
	{
		var modified = false;
		var request = [];
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
						if (!option.template && !(option.type === 'info'))
						{
							var oldValue = option.value;
							var newValue = getOptionValue(option);
							if (section.hidden)
							{
								newValue = oldValue;
							}
							modified = modified || (oldValue != newValue) || !option.exists;
							var opt = {Name: option.name, Value: newValue};
							request.push(opt);
						}
					}
					modified = modified || section.modified;
				}
			}
		}

		return modified ? request : [];
	}

	this.saveChanges = function()
	{
		var serverSaveRequest = prepareSaveRequest();

		if (serverSaveRequest.length === 0)
		{
			Notification.show('#Notif_Config_Unchanged');
			return;
		}

		showSaveBanner();

		Util.show('#ConfigSaved_Reload, #ConfigReload', serverSaveRequest.length > 0);
		Util.show('#ConfigClose, #ConfigSaved_Close', serverSaveRequest.length === 0);

		if (serverSaveRequest.length > 0)
		{
			$('#Notif_Config_Failed_Filename').text(Options.option('ConfigFile'));
			RPC.call('saveconfig', [serverSaveRequest], saveCompleted);
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
			Notification.show('#Notif_Config_Failed');
		}
	}

	this.close = function()
	{
		$('#DownloadsTabLink').tab('show');
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
			showSection('Search');
		}
		else
		{
			filterClear();
		}
	}

	function filterClear()
	{
		filterText = '';
		showSection(lastSection);
		$('.ConfigSearch').hide();
		$ConfigTabBadge.hide();
		$ConfigTabBadgeEmpty.show();
	}

	function search()
	{
		$ConfigTabBadge.show();
		$ConfigTabBadgeEmpty.hide();
		$ConfigContent.addClass('search');

		$ConfigData.children().hide();

		var words = filterText.toLowerCase().split(' ');
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
							if (filterOption(option, words))
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

		filterStaticPages(words);

		markLastControlGroup();

		$ConfigTitle.text('SEARCH RESULTS');

		updateTabInfo($ConfigTabBadge, { filter: true, available: available, total: total});
	}

	function filterOption(option, words)
	{
		return filterWords(option.caption + ' ' + option.description + ' ' + option.value, words);
	}

	function filterStaticPages(words)
	{
		$ConfigData.children().filter('.config-static').each(function(index, element)
			{
				var text = $(element).text();
				Util.show(element, filterWords(text, words));
			});
	}

	function filterWords(text, words)
	{
		var search = text.toLowerCase();

		for (var i = 0; i < words.length; i++)
		{
			if (search.indexOf(words[i]) === -1)
			{
				return false;
			}
		}

		return true;
	}

	function markLastControlGroup()
	{
		$ConfigData.children().removeClass('last-group');
		$ConfigData.children().filter(':visible').last().addClass('last-group');
	}

	this.reloadConfirm = function()
	{
		ConfirmDialog.showModal('ReloadConfirmDialog', Config.reload);
	}

	this.reload = function()
	{
		Refresher.pause();

		$('#ConfigReloadAction').text('Stopping all activities and reloading...');
		$('#ConfigReloadInfoNotes').hide();

		$('body').fadeOut(function()
		{
			$('#Navbar, #MainContent').hide();
			$('#ConfigSaved').hide();
			$('body').toggleClass('navfixed', false);
			$('body').show();
			$('#ConfigReloadInfo').fadeIn();
			reloadTime = new Date();
			RPC.call('reload', [], reloadCheckStatus);
		});
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
	var scriptList;
	var orderChanged;
	var orderMode;

	this.init = function()
	{
		$ScriptListDialog = $('#ScriptListDialog');
		$('#ScriptListDialog_Save').click(save);

		$ScriptTable = $('#ScriptListDialog_ScriptTable');

		$ScriptTable.fasttable(
			{
				pagerContainer: $('#ScriptListDialog_ScriptTable_pager'),
				headerCheck: $('#ScriptListDialog_ScriptTable > thead > tr:first-child'),
				infoEmpty: 'No scripts found. If you just changed option "ScriptDir", save settings and reload NZBGet.',
				pageSize: 1000
			});

		$ScriptTable.on('click', 'tbody div.check',
			function(event) { $ScriptTable.fasttable('itemCheckClick', this.parentNode.parentNode, event); });
		$ScriptTable.on('click', 'thead div.check',
			function() { $ScriptTable.fasttable('titleCheckClick') });
		$ScriptTable.on('mousedown', Util.disableShiftMouseDown);

		$ScriptListDialog.on('hidden', function()
		{
			// cleanup
			$ScriptTable.fasttable('update', []);
		});
	}

	this.showModal = function(_option, _config)
	{
		option = _option;
		config = _config;
		orderChanged = false;
		orderMode = option.name === 'ScriptOrder';

		if (orderMode)
		{
			$('#ScriptListDialog_Title').text('Reorder scripts');
			$('#ScriptListDialog_Instruction').text('Hover mouse over table elements for reorder buttons to appear.');
		}
		else
		{
			$('#ScriptListDialog_Title').text('Choose scripts');
			$('#ScriptListDialog_Instruction').html('Select scripts for option <strong>' + option.name + '</strong>.');
		}

		$ScriptTable.toggleClass('table-hidecheck', orderMode);
		$ScriptTable.toggleClass('table-check table-cancheck', !orderMode);
		$('#ScriptListDialog_OrderInfo').toggleClass('alert alert-info', !orderMode);
		Util.show('#ScriptListDialog_OrderInfo', orderMode, 'inline-block');

		buildScriptList();
		var selectedList = parseCommaList(Config.getOptionValue(option));
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
				search: ''
			};
			data.push(item);

			if (!orderMode && selectedList && selectedList.indexOf(scriptName) > -1)
			{
				$ScriptTable.fasttable('checkRow', scriptName, true);
			}
		}
		$ScriptTable.fasttable('update', data);
	}

	function parseCommaList(commaList)
	{
		var valueList = commaList.split(/[,;]+/);
		for (var i=0; i < valueList.length; i++)
		{
			valueList[i] = valueList[i].trim();
			if (valueList[i] === '')
			{
				valueList.splice(i, 1);
				i--;
			}
		}
		return valueList;
	}

	function buildScriptList()
	{
		var orderList = parseCommaList(Config.getOptionValue(Config.findOptionByName('ScriptOrder')));

		var availableScripts = [];
		for (var i=1; i < config.length; i++)
		{
			availableScripts.push(config[i].scriptName);
		}
		availableScripts.sort();

		scriptList = [];

		// first add all scripts from orderList
		for (var i=0; i < orderList.length; i++)
		{
			var scriptName = orderList[i];
			if (availableScripts.indexOf(scriptName) > -1)
			{
				scriptList.push(scriptName);
			}
		}

		// second add all other scripts from script list
		for (var i=0; i < availableScripts.length; i++)
		{
			var scriptName = availableScripts[i];
			if (scriptList.indexOf(scriptName) == -1)
			{
				scriptList.push(scriptName);
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
				if (checkedRows.indexOf(scriptName) > -1)
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
			control.val(scriptList.join(', '));
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
