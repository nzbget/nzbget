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
* Some code was borrowed from:
* 1. Greg Weber's uiTableFilter jQuery plugin (http://gregweber.info/projects/uitablefilter)
* 2. Denny Ferrassoli & Charles Christolini's TypeWatch jQuery plugin (http://github.com/dennyferra/TypeWatch)
* 3. Justin Britten's tablesorterFilter jQuery plugin (http://www.justinbritten.com/work/2008/08/tablesorter-filter-results-based-on-search-string/)
* 4. Allan Jardine's Bootstrap Pagination jQuery plugin for DataTables (http://datatables.net/)
*/

/*
 * In this module:
 *   HTML tables with:
 *     1) very fast content updates;
 *     2) automatic pagination;
 *     3) search/filtering;
 *     4) drag and drop.
 *
 * What makes it unique and fast?
 * The tables are designed to be updated very often (up to 10 times per second). This has two challenges:
 *   1) updating of whole content is slow because the DOM updates are slow.
 *   2) if the DOM is updated during user interaction the user input is not processed correctly.
 *       For example if the table is updated after the user pressed mouse key but before he/she released
 *       the key, the click is not processed because the element, on which the click was performed,
 *       doesn't exist after the update of DOM anymore.
 *
 * How Fasttable solves these problems? The solutions is to update only rows and cells,
 * which were changed by keeping the unchanged DOM-elements.
 *
 * Important: the UI of table must be designed in a way, that the cells which are frequently changed
 * (like remaining download size) should not be clickable, whereas the cells which are rarely changed
 * (e. g. Download name) can be clickable.
 */

(function($) {

	'use strict';

	$.fn.fasttable = function(method)
	{
		if (methods[method])
		{
			return methods[method].apply( this, Array.prototype.slice.call( arguments, 1 ));
		}
		else if ( typeof method === 'object' || ! method )
		{
			return methods.init.apply( this, arguments );
		}
		else
		{
			$.error( 'Method ' +  method + ' does not exist on jQuery.fasttable' );
		}
	};

	var methods =
	{
		defaults : function()
		{
			return defaults;
		},

		init : function(options)
		{
			return this.each(function()
			{
				var $this = $(this);
				var data = $this.data('fasttable');

				// If the plugin hasn't been initialized yet
				if (!data)
				{
					/*
					Do more setup stuff here
					*/

					var config = {};
					config = $.extend(config, defaults, options);

					config.filterInput = $(config.filterInput);
					config.filterClearButton = $(config.filterClearButton);
					config.pagerContainer = $(config.pagerContainer);
					config.infoContainer = $(config.infoContainer);
					config.dragBox = $(config.dragBox);
					config.dragContent = $(config.dragContent);
					config.dragBadge = $(config.dragBadge);
					config.selector = $('th.table-selector', $this);

					var searcher = new FastSearcher();

					// Create a timer which gets reset upon every keyup event.
					// Perform filter only when the timer's wait is reached (user finished typing or paused long enough to elapse the timer).
					// Do not perform the filter is the query has not changed.
					// Immediately perform the filter if the ENTER key is pressed.

					var timer;

					config.filterInput.keyup(function()
					{
						var timerWait = 500;
						var overrideBool = false;
						var inputBox = this;

						// Was ENTER pushed?
						if (inputBox.keyCode == 13)
						{
							timerWait = 1;
							overrideBool = true;
						}

						var timerCallback = function()
						{
							var value = inputBox.value.trim();
							var data = $this.data('fasttable');
							if ((value != data.lastFilter) || overrideBool)
							{
								applyFilter(data, value);
							}
						};

						// Reset the timer
						clearTimeout(timer);
						timer = setTimeout(timerCallback, timerWait);

						return false;
					});

					config.filterClearButton.click(function()
					{
						var data = $this.data('fasttable');
						data.config.filterInput.val('');
						applyFilter(data, '');
					});

					config.pagerContainer.on('click', 'li', function (e)
					{
						e.preventDefault();
						var data = $this.data('fasttable');
						var pageNum = $(this).text();
						if (pageNum.indexOf('Prev') > -1)
						{
							data.curPage--;
						}
						else if (pageNum.indexOf('Next') > -1)
						{
							data.curPage++;
						}
						else if (isNaN(parseInt(pageNum)))
						{
							return;
						}
						else
						{
							data.curPage = parseInt(pageNum);
						}
						refresh(data);
					});

					var data = {
						target: $this,
						config: config,
						pageSize: parseInt(config.pageSize),
						maxPages: parseInt(config.maxPages),
						pageDots: Util.parseBool(config.pageDots),
						curPage: 1,
						checkedRows: {},
						checkedCount: 0,
						lastClickedRowID: null,
						searcher: searcher
					};

					initDragDrop(data);

					$this.on('click', 'thead > tr', function(e) { titleCheckClick(data, e); });
					$this.on('click', 'tbody > tr', function(e) { itemCheckClick(data, e); });

					$this.data('fasttable', data);
				}
			});
		},

		destroy : function()
		{
			return this.each(function()
			{
				var $this = $(this);

				// Namespacing FTW
				$(window).unbind('.fasttable');
				$this.removeData('fasttable');
			});
		},

		update : updateContent,

		setPageSize : setPageSize,

		setCurPage : setCurPage,

		applyFilter : function(filter)
		{
			applyFilter($(this).data('fasttable'), filter);
		},

		filteredContent : function()
		{
			return $(this).data('fasttable').filteredContent;
		},

		availableContent : function()
		{
			return $(this).data('fasttable').availableContent;
		},

		checkedRows : function()
		{
			return $(this).data('fasttable').checkedRows;
		},

		checkedCount : function()
		{
			return $(this).data('fasttable').checkedCount;
		},

		pageCheckedCount : function()
		{
			return $(this).data('fasttable').pageCheckedCount;
		},

		checkRow : function(id, checked)
		{
			checkRow($(this).data('fasttable'), id, checked);
		},

		processShortcut : function(key)
		{
			return processShortcut($(this).data('fasttable'), key);
		},
	};

	function updateContent(content)
	{
		var data = $(this).data('fasttable');
		if (content)
		{
			data.content = content;
		}
		refresh(data);
		blinkMovedRecords(data);
	}

	function applyFilter(data, filter)
	{
		data.lastFilter = filter;
		if (data.content)
		{
			data.curPage = 1;
			data.hasFilter = filter !== '';
			data.searcher.compile(filter);
			refresh(data);
		}
		if (filter !== '' && data.config.filterInputCallback)
		{
			data.config.filterInputCallback(filter);
		}
		if (filter === '' && data.config.filterClearCallback)
		{
			data.config.filterClearCallback();
		}
	}

	function refresh(data)
	{
		refilter(data);
		validateChecks(data);
		updatePager(data);
		updateInfo(data);
		updateSelector(data);
		updateTable(data);
	}

	function refilter(data)
	{
		data.availableContent = [];
		data.filteredContent = [];
		for (var i = 0; i < data.content.length; i++)
		{
			var item = data.content[i];
			if (data.hasFilter && item.search === undefined && data.config.fillSearchCallback)
			{
				data.config.fillSearchCallback(item);
			}

			if (!data.hasFilter || data.searcher.exec(item.data))
			{
				data.availableContent.push(item);
				if (!data.config.filterCallback || data.config.filterCallback(item))
				{
					data.filteredContent.push(item);
				}
			}
		}
	}

	function updateTable(data)
	{
		var oldTable = data.target[0];
		var newTable = buildTBody(data);
		updateTBody(data, oldTable, newTable);
	}

	function buildTBody(data)
	{
		var table = $('<table><tbody></tbody></table>')[0];
		for (var i=0; i < data.pageContent.length; i++)
		{
			var item = data.pageContent[i];

			var row = table.insertRow(table.rows.length);

			row.fasttableID = item.id;
			if (data.checkedRows[item.id])
			{
				row.className = 'checked';
			}
			if (data.config.renderRowCallback)
			{
				data.config.renderRowCallback(row, item);
			}

			if (!item.fields)
			{
				if (data.config.fillFieldsCallback)
				{
					data.config.fillFieldsCallback(item);
				}
				else
				{
					item.fields = [];
				}
			}

			for (var j=0; j < item.fields.length; j++)
			{
				var cell = row.insertCell(row.cells.length);
				cell.innerHTML = item.fields[j];
				if (data.config.renderCellCallback)
				{
					data.config.renderCellCallback(cell, j, item);
				}
			}
		}

		titleCheckRedraw(data);

		if (data.config.renderTableCallback)
		{
			data.config.renderTableCallback(table);
		}

		return table;
	}

	function updateTBody(data, oldTable, newTable)
	{
		var headerRows = $('thead > tr', oldTable).length;
		var oldTRs = oldTable.rows;
		var newTRs = newTable.rows;
		var oldTBody = $('tbody', oldTable)[0];
		var oldTRsLength = oldTRs.length - headerRows; // evlt. skip header row
		var newTRsLength = newTRs.length;

		for (var i=0; i < newTRs.length; )
		{
			var newTR = newTRs[i];

			if (i < oldTRsLength)
			{
				// update existing row
				var oldTR = oldTRs[i + headerRows]; // evlt. skip header row
				var oldTDs = oldTR.cells;
				var newTDs = newTR.cells;

				oldTR.className = newTR.className;
				oldTR.fasttableID = newTR.fasttableID;

				for (var j=0, n = 0; j < oldTDs.length; j++, n++)
				{
					var oldTD = oldTDs[j];
					var newTD = newTDs[n];
					var oldHtml = oldTD.outerHTML;
					var newHtml = newTD.outerHTML;
					if (oldHtml !== newHtml)
					{
						oldTR.replaceChild(newTD, oldTD);
						n--;
					}
				}
				i++;
			}
			else
			{
				// add new row
				oldTBody.appendChild(newTR);
			}
		}

		var maxTRs = newTRsLength + headerRows; // evlt. skip header row;
		while (oldTRs.length > maxTRs)
		{
			oldTable.deleteRow(oldTRs.length - 1);
		}
	}

	function updatePager(data)
	{
		data.pageCount = Math.ceil(data.filteredContent.length / data.pageSize);
		if (data.curPage < 1)
		{
			data.curPage = 1;
		}
		if (data.curPage > data.pageCount)
		{
			data.curPage = data.pageCount;
		}

		var startIndex = (data.curPage - 1) * data.pageSize;
		data.pageContent = data.filteredContent.slice(startIndex, startIndex + data.pageSize);

		var pagerObj = data.config.pagerContainer;
		var pagerHtml = buildPagerHtml(data);

		var oldPager = pagerObj[0];
		var newPager = $(pagerHtml)[0];

		updatePagerContent(data, oldPager, newPager);
	}

	function buildPagerHtml(data)
	{
		var iListLength = data.maxPages;
		var iStart, iEnd, iHalf = Math.floor(iListLength/2);

		if (data.pageCount < iListLength)
		{
			iStart = 1;
			iEnd = data.pageCount;
		}
		else if (data.curPage -1 <= iHalf)
		{
			iStart = 1;
			iEnd = iListLength;
		}
		else if (data.curPage - 1 >= (data.pageCount-iHalf))
		{
			iStart = data.pageCount - iListLength + 1;
			iEnd = data.pageCount;
		}
		else
		{
			iStart = data.curPage - 1 - iHalf + 1;
			iEnd = iStart + iListLength - 1;
		}

		var pager = '<ul>';
		pager += '<li' + (data.curPage === 1 || data.curPage === 0 ? ' class="disabled"' : '') +
			'><a href="#" title="Previous page' + (data.config.shortcuts ? ' [Left]' : '') + '">&larr; Prev</a></li>';

		if (iStart > 1)
		{
			pager += '<li><a href="#"' + (data.config.shortcuts ? ' title="First page [Shift+Left]"' : '') + '>1</a></li>';
			if (iStart > 2 && data.pageDots)
			{
				pager += '<li class="disabled"><a href="#">&#133;</a></li>';
			}
		}

		for (var j=iStart; j<=iEnd; j++)
		{
			pager += '<li' + ((j===data.curPage) ? ' class="active"' : '') +
				'><a href="#"' +
				(data.config.shortcuts && j === 1 ? ' title="First page [Shift+Left]"' :
				 data.config.shortcuts && j === data.pageCount ? ' title="Last page [Shift+Right]"' : '') +
				'>' + j + '</a></li>';
		}

		if (iEnd != data.pageCount)
		{
			if (iEnd < data.pageCount - 1 && data.pageDots)
			{
				pager += '<li class="disabled"><a href="#">&#133;</a></li>';
			}
			pager += '<li><a href="#"' + (data.config.shortcuts ? ' title="Last page [Shift+Right]"' : '') + '>' + data.pageCount + '</a></li>';
		}

		pager += '<li' + (data.curPage === data.pageCount || data.pageCount === 0 ? ' class="disabled"' : '') +
			'><a href="#" title="Next page' + (data.config.shortcuts ? ' [Right]' : '') + '">Next &rarr;</a></li>';
		pager += '</ul>';

		return pager;
	}

	function updatePagerContent(data, oldPager, newPager)
	{
		var oldLIs = oldPager.getElementsByTagName('li');
		var newLIs = newPager.getElementsByTagName('li');

		var oldLIsLength = oldLIs.length;
		var newLIsLength = newLIs.length;

		for (var i=0, n=0; i < newLIs.length; i++, n++)
		{
			var newLI = newLIs[i];

			if (n < oldLIsLength)
			{
				// update existing LI
				var oldLI = oldLIs[n];

				var oldHtml = oldLI.outerHTML;
				var newHtml = newLI.outerHTML;
				if (oldHtml !== newHtml)
				{
					oldPager.replaceChild(newLI, oldLI);
					i--;
				}
			}
			else
			{
				// add new LI
				oldPager.appendChild(newLI);
				i--;
			}
		}

		while (oldLIs.length > newLIsLength)
		{
			oldPager.removeChild(oldPager.lastChild);
		}
	}

	function updateInfo(data)
	{
		if (data.content.length === 0)
		{
			var infoText = data.config.infoEmpty;
		}
		else if (data.curPage === 0)
		{
			var infoText = 'No matching records found (total ' + data.content.length + ')';
		}
		else
		{
			var firstRecord = (data.curPage - 1) * data.pageSize + 1;
			var lastRecord = firstRecord + data.pageContent.length - 1;
			var infoText = 'Showing records ' + firstRecord + '-' + lastRecord + ' from ' + data.filteredContent.length;
			if (data.filteredContent.length != data.content.length)
			{
				infoText += ' filtered (total ' + data.content.length + ')';
			}
		}
		data.config.infoContainer.html(infoText);

		if (data.config.updateInfoCallback)
		{
			data.config.updateInfoCallback({
				total: data.content.length,
				available: data.availableContent.length,
				filtered: data.filteredContent.length,
				firstRecord: firstRecord,
				lastRecord: lastRecord
			});
		}
	}

	function updateSelector(data)
	{
		data.pageCheckedCount = 0;
		if (data.checkedCount > 0 && data.filteredContent.length > 0)
		{
			for (var i = (data.curPage - 1) * data.pageSize; i < Math.min(data.curPage * data.pageSize, data.filteredContent.length); i++)
			{
				data.pageCheckedCount += data.checkedRows[data.filteredContent[i].id] ? 1 : 0;
			}
		}
		data.config.selector.css('display', data.pageCheckedCount === data.checkedCount ? 'none' : '');
		if (data.checkedCount !== data.pageCheckedCount)
		{
			data.config.selector.text('' + (data.checkedCount - data.pageCheckedCount) +
				(data.checkedCount - data.pageCheckedCount > 1 ? ' records' : ' record') +
				' selected on other pages');
		}
	}

	function setPageSize(pageSize, maxPages, pageDots)
	{
		var data = $(this).data('fasttable');
		data.pageSize = parseInt(pageSize);
		data.curPage = 1;
		if (maxPages !== undefined)
		{
			data.maxPages = maxPages;
		}
		if (pageDots !== undefined)
		{
			data.pageDots = pageDots;
		}
		refresh(data);
	}

	function setCurPage(page)
	{
		var data = $(this).data('fasttable');
		data.curPage = parseInt(page);
		refresh(data);
	}

	function checkedIds(data)
	{
		var checkedRows = data.checkedRows;
		var checkedIds = [];
		for (var i = 0; i < data.content.length; i++)
		{
			var id = data.content[i].id;
			if (checkedRows[id])
			{
				checkedIds.push(id);
			}
		}
		return checkedIds;
	}

	function titleCheckRedraw(data)
	{
		var filteredContent = data.filteredContent;
		var checkedRows = data.checkedRows;

		var hasSelectedItems = false;
		var hasUnselectedItems = false;
		for (var i = 0; i < filteredContent.length; i++)
		{
			if (checkedRows[filteredContent[i].id])
			{
				hasSelectedItems = true;
			}
			else
			{
				hasUnselectedItems = true;
			}
		}

		var headerRow = $('thead > tr', data.target);
		if (hasSelectedItems && hasUnselectedItems)
		{
			headerRow.removeClass('checked').addClass('checkremove');
		}
		else if (hasSelectedItems)
		{
			headerRow.removeClass('checkremove').addClass('checked');
		}
		else
		{
			headerRow.removeClass('checked').removeClass('checkremove');
		}
	}

	function itemCheckClick(data, event)
	{
		var checkmark = $(event.target).hasClass('check');
		if (data.dragging || (!checkmark && !data.config.rowSelect))
		{
			return;
		}

		var row = $(event.target).closest('tr', data.target)[0];
		var id = row.fasttableID;
		var doToggle = true;
		var checkedRows = data.checkedRows;

		if (event.shiftKey && data.lastClickedRowID != null)
		{
			var checked = checkedRows[id];
			doToggle = !checkRange(data, id, data.lastClickedRowID, !checked);
		}

		if (doToggle)
		{
			toggleCheck(data, id);
		}

		data.lastClickedRowID = id;

		refresh(data);
	}

	function titleCheckClick(data, event)
	{
		var checkmark = $(event.target).hasClass('check');
		if (data.dragging || (!checkmark && !data.config.rowSelect))
		{
			return;
		}

		var filteredContent = data.filteredContent;
		var checkedRows = data.checkedRows;

		var hasSelectedItems = false;
		for (var i = 0; i < filteredContent.length; i++)
		{
			if (checkedRows[filteredContent[i].id])
			{
				hasSelectedItems = true;
				break;
			}
		}

		data.lastClickedRowID = null;
		checkAll(data, !hasSelectedItems);
	}

	function toggleCheck(data, id)
	{
		var checkedRows = data.checkedRows;
		if (checkedRows[id])
		{
			checkedRows[id] = undefined;
			data.checkedCount--;
		}
		else
		{
			checkedRows[id] = true;
			data.checkedCount++;
		}
	}

	function checkAll(data, checked)
	{
		var filteredContent = data.filteredContent;

		for (var i = 0; i < filteredContent.length; i++)
		{
			checkRow(data, filteredContent[i].id, checked);
		}

		refresh(data);
	}

	function checkRange(data, from, to, checked)
	{
		var filteredContent = data.filteredContent;
		var indexFrom = indexOfID(filteredContent, from);
		var indexTo = indexOfID(filteredContent, to);
		if (indexFrom === -1 || indexTo === -1)
		{
			return false;
		}

		if (indexTo < indexFrom)
		{
			var tmp = indexTo; indexTo = indexFrom; indexFrom = tmp;
		}

		for (var i = indexFrom; i <= indexTo; i++)
		{
			checkRow(data, filteredContent[i].id, checked);
		}

		return true;
	}

	function checkRow(data, id, checked)
	{
		if (checked)
		{
			if (!data.checkedRows[id])
			{
				data.checkedCount++;
			}
			data.checkedRows[id] = true;
		}
		else
		{
			if (data.checkedRows[id])
			{
				data.checkedCount--;
			}
			data.checkedRows[id] = undefined;
		}
	}

	function indexOfID(content, id)
	{
		for (var i = 0; i < content.length; i++)
		{
			if (id === content[i].id)
			{
				return i;
			}
		}
		return -1;
	}

	function validateChecks(data)
	{
		var checkedRows = data.checkedRows;
		data.checkedRows = {}
		data.checkedCount = 0;
		for (var i = 0; i < data.content.length; i++)
		{
			if (checkedRows[data.content[i].id])
			{
				data.checkedRows[data.content[i].id] = true;
				data.checkedCount++;
			}
		}
	}

	//*************** DRAG-N-DROP

	function initDragDrop(data)
	{
		data.target[0].addEventListener('mousedown', function(e) { mouseDown(data, e); }, true);
		data.target[0].addEventListener('touchstart', function(e) { mouseDown(data, e); }, true);

		data.moveIds = [];
		data.dropAfter = false;
		data.dropId = null;
		data.dragging = false;
		data.dragRow = $('');
		data.cancelDrag = false;
		data.downPos = null;
		data.blinkIds = [];
		data.blinkState = null;
		data.wantBlink = false;
	}

	function touchToMouse(e)
	{
		if (e.type === 'touchstart' || e.type === 'touchmove' || e.type === 'touchend')
		{
			e.clientX = e.changedTouches[0].clientX;
			e.clientY = e.changedTouches[0].clientY;
		}
	}

	function mouseDown(data, e)
	{
		data.dragging = false;
		data.dropId = null;
		data.dragRow = $(e.target).closest('tr', data.target);

		var checkmark = $(e.target).hasClass('check') ||
			($(e.target).find('.check').length > 0 && !$('body').hasClass('phone'));
		var head = $(e.target).closest('tr', data.target).parent().is('thead');
		if (head || !(checkmark || (data.config.rowSelect && e.type === 'mousedown')) ||
			data.dragRow.length != 1 || e.ctrlKey || e.altKey || e.metaKey)
		{
			return;
		}

		touchToMouse(e);
		if (e.type === 'mousedown')
		{
			e.preventDefault();
		}

		if (!data.config.dragEndCallback)
		{
			return;
		}

		data.downPos = { x: e.clientX, y: e.clientY };

		data.mouseMove = function(e) { mouseMove(data, e); };
		data.mouseUp = function(e) { mouseUp(data, e); };
		data.keyDown = function(e) { keyDown(data, e); };
		document.addEventListener('mousemove', data.mouseMove, true);
		document.addEventListener('touchmove', data.mouseMove, true);
		document.addEventListener('mouseup', data.mouseUp, true);
		document.addEventListener('touchend', data.mouseUp, true);
		document.addEventListener('touchcancel', data.mouseUp, true);
		document.addEventListener('keydown', data.keyDown, true);
	}

	function mouseMove(data, e)
	{
		touchToMouse(e);
		e.preventDefault();

		if (e.touches && e.touches.length > 1)
		{
			data.cancelDrag = true;
			mouseUp(data, e);
			return;
		}

		if (!data.dragging)
		{
			if (Math.abs(data.downPos.x - e.clientX) < 5 &&
				Math.abs(data.downPos.y - e.clientY) < 5)
			{
				return;
			}
			startDrag(data, e);
			if (data.dragCancel)
			{
				mouseUp(data, e);
				return;
			}
		}

		updateDrag(data, e.clientX, e.clientY);
		autoScroll(data, e.clientX, e.clientY);
	}

	function startDrag(data, e)
	{
		if (data.config.dragStartCallback)
		{
			data.config.dragStartCallback();
		}

		var offsetX = $(document).scrollLeft();
		var offsetY = $(document).scrollTop();
		var rf = data.dragRow.offset();
		data.dragOffset = { x: data.downPos.x - rf.left + offsetX,
			y: Math.min(Math.max(data.downPos.y - rf.top + offsetY, 0), data.dragRow.height()) };

		var checkedRows = data.checkedRows;
		var chkIds = checkedIds(data);
		var id = data.dragRow[0].fasttableID;
		data.moveIds = checkedRows[id] ? chkIds : [id];
		data.dragging = true;
		data.cancelDrag = false;

		buildDragBox(data);
		data.config.dragBox.css('display', 'block');
		data.dragRow.addClass('drag-source');
		$('html').addClass('drag-progress');
		data.oldOverflowX = $('body').css('overflow-x');
		$('body').css('overflow-x', 'hidden');
	}

	function buildDragBox(data)
	{
		var tr = data.dragRow.clone();
		var table = data.target.clone();
		$('tr', table).remove();
		$('thead', table).remove();
		$('tbody', table).append(tr);
		table.css('margin', 0);
		data.config.dragContent.html(table);
		data.config.dragBadge.text(data.moveIds.length);
		data.config.dragBadge.css('display', data.moveIds.length > 1 ? 'block' : 'none');
		data.config.dragBox.css({left: data.target.offset().left, width: data.dragRow.width()});
		var tds = $('td', tr);
		$('td', data.dragRow).each(function(ind, el) { $(tds[ind]).css('width', $(el).width()); });
	}

	function updateDrag(data, x, y)
	{
		var offsetX = $(document).scrollLeft();
		var offsetY = $(document).scrollTop();
		var posX = x + offsetX;
		var posY = y + offsetY;

		data.config.dragBox.css({
			left: posX - data.dragOffset.x,
			top: Math.max(Math.min(posY - data.dragOffset.y, offsetY + $(window).height() - data.config.dragBox.height() - 2), offsetY + 2)});

		var dt = data.config.dragBox.offset().top;
		var dh = data.config.dragBox.height();

		var rows = $('tbody > tr', data.target);
		for (var i = 0; i < rows.length; i++)
		{
			var row = $(rows[i]);
			var rt = row.offset().top;
			var rh = row.height();
			if (row[0] !== data.dragRow[0])
			{
				if ((dt >= rt && dt <= rt + rh / 2) ||
					(dt < rt && i == 0))
				{
					data.dropAfter = false;
					row.before(data.dragRow);
					data.dropId = row[0].fasttableID;
					break;
				}
				if ((dt + dh >= rt + rh / 2 && dt + dh <= rt + rh) ||
					(dt + dh > rt + rh && i === rows.length - 1))
				{
					data.dropAfter = true;
					row.after(data.dragRow);
					data.dropId = row[0].fasttableID;
					break;
				}
			}
		}

		if (data.dropId === null)
		{
			data.dropId = data.dragRow[0].fasttableID;
			data.dropAfter = true;
		}
	}

	function autoScroll(data, x, y)
	{
		// works properly only if the table lays directly on the page (not in another scrollable div)

		data.scrollStep = (y > $(window).height() - 20 ? 1 : y < 20 ? -1 : 0) * 5;
		if (data.scrollStep !== 0 && !data.scrollTimer)
		{
			var scroll = function()
			{
				$(document).scrollTop($(document).scrollTop() + data.scrollStep);
				updateDrag(data, x, y + data.scrollStep);
				data.scrollTimer = data.scrollStep == 0 ? null : setTimeout(scroll, 10);
			}
			data.scrollTimer = setTimeout(scroll, 500);
		}
	}

	function mouseUp(data, e)
	{
		document.removeEventListener('mousemove', data.mouseMove, true);
		document.removeEventListener('touchmove', data.mouseMove, true);
		document.removeEventListener('mouseup', data.mouseUp, true);
		document.removeEventListener('touchend', data.mouseUp, true);
		document.removeEventListener('touchcancel', data.mouseUp, true);
		document.removeEventListener('keydown', data.keyDown, true);

		if (!data.dragging)
		{
			return;
		}

		data.dragging = false;
		data.cancelDrag = data.cancelDrag || e.type === 'touchcancel';
		data.dragRow.removeClass('drag-source');
		$('html').removeClass('drag-progress');
		$('body').css('overflow-x', data.oldOverflowX);
		data.config.dragBox.hide();
		data.scrollStep = 0;
		clearTimeout(data.scrollTimer);
		data.scrollTimer = null;
		moveRecords(data);
	}

	function keyDown(data, e)
	{
		if (e.keyCode == 27) // ESC-key
		{
			data.cancelDrag = true;
			e.preventDefault();
			mouseUp(data, e);
		}
	}

	function moveRecords(data)
	{
		if (data.dropId !== null && !data.cancelDrag &&
			!(data.moveIds.length == 1 && data.dropId == data.moveIds[0]))
		{
			data.blinkIds = data.moveIds;
			data.moveIds = [];
			data.blinkState = data.config.dragBlink === 'none' ? 0 : 3;
			data.wantBlink = data.blinkState > 0;
			moveRows(data);
		}
		else
		{
			data.dropId = null;
		}

		if (data.dropId === null)
		{
			data.moveIds = [];
		}

		refresh(data);

		data.config.dragEndCallback(data.dropId !== null ?
			{
				ids: data.blinkIds,
				position: data.dropId,
				direction: data.dropAfter ? 'after' : 'before'
			} : null);

		if (data.config.dragBlink === 'direct')
		{
			data.target.fasttable('update');
		}
	}

	function moveRows(data)
	{
		var movedIds = data.blinkIds;
		var movedRecords = [];

		for (var i = 0; i < data.content.length; i++)
		{
			var item = data.content[i];
			if (movedIds.indexOf(item.id) > -1)
			{
				movedRecords.push(item);
				data.content.splice(i, 1);
				i--;

				if (item.id === data.dropId)
				{
					if (i >= 0)
					{
						data.dropId = data.content[i].id;
						data.dropAfter = true;
					}
					else if (i + 1 < data.content.length)
					{
						data.dropId = data.content[i + 1].id;
						data.dropAfter = false;
					}
					else
					{
						data.dropId = null;
					}
				}
			}
		}

		if (data.dropId === null)
		{
			// restore content
			for (var j = 0; j < movedRecords.length; j++)
			{
				data.content.push(movedRecords[j]);
			}
			return;
		}

		for (var i = 0; i < data.content.length; i++)
		{
			if (data.content[i].id === data.dropId)
			{
				for (var j = movedRecords.length - 1; j >= 0; j--)
				{
					data.content.splice(data.dropAfter ? i + 1 : i, 0, movedRecords[j]);
				}
				break;
			}
		}
	}

	function blinkMovedRecords(data)
	{
		if (data.blinkIds.length > 0)
		{
			blinkProgress(data, data.wantBlink);
			data.wantBlink = false;
		}
	}

	function blinkProgress(data, recur)
	{
		var rows = $('tr', data.target);
		rows.removeClass('drag-finish');
		rows.each(function(ind, el)
			{
				var id = el.fasttableID;
				if (data.blinkIds.indexOf(id) > -1 &&
					(data.blinkState === 1 || data.blinkState === 3 || data.blinkState === 5))
				{
					$(el).addClass('drag-finish');
				}
			});

		if (recur && data.blinkState > 0)
		{
			setTimeout(function()
				{
					data.blinkState -= 1;
					blinkProgress(data, true);
				},
				150);
		}

		if (data.blinkState === 0)
		{
			data.blinkIds = [];
		}
	}

	//*************** KEYBOARD

	function processShortcut(data, key)
	{
		switch (key)
		{
			case 'Left': data.curPage = Math.max(data.curPage - 1, 1); refresh(data); return true;
			case 'Shift+Left': data.curPage = 1; refresh(data); return true;
			case 'Right': data.curPage = Math.min(data.curPage + 1, data.pageCount); refresh(data); return true;
			case 'Shift+Right': data.curPage = data.pageCount; refresh(data); return true;
			case 'Shift+F': data.config.filterInput.focus(); return true;
			case 'Shift+C': data.config.filterClearButton.click(); return true;
		}
	}

	//*************** CONFIG

	var defaults =
	{
		filterInput: '#TableFilter',
		filterClearButton: '#TableClear',
		pagerContainer: '#TablePager',
		infoContainer: '#TableInfo',
		dragBox: '#TableDragBox',
		dragContent: '#TableDragContent',
		dragBadge: '#TableDragBadge',
		dragBlink: 'none', // none, direct, update
		pageSize: 10,
		maxPages: 5,
		pageDots: true,
		rowSelect: false,
		shortcuts: false,
		infoEmpty: 'No records',
		renderRowCallback: undefined,
		renderCellCallback: undefined,
		renderTableCallback: undefined,
		fillFieldsCallback: undefined,
		updateInfoCallback: undefined,
		filterInputCallback: undefined,
		filterClearCallback: undefined,
		fillSearchCallback: undefined,
		filterCallback: undefined,
		dragStartCallback: undefined,
		dragEndCallback: undefined
	};

})(jQuery);

function FastSearcher()
{
	'use strict';

	this.source;
	this.len = 0;
	this.p = 0;

	this.initLexer = function(source)
	{
		this.source = source;
		this.len = source.length;
		this.p = 0;
	}

	this.nextToken = function()
	{
		while (this.p < this.len)
		{
			var ch = this.source[this.p++];
			switch (ch) {
				case ' ':
				case '\t':
					continue;

				case '-':
				case '(':
				case ')':
				case '|':
					return ch;

				default:
					this.p--;
					var token = '';
					var quote = false;
					while (this.p < this.len)
					{
						var ch = this.source[this.p++];
						if (quote)
						{
							if (ch === '"')
							{
								quote = false;
								ch = '';
							}
						}
						else
						{
							if (ch === '"')
							{
								quote = true;
								ch = '';
							}
							else if (' \t()|'.indexOf(ch) > -1)
							{
								this.p--;
								return token;
							}
						}
						token += ch;
					}
					return token;
			}
		}
		return null;
	}

	this.compile = function(searchstr)
	{
		var _this = this;
		this.initLexer(searchstr);

		function expression(greedy)
		{
			var node = null;
			while (true)
			{
				var token = _this.nextToken();
				var node2 = null;
				switch (token)
				{
					case null:
					case ')':
						return node;

					case '-':
						node2 = expression(false);
						node2 = node2 ? _this.not(node2) : node2;
						break;

					case '(':
						node2 = expression(true);
						break;

					case '|':
						node2 = expression(false);
						break;

					default:
						node2 = _this.term(token);
				}

				if (node && node2)
				{
					node = token === '|' ? _this.or(node, node2) : _this.and(node, node2);
				}
				else if (node2)
				{
					node = node2;
				}

				if (!greedy && node)
				{
					return node;
				}
			}
		}

		this.root = expression(true);
	}

	this.root = null;
	this.data = null;

	this.exec = function(data) {
		this.data = data;
		return this.root ? this.root.eval() : true;
	}

	this.and = function(L, R) {
		return {
			L: L, R: R,
			eval: function() { return this.L.eval() && this.R.eval(); }
		};
	}

	this.or = function(L, R) {
		return {
			L: L, R: R,
			eval: function() { return this.L.eval() || this.R.eval(); }
		};
	}

	this.not = function(M) {
		return {
			M: M,
			eval: function() { return !this.M.eval();}
		};
	}

	this.term = function(term) {
		return this.compileTerm(term);
	}

	var COMMANDS = [ ':', '>=', '<=', '<>', '>', '<', '=' ];

	this.compileTerm = function(term) {
		var _this = this;
		var text = term.toLowerCase();
		var field;

		var command;
		var commandIndex;
		for (var i = 0; i < COMMANDS.length; i++)
		{
			var cmd = COMMANDS[i];
			var p = term.indexOf(cmd);
			if (p > -1 && (p < commandIndex || commandIndex === undefined))
			{
				commandIndex = p;
				command = cmd;
			}
		}

		if (command !== undefined)
		{
			field = term.substring(0, commandIndex);
			text = text.substring(commandIndex + command.length);
		}

		return {
			command: command,
			text: text,
			field: field,
			eval: function() { return _this.evalTerm(this); }
		};
	}

	this.evalTerm = function(term) {
		var text = term.text;
		var field = term.field;
		var content = this.fieldValue(this.data, field);

		if (content === undefined)
		{
			return false;
		}

		switch (term.command)
		{
			case undefined:
			case ':':
				return content.toString().toLowerCase().indexOf(text) > -1;
			case '=':
				return content.toString().toLowerCase() == text;
			case '<>':
				return content.toString().toLowerCase() != text;
			case '>':
				return parseInt(content) > parseInt(text);
			case '>=':
				return parseInt(content) >= parseInt(text);
			case '<':
				return parseInt(content) < parseInt(text);
			case '<=':
				return parseInt(content) <= parseInt(text);
			default:
				return false;
		}
	}

	this.fieldValue = function(data, field) {
		var value = '';
		if (field !== undefined)
		{
			value = data[field];
			if (value === undefined)
			{
				if (this.nameMap === undefined)
				{
					this.buildNameMap(data);
				}
				value = data[this.nameMap[field.toLowerCase()]];
			}
		}
		else
		{
			if (data._search === true)
			{
				for (var prop in data)
				{
					value += ' ' + data[prop];
				}
			}
			else
			{
				for (var i = 0; i < data._search.length; i++)
				{
					value += ' ' + data[data._search[i]];
				}
			}
		}
		return value;
	}

	this.nameMap = undefined;
	this.buildNameMap = function(data)
	{
		this.nameMap = {};
		for (var prop in data)
		{
			this.nameMap[prop.toLowerCase()] = prop;
		}
	}
}
