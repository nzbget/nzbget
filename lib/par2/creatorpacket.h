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

#ifndef __CREATORPACKET_H__
#define __CREATORPACKET_H__

namespace Par2
{

// The creator packet records details as to which PAR2 client
// created a particular recovery file.

// The PAR 2.0 specification requires the presence of a
// creator packet, but it is not actually needed for the 
// verification or recovery of damaged files.

class CreatorPacket : public CriticalPacket
{
public:
  // Construct the packet
  CreatorPacket(void) {};
  ~CreatorPacket(void) {};

  // Create a creator packet for a specified set id hash value
  bool Create(const MD5Hash &set_id_hash);

  // Load a creator packet from a specified file
  bool Load(DiskFile *diskfile, u64 offset, PACKET_HEADER &header);
};

} // end namespace Par2

#endif // __CREATORPACKET_H__
