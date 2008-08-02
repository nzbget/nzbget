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

#######################    Settings section     #######################

# Set the full path to unrar if it is not in your PATH
UnrarCmd=unrar

# Delete rar-files after unpacking (1, 0)
DeleteRarFiles=0

# Joint TS-files (1, 0)
JoinTS=0

# Rename img-files to iso (1, 0)
RenameIMG=0

####################### End of settings section #######################


# Parameters passed to script by nzbget:
#  1 - path to destination dir, where downloaded files are located; 
#  2 - name of nzb-file processed; 
#  3 - name of par-file processed (if par-checked) or empty string (if not); 
#  4 - result of par-check: 
#      0 - not checked: par-check disabled or nzb-file does not contain any 
#          par-files; 
#      1 - checked and failed to repair; 
#      2 - checked and sucessfully repaired; 
#      3 - checked and can be repaired but repair is disabled; 
#  5 - state of nzb-job: 
#      0 - there are more collections in this nzb-file queued; 
#      1 - this was the last collection in nzb-file; 
#  6 - indication of failed par-jobs for current nzb-file: 
#      0 - no failed par-jobs; 
#      1 - current par-job or any of the previous par-jobs for the 
#          same nzb-files failed; 

DownloadDir="$1"   
NzbFile="$2" 
ParCheck=$4 
NzbState=$5  
ParFail=$6 

# Check if all is downloaded and repaired
if [ "$#" -lt 6 ] 
then
	echo "*** NZBGet post-process script ***"
	echo "This script is supposed to be called from nzbget."
	exit
fi 

echo "[INFO] Unpack: Post-process script successfully started"

# Check if all is downloaded and repaired
if [ ! "$NzbState" -eq 1 ] 
then
	echo "[INFO] Unpack: Not the last collection in nzb-file, exiting"
	exit
fi 
if [ ! "$ParCheck" -eq 2 ] 
then
	echo "[WARNING] Unpack: Par-check failed or disabled, exiting"
	exit
fi 
if [ ! "$ParFail" -eq 0 ] 
then
	echo "[WARNING] Unpack: Previous par-check failed, exiting"
	exit
fi 

# All OK, processing the files
cd "$DownloadDir" 

# Make a temporary directory to store the unrarred files
mkdir extracted
   
# Remove the Par files
echo "[INFO] Unpack: Deleting par2-files"
rm *.[pP][aA][rR]2 
   
# Unrar the files (if any) to the temporary directory, if there are no rar files this will do nothing
if (ls *.rar >/dev/null)
then
    echo "[INFO] Unpack: Unraring"
	$UnrarCmd x -y -p- -o+ "*.rar"  ./extracted/ 
fi

if [ $JoinTS -eq 1 ]
then
	# Join any split .ts files if they are named xxxx.0000.ts xxxx.0001.ts
	# They will be joined together to a file called xxxx.0001.ts
	if (ls *.ts >/dev/null)
	then
	    echo "[INFO] Unpack: Joining ts-files"
		tsname=`find . -name "*0001.ts" |awk -F/ '{print $NF}'`
		cat *0???.ts > ./extracted/$tsname
	fi   
   
	# Remove all the split .ts files
    echo "[INFO] Unpack: Deleting source ts-files"
	rm *0???.ts
fi
   
# Remove the rar files
if [ $DeleteRarFiles -eq 1 ]
then  
    echo "[INFO] Unpack: Deleting rar-files"
	rm *.r[0-9][0-9]
	rm *.rar
	rm *.s[0-9][0-9] 
fi
   
# Go to the temp directory and try to unrar again.  
# If there are any rars inside the extracted rars then these will no also be unrarred
cd extracted
if (ls *.rar >/dev/null)
then
    echo "[INFO] Unpack: Calling unrar (second pass)"
	$UnrarCmd x -y -p- -o+ "*.rar"

	# Delete the Rar files
	if [ $DeleteRarFiles -eq 1 ]
	then  
	    echo "[INFO] Unpack: Deleting rar-files (second pass)"
		rm *.r[0-9][0-9]
		rm *.rar
		rm *.s[0-9][0-9] 
	fi
fi
   
# Move everything back to the Download folder
mv * ..
cd ..
   
# Clean up the temp folder
echo "[INFO] Unpack: Cleaning up"
rmdir extracted
chmod -R a+rw . 
rm *.nzb
rm *.1
rm .sfv
rm _brokenlog.txt

if [ $RenameIMG -eq 1 ]
then
# Rename img file to iso
# It will be renamed to .img.iso so you can see that it has been renamed
	if (ls *.img >/dev/null)
	then
	    echo "[INFO] Unpack: Renaming img-files to iso"
		imgname=`find . -name "*.img" |awk -F/ '{print $NF}'`
		mv $imgname $imgname.iso
	fi   
fi
