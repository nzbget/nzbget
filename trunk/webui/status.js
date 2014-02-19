/*
 * This file is part of nzbget
 *
 * Copyright (C) 2012-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *   1) Status Infos on main page (speed, time, paused state etc.);
 *   2) Statistics and Status dialog;
 *   3) Limit dialog (speed and active news servers).
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
	var $StatusPausing;
	var $StatusPaused;
	var $StatusLeft;
	var $StatusSpeed;
	var $StatusSpeedIcon;
	var $StatusTimeIcon;
	var $StatusTime;
	var $StatusURLs;
	var $PlayBlock;
	var $PlayButton;
	var $PauseButton;
	var $PlayAnimation;
	var $StatDialog;
	var $ScheduledPauseDialog;
	var $PauseForInput;

	// State
	var status;
	var lastPlayState = 0;
	var lastAnimState = 0;
	var playInitialized = false;
	var modalShown = false;

	this.init = function()
	{
		$CHPauseDownload = $('#CHPauseDownload');
		$CHPausePostProcess = $('#CHPausePostProcess');
		$CHPauseScan = $('#CHPauseScan');
		$PlayBlock = $('#PlayBlock');
		$PlayButton = $('#PlayButton');
		$PauseButton = $('#PauseButton');
		$PlayAnimation = $('#PlayAnimation');
		$StatusPausing = $('#StatusPausing');
		$StatusPaused = $('#StatusPaused');
		$StatusLeft = $('#StatusLeft');
		$StatusSpeed = $('#StatusSpeed');
		$StatusSpeedIcon = $('#StatusSpeedIcon');
		$StatusTimeIcon = $('#StatusTimeIcon');
		$StatusTime = $('#StatusTime');
		$StatusURLs = $('#StatusURLs');
		$StatDialog = $('#StatDialog');
		$ScheduledPauseDialog = $('#ScheduledPauseDialog')
		$PauseForInput = $('#PauseForInput');
		
		if (UISettings.setFocus)
		{
			$LimitDialog.on('shown', function()
			{
				$('#SpeedLimitInput').focus();
			});
			$ScheduledPauseDialog.on('shown', function()
			{
				$('#PauseForInput').focus();
			});
		}

		$PlayAnimation.hover(function() { $PlayBlock.addClass('hover'); }, function() { $PlayBlock.removeClass('hover'); });
		
		// temporary pause the play animation if any modal is shown (to avoid artifacts in safari)
		$('body >.modal').on('show', modalShow);
		$('body > .modal').on('hide', modalHide);
	}

	this.update = function()
	{
		var _this = this;
		RPC.call('status', [], 
			function(curStatus)
			{
				status = curStatus;
				_this.status = status;
				RPC.next();
			});
	}

	this.redraw = function()
	{
		redrawStatistics();
		redrawInfo()
	}
	
	function redrawStatistics()
	{
		var content = '';

		content += '<tr><td>NZBGet version</td><td class="text-right">' + Options.option('Version') + '</td></tr>';
		content += '<tr><td>Uptime</td><td class="text-right">' + Util.formatTimeHMS(status.UpTimeSec) + '</td></tr>';
		content += '<tr><td>Download time</td><td class="text-right">' + Util.formatTimeHMS(status.DownloadTimeSec) + '</td></tr>';
		content += '<tr><td>Total downloaded</td><td class="text-right">' + Util.formatSizeMB(status.DownloadedSizeMB) + '</td></tr>';
		content += '<tr><td>Remaining</td><td class="text-right">' + Util.formatSizeMB(status.RemainingSizeMB) + '</td></tr>';
		content += '<tr><td>Free disk space</td><td class="text-right">' + Util.formatSizeMB(status.FreeDiskSpaceMB) + '</td></tr>';
		content += '<tr><td>Average download speed</td><td class="text-right">' + Util.round0(status.AverageDownloadRate / 1024) + ' KB/s</td></tr>';
		content += '<tr><td>Current download speed</td><td class="text-right">' + Util.round0(status.DownloadRate / 1024) + ' KB/s</td></tr>';
		content += '<tr><td>Current speed limit</td><td class="text-right">' + Util.round0(status.DownloadLimit / 1024) + ' KB/s</td></tr>';

		$('#StatisticsTable tbody').html(content);

		content = '';

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

		if (status.ResumeTime > 0)
		{
			content += '<tr><td>Autoresume</td><td class="text-right">' + Util.formatTimeHMS(status.ResumeTime - status.ServerTime) + '</td></tr>';
		}
			
		content += '</tbody>';
		content += '</table>';

		$('#StatusTable tbody').html(content);
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
			$StatusSpeed.html('--- KB/s');
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
			$StatusSpeed.html(Util.round0(status.DownloadRate / 1024) + ' KB/s');
			if (status.DownloadRate > 0)
			{
				$StatusTime.html(Util.formatTimeLeft(status.RemainingSizeMB*1024/(status.DownloadRate/1024)));
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
		$StatusTime.toggleClass('scheduled-resume', status.ServerStandBy && status.ResumeTime > 0);
		$StatusTimeIcon.toggleClass('icon-time', !(status.ServerStandBy && status.ResumeTime > 0));
		$StatusTimeIcon.toggleClass('icon-time-orange', status.ServerStandBy && status.ResumeTime > 0);
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
				Notification.show('#Notif_Downloads_Pausing');
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
		var Anim = (!status.ServerStandBy || status.FeedActive ||
			(status.PostJobCount > 0 && !status.PostPaused) ||
			(status.UrlCount > 0 && (!status.DownloadPaused || Options.option('UrlForce') === 'yes'))) &&
			(UISettings.refreshInterval !== 0) && !UISettings.connectionError;
		if (Anim === lastAnimState)
		{
			return;
		}

		lastAnimState = Anim;

		if (UISettings.activityAnimation && !modalShown)
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
		//Notification.show('#Notif_Play');

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
		$StatDialog.modal();
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
		$ScheduledPauseDialog.modal();
	}

	this.pauseForClick = function()
	{
		var val = $PauseForInput.val();
		var minutes = parseInt(val);

		if (isNaN(minutes) || minutes <= 0)
		{
			return;
		}
		
		$ScheduledPauseDialog.modal('hide');
		this.scheduledPauseClick(minutes * 60);
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
				pagerContainer: $('#LimitDialog_ServerTable_pager'),
				headerCheck: $('#LimitDialog_ServerTable > thead > tr:first-child'),
				hasHeader: false,
				pageSize: 100
			});

		$ServerTable.on('click', 'tbody div.check',
			function(event) { $ServerTable.fasttable('itemCheckClick', this.parentNode.parentNode, event); });
		$ServerTable.on('click', 'thead div.check',
			function() { $ServerTable.fasttable('titleCheckClick') });
		$ServerTable.on('mousedown', Util.disableShiftMouseDown);

		$LimitDialog.on('hidden', function()
		{
			// cleanup
			$ServerTable.fasttable('update', []);
		});
	}

	this.showModal = function()
	{
		changed = false;
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
			var name = Options.option('Server' + server.ID + '.Name');
			if (name === null || name === '')
			{
				var host = Options.option('Server' + server.ID + '.Host');
				var port = Options.option('Server' + server.ID + '.Port');
				name = (host === null ? '' : host) + ':' + (port === null ? '119' : port);
			}
			var fields = ['<div class="check img-check"></div>', server.ID + '. ' + name];
			var item =
			{
				id: server.ID,
				fields: fields,
				search: ''
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
	
	function saveServers()
	{
		var checkedRows = $ServerTable.fasttable('checkedRows');
		var command = [];
		
		for (var i=0; i < Status.status.NewsServers.length; i++)
		{
			var server = Status.status.NewsServers[i];
			var selected = checkedRows.indexOf(server.ID) > -1;
			if (server.Active != selected)
			{
				command.push([server.ID, selected]);
				changed = true;
			}
		}
		
		if (command.length > 0)
		{
			RPC.call('editserver', command, function()
			{
				completed();
			});
		}
		else
		{
			completed();
		}
	}
	
	function completed()
	{
		$LimitDialog.modal('hide');
		if (changed)
		{
			Notification.show('#Notif_SetSpeedLimit');
		}
		Refresher.update();
	}
}(jQuery));
