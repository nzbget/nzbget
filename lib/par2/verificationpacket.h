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

#ifndef __VERIFICATIONPACKET_H__
#define __VERIFICATIONPACKET_H__

namespace Par2
{

// The file verification packet stores details that allow individual blocks
// of valid data within a damaged file to be identified.

class VerificationPacket : public CriticalPacket
{
public:
  // Construct the packet
  VerificationPacket(void) {};
  ~VerificationPacket(void) {};

  // Create a packet large enough for the specified number of blocks
  bool Create(u32 blockcount);

  // Set the fileid (computed from the file description packet).
  void FileId(const MD5Hash &fileid);

  // Set the block hash and block crc for a specific data block.
  void SetBlockHashAndCRC(u32 blocknumber, const MD5Hash &hash, u32 crc);

  // Load a verification packet from a specified file
  bool Load(DiskFile *diskfile, u64 offset, PACKET_HEADER &header);

  // Get the FileId
  const MD5Hash& FileId(void) const;

  // Get the block count
  u32 BlockCount(void) const;

  // Get a specific verification entry
  const FILEVERIFICATIONENTRY* VerificationEntry(u32 blocknumber) const;

protected:
  u32 blockcount;
};

inline const MD5Hash& VerificationPacket::FileId(void) const
{
  assert(packetdata != 0);

  return ((FILEVERIFICATIONPACKET*)packetdata)->fileid;
}

inline u32 VerificationPacket::BlockCount(void) const
{
  assert(packetdata != 0);

  return blockcount;
}

inline const FILEVERIFICATIONENTRY* VerificationPacket::VerificationEntry(u32 blocknumber) const
{
  assert(packetdata != 0);

//  return &((FILEVERIFICATIONPACKET*)packetdata)->entries()[blocknumber];
  return &((FILEVERIFICATIONPACKET*)packetdata)->entries[blocknumber];
}

} // end namespace Par2

#endif // __VERIFICATIONPACKET_H__
