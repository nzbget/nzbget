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

# Strict error handling for debugging
set -o nounset
set -o errexit

# Installer title
TITLE=
# Size of installer script (package header)
HEADER=
# Size of tar.gz archive (package payload)
PAYLOAD=
# Md5 sum of payload
MD5=
# List of included CPU architecture binaries
DISTARCHS=

SILENT=no
ALLARCHS="$DISTARCHS all"
ARCH=""
MANUALARCH=no
OUTDIR="nzbget"
PRINTEDTITLE=no
JUSTUNPACK=no
UPDATE=no
VERIFY=yes

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
    LIMARCH=`echo " $ALLARCHS " | sed "s/ $1 //"`
    if test " $ALLARCHS " = "$LIMARCH"; then
        return 1
    fi
    return 0
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
    Info "This installer supports Linux kernel 2.6 or newer and the following CPU architectures:"
    PrintArch "i686"     "    i686     - x86, 32 or 64 Bit"
    PrintArch "armel"    "    armel    - ARMv5/v6 (ARM9 and ARM11 families)"
    PrintArch "armhf"    "    armhf    - ARMv7 (Cortex family)"
    PrintArch "mipsel"   "    mipsel   - MIPS (little endian)"
    PrintArch "mipseb"   "    mipseb   - MIPS (big endian)"
    PrintArch "ppc6xx"   "    ppc6xx   - PowerPC 6xx (603e series)"
    PrintArch "ppc500"   "    ppc500   - PowerPC e500 (core e500v1/e500v2)"
    Info ""
    Info "Usage: sh $(basename $0) [options]"
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
    REQSIZE=$((HEADER + PAYLOAD))
    ACTSIZE=`stat -c%s "$0" 2>/dev/null | cat`
    if test "$ACTSIZE" = ""; then
        ACTSIZE=`ls -l "$0" 2>/dev/null | sed -n 's/.* .* .* \(.*\) .* .* .* .* .*/\1/p'`
    fi
    if test "$REQSIZE" != "$ACTSIZE"; then
        Error "Corrupted installer package detected: file size mismatch."
    fi
    
    ACTMD5=`dd "if=$0" bs=$HEADER skip=1 2>/dev/null | md5sum 2>/dev/null | cut -b-32`
    LEN=${#ACTMD5}
    if test "$LEN" = "32" -a "$MD5" != "$ACTMD5"; then
        Error "Corrupted installer package detected: checksum mismatch."
    fi
}

DetectEndianness()
{
    # Sixth byte of any executable indicates endianness
    ENDBYTE=`dd if=/bin/sh bs=1 count=6 2>/dev/null | sed -n 's/ELF.\(.*\)/\1/p'`

    ENDIAN=unknown
    if test $ENDBYTE="\x01"; then
        ENDIAN=little
    elif test $ENDBYTE="\x02"; then
        ENDIAN=big
    fi
}

DetectArch()
{
    OS=`uname -s`
    if test "$OS" != "Linux"; then
        PrintHelp
        Error "Operating system ($OS) isn't supported by this installer."
    fi

    if test "$UPDATE" = "yes"; then
        ARCH=`cat "$OUTDIR/installer.cfg" 2>/dev/null | sed -n 's/^arch=\(.*\)$/\1/p'`
    fi

    if test "$ARCH" = ""; then
        CPU=`uname -m`
        case $CPU in
            i386|i686|x86_64)
                ARCH=i686
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
            ppc)
                ARCH=ppcx
                ;;
        esac
    fi

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
    # Prepare list of files to ignore
    EXARCHS=""
    if test "$JUSTUNPACK" = "no" -a "$ARCH" != "all"; then
        rm -f /tmp/nzbget-installer.tmp
        for TARG in $ALLARCHS
        do
            if test "$TARG" != "$ARCH"; then
                echo "nzbget-$TARG" >> /tmp/nzbget-installer.tmp
                echo "unrar-$TARG" >> /tmp/nzbget-installer.tmp
                echo "7za-$TARG" >> /tmp/nzbget-installer.tmp
                EXARCHS="-X /tmp/nzbget-installer.tmp"
            fi
        done
    fi

    # Unpack (skip ignorable files)
    mkdir -p $OUTDIR
    dd "if=$0" bs=$HEADER skip=1 2> /dev/null | gzip -cd | ( cd $OUTDIR; tar x $EXARCHS 2>&1 ) || { Error "Unpacking failed."; kill -15 $$; }

    if test "$EXARCHS" != ""; then
        rm -f /tmp/nzbget-installer.tmp
    fi

    # Rename unpacked binaries files
    if test "$JUSTUNPACK" = "no" -a "$ARCH" != "all"; then
        OLDDIR=`pwd`
        cd $OUTDIR;
        rm -f nzbget
        rm -f unrar
        mv nzbget-$ARCH nzbget
        mv unrar-$ARCH unrar
        mv 7za-$ARCH 7za
        echo "arch=$ARCH" > "installer.cfg"
        cd $OLDDIR
    fi
}

TAR()
{
    dd "if=$0" bs=$HEADER skip=1 2> /dev/null | gzip -cd | tar "$ARG" $@
    exit $?
}

Configure()
{
    cd $OUTDIR
    QUICKHELP=no

    if test ! -f nzbget.conf; then
        cp ./webui/nzbget.conf.template nzbget.conf
        QUICKHELP=yes
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
            MANUALARCH=yes
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
    if test "$MANUALARCH" = "yes"; then
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
        IP=""
        {
            IP=`ifconfig | sed -rn 's/.*r:([^ ]+) .*/\1/p' | head -n 1` || true
        } > /dev/null 2>&1
        if test "$IP" = ""; then
            IP="localhost"
        fi
        Info "Web-interface runs on http://$IP:6789"
    else
        Info "Successfully installed into $ABSOUTDIR"
    fi
    Info "For support please visit http://nzbget.net/forum"
else
    Info "Unpacked into $ABSOUTDIR"
fi

exit
#END-OF-INSTALLER
