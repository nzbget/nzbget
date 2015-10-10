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
 *   1) Download tab;
 *   2) Functions for html generation for downloads, also used from other modules (edit and add dialogs).
 */

/*** DOWNLOADS TAB ***********************************************************/
 
var Downloads = (new function($)
{
	'use strict';

	// Controls
	var $DownloadsTable;
	var $DownloadsTabBadge;
	var $DownloadsTabBadgeEmpty;
	var $DownloadQueueEmpty;
	var $DownloadsRecordsPerPage;
	var $DownloadsTable_Name;

	// State
	var notification = null;
	var updateTabInfo;
	var groups;
	var urls;
	var nameColumnWidth = null;
	var minLevel = null;

	var statusData = {
		'QUEUED': { Text: 'QUEUED', PostProcess: false },
		'FETCHING': { Text: 'FETCHING', PostProcess: false },
		'DOWNLOADING': { Text: 'DOWNLOADING', PostProcess: false },
		'QS_QUEUED': { Text: 'QS-QUEUED', PostProcess: true },
		'QS_EXECUTING': { Text: 'QUEUE-SCRIPT', PostProcess: true },
		'PP_QUEUED': { Text: 'PP-QUEUED', PostProcess: true },
		'PAUSED': { Text: 'PAUSED', PostProcess: false },
		'LOADING_PARS': { Text: 'CHECKING', PostProcess: true },
		'VERIFYING_SOURCES': { Text: 'CHECKING', PostProcess: true },
		'REPAIRING': { Text: 'REPAIRING', PostProcess: true },
		'VERIFYING_REPAIRED': { Text: 'VERIFYING', PostProcess: true },
		'RENAMING': { Text: 'RENAMING', PostProcess: true },
		'MOVING': { Text: 'MOVING', PostProcess: true },
		'UNPACKING': { Text: 'UNPACKING', PostProcess: true },
		'EXECUTING_SCRIPT': { Text: 'PROCESSING', PostProcess: true },
		'PP_FINISHED': { Text: 'FINISHED', PostProcess: false }
		};
	this.statusData = statusData;

	this.init = function(options)
	{
		updateTabInfo = options.updateTabInfo;

		$DownloadsTable = $('#DownloadsTable');
		$DownloadsTabBadge = $('#DownloadsTabBadge');
		$DownloadsTabBadgeEmpty = $('#DownloadsTabBadgeEmpty');
		$DownloadQueueEmpty = $('#DownloadQueueEmpty');
		$DownloadsRecordsPerPage = $('#DownloadsRecordsPerPage');
		$DownloadsTable_Name = $('#DownloadsTable_Name');

		var recordsPerPage = UISettings.read('DownloadsRecordsPerPage', 10);
		$DownloadsRecordsPerPage.val(recordsPerPage);
		$('#DownloadsTable_filter').val('');

		$DownloadsTable.fasttable(
			{
				filterInput: $('#DownloadsTable_filter'),
				filterClearButton: $("#DownloadsTable_clearfilter"),
				pagerContainer: $('#DownloadsTable_pager'),
				infoContainer: $('#DownloadsTable_info'),
				headerCheck: $('#DownloadsTable > thead > tr:first-child'),
				infoEmpty: '&nbsp;', // this is to disable default message "No records"
				pageSize: recordsPerPage,
				maxPages: UISettings.miniTheme ? 1 : 5,
				pageDots: !UISettings.miniTheme,
				fillFieldsCallback: fillFieldsCallback,
				renderCellCallback: renderCellCallback,
				updateInfoCallback: updateInfo
			});

		$DownloadsTable.on('click', 'a', itemClick);
		$DownloadsTable.on('click', UISettings.rowSelect ? 'tbody tr' : 'tbody div.check',
			function(event) { $DownloadsTable.fasttable('itemCheckClick', UISettings.rowSelect ? this : this.parentNode.parentNode, event); });
		$DownloadsTable.on('click', 'thead div.check',
			function() { $DownloadsTable.fasttable('titleCheckClick') });
		$DownloadsTable.on('mousedown', Util.disableShiftMouseDown);
	}

	this.applyTheme = function()
	{
		$DownloadsTable.fasttable('setPageSize', UISettings.read('DownloadsRecordsPerPage', 10),
			UISettings.miniTheme ? 1 : 5, !UISettings.miniTheme);
	}

	this.update = function()
	{
		if (!groups)
		{
			$('#DownloadsTable_Category').css('width', DownloadsUI.calcCategoryColumnWidth());
		}
		
		RPC.call('listgroups', [], groups_loaded);
	}

	function groups_loaded(_groups)
	{
		groups = _groups;
		prepare();
		RPC.next();
	}

	function prepare()
	{
		for (var j=0, jl=groups.length; j < jl; j++)
		{
			var group = groups[j];
			group.postprocess = statusData[group.Status].PostProcess;
		}
	}

	this.redraw = function()
	{
		redraw_table();

		Util.show($DownloadsTabBadge, groups.length > 0);
		Util.show($DownloadsTabBadgeEmpty, groups.length === 0 && UISettings.miniTheme);
		Util.show($DownloadQueueEmpty, groups.length === 0);
	}

	this.resize = function()
	{
		calcProgressLabels();
	}
	
	/*** TABLE *************************************************************************/

	var SEARCH_FIELDS = ['name', 'status', 'priority', 'category', 'estimated', 'age', 'size', 'remaining'];
	
	function redraw_table()
	{
		var data = [];

		for (var i=0; i < groups.length; i++)
		{
			var group = groups[i];

			group.name = group.NZBName;
			group.status = DownloadsUI.buildStatusText(group);
			group.priority = DownloadsUI.buildPriorityText(group.MaxPriority);
			group.category = group.Category;
			group.estimated = DownloadsUI.buildEstimated(group);
			group.size = Util.formatSizeMB(group.FileSizeMB, group.FileSizeLo);
			group.sizemb = group.FileSizeMB;
			group.sizegb = group.FileSizeMB / 1024;
			group.left = Util.formatSizeMB(group.RemainingSizeMB-group.PausedSizeMB, group.RemainingSizeLo-group.PausedSizeLo);
			group.leftmb = group.RemainingSizeMB-group.PausedSizeMB;
			group.leftgb = group.leftmb / 1024;
			group.dupe = DownloadsUI.buildDupeText(group.DupeKey, group.DupeScore, group.DupeMode);
			var age_sec = new Date().getTime() / 1000 - (group.MinPostTime + UISettings.timeZoneCorrection*60*60);
			group.age = Util.formatAge(group.MinPostTime + UISettings.timeZoneCorrection*60*60);
			group.agem = Util.round0(age_sec / 60);
			group.ageh = Util.round0(age_sec / (60*60));
			group.aged = Util.round0(age_sec / (60*60*24));

			group._search = SEARCH_FIELDS;

			var item =
			{
				id: group.NZBID,
				data: group
			};

			data.push(item);
		}

		$DownloadsTable.fasttable('update', data);
	}

	function fillFieldsCallback(item)
	{
		var group = item.data;

		var status = DownloadsUI.buildStatus(group);
		var priority = DownloadsUI.buildPriority(group.MaxPriority);
		var progresslabel = DownloadsUI.buildProgressLabel(group, nameColumnWidth);
		var progress = DownloadsUI.buildProgress(group, item.data.size, item.data.left, item.data.estimated);
		var dupe = DownloadsUI.buildDupe(group.DupeKey, group.DupeScore, group.DupeMode);
		
		var age = new Date().getTime() / 1000 - (group.MinPostTime + UISettings.timeZoneCorrection*60*60);
		var propagation = '';
		if (group.ActiveDownloads == 0 && age < parseInt(Options.option('PropagationDelay')) * 60)
		{
			propagation = '<span class="label label-warning" title="Very recent post, temporary delayed (see option PropagationDelay)">delayed</span> ';
		}

		var name = '<a href="#" data-nzbid="' + group.NZBID + '">' + Util.textToHtml(Util.formatNZBName(group.NZBName)) + '</a>';

		var url = '';
		if (group.Kind === 'URL')
		{
			url = '<span class="label label-info">URL</span> ';
		}
		
		var health = '';
		if (group.Health < 1000 && (!group.postprocess ||
			(group.Status === 'PP_QUEUED' && group.PostTotalTimeSec === 0)))
		{
			health = ' <span class="label ' + 
				(group.Health >= group.CriticalHealth ? 'label-warning' : 'label-important') +
				'">health: ' + Math.floor(group.Health / 10) + '%</span> ';
		}

		var backup = '';
		var backupPercent = calcBackupPercent(group);
		if (backupPercent > 0)
		{
			backup = ' <a href="#" data-nzbid="' + group.NZBID + '" data-area="backup" class="badge-link"><span class="label label-warning" title="using backup news servers">backup: ' + 
				(backupPercent < 10 ? Util.round1(backupPercent) : Util.round0(backupPercent)) + '%</span> ';
		}
		
		var category = Util.textToHtml(group.Category);

		if (!UISettings.miniTheme)
		{
			var info = name + ' ' + url + priority + dupe + health + backup + propagation + progresslabel;
			item.fields = ['<div class="check img-check"></div>', status, info, category, item.data.age, progress, item.data.estimated];
		}
		else
		{
			var info = '<div class="check img-check"></div><span class="row-title">' + name + '</span>' + url +
				' ' + (group.Status === 'QUEUED' ? '' : status) + ' ' + priority + dupe + health + backup + propagation;
			if (category)
			{
				info += ' <span class="label label-status">' + category + '</span>';
			}

			if (progresslabel)
			{
				progress = '<div class="downloads-progresslabel">' + progresslabel + '</div>' + progress;
			}
			item.fields = [info, progress];
		}
	}

	function renderCellCallback(cell, index, item)
	{
		if (4 <= index && index <= 7)
		{
			cell.className = 'text-right';
		}
	}
	
	function calcBackupPercent(group)
	{
		var downloadedArticles = group.SuccessArticles + group.FailedArticles;
		if (downloadedArticles === 0)
		{
			return 0;
		}
		
		if (minLevel === null)
		{
			for (var i=0; i < Status.status.NewsServers.length; i++)
			{
				var server = Status.status.NewsServers[i];
				var level = parseInt(Options.option('Server' + server.ID + '.Level'));
				if (minLevel === null || minLevel > level)
				{
					minLevel = level;
				}
			}	
		}
		
		var backupArticles = 0;
		for (var j=0; j < group.ServerStats.length; j++)
		{
			var stat = group.ServerStats[j];
			var level = parseInt(Options.option('Server' + stat.ServerID + '.Level'));
			if (level > minLevel && stat.SuccessArticles > 0)
			{
				backupArticles += stat.SuccessArticles;
			}
		}

		var backupPercent = 0;
		if (backupArticles > 0)
		{
			backupPercent = backupArticles * 100.0 / downloadedArticles;
		}
		return backupPercent;
	}

	this.recordsPerPageChange = function()
	{
		var val = $DownloadsRecordsPerPage.val();
		UISettings.write('DownloadsRecordsPerPage', val);
		$DownloadsTable.fasttable('setPageSize', val);
	}

	function updateInfo(stat)
	{
		updateTabInfo($DownloadsTabBadge, stat);
	}

	function calcProgressLabels()
	{
		var progressLabels = $('.label-inline', $DownloadsTable);

		if (UISettings.miniTheme)
		{
			nameColumnWidth = null;
			progressLabels.css('max-width', '');
			return;
		}

		progressLabels.hide();
		nameColumnWidth = Math.max($DownloadsTable_Name.width(), 50) - 4*2;  // 4 - padding of span
		progressLabels.css('max-width', nameColumnWidth);
		progressLabels.show();
	}
	
	/*** EDIT ******************************************************/

	function itemClick(e)
	{
		e.preventDefault();
		e.stopPropagation();
		var nzbid = $(this).attr('data-nzbid');
		var area = $(this).attr('data-area');
		$(this).blur();
		DownloadsEditDialog.showModal(nzbid, groups, area);
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

	/*** CHECKMARKS ******************************************************/

	function checkBuildEditIDList(allowPostProcess, allowUrl, allowEmpty)
	{
		var checkedRows = $DownloadsTable.fasttable('checkedRows');

		var hasIDs = false;
		var checkedEditIDs = [];
		for (var i = 0; i < groups.length; i++)
		{
			var group = groups[i];
			if (checkedRows[group.NZBID])
			{
				if (group.postprocess && !allowPostProcess)
				{
					Notification.show('#Notif_Downloads_CheckPostProcess');
					return null;
				}
				if (group.Kind === 'URL' && !allowUrl)
				{
					Notification.show('#Notif_Downloads_CheckURL');
					return null;
				}

				checkedEditIDs.push(group.NZBID);
			}
		}

		if (checkedEditIDs.length === 0 && !allowEmpty)
		{
			Notification.show('#Notif_Downloads_Select');
			return null;
		}

		return checkedEditIDs;
	}

	/*** TOOLBAR: SELECTED ITEMS ******************************************************/

	this.editClick = function()
	{
		var checkedEditIDs = checkBuildEditIDList(false, true);
		if (!checkedEditIDs)
		{
			return;
		}

		if (checkedEditIDs.length == 1)
		{
			DownloadsEditDialog.showModal(checkedEditIDs[0], groups);
		}
		else
		{
			DownloadsMultiDialog.showModal(checkedEditIDs, groups);
		}
	}

	this.mergeClick = function()
	{
		var checkedEditIDs = checkBuildEditIDList(false, false);
		if (!checkedEditIDs)
		{
			return;
		}

		if (checkedEditIDs.length < 2)
		{
			Notification.show('#Notif_Downloads_SelectMulti');
			return;
		}

		DownloadsMergeDialog.showModal(checkedEditIDs, groups);
	}

	this.pauseClick = function()
	{
		var checkedEditIDs = checkBuildEditIDList(false, false);
		if (!checkedEditIDs)
		{
			return;
		}
		notification = '#Notif_Downloads_Paused';
		RPC.call('editqueue', ['GroupPause', 0, '', checkedEditIDs], editCompleted);
	}

	this.resumeClick = function()
	{
		var checkedEditIDs = checkBuildEditIDList(false, false);
		if (!checkedEditIDs)
		{
			return;
		}
		notification = '#Notif_Downloads_Resumed';
		RPC.call('editqueue', ['GroupResume', 0, '', checkedEditIDs], function()
		{
			if (Options.option('ParCheck') === 'force')
			{
				editCompleted();
			}
			else
			{
				RPC.call('editqueue', ['GroupPauseExtraPars', 0, '', checkedEditIDs], editCompleted);
			}
		});
	}

	this.deleteClick = function()
	{
		var checkedRows = $DownloadsTable.fasttable('checkedRows');
		var downloadIDs = [];
		var postprocessIDs = [];
		var hasNzb = false;
		var hasUrl = false;
		for (var i = 0; i < groups.length; i++)
		{
			var group = groups[i];
			if (checkedRows[group.NZBID])
			{
				if (group.postprocess)
				{
					postprocessIDs.push(group.NZBID);
				}
				downloadIDs.push(group.NZBID);
				hasNzb = hasNzb || group.Kind === 'NZB';
				hasUrl = hasUrl || group.Kind === 'URL';
			}
		}

		if (downloadIDs.length === 0 && postprocessIDs.length === 0)
		{
			Notification.show('#Notif_Downloads_Select');
			return;
		}

		notification = '#Notif_Downloads_Deleted';

		var deletePosts = function()
		{
			if (postprocessIDs.length > 0)
			{
				RPC.call('editqueue', ['PostDelete', 0, '', postprocessIDs], editCompleted);
			}
			else
			{
				editCompleted();
			}
		};

		var deleteGroups = function(command)
		{
			if (downloadIDs.length > 0)
			{
				RPC.call('editqueue', [command, 0, '', downloadIDs], deletePosts);
			}
			else
			{
				deletePosts();
			}
		};

		DownloadsUI.deleteConfirm(deleteGroups, true, hasNzb, hasUrl);
	}

	this.moveClick = function(action)
	{
		var checkedEditIDs = checkBuildEditIDList(true, true);
		if (!checkedEditIDs)
		{
			return;
		}

		var EditAction = '';
		var EditOffset = 0;
		switch (action)
		{
			case 'top':
				EditAction = 'GroupMoveTop';
				checkedEditIDs.reverse();
				break;
			case 'bottom':
				EditAction = 'GroupMoveBottom';
				break;
			case 'up':
				EditAction = 'GroupMoveOffset';
				EditOffset = -1;
				break;
			case 'down':
				EditAction = 'GroupMoveOffset';
				EditOffset = 1;
				checkedEditIDs.reverse();
				break;
		}

		notification = '';
		RPC.call('editqueue', [EditAction, EditOffset, '', checkedEditIDs], editCompleted);
	}

	this.sort = function(order)
	{
		var checkedEditIDs = checkBuildEditIDList(true, true, true);
		notification = '#Notif_Downloads_Sorted';
		RPC.call('editqueue', ['GroupSort', 0, order, checkedEditIDs], editCompleted);
	}
}(jQuery));


/*** FUNCTIONS FOR HTML GENERATION (also used from other modules) *****************************/

var DownloadsUI = (new function($)
{
	'use strict';
	
	// State
	var categoryColumnWidth = null;
	var dupeCheck = null;
	
	this.fillPriorityCombo = function(combo)
	{
		combo.empty();
		combo.append('<option value="900">force</option>');
		combo.append('<option value="100">very high</option>');
		combo.append('<option value="50">high</option>');
		combo.append('<option value="0">normal</option>');
		combo.append('<option value="-50">low</option>');
		combo.append('<option value="-100">very low</option>');
	}

	this.fillCategoryCombo = function(combo)
	{
		combo.empty();
		combo.append('<option></option>');

		for (var i=0; i < Options.categories.length; i++)
		{
			combo.append($('<option></option>').text(Options.categories[i]));
		}
	}

	this.buildStatusText = function(group)
	{
		var statusText = Downloads.statusData[group.Status].Text;
		if (statusText === undefined)
		{
			statusText = 'Internal error(' + group.Status + ')';
		}
		return statusText;
	}
		
	this.buildStatus = function(group)
	{
		var statusText = Downloads.statusData[group.Status].Text;
		var badgeClass = '';
		
		if (group.postprocess && group.Status !== 'PP_QUEUED' && group.Status !== 'QS_QUEUED')
		{
			badgeClass = Status.status.PostPaused && group.MinPriority < 900 ? 'label-warning' : 'label-success';
		}
		else if (group.Status === 'DOWNLOADING' || group.Status === 'FETCHING' || group.Status === 'QS_EXECUTING')
		{
			badgeClass = 'label-success';
		}
		else if (group.Status === 'PAUSED')
		{
			badgeClass = 'label-warning';
		}
		else if (statusText === undefined)
		{
			statusText = 'INTERNAL_ERROR (' + group.Status + ')';
			badgeClass = 'label-important';
		}
		
		return '<span class="label label-status ' + badgeClass + '">' + statusText + '</span>';
	}

	this.buildProgress = function(group, totalsize, remaining, estimated)
	{
		if (group.Status === 'DOWNLOADING' ||
			(group.postprocess && !(Status.status.PostPaused && group.MinPriority < 900)))
		{
			var kind = 'progress-success';
		}
		else if (group.Status === 'PAUSED' ||
			(group.postprocess && !(Status.status.PostPaused && group.MinPriority < 900)))
		{
			var kind = 'progress-warning';
		}
		else
		{
			var kind = 'progress-none';
		}

		var totalMB = group.FileSizeMB-group.PausedSizeMB;
		var remainingMB = group.RemainingSizeMB-group.PausedSizeMB;
		var percent = Math.round((totalMB - remainingMB) / totalMB * 100);
		var progress = '';

		if (group.postprocess)
		{
			totalsize = '';
			remaining = '';
			percent = Math.round(group.PostStageProgress / 10);
		}
		
		if (group.Kind === 'URL')
		{
			totalsize = '';
			remaining = '';
		}

		if (!UISettings.miniTheme)
		{
			progress =
				'<div class="progress-block">'+
				'<div class="progress progress-striped ' + kind + '">'+
				'<div class="bar" style="width:' + percent + '%;"></div>'+
				'</div>'+
				'<div class="bar-text-left">' + totalsize + '</div>'+
				'<div class="bar-text-right">' + remaining + '</div>'+
				'</div>';
		}
		else
		{
			progress =
				'<div class="progress-block">'+
				'<div class="progress progress-striped ' + kind + '">'+
				'<div class="bar" style="width:' + percent + '%;"></div>'+
				'</div>'+
				'<div class="bar-text-left">' + (totalsize !== '' ? 'total ' : '') + totalsize + '</div>'+
				'<div class="bar-text-center">' + (estimated !== '' ? '[' + estimated + ']': '') + '</div>'+
				'<div class="bar-text-right">' + remaining + (remaining !== '' ? ' left' : '') + '</div>'+
				'</div>';
		}

		return progress;
	}

	this.buildEstimated = function(group)
	{
		if (group.postprocess)
		{
			if (group.PostStageProgress > 0)
			{
				return Util.formatTimeLeft(group.PostStageTimeSec / group.PostStageProgress * (1000 - group.PostStageProgress));
			}
		}
		else if (group.Status !== 'PAUSED' && Status.status.DownloadRate > 0)
		{
			return Util.formatTimeLeft((group.RemainingSizeMB-group.PausedSizeMB)*1024/(Status.status.DownloadRate/1024));
		}

		return '';
	}

	this.buildProgressLabel = function(group, maxWidth)
	{
		var text = '';
		if (group.postprocess && !(Status.status.PostPaused && group.MinPriority < 900))
		{
			switch (group.Status)
			{
				case "LOADING_PARS":
				case "VERIFYING_SOURCES":
				case "VERIFYING_REPAIRED":
				case "UNPACKING":
				case "RENAMING":
				case "EXECUTING_SCRIPT":
					text = group.PostInfoText;
					break;
			}
		}

		return text !== '' ? ' <span class="label label-success label-inline" style="max-width:' +
			maxWidth +'px">' + text + '</span>' : '';
	}

	this.buildPriorityText = function(priority)
	{
		switch (priority)
		{
			case 0: return '';
			case 900: return 'force priority';
			case 100: return 'very high priority';
			case 50: return 'high priority';
			case -50: return 'low priority';
			case -100: return 'very low priority';
			default: return 'priority: ' + priority;
		}
	}

	this.buildPriority = function(priority)
	{
		switch (priority)
		{
			case 0: return '';
			case 900: return ' <span class="label label-priority label-important">force priority</span>';
			case 100: return ' <span class="label label-priority label-important">very high priority</span>';
			case 50: return ' <span class="label label-priority label-important">high priority</span>';
			case -50: return ' <span class="label label-priority label-info">low priority</span>';
			case -100: return ' <span class="label label-priority label-info">very low priority</span>';
		}
		if (priority > 0)
		{
			return ' <span class="label label-priority label-important">priority: ' + priority + '</span>';
		}
		else if (priority < 0)
		{
			return ' <span class="label label-priority label-info">priority: ' + priority + '</span>';
		}
	}
	
	function formatDupeText(dupeKey, dupeScore, dupeMode)
	{
		dupeKey = dupeKey.replace('rageid=', '');
		dupeKey = dupeKey.replace('imdb=', '');
		dupeKey = dupeKey.replace('series=', '');
		dupeKey = dupeKey.replace('nzb=', '#');
		dupeKey = dupeKey.replace('=', ' ');
		dupeKey = dupeKey === '' ? 'title' : dupeKey;
		return dupeKey;
	}

	this.buildDupeText = function(dupeKey, dupeScore, dupeMode)
	{
		if (dupeCheck == null)
		{
			dupeCheck = Options.option('DupeCheck') === 'yes';
		}

		if (dupeCheck && dupeKey != '' && UISettings.dupeBadges)
		{
			return formatDupeText(dupeKey, dupeScore, dupeMode);
		}
		else
		{
			return '';
		}
	}

	this.buildDupe = function(dupeKey, dupeScore, dupeMode)
	{
		if (dupeCheck == null)
		{
			dupeCheck = Options.option('DupeCheck') === 'yes';
		}

		if (dupeCheck && dupeKey != '' && UISettings.dupeBadges)
		{
			return ' <span class="label' + (dupeMode === 'FORCE' ? ' label-important' : '') +
				'" title="Duplicate key: ' + dupeKey +
				(dupeScore !== 0 ? '; score: ' + dupeScore : '') +
				(dupeMode !== 'SCORE' ? '; mode: ' + dupeMode.toLowerCase() : '') +
				'">' + formatDupeText(dupeKey, dupeScore, dupeMode) + '</span> ';
		}
		else
		{
			return '';
		}
	}

	this.resetCategoryColumnWidth = function()
	{
		categoryColumnWidth = null;
	}

	this.calcCategoryColumnWidth = function()
	{
		if (categoryColumnWidth === null)
		{
			var widthHelper = $('<div></div>').css({'position': 'absolute', 'float': 'left', 'white-space': 'nowrap', 'visibility': 'hidden'}).appendTo($('body'));

			// default (min) width
			categoryColumnWidth = 60;

			for (var i = 1; ; i++)
			{
				var opt = Options.option('Category' + i + '.Name');
				if (!opt)
				{
					break;
				}
				widthHelper.text(opt);
				var catWidth = widthHelper.width();
				categoryColumnWidth = Math.max(categoryColumnWidth, catWidth);
			}
						
			widthHelper.remove();
			
			categoryColumnWidth += 'px';
		}

		return categoryColumnWidth;
	}

	this.deleteConfirm = function(actionCallback, multi, hasNzb, hasUrl)
	{
		var dupeCheck = Options.option('DupeCheck') === 'yes';
		var cleanupDisk = Options.option('DeleteCleanupDisk') === 'yes';
		var history = Options.option('KeepHistory') !== '0';
		var dialog = null;

		function init(_dialog)
		{
			dialog = _dialog;

			if (!multi)
			{
				var html = $('#ConfirmDialog_Text').html();
				html = html.replace(/downloads/g, 'download');
				$('#ConfirmDialog_Text').html(html);
			}

			$('#DownloadsDeleteConfirmDialog_Delete', dialog).prop('checked', true);
			$('#DownloadsDeleteConfirmDialog_Delete', dialog).prop('checked', true);
			$('#DownloadsDeleteConfirmDialog_DeleteDupe', dialog).prop('checked', false);
			$('#DownloadsDeleteConfirmDialog_DeleteFinal', dialog).prop('checked', false);
			Util.show($('#DownloadsDeleteConfirmDialog_Options', dialog), history);
			Util.show($('#DownloadsDeleteConfirmDialog_Simple', dialog), !history);
			Util.show($('#DownloadsDeleteConfirmDialog_DeleteDupe,#DownloadsDeleteConfirmDialog_DeleteDupeLabel', dialog), dupeCheck && hasNzb);
			Util.show($('#DownloadsDeleteConfirmDialog_Remain', dialog), !cleanupDisk && hasNzb);
			Util.show($('#DownloadsDeleteConfirmDialog_Cleanup', dialog), cleanupDisk && hasNzb);
			Util.show('#ConfirmDialog_Help', history && dupeCheck && hasNzb);
		};

		function action()
		{
			var deleteNormal = $('#DownloadsDeleteConfirmDialog_Delete', dialog).is(':checked');
			var deleteDupe = $('#DownloadsDeleteConfirmDialog_DeleteDupe', dialog).is(':checked');
			var deleteFinal = $('#DownloadsDeleteConfirmDialog_DeleteFinal', dialog).is(':checked');
			var command = deleteNormal ? 'GroupDelete' : (deleteDupe ? 'GroupDupeDelete' : 'GroupFinalDelete');
			actionCallback(command);
		}

		ConfirmDialog.showModal('DownloadsDeleteConfirmDialog', action, init);
	}
}(jQuery));
