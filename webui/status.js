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
 * In this module:
 *   1) Status Infos on main page (speed, time, paused state etc.);
 *   2) Statistics and Status dialog;
 *   3) Limit dialog (speed and active news servers);
 *   4) Filter menu.
 */

/*** STATUS INFOS ON MAIN PAGE AND STATISTICS DIALOG ****************************************/

var Status = (new function($)
{
	'use strict';

	// Properties (public)
	this.status;

	// Controls
	var $CHPauseDownload;
	var $CHPausePostProcess;
	var $CHPauseScan;
	var $StatusSpeed;
	var $StatusSpeedIcon;
	var $StatusTimeIcon;
	var $StatusTime;
	var $PlayBlock;
	var $PlayButton;
	var $PauseButton;
	var $PlayAnimation;
	var $ScheduledPauseDialog;
	var $PauseForInput;
	var $PauseForPreview;

	// State
	var status;
	var lastPlayState = 0;
	var lastAnimState = 0;
	var playInitialized = false;
	var modalShown = false;
	var titleGen = [];

	var validTimePatterns = [
		/^=\d{1,2}(:[0-5][0-9])?$/, // 24h exact
		/^=\d{1,2}(:[0-5][0-9])?(AM|PM)$/i, // 12h exact
		/^\d+(:[0-5][0-9])?$/, // 24h relative
		/^\d+(h|m)?$/i, // relative minutes or hours
	];

	this.init = function()
	{
		$CHPauseDownload = $('#CHPauseDownload');
		$CHPausePostProcess = $('#CHPausePostProcess');
		$CHPauseScan = $('#CHPauseScan');
		$PlayBlock = $('#PlayBlock');
		$PlayButton = $('#PlayButton');
		$PauseButton = $('#PauseButton');
		$PlayAnimation = $('#PlayAnimation');
		$StatusSpeed = $('#StatusSpeed');
		$StatusSpeedIcon = $('#StatusSpeedIcon');
		$StatusTimeIcon = $('#StatusTimeIcon');
		$StatusTime = $('#StatusTime');
		$ScheduledPauseDialog = $('#ScheduledPauseDialog');
		$PauseForInput = $('#PauseForInput');
		$PauseForPreview = $('#PauseForPreview');

		if (UISettings.setFocus)
		{
			$ScheduledPauseDialog.on('shown', function()
			{
				$('#PauseForInput').focus();
			});
		}

		$PlayAnimation.hover(function() { $PlayBlock.addClass('hover'); }, function() { $PlayBlock.removeClass('hover'); });
		$PauseForInput.keyup(function(e)
		{
			if (e.which == 13) return;

			calculateSeconds($(this).val());
		});

		// temporary pause the play animation if any modal is shown (to avoid artifacts in safari)
		$('body >.modal').on('show', modalShow);
		$('body > .modal').on('hide', modalHide);

		StatDialog.init();
		FilterMenu.init();
		initTitle();
	}

	this.update = function()
	{
		var _this = this;
		RPC.call('status', [],
			function(curStatus)
			{
				status = curStatus;
				_this.status = status;
				StatDialog.update();
			});
	}

	this.redraw = function()
	{
		redrawInfo();
		StatDialog.redraw();
	}

	function redrawInfo()
	{
		Util.show($CHPauseDownload, status.DownloadPaused);
		Util.show($CHPausePostProcess, status.PostPaused);
		Util.show($CHPauseScan, status.ScanPaused);

		updatePlayAnim();
		updatePlayButton();

		if (status.ServerStandBy)
		{
			$StatusSpeed.html('--- MB/s');
			if (status.ResumeTime > 0)
			{
				$StatusTime.html(Util.formatTimeLeft(status.ResumeTime - status.ServerTime));
			}
			else if (status.RemainingSizeMB > 0 || status.RemainingSizeLo > 0)
			{
				if (status.AverageDownloadRate > 0)
				{
					$StatusTime.html(Util.formatTimeLeft(status.RemainingSizeMB*1024/(status.AverageDownloadRate/1024)));
				}
				else
				{
					$StatusTime.html('--h --m');
				}
			}
			else
			{
				$StatusTime.html('0h 0m');
			}
		}
		else
		{
			$StatusSpeed.html(Util.formatSpeed(status.DownloadRate));
			if (status.DownloadRate > 0)
			{
				$StatusTime.html(Util.formatTimeLeft(
					(status.DownloadPaused ? status.ForcedSizeMB : status.RemainingSizeMB) *1024/(status.DownloadRate/1024)));
			}
			else
			{
				$StatusTime.html('--h --m');
			}
		}

		var limit = status.DownloadLimit > 0;
		if (!limit)
		{
			for (var i=0; i < Status.status.NewsServers.length; i++)
			{
				limit = !Status.status.NewsServers[i].Active;
				if (limit)
				{
					break;
				}
			}
		}

		$StatusSpeedIcon.toggleClass('icon-plane', !limit);
		$StatusSpeedIcon.toggleClass('icon-truck', limit);

		var statWarning = (status.ServerStandBy && status.ResumeTime > 0) || status.QuotaReached;
		$StatusTime.toggleClass('orange', statWarning);
		$StatusTimeIcon.toggleClass('icon-time', !statWarning);
		$StatusTimeIcon.toggleClass('icon-time-orange', statWarning);

		updateTitle();
	}

	function updatePlayButton()
	{
		var Play = !status.DownloadPaused;
		if (Play === lastPlayState)
		{
			return;
		}

		lastPlayState = Play;

		var hideBtn = Play ? $PlayButton : $PauseButton;
		var showBtn = !Play ? $PlayButton : $PauseButton;

		if (playInitialized)
		{
			hideBtn.fadeOut(500);
			showBtn.fadeIn(500);
			if (!Play && !status.ServerStandBy)
			{
				PopupNotification.show('#Notif_Downloads_Pausing');
			}
		}
		else
		{
			hideBtn.hide();
			showBtn.show();
		}

		if (Play)
		{
			$PlayAnimation.removeClass('pause').addClass('play');
		}
		else
		{
			$PlayAnimation.removeClass('play').addClass('pause');
		}

		playInitialized = true;
	}

	function updatePlayAnim()
	{
		// Animate if either any downloads or post-processing is in progress
		var Anim = (!status.ServerStandBy || status.FeedActive || status.QueueScriptCount > 0 ||
			(status.PostJobCount > 0 && !status.PostPaused) ||
			(status.UrlCount > 0 && (!status.DownloadPaused || Options.option('UrlForce') === 'yes'))) &&
			(UISettings.refreshInterval !== 0) && !UISettings.connectionError && UISettings.activityAnimation;
		if (Anim === lastAnimState)
		{
			return;
		}

		lastAnimState = Anim;

		if (!modalShown)
		{
			if (Anim)
			{
				$PlayAnimation.fadeIn(1000);
			}
			else
			{
				$PlayAnimation.fadeOut(1000);
			}
		}
	}

	this.playClick = function()
	{
		//PopupNotification.show('#Notif_Play');

		if (lastPlayState)
		{
			// pause all activities
			RPC.call('pausedownload', [],
				function(){RPC.call('pausepost', [],
				function(){RPC.call('pausescan', [], Refresher.update)})});
		}
		else
		{
			// resume all activities
			RPC.call('resumedownload', [],
				function(){RPC.call('resumepost', [],
				function(){RPC.call('resumescan', [], Refresher.update)})});
		}
	}

	this.pauseClick = function(data)
	{
		switch (data)
		{
			case 'download':
				var method = status.DownloadPaused ? 'resumedownload' : 'pausedownload';
				break;
			case 'post':
				var method = status.PostPaused ? 'resumepost' : 'pausepost';
				break;
			case 'scan':
				var method = status.ScanPaused ? 'resumescan' : 'pausescan';
				break;
		}
		RPC.call(method, [], Refresher.update);
	}

	this.statDialogClick = function()
	{
		StatDialog.showModal();
	}

	this.scheduledPauseClick = function(seconds)
	{
		RPC.call('pausedownload', [],
			function(){RPC.call('pausepost', [],
			function(){RPC.call('pausescan', [],
			function(){RPC.call('scheduleresume', [seconds], Refresher.update)})})});
	}

	this.scheduledPauseDialogClick = function()
	{
		$PauseForInput.val('');
		$PauseForPreview.addClass('invisible');
		$ScheduledPauseDialog.modal();
	}

	this.pauseForClick = function()
	{
		var val = $PauseForInput.val();
		var seconds = calculateSeconds(val);

		if (isNaN(seconds) || seconds <= 0)
		{
			return;
		}

		$ScheduledPauseDialog.modal('hide');
		this.scheduledPauseClick(seconds);
	}

	function isTimeInputValid(str)
	{
		for (var i = 0; i < validTimePatterns.length; i++)
		{
			if (validTimePatterns[i].test(str)) return true;
		}
	}

	function calculateSeconds(parsable) {
		parsable = parsable.toLowerCase();

		if (!isTimeInputValid(parsable))
		{
			$PauseForPreview.addClass('invisible');
			return;
		}

		var now = new Date(), future = new Date();
		var hours = 0, minutes = 0;

		var mode = /^=/.test(parsable) ? 'exact' : 'relative';
		var indicator = (parsable.match(/h|m|am|pm$/i) || [])[0];
		var parsedTime = parsable.match(/(\d+):?(\d+)?/) || [];
		var primaryValue = parsedTime[1];
		var secondaryValue = parsedTime[2];
		var is12H = (indicator === 'am' || indicator === 'pm');

		if (indicator === undefined && secondaryValue === undefined)
		{
			if (mode === 'exact') hours = parseInt(primaryValue);
			else minutes = parseInt(primaryValue);
		}
		else if (indicator === 'm')
		{
			minutes = parseInt(primaryValue);
		}
		else
		{
			hours = parseInt(primaryValue);
			if (secondaryValue) minutes = parseInt(secondaryValue);
			if (indicator === 'pm' && hours < 12) hours += 12;
		}

		if ((mode !== 'exact' && (is12H || (hours > 0 && minutes > 59))) ||
			(mode === 'exact' && (hours < 0 || hours > 23 || minutes < 0 || minutes > 59)))
		{
			$PauseForPreview.addClass('invisible');
			return;
		}

		if (mode === 'exact')
		{
			future.setHours(hours, minutes, 0, 0);

			if (future < now) future.setDate(now.getDate() + 1);
		}
		else
		{
			future.setHours(now.getHours() + hours, now.getMinutes() + minutes);
		}

		$PauseForPreview.find('strong')
			.text((future.getDay() !== now.getDay()) ? future.toLocaleString() : future.toLocaleTimeString())
			.end()
			.removeClass('invisible');

		return (future - now)/1000;
	}

	function modalShow()
	{
		modalShown = true;
		if (lastAnimState)
		{
			$PlayAnimation.hide();
		}
	}

	function modalHide()
	{
		if (lastAnimState)
		{
			$PlayAnimation.show();
		}
		modalShown = false;
	}

	this.serverName = function(server)
	{
		var name = Options.option('Server' + server.ID + '.Name');
		if (name === null || name === '')
		{
			var host = Options.option('Server' + server.ID + '.Host');
			var port = Options.option('Server' + server.ID + '.Port');
			name = (host === null ? '' : host) + ':' + (port === null ? '119' : port);
		}
		return name;
	}

	function initTitle()
	{
		function format(pattern, paramFunc)
		{
			if (UISettings.connectionError)
			{
				var value = '?';
				var isEmpty = false;
			}
			else
			{
				var param = paramFunc();
				var value = param[0];
				var isEmpty = param[1];
			}

			if (isEmpty && pattern != '%VAR%')
			{
				return '';
			}

			switch (pattern)
			{
				case '%VAR%': return value;
				case '%VAR-%': return '' + value + ' - ';
				case '%(VAR)%': return '(' + value + ')';
				case '%(VAR-)%': return '(' + value + ') - ';
				case '%[VAR]%': return '[' + value + ']';
				case '%[VAR-]%': return '[' + value + '] - ';
			}

			return Downloads.groups.length > 0 ? '' + Downloads.groups.length + ' - ' : '';
		};

		function fill(varname, paramFunc)
		{
			titleGen['%' + varname + '%'] = function() { return format('%VAR%', paramFunc); };
			titleGen['%' + varname + '-%'] = function() { return format('%VAR-%', paramFunc); };
			titleGen['%(' + varname + ')%'] = function() { return format('%(VAR)%', paramFunc); };
			titleGen['%(' + varname + '-)%'] = function() { return format('%(VAR-)%', paramFunc); };
			titleGen['%[' + varname + ']%'] = function() { return format('%[VAR]%', paramFunc); };
			titleGen['%[' + varname + '-]%'] = function() { return format('%[VAR-]%', paramFunc); };
		}

		fill('COUNT', function() { return [Downloads.groups.length, Downloads.groups.length == 0]; });
		fill('SPEED', function() { return [$StatusSpeed.text(), status.ServerStandBy]; });
		fill('TIME', function() { return [$StatusTime.text(), status.ServerStandBy]; });
		fill('PAUSE', function() { return ['||', !status.DownloadPaused]; });
	}

	function updateTitle()
	{
		var title = UISettings.windowTitle;

		for (var name in titleGen)
		{
			if (title.indexOf(name) > -1)
			{
				var value = titleGen[name]();
				title = title.replace(name, value);
			}
		}

		title = title.trim();

		window.document.title = title;
	}
	this.updateTitle = updateTitle;
}(jQuery));


/*** STATISTICS DIALOG *******************************************************/

var StatDialog = (new function($)
{
	'use strict';

	// Controls
	var $StatDialog;
	var $StatDialog_DataVersion;
	var $StatDialog_DataUptime;
	var $StatDialog_DataDownloadTime;
	var $StatDialog_DataTotalDownloaded;
	var $StatDialog_DataRemaining;
	var $StatDialog_DataFree;
	var $StatDialog_DataAverageSpeed;
	var $StatDialog_DataCurrentSpeed;
	var $StatDialog_DataSpeedLimit;
	var $StatDialog_ArticleCache;
	var $StatDialog_QueueScripts;
	var $StatDialog_ChartBlock;
	var $StatRangeDialog;
	var $StatRangeDialog_PeriodInput;
	var $StatDialog_Tooltip;
	var $StatDialog_TodaySize;
	var $StatDialog_MonthSize;
	var $StatDialog_AllTimeSize;
	var $StatDialog_CustomSize;
	var $StatDialog_Custom;

	// State
	var visible = false;
	var lastPage;
	var lastTab = null;
	var lastFullscreen;
	var servervolumes = null;
	var prevServervolumes = null;
	var curRange = 'MIN';
	var redrawLock = 0;
	var needChartUpdate = false;
	var curServer = 0;
	var monthListInitialized = false;
	var curMonth = null;
	var monYear = false;
	var monStartIndex = 0;
	var monEndIndex = 0;
	var monStartDate;
	var chartData = null;
	var mouseOverIndex = -1;
	var clockOK = false;
	var volumeMode = false;

	var monthNames = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];

	this.init = function()
	{
		$StatDialog = $('#StatDialog');
		$StatDialog_DataVersion = $('#StatDialog_DataVersion');
		$StatDialog_DataUptime = $('#StatDialog_DataUptime');
		$StatDialog_DataDownloadTime = $('#StatDialog_DataDownloadTime');
		$StatDialog_DataTotalDownloaded = $('#StatDialog_DataTotalDownloaded');
		$StatDialog_DataRemaining = $('#StatDialog_DataRemaining');
		$StatDialog_DataFree = $('#StatDialog_DataFree');
		$StatDialog_DataAverageSpeed = $('#StatDialog_DataAverageSpeed');
		$StatDialog_DataCurrentSpeed = $('#StatDialog_DataCurrentSpeed');
		$StatDialog_DataSpeedLimit = $('#StatDialog_DataSpeedLimit');
		$StatDialog_ArticleCache = $('#StatDialog_ArticleCache');
		$StatDialog_QueueScripts = $('#StatDialog_QueueScripts');
		$StatDialog_ChartBlock = $('#StatDialog_ChartBlock');
		$StatRangeDialog = $('#StatRangeDialog');
		$StatRangeDialog_PeriodInput = $('#StatRangeDialog_PeriodInput');
		$StatDialog_Tooltip = $('#StatDialog_Tooltip');
		$StatDialog_TodaySize = $('#StatDialog_TodaySize');
		$StatDialog_MonthSize = $('#StatDialog_MonthSize');
		$StatDialog_AllTimeSize = $('#StatDialog_AllTimeSize');
		$StatDialog_CustomSize = $('#StatDialog_CustomSize');
		$StatDialog_Custom = $('#StatDialog_Custom');

		$('#StatDialog_ServerMenuAll').click(chooseServer);
		$('#StatDialog_Volumes').click(tabClick);
		$('#StatDialog_Back').click(backClick);

		$StatDialog.on('hidden', function()
		{
			// cleanup
			lastTab = null;
			servervolumes = null;
			prevServervolumes = null;
			$StatDialog_ChartBlock.empty();
			visible = false;
		});

		if (UISettings.setFocus)
		{
			$StatRangeDialog.on('shown', function()
			{
				$StatRangeDialog_PeriodInput.focus();
			});
		}

		$StatRangeDialog.on('hidden', StatRangeDialogHidden);

		TabDialog.extend($StatDialog);
	}

	this.update = function()
	{
		if (visible)
		{
			RPC.call('servervolumes', [], servervolumes_loaded);
		}
		else
		{
			RPC.next();
		}
	}

	function servervolumes_loaded(volumes)
	{
		prevServervolumes = servervolumes;
		servervolumes = volumes;
		RPC.next();
	}

	function firstLoadStatisticsData()
	{
		RPC.call('servervolumes', [], function (volumes)
			{
				prevServervolumes = servervolumes;
				servervolumes = volumes;
				updateMonthList();
				StatDialog.redraw();
			});
	}

	this.showModal = function(serverId)
	{
		volumeMode = serverId !== undefined;
		curServer = serverId !== undefined ? serverId : 0;

		$('#StatDialog_GeneralTab').show();
		$('#StatDialog_VolumesTab').hide();
		$('#StatDialog_Back').hide();
		$('#StatDialog_BackSpace').show();
		$('#StatDialog_Title').text('Statistics and Status');
		Util.show('#StatDialog_ArticleCache_Row', Options.option('ArticleCache') !== '0');
		Util.show('#StatDialog_QueueScripts_Row', Status.status.QueueScriptCount > 0);
		$StatDialog.removeClass('modal-large').addClass('modal-mini');

		if (Options.option('QuotaStartDay') != '1')
		{
			$('#StatDialog_MonthTitle').text('Billing month:');
		}

		$('#StatDialog_Volume_MONTH, #StatDialog_Volume_MONTH2').text(monthNames[(new Date()).getMonth()] + ' ' + (new Date()).getFullYear());

		monthListInitialized = false;
		updateServerList();
		lastTab = null;
		$StatDialog.restoreTab();
		visible = true;
		redrawStatistics();
		$StatDialog.modal();
		firstLoadStatisticsData();

		if (volumeMode)
		{
			$('#StatDialog_Volumes').click();
		}
	}

	this.redraw = function()
	{
		if (visible)
		{
			redrawStatistics();
			if (servervolumes !== null && lastTab === '#StatDialog_VolumesTab')
			{
				if (redrawLock > 0)
				{
					needChartUpdate = true;
				}
				else
				{
					if (!monthListInitialized)
					{
						updateMonthList();
					}
					redrawChart();
				}
			}
		}
	}

	function redrawStatistics()
	{
		var status = Status.status;

		$StatDialog_DataVersion.text(Options.option('Version'));
		$StatDialog_DataUptime.text(Util.formatTimeHMS(status.UpTimeSec));
		$StatDialog_DataDownloadTime.text(Util.formatTimeHMS(status.DownloadTimeSec));
		$StatDialog_DataTotalDownloaded.html(Util.formatSizeMB(status.DownloadedSizeMB));
		$StatDialog_DataRemaining.html(Util.formatSizeMB(status.RemainingSizeMB));
		$StatDialog_DataFree.html(Util.formatSizeMB(status.FreeDiskSpaceMB));
		$StatDialog_DataAverageSpeed.html(Util.formatSpeed(status.AverageDownloadRate));
		$StatDialog_DataCurrentSpeed.html(Util.formatSpeed(status.DownloadRate));
		$StatDialog_DataSpeedLimit.html(Util.formatSpeed(status.DownloadLimit));
		$StatDialog_ArticleCache.html(Util.formatSizeMB(status.ArticleCacheMB, status.ArticleCacheLo));
		$StatDialog_QueueScripts.html(status.QueueScriptCount);

		var content = '';
		content += '<tr><td>Download</td><td class="text-right">' +
			(status.DownloadPaused ?
			'<span class="label label-status label-warning">paused</span>' :
			'<span class="label label-status label-success">active</span>') +
			'</td></tr>';

		content += '<tr><td>Post-processing</td><td class="text-right">' + (Options.option('PostProcess') === '' ?
			'<span class="label label-status">disabled</span>' :
			(status.PostPaused ?
			'<span class="label label-status label-warning">paused</span>' :
			'<span class="label label-status label-success">active</span>')) +
			'</td></tr>';

		content += '<tr><td>NZB-Directory scan</td><td class="text-right">' + (Options.option('NzbDirInterval') === '0' ?
			'<span class="label label-status">disabled</span>' :
			(status.ScanPaused ?
			'<span class="label label-status label-warning">paused</span>' :
			'<span class="label label-status label-success">active</span>')) +
			'</td></tr>';

		if (status.QuotaReached)
		{
			content += '<tr><td>Download quota</td><td class="text-right"><span class="label label-status label-warning">reached</span></td></tr>';
		}

		if (status.ResumeTime > 0)
		{
			content += '<tr><td>Autoresume</td><td class="text-right">' + Util.formatTimeHMS(status.ResumeTime - status.ServerTime) + '<i class="icon-empty"/></td></tr>';
		}

		$('#StatusTable tbody').html(content);
	}

	function tabClick(e)
	{
		e.preventDefault();

		if (!volumeMode)
		{
			$('#StatDialog_Back').fadeIn(500);
			$('#StatDialog_BackSpace').hide();
		}
		lastTab = '#' + $(this).attr('data-tab');
		lastPage = $(lastTab);
		lastFullscreen = ($(this).attr('data-fullscreen') === 'true') && !UISettings.miniTheme;
		redrawLock++;
		$StatDialog.switchTab($('#StatDialog_GeneralTab'), lastPage,
			e.shiftKey || !UISettings.slideAnimation || volumeMode ? 0 : 500,
			{ fullscreen: lastFullscreen,
			  toggleClass: 'modal-mini modal-large',
			  mini: UISettings.miniTheme,
			  complete: tabSwitchCompleted});
		if (lastTab === '#StatDialog_VolumesTab')
		{
			if (servervolumes)
			{
				redrawChart();
			}
			$('#StatDialog_Title').text('Downloaded volumes');
		}
	}

	function backClick(e)
	{
		e.preventDefault();
		$('#StatDialog_Back').fadeOut(500, function()
		{
			$('#StatDialog_BackSpace').show();
		});

		$StatDialog.switchTab(lastPage, $('#StatDialog_GeneralTab'),
			e.shiftKey || !UISettings.slideAnimation ? 0 : 500,
			{ fullscreen: lastFullscreen,
			  toggleClass: 'modal-mini modal-large',
			  mini: UISettings.miniTheme,
			  back: true});
		lastTab = null;

		$('#StatDialog_Title').text('Statistics and Status');
	}

	function tabSwitchCompleted()
	{
		redrawLock--;
		if (needChartUpdate)
		{
			needChartUpdate = false;
			if (!monthListInitialized)
			{
				updateMonthList();
			}
			redrawChart();
		}
		Frontend.alignPopupMenu('#StatDialog_MonthMenu', true);
	}

	function size64(size)
	{
		return size.SizeMB < 2000 ? size.SizeLo / 1024.0 / 1024.0 : size.SizeMB;
	}

	function redrawChart()
	{
		var serverNo = curServer;
		var lineLabels = [];
		var dataLabels = [];
		var chartDataTB = [];
		var chartDataGB = [];
		var chartDataMB = [];
		var chartDataKB = [];
		var chartDataB = [];
		var curPoint = null;
		var sumMB = 0;
		var sumLo = 0;
		var maxSizeMB = 0;
		var maxSizeLo = 0;

		function addData(bytes, dataLab, lineLab)
		{
			dataLabels.push(dataLab);
			lineLabels.push(lineLab);

			if (bytes === null)
			{
				chartDataTB.push(null);
				chartDataGB.push(null);
				chartDataMB.push(null);
				chartDataKB.push(null);
				chartDataB.push(null);
				return;
			}
			chartDataTB.push(bytes.SizeMB / 1024.0 / 1024.0);
			chartDataGB.push(bytes.SizeMB / 1024.0);
			chartDataMB.push(size64(bytes));
			chartDataKB.push(bytes.SizeLo / 1024.0);
			chartDataB.push(bytes.SizeLo);
			if (bytes.SizeMB > maxSizeMB)
			{
				maxSizeMB = bytes.SizeMB;
			}
			if (bytes.SizeLo > maxSizeLo)
			{
				maxSizeLo = bytes.SizeLo;
			}
			sumMB += bytes.SizeMB;
			sumLo += bytes.SizeLo;
		}

		function drawMinuteGraph()
		{
			// the current slot may be not fully filled yet,
			// to make the chart smoother for current slot we use the data from the previous reading
			// and we show the previous slot as current.
			curPoint = servervolumes[serverNo].SecSlot;
			for (var i = 0; i < 60; i++)
			{
				addData((i == curPoint && prevServervolumes !== null ? prevServervolumes : servervolumes)[serverNo].BytesPerSeconds[i],
					i + 's', i % 10 == 0 || i == 59 ? i : '');
			}
			if (prevServervolumes !== null)
			{
				curPoint = curPoint > 0 ? curPoint-1 : 59;
			}
		}

		function drawHourGraph()
		{
			for (var i = 0; i < 60; i++)
			{
				addData(servervolumes[serverNo].BytesPerMinutes[i],
					i + 'm', i % 10 == 0 || i == 59 ? i : '');
			}
			curPoint = servervolumes[serverNo].MinSlot;
		}

		function drawDayGraph()
		{
			for (var i = 0; i < 24; i++)
			{
				addData(servervolumes[serverNo].BytesPerHours[i],
					i + 'h', i % 3 == 0 || i == 23 ? i : '');
			}
			curPoint = servervolumes[serverNo].HourSlot;
		}

		function drawMonthGraph()
		{
			var len = servervolumes[serverNo].BytesPerDays.length;
			var daySlot = servervolumes[serverNo].DaySlot;
			var slotDelta = servervolumes[0].FirstDay - servervolumes[serverNo].FirstDay;
			var dt = new Date(monStartDate.getTime());
			var day = 1;
			for (var i = monStartIndex; i <= monEndIndex; i++, day++)
			{
				dt.setDate(day);
				var slot = i + slotDelta;
				addData((slot >= 0 && slot < len ? servervolumes[serverNo].BytesPerDays[slot] : null),
					dt.toDateString(), (day == 1 || day % 5 == 0 || (day < 30 && i === monEndIndex) ? day : ''));
				if (slot === daySlot)
				{
					curPoint = day-1;
				}
			}
			// ensure the line has always the same length (looks nicer)
			for (; day < 32; day++)
			{
				addData(null, null, null);
			}
		}

		function drawYearGraph()
		{
			var firstMon = -1;
			var lastMon = -1;
			var monDataMB = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
			var monDataLo = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];

			// aggregate daily volumes into months
			var daySlot = servervolumes[serverNo].DaySlot;
			var slotDelta = servervolumes[0].FirstDay - servervolumes[serverNo].FirstDay;
			var day = 0;
			for (var i = monStartIndex; i <= monEndIndex; i++, day++)
			{
				var dt = new Date(monStartDate.getTime() + day*24*60*60*1000);
				var slot = i + slotDelta;
				var bytes = servervolumes[serverNo].BytesPerDays[slot];
				if (bytes)
				{
					var mon = dt.getMonth();
					monDataMB[mon] += bytes.SizeMB;
					monDataLo[mon] += bytes.SizeLo;
					if (firstMon === -1)
					{
						firstMon = mon;
					}
					if (mon > lastMon)
					{
						lastMon = mon;
					}
					if (slot === daySlot)
					{
						curPoint = mon;
					}
				}
			}

			for (var i = 0; i < 12; i++)
			{
				addData(firstMon > -1 && i >= firstMon && i <= lastMon ? {SizeMB: monDataMB[i], SizeLo: monDataLo[i]} : null,
					monthNames[i] + ' ' + curMonth, monthNames[i].substr(0, 3));
			}
		}

		if (curRange === 'MIN')
		{
			drawMinuteGraph();
		}
		else if (curRange === 'HOUR')
		{
			drawHourGraph();
		}
		else if (curRange === 'DAY')
		{
			drawDayGraph();
		}
		else if (curRange === 'MONTH' && !monYear)
		{
			drawMonthGraph();
		}
		else if (curRange === 'MONTH' && monYear)
		{
			drawYearGraph();
		}

		var serieData = maxSizeMB > 1024*1024 ? chartDataTB :
					maxSizeMB > 1024 ? chartDataGB :
					maxSizeMB > 1 || maxSizeLo == 0 ? chartDataMB :
					maxSizeLo > 1024 ? chartDataKB : chartDataB;

		var units = maxSizeMB > 1024*1024 ? ' TB' :
				maxSizeMB > 1024 ? ' GB' :
				maxSizeMB > 1 || maxSizeLo == 0 ? ' MB' :
				maxSizeLo > 1024 ? ' KB' : ' B';

		var curPointData = [];
		for (var i = 0; i < serieData.length; i++)
		{
			curPointData.push(i===curPoint ? serieData[i] : null);
		}

		chartData = {
			serieData: serieData,
			serieDataMB: chartDataMB,
			serieDataLo: chartDataB,
			sumMB: sumMB,
			sumLo: sumLo,
			dataLabels: dataLabels
		};

		$StatDialog_ChartBlock.empty();
		$StatDialog_ChartBlock.html('<div id="StatDialog_Chart"></div>');
		$('#StatDialog_Chart').chart({
			values: { serie1 : serieData, serie2: curPointData },
			labels: lineLabels,
			type: 'line',
			margins: [10, 15, 20, 60],
			defaultSeries: {
				rounded: 0.5,
				fill: true,
				plotProps: {
					'stroke-width': 3.0
				},
				dot: true,
				dotProps: {
					stroke: '#FFF',
					size: 3.0,
					'stroke-width': 1.0,
					fill: '#5AF'
				},
				highlight: {
					scaleSpeed: 0,
					scaleEasing: '>',
					scale: 2.0
				},
				tooltip: {
					active: false,
				},
	            color: '#5AF'
	        },
			series: {
				serie2: {
					dotProps: {
						stroke: '#F21860',
						fill: '#F21860',
						size: 3.5,
						'stroke-width': 2.5
					},
					highlight: {
						scale: 1.5
					},
				}
			},
			defaultAxis: {
				labels: true,
				labelsProps: {
					'font-size': 13
				},
				labelsDistance: 12
			},
			axis: {
				l: {
					labels: true,
					suffix: units
				}
			},
			features: {
				grid: {
					draw: [true, false],
					forceBorder: true,
					props: {
						stroke: '#e0e0e0',
						'stroke-width': 1
					},
					ticks: {
						active: [true, false, false],
						size: [6, 0],
						props: {
							stroke: '#e0e0e0'
						}
					}
            	},
				mousearea: {
					type: 'axis',
			        onMouseOver: chartMouseOver,
					onMouseExit: chartMouseExit,
					onMouseOut: chartMouseExit
				},
			}
		});

		simulateMouseEvent();

		updateCounters();
	}

	function chartMouseOver(env, serie, index, mouseAreaData)
	{
		if (mouseOverIndex > -1)
		{
			var env = $('#StatDialog_Chart').data('elycharts_env');
			$.elycharts.mousemanager.onMouseOutArea(env, false, mouseOverIndex, env.mouseAreas[mouseOverIndex]);
		}
		mouseOverIndex = index;
		$StatDialog_Tooltip.html(chartData.dataLabels[index] + ': <span class="stat-size">' +
			Util.formatSizeMB(chartData.serieDataMB[index], chartData.serieDataLo[index]) + '</span>');
	}

	function chartMouseExit(env, serie, index, mouseAreaData)
	{
		mouseOverIndex = -1;
		var title = curRange === 'MIN' ? '60 seconds' :
			curRange === 'HOUR' ? '60 minutes' :
			curRange === 'DAY' ? '24 hours' :
			curRange === 'MONTH' ? $('#StatDialog_Volume_MONTH').text() : 'Sum';

		$StatDialog_Tooltip.html(title + ': <span class="stat-size">' + Util.formatSizeMB(chartData.sumMB, chartData.sumLo) + '</span>');
	}

	function simulateMouseEvent()
	{
		if (mouseOverIndex > -1)
		{
			var env = $('#StatDialog_Chart').data('elycharts_env');
			$.elycharts.mousemanager.onMouseOverArea(env, false, mouseOverIndex, env.mouseAreas[mouseOverIndex]);
		}
		else
		{
			chartMouseExit()
		}
	}

	this.chooseRange = function(range)
	{
		curRange = range;
		updateRangeButtons();
		mouseOverIndex = -1;
		redrawChart();
	}

	function updateRangeButtons()
	{
		$('#StatDialog_Toolbar .volume-range').removeClass('btn-inverse');
		$('#StatDialog_Volume_' + curRange + ',#StatDialog_Volume_' + curRange + '2,#StatDialog_Volume_' + curRange + '3').addClass('btn-inverse');
	}

	function updateServerList()
	{
		var menu = $('#StatDialog_ServerMenu');
		var menuItemTemplate = $('.volume-server-template', menu);
		var insertPos = $('#StatDialog_ServerMenuDivider', menu);

		$('.volume-server', menu).remove();
		for (var i=0; i < Status.status.NewsServers.length; i++)
		{
			var server = Status.status.NewsServers[i];
			var name = server.ID + '. ' + Status.serverName(server);
			var item = menuItemTemplate.clone().removeClass('volume-server-template hide').addClass('volume-server');
			var a = $('a', item);
			a.html('<i class="' + (i === curServer-1 ? 'icon-ok' : 'icon-empty') + '"></i>' + Util.textToHtml(name));
			a.attr('data-id', server.ID);
			a.click(chooseServer);
			insertPos.before(item);
		}

		$('#StatDialog_ServerCap').text(curServer > 0 ? Status.serverName(Status.status.NewsServers[curServer-1]) : 'All news servers');
		$('#StatDialog_ServerMenuAll i').toggleClass('icon-ok', curServer === 0).toggleClass('icon-empty', curServer !== 0);
	}

	function chooseServer(server)
	{
		curServer = parseInt($(this).attr('data-id'));
		updateServerList();
		redrawChart();
	}

	function dayToDate(epochDay)
	{
		var dt = new Date(epochDay * 24*60*60 * 1000);
		dt = new Date(dt.getTime() + dt.getTimezoneOffset() * 60*1000);
		return dt;
	}

	function dateToDay(date)
	{
		var epochDay = Math.ceil((date.getTime() - date.getTimezoneOffset() * 60*1000) / (1000*24*60*60));
		return epochDay;
	}

	function updateMonthList()
	{
		monthListInitialized = true;

		var firstDay = servervolumes[0].FirstDay;
		var curDay = firstDay + servervolumes[0].DaySlot;
		var firstDt = dayToDate(firstDay);
		var curDt = dayToDate(curDay);

		var menu = $('#StatDialog_MonthMenu');
		var menuItemTemplate = $('.volume-month-template', menu);
		var insertPos = $('#StatDialog_MonthMenuYears', menu);

		$('.volume-month', menu).remove();

		// does computer running NZBGet has correct date (after 1-Jan-2013)?
		clockOK = firstDay > 0 && servervolumes[0].DaySlot > -1;

		if (!clockOK)
		{
			updatePeriod();
			return;
		}

		// show last three months in the menu
		firstDt.setDate(1);
		var monDt = new Date(curDt.getTime());
		monDt.setDate(1);
		for (var i=0; i<3; i++)
		{
			if (monDt < firstDt)
			{
				break;
			}

			var name = monthNames[monDt.getMonth()] + ' ' + monDt.getFullYear();
			var monId = '' + monDt.getFullYear() + '-' + monDt.getMonth();

			if (curMonth === null)
			{
				curMonth = monId;
			}

			var item = menuItemTemplate.clone().removeClass('volume-month-template hide').addClass('volume-month');
			var a = $('a', item);
			a.html('<i class="' + (monId === curMonth ? 'icon-ok' : 'icon-empty') + '"></i>' + name);
			a.attr('data-id', monId);
			a.click(chooseMonth);
			insertPos.before(item);

			monDt.setMonth(monDt.getMonth() - 1);
		}

		// show last two years in the menu
		var insertPos = $('#StatDialog_MonthMenuDivider', menu);
		firstDt.setMonth(0);
		monDt = new Date(curDt.getTime());
		monDt.setDate(1);
		monDt.setMonth(0);
		for (var i=0; i<2; i++)
		{
			if (monDt < firstDt)
			{
				break;
			}

			var name = monDt.getFullYear();
			var monId = '' + monDt.getFullYear();

			var item = menuItemTemplate.clone().removeClass('volume-month-template hide').addClass('volume-month');
			var a = $('a', item);
			a.html('<i class="' + (monId === curMonth  ? 'icon-ok' : 'icon-empty') + '"></i>' + name);
			a.attr('data-id', monId);
			a.click(chooseMonth);
			insertPos.before(item);

			monDt.setFullYear(monDt.getFullYear() - 1);
		}

		updatePeriod();
	}

	function updatePeriod()
	{
		if (!clockOK)
		{
			monStartDate = new Date(2000, 1);
			monStartIndex = -1;
			monEndIndex = -1;
			return;
		}

		var cap;
		var monStart;
		var monEnd;

		monYear = curMonth.indexOf('-') === -1;
		if (monYear)
		{
			cap = curMonth;
			var year = parseInt(curMonth);
			monStart = new Date(year, 0);
			monEnd = new Date(year, 11, 31);
		}
		else
		{
			var month = parseInt(curMonth.substr(5, 2));
			var year = parseInt(curMonth.substring(0, 4));
			cap = monthNames[month] + ' ' + year;
			monStart = new Date(year, month);
			monEnd = new Date(year, month + 1);
			monEnd.setDate(0);
		}

		$('#StatDialog_Volume_MONTH, #StatDialog_Volume_MONTH2').text(cap);

		monStartDate = monStart;
		var firstDay = servervolumes[0].FirstDay;
		monStart = dateToDay(monStart);
		monEnd = dateToDay(monEnd);
		monStartIndex = monStart - firstDay;
		monEndIndex = monEnd - firstDay;
	}

	function updateCounters()
	{
		$StatDialog_TodaySize.html(Util.formatSizeMB(Status.status.DaySizeMB, Status.status.DaySizeLo));
		$StatDialog_MonthSize.html(Util.formatSizeMB(Status.status.MonthSizeMB, Status.status.MonthSizeLo));
		$StatDialog_AllTimeSize.html(Util.formatSizeMB(servervolumes[curServer].TotalSizeMB, servervolumes[curServer].TotalSizeLo));
		$StatDialog_CustomSize.html(Util.formatSizeMB(servervolumes[curServer].CustomSizeMB, servervolumes[curServer].CustomSizeLo));
		$StatDialog_Custom.attr('title', 'reset on ' + Util.formatDateTime(servervolumes[curServer].CustomTime));
	}

	function chooseMonth()
	{
		setMonth($(this).attr('data-id'));
	}

	function setMonth(month)
	{
		curRange = 'MONTH';
		curMonth = month;
		updateRangeButtons();
		updateMonthList();
		redrawChart();
	}

	this.chooseOtherMonth = function()
	{
		$StatRangeDialog_PeriodInput.val('');
		redrawLock++;
		$StatRangeDialog.modal({backdrop: false});
	}

	function StatRangeDialogHidden()
	{
		redrawLock--;
		StatDialog.redraw();
	}

	this.setPeriod = function()
	{
		var period = $StatRangeDialog_PeriodInput.val();
		if (period.indexOf('-') === -1)
		{
			var year = parseInt(period);
			if (year < 2013 || year > 2050)
			{
				PopupNotification.show('#Notif_StatRangeError');
				return;
			}
			period = '' + year;
		}
		else
		{
			var month = parseInt(period.substr(5, 2));
			var year = parseInt(period.substring(0, 4));
			if (year < 2013 || year > 2050 || month < 1 || month > 12)
			{
				PopupNotification.show('#Notif_StatRangeError');
				return;
			}
			period = year + '-' + (month-1);
		}

		$StatRangeDialog.modal('hide');
		setMonth(period);
	}

	this.resetCounter = function()
	{
		$('#StatDialogResetConfirmDialog_Server').text(curServer === 0 ? 'all news servers' : $('#StatDialog_ServerCap').text());
		$('#StatDialogResetConfirmDialog_Time').text(Util.formatDateTime(servervolumes[curServer].CustomTime));
		ConfirmDialog.showModal('StatDialogResetConfirmDialog', doResetCounter);
	}

	function doResetCounter()
	{
		RPC.call('resetservervolume', [curServer === 0 ? -1 : curServer, 'CUSTOM'], function()
		{
			PopupNotification.show('#Notif_StatReset');
			Refresher.update();
		});
	}
}(jQuery));


/*** LIMIT DIALOG *******************************************************/

var LimitDialog = (new function($)
{
	'use strict'

	// Controls
	var $LimitDialog;
	var $ServerTable;
	var $LimitDialog_SpeedInput;

	// State
	var changed;

	this.init = function()
	{
		$LimitDialog = $('#LimitDialog');
		$LimitDialog_SpeedInput = $('#LimitDialog_SpeedInput');
		$('#LimitDialog_Save').click(save);
		$ServerTable = $('#LimitDialog_ServerTable');

		$ServerTable.fasttable(
			{
				pagerContainer: '#LimitDialog_ServerTable_pager',
				rowSelect: UISettings.rowSelect,
				pageSize: 100
			});

		if (UISettings.setFocus)
		{
			$LimitDialog.on('shown', function()
			{
				$('#LimitDialog_SpeedInput').focus();
			});
		}

		$LimitDialog.on('hidden', function()
		{
			// cleanup
			$ServerTable.fasttable('update', []);
		});
	}

	this.clicked = function(e)
	{
		if (e.metaKey || e.ctrlKey)
		{
			toggleLimit();
		}
		else
		{
			showModal();
		}
	}

	function showModal()
	{
		var rate = Util.round0(Status.status.DownloadLimit / 1024);
		$LimitDialog_SpeedInput.val(rate > 0 ? rate : '');
		updateTable();
		$LimitDialog.modal({backdrop: 'static'});
	}

	function updateTable()
	{
		var data = [];
		for (var i=0; i < Status.status.NewsServers.length; i++)
		{
			var server = Status.status.NewsServers[i];
			var name = Status.serverName(server);
			var fields = ['<div class="check img-check"></div>', server.ID + '. ' + name];
			var item =
			{
				id: server.ID,
				fields: fields,
			};
			data.push(item);

			$ServerTable.fasttable('checkRow', server.ID, server.Active);
		}
		$ServerTable.fasttable('update', data);
		Util.show('#LimitDialog_ServerBlock', data.length > 0);
	}

	function save(e)
	{
		var val = $LimitDialog_SpeedInput.val();
		var rate = 0;
		if (val == '')
		{
			rate = 0;
		}
		else
		{
			rate = parseInt(val);
			if (isNaN(rate))
			{
				return;
			}
		}

		var checkedRows = $ServerTable.fasttable('checkedRows');
		var servers = [];

		for (var i=0; i < Status.status.NewsServers.length; i++)
		{
			var server = Status.status.NewsServers[i];
			var selected = checkedRows[server.ID] !== undefined;
			if (server.Active != selected)
			{
				servers.push([server.ID, selected]);
			}
		}

		saveLimit(rate, servers);
	}

	function saveLimit(rate, servers)
	{
		function saveServers()
		{
			if (servers.length > 0)
			{
				changed = true;
				RPC.call('editserver', servers, function()
				{
					completed();
				});
			}
			else
			{
				completed();
			}
		}

		changed = false;
		var oldRate = Util.round0(Status.status.DownloadLimit / 1024);

		if (rate != oldRate)
		{
			changed = true;
			RPC.call('rate', [rate], function()
			{
				saveServers();
			});
		}
		else
		{
			saveServers();
		}
	}

	function completed()
	{
		$LimitDialog.modal('hide');
		if (changed)
		{
			PopupNotification.show('#Notif_SetSpeedLimit');
		}
		Refresher.update();
	}

	function toggleLimit()
	{
		var limited = Status.status.DownloadLimit > 0;
		for (var i=0; i < Status.status.NewsServers.length; i++)
		{
			var server = Status.status.NewsServers[i];
			limited = limited || !server.Active;
		}

		var defRate = Options.option('DownloadRate');
		var rate = limited ? 0 : parseInt(defRate === '' ? 0 : defRate);
		var servers = [];

		for (var i=0; i < Status.status.NewsServers.length; i++)
		{
			var server = Status.status.NewsServers[i];
			var defActive = Options.option('Server' + (i + 1) + '.Active') === 'yes';
			var newActive = limited ? true : defActive;
			if (server.Active != newActive)
			{
				servers.push([server.ID, newActive]);
			}
		}

		saveLimit(rate, servers);
	}
}(jQuery));

/*** FILTER MENU *********************************************************/

var FilterMenu = (new function($)
{
	'use strict';

	var $SaveFilterDialog;
	var $SaveFilterInput;
	var $Table_filter;
	var ignoreClick = false;
	var $Table_filter;
	var tabName;
	var items;

	this.init = function()
	{
		$SaveFilterDialog = $('#SaveFilterDialog');
		$SaveFilterInput = $('#SaveFilterInput');

		if (UISettings.setFocus)
		{
			$SaveFilterDialog.on('shown', function ()
			{
				$SaveFilterInput.focus();
			});
		}
	}

	this.setTab = function(tabname)
	{
		tabName = tabname;
		$Table_filter = $('#' + tabName + 'Table_filter');
		load();
	}

	this.redraw = function()
	{
		var menu = $('#FilterMenu');
		var menuItemTemplate = $('.filter-menu-template', menu);
		var insertPos = $('#FilterMenu_Divider', menu);

		$('.filter-menu', menu).remove();

		for (var i = 0; i < items.length; i++)
		{
			var name = items[i].name;
			var item = menuItemTemplate.clone().removeClass('filter-menu-template').removeClass('hide').addClass('filter-menu');
			var t = $('span', item);
			t.text(name);
			var a = $('a', item);
			a.click(applyFilter);
			a.attr('data-id', i);
			var im = $('button', item);
			im.click(deleteFilter);
			im.attr('data-id', i);
			insertPos.before(item);
		}

		Util.show('#FilterMenu_Empty', items.length === 0);

		if (UISettings.miniTheme)
		{
			Frontend.alignPopupMenu('#FilterMenu');
		}
	}

	function applyFilter()
	{
		if (ignoreClick)
		{
			ignoreClick = false;
			return;
		}
		var id = parseInt($(this).attr('data-id'));
		$Table_filter.val(items[id].filter);
		$('#' + tabName +'Table').fasttable('applyFilter', $Table_filter.val());
	}

	function deleteFilter()
	{
		ignoreClick = true;
		var id = parseInt($(this).attr('data-id'));
		items.splice(id, 1);
		save();
	}

	this.saveDialogClick = function()
	{
		if ($Table_filter.val() === '')
		{
			PopupNotification.show('#Notif_SaveFilterEmpty');
			return;
		}

		var filter = $Table_filter.val();
		var name = filter;

		// reuse the name if the filter already exists
		for (var i = 0; i < items.length; i++)
		{
			if (items[i].filter === filter)
			{
				name = items[i].name;
				break;
			}
		}

		$SaveFilterInput.val(name);
		$SaveFilterDialog.modal();
	}

	this.saveClick = function()
	{
		$SaveFilterDialog.modal('hide');

		var name = $SaveFilterInput.val();
		var filter = $Table_filter.val();

		// rename if already exists
		for (var i = 0; i < items.length; i++)
		{
			if (items[i].filter === filter)
			{
				items[i].name = name;
				save();
				return;
			}
		}

		// doesn't exist - add new
		items.push({name: name, filter: filter});
		save();
	}

	function load()
	{
		items = JSON.parse(UISettings.read('Filter_' + tabName, '[]'));
	}

	function save()
	{
		UISettings.write('Filter_' + tabName, JSON.stringify(items));
	}
}(jQuery));
