#!/bin/sh 
#
# This file if part of nzbget
#
# Cleanup post-processing script for NZBGet
#
# Copyright (C) 2008-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
# $Revision$
# $Date$
#


##############################################################################
### NZBGET POST-PROCESSING SCRIPT                                          ###

# Clean up the destination directory.

### NZBGET POST-PROCESSING SCRIPT                                          ###
##############################################################################


# NZBGet passes following arguments to postprocess-programm as environment
# variables:
#  NZBPP_DIRECTORY    - path to destination dir for downloaded files;
#  NZBPP_NZBNAME      - user-friendly name of processed nzb-file as it is displayed
#                       by the program. The file path and extension are removed.
#                       If download was renamed, this parameter reflects the new name;
#  NZBPP_NZBFILENAME  - name of processed nzb-file. It includes file extension and also
#                       may include full path;
#  NZBPP_CATEGORY     - category assigned to nzb-file (can be empty string);
#  NZBPP_PARSTATUS    - result of par-check:
#                       0 = not checked: par-check is disabled or nzb-file does
#                           not contain any par-files;
#                       1 = checked and failed to repair;
#                       2 = checked and successfully repaired;
#                       3 = checked and can be repaired but repair is disabled.
#  NZBPP_UNPACKSTATUS - result of unpack:
#                       0 = unpack is disabled or was skipped due to nzb-file
#                           properties or due to errors during par-check;
#                       1 = unpack failed;
#                       2 = unpack successful.


# Exit codes
POSTPROCESS_PARCHECK=92
POSTPROCESS_SUCCESS=93
POSTPROCESS_ERROR=94
POSTPROCESS_NONE=95

# Check if the script is called from nzbget 11.0 or later
if [ "$NZBOP_SCRIPTDIR" = "" ]; then
	echo "*** NZBGet post-processing script ***"
	echo "This script is supposed to be called from nzbget (11.0 or later)."
	exit $POSTPROCESS_ERROR
fi

echo "[DETAIL] Script successfully started"

cd "$NZBPP_DIRECTORY"

# Check nzbget.conf options
BadConfig=0

if [ "$NZBOP_UNPACK" != "yes" ]; then
	echo "[ERROR] Please enable option \"Unpack\" in nzbget configuration file"
	BadConfig=1
fi

if [ "$BadConfig" -eq 1 ]; then
	echo "[ERROR] Exiting due to incompatible nzbget configuration"
	exit $POSTPROCESS_ERROR
fi

# Check par status
if [ "$NZBPP_PARSTATUS" -eq 3 ]; then
	echo "[WARNING] Par-check successful, but Par-repair disabled, exiting"
	exit $POSTPROCESS_NONE
fi
if [ "$NZBPP_PARSTATUS" -eq 1 ]; then
	echo "[WARNING] Par-check failed, exiting"
	exit $POSTPROCESS_NONE
fi

# Check unpack status
if [ "$NZBPP_UNPACKSTATUS" -eq 1 ]; then
	echo "[WARNING] Unpack failed, exiting"
	exit $POSTPROCESS_NONE
fi
if [ "$NZBPP_UNPACKSTATUS" -eq 0 -a "$NZBPP_PARSTATUS" -ne 2 ]; then
	# Unpack is disabled or was skipped due to nzb-file properties or due to errors during par-check

	if (ls *.rar *.7z *.7z.??? >/dev/null 2>&1); then
		echo "[WARNING] Archive files exist but unpack skipped, exiting"
		exit $POSTPROCESS_NONE
	fi

	if (ls *.par2 >/dev/null 2>&1); then
		echo "[WARNING] Unpack skipped and par-check skipped (although par2-files exist), exiting"
		exit $POSTPROCESS_NONE
	fi

	if [ -f "_brokenlog.txt" ]; then
		echo "[WARNING] _brokenlog.txt exists, download is probably damaged, exiting"
		exit $POSTPROCESS_NONE
	fi

	echo "[INFO] Neither archive- nor par2-files found, _brokenlog.txt doesn't exist, considering download successful"
fi

# Check if destination directory exists (important for reprocessing of history items)
if [ ! -d "$NZBPP_DIRECTORY" ]; then
	echo "[ERROR] Nothing to post-process: destination directory $NZBPP_DIRECTORY doesn't exist"
	exit $POSTPROCESS_ERROR
fi

# All checks done, now doing the clean up job

# Clean up
echo "[INFO] Cleaning up"
chmod -R a+rw .
rm *.1 >/dev/null 2>&1
rm _brokenlog.txt >/dev/null 2>&1
rm *.[pP][aA][rR]2 >/dev/null 2>&1
rm *.nzb >/dev/null 2>&1
rm *.sfv >/dev/null 2>&1
rm *.url >/dev/null 2>&1

# All OK
exit $POSTPROCESS_SUCCESS
