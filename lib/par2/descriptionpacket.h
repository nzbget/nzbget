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

#ifndef __DESCRIPTIONPACKET_H__
#define __DESCRIPTIONPACKET_H__

namespace Par2
{

// The description packet records details about a file (including its name,
// size, and the Hash of both the whole file and the first 16k of the file).

class DescriptionPacket : public CriticalPacket
{
public:
  // Construct the packet
  DescriptionPacket(void) {};
  ~DescriptionPacket(void) {};

public:
  // Construct the packet and store the filename and size.
  bool Create(string _filename, u64 _filesize);

  // Store the computed Hash values in the packet.
  void Hash16k(const MD5Hash &hash);
  void HashFull(const MD5Hash &hash);

  // Compute and return the file id hash from information in the packet
  void ComputeFileId(void);
  const MD5Hash& FileId(void) const;

  // Return the size of the file
  u64 FileSize(void) const;

public:
  // Load a description packet from a specified file
  bool Load(DiskFile *diskfile, u64 offset, PACKET_HEADER &header);

  // Return the name of the file
  string FileName(void) const;

  // Get the Hash values from the packet
  const MD5Hash& HashFull(void) const;
  const MD5Hash& Hash16k(void) const;
protected:
	string filename;
};

// Get the file id from the packet
inline const MD5Hash& DescriptionPacket::FileId(void) const
{
  assert(packetdata != 0);

  return ((const FILEDESCRIPTIONPACKET*)packetdata)->fileid;
}

// Get the size of the file from the packet
inline u64 DescriptionPacket::FileSize(void) const
{
  assert(packetdata != 0);

  return ((const FILEDESCRIPTIONPACKET*)packetdata)->length;
}

// Get the name of the file from the packet
// NB whilst the file format does not guarantee that the name will have a NULL
// termination character, par2cmdline always allocates a little extra data
// and fills it with NULLs to allow the filename to be directly read out of
// the packet.
inline string DescriptionPacket::FileName(void) const
{
  return filename.c_str();
}

// Get the full file hash value from the packet
inline const MD5Hash& DescriptionPacket::HashFull(void) const
{
  assert(packetdata != 0);

  return ((const FILEDESCRIPTIONPACKET*)packetdata)->hashfull;
}

// The the hash of the first 16k of the file from the packet
inline const MD5Hash& DescriptionPacket::Hash16k(void) const
{
  assert(packetdata != 0);

  return ((const FILEDESCRIPTIONPACKET*)packetdata)->hash16k;
}

} // end namespace Par2

#endif // __DESCRIPTIONPACKET_H__
