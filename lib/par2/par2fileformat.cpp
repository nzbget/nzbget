//  This file is part of par2cmdline (a PAR 2.0 compatible file verification and
//  repair tool). See http://parchive.sourceforge.net for details of PAR 2.0.
//
//  Copyright (c) 2003 Peter Brian Clements
//
//  par2cmdline is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  par2cmdline is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA

#include "nzbget.h"
#include "par2cmdline.h"

namespace Par2
{

MAGIC      packet_magic                = {{'P', 'A', 'R', '2', '\0','P', 'K', 'T'}};
PACKETTYPE fileverificationpacket_type = {{'P', 'A', 'R', ' ', '2', '.', '0', '\0', 'I', 'F', 'S', 'C', '\0','\0','\0','\0'}};
PACKETTYPE filedescriptionpacket_type  = {{'P', 'A', 'R', ' ', '2', '.', '0', '\0', 'F', 'i', 'l', 'e', 'D', 'e', 's', 'c' }};
PACKETTYPE mainpacket_type             = {{'P', 'A', 'R', ' ', '2', '.', '0', '\0', 'M', 'a', 'i', 'n', '\0','\0','\0','\0'}};
PACKETTYPE recoveryblockpacket_type    = {{'P', 'A', 'R', ' ', '2', '.', '0', '\0', 'R', 'e', 'c', 'v', 'S', 'l', 'i', 'c' }};
PACKETTYPE creatorpacket_type          = {{'P', 'A', 'R', ' ', '2', '.', '0', '\0', 'C', 'r', 'e', 'a', 't', 'o', 'r', '\0'}};

} // end namespace Par2
