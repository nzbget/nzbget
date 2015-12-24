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

RecoveryPacket::RecoveryPacket(void)
{
  diskfile = NULL;
  offset = 0;
  packetcontext = NULL;
}

RecoveryPacket::~RecoveryPacket(void)
{
  delete packetcontext;
}

// Create a recovery packet. 

// The packet header can be almost completely filled in using the supplied
// information and computation of the hash of the packet started immediately.
// The hash will be updated as new data is written to the packet.

void RecoveryPacket::Create(DiskFile      *_diskfile, 
                            u64            _offset, 
                            u64            _blocksize, 
                            u32            _exponent, 
                            const MD5Hash &_setid)
{
  diskfile = _diskfile;
  offset = _offset;

  // Record everything we know in the packet.
  packet.header.magic  = packet_magic;
  packet.header.length = sizeof(packet) + _blocksize;
  //packet.header.hash;  // Not known yet.
  packet.header.setid  = _setid;
  packet.header.type   = recoveryblockpacket_type;
  packet.exponent      = _exponent;

  // Start computation of the packet hash
  packetcontext = new MD5Context;
  packetcontext->Update(&packet.header.setid, 
                        sizeof(RECOVERYBLOCKPACKET)-offsetof(RECOVERYBLOCKPACKET, header.setid));

  // Set the data block to immediatly follow the header on disk
  datablock.SetLocation(_diskfile, _offset + sizeof(packet));
  datablock.SetLength(_blocksize);
}

// Write data from the buffer to the data block on disk
bool RecoveryPacket::WriteData(u64 position, 
                               size_t size, 
                               const void *buffer)
{
  // Update the packet hash
  packetcontext->Update(buffer, size);

  // Write the data to the data block
  size_t wrote;
  return datablock.WriteData(position, size, buffer, wrote);
}

// Write the header of the packet to disk
bool RecoveryPacket::WriteHeader(void)
{
  // Finish computing the packet hash
  packetcontext->Final(packet.header.hash);

  // Write the header to disk
  return diskfile->Write(offset, &packet, sizeof(packet));  
}

// Load the recovery packet from disk.
//
// The header of the packet will already have been read from disk. The only
// thing that actually needs to be read is the exponent value.
// The recovery data is not read from disk at this point. Its location
// is recovered in the DataBlock object.

bool RecoveryPacket::Load(DiskFile      *_diskfile, 
                          u64            _offset, 
                          PACKET_HEADER &_header)
{
  diskfile = _diskfile;
  offset = _offset;

  // Is the packet actually large enough
  if (_header.length <= sizeof(packet))
  {
    return false;
  }

  // Save the fixed header
  packet.header = _header;

  // Set the data block to immediatly follow the header on disk
  datablock.SetLocation(diskfile, offset + sizeof(packet));
  datablock.SetLength(packet.header.length - sizeof(packet));

  // Read the rest of the packet header
  return diskfile->Read(offset + sizeof(packet.header), &packet.exponent, sizeof(packet)-sizeof(packet.header));
}

} // end namespace Par2
