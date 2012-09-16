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
var downloads_DownloadsTable;
var downloads_DownloadsTabBadge;
var downloads_DownloadsTabBadgeEmpty;
var downloads_DownloadQueueEmpty;
var downloads_DownloadsRecordsPerPage;

var downloads_notification = null;

function downloads_init()
{
	downloads_DownloadsTable = $('#DownloadsTable');
	downloads_DownloadsTabBadge = $('#DownloadsTabBadge');
	downloads_DownloadsTabBadgeEmpty = $('#DownloadsTabBadgeEmpty');
	downloads_DownloadQueueEmpty = $('#DownloadQueueEmpty');
	downloads_DownloadsRecordsPerPage = $('#DownloadsRecordsPerPage');

	var RecordsPerPage = getSetting('DownloadsRecordsPerPage', 10);
	downloads_DownloadsRecordsPerPage.val(RecordsPerPage);

	downloads_DownloadsTable.fasttable(
		{
			filterInput: $('#DownloadsTable_filter'),
			filterClearButton: $("#DownloadsTable_clearfilter"),
			pagerContainer: $('#DownloadsTable_pager'),
			infoContainer: $('#DownloadsTable_info'),
			headerCheck: $('#DownloadsTable > thead > tr:first-child'),
			filterCaseSensitive: false,
			//infoEmpty: '&nbsp;',
			pageSize: RecordsPerPage,
			maxPages: Settings_MiniTheme ? 1 : 5,
			pageDots: !Settings_MiniTheme,
			fillFieldsCallback: downloads_fillFieldsCallback,
			renderCellCallback: downloads_renderCellCallback,
			updateInfoCallback: downloads_updateInfo
		});

	downloads_DownloadsTable.on('click', 'a', downloads_edit_click);
	downloads_DownloadsTable.on('click', 'tbody div.check',
		function(event) { downloads_DownloadsTable.fasttable('itemCheckClick', this.parentNode.parentNode, event); });
	downloads_DownloadsTable.on('click', 'thead div.check',
		function() { downloads_DownloadsTable.fasttable('titleCheckClick') });
	downloads_DownloadsTable.on('mousedown', util_disableShiftMouseDown);
}

function downloads_theme()
{
	downloads_DownloadsTable.fasttable('setPageSize', getSetting('DownloadsRecordsPerPage', 10),
		Settings_MiniTheme ? 1 : 5, !Settings_MiniTheme);
}

function downloads_update()
{
	rpc('listgroups', [], downloads_groups_loaded);
}

function downloads_groups_loaded(groups)
{
	Groups = groups;
	rpc('postqueue', [100], downloads_posts_loaded);
}

function downloads_posts_loaded(posts)
{
	downloads_mergequeues(posts);
	downloads_prepare();
	rpc('urlqueue', [], downloads_urls_loaded);
}

function downloads_urls_loaded(urls)
{
	Urls = urls;
	loadNext();
}

function downloads_mergequeues(posts)
{
	var lastPPItemIndex = -1;
	for (var i=0, il=posts.length; i < il; i++)
	{
		var post = posts[i];
		var found = false;
		for (var j=0, jl=Groups.length; j < jl; j++)
		{
			var group = Groups[j];
			if (group.NZBID === post.NZBID)
			{
				found = true;
				group.post = post;
				lastPPItemIndex = j;
				break;
			}
		}

		if (!found)
		{
			// create a virtual group-item
			var group = {post: post};
			group.NZBID = post.NZBID;
			group.NZBName = post.NZBName;
			group.MaxPriority = 0;
			group.Category = '';
			group.LastID = 0;
			group.MinPostTime = 0;
			group.FileSizeMB = 0;
			group.FileSizeLo = 0;
			group.RemainingSizeMB = 0;
			group.RemainingSizeLo = 0;
			group.PausedSizeMB = 0;
			group.PausedSizeLo = 0;
			group.FileCount = 0;
			group.RemainingFileCount = 0;
			group.RemainingParCount = 0;

			// insert it after the last pp-item
			if (lastPPItemIndex > -1)
			{
				Groups.splice(lastPPItemIndex + 1, 0, group);
			}
			else
			{
				Groups.unshift(group);
			}
		}
	}
}

function downloads_prepare()
{
	for (var j=0, jl=Groups.length; j < jl; j++)
	{
		downloads_detect_status(Groups[j]);
	}
}

function downloads_redraw()
{
	downloads_redraw_table();

	show(downloads_DownloadsTabBadge, Groups.length > 0);
	show(downloads_DownloadsTabBadgeEmpty, Groups.length === 0 && Settings_MiniTheme);
	show(downloads_DownloadQueueEmpty, Groups.length === 0);
}

/*** TABLE *************************************************************************/

function downloads_redraw_table()
{
	var data = [];

	for (var i=0; i < Groups.length; i++)
	{
		var group = Groups[i];

		var nametext = group.NZBName;
		var priority = downloads_build_priorityText(group.MaxPriority);
		var estimated = downloads_build_estimated(group);
		var age = FormatAge(group.MinPostTime + Settings_TimeZoneCorrection*60*60);
		var size = FormatSizeMB(group.FileSizeMB, group.FileSizeLo);
		var remaining = FormatSizeMB(group.RemainingSizeMB-group.PausedSizeMB, group.RemainingSizeLo-group.PausedSizeLo);

		var item =
		{
			id: group.NZBID,
			group: group,
			data: { age: age, estimated: estimated, size: size, remaining: remaining },
			search: group.status + ' ' + nametext + ' ' + priority + ' ' + group.Category + ' ' + age + ' ' + size + ' ' + remaining + ' ' + estimated
		};

		data.push(item);
	}

	downloads_DownloadsTable.fasttable('update', data);
}

function downloads_fillFieldsCallback(item)
{
	var group = item.group;

	var status = downloads_build_status(group);
	var priority = downloads_build_priority(group.MaxPriority);
	var progresslabel = downloads_build_progresslabel(group);
	var progress = downloads_build_progress(group, item.data.size, item.data.remaining, item.data.estimated);

	var name = '<a href="#" nzbid="' + group.NZBID + '">' + TextToHtml(FormatNZBName(group.NZBName)) + '</a>';
	var category = TextToHtml(group.Category);

	if (!Settings_MiniTheme)
	{
		var info = name + ' ' + priority + progresslabel;
		item.fields = ['<div class="check"></div>', status, info, category, item.data.age, progress, item.data.estimated];
	}
	else
	{
		var info = '<div class="check"></div><span class="row-title">' + name + '</span>' +
			' ' + (group.status === 'queued' ? '' : status) + ' ' + priority;
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

function downloads_renderCellCallback(cell, index, item)
{
	if (4 <= index && index <= 7)
	{
		cell.className = 'text-right';
	}
}

function downloads_detect_status(group)
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
			case 'EXECUTING_SCRIPT': group.status = 'unpacking'; break;
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

function downloads_build_status(group)
{
	if (group.postprocess && group.status !== 'pp-queued')
	{
		if (Status.PostPaused)
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
		default: return '<span class="label label-status label-danger">internal error(' + group.status + ')</span>';
	}
}

function downloads_build_progress(group, totalsize, remaining, estimated)
{
	if (group.status === 'downloading' || (group.postprocess && !Status.PostPaused))
	{
		var kind = 'progress-success';
	}
	else if (group.status === 'paused' || (group.postprocess && Status.PostPaused))
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

	if (!Settings_MiniTheme)
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

function downloads_build_estimated(group)
{
	if (group.postprocess)
	{
		if (group.post.StageProgress > 0)
		{
			return FormatTimeHMS(group.post.StageTimeSec / group.post.StageProgress * (1000 - group.post.StageProgress));
		}
	}
	else if (!group.paused && Status.DownloadRate > 0)
	{
		return FormatTimeLeft((group.RemainingSizeMB-group.PausedSizeMB)*1024/(Status.DownloadRate/1024));
	}

	return '';
}

function downloads_build_progresslabel(group)
{
	var text = '';
	if (group.postprocess && !Status.PostPaused && group.post.Stage !== 'REPAIRING')
	{
		if (group.post.Log && group.post.Log.length > 0)
		{
			text = group.post.Log[group.post.Log.length-1].Text;
		}
		else if (group.post.ProgressLabel !== '')
		{
			text = group.post.ProgressLabel;
		}
	}

	return text !== '' ? ' <span class="label label-success">' + text + '</span>' : '';
}

function downloads_build_priorityText(priority)
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

function downloads_build_priority(priority)
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

function downloads_RecordsPerPage_change()
{
	var val = downloads_DownloadsRecordsPerPage.val();
	setSetting('DownloadsRecordsPerPage', val);
	downloads_DownloadsTable.fasttable('setPageSize', val);
}

function downloads_updateInfo(stat)
{
	tab_updateInfo(downloads_DownloadsTabBadge, stat);
}

/*** EDIT ******************************************************/

function downloads_edit_click()
{
	var nzbid = $(this).attr('nzbid');
	edit_group_dialog(nzbid);
}

function downloads_edit_completed()
{
	refresh_update();
	if (downloads_notification)
	{
		animateAlert(downloads_notification);
		downloads_notification = null;
	}
}

function downloads_fillPriorityCombo(combo)
{
	combo.empty();
	combo.append('<option value="-100">very low</option>');
	combo.append('<option value="-50">low</option>');
	combo.append('<option value="0">normal</option>');
	combo.append('<option value="50">high</option>');
	combo.append('<option value="100">very high</option>');
}

function downloads_fillCategoryCombo(combo)
{
	combo.empty();
	combo.append('<option></option>');

	for (var i=0; i < Categories.length; i++)
	{
		combo.append($('<option></option>').text(Categories[i]));
	}
}

/*** CHECKMARKS ******************************************************/

function downloads_check_buildEditIDList(UseLastID)
{
	var checkedRows = downloads_DownloadsTable.fasttable('checkedRows');

	var hasIDs = false;
	var checkedEditIDs = [];
	for (var i = 0; i < Groups.length; i++)
	{
		var group = Groups[i];
		if (checkedRows.indexOf(group.NZBID) > -1)
		{
			if (group.postprocess)
			{
				animateAlert('#Notif_Downloads_CheckPostProcess');
				return null;
			}

			checkedEditIDs.push(UseLastID ? group.LastID : group.NZBID);
		}
	}

	if (checkedEditIDs.length === 0)
	{
		animateAlert('#Notif_Downloads_Select');
		return null;
	}

	return checkedEditIDs;
}

/*** TOOLBAR: SELECTED ITEMS ******************************************************/

function downloads_selected_edit_click()
{
	var checkedEditIDs = downloads_check_buildEditIDList(false);
	if (!checkedEditIDs)
	{
		return;
	}

	if (checkedEditIDs.length == 1)
	{
		edit_group_dialog(checkedEditIDs[0]);
	}
	else
	{
		edit_multi_dialog(checkedEditIDs);
	}
}

function downloads_selected_merge_click()
{
	var checkedEditIDs = downloads_check_buildEditIDList(false);
	if (!checkedEditIDs)
	{
		return;
	}

	if (checkedEditIDs.length < 2)
	{
		animateAlert('#Notif_Downloads_SelectMulti');
		return;
	}

	edit_merge_dialog(checkedEditIDs);
}

function downloads_selected_pause_click()
{
	var checkedEditIDs = downloads_check_buildEditIDList(true);
	if (!checkedEditIDs)
	{
		return;
	}
	downloads_notification = '#Notif_Downloads_Paused';
	rpc('editqueue', ['GroupPause', 0, '', checkedEditIDs], downloads_edit_completed);
}

function downloads_selected_resume_click()
{
	var checkedEditIDs = downloads_check_buildEditIDList(true);
	if (!checkedEditIDs)
	{
		return;
	}
	downloads_notification = '#Notif_Downloads_Resumed';
	rpc('editqueue', ['GroupResume', 0, '', checkedEditIDs], function()
	{
		rpc('editqueue', ['GroupPauseExtraPars', 0, '', checkedEditIDs], downloads_edit_completed);
	});
}

function downloads_selected_delete_click()
{
	var checkedRows = downloads_DownloadsTable.fasttable('checkedRows');
	var downloadIDs = [];
	var postprocessIDs = [];
	for (var i = 0; i < Groups.length; i++)
	{
		var group = Groups[i];
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
		animateAlert('#Notif_Downloads_Select');
		return;
	}

	downloads_notification = '#Notif_Downloads_Deleted';

	var deletePosts = function()
	{
		if (postprocessIDs.length > 0)
		{
			rpc('editqueue', ['PostDelete', 0, '', postprocessIDs], downloads_edit_completed);
		}
		else
		{
			downloads_edit_completed();
		}
	};

	var deleteGroups = function()
	{
		if (downloadIDs.length > 0)
		{
			rpc('editqueue', ['GroupDelete', 0, '', downloadIDs], deletePosts);
		}
		else
		{
			deletePosts();
		}
	};

	confirm_dialog_show('DownloadsDeleteConfirmDialog', deleteGroups);
}

function downloads_selected_move_click(action)
{
	var checkedEditIDs = downloads_check_buildEditIDList(true);
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

	downloads_notification = '';
	rpc('editqueue', [EditAction, EditOffset, '', checkedEditIDs], downloads_edit_completed);
}

/*** TOOLBAR: GENERAL *****************************************************************/

function downloads_add_click()
{
	upload_Show();
}
