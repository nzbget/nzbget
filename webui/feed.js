/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * $Revision: 0 $
 * $Date: 2013-06-24 00:00:00 +0200 (Mo, 24 Jun 2013) $
 *
 */

/*
 * In this module:
 *   1) Feed view/preview dialog;
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
				var a = $('a', item);
				a.text(name !== '' ? name : 'Feed' + i);
				a.attr('data-id', i);
				a.click(viewFeed);
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
	
	this.fetchAll = function()
	{
		RPC.call('fetchfeeds', [], function()
		{
			Notification.show('#Notif_Feeds_FetchAll');
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
	var category = null;
	var priority = null;
	var pageSize = 100;

	this.init = function()
	{
		$FeedDialog = $('#FeedDialog');

		$ItemTable = $('#FeedDialog_ItemTable');
		$ItemTable.fasttable(
			{
				filterInput: '#FeedDialog_ItemTable_filter',
				pagerContainer: '#FeedDialog_ItemTable_pager',
				filterCaseSensitive: false,
				headerCheck: '#FeedDialog_ItemTable > thead > tr:first-child',
				pageSize: pageSize,
				hasHeader: true,
				renderCellCallback: itemsTableRenderCellCallback
			});

		$ItemTable.on('click', 'tbody div.check',
			function(event) { $ItemTable.fasttable('itemCheckClick', this.parentNode.parentNode, event); });
		$ItemTable.on('click', 'thead div.check',
			function() { $ItemTable.fasttable('titleCheckClick') });
		$ItemTable.on('mousedown', Util.disableShiftMouseDown);
	
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

	this.showModal = function(id, name, url, filter, _category, _priority)
	{
		Refresher.pause();

		$ItemTable.fasttable('update', []);

		enableAllButtons();
		$FeedDialog.restoreTab();
		
		$('#FeedDialog_ItemTable_filter').val('');
		$('#FeedDialog_ItemTable_pagerBlock').hide();

		items = null;
		
		$FeedDialog.modal({backdrop: 'static'});
		$FeedDialog.maximize({mini: UISettings.miniTheme});

		$('.loading-block', $FeedDialog).show();

		if (id > 0)
		{
			var name = Options.option('Feed' + id + '.Name');
			category = Options.option('Feed' + id + '.Category');
			priority = Options.option('Feed' + id + '.Priority');
			$('#FeedDialog_Title').text(name !== '' ? name : 'Feed');
			RPC.call('viewfeed', [id], itemsLoaded, feedFailure);
		}
		else
		{
			$('#FeedDialog_Title').text(name !== '' ? name : 'Feed Preview');
			category = _category;
			priority = _priority;
			RPC.call('previewfeed', [name, url, filter], itemsLoaded, feedFailure);
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
				case 'BACKLOG': status = '<span class="label label-status">BACKLOG</span>'; break;
				case 'FETCHED': status = '<span class="label label-status label-success">FETCHED</span>'; break;
				case 'NEW': status = '<span class="label label-status  label-info">NEW</span>'; break;
				default: status = '<span class="label label-status label-important">internal error(' + item.Status + ')</span>';
			}
			
			var name = Util.textToHtml(item.Name);
			name = name.replace(/\./g, '.<wbr>').replace(/_/g, '_<wbr>');
			
			var fields;

			if (!UISettings.miniTheme)
			{
				fields = ['<div class="check img-check"></div>', status, name, item.Category, age, size];
			}
			else
			{
				var info = '<div class="check img-check"></div><span class="row-title">' + name + '</span>' + ' ' + status;
				fields = [info];
			}
			
			var item =
			{
				id: item.URL,
				item: item,
				fields: fields,
				search: item.Status + ' ' + item.Name + ' ' + item.URL + ' ' + item.Category  + ' ' + age + ' ' + size
			};

			data.push(item);
		}

		$ItemTable.fasttable('update', data);
		$ItemTable.fasttable('setCurPage', 1);

		Util.show('#FeedDialog_ItemTable_pagerBlock', data.length > pageSize);
	}

	function itemsTableRenderCellCallback(cell, index, item)
	{
		if (index > 3)
		{
			cell.className = 'text-right';
		}
	}
	
	this.fetch = function()
	{
		var checkedRows = $ItemTable.fasttable('checkedRows');
		if (checkedRows.length == 0)
		{
			Notification.show('#Notif_FeedDialog_Select');
			return;
		}

		disableAllButtons();

		var fetchItems = [];
		for (var i = 0; i < items.length; i++)
		{
			var item = items[i];
			if (checkedRows.indexOf(item.URL) > -1)
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
			var name = fetchItems[0].Name;
			if (name.substr(name, name.length-3, 3).toLowerCase() !== '.nzb')
			{
				name += '.nzb';
			}
			RPC.call('appendurl', [name, category, priority !== '' ? parseInt(priority) : 0, false, fetchItems[0].URL], function()
			{
				fetchItems.shift();
				fetchNextItem(fetchItems);
			})
		}
		else
		{
			$FeedDialog.modal('hide');
			Notification.show('#Notif_FeedDialog_Fetched');
		}
	}
	
}(jQuery));
