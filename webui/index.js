/*
 * This file is part of nzbget. See <http://nzbget.net>.
 *
 * Copyright (C) 2012-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 * In this module:
 *   1) Web-interface intialization;
 *   2) Web-interface settings;
 *   3) Refresh handling;
 *   4) Window resize handling including automatic theme switching (desktop/phone);
 *   5) Confirmation dialog;
 *   6) Popup notifications.
 */

/*** WEB-INTERFACE SETTINGS (THIS IS NOT NZBGET CONFIG!) ***********************************/

var UISettings = (new function($)
{
	'use strict';

	/*** Web-interface configuration *************/

	// Options having descriptions can be edited directly in web-interface on settings page.

	this.description = [];

	this.description['activityAnimation'] = 'Animation on play/pause button (yes, no).';
	this.activityAnimation = true;

	this.description['refreshAnimation'] = 'Animation on refresh button (yes, no).';
	this.refreshAnimation = true;

	this.description['slideAnimation'] = 'Animation of tab changes in tabbed dialogs (yes, no).';
	this.slideAnimation = true;

	this.description['setFocus'] = 'Automatically set focus to the first control in dialogs (yes, no).\n\n' +
	  'Not recommended for devices without physical keyboard.';
	this.setFocus = false;

	this.description['showNotifications'] = 'Show popup notifications (yes, no).';
	this.showNotifications = true;

	this.description['dupeBadges'] = 'Show badges with duplicate info in downloads and history (yes, no).';
	this.dupeBadges = false;

	this.description['rowSelect'] = 'Select records by clicking on any part of the row, not just on the check mark (yes, no).';
	this.rowSelect = false;

	this.description['windowTitle'] = 'Window-title for browser.\n\n' +
		'The following variables can be used within placeholders to insert current data:\n' +
		'   COUNT - number of items in queue;\n' +
		'   SPEED - current download speed.\n' +
		'   TIME - remaining time;\n' +
		'   PAUSE - "download paused"-indicator.\n\n' +
		'To form a placeholder surround variable with percent-characters, for example: %COUNT%.\n\n' +
		'To improve formating there is a special syntax. If variable value is empty or null then nothing is inserted:\n' +
		'%(VARNAME)% - show variable value inside parenthesis;\n' +
		'%[VARNAME]% - show variable value inside square brackets;\n' +
		'%VARNAME-% - append hyphen to variable value;\n' +
		'%(VARNAME-)% - show variable value with hyphen inside parenthesis;\n' +
		'%[VARNAME-]% - show variable value with hyphen inside square brackets.\n\n' +
		'Examples:\n' +
		' "%(COUNT-)% NZBGet" - show number of downloads in parenthesis followed by a hyphen; don\'t show "(0) - " if queue is empty;\n' +
		' "%PAUSE% %(COUNT-)% NZBGet" - as above but also show pause-indicator if paused;\n' +
		' "%[COUNT]% %SPEED-% NZBGet" - show number of downloads and speed if not null (default setting).';
	this.windowTitle = '%[COUNT]% %SPEED-% NZBGet';

	this.description['refreshRetries'] = 'Number of refresh attempts if a communication error occurs (0-99).\n\n' +
	  'If all attempts fail, an error is displayed and the automatic refresh stops.'
	this.refreshRetries = 4;

	// Time zone correction in hours.
	// You shouldn't require this unless you can't set the time zone on your computer/device properly.
	this.timeZoneCorrection = 0;

	// Default refresh interval.
	// The choosen interval is saved in web-browser and then restored.
	// The default value sets the interval on first use only.
	this.refreshInterval = 1;

	// URL for communication with NZBGet via JSON-RPC
	this.rpcUrl = './jsonrpc';


	/*** No user configurable settings below this line (do not edit) *************/

	// Current state
	this.miniTheme = false;
	this.showEditButtons = true;
	this.connectionError = false;

	this.load = function()
	{
		this.refreshInterval = parseFloat(this.read('RefreshInterval', this.refreshInterval));
		this.refreshAnimation = this.read('RefreshAnimation', this.refreshAnimation) === 'true';
		this.activityAnimation = this.read('ActivityAnimation', this.activityAnimation) === 'true';
		this.slideAnimation = this.read('SlideAnimation', this.slideAnimation) === 'true';
		this.setFocus = this.read('SetFocus', this.setFocus) === 'true';
		this.showNotifications = this.read('ShowNotifications', this.showNotifications) === 'true';
		this.dupeBadges = this.read('DupeBadges', this.dupeBadges) === 'true';
		this.rowSelect = this.read('RowSelect', this.rowSelect) === 'true';
		this.windowTitle = this.read('WindowTitle', this.windowTitle);
		this.refreshRetries = parseFloat(this.read('RefreshRetries', this.refreshRetries));
	}

	this.save = function()
	{
		this.write('RefreshInterval', this.refreshInterval);
		this.write('RefreshAnimation', this.refreshAnimation);
		this.write('ActivityAnimation', this.activityAnimation);
		this.write('SlideAnimation', this.slideAnimation);
		this.write('SetFocus', this.setFocus);
		this.write('ShowNotifications', this.showNotifications);
		this.write('DupeBadges', this.dupeBadges);
		this.write('RowSelect', this.rowSelect);
		this.write('WindowTitle', this.windowTitle);
		this.write('RefreshRetries', this.refreshRetries);
	}

	this.read = function(key, def)
	{
		var v = localStorage.getItem(key);
		if (v === null)
		{
			return def.toString();
		}
		else
		{
			return v;
		}
	}

	this.write = function(key, value)
	{
		localStorage.setItem(key, value);
	}
}(jQuery));


/*** START WEB-APPLICATION ***********************************************************/

$(document).ready(function()
{
	Frontend.init();
});


/*** FRONTEND MAIN PAGE ***********************************************************/

var Frontend = (new function($)
{
	'use strict';

	// State
	var initialized = false;
	var firstLoad = true;
	var mobileSafari = false;
	var scrollbarWidth = 0;
	var switchingTheme = false;
	var activeTab = 'Downloads';
	var lastTab = '';
	var lastMenu = $();

	this.init = function()
	{
		window.onerror = error;

		if (!checkBrowser())
		{
			return;
		}

		$('#FirstUpdateInfo').show();

		UISettings.load();
		Refresher.init();

		initControls();
		switchTheme();
		windowResized();

		Options.init();
		Status.init();
		Downloads.init({ updateTabInfo: updateTabInfo });
		Messages.init({ updateTabInfo: updateTabInfo });
		History.init({ updateTabInfo: updateTabInfo });
		Upload.init();
		Feeds.init();
		FeedDialog.init();
		FeedFilterDialog.init();
		Config.init({ updateTabInfo: updateTabInfo });
		ConfigBackupRestore.init();
		ConfirmDialog.init();
		UpdateDialog.init();
		ExecScriptDialog.init();
		AlertDialog.init();
		ScriptListDialog.init();
		RestoreSettingsDialog.init();
		LimitDialog.init();

		DownloadsEditDialog.init();
		DownloadsMultiDialog.init();
		DownloadsMergeDialog.init();
		DownloadsSplitDialog.init();
		HistoryEditDialog.init();
		PurgeHistoryDialog.init();

		$(window).resize(windowResized);

		initialized = true;

		authorize();
	}

	function initControls()
	{
		mobileSafari = $.browser.safari && navigator.userAgent.toLowerCase().match(/(iphone|ipod|ipad)/) != null;
		scrollbarWidth = calcScrollbarWidth();

		var FadeMainTabs = !$.browser.opera;
		if (!FadeMainTabs)
		{
			$('#DownloadsTab').removeClass('fade').removeClass('in');
		}

		$('#Navbar a[data-toggle="tab"]').on('show', beforeTabShow);
		$('#Navbar a[data-toggle="tab"]').on('shown', afterTabShow);
		setupSearch();

		$('li > a:has(table)').addClass('has-table');
		$(document).on('keydown', keyDown);
		$(window).scroll(windowScrolled);
	}

	function checkBrowser()
	{
		if ($.browser.msie && parseInt($.browser.version, 10) < 9)
		{
			$('#FirstUpdateInfo').hide();
			$('#UnsupportedBrowserIE8Alert').show();
			return false;
		}
		return true;
	}

	function error(message, source, lineno)
	{
		if (source == '')
		{
			// ignore false errors without source information (sometimes happen in Safari)
			return false;
		}

		$('#FirstUpdateInfo').hide();
		$('#ErrorAlert-title').text('Error in ' + source + ' (line ' + lineno + ')');
		$('#ErrorAlert-text').text(message);
		$('#ErrorAlert').show();

		if (Refresher)
		{
			Refresher.pause();
		}

		return false;
	}

	this.loadCompleted = function()
	{
		Downloads.redraw();
		Status.redraw();
		Messages.redraw();
		History.redraw();

		if (firstLoad)
		{
			Feeds.redraw();
			$('#FirstUpdateInfo').hide();
			$('#Navbar').show();
			$('#MainTabContent').show();
			$('#version').text(Options.option('Version'));
			selectInitialTab();
			windowResized();
			firstLoad = false;
			UpdateDialog.checkUpdate();
		}
	}

	function selectInitialTab()
	{
		var location = window.location.toString();
		var link = null;
		if (location.indexOf('#downloads') > -1)
			link = 'DownloadsTabLink';
		else if (location.indexOf('#history') > -1)
			link = 'HistoryTabLink';
		else if (location.indexOf('#messages') > -1)
			link = 'MessagesTabLink';
		else if (location.indexOf('#settings') > -1)
			link = 'ConfigTabLink';
		if (link)
		{
			$('#DownloadsTab').removeClass('fade');
			$('#' + link).click();
			$('#DownloadsTab').addClass('fade');
		}
	}

	function beforeTabShow(e)
	{
		var tabname = $(e.target).attr('href');
		tabname = tabname.substr(1, tabname.length - 4);

		if (activeTab === 'Config' && !Config.canLeaveTab(e.target))
		{
			e.preventDefault();
			return;
		}

		lastTab = activeTab;
		activeTab = tabname;

		$('#SearchBlock .search-query, #SearchBlock .search-clear').hide();
		$('#' + activeTab + 'Table_filter, #' + activeTab + 'Table_clearfilter').show();

		switch (activeTab)
		{
			case 'Config': Config.show(); break;
			case 'Messages': Messages.show(); break;
			case 'History': History.show(); break;
		}

		FilterMenu.setTab(activeTab);
	}

	function afterTabShow(e)
	{
		switch (lastTab)
		{
			case 'Config': Config.hide(); break;
			case 'Messages': Messages.hide(); break;
			case 'History': History.hide(); break;
		}
		switch (activeTab)
		{
			case 'Config': Config.shown(); break;
		}
	}

	function setupSearch()
	{
		$('.navbar-search .search-query').on('focus', function()
		{
			$(this).next().removeClass('icon-remove-white').addClass('icon-remove');
			$('#SearchBlock_Caret').addClass('focused');
		});

		$('.navbar-search .search-query').on('blur', function()
		{
			$(this).next().removeClass('icon-remove').addClass('icon-remove-white');
			$('#SearchBlock_Caret').removeClass('focused');
		});

		$('.navbar-search').show();
		beforeTabShow({target: $('#DownloadsTabLink')});
	}

	function keyDown(e)
	{
		var key = Util.keyName(e);

		var modals = $('.modal:visible');
		if (modals.length > 0)
		{
			if (key === 'Enter' && !Util.wantsReturn(e.target))
			{
				var primaryButton = $('.btn-primary:visible', modals.last());
				if (primaryButton.length === 1)
				{
					primaryButton.click();
				}
				return false;
			}
			return;
		}

		var filterBox = $('#DownloadsTable_filter, #HistoryTable_filter, #MessagesTable_filter, #ConfigTable_filter');
		if (filterBox.is(':focus') && (key === 'Escape' || key === 'Enter'))
		{
			filterBox.blur();
			return false;
		}

		if (!(Util.isInputControl(e.target) && $(e.target).is(':visible')))
		{
			switch (activeTab)
			{
				case 'Downloads': if (Downloads.processShortcut(key)) return false;
				case 'History': if (History.processShortcut(key)) return false;
				case 'Messages': if (Messages.processShortcut(key)) return false;
				case 'Config': if (Config.processShortcut(key)) return false;
			}
			switch (key)
			{
				case 'Shift+D': $('#DownloadsTabLink').click(); return false;
				case 'Shift+H': $('#HistoryTabLink').click(); return false;
				case 'Shift+M': $('#MessagesTabLink').click(); return false;
				case 'Shift+S': $('#ConfigTabLink').click(); return false;
				case 'Shift+L': $('#StatusSpeed').click(); return false;
				case 'Shift+A': $('#StatusTime').click(); return false;
				case 'Shift+R': $('#RefreshButton').click(); return false;
				case 'Shift+P': $('#PlayPauseButton').click(); return false;
				case 'Shift+T': $('#ScheduledPauseButton').click(); return false;
			}
		}
	}

	function windowScrolled()
	{
		$('body').toggleClass('scrolled', $(window).scrollTop() > 0 && !UISettings.miniTheme);
	}

	function calcScrollbarWidth()
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

	function windowResized()
	{
		var oldMiniTheme = UISettings.miniTheme;
		UISettings.miniTheme = $(window).width() < 560;
		if (oldMiniTheme !== UISettings.miniTheme)
		{
			switchTheme();
		}

		resizeNavbar();

		alignPopupMenu('#PlayMenu');
		alignPopupMenu('#RefreshMenu');
		alignPopupMenu('#RssMenu');
		alignPopupMenu('#StatDialog_MonthMenu', true);

		alignCenterDialogs();

		if (initialized)
		{
			Downloads.resize();
		}
	}

	function alignPopupMenu(menu, right)
	{
		var center = UISettings.miniTheme;
		var $elem = $(menu);
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
			var off = $elem.parent().offset();
			if (off.left + $elem.outerWidth() > $(window).width())
			{
				var left = $(window).width() - $elem.outerWidth() - off.left;
				$elem.css({ left: left });
			}
			if (right)
			{
				$elem.addClass('pull-right');
			}
		}
	}
	this.alignPopupMenu = alignPopupMenu;

	function showPopupMenu(menu, anchor, rect)
	{
		var $menu = $(menu);
		if ($menu.is(':visible'))
		{
			$menu.hide();
			return;
		}

		lastMenu.hide();
		lastMenu = $menu;

		$menu.css({
			left: rect.left + (anchor.indexOf('right') > -1 ? rect.width - $menu.outerWidth() : 0),
			top: rect.top + (anchor.indexOf('top') > -1 ? - $menu.outerHeight() : rect.height)
		});
		$menu.show();

		if ($menu.offset().top < $(window).scrollTop())
		{
			$menu.css({ top: rect.top + rect.height });
		}
		if ($menu.offset().left + $menu.outerWidth() > $(window).width())
		{
			$menu.css({ left: $(window).width() - $menu.outerWidth() });
		}
		if ($menu.offset().top + $menu.outerHeight() > $(window).height() + $(window).scrollTop())
		{
			$menu.css({ top: rect.top - $menu.outerHeight() });
		}
		if ($menu.offset().top < $(window).scrollTop())
		{
			$menu.css({ top: $(window).scrollTop() });
		}

		if (UISettings.miniTheme)
		{
			alignPopupMenu($menu);
		}

		$('html').on('click.PopupMenu', function () { $menu.hide(); $('html').off('click.PopupMenu'); });
	}
	this.showPopupMenu = showPopupMenu;

	function alignCenterDialogs()
	{
		$.each($('.modal-center'), function(index, element) {
			Util.centerDialog(element, true);
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

		if (UISettings.miniTheme)
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

	function updateTabInfo(control, stat)
	{
		control.toggleClass('badge-info', stat.available == stat.total).toggleClass('badge-warning', stat.available != stat.total);
		control.html(stat.available);
		control.toggleClass('badge2', stat.total > 9);
		control.toggleClass('badge3', stat.total > 99);

		if (control.lastOuterWidth !== control.outerWidth())
		{
			resizeNavbar();
			control.lastOuterWidth = control.outerWidth();
		}
	}

	function switchTheme()
	{
		switchingTheme = true;

		$('#DownloadsTable tbody').empty();
		$('#HistoryTable tbody').empty();
		$('#MessagesTable tbody').empty();

		$('body').toggleClass('phone', UISettings.miniTheme);
		$('.datatable').toggleClass('table-bordered', !UISettings.miniTheme);
		$('#DownloadsTable').toggleClass('table-check', !UISettings.miniTheme || UISettings.showEditButtons);
		$('#HistoryTable').toggleClass('table-check', !UISettings.miniTheme || UISettings.showEditButtons);

		alignPopupMenu('#PlayMenu');
		alignPopupMenu('#RefreshMenu');
		alignPopupMenu('#RssMenu');
		alignPopupMenu('#StatDialog_MonthMenu', true);

		if (UISettings.miniTheme)
		{
			$('#RefreshBlock').appendTo($('#RefreshBlockPhone'));
			$('#DownloadsRecordsPerPageBlock').appendTo($('#DownloadsRecordsPerPageBlockPhone'));
			$('#HistoryRecordsPerPageBlock').appendTo($('#HistoryRecordsPerPageBlockPhone'));
			$('#MessagesRecordsPerPageBlock').appendTo($('#MessagesRecordsPerPageBlockPhone'));
			$('#StatDialog_MonthMenu').appendTo($('#StatDialog_MonthBlockPhone'));
		}
		else
		{
			$('#RefreshBlock').appendTo($('#RefreshBlockDesktop'));
			$('#DownloadsRecordsPerPageBlock').appendTo($('#DownloadsTableTopBlock'));
			$('#HistoryRecordsPerPageBlock').appendTo($('#HistoryTableTopBlock'));
			$('#MessagesRecordsPerPageBlock').appendTo($('#MessagesTableTopBlock'));
			$('#StatDialog_MonthMenu').appendTo($('#StatDialog_MonthBlockTop'));
		}

		if (initialized && !firstLoad)
		{
			Downloads.applyTheme();
			History.applyTheme();
			Messages.applyTheme();
			windowResized();
		}

		switchingTheme = false;
	}
	
	function authorize()
	{
		var formAuth = document.cookie.indexOf('Auth-Type=form') > -1;
		if (!formAuth)
		{
			Refresher.update();
			return;
		}

		function sendAuth()
		{
			var username = $('#LoginDialog_Username').val();
			var password = $('#LoginDialog_Password').val();
			var headers = [{name: 'X-Authorization', value: 'Basic ' + window.btoa(username + ':' + password)}];
			RPC.call('version', [],
				function(version)
				{
					$('#LoginDialog').modal('hide');
					// reloading of page is needed for certain browsers to force save-password-dialog
					document.location.reload();
				},
				function(err, result)
				{
					$('#LoginDialog_PasswordBlock').removeClass('last-group');
					$('#LoginDialog_Error').show();
					if (!$('#LoginDialog_Password').is(":focus"))
					{
						$('#LoginDialog_Username').focus();
					}
				},
				{ custom_headers: headers });
		}

		$('#LoginDialog_Form').submit(function(e)
			{
				if ($('#LoginDialog_Error').is(":visible"))
				{
					$('#LoginDialog_Error').hide();
					$('#LoginDialog_PasswordBlock').addClass('last-group');
					setTimeout(sendAuth, 500);
				}
				else
				{
					setTimeout(sendAuth, 0);
				}

				return false;
			});

		// try RPC call, it may work without extra authorization
		RPC.call('version', [], Refresher.update, function()
			{
				$('#LoginDialog').modal({backdrop: 'static'});
				$('#LoginDialog_Username').focus();
			}, { timeout: 10000 } );
	}
}(jQuery));


/*** REFRESH CONTROL *********************************************************/

var Refresher = (new function($)
{
	'use strict';

	// State
	var loadQueue;
	var firstLoad = true;
	var secondsToUpdate = -1;
	var refreshTimer = 0;
	var indicatorTimer = 0;
	var indicatorFrame=0;
	var refreshPaused = 0;
	var refreshing = false;
	var refreshNeeded = false;
	var refreshErrors = 0;

	this.init = function()
	{
		RPC.rpcUrl = UISettings.rpcUrl;
		RPC.connectErrorMessage = 'Cannot establish connection to NZBGet.'
		RPC.defaultFailureCallback = rpcFailure;
		RPC.next = loadNext;
		RPC.safeMethods = ['version', 'status', 'listgroups', 'history', 'listfiles',
			'log', 'loadlog', 'logscript', 'logupdate', 'config', 'loadconfig',
			'configtemplates', 'readurl', 'servervolumes'];

		$('#RefreshMenu li a').click(refreshIntervalClick);
		$('#RefreshButton').click(refreshClick);
		updateRefreshMenu();
	}

	function refresh()
	{
		UISettings.connectionError = false;
		$('#ErrorAlert').hide();
		refreshStarted();

		loadQueue = 4;
		Status.update();
		Downloads.update();
		Messages.update();
		History.update();

		if (firstLoad)
		{
			loadQueue += 2; // options and configtemplates
			Options.update();
		}
	}

	function loadNext()
	{
		loadQueue--;
		if (loadQueue === 0)
		{
			firstLoad = false;
			Frontend.loadCompleted();
			refreshCompleted();
		}
	}

	function rpcFailure(res, result)
	{
		// If a communication error occurs during status refresh we retry:
		// first attempt is made immediately, other attempts are made after defined refresh interval
		if (refreshing && !(result && result.error))
		{
			refreshErrors = refreshErrors + 1;
			if (refreshErrors === 1 && refreshErrors <= UISettings.refreshRetries)
			{
				refresh();
				return;
			}
			else if (refreshErrors <= UISettings.refreshRetries)
			{
				$('#RefreshError').show();
				scheduleNextRefresh();
				return;
			}
		}

		Refresher.pause();
		UISettings.connectionError = true;
		$('#FirstUpdateInfo').hide();
		$('#ErrorAlert-text').html(res);
		$('#ErrorAlert').show();
		$('#RefreshError').hide();
		if (Status.status)
		{
			// stop animations
			Status.redraw();
		}

		$('html, body').animate({scrollTop: 0 }, 400);
	};

	function refreshStarted()
	{
		clearTimeout(refreshTimer);
		refreshPaused = 0;
		refreshing = true;
		refreshNeeded = false;
		refreshAnimationShow();
	}

	function refreshCompleted()
	{
		refreshing = false;
		refreshErrors = 0;
		$('#RefreshError').hide();
		scheduleNextRefresh();
	}

	this.isPaused = function()
	{
		return refreshPaused > 0;
	}

	this.pause = function()
	{
		clearTimeout(refreshTimer);
		refreshPaused++;
	}

	this.resume = function(wantUpdate)
	{
		refreshPaused--;
		if (refreshPaused === 0 && wantUpdate)
		{
			this.update();
		}
		else if (refreshPaused === 0 && UISettings.refreshInterval > 0)
		{
			countSeconds();
		}
	}

	this.update = function()
	{
		refreshNeeded = true;
		refreshPaused = 0;
		if (!refreshing)
		{
			scheduleNextRefresh();
		}
	}

	function refreshClick()
	{
		if (indicatorFrame > 10)
		{
			// force animation restart
			indicatorFrame = 0;
		}
		refreshErrors = 0;
		refresh();
	}

	function scheduleNextRefresh()
	{
		clearTimeout(refreshTimer);
		secondsToUpdate = refreshNeeded ? 0 : UISettings.refreshInterval;
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
		if (UISettings.refreshAnimation && indicatorTimer === 0)
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

		if ((!refreshing && indicatorFrame === 0 && (UISettings.refreshInterval === 0 || UISettings.refreshInterval > 1 || !UISettings.refreshAnimation)) || UISettings.connectionError)
		{
			indicatorTimer = 0;
		}
		else
		{
			// schedule next frame update
			indicatorTimer = setTimeout(refreshAnimationFrame, 100);
		}
	}

	function refreshIntervalClick()
	{
		var data = $(this).parent().attr('data');
		UISettings.refreshInterval = parseFloat(data);
		scheduleNextRefresh();
		updateRefreshMenu();
		UISettings.save();

		if (UISettings.refreshInterval === 0)
		{
			// stop animation
			Status.redraw();
		}
	}

	function updateRefreshMenu()
	{
		Util.setMenuMark($('#RefreshMenu'), UISettings.refreshInterval);
	}
}(jQuery));


function TODO(text)
{
	$('#Notif_NotImplemented_Param').html(text === undefined ? '' : ': ' + text);
	PopupNotification.show('#Notif_NotImplemented');
}


/*** CONFIRMATION DIALOG *****************************************************/

var ConfirmDialog = (new function($)
{
	'use strict';

	// Controls
	var $ConfirmDialog;

	// State
	var actionCallback;
	var confirmed = false;

	this.init = function()
	{
		$ConfirmDialog = $('#ConfirmDialog');
		$ConfirmDialog.on('hidden', hidden);
		$('#ConfirmDialog_OK').click(click);
	}

	this.showModal = function(id, _actionCallback, initCallback, selCount)
	{
		$('#ConfirmDialog_Title').html($('#' + id + '_Title').html());
		$('#ConfirmDialog_Text').html($('#' + id + '_Text').html());
		$('#ConfirmDialog_OK').html($('#' + id + '_OK').html());
		var helpId = $('#' + id + '_Help').html();
		$('#ConfirmDialog_Help').attr('href', '#' + helpId);
		Util.show('#ConfirmDialog_Help', helpId !== null);

        if (selCount > 1)
        {
            var html = $('#ConfirmDialog_Text').html();
            html = html.replace(/selected/g, selCount + ' selected');
            $('#ConfirmDialog_Text').html(html);
        }

		actionCallback = _actionCallback;
		if (initCallback)
		{
			initCallback($ConfirmDialog);
		}

		Util.centerDialog($ConfirmDialog, true);
		confirmed = false;
		$ConfirmDialog.modal({backdrop: 'static'});

		// avoid showing multiple backdrops when the modal is shown from other modal
		var backdrops = $('.modal-backdrop');
		if (backdrops.length > 1)
		{
			backdrops.last().remove();
		}
	}

	function hidden()
	{
		if (confirmed)
		{
			actionCallback($ConfirmDialog);
		}

		// confirm dialog copies data from other nodes
		// the copied DOM nodes must be destroyed
		$('#ConfirmDialog_Title').empty();
		$('#ConfirmDialog_Text').empty();
		$('#ConfirmDialog_OK').empty();
	}

	function click(event)
	{
		event.preventDefault(); // avoid scrolling
		confirmed = true;
		$ConfirmDialog.modal('hide');
	}
}(jQuery));


/*** ALERT DIALOG *****************************************************/

var AlertDialog = (new function($)
{
	'use strict';

	// Controls
	var $AlertDialog;

	this.init = function()
	{
		$AlertDialog = $('#AlertDialog');
	}

	this.showModal = function(title, text)
	{
		$('#AlertDialog_Title').html(title);
		$('#AlertDialog_Text').html(text);
		Util.centerDialog($AlertDialog, true);
		$AlertDialog.modal();
	}
}(jQuery));


/*** NOTIFICATIONS *********************************************************/

var PopupNotification = (new function($)
{
	'use strict';

	this.show = function(alert, completeFunc)
	{
		if (UISettings.showNotifications || $(alert).hasClass('alert-error'))
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
}(jQuery));
