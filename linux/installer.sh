#!/bin/sh
#
#  This file is part of nzbget. See <http://nzbget.net>.
#
#  Copyright (C) 2015-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

# Strict error handling for debugging
set -o nounset
set -o errexit

# Installer title
TITLE=
# Size of installer script (package header)
HEADER=
# Size of installer package (header + payload)
TOTAL=
# Md5 sum of payload
MD5=
# List of included CPU architecture binaries
DISTARCHS=
# Target platform
PLATFORM=

SILENT=no
ALLARCHS="$DISTARCHS all"
ARCH=""
SELECT=auto
OUTDIR="nzbget"
PRINTEDTITLE=no
JUSTUNPACK=no
UPDATE=no
VERIFY=yes
OS=""

Info()
{
    if test "$SILENT" = "no"; then
        echo "$1"
    fi
}

Error()
{
    Info "ERROR: $1"
    exit 1
}

ValidArch()
{
    for TARG in $ALLARCHS; do
        if test "$TARG" = "$1"; then
            return 0
        fi 
    done
    return 1
}

PrintArch()
{
    if ValidArch $1; then
        Info "$2"
    fi
}

PrintHelp()
{
    if test "$PRINTEDTITLE" = "no"; then
        Info "Installer for $TITLE"
        Info ""
    fi
    if test "$PLATFORM" = "linux"; then
        Info "This installer supports Linux kernel 2.6 or newer and the following CPU architectures:"
    fi
    if test "$PLATFORM" = "freebsd"; then
        Info "This installer supports FreeBSD 9.1 or newer and the following CPU architectures:"
    fi
    PrintArch "i686"     "    i686     - x86, 32 or 64 Bit"
    PrintArch "x86_64"   "    x86_64   - x86, 64 Bit"
    PrintArch "armel"    "    armel    - ARMv5/v6 (ARM9 and ARM11 families)"
    PrintArch "armhf"    "    armhf    - ARMv7/v8 (Cortex family)"
    PrintArch "aarch64"  "    aarch64  - ARMv8, 64 Bit"
    PrintArch "mipsel"   "    mipsel   - MIPS (little endian)"
    PrintArch "mipseb"   "    mipseb   - MIPS (big endian)"
    PrintArch "ppc6xx"   "    ppc6xx   - PowerPC 6xx (603e series)"
    PrintArch "ppc500"   "    ppc500   - PowerPC e500 (core e500v1/e500v2)"
    Info ""

    # Check if command 'basename' is available and fallback to full path if it's not
    TEST=`eval TEST='$(basename /base/bin)' 2>/dev/null; echo $TEST`
    if test "$TEST" != ""; then
        EXENAME=$(basename $0)
    else
        EXENAME=$0
    fi

    Info "Usage: sh $EXENAME [options]"
    Info "    --help            - print this help"
    Info "    --arch <cpu>      - set CPU-architecture"
    Info "    --destdir <path>  - set destination directory"
    Info "    --list            - list package content"
    Info "    --unpack          - just unpack, skip setup"
    Info "    --silent          - silent mode"
    Info "    --nocheck         - skip integrity check"
    Info "    --tar <args>      - run custom tar command on archive"
}

Verify()
{
    # Checking installer package file size
    REQSIZE=$TOTAL
    # Trying to get the size via 'stat' command, fallback to 'ls -l' if 'stat' isn't available
    ACTSIZE=`stat -c%s "$0" 2>/dev/null | cat`
    if test "$ACTSIZE" = ""; then
        NUM=
        for FIELD in `ls -l "$0" 2>/dev/null`
        do
            if test "$NUM" = "aaaa"; then
                ACTSIZE="$FIELD"
                break
            fi
            NUM="a$NUM"
        done
    fi
    if test "$REQSIZE" != "$ACTSIZE"; then
        Error "Corrupted installer package detected: file size mismatch."
    fi

    # Checking checksum (MD5) of package payload, only if command 'md5sum' is available
    ACTMD5=`dd "if=$0" bs=$HEADER skip=1 2>/dev/null | /bin/sh -c md5sum 2>/dev/null | cut -b-32 2>/dev/null | cat`
    LEN=${#ACTMD5}
    if test "$LEN" = "32" -a "$MD5" != "$ACTMD5"; then
        Error "Corrupted installer package detected: checksum mismatch."
    fi
}

DetectEndianness()
{
    # Sixth byte of any executable indicates endianness
    ENDBYTE=`dd if=/bin/sh bs=1 count=6 2>/dev/null | sed -n 's/.ELF.\(.*\)/\1/p'`

    ENDIAN=unknown
    if test "$ENDBYTE" = $'\001'; then
        ENDIAN=little
    elif test "$ENDBYTE" = $'\002'; then
        ENDIAN=big
    fi
}

DetectArch()
{
    OS=`uname -s`
    if test "(" "$PLATFORM" = "linux" -a "$OS" != "Linux" -a "$OS" != "FreeBSD" ")" -o \
            "(" "$PLATFORM" = "freebsd" -a "$OS" != "FreeBSD" ")" ; then
        PrintHelp
        Error "Operating system ($OS) isn't supported by this installer."
    fi

    if test "$UPDATE" = "yes"; then
        ARCH=`cat "$OUTDIR/installer.cfg" 2>/dev/null | sed -n 's/^arch=\(.*\)$/\1/p'`
        SELECT=`cat "$OUTDIR/installer.cfg" 2>/dev/null | sed -n 's/^select=\(.*\)$/\1/p'`
    fi

    if test "$ARCH" = ""; then
        CPU=`uname -m`
        case $CPU in
            i386|i686)
                ARCH=i686
                ;;
            x86_64|amd64)
                ARCH=x86_64
                ;;
            mips)
                ARCH=mipsx
                ;;
            armv5*|armv6*|armel)
                ARCH=armel
                ;;
            armv7*|armv8*)
                ARCH=armhf
                ;;
            aarch64)
                ARCH=aarch64
                ;;
            ppc)
                ARCH=ppcx
                ;;
        esac
    fi

    if test "$OS" = "Linux"; then
        if test "$ARCH" = ""; then
            MIPS=`cat /proc/cpuinfo | sed -n 's/.*:.*\(mips\).*/&/p'`
            if test "$MIPS" != ""; then
                ARCH=mipsx
            fi
        fi

        if test "$ARCH" = "mipsx"; then
            DetectEndianness
            if test "$ENDIAN" = "big"; then
                ARCH=mipseb
            else
                ARCH=mipsel
            fi
        fi

        if test "$ARCH" = "ppcx"; then
            E500=`cat /proc/cpuinfo | sed -n 's/.*:.*\(e500\).*/&/p'`
            if test "$E500" != ""; then
                ARCH=ppc500
            else
                ARCH=ppc6xx
            fi
        fi
    fi

    if test "$ARCH" = ""; then
        PrintHelp
        Error "CPU architecture ($CPU) isn't supported by this installer."
    fi

    if ! ValidArch $ARCH; then
        Error "CPU architecture ($ARCH) isn't supported by this installer."
    fi
}

Unpack()
{
    mkdir -p "$OUTDIR"

    # Prepare list of files to ignore
    EXARCHS=""
    if test "$JUSTUNPACK" = "no" -a "$ARCH" != "all"; then
        rm -f "$OUTDIR/installer.tmp"
        for TARG in $ALLARCHS
        do
            if test "$TARG" != "$ARCH"; then
                echo "nzbget-$TARG" >> "$OUTDIR/installer.tmp"
                echo "unrar-$TARG" >> "$OUTDIR/installer.tmp"
                echo "7za-$TARG" >> "$OUTDIR/installer.tmp"
                EXARCHS="-X installer.tmp"
            fi
        done
    fi

    # Unpack (skip ignorable files)
    dd "if=$0" bs=$HEADER skip=1 2> /dev/null | gzip -c -d | ( cd "$OUTDIR"; tar xf - $EXARCHS 2>&1 ) || { Error "Unpacking failed."; rm -f "$OUTDIR/installer.tmp"; kill -15 $$; }

    if test "$EXARCHS" != ""; then
        rm -f "$OUTDIR/installer.tmp"
    fi

    # Rename unpacked binary files and store arch selection
    if test "$JUSTUNPACK" = "no" -a "$ARCH" != "all"; then
        OLDDIR=`pwd`
        cd "$OUTDIR"
        rm -f nzbget
        rm -f unrar
        rm -f 7za
        mv nzbget-$ARCH nzbget
        mv unrar-$ARCH unrar
        mv 7za-$ARCH 7za
        echo "arch=$ARCH" > "installer.cfg"
        echo "select=$SELECT" >> "installer.cfg"
        cd "$OLDDIR"
    fi
}

TAR()
{
    dd "if=$0" bs=$HEADER skip=1 2> /dev/null | gzip -c -d | tar ${ARG}f - $@
    exit $?
}

# 'Expr' provides two versions of expression evaluations: either using shell
# built-in syntax $((arith)) or using command 'expr'.
# If both fail, an empty string is returned (caller must check for this).
# Currently supporting '+' and '/' operations (can be extended for others if needed).
Expr()
{
    ARG1=$1
    OP=$2
    ARG2=$3
    RET=
    TEST=`eval TEST='$((1+2))' 2>/dev/null || 1; echo $TEST`
    if test "$TEST" = "3"; then
        case "$OP" in
            "+")
                eval RET='$((ARG1 + ARG2))'
                ;;
            "/")
                eval RET='$((ARG1 / ARG2))'
                ;;
        esac
    else
        TEST=`expr 1 + 2 2>/dev/null | cat`
        if test "$TEST" = "3"; then
            case "$OP" in
                "+")
                    RET=`expr $ARG1 + $ARG2`
                    ;;
                "/")
                    RET=`expr $ARG1 / $ARG2`
                    ;;
            esac
        fi
    fi
    echo "$RET"
}

ConfigureLinux()
{
    # Adjusting config file to current system

    MEMFREE=`cat /proc/meminfo | sed -n 's/^MemFree: *\([0-9]*\).*/\1/p' 2>/dev/null | cat`
    MEMCACHED=`cat /proc/meminfo | sed -n 's/^Cached: *\([0-9]*\).*/\1/p' 2>/dev/null | cat`
    if test "$MEMFREE" != "" -a "$MEMCACHED" != ""; then
        TOTALFREE=$(Expr $MEMFREE + $MEMCACHED)
        # Expression evaluation with "Expr" may fail, check the result
        TOTALFREE=$(Expr "$TOTALFREE" / 1024)
        if test "$TOTALFREE" != ""; then
            Info "  Free memory detected: $TOTALFREE MB"
        else
            Info "  Free memory detected: $MEMFREE KB + $MEMCACHED KB"
            TOTALFREE=0
        fi
        if test $TOTALFREE -gt 250000 -o $MEMFREE -gt 250000 -o $MEMCACHED -gt 250000; then
            Info "  Activating article cache (ArticleCache=100)"
            sed 's:^ArticleCache=.*:ArticleCache=100:' -i nzbget.conf
            Info "  Increasing write buffer (WriteBuffer=1024)"
            sed 's:^WriteBuffer=.*:WriteBuffer=1024:' -i nzbget.conf
            Info "  Increasing par repair buffer (ParBuffer=100)"
            sed 's:^ParBuffer=.*:ParBuffer=100:' -i nzbget.conf
            Info "  Activating direct rename (DirectRename=yes)"
            sed 's:^DirectRename=.*:DirectRename=yes:' -i nzbget.conf
        elif test $TOTALFREE -gt 25000 -o $MEMFREE -gt 25000 -o $MEMCACHED -gt 25000; then
            Info "  Increasing write buffer (WriteBuffer=256)"
            sed 's:^WriteBuffer=.*:WriteBuffer=256:' -i nzbget.conf
        fi
    fi

    BOGOLIST=`cat /proc/cpuinfo | sed -n 's/^bogomips\s*:\s\([0-9]*\).*/\1/pI' 2>/dev/null | cat`
    if test "$BOGOLIST" != ""; then
        BOGOMIPS=0
        for CPU1 in $BOGOLIST
        do
            BOGOMIPS=$(Expr $BOGOMIPS + $CPU1)
            if test "$BOGOMIPS" = ""; then
                # Expression evaluation with "Expr" failed, using BogoMIPS for first core
                BOGOMIPS=$CPU1
            fi
        done
        Info "  CPU speed detected: $BOGOMIPS BogoMIPS"
        if test $BOGOMIPS -lt 4000; then
            Info "  Disabling download during par check/repair (ParPauseQueue=yes)"
            sed 's:^ParPauseQueue=.*:ParPauseQueue=yes:' -i nzbget.conf
            Info "  Disabling download during unpack (UnpackPauseQueue=yes)"
            sed 's:^UnpackPauseQueue=.*:UnpackPauseQueue=yes:' -i nzbget.conf
            Info "  Disabling download during post-processing (ScriptPauseQueue=yes)"
            sed 's:^ScriptPauseQueue=.*:ScriptPauseQueue=yes:' -i nzbget.conf
        else
            Info "  Simultaneous download and post-processing is on"
            Info "  Activating direct unpack (DirectUnpack=yes)"
            sed 's:^DirectUnpack=.*:DirectUnpack=yes:' -i nzbget.conf
        fi
    fi
}

ConfigureFreeBSD()
{
    # Adjusting config file to current system

    # No memory check, assuming all supported FreeBSD machines have enough memory

    Info "  Activating article cache (ArticleCache=100)"
    sed -i '' 's:^ArticleCache=.*:ArticleCache=100:' nzbget.conf
    Info "  Increasing write buffer (WriteBuffer=1024)"
    sed -i '' 's:^WriteBuffer=.*:WriteBuffer=1024:' nzbget.conf
    Info "  Increasing par repair buffer (ParBuffer=100)"
    sed -i '' 's:^ParBuffer=.*:ParBuffer=100:' nzbget.conf
    Info "  Activating direct rename (DirectRename=yes)"
    sed -i '' 's:^DirectRename=.*:DirectRename=yes:' nzbget.conf
    Info "  Activating direct unpack (DirectUnpack=yes)"
    sed -i '' 's:^DirectUnpack=.*:DirectUnpack=yes:' nzbget.conf
}

Configure()
{
    cd "$OUTDIR"
    QUICKHELP=no

    if test ! -f nzbget.conf; then
        cp ./webui/nzbget.conf.template nzbget.conf

        if test "$OS" = "Linux"; then
            ConfigureLinux
        fi

        if test "$OS" = "FreeBSD"; then
            ConfigureFreeBSD
        fi

        QUICKHELP=yes
    fi
}

Linux2FreeBSD()
{
    if test "$PLATFORM" = "linux" -a "$OS" = "FreeBSD" ; then
        # Using Linux installer on FreeBSD machine

        if test "$1" = "brand" ; then
            Info "  Branding Linux binaries for use on FreeBSD"
            brandelf -t Linux nzbget
            brandelf -t Linux unrar
            brandelf -t Linux 7za
        fi

        if test "$1" = "kernel-check" ; then
            MODLINUX=`kldstat | sed -n 's/\(linux.ko\)/\1/p'`
            if test "$ARCH" = "x86_64"; then
                MODLINUX=`kldstat | sed -n 's/\(linux64.ko\)/\1/p'`
            fi
            if test "$MODLINUX" = ""; then
                Info ""
                Info "WARNING: Linux kernel module isn't loaded. See http://nzbget.net/installation-on-freebsd"
            fi
        fi
    fi
}

# ParseCommandLine
while true
do
    PARAM=${1:-}
    case $PARAM in
        -h|--help)
            PrintHelp
            exit 0
            ;;
        --silent)
            SILENT=yes
            shift
            ;;
        --arch)
            ARCH=${2:-}
            SELECT=manual
            if ! ValidArch $ARCH; then
                PrintHelp
                Info ""
                Error "Bad argument ($ARCH) to option --arch."
                exit 1
            fi
            shift 2
            ;;
        --destdir)
            OUTDIR=${2:-}
            if test "$OUTDIR" = ""; then
                PrintHelp
                exit 1
            fi
            shift 2
            ;;
        --unpack)
            JUSTUNPACK=yes
            shift
            ;;
        --list)
            ARG=t
            TAR
            exit $?
            ;;
        --tar)
            ARG=${2:-}
            if test "$ARG" = ""; then
                PrintHelp
                exit 1
            fi
            shift 2
            TAR
            exit $?
            ;;
        --update)
            UPDATE=yes
            shift
            ;;
        --nocheck)
            VERIFY=no
            shift
            ;;
        "")
            break
            ;;
        *)
            PrintHelp
            exit 1
            ;;
    esac
done

Info "Installer for $TITLE"
if test "$SILENT" = "no"; then
    PRINTEDTITLE=yes
fi

if test "$VERIFY" = "yes"; then
    Info "Verifying package..."
    Verify
fi

if test "$JUSTUNPACK" = "no"; then
    Info "Checking system..."
    DetectArch
    if test "$SELECT" = "manual"; then
        Info "CPU-Architecture: $ARCH (manually set)"
    else
        Info "CPU-Architecture: $ARCH"
    fi
fi

Info "Unpacking..."
Unpack

ABSOUTDIR=`cd "$OUTDIR"; pwd`

if test "$JUSTUNPACK" = "no"; then
    Info "Configuring..."
    Configure
    Linux2FreeBSD "brand"

    Info "Installation completed"

    if test "$QUICKHELP" = "yes" -a "$SILENT" = "no"; then
        Info ""
        Info "Quick help (from nzbget-directory):"
        Info "   ./nzbget -s        - start nzbget in console mode"
        Info "   ./nzbget -D        - start nzbget in daemon mode (in background)"
        Info "   ./nzbget -C        - connect to background process"
        Info "   ./nzbget -Q        - stop background process"
        Info "   ./nzbget -h        - help screen with all commands"
        Info ""
        Info "Successfully installed into $ABSOUTDIR"

        # Trying to get current IP-address        
        IP=""
        INTERFACE=""
        {
            # First find default interface (ie 'wlan0') with the 'route' command
            INTERFACE=`route -n | sed -rn 's/^0.0.0.0.* ([^ ]+)$/\1/p'`
        } > /dev/null 2>&1
        if test "$INTERFACE" != ""; then
            # OK, a default route 0.0.0.0 + corresponding interface was found
            # Now find the IPv4 address on that interface:
            {
                IP=`ifconfig "$INTERFACE" | sed -rn 's/.*r:([^ ]+) .*/\1/p'`
            } > /dev/null 2>&1
        fi
        if test "$IP" = ""; then
            IP="localhost"
        fi

        Info "Web-interface is on http://$IP:6789 (login:nzbget, password:tegbzn6789)"
    else
        Info "Successfully installed into $ABSOUTDIR"
    fi
    Info "For support please visit http://nzbget.net/forum"
    Linux2FreeBSD "kernel-check"
else
    Info "Unpacked into $ABSOUTDIR"
fi

exit
#END-OF-INSTALLER
