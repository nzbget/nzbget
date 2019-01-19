/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2013-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * In this module:
 *   1) Feeds menu;
 *   2) Feed view/preview dialog;
 *   3) Feed filter dialog.
 */


/*** FEEDS **********************************************/

var Feeds = (new function($)
{
	'use strict';

	this.init = function()
	{
	}

	this.redraw = function()
	{
		var menu = $('#RssMenu');
		var menuItemTemplate = $('.feed-menu-template', menu);
		menuItemTemplate.removeClass('feed-menu-template').removeClass('hide').addClass('feed-menu');
		var insertPos = $('#RssMenu_Divider', menu);

		$('.feed-menu', menu).remove();
		for (var i=1; ;i++)
		{
			var url = Options.option('Feed' + i + '.URL');
			if (url === null)
			{
				break;
			}

			if (url.trim() !== '')
			{
				var item = menuItemTemplate.clone();
				var name = Options.option('Feed' + i + '.Name');
				var a = $('span', item);
				a.text(name !== '' ? name : 'Feed' + i);
				a.attr('data-id', i);
				a.click(viewFeed);
				var im = $('button', item);
				im.click(fetchFeed);
				im.attr('data-id', i);
				insertPos.before(item);
			}
		}

		Util.show('#RssMenuBlock', $('.feed-menu', menu).length > 0);
	}

	function viewFeed()
	{
		var id = parseInt($(this).attr('data-id'));
		FeedDialog.showModal(id);
	}

	function fetchFeed()
	{
		var id = parseInt($(this).attr('data-id'));
		RPC.call('fetchfeed', [id], function()
		{
			PopupNotification.show('#Notif_Feeds_Fetch');
		});
	}

	this.fetchAll = function()
	{
		RPC.call('fetchfeed', [0], function()
		{
			PopupNotification.show('#Notif_Feeds_Fetch');
		});
	}
}(jQuery));


/*** FEEDS VIEW / PREVIEW DIALOG **********************************************/

var FeedDialog = (new function($)
{
	'use strict';

	// Controls
	var $FeedDialog;
	var $ItemTable;

	// State
	var items = null;
	var pageSize = 100;
	var curFilter = 'ALL';
	var filenameMode = false;
	var tableInitialized = false;

	this.init = function()
	{
		$FeedDialog = $('#FeedDialog');

		$ItemTable = $('#FeedDialog_ItemTable');
		$ItemTable.fasttable(
			{
				filterInput: '#FeedDialog_ItemTable_filter',
				pagerContainer: '#FeedDialog_ItemTable_pager',
				rowSelect: UISettings.rowSelect,
				pageSize: pageSize,
				renderCellCallback: itemsTableRenderCellCallback
			});

		$FeedDialog.on('hidden', function()
		{
			// cleanup
			$ItemTable.fasttable('update', []);
			// resume updates
			Refresher.resume();
		});

		TabDialog.extend($FeedDialog);

		if (UISettings.setFocus)
		{
			$FeedDialog.on('shown', function()
			{
				//$('#FeedDialog_Name').focus();
			});
		}
	}

	this.showModal = function(id, name, url, filter, backlog, pauseNzb, category, priority, interval, feedscript)
	{
		Refresher.pause();

		enableAllButtons();
		$FeedDialog.restoreTab();

		$('#FeedDialog_ItemTable_filter').val('');
		$('#FeedDialog_ItemTable_pagerBlock').hide();

		$ItemTable.fasttable('update', []);
		$ItemTable.fasttable('applyFilter', '');

		items = null;

		curFilter = 'ALL';
		filenameMode = false;
		tableInitialized = false;
		$('#FeedDialog_Toolbar .badge').text('?');
		updateFilterButtons(undefined, undefined, undefined, false);
		tableInitialized = false;

		$FeedDialog.modal({backdrop: 'static'});
		$FeedDialog.maximize({mini: UISettings.miniTheme});

		$('.loading-block', $FeedDialog).show();

		if (name === undefined)
		{
			var name = Options.option('Feed' + id + '.Name');
			$('#FeedDialog_Title').text(name !== '' ? name : 'Feed');
			RPC.call('viewfeed', [id, false], itemsLoaded, feedFailure);
		}
		else
		{
			$('#FeedDialog_Title').text(name !== '' ? name : 'Feed Preview');
			var feedBacklog = backlog === 'yes';
			var feedPauseNzb = pauseNzb === 'yes';
			var feedCategory = category;
			var feedPriority = parseInt(priority);
			var feedInterval = parseInt(interval);
			var feedScript = feedscript;
			RPC.call('previewfeed', [id, name, url, filter, feedBacklog, feedPauseNzb, feedCategory,
				feedPriority, feedInterval, feedScript, false, 0, ''], itemsLoaded, feedFailure);
		}

		if (!UISettings.miniTheme)
		{
			$('#FeedDialog_TableBlock').removeClass('modal-inner-scroll');
			$('#FeedDialog_TableBlock').css('top', '');
			$('#FeedDialog_TableBlock').css('top', $('#FeedDialog_TableBlock').position().top);
			$('#FeedDialog_TableBlock').addClass('modal-inner-scroll');
		}
	}

	function feedFailure(res)
	{
		$FeedDialog.modal('hide');
		AlertDialog.showModal('Error', res);
	}

	function disableAllButtons()
	{
		$('#FeedDialog .modal-footer .btn').attr('disabled', 'disabled');
		setTimeout(function()
		{
			$('#FeedDialog_Transmit').show();
		}, 500);
	}

	function enableAllButtons()
	{
		$('#FeedDialog .modal-footer .btn').removeAttr('disabled');
		$('#FeedDialog_Transmit').hide();
	}

	function itemsLoaded(itemsArr)
	{
		$('.loading-block', $FeedDialog).hide();
		items = itemsArr;
		updateTable();
		$('.modal-inner-scroll', $FeedDialog).scrollTop(100).scrollTop(0);
	}

	function updateTable()
	{
		var countNew = 0;
		var countFetched = 0;
		var countBacklog = 0;
		var differentFilenames = false;

		var data = [];

		for (var i=0; i < items.length; i++)
		{
			var item = items[i];

			var age = Util.formatAge(item.Time + UISettings.timeZoneCorrection*60*60);
			var size = (item.SizeMB > 0 || item.SizeLo > 0 || item.SizeHi > 0) ? Util.formatSizeMB(item.SizeMB, item.SizeLo) : '';

			var status;
			switch (item.Status)
			{
				case 'UNKNOWN': status = '<span class="label label-status label-important">UNKNOWN</span>'; break;
				case 'BACKLOG': status = '<span class="label label-status">BACKLOG</span>'; countBacklog +=1; break;
				case 'FETCHED': status = '<span class="label label-status label-success">FETCHED</span>'; countFetched +=1; break;
				case 'NEW': status = '<span class="label label-status  label-info">NEW</span>'; countNew +=1; break;
				default: status = '<span class="label label-status label-important">internal error(' + item.Status + ')</span>';
			}

			if (!(curFilter === item.Status || curFilter === 'ALL'))
			{
				continue;
			}

			differentFilenames = differentFilenames || (item.Filename !== item.Title);

			var itemName = filenameMode ? item.Filename : item.Title;
			var name = Util.textToHtml(itemName);
			name = name.replace(/\./g, '.<wbr>').replace(/_/g, '_<wbr>');

			var fields;

			if (!UISettings.miniTheme)
			{
				fields = ['<div class="check img-check"></div>', status, name, item.Category, age, size];
			}
			else
			{
				var info = '<div class="check img-check"></div><span class="row-title">' + name + '</span>' + ' ' + status;
				if (item.Category !== '')
				{
					info += ' <span class="label label-info">' + item.Category + '</span>';
				}
				info += ' <span class="label label-info">' + age + '</span>' +
					' <span class="label label-info">' + size + '</span>';
				fields = [info];
			}

			var item =
			{
				id: item.URL,
				item: item,
				fields: fields,
				data: { status: item.Status, name: itemName, category: item.Category, age: age, size: size, _search: true }
			};

			data.push(item);
		}

		$ItemTable.fasttable('update', data);
		$ItemTable.fasttable('setCurPage', 1);

		Util.show('#FeedDialog_ItemTable_pagerBlock', data.length > pageSize);
		updateFilterButtons(countNew, countFetched, countBacklog, differentFilenames);
	}

	function itemsTableRenderCellCallback(cell, index, item)
	{
		if (index > 3)
		{
			cell.className = 'text-right';
		}
	}

	function updateFilterButtons(countNew, countFetched, countBacklog, differentFilenames)
	{
		if (countNew != undefined)
		{
			$('#FeedDialog_Badge_ALL,#FeedDialog_Badge_ALL2').text(countNew + countFetched + countBacklog);
			$('#FeedDialog_Badge_NEW,#FeedDialog_Badge_NEW2').text(countNew);
			$('#FeedDialog_Badge_FETCHED,#FeedDialog_Badge_FETCHED2').text(countFetched);
			$('#FeedDialog_Badge_BACKLOG,#FeedDialog_Badge_BACKLOG2').text(countBacklog);
		}

		$('#FeedDialog_Toolbar .btn').removeClass('btn-inverse');
		$('#FeedDialog_Badge_' + curFilter + ',#FeedDialog_Badge_' + curFilter + '2').closest('.btn').addClass('btn-inverse');
		$('#FeedDialog_Toolbar .badge').removeClass('badge-active');
		$('#FeedDialog_Badge_' + curFilter + ',#FeedDialog_Badge_' + curFilter + '2').addClass('badge-active');

		if (differentFilenames != undefined && !tableInitialized)
		{
			Util.show('#FeedDialog .FeedDialog-names', differentFilenames);
			tableInitialized = true;
		}

		$('#FeedDialog_Titles,#FeedDialog_Titles2').toggleClass('btn-inverse', !filenameMode);
		$('#FeedDialog_Filenames,#FeedDialog_Filenames2').toggleClass('btn-inverse', filenameMode);
		$('#FeedDialog_ItemTable_Name').text(filenameMode ? 'Filename' : 'Title');
	}

	this.fetch = function()
	{
		var checkedRows = $ItemTable.fasttable('checkedRows');
		var checkedCount = $ItemTable.fasttable('checkedCount');
		if (checkedCount === 0)
		{
			PopupNotification.show('#Notif_FeedDialog_Select');
			return;
		}

		disableAllButtons();

		var fetchItems = [];
		for (var i = 0; i < items.length; i++)
		{
			var item = items[i];
			if (checkedRows[item.URL])
			{
				fetchItems.push(item);
			}
		}

		fetchNextItem(fetchItems);
	}

	function fetchNextItem(fetchItems)
	{
		if (fetchItems.length > 0)
		{
			var name = fetchItems[0].Filename;
			if (name.substr(name.length-4, 4).toLowerCase() !== '.nzb')
			{
				name += '.nzb';
			}
			RPC.call('append', [name, fetchItems[0].URL, fetchItems[0].AddCategory, fetchItems[0].Priority, false,
				false, fetchItems[0].DupeKey, fetchItems[0].DupeScore, fetchItems[0].DupeMode],
				function()
			{
				fetchItems.shift();
				fetchNextItem(fetchItems);
			})
		}
		else
		{
			$FeedDialog.modal('hide');
			PopupNotification.show('#Notif_FeedDialog_Fetched');
		}
	}

	this.filter = function(type)
	{
		curFilter = type;
		updateTable();
	}

	this.setFilenameMode = function(mode)
	{
		filenameMode = mode;
		updateTable();
	}
}(jQuery));


/*** FEED FILTER DIALOG **********************************************/

var FeedFilterDialog = (new function($)
{
	'use strict';

	// Controls
	var $FeedFilterDialog;
	var $ItemTable;
	var $Splitter;
	var $FilterInput;
	var $FilterBlock;
	var $FilterNumbers;
	var $PreviewBlock;
	var $ModalBody;
	var $LoadingBlock;
	var $CHAutoRematch;
	var $RematchIcon;

	// State
	var items = null;
	var pageSize = 100;
	var curFilter = 'ALL';
	var filenameMode = false;
	var tableInitialized = false;
	var saveCallback;
	var splitStartPos;
	var feedId;
	var feedName;
	var feedUrl;
	var feedFilter;
	var feedBacklog;
	var feedPauseNzb;
	var feedCategory;
	var feedPriority;
	var feedInterval;
	var feedScript;
	var cacheTimeSec;
	var cacheId;
	var updating;
	var updateTimerIntitialized = false;
	var autoUpdate = false;
	var splitRatio;
	var firstUpdate;
	var lineNo;
	var showLines;

	this.init = function()
	{
		$FeedFilterDialog = $('#FeedFilterDialog');
		$Splitter = $('#FeedFilterDialog_Splitter');
		$Splitter.mousedown(splitterMouseDown);
		$('#FeedFilterDialog_Save').click(save);
		$FilterInput = $('#FeedFilterDialog_FilterInput');
		$FilterBlock = $('#FeedFilterDialog_FilterBlock');
		$FilterNumbers = $('#FeedFilterDialog_FilterNumbers');
		$PreviewBlock = $('#FeedFilterDialog_PreviewBlock');
		$ModalBody = $('.modal-body', $FeedFilterDialog);
		$LoadingBlock = $('.loading-block', $FeedFilterDialog);
		$CHAutoRematch = $('#FeedFilterDialog_CHAutoRematch');
		$RematchIcon = $('#FeedFilterDialog_RematchIcon');

		autoUpdate = UISettings.read('$FeedFilterDialog_AutoRematch', '1') == '1';
		updateRematchState();
		initLines();

		$ItemTable = $('#FeedFilterDialog_ItemTable');
		$ItemTable.fasttable(
			{
				filterInput: '',
				pagerContainer: '#FeedFilterDialog_ItemTable_pager',
				headerCheck: '',
				pageSize: pageSize,
				renderCellCallback: itemsTableRenderCellCallback
			});

		$ItemTable.on('mousedown', Util.disableShiftMouseDown);

		$FilterInput.keypress(filterKeyPress);

		$FeedFilterDialog.on('hidden', function()
		{
			// cleanup
			$ItemTable.fasttable('update', []);
			$(window).off('resize', windowResized);
			// resume updates
			Refresher.resume();
		});

		TabDialog.extend($FeedFilterDialog);

		if (UISettings.setFocus)
		{
			$FeedFilterDialog.on('shown', function()
			{
				$FilterInput.focus();
			});
		}
	}

	this.showModal = function(id, name, url, filter, backlog, pauseNzb, category, priority, interval, feedscript, _saveCallback)
	{
		saveCallback = _saveCallback;

		Refresher.pause();

		$ItemTable.fasttable('update', []);

		$FeedFilterDialog.restoreTab();
		$(window).on('resize', windowResized);
		splitterRestore();

		$('#FeedFilterDialog_ItemTable_pagerBlock').hide();
		$FilterInput.val(filter.replace(/\s*%\s*/g, '\n'));

		items = null;
		firstUpdate = true;
		curFilter = 'ALL';
		filenameMode = false;
		tableInitialized = false;
		$('#FeedFilterDialog_Toolbar .badge').text('?');
		updateFilterButtons(undefined, undefined, undefined, false);
		tableInitialized = false;

		$FeedFilterDialog.modal({backdrop: 'static'});
		$FeedFilterDialog.maximize({mini: UISettings.miniTheme});

		updateLines();
		$LoadingBlock.show();

		$('#FeedFilterDialog_Title').text(name !== '' ? name : 'Feed Preview');
		feedId = id;
		feedName = name;
		feedUrl = url;
		feedFilter = filter;
		feedBacklog = backlog === 'yes';
		feedPauseNzb = pauseNzb === 'yes';
		feedCategory = category;
		feedPriority = parseInt(priority);
		feedInterval = parseInt(interval);
		feedScript = feedscript;
		cacheId = '' + Math.random()*10000000;
		cacheTimeSec = 60*10; // 10 minutes

		if (url !== '')
		{
			RPC.call('previewfeed', [feedId, name, url, filter, feedBacklog, feedPauseNzb, feedCategory, feedPriority,
				feedInterval, feedScript, true, cacheTimeSec, cacheId], itemsLoaded, feedFailure);
		}
		else
		{
			$LoadingBlock.hide();
		}
	}

	this.rematch = function()
	{
		updateFilter();
	}

	function updateFilter()
	{
		if (feedUrl == '')
		{
			return;
		}

		tableInitialized = false;
		updating = true;

		var filter = $FilterInput.val().replace(/\n/g, '%');
		RPC.call('previewfeed', [feedId, feedName, feedUrl, filter, feedBacklog, feedPauseNzb, feedCategory, feedPriority,
			feedInterval, feedScript, true, cacheTimeSec, cacheId], itemsLoaded, feedFailure);

		setTimeout(function()
		{
			if (updating)
			{
				$LoadingBlock.show();
			}
		}, 500);
	}

	function feedFailure(msg, result)
	{
		updating = false;
		var filter = $FilterInput.val().replace(/\n/g, ' % ');
		if (firstUpdate && filter === feedFilter)
		{
			$FeedFilterDialog.modal('hide');
		}
		$LoadingBlock.hide();
		AlertDialog.showModal('Error', result ? result.error.message : msg);
	}

	function itemsLoaded(itemsArr)
	{
		updating = false;
		$LoadingBlock.hide();
		items = itemsArr;
		updateTable();
		if (firstUpdate)
		{
			$('.modal-inner-scroll', $FeedFilterDialog).scrollTop(100).scrollTop(0);
		}
		firstUpdate = false;

		if (!updateTimerIntitialized)
		{
			setupUpdateTimer();
			updateTimerIntitialized = true;
		}
	}

	function updateTable()
	{
		var countAccepted = 0;
		var countRejected = 0;
		var countIgnored = 0;
		var differentFilenames = false;

		var filter = $FilterInput.val().split('\n');

		var data = [];

		for (var i=0; i < items.length; i++)
		{
			var item = items[i];

			var age = Util.formatAge(item.Time + UISettings.timeZoneCorrection*60*60);
			var size = (item.SizeMB > 0 || item.SizeLo > 0 || item.SizeHi > 0) ? Util.formatSizeMB(item.SizeMB, item.SizeLo) : '';

			var status;
			switch (item.Match)
			{
				case 'ACCEPTED':
					var addInfo = [item.AddCategory !== feedCategory ? 'category: ' + item.AddCategory : null,
						item.Priority !== feedPriority ? DownloadsUI.buildPriorityText(item.Priority) : null,
						item.PauseNzb !== feedPauseNzb ? (item.PauseNzb ? 'paused' : 'unpaused') : null,
						item.DupeScore != 0 ? 'dupe-score: ' + item.DupeScore : null,
						item.DupeKey !== '' ? 'dupe-key: ' + item.DupeKey : null,
						item.DupeMode !== 'SCORE' ? 'dupe-mode: ' + item.DupeMode.toLowerCase() : null].
						filter(function(e){return e}).join('; ');
					status = '<span class="label label-status label-success" title="' + Util.textToAttr(addInfo) + '">ACCEPTED</span>';
					countAccepted += 1;
					break;
				case 'REJECTED': status = '<span class="label label-status label-important">REJECTED</span>'; countRejected += 1; break;
				case 'IGNORED': status = '<span class="label label-status">IGNORED</span>'; countIgnored += 1; break;
				default: status = '<span class="label label-status label-important">internal error(' + item.Match + ')</span>'; break;
			}

			if (!(curFilter === item.Match || curFilter === 'ALL'))
			{
				continue;
			}

			differentFilenames = differentFilenames || (item.Filename !== item.Title);

			var itemName = filenameMode ? item.Filename : item.Title;
			var name = Util.textToHtml(itemName);
			name = name.replace(/\./g, '.<wbr>').replace(/_/g, '_<wbr>');

			var rule = '';
			if (item.Rule > 0)
			{
				rule = '<span class="label label-status label-info filter-rule" title="' +
					Util.textToAttr(filter[item.Rule-1]) +'" '+
					'onclick="FeedFilterDialog.selectRule(' + item.Rule +')"> ' + item.Rule + ' </span>';
			}

			var fields;

			if (!UISettings.miniTheme)
			{
				fields = [status, rule, name, item.Category, age, size];
			}
			else
			{
				var info = '<span class="row-title">' + name + '</span>' + ' ' + status;
				fields = [info];
			}

			var dataItem =
			{
				id: item.URL,
				item: item,
				fields: fields,
				data: { match: item.Match, rule: item.Rule, title: itemName, category: item.Category, age: age, size: size, _search: true }
			};

			data.push(dataItem);
		}

		$ItemTable.fasttable('update', data);

		Util.show('#FeedFilterDialog_ItemTable_pagerBlock', data.length > pageSize);
		updateFilterButtons(countAccepted, countRejected, countIgnored, differentFilenames);
	}

	function itemsTableRenderCellCallback(cell, index, item)
	{
		if (index > 3)
		{
			cell.className = 'text-right';
		}
	}

	function updateFilterButtons(countAccepted, countRejected, countIgnored, differentFilenames)
	{
		if (countAccepted != undefined)
		{
			$('#FeedFilterDialog_Badge_ALL,#FeedFilterDialog_Badge_ALL2').text(countAccepted + countRejected + countIgnored);
			$('#FeedFilterDialog_Badge_ACCEPTED,#FeedFilterDialog_Badge_ACCEPTED2').text(countAccepted);
			$('#FeedFilterDialog_Badge_REJECTED,#FeedFilterDialog_Badge_REJECTED2').text(countRejected);
			$('#FeedFilterDialog_Badge_IGNORED,#FeedFilterDialog_Badge_IGNORED2').text(countIgnored);
		}

		$('#FeedFilterDialog_Toolbar .FeedFilterDialog-filter .btn').removeClass('btn-inverse');
		$('#FeedFilterDialog_Badge_' + curFilter + ',#FeedFilterDialog_Badge_' + curFilter + '2').closest('.btn').addClass('btn-inverse');
		$('#FeedFilterDialog_Toolbar .badge').removeClass('badge-active');
		$('#FeedFilterDialog_Badge_' + curFilter + ',#FeedFilterDialog_Badge_' + curFilter + '2').addClass('badge-active');

		if (differentFilenames != undefined && !tableInitialized)
		{
			Util.show('#FeedFilterDialog .FeedFilterDialog-names', differentFilenames);
			tableInitialized = true;
		}

		$('#FeedFilterDialog_Titles,#FeedFilterDialog_Titles2').toggleClass('btn-inverse', !filenameMode);
		$('#FeedFilterDialog_Filenames,#FeedFilterDialog_Filenames2').toggleClass('btn-inverse', filenameMode);
		$('#FeedFilterDialog_ItemTable_Name').text(filenameMode ? 'Filename' : 'Title');
	}

	this.filter = function(type)
	{
		curFilter = type;
		updateTable();
	}

	this.setFilenameMode = function(mode)
	{
		filenameMode = mode;
		updateTable();
	}

	function save(e)
	{
		e.preventDefault();

		$FeedFilterDialog.modal('hide');
		var filter = $FilterInput.val().replace(/\n/g, ' % ');
		saveCallback(filter);
	}

	function setupUpdateTimer()
	{
		// Create a timer which gets reset upon every keyup event.
		// Perform filter only when the timer's wait is reached (user finished typing or paused long enough to elapse the timer).
		// Do not perform the filter if the query has not changed.

		var timer;
		var lastFilter = $FilterInput.val();

		$FilterInput.keyup(function()
		{
			var timerCallback = function()
			{
				var value = $FilterInput.val();
				if (value != lastFilter)
				{
					lastFilter = value;
					if (autoUpdate)
					{
						updateFilter();
					}
				}
			};

			// Reset the timer
			clearTimeout(timer);
			timer = setTimeout(timerCallback, 500);

			return false;
		});
	}

	this.autoRematch = function()
	{
		autoUpdate = !autoUpdate;
		UISettings.write('$FeedFilterDialog_AutoRematch', autoUpdate ? '1' : '0');
		updateRematchState();
		if (autoUpdate)
		{
			updateFilter();
		}
	}

	function updateRematchState()
	{
		Util.show($CHAutoRematch, autoUpdate);
		$RematchIcon.toggleClass('icon-process', !autoUpdate);
		$RematchIcon.toggleClass('icon-process-auto', autoUpdate);
	}

	function filterKeyPress(event)
	{
		if (event.which == 37)
		{
			event.preventDefault();
			alert('Percent character (%) cannot be part of a filter because it is used\nas line separator when saving filter into configuration file.');
		}
	}

	/*** SPLITTER ***/

	function splitterMouseDown(e)
	{
		e.stopPropagation();
		e.preventDefault();
		splitStartPos = e.pageX;
		$(document).bind("mousemove", splitterMouseMove).bind("mouseup", splitterMouseUp);
		$ModalBody.css('cursor', 'col-resize');
		$FilterInput.css('cursor', 'col-resize');
	}

	function splitterMouseMove(e)
	{
		var newPos = e.pageX;
		var right = $PreviewBlock.position().left + $PreviewBlock.width();
		newPos = newPos < 150 ? 150 : newPos;
		newPos = newPos > right - 150 ? right - 150 : newPos;
		splitterMove(newPos - splitStartPos);
		splitStartPos = newPos;
	}

	function splitterMouseUp(e)
	{
		$ModalBody.css('cursor', '');
		$FilterInput.css('cursor', '');
		$(document).unbind("mousemove", splitterMouseMove).unbind("mouseup", splitterMouseUp);
		splitterSave();
	}

	function splitterMove(delta)
	{
		$FilterBlock.css('width', parseInt($FilterBlock.css('width')) + delta);
		$PreviewBlock.css('left', parseInt($PreviewBlock.css('left')) + delta);
		$Splitter.css('left', parseInt($Splitter.css('left')) + delta);
	}

	function splitterSave()
	{
		if (!UISettings.miniTheme)
		{
			splitRatio = parseInt($FilterBlock.css('width')) / $(window).width();
			UISettings.write('$FeedFilterDialog_SplitRatio', splitRatio);
		}
	}

	function splitterRestore()
	{
		if (!UISettings.miniTheme)
		{
			var oldSplitRatio = parseInt($FilterBlock.css('width')) / $(window).width();
			splitRatio = UISettings.read('$FeedFilterDialog_SplitRatio', oldSplitRatio);
			windowResized();
		}
	}

	function windowResized()
	{
		if (!UISettings.miniTheme)
		{
			var oldWidth = parseInt($FilterBlock.css('width'));
			var winWidth = $(window).width();
			var newWidth = Math.round(winWidth * splitRatio);
			var right = winWidth - 30;
			newWidth = newWidth > right - 150 ? right - 150 : newWidth;
			newWidth = newWidth < 150 ? 150 : newWidth;
			splitterMove(newWidth - oldWidth);
		}
	}

	/*** LINE SELECTION ***/

	this.selectRule = function(rule)
	{
		selectTextareaLine($FilterInput[0], rule);
	}

	function selectTextareaLine(tarea, lineNum)
	{
		lineNum--; // array starts at 0
		var lines = tarea.value.split("\n");

		// calculate start/end
		var startPos = 0;
		for (var x = 0; x < lines.length; x++)
		{
			if (x == lineNum)
			{
				break;
			}
			startPos += (lines[x].length+1);
		}

		var endPos = lines[lineNum].length+startPos;

		if (typeof(tarea.selectionStart) != "undefined")
		{
			tarea.focus();
			tarea.selectionStart = startPos;
			tarea.selectionEnd = endPos;
		}
	}

	/*** LINE NUMBERS ***/

	// Idea and portions of code from LinedTextArea plugin by Alan Williamson
	// http://files.aw20.net/jquery-linedtextarea/jquery-linedtextarea.html

	function initLines()
	{
		showLines = !UISettings.miniTheme;
		if (showLines)
		{
			lineNo = 1;
			$FilterInput.scroll(updateLines);
		}
	}

	function updateLines()
	{
		if (!UISettings.miniTheme && showLines)
		{
			var domTextArea = $FilterInput[0];
			var scrollTop = domTextArea.scrollTop;
			var clientHeight = domTextArea.clientHeight;
			$FilterNumbers.css('margin-top', (-1*scrollTop) + "px");
			lineNo = fillOutLines(scrollTop + clientHeight, lineNo);
		}
	}

	function fillOutLines(h, lineNo)
	{
		while ($FilterNumbers.height() - h <= 0)
		{
			$FilterNumbers.append("<div class='lineno'>" + lineNo + "</div>");
			lineNo++;
		}
		return lineNo;
	}
}(jQuery));
