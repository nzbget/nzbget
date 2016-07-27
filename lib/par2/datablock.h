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

#ifndef __DATABLOCK_H__
#define __DATABLOCK_H__

namespace Par2
{

class DiskFile;

// A Data Block is a block of data of a specific length at a specific
// offset in a specific file.

// It may be either a block of data in a source file from which recovery
// data is being computed, a block of recovery data in a recovery file, or
// a block in a target file that is being reconstructed.

class DataBlock
{
public:
  DataBlock(void);
  ~DataBlock(void);

public:
  // Set the length of the block
  void SetLength(u64 length);

  // Set the location of the block
  void SetLocation(DiskFile *diskfile, u64 offset);
  void ClearLocation(void);

public:
  // Check to see if the location of the block has been set
  bool IsSet(void) const;

  // Which disk file is this data block in
  DiskFile* GetDiskFile(void) const;

  // What offset is the block located at
  u64 GetOffset(void) const;

  // What is the length of this block
  u64 GetLength(void) const;

public:
  // Open the disk file if it is not already open (so that it can be read)
  bool Open(void);

  // Read some of the data from disk into memory.
  bool ReadData(u64 position, size_t size, void *buffer);

  // Write some of the data from memory to disk
  bool WriteData(u64 position, size_t size, const void *buffer, size_t &wrote);

protected:
  DiskFile *diskfile;  // Which disk file is the block associated with
  u64       offset;    // What is the file offset
  u64       length;    // How large is the block
};


// Construct the data block
inline DataBlock::DataBlock(void)
{
  diskfile = 0;
  offset = 0;
  length = 0;
}

// Destroy the data block
inline DataBlock::~DataBlock(void)
{
}

// Set the length of the block
inline void DataBlock::SetLength(u64 _length)
{
  length = _length;
}

// Set the location of the block
inline void DataBlock::SetLocation(DiskFile *_diskfile, u64 _offset)
{
  diskfile = _diskfile;
  offset = _offset;
}

// Clear the location of the block
inline void DataBlock::ClearLocation(void)
{
  diskfile = 0;
  offset = 0;
}

// Check to see of the location is known
inline bool DataBlock::IsSet(void) const
{
  return (diskfile != 0);
}

// Which disk file is this data block in
inline DiskFile* DataBlock::GetDiskFile(void) const
{
  return diskfile;
}

// What offset is the block located at
inline u64 DataBlock::GetOffset(void) const
{
  return offset;
}

// What is the length of this block
inline u64 DataBlock::GetLength(void) const
{
  return length;
}

} // end namespace Par2

#endif // __DATABLOCK_H__
