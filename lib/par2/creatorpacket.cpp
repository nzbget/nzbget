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

#ifdef _MSC_VER
#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif
#endif

namespace Par2
{

// Construct the creator packet. 

// The only external information required to complete construction is 
// the set_id_hash (which is normally computed from information in the
// main packet).

bool CreatorPacket::Create(const MD5Hash &setid)
{
  string creator = "Created by " PACKAGE " version " VERSION ".";

  // Allocate a packet just large enough for creator name
  CREATORPACKET *packet = (CREATORPACKET *)AllocatePacket(sizeof(*packet) + (~3 & (3+(u32)creator.size())));

  // Fill in the details the we know
  packet->header.magic = packet_magic;
  packet->header.length = packetlength;
  //packet->header.hash;  // Compute shortly
  packet->header.setid = setid;
  packet->header.type = creatorpacket_type;

  // Copy the creator description into the packet
  memcpy(packet->client, creator.c_str(), creator.size());

  // Compute the packet hash
  MD5Context packetcontext;
  packetcontext.Update(&packet->header.setid, packetlength - offsetof(PACKET_HEADER, setid));
  packetcontext.Final(packet->header.hash);

  return true;
}

// Load the packet from disk.

bool CreatorPacket::Load(DiskFile *diskfile, u64 offset, PACKET_HEADER &header)
{
  // Is the packet long enough
  if (header.length <= sizeof(CREATORPACKET))
  {
    return false;
  }

  // Is the packet too large (what is the longest reasonable creator description)
  if (header.length - sizeof(CREATORPACKET) > 100000)
  {
    return false;
  }

  // Allocate the packet (with a little extra so we will have NULLs after the description)
  CREATORPACKET *packet = (CREATORPACKET *)AllocatePacket((size_t)header.length, 4);
  packet->header = header;

  // Load the rest of the packet from disk
  return diskfile->Read(offset + sizeof(PACKET_HEADER), 
                        packet->client, 
                        (size_t)packet->header.length - sizeof(PACKET_HEADER));
}

} // end namespace Par2
