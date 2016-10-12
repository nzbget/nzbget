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

#ifndef __MAINPACKET_H__
#define __MAINPACKET_H__

namespace Par2
{

// The main packet ties all other critical packets together.
// It specifies the block size to use for both verification of
// files and for the Reed Solomon computation.
// It also specifies how many of the source files are repairable
// and in what order they should be processed.

class MainPacket : public CriticalPacket
{
public:
  // Construct the packet
  MainPacket(void) {};
  ~MainPacket(void) {};

public:
  // Construct the main packet from the source file list and block size.
  // "sourcefiles" will be sorted base on their FileId value.
  /*bool Create(vector<Par2CreatorSourceFile*> &sourcefiles,
              u64 _blocksize);*/

  // Load a main packet from a specified file
  bool Load(DiskFile *diskfile, u64 offset, PACKET_HEADER &header);

public:
  // Get the set id.
  const MD5Hash& SetId(void) const;

  // Get the block size.
  u64 BlockSize(void) const;

  // Get the file counts.
  u32 RecoverableFileCount(void) const;
  u32 TotalFileCount(void) const;

  // Get the fileid of one file
  const MD5Hash& FileId(u32 filenumber) const;

protected:
  u64 blocksize;
  u32 totalfilecount;
  u32 recoverablefilecount;
};

// Get the data block size
inline u64 MainPacket::BlockSize(void) const
{
  assert(packetdata != 0);

  return blocksize;
}

// Get the number of recoverable files
inline u32 MainPacket::RecoverableFileCount(void) const
{
  assert(packetdata != 0);

  return recoverablefilecount;
}

// Get the total number of files
inline u32 MainPacket::TotalFileCount(void) const
{
  assert(packetdata != 0);

  return totalfilecount;
}

// Get the file id hash of one of the files
inline const MD5Hash& MainPacket::FileId(u32 filenumber) const
{
  assert(packetdata != 0);
  assert(filenumber<totalfilecount);

//  return ((const MAINPACKET*)packetdata)->fileid()[filenumber];
  return ((const MAINPACKET*)packetdata)->fileid[filenumber];
}

inline const MD5Hash& MainPacket::SetId(void) const
{
  return ((const MAINPACKET*)packetdata)->header.setid;
}

} // end namespace Par2

#endif // __MAINPACKET_H__
