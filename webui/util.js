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
 
function createXMLHttpRequest()
{
	var xmlHttp;

	if (window.XMLHttpRequest)
	{
		xmlHttp = new XMLHttpRequest();
	}
	else if (window.ActiveXObject)
	{
		try
		{
			xmlHttp = new ActiveXObject("Msxml2.XMLHTTP");
		}
		catch(e)
		{
			try
			{
				xmlHttp = new ActiveXObject("Microsoft.XMLHTTP");
			}
			catch(e)
			{
				throw(e);
			}
		}
	}

	if (xmlHttp==null)
	{
		alert("Your browser does not support XMLHTTP.");
		throw("Your browser does not support XMLHTTP.");
	}

	return xmlHttp;
}

function FormatTimeHMS(sec)
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

function FormatTimeLeft(sec)
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

function FormatDateTime(unixTime)
{
	var dt = new Date(unixTime * 1000);
	var h = dt.getHours();
	var m = dt.getMinutes();
	var s = dt.getSeconds();
	return dt.toDateString() + ' ' + (h < 10 ? '0' : '') + h + ':' + (m < 10 ? '0' : '') + m + ':' + (s < 10 ? '0' : '') + s;
}

function FormatSizeMB(sizeMB, sizeLo)
{
	if (sizeLo !== undefined && sizeMB < 100)
	{
		sizeMB = sizeLo / 1024 / 1024;
	}

	if (sizeMB > 10240)
	{
		return round1(sizeMB / 1024.0) + '&nbsp;GB';
	}
	else if (sizeMB > 1024)
	{
		return round2(sizeMB / 1024.0) + '&nbsp;GB';
	}
	else if (sizeMB > 100)
	{
		return round0(sizeMB) + '&nbsp;MB';
	}
	else if (sizeMB > 10)
	{
		return round1(sizeMB) + '&nbsp;MB';
	}
	else
	{
		return round2(sizeMB) + '&nbsp;MB';
	}
}

function FormatAge(time)
{
	if (time == 0)
	{
		return '';
	}

	var diff = new Date().getTime() / 1000 - time;
	if (diff > 60*60*24)
	{
		return round0(diff / (60*60*24))  +'&nbsp;d';
	}
	else
	{
		return round0(diff / (60*60))  +'&nbsp;h';
	}
}

function round0(arg)
{
	return Math.round(arg);
}

function round1(arg)
{
	return arg.toFixed(1);
}

function round2(arg)
{
	return arg.toFixed(2);
}

function FormatNZBName(NZBName)
{
	return NZBName.replace(/\./g, ' ')
		.replace(/_/g, ' ');
}

function TextToHtml(str)
{
	return str.replace(/&/g, '&amp;')
		.replace(/</g, '&lt;')
		.replace(/>/g, '&gt;');
}

/*
function EscapeHtml(str)
{
	return str.replace(/([ !"#$%&'()*+,.\/:;<=>?@[\\\]^`{|}~])/g,'\\\\$1');
}
*/

function setMenuMark(menu, data)
{
	// remove marks from all items
	$('li table tr td:first-child', menu).html('');
	// set mark on selected item
	mark = $('li[data="mark"]', menu).html();
	$('li[data="' + data + '"] table tr td:first-child', menu).html(mark);
}

function IsEnterKey(e)
{
	var keynum;
	if(window.event)
	{
		// IE
		keynum = e.keyCode;
	}
	else if(e.which)
	{
		// Netscape/Firefox/Opera
		keynum = e.which;
	}
	return keynum == 13;
}

function show(jqQuery, visible, display)
{
	if (display)
	{
		$(jqQuery).css({display: visible ? display : 'none'});
	}
	else
	{
		visible ? $(jqQuery).show() : $(jqQuery).hide();
	}
}

function getSetting(key, def)
{
	var v = localStorage.getItem(key);
	if (v === null || v === '')
	{
		return def;
	}
	else
	{
		return v;
	}
}

function setSetting(key, value)
{
	localStorage.setItem(key, value);
}

function parseBool(value)
{
	return ''+value == 'true';
}

function util_disableShiftMouseDown(event)
{
	// disable default shift+click behaviour, which is to select a text
	if (event.shiftKey)
	{
		event.stopPropagation();
		event.preventDefault();
	}
}

function switch_click(control)
{
    var state = $(control).val().toLowerCase();
	$('.btn', $(control).parent()).removeClass('btn-primary');
	$(control).addClass('btn-primary');
}

function switch_getValue(control)
{
	var state = $('.btn-primary', $(control).parent()).val();
	return state;
}

function util_restoreTab(dialog)
{
	var body = $('.modal-body', dialog);
	var footer = $('.modal-footer', dialog);
	var header = $('.modal-header', dialog);
	dialog.css({margin: '', left: '', top: '', bottom: '', right: '', width: '', height: ''});
	body.css({position: '', height: '', left: '', right: '', top: '', bottom: '', 'max-height': ''});
	footer.css({position: '', left: '', right: '', bottom: ''});
}

function util_switchTab(dialog, fromTab, toTab, duration, options)
{
	var sign = options.back ? -1 : 1;
	var fullscreen = options.fullscreen && !options.back;
	var bodyPadding = 30;
	var dialogMargin = options.mini ? 0 : 15;
	var dialogBorder = 2;

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
		util_restoreTab(dialog);
	}

	fromTab.hide();
	toTab.show();

	// CONTROL POINT: at this point the destination dialog size is active
	// store destination positions and sizes

	var newBodyHeight = fullscreen ? windowHeight - header.outerHeight() - footer.outerHeight() - dialogMargin*2 - bodyPadding : body.height();
	var newTabWidth = fullscreen ? windowWidth - dialogMargin*2 - dialogBorder - bodyPadding : toTab.width();
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
	fromTab.css({position: 'absolute', left: leftPos, width: oldTabWidth});
	toTab.css({position: 'absolute', width: newTabWidth, height: newBodyHeight, 
		left: sign * ((options.back ? newTabWidth : oldTabWidth) + bodyPadding)});
	fromTab.show();

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
	}

	fromTab.animate({left: sign * -((options.back ? newTabWidth : oldTabWidth) + bodyPadding)}, duration);
	toTab.animate({left: leftPos}, duration, function()
		{
			fromTab.hide();
			fromTab.css({position: '', width: '', height: '', left: ''});
			toTab.css({position: '', width: '', height: '', left: ''});
			dialog.css({overflow: '', width: (fullscreen ? 'auto' : ''), height: (fullscreen ? 'auto' : '')});
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
				util_restoreTab(dialog);
			}
			if (options.complete)
			{
				options.complete();
			}
		});
}
