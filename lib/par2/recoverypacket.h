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

#ifndef __RECOVERYPACKET_H__
#define __RECOVERYPACKET_H__

namespace Par2
{

// The RecoveryPacket object is used to access a specific recovery
// packet within a recovery file (for when creating them, or using
// them during a repair operation).

// Because the actual recovery data for the packet may be large, the
// RecoveryPacket object only contains a copy of packet header and
// exponent value for the block and uses a DataBlock object for
// all accesses to the actual recovery data in the packet.

class RecoveryPacket
{
public:
  RecoveryPacket(void);
  ~RecoveryPacket(void);

public:
  // Create a recovery packet for a specified file.
  void Create(DiskFile      *diskfile,  // Which file will the packet be stored in
              u64            offset,    // At what offset will the packet be stored
              u64            blocksize, // How much recovery data will it contain
              u32            exponent,  // What exponent value will be used
              const MD5Hash &setid);    // What is the SetId
  // Write some data to the recovery data block and update the recovery packet.
  bool WriteData(u64         position,  // Relative position within the data block
                 size_t      size,      // Size of data to write to block
                 const void *buffer);   // Buffer containing the data to write
  // Finish computing the hash of the recovery packet and write the header to disk.
  bool WriteHeader(void);

public:
  // Load a recovery packet from a specified file
  bool Load(DiskFile *diskfile, u64 offset, PACKET_HEADER &header);

public:
  // Get the lenght of the packet.
  u64 PacketLength(void) const;

  // The the exponent of the packet.
  u32 Exponent(void) const;

  // The length of the recovery data
  u64 BlockSize(void) const;

  // The data block
  DataBlock* GetDataBlock(void);

protected:
  DiskFile           *diskfile;       // The specific file that this packet is stored in
  u64                 offset;         // The offset at which the packet is stored

  RECOVERYBLOCKPACKET packet;         // The packet (excluding the actual recovery data)

  MD5Context         *packetcontext;  // MD5 Context used to compute the packet hash

  DataBlock           datablock;      // The recovery data block.
};

inline u64 RecoveryPacket::PacketLength(void) const
{
  return packet.header.length;
}

inline u32 RecoveryPacket::Exponent(void) const
{
  return packet.exponent;
}

inline u64 RecoveryPacket::BlockSize(void) const
{
  return packet.header.length - sizeof(packet);
}

inline DataBlock* RecoveryPacket::GetDataBlock(void)
{
  return &datablock;
}

} // end namespace Par2

#endif // __RECOVERYPACKET_H__
