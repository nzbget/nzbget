#!/bin/sh 
#
# NZBGet post-process script
# Script will unrar downloaded rar files, join ts-files and rename img-files to iso.
#
# Copyright (C) 2008 Peter Roubos <peterroubos@hotmail.com>
# Copyright (C) 2008 Otmar Werner
# Copyright (C) 2008 Andrei Prygounkov <hugbug@users.sourceforge.net>
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
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
#

#######################    Usage instructions     #######################
# 1. To use this script with nzbget set the option "PostProcess" in
#    nzbget configuration file to point to this script file. E.g.:
#        PostProcess=/home/user/nzbget/postprocess-example.sh
#
# 2. The script supports the feature called "delayed par-check".
#    That means it can try to unpack downloaded files without par-checking
#    them fisrt. Only if unpack fails, the script schedules par-check,
#    then unpacks again.
#    To use delayed par-check set following options in nzbget configuration file:
#        ParCheck=no
#        ParRepair=yes
#        LoadPars=one (or) LoadPars=all
#
# 3. If you want to par-check/repair all files before trying to unpack them,
#    set option "ParCheck=yes".
#
# 4. There are few user-configurable settings in the script.
#    See the next section.
####################### End of Usage instructions #######################

#######################    Settings section     #######################

# Set the full path to unrar if it is not in your PATH
UnrarCmd=unrar

# Delete rar-files after unpacking (1, 0)
DeleteRarFiles=1

# Joint TS-files (1, 0)
JoinTS=0

# Rename img-files to iso (1, 0)
RenameIMG=1

####################### End of settings section #######################


# NZBGet passes following arguments to postprocess-programm as environment
# variables:
#  NZBPP_DIRECTORY    - path to destination dir for downloaded files;
#  NZBPP_NZBFILENAME  - name of processed nzb-file;
#  NZBPP_PARFILENAME  - name of par-file or empty string (if no collections were 
#                       found);
#  NZBPP_PARSTATUS    - result of par-check:
#                       0 = not checked: par-check disabled or nzb-file does
#                           not contain any par-files;
#                       1 = checked and failed to repair;
#                       2 = checked and successfully repaired;
#                       3 = checked and can be repaired but repair is disabled;
#  NZBPP_NZBCOMPLETED - state of nzb-job:
#                       0 = there are more collections in this nzb-file queued;
#                       1 = this was the last collection in nzb-file;
#  NZBPP_PARFAILED    - indication of failed par-jobs for current nzb-file:
#                       0 = no failed par-jobs;
#                       1 = current par-job or any of the previous par-jobs for
#                           the same nzb-files failed;
#  NZBPP_CATEGORY     - category assigned to nzb-file (can be empty string).


# Exit codes
POSTPROCESS_PARCHECK_CURRENT=91
POSTPROCESS_PARCHECK_ALL=92
POSTPROCESS_ALLOK=93
POSTPROCESS_ERROR=1

if [ "$NZBPP_DIRECTORY" = "" ]; then
	echo "*** NZBGet post-process script ***"
	echo "This script is supposed to be called from nzbget."
	exit $POSTPROCESS_ERROR
fi 

echo "[INFO] Unpack: Post-process script successfully started"

# Check if all collections in nzb-file are downloaded
if [ ! "$NZBPP_NZBCOMPLETED" -eq 1 ]; then
	echo "[INFO] Unpack: Not the last collection in nzb-file, exiting"
	exit $POSTPROCESS_ERROR
fi 

if [ "$NZBPP_PARSTATUS" -eq 1 -o "$NZBPP_PARSTATUS" -eq 3 -o "$NZBPP_PARFAILED" -eq 1 ]; then
	if [ "$NZBPP_PARSTATUS" -eq 3 ]; then
		echo "[WARNING] Unpack: Par-check successful, but Par-repair disabled, exiting"
	else
		echo "[WARNING] Unpack: Par-check failed, exiting"
	fi
	exit $POSTPROCESS_ERROR
fi 

# Check nzbget.conf options
BadConfig=0

if [ "$NZBOP_ALLOWREPROCESS" == "yes"  ]; then
	echo "[ERROR] Unpack: Please disable option \"AllowReProcess\" in nzbget configuration file"
	BadConfig=1
fi 

if [ "$NZBOP_LOADPARS" == "none"  ]; then
	echo "[ERROR] Unpack: Please set option \"LoadPars\" to \"One\" or \"All\" in nzbget configuration file"
	BadConfig=1
fi

if [ "$NZBOP_PARREPAIR" == "no"  ]; then
	echo "[ERROR] Unpack: Please set option \"ParRepair\" to \"Yes\" in nzbget configuration file"
	BadConfig=1
fi

if [ "$BadConfig" -eq 1  ]; then
	echo "[ERROR] Unpack: Existing because of not compatible nzbget configuration"
	exit $POSTPROCESS_ERROR
fi 


# All checks done, now processing the files

cd "$NZBPP_DIRECTORY"

# If not just repaired and file "_brokenlog.txt" exists, the collection is damaged
# exiting with returning code $POSTPROCESS_PARCHECK_ALL to request par-repair
if [ ! "$NZBPP_PARSTATUS" -eq 2 ]; then
	if [ -f "_brokenlog.txt" ]; then
		if (ls *.[pP][aA][rR]2  >/dev/null 2>/dev/null); then
			echo "[INFO] Unpack: Brokenlog found, requesting par-repair"
			exit $POSTPROCESS_PARCHECK_ALL
		fi
	fi
fi

# All OK, processing the files

# Make a temporary directory to store the unrarred files
ExtractedDirExists=0
if [ -d extracted ]; then
	ExtractedDirExists=1
fi

mkdir extracted
   
# Unrar the files (if any) to the temporary directory, if there are no rar files this will do nothing
if (ls *.rar >/dev/null 2>/dev/null); then
    echo "[INFO] Unpack: Unraring"
	$UnrarCmd x -y -p- -o+ "*.rar"  ./extracted/
	if [ "$?" -eq 3 ]; then
   		echo "[ERROR] Unpack: Unrar failed"
   		if [ "$ExtractedDirExists" -eq 0 ]; then
			rm -R extracted
		fi
		if (ls *.[pP][aA][rR]2  >/dev/null 2>/dev/null); then
			echo "[INFO] Unpack: Requesting par-repair"
			exit $POSTPROCESS_PARCHECK_ALL
		fi
		exit $POSTPROCESS_ERROR
	fi
fi

if [ $JoinTS -eq 1 ]; then
	# Join any split .ts files if they are named xxxx.0000.ts xxxx.0001.ts
	# They will be joined together to a file called xxxx.0001.ts
	if (ls *.ts >/dev/null 2>/dev/null); then
	    echo "[INFO] Unpack: Joining ts-files"
		tsname=`find . -name "*0001.ts" |awk -F/ '{print $NF}'`
		cat *0???.ts > ./extracted/$tsname
	fi   
   
	# Remove all the split .ts files
    echo "[INFO] Unpack: Deleting source ts-files"
	rm *0???.ts
fi
   
# Remove the rar files
if [ $DeleteRarFiles -eq 1 ]; then
    echo "[INFO] Unpack: Deleting rar-files"
	if (ls *.r[0-9][0-9] >/dev/null 2>/dev/null); then
		rm *.r[0-9][0-9]
	fi
	if (ls *.rar >/dev/null 2>/dev/null); then
		rm *.rar
	fi
	if (ls *.s[0-9][0-9] >/dev/null 2>/dev/null); then
		rm *.s[0-9][0-9]
	fi
fi
   
# Go to the temp directory and try to unrar again.  
# If there are any rars inside the extracted rars then these will no also be unrarred
cd extracted
if (ls *.rar >/dev/null 2>/dev/null); then
    echo "[INFO] Unpack: Unraring (second pass)"
	$UnrarCmd x -y -p- -o+ "*.rar"

	if [ "$?" -eq 3 ]; then
		echo "[INFO] Unpack: Unrar (second pass) failed"
		exit $POSTPROCESS_ERROR
	fi

	# Delete the Rar files
	if [ $DeleteRarFiles -eq 1 ]; then
	    echo "[INFO] Unpack: Deleting rar-files (second pass)"
		if (ls *.r[0-9][0-9] >/dev/null 2>/dev/null); then
			rm *.r[0-9][0-9]
		fi
		if (ls *.rar >/dev/null 2>/dev/null); then
			rm *.rar
		fi
		if (ls *.s[0-9][0-9] >/dev/null 2>/dev/null); then
			rm *.s[0-9][0-9]
		fi
	fi
fi
   
# Move everything back to the Download folder
mv * ..
cd ..
   
# Clean up the temp folder
echo "[INFO] Unpack: Cleaning up"
rmdir extracted
chmod -R a+rw . 
if (ls *.nzb >/dev/null 2>/dev/null); then
	rm *.nzb
fi
if (ls *.1 >/dev/null 2>/dev/null); then
	rm *.1
fi
if (ls *.sfv >/dev/null 2>/dev/null); then
	rm *.sfv
fi
if [ -f "_brokenlog.txt" ]; then
	rm _brokenlog.txt
fi
if (ls *.[pP][aA][rR]2  >/dev/null 2>/dev/null); then
	rm *.[pP][aA][rR]2 
fi

if [ $RenameIMG -eq 1 ]; then
	# Rename img file to iso
	# It will be renamed to .img.iso so you can see that it has been renamed
	if (ls *.img >/dev/null 2>/dev/null); then
	    echo "[INFO] Unpack: Renaming img-files to iso"
		imgname=`find . -name "*.img" |awk -F/ '{print $NF}'`
		mv $imgname $imgname.iso
	fi   
fi

# All OK, requesting cleaning up of download queue
exit $POSTPROCESS_ALLOK
