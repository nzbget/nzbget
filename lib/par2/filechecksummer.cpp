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

// Construct the checksummer and allocate buffers

FileCheckSummer::FileCheckSummer(DiskFile   *_diskfile,
                                 u64         _blocksize,
                                 const u32 (&_windowtable)[256],
                                 u32         _windowmask)
: diskfile(_diskfile)
, blocksize(_blocksize)
, windowtable(_windowtable)
, windowmask(_windowmask)
{
  buffer = new char[(size_t)blocksize*2];

  filesize = diskfile->FileSize();

  currentoffset = 0;
}

FileCheckSummer::~FileCheckSummer(void)
{
  delete [] buffer;
}

// Start reading the file at the beginning
bool FileCheckSummer::Start(void)
{
  currentoffset = readoffset = 0;

  tailpointer = outpointer = buffer;
  inpointer = &buffer[blocksize];

  // Fill the buffer with new data
  if (!Fill())
    return false;

  // Compute the checksum for the block
  checksum = ~0 ^ CRCUpdateBlock(~0, (size_t)blocksize, buffer);

  return true;
}

// Jump ahead
bool FileCheckSummer::Jump(u64 distance)
{
  // Are we already at the end of the file
  if (currentoffset >= filesize)
    return false;

  // Special distances
  if (distance == 0)
    return false;
  if (distance == 1)
    return Step();

  // Not allowed to jump more than one block
  assert(distance <= blocksize);
  if (distance > blocksize)
    distance = blocksize;

  // Advance the current offset and check if we have reached the end of the file
  currentoffset += distance;
  if (currentoffset >= filesize)
  {
    currentoffset = filesize;
    tailpointer = outpointer = buffer;
    memset(buffer, 0, (size_t)blocksize);
    checksum = 0;

    return true;
  }

  // Move past the data being discarded
  outpointer += distance;
  assert(outpointer <= tailpointer);

  // Is there any data left in the buffer that we are keeping
  size_t keep = tailpointer - outpointer;
  if (keep > 0)
  {
    // Move it back to the start of the buffer
    memmove(buffer, outpointer, keep);
    tailpointer = &buffer[keep];
  }
  else
  {
    tailpointer = buffer;
  }

  outpointer = buffer;
  inpointer = &buffer[blocksize];

  if (!Fill())
    return false;

  // Compute the checksum for the block
  checksum = ~0 ^ CRCUpdateBlock(~0, (size_t)blocksize, buffer);

  return true;
}

// Fill the buffer from disk

bool FileCheckSummer::Fill(void)
{
  // Have we already reached the end of the file
  if (readoffset >= filesize)
    return true;

  // How much data can we read into the buffer
  size_t want = (size_t)min(filesize-readoffset, (u64)(&buffer[2*blocksize]-tailpointer));

  if (want > 0)
  {
    // Read data
    if (!diskfile->Read(readoffset, tailpointer, want))
      return false;

    UpdateHashes(readoffset, tailpointer, want);
    readoffset += want;
    tailpointer += want;
  }

  // Did we fill the buffer
  want = &buffer[2*blocksize] - tailpointer;
  if (want > 0)
  {
    // Blank the rest of the buffer
    memset(tailpointer, 0, want);
  }

  return true;
}

// Update the full file hash and the 16k hash using the new data
void FileCheckSummer::UpdateHashes(u64 offset, const void *buffer, size_t length)
{
  // Are we already beyond the first 16k
  if (offset >= 16384)
  {
    contextfull.Update(buffer, length);
  }
  // Would we reach the 16k mark
  else if (offset+length >= 16384)
  {
    // Finish the 16k hash
    size_t first = (size_t)(16384-offset);
    context16k.Update(buffer, first);

    // Continue with the full hash
    contextfull = context16k;

    // Do we go beyond the 16k mark
    if (offset+length > 16384)
    {
      contextfull.Update(&((const char*)buffer)[first], length-first);
    }
  }
  else
  {
    context16k.Update(buffer, length);
  }
}

// Return the full file hash and the 16k file hash
void FileCheckSummer::GetFileHashes(MD5Hash &hashfull, MD5Hash &hash16k) const
{
  // Compute the hash of the first 16k
  MD5Context context = context16k;
  context.Final(hash16k);

  // Is the file smaller than 16k
  if (filesize < 16384)
  {
    // The hashes are the same
    hashfull = hash16k;
  }
  else
  {
    // Compute the hash of the full file
    context = contextfull;
    context.Final(hashfull);
  }
}

// Compute and return the current hash
MD5Hash FileCheckSummer::Hash(void)
{
  MD5Context context;
  context.Update(outpointer, (size_t)blocksize);

  MD5Hash hash;
  context.Final(hash);

  return hash;
}

u32 FileCheckSummer::ShortChecksum(u64 blocklength)
{
  u32 crc = CRCUpdateBlock(~0, (size_t)blocklength, outpointer);
  
  if (blocksize > blocklength)
  {
    crc = CRCUpdateBlock(crc, (size_t)(blocksize-blocklength));
  }

  crc ^= ~0;

  return crc;
}

MD5Hash FileCheckSummer::ShortHash(u64 blocklength)
{
  MD5Context context;
  context.Update(outpointer, (size_t)blocklength);

  if (blocksize > blocklength)
  {
    context.Update((size_t)(blocksize-blocklength));
  }

  // Get the hash value
  MD5Hash hash;
  context.Final(hash);

  return hash;
}

} // end namespace Par2
