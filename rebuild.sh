#!/bin/bash

autoreconf --install
./configure --prefix=/usr
make clean all
