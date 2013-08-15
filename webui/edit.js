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
 *   1) Download edit dialog;
 *   2) Download multi edit dialog (edit multiple items);
 *   3) Download merge dialog;
 *   4) Download split dialog;
 *   5) History edit dialog.
 */
 
/*** DOWNLOAD EDIT DIALOG ************************************************************/
 
var DownloadsEditDialog = (new function($)
{
	'use strict';

	// Controls
	var $DownloadsEditDialog;
	var $DownloadsLogTable;
	var $DownloadsFileTable;
	var $DownloadsEdit_ParamData;
	
	// State
	var curGroup;
	var notification = null;
	var postParams = [];
	var lastPage;
	var lastFullscreen;
	var logFilled;
	var files;

	this.init = function()
	{
		$DownloadsEditDialog = $('#DownloadsEditDialog');
		$DownloadsEdit_ParamData = $('#DownloadsEdit_ParamData');

		$('#DownloadsEdit_Save').click(saveChanges);
		$('#DownloadsEdit_Pause').click(itemPause);
		$('#DownloadsEdit_Resume').click(itemResume);
		$('#DownloadsEdit_Delete').click(itemDelete);
		$('#DownloadsEdit_CancelPP').click(itemCancelPP);
		$('#DownloadsEdit_Param, #DownloadsEdit_Log, #DownloadsEdit_File').click(tabClick);
		$('#DownloadsEdit_Back').click(backClick);

		$DownloadsLogTable = $('#DownloadsEdit_LogTable');
		$DownloadsLogTable.fasttable(
			{
				filterInput: '#DownloadsEdit_LogTable_filter',
				pagerContainer: '#DownloadsEdit_LogTable_pager',
				filterCaseSensitive: false,
				pageSize: 100,
				maxPages: 3,
				hasHeader: true,
				renderCellCallback: logTableRenderCellCallback
			});
			
		$DownloadsFileTable = $('#DownloadsEdit_FileTable');
		$DownloadsFileTable.fasttable(
			{
				filterInput: '#DownloadsEdit_FileTable_filter',
				pagerContainer: '#DownloadsEdit_FileTable_pager',
				filterCaseSensitive: false,
				headerCheck: '#DownloadsEdit_FileTable > thead > tr:first-child',
				pageSize: 10000,
				hasHeader: true,
				renderCellCallback: fileTableRenderCellCallback
			});

		$DownloadsFileTable.on('click', 'tbody div.check',
			function(event) { $DownloadsFileTable.fasttable('itemCheckClick', this.parentNode.parentNode, event); });
		$DownloadsFileTable.on('click', 'thead div.check',
			function() { $DownloadsFileTable.fasttable('titleCheckClick') });
		$DownloadsFileTable.on('mousedown', Util.disableShiftMouseDown);
			
		$DownloadsEditDialog.on('hidden', function()
		{
			// cleanup
			$DownloadsLogTable.fasttable('update', []);
			$DownloadsFileTable.fasttable('update', []);
			$DownloadsEdit_ParamData.empty();
			// resume updates
			Refresher.resume();
		});

		TabDialog.extend($DownloadsEditDialog);
		
		if (UISettings.setFocus)
		{
			$DownloadsEditDialog.on('shown', function()
			{
				if ($('#DownloadsEdit_NZBName').is(":visible"))
				{
					$('#DownloadsEdit_NZBName').focus();
				}
			});
		}
	}

	this.showModal = function(nzbid, allGroups)
	{
		var group = null;

		// find Group object
		for (var i=0; i<allGroups.length; i++)
		{
			var gr = allGroups[i];
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

		Refresher.pause();

		curGroup = group;

		var status = DownloadsUI.buildStatus(group);
		var age = Util.formatAge(group.MinPostTime + UISettings.timeZoneCorrection*60*60);
		var size = Util.formatSizeMB(group.FileSizeMB, group.FileSizeLo);
		var remaining = Util.formatSizeMB(group.RemainingSizeMB-group.PausedSizeMB, group.RemainingSizeLo-group.PausedSizeLo);
		var unpausedSize = Util.formatSizeMB(group.PausedSizeMB, group.PausedSizeLo);
		var estimated = group.paused ? '' : (Status.status.DownloadRate > 0 ? Util.formatTimeHMS((group.RemainingSizeMB-group.PausedSizeMB)*1024/(Status.status.DownloadRate/1024)) : '');

		var table = '';
		table += '<tr><td>Age</td><td class="text-right">' + age + '</td></tr>';
		table += '<tr><td>Total</td><td class="text-right">' + size + '</td></tr>';
		table += '<tr><td>Paused</td><td class="text-right">' + unpausedSize + '</td></tr>';
		table += '<tr><td>Unpaused</td><td class="text-right">' + remaining + '</td></tr>';
		//table += '<tr><td>Active downloads</td><td class="text-right">' + group.ActiveDownloads + '</td></tr>';
		//table += '<tr><td>Estimated time</td><td class="text-right">' + estimated + '</td></tr>';
		table += '<tr><td>Health (current/critical)</td><td class="text-center">' + Math.floor(group.Health / 10) + 
			'% / ' + Math.floor(group.CriticalHealth / 10) + '%</td></tr>';
		table += '<tr><td>Files (total/remaining/pars)</td><td class="text-center">' + group.FileCount + ' / ' +
			group.RemainingFileCount + ' / ' + group.RemainingParCount + '</td></tr>';
		$('#DownloadsEdit_Statistics').html(table);

		$('#DownloadsEdit_Title').text(Util.formatNZBName(group.NZBName));
		$('DownloadsEdit_Title').html($('#DownloadsEdit_Title').html() + '&nbsp;' + status);

		$('#DownloadsEdit_NZBName').attr('value', group.NZBName);
		$('#DownloadsEdit_NZBName').attr('readonly', group.postprocess);

		// Priority
		var v = $('#DownloadsEdit_Priority');
		DownloadsUI.fillPriorityCombo(v);
		v.val(group.MaxPriority);
		if (v.val() != group.MaxPriority)
		{
			v.append('<option selected="selected">' + group.MaxPriority +'</option>');
		}
		v.attr('disabled', 'disabled');

		// Category
		var v = $('#DownloadsEdit_Category');
		DownloadsUI.fillCategoryCombo(v);
		v.val(group.Category);
		if (v.val() != group.Category)
		{
			v.append($('<option selected="selected"></option>').text(group.Category));
		}

		$DownloadsLogTable.fasttable('update', []);
		$DownloadsFileTable.fasttable('update', []);

		var postParamConfig = ParamTab.createPostParamConfig();
		
		Util.show('#DownloadsEdit_NZBNameReadonly', group.postprocess);
		Util.show('#DownloadsEdit_CancelPP', group.postprocess);
		Util.show('#DownloadsEdit_Delete', !group.postprocess);
		Util.show('#DownloadsEdit_Pause', !group.postprocess);
		Util.show('#DownloadsEdit_Resume', false);
		Util.show('#DownloadsEdit_Save', !group.postprocess);
		var postParam = postParamConfig[0].options.length > 0;
		var postLog = group.postprocess && group.post.Log.length > 0;
		Util.show('#DownloadsEdit_Param', postParam);
		Util.show('#DownloadsEdit_Log', postLog);

		if (group.postprocess)
		{
			$('#DownloadsEdit_NZBName').attr('disabled', 'disabled');
			$('#DownloadsEdit_Priority').attr('disabled', 'disabled');
			$('#DownloadsEdit_Category').attr('disabled', 'disabled');
			$('#DownloadsEdit_Close').addClass('btn-primary');
			$('#DownloadsEdit_Close').text('Close');
		}
		else
		{
			$('#DownloadsEdit_NZBName').removeAttr('disabled');
			$('#DownloadsEdit_Priority').removeAttr('disabled');
			$('#DownloadsEdit_Category').removeAttr('disabled');
			$('#DownloadsEdit_Close').removeClass('btn-primary');
			$('#DownloadsEdit_Close').text('Cancel');

			if (group.RemainingSizeHi == group.PausedSizeHi && group.RemainingSizeLo == group.PausedSizeLo)
			{
				$('#DownloadsEdit_Resume').show();
				$('#DownloadsEdit_Pause').hide();
			}
		}

		if (postParam)
		{
			postParams = ParamTab.buildPostParamTab($DownloadsEdit_ParamData, postParamConfig, curGroup.Parameters);
		}

		EditUI.buildDNZBLinks(curGroup.Parameters, 'DownloadsEdit_DNZB');
		
		enableAllButtons();

		$('#DownloadsEdit_GeneralTab').show();
		$('#DownloadsEdit_ParamTab').hide();
		$('#DownloadsEdit_LogTab').hide();
		$('#DownloadsEdit_FileTab').hide();
		$('#DownloadsEdit_Back').hide();
		$('#DownloadsEdit_BackSpace').show();
		$DownloadsEditDialog.restoreTab();
		
		$('#DownloadsEdit_FileTable_filter').val('');
		$('#DownloadsEdit_LogTable_filter').val('');
		$('#DownloadsEdit_LogTable_pagerBlock').hide();

		files = null;
		logFilled = false;
		notification = null;

		$DownloadsEditDialog.modal({backdrop: 'static'});
	}

	function completed()
	{
		$DownloadsEditDialog.modal('hide');
		Refresher.update();
		if (notification)
		{
			Notification.show(notification);
			notification = null;
		}
	}
	
	function tabClick(e)
	{
		e.preventDefault();

		$('#DownloadsEdit_Back').fadeIn(500);
		$('#DownloadsEdit_BackSpace').hide();
		var tab = '#' + $(this).attr('data-tab');
		lastPage = $(tab);
		lastFullscreen = ($(this).attr('data-fullscreen') === 'true') && !UISettings.miniTheme;
		
		$('#DownloadsEdit_FileBlock').removeClass('modal-inner-scroll');
		$('#DownloadsEdit_FileBlock').css('top', '');
		
		if (UISettings.miniTheme && files === null)
		{
			$('#DownloadsEdit_FileBlock').css('min-height', $DownloadsEditDialog.height());
		}

		if (UISettings.miniTheme && !logFilled)
		{
			$('#DownloadsEdit_LogBlock').css('min-height', $DownloadsEditDialog.height());
		}
		
		$DownloadsEditDialog.switchTab($('#DownloadsEdit_GeneralTab'), lastPage, 
			e.shiftKey || !UISettings.slideAnimation ? 0 : 500, 
			{fullscreen: lastFullscreen, mini: UISettings.miniTheme, complete: function()
				{
					if (!UISettings.miniTheme)
					{
						$('#DownloadsEdit_FileBlock').css('top', $('#DownloadsEdit_FileBlock').position().top);
						$('#DownloadsEdit_FileBlock').addClass('modal-inner-scroll');
					}
					else
					{
						$('#DownloadsEdit_FileBlock').css('min-height', '');
						$('#DownloadsEdit_LogBlock').css('min-height', '');
					}
				}});

		if (tab === '#DownloadsEdit_LogTab' && !logFilled && curGroup.post && 
			curGroup.post.Log && curGroup.post.Log.length > 0)
		{
			fillLog();
		}

		if (tab === '#DownloadsEdit_FileTab' && files === null)
		{
			fillFiles();
		}
	}

	function backClick(e)
	{
		e.preventDefault();
		$('#DownloadsEdit_Back').fadeOut(500, function()
		{
			$('#DownloadsEdit_BackSpace').show();
		});

		$('#DownloadsEdit_FileBlock').removeClass('modal-inner-scroll');
		$('#DownloadsEdit_FileBlock').css('top', '');
		
		$DownloadsEditDialog.switchTab(lastPage, $('#DownloadsEdit_GeneralTab'), 
			e.shiftKey || !UISettings.slideAnimation ? 0 : 500,
			{fullscreen: lastFullscreen, mini: UISettings.miniTheme, back: true});
	}

	function disableAllButtons()
	{
		$('#DownloadsEditDialog .modal-footer .btn').attr('disabled', 'disabled');
		setTimeout(function()
		{
			$('#DownloadsEdit_Transmit').show();
		}, 500);
	}

	function enableAllButtons()
	{
		$('#DownloadsEditDialog .modal-footer .btn').removeAttr('disabled');
		$('#DownloadsEdit_Transmit').hide();
	}

	function saveChanges(e)
	{
		e.preventDefault();
		disableAllButtons();
		notification = null;
		saveName();
	}

	function saveName()
	{
		var name = $('#DownloadsEdit_NZBName').val();
		name !== curGroup.NZBName && !curGroup.postprocess ?
			RPC.call('editqueue', ['GroupSetName', 0, name, [curGroup.LastID]], function()
			{
				notification = '#Notif_Downloads_Saved';
				savePriority();
			})
			:savePriority();
	}

	function savePriority()
	{
		var priority = parseInt($('#DownloadsEdit_Priority').val());
		priority !== curGroup.MaxPriority && curGroup.LastID > 0 ?
			RPC.call('editqueue', ['GroupSetPriority', 0, ''+priority, [curGroup.LastID]], function()
			{
				notification = '#Notif_Downloads_Saved';
				saveCategory();
			})
			: saveCategory();
	}

	function saveCategory()
	{
		var category = $('#DownloadsEdit_Category').val();
		category !== curGroup.Category && curGroup.LastID > 0	?
			RPC.call('editqueue', ['GroupSetCategory', 0, category, [curGroup.LastID]], function()
			{
				notification = '#Notif_Downloads_Saved';
				saveParam();
			})
			: saveParam();
	}

	function itemPause(e)
	{
		e.preventDefault();
		disableAllButtons();
		notification = '#Notif_Downloads_Paused';
		RPC.call('editqueue', ['GroupPause', 0, '', [curGroup.LastID]], completed);
	}

	function itemResume(e)
	{
		e.preventDefault();
		disableAllButtons();
		notification = '#Notif_Downloads_Resumed';
		RPC.call('editqueue', ['GroupResume', 0, '', [curGroup.LastID]], function()
		{
			if (Options.option('ParCheck') === 'force')
			{
				completed();
			}
			else
			{
				RPC.call('editqueue', ['GroupPauseExtraPars', 0, '', [curGroup.LastID]], completed);
			}
		});
	}

	function itemDelete(e)
	{
		e.preventDefault();
		Util.show('#DownloadEditDeleteConfirmDialog_Cleanup', Options.option('DeleteCleanupDisk') === 'yes');
		Util.show('#DownloadEditDeleteConfirmDialog_Remain', Options.option('DeleteCleanupDisk') != 'yes');
		ConfirmDialog.showModal('DownloadEditDeleteConfirmDialog', doItemDelete);
	}

	function doItemDelete()
	{
		disableAllButtons();
		notification = '#Notif_Downloads_Deleted';
		RPC.call('editqueue', ['GroupDelete', 0, '', [curGroup.LastID]], completed);
	}

	function itemCancelPP(e)
	{
		e.preventDefault();
		disableAllButtons();
		notification = '#Notif_Downloads_PostCanceled';

		var postDelete = function()
		{
			RPC.call('editqueue', ['PostDelete', 0, '', [curGroup.post.ID]], completed);
		};

		if (curGroup.LastID > 0)
		{
			RPC.call('editqueue', ['GroupDelete', 0, '', [curGroup.LastID]], postDelete);
		}
		else
		{
			postDelete();
		}
	}

	/*** TAB: POST-PROCESSING PARAMETERS **************************************************/

	function saveParam()
	{
		var paramList = ParamTab.prepareParamRequest(postParams);
		saveNextParam(paramList);
	}

	function saveNextParam(paramList)
	{
		if (paramList.length > 0)
		{
			RPC.call('editqueue', ['GroupSetParameter', 0, paramList[0], [curGroup.LastID]], function()
			{
				notification = '#Notif_Downloads_Saved';
				paramList.shift();
				saveNextParam(paramList);
			})
		}
		else
		{
			saveFiles();
		}
	}

	/*** TAB: LOG *************************************************************************/

	function fillLog()
	{
		logFilled = true;
		var data = [];

		for (var i=0; i < curGroup.post.Log.length; i++)
		{
			var message = curGroup.post.Log[i];

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
			var time = Util.formatDateTime(message.Time + UISettings.timeZoneCorrection*60*60);
			var fields;

			if (!UISettings.miniTheme)
			{
				fields = [kind, time, text];
			}
			else
			{
				var info = kind + ' <span class="label">' + time + '</span> ' + text;
				fields = [info];
			}
			
			var item =
			{
				id: message,
				fields: fields,
				search: message.Kind + ' ' + time + ' ' + message.Text
			};

			data.unshift(item);
		}

		$DownloadsLogTable.fasttable('update', data);
		$DownloadsLogTable.fasttable('setCurPage', 1);
		Util.show('#DownloadsEdit_LogTable_pagerBlock', data.length > 100);
	}

	function logTableRenderCellCallback(cell, index, item)
	{
		if (index === 0)
		{
			cell.width = '65px';
		}
	}

	/*** TAB: FILES *************************************************************************/

	function fillFiles()
	{
		$('.loading-block', $DownloadsEditDialog).show();
		RPC.call('listfiles', [0, 0, curGroup.NZBID], filesLoaded);
	}

	function filesLoaded(fileArr)
	{
		$('.loading-block', $DownloadsEditDialog).hide();

		files = fileArr;

		var data = [];

		for (var i=0; i < files.length; i++)
		{
			var file = files[i];

			if (!file.status)
			{
				file.status = file.Paused ? (file.ActiveDownloads > 0 ? 'pausing' : 'paused') : (file.ActiveDownloads > 0 ? 'downloading' : 'queued');
			}
			
			var age = Util.formatAge(file.PostTime + UISettings.timeZoneCorrection*60*60);
			var size = Util.formatSizeMB(0, file.FileSizeLo);
			if (file.FileSizeLo !== file.RemainingSizeLo)
			{
				size = '(' + Util.round0(file.RemainingSizeLo / file.FileSizeLo * 100) + '%) ' + size;
			}	

			var status;
			switch (file.status)
			{
				case 'downloading':
				case 'pausing': status = '<span class="label label-status label-success">' + file.status + '</span>'; break;
				case 'paused': status = '<span class="label label-status label-warning">paused</span>'; break;
				case 'queued': status = '<span class="label label-status">queued</span>'; break;
				case 'deleted': status = '<span class="label label-status label-important">deleted</span>'; break;
				default: status = '<span class="label label-status label-important">internal error(' + file.status + ')</span>';
			}
			
			var priority = '';
			if (file.Priority != curGroup.MaxPriority)
			{
				priority = DownloadsUI.buildPriority(file.Priority);
			}

			var name = Util.textToHtml(file.Filename);
			var fields;

			if (!UISettings.miniTheme)
			{
				var info = name + ' ' + priority;
				fields = ['<div class="check img-check"></div>', status, info, age, size];
			}
			else
			{
				var info = '<div class="check img-check"></div><span class="row-title">' + name + '</span>' +
					' ' + (file.status === 'queued' ? '' : status) + ' ' + priority;
				fields = [info];
			}
			
			var item =
			{
				id: file.ID,
				file: file,
				fields: fields,
				search: file.status + ' ' + file.Filename + ' ' + priority + ' ' + age + ' ' + size
			};

			data.push(item);
		}

		$DownloadsFileTable.fasttable('update', data);
		$DownloadsFileTable.fasttable('setCurPage', 1);
	}

	function fileTableRenderCellCallback(cell, index, item)
	{
		if (index > 2)
		{
			cell.className = 'text-right';
		}
	}

	this.editActionClick = function(action)
	{
		if (files.length == 0)
		{
			return;
		}

		var checkedRows = $DownloadsFileTable.fasttable('checkedRows');
		if (checkedRows.length == 0)
		{
			Notification.show('#Notif_Edit_Select');
			return;
		}

		for (var i = 0; i < files.length; i++)
		{
			var file = files[i];
			file.moved = false;
		}

		var editIDList = [];
		var splitError = false;
		
		for (var i = 0; i < files.length; i++)
		{
			var n = i;
			if (action === 'down' || action === 'top')
			{
				// iterate backwards in the file list
				n = files.length-1-i;
			}
			var file = files[n];
			
			if (checkedRows.indexOf(file.ID) > -1)
			{
				editIDList.push(file.ID);
				
				switch (action)
				{
					case 'pause':
						file.status = 'paused';
						file.editAction = action;
						break;
					case 'resume':
						file.status = 'queued';
						file.editAction = action;
						break;
					case 'delete':
						file.status = 'deleted';
						file.editAction = action;
						break;
					case 'top':
						if (!file.moved)
						{
							files.splice(n, 1);
							files.unshift(file);
							file.moved = true;
							file.editMoved = true;
							i--;
						}
						break;
					case 'up':
						if (!file.moved && i > 0)
						{
							files.splice(i, 1);
							files.splice(i-1, 0, file);
							file.moved = true;
							file.editMoved = true;
						}
						break;
					case 'down':
						if (!file.moved && i > 0)
						{
							files.splice(n, 1);
							files.splice(n+1, 0, file);
							file.moved = true;
							file.editMoved = true;
						}
						break;
					case 'bottom':
						if (!file.moved)
						{
							files.splice(i, 1);
							files.push(file);
							file.moved = true;
							file.editMoved = true;
							i--;
						}
						break;
					case 'split':
						if (file.ActiveDownloads > 0 || file.FileSizeLo !== file.RemainingSizeLo)
						{
							splitError = true;
						}
						break;
				}
			}
		}
	
		if (action === 'split')
		{
			if (splitError)
			{
				Notification.show('#Notif_Downloads_SplitNotPossible');
			}
			else
			{
				DownloadsSplitDialog.showModal(curGroup, editIDList);
			}
		}
	
		filesLoaded(files);
	}

	function saveFilesActions(actions, commands)
	{
		if (actions.length === 0 || !files || files.length === 0)
		{
			saveFileOrder();
			return;
		}
		
		var action = actions.shift();
		var command = commands.shift();

		var IDs = [];
		for (var i = 0; i < files.length; i++)
		{
			var file = files[i];
			if (file.editAction === action)
			{
				IDs.push(file.ID);
			}
		}

		if (IDs.length > 0)
		{
			RPC.call('editqueue', [command, 0, '', IDs], function()
			{
				notification = '#Notif_Downloads_Saved';
				saveFilesActions(actions, commands);
			})
		}
		else
		{
			saveFilesActions(actions, commands);
		}
	}

	function saveFiles()
	{
		saveFilesActions(['pause', 'resume', 'delete'], ['FilePause', 'FileResume', 'FileDelete']);
	}

	function saveFileOrder()
	{
		if (!files || files.length === 0)
		{
			completed();
			return;
		}

		var IDs = [];
		var hasMovedFiles = false;
		for (var i = 0; i < files.length; i++)
		{
			var file = files[i];
			IDs.push(file.ID);
			hasMovedFiles |= file.editMoved;
		}
		
		if (hasMovedFiles)
		{
			RPC.call('editqueue', ['FileReorder', 0, '', IDs], function()
			{
				notification = '#Notif_Downloads_Saved';
				completed();
			})
		}
		else
		{
			completed();
		}
	}
}(jQuery));


/*** COMMON FUNCTIONS FOR EDIT DIALOGS ************************************************************/

var EditUI = (new function($)
{
	'use strict'

	this.buildDNZBLinks = function(parameters, prefix)
	{
		$('.' + prefix).hide();
		var hasItems = false;
		
		for (var i=0; i < parameters.length; i++)
		{
			var param = parameters[i];
			if (param.Name.substr(0, 6) === '*DNZB:')
			{
				var linkName = param.Name.substr(6, 100);
				var $paramLink = $('#' + prefix + '_' + linkName);
				if($paramLink.length > 0)
				{
					$paramLink.attr('href', param.Value);
					$paramLink.show();
					hasItems = true;
				}
			}
		}
		
		Util.show('#' + prefix + '_Section', hasItems);
	}
}(jQuery));


/*** PARAM TAB FOR EDIT DIALOGS ************************************************************/

var ParamTab = (new function($)
{
	'use strict'

	this.buildPostParamTab = function(configData, postParamConfig, parameters)
	{
		var postParams = $.extend(true, [], postParamConfig);
		Options.mergeValues(postParams, parameters);
		var content = Config.buildOptionsContent(postParams[0]);
		configData.empty();
		configData.append(content);
		configData.addClass('retain-margin');

		var lastClass = '';
		var lastDiv = null;
		for (var i=0; i < configData.children().length; i++)
		{
			var div = $(configData.children()[i]);
			var divClass = div.attr('class');
			if (divClass != lastClass && lastClass != '')
			{
				lastDiv.addClass('wants-divider');
			}
			lastDiv = div;
			lastClass = divClass;
		}
		return postParams;
	}

	this.createPostParamConfig = function()
	{
		var postParamConfig = Options.postParamConfig;
		defineBuiltinParams(postParamConfig);
		return postParamConfig;
	}
	
	function defineBuiltinParams(postParamConfig)
	{
	    if (postParamConfig.length == 0)
	    {
	        postParamConfig.push({category: 'P', postparam: true, options: []});
	    }
	    
		if (!Options.findOption(postParamConfig[0].options, '*Unpack:'))
		{
			postParamConfig[0].options.unshift({name: '*Unpack:Password', value: '', defvalue: '', select: [], caption: 'Password', sectionId: '_Unpack_', description: 'Unpack-password for encrypted archives.'});
			postParamConfig[0].options.unshift({name: '*Unpack:', value: '', defvalue: 'yes', select: ['yes', 'no'], caption: 'Unpack', sectionId: '_Unpack_', description: 'Unpack rar and 7-zip archives.'});
		}
	}
	
	this.prepareParamRequest = function(postParams)
	{
		var request = [];
		for (var i=0; i < postParams.length; i++)
		{
			var section = postParams[i];
			for (var j=0; j < section.options.length; j++)
			{
				var option = section.options[j];
				if (!option.template && !section.hidden)
				{
					var oldValue = option.value;
					var newValue = Config.getOptionValue(option);
					if (oldValue != newValue && !(oldValue === '' && newValue === option.defvalue))
					{
						var opt = option.name + '=' + newValue;
						request.push(opt);
					}
				}
			}
		}
		return request;
	}
}(jQuery));


/*** DOWNLOAD MULTI EDIT DIALOG ************************************************************/

var DownloadsMultiDialog = (new function($)
{
	'use strict'

	// Controls
	var $DownloadsMultiDialog;
	
	// State
	var multiIDList;
	var notification = null;
	var oldPriority;
	var oldCategory;
	
	this.init = function()
	{
		$DownloadsMultiDialog = $('#DownloadsMultiDialog');
		
		$('#DownloadsMulti_Save').click(saveChanges);

		$DownloadsMultiDialog.on('hidden', function ()
		{
			Refresher.resume();
		});

		if (UISettings.setFocus)
		{
			$DownloadsMultiDialog.on('shown', function ()
			{
				if ($('#DownloadsMulti_Priority').is(":visible"))
				{
					$('#DownloadsMulti_Priority').focus();
				}
			});
		}
	}

	this.showModal = function(nzbIdList, allGroups)
	{
		var groups = [];
		multiIDList = [];

		for (var i=0; i<allGroups.length; i++)
		{
			var gr = allGroups[i];
			if (nzbIdList.indexOf(gr.NZBID) > -1)
			{
				groups.push(gr);
				multiIDList.push(gr.LastID);
			}
		}
		if (groups.length == 0)
		{
			return;
		}

		Refresher.pause();

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

		var size = Util.formatSizeMB(FileSizeMB, FileSizeLo);
		var remaining = Util.formatSizeMB(RemainingSizeMB-PausedSizeMB, RemainingSizeLo-PausedSizeLo);
		var unpausedSize = Util.formatSizeMB(PausedSizeMB, PausedSizeLo);
		var estimated = paused ? '' : (Status.status.DownloadRate > 0 ? Util.formatTimeHMS((RemainingSizeMB-PausedSizeMB)*1024/(Status.status.DownloadRate/1024)) : '');

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
		DownloadsUI.fillPriorityCombo(v);
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
		oldPriority = v.val();
		$('#DownloadsMulti_Priority').removeAttr('disabled');

		// Category
		var v = $('#DownloadsMulti_Category');
		DownloadsUI.fillCategoryCombo(v);
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
		oldCategory = v.val();

		enableAllButtons();
		$('#DownloadsMulti_GeneralTabLink').tab('show');

		notification = null;
		
		$DownloadsMultiDialog.modal({backdrop: 'static'});
	}

	function enableAllButtons()
	{
		$('#DownloadsMulti .modal-footer .btn').removeAttr('disabled');
		$('#DownloadsMulti_Transmit').hide();
	}

	function disableAllButtons()
	{
		$('#DownloadsMulti .modal-footer .btn').attr('disabled', 'disabled');
		setTimeout(function()
		{
			$('#DownloadsMulti_Transmit').show();
		}, 500);
	}

	function saveChanges(e)
	{
		e.preventDefault();
		disableAllButtons();
		savePriority();
	}

	function savePriority()
	{
		var priority = $('#DownloadsMulti_Priority').val();
		(priority !== oldPriority && priority !== '<multiple values>') ?
			RPC.call('editqueue', ['GroupSetPriority', 0, priority, multiIDList], function()
			{
				notification = '#Notif_Downloads_Saved';
				saveCategory();
			})
			: saveCategory();
	}

	function saveCategory()
	{
		var category = $('#DownloadsMulti_Category').val();
		(category !== oldCategory && category !== '<multiple values>') ?
			RPC.call('editqueue', ['GroupSetCategory', 0, category, multiIDList], function()
			{
				notification = '#Notif_Downloads_Saved';
				completed();
			})
			: completed();
	}
	
	function completed()
	{
		$DownloadsMultiDialog.modal('hide');
		Refresher.update();
		if (notification)
		{
			Notification.show(notification);
		}
	}
}(jQuery));


/*** DOWNLOAD MERGE DIALOG ************************************************************/

var DownloadsMergeDialog = (new function($)
{
	'use strict'

	// Controls
	var $DownloadsMergeDialog;
	
	// State
	var mergeEditIDList;

	this.init = function()
	{
		$DownloadsMergeDialog = $('#DownloadsMergeDialog');
		
		$('#DownloadsMerge_Merge').click(merge);

		$DownloadsMergeDialog.on('hidden', function ()
		{
			Refresher.resume();
		});

		if (UISettings.setFocus)
		{
			$DownloadsMergeDialog.on('shown', function ()
			{
				$('#DownloadsMerge_Merge').focus();
			});
		}
	}

	this.showModal = function(nzbIdList, allGroups)
	{
		Refresher.pause();

		mergeEditIDList = [];
		$('#DownloadsMerge_Files').empty();
		for (var i = 0; i < allGroups.length; i++)
		{
			var group = allGroups[i];
			if (nzbIdList.indexOf(group.NZBID) > -1)
			{
				mergeEditIDList.push(group.LastID);
				var html = '<table><tr><td width="18px" valign="top"><i class="icon-file" style="vertical-align:top;margin-top:2px;"></i></td><td>' +
					Util.formatNZBName(group.NZBName) + '</td></tr></table>';
				$('#DownloadsMerge_Files').append(html);
			}
		}

		$DownloadsMergeDialog.modal({backdrop: 'static'});
	}

	function merge()
	{
		RPC.call('editqueue', ['GroupMerge', 0, '', mergeEditIDList], completed);
	}

	function completed()
	{
		$DownloadsMergeDialog.modal('hide');
		Refresher.update();
		Notification.show('#Notif_Downloads_Merged');
	}
}(jQuery));


/*** DOWNLOAD SPLIT DIALOG ************************************************************/

var DownloadsSplitDialog = (new function($)
{
	'use strict'

	// Controls
	var $DownloadsSplitDialog;
	
	// State
	var splitEditIDList;

	this.init = function()
	{
		$DownloadsSplitDialog = $('#DownloadsSplitDialog');
		
		$('#DownloadsSplit_Split').click(split);

		$DownloadsSplitDialog.on('hidden', function ()
		{
			Refresher.resume();
		});

		if (UISettings.setFocus)
		{
			$DownloadsSplitDialog.on('shown', function ()
			{
				$('#DownloadsSplit_Merge').focus();
			});
		}
	}

	this.showModal = function(group, editIDList)
	{
		Refresher.pause();
		splitEditIDList = editIDList;
		var groupName = group.NZBName + ' (' + editIDList[0] + (editIDList.length > 1 ? '-' + editIDList[editIDList.length-1] : '') + ')';
		$('#DownloadsSplit_NZBName').attr('value', groupName);
		$DownloadsSplitDialog.modal({backdrop: 'static'});
	}

	function split()
	{
		var groupName = $('#DownloadsSplit_NZBName').val();
		RPC.call('editqueue', ['FileSplit', 0, groupName, splitEditIDList], completed);
	}

	function completed(result)
	{
		$('#DownloadsEditDialog').modal('hide');
		$DownloadsSplitDialog.modal('hide');
		Refresher.update();
		Notification.show(result ? '#Notif_Downloads_Splitted' : '#Notif_Downloads_SplitError');
	}
}(jQuery));


/*** EDIT HISTORY DIALOG *************************************************************************/

var HistoryEditDialog = (new function()
{
	'use strict'

	// Controls
	var $HistoryEditDialog;
	var $HistoryEdit_ParamData;

	// State
	var curHist;
	var notification = null;
	var postParams = [];
	var lastPage;
	var lastFullscreen;
	var saveParamCompleted;

	this.init = function()
	{
		$HistoryEditDialog = $('#HistoryEditDialog');
		$HistoryEdit_ParamData = $('#HistoryEdit_ParamData');

		$('#HistoryEdit_Save').click(saveChanges);
		$('#HistoryEdit_Delete').click(itemDelete);
		$('#HistoryEdit_Return, #HistoryEdit_ReturnURL').click(itemReturn);
		$('#HistoryEdit_Reprocess').click(itemReprocess);
		$('#HistoryEdit_Param').click(tabClick);
		$('#HistoryEdit_Back').click(backClick);
		
		$HistoryEditDialog.on('hidden', function ()
		{
			$HistoryEdit_ParamData.empty();
			// resume updates
			Refresher.resume();
		});
		
		TabDialog.extend($HistoryEditDialog);
	}

	this.showModal = function(hist)
	{
		Refresher.pause();

		curHist = hist;

		var status;
		if (hist.Kind === 'URL')
		{
			status = HistoryUI.buildStatus(hist.status, '');
		}
		else
		{
			status = '<span class="label label-status ' + 
				(hist.Health === 1000 ? 'label-success' : hist.Health >= hist.CriticalHealth ? 'label-warning' : 'label-important') +
				'">health: ' + Math.floor(hist.Health / 10) + '%</span>';

			if (hist.Deleted && !hist.HealthDeleted)
			{
				status += ' ' + HistoryUI.buildStatus('deleted', '');
			}
			else
			{
				if (hist.HealthDeleted)
				{
					status += ' ' + HistoryUI.buildStatus('aborted', '');
				}
				else
				{
					status += ' ' + HistoryUI.buildStatus(hist.ParStatus, 'Par: ') +
						' ' + (Options.option('Unpack') == 'yes' || hist.UnpackStatus != 'NONE' ? HistoryUI.buildStatus(hist.UnpackStatus, 'Unpack: ') : '')  +
						' ' + (hist.MoveStatus === "FAILURE" ? HistoryUI.buildStatus(hist.MoveStatus, 'Move: ') : '');
				}
				for (var i=0; i<hist.ScriptStatuses.length; i++)
				{
					var scriptStatus = hist.ScriptStatuses[i];
					status += ' ' + HistoryUI.buildStatus(scriptStatus.Status, Options.shortScriptName(scriptStatus.Name) + ': ') + ' ';
				}
			}
		}

		$('#HistoryEdit_Title').text(Util.formatNZBName(hist.Name));
		if (hist.Kind === 'URL')
		{
			$('#HistoryEdit_Title').html($('#HistoryEdit_Title').html() + '&nbsp;' + '<span class="label label-info">URL</span>');
		}

		$('#HistoryEdit_Status').html(status);
		$('#HistoryEdit_Category').text(hist.Category);
		$('#HistoryEdit_Path').text(hist.FinalDir !== '' ? hist.FinalDir : hist.DestDir);

		var size = Util.formatSizeMB(hist.FileSizeMB, hist.FileSizeLo);

		var table = '';
		table += '<tr><td>Total</td><td class="text-right">' + size + '</td></tr>';
		table += '<tr><td>Files (total/parked)</td><td class="text-right">' + hist.FileCount + '/' + hist.RemainingFileCount + '</td></tr>';
		$('#HistoryEdit_Statistics').html(table);

		Util.show($('#HistoryEdit_Return'), hist.RemainingFileCount > 0);
		Util.show($('#HistoryEdit_ReturnURL'), hist.Kind === 'URL');
		Util.show($('#HistoryEdit_PathGroup, #HistoryEdit_StatisticsGroup, #HistoryEdit_Reprocess'), hist.Kind === 'NZB');

		var postParamConfig = ParamTab.createPostParamConfig();
		var postParam = hist.Kind === 'NZB' && postParamConfig[0].options.length > 0;
		Util.show('#HistoryEdit_Param', postParam);
		Util.show('#HistoryEdit_Save', postParam);
		$('#HistoryEdit_Close').toggleClass('btn-primary', !postParam);
		$('#HistoryEdit_Close').text(postParam ? 'Cancel' : 'Close');
		
		if (postParam)
		{
			postParams = ParamTab.buildPostParamTab($HistoryEdit_ParamData, postParamConfig, curHist.Parameters);
		}
		
		EditUI.buildDNZBLinks(curHist.Parameters, 'HistoryEdit_DNZB');

		enableAllButtons();
		
		$('#HistoryEdit_GeneralTab').show();
		$('#HistoryEdit_ParamTab').hide();
		$('#HistoryEdit_Back').hide();
		$('#HistoryEdit_BackSpace').show();
		$HistoryEditDialog.restoreTab();
		
		notification = null;
		
		$HistoryEditDialog.modal({backdrop: 'static'});
	}

	function tabClick(e)
	{
		e.preventDefault();

		$('#HistoryEdit_Back').fadeIn(500);
		$('#HistoryEdit_BackSpace').hide();
		var tab = '#' + $(this).attr('data-tab');
		lastPage = $(tab);
		lastFullscreen = ($(this).attr('data-fullscreen') === 'true') && !UISettings.miniTheme;
		
		$HistoryEditDialog.switchTab($('#HistoryEdit_GeneralTab'), lastPage, 
			e.shiftKey || !UISettings.slideAnimation ? 0 : 500, 
			{fullscreen: lastFullscreen, mini: UISettings.miniTheme});
	}

	function backClick(e)
	{
		e.preventDefault();
		$('#HistoryEdit_Back').fadeOut(500, function()
		{
			$('#HistoryEdit_BackSpace').show();
		});

		$HistoryEditDialog.switchTab(lastPage, $('#HistoryEdit_GeneralTab'), 
			e.shiftKey || !UISettings.slideAnimation ? 0 : 500,
			{fullscreen: lastFullscreen, mini: UISettings.miniTheme, back: true});
	}

	function disableAllButtons()
	{
		$('#HistoryEditDialog .modal-footer .btn').attr('disabled', 'disabled');
		setTimeout(function()
		{
			$('#HistoryEdit_Transmit').show();
		}, 500);
	}

	function enableAllButtons()
	{
		$('#HistoryEditDialog .modal-footer .btn').removeAttr('disabled');
		$('#HistoryEdit_Transmit').hide();
	}

	function itemDelete(e)
	{
		e.preventDefault();
		ConfirmDialog.showModal('HistoryEditDeleteConfirmDialog', doItemDelete);
	}

	function doItemDelete()
	{
		disableAllButtons();
		notification = '#Notif_History_Deleted';
		RPC.call('editqueue', ['HistoryDelete', 0, '', [curHist.ID]], completed);
	}

	function itemReturn(e)
	{
		e.preventDefault();
		disableAllButtons();
		notification = '#Notif_History_Returned';
		RPC.call('editqueue', ['HistoryReturn', 0, '', [curHist.ID]], completed);
	}

	function itemReprocess(e)
	{
		e.preventDefault();
		disableAllButtons();
		saveParam(function()
			{
				notification = '#Notif_History_Reproces';
				RPC.call('editqueue', ['HistoryProcess', 0, '', [curHist.ID]], completed);
			});
	}

	function completed()
	{
		$HistoryEditDialog.modal('hide');
		Refresher.update();
		if (notification)
		{
			Notification.show(notification);
			notification = null;
		}
	}
	
	function saveChanges(e)
	{
		e.preventDefault();
		disableAllButtons();
		notification = null;
		saveParam(completed);
	}
	
	/*** TAB: POST-PROCESSING PARAMETERS **************************************************/

	function saveParam(_saveParamCompleted)
	{
		saveParamCompleted = _saveParamCompleted;
		var paramList = ParamTab.prepareParamRequest(postParams);
		saveNextParam(paramList);
	}

	function saveNextParam(paramList)
	{
		if (paramList.length > 0)
		{
			RPC.call('editqueue', ['HistorySetParameter', 0, paramList[0], [curHist.ID]], function()
			{
				notification = '#Notif_History_Saved';
				paramList.shift();
				saveNextParam(paramList);
			})
		}
		else
		{
			saveParamCompleted();
		}
	}
	
}(jQuery));
