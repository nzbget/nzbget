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

// Convert hash values to hex

ostream& operator<<(ostream &result, const MD5Hash &h)
{
  char buffer[33];

  sprintf(buffer, 
          "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
          h.hash[15], h.hash[14], h.hash[13], h.hash[12],
          h.hash[11], h.hash[10], h.hash[9],  h.hash[8],
          h.hash[7],  h.hash[6],  h.hash[5],  h.hash[4],
          h.hash[3],  h.hash[2],  h.hash[1],  h.hash[0]);

  return result << buffer;
}

string MD5Hash::print(void) const
{
  char buffer[33];

  sprintf(buffer, 
          "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
          hash[15], hash[14], hash[13], hash[12],
          hash[11], hash[10], hash[9],  hash[8],
          hash[7],  hash[6],  hash[5],  hash[4],
          hash[3],  hash[2],  hash[1],  hash[0]);

  return buffer;
}

MD5State::MD5State(void)
{
  Reset();
}

// Initialise the 16 byte state
void MD5State::Reset(void)
{
  state[0] = 0x67452301;
  state[1] = 0xefcdab89;
  state[2] = 0x98badcfe;
  state[3] = 0x10325476;
}

// Update the state using 64 bytes of new data
void MD5State::UpdateState(const u32 (&block)[16])
{
  // Primitive operations
#define F1(x,y,z)    ( ((x) & (y)) | ((~(x)) & (z)) )
#define F2(x,y,z)    ( ((x) & (z)) | ((~(z)) & (y)) )
#define F3(x,y,z)    ( (x) ^ (y) ^ (z) )
#define F4(x,y,z)    ( (y) ^ ( (x) | ~(z) ) )

// The first version of ROL does not work on an Alpha CPU!
//#define ROL(x,y)   ( ((x) << (y)) | (((unsigned int)x) >> (32-y)) )
#define ROL(x,y)     ( ((x) << (y)) | (((x) >> (32-y)) & ((1<<y)-1)))
  
#define ROUND(f,w,x,y,z,k,s,ti)   w = x + ROL(w + f(x,y,z) + block[k] + ti, s)

  u32 a = state[0];
  u32 b = state[1];
  u32 c = state[2];
  u32 d = state[3];

  ROUND(F1, a, b, c, d,  0,  7, 0xd76aa478);
  ROUND(F1, d, a, b, c,  1, 12, 0xe8c7b756);
  ROUND(F1, c, d, a, b, 2, 17, 0x242070db);
  ROUND(F1, b, c, d, a,  3, 22, 0xc1bdceee);
  
  ROUND(F1, a, b, c, d,  4,  7, 0xf57c0faf);
  ROUND(F1, d, a, b, c,  5, 12, 0x4787c62a);
  ROUND(F1, c, d, a, b,  6, 17, 0xa8304613);
  ROUND(F1, b, c, d, a,  7, 22, 0xfd469501);
  
  ROUND(F1, a, b, c, d,  8,  7, 0x698098d8);
  ROUND(F1, d, a, b, c,  9, 12, 0x8b44f7af);
  ROUND(F1, c, d, a, b, 10, 17, 0xffff5bb1);
  ROUND(F1, b, c, d, a, 11, 22, 0x895cd7be);
  
  ROUND(F1, a, b, c, d, 12,  7, 0x6b901122);
  ROUND(F1, d, a, b, c, 13, 12, 0xfd987193);
  ROUND(F1, c, d, a, b, 14, 17, 0xa679438e);
  ROUND(F1, b, c, d, a, 15, 22, 0x49b40821);
  
  ROUND(F2, a, b, c, d,  1,  5, 0xf61e2562);
  ROUND(F2, d, a, b, c,  6,  9, 0xc040b340);
  ROUND(F2, c, d, a, b, 11, 14, 0x265e5a51);
  ROUND(F2, b, c, d, a,  0, 20, 0xe9b6c7aa);
  
  ROUND(F2, a, b, c, d,  5,  5, 0xd62f105d);
  ROUND(F2, d, a, b, c, 10,  9, 0x02441453);
  ROUND(F2, c, d, a, b, 15, 14, 0xd8a1e681);
  ROUND(F2, b, c, d, a,  4, 20, 0xe7d3fbc8);
  
  ROUND(F2, a, b, c, d,  9,  5, 0x21e1cde6);
  ROUND(F2, d, a, b, c, 14,  9, 0xc33707d6);
  ROUND(F2, c, d, a, b,  3, 14, 0xf4d50d87);
  ROUND(F2, b, c, d, a,  8, 20, 0x455a14ed);
  
  ROUND(F2, a, b, c, d, 13,  5, 0xa9e3e905);
  ROUND(F2, d, a, b, c,  2,  9, 0xfcefa3f8);
  ROUND(F2, c, d, a, b,  7, 14, 0x676f02d9);
  ROUND(F2, b, c, d, a, 12, 20, 0x8d2a4c8a);
  
  ROUND(F3, a, b, c, d,  5,  4, 0xfffa3942);
  ROUND(F3, d, a, b, c,  8, 11, 0x8771f681);
  ROUND(F3, c, d, a, b, 11, 16, 0x6d9d6122);
  ROUND(F3, b, c, d, a, 14, 23, 0xfde5380c);
  
  ROUND(F3, a, b, c, d,  1,  4, 0xa4beea44);
  ROUND(F3, d, a, b, c,  4, 11, 0x4bdecfa9);
  ROUND(F3, c, d, a, b,  7, 16, 0xf6bb4b60);
  ROUND(F3, b, c, d, a, 10, 23, 0xbebfbc70);
  
  ROUND(F3, a, b, c, d, 13,  4, 0x289b7ec6);
  ROUND(F3, d, a, b, c,  0, 11, 0xeaa127fa);
  ROUND(F3, c, d, a, b,  3, 16, 0xd4ef3085);
  ROUND(F3, b, c, d, a,  6, 23, 0x04881d05);
  
  ROUND(F3, a, b, c, d,  9,  4, 0xd9d4d039);
  ROUND(F3, d, a, b, c, 12, 11, 0xe6db99e5);
  ROUND(F3, c, d, a, b, 15, 16, 0x1fa27cf8);
  ROUND(F3, b, c, d, a,  2, 23, 0xc4ac5665);
  
  ROUND(F4, a, b, c, d,  0,  6, 0xf4292244);
  ROUND(F4, d, a, b, c,  7, 10, 0x432aff97);
  ROUND(F4, c, d, a, b, 14, 15, 0xab9423a7);
  ROUND(F4, b, c, d, a,  5, 21, 0xfc93a039);
  
  ROUND(F4, a, b, c, d, 12,  6, 0x655b59c3);
  ROUND(F4, d, a, b, c,  3, 10, 0x8f0ccc92);
  ROUND(F4, c, d, a, b, 10, 15, 0xffeff47d);
  ROUND(F4, b, c, d, a, 1, 21, 0x85845dd1);
  
  ROUND(F4, a, b, c, d,  8,  6, 0x6fa87e4f);
  ROUND(F4, d, a, b, c, 15, 10, 0xfe2ce6e0);
  ROUND(F4, c, d, a, b,  6, 15, 0xa3014314);
  ROUND(F4, b, c, d, a, 13, 21, 0x4e0811a1);
  
  ROUND(F4, a, b, c, d,  4,  6, 0xf7537e82);
  ROUND(F4, d, a, b, c, 11, 10, 0xbd3af235);
  ROUND(F4, c, d, a, b,  2, 15, 0x2ad7d2bb);
  ROUND(F4, b, c, d, a,  9, 21, 0xeb86d391);

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
}

MD5Context::MD5Context(void)
: MD5State()
, used(0)
, bytes(0)
{
}

void MD5Context::Reset(void)
{
  MD5State::Reset();
  used = 0;
  bytes = 0;
}

// Update using 0 bytes
void MD5Context::Update(size_t length)
{
  u32 wordblock[16];
  memset(wordblock, 0, sizeof(wordblock));

  // If there is already some data in the buffer, update
  // enough 0 bytes to take us to a whole buffer
  if (used > 0)
  {
    size_t size = min(buffersize-used, length);
    Update(wordblock, size);
    length -= size;
  }

  // Update as many whole buffers as possible
  while (length >= buffersize)
  {
    Update(wordblock, buffersize);

    length -= buffersize;
  }

  // Update any remainder
  if (length > 0)
  {
    Update(wordblock, length);
  }
}

// Update using data from a buffer
void MD5Context::Update(const void *buffer, size_t length)
{
  const unsigned char *current = (const unsigned char *)buffer;

  // Update the total amount of data processed.
  bytes += length;

  // Process any whole blocks
  while (used + length >= buffersize) 
  {
    size_t have = buffersize - used;

    memcpy(&block[used], current, have);

    current += have;
    length -= have;

    u32 wordblock[16];
    for (int i=0; i<16; i++) 
    {
      // Convert source data from little endian format to internal format if different
      wordblock[i] = ( ((u32)block[i*4+3]) << 24 ) |
                     ( ((u32)block[i*4+2]) << 16 ) |
                     ( ((u32)block[i*4+1]) <<  8 ) |
                     ( ((u32)block[i*4+0]) <<  0 );
    }

    MD5State::UpdateState(wordblock);

    used = 0;
  }

  // Store any remainder
  if (length > 0) 
  {
    memcpy(&block[used], current, length);
    used += length;
  } 
}

// Finalise the computation and extract the Hash value
void MD5Context::Final(MD5Hash &output)
{
  // Temporary work buffer
  u8 buffer[64];

  // How many bits were processed
  u64 bits = bytes << 3;

  // Pad as much as needed so that there are exactly 8 bytes needed to fill the buffer
  size_t padding;
  if (used >= buffersize-8)
  {
    padding = buffersize-8 + buffersize - used;
  }
  else
  {
    padding = buffersize-8              - used;
  }
  memset(buffer, 0, padding);
  buffer[0] = 0x80;
  Update(buffer, padding);

  // Pad with an additional 8 bytes containing the bit count in little endian format
  buffer[7] = (unsigned char)((bits >> 56) & 0xFF);
  buffer[6] = (unsigned char)((bits >> 48) & 0xFF);
  buffer[5] = (unsigned char)((bits >> 40) & 0xFF);
  buffer[4] = (unsigned char)((bits >> 32) & 0xFF);
  buffer[3] = (unsigned char)((bits >> 24) & 0xFF);
  buffer[2] = (unsigned char)((bits >> 16) & 0xFF);
  buffer[1] = (unsigned char)((bits >>  8) & 0xFF);
  buffer[0] = (unsigned char)((bits >>  0) & 0xFF);
  Update(buffer, 8);

  for (int i = 0; i < 4; i++) 
  {
    // Read out the state and convert it from internal format to little endian format
    output.hash[4*i+3] = (u8)((MD5State::state[i] >> 24) & 0xFF);
    output.hash[4*i+2] = (u8)((MD5State::state[i] >> 16) & 0xFF);
    output.hash[4*i+1] = (u8)((MD5State::state[i] >>  8) & 0xFF);
    output.hash[4*i+0] = (u8)((MD5State::state[i] >>  0) & 0xFF);
  }
}

// Return the Hash value
MD5Hash MD5Context::Hash(void) const
{
  MD5Hash output;

  for (unsigned int i = 0; i < 4; i++) 
  {
    // Read out the state and convert it from internal format to little endian format
    output.hash[4*i+3] = (unsigned char)((MD5State::state[i] >> 24) & 0xFF);
    output.hash[4*i+2] = (unsigned char)((MD5State::state[i] >> 16) & 0xFF);
    output.hash[4*i+1] = (unsigned char)((MD5State::state[i] >>  8) & 0xFF);
    output.hash[4*i+0] = (unsigned char)((MD5State::state[i] >>  0) & 0xFF);
  }

  return output;
}

ostream& operator<<(ostream &result, const MD5Context &c)
{
  char buffer[50];

  sprintf(buffer,
          "%08X%08X%08X%08X:%08X%08X",
          c.state[3],c.state[2],c.state[1],c.state[0],
          (u32)((c.bytes >> 32) & 0xffffffff),
          (u32)(c.bytes & 0xffffffff));

  return result << buffer;
}

string MD5Context::print(void) const
{
  char buffer[50];

  sprintf(buffer,
          "%08X%08X%08X%08X:%08X%08X",
          state[3],state[2],state[1],state[0],
          (u32)((bytes >> 32) & 0xffffffff),
          (u32)(bytes & 0xffffffff));

  return buffer;
}

} // end namespace Par2
