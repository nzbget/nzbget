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
 *     3) search/filtering.
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
					config.headerCheck = $(config.headerCheck);
					
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
					
					$this.data('fasttable', {
							target : $this,
							config : config,
							pageSize : parseInt(config.pageSize),
							maxPages : parseInt(config.maxPages),
							pageDots : Util.parseBool(config.pageDots),
							curPage : 1,
							checkedRows: {},
							checkedCount: 0,
							lastClickedRowID: null,
							searcher: searcher
						});
				}
			});
		},

		destroy : function()
		{
			return this.each(function()
			{
				var $this = $(this);
				var data = $this.data('fasttable');

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

		checkRow : function(id, checked)
		{
			checkRow($(this).data('fasttable'), id, checked);
		},
		
		itemCheckClick : itemCheckClick,
		
		titleCheckClick : titleCheckClick
	};

	function updateContent(content)
	{
		var data = $(this).data('fasttable');
		if (content)
		{
			data.content = content;
		}
		refresh(data);
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
		var oldTRs = oldTable.rows;
		var newTRs = newTable.rows;
		var oldTBody = $('tbody', oldTable)[0];
		var oldTRsLength = oldTRs.length - (data.config.hasHeader ? 1 : 0); // evlt. skip header row
		var newTRsLength = newTRs.length;

		for (var i=0; i < newTRs.length; )
		{
			var newTR = newTRs[i];

			if (i < oldTRsLength)
			{
				// update existing row
				var oldTR = oldTRs[i + (data.config.hasHeader ? 1 : 0)]; // evlt. skip header row
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

		var maxTRs = newTRsLength + (data.config.hasHeader ? 1 : 0); // evlt. skip header row;
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
		pager += '<li' + (data.curPage === 1 || data.curPage === 0 ? ' class="disabled"' : '') + '><a href="#">&larr; Prev</a></li>';

		if (iStart > 1)
		{
			pager += '<li><a href="#">1</a></li>';
			if (iStart > 2 && data.pageDots)
			{
				pager += '<li class="disabled"><a href="#">&#133;</a></li>';
			}
		}

		for (var j=iStart; j<=iEnd; j++)
		{
			pager += '<li' + ((j===data.curPage) ? ' class="active"' : '') + '><a href="#">' + j + '</a></li>';
		}

		if (iEnd != data.pageCount)
		{
			if (iEnd < data.pageCount - 1 && data.pageDots)
			{
				pager += '<li class="disabled"><a href="#">&#133;</a></li>';
			}
			pager += '<li><a href="#">' + data.pageCount + '</a></li>';
		}

		pager += '<li' + (data.curPage === data.pageCount || data.pageCount === 0 ? ' class="disabled"' : '') + '><a href="#">Next &rarr;</a></li>';
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
		
		if (hasSelectedItems && hasUnselectedItems)
		{
			data.config.headerCheck.removeClass('checked').addClass('checkremove');
		}
		else if (hasSelectedItems)
		{
			data.config.headerCheck.removeClass('checkremove').addClass('checked');
		}
		else
		{
			data.config.headerCheck.removeClass('checked').removeClass('checkremove');
		}
	}

	function itemCheckClick(row, event)
	{
		var data = $(this).data('fasttable');
		var checkedRows = data.checkedRows;

		var id = row.fasttableID;
		var doToggle = true;

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

	function titleCheckClick()
	{
		var data = $(this).data('fasttable');
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
		var index = checkedRows[id];
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
		var filteredContent = data.filteredContent;
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
	
	var defaults =
	{
		filterInput: '#table-filter',
		filterClearButton: '#table-clear',
		pagerContainer: '#table-pager',
		infoContainer: '#table-info',
		pageSize: 10,
		maxPages: 5,
		pageDots: true,
		hasHeader: true,
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
		headerCheck: '#table-header-check'
	};

})(jQuery);

function FastSearcher()
{
	'use strict';

	this.source;
	this.len;
	this.p;

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

	this.nameMap;
	this.buildNameMap = function(data)
	{
		this.nameMap = {};
		for (var prop in data)
		{
			this.nameMap[prop.toLowerCase()] = prop;
		}
	}
}
