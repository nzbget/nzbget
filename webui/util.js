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
 * In this module:
 *   1) Common utilitiy functions (format time, size, etc);
 *   2) Slideable tab dialog extension;
 *   3) Communication via JSON-RPC.
 */
 
/*** UTILITY FUNCTIONS *********************************************************/

var Util = (new function($)
{
	'use strict';

	this.formatTimeHMS = function(sec)
	{
		var hms = '';
		var days = Math.floor(sec / 86400);
		if (days > 0)
		{
			hms = days	+ 'd ';
		}
		var hours = Math.floor((sec % 86400) / 3600);
		hms = hms + hours + ':';
		var minutes = Math.floor((sec / 60) % 60);
		if (minutes < 10)
		{
			hms = hms + '0';
		}
		hms = hms + minutes + ':';
		var seconds = Math.floor(sec % 60);
		if (seconds < 10)
		{
			hms = hms + '0';
		}
		hms = hms + seconds;
		return hms;
	}

	this.formatTimeLeft = function(sec)
	{
		var hms = '';
		var days = Math.floor(sec / 86400);
		var hours = Math.floor((sec % 86400) / 3600);
		var minutes = Math.floor((sec / 60) % 60);
		var seconds = Math.floor(sec % 60);

		if (days > 10)
		{
			return days + 'd';
		}
		if (days > 0)
		{
			return days + 'd ' + hours + 'h';
		}
		if (hours > 0)
		{
			return hours + 'h ' + (minutes < 10 ? '0' : '') + minutes + 'm';
		}
		if (minutes > 0)
		{
			return minutes + 'm ' + (seconds < 10 ? '0' : '') + seconds + 's';
		}

		return seconds + 's';
	}

	this.formatDateTime = function(unixTime)
	{
		var dt = new Date(unixTime * 1000);
		var h = dt.getHours();
		var m = dt.getMinutes();
		var s = dt.getSeconds();
		return dt.toDateString() + ' ' + (h < 10 ? '0' : '') + h + ':' + (m < 10 ? '0' : '') + m + ':' + (s < 10 ? '0' : '') + s;
	}

	this.formatSizeMB = function(sizeMB, sizeLo)
	{
		if (sizeLo !== undefined && sizeMB < 1024)
		{
			sizeMB = sizeLo / 1024.0 / 1024.0;
		}

		if (sizeMB >= 1024 * 1024 * 100)
		{
			return this.round0(sizeMB / 1024.0 / 1024.0) + '&nbsp;TB';
		}
		else if (sizeMB >= 1024 * 1024 * 10)
		{
			return this.round1(sizeMB / 1024.0 / 1024.0) + '&nbsp;TB';
		}
		else if (sizeMB >= 1024 * 1000)
		{
			return this.round2(sizeMB / 1024.0 / 1024.0) + '&nbsp;TB';
		}
		else if (sizeMB >= 1024 * 100)
		{
			return this.round0(sizeMB / 1024.0) + '&nbsp;GB';
		}
		else if (sizeMB >= 1024 * 10)
		{
			return this.round1(sizeMB / 1024.0) + '&nbsp;GB';
		}
		else if (sizeMB >= 1000)
		{
			return this.round2(sizeMB / 1024.0) + '&nbsp;GB';
		}
		else if (sizeMB >= 100)
		{
			return this.round0(sizeMB) + '&nbsp;MB';
		}
		else if (sizeMB >= 10)
		{
			return this.round1(sizeMB) + '&nbsp;MB';
		}
		else
		{
			return this.round2(sizeMB) + '&nbsp;MB';
		}
	}

	this.formatSpeed = function(bytesPerSec)
	{
		if (bytesPerSec >= 100 * 1024 * 1024)
		{
			return Util.round0(bytesPerSec / 1024.0 / 1024.0) + '&nbsp;MB/s';
		}
		else if (bytesPerSec >= 10 * 1024 * 1024)
		{
			return Util.round1(bytesPerSec / 1024.0 / 1024.0) + '&nbsp;MB/s';
		}
		else if (bytesPerSec >= 1024 * 1000)
		{
			return Util.round2(bytesPerSec / 1024.0 / 1024.0) + '&nbsp;MB/s';
		}
		else
		{
			return Util.round0(bytesPerSec / 1024.0) + '&nbsp;KB/s';
		}
	}
	
	this.formatAge = function(time)
	{
		if (time == 0)
		{
			return '';
		}

		var diff = new Date().getTime() / 1000 - time;
		if (diff > 60*60*24)
		{
			return this.round0(diff / (60*60*24))  +'&nbsp;d';
		}
		else if (diff > 60*60)
		{
			return this.round0(diff / (60*60))  +'&nbsp;h';
		}
		else
		{
			return this.round0(diff / (60))  +'&nbsp;m';
		}
	}

	this.round0 = function(arg)
	{
		return Math.round(arg);
	}

	this.round1 = function(arg)
	{
		return arg.toFixed(1);
	}

	this.round2 = function(arg)
	{
		return arg.toFixed(2);
	}

	this.formatNZBName = function(NZBName)
	{
		return NZBName.replace(/\./g, ' ')
			.replace(/_/g, ' ');
	}

	this.textToHtml = function(str)
	{
		return str.replace(/&/g, '&amp;')
			.replace(/</g, '&lt;')
			.replace(/>/g, '&gt;');
	}

	this.textToAttr = function(str)
	{
		return str.replace(/&/g, '&amp;')
			.replace(/</g, '&lt;')
			.replace(/"/g, '&quot;')
			.replace(/'/g, '&#39;');
	}

	this.setMenuMark = function(menu, data)
	{
		// remove marks from all items
		$('li table tr td:first-child', menu).html('');
		// set mark on selected item
		var mark = $('li[data="mark"]', menu).html();
		$('li[data="' + data + '"] table tr td:first-child', menu).html(mark);
	}

	this.show = function(jqSelector, visible, display)
	{
		if (display)
		{
			$(jqSelector).css({display: visible ? display : 'none'});
		}
		else
		{
			visible ? $(jqSelector).show() : $(jqSelector).hide();
		}
	}

	this.parseBool = function(value)
	{
		return ''+value == 'true';
	}
	
	this.disableShiftMouseDown = function(event)
	{
		// disable default shift+click behaviour, which is to select a text
		if (event.shiftKey)
		{
			event.stopPropagation();
			event.preventDefault();
		}
	}
	
	this.centerDialog = function(dialog, center)
	{
		var $elem = $(dialog);
		if (center)
		{
			var top = ($(window).height() - $elem.outerHeight()) * 0.4;
			top = top > 0 ? top : 0;
			$elem.css({ top: top});
		}
		else
		{
			$elem.css({ top: '' });
		}
	}

	this.parseCommaList = function(commaList)
	{
		var valueList = commaList.split(/[,;]+/);
		for (var i=0; i < valueList.length; i++)
		{
			valueList[i] = valueList[i].trim();
			if (valueList[i] === '')
			{
				valueList.splice(i, 1);
				i--;
			}
		}
		return valueList;
	}

}(jQuery));


/*** MODAL DIALOG WITH SLIDEABLE TABS *********************************************************/

var TabDialog = (new function($)
{
	'use strict';
	
	this.extend = function(dialog)
	{
		dialog.restoreTab = restoreTab;
		dialog.switchTab = switchTab;
		dialog.maximize = maximize;
	}
	
	function maximize(options)
	{
		var bodyPadding = 15;
		var dialog = this;
		var body = $('.modal-body', dialog);
		var footer = $('.modal-footer', dialog);
		var header = $('.modal-header', dialog);
		body.css({top: header.outerHeight(), bottom: footer.outerHeight()});
		if (options.mini)
		{
			var scrollheader = $('.modal-scrollheader', dialog);
			var scroll = $('.modal-inner-scroll', dialog);
			scroll.css('min-height', dialog.height() - header.outerHeight() - footer.outerHeight() - scrollheader.height() - bodyPadding*2);
		}
	}

	function restoreTab()
	{
		var dialog = this;
		var body = $('.modal-body', dialog);
		var footer = $('.modal-footer', dialog);
		var header = $('.modal-header', dialog);
		dialog.css({margin: '', left: '', top: '', bottom: '', right: '', width: '', height: ''});
		body.css({position: '', height: '', left: '', right: '', top: '', bottom: '', 'max-height': ''});
		footer.css({position: '', left: '', right: '', bottom: ''});
	}

	function switchTab(fromTab, toTab, duration, options)
	{
		var dialog = this;
		var sign = options.back ? -1 : 1;
		var fullscreen = options.fullscreen && !options.back;
		var bodyPadding = 15;
		var dialogMargin = options.mini ? 0 : 15;
		var dialogBorder = 2;
		var toggleClass = options.toggleClass ? options.toggleClass : '';

		var body = $('.modal-body', dialog);
		var footer = $('.modal-footer', dialog);
		var header = $('.modal-header', dialog);

		var oldBodyHeight = body.height();
		var oldWinHeight = dialog.height();
		var windowWidth = $(window).width();
		var windowHeight = $(window).height();
		var oldTabWidth = fromTab.width();
		var dialogStyleFS, bodyStyleFS, footerStyleFS;

		if (options.fullscreen && options.back)
		{
			// save fullscreen state for later use
			dialogStyleFS = dialog.attr('style');
			bodyStyleFS = body.attr('style');
			footerStyleFS = footer.attr('style');
			// restore non-fullscreen state to calculate proper destination sizes
			dialog.restoreTab();
		}

		fromTab.hide();
		toTab.show();
		dialog.toggleClass(toggleClass);

		// CONTROL POINT: at this point the destination dialog size is active
		// store destination positions and sizes

		var newBodyHeight = fullscreen ? windowHeight - header.outerHeight() - footer.outerHeight() - dialogMargin*2 - bodyPadding*2 : body.height();
		var newTabWidth = fullscreen ? windowWidth - dialogMargin*2 - dialogBorder - bodyPadding*2 : toTab.width();
		var leftPos = toTab.position().left;
		var newDialogPosition = dialog.position();
		var newDialogWidth = dialog.width();
		var newDialogHeight = dialog.height();
		var newDialogMarginLeft = dialog.css('margin-left');
		var newDialogMarginTop = dialog.css('margin-top');

		// restore source dialog size

		if (options.fullscreen && options.back)
		{
			// restore fullscreen state
			dialog.attr('style', dialogStyleFS);
			body.attr('style', bodyStyleFS);
			footer.attr('style', footerStyleFS);
		}

		body.css({position: '', height: oldBodyHeight});
		dialog.css('overflow', 'hidden');
		fromTab.css({position: 'absolute', left: leftPos, width: oldTabWidth, height: oldBodyHeight});
		toTab.css({position: 'absolute', width: newTabWidth, height: oldBodyHeight, 
			left: sign * ((options.back ? newTabWidth : oldTabWidth) + bodyPadding*2)});
		fromTab.show();
		dialog.toggleClass(toggleClass);

		// animate dialog to destination position and sizes

		if (options.fullscreen && options.back)
		{
			body.css({position: 'absolute'});
			dialog.animate({
					'margin-left': newDialogMarginLeft,
					'margin-top': newDialogMarginTop,
					left: newDialogPosition.left,
					top: newDialogPosition.top,
					right: newDialogPosition.left + newDialogWidth,
					bottom: newDialogPosition.top + newDialogHeight,
					width: newDialogWidth,
					height: newDialogHeight
				},
				duration);

			body.animate({height: newBodyHeight, 'max-height': newBodyHeight}, duration);
		}
		else if (options.fullscreen)
		{
			dialog.css({height: dialog.height()});
			footer.css({position: 'absolute', left: 0, right: 0, bottom: 0});
			dialog.animate({
					margin: dialogMargin,
					left: '0%', top: '0%', bottom: '0%', right: '0%',
					width: windowWidth - dialogMargin*2,
					height: windowHeight - dialogMargin*2
				},
				duration);

			body.animate({height: newBodyHeight, 'max-height': newBodyHeight}, duration);
		}
		else
		{
			body.animate({height: newBodyHeight}, duration);
			dialog.animate({width: newDialogWidth, 'margin-left': newDialogMarginLeft}, duration);
		}

		fromTab.animate({left: sign * -((options.back ? newTabWidth : oldTabWidth) + bodyPadding*2), 
			height: newBodyHeight + bodyPadding}, duration);
		toTab.animate({left: leftPos, height: newBodyHeight + bodyPadding}, duration, function()
			{
				fromTab.hide();
				fromTab.css({position: '', width: '', height: '', left: ''});
				toTab.css({position: '', width: '', height: '', left: ''});
				dialog.css({overflow: '', width: (fullscreen ? 'auto' : ''), height: (fullscreen ? 'auto' : ''), 'margin-left': (fullscreen ? dialogMargin : '')});
				dialog.toggleClass(toggleClass);
				if (fullscreen)
				{
					body.css({position: 'absolute', height: '', left: 0, right: 0, 
						top: header.outerHeight(),
						bottom: footer.outerHeight(),
						'max-height': 'inherit'});
				}
				else
				{
					body.css({position: '', height: ''});
				}
				if (options.fullscreen && options.back)
				{
					// restore non-fullscreen state
					dialog.restoreTab();
				}
				if (options.complete)
				{
					options.complete();
				}
			});
	}
}(jQuery));


/*** REMOTE PROCEDURE CALLS VIA JSON-RPC *************************************************/

var RPC = (new function($)
{
	'use strict';
	
	// Properties
	this.rpcUrl;
	this.defaultFailureCallback;
	this.connectErrorMessage = 'Cannot establish connection';
	var XAuthToken;

	this.call = function(method, params, completed_callback, failure_callback, timeout)
	{
		var _this = this;
		
		var request = JSON.stringify({nocache: new Date().getTime(), method: method, params: params});
		var xhr = new XMLHttpRequest();

		xhr.open('post', this.rpcUrl);
		
		if (XAuthToken !== undefined)
		{
			xhr.setRequestHeader('X-Auth-Token', XAuthToken);
		}
		
		if (timeout)
		{
			xhr.timeout = timeout;
		}

		// Example for cross-domain access:
		//xhr.open('post', 'http://localhost:6789/jsonrpc');
		//xhr.withCredentials = 'true';
		//xhr.setRequestHeader('Authorization', 'Basic ' + window.btoa('myusername:mypassword'));

		xhr.onreadystatechange = function()
		{
			if (xhr.readyState === 4)
			{
					var res = 'Unknown error';
					var result;
					if (xhr.status === 200)
					{
						if (xhr.responseText != '')
						{
							try
							{
								result = JSON.parse(xhr.responseText);
							}
							catch (e)
							{
								res = e;
							}
							if (result)
							{
								if (result.error == null)
								{
									XAuthToken = xhr.getResponseHeader('X-Auth-Token');
									res = result.result;
									completed_callback(res);
									return;
								}
								else
								{
									res = result.error.message;
									if (result.error.message != 'Access denied')
									{
										res = res + '<br><br>Request: ' + request;
									}
								}
							}
						}
						else
						{
							res = 'No response received.';
						}
					}
					else if (xhr.status === 0)
					{
						res = _this.connectErrorMessage;
					}
					else
					{
						res = 'Invalid Status: ' + xhr.status;
					}

					if (failure_callback)
					{
						failure_callback(res, result);
					}
					else
					{
						_this.defaultFailureCallback(res, result);
					}
			}
		};
		xhr.send(request);
	}
}(jQuery));
