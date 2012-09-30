/*
 *	This file is part of nzbget
 *
 *	Copyright (C) 2012 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * $Revision$
 * $Date$
 *
 */

/* controls */
var config_ConfigNav;
var config_ConfigData;
var config_ConfigTabBadge;
var config_ConfigTabBadgeEmpty;

var config_ServerTemplateData = null;
var config_PostTemplateData = null;
var config_ServerConfig;
var config_PostConfig;
var config_AllConfig;
var config_ServerValues;
var config_PostValues;
var config_ConfigTable;
var config_filterText = '';
var config_lastSection;
var config_ReloadTime;

var config_HIDDEN_SECTION = ['DISPLAY (TERMINAL)', 'POSTPROCESSING-PARAMETERS', 'POST-PROCESSING-PARAMETERS'];

function config_init()
{
	config_ConfigNav = $('#ConfigNav');
	config_ConfigData = $('#ConfigData');
	config_ConfigTabBadge = $('#ConfigTabBadge');
	config_ConfigTabBadgeEmpty = $('#ConfigTabBadgeEmpty');
	$('#ConfigTable_filter').val('');

	$('#ConfigTabLink').on('show', config_show);
	$('#ConfigTabLink').on('shown', config_shown);

	config_ConfigNav.on('click', 'li > a', config_nav_click);

	config_ConfigTable = $({});
	config_ConfigTable.fasttable(
		{
			filterInput: $('#ConfigTable_filter'),
			filterClearButton: $("#ConfigTable_clearfilter"),
			filterInputCallback: config_filterInput,
			filterClearCallback: config_filterClear
		});
}

function config_cleanup()
{
	config_ServerTemplateData = null;
	config_PostTemplateData = null;
	config_ServerConfig = null;
	config_PostConfig = null;
	config_AllConfig = null;
	config_ServerValues = null;
	config_PostValues = null;
	config_ConfigNav.children().not('.config-static').remove();
	config_ConfigData.children().not('.config-static').remove();
}

function config_update()
{
	// RPC-function "config" returns CURRENT configurations settings loaded in NZBGet
	rpc('config', [], function(config) {
		Config = config;
		config_initCategories();
		loadNext();
	});
}

function config_show()
{
	config_removeSaveBanner();
	$('#ConfigSaved').hide();
	$('#ConfigLoadInfo').show();
	$('#ConfigLoadServerTemplateError').hide();
	$('#ConfigLoadPostTemplateError').hide();
	$('#ConfigLoadError').hide();
	$('#ConfigContent').hide();
}

function config_shown()
{
	config_loadConfig();
}

/*** LOADING CONFIG ********************************************************************/

function config_loadConfig()
{
	// RPC-function "loadconfig" reads the configuration settings from NZBGet configuration file.
	// that's not neccessary the same settings returned by RPC-function "config". Tha could be the case,
	// for example, if the settings were modified but NZBGet was not restarted.
	rpc('loadconfig', ['SERVER'], config_serverValuesLoaded, config_loadError);
}

function config_loadError(message, resultObj)
{
	$('#ConfigLoadInfo').hide();
	$('#ConfigLoadError').show();
	if (resultObj && resultObj.error && resultObj.error.message)
	{
		message = resultObj.error.message;
	}
	$('#ConfigLoadErrorText').text(message);
}

function config_serverValuesLoaded(data)
{
	config_ServerValues = data;
	$.get('nzbget.conf', config_serverTemplateLoaded, 'html').error(
		function ()
		{
			$('#ConfigLoadInfo').hide();
			$('#ConfigLoadServerTemplateError').show();
		}
	);
}

function config_serverTemplateLoaded(data)
{
	config_ServerTemplateData = data;
	rpc('loadconfig', ['POST'], config_postValuesLoaded, config_loadError);
}

function config_postValuesLoaded(data)
{
	config_PostValues = data;

	if (config_PostValues.length > 0)
	{
		// loading post-processing configuration (if the option PostProcess is set)
		var optionPostProcess = config_FindOption(Config, 'PostProcess');
		var filename = optionPostProcess.Value.replace(/^.*[\\\/]/, ''); // extract file name (remove path)
		if (filename.lastIndexOf('.') > -1)
		{
			filename = filename.substr(0, filename.lastIndexOf('.')) + '.conf'; // replace extension to '.conf'
		}
		else
		{
			filename += '.conf';
		}

		$.get(filename, config_postTemplateLoaded, 'html').error(
			function ()
			{
				//$('#ConfigLoadInfo').hide();
				$('.ConfigLoadPostTemplateErrorFilename').text(filename);
				$('#ConfigLoadPostTemplateError').show();
				config_PostValues = null;
				config_loadComplete();
			}
		);
	}
	else
	{
		config_loadComplete();
	}
}

function config_postTemplateLoaded(data)
{
	config_PostTemplateData = data;
	config_loadComplete();
}

function config_loadComplete()
{
	config_build();
	$('#ConfigLoadInfo').hide();
	$('#ConfigContent').show();
}

/*** PARSE CONFIG AND BUILD INTERNAL STRUCTURES **********************************************/

function config_readConfigTemplate(filedata, hiddensections, category)
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
			section.hidden = !(hiddensections === undefined || hiddensections.indexOf(section.name) == -1);
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

function config_FindOption(options, name)
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

function config_FindOptionGlobal(config, name)
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

function config_FindOptionById(config, formId)
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

function config_FindSectionById(sectionId)
{
	for (var i=0; i < config_AllConfig.length; i++)
	{
		var section = config_AllConfig[i];
		if (section.id === sectionId)
		{
			return section;
		}
	}
	return null;
}

function config_MergeValues(config, values)
{
	// copy values
	for (var i=0; i < config.length; i++)
	{
		var section = config[i];
		if (section.multi)
		{
			// multi sections (news-servers, scheduler)

			var subexists = true;
			for (k = 1; subexists; k++)
			{
				subexists = false;
				for (var m=0; m < section.options.length; m++)
				{
					var option = section.options[m];
					if (option.name.indexOf('1.') > -1)
					{
						var name = option.name.replace(/1/, k);
						var val = config_FindOption(values, name);
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
							var val = config_FindOption(values, name);
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
				var val = config_FindOption(values, option.name);
				if (val)
				{
					option.value = val.Value;
				}
			}
		}
	}
}

function config_initCategories()
{
	Categories = [];
	for (var i=0; i < Config.length; i++)
	{
		var option = Config[i];
		if ((option.Name.toLowerCase().substring(0, 8) === 'category') && (option.Name.toLowerCase().indexOf('.name') > -1))
		{
			Categories.push(option.Value);
		}
	}
}

/*** GENERATE HTML PAGE *****************************************************************/

function config_BuildOptionsContent(section, sectionframe)
{
	var html = '';

	if (sectionframe)
	{
		html += '<div class = "block"><center><b>' + section.name + '</b></center><br>';
	}

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
				html += config_BuildMultiRowEnd(section, lastmultiid, true, true);
				lastmultiid = option.multiid;
				firstmultioption = true;
			}
			if (section.multi && firstmultioption)
			{
				html += config_BuildMultiRowStart(section, option.multiid, option);
				firstmultioption = false;
			}
			html += config_BuildOptionRow(option, section);
			hasoptions = true;
			op++;
		}
	}

	if (section.multi)
	{
		html += config_BuildMultiRowEnd(section, lastmultiid, false, hasoptions);
	}

	if (sectionframe)
	{
		html += '</div><br>';
	}

	return html;
}

function config_BuildMultiSetContent(section, multiid)
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
				html += config_BuildMultiRowStart(section, multiid, option);
				firstmultioption = false;
			}
			html += config_BuildOptionRow(option, section);
			hasoptions = true;
			op++;
		}
	}
	html += config_BuildMultiRowEnd(section, multiid, true, hasoptions);

	return html;
}

function config_BuildOptionRow(option, section)
{
	var value = option.value;
	if (value === '')
	{
		value = option.defvalue;
	}

	option.formId = section.category + '-' + option.name.replace(/[\.|$]/g, '_');

	var caption = option.name;
	if (section.multi)
	{
		caption = '<span class="config-multicaption">' + caption.substring(0, caption.indexOf('.') + 1) + '</span>' + caption.substring(caption.indexOf('.') + 1);
	}

	var html =
		'<div class="control-group ' + section.id + (section.multi ? ' multiid' + option.multiid + ' multiset' : '') + '">'+
			'<label class="control-label nowrap">' + caption + (option.value === '' && value !== '' ? ' <a data-toggle="modal" href="#ConfigNewOptionHelp" class="label label-info">new</a>' : '') + '</label>'+
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
			pvalue = option.select[j];
			if (value && pvalue.toLowerCase() === value.toLowerCase())
			{
				html += '<input type="button" class="btn btn-primary" value="' + pvalue + '" onclick="switch_click(this)">';
				valfound = true;
			}
			else
			{
				html += '<input type="button" class="btn" value="' + pvalue + '" onclick="switch_click(this)">';
			}
		}
		if (!valfound)
		{
			html += '<input type="button" class="btn btn-primary" value="' + value + '" onclick="switch_click(this)">';
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
		htmldescr = htmldescr.replace(/OPENTAG/g, '<a class="option" href="#" data-category="' + section.category + '" onclick="config_scrollToOption(event, this)">');
		htmldescr = htmldescr.replace(/CLOSETAG/g, '</a>');
		htmldescr = htmldescr.replace(/&/g, '&amp;');

		// highlight first line
		htmldescr = htmldescr.replace(/\n/, '</span>\n');
		htmldescr = '<span class="help-option-title">' + htmldescr;

		htmldescr = htmldescr.replace(/\n/g, '<br>');
		htmldescr = htmldescr.replace(/NOTE: /g, '<span class="label label-warning">NOTE:</span> ');

		if (htmldescr.indexOf('INFO FOR DEVELOPERS:') > -1)
		{
			htmldescr = htmldescr.replace(/INFO FOR DEVELOPERS:<br>/g, '<input class="btn btn-mini" value="Show more info for developers" type="button" onclick="config_showSpoiler(this)"><span class="hide">');
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

function config_BuildMultiRowStart(section, multiid, option)
{
	var name = option.name;
	var setname = name.substr(0, name.indexOf('.'));
	var html = '<div class="config-settitle ' + section.id + ' multiid' + multiid + ' multiset">' + setname + '</div>';
	return html;
}

function config_BuildMultiRowEnd(section, multiid, hasmore, hasoptions)
{
	var name = section.options[0].name;
	var setname = name.substr(0, name.indexOf('1'));
	var html = '';

	if (hasoptions)
	{
		html += '<div class="' + section.id + ' multiid' + multiid + ' multiset">';
		html += '<button type="button" class="btn config-delete" data-multiid="' + multiid + ' multiset" ' +
			'onclick="config_delete_click(this, \'' + setname + '\',\'' + section.id + '\',\'' + section.category + '\')">Delete ' + setname + multiid + '</button>';
		html += '<hr>';
		html += '</div>';
	}

	if (!hasmore)
	{
		var nextid = hasoptions ? multiid + 1 : 1;
		html += '<div class="' + section.id + '">';
		html += '<button type="button" class="btn config-add ' + section.id + ' multiset" onclick="config_add_click(\'' + setname + '\',\'' + section.id + '\',\'' + section.category +
		  '\')">Add ' + (hasoptions ? 'another ' : '') + setname + '</button>';
		html += '</div>';
	}

	return html;
}

function config_buildConfig(config, category)
{
	for (var i=0; i < config.length; i++)
	{
		var section = config[i];
		if (!section.hidden)
		{
			var html = $('<li><a href="#' + section.id + '">' + section.name + '</a></li>');
			config_ConfigNav.append(html);

			var content = config_BuildOptionsContent(section, false);
			config_ConfigData.append(content);
		}
	}
}

function config_build()
{
	config_ServerConfig = config_readConfigTemplate(config_ServerTemplateData, config_HIDDEN_SECTION, 'S');
	config_MergeValues(config_ServerConfig, config_ServerValues);

	config_AllConfig = [];
	config_AllConfig.push.apply(config_AllConfig, config_ServerConfig);

	if (config_PostTemplateData)
	{
		config_PostConfig = config_readConfigTemplate(config_PostTemplateData, config_HIDDEN_SECTION, 'P');
		config_MergeValues(config_PostConfig, config_PostValues);

		config_AllConfig.push.apply(config_AllConfig, config_PostConfig);
	}

	config_ConfigNav.children().not('.config-static').remove();
	config_ConfigData.children().not('.config-static').remove();

	config_ConfigNav.append('<li class="nav-header">NZBGet Settings</li>');
	config_buildConfig(config_ServerConfig, 'S');

	if (config_PostConfig)
	{
		config_ConfigNav.append('<li class="nav-header">Post-Processing Script Settings</li>');
		config_buildConfig(config_PostConfig, 'P');
	}

	config_ConfigNav.append('<li class="divider hide ConfigSearch"></li>');
	config_ConfigNav.append('<li class="hide ConfigSearch"><a href="#Search">SEARCH RESULTS</a></li>');

	config_showSection('Config-Info');

	if (config_filterText !== '')
	{
		config_filterInput(config_filterText);
	}
}

function config_scrollOptionIntoView(optFormId)
{
	var category = optFormId.substr(0, 1);
	var config = category === 'S' ? config_ServerConfig : config_PostConfig;
	var option = config_FindOptionById(config, optFormId);

	// switch to tab and scroll the option into view
	config_showSection(option.sectionId);

	var element = $('#' + option.formId);
	var parent = $('html,body');

	parent[0].scrollIntoView(true);
	var offsetY = 15;
	if ($('body').hasClass('navfixed')) {
		offsetY = 55;
	}
    parent.animate({ scrollTop: parent.scrollTop() + element.offset().top - parent.offset().top - offsetY }, { duration: 'slow', easing: 'swing' });
}

/*** CHANGE/ADD/REMOVE OPTIONS *************************************************************/

function config_nav_click(event)
{
	event.preventDefault();
	var sectionId = $(this).attr('href').substr(1);
	config_showSection(sectionId);
}

function config_showSection(sectionId)
{
	var link = $('a[href="#' + sectionId + '"]', config_ConfigNav);
	$('li', config_ConfigNav).removeClass('active');
	link.closest('li').addClass('active');
	$('#ConfigContent').removeClass('search');

	$('#ConfigInfo').hide();

	if (sectionId === 'Search')
	{
		config_search();
		return;
	}

	config_lastSection = sectionId;

	if (sectionId === 'Config-Info')
	{
		$('#ConfigInfo').show();
		config_ConfigData.children().hide();
		$('#ConfigTitle').text('INFO: SETTINGS');
		return;
	}

	if (sectionId === 'Config-System')
	{
		config_ConfigData.children().hide();
		$('.config-system', config_ConfigData).show();
		config_markLastControlGroup();
		$('#ConfigTitle').text('SYSTEM');
		return;
	}

	config_ConfigData.children().hide();
	var opts = $('.' + sectionId, config_ConfigData);
	opts.show();
	config_markLastControlGroup();

	var section = config_FindSectionById(sectionId);
	$('#ConfigTitle').text(section.name);
}

function config_delete_click(control, setname, sectionId, sectionCategory)
{
	var multiid = parseInt($(control).attr('data-multiid'));
	$('#ConfigDeleteConfirmDialog_Option').text(setname + multiid);
	confirm_dialog_show('ConfigDeleteConfirmDialog', function()
	{
		config_delete(setname, multiid, sectionId, sectionCategory);
	});
}

function config_delete(setname, multiid, sectionId, sectionCategory)
{
	// remove options from page, using a temporary div for slide effect
	var opts = $('.' + sectionId + '.multiid' + multiid, config_ConfigData);
	var div = $('<div></div>');
	opts.first().before(div);
	div.append(opts);
	div.slideUp('normal', function()
	{
		div.remove();
	});

	// remove option set from config
	var section = config_FindSectionById(sectionId);
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
	config_ReformatSection(section, setname);

	section.modified = true;
}

function config_ReformatSection(section, setname)
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
				$('.config-settitle.' + section.id + '.multiid' + oldMultiId, config_ConfigData).text(setname + newMultiId);
				$('.' + section.id + '.multiid' + oldMultiId + ' .config-multicaption', config_ConfigData).text(setname + newMultiId + '.');
				$('.' + section.id + '.multiid' + oldMultiId + ' .config-delete', config_ConfigData).text('Delete ' + setname + newMultiId).attr('data-multiid', newMultiId);

				//update class
				$('.' + section.id + '.multiid' + oldMultiId, config_ConfigData).removeClass('multiid' + oldMultiId).addClass('multiid' + newMultiId);
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
	var addButton = $('.config-add.' + section.id, config_ConfigData);
	addButton.text('Add ' + (newMultiId > 0 ? 'another ' : '') + setname);
}

function config_add_click(setname, sectionId, sectionCategory)
{
	// find section
	var section = config_FindSectionById(sectionId);

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
	var html = config_BuildMultiSetContent(section, multiid);

	var addButton = $('.config-add.' + section.id, config_ConfigData);
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

function config_GetOptionValue(option)
{
	var control = $('#' + option.formId);
	if (option.type === 'switch')
	{
		return switch_getValue(control);
	}
	else
	{
		return control.val();
	}
}

function config_PrepareSaveRequest(config, category)
{
	var modified = false;
	request = [];
	for (var i=0; i < config.length; i++)
	{
		var section = config[i];
		for (var j=0; j < section.options.length; j++)
		{
			var option = section.options[j];
			if (!option.template)
			{
				var oldValue = option.value;
				var newValue = config_GetOptionValue(option);
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

function config_Save()
{
	var serverSaveRequest = config_PrepareSaveRequest(config_ServerConfig);
	var postSaveRequest = null;
	if (config_PostValues.length > 0)
	{
		postSaveRequest = config_PrepareSaveRequest(config_PostConfig);
	}

	if (serverSaveRequest.length === 0 && (!postSaveRequest || postSaveRequest.length === 0))
	{
		animateAlert('#Notif_Config_Unchanged');
		return;
	}

	config_showSaveBanner();

	show('#ConfigSaved_Reload, #ConfigReload', serverSaveRequest.length > 0);
	show('#ConfigClose, #ConfigSaved_Close', serverSaveRequest.length === 0);

	if (serverSaveRequest.length > 0)
	{
		$('#Notif_Config_Failed_Filename').text(config_FindOption(Config, 'ConfigFile').Value);
		rpc('saveconfig', ['SERVER', serverSaveRequest],
			function(result)
			{
				if (result && postSaveRequest && postSaveRequest.length > 0)
				{
					$('#Notif_Config_Failed_Filename').text(config_FindOption(Config, 'PostConfigFile').Value);
					rpc('saveconfig', ['POST', postSaveRequest], config_save_completed);
				}
				else
				{
					config_save_completed(result);
				}
			});
	}
	else if (postSaveRequest && postSaveRequest.length > 0)
	{
		$('#Notif_Config_Failed_Filename').text(config_FindOption(Config, 'PostConfigFile').Value);
		rpc('saveconfig', ['POST', postSaveRequest], config_save_completed);
	}
}

function config_showSaveBanner()
{
	//TODO: replace with a better "saving progress"-indicator
	$('#Config_Save').attr('disabled', 'disabled');
}

function config_removeSaveBanner()
{
	$('#Config_Save').removeAttr('disabled');
}

function config_save_completed(result)
{
	config_removeSaveBanner();
	if (result)
	{
		$('#ConfigContent').fadeOut(function() { $('#ConfigSaved').fadeIn(); });
	}
	else
	{
		animateAlert('#Notif_Config_Failed');
	}
}

function config_Close()
{
	$('#DownloadsTabLink').tab('show');
}

function config_scrollToOption(event, control)
{
	event.preventDefault();
	var optname = $(control).text();
	var category = $(control).attr('data-category') || 'S';
	var config = category === 'S' ? config_ServerConfig : config_PostConfig;
	var option = config_FindOptionGlobal(config, optname);
	if (!option)
	{
		config = category !== 'S' ? config_ServerConfig : config_PostConfig;
		option = config_FindOptionGlobal(config, optname);
	}

	if (option)
	{
		config_scrollOptionIntoView(option.formId);
	}
}

function config_showSpoiler(control)
{
	$(control).hide();
	$(control).next().show();
}

function config_filterInput(value)
{
	config_filterText = value;
	if (config_filterText.trim() !== '')
	{
		$('.ConfigSearch').show();
		config_showSection('Search');
	}
	else
	{
		config_filterClear();
	}
}

function config_filterClear()
{
	config_filterText = '';
	config_showSection(config_lastSection);
	$('.ConfigSearch').hide();
	config_ConfigTabBadge.hide();
	config_ConfigTabBadgeEmpty.show();
}

function config_search()
{
	config_ConfigTabBadge.show();
	config_ConfigTabBadgeEmpty.hide();
	$('#ConfigContent').addClass('search');

	config_ConfigData.children().hide();

	var words = config_filterText.toLowerCase().split(' ');
	var total = 0;
	var available = 0;

	for (var i=0; i < config_AllConfig.length; i++)
	{
		var section = config_AllConfig[i];
		if (!section.hidden)
		{
			for (var j=0; j < section.options.length; j++)
			{
				var option = section.options[j];
				if (!option.template)
				{
					total++;
					if (config_filterOption(option, words))
					{
						available++;
						var opt = $('#' + option.formId).closest('.control-group');
						opt.show();
					}
				}
			}
		}
	}

	config_filterStaticPages(words);

	config_markLastControlGroup();

	$('#ConfigTitle').text('SEARCH RESULTS');

	tab_updateInfo(config_ConfigTabBadge, { filter: true, available: available, total: total});
}

function config_filterOption(option, words)
{
	return config_filterWords(option.name + ' ' + option.description + ' ' + option.value, words);
}

function config_filterStaticPages(words)
{
	config_ConfigData.children().filter('.config-static').each(function(index, element)
		{
			var text = $(element).text();
			show(element, config_filterWords(text, words));
		});
}

function config_filterWords(text, words)
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

function config_markLastControlGroup()
{
	config_ConfigData.children().removeClass('last-group');
	config_ConfigData.children().filter(':visible').last().addClass('last-group');
}

function config_ReloadConfirm()
{
	confirm_dialog_show('ReloadConfirmDialog', config_Reload);
}

function config_Reload()
{
	refresh_pause();

	$('#ConfigReloadAction').text('Stopping all activities and reloading...');
	$('#ConfigReloadInfoNotes').hide();

	$('body').fadeOut(function()
	{
		$('#Navbar, #MainContent').hide();
		$('#ConfigSaved').hide();
		$('body').toggleClass('navfixed', false);
		$('body').show();
		$('#ConfigReloadInfo').fadeIn();
		config_ReloadTime = new Date();
		rpc('reload', [], config_Reload_CheckStatus);
	});
}

function config_Reload_CheckStatus()
{
	rpc('status', [], function(status)
	{
		// OK, checking if it is a restarted instance
		if (status.UpTimeSec >= Status.UpTimeSec)
		{
			// the old instance is not restarted yet
			// waiting 0.5 sec. and retrying
			setTimeout(config_Reload_CheckStatus, 500);
			config_Reload_CheckNotes();
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
		setTimeout(config_Reload_CheckStatus, 500);
		config_Reload_CheckNotes();
	});
}

function config_Reload_CheckNotes()
{
	// if reload takes more than 30 sec. show additional tips
	if (new Date() - config_ReloadTime > 30000)
	{
		$('#ConfigReloadInfoNotes').show(1000);
	}
}
