/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2012 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

(function($) {

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
		init : function( options )
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

					config = {};
					config = $.extend(config, defaults, options);

					// Create a timer which gets reset upon every keyup event.
					// Perform filter only when the timer's wait is reached (user finished typing or paused long enough to elapse the timer).
					// Do not perform the filter is the query has not changed.
					// Immediately perform the filter if the ENTER key is pressed.

					var timer;

					$(config.filterInput).keyup(function()
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
							var value = inputBox.value;

							if ((value != inputBox.lastValue) || (overrideBool))
							{
								inputBox.lastValue = value;
								var data = $this.data('fasttable');
								if (data.content)
								{
									data.curPage = 1;
									refresh(data);
								}
							}
						};

						// Reset the timer
						clearTimeout(timer);
						timer = setTimeout(timerCallback, timerWait);

						return false;
					});

					$(config.filterClearButton).click(function()
					{
						var data = $this.data('fasttable');
						$(data.config.filterInput).val('');
						refresh(data);
					});
						
					$(config.pagerContainer).on('click', 'li', function (e)
					{
						e.preventDefault();
						var data = $this.data('fasttable');
						pageNum = $(this).text();
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
							pageDots : parseBool(config.pageDots),
							curPage : 1,
							checkedRows: [],
							lastClickedRowID: 0
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
		
		filteredContent : function()
		{
			return $(this).data('fasttable').filteredContent;
		},

		checkedRows : function()
		{
			return $(this).data('fasttable').checkedRows;
		},
		
		itemCheckClick : itemCheckClick,
		
		titleCheckClick : titleCheckClick
	};

	function has_words(str, words, caseSensitive)
	{
		var text = caseSensitive ? str : str.toLowerCase();

		for (var i = 0; i < words.length; i++)
		{
			if (text.indexOf(words[i]) === -1)
				return false;
		}

		return true;
	}

	function updateContent(content)
	{
		var data = $(this).data('fasttable');
		if (content)
		{
			data.content = content;
		}
		refresh(data);
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
		filterInput = $(data.config.filterInput);
		var phrase = filterInput.length > 0 ? filterInput.val() : '';
		var caseSensitive = data.config.filterCaseSensitive;
		var words = caseSensitive ? phrase.split(' ') : phrase.toLowerCase().split(' ');

		if (words.length === 1 && words[0] === '')
		{
			data.filteredContent = data.content;
		}
		else
		{
			data.filteredContent = [];
			for (var i = 0; i < data.content.length; i++)
			{
				var item = data.content[i];
				if (has_words(item.search, words, caseSensitive))
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
			if (data.checkedRows.indexOf(item.id) > -1)
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

		var pagerObj = $(data.config.pagerContainer);
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
			if (data.filteredContent != data.content)
			{
				infoText += ' filtered (total ' + data.content.length + ')';
			}
		}
		$(data.config.infoContainer).html(infoText);
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
			if (checkedRows.indexOf(filteredContent[i].id) === -1)
			{
				hasUnselectedItems = true;
			}
			else
			{
				hasSelectedItems = true;
			}
		}
		
		if (hasSelectedItems && hasUnselectedItems)
		{
			$(data.config.headerCheck).removeClass('checked').addClass('checkremove');
		}
		else if (hasSelectedItems)
		{
			$(data.config.headerCheck).removeClass('checkremove').addClass('checked');
		}
		else
		{
			$(data.config.headerCheck).removeClass('checked').removeClass('checkremove');
		}
	}

	function itemCheckClick(row, event)
	{
		var data = $(this).data('fasttable');
		var checkedRows = data.checkedRows;

		var id = row.fasttableID;
		var doToggle = true;

		if (event.shiftKey && data.lastClickedRowID > 0)
		{
			var checked = checkedRows.indexOf(id) > -1;
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
			if (checkedRows.indexOf(filteredContent[i].id) > -1)
			{
				hasSelectedItems = true;
				break;
			}
		}

		data.lastClickedRowID = 0;
		checkAll(data, !hasSelectedItems);
	}

	function toggleCheck(data, id)
	{
		var checkedRows = data.checkedRows;
		var index = checkedRows.indexOf(id);
		if (index > -1)
		{
			checkedRows.splice(index, 1);
		}
		else
		{
			checkedRows.push(id);
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
			if (data.checkedRows.indexOf(id) === -1)
			{
				data.checkedRows.push(id);
			}
		}
		else
		{
			var index = data.checkedRows.indexOf(id);
			if (index > -1)
			{
				data.checkedRows.splice(index, 1);
			}
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

		var ids = [];
		for (var i = 0; i < data.content.length; i++)
		{
			ids.push(data.content[i].id);
		}

		for (var i = 0; i < checkedRows.length; i++)
		{
			if (ids.indexOf(checkedRows[i]) === -1)
			{
				checkedRows.splice(i, 1);
				i--;
			}
		}
	}
	
	var defaults =
	{
		filterInput: '#table-filter',
		filterClearButton: '#table-clear',
		filterCaseSensitive: false,
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
		headerCheck: '#table-header-check'
	};

})(jQuery);