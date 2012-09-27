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

var Settings_RefreshInterval = 1;
var Settings_RefreshAnimation = true;
var Settings_PlayAnimation = true;
var Settings_TimeZoneCorrection = 0; // not fully implemented
var Settings_MaxMessages = 1000;  // must be the same as in nzbget.conf
var Settings_SetFocus = false; // automatically set focus to the first control in dialogs (not good on touch devices, because pops up the on-screen-keyboard)

var Settings_ShowNotifications = true;
var Settings_MiniTheme = false;
var Settings_MiniThemeAuto = true;
var Settings_NavbarFixed = false;
var Settings_ShowEditButtons = true;

// Global state
var Status;
var Groups;
var Urls;
var Messages;
var History;
var Config;
var secondsToUpdate = -1;
var refreshTimer = 0;
var indicatorTimer = 0;
var indicatorFrame=0;
var nzbgetVersion;
var loadQueue;
var firstLoad = true;
var TBDownloadInfoWidth;
var refreshPaused = 0;
var confirm_dialog;
var confirm_dialog_func;
var Categories = [];
var refreshing = false;
var refreshNeeded = false;
var initialized = false;
var firstRun = false;
var mobileSafari = false;
var scrollbarWidth = 0;
var switchingTheme = false;
var State_ConnectionError = false;

// Const
var NZBGET_RPC_URL = './jsonrpc';

$(document).ready(
	function()
	{
		$('#FirstUpdateInfo').show();

		index_init();

		loadSettings();
		windowResized();

		status_init();
		downloads_init();
		edit_init();
		messages_init();
		history_init();
		upload_init();
		config_init();

		$(window).resize(windowResized);

		initialized = true;
		refresh();

		// DEBUG: activate config tab
		//$('#DownloadsTab').removeClass('fade').removeClass('in');
		//$('#ConfigTabLink').tab('show');
	}
);

function rpc(method, params, completed_callback, failure_callback)
{
	var request = JSON.stringify({method: method, params: params});
	var xhr = createXMLHttpRequest();

	xhr.open('post', NZBGET_RPC_URL);

	// Example for cross-domain access:
	//xhr.open('post', 'http://localhost:6789/jsonrpc');
	//xhr.withCredentials = 'true';
	//xhr.setRequestHeader('Authorization', 'Basic ' + window.btoa('nzbget:mypassword'));

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
								res = result.result;
								completed_callback(res);
								return;
							}
							else
							{
								res = result.error.message + '<br><br>Request: ' + request;
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
					res = 'Cannot establish connection to NZBGet.';
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
					rpc_failure(res, result);
				}
		}
	};
	xhr.send(request);
}

function rpc_failure(res)
{
	State_ConnectionError = true;
	$('#FirstUpdateInfo').hide();
	$('#RPCError-text').html(res);
	$('#RPCError').show();
	if (Status)
	{
		// stop animations
		status_redraw();
	}
}

function refresh()
{
	clearTimeout(refreshTimer);
    refreshPaused = 0;
    State_ConnectionError = false;
	$('#RPCError').hide();
	refreshing = true;
	refreshNeeded = false;
	refreshAnimationShow();

	loadQueue = new Array(version_update, config_update, status_update, downloads_update, messages_update, history_update);

	if (nzbgetVersion != null)
	{
		// query NZBGet version and Configuration only on first refresh
		loadQueue.shift();
		loadQueue.shift();
	}
	else
	{
		// load categories only on first refresh
		categories_load();
	}

	loadNext();
}

function loadNext()
{
	if (loadQueue.length > 0)
	{
		var nextStep = loadQueue[0];
		loadQueue.shift();
		nextStep();
	}
	else
	{
		loadCompleted();
	}
}

function loadCompleted()
{
	downloads_redraw();
	status_redraw();
	messages_redraw();
	history_redraw();

	if (firstLoad)
	{
		$('#FirstUpdateInfo').hide();
		$('#Navbar').show();
		$('#MainTabContent').show();
		windowResized();
		firstLoad = false;
	}

	refreshing = false;

	if (firstRun)
	{
		firstRun = false;
		checkMobileTheme();
	}

	scheduleNextRefresh();
}

function pageLoaded()
{
	setTimeout(refresh, 0);
}

function version_update()
{
	rpc('version', [], function(version) {
		nzbgetVersion = version;
		$('#version').text(version);
		setSetting('version', version);
		loadNext();
	});
}

function categories_load()
{
	$.get('categories.txt', function(data) {
		Categories = $.trim(data).replace(/\r/g, '').split('\n');
	}, 'html');
}

/****************************************************************
* Auto refresh and Refresh menu
*/

function refresh_click()
{
	if (indicatorFrame > 10)
	{
		// force animation restart
		indicatorFrame = 0;
	}
	refresh();
}

function scheduleNextRefresh()
{
	clearTimeout(refreshTimer);
	secondsToUpdate = refreshNeeded ? 0 : Settings_RefreshInterval;
	if (secondsToUpdate > 0 || refreshNeeded)
	{
		secondsToUpdate += 0.1;
		countSeconds();
	}
}

function countSeconds()
{
    if (refreshPaused > 0)
    {
        return;
    }

	secondsToUpdate -= 0.1;
	if (secondsToUpdate <= 0)
	{
		refresh();
	}
	else
	{
		refreshTimer = setTimeout(countSeconds, 100);
	}
}

function refreshAnimationShow()
{
	if (Settings_RefreshAnimation && indicatorTimer === 0)
	{
		refreshAnimationFrame();
	}
}

function refreshAnimationFrame()
{
	// animate next frame
	indicatorFrame++;

	if (indicatorFrame === 20)
	{
		indicatorFrame = 0;
	}

	var f = indicatorFrame <= 10 ? indicatorFrame : 0;

	var degree = 360 * f / 10;

	$('#RefreshAnimation').css({
		'-webkit-transform': 'rotate(' + degree + 'deg)',
		   '-moz-transform': 'rotate(' + degree + 'deg)',
			'-ms-transform': 'rotate(' + degree + 'deg)',
			 '-o-transform': 'rotate(' + degree + 'deg)',
				'transform': 'rotate(' + degree + 'deg)'
	});

	if (!refreshing && indicatorFrame === 0 && (Settings_RefreshInterval === 0 || Settings_RefreshInterval > 1 || !Settings_RefreshAnimation) || State_ConnectionError)
	{
		indicatorTimer = 0;
	}
	else
	{
		// schedule next frame update
		indicatorTimer = setTimeout(refreshAnimationFrame, 100);
	}
}

function refresh_pause()
{
	clearTimeout(refreshTimer);
    refreshPaused++;
}

function refresh_resume()
{
    refreshPaused--;

    if (refreshPaused === 0 && Settings_RefreshInterval > 0)
    {
        countSeconds();
    }
}

function refresh_update()
{
	refreshNeeded = true;
	refreshPaused = 0;
	if (!refreshing)
	{
		scheduleNextRefresh();
	}
}

function refreshIntervalClick()
{
	var data = $(this).parent().attr('data');
	Settings_RefreshInterval = parseFloat(data);
	scheduleNextRefresh();
	updateRefreshMenu();
	saveSettings();

	if (Settings_RefreshInterval === 0)
	{
		// stop animation
		status_redraw();
	}
}

function updateRefreshMenu()
{
	setMenuMark($('#RefreshMenu'), Settings_RefreshInterval);
}

/* END - Auto refresh and Refresh menu
*****************************************************************/

function index_init()
{
	mobileSafari = $.browser.safari && navigator.userAgent.toLowerCase().match(/(iphone|ipod|ipad)/) != null;
	scrollbarWidth = index_scrollbarWidth();

	$('#RefreshMenu li a').click(refreshIntervalClick);

	TBDownloadInfoWidth = $('#TBDownloadInfo').width();
	confirm_dialog_init();

	var FadeMainTabs = !$.browser.opera;
	if (!FadeMainTabs)
	{
		$('#DownloadsTab').removeClass('fade').removeClass('in');
	}

	$('#Navbar a[data-toggle="tab"]').on('show', index_mainTabBeforeShow);
	$('#Navbar a[data-toggle="tab"]').on('shown', index_mainTabAfterShow);
	index_setupSearch();

	$(window).scroll(index_windowScrolled);
}

function index_mainTabBeforeShow(e)
{
	var tabname = $(e.target).attr('href');
	tabname = tabname.substr(1, tabname.length - 4);
	$('#SearchBlock .search-query, #SearchBlock .search-clear').hide();
	$('#' + tabname + 'Table_filter, #' + tabname + 'Table_clearfilter').show();
}

function index_mainTabAfterShow(e)
{
	if ($(e.target).attr('href') !== '#ConfigTab')
	{
		config_cleanup();
	}
}

function index_setupSearch()
{
	$('.navbar-search .search-query').on('focus', function()
	{
		$(this).next().removeClass('icon-white');
	});

	$('.navbar-search .search-query').on('blur', function()
	{
		$(this).next().addClass('icon-white');
	});

	$('.navbar-search').show();
	index_mainTabBeforeShow({target: $('#DownloadsTabLink')});
}

function index_windowScrolled()
{
	$('body').toggleClass('scrolled', $(window).scrollTop() > 0 && !Settings_MiniTheme);
}

function index_scrollbarWidth()
{
    var div = $('<div style="width:50px;height:50px;overflow:hidden;position:absolute;top:-200px;left:-200px;"><div style="height:100px;"></div>');
    // Append our div, do our calculation and then remove it
    $('body').append(div);
    var w1 = $('div', div).innerWidth();
    div.css('overflow-y', 'scroll');
    var w2 = $('div', div).innerWidth();
    $(div).remove();
    return (w1 - w2);
}

function loadSettings()
{
	var savedVersion = getSetting('version', 'version');
	firstRun = savedVersion === 'version';
	$('#version').text(savedVersion);

	Settings_RefreshInterval = parseFloat(getSetting('RefreshInterval', Settings_RefreshInterval));

	// visualize settings
	settingsToControls();
}

function saveSettings()
{
	setSetting('RefreshInterval', Settings_RefreshInterval);
}

function settingsToControls()
{
	updateRefreshMenu();
	switchTheme();
}

function TODO()
{
	animateAlert('#Notif_NotImplemented');
}

function confirm_dialog_init()
{
	confirm_dialog = $('#ConfirmDialog');
	confirm_dialog.on('hidden', confirm_dialog_hidden);
	$('#ConfirmDialog_OK').click(confirm_dialog_click);
}

function confirm_dialog_show(id, okFunc)
{
	$('#ConfirmDialog_Title').html($('#' + id + '_Title').html());
	$('#ConfirmDialog_Text').html($('#' + id + '_Text').html());
	$('#ConfirmDialog_OK').html($('#' + id + '_OK').html());
	centerDialog('#ConfirmDialog', true);
	confirm_dialog_func = okFunc;
	confirm_dialog.modal();
}

function confirm_dialog_hidden()
{
	// confirm dialog copies data from other nodes
	// the copied dom nodes must be destroyed
	$('#ConfirmDialog_Title').empty();
	$('#ConfirmDialog_Text').empty();
	$('#ConfirmDialog_OK').empty();
}

function confirm_dialog_click(event)
{
	event.preventDefault(); // avoid scrolling
	confirm_dialog_func();
	confirm_dialog.modal('hide');
}

function animateAlert(alert, completeFunc)
{
	if (Settings_ShowNotifications || $(alert).hasClass('alert-error'))
	{
		$(alert).animate({'opacity':'toggle'});
		var duration = $(alert).attr('data-duration');
		if (duration == null)
		{
			duration = 1000;
		}
		window.setTimeout(function()
		{
			$(alert).animate({'opacity':'toggle'}, completeFunc);
		}, duration);
	}
	else if (completeFunc)
	{
		completeFunc();
	}
}

function windowResized()
{
	if (Settings_MiniThemeAuto)
	{
		oldSettings_MiniTheme = Settings_MiniTheme;
		Settings_MiniTheme = $(window).width() < 560;
		if (oldSettings_MiniTheme !== Settings_MiniTheme)
		{
			switchTheme();
		}
	}

	resizeNavbar();

	if (Settings_MiniTheme)
	{
		centerPopupMenu('#PlayMenu', true);
		centerPopupMenu('#RefreshMenu', true);
	}

	centerCenterDialogs();
}

function centerPopupMenu(menu, center)
{
	$elem = $(menu);
	if (center)
	{
		$elem.removeClass('pull-right');
		var top = ($(window).height() - $elem.outerHeight())/2;
		top = top > 0 ? top : 0;
		var off = $elem.parent().offset();
		top -= off.top;
		var left = ($(window).width() - $elem.outerWidth())/2;
		left -= off.left;
		$elem.css({
			left: left,
			top: top,
			right: 'inherit'
		});
	}
	else
	{
		$elem.css({
			left: '',
			top: '',
			right: ''
		});
	}
}

function centerDialog(dialog, center)
{
	$elem = $(dialog);
	if (center)
	{
		var top = ($(window).height() - $elem.outerHeight())/2;
		top = top > 0 ? top : 0;
		$elem.css({ top: top});
	}
	else
	{
		$elem.css({ top: '' });
	}
}

function centerCenterDialogs()
{
	$.each($('.modal-center'), function(index, element) {
		centerDialog(element, true);
	});
}

function resizeNavbar()
{
	var ScrollDelta = scrollbarWidth;
	if ($(document).height() > $(window).height())
	{
		// scrollbar is already visible, not need to acount on it
		ScrollDelta = 0;
	}

	if (Settings_MiniTheme)
	{
		var w = $('#NavbarContainer').width() - $('#RefreshBlockPhone').outerWidth() - ScrollDelta;
		var $btns = $('#Navbar ul.nav > li');
		var buttonWidth = w / $btns.length;
		$btns.css('min-width', buttonWidth + 'px');
		$('#NavLinks').css('margin-left', 0);
		$('body').toggleClass('navfixed', false);
	}
	else
	{
		var InfoBlockMargin = 10;
		var w = $('#SearchBlock').position().left - $('#InfoBlock').position().left - $('#InfoBlock').width() - InfoBlockMargin * 2 - ScrollDelta;
		var n = $('#NavLinks').width();
		var offset = (w - n) / 2;
		var fixed = true;
		if (offset < 0)
		{
			w = $('#NavbarContainer').width() - ScrollDelta;
			offset = (w - n) / 2;
			fixed = false;
		}
		offset = offset > 0 ? offset : 0;
		$('#NavLinks').css('margin-left', offset);

		// as of Aug 2012 Mobile Safari does not support "position:fixed"
		$('body').toggleClass('navfixed', fixed && !mobileSafari);

		if (switchingTheme)
		{
			$('#Navbar ul.nav > li').css('min-width', '');
		}
	}
}

function switchTheme()
{
	switchingTheme = true;

	$('#DownloadsTable tbody').empty();
	$('#HistoryTable tbody').empty();
	$('#MessagesTable tbody').empty();

	$('body').toggleClass('phone', Settings_MiniTheme);
	$('.datatable').toggleClass('table-bordered', !Settings_MiniTheme);
	$('#DownloadsTable').toggleClass('table-check', !Settings_MiniTheme || Settings_ShowEditButtons);
	$('#HistoryTable').toggleClass('table-check', !Settings_MiniTheme);

	centerPopupMenu('#PlayMenu', Settings_MiniTheme);
	centerPopupMenu('#RefreshMenu', Settings_MiniTheme);

	if (Settings_MiniTheme)
	{
		$('#RefreshBlock').appendTo($('#RefreshBlockPhone'));
		$('#DownloadsRecordsPerPageBlock').appendTo($('#DownloadsRecordsPerPageBlockPhone'));
		$('#HistoryRecordsPerPageBlock').appendTo($('#HistoryRecordsPerPageBlockPhone'));
		$('#MessagesRecordsPerPageBlock').appendTo($('#MessagesRecordsPerPageBlockPhone'));
	}
	else
	{
		$('#RefreshBlock').appendTo($('#RefreshBlockDesktop'));
		$('#DownloadsRecordsPerPageBlock').appendTo($('#DownloadsTableTopBlock'));
		$('#HistoryRecordsPerPageBlock').appendTo($('#HistoryTableTopBlock'));
		$('#MessagesRecordsPerPageBlock').appendTo($('#MessagesTableTopBlock'));
	}

	if (initialized)
	{
		downloads_redraw();
		history_redraw();
		messages_redraw();

		downloads_theme();
		history_theme();
		messages_theme();
		windowResized();
	}

	switchingTheme = false;
}

function checkMobileTheme()
{
	if ( /Android|webOS|iPhone|iPod|BlackBerry/i.test(navigator.userAgent) &&
		$(window).width() < 560 && !Settings_MiniTheme && !Settings_MiniThemeAuto &&
		confirm('Looks like you are using a smartphone.\nWould you like to switch to mobile version?\n(You can switch back via settings menu.)'))
	{
		Settings_MiniTheme = true;
		saveSettings();
		switchTheme()
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

/****************************************************************
* Common Tab functions
*/

function tab_updateInfo(control, stat)
{
	if (stat.filter)
	{
		control.removeClass('badge-info').addClass('badge-warning');
	}
	else
	{
		control.removeClass('badge-warning').addClass('badge-info');
	}

	control.html(stat.available);
	control.toggleClass('badge2', stat.total > 9);
	control.toggleClass('badge3', stat.total > 99);

	if (control.lastOuterWidth !== control.outerWidth())
	{
		resizeNavbar();
		control.lastOuterWidth = control.outerWidth();
	}
}

/* END - Common Tab functions
*****************************************************************/

