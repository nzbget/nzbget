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

/* controls */
var status_SpeedLimitInput;
var status_CHPauseDownload;
var status_CHPausePostProcess;
var status_CHPauseScan;
var status_CHSoftPauseDownload;
var status_StatusPausing;
var status_StatusPaused;
var status_StatusSoftPaused;
var status_StatusLeft;
var status_StatusSpeed;
var status_StatusSpeedIcon;
var status_StatusTime;
var status_StatusURLs;
var status_PlayBlock;
var status_PlayButton;
var status_PauseButton;
var status_PlayAnimation;
var status_CurSpeedLimit;
var status_CurSpeedLimitBlock;

var status_LastPlayState = 0;
var status_LastAnimState = 0;
var status_PlayInitialized = false;
var status_limit_dialog;
var status_PlayFrame = 0;
var status_PlayFrameSize = 40;
var status_PlayState = 0;
var status_PlayStep = 0;
var status_LastSoftPauseState = 0;

function status_init()
{
	status_SpeedLimitInput = $('#SpeedLimitInput');
	status_CHPauseDownload = $('#CHPauseDownload');
	status_CHPausePostProcess = $('#CHPausePostProcess');
	status_CHPauseScan = $('#CHPauseScan');
	status_CHSoftPauseDownload = $('#CHSoftPauseDownload');
	status_PlayBlock = $('#PlayBlock');
	status_PlayButton = $('#PlayButton');
	status_PauseButton = $('#PauseButton');
	status_PlayAnimation = $('#PlayAnimation');
	status_StatusPausing = $('#StatusPausing');
	status_StatusPaused = $('#StatusPaused');
	status_StatusSoftPaused = $('#StatusSoftPaused');
	status_StatusLeft = $('#StatusLeft');
	status_StatusSpeed = $('#StatusSpeed');
	status_StatusSpeedIcon = $('#StatusSpeedIcon');
	status_StatusTime = $('#StatusTime');
	status_StatusURLs = $('#StatusURLs');
	status_CurSpeedLimit = $('#CurSpeedLimit');
	status_CurSpeedLimitBlock = $('#CurSpeedLimitBlock');

	$('#PlayMenu li[data] a').click(status_Pause_click);

	status_limit_dialog = $('#LimitDialog');

	status_PlayAnimation.hover(function() { status_PlayBlock.addClass('hover'); }, function() { status_PlayBlock.removeClass('hover'); });
}

function status_update()
{
	rpc('status', [], status_loaded);
}

function status_loaded(status)
{
	Status = status;
	loadNext();
}

function status_redraw()
{
	status_statistics();
	status_info()
}

function status_statistics()
{
	var content = '';

	content += '<tr><td>NZBGet version</td><td class="text-right">' + nzbgetVersion + '</td></tr>';
	content += '<tr><td>Uptime</td><td class="text-right">' + FormatTimeHMS(Status.UpTimeSec) + '</td></tr>';
	content += '<tr><td>Download time</td><td class="text-right">' + FormatTimeHMS(Status.DownloadTimeSec) + '</td></tr>';
	content += '<tr><td>Total downloaded</td><td class="text-right">' + FormatSizeMB(Status.DownloadedSizeMB) + '</td></tr>';
	content += '<tr><td>Remaining</td><td class="text-right">' + FormatSizeMB(Status.RemainingSizeMB) + '</td></tr>';
	content += '<tr><td>Free disk space</td><td class="text-right">' + FormatSizeMB(Status.FreeDiskSpaceMB) + '</td></tr>';
	content += '<tr><td>Average download speed</td><td class="text-right">' + round0(Status.AverageDownloadRate / 1024) + ' KB/s</td></tr>';
	content += '<tr><td>Current download speed</td><td class="text-right">' + round0(Status.DownloadRate / 1024) + ' KB/s</td></tr>';
	content += '<tr><td>Current speed limit</td><td class="text-right">' + round0(Status.DownloadLimit / 1024) + ' KB/s</td></tr>';

	$('#StatisticsTable tbody').html(content);

	content = '';
	content += '<tr><td>Download</td><td class="text-right">';
	if (Status.DownloadPaused || Status.Download2Paused)
	{
		content += Status.Download2Paused ? '<span class="label label-status label-warning">paused</span>' : '';
		content += Status.Download2Paused && Status.DownloadPaused ? ' + ' : '';
		content += Status.DownloadPaused ? '<span class="label label-status label-warning">soft-paused</span>' : '';
	}
	else
	{
		content += '<span class="label label-status label-success">active</span>';
	}
	content += '</td></tr>';

	var option = config_FindOption(Config, 'PostProcess');
	content += '<tr><td>Post-processing</td><td class="text-right">' + (option && option.Value === '' ?
		'<span class="label label-status">disabled</span>' :
		(Status.PostPaused ?
		'<span class="label label-status label-warning">paused</span>' :
		'<span class="label label-status label-success">active</span>')) +
		'</td></tr>';

	option = config_FindOption(Config, 'NzbDirInterval');
	content += '<tr><td>NZB-Directory scan</td><td class="text-right">' + (option && option.Value === '0' ?
		'<span class="label label-status">disabled</span>' :
		(Status.ScanPaused ?
		'<span class="label label-status label-warning">paused</span>' :
		'<span class="label label-status label-success">active</span>')) +
		'</td></tr>';

	content += '</tbody>';
	content += '</table>';

	$('#StatusTable tbody').html(content);
}

function status_info()
{
	show(status_CHPauseDownload, Status.Download2Paused);
	show(status_CHPausePostProcess, Status.PostPaused);
	show(status_CHPauseScan, Status.ScanPaused);
	show(status_CHSoftPauseDownload, Status.DownloadPaused);

	status_updatePlayAnim();
	status_updatePlayButton();

	if (Status.ServerStandBy)
	{
		status_StatusSpeed.html('--- KB/s');
		if (Status.RemainingSizeHi > 0 || Status.RemainingSizeLo > 0)
		{
			if (Status.AverageDownloadRate > 0)
			{
				status_StatusTime.html(FormatTimeLeft(Status.RemainingSizeMB*1024/(Status.AverageDownloadRate/1024)));
			}
			else
			{
				status_StatusTime.html('--h --m');
			}
		}
		else
		{
			status_StatusTime.html('0h 0m');
		}
	}
	else
	{
		status_StatusSpeed.html(round0(Status.DownloadRate / 1024) + ' KB/s');
		if (Status.DownloadRate > 0)
		{
			status_StatusTime.html(FormatTimeLeft(Status.RemainingSizeMB*1024/(Status.DownloadRate/1024)));
		}
		else
		{
			status_StatusTime.html('--h --m');
		}
	}


	status_StatusSpeedIcon.toggleClass('icon-plane', Status.DownloadLimit === 0);
	status_StatusSpeedIcon.toggleClass('icon-truck', Status.DownloadLimit !== 0);
}

function status_updatePlayButton()
{
	var SoftPause = Status.DownloadPaused && (!status_LastAnimState || !Settings_PlayAnimation);
	if (SoftPause !== status_LastSoftPauseState)
	{
		status_LastSoftPauseState = SoftPause;
		$('#PauseButton').removeClass('img-download-green').removeClass('img-download-green-orange').
			addClass(SoftPause ? 'img-download-green-orange' : 'img-download-green');
		$('#PlayButton').removeClass('img-download-orange').removeClass('img-download-orange-orange').
			addClass(SoftPause ? 'img-download-orange-orange' : 'img-download-orange');
	}

	var Play = !Status.Download2Paused;
	if (Play === status_LastPlayState)
	{
		return;
	}

	status_LastPlayState = Play;

	var hideBtn = Play ? $('#PlayButton') : $('#PauseButton');
	var showBtn = !Play ? $('#PlayButton') : $('#PauseButton');

	if (status_PlayInitialized)
	{
		hideBtn.fadeOut(500);
		showBtn.fadeIn(500);
		if (!Play && !Status.ServerStandBy)
		{
			animateAlert('#Notif_Downloads_Pausing');
		}
	}
	else
	{
		hideBtn.hide();
		showBtn.show();
	}

	if (Play)
	{
		status_PlayAnimation.removeClass('pause').addClass('play');
	}
	else
	{
		status_PlayAnimation.removeClass('play').addClass('pause');
	}

	status_PlayInitialized = true;
}

function status_updatePlayAnim()
{
	// Animate if either any downloads or post-processing is in progress
	var Anim = (!Status.ServerStandBy || (Status.PostJobCount > 0 && !Status.PostPaused)) &&
		(Settings_RefreshInterval !== 0) && !State_ConnectionError;
	if (Anim === status_LastAnimState)
	{
		return;
	}

	status_LastAnimState = Anim;

	if (Settings_PlayAnimation)
	{
		if (Anim)
		{
			status_PlayAnimation.fadeIn(1000);
		}
		else
		{
			status_PlayAnimation.fadeOut(1000);
		}
	}
}

function status_Play_click()
{
	//animateAlert('#Notif_Play');

	if (status_LastPlayState)
	{
		// pause all activities
		rpc('pausedownload2', [],
			function(){rpc('pausepost', [],
			function(){rpc('pausescan', [], refresh_update)})});
	}
	else
	{
		// resume all activities
		rpc('resumedownload2', [],
			function(){rpc('resumepost', [],
			function(){rpc('resumescan', [], refresh_update)})});
	}
}

function status_setSpeedLimit_click()
{
	var val = status_SpeedLimitInput.val();
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
	rpc('rate', [rate], function()
	{
		$('#LimitDialog').modal('hide');
		animateAlert('#Notif_SetSpeedLimit');
		refresh_update();
	});
}

function status_Pause_click(data)
{
	switch (data)
	{
		case 'download2':
			var method = Status.Download2Paused ? 'resumedownload2' : 'pausedownload2';
			break;
		case 'post':
			var method = Status.PostPaused ? 'resumepost' : 'pausepost';
			break;
		case 'scan':
			var method = Status.ScanPaused ? 'resumescan' : 'pausescan';
			break;
		case 'download':
			var method = Status.DownloadPaused ? 'resumedownload' : 'pausedownload';
			break;
	}
	rpc(method, [], refresh_update);
}

function status_limit_click()
{
	status_SpeedLimitInput.val('');
	status_CurSpeedLimit.text(Status.DownloadLimit === 0 ? 'none' : round0(Status.DownloadLimit / 1024) + ' KB/s');
	show(status_CurSpeedLimitBlock, Status.DownloadLimit !== 0);
	status_limit_dialog.modal();
}

function status_PlayRotate()
{
	// animate next frame
	status_PlayFrame++;

	if (status_PlayFrame >= status_PlayFrameSize)
	{
		status_PlayFrame = 0;
	}

	var f = status_PlayFrame <= status_PlayFrameSize ? status_PlayFrame : 0;

	var degree = 360 * f / status_PlayFrameSize;

	status_PlayAnimation.css({
		'-webkit-transform': 'rotate(' + degree + 'deg)',
		   '-moz-transform': 'rotate(' + degree + 'deg)',
			'-ms-transform': 'rotate(' + degree + 'deg)',
			 '-o-transform': 'rotate(' + degree + 'deg)',
				'transform': 'rotate(' + degree + 'deg)'
	});


	var extra = '';
	status_PlayStep++;

	if (status_PlayState === 0)
	{
		status_PlayFrameSize -= 0.2;

		// fading in
		if (status_PlayStep <= 20)
		{
			status_PlayAnimation.css('opacity', status_PlayStep / 20);
		}

		// fastening
		if (status_PlayFrameSize < 20)
		{
			status_PlayFrameSize = 20;
			status_PlayState++;
			status_PlayStep = 0;
		}
	}
	else if (status_PlayState === 1)
	{
		if (status_PlayStep > 100)
		{
			status_PlayState++;
			status_PlayStep = 0;
			//status_PlayState = 4;
		}
	}
	else if (status_PlayState === 2)
	{
		status_PlayFrameSize += 0.2;
		// slowing
		if (status_PlayFrameSize > 50)
		{
			status_PlayFrameSize = 50;
			status_PlayState++;
			status_PlayStep = 0;
		}
	}
	else if (status_PlayState === 3)
	{
		if (status_PlayStep > 100)
		{
			status_PlayState++;
			status_PlayStep = 0;
		}
	}
	else if (status_PlayState === 4)
	{
		// fading out
		if (status_PlayStep <= 50)
		{
			status_PlayAnimation.css('opacity', (50 - status_PlayStep) / 50);
		}

		if (status_PlayStep > 100)
		{
			status_PlayState = 0;
			status_PlayStep = 0;
		}
	}
}

