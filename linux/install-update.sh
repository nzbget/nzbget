#!/bin/sh
#
#  This file is part of nzbget. See <http://nzbget.net>.
#
#  Copyright (C) 2015-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

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
    DNL_FIELD="testing-download"
    SIG_FIELD="testing-signature"
elif test "$NZBUP_BRANCH" = "STABLE"; then
    VER_FIELD="stable-version"
    DNL_FIELD="stable-download"
    SIG_FIELD="stable-signature"
else
    echo "[ERROR] Unsupported branch $NZBUP_BRANCH"
    exit 1
fi

VER=`cat "$NZBOP_TEMPDIR/NZBGET_UPDATE.txt" | sed -n "s/^.*$VER_FIELD.*: \"\(.*\)\".*/\1/p"`
DNL_LINK=`cat "$NZBOP_TEMPDIR/NZBGET_UPDATE.txt" | sed -n "s/^.*$DNL_FIELD.*: \"\(.*\)\".*/\1/p"`
SIG_LINK=`cat "$NZBOP_TEMPDIR/NZBGET_UPDATE.txt" | sed -n "s/^.*$SIG_FIELD.*: \"\(.*\)\".*/\1/p"`
rm -f "$NZBOP_TEMPDIR/NZBGET_UPDATE.txt"

SIGNATURE="nzbget-$VER.sig.txt"
echo "Downloading verification signature..."
rm -f "$NZBOP_TEMPDIR/$SIGNATURE"
"$NZBOP_APPBIN" -B webget "$NZBOP_TEMPDIR/$SIGNATURE" "$SIG_LINK" 2>/dev/null
if test "$?" != "0"; then
    echo "[ERROR] Download failed, please try again later"
    exit 1
fi

INSTALLER="nzbget-$VER-bin-linux.run"
echo "Downloading $INSTALLER..."
rm -f "$NZBOP_TEMPDIR/$INSTALLER"
"$NZBOP_APPBIN" -B webget "$NZBOP_TEMPDIR/$INSTALLER" "$DNL_LINK" 2>/dev/null
if test "$?" != "0"; then
    echo "[ERROR] Download failed, please try again later"
    exit 1
fi

echo "Verifying package authenticity..."
"$NZBOP_APPBIN" -B verify "$NZBOP_APPDIR/pubkey.pem" "$NZBOP_TEMPDIR/$SIGNATURE" "$NZBOP_TEMPDIR/$INSTALLER" 2>/dev/null
if test "$?" != "93"; then
    echo "[ERROR] Package authenticity verification failed"
    rm -f "$NZBOP_TEMPDIR/$INSTALLER"
    rm -f "$NZBOP_TEMPDIR/$SIGNATURE"
    exit 1
fi
rm -f "$NZBOP_TEMPDIR/$SIGNATURE"

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

# Recreating command line used to start NZBGet
CMDLINE=
N=1
while true
do
    PARAMNAME="NZBUP_CMDLINE$N"
    eval PARAM="\$NZBUP_CMDLINE${N}"
    if test "$PARAM" = ""; then
        break
    fi
    if test "$CMDLINE" != ""; then
        CMDLINE="$CMDLINE "
    fi
    CMDLINE="$CMDLINE\"$PARAM\""
    # Using "case" to implement expression "N=N+1" to overcome possibly disabled expression support on Busybox
    case $N in
        1) N=2 ;; 2) N=3 ;; 3) N=4 ;; 4) N=5 ;; 5) N=6 ;; 6) N=7 ;; 7) N=8 ;; 8) N=9 ;; 9) N=10 ;; 10) N=11 ;;
        11) N=12 ;; 12) N=13 ;; 13) N=14 ;; 14) N=15 ;; 15) N=16 ;; 16) N=17 ;; 17) N=18 ;; 18) N=19 ;; 19) N=20 ;;
        21) N=22 ;; 22) N=23 ;; 23) N=24 ;; 24) N=25 ;; 25) N=26 ;; 26) N=27 ;; 27) N=28 ;; 28) N=29 ;; 29) N=30 ;;
        31) N=32 ;; 32) N=33 ;; 33) N=34 ;; 34) N=35 ;; 35) N=36 ;; 36) N=37 ;; 37) N=38 ;; 38) N=39 ;; 39) N=40 ;;
        41) N=42 ;; 42) N=43 ;; 43) N=44 ;; 44) N=45 ;; 45) N=46 ;; 46) N=47 ;; 47) N=48 ;; 48) N=49 ;; 49) N=50 ;;
        *)
            echo "..."
            echo "[ERROR] Could not restart NZBGet: cannot recreate command line"
            echo "[ERROR] Please restart NZBGet manually (reloading via web-interface isn't sufficient)"
            exit 1
    esac
done

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
    RUNNING=`ps $PSOPT | sed -n "s/^[\s ]*$NZBUP_PROCESSID[\s ].*/&/p"` 
    if test "$RUNNING" = ""; then
        break
    fi
    sleep 1
done

echo "Starting NZBGet..."

# Starting NZBGet
eval "$NZBOP_APPBIN" $CMDLINE

exit 0
