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

// The one and only CCITT CRC32 lookup table
crc32table ccitttable(0xEDB88320L);

// Construct the CRC32 lookup table from the specified polynomial
void GenerateCRC32Table(u32 polynomial, u32 (&table)[256])
{
  for (u32 i = 0; i <= 255 ; i++) 
  {
    u32 crc = i;

    for (u32 j = 0; j < 8; j++) 
    {
      crc = (crc >> 1) ^ ((crc & 1) ? polynomial : 0);
    }

    table[i] = crc;
  }
}

// Construct a CRC32 lookup table for windowing
void GenerateWindowTable(u64 window, u32 (&target)[256])
{
  for (u32 i=0; i<=255; i++)
  {
    u32 crc = ccitttable.table[i];

    for (u64 j=0; j<window; j++)
    {
      crc = ((crc >> 8) & 0x00ffffffL) ^ ccitttable.table[(u8)crc];
    }

    target[i] = crc;
  }
}

// Construct the mask value to apply to the CRC when windowing
u32 ComputeWindowMask(u64 window)
{
  u32 result = ~0;
  while (window > 0)
  {
    result = CRCUpdateChar(result, (char)0);

    window--;
  }
  result ^= ~0;

  return result;
}

} // end namespace Par2
