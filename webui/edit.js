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

var edit_curGroup;
var edit_GroupDialog;
var edit_MergeDialog;
var edit_MultiDialog;
var edit_DownloadsEdit_LogTable;
var edit_notification = null;
var edit_MergeEditIDList;
var edit_MultiIDList;
var edit_OldPriority;
var edit_OldCategory;

function edit_init()
{
	edit_GroupDialog = $('#DownloadsEdit');
	edit_MergeDialog = $('#DownloadsMerge');
	edit_MultiDialog = $('#DownloadsMulti');
	edit_group_init();
	edit_merge_init();
	edit_multi_init();

	edit_DownloadsEdit_LogTable = $('#DownloadsEdit_LogTable');
	edit_DownloadsEdit_LogTable.fasttable(
		{
			filterInput: $('#DownloadsEdit_LogTable_filter'),
			pagerContainer: $('#DownloadsEdit_LogTable_pager'),
			filterCaseSensitive: false,
			pageSize: 5,
			maxPages: 3,
			hasHeader: false,
			renderCellCallback: edit_DownloadsEdit_LogTable_renderCellCallback
		});
}

function edit_completed()
{
	edit_GroupDialog.modal('hide');
	edit_MergeDialog.modal('hide');
	edit_MultiDialog.modal('hide');
	refresh_update();
	if (edit_notification)
	{
		animateAlert(edit_notification);
		edit_notification = null;
	}
}

/*** EDIT GROUP DIALOG *************************************************************************/

function edit_group_init()
{
	$('#DownloadsEdit_Save').click(edit_group_SaveChanges);
	$('#DownloadsEdit_Pause').click(edit_group_Pause);
	$('#DownloadsEdit_Resume').click(edit_group_Resume);
	$('#DownloadsEdit_Delete').click(edit_group_Delete);
	$('#DownloadsEdit_CancelPP').click(edit_group_CancelPP);

	edit_GroupDialog.on('hidden', function ()
	{
		refresh_resume();
	});

	if (Settings_SetFocus)
	{
		edit_GroupDialog.on('shown', function ()
		{
			if ($('#DownloadsEdit_NZBNameGroup').is(":visible"))
			{
				$('#DownloadsEdit_NZBNameGroup').focus();
			}
		});
	}
}

function edit_group_dialog(nzbid)
{
	var group = null;

	// find Group object
	for (var i=0; i<Groups.length; i++)
	{
		var gr = Groups[i];
		if (gr.NZBID == nzbid)
		{
			group = gr;
			break;
		}
	}
	if (group == null)
	{
		return;
	}

	refresh_pause();

	edit_curGroup = group; // global

	var status = downloads_build_status(group);
	var age = FormatAge(group.MinPostTime + Settings_TimeZoneCorrection*60*60);
	var size = FormatSizeMB(group.FileSizeMB, group.FileSizeLo);
	var remaining = FormatSizeMB(group.RemainingSizeMB-group.PausedSizeMB, group.RemainingSizeLo-group.PausedSizeLo);
	var unpausedSize = FormatSizeMB(group.PausedSizeMB, group.PausedSizeLo);
	var estimated = group.paused ? '' : (Status.DownloadRate > 0 ? FormatTimeHMS((group.RemainingSizeMB-group.PausedSizeMB)*1024/(Status.DownloadRate/1024)) : '');

	var table = '';
	table += '<tr><td>Age</td><td class="text-right">' + age + '</td></tr>';
	table += '<tr><td>Total</td><td class="text-right">' + size + '</td></tr>';
	table += '<tr><td>Paused</td><td class="text-right">' + unpausedSize + '</td></tr>';
	table += '<tr><td>Unpaused</td><td class="text-right">' + remaining + '</td></tr>';
	table += '<tr><td>Estimated time</td><td class="text-right">' + estimated + '</td></tr>';
	table += '<tr><td>Files (total/remaining/pars)</td><td class="text-center">' + group.FileCount + ' / ' +
		group.RemainingFileCount + ' / ' + group.RemainingParCount + '</td></tr>';
	$('#DownloadsEdit_Statistics').html(table);

	$('#DownloadsEdit_Title').text(FormatNZBName(group.NZBName));
	$('DownloadsEdit_Title').html($('#DownloadsEdit_Title').html() + '&nbsp;' + status);

	$('#DownloadsEdit_NZBName').attr('value', group.NZBName);
	$('#DownloadsEdit_NZBName').attr('readonly', group.postprocess);

	// Priority
	var v = $('#DownloadsEdit_Priority');
	downloads_fillPriorityCombo(v);
	v.val(group.MaxPriority);
	if (v.val() != group.MaxPriority)
	{
		v.append('<option selected="selected">' + group.MaxPriority +'</option>');
	}
	v.attr('disabled', 'disabled');

	// Category
	var v = $('#DownloadsEdit_Category');
	downloads_fillCategoryCombo(v);
	v.val(group.Category);
	if (v.val() != group.Category)
	{
		v.append($('<option selected="selected"></option>').text(group.Category));
	}

	if (group.post && group.post.Log && group.post.Log.length > 0)
	{
		edit_fillPostLog();
	}

	show($('#DownloadsEdit_NZBNameReadonly'), group.postprocess);
	show($('#DownloadsEdit_CancelPPGroup'), group.postprocess);
	show($('#DownloadsEdit_DeleteGroup'), !group.postprocess);
	show($('#DownloadsEdit_PauseGroup'), !group.postprocess);
	show($('#DownloadsEdit_ResumeGroup'), false);
	show($('#DownloadsEdit_Save'), !group.postprocess);
	show($('#DownloadsEdit_Log'), group.postprocess && group.post.Log.length > 0);
	show($('#DownloadsEdit_PostProcessingGroup'), group.postprocess && group.post.Log.length > 0);
	group.postprocess && group.post.Log.length > 0 ? 
		$('#DownloadsEdit_StatisticsGroup').removeClass('control-group-last') : 
		$('#DownloadsEdit_StatisticsGroup').addClass('control-group-last');

	if (group.postprocess)
	{
		$('#DownloadsEdit_NZBName').attr('disabled', 'disabled');
		$('#DownloadsEdit_Priority').attr('disabled', 'disabled');
		$('#DownloadsEdit_Category').attr('disabled', 'disabled');
		$('#DownloadsEdit_Close').addClass('btn-primary');
	}
	else
	{
		$('#DownloadsEdit_NZBName').removeAttr('disabled');
		$('#DownloadsEdit_Priority').removeAttr('disabled');
		$('#DownloadsEdit_Category').removeAttr('disabled');
		$('#DownloadsEdit_Close').removeClass('btn-primary');

		if (group.RemainingSizeHi == group.PausedSizeHi && group.RemainingSizeLo == group.PausedSizeLo)
		{
			$('#DownloadsEdit_ResumeGroup').show();
			$('#DownloadsEdit_PauseGroup').hide();
		}
	}

	edit_group_EnableAllButtons();
	$('#DownloadsEdit_GeneralTabLink').tab('show');

	edit_GroupDialog.modal({backdrop: 'static'});
}

function edit_fillPostLog()
{
	var data = [];

	for (var i=0; i < edit_curGroup.post.Log.length; i++)
	{
		var message = edit_curGroup.post.Log[i];

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

		var item =
		{
			id: message,
			fields: [kind, text],
			search: message.Kind + ' ' + message.Text
		};

		data.unshift(item);
	}

	$('#DownloadsEdit_LogTable_filter').val('');
	edit_DownloadsEdit_LogTable.fasttable('update', data);
	edit_DownloadsEdit_LogTable.fasttable('setCurPage', 1);
}

function edit_DownloadsEdit_LogTable_renderCellCallback(cell, index, item)
{
	if (index === 0)
	{
		cell.width = '65px';
	}
}

function edit_group_DisableAllButtons()
{
	$('#DownloadsEdit .modal-footer .btn').attr('disabled', 'disabled');
	setTimeout(function()
	{
		$('#DownloadsEdit_Transmit').show();
	}, 500);
}

function edit_group_EnableAllButtons()
{
	$('#DownloadsEdit .modal-footer .btn').removeAttr('disabled');
	$('#DownloadsEdit_Transmit').hide();
}

function edit_group_SaveChanges()
{
	edit_group_DisableAllButtons();
	edit_notification = null;
	edit_group_SaveName();
}

function edit_group_SaveName()
{
	var name = $('#DownloadsEdit_NZBName').val();
	name !== edit_curGroup.NZBName && !edit_curGroup.postprocess ?
		rpc('editqueue', ['GroupSetName', 0, name, [edit_curGroup.LastID]], function()
		{
			edit_notification = '#Notif_Downloads_Saved';
			edit_group_SavePriority();
		})
		:edit_group_SavePriority();
}

function edit_group_SavePriority()
{
	var priority = parseInt($('#DownloadsEdit_Priority').val());
	priority !== edit_curGroup.MaxPriority && edit_curGroup.LastID > 0 ?
		rpc('editqueue', ['GroupSetPriority', 0, ''+priority, [edit_curGroup.LastID]], function()
		{
			edit_notification = '#Notif_Downloads_Saved';
			edit_group_SaveCategory();
		})
		: edit_group_SaveCategory();
}

function edit_group_SaveCategory()
{
	var category = $('#DownloadsEdit_Category').val();
	category !== edit_curGroup.Category && edit_curGroup.LastID > 0	?
		rpc('editqueue', ['GroupSetCategory', 0, category, [edit_curGroup.LastID]], function()
		{
			edit_notification = '#Notif_Downloads_Saved';
			edit_completed();
		})
		: edit_completed();
}

function edit_group_Pause()
{
	edit_group_DisableAllButtons();
	edit_notification = '#Notif_Downloads_Paused';
	rpc('editqueue', ['GroupPause', 0, '', [edit_curGroup.LastID]], edit_completed);
}

function edit_group_Resume()
{
	edit_group_DisableAllButtons();
	edit_notification = '#Notif_Downloads_Resumed';
	rpc('editqueue', ['GroupResume', 0, '', [edit_curGroup.LastID]], function()
	{
		rpc('editqueue', ['GroupPauseExtraPars', 0, '', [edit_curGroup.LastID]], edit_completed);
	});
}

function edit_group_Delete()
{
	edit_group_DisableAllButtons();
	edit_notification = '#Notif_Downloads_Deleted';
	rpc('editqueue', ['GroupDelete', 0, '', [edit_curGroup.LastID]], edit_completed);
}

function edit_group_CancelPP()
{
	edit_group_DisableAllButtons();
	edit_notification = '#Notif_Downloads_PostCanceled';

	var postDelete = function()
	{
		rpc('editqueue', ['PostDelete', 0, '', [edit_curGroup.post.ID]], edit_completed);
	};

	if (edit_curGroup.LastID > 0)
	{
		rpc('editqueue', ['GroupDelete', 0, '', [edit_curGroup.LastID]], postDelete);
	}
	else
	{
		postDelete();
	}
}

/*** MERGE DIALOG *************************************************************************/

function edit_merge_init()
{
	$('#DownloadsMerge_Merge').click(edit_merge_Merge);

	edit_MergeDialog.on('hidden', function ()
	{
		refresh_resume();
	});

	if (Settings_SetFocus)
	{
		edit_MergeDialog.on('shown', function ()
		{
			$('#DownloadsMerge_Merge').focus();
		});
	}
}

function edit_merge_dialog(nzbIdList)
{
	refresh_pause();

	edit_MergeEditIDList = [];
	$('#DownloadsMerge_Files').empty();
	for (var i = 0; i < Groups.length; i++)
	{
		var group = Groups[i];
		if (nzbIdList.indexOf(group.NZBID) > -1)
		{
			edit_MergeEditIDList.push(group.LastID);
			var html = '<table><tr><td width="18px" valign="top"><i class="icon-file" style="vertical-align:top;margin-top:2px;"></i></td><td>' +
				FormatNZBName(group.NZBName) + '</td></tr></table>';
			$('#DownloadsMerge_Files').append(html);
		}
	}

	edit_notification = '#Notif_Downloads_Merged';
	edit_MergeDialog.modal({backdrop: 'static'});
}

function edit_merge_Merge()
{
	rpc('editqueue', ['GroupMerge', 0, '', edit_MergeEditIDList], edit_completed);
}

/*** MULTI GROUP DIALOG *************************************************************************/

function edit_multi_init()
{
	$('#DownloadsMulti_Save').click(edit_multi_SaveChanges);

	edit_MultiDialog.on('hidden', function ()
	{
		refresh_resume();
	});

	if (Settings_SetFocus)
	{
		edit_MultiDialog.on('shown', function ()
		{
			if ($('#DownloadsMulti_Priority').is(":visible"))
			{
				$('#DownloadsMulti_Priority').focus();
			}
		});
	}
}

function edit_multi_dialog(nzbIdList)
{
	var groups = [];
	edit_MultiIDList = [];

	for (var i=0; i<Groups.length; i++)
	{
		var gr = Groups[i];
		if (nzbIdList.indexOf(gr.NZBID) > -1)
		{
			groups.push(gr);
			edit_MultiIDList.push(gr.LastID);
		}
	}
	if (groups.length == 0)
	{
		return;
	}

	refresh_pause();

	var FileSizeMB = 0, FileSizeLo = 0;
	var RemainingSizeMB = 0, RemainingSizeLo = 0;
	var PausedSizeMB = 0, PausedSizeLo = 0;
	var FileCount = 0, RemainingFileCount = 0, RemainingParCount = 0;
	var paused = true;
	var Priority = groups[0].MaxPriority;
	var PriorityDiff = false;
	var Category = groups[0].Category;
	var CategoryDiff = false;

	for (var i=0; i<groups.length; i++)
	{
		var group = groups[i];
		FileSizeMB += group.FileSizeMB;
		RemainingSizeMB += group.RemainingSizeMB;
		RemainingSizeLo += group.RemainingSizeLo;
		PausedSizeMB += group.PausedSizeMB;
		PausedSizeLo += group.PausedSizeLo;
		FileCount += group.FileCount;
		RemainingFileCount += group.RemainingFileCount;
		RemainingParCount += group.RemainingParCount;
		paused = paused && group.paused;
		PriorityDiff = PriorityDiff || (Priority !== group.MaxPriority);
		CategoryDiff = CategoryDiff || (Category !== group.Category);
	}
	
	var size = FormatSizeMB(FileSizeMB, FileSizeLo);
	var remaining = FormatSizeMB(RemainingSizeMB-PausedSizeMB, RemainingSizeLo-PausedSizeLo);
	var unpausedSize = FormatSizeMB(PausedSizeMB, PausedSizeLo);
	var estimated = paused ? '' : (Status.DownloadRate > 0 ? FormatTimeHMS((RemainingSizeMB-PausedSizeMB)*1024/(Status.DownloadRate/1024)) : '');

	var table = '';
	table += '<tr><td>Total</td><td class="text-right">' + size + '</td></tr>';
	table += '<tr><td>Paused</td><td class="text-right">' + unpausedSize + '</td></tr>';
	table += '<tr><td>Unpaused</td><td class="text-right">' + remaining + '</td></tr>';
	table += '<tr><td>Estimated time</td><td class="text-right">' + estimated + '</td></tr>';
	table += '<tr><td>Files (total/remaining/pars)</td><td class="text-center">' + FileCount + ' / ' +
		RemainingFileCount + ' / ' + RemainingParCount + '</td></tr>';
	$('#DownloadsMulti_Statistics').html(table);

	$('#DownloadsMulti_Title').text('Multiple records (' + groups.length + ')');

	// Priority
	var v = $('#DownloadsMulti_Priority');
	downloads_fillPriorityCombo(v);
	v.val(Priority);
	if (v.val() != Priority)
	{
		v.append('<option>' + Priority +'</option>');
		v.val(Priority);
	}
	if (PriorityDiff)
	{
		v.append('<option selected="selected">&lt;multiple values&gt;</option>');
	}
	edit_OldPriority = v.val();
	$('#DownloadsMulti_Priority').removeAttr('disabled');

	// Category
	var v = $('#DownloadsMulti_Category');
	downloads_fillCategoryCombo(v);
	v.val(Category);
	if (v.val() != Category)
	{
		v.append($('<option></option>').text(Category));
		v.val(Category);
	}
	if (CategoryDiff)
	{
		v.append('<option selected="selected">&lt;multiple values&gt;</option>');
	}
	edit_OldCategory = v.val();

	edit_multi_EnableAllButtons();
	$('#DownloadsMulti_GeneralTabLink').tab('show');

	edit_MultiDialog.modal({backdrop: 'static'});
}

function edit_multi_EnableAllButtons()
{
	$('#DownloadsMulti .modal-footer .btn').removeAttr('disabled');
	$('#DownloadsMulti_Transmit').hide();
}

function edit_multi_DisableAllButtons()
{
	$('#DownloadsMulti .modal-footer .btn').attr('disabled', 'disabled');
	setTimeout(function()
	{
		$('#DownloadsMulti_Transmit').show();
	}, 500);
}

function edit_multi_SaveChanges()
{
	edit_multi_DisableAllButtons();
	edit_notification = null;
	edit_multi_SavePriority();
}

function edit_multi_SavePriority()
{
	var priority = $('#DownloadsMulti_Priority').val();
	(priority !== edit_OldPriority && priority !== '<multiple values>') ?
		rpc('editqueue', ['GroupSetPriority', 0, priority, edit_MultiIDList], function()
		{
			edit_notification = '#Notif_Downloads_Saved';
			edit_multi_SaveCategory();
		})
		: edit_multi_SaveCategory();
}

function edit_multi_SaveCategory()
{
	var category = $('#DownloadsMulti_Category').val();
	(category !== edit_OldCategory && category !== '<multiple values>') ?
		rpc('editqueue', ['GroupSetCategory', 0, category, edit_MultiIDList], function()
		{
			edit_notification = '#Notif_Downloads_Saved';
			edit_completed();
		})
		: edit_completed();
}
