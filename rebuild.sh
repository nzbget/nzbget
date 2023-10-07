#!/bin/bash

# apt-based build dependencies, for debian and derivatives (like Ubuntu).
pkgmgr=$(which apt)
if [ ! -z "${pkgmgr}" ]
then
    sudo "${pkgmgr}" install -y build-essential autoconf pkg-config libxml2-dev libncurses-dev libssl-dev zlib1g-dev
    pkg-config libxml2 libncurses libssl zlib
fi

# [todo]: add support for RPM-based systems, and others. Pull requests welcome

autoreconf --install
./configure --prefix=/usr
make clean all
