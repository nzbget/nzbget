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
 *   1) File upload dialog (local files, urls, scan);
 *   2) Drag-n-drop events handling on main page.
 */
 
/*** FILE UPLOAD (DRAG-N-DROP, URLS, SCAN) ************************************************/
 
var Upload = (new function($)
{
	'use strict';

	// Controls
	var $AddDialog;
	
	// State
	var dragin = false;
	var files = [];
	var filesSuccess = [];
	var index;
	var errors = false;
	var needRefresh = false;
	var failure_message = null;
	var url = '';

	this.init = function()
	{
		var target = $('#DownloadsTab')[0];
		target.addEventListener('dragenter', bodyDragOver);
		target.addEventListener('dragover', bodyDragOver);
		target.addEventListener('dragleave', bodyDragLeave);
		target.addEventListener('drop', bodyDrop, false);

		target = $('#AddDialog_Target')[0];
		target.addEventListener('dragenter', dialogDragOver);
		target.addEventListener('dragover', dialogDragOver);
		target.addEventListener('dragleave', dialogDragLeave);
		target.addEventListener('drop', dialogDrop, false);

		$AddDialog = $('#AddDialog');

		$AddDialog.on('hidden', function ()
		{
			Refresher.resume();
			files = [];
			filesSuccess = [];
			if (needRefresh)
			{
				Refresher.update();
			}
		});

		if (UISettings.setFocus)
		{
			$AddDialog.on('shown', function ()
			{
				if (files.length === 0)
				{
					$('#AddDialog_URL').focus();
				}
			});
		}

		$('#AddDialog_Select').click(selectFiles);
		$('#AddDialog_Submit').click(submit);
		$('#AddDialog_Input')[0].addEventListener("change", fileSelectHandler, false);
		$('#AddDialog_Scan').click(scan);
	}

	function bodyDragOver(event)
	{
		if ((event.dataTransfer.types.contains && event.dataTransfer.types.contains('Files')) ||
			(event.dataTransfer.types.indexOf && event.dataTransfer.types.indexOf('Files') > -1) ||
			(event.dataTransfer.files && event.dataTransfer.files.length > 0))
		{
			event.stopPropagation();
			event.preventDefault();

			if (!dragin)
			{
				dragin = true;
				$('body').addClass('dragover');
			}
		}
	}

	function bodyDragLeave(event)
	{
		dragin = false;
		$('body').removeClass('dragover');
	}

	function bodyDrop(event)
	{
		event.preventDefault();
		bodyDragLeave();

		if (!event.dataTransfer.files)
		{
			showDnDUnsupportedAlert();
			return;
		}

		showModal(event.dataTransfer.files);
	}

	function dialogDragOver(event)
	{
		event.stopPropagation();
		event.preventDefault();

		if (!dragin)
		{
			dragin = true;
			$('#AddDialog_Target').addClass('dragover');
		}
	}

	function dialogDragLeave(event)
	{
		dragin = false;
		$('#AddDialog_Target').removeClass('dragover');
	}

	function dialogDrop(event)
	{
		event.preventDefault();
		dialogDragLeave();

		if (!event.dataTransfer.files)
		{
			showDnDUnsupportedAlert();
			return;
		}

		addFiles(event.dataTransfer.files);
	}

	function showDnDUnsupportedAlert()
	{
		setTimeout(function()
		{
			alert("Unfortunately your browser doesn't support drag and drop for files.\n\nPlease use alternative ways to add files to queue:\nadd via URL or put the files directly into incoming nzb-directory.");
		}, 50);
	}

	function selectFiles()
	{
		var inp = $('#AddDialog_Input');

		// Reset file input control; needed for IE10 but produce problems with opera (the old non-webkit one).
		if ($.browser.msie)
		{
			inp.wrap('<form>').closest('form').get(0).reset();
			inp.unwrap();
		}
		
		inp.click();
	}

	function fileSelectHandler(event)
	{
		if (!event.target.files)
		{
			alert("Unfortunately your browser doesn't support direct access to local files.\n\nPlease use alternative ways to add files to queue:\nadd via URL or put the files directly into incoming nzb-directory.");
			return;
		}
		addFiles(event.target.files);
	}

	function addFiles(selectedFiles)
	{
		var list = '';
		for (var i = 0; i<selectedFiles.length; i++)
		{
			var file = selectedFiles[i];
			var filename = file.name.replace(/\.queued$/g, '');
			var html = '<table><tr><td width="18px" valign="top"><i class="icon-file" style="vertical-align:top;margin-top:2px;"></i><img class="hide" style="vertical-align:top;margin-top:1px;" src="img/transmit-file.gif" width="16px" height="16px"></td><td>' +
				Util.formatNZBName(filename) + '</td></tr></table>';
			$('#AddDialog_Files').append(html);
			files.push(file);
		}
		$('#AddDialog_Files').show();
		$('#AddDialog_FilesHelp').hide();
	}

	this.addClick = function()
	{
		showModal();
	}
	
	function showModal(droppedFiles)
	{
		Refresher.pause();

		$('#AddDialog_Files').empty();
		$('#AddDialog_URL').val('');
		$('#AddDialog_FilesHelp').show();
		$('#AddDialog_URLLabel img').hide();
		$('#AddDialog_URLLabel i').hide();
		enableAllButtons();

		var v = $('#AddDialog_Priority');
		DownloadsUI.fillPriorityCombo(v);
		v.val(0);
		DownloadsUI.fillCategoryCombo($('#AddDialog_Category'));

		files = [];
		filesSuccess = [];

		if (droppedFiles)
		{
			addFiles(droppedFiles);
		}

		$AddDialog.modal({backdrop: 'static'});
	}

	function disableAllButtons()
	{
		$('#AddDialog .modal-footer .btn, #AddDialog_Select, #AddDialog_Scan').attr('disabled', 'disabled');
	}

	function enableAllButtons()
	{
		$('#AddDialog .modal-footer .btn, #AddDialog_Select, #AddDialog_Scan').removeAttr('disabled');
		$('#AddDialog_Transmit').hide();
	}

	function submit()
	{
		disableAllButtons();

		if (files.length > 0)
		{
			if (!window.FileReader)
			{
				$AddDialog.modal('hide');
				alert("Unfortunately your browser doesn't support FileReader API.\n\nPlease use alternative ways to add files to queue:\nadd via URL or put the files directly into incoming nzb-directory.");
				return;
			}

			var testreader = new FileReader();
			if (!testreader.readAsBinaryString && !testreader.readAsDataURL)
			{
				$AddDialog.modal('hide');
				alert("Unfortunately your browser doesn't support neither \"readAsBinaryString\" nor \"readAsDataURL\" functions of FileReader API.\n\nPlease use alternative ways to add files to queue:\nadd via URL or put the files directly into incoming nzb-directory.");
				return;
			}
		}

		needRefresh = false;
		errors = false;
		failure_message = null;
		index = 0;
		url = $('#AddDialog_URL').val();

	/*
		setTimeout(function(){
			$('#AddDialog_Transmit').show();
		}, 500);
	*/

		if (url.length > 0)
		{
			urlNext();
		}
		else
		{
			fileNext();
		}
	}

	function fileNext()
	{
		if (index === files.length)
		{
			allCompleted();
			return;
		}

		var file = files[index];

		if (filesSuccess.indexOf(file) > -1)
		{
			// file already uploaded
			index++;
			setTimeout(next, 50);
			return;
		}

		$('#AddDialog_Files table:eq(' + index + ') img').show();
		$('#AddDialog_Files table:eq(' + index + ') i').hide();

		var reader = new FileReader();
		reader.onload = function (event)
		{
			var base64str;
			if (reader.readAsBinaryString)
			{
				base64str = window.btoa(event.target.result);
			}
			else
			{
				base64str = event.target.result.replace(/^data:[^,]+,/, '');
			}
			var category = $('#AddDialog_Category').val();
			var priority = parseInt($('#AddDialog_Priority').val());
			var filename = file.name.replace(/\.queued$/g, '');
			RPC.call('append', [filename, category, priority, false, base64str], fileCompleted, fileFailure);
		};

		if (reader.readAsBinaryString)
		{
			reader.readAsBinaryString(file);
		}
		else
		{
			reader.readAsDataURL(file);
		}
	}

	function fileCompleted(result)
	{
		errors |= !result;
		needRefresh |= result;
		if (result)
		{
			filesSuccess.push(files[index]);
		}
		$('#AddDialog_Files table:eq(' + index + ') img').hide();
		$('#AddDialog_Files table:eq(' + index + ') i').removeClass('icon-file').addClass(
			result ? 'icon-ok' : 'icon-remove').show();
		index++;
		fileNext();
	}

	function fileFailure(res)
	{
		failure_message = res;
		fileCompleted(false);
	}

	function urlNext()
	{
		$('#AddDialog_URLLabel img').show();
		$('#AddDialog_URLLabel i').hide();

		var category = $('#AddDialog_Category').val();
		var priority = parseInt($('#AddDialog_Priority').val());

		RPC.call('appendurl', ['', category, priority, false, url], urlCompleted, urlFailure);
	}

	function urlCompleted(result)
	{
		errors |= !result;
		needRefresh |= result;
		if (result)
		{
			$('#AddDialog_URL').empty();
		}
		$('#AddDialog_URLLabel img').hide();
		$('#AddDialog_URLLabel i').removeClass('icon-ok').removeClass('icon-remove').addClass(
			result ? 'icon-ok' : 'icon-remove').show();

		fileNext();
	}

	function urlFailure(res)
	{
		failure_message = res;
		urlCompleted(false);
	}

	function allCompleted()
	{
		if (errors)
		{
			enableAllButtons();
			// using timeout for browser to update UI (buttons) before showing the alert
			setTimeout(function()
			{
				if (failure_message)
				{
					alert((index > 1 ? 'One or more files' : 'The file') + ' could not be added to the queue:\n' + failure_message);
				}
				else
				{
					alert((index > 1 ? 'One or more files' : 'The file') + ' could not be added to the queue.\nPlease check the messages tab for any error messages.');
				}
				needRefresh = true;
			}, 100);
		}
		else
		{
			$AddDialog.modal('hide');
			if (index > 0)
			{
				Notification.show('#Notif_AddFiles');
			}
		}
	}

	function scan()
	{
		disableAllButtons();

		setTimeout(function(){
			$('#AddDialog_Transmit').show();
		}, 500);

		RPC.call('scan', [true], function()
		{
			needRefresh = true;
			$AddDialog.modal('hide');
			Notification.show('#Notif_Scan');
		});
	}
}(jQuery));
