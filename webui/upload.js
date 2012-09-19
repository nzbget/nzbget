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

var upload_dialog;
var upload_dragin = false;
var upload_files = [];
var upload_filesSuccess = [];
var upload_index;
var upload_errors = false;
var upload_needRefresh = false;
var upload_failure_message = null;
var upload_url = '';

function upload_init()
{
	var target = $('#DownloadsTab')[0];
	target.addEventListener('dragenter', upload_body_dragover);
	target.addEventListener('dragover', upload_body_dragover);
	target.addEventListener('dragleave', upload_body_dragleave);
	target.addEventListener('drop', upload_body_drop, false);

	target = $('#AddDialog_Target')[0];
	target.addEventListener('dragenter', upload_dialog_dragover);
	target.addEventListener('dragover', upload_dialog_dragover);
	target.addEventListener('dragleave', upload_dialog_dragleave);
	target.addEventListener('drop', upload_dialog_drop, false);

	upload_dialog = $('#AddDialog');

	upload_dialog.on('hidden', function ()
	{
		refresh_resume();
		upload_files = [];
		upload_filesSuccess = [];
		if (upload_needRefresh)
		{
			refresh_update();
		}
	});

	if (Settings_SetFocus)
	{
		upload_dialog.on('shown', function ()
		{
			if (upload_files.length === 0)
			{
				$('#AddDialog_URL').focus();
			}
		});
	}

	$('#AddDialog_Select').click(upload_Select);
	$('#AddDialog_Submit').click(upload_Submit);
	$('#AddDialog_Input')[0].addEventListener("change", upload_FileSelectHandler, false);
	$('#AddDialog_Scan').click(upload_Scan);
}

function upload_body_dragover(event)
{
	if ((event.dataTransfer.types.contains && event.dataTransfer.types.contains('Files')) ||
		(event.dataTransfer.types.indexOf && event.dataTransfer.types.indexOf('Files') > -1) ||
		(event.dataTransfer.files && event.dataTransfer.files.length > 0))
	{
		event.stopPropagation();
		event.preventDefault();

		if (!upload_dragin)
		{
			upload_dragin = true;
			$('body').addClass('dragover');
		}
	}
}

function upload_body_dragleave(event)
{
	upload_dragin = false;
	$('body').removeClass('dragover');
}

function upload_body_drop(event)
{
	event.preventDefault();
	upload_body_dragleave();

	if (!event.dataTransfer.files)
	{
		upload_DnDUnsupported();
		return;
	}

	upload_Show(event.dataTransfer.files);
}

function upload_dialog_dragover(event)
{
	event.stopPropagation();
	event.preventDefault();

	if (!upload_dragin)
	{
		upload_dragin = true;
		$('#AddDialog_Target').addClass('dragover');
	}
}

function upload_dialog_dragleave(event)
{
	upload_dragin = false;
	$('#AddDialog_Target').removeClass('dragover');
}

function upload_dialog_drop(event)
{
	event.preventDefault();
	upload_dialog_dragleave();

	if (!event.dataTransfer.files)
	{
		upload_DnDUnsupported();
		return;
	}

	upload_addFiles(event.dataTransfer.files);
}

function upload_DnDUnsupported()
{
	setTimeout(function()
	{
		alert("Unfortunately your browser doesn't support drag and drop for files.\n\nPlease use alternative ways to add files to queue:\nadd via URL or put the files directly into incoming nzb-directory.");
	}, 50);
}

function upload_Select()
{
	$('#AddDialog_Input').click();
}

function upload_FileSelectHandler(event)
{
	upload_addFiles(event.target.files);
}

function upload_addFiles(files)
{
	var list = '';
	for (var i = 0; i<files.length; i++)
	{
		var file = files[i];
		var html = '<table><tr><td width="18px" valign="top"><i class="icon-file" style="vertical-align:top;margin-top:2px;"></i><img class="hide" style="vertical-align:top;margin-top:1px;" src="img/transmit-file.gif"></td><td>' +
			FormatNZBName(file.name) + '</td></tr></table>';
		$('#AddDialog_Files').append(html);
		upload_files.push(file);
	}
	$('#AddDialog_Files').show();
	$('#AddDialog_FilesHelp').hide();
}

function upload_Show(files)
{
	refresh_pause();

	$('#AddDialog_Files').empty();
	$('#AddDialog_URL').val('');
	$('#AddDialog_FilesHelp').show();
	$('#AddDialog_URLLabel img').hide();
	$('#AddDialog_URLLabel i').hide();
	upload_EnableAllButtons();

	var v = $('#AddDialog_Priority');
	downloads_fillPriorityCombo(v);
	v.val(0);
	downloads_fillCategoryCombo($('#AddDialog_Category'));

	upload_files = [];
	upload_filesSuccess = [];

	if (files)
	{
		upload_addFiles(files);
	}

	upload_dialog.modal({backdrop: 'static'});
}

function upload_DisableAllButtons()
{
	$('#AddDialog .modal-footer .btn, #AddDialog_Select, #AddDialog_Scan').attr('disabled', 'disabled');
}

function upload_EnableAllButtons()
{
	$('#AddDialog .modal-footer .btn, #AddDialog_Select, #AddDialog_Scan').removeAttr('disabled');
	$('#AddDialog_Transmit').hide();
}

function upload_Submit()
{
	upload_DisableAllButtons();

	if (upload_files.length > 0)
	{
		if (!window.FileReader)
		{
			upload_dialog.modal('hide');
			alert("Unfortunately your browser doesn't support FileReader API.\n\nPlease use alternative ways to add files to queue:\nadd via URL or put the files directly into incoming nzb-directory.");
			return;
		}

		var testreader = new FileReader();
		if (!testreader.readAsBinaryString)
		{
			upload_dialog.modal('hide');
			alert("Unfortunately your browser doesn't support the function \"readAsBinaryString\" of FileReader API.\n\nPlease use alternative ways to add files to queue:\nadd via URL or put the files directly into incoming nzb-directory.");
			return;
		}
	}

	upload_needRefresh = false;
	upload_errors = false;
	upload_failure_message = null;
	upload_index = 0;
	upload_url = $('#AddDialog_URL').val();

/*
	setTimeout(function(){
		$('#AddDialog_Transmit').show();
	}, 500);
*/

	if (upload_url.length > 0)
	{
		upload_urlNext();
	}
	else
	{
		upload_fileNext();
	}
}

function upload_fileNext()
{
	if (upload_index === upload_files.length)
	{
		upload_allCompleted();
		return;
	}

	var file = upload_files[upload_index];

	if (upload_filesSuccess.indexOf(file) > -1)
	{
		// file already uploaded
		upload_index++;
		setTimeout(upload_next, 50);
		return;
	}

	$('#AddDialog_Files table:eq(' + upload_index + ') img').show();
	$('#AddDialog_Files table:eq(' + upload_index + ') i').hide();

	var reader = new FileReader();
	reader.onload = function (event)
	{
		var base64str = window.btoa(event.target.result);
		var category = $('#AddDialog_Category').val();
		rpc('append', [file.name, category, false, base64str], upload_fileCompleted, upload_fileFailure);
	};

	reader.readAsBinaryString(file);
}

function upload_fileCompleted(result)
{
	upload_errors |= !result;
	upload_needRefresh |= result;
	if (result)
	{
		upload_filesSuccess.push(upload_files[upload_index]);
	}
	$('#AddDialog_Files table:eq(' + upload_index + ') img').hide();
	$('#AddDialog_Files table:eq(' + upload_index + ') i').removeClass('icon-file').addClass(
		result ? 'icon-ok' : 'icon-remove').show();
	upload_index++;
	upload_fileNext();
}

function upload_fileFailure(res)
{
	upload_failure_message = res;
	upload_fileCompleted(false);
}

function upload_urlNext()
{
	$('#AddDialog_URLLabel img').show();
	$('#AddDialog_URLLabel i').hide();

	var category = $('#AddDialog_Category').val();
	var priority = parseInt($('#AddDialog_Priority').val());

	rpc('appendurl', ['', category, priority, false, upload_url], upload_urlCompleted, upload_urlFailure);
}

function upload_urlCompleted(result)
{
	upload_errors |= !result;
	upload_needRefresh |= result;
	if (result)
	{
		$('#AddDialog_URL').empty();
	}
	$('#AddDialog_URLLabel img').hide();
	$('#AddDialog_URLLabel i').removeClass('icon-ok').removeClass('icon-remove').addClass(
		result ? 'icon-ok' : 'icon-remove').show();

	upload_fileNext();
}

function upload_urlFailure(res)
{
	upload_failure_message = res;
	upload_urlCompleted(false);
}

function upload_allCompleted()
{
	if (upload_errors)
	{
		upload_EnableAllButtons();
		// using timeout for browser to update UI (buttons) before showing the alert
		setTimeout(function()
		{
			if (upload_failure_message)
			{
				alert((upload_index > 1 ? 'One or more files' : 'The file') + ' could not be added to the queue:\n' + upload_failure_message);
			}
			else
			{
				alert((upload_index > 1 ? 'One or more files' : 'The file') + ' could not be added to the queue.\nPlease check the messages tab for any error messages.');
			}
			upload_needRefresh = true;
		}, 100);
	}
	else
	{
		upload_dialog.modal('hide');
		if (upload_index > 0)
		{
			animateAlert('#Notif_AddFiles');
		}
	}
}

function upload_Scan()
{
	upload_DisableAllButtons();

	setTimeout(function(){
		$('#AddDialog_Transmit').show();
	}, 500);

	rpc('scan', [true], function()
	{
		upload_needRefresh = true;
		upload_dialog.modal('hide');
		animateAlert('#Notif_Scan');
	});
}
