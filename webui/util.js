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
