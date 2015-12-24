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

#ifndef __CRITICALPACKET_H__
#define __CRITICALPACKET_H__

namespace Par2
{

// Base class for main packet, file verification packet, file description packet
// and creator packet.

// These packets are all small and are held in memory in their entirity

class CriticalPacket
{
public:
  CriticalPacket(void);
  ~CriticalPacket(void);

public:
  // Write a copy of the packet to the specified file at the specified offset
  bool    WritePacket(DiskFile &diskfile, u64 fileoffset) const;

  // Obtain the lenght of the packet.
  size_t  PacketLength(void) const;

  // Allocate some memory for the packet (plus some extra padding).
  void*   AllocatePacket(size_t length, size_t extra = 0);

  // Finish a packet (by storing the set_id_hash and then computing the packet_hash).
  void    FinishPacket(const MD5Hash &set_id_hash);

protected:
  u8     *packetdata;
  size_t  packetlength;
};

inline CriticalPacket::CriticalPacket(void)
{
  // There is no data initially
  packetdata = 0;
  packetlength = 0;
}

inline CriticalPacket::~CriticalPacket(void)
{
  // Delete the data for the packet
  delete [] packetdata;
}

inline size_t CriticalPacket::PacketLength(void) const
{
  return packetlength;
}

inline void* CriticalPacket::AllocatePacket(size_t length, size_t extra)
{
  // Hey! We can't allocate the packet twice
  assert(packetlength == 0 && packetdata == 0);

  // Remember the requested packet length
  packetlength = length;

  // Allocate and clear the requested packet length plus the extra.
  packetdata = new u8[length+extra];
  memset(packetdata, 0, length+extra);

  return packetdata;
}

// Class used to record the fact that a copy of a particular critical packet
// will be written to a particular file at a specific offset.

class CriticalPacketEntry
{
public:
  CriticalPacketEntry(DiskFile *_diskfile, 
                      u64 _offset, 
                      const CriticalPacket *_packet)
    : diskfile(_diskfile)
    , offset(_offset)
    , packet(_packet)
  {}
  CriticalPacketEntry(void)
    : diskfile(0)
    , offset(0)
    , packet(0)
  {}
  CriticalPacketEntry(const CriticalPacketEntry &other)
    : diskfile(other.diskfile)
    , offset(other.offset)
    , packet(other.packet)
  {}
  CriticalPacketEntry& operator=(const CriticalPacketEntry &other)
  {
    diskfile = other.diskfile;
    offset = other.offset;
    packet = other.packet;
    return *this;
  }

public:
  // Write the packet to disk.
  bool   WritePacket(void) const;

  // Obtain the length of the packet.
  u64    PacketLength(void) const;

protected:
  DiskFile             *diskfile;
  u64                   offset;
  const CriticalPacket *packet;
};

inline bool CriticalPacketEntry::WritePacket(void) const
{
  assert(packet != 0 && diskfile != 0);

  // Tell the packet to write itself to disk
  return packet->WritePacket(*diskfile, offset);
}

inline u64 CriticalPacketEntry::PacketLength(void) const
{
  assert(packet != 0);

  // Ask the packet how big it is.
  return packet->PacketLength();
}

} // end namespace Par2

#endif // __CRITICALPACKET_H__
