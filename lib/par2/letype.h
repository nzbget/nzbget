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

#ifndef __LETYPE_H__
#define __LETYPE_H__

namespace Par2
{

#if __BYTE_ORDER == __LITTLE_ENDIAN

typedef u16 leu16;
typedef u32 leu32;
typedef u64 leu64;

#else

struct leu16
{
  leu16& operator=(const u16 &other);

  operator u16(void) const;

  u16 value;
};

inline leu16& leu16::operator=(const u16 &other)
{
  ((unsigned char*)&value)[0] = (unsigned char)((other >> 0) & 0xff);
  ((unsigned char*)&value)[1] = (unsigned char)((other >> 8) & 0xff);

  return *this;
}

inline leu16::operator u16(void) const
{
  return ((unsigned char*)&value)[0] << 0 |
         ((unsigned char*)&value)[1] << 8;
}


struct leu32
{
  leu32& operator=(const u32 &other);

  operator u32(void) const;

  u32 value;
};

inline leu32& leu32::operator=(const u32 &other)
{
  ((unsigned char*)&value)[0] = (unsigned char)((other >> 0) & 0xff);
  ((unsigned char*)&value)[1] = (unsigned char)((other >> 8) & 0xff);
  ((unsigned char*)&value)[2] = (unsigned char)((other >> 16) & 0xff);
  ((unsigned char*)&value)[3] = (unsigned char)((other >> 24) & 0xff);

  return *this;
}

inline leu32::operator u32(void) const
{
  return ((unsigned char*)&value)[0] << 0 |
         ((unsigned char*)&value)[1] << 8 |
         ((unsigned char*)&value)[2] << 16 |
         ((unsigned char*)&value)[3] << 24;
}


struct leu64
{
  leu64& operator=(const u64 &other);

  operator u64(void) const;

  u64 value;
};

inline leu64& leu64::operator=(const u64 &other)
{
  ((unsigned char*)&value)[0] = (unsigned char)((other >> 0) & 0xff);
  ((unsigned char*)&value)[1] = (unsigned char)((other >> 8) & 0xff);
  ((unsigned char*)&value)[2] = (unsigned char)((other >> 16) & 0xff);
  ((unsigned char*)&value)[3] = (unsigned char)((other >> 24) & 0xff);
  ((unsigned char*)&value)[4] = (unsigned char)((other >> 32) & 0xff);
  ((unsigned char*)&value)[5] = (unsigned char)((other >> 40) & 0xff);
  ((unsigned char*)&value)[6] = (unsigned char)((other >> 48) & 0xff);
  ((unsigned char*)&value)[7] = (unsigned char)((other >> 56) & 0xff);

  return *this;
}

inline leu64::operator u64(void) const
{
  return (u64)(((unsigned char*)&value)[0]) << 0 |
         (u64)(((unsigned char*)&value)[1]) << 8 |
         (u64)(((unsigned char*)&value)[2]) << 16 |
         (u64)(((unsigned char*)&value)[3]) << 24 |
         (u64)(((unsigned char*)&value)[4]) << 32 |
         (u64)(((unsigned char*)&value)[5]) << 40 |
         (u64)(((unsigned char*)&value)[6]) << 48 |
         (u64)(((unsigned char*)&value)[7]) << 56;
}

#endif

} // end namespace Par2

#endif // __LETYPE_H__
