/*
 * This file is part of nzbget
 *
 * Copyright (C) 2012-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *   1) History tab;
 *   2) Functions for html generation for history, also used from other modules (edit dialog).
 */

/*** HISTORY TAB AND EDIT HISTORY DIALOG **********************************************/

var History = (new function($)
{
	'use strict';

	// Controls
	var $HistoryTable;
	var $HistoryTabBadge;
	var $HistoryTabBadgeEmpty;
	var $HistoryRecordsPerPage;

	// State
	var history;
	var notification = null;
	var updateTabInfo;
	var curFilter = 'ALL';
	var activeTab = false;
	var showDup = false;

	this.init = function(options)
	{
		updateTabInfo = options.updateTabInfo;

		$HistoryTable = $('#HistoryTable');
		$HistoryTabBadge = $('#HistoryTabBadge');
		$HistoryTabBadgeEmpty = $('#HistoryTabBadgeEmpty');
		$HistoryRecordsPerPage = $('#HistoryRecordsPerPage');

		var recordsPerPage = UISettings.read('HistoryRecordsPerPage', 10);
		$HistoryRecordsPerPage.val(recordsPerPage);
		$('#HistoryTable_filter').val('');

		$HistoryTable.fasttable(
			{
				filterInput: $('#HistoryTable_filter'),
				filterClearButton: $("#HistoryTable_clearfilter"),
				pagerContainer: $('#HistoryTable_pager'),
				infoContainer: $('#HistoryTable_info'),
				headerCheck: $('#HistoryTable > thead > tr:first-child'),
				pageSize: recordsPerPage,
				maxPages: UISettings.miniTheme ? 1 : 5,
				pageDots: !UISettings.miniTheme,
				fillFieldsCallback: fillFieldsCallback,
				filterCallback: filterCallback,
				renderCellCallback: renderCellCallback,
				updateInfoCallback: updateInfo
			});

		$HistoryTable.on('click', 'a', editClick);
		$HistoryTable.on('click', UISettings.rowSelect ? 'tbody tr' : 'tbody div.check',
			function(event) { $HistoryTable.fasttable('itemCheckClick', UISettings.rowSelect ? this : this.parentNode.parentNode, event); });
		$HistoryTable.on('click', 'thead div.check',
			function() { $HistoryTable.fasttable('titleCheckClick') });
		$HistoryTable.on('mousedown', Util.disableShiftMouseDown);
	}

	this.applyTheme = function()
	{
		$HistoryTable.fasttable('setPageSize', UISettings.read('HistoryRecordsPerPage', 10),
			UISettings.miniTheme ? 1 : 5, !UISettings.miniTheme);
	}

	this.show = function()
	{
		activeTab = true;
		this.redraw();
	}

	this.hide = function()
	{
		activeTab = false;
	}

	this.update = function()
	{
		if (!history)
		{
			$('#HistoryTable_Category').css('width', DownloadsUI.calcCategoryColumnWidth());
			initFilterButtons();
		}

		RPC.call('history', [showDup], loaded);
	}

	function loaded(curHistory)
	{
		history = curHistory;
		prepare();
		RPC.next();
	}

	function prepare()
	{
		for (var j=0, jl=history.length; j < jl; j++)
		{
			var hist = history[j];
			if (hist.Status === 'DELETED/MANUAL' || hist.Status === 'DELETED/GOOD' ||
				hist.Status === 'DELETED/SUCCESS' || hist.Status === 'DELETED/COPY')
			{
				hist.FilterKind = 'DELETED';
			}
			else if (hist.Status === 'DELETED/DUPE')
			{
				hist.FilterKind = 'DUPE';
			}
			else if (hist.Status.substring(0, 7) === 'SUCCESS')
			{
				hist.FilterKind = 'SUCCESS';
			}
			else
			{
				hist.FilterKind = 'FAILURE';
			}
		}
	}

	var SEARCH_FIELDS = ['name', 'status', 'priority', 'category', 'age', 'size', 'time'];

	this.redraw = function()
	{
		var data = [];

		for (var i=0; i < history.length; i++)
		{
			var hist = history[i];

			var kind = hist.Kind;
			hist.status = HistoryUI.buildStatusText(hist);
			hist.name = hist.Name;
			hist.size = kind === 'URL' ? '' : Util.formatSizeMB(hist.FileSizeMB);
			hist.sizemb = hist.FileSizeMB;
			hist.sizegb = hist.FileSizeMB / 1024;
			hist.time = Util.formatDateTime(hist.HistoryTime + UISettings.timeZoneCorrection*60*60);
			hist.category = kind !== 'DUP' ? hist.Category : '';
			hist.dupe = DownloadsUI.buildDupeText(hist.DupeKey, hist.DupeScore, hist.DupeMode);
			var age_sec = kind === 'NZB' ? new Date().getTime() / 1000 - (hist.MinPostTime + UISettings.timeZoneCorrection*60*60) : 0;
			hist.age = kind === 'NZB' ? Util.formatAge(hist.MinPostTime + UISettings.timeZoneCorrection*60*60) : '';
			hist.agem = Util.round0(age_sec / 60);
			hist.ageh = Util.round0(age_sec / (60*60));
			hist.aged = Util.round0(age_sec / (60*60*24));

			hist._search = SEARCH_FIELDS;

			var item =
			{
				id: hist.ID,
				data: hist,
			};

			data.push(item);
		}

		$HistoryTable.fasttable('update', data);

		Util.show($HistoryTabBadge, history.length > 0);
		Util.show($HistoryTabBadgeEmpty, history.length === 0 && UISettings.miniTheme);
	}

	function fillFieldsCallback(item)
	{
		var hist = item.data;

		var status = HistoryUI.buildStatus(hist);

		var name = '<a href="#" histid="' + hist.ID + '">' + Util.textToHtml(Util.formatNZBName(hist.Name)) + '</a>';
		var dupe = DownloadsUI.buildDupe(hist.DupeKey, hist.DupeScore, hist.DupeMode);
		var category = '';

		if (hist.Kind !== 'DUP')
		{
			var category = Util.textToHtml(hist.Category);
		}

		if (hist.Kind === 'URL')
		{
			name += ' <span class="label label-info">URL</span>';
		}
		else if (hist.Kind === 'DUP')
		{
			name += ' <span class="label label-info">hidden</span>';
		}

		if (!UISettings.miniTheme)
		{
			item.fields = ['<div class="check img-check"></div>', status, item.data.time, name + dupe, category, item.data.age, item.data.size];
		}
		else
		{
			var info = '<div class="check img-check"></div><span class="row-title">' + name + '</span>' + dupe +
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

	function renderCellCallback(cell, index, item)
	{
		if (index === 2)
		{
			cell.className = 'text-center';
		}
		else if (index === 5 || index === 6)
		{
			cell.className = 'text-right';
		}
	}

	this.recordsPerPageChange = function()
	{
		var val = $HistoryRecordsPerPage.val();
		UISettings.write('HistoryRecordsPerPage', val);
		$HistoryTable.fasttable('setPageSize', val);
	}

	function updateInfo(stat)
	{
		updateTabInfo($HistoryTabBadge, stat);
		if (activeTab)
		{
			updateFilterButtons();
		}
	}

	this.actionClick = function(action)
	{
		var checkedRows = $HistoryTable.fasttable('checkedRows');
		var checkedCount = $HistoryTable.fasttable('checkedCount');
		if (checkedCount === 0)
		{
			Notification.show('#Notif_History_Select');
			return;
		}

		var hasNzb = false;
		var hasUrl = false;
		var hasDup = false;
		var hasFailed = false;
		for (var i = 0; i < history.length; i++)
		{
			var hist = history[i];
			if (checkedRows[hist.ID])
			{
				hasNzb |= hist.Kind === 'NZB';
				hasUrl |= hist.Kind === 'URL';
				hasDup |= hist.Kind === 'DUP';
				hasFailed |= hist.ParStatus === 'FAILURE' || hist.UnpackStatus === 'FAILURE';
			}
		}

		switch (action)
		{
			case 'DELETE':
				notification = '#Notif_History_Deleted';
				HistoryUI.deleteConfirm(historyAction, hasNzb, hasDup, hasFailed, true);
				break;

			case 'REPROCESS':
				if (hasUrl || hasDup)
				{
					Notification.show('#Notif_History_CantReprocess');
					return;
				}
				notification = '#Notif_History_Reprocess';
				historyAction('HistoryProcess');
				break;

			case 'REDOWNLOAD':
				if (hasDup)
				{
					Notification.show('#Notif_History_CantRedownload');
					return;
				}
				notification = '#Notif_History_Returned';
				ConfirmDialog.showModal('HistoryEditRedownloadConfirmDialog',
					function () { historyAction('HistoryRedownload') },
					function () { HistoryUI.confirmMulti(checkedCount > 1); });
				break;

			case 'MARKSUCCESS':
			case 'MARKGOOD':
			case 'MARKBAD':
				if (hasUrl)
				{
					Notification.show('#Notif_History_CantMark');
					return;
				}
				notification = '#Notif_History_Marked';

				ConfirmDialog.showModal(action === 'MARKSUCCESS' ? 'HistoryEditSuccessConfirmDialog' : 
					action === 'MARKGOOD' ? 'HistoryEditGoodConfirmDialog' : 'HistoryEditBadConfirmDialog',
					function () // action
					{
						historyAction(action === 'MARKSUCCESS' ? 'HistoryMarkSuccess' :
							action === 'MARKGOOD' ? 'HistoryMarkGood' :'HistoryMarkBad');
					},
					function (_dialog) // init
					{
						HistoryUI.confirmMulti(checkedCount > 1);
					}
				);
				break;
		}
	}

	function historyAction(command)
	{
		Refresher.pause();

		var ids = [];
		var checkedRows = $HistoryTable.fasttable('checkedRows');
		for (var id in checkedRows)
		{
			ids.push(parseInt(id));
		}		
		
		RPC.call('editqueue', [command, 0, '', ids], function()
		{
			editCompleted();
		});
	}

	function editCompleted()
	{
		Refresher.update();
		if (notification)
		{
			Notification.show(notification);
			notification = null;
		}
	}

	function editClick(e)
	{
		e.preventDefault();
		e.stopPropagation();

		var histid = $(this).attr('histid');
		$(this).blur();

		var hist = null;

		// find history object
		for (var i=0; i<history.length; i++)
		{
			var gr = history[i];
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

		HistoryEditDialog.showModal(hist);
	}

	function filterCallback(item)
	{
		return !activeTab || curFilter === 'ALL' || item.data.FilterKind === curFilter;
	}

	function initFilterButtons()
	{
		Util.show($('#History_Badge_DUPE, #History_Badge_DUPE2').closest('.btn'), Options.option('DupeCheck') === 'yes');
	}

	function updateFilterButtons()
	{
		var countSuccess = 0;
		var countFailure = 0;
		var countDeleted = 0;
		var countDupe = 0;

		var data = $HistoryTable.fasttable('availableContent');

		for (var i=0; i < data.length; i++)
		{
			var hist = data[i].data;
			switch (hist.FilterKind)
			{
				case 'SUCCESS': countSuccess++; break;
				case 'FAILURE': countFailure++; break;
				case 'DELETED': countDeleted++; break;
				case 'DUPE': countDupe++; break;
			}
		}
		$('#History_Badge_ALL,#History_Badge_ALL2').text(countSuccess + countFailure + countDeleted + countDupe);
		$('#History_Badge_SUCCESS,#History_Badge_SUCCESS2').text(countSuccess);
		$('#History_Badge_FAILURE,#History_Badge_FAILURE2').text(countFailure);
		$('#History_Badge_DELETED,#History_Badge_DELETED2').text(countDeleted);
		$('#History_Badge_DUPE,#History_Badge_DUPE2').text(countDupe);

		$('#HistoryTab_Toolbar .history-filter').removeClass('btn-inverse');
		$('#History_Badge_' + curFilter + ',#History_Badge_' + curFilter + '2').closest('.history-filter').addClass('btn-inverse');
		$('#HistoryTab_Toolbar .badge').removeClass('badge-active');
		$('#History_Badge_' + curFilter + ',#History_Badge_' + curFilter + '2').addClass('badge-active');
	}

	this.filter = function(type)
	{
		curFilter = type;
		History.redraw();
	}

	this.dupClick = function()
	{
		showDup = !showDup;
		$('#History_Dup').toggleClass('btn-inverse', showDup);
		$('#History_DupIcon').toggleClass('icon-mask', !showDup).toggleClass('icon-mask-white', showDup);
		Refresher.update();
	}

}(jQuery));


/*** FUNCTIONS FOR HTML GENERATION (also used from other modules) *****************************/

var HistoryUI = (new function($)
{
	'use strict';

	this.buildStatusText = function(hist)
	{
		var status = hist.Status;
		var total = status.substring(0, 7);
		var detail = status.substring(8, 100);
		switch (total)
		{
			case 'SUCCESS':
				return detail === 'GOOD' ? 'GOOD' : 'SUCCESS';
			case 'FAILURE':
				return detail === 'BAD' ? 'BAD' : (status === 'FAILURE/INTERNAL_ERROR' ? 'INTERNAL_ERROR' : 'FAILURE');
			case 'WARNING':
				return detail === 'SCRIPT' ? 'PP-FAILURE' : detail;
			case 'DELETED':
				return detail === 'MANUAL' ? 'DELETED' : detail;
			default:
				return 'INTERNAL_ERROR (' + status + ')';
		}
	}

	this.buildStatus = function(hist)
	{
		var total = hist.Status.substring(0, 7);
		var statusText = HistoryUI.buildStatusText(hist);
		var badgeClass = '';
		switch (total)
		{
			case 'SUCCESS':
				badgeClass = 'label-success'; break;
			case 'FAILURE':
				badgeClass = 'label-important'; break;
			case 'WARNING':
				badgeClass = 'label-warning'; break;
		}
		return '<span class="label label-status ' + badgeClass + '">' + statusText + '</span>';
	}
	
	this.deleteConfirm = function(actionCallback, hasNzb, hasDup, hasFailed, multi)
	{
		var dupeCheck = Options.option('DupeCheck') === 'yes';
		var cleanupDisk = Options.option('DeleteCleanupDisk') === 'yes';
		var dialog = null;

		function init(_dialog)
		{
			dialog = _dialog;
			HistoryUI.confirmMulti(multi);
			$('#HistoryDeleteConfirmDialog_Hide', dialog).prop('checked', true);
			Util.show($('#HistoryDeleteConfirmDialog_Options', dialog), hasNzb && dupeCheck);
			Util.show($('#HistoryDeleteConfirmDialog_Simple', dialog), !(hasNzb && dupeCheck));
			Util.show($('#HistoryDeleteConfirmDialog_DeleteWillCleanup', dialog), hasNzb && hasFailed && cleanupDisk);
			Util.show($('#HistoryDeleteConfirmDialog_DeleteCanCleanup', dialog), hasNzb && hasFailed && !cleanupDisk);
			Util.show($('#HistoryDeleteConfirmDialog_DeleteNoCleanup', dialog), !(hasNzb && hasFailed));
			Util.show($('#HistoryDeleteConfirmDialog_DupAlert', dialog), !hasNzb && dupeCheck && hasDup);
			Util.show('#ConfirmDialog_Help', hasNzb && dupeCheck);
		};

		function action()
		{
			var hide = $('#HistoryDeleteConfirmDialog_Hide', dialog).is(':checked');
			var command = hasNzb && hide ? 'HistoryDelete' : 'HistoryFinalDelete';
			actionCallback(command);
		}

		ConfirmDialog.showModal('HistoryDeleteConfirmDialog', action, init);
	}
	
	this.confirmMulti = function(multi)
	{
		if (multi === undefined || !multi)
		{
			var html = $('#ConfirmDialog_Text').html();
			html = html.replace(/records/g, 'record');
			html = html.replace(/nzbs/g, 'nzb');
			$('#ConfirmDialog_Text').html(html);
		}
	}
}(jQuery));
