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

u32 gcd(u32 a, u32 b)
{
  if (a && b)
  {
    while (a && b)
    {
      if (a>b)
      {
        a = a%b;
      }
      else
      {
        b = b%a;
      }
    }

    return a+b;
  }
  else
  {
    return 0;
  }
}

template <> bool ReedSolomon<Galois8>::SetInput(const vector<bool> &present)
{
  inputcount = (u32)present.size();

  datapresentindex = new u32[inputcount];
  datamissingindex = new u32[inputcount];
  database         = new G::ValueType[inputcount];

  G::ValueType base = 1;

  for (unsigned int index=0; index<inputcount; index++)
  {
    // Record the index of the file in the datapresentindex array 
    // or the datamissingindex array
    if (present[index])
    {
      datapresentindex[datapresent++] = index;
    }
    else
    {
      datamissingindex[datamissing++] = index;
    }

    database[index] = base++;
  }

  return true;
}

template <> bool ReedSolomon<Galois8>::SetInput(u32 count)
{
  inputcount = count;

  datapresentindex = new u32[inputcount];
  datamissingindex = new u32[inputcount];
  database         = new G::ValueType[inputcount];

  G::ValueType base = 1;

  for (unsigned int index=0; index<count; index++)
  {
    // Record that the file is present
    datapresentindex[datapresent++] = index;

    database[index] = base++;
  }

  return true;
}

template <> bool ReedSolomon<Galois8>::Process(size_t size, u32 inputindex, const void *inputbuffer, u32 outputindex, void *outputbuffer)
{
  // Look up the appropriate element in the RS matrix
  Galois8 factor = leftmatrix[outputindex * (datapresent + datamissing) + inputindex];

  // Do nothing if the factor happens to be 0
  if (factor == 0)
    return eSuccess;

#ifdef LONGMULTIPLY
  // The 8-bit long multiplication tables
  Galois8 *table = glmt->tables;

  // Split the factor into Low and High bytes
  unsigned int fl = (factor >> 0) & 0xff;

  // Get the four separate multiplication tables
  Galois8 *LL = &table[(0*256 + fl) * 256 + 0]; // factor.low  * source.low

  // Combine the four multiplication tables into two
  unsigned int L[256];

  unsigned int *pL = &L[0];

  for (unsigned int i=0; i<256; i++)
  {
    *pL = *LL;

    pL++;
    LL++;
  }

  // Treat the buffers as arrays of 32-bit unsigned ints.
  u32 *src4 = (u32 *)inputbuffer;
  u32 *end4 = (u32 *)&((u8*)inputbuffer)[size & ~3];
  u32 *dst4 = (u32 *)outputbuffer;

  // Process the data
  while (src4 < end4)
  {
    u32 s = *src4++;

    // Use the two lookup tables computed earlier
    *dst4++ ^= (L[(s >> 0) & 0xff]      )
            ^  (L[(s >> 8) & 0xff] << 8 )
            ^  (L[(s >> 16)& 0xff] << 16)
            ^  (L[(s >> 24)& 0xff] << 24);
  }

  // Process any left over bytes at the end of the buffer
  if (size & 3)
  {
    u8 *src1 = &((u8*)inputbuffer)[size & ~3];
    u8 *end1 = &((u8*)inputbuffer)[size];
    u8 *dst1 = &((u8*)outputbuffer)[size & ~3];

    // Process the data
    while (src1 < end1)
    {
      u8 s = *src1++;
      *dst1++ ^= L[s];
    }
  }
#else
  // Treat the buffers as arrays of 16-bit Galois values.

  Galois8 *src = (Galois8 *)inputbuffer;
  Galois8 *end = (Galois8 *)&((u8*)inputbuffer)[size];
  Galois8 *dst = (Galois8 *)outputbuffer;

  // Process the data
  while (src < end)
  {
    *dst++ += *src++ * factor;
  }
#endif

  return eSuccess;
}



////////////////////////////////////////////////////////////////////////////////////////////



// Set which of the source files are present and which are missing
// and compute the base values to use for the vandermonde matrix.
template <> bool ReedSolomon<Galois16>::SetInput(const vector<bool> &present)
{
  inputcount = (u32)present.size();

  datapresentindex = new u32[inputcount];
  datamissingindex = new u32[inputcount];
  database         = new G::ValueType[inputcount];

  unsigned int logbase = 0;

  for (unsigned int index=0; index<inputcount; index++)
  {
    // Record the index of the file in the datapresentindex array 
    // or the datamissingindex array
    if (present[index])
    {
      datapresentindex[datapresent++] = index;
    }
    else
    {
      datamissingindex[datamissing++] = index;
    }

    // Determine the next useable base value.
    // Its log must must be relatively prime to 65535
    while (gcd(G::Limit, logbase) != 1)
    {
      logbase++;
    }
    if (logbase >= G::Limit)
    {
      cerr << "Too many input blocks for Reed Solomon matrix." << endl;
      return false;
    }
    G::ValueType base = G(logbase++).ALog();

    database[index] = base;
  }

  return true;
}

// Record that the specified number of source files are all present
// and compute the base values to use for the vandermonde matrix.
template <> bool ReedSolomon<Galois16>::SetInput(u32 count)
{
  inputcount = count;

  datapresentindex = new u32[inputcount];
  datamissingindex = new u32[inputcount];
  database         = new G::ValueType[inputcount];

  unsigned int logbase = 0;

  for (unsigned int index=0; index<count; index++)
  {
    // Record that the file is present
    datapresentindex[datapresent++] = index;

    // Determine the next useable base value.
    // Its log must must be relatively prime to 65535
    while (gcd(G::Limit, logbase) != 1)
    {
      logbase++;
    }
    if (logbase >= G::Limit)
    {
      cerr << "Too many input blocks for Reed Solomon matrix." << endl;
      return false;
    }
    G::ValueType base = G(logbase++).ALog();

    database[index] = base;
  }

  return true;
}

template <> bool ReedSolomon<Galois16>::Process(size_t size, u32 inputindex, const void *inputbuffer, u32 outputindex, void *outputbuffer)
{
  // Look up the appropriate element in the RS matrix

  Galois16 factor = leftmatrix[outputindex * (datapresent + datamissing) + inputindex];
  // Do nothing if the factor happens to be 0
  if (factor == 0)
    return eSuccess;

#ifdef LONGMULTIPLY
  // The 8-bit long multiplication tables
  Galois16 *table = glmt->tables;

  // Split the factor into Low and High bytes
  unsigned int fl = (factor >> 0) & 0xff;
  unsigned int fh = (factor >> 8) & 0xff;

  // Get the four separate multiplication tables
  Galois16 *LL = &table[(0*256 + fl) * 256 + 0]; // factor.low  * source.low
  Galois16 *LH = &table[(1*256 + fl) * 256 + 0]; // factor.low  * source.high
  Galois16 *HL = &table[(1*256 + 0) * 256 + fh]; // factor.high * source.low
  Galois16 *HH = &table[(2*256 + fh) * 256 + 0]; // factor.high * source.high

  // Combine the four multiplication tables into two
  unsigned int L[256];
  unsigned int H[256];

#if __BYTE_ORDER == __LITTLE_ENDIAN
  unsigned int *pL = &L[0];
  unsigned int *pH = &H[0];
#else
  unsigned int *pL = &H[0];
  unsigned int *pH = &L[0];
#endif

  for (unsigned int i=0; i<256; i++)
  {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    *pL = *LL + *HL;
#else
    unsigned int temp = *LL + *HL;
    *pL = (temp >> 8) & 0xff | (temp << 8) & 0xff00;
#endif

    pL++;
    LL++;
    HL+=256;

#if __BYTE_ORDER == __LITTLE_ENDIAN
    *pH = *LH + *HH;
#else
    temp = *LH + *HH;
    *pH = (temp >> 8) & 0xff | (temp << 8) & 0xff00;
#endif

    pH++;
    LH++;
    HH++;
  }

  // Treat the buffers as arrays of 32-bit unsigned ints.
  u32 *src = (u32 *)inputbuffer;
  u32 *end = (u32 *)&((u8*)inputbuffer)[size];
  u32 *dst = (u32 *)outputbuffer;
  
  // Process the data
  while (src < end)
  {
    u32 s = *src++;

    // Use the two lookup tables computed earlier
//#if __BYTE_ORDER == __LITTLE_ENDIAN
    u32 d = *dst ^ (L[(s >> 0) & 0xff]      )
                 ^ (H[(s >> 8) & 0xff]      )
                 ^ (L[(s >> 16)& 0xff] << 16)
                 ^ (H[(s >> 24)& 0xff] << 16);
    *dst++ = d;
//#else
//    *dst++ ^= (L[(s >> 8) & 0xff]      )
//           ^  (H[(s >> 0) & 0xff]      )
//           ^  (L[(s >> 24)& 0xff] << 16)
//           ^  (H[(s >> 16)& 0xff] << 16);
//#endif
  }
#else
  // Treat the buffers as arrays of 16-bit Galois values.

  // BUG: This only works for __LITTLE_ENDIAN
  Galois16 *src = (Galois16 *)inputbuffer;
  Galois16 *end = (Galois16 *)&((u8*)inputbuffer)[size];
  Galois16 *dst = (Galois16 *)outputbuffer;

  // Process the data
  while (src < end)
  {
    *dst++ += *src++ * factor;
  }
#endif

  return eSuccess;
}

} // end namespace Par2
