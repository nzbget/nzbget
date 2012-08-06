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
	var content = '';

	content += '<tr><td>NZBGet version</td><td class="text-right">' + nzbgetVersion + '</td></tr>';
	content += '<tr><td>Uptime</td><td class="text-right">' + FormatTimeHMS(Status.UpTimeSec) + '</td></tr>';
	content += '<tr><td>Download time</td><td class="text-right">' + FormatTimeHMS(Status.DownloadTimeSec) + '</td></tr>';
	content += '<tr><td>Total downloaded</td><td class="text-right">' + FormatSizeMB(Status.DownloadedSizeMB) + '</td></tr>';
	content += '<tr><td>Remaining</td><td class="text-right">' + FormatSizeMB(Status.RemainingSizeMB) + '</td></tr>';
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
		
	content += '<tr><td>Post-processing</td><td class="text-right">' + (Status.PostPaused ? 
		'<span class="label label-status label-warning">paused</span>' : 
		'<span class="label label-status label-success">active</span>') + 
		'</td></tr>';
	content += '<tr><td>NZB-Directory scan</td><td class="text-right">' + (Status.ScanPaused ? 
		'<span class="label label-status label-warning">paused</span>' : 
		'<span class="label label-status label-success">active</span>') + 
		'</td></tr>';
	
	content += '</tbody>';
	content += '</table>';
	
	$('#StatusTable tbody').html(content);
}
