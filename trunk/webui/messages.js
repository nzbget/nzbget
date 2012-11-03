/*
 * This file is part of nzbget
 *
 * Copyright (C) 2012 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *   1) Messages tab.
 */
 
/*** MESSAGES TAB *********************************************************************/
 
var Messages = (new function($)
{
	'use strict';
	
	// Controls
	var $MessagesTable;
	var $MessagesTabBadge;
	var $MessagesTabBadgeEmpty;
	var $MessagesRecordsPerPage;

	// State
	var messages;
	var maxMessages = null;
	var lastID = 0;
	var updateTabInfo;

	this.init = function(options)
	{
		updateTabInfo = options.updateTabInfo;
		
		$MessagesTable = $('#MessagesTable');
		$MessagesTabBadge = $('#MessagesTabBadge');
		$MessagesTabBadgeEmpty = $('#MessagesTabBadgeEmpty');
		$MessagesRecordsPerPage = $('#MessagesRecordsPerPage');

		var recordsPerPage = UISettings.read('MessagesRecordsPerPage', 10);
		$MessagesRecordsPerPage.val(recordsPerPage);

		$MessagesTable.fasttable(
			{
				filterInput: '#MessagesTable_filter',
				filterClearButton: '#MessagesTable_clearfilter',
				pagerContainer: '#MessagesTable_pager',
				infoContainer: '#MessagesTable_info',
				filterCaseSensitive: false,
				pageSize: recordsPerPage,
				maxPages: UISettings.miniTheme ? 1 : 5,
				pageDots: !UISettings.miniTheme,
				fillFieldsCallback: fillFieldsCallback,
				fillSearchCallback: fillSearchCallback,
				renderCellCallback: renderCellCallback,
				updateInfoCallback: updateInfo
			});
	}

	this.applyTheme = function()
	{
		$MessagesTable.fasttable('setPageSize', UISettings.read('MessagesRecordsPerPage', 10), 
			UISettings.miniTheme ? 1 : 5, !UISettings.miniTheme);
	}

	this.update = function()
	{
		if (maxMessages === null)
		{
			maxMessages = parseInt(Options.option('LogBufferSize'));
		}
		
		if (lastID === 0)
		{
			RPC.call('log', [0, maxMessages], loaded);
		}
		else
		{
			RPC.call('log', [lastID+1, 0], loaded);
		}
	}

	function loaded(newMessages)
	{
		merge(newMessages);
		RPC.next();
	}
	
	function merge(newMessages)
	{
		if (lastID === 0)
		{
			messages = newMessages;
		}
		else
		{
			messages = messages.concat(newMessages);
			messages.splice(0, messages.length-maxMessages);
		}

		if (messages.length > 0)
		{
			lastID = messages[messages.length-1].ID;
		}
	}

	this.redraw = function()
	{
		var data = [];

		for (var i=0; i < messages.length; i++)
		{
			var message = messages[i];

			var item =
			{
				id: message.ID,
				message: message
			};

			data.unshift(item);
		}

		$MessagesTable.fasttable('update', data);

		Util.show($MessagesTabBadge, messages.length > 0);
		Util.show($MessagesTabBadgeEmpty, messages.length === 0 && UISettings.miniTheme);
	}

	function fillFieldsCallback(item)
	{
		var message = item.message;

		var kind;
		switch (message.Kind)
		{
			case 'INFO': kind = '<span class="label label-status label-success">info</span>'; break;
			case 'DETAIL': kind = '<span class="label label-status label-info">detail</span>'; break;
			case 'WARNING': kind = '<span class="label label-status label-warning">warning</span>'; break;
			case 'ERROR': kind = '<span class="label label-status label-important">error</span>'; break;
			case 'DEBUG': kind = '<span class="label label-status">debug</span>'; break;
		}

		var text = Util.textToHtml(message.Text);
		if (!item.time)
		{
			item.time = Util.formatDateTime(message.Time + UISettings.timeZoneCorrection*60*60);
		}

		if (!UISettings.miniTheme)
		{
			item.fields = [kind, item.time, text];
		}
		else
		{
			var info = kind + ' <span class="label">' + item.time + '</span> ' + text;
			item.fields = [info];
		}
	}

	function fillSearchCallback(item)
	{
		if (!item.time)
		{
			item.time = Util.formatDateTime(item.message.Time + UISettings.timeZoneCorrection*60*60);
		}

		item.search = item.message.Kind + ' ' + item.time + ' ' + item.message.Text;
	}

	function renderCellCallback(cell, index, item)
	{
		if (index === 1)
		{
			cell.className = 'text-center';
		}
	}
	
	function updateInfo(stat)
	{
		updateTabInfo($MessagesTabBadge, stat);
	}

	this.recordsPerPageChange = function()
	{
		var val = $MessagesRecordsPerPage.val();
		UISettings.write('MessagesRecordsPerPage', val);
		$MessagesTable.fasttable('setPageSize', val);
	}
}(jQuery));
