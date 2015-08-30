/*
 * This file is part of nzbget
 *
 * Copyright (C) 2012-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
	var infos = [];
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
			infos = [];
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
		
		AddParamDialog.init();
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
			var html = '<a class="link-black" href="#" onclick="Upload.renameClick(' + files.length + ')" title="Click to rename">'+
				'<table><tr><td width="18px" valign="top"><i class="icon-file" style="vertical-align:top;margin-top:2px;">'+
				'</i><img class="hide" style="vertical-align:top;margin-top:1px;" src="img/transmit-file.gif" width="16px" height="16px"></td>'+
				'<td id="AddDialog_File' + files.length + '">' + Util.formatNZBName(filename) + '</td></tr></table></a>';
			$('#AddDialog_Files').append(html);
			files.push(file);

			var p = filename.lastIndexOf('.');
			var name = p > -1 ? filename.substr(0, p) : filename;
			var ext = p > -1 ? filename.substr(filename.lastIndexOf('.')) : '';
			var info = { name: name, ext: ext, password: '', dupekey: '', dupescore: 0 };
			infos.push(info);
		}
		$('#AddDialog_Files').show();
		$('#AddDialog_FilesHelp').hide();
	}

	this.addClick = function()
	{
		showModal();
	}
	
	this.renameClick = function(no)
	{
		var file = files[no];
		var info = infos[no];
		AddParamDialog.showModal(info, function() {
			$('#AddDialog_File' + no).html(Util.formatNZBName(info.name + info.ext));
		});
	}

	function showModal(droppedFiles)
	{
		Refresher.pause();

		$('#AddDialog_Files').empty();
		$('#AddDialog_URL').val('');
		$('#AddDialog_FilesHelp').show();
		$('#AddDialog_URLLabel img').hide();
		$('#AddDialog_URLLabel i').hide();
		$('#AddDialog_Paused').prop('checked', false);
		$('#AddDialog_DupeForce').prop('checked', false);
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
		var info = infos[index];

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
			var filename = info.name + info.ext;
			var addPaused = $('#AddDialog_Paused').is(':checked');
			var dupeMode = $('#AddDialog_DupeForce').is(':checked') ? "FORCE" : "SCORE";
			var params = info.password === '' ? [] : [{'*Unpack:password' : info.password}];
			RPC.call('append', [filename, base64str, category, priority, false, addPaused, info.dupekey, info.dupescore, dupeMode, params], fileCompleted, fileFailure);
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
		var failure = result < 0 || (result == 0 && Options.option('ScanScript') === '');
		errors |= failure;
		needRefresh |= !failure;
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
		var addPaused = $('#AddDialog_Paused').is(':checked');
		var dupeMode = $('#AddDialog_DupeForce').is(':checked') ? "FORCE" : "SCORE";

		RPC.call('append', ['', url, category, priority, false, addPaused, '', 0, dupeMode], urlCompleted, urlFailure);
	}

	function urlCompleted(result)
	{
		var failure = result <= 0;
		errors |= failure;
		needRefresh |= !failure;
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

/*** ADD FILE PARAM DIALOG *******************************************************/

var AddParamDialog = (new function($)
{
	'use strict'

	// Controls
	var $AddParamDialog;
	var $AddParamDialog_NZBName;
	var $AddParamDialog_Password;
	var $AddParamDialog_DupeKey;
	var $AddParamDialog_DupeScore;
	
	// State
	var info;
	var saveCallback;

	this.init = function()
	{
		$AddParamDialog = $('#AddParamDialog');
		$AddParamDialog_NZBName = $('#AddParamDialog_NZBName');
		$AddParamDialog_Password = $('#AddParamDialog_Password');
		$AddParamDialog_DupeKey = $('#AddParamDialog_DupeKey');
		$AddParamDialog_DupeScore = $('#AddParamDialog_DupeScore');
		
		$('#AddParamDialog_Save').click(save);

		$AddParamDialog.on('shown', function()
		{
			$('.modal-backdrop').last().addClass('modal-2');
			if (UISettings.setFocus)
			{
				$('#AddParamDialog_NZBName').focus();
			}
		});

		$AddParamDialog.on('hidden', function()
		{
			$('.modal-backdrop').removeClass('modal-2');
		});
	}

	this.showModal = function(_info, _saveCallback)
	{
		info = _info;
		saveCallback = _saveCallback;

		$AddParamDialog_NZBName.val(info.name);
		$AddParamDialog_Password.val(info.password);
		$AddParamDialog_DupeKey.val(info.dupekey);
		$AddParamDialog_DupeScore.val(info.dupescore);

		$AddParamDialog.modal({backdrop: 'static'});
	}

	function save(e)
	{
		info.name = $AddParamDialog_NZBName.val();
		info.password = $AddParamDialog_Password.val();
		info.dupekey = $AddParamDialog_DupeKey.val();
		info.dupescore = parseInt($AddParamDialog_DupeScore.val());

		$AddParamDialog.modal('hide');
		saveCallback(info);
	}
}(jQuery));
