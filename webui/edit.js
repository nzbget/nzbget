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
	var $DownloadsFileTable;
	var $DownloadsEdit_ParamData;
	var $ServStatsTable;

	// State
	var curGroup;
	var notification = null;
	var postParams = [];
	var lastPage;
	var lastFullscreen;
	var logFilled;
	var files;
	var refreshTimer = 0;
	var showing;
	var oldCategory;

	this.init = function()
	{
		$DownloadsEditDialog = $('#DownloadsEditDialog');
		$DownloadsEdit_ParamData = $('#DownloadsEdit_ParamData');

		$('#DownloadsEdit_Save').click(saveChanges);
		$('#DownloadsEdit_Actions').click(itemActions);
		$('#DownloadsEdit_Param, #DownloadsEdit_Log, #DownloadsEdit_File, #DownloadsEdit_Dupe').click(tabClick);
		$('#DownloadsEdit_Back').click(backClick);
		$('#DownloadsEdit_Category').change(categoryChange);

		LogTab.init('Downloads');

		$DownloadsFileTable = $('#DownloadsEdit_FileTable');
		$DownloadsFileTable.fasttable(
			{
				filterInput: '#DownloadsEdit_FileTable_filter',
				pagerContainer: '#DownloadsEdit_FileTable_pager',
				rowSelect: UISettings.rowSelect,
				pageSize: 10000,
				renderCellCallback: fileTableRenderCellCallback
			});

		$ServStatsTable = $('#DownloadsEdit_ServStatsTable');
		$ServStatsTable.fasttable(
			{
				filterInput: '#DownloadsEdit_ServStatsTable_filter',
				pagerContainer: '#DownloadsEdit_ServStatsTable_pager',
				pageSize: 100,
				maxPages: 3,
				renderCellCallback: EditUI.servStatsTableRenderCellCallback
			});

		$DownloadsEditDialog.on('hidden', function()
		{
			// cleanup
			LogTab.reset('Downloads');
			$DownloadsFileTable.fasttable('update', []);
			$DownloadsEdit_ParamData.empty();
			clearTimeout(refreshTimer);
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

	this.showModal = function(nzbid, allGroups, area)
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

		var size = Util.formatSizeMB(group.FileSizeMB, group.FileSizeLo);
		var remaining = Util.formatSizeMB(group.RemainingSizeMB-group.PausedSizeMB, group.RemainingSizeLo-group.PausedSizeLo);
		var pausedSize = Util.formatSizeMB(group.PausedSizeMB, group.PausedSizeLo);
		var completion = group.SuccessArticles + group.FailedArticles > 0 ? Util.round0(group.SuccessArticles * 100.0 / (group.SuccessArticles +  group.FailedArticles)) + '%' : '--';
		if (group.FailedArticles > 0 && completion === '100%')
		{
			completion = '99.9%';
		}

		var table = '';
		//table += '<tr><td>Age</td><td class="text-right">' + age + '</td></tr>';
		table += '<tr><td>Total</td><td class="text-right">' + size + '</td></tr>';
		table += '<tr><td>Paused</td><td class="text-right">' + pausedSize + '</td></tr>';
		table += '<tr><td>Unpaused</td><td class="text-right">' + remaining + '</td></tr>';
		//table += '<tr><td>Size (total/remaining/paused)</td><td class="text-right">4.10 / 4.10 / 0.00 GB</td></tr>';
		//table += '<tr><td>Active downloads</td><td class="text-right">' + group.ActiveDownloads + '</td></tr>';
		//table += '<tr><td>Estimated time</td><td class="text-right">' + estimated + '</td></tr>';
		table += '<tr><td>Health (critical/current)</td><td class="text-right">' +
			Math.floor(group.CriticalHealth / 10) + '% / ' + Math.floor(group.Health / 10) + '%</td></tr>';
		table += '<tr><td>Files (total/remaining/pars)</td><td class="text-right">' + group.FileCount + ' / ' +
			group.RemainingFileCount + ' / ' + group.RemainingParCount + '</td></tr>';
		table += '<tr><td>' +
			(group.ServerStats.length > 0 ? '<a href="#" id="DownloadsEdit_ServStats" data-tab="DownloadsEdit_ServStatsTab" title="Per-server statistics">' : '') +
			'Articles (total/completion)' +
			(group.ServerStats.length > 0 ? ' <i class="icon-forward" style="opacity:0.6;"></i></a>' : '') +
			'</td><td class="text-right">' + group.TotalArticles + ' / ' + completion + '</td></tr>';
		$('#DownloadsEdit_Statistics').html(table);

		$('#DownloadsEdit_ServStats').click(tabClick);
		EditUI.fillServStats($ServStatsTable, group);
		$ServStatsTable.fasttable('setCurPage', 1);

		$('#DownloadsEdit_Title').html(Util.formatNZBName(group.NZBName) +
		    (group.Kind === 'URL' ? '&nbsp;<span class="label label-info">URL</span>' : ''));

		$('#DownloadsEdit_NZBName').attr('value', group.NZBName);
		$('#DownloadsEdit_NZBName').attr('readonly', group.postprocess);
		$('#DownloadsEdit_URL').attr('value', group.URL);

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
		v = $('#DownloadsEdit_Category');
		DownloadsUI.fillCategoryCombo(v);
		v.val(group.Category);
		if (v.val() != group.Category)
		{
			v.append($('<option selected="selected"></option>').text(group.Category));
		}

		// duplicate settings
		$('#DownloadsEdit_DupeKey').val(group.DupeKey);
		$('#DownloadsEdit_DupeScore').val(group.DupeScore);
		$('#DownloadsEdit_DupeMode').val(group.DupeMode);

		$DownloadsFileTable.fasttable('update', []);

		var postParamConfig = ParamTab.createPostParamConfig();

		Util.show('#DownloadsEdit_NZBNameReadonly', group.postprocess);
		Util.show('#DownloadsEdit_Save', !group.postprocess);
		Util.show('#DownloadsEdit_StatisticsGroup', group.Kind === 'NZB');
		Util.show('#DownloadsEdit_File', group.Kind === 'NZB');
		Util.show('#DownloadsEdit_URLGroup', group.Kind === 'URL');
		$('#DownloadsEdit_CategoryGroup').toggleClass('control-group-last', group.Kind === 'URL');
		var dupeCheck = Options.option('DupeCheck') === 'yes';
		Util.show('#DownloadsEdit_Dupe', dupeCheck);
		var postParam = postParamConfig[0].options.length > 0 && group.Kind === 'NZB';
		var postLog = group.MessageCount > 0;
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
		}

		if (postParam)
		{
			postParams = ParamTab.buildPostParamTab($DownloadsEdit_ParamData, postParamConfig, curGroup.Parameters);
		}

		enableAllButtons();

		$('#DownloadsEdit_GeneralTab').show();
		$('#DownloadsEdit_ParamTab').hide();
		$('#DownloadsEdit_ServStatsTab').hide();
		$('#DownloadsEdit_LogTab').hide();
		$('#DownloadsEdit_FileTab').hide();
		$('#DownloadsEdit_DupeTab').hide();
		$('#DownloadsEdit_Back').hide();
		$('#DownloadsEdit_BackSpace').show();
		$DownloadsEditDialog.restoreTab();

		$('#DownloadsEdit_FileTable_filter').val('');
		$DownloadsFileTable.fasttable('setCurPage', 1);
		$DownloadsFileTable.fasttable('applyFilter', '');

		LogTab.reset('Downloads');

		files = null;
		logFilled = false;
		notification = null;
		oldCategory = curGroup.Category;

		if (area === 'backup')
		{
			showing = true;
			$('#DownloadsEdit_ServStats').trigger('click');
		}
		showing = false;

		$DownloadsEditDialog.modal({backdrop: 'static'});
	}

	function completed()
	{
		$DownloadsEditDialog.modal('hide');
		Refresher.update();
		if (notification)
		{
			PopupNotification.show(notification);
			notification = null;
		}
	}

	function tabClick(e)
	{
		e.preventDefault();

		$('#DownloadsEdit_Back').fadeIn(showing ? 0 : 500);
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
			e.shiftKey || !UISettings.slideAnimation || showing ? 0 : 500,
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

		if (tab === '#DownloadsEdit_LogTab' && !logFilled && (curGroup.postprocess || curGroup.MessageCount > 0))
		{
			LogTab.fill('Downloads', curGroup);
			logFilled = true;
		}

		if (tab === '#DownloadsEdit_FileTab' && files === null)
		{
			fillFiles();
		}

		if (tab === '#DownloadsEdit_ServStatsTab')
		{
			scheduleRefresh();
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

		clearTimeout(refreshTimer);
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
			RPC.call('editqueue', ['GroupSetName', name, [curGroup.NZBID]], function()
			{
				notification = '#Notif_Downloads_Saved';
				savePriority();
			})
			:savePriority();
	}

	function savePriority()
	{
		var priority = parseInt($('#DownloadsEdit_Priority').val());
		priority !== curGroup.MaxPriority ?
			RPC.call('editqueue', ['GroupSetPriority', '' + priority, [curGroup.NZBID]], function()
			{
				notification = '#Notif_Downloads_Saved';
				saveCategory();
			})
			: saveCategory();
	}

	function saveCategory()
	{
		var category = $('#DownloadsEdit_Category').val();
		category !== curGroup.Category ?
			RPC.call('editqueue', ['GroupSetCategory', category, [curGroup.NZBID]], function()
			{
				notification = '#Notif_Downloads_Saved';
				saveDupeKey();
			})
			: saveDupeKey();
	}

	function itemActions(e)
	{
		e.preventDefault();
		e.stopPropagation();
		var elem = $('#DownloadsEdit_Actions').parent();

		DownloadsActionsMenu.showPopupMenu(curGroup, 'top',
			{ left: elem.offset().left, top: elem.offset().top - 1,
				width: elem.width(), height: elem.height() + 2 },
			function(_notification)
			{
				disableAllButtons();
				notification = _notification;
			},
			completed);
	}

	function categoryChange()
	{
		var category = $('#DownloadsEdit_Category').val();
		ParamTab.reassignParams(postParams, oldCategory, category);
		oldCategory = category;
	}

	/*** TAB: POST-PROCESSING PARAMETERS **************************************************/

	function saveParam()
	{
		if (curGroup.Kind === 'URL')
		{
			completed();
			return;
		}

		var paramList = ParamTab.prepareParamRequest(postParams);
		saveNextParam(paramList);
	}

	function saveNextParam(paramList)
	{
		if (paramList.length > 0)
		{
			RPC.call('editqueue', ['GroupSetParameter', paramList[0], [curGroup.NZBID]], function()
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

	/*** TAB: DUPLICATE SETTINGS **************************************************/

	function saveDupeKey()
	{
		var value = $('#DownloadsEdit_DupeKey').val();
		value !== curGroup.DupeKey ?
			RPC.call('editqueue', ['GroupSetDupeKey', value, [curGroup.NZBID]], function()
			{
				notification = '#Notif_Downloads_Saved';
				saveDupeScore();
			})
			:saveDupeScore();
	}

	function saveDupeScore()
	{
		var value = $('#DownloadsEdit_DupeScore').val();
		value != curGroup.DupeScore ?
			RPC.call('editqueue', ['GroupSetDupeScore', value, [curGroup.NZBID]], function()
			{
				notification = '#Notif_Downloads_Saved';
				saveDupeMode();
			})
			:saveDupeMode();
	}

	function saveDupeMode()
	{
		var value = $('#DownloadsEdit_DupeMode').val();
		value !== curGroup.DupeMode ?
			RPC.call('editqueue', ['GroupSetDupeMode', value, [curGroup.NZBID]], function()
			{
				notification = '#Notif_Downloads_Saved';
				saveParam();
			})
			:saveParam();
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

			var FileSizeMB = (file.FileSizeHi * 4096) + (file.FileSizeLo / 1024 / 1024);
			var RemainingSizeMB = (file.RemainingSizeHi * 4096) + (file.RemainingSizeLo / 1024 / 1024);
			var age = Util.formatAge(file.PostTime + UISettings.timeZoneCorrection*60*60);
			var size = Util.formatSizeMB(FileSizeMB, file.FileSizeLo);
			if (FileSizeMB !== RemainingSizeMB || file.FileSizeLo !== file.RemainingSizeLo)
			{
				size = '(' + Util.round0((file.FileSizeHi > 0 ?
					RemainingSizeMB / FileSizeMB :
					file.RemainingSizeLo / file.FileSizeLo) * 100) + '%) ' + size;
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

			var name = Util.textToHtml(file.Filename);
			var fields;

			if (!UISettings.miniTheme)
			{
				var info = name;
				fields = ['<div class="check img-check"></div>', status, info, age, size];
			}
			else
			{
				var info = '<div class="check img-check"></div><span class="row-title">' + name + '</span>' +
					' ' + (file.status === 'queued' ? '' : status);
				fields = [info];
			}

			var item =
			{
				id: file.ID,
				file: file,
				fields: fields,
				data: { status: file.status, name: file.Filename, age: age, size: size, _search: true }
			};

			data.push(item);
		}

		$DownloadsFileTable.fasttable('update', data);
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
		var checkedCount = $DownloadsFileTable.fasttable('checkedCount');
		if (checkedCount === 0)
		{
			PopupNotification.show('#Notif_Edit_Select');
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

			if (checkedRows[file.ID])
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
						if (file.ActiveDownloads > 0 || file.Progress > 0)
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
				PopupNotification.show('#Notif_Downloads_SplitNotPossible');
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
			RPC.call('editqueue', [command, '', IDs], function()
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
			RPC.call('editqueue', ['FileReorder', '', IDs], function()
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

	/*** TAB: PER-SERVER STATUSTICS *****************************************************************/

	function scheduleRefresh()
	{
		refreshTimer = setTimeout(updateServStats, UISettings.refreshInterval * 1000);
	}

	function updateServStats()
	{
		RPC.call('listgroups', [], groups_loaded);
	}

	function groups_loaded(groups)
	{
		for (var i=0, il=groups.length; i < il; i++)
		{
			var group = groups[i];
			if (group.NZBID === curGroup.NZBID)
			{
				curGroup.ServerStats = group.ServerStats;
				EditUI.fillServStats($ServStatsTable, group);
				scheduleRefresh();
				break;
			}
		}
	}
}(jQuery));


/*** COMMON FUNCTIONS FOR EDIT DIALOGS ************************************************************/

var EditUI = (new function($)
{
	'use strict'

	/*** TAB: SERVER STATISTICS **************************************************/

	this.fillServStats = function(table, editItem)
	{
		var data = [];
		for (var i=0; i < Status.status.NewsServers.length; i++)
		{
			var server = Status.status.NewsServers[i];
			var name = Options.option('Server' + server.ID + '.Name');
			if (name === null || name === '')
			{
				var host = Options.option('Server' + server.ID + '.Host');
				var port = Options.option('Server' + server.ID + '.Port');
				name = (host === null ? '' : host) + ':' + (port === null ? '119' : port);
			}

			var articles = '--';
			var artquota = '--';
			var success = '--';
			var failures = '--';
			for (var j=0; j < editItem.ServerStats.length; j++)
			{
				var stat = editItem.ServerStats[j];
				if (stat.ServerID === server.ID && stat.SuccessArticles + stat.FailedArticles > 0)
				{
					articles = stat.SuccessArticles + stat.FailedArticles;
					artquota = Util.round0(articles * 100.0 / (editItem.SuccessArticles + editItem.FailedArticles)) + '%';
					success = Util.round0(stat.SuccessArticles * 100.0 / articles) + '%';
					failures = Util.round0(stat.FailedArticles * 100.0 / articles) + '%';
					if (stat.FailedArticles > 0 && failures === '0%')
					{
						success = '99.9%';
						failures = '0.1%';
					}
					success = '<span title="' + stat.SuccessArticles + ' article' + (stat.SuccessArticles === 1 ? '' : 's') + '">' + success + '</span>';
					failures = '<span title="' + stat.FailedArticles + ' article' + (stat.FailedArticles === 1 ? '' : 's') + '">' + failures + '</span>';
					break;
				}
			}

			var fields = [server.ID + '. ' + name, articles, artquota, success, failures];
			var item =
			{
				id: server.ID,
				fields: fields,
			};
			data.push(item);
		}
		table.fasttable('update', data);
	}

	this.servStatsTableRenderCellCallback = function (cell, index, item)
	{
		if (index > 0)
		{
			cell.className = 'text-right';
		}
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
					if (oldValue != newValue && !((oldValue === null || oldValue === '') && newValue === option.defvalue))
					{
						var opt = option.name + '=' + newValue;
						request.push(opt);
					}
				}
			}
		}
		return request;
	}

	function buildCategoryScriptList(category)
	{
		var scriptList = [];

		for (var i=0; i < Options.categories.length; i++)
		{
			if (category === Options.categories[i])
			{
				scriptList = Util.parseCommaList(Options.option('Category' + (i + 1) + '.Extensions'));
				if (scriptList.length === 0)
				{
					scriptList = Util.parseCommaList(Options.option('Extensions'));
				}
				if (Options.option('Category' + (i + 1) + '.Unpack') === 'yes')
				{
					scriptList.push('*Unpack');
				}
				return scriptList;
			}
		}

		// empty category or category not found
		scriptList = Util.parseCommaList(Options.option('Extensions'));
		if (Options.option('Unpack') === 'yes')
		{
			scriptList.push('*Unpack');
		}
		return scriptList;
	}

	this.reassignParams = function(postParams, oldCategory, newCategory)
	{
		var oldScriptList = buildCategoryScriptList(oldCategory);
		var newScriptList = buildCategoryScriptList(newCategory);

		for (var i=0; i < postParams.length; i++)
		{
			var section = postParams[i];
			for (var j=0; j < section.options.length; j++)
			{
				var option = section.options[j];
				if (!option.template && !section.hidden && option.name.substr(option.name.length - 1, 1) === ':')
				{
					var scriptName = option.name.substr(0, option.name.length-1);
					if (oldScriptList.indexOf(scriptName) > -1 && newScriptList.indexOf(scriptName) === -1)
					{
						Config.setOptionValue(option, 'no');
					}
					else if (oldScriptList.indexOf(scriptName) === -1 && newScriptList.indexOf(scriptName) > -1)
					{
						Config.setOptionValue(option, 'yes');
					}
				}
			}
		}
	}

}(jQuery));


/*** LOG TAB FOR EDIT DIALOGS ************************************************************/

var LogTab = (new function($)
{
	'use strict'

	var curLog;
	var curItem;

	this.init = function(name)
	{
		var recordsPerPage = UISettings.read('ItemLogRecordsPerPage', 10);
		$('#' + name + 'LogRecordsPerPage').val(recordsPerPage);

		var $LogTable = $('#' + name + 'Edit_LogTable');
		$LogTable.fasttable(
			{
				filterInput: '#' + name + 'Edit_LogTable_filter',
				pagerContainer: '#' + name + 'Edit_LogTable_pager',
				pageSize: recordsPerPage,
				maxPages: 3,
				renderCellCallback: logTableRenderCellCallback
			});
	}

	this.reset = function(name)
	{
		var $LogTable = $('#' + name + 'Edit_LogTable');
		$LogTable.fasttable('update', []);
		$LogTable.fasttable('setCurPage', 1);
		$LogTable.fasttable('applyFilter', '');

		$('#' + name + 'Edit_LogTable_filter').val('');
	}

	this.fill = function(name, item)
	{
		curItem = item;

		function logLoaded(log)
		{
		curLog = log;

			$('#' + name + 'EditDialog .loading-block').hide();
			var $LogTable = $('#' + name + 'Edit_LogTable');
			var data = [];

			for (var i=0; i < log.length; i++)
			{
				var message = log[i];

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
					data: { kind: message.Kind, time: time, text: message.Text, _search: true }
				};

				data.unshift(item);
			}

			$LogTable.fasttable('update', data);
		}

		var recordsPerPage = UISettings.read('ItemLogRecordsPerPage', 10);
		$('#' + name + 'LogRecordsPerPage').val(recordsPerPage);

		$('#' + name + 'EditDialog .loading-block').show();
		RPC.call('loadlog', [item.NZBID, 0, 10000], logLoaded);
	}

	function logTableRenderCellCallback(cell, index, item)
	{
		if (index === 0)
		{
			cell.width = '65px';
		}
	}

	this.recordsPerPageChange = function(name)
	{
		var val = $('#' + name + 'LogRecordsPerPage').val();
		UISettings.write('ItemLogRecordsPerPage', val);
		var $LogTable = $('#' + name + 'Edit_LogTable');
		$LogTable.fasttable('setPageSize', val);
	}

	this.export = function()
	{
		var filename = curItem.NZBName + '.log';
		var logstr = '';

		for (var i=0; i < curLog.length; i++)
		{
			var message = curLog[i];
			var time = Util.formatDateTime(message.Time + UISettings.timeZoneCorrection*60*60);
			logstr += time + '\t' + message.Kind + '\t' + message.Text + '\n';
		}

		if (!Util.saveToLocalFile(logstr, "text/plain;charset=utf-8", filename))
		{
			var queueDir = Options.option('QueueDir');
			var pathSeparator = queueDir.indexOf('\\') > -1 ? '\\' : '/';
			alert('Unfortunately your browser doesn\'t support access to local file system.\n\n' +
				'The log of this nzb can be found in file "' +
				queueDir + pathSeparator + 'n' + curItem.NZBID + '.log"');
		}
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
				multiIDList.push(gr.NZBID);
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
			paused = paused && group.Status === 'PAUSED';
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
			RPC.call('editqueue', ['GroupSetPriority', priority, multiIDList], function()
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
			RPC.call('editqueue', ['GroupApplyCategory', category, multiIDList], function()
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
			PopupNotification.show(notification);
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
				mergeEditIDList.push(group.NZBID);
				var html = '<table><tr><td width="18px" valign="top"><i class="icon-file" style="vertical-align:top;margin-top:2px;"></i></td><td>' +
					Util.formatNZBName(group.NZBName) + '</td></tr></table>';
				$('#DownloadsMerge_Files').append(html);
			}
		}

		$DownloadsMergeDialog.modal({backdrop: 'static'});
	}

	function merge()
	{
		RPC.call('editqueue', ['GroupMerge', '', mergeEditIDList], completed);
	}

	function completed()
	{
		$DownloadsMergeDialog.modal('hide');
		Refresher.update();
		PopupNotification.show('#Notif_Downloads_Merged');
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
		RPC.call('editqueue', ['FileSplit', groupName, splitEditIDList], completed);
	}

	function completed(result)
	{
		$('#DownloadsEditDialog').modal('hide');
		$DownloadsSplitDialog.modal('hide');
		Refresher.update();
		PopupNotification.show(result ? '#Notif_Downloads_Splitted' : '#Notif_Downloads_SplitError');
	}
}(jQuery));


/*** EDIT HISTORY DIALOG *************************************************************************/

var HistoryEditDialog = (new function($)
{
	'use strict'

	// Controls
	var $HistoryEditDialog;
	var $HistoryEdit_ParamData;
	var $ServStatsTable;

	// State
	var curHist;
	var notification = null;
	var postParams = [];
	var lastPage;
	var lastFullscreen;
	var saveCompleted;
	var logFilled;
	var showing;

	this.init = function()
	{
		$HistoryEditDialog = $('#HistoryEditDialog');
		$HistoryEdit_ParamData = $('#HistoryEdit_ParamData');

		$('#HistoryEdit_Save').click(saveChanges);
		$('#HistoryEdit_Actions').click(itemActions);
		$('#HistoryEdit_Param, #HistoryEdit_Dupe, #HistoryEdit_Log').click(tabClick);
		$('#HistoryEdit_Back').click(backClick);

		LogTab.init('History');

		$ServStatsTable = $('#HistoryEdit_ServStatsTable');
		$ServStatsTable.fasttable(
			{
				filterInput: '#HistoryEdit_ServStatsTable_filter',
				pagerContainer: '#HistoryEdit_ServStatsTable_pager',
				pageSize: 100,
				maxPages: 3,
				renderCellCallback: EditUI.servStatsTableRenderCellCallback
			});

		$HistoryEditDialog.on('hidden', function ()
		{
			$HistoryEdit_ParamData.empty();
			LogTab.reset('History');
			// resume updates
			Refresher.resume();
		});

		TabDialog.extend($HistoryEditDialog);
	}

	this.showModal = function(hist, area)
	{
		Refresher.pause();

		curHist = hist;

		var status = '';
		if (hist.Kind === 'NZB')
		{
			if (hist.DeleteStatus === '' || hist.DeleteStatus === 'HEALTH')
			{
				status = '<span class="label label-status ' +
					(hist.Health === 1000 ? 'label-success' : hist.Health >= hist.CriticalHealth ? 'label-warning' : 'label-important') +
					'">health: ' + Math.floor(hist.Health / 10) + '%</span>';
			}

			if (hist.MarkStatus !== 'NONE')
			{
				status += ' ' + buildStatus(hist.MarkStatus, 'Mark: ');
			}

			else if (hist.DeleteStatus === 'NONE')
			{
				var exParStatus = hist.ExParStatus === 'RECIPIENT' ? ' ' + '<span title="Repaired using ' + hist.ExtraParBlocks + ' par-block' +
						(hist.ExtraParBlocks > 1 ? 's' : '') + ' from other duplicate(s)">' + buildStatus(hist.ExParStatus, 'ExPar: ') + '</span>' :
					hist.ExParStatus === 'DONOR' ? ' ' + '<span title="Donated ' + -hist.ExtraParBlocks + ' par-block' +
						(-hist.ExtraParBlocks > 1 ? 's' : '') + ' to repair other duplicate(s)">' + buildStatus(hist.ExParStatus, 'ExPar: ') + '</span>' : '';
				status += ' ' + buildStatus(hist.ParStatus, 'Par: ') + exParStatus +
					' ' + (Options.option('Unpack') == 'yes' || hist.UnpackStatus != 'NONE' ? buildStatus(hist.UnpackStatus, 'Unpack: ') : '')  +
					' ' + (hist.MoveStatus === "FAILURE" ? buildStatus(hist.MoveStatus, 'Move: ') : '');
			}
			else
			{
				status += ' ' + buildStatus('DELETED-' + hist.DeleteStatus, 'Delete: ');
			}

			for (var i=0; i<hist.ScriptStatuses.length; i++)
			{
				var scriptStatus = hist.ScriptStatuses[i];
				status += ' ' + buildStatus(scriptStatus.Status, Options.shortScriptName(scriptStatus.Name) + ': ') + ' ';
			}
		}
		else if (hist.Kind === 'URL')
		{
			if (hist.DeleteStatus !== 'NONE')
			{
				status = buildStatus('DELETED-' + hist.DeleteStatus, 'Delete: ');
			}
			else if (hist.UrlStatus == 'SCAN_SKIPPED')
			{
				status = buildStatus('SUCCESS', 'Fetch: ') + ' ' +
					buildStatus('SCAN_SKIPPED', 'Scan: ');
			}
			else if (hist.UrlStatus == 'SCAN_FAILURE')
			{
				status = buildStatus('SUCCESS', 'Fetch: ') + ' ' +
					buildStatus('FAILURE', 'Scan: ');
			}
			else
			{
				status = buildStatus(hist.UrlStatus, 'Fetch: ');
			}
		}
		else if (hist.Kind === 'DUP')
		{
			status = buildStatus(hist.DupStatus, '');
		}
		$('#HistoryEdit_Status').html(status);

		$('#HistoryEdit_Title').text(Util.formatNZBName(hist.Name));
		if (hist.Kind !== 'NZB')
		{
			$('#HistoryEdit_Title').html($('#HistoryEdit_Title').html() + '&nbsp;' + '<span class="label label-info">' +
				(hist.Kind === 'DUP' ? 'hidden' : hist.Kind) + '</span>');
		}

		$('#HistoryEdit_NZBName').val(hist.Name);

		if (hist.Kind !== 'DUP')
		{
			// Category
			var v = $('#HistoryEdit_Category');
			DownloadsUI.fillCategoryCombo(v);
			v.val(hist.Category);
			if (v.val() != hist.Category)
			{
				v.append($('<option selected="selected"></option>').text(hist.Category));
			}
		}

		if (hist.Kind === 'NZB')
		{
			$('#HistoryEdit_Path').val(hist.FinalDir !== '' ? hist.FinalDir : hist.DestDir);

			var size = Util.formatSizeMB(hist.FileSizeMB, hist.FileSizeLo);
			var completion = hist.SuccessArticles + hist.FailedArticles > 0 ? Util.round0(hist.SuccessArticles * 100.0 / (hist.SuccessArticles +  hist.FailedArticles)) + '%' : '--';
			if (hist.FailedArticles > 0 && completion === '100%')
			{
				completion = '99.9%';
			}

			var table = '';
			table += '<tr><td><a href="#" id="HistoryEdit_TimeStats" data-tab="HistoryEdit_TimeStatsTab" title="Size and time statistics">Total '+
				'<i class="icon-forward" style="opacity:0.6;"></i></a>' +
				'</td><td class="text-center">' + size + '</td></tr>';
			table += '<tr><td>Files (total/remaining)</td><td class="text-center">' + hist.FileCount + ' / ' + hist.RemainingFileCount + '</td></tr>';
			table += '<tr><td>' +
				(hist.ServerStats.length > 0 ? '<a href="#" id="HistoryEdit_ServStats" data-tab="HistoryEdit_ServStatsTab" title="Per-server statistics">' : '') +
				'Articles (total/completion)' +
				(hist.ServerStats.length > 0 ? ' <i class="icon-forward" style="opacity:0.6;"></i></a>' : '') +
				'</td><td class="text-center">' + hist.TotalArticles + ' / ' + completion + '</td></tr>';
			$('#HistoryEdit_Statistics').html(table);

			$('#HistoryEdit_ServStats').click(tabClick);
			EditUI.fillServStats($ServStatsTable, hist);
			$ServStatsTable.fasttable('setCurPage', 1);

			$('#HistoryEdit_TimeStats').click(tabClick);
			fillTimeStats();
		}

		$('#HistoryEdit_DupeKey').val(hist.DupeKey);
		$('#HistoryEdit_DupeScore').val(hist.DupeScore);
		$('#HistoryEdit_DupeMode').val(hist.DupeMode);
		$('#HistoryEdit_DupeBackup').prop('checked', hist.DeleteStatus === 'DUPE');
		$('#HistoryEdit_DupeBackup').prop('disabled', !(hist.DeleteStatus === 'DUPE' || hist.DeleteStatus === 'MANUAL'));
		Util.show($('#HistoryEdit_DupeBackup').closest('.control-group'), hist.Kind === 'NZB');
		$('#HistoryEdit_DupeMode').closest('.control-group').toggleClass('last-group', hist.Kind !== 'NZB');

		Util.show('#HistoryEdit_PathGroup, #HistoryEdit_StatisticsGroup', hist.Kind === 'NZB');
		Util.show('#HistoryEdit_CategoryGroup', hist.Kind !== 'DUP');
		Util.show('#HistoryEdit_DupGroup', hist.Kind === 'DUP');
		var dupeCheck = Options.option('DupeCheck') === 'yes';
		Util.show('#HistoryEdit_Dupe', dupeCheck);
		$('#HistoryEdit_CategoryGroup').toggleClass('control-group-last', hist.Kind === 'URL');

		Util.show('#HistoryEdit_URLGroup', hist.Kind === 'URL');
		$('#HistoryEdit_URL').attr('value', hist.URL);

		var postParamConfig = ParamTab.createPostParamConfig();
		var postParam = hist.Kind === 'NZB' && postParamConfig[0].options.length > 0;
		Util.show('#HistoryEdit_Param', postParam);

		if (postParam)
		{
			postParams = ParamTab.buildPostParamTab($HistoryEdit_ParamData, postParamConfig, curHist.Parameters);
		}

		var postLog = hist.MessageCount > 0;
		Util.show('#HistoryEdit_Log', postLog);

		enableAllButtons();

		$('#HistoryEdit_GeneralTab').show();
		$('#HistoryEdit_ParamTab').hide();
		$('#HistoryEdit_ServStatsTab').hide();
		$('#HistoryEdit_TimeStatsTab').hide();
		$('#HistoryEdit_DupeTab').hide();
		$('#HistoryEdit_LogTab').hide();
		$('#HistoryEdit_Back').hide();
		$('#HistoryEdit_BackSpace').show();
		$HistoryEditDialog.restoreTab();

		LogTab.reset('History');

		logFilled = false;
		notification = null;

		if (area === 'backup')
		{
			showing = true;
			$('#HistoryEdit_ServStats').trigger('click');
		}
		showing = false;

		$HistoryEditDialog.modal({backdrop: 'static'});
	}

	function buildStatus(status, prefix)
	{
		switch (status)
		{
			case 'SUCCESS':
			case 'GOOD':
			case 'RECIPIENT':
			case 'DONOR':
				return '<span class="label label-status label-success">' + prefix + status + '</span>';
			case 'FAILURE':
				return '<span class="label label-status label-important">' + prefix + 'failure</span>';
			case 'BAD':
				return '<span class="label label-status label-important">' + prefix + status + '</span>';
			case 'REPAIR_POSSIBLE':
				return '<span class="label label-status label-warning">' + prefix + 'repairable</span>';
			case 'MANUAL': // PAR-MANUAL
			case 'SPACE':
			case 'PASSWORD':
				return '<span class="label label-status label-warning">' + prefix + status + '</span>';
			case 'DELETED-DUPE':
			case 'DELETED-MANUAL':
			case 'DELETED-COPY':
			case 'DELETED-GOOD':
			case 'DELETED-SUCCESS':
				return '<span class="label label-status">' + prefix + status.substr(8).toLowerCase() + '</span>';
			case 'DELETED-HEALTH':
				return '<span class="label label-status label-important">' + prefix + 'health</span>';
			case 'DELETED-BAD':
				return '<span class="label label-status label-important">' + prefix + 'bad</span>';
			case 'DELETED-SCAN':
				return '<span class="label label-status label-important">' + prefix + 'scan</span>';
			case 'SCAN_SKIPPED':
				return '<span class="label label-status label-warning"">' + prefix + 'skipped</span>';
			case 'NONE':
				return '<span class="label label-status">' + prefix + 'none</span>';
			default:
				return '<span class="label label-status">' + prefix + status + '</span>';
		}
	}

	function fillTimeStats()
	{
		var hist = curHist;
		var downloaded = Util.formatSizeMB(hist.DownloadedSizeMB, hist.DownloadedSizeLo);
		var speed = hist.DownloadTimeSec > 0 ? Util.formatSpeed((hist.DownloadedSizeMB > 1024 ? hist.DownloadedSizeMB * 1024.0 * 1024.0 : hist.DownloadedSizeLo) / hist.DownloadTimeSec) : '--';
		var table = '';
		table += '<tr><td>Downloaded size</td><td class="text-center">' + downloaded + '</td></tr>';
		table += '<tr><td>Download speed</td><td class="text-center">' + speed + '</td></tr>';
		table += '<tr><td>Total time</td><td class="text-center">' + Util.formatTimeHMS(hist.DownloadTimeSec + hist.PostTotalTimeSec) + '</td></tr>';
		table += '<tr><td>Download time</td><td class="text-center">' + Util.formatTimeHMS(hist.DownloadTimeSec) + '</td></tr>';
		table += '<tr><td>Verification time </td><td class="text-center">' + Util.formatTimeHMS(hist.ParTimeSec - hist.RepairTimeSec) + '</td></tr>';
		table += '<tr><td>Repair time</td><td class="text-center">' + Util.formatTimeHMS(hist.RepairTimeSec) + '</td></tr>';
		table += '<tr><td>Unpack time</td><td class="text-center">' + Util.formatTimeHMS(hist.UnpackTimeSec) + '</td></tr>';
		table += hist.ExtraParBlocks > 0 ? '<tr><td>Received extra par-blocks</td><td class="text-center">' + hist.ExtraParBlocks + '</td></tr>' :
			hist.ExtraParBlocks < 0 ? '<tr><td>Donated par-blocks</td><td class="text-center">' + - hist.ExtraParBlocks + '</td></tr>' : '';

		$('#HistoryEdit_TimeStatsTable tbody').html(table);
	}

	function tabClick(e)
	{
		e.preventDefault();

		$('#HistoryEdit_Back').fadeIn(showing ? 0 : 500);
		$('#HistoryEdit_BackSpace').hide();
		var tab = '#' + $(this).attr('data-tab');
		lastPage = $(tab);
		lastFullscreen = ($(this).attr('data-fullscreen') === 'true') && !UISettings.miniTheme;

		$HistoryEditDialog.switchTab($('#HistoryEdit_GeneralTab'), lastPage,
			e.shiftKey || !UISettings.slideAnimation || showing ? 0 : 500,
			{fullscreen: lastFullscreen, mini: UISettings.miniTheme});

		if (tab === '#HistoryEdit_LogTab' && !logFilled && curHist.MessageCount > 0)
		{
			LogTab.fill('History', curHist);
			logFilled = true;
		}
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

	function completed()
	{
		$HistoryEditDialog.modal('hide');
		Refresher.update();
		if (notification)
		{
			PopupNotification.show(notification);
			notification = null;
		}
	}

	function saveChanges(e)
	{
		e.preventDefault();
		disableAllButtons();
		notification = null;
		saveCompleted = completed;
		saveName();
	}

	function saveName()
	{
		var name = $('#HistoryEdit_NZBName').val();
		name !== curHist.Name && !curHist.postprocess ?
			RPC.call('editqueue', ['HistorySetName', name, [curHist.ID]], function()
			{
				notification = '#Notif_History_Saved';
				saveCategory();
			})
			:saveCategory();
	}

	function saveCategory()
	{
		var category = $('#HistoryEdit_Category').val();
		category !== curHist.Category && curHist.Kind !== 'DUP' ?
			RPC.call('editqueue', ['HistorySetCategory', category, [curHist.ID]], function()
			{
				notification = '#Notif_History_Saved';
				saveDupeKey();
			})
			: saveDupeKey();
	}

	function itemActions(e)
	{
		e.preventDefault();
		e.stopPropagation();
		var elem = $('#HistoryEdit_Actions').parent();

		HistoryActionsMenu.showPopupMenu(curHist, 'top',
			{ left: elem.offset().left, top: elem.offset().top - 1,
				width: elem.width(), height: elem.height() + 2 },
			function(_notification, actionCallback)
			{
				disableAllButtons();
				notification = _notification;
				saveCompleted = actionCallback;
				saveName();
				return true; // async
			},
			completed);
	}

	/*** TAB: POST-PROCESSING PARAMETERS **************************************************/

	function saveParam()
	{
		if (curHist.Kind !== 'NZB')
		{
			saveCompleted();
			return;
		}

		var paramList = ParamTab.prepareParamRequest(postParams);
		saveNextParam(paramList);
	}

	function saveNextParam(paramList)
	{
		if (paramList.length > 0)
		{
			RPC.call('editqueue', ['HistorySetParameter', paramList[0], [curHist.ID]], function()
			{
				notification = '#Notif_History_Saved';
				paramList.shift();
				saveNextParam(paramList);
			})
		}
		else
		{
			saveCompleted();
		}
	}

	/*** TAB: DUPLICATE SETTINGS **************************************************/

	function saveDupeKey()
	{
		var value = $('#HistoryEdit_DupeKey').val();
		value !== curHist.DupeKey ?
			RPC.call('editqueue', ['HistorySetDupeKey', value, [curHist.ID]], function()
			{
				notification = '#Notif_History_Saved';
				saveDupeScore();
			})
			:saveDupeScore();
	}

	function saveDupeScore()
	{
		var value = $('#HistoryEdit_DupeScore').val();
		value != curHist.DupeScore ?
			RPC.call('editqueue', ['HistorySetDupeScore', value, [curHist.ID]], function()
			{
				notification = '#Notif_History_Saved';
				saveDupeMode();
			})
			:saveDupeMode();
	}

	function saveDupeMode()
	{
		var value = $('#HistoryEdit_DupeMode').val();
		value !== curHist.DupeMode ?
			RPC.call('editqueue', ['HistorySetDupeMode', value, [curHist.ID]], function()
			{
				notification = '#Notif_History_Saved';
				saveDupeBackup();
			})
			:saveDupeBackup();
	}

	function saveDupeBackup()
	{
		var canChange = curHist.DeleteStatus === 'DUPE' || curHist.DeleteStatus === 'MANUAL';
		var oldValue = curHist.DeleteStatus === 'DUPE';
		var value = $('#HistoryEdit_DupeBackup').is(':checked');
		canChange && value !== oldValue ?
			RPC.call('editqueue', ['HistorySetDupeBackup', value ? "YES" : "NO", [curHist.ID]], function()
			{
				notification = '#Notif_History_Saved';
				saveParam();
			})
			:saveParam();
	}
}(jQuery));
