#!/bin/bash

# apt-based build dependencies, for debian and derivatives (like Ubuntu).
sudo apt install -y build-essential autoconf pkg-config libxml2-dev libncurses-dev libssl-dev zlib1g-dev
# [todo]: add support for RPM-based systems, and others. Pull requests welcome

pkg-config libxml2 libncurses libssl zlib

autoreconf --install
./configure --prefix=/usr
make clean all
