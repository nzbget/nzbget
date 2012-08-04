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
 * $Revision: 1 $
 * $Date: 2012-05-01 00:00:00 +0200 (Di, 01 Mai 2012) $
 *
 */

/* controls */
var messages_MessagesTable;
var messages_MessagesTabBadge;
var messages_MessagesTabBadgeEmpty;
var messages_MessagesRecordsPerPage;
var messages_lastID = 0;

function messages_init()
{
	messages_MessagesTable = $('#MessagesTable');
	messages_MessagesTabBadge = $('#MessagesTabBadge');
	messages_MessagesTabBadgeEmpty = $('#MessagesTabBadgeEmpty');
	messages_MessagesRecordsPerPage = $('#MessagesRecordsPerPage');

	var RecordsPerPage = getSetting('MessagesRecordsPerPage', 10);
	messages_MessagesRecordsPerPage.val(RecordsPerPage);

	messages_MessagesTable.fasttable(
		{
			filterInput: $('#MessagesTable_filter'),
			filterClearButton: $("#MessagesTable_clearfilter"),
			pagerContainer: $('#MessagesTable_pager'),
			infoContainer: $('#MessagesTable_info'),
			filterCaseSensitive: false,
			pageSize: RecordsPerPage,
			maxPages: Settings_MiniTheme ? 1 : 5,
			pageDots: !Settings_MiniTheme,
			fillFieldsCallback: messages_fillFieldsCallback,
			renderCellCallback: messages_renderCellCallback
		});
}

function messages_theme()
{
	messages_MessagesTable.fasttable('setPageSize', getSetting('MessagesRecordsPerPage', 10), 
		Settings_MiniTheme ? 1 : 5, !Settings_MiniTheme);
}

function messages_update()
{
	if (messages_lastID === 0)
	{
		rpc('log', [0, Settings_MaxMessages], messages_loaded);
	}
	else
	{
		rpc('log', [messages_lastID+1, 0], messages_loaded);
	}
}

function messages_loaded(messages)
{
	messages_merge(messages);
	loadNext();
}

function messages_merge(messages)
{
	if (messages_lastID === 0)
	{
		Messages = messages;
	}
	else
	{
		Messages = Messages.concat(messages);
		Messages.splice(0, Messages.length-Settings_MaxMessages);
	}

	if (Messages.length > 0)
	{
		messages_lastID = Messages[Messages.length-1].ID;
	}
}

function messages_redraw()
{
	var data = [];

	for (var i=0; i < Messages.length; i++)
	{
		var message = Messages[i];

		var time = FormatDateTime(message.Time);

		var item =
		{
			id: message.ID,
			message: message,
			data: { time: time },
			search: message.Kind + ' ' + time + ' ' + message.Text
		};

		data.unshift(item);
	}

	messages_MessagesTable.fasttable('update', data);

	messages_MessagesTabBadge.html(Messages.length);
	show(messages_MessagesTabBadge, Messages.length > 0);
	show(messages_MessagesTabBadgeEmpty, Messages.length === 0 && Settings_MiniTheme);
}

function messages_fillFieldsCallback(item)
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

	var text = TextToHtml(message.Text);

	if (!Settings_MiniTheme)
	{
		item.fields = [kind, item.data.time, text];
	}
	else
	{
		var info = kind + ' <span class="label">' + item.data.time + '</span> ' + text;
		item.fields = [info];
	}
}

function messages_renderCellCallback(cell, index, item)
{
	if (index === 1)
	{
		cell.className = 'text-center';
	}
}

function messages_RecordsPerPage_change()
{
	var val = messages_MessagesRecordsPerPage.val();
	setSetting('MessagesRecordsPerPage', val);
	messages_MessagesTable.fasttable('setPageSize', val);
}