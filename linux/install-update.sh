#!/bin/sh

#  This file is part of nzbget
#
#  Copyright (C) 2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#

BASE_URL="http://nzbget.net/download"

if test "$NZBUP_PROCESSID" = ""; then
    echo "This file is not supposed to be executed directly. To update NZBGet please choose Settings -> SYSTEM -> Check for update in the web-interface."
    exit 1
fi

INSTALLERCFG=`cat "$NZBOP_APPDIR/installer.cfg" 2>/dev/null`
if test "$INSTALLERCFG" = ""; then
    echo "[ERROR] File \"installer.cfg\" is missing in the installation directory. Please reinstall NZBGet."
    exit 1
fi

echo "Downloading version information..."
UPDATE_INFO_LINK=`cat "$NZBOP_APPDIR/webui/package-info.json" | sed -n 's/^.*update-info-link.*: "\(.*\)".*/\1/p'`
"$NZBOP_APPBIN" -B webget "$NZBOP_TEMPDIR/NZBGET_UPDATE.txt" "$UPDATE_INFO_LINK" 2>/dev/null
if test "$?" != "0"; then
    echo "[ERROR] Download failed, please try again later"
    exit 1
fi

if test "$NZBUP_BRANCH" = "TESTING"; then
    VER_FIELD="testing-version"
elif test "$NZBUP_BRANCH" = "STABLE"; then
    VER_FIELD="stable-version"
else
    echo "[ERROR] Unsupported branch $NZBUP_BRANCH"
    exit 1
fi

VER=`cat "$NZBOP_TEMPDIR/NZBGET_UPDATE.txt" | sed -n "s/^.*$VER_FIELD.*: \"\(.*\)\".*/\1/p"`
rm -f "$NZBOP_TEMPDIR/NZBGET_UPDATE.txt"

INSTALLER="nzbget-$VER-bin-linux.run"
echo "Downloading $INSTALLER..."
rm -f "$NZBOP_TEMPDIR/$INSTALLER"
"$NZBOP_APPBIN" -B webget "$NZBOP_TEMPDIR/$INSTALLER" "$BASE_URL/$INSTALLER" 2>/dev/null
if test "$?" != "0"; then
    echo "[ERROR] Download failed, please try again later"
    exit 1
fi

echo "Updating NZBGet..."
echo "..."
sh "$NZBOP_TEMPDIR/$INSTALLER" --update --destdir "$NZBOP_APPDIR"
if test "$?" != "0"; then
    echo "[ERROR] Update failed, installer terminated with error status"
    exit 1
fi
rm -f "$NZBOP_TEMPDIR/$INSTALLER"
echo "..."
echo "Update completed"

echo "Restarting NZBGet..."
sleep 1
echo "[NZB] QUIT"

echo "Waiting for NZBGet to terminate"
PSOPT="-A"
OPTWORKING=`ps $PSOPT` 2>/dev/null
if test "$?" != "0"; then
    PSOPT=""
fi
while true
do
    RUNNING=`ps $PSOPT | sed -n "s/^\s*$NZBUP_PROCESSID\s.*/&/p"` 
    if test "$RUNNING" = ""; then
        break
    fi
    sleep 1
done

echo "Starting NZBGet..."

# Recreating command line used to start NZBGet
CMDLINE=
NUM=1
while true
do
    PARAMNAME="NZBUP_CMDLINE$NUM"
    eval PARAM="\$NZBUP_CMDLINE${NUM}"
    if test "$PARAM" = ""; then
        break
    fi
    if test "$CMDLINE" != ""; then
        CMDLINE="$CMDLINE "
    fi
    CMDLINE="$CMDLINE\"$PARAM\""
    NUM=$((NUM + 1))
done

# Starting NZBGet
eval "$NZBOP_APPBIN" $CMDLINE

exit 0
