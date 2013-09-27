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

	this.init = function(options)
	{
		updateTabInfo = options.updateTabInfo;

		$DownloadsTable = $('#DownloadsTable');
		$DownloadsTabBadge = $('#DownloadsTabBadge');
		$DownloadsTabBadgeEmpty = $('#DownloadsTabBadgeEmpty');
		$DownloadQueueEmpty = $('#DownloadQueueEmpty');
		$DownloadsRecordsPerPage = $('#DownloadsRecordsPerPage');
		$DownloadsTable_Name = $('#DownloadsTable_Name');

		var recordsPerPage = UISettings.read('$DownloadsRecordsPerPage', 10);
		$DownloadsRecordsPerPage.val(recordsPerPage);

		$DownloadsTable.fasttable(
			{
				filterInput: $('#DownloadsTable_filter'),
				filterClearButton: $("#DownloadsTable_clearfilter"),
				pagerContainer: $('#DownloadsTable_pager'),
				infoContainer: $('#DownloadsTable_info'),
				headerCheck: $('#DownloadsTable > thead > tr:first-child'),
				filterCaseSensitive: false,
				infoEmpty: '&nbsp;', // this is to disable default message "No records"
				pageSize: recordsPerPage,
				maxPages: UISettings.miniTheme ? 1 : 5,
				pageDots: !UISettings.miniTheme,
				fillFieldsCallback: fillFieldsCallback,
				renderCellCallback: renderCellCallback,
				updateInfoCallback: updateInfo
			});

		$DownloadsTable.on('click', 'a', itemClick);
		$DownloadsTable.on('click', 'tbody div.check',
			function(event) { $DownloadsTable.fasttable('itemCheckClick', this.parentNode.parentNode, event); });
		$DownloadsTable.on('click', 'thead div.check',
			function() { $DownloadsTable.fasttable('titleCheckClick') });
		$DownloadsTable.on('mousedown', Util.disableShiftMouseDown);
	}

	this.applyTheme = function()
	{
		$DownloadsTable.fasttable('setPageSize', UISettings.read('$DownloadsRecordsPerPage', 10),
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
		RPC.call('postqueue', [100], posts_loaded);
	}

	function posts_loaded(posts)
	{
		mergequeues(posts);
		prepare();
		RPC.call('urlqueue', [], urls_loaded);
	}

	function urls_loaded(_urls)
	{
		urls = _urls;
		RPC.next();
	}

	function mergequeues(posts)
	{
		var lastPPItemIndex = -1;
		for (var i=0, il=posts.length; i < il; i++)
		{
			var post = posts[i];
			var found = false;
			for (var j=0, jl=groups.length; j < jl; j++)
			{
				var group = groups[j];
				if (group.NZBID === post.NZBID)
				{
					found = true;
					if (!group.post)
					{
						group.post = post;
					}
					lastPPItemIndex = j;
					break;
				}
			}

			if (!found)
			{
				// create a virtual group-item:
				// post-item has most of fields the group-item has,
				// we use post-item as basis and then add missing fields.
				group = $.extend({}, post);
				group.post = post;
				group.MaxPriority = 0;
				group.LastID = 0;
				group.MinPostTime = 0;
				group.RemainingSizeMB = 0;
				group.RemainingSizeLo = 0;
				group.PausedSizeMB = 0;
				group.PausedSizeLo = 0;
				group.RemainingFileCount = 0;
				group.RemainingParCount = 0;

				// insert it after the last pp-item
				if (lastPPItemIndex > -1)
				{
					groups.splice(lastPPItemIndex + 1, 0, group);
				}
				else
				{
					groups.unshift(group);
				}
			}
		}
	}

	function prepare()
	{
		for (var j=0, jl=groups.length; j < jl; j++)
		{
			detectStatus(groups[j]);
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

	function redraw_table()
	{
		var data = [];

		for (var i=0; i < groups.length; i++)
		{
			var group = groups[i];

			var nametext = group.NZBName;
			var priority = DownloadsUI.buildPriorityText(group.MaxPriority);
			var estimated = DownloadsUI.buildEstimated(group);
			var age = Util.formatAge(group.MinPostTime + UISettings.timeZoneCorrection*60*60);
			var size = Util.formatSizeMB(group.FileSizeMB, group.FileSizeLo);
			var remaining = Util.formatSizeMB(group.RemainingSizeMB-group.PausedSizeMB, group.RemainingSizeLo-group.PausedSizeLo);
			var dupe = DownloadsUI.buildDupeText(group.DupeMark, group.DupeKey, group.DupeScore);
			
			var item =
			{
				id: group.NZBID,
				group: group,
				data: { age: age, estimated: estimated, size: size, remaining: remaining },
				search: group.status + ' ' + nametext + ' ' + priority + ' ' + dupe + ' ' + group.Category + ' ' + age + ' ' + size + ' ' + remaining + ' ' + estimated
			};

			data.push(item);
		}

		$DownloadsTable.fasttable('update', data);
	}

	function fillFieldsCallback(item)
	{
		var group = item.group;

		var status = DownloadsUI.buildStatus(group);
		var priority = DownloadsUI.buildPriority(group.MaxPriority);
		var progresslabel = DownloadsUI.buildProgressLabel(group, nameColumnWidth);
		var progress = DownloadsUI.buildProgress(group, item.data.size, item.data.remaining, item.data.estimated);
		var dupe = DownloadsUI.buildDupe(group.DupeMark, group.DupeKey, group.DupeScore);

		var name = '<a href="#" nzbid="' + group.NZBID + '">' + Util.textToHtml(Util.formatNZBName(group.NZBName)) + '</a>';
		
		var health = '';
		if (group.Health < 1000 && (!group.postprocess ||
			(group.status === 'pp-queued' && group.post.TotalTimeSec === 0)))
		{
			health = ' <span class="label ' + 
				(group.Health >= group.CriticalHealth ? 'label-warning' : 'label-important') +
				'">health: ' + Math.floor(group.Health / 10) + '%</span> ';
		}
		
		var category = Util.textToHtml(group.Category);

		if (!UISettings.miniTheme)
		{
			var info = name + ' ' + priority + dupe + health + progresslabel;
			item.fields = ['<div class="check img-check"></div>', status, info, category, item.data.age, progress, item.data.estimated];
		}
		else
		{
			var info = '<div class="check img-check"></div><span class="row-title">' + name + '</span>' +
				' ' + (group.status === 'queued' ? '' : status) + ' ' + priority + dupe + health;
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

	function detectStatus(group)
	{
		group.paused = (group.PausedSizeLo != 0) && (group.RemainingSizeLo == group.PausedSizeLo);
		group.postprocess = group.post !== undefined;
		if (group.postprocess)
		{
			switch (group.post.Stage)
			{
				case 'QUEUED': group.status = 'pp-queued'; break;
				case 'LOADING_PARS': group.status = 'checking'; break;
				case 'VERIFYING_SOURCES': group.status = 'checking'; break;
				case 'REPAIRING': group.status = 'repairing'; break;
				case 'VERIFYING_REPAIRED': group.status = 'verifying'; break;
				case 'RENAMING': group.status = 'renaming'; break;
				case 'MOVING': group.status = 'moving'; break;
				case 'UNPACKING': group.status = 'unpacking'; break;
				case 'EXECUTING_SCRIPT': group.status = 'processing'; break;
				case 'FINISHED': group.status = 'finished'; break;
				default: group.status = 'error: ' + group.post.Stage; break;
			}
		}
		else if (group.ActiveDownloads > 0)
		{
			group.status = 'downloading';
		}
		else if (group.paused)
		{
			group.status = 'paused';
		}
		else
		{
			group.status = 'queued';
		}
	}

	this.recordsPerPageChange = function()
	{
		var val = $DownloadsRecordsPerPage.val();
		UISettings.write('$DownloadsRecordsPerPage', val);
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
		var nzbid = $(this).attr('nzbid');
		$(this).blur();
		DownloadsEditDialog.showModal(nzbid, groups);
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

	function checkBuildEditIDList(UseLastID)
	{
		var checkedRows = $DownloadsTable.fasttable('checkedRows');

		var hasIDs = false;
		var checkedEditIDs = [];
		for (var i = 0; i < groups.length; i++)
		{
			var group = groups[i];
			if (checkedRows.indexOf(group.NZBID) > -1)
			{
				if (group.postprocess)
				{
					Notification.show('#Notif_Downloads_CheckPostProcess');
					return null;
				}

				checkedEditIDs.push(UseLastID ? group.LastID : group.NZBID);
			}
		}

		if (checkedEditIDs.length === 0)
		{
			Notification.show('#Notif_Downloads_Select');
			return null;
		}

		return checkedEditIDs;
	}

	/*** TOOLBAR: SELECTED ITEMS ******************************************************/

	this.editClick = function()
	{
		var checkedEditIDs = checkBuildEditIDList(false);
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
		var checkedEditIDs = checkBuildEditIDList(false);
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
		var checkedEditIDs = checkBuildEditIDList(true);
		if (!checkedEditIDs)
		{
			return;
		}
		notification = '#Notif_Downloads_Paused';
		RPC.call('editqueue', ['GroupPause', 0, '', checkedEditIDs], editCompleted);
	}

	this.resumeClick = function()
	{
		var checkedEditIDs = checkBuildEditIDList(true);
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
		for (var i = 0; i < groups.length; i++)
		{
			var group = groups[i];
			if (checkedRows.indexOf(group.NZBID) > -1)
			{
				if (group.postprocess)
				{
					postprocessIDs.push(group.post.ID);
				}
				if (group.LastID > 0)
				{
					downloadIDs.push(group.LastID);
				}
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

		var deleteGroups = function()
		{
			if (downloadIDs.length > 0)
			{
				RPC.call('editqueue', ['GroupDelete', 0, '', downloadIDs], deletePosts);
			}
			else
			{
				deletePosts();
			}
		};

		Util.show('#DownloadsDeleteConfirmDialog_Cleanup', Options.option('DeleteCleanupDisk') === 'yes');
		Util.show('#DownloadsDeleteConfirmDialog_Remain', Options.option('DeleteCleanupDisk') != 'yes');
		
		ConfirmDialog.showModal('DownloadsDeleteConfirmDialog', deleteGroups);
	}

	this.moveClick = function(action)
	{
		var checkedEditIDs = checkBuildEditIDList(true);
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
}(jQuery));


/*** FUNCTIONS FOR HTML GENERATION (also used from other modules) *****************************/

var DownloadsUI = (new function($)
{
	'use strict';
	
	// State
	var categoryColumnWidth = null;
	
	this.fillPriorityCombo = function(combo)
	{
		combo.empty();
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

	this.buildStatus = function(group)
	{
		if (group.postprocess && group.status !== 'pp-queued')
		{
			if (Status.status.PostPaused)
			{
				return '<span class="label label-status label-warning">' + group.status + '</span>';
			}
			else
			{
				return '<span class="label label-status label-success">' + group.status + '</span>';
			}
		}
		switch (group.status)
		{
			case 'pp-queued': return '<span class="label label-status">pp-queued</span>';
			case 'downloading': return '<span class="label label-status label-success">downloading</span>';
			case 'paused': return '<span class="label label-status label-warning">paused</span>';
			case 'queued': return '<span class="label label-status">queued</span>';
			default: return '<span class="label label-status label-important">internal error(' + group.status + ')</span>';
		}
	}

	this.buildProgress = function(group, totalsize, remaining, estimated)
	{
		if (group.status === 'downloading' || (group.postprocess && !Status.status.PostPaused))
		{
			var kind = 'progress-success';
		}
		else if (group.status === 'paused' || (group.postprocess && Status.status.PostPaused))
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
			percent = Math.round(group.post.StageProgress / 10);
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
			if (group.post.StageProgress > 0)
			{
				return Util.formatTimeLeft(group.post.StageTimeSec / group.post.StageProgress * (1000 - group.post.StageProgress));
			}
		}
		else if (!group.paused && Status.status.DownloadRate > 0)
		{
			return Util.formatTimeLeft((group.RemainingSizeMB-group.PausedSizeMB)*1024/(Status.status.DownloadRate/1024));
		}

		return '';
	}

	this.buildProgressLabel = function(group, maxWidth)
	{
		var text = '';
		if (group.postprocess && !Status.status.PostPaused)
		{
			switch (group.post.Stage)
			{
				case "REPAIRING":
					break;
				case "LOADING_PARS":
				case "VERIFYING_SOURCES":
				case "VERIFYING_REPAIRED":
				case "UNPACKING":
				case "RENAMING":
					text = group.post.ProgressLabel;
					break;
				case "EXECUTING_SCRIPT":
					if (group.post.Log && group.post.Log.length > 0)
					{
						text = group.post.Log[group.post.Log.length-1].Text;
						// remove "for <nzb-name>" from label text
						text = text.replace(' for ' + group.NZBName, ' ');
					}
					else
					{
						text = group.post.ProgressLabel;
					}
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
	
	function formatDupeText(dupeKey, dupeScore)
	{
		dupeKey = dupeKey.replace('rageid=', '');
		dupeKey = dupeKey.replace('imdb=', '');
		dupeKey = dupeKey.replace('nzb=', '#');
		dupeKey = dupeKey.replace('=', ' ');
		dupeKey = dupeKey === '' ? 'title' : dupeKey;
		return dupeKey;
	}

	this.buildDupeText = function(dupe, dupeKey, dupeScore)
	{
		if (dupe)
		{
			return formatDupeText(dupeKey, dupeScore);
		}
		else
		{
			return '';
		}
	}

	this.buildDupe = function(dupe, dupeKey, dupeScore)
	{
		if (dupe)
		{
			return ' <span class="label" title="Duplicate: ' + dupeKey + (dupeScore != 0 ? ' (score: ' + dupeScore + ')' : '') +
				'">' + formatDupeText(dupeKey, dupeScore) + '</span> ';
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
}(jQuery));
