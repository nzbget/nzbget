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

// Construct the main packet from the source files and the block size
/*
bool MainPacket::Create(vector<Par2CreatorSourceFile*> &sourcefiles, u64 _blocksize)
{
  recoverablefilecount = totalfilecount =(u32)sourcefiles.size();
  blocksize = _blocksize;

  // Allocate memory for the main packet with enough fileid entries
  MAINPACKET *packet = (MAINPACKET *)AllocatePacket(sizeof(MAINPACKET) + totalfilecount * sizeof(MD5Hash));

  // Record the details we already know in the packet
  packet->header.magic         = packet_magic;
  packet->header.length        = packetlength;
  //packet->header.hash;         // Compute shortly
  //packet->header.setid;        // Compute shortly
  packet->header.type          = mainpacket_type;

  packet->blocksize            = _blocksize;
  packet->recoverablefilecount = totalfilecount;
  //packet->fileid;              // Compute shortly

  // Sort the source files according to their fileid values
  if (totalfilecount > 1)
  {
    sort(sourcefiles.begin(), sourcefiles.end(), Par2CreatorSourceFile::CompareLess);
  }

  // Store the fileid values in the main packet
  vector<Par2CreatorSourceFile*>::const_iterator sourcefile;
  MD5Hash *hash;
  for ((sourcefile=sourcefiles.begin()),(hash=packet->fileid);
       sourcefile!=sourcefiles.end();
       ++sourcefile, ++hash)
  {
    *hash = (*sourcefile)->FileId();
  }

  // Compute the set_id_hash
  MD5Context setidcontext;
  setidcontext.Update(&packet->blocksize, packetlength - offsetof(MAINPACKET, blocksize));
  setidcontext.Final(packet->header.setid);

  // Compute the packet_hash
  MD5Context packetcontext;
  packetcontext.Update(&packet->header.setid, packetlength - offsetof(MAINPACKET, header.setid));
  packetcontext.Final(packet->header.hash);

  return true;
}
*/

// Load a main packet from a specified file

bool MainPacket::Load(DiskFile *diskfile, u64 offset, PACKET_HEADER &header)
{
  // Is the packet large enough
  if (header.length < sizeof(MAINPACKET))
  {
    return false;
  }

  // Is there a whole number of fileid values
  if (0 < (header.length - sizeof(MAINPACKET)) % sizeof(MD5Hash))
  {
    return false;
  }

  // Is the packet too large
  if (header.length > sizeof(MAINPACKET) + 32768 * sizeof(MD5Hash))
  {
    return false;
  }

  // Compute the total number of entries in the fileid array
  totalfilecount = (u32)(((size_t)header.length - sizeof(MAINPACKET)) / sizeof(MD5Hash));

  MAINPACKET *packet = (MAINPACKET *)AllocatePacket((size_t)header.length);

  packet->header = header;

  // Read the rest of the packet from disk
  if (!diskfile->Read(offset + sizeof(PACKET_HEADER), 
                      &packet->blocksize, 
                     (size_t)packet->header.length - sizeof(PACKET_HEADER)))
    return false;

  // Does the packet have enough fileid values
  recoverablefilecount = packet->recoverablefilecount;
  if (recoverablefilecount > totalfilecount)
  {
    return false;
  }

  // Is the block size valid
  blocksize = packet->blocksize;
  if (blocksize == 0 || (blocksize & 3) != 0)
  {
    return false;
  }

  return true;
}

} // end namespace Par2
