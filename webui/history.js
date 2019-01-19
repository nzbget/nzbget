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
 *   1) History tab;
 *   2) Functions for html generation for history, also used from other modules (edit dialog);
 *   3) Popup menus in history list.
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
	var $CategoryMenu;

	// State
	var history;
	var notification = null;
	var updateTabInfo;
	var curFilter = 'ALL';
	var activeTab = false;
	var showDup = false;
	var cached = false;

	this.init = function(options)
	{
		updateTabInfo = options.updateTabInfo;

		$HistoryTable = $('#HistoryTable');
		$HistoryTabBadge = $('#HistoryTabBadge');
		$HistoryTabBadgeEmpty = $('#HistoryTabBadgeEmpty');
		$HistoryRecordsPerPage = $('#HistoryRecordsPerPage');
		$CategoryMenu = $('#HistoryCategoryMenu');

		var recordsPerPage = UISettings.read('HistoryRecordsPerPage', 10);
		$HistoryRecordsPerPage.val(recordsPerPage);
		$('#HistoryTable_filter').val('');

		$HistoryTable.fasttable(
			{
				filterInput: '#HistoryTable_filter',
				filterClearButton: '#HistoryTable_clearfilter',
				pagerContainer: '#HistoryTable_pager',
				infoContainer: '#HistoryTable_info',
				rowSelect: UISettings.rowSelect,
				pageSize: recordsPerPage,
				maxPages: UISettings.miniTheme ? 1 : 5,
				pageDots: !UISettings.miniTheme,
				shortcuts: true,
				fillFieldsCallback: fillFieldsCallback,
				filterCallback: filterCallback,
				renderCellCallback: renderCellCallback,
				updateInfoCallback: updateInfo
			});

		$HistoryTable.on('click', 'a', editClick);
		$HistoryTable.on('click', 'td:nth-child(2).dropdown-cell > div', statusClick);
		$HistoryTable.on('click', 'td:nth-child(5).dropdown-cell > div:not(.dropdown-disabled)', categoryClick);
		$CategoryMenu.on('click', 'a', categoryMenuClick);

		HistoryActionsMenu.init();
	}

	this.applyTheme = function()
	{
		this.redraw(true);
		$HistoryTable.fasttable('setPageSize', UISettings.read('HistoryRecordsPerPage', 10),
			UISettings.miniTheme ? 1 : 5, !UISettings.miniTheme);
	}

	this.show = function()
	{
		activeTab = true;
		this.redraw(true);
	}

	this.hide = function()
	{
		activeTab = false;
	}

	this.update = function()
	{
		RPC.call('history', [showDup], loaded, null, { prefer_cached: true });
	}

	function loaded(_history, _cached)
	{
		if (!history)
		{
			$('#HistoryTable_Category').css('width', DownloadsUI.calcCategoryColumnWidth());
			initFilterButtons();
		}

		cached = _cached;
		if (!Refresher.isPaused() && !cached)
		{
			history = _history;
			prepare();
		}
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

	this.redraw = function(force)
	{
		if (cached && !force)
		{
			return;
		}

		if (!Refresher.isPaused())
		{
			redraw_table();
		}

		Util.show($HistoryTabBadge, history.length > 0);
		Util.show($HistoryTabBadgeEmpty, history.length === 0 && UISettings.miniTheme);
	}

	var SEARCH_FIELDS = ['name', 'status', 'priority', 'category', 'age', 'size', 'time'];

	function redraw_table()
	{
		var data = [];

		for (var i=0; i < history.length; i++)
		{
			var hist = history[i];

			var kind = hist.Kind;
			hist.status = HistoryUI.buildStatusText(hist);
			hist.name = hist.Name;
			hist.size = kind === 'URL' && hist.FileSizeLo == 0 && hist.FileSizeHi == 0 ? '' : Util.formatSizeMB(hist.FileSizeMB);
			hist.sizemb = hist.FileSizeMB;
			hist.sizegb = hist.FileSizeMB / 1024;
			hist.time = Util.formatDateTime(hist.HistoryTime + UISettings.timeZoneCorrection*60*60);
			hist.category = kind !== 'DUP' ? hist.Category : '';
			hist.dupe = DownloadsUI.buildDupeText(hist.DupeKey, hist.DupeScore, hist.DupeMode);
			var age_sec = hist.MinPostTime > 0 ? new Date().getTime() / 1000 - (hist.MinPostTime + UISettings.timeZoneCorrection*60*60) : 0;
			hist.age = hist.MinPostTime > 0 ? Util.formatAge(hist.MinPostTime + UISettings.timeZoneCorrection*60*60) : '';
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
	}

	function fillFieldsCallback(item)
	{
		var hist = item.data;

		var status = HistoryUI.buildStatus(hist);

		var name = '<a href="#" data-nzbid="' + hist.ID + '">' + Util.textToHtml(Util.formatNZBName(hist.Name)) + '</a>';
		name += DownloadsUI.buildEncryptedLabel(hist.Kind === 'NZB' ? hist.Parameters : []);

		var dupe = DownloadsUI.buildDupe(hist.DupeKey, hist.DupeScore, hist.DupeMode);
		var category = hist.Kind !== 'DUP' ?
			(hist.Category !== '' ? Util.textToHtml(hist.Category) : '<span class="none-category">None</span>')
			: '';
		var backup = hist.Kind === 'NZB' ? DownloadsUI.buildBackupLabel(hist) : '';

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
			status = '<div data-nzbid="' + hist.NZBID + '">' + status + '</div>';
			category = '<div data-nzbid="' + hist.NZBID + '"' +  (hist.Kind === 'DUP' ? ' class="dropdown-disabled"' : '') + '>' + category + '</div>';
			item.fields = ['<div class="check img-check"></div>', status, item.data.time, name + dupe + backup, category, item.data.age, item.data.size];
		}
		else
		{
			var info = '<div class="check img-check"></div><span class="row-title">' + name + '</span>' + dupe +
				' ' + status + backup + ' <span class="label">' + item.data.time + '</span>';
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
		if (index === 1 || index === 4)
		{
			cell.className = !UISettings.miniTheme ? 'dropdown-cell' : '';
		}
		else if (index === 2)
		{
			cell.className = 'text-center' + (!UISettings.miniTheme ? ' dropafter-cell' : '');
		}
		else if (index === 5)
		{
			cell.className = 'text-right' + (!UISettings.miniTheme ? ' dropafter-cell' : '');
		}
		else if (index === 6)
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
			PopupNotification.show('#Notif_History_Select');
			return;
		}

		var pageCheckedCount = $HistoryTable.fasttable('pageCheckedCount');
		var checkedPercentage = Util.round0(checkedCount / history.length * 100);
		
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
				hasFailed |= hist.ParStatus === 'FAILURE' || hist.UnpackStatus === 'FAILURE' ||
					hist.DeleteStatus != 'NONE';
			}
		}

		switch (action)
		{
			case 'DELETE':
				notification = '#Notif_History_Deleted';
				HistoryUI.deleteConfirm(historyAction, hasNzb, hasDup, hasFailed, true, checkedCount, pageCheckedCount, checkedPercentage);
				break;

			case 'REPROCESS':
				if (hasUrl || hasDup)
				{
					PopupNotification.show('#Notif_History_CantReprocess');
					return;
				}
				notification = '#Notif_History_Reprocess';
				historyAction('HistoryProcess');
				break;

			case 'REDOWNLOAD':
				if (hasDup)
				{
					PopupNotification.show('#Notif_History_CantRedownload');
					return;
				}
				notification = '#Notif_History_Returned';
				ConfirmDialog.showModal('HistoryEditRedownloadConfirmDialog',
					function () { historyAction('HistoryRedownload') },
					function () { HistoryUI.confirmMulti(checkedCount > 1); },
					checkedCount);
				break;

			case 'MARKSUCCESS':
			case 'MARKGOOD':
			case 'MARKBAD':
				if (hasUrl)
				{
					PopupNotification.show('#Notif_History_CantMark');
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
					},
					checkedCount
				);
				break;
		}
	}

	function historyAction(command)
	{
		Refresher.pause();

		var ids = buildContextIdList();

		RPC.call('editqueue', [command, '', ids], function()
		{
			editCompleted();
		});
	}

	function editCompleted()
	{
		Refresher.update();
		if (notification)
		{
			PopupNotification.show(notification);
			notification = null;
		}
	}

	function editClick(e)
	{
		e.preventDefault();
		e.stopPropagation();

		var histid = $(this).attr('data-nzbid');
		var area = $(this).attr('data-area');
		$(this).blur();

		var hist = findHist(histid);
		if (hist == null)
		{
			return;
		}

		HistoryEditDialog.showModal(hist, area);
	}

	function findHist(nzbid)
	{
		for (var i=0; i<history.length; i++)
		{
			var gr = history[i];
			if (gr.NZBID == nzbid)
			{
				return gr;
			}
		}
		return null;
	}

	function buildContextIdList(hist)
	{
		var editIds = [];
		var checkedRows = $HistoryTable.fasttable('checkedRows');
		for (var id in checkedRows)
		{
			editIds.push(parseInt(id));
		}
		if (hist !== undefined && editIds.indexOf(hist.NZBID) === -1)
		{
			editIds = [hist.NZBID];
		}
		return editIds;
	}
	this.buildContextIdList = buildContextIdList;

	function statusClick(e)
	{
		e.preventDefault();
		e.stopPropagation();
		var hist = findHist($(this).attr('data-nzbid'));

		HistoryActionsMenu.showPopupMenu(hist, 'left',
			{ left: $(this).offset().left - 30, top: $(this).offset().top,
				width: $(this).width() + 30, height: $(this).outerHeight() - 2 },
			function(_notification) { notification = _notification; },
			editCompleted);
	}

	function categoryClick(e)
	{
		e.preventDefault();
		e.stopPropagation();
		DownloadsUI.fillCategoryMenu($CategoryMenu);

		var hist = findHist($(this).attr('data-nzbid'));
		var editIds = buildContextIdList(hist);
		$CategoryMenu.data('nzbids', editIds);
		DownloadsUI.updateContextWarning($CategoryMenu, editIds);
		$('i', $CategoryMenu).removeClass('icon-ok').addClass('icon-empty');
		$('li[data="' + hist.Category + '"] i', $CategoryMenu).addClass('icon-ok');

		Frontend.showPopupMenu($CategoryMenu, 'bottom-left',
			{ left: $(this).offset().left - 30, top: $(this).offset().top,
				width: $(this).width() + 30, height: $(this).outerHeight() - 2 });
	}

	function categoryMenuClick(e)
	{
		e.preventDefault();
		var category = $(this).parent().attr('data');
		var nzbids = $CategoryMenu.data('nzbids');
		notification = '#Notif_History_Changed';
		RPC.call('editqueue', ['HistorySetCategory', category, nzbids], editCompleted);
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
		History.redraw(true);
	}

	this.dupClick = function()
	{
		showDup = !showDup;
		$('#History_Dup').toggleClass('btn-inverse', showDup);
		$('#History_DupIcon').toggleClass('icon-mask', !showDup).toggleClass('icon-mask-white', showDup);
		Refresher.update();
	}

	this.processShortcut = function(key)
	{
		switch (key)
		{
			case 'D': case 'Delete': case 'Meta+Backspace': History.actionClick('DELETE'); return true;
			case 'P': History.actionClick('REPROCESS'); return true;
			case 'N': History.actionClick('REDOWNLOAD'); return true;
			case 'M': History.actionClick('MARKSUCCESS'); return true;
			case 'G': History.actionClick('MARKGOOD'); return true;
			case 'B': History.actionClick('MARKBAD'); return true;
			case 'A': History.filter('ALL'); return true;
			case 'S': History.filter('SUCCESS'); return true;
			case 'F': History.filter('FAILURE'); return true;
			case 'L': History.filter('DELETED'); return true;
			case 'U': History.filter('DUPE'); return true;
			case 'H': History.dupClick(); return true;
		}
		return $HistoryTable.fasttable('processShortcut', key);
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

	this.deleteConfirm = function(actionCallback, hasNzb, hasDup, hasFailed, multi, selCount, pageSelCount, selPercentage)
	{
		var dupeCheck = Options.option('DupeCheck') === 'yes';
		var dialog = null;

		function init(_dialog)
		{
			dialog = _dialog;
			HistoryUI.confirmMulti(multi);
			$('#HistoryDeleteConfirmDialog_Hide', dialog).prop('checked', true);
			Util.show($('#HistoryDeleteConfirmDialog_Options', dialog), hasNzb && dupeCheck);
			Util.show($('#HistoryDeleteConfirmDialog_Simple', dialog), !(hasNzb && dupeCheck));
			Util.show($('#HistoryDeleteConfirmDialog_DeleteWillCleanup', dialog), hasNzb && hasFailed);
			Util.show($('#HistoryDeleteConfirmDialog_DeleteNoCleanup', dialog), !(hasNzb && hasFailed));
			Util.show($('#HistoryDeleteConfirmDialog_DupAlert', dialog), !hasNzb && dupeCheck && hasDup);
			Util.show('#ConfirmDialog_Help', hasNzb && dupeCheck);
		};

		function action()
		{
			var hide = $('#HistoryDeleteConfirmDialog_Hide', dialog).is(':checked');
			var command = hasNzb && hide ? 'HistoryDelete' : 'HistoryFinalDelete';
			if (selCount - pageSelCount > 0 && selCount >= 50)
			{
				PurgeHistoryDialog.showModal(function(){actionCallback(command);}, selCount, selPercentage);
			}
			else
			{
				actionCallback(command);
			}
		}

		ConfirmDialog.showModal('HistoryDeleteConfirmDialog', action, init, selCount);
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

/*** HISTORY ACTION MENU *************************************************************************/

var HistoryActionsMenu = (new function($)
{
	'use strict'

	var $ActionsMenu;
	var curHist;
	var beforeCallback;
	var completedCallback;
	var editIds;

	this.init = function()
	{
		$ActionsMenu = $('#HistoryActionsMenu');
		$('#HistoryActions_Delete').click(itemDelete);
		$('#HistoryActions_Return, #HistoryActions_ReturnURL').click(itemReturn);
		$('#HistoryActions_Reprocess').click(itemReprocess);
		$('#HistoryActions_Redownload').click(itemRedownload);
		$('#HistoryActions_RetryFailed').click(itemRetryFailed);
		$('#HistoryActions_MarkSuccess').click(itemSuccess);
		$('#HistoryActions_MarkGood').click(itemGood);
		$('#HistoryActions_MarkBad').click(itemBad);
	}

	this.showPopupMenu = function(hist, anchor, rect, before, completed)
	{
		curHist = hist;
		beforeCallback = before;
		completedCallback = completed;
		editIds = History.buildContextIdList(hist);

		// setup menu items
		Util.show('#HistoryActions_Return', hist.RemainingFileCount > 0);
		Util.show('#HistoryActions_ReturnURL', hist.Kind === 'URL');
		Util.show('#HistoryActions_Redownload, #HistoryActions_Reprocess', hist.Kind === 'NZB');
		Util.show('#HistoryActions_RetryFailed', hist.Kind === 'NZB' && hist.FailedArticles > 0 && hist.RetryData);
		var dupeCheck = Options.option('DupeCheck') === 'yes';
		Util.show('#HistoryActions_MarkSuccess', dupeCheck && ((hist.Kind === 'NZB' && hist.MarkStatus !== 'SUCCESS') || (hist.Kind === 'DUP' && hist.DupStatus !== 'SUCCESS')) &&
			hist.Status.substr(0, 7) !== 'SUCCESS');
		Util.show('#HistoryActions_MarkGood', dupeCheck && ((hist.Kind === 'NZB' && hist.MarkStatus !== 'GOOD') || (hist.Kind === 'DUP' && hist.DupStatus !== 'GOOD')));
		Util.show('#HistoryActions_MarkBad', dupeCheck && hist.Kind !== 'URL');
		DownloadsUI.updateContextWarning($ActionsMenu, editIds);

		DownloadsUI.buildDNZBLinks(hist.Parameters ? hist.Parameters : [], 'HistoryActions_DNZB');
		
		Frontend.showPopupMenu($ActionsMenu, anchor, rect);
	}
	
	function itemDelete(e)
	{
		e.preventDefault();
		HistoryUI.deleteConfirm(doItemDelete, curHist.Kind === 'NZB', curHist.Kind === 'DUP',
			curHist.ParStatus === 'FAILURE' || curHist.UnpackStatus === 'FAILURE' ||
			curHist.DeleteStatus != 'NONE', false);
	}

	function execAction(command, notification)
	{
		function performAction()
		{
			RPC.call('editqueue', [command, '', editIds], completedCallback);
		}

		var async = beforeCallback(notification, performAction);
		if (!async)
		{
			performAction();
		}
	}

	function doItemDelete(command)
	{
		execAction(command, '#Notif_History_Deleted');
	}

	function itemReturn(e)
	{
		e.preventDefault();
		execAction('HistoryReturn', '#Notif_History_Returned');
	}

	function itemRedownload(e)
	{
		e.preventDefault();
		if (curHist.SuccessArticles > 0)
		{
			ConfirmDialog.showModal('HistoryEditRedownloadConfirmDialog', doItemRedownload,
				function () { HistoryUI.confirmMulti(false); });
		}
		else
		{
			doItemRedownload();
		}
	}

	function doItemRedownload()
	{
		execAction('HistoryRedownload', '#Notif_History_Returned');
	}

	function itemReprocess(e)
	{
		e.preventDefault();
		execAction('HistoryProcess', '#Notif_History_Reprocess');
	}

	function itemRetryFailed(e)
	{
		e.preventDefault();
		execAction('HistoryRetryFailed', '#Notif_History_RetryFailed');
	}

	function itemSuccess(e)
	{
		e.preventDefault();
		ConfirmDialog.showModal('HistoryEditSuccessConfirmDialog', doItemSuccess,
			function () { HistoryUI.confirmMulti(editIds.length > 1); });
	}

	function doItemSuccess()
	{
		execAction('HistoryMarkSuccess', '#Notif_History_Marked');
	}

	function itemGood(e)
	{
		e.preventDefault();
		ConfirmDialog.showModal('HistoryEditGoodConfirmDialog', doItemGood,
			function () { HistoryUI.confirmMulti(editIds.length > 1); });
	}

	function doItemGood()
	{
		execAction('HistoryMarkGood', '#Notif_History_Marked');
	}

	function itemBad(e)
	{
		e.preventDefault();
		ConfirmDialog.showModal('HistoryEditBadConfirmDialog', doItemBad,
			function () { HistoryUI.confirmMulti(editIds.length > 1); });
	}

	function doItemBad()
	{
		execAction('HistoryMarkBad', '#Notif_History_Marked');
	}
}(jQuery));

/*** PURGE HISTORY DIALOG *****************************************************/

var PurgeHistoryDialog = (new function($)
{
	'use strict';

	// Controls
	var $PurgeHistoryDialog;

	// State
	var actionCallback;

	this.init = function()
	{
		$PurgeHistoryDialog = $('#PurgeHistoryDialog');
	}

	this.showModal = function(_actionCallback, count, percentage)
	{
		actionCallback = _actionCallback;
		$('#PurgeHistoryDialog_count,#PurgeHistoryDialog_count2').text(count);
		$('#PurgeHistoryDialog_percentage').text(percentage);
		Util.centerDialog($PurgeHistoryDialog, true);
		$PurgeHistoryDialog.modal({backdrop: 'static'});
	}

	this.delete = function(event)
	{
		event.preventDefault(); // avoid scrolling
		$PurgeHistoryDialog.modal('hide');
		actionCallback();
	}
}(jQuery));
