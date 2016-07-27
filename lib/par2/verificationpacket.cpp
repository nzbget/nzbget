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

// Create a packet large enough for the specified number of blocks

bool VerificationPacket::Create(u32 _blockcount)
{
  blockcount = _blockcount;

  // Allocate a packet large enough to hold the required number of verification entries.
  FILEVERIFICATIONPACKET *packet = (FILEVERIFICATIONPACKET*)AllocatePacket(sizeof(FILEVERIFICATIONPACKET) + blockcount * sizeof(FILEVERIFICATIONENTRY));

  // Record everything we know in the packet.
  packet->header.magic  = packet_magic;
  packet->header.length = packetlength;
  //packet->header.hash;  // Not known yet
  //packet->header.setid; // Not known yet
  packet->header.type   = fileverificationpacket_type;

  //packet->fileid;       // Not known yet
  //packet->entries;      // Not known yet

  return true;
}

void VerificationPacket::FileId(const MD5Hash &fileid)
{
  assert(packetdata != 0);

  // Store the fileid in the packet.
  ((FILEVERIFICATIONPACKET*)packetdata)->fileid = fileid;
}

void VerificationPacket::SetBlockHashAndCRC(u32 blocknumber, const MD5Hash &hash, u32 crc)
{
  assert(packetdata != 0);
  assert(blocknumber < blockcount);

  // Store the block hash and block crc in the packet.
  FILEVERIFICATIONENTRY &entry = ((FILEVERIFICATIONPACKET*)packetdata)->entries[blocknumber];

  entry.hash = hash;
  entry.crc = crc;
}

bool VerificationPacket::Load(DiskFile *diskfile, u64 offset, PACKET_HEADER &header)
{
  // Is the packet large enough
  if (header.length <= sizeof(FILEVERIFICATIONPACKET))
  {
    return false;
  }

  // Does the packet have a whole number of verification records
  if (0 < ((header.length - sizeof(FILEVERIFICATIONPACKET)) % sizeof(FILEVERIFICATIONENTRY)))
  {
    return false;
  }

  // Is the packet too large
  if (header.length > sizeof(FILEVERIFICATIONPACKET) + 32768 * sizeof(FILEVERIFICATIONENTRY))
  {
    return false;
  }

  // Allocate the packet
  FILEVERIFICATIONPACKET *packet = (FILEVERIFICATIONPACKET*)AllocatePacket((size_t)header.length);
  packet->header = header;

  // How many blocks are there
  blockcount = (u32)((((FILEVERIFICATIONPACKET*)packetdata)->header.length - sizeof(FILEVERIFICATIONPACKET)) / sizeof(FILEVERIFICATIONENTRY));

  // Read the rest of the packet
  return diskfile->Read(offset + sizeof(PACKET_HEADER), 
                        &packet->fileid, 
                        (size_t)packet->header.length - sizeof(PACKET_HEADER));
}

} // end namespace Par2
