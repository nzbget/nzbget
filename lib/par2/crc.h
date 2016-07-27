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

#ifndef __CRC_H__
#define __CRC_H__

namespace Par2
{

// These global functions are used to compute the CCITT CRC32 checksum of
// blocks of data. 

// The CRC for a block of data may be computed piecemeal be repeatedly 
// calling CRCUpdateChar, and CRCUpdateBlock. 

// Given the CRC for a block of data in a buffer, CRCSlideChar may be used 
// to quickly compute the CRC for the block of data in the buffer that is the
// same size but offset one character later in the buffer.


// Construct the CRC32 lookup table from the specified polynomial
void GenerateCRC32Table(u32 polynomial, u32 (&table)[256]);

// A CRC32 lookup table
struct crc32table
{
  crc32table(u32 polynomial)
  {
    GenerateCRC32Table(polynomial, table);
  }

  u32 table[256];
};

// The one and only CCITT CRC32 lookup table
extern crc32table ccitttable;

// Update the CRC using one character
inline u32 CRCUpdateChar(u32 crc, u8 ch)
{
  return ((crc >> 8) & 0x00ffffffL) ^ ccitttable.table[(u8)crc ^ ch];
}

// Update the CRC using a block of characters in a buffer
inline u32 CRCUpdateBlock(u32 crc, size_t length, const void *buffer)
{
  const unsigned char *current = (const unsigned char *)buffer;

  while (length-- > 0)
  {
    crc =  ((crc >> 8) & 0x00ffffffL) ^ ccitttable.table[(u8)crc ^ (*current++)];
  }

  return crc;
}

// Update the CRC using a block of 0s.
inline u32 CRCUpdateBlock(u32 crc, size_t length)
{
  while (length-- > 0)
  {
    crc =  ((crc >> 8) & 0x00ffffffL) ^ ccitttable.table[(u8)crc];
  }

  return crc;
}

// Construct a CRC32 lookup table for windowing
void GenerateWindowTable(u64 window, u32 (&windowtable)[256]);
// Construct the mask value to apply to the CRC when windowing
u32 ComputeWindowMask(u64 window);

// Slide the CRC along a buffer by one character (removing the old and adding the new).
// The new character is added using the main CCITT CRC32 table, and the old character
// is removed using the windowtable.
inline u32 CRCSlideChar(u32 crc, u8 chNew, u8 chOld, const u32 (&windowtable)[256])
{
  return ((crc >> 8) & 0x00ffffffL) ^ ccitttable.table[(u8)crc ^ chNew] ^ windowtable[chOld];
}

/*

  char *buffer;
  u64 window;

  //...

  u32 windowtable[256];
  GenerateWindowTable(window, windowtable);
  u32 windowmask = ComputeWindowMask(window);

  u32 crc = ~0 ^ CRCUpdateBlock(~0, window, buffer);
  crc = windowmask ^ CRCSlideChar(windowmask ^ crc, buffer[window], buffer[0], windowtable);

  assert(crc == ~0 ^ CRCUpdateBlock(~0, window, buffer+1));

*/

} // end namespace Par2

#endif // __CRC_H__
