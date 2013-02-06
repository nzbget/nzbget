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
 *   1) Loading of program options and post-processing script options;
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
	var postTemplateData = null;
	var postTemplateFilename;
	var serverValues;
	var postValues;
	var loadComplete;
	var loadConfigError;
	var loadServerTemplateError;
	var loadPostTemplateError;

	var HIDDEN_SECTION = ['DISPLAY (TERMINAL)', 'POSTPROCESSING-PARAMETERS', 'POST-PROCESSING-PARAMETERS'];
	var POSTPARAM_SECTION = ['POSTPROCESSING-PARAMETERS', 'POST-PROCESSING-PARAMETERS'];

	this.init = function()
	{
	}

	this.update = function()
	{
		// RPC-function "config" returns CURRENT configurations settings loaded in NZBGet
		RPC.call('config', [], function(_options) {
			_this.options = _options;
			initCategories();

			// loading post-processing script parameters config (if exists)
			_this.postParamConfig = [];
			RPC.call('loadconfig', ['POST'], function(postValues)
				{
					if (postValues.length > 0)
					{
						loadPostTemplate(function(data)
							{
								initPostParamConfig(data);
								RPC.next();
							},
							RPC.next);
					}
					else
					{
						RPC.next();
					}
				},
				RPC.next);
		});
	}

	this.cleanup = function()
	{
		serverTemplateData = null;
		postTemplateData = null;
		serverValues = null;
		postValues = null;
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
		loadPostTemplateError = callbacks.postTemplateError;

		// RPC-function "loadconfig" reads the configuration settings from NZBGet configuration file.
		// that's not neccessary the same settings returned by RPC-function "config". This could be the case,
		// for example, if the settings were modified but NZBGet was not restarted.
		RPC.call('loadconfig', ['SERVER'], serverValuesLoaded, loadConfigError);
	}

	function serverValuesLoaded(data)
	{
		serverValues = data;
		$.get('nzbget.conf', serverTemplateLoaded, 'html').error(loadServerTemplateError);
	}

	function serverTemplateLoaded(data)
	{
		serverTemplateData = data;
		RPC.call('loadconfig', ['POST'], postValuesLoaded, loadConfigError);
	}

	function postValuesLoaded(data)
	{
		postValues = data;

		if (postValues.length > 0)
		{
			loadPostTemplate(postTemplateLoaded, function()
				{
					loadPostTemplateError(postTemplateFilename);
					postValues = [];
					complete();
				});
		}
		else
		{
			complete();
		}
	}

	function loadPostTemplate(okCallback, failureCallback)
	{
		// loading post-processing configuration (if the option PostProcess is set)
		var filename = _this.option('PostProcess').replace(/^.*[\\\/]/, ''); // extract file name (remove path)
		if (filename.lastIndexOf('.') > -1)
		{
			filename = filename.substr(0, filename.lastIndexOf('.')) + '.conf'; // replace extension to '.conf'
		}
		else
		{
			filename += '.conf';
		}

		postTemplateFilename = filename;

		$.get(filename, okCallback, 'html').error(failureCallback);
	}

	function postTemplateLoaded(data)
	{
		postTemplateData = data;
		complete();
	}

	function complete()
	{
		if (serverTemplateData === null)
		{
			// the loading was cancelled and the data were discarded (via method "cleanup()")
			return;
		}
		
		var ServerConfig = readConfigTemplate(serverTemplateData, undefined, HIDDEN_SECTION, 'S');
		mergeValues(ServerConfig, serverValues);

		var PostConfig = null;
		if (postTemplateData)
		{
			PostConfig = readConfigTemplate(postTemplateData, undefined, HIDDEN_SECTION, 'P');
			mergeValues(PostConfig, postValues);
		}

		serverValues = null;
		postValues = null;

		loadComplete(ServerConfig, PostConfig);
	}

	/*** PARSE CONFIG AND BUILD INTERNAL STRUCTURES **********************************************/

	function readConfigTemplate(filedata, visiblesections, hiddensections, category)
	{
		var config = [];
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
				section.id = category + '_' + section.name.replace(/ |\/|-/g, '_')
				section.category = category;
				section.options = [];
				description = '';
				section.hidden = !(hiddensections === undefined || (hiddensections.indexOf(section.name) == -1)) ||
					(visiblesections !== undefined && (visiblesections.indexOf(section.name) == -1));
				config.push(section);
			}
			else if (line.substring(0, 2) === '# ' || line === '#')
			{
				if (description !== '')
				{
					description += ' ';
				}
				description += line.substr(1, 1000).trim();
				var lastchar = description.substr(description.length - 1, 1);
				if (lastchar === '.' && firstdescrline === '')
				{
					firstdescrline = description;
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
					section.id = category + '_' + section.name;
					section.category = category;
					section.options = [];
					description = '';
					config.push(section);
				}

				var option = {};
				var enabled = line.substr(0, 1) !== '#';
				option.name = line.substr(enabled ? 0 : 1, line.indexOf('=') - (enabled ? 0 : 1)).trim();
				option.defvalue = line.substr(line.indexOf('=') + 1, 1000).trim();
				option.description = description;
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
				}

				if (option.name.indexOf('1.') > -1)
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

				description = '';
				firstdescrline = '';
			}
			else
			{
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
								newoption.template = false;
								newoption.multiid = k;
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
					var val = findOption(values, option.name);
					if (val)
					{
						option.value = val.Value;
					}
				}
			}
		}
	}
	this.mergeValues = mergeValues;

	function initPostParamConfig(data)
	{
		var postConfig = readConfigTemplate(data, POSTPARAM_SECTION, undefined, 'P');

		_this.postParamConfig = [];

		// delete all sections except of "POSTPROCESSING-PARAMETERS" or "POST-PROCESSING-PARAMETERS"
		for (var i=0; i < postConfig.length; i++)
		{
			var section = postConfig[i];
			if (!section.hidden)
			{
				section.postparam = true;
				_this.postParamConfig.push(section);
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

	// State
	var serverConfig;
	var postConfig;
	var allConfig;
	var filterText = '';
	var lastSection;
	var reloadTime;
	var updateTabInfo;

	this.init = function(options)
	{
		updateTabInfo = options.updateTabInfo;

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
		serverConfig = null;
		postConfig = null;
		allConfig = null;
		$ConfigNav.children().not('.config-static').remove();
		$ConfigData.children().not('.config-static').remove();
	}

	function show()
	{
		removeSaveBanner();
		$('#ConfigSaved').hide();
		$('#ConfigLoadInfo').show();
		$('#ConfigLoadServerTemplateError').hide();
		$('#ConfigLoadPostTemplateError').hide();
		$('#ConfigLoadError').hide();
		$ConfigContent.hide();
	}

	function shown()
	{
		Options.loadConfig({
			complete: buildPage, 
			configError: loadConfigError,
			serverTemplateError: loadServerTemplateError,
			postTemplateError: loadPostTemplateError
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

	function loadPostTemplateError(postTemplateFilename)
	{
		//$('#ConfigLoadInfo').hide();
		$('.ConfigLoadPostTemplateErrorFilename').text(postTemplateFilename);
		$('#ConfigLoadPostTemplateError').show();
	}
	
	function findOptionByName(config, name)
	{
		name = name.toLowerCase();

		for (var i=0; i < config.length; i++)
		{
			var section = config[i];
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
		return null;
	}

	function findOptionById(config, formId)
	{
		for (var i=0; i < config.length; i++)
		{
			var section = config[i];
			for (var j=0; j < section.options.length; j++)
			{
				var option = section.options[j];
				if (option.formId === formId)
				{
					return option;
				}
			}
		}
		return null;
	}

	function findSectionById(sectionId)
	{
		for (var i=0; i < allConfig.length; i++)
		{
			var section = allConfig[i];
			if (section.id === sectionId)
			{
				return section;
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
		if (value === '')
		{
			value = option.defvalue;
		}

		option.formId = section.category + '-' + option.name.replace(/[\.|$|\:|\*]/g, '_');

		var caption = option.caption ? option.caption : option.name;
		if (section.multi)
		{
			caption = '<span class="config-multicaption">' + caption.substring(0, caption.indexOf('.') + 1) + '</span>' + caption.substring(caption.indexOf('.') + 1);
		}

		var html =
			'<div class="control-group ' + section.id + (section.multi ? ' multiid' + option.multiid + ' multiset' : '') + '">'+
				'<label class="control-label nowrap">' +
				'<a class="option-name" href="#" data-category="' + section.category + '" onclick="Config.scrollToOption(event, this)">' + caption + '</a>' +
				(option.value === '' && value !== '' && !section.postparam ?
					' <a data-toggle="modal" href="#ConfigNewOptionHelp" class="label label-info">new</a>' : '') + '</label>'+
				'<div class="controls">';

		if (option.select.length > 1)
		{
			/*
			option.type = 'select';
			html += '<select id="' + option.formId + '">';
			var valfound = false;
			for (var j=0; j < option.select.length; j++)
			{
				pvalue = option.select[j];
				if (value && pvalue.toLowerCase() === value.toLowerCase())
				{
					html += '<option selected="selected">' + pvalue + '</option>';
					valfound = true;
				}
				else
				{
					html += '<option>' + pvalue + '</option>';
				}
			}
			if (!valfound)
			{
				html += '<option selected="selected">' + value + '</option>';
			}
			html += '</select>';
			*/

			option.type = 'switch';
			html +=	'<div class="btn-group btn-switch" id="' + option.formId + '">';

			var valfound = false;
			for (var j=0; j < option.select.length; j++)
			{
				var pvalue = option.select[j];
				if (value && pvalue.toLowerCase() === value.toLowerCase())
				{
					html += '<input type="button" class="btn btn-primary" value="' + pvalue + '" onclick="Config.switchClick(this)">';
					valfound = true;
				}
				else
				{
					html += '<input type="button" class="btn" value="' + pvalue + '" onclick="Config.switchClick(this)">';
				}
			}
			if (!valfound)
			{
				html += '<input type="button" class="btn btn-primary" value="' + value + '" onclick="Config.switchClick(this)">';
			}

			html +='</div>';

		}
		else if (option.select.length === 1)
		{
			option.type = 'numeric';
			html += '<div class="input-append">'+
				'<input type="text" id="' + option.formId + '" value="' + value + '" class="editnumeric">'+
				'<span class="add-on">'+ option.select[0] +'</span>'+
				'</div>';
		}
		else if (option.name.toLowerCase() === 'serverpassword')
		{
			option.type = 'password';
			html += '<input type="password" id="' + option.formId + '" value="' + value + '" class="editsmall">';
		}
		else if (option.name.toLowerCase().indexOf('username') > -1 ||
				option.name.toLowerCase().indexOf('password') > -1 ||
				   option.name.indexOf('IP') > -1)
		{
			option.type = 'text';
			html += '<input type="text" id="' + option.formId + '" value="' + value + '" class="editsmall">';
		}
		else
		{
			option.type = 'text';
			html += '<input type="text" id="' + option.formId + '" value="' + value + '" class="editlarge">';
		}

		if (option.description !== '')
		{
			var htmldescr = option.description;
			htmldescr = htmldescr.replace(/NOTE: do not forget to uncomment the next line.\n/, '');
			htmldescr = htmldescr.replace(/\</g, 'OPENTAG');
			htmldescr = htmldescr.replace(/\>/g, 'CLOSETAG');
			htmldescr = htmldescr.replace(/OPENTAG/g, '<a class="option" href="#" data-category="' + section.category + '" onclick="Config.scrollToOption(event, this)">');
			htmldescr = htmldescr.replace(/CLOSETAG/g, '</a>');
			htmldescr = htmldescr.replace(/&/g, '&amp;');

			// highlight first line
			htmldescr = htmldescr.replace(/\n/, '</span>\n');
			htmldescr = '<span class="help-option-title">' + htmldescr;

			htmldescr = htmldescr.replace(/\n/g, '<br>');
			htmldescr = htmldescr.replace(/NOTE: /g, '<span class="label label-warning">NOTE:</span> ');

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
		var name = option.name;
		var setname = name.substr(0, name.indexOf('.'));
		var html = '<div class="config-settitle ' + section.id + ' multiid' + multiid + ' multiset">' + setname + '</div>';
		return html;
	}

	function buildMultiRowEnd(section, multiid, hasmore, hasoptions)
	{
		var name = section.options[0].name;
		var setname = name.substr(0, name.indexOf('1'));
		var html = '';

		if (hasoptions)
		{
			html += '<div class="' + section.id + ' multiid' + multiid + ' multiset">';
			html += '<button type="button" class="btn config-delete" data-multiid="' + multiid + ' multiset" ' +
				'onclick="Config.deleteSet(this, \'' + setname + '\',\'' + section.id + '\',\'' + section.category + '\')">Delete ' + setname + multiid + '</button>';
			html += '<hr>';
			html += '</div>';
		}

		if (!hasmore)
		{
			var nextid = hasoptions ? multiid + 1 : 1;
			html += '<div class="' + section.id + '">';
			html += '<button type="button" class="btn config-add ' + section.id + ' multiset" onclick="Config.addSet(\'' + setname + '\',\'' + section.id + '\',\'' + section.category +
			  '\')">Add ' + (hasoptions ? 'another ' : '') + setname + '</button>';
			html += '</div>';
		}

		return html;
	}

	function buildConfig(config, category)
	{
		for (var i=0; i < config.length; i++)
		{
			var section = config[i];
			if (!section.hidden)
			{
				var html = $('<li><a href="#' + section.id + '">' + section.name + '</a></li>');
				$ConfigNav.append(html);

				var content = buildOptionsContent(section);
				$ConfigData.append(content);
			}
		}
	}

	function buildPage(_serverConfig, _postConfig)
	{
		serverConfig = _serverConfig;
		postConfig = _postConfig;
		allConfig = [];
		allConfig.push.apply(allConfig, serverConfig);

		if (postConfig)
		{
			allConfig.push.apply(allConfig, postConfig);
		}

		$ConfigNav.children().not('.config-static').remove();
		$ConfigData.children().not('.config-static').remove();

		$ConfigNav.append('<li class="nav-header">NZBGet Settings</li>');
		buildConfig(serverConfig, 'S');

		if (postConfig)
		{
			$ConfigNav.append('<li class="nav-header">Post-Processing Script Settings</li>');
			buildConfig(postConfig, 'P');
		}

		$ConfigNav.append('<li class="divider hide ConfigSearch"></li>');
		$ConfigNav.append('<li class="hide ConfigSearch"><a href="#Search">SEARCH RESULTS</a></li>');

		showSection('Config-Info');

		if (filterText !== '')
		{
			filterInput(filterText);
		}

		$('#ConfigLoadInfo').hide();
		$ConfigContent.show();
	}

	function scrollOptionIntoView(optFormId)
	{
		var category = optFormId.substr(0, 1);
		var config = category === 'S' ? serverConfig : postConfig;
		var option = findOptionById(config, optFormId);

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
		$ConfigTitle.text(section.name);
	}

	this.deleteSet = function(control, setname, sectionId, sectionCategory)
	{
		var multiid = parseInt($(control).attr('data-multiid'));
		$('#ConfigDeleteConfirmDialog_Option').text(setname + multiid);
		ConfirmDialog.showModal('ConfigDeleteConfirmDialog', function()
		{
			deleteOptionSet(setname, multiid, sectionId, sectionCategory);
		});
	}

	function deleteOptionSet(setname, multiid, sectionId, sectionCategory)
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
					var div = $('#' + section.category + '-' + setname + oldMultiId);
					div.attr('id', section.category + '-' + setname + newMultiId);

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

	this.addSet = function(setname, sectionId, sectionCategory)
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

	function prepareSaveRequest(config, category)
	{
		var modified = false;
		var request = [];
		for (var i=0; i < config.length; i++)
		{
			var section = config[i];
			for (var j=0; j < section.options.length; j++)
			{
				var option = section.options[j];
				if (!option.template)
				{
					var oldValue = option.value;
					var newValue = getOptionValue(option);
					if (section.hidden)
					{
						newValue = oldValue;
					}
					modified = modified || (oldValue != newValue);
					var opt = {Name: option.name, Value: newValue};
					request.push(opt);
				}
			}
			modified = modified || section.modified;
		}

		return modified ? request : [];
	}

	this.saveChanges = function()
	{
		var serverSaveRequest = prepareSaveRequest(serverConfig);
		var postSaveRequest = null;
		if (postConfig)
		{
			postSaveRequest = prepareSaveRequest(postConfig);
		}

		if (serverSaveRequest.length === 0 && (!postSaveRequest || postSaveRequest.length === 0))
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
			RPC.call('saveconfig', ['SERVER', serverSaveRequest],
				function(result)
				{
					if (result && postSaveRequest && postSaveRequest.length > 0)
					{
						$('#Notif_Config_Failed_Filename').text(Options.option('PostConfigFile'));
						RPC.call('saveconfig', ['POST', postSaveRequest], saveCompleted);
					}
					else
					{
						saveCompleted(result);
					}
				});
		}
		else if (postSaveRequest && postSaveRequest.length > 0)
		{
			$('#Notif_Config_Failed_Filename').text(Options.option('PostConfigFile'));
			RPC.call('saveconfig', ['POST', postSaveRequest], saveCompleted);
		}
	}

	function showSaveBanner()
	{
		//TODO: replace with a better "saving progress"-indicator
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
			$('#ConfigLoadPostTemplateError').hide();
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

		var optname = $(control).text();
		var category = $(control).attr('data-category') || 'S';
		var config = category === 'S' ? serverConfig : postConfig;
		var option = findOptionByName(config, optname);
		if (!option)
		{
			config = category !== 'S' ? serverConfig : postConfig;
			option = findOptionByName(config, optname);
		}

		if (option)
		{
			scrollOptionIntoView(option.formId);
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

		for (var i=0; i < allConfig.length; i++)
		{
			var section = allConfig[i];
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

		filterStaticPages(words);

		markLastControlGroup();

		$ConfigTitle.text('SEARCH RESULTS');

		updateTabInfo($ConfigTabBadge, { filter: true, available: available, total: total});
	}

	function filterOption(option, words)
	{
		return filterWords(option.name + ' ' + option.description + ' ' + option.value, words);
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
