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
 * $Revision: 1 $
 * $Date: 2012-05-01 00:00:00 +0200 (Di, 01 Mai 2012) $
 *
 */

var Settings_RefreshInterval = 5;
var Settings_RefreshIndicator = false;
var Settings_TimeZoneCorrection = 0;
var Settings_ShowNotifications = true;
var Settings_MaxMessages = 1000;  // must be the same as in nzbget.conf
var Settings_SetFocus = false; // automatically set focus to the first control in dialogs (not good on touch devices, because pops up the on-screen-keyboard)

// Download toolbar and info settings
var Settings_ShowEditButtons = true;
var Settings_ShowMoveButtons = true;
var Settings_ShowAddButton = true;
var Settings_ShowSpeedLimitControl = true;
var Settings_ShowScanButton = true;
var Settings_ShowDownloadsLeft = true;
var Settings_ShowDownloadsTime = true;
var Settings_ShowDownloadsSpeed = true;
var Settings_ShowNotifications = true;
var Settings_MiniTheme = false;
var Settings_MiniThemeAuto = false;

// Global state
var Status;
var Groups;
var Urls;
var Messages;
var History;
var secondsToUpdate = -1;
var refreshTimer = 0;
var nzbgetVersion;
var loadQueue;
var SpeedLimitInput_focused = false;
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

// Const
var NZBGET_RPC_URL = './jsonrpc';

$(document).ready(
	function()
	{
		$('#FirstUpdateInfo').show();

		loadSettings();
		setupEvents();

		index_init();
		downloads_init();
		edit_init();
		messages_init();
		history_init();
		upload_init();

		$(window).resize(windowResized);

		initialized = true;
		refresh();
	}
);

function createXMLHttpRequest() {
	var xmlHttp;

	if (window.XMLHttpRequest) {
		xmlHttp = new XMLHttpRequest();
	} else if (window.ActiveXObject) {
		try {
			xmlHttp = new ActiveXObject("Msxml2.XMLHTTP");
		} catch(e) {
			try {
				xmlHttp = new ActiveXObject("Microsoft.XMLHTTP");
			} catch(e) {
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
				var res;
				if (xhr.status === 200)
				{
					if (xhr.responseText != '')
					{
						var result = JSON.parse(xhr.responseText);
						if (result.error==null)
						{
							$('#RPCError').hide();
							res = result.result;
							completed_callback(res);
							return;
						}
						else
						{
							res = result.error.message + '<br><br>Request: ' + request;
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
					failure_callback(res);
				}
				else
				{
					$('#FirstUpdateInfo').hide();
					$('#RPCError-text').html(res);
					$('#RPCError').show();
				}
		}
	};
	xhr.send(request);
}

function refresh()
{
	clearTimeout(refreshTimer);
    refreshPaused = 0;
	refreshing = true;
	refreshNeeded = false;
	refreshIndicatorShow();

	loadQueue = new Array(version_update, status_update, downloads_update, messages_update, history_update);

	if (nzbgetVersion != null)
	{
		// query NZBGet version only on first refresh
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
function scheduleNextRefresh()
{
	clearTimeout(refreshTimer);
	secondsToUpdate = refreshNeeded ? 0 : Settings_RefreshInterval;
	if (secondsToUpdate > 0 || refreshNeeded)
	{
		secondsToUpdate += 0.1;
		countSeconds();
	}
	else
	{
		refreshIndicatorHide();
	}
}

function refreshIndicatorShow()
{
	$('#RefreshIndicator').html('Refreshing...');
}

function refreshIndicatorHide()
{
	$('#RefreshIndicator').empty();
}

function refreshIndicatorWait()
{
	$('#RefreshIndicator').html('Waiting: ' + round1(secondsToUpdate));
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
		refreshIndicatorWait();
		refreshTimer = setTimeout(countSeconds, 100);
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
	if (data === 'indicator')
	{
		Settings_RefreshIndicator = !Settings_RefreshIndicator;
	}
	else
	{
		Settings_RefreshInterval = parseFloat(data);
		scheduleNextRefresh();
	}
	updateRefreshMenu();
	saveSettings();
}

function updateRefreshMenu()
{
	setMenuMark($('#RefreshMenu'), Settings_RefreshInterval);
	$('#RefreshIndicatorItem').html(Settings_RefreshIndicator ? $('li[data="mark"]', $('#RefreshMenu')).html() : '');
	show('#RefreshIndicatorBlock', Settings_RefreshIndicator);
}

/* END - Auto refresh and Refresh menu
*****************************************************************/

function index_init()
{
	TBDownloadInfoWidth = $('#TBDownloadInfo').width();
	confirm_dialog_init();

	show('#SettingsMenuLI_MiniTheme', !Settings_MiniThemeAuto);
	
	var FadeMainTabs = !$.browser.opera;
	if (!FadeMainTabs)
	{
		$('#DownloadsTab').removeClass('fade').removeClass('in');
	}
}

/****************************************************************
* Toolbar settings
*/

function updateToolbarSettings()
{
	var collapsedButtons = ! (Settings_ShowAddButton && Settings_ShowEditButtons && Settings_ShowMoveButtons && Settings_ShowScanButton);
	show('#TBCollapsedButtonsMenu', collapsedButtons);
	//$('#TBAddButton').toggleClass('btn-first', !collapsedButtons);

	show('#TBAddButton, #CHShowAddButton', Settings_ShowAddButton);
	show('#CollapsedButtonsMenu .TBAddItem', !Settings_ShowAddButton);

	show('#CHShowEditButtons', Settings_ShowEditButtons);
	show('#DownloadsToolbar .phone-hide.TBEditButtons', Settings_ShowEditButtons && !Settings_MiniTheme);
	show('#DownloadsToolbar .phone-only.TBEditButtons', Settings_ShowEditButtons && Settings_MiniTheme, 'inline-block');
	show('#CollapsedButtonsMenu .TBEditItem', !Settings_ShowEditButtons);

	show('#CHShowMoveButtons', Settings_ShowMoveButtons);
	show('#DownloadsToolbar .phone-hide.TBMoveButtons', Settings_ShowMoveButtons && !Settings_MiniTheme);
	show('#DownloadsToolbar .phone-only.TBMoveButtons', Settings_ShowMoveButtons && Settings_MiniTheme, 'inline-block');
	show('#CollapsedButtonsMenu .TBMoveItem', !Settings_ShowMoveButtons);

	show('#TBScanButton, #CHShowScanButton', Settings_ShowScanButton);
	show('#CollapsedButtonsMenu .TBScanItem', !Settings_ShowScanButton);

	show('#SepAddItem', !Settings_ShowAddButton && (!Settings_ShowEditButtons || !Settings_ShowMoveButtons || !Settings_ShowScanButton));
	show('#SepEditItem', !Settings_ShowEditButtons && (!Settings_ShowMoveButtons || !Settings_ShowScanButton));
	show('#SepMoveItem', !Settings_ShowMoveButtons && !Settings_ShowScanButton);

	show('#TBSpeedLimitControl, #CHShowSpeedLimitControl', Settings_ShowSpeedLimitControl);

	show('#TBDownloadsLeft, #CHShowDownloadsLeft', Settings_ShowDownloadsLeft);
	show('#TBDownloadsTime, #CHShowDownloadsTime', Settings_ShowDownloadsTime);
	show('#TBDownloadsSpeed, #CHShowDownloadsSpeed', Settings_ShowDownloadsSpeed);
	show('#CHShowNotifications', Settings_ShowNotifications);
	show('#CHMiniTheme', Settings_MiniTheme);
}

function toolbarOptMenuClick()
{
	var data = $(this).parent().attr('data');
	switch (data)
	{
		case 'ShowMoveButtons':
			Settings_ShowMoveButtons = !Settings_ShowMoveButtons;
			break;
		case 'ShowAddButton':
			Settings_ShowAddButton = !Settings_ShowAddButton;
			break;
		case 'ShowScanButton':
			Settings_ShowScanButton = !Settings_ShowScanButton;
			break;
		case 'ShowSpeedLimitControl':
			Settings_ShowSpeedLimitControl = !Settings_ShowSpeedLimitControl;
			break;
		case 'ShowEditButtons':
			Settings_ShowEditButtons = !Settings_ShowEditButtons;
			switchTheme(); // update checkmarks
			break;
		case 'ShowDownloadsLeft':
			Settings_ShowDownloadsLeft = !Settings_ShowDownloadsLeft;
			break;
		case 'ShowDownloadsTime':
			Settings_ShowDownloadsTime = !Settings_ShowDownloadsTime;
			break;
		case 'ShowDownloadsSpeed':
			Settings_ShowDownloadsSpeed = !Settings_ShowDownloadsSpeed;
			break;
		case 'ShowNotifications':
			Settings_ShowNotifications = !Settings_ShowNotifications;
			break;
		case 'MiniTheme':
			Settings_MiniTheme = !Settings_MiniTheme;
			switchTheme();
			break;
	}
	updateToolbarSettings();
	saveSettings();
    windowResized();
}

/* END - Toolbar settings
*****************************************************************/

function loadSettings()
{
	var savedVersion = getSetting('version', 'version');
	firstRun = savedVersion === 'version';
	$('#version').text(savedVersion);
	
	Settings_RefreshInterval = parseFloat(getSetting('RefreshInterval', Settings_RefreshInterval));
	Settings_RefreshIndicator = parseBool(getSetting('RefreshIndicator', Settings_RefreshIndicator));

	Settings_ShowEditButtons = parseBool(getSetting('ShowEditButtons', Settings_ShowEditButtons));
	Settings_ShowMoveButtons = parseBool(getSetting('ShowMoveButtons', Settings_ShowMoveButtons));
	Settings_ShowAddButton = parseBool(getSetting('ShowAddButton', Settings_ShowAddButton));
	Settings_ShowSpeedLimitControl = parseBool(getSetting('ShowSpeedLimitControl', Settings_ShowSpeedLimitControl));
	Settings_ShowScanButton = parseBool(getSetting('ShowScanButton', Settings_ShowScanButton));

	Settings_ShowDownloadsLeft = parseBool(getSetting('ShowDownloadsLeft', Settings_ShowDownloadsLeft));
	Settings_ShowDownloadsTime = parseBool(getSetting('ShowDownloadsTime', Settings_ShowDownloadsTime));
	Settings_ShowDownloadsSpeed = parseBool(getSetting('ShowDownloadsSpeed', Settings_ShowDownloadsSpeed));

	Settings_ShowNotifications = parseBool(getSetting('ShowNotifications', Settings_ShowNotifications));
	Settings_MiniTheme = parseBool(getSetting('MiniTheme', Settings_MiniTheme));

	// visualize settings
	settingsToControls();
}

function saveSettings()
{
	setSetting('RefreshInterval', Settings_RefreshInterval);
	setSetting('RefreshIndicator', Settings_RefreshIndicator);

	setSetting('ShowEditButtons', Settings_ShowEditButtons);
	setSetting('ShowAddButton', Settings_ShowAddButton);
	setSetting('ShowMoveButtons', Settings_ShowMoveButtons);
	setSetting('ShowSpeedLimitControl', Settings_ShowSpeedLimitControl);
	setSetting('ShowScanButton', Settings_ShowScanButton);

	setSetting('ShowDownloadsLeft', Settings_ShowDownloadsLeft);
	setSetting('ShowDownloadsTime', Settings_ShowDownloadsTime);
	setSetting('ShowDownloadsSpeed', Settings_ShowDownloadsSpeed);

	setSetting('ShowNotifications', Settings_ShowNotifications);
	setSetting('MiniTheme', Settings_MiniTheme);
}

function settingsToControls()
{
	updateRefreshMenu();
	updateToolbarSettings();
	switchTheme();
}

function setupEvents()
{
	$('#RefreshMenu li a').click(refreshIntervalClick);
	$('#SettingsMenu li a').click(toolbarOptMenuClick);
	$('#ToolbarOptMenu li a').click(toolbarOptMenuClick);
	$('#HandbrakeMenu li[data] a').click(downloads_Pause_click);
}

function TODO()
{
	animateAlert('#Notif_NotImplemented');
}

function confirm_dialog_init()
{
	confirm_dialog = $('#ConfirmDialog');
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

function confirm_dialog_click()
{
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

	if (!Settings_MiniTheme)
	{
		$('#ToolbarOptMenu').toggleClass('pull-right', $('#ToolbarOptButton').offset().left > 180);
		$('#HandbrakeMenu').toggleClass('pull-right', $('#HandbrakeButton').offset().left > 240);
		$('#SettingsMenu').toggleClass('pull-right', $('#SettingsMenuLI').offset().left > 100);
	}
	else
	{
		centerPopupMenu('#ToolbarOptMenu', true);
		centerPopupMenu('#HandbrakeMenu', true);
		centerPopupMenu('#SettingsMenu', true);
		centerPopupMenu('#RefreshMenu', true);
		centerPopupMenu('#CollapsedButtonsMenu', true);
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
	if (Settings_MiniTheme)
	{
		var w = $('#NavbarContainer').innerWidth() - $('#RefreshToolbar').outerWidth() - 30;
		var $btns = $('ul.nav > li');
		var buttonWidth = w / ($btns.length - 1); // -1 - don't count refresh indicator
		buttonWidth = buttonWidth > 90 ? 90 : buttonWidth;
		$btns.css({ 'min-width': buttonWidth + 'px' });
	}
}

function switchTheme()
{
	$('#DownloadsTable tbody').empty();
	$('#HistoryTable tbody').empty();
	$('#MessagesTable tbody').empty();

	$('body').toggleClass('phone', Settings_MiniTheme);
	$('.datatable').toggleClass('table-bordered', !Settings_MiniTheme);
	$('#DownloadsTable').toggleClass('table-check', !Settings_MiniTheme || Settings_ShowEditButtons);
	$('#HistoryTable').toggleClass('table-check', !Settings_MiniTheme);

	if (Settings_MiniTheme)
	{
		$('.search-query').attr('placeholder', 'Search');
	}
	else
	{
		$('.search-query').removeAttr('placeholder');
	}

	updateToolbarSettings();
	centerPopupMenu('#ToolbarOptMenu', Settings_MiniTheme);
	centerPopupMenu('#HandbrakeMenu', Settings_MiniTheme);
	centerPopupMenu('#SettingsMenu', Settings_MiniTheme);
	centerPopupMenu('#RefreshMenu', Settings_MiniTheme);
	centerPopupMenu('#CollapsedButtonsMenu', Settings_MiniTheme);

	if (initialized)
	{
		downloads_redraw();
		history_redraw();
		messages_redraw();

		downloads_theme();
		history_theme();
		messages_theme();
	}

    windowResized();
}

function checkMobileTheme()
{
	if ( /Android|webOS|iPhone|iPod|BlackBerry/i.test(navigator.userAgent) &&  
		$(window).width() < 560 && !Settings_MiniTheme &&
		confirm('Looks like you are using a smartphone.\nWould you like to switch to mobile version?\n(You can switch back via settings menu.)'))
	{
		Settings_MiniTheme = true;
		saveSettings();
		switchTheme()
	}
}
