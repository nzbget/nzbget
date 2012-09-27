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
var history_HistoryTable;
var history_HistoryTabBadge;
var history_HistoryTabBadgeEmpty;
var history_HistoryRecordsPerPage;

var history_dialog;
var history_curHist;
var history_notification = null;

function history_init()
{
	history_HistoryTable = $('#HistoryTable');
	history_HistoryTabBadge = $('#HistoryTabBadge');
	history_HistoryTabBadgeEmpty = $('#HistoryTabBadgeEmpty');
	history_HistoryRecordsPerPage = $('#HistoryRecordsPerPage');
	history_dialog = $('#HistoryEdit');
	history_edit_init();

	var RecordsPerPage = getSetting('HistoryRecordsPerPage', 10);
	history_HistoryRecordsPerPage.val(RecordsPerPage);

	history_HistoryTable.fasttable(
		{
			filterInput: $('#HistoryTable_filter'),
			filterClearButton: $("#HistoryTable_clearfilter"),
			pagerContainer: $('#HistoryTable_pager'),
			infoContainer: $('#HistoryTable_info'),
			headerCheck: $('#HistoryTable > thead > tr:first-child'),
			filterCaseSensitive: false,
			pageSize: RecordsPerPage,
			maxPages: Settings_MiniTheme ? 1 : 5,
			pageDots: !Settings_MiniTheme,
			fillFieldsCallback: history_fillFieldsCallback,
			renderCellCallback: history_renderCellCallback,
			updateInfoCallback: history_updateInfo
		});

	history_HistoryTable.on('click', 'a', history_edit_click);
	history_HistoryTable.on('click', 'tbody div.check',
		function(event) { history_HistoryTable.fasttable('itemCheckClick', this.parentNode.parentNode, event); });
	history_HistoryTable.on('click', 'thead div.check',
		function() { history_HistoryTable.fasttable('titleCheckClick') });
	history_HistoryTable.on('mousedown', util_disableShiftMouseDown);
}

function history_theme()
{
	history_HistoryTable.fasttable('setPageSize', getSetting('HistoryRecordsPerPage', 10),
		Settings_MiniTheme ? 1 : 5, !Settings_MiniTheme);
}

function history_update()
{
	rpc('history', [], history_loaded);
}

function history_loaded(history)
{
	History = history;
	history_prepare();
	loadNext();
}

function history_prepare()
{
	for (var j=0, jl=History.length; j < jl; j++)
	{
		history_detect_status(History[j]);
	}
}

function history_detect_status(hist)
{
	if (hist.Kind === 'NZB')
	{
		switch (hist.ScriptStatus)
		{
			case 'SUCCESS': hist.status = 'success'; break;
			case 'FAILURE': hist.status = 'failure'; break;
			case 'UNKNOWN': hist.status = 'unknown'; break;
			case 'NONE':
				switch (hist.ParStatus)
				{
					case 'SUCCESS': hist.status = 'success'; break;
					case 'REPAIR_POSSIBLE': hist.status = 'repairable'; break;
					case 'FAILURE': hist.status = 'failure'; break;
					case 'NONE': hist.status = 'none'; break;
				}
		}
	}
	else if (hist.Kind === 'URL')
	{
		switch (hist.UrlStatus)
		{
			case 'SUCCESS': hist.status = 'success'; break;
			case 'FAILURE': hist.status = 'failure'; break;
			case 'UNKNOWN': hist.status = 'unknown'; break;
		}
	}
}

function history_redraw()
{
	var data = [];

	for (var i=0; i < History.length; i++)
	{
		var hist = History[i];

		var kind = hist.Kind;
		var statustext = hist.status === 'none' ? 'unknown' : hist.status;
		var size = kind === 'NZB' ? FormatSizeMB(hist.FileSizeMB) : '';

		var textname = hist.Name;
		if (kind === 'URL')
		{
			textname += ' URL';
		}

		var time = FormatDateTime(hist.HistoryTime);

		var item =
		{
			id: hist.ID,
			hist: hist,
			data: {time: time, size: size},
			search: statustext + ' ' + time + ' ' + textname + ' ' + hist.Category + ' ' + size
		};

		data.push(item);
	}

	history_HistoryTable.fasttable('update', data);

	show(history_HistoryTabBadge, History.length > 0);
	show(history_HistoryTabBadgeEmpty, History.length === 0 && Settings_MiniTheme);
}

function history_fillFieldsCallback(item)
{
	var hist = item.hist;

	var status = history_build_status(hist);

	var name = '<a href="#" histid="' + hist.ID + '">' + TextToHtml(FormatNZBName(hist.Name)) + '</a>';
	var category = TextToHtml(hist.Category);

	if (hist.Kind === 'URL')
	{
		name += ' <span class="label label-info">URL</span>';
	}

	if (!Settings_MiniTheme)
	{
		item.fields = ['<div class="check img-check"></div>', status, item.data.time, name, category, item.data.size];
	}
	else
	{
		var info = '<div class="check img-check"></div><span class="row-title">' + name + '</span>' +
			' ' + status + ' <span class="label">' + item.data.time + '</span>';
		if (category)
		{
			info += ' <span class="label label-status">' + category + '</span>';
		}
		if (hist.Kind === 'NZB')
		{
			info += ' <span class="label">' + item.data.size + '</span>';
		}
		item.fields = [info];
	}
}

function history_renderCellCallback(cell, index, item)
{
	if (index === 2)
	{
		cell.className = 'text-center';
	}
	else if (index === 5)
	{
		cell.className = 'text-right';
	}
}

function history_build_status(hist)
{
	switch (hist.status)
	{
		case 'success': return '<span class="label label-status label-success">success</span>';
		case 'failure': return '<span class="label label-status label-important">failure</span>';
		case 'unknown': return '<span class="label label-status label-info">unknown</span>';
		case 'repairable': return '<span class="label label-status label-success">repairable</span>';
		case 'none': return '<span class="label label-status">unknown</span>';
		default: return '<span class="label label-status label-danger">internal error(' + hist.status + ')</span>';
	}
}

function history_RecordsPerPage_change()
{
	var val = history_HistoryRecordsPerPage.val();
	setSetting('HistoryRecordsPerPage', val);
	history_HistoryTable.fasttable('setPageSize', val);
}

function history_updateInfo(stat)
{
	tab_updateInfo(history_HistoryTabBadge, stat);
}

function history_edit_init()
{
	$('#HistoryEdit_Delete').click(history_edit_Delete);
	$('#HistoryEdit_Return').click(history_edit_Return);
	$('#HistoryEdit_Reprocess').click(history_edit_Reprocess);

	history_dialog.on('hidden', function () {
		refresh_resume();
	});
}

function history_edit_click()
{
	var histid = $(this).attr('histid');

	var hist = null;

	// find History object
	for (var i=0; i<History.length; i++)
	{
		var gr = History[i];
		if (gr.ID == histid)
		{
			hist = gr;
			break;
		}
	}
	if (hist == null)
	{
		return;
	}

	refresh_pause();

	history_curHist = hist; // global

	var status = history_build_status(hist);

	$('#HistoryEdit_Title').text(FormatNZBName(hist.Name));
	if (hist.Kind === 'URL')
	{
		$('#HistoryEdit_Title').html($('#HistoryEdit_Title').html() + '&nbsp;' + '<span class="label label-info">URL</span>');
	}

	$('#HistoryEdit_Status').html(status);
	$('#HistoryEdit_Category').text(hist.Category !== '' ? hist.Category : '<empty>');
	$('#HistoryEdit_Path').text(hist.DestDir);

	var size = FormatSizeMB(hist.FileSizeMB, hist.FileSizeLo);

	var table = '';
	table += '<tr><td>Total</td><td class="text-right">' + size + '</td></tr>';
	table += '<tr><td>Files (total/parked)</td><td class="text-right">' + hist.FileCount + '/' + hist.RemainingFileCount + '</td></tr>';
	$('#HistoryEdit_Statistics').html(table);

	show($('#HistoryEdit_ReturnGroup'), hist.RemainingFileCount > 0 || hist.Kind === 'URL');
	show($('#HistoryEdit_PathGroup, #HistoryEdit_StatisticsGroup, #HistoryEdit_ReprocessGroup'), hist.Kind === 'NZB');

	history_edit_EnableAllButtons();

	history_dialog.modal({backdrop: 'static'});
}

function history_edit_DisableAllButtons()
{
	$('#HistoryEdit .modal-footer .btn').attr('disabled', 'disabled');
	setTimeout(function()
	{
		$('#HistoryEdit_Transmit').show();
	}, 500);
}

function history_edit_EnableAllButtons()
{
	$('#HistoryEdit .modal-footer .btn').removeAttr('disabled');
	$('#HistoryEdit_Transmit').hide();
}

function history_edit_completed()
{
	history_dialog.modal('hide');
	refresh_update();
	if (history_notification)
	{
		animateAlert(history_notification);
		history_notification = null;
	}
}

function history_edit_Delete()
{
	history_edit_DisableAllButtons();
	history_notification = '#Notif_History_Deleted';
	rpc('editqueue', ['HistoryDelete', 0, '', [history_curHist.ID]], history_edit_completed);
}

function history_edit_Return()
{
	history_edit_DisableAllButtons();
	history_notification = '#Notif_History_Returned';
	rpc('editqueue', ['HistoryReturn', 0, '', [history_curHist.ID]], history_edit_completed);
}

function history_edit_Reprocess()
{
	history_edit_DisableAllButtons();
	history_notification = '#Notif_History_Reproces';
	rpc('editqueue', ['HistoryProcess', 0, '', [history_curHist.ID]], history_edit_completed);
}

function history_delete_click()
{
	if (History.length == 0)
	{
		return;
	}

	var checkedRows = history_HistoryTable.fasttable('checkedRows');
	if (checkedRows.length > 0)
	{
		confirm_dialog_show('HistoryDeleteConfirmDialog', history_delete);
	}
	else
	{
		confirm_dialog_show('HistoryClearConfirmDialog', history_clear);
	}
}

function history_delete()
{
	refresh_pause();

	var IDs = history_HistoryTable.fasttable('checkedRows');

	rpc('editqueue', ['HistoryDelete', 0, '', [IDs]], function()
	{
		history_notification = '#Notif_History_Deleted';
		history_edit_completed();
	});
}

function history_clear()
{
	refresh_pause();

	var IDs = [];
	for (var i=0; i<History.length; i++)
	{
		IDs.push(History[i].ID);
	}

	rpc('editqueue', ['HistoryDelete', 0, '', [IDs]], function()
	{
		history_notification = '#Notif_History_Cleared';
		history_edit_completed();
	});
}
