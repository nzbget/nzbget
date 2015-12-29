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
#include "Util.h"

#ifdef _MSC_VER
#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif
#endif

namespace Par2
{

// Construct the packet and store the filename and size.

bool DescriptionPacket::Create(string filename, u64 filesize)
{
  // Allocate some extra bytes for the packet in memory so that strlen() can
  // be used on the filename. The extra bytes do not get written to disk.
  FILEDESCRIPTIONPACKET *packet = (FILEDESCRIPTIONPACKET *)AllocatePacket(sizeof(*packet) + (~3 & (3 + (u32)filename.size())), 4);

  // Store everything that is currently known in the packet.

  packet->header.magic  = packet_magic;
  packet->header.length = packetlength;
  //packet->header.hash;  // Not known yet
  //packet->header.setid; // Not known yet
  packet->header.type   = filedescriptionpacket_type;

  //packet->fileid;       // Not known yet
  //packet->hashfull;     // Not known yet
  //packet->hash16k;      // Not known yet
  packet->length        = filesize;

  memcpy(packet->name, filename.c_str(), filename.size());

  return true;
}


void DescriptionPacket::Hash16k(const MD5Hash &hash)
{
  ((FILEDESCRIPTIONPACKET *)packetdata)->hash16k = hash;
}

void DescriptionPacket::HashFull(const MD5Hash &hash)
{
  ((FILEDESCRIPTIONPACKET *)packetdata)->hashfull = hash;
}

void DescriptionPacket::ComputeFileId(void)
{
  FILEDESCRIPTIONPACKET *packet = ((FILEDESCRIPTIONPACKET *)packetdata);

  // Compute the fileid from the hash, length, and name fields in the packet.

  MD5Context context;
  context.Update(&packet->hash16k, 
                 sizeof(FILEDESCRIPTIONPACKET)-offsetof(FILEDESCRIPTIONPACKET,hash16k)
                 +strlen((const char*)packet->name));
  context.Final(packet->fileid);
}

// Load a description packet from a specified file
bool DescriptionPacket::Load(DiskFile *diskfile, u64 offset, PACKET_HEADER &header)
{
  // Is the packet big enough
  if (header.length <= sizeof(FILEDESCRIPTIONPACKET))
  {
    return false;
  }

  // Is the packet too large (what is the longest permissible filename)
  if (header.length - sizeof(FILEDESCRIPTIONPACKET) > 100000)
  {
    return false;
  }

  // Allocate the packet (with a little extra so we will have NULLs after the filename)
  FILEDESCRIPTIONPACKET *packet = (FILEDESCRIPTIONPACKET *)AllocatePacket((size_t)header.length, 4);

  packet->header = header;

  // Read the rest of the packet from disk
  if (!diskfile->Read(offset + sizeof(PACKET_HEADER), 
                      &packet->fileid, 
                      (size_t)packet->header.length - sizeof(PACKET_HEADER)))
    return false;

  filename = *WebUtil::Latin1ToUtf8((char*)((FILEDESCRIPTIONPACKET*)packetdata)->name);

  // Are the file and 16k hashes consistent
  if (packet->length <= 16384 && packet->hash16k != packet->hashfull)
  {
    return false;
  }

  return true;
}

} // end namespace Par2
