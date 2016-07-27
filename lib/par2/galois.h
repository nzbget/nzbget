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

#ifndef __GALOIS_H__
#define __GALOIS_H__

namespace Par2
{

template <const unsigned int bits, const unsigned int generator, typename valuetype> class GaloisTable;
template <const unsigned int bits, const unsigned int generator, typename valuetype> class Galois;

template <class g> class GaloisLongMultiplyTable;

// This source file defines the Galois object for carrying out
// arithmetic in GF(2^16) using the generator 0x1100B.

// Also defined are the GaloisTable object (which contains log and
// anti log tables for use in multiplication and division), and
// the GaloisLongMultiplyTable object (which contains tables for
// carrying out multiplation of 16-bit galois numbers 8 bits at a time).

template <const unsigned int bits, const unsigned int generator, typename valuetype>
class GaloisTable
{
public:
  typedef valuetype ValueType;

  GaloisTable(void);

  enum
  {
    Bits = bits,
    Count = 1<<Bits,
    Limit = Count-1,
    Generator = generator,
  };

  ValueType log[Count];
  ValueType antilog[Count];
};

template <const unsigned int bits, const unsigned int generator, typename valuetype>
class Galois
{
public:
  typedef valuetype ValueType;

  // Basic constructors
  Galois(void) {};
  Galois(ValueType v);

  // Copy and assignment
  Galois(const Galois &right) {value = right.value;}
  Galois& operator = (const Galois &right) { value = right.value; return *this;}

  // Addition
  Galois operator + (const Galois &right) const { return (value ^ right.value); }
  Galois& operator += (const Galois &right) { value ^= right.value; return *this;}

  // Subtraction
  Galois operator - (const Galois &right) const { return (value ^ right.value); }
  Galois& operator -= (const Galois &right) { value ^= right.value; return *this;}

  // Multiplication
  Galois operator * (const Galois &right) const;
  Galois& operator *= (const Galois &right);

  // Division
  Galois operator / (const Galois &right) const;
  Galois& operator /= (const Galois &right);

  // Power
  Galois pow(unsigned int right) const;
  Galois operator ^ (unsigned int right) const;
  Galois& operator ^= (unsigned int right);

  // Cast to value and value access
  operator ValueType(void) const {return value;}
  ValueType Value(void) const {return value;}

  // Direct log and antilog
  ValueType Log(void) const;
  ValueType ALog(void) const;

  enum 
  {
    Bits  = GaloisTable<bits,generator,valuetype>::Bits,
    Count = GaloisTable<bits,generator,valuetype>::Count,
    Limit = GaloisTable<bits,generator,valuetype>::Limit,
  };

protected:
  ValueType value;

  static GaloisTable<bits,generator,valuetype> table;
};

#ifdef LONGMULTIPLY
template <class g> 
class GaloisLongMultiplyTable
{
public:
  GaloisLongMultiplyTable(void);

  typedef g G;

  enum
  {
    Bytes = ((G::Bits + 7) >> 3),
    Count = ((Bytes * (Bytes+1)) / 2),
  };

  G tables[Count * 256 * 256];
};
#endif

// Construct the log and antilog tables from the generator

template <const unsigned int bits, const unsigned int generator, typename valuetype>
inline GaloisTable<bits,generator,valuetype>::GaloisTable(void)
{
  u32 b = 1;

  for (u32 l=0; l<Limit; l++)
  {
    log[b]     = (ValueType)l;
    antilog[l] = (ValueType)b;

    b <<= 1;
    if (b & Count) b ^= Generator;
  }

  log[0] = (ValueType)Limit;
  antilog[Limit] = 0;
}


// The one and only galois log/antilog table object

template <const unsigned int bits, const unsigned int generator, typename valuetype>
GaloisTable<bits,generator,valuetype> Galois<bits,generator,valuetype>::table;


template <const unsigned int bits, const unsigned int generator, typename valuetype>
inline Galois<bits,generator,valuetype>::Galois(typename Galois<bits,generator,valuetype>::ValueType v)
{
  value = v;
}

template <const unsigned int bits, const unsigned int generator, typename valuetype>
inline Galois<bits,generator,valuetype> Galois<bits,generator,valuetype>::operator * (const Galois<bits,generator,valuetype> &right) const
{ 
  if (value == 0 || right.value == 0) return 0;
  unsigned int sum = table.log[value] + table.log[right.value];
  if (sum >= Limit) 
  {
    return table.antilog[sum-Limit];
  }
  else
  {
    return table.antilog[sum];
  }
}

template <const unsigned int bits, const unsigned int generator, typename valuetype>
inline Galois<bits,generator,valuetype>& Galois<bits,generator,valuetype>::operator *= (const Galois<bits,generator,valuetype> &right)
{ 
  if (value == 0 || right.value == 0) 
  {
    value = 0;
  }
  else
  {
    unsigned int sum = table.log[value] + table.log[right.value];
    if (sum >= Limit) 
    {
      value = table.antilog[sum-Limit];
    }
    else
    {
      value = table.antilog[sum];
    }
  }

  return *this;
}

template <const unsigned int bits, const unsigned int generator, typename valuetype>
inline Galois<bits,generator,valuetype> Galois<bits,generator,valuetype>::operator / (const Galois<bits,generator,valuetype> &right) const
{ 
  if (value == 0) return 0;

  assert(right.value != 0);
  if (right.value == 0) {return 0;} // Division by 0!

  int sum = table.log[value] - table.log[right.value];
  if (sum < 0) 
  {
    return table.antilog[sum+Limit];
  }
  else
  {
    return table.antilog[sum];
  }
}

template <const unsigned int bits, const unsigned int generator, typename valuetype>
inline Galois<bits,generator,valuetype>& Galois<bits,generator,valuetype>::operator /= (const Galois<bits,generator,valuetype> &right)
{ 
  if (value == 0) return *this;

  assert(right.value != 0);
  if (right.value == 0) {return *this;} // Division by 0!

  int sum = table.log[value] - table.log[right.value];
  if (sum < 0) 
  {
    value = table.antilog[sum+Limit];
  }
  else
  {
    value = table.antilog[sum];
  }

  return *this;
}

template <const unsigned int bits, const unsigned int generator, typename valuetype>
inline Galois<bits,generator,valuetype> Galois<bits,generator,valuetype>::pow(unsigned int right) const
{
  if (right == 0) return 1;
  if (value == 0) return 0;

  unsigned int sum = table.log[value] * right;

  sum = (sum >> Bits) + (sum & Limit);
  if (sum >= Limit) 
  {
    return table.antilog[sum-Limit];
  }
  else
  {
    return table.antilog[sum];
  }  
}

template <const unsigned int bits, const unsigned int generator, typename valuetype>
inline Galois<bits,generator,valuetype> Galois<bits,generator,valuetype>::operator ^ (unsigned int right) const
{
  if (right == 0) return 1;
  if (value == 0) return 0;

  unsigned int sum = table.log[value] * right;

  sum = (sum >> Bits) + (sum & Limit);
  if (sum >= Limit) 
  {
    return table.antilog[sum-Limit];
  }
  else
  {
    return table.antilog[sum];
  }  
}

template <const unsigned int bits, const unsigned int generator, typename valuetype>
inline Galois<bits,generator,valuetype>& Galois<bits,generator,valuetype>::operator ^= (unsigned int right)
{
  if (right == 1) {value = 1; return *this;}
  if (value == 0) return *this;

  unsigned int sum = table.log[value] * right;

  sum = (sum >> Bits) + (sum & Limit);
  if (sum >= Limit) 
  {
    value = table.antilog[sum-Limit];
  }
  else
  {
    value = table.antilog[sum];
  }

  return *this;
}

template <const unsigned int bits, const unsigned int generator, typename valuetype>
inline valuetype Galois<bits,generator,valuetype>::Log(void) const
{
  return table.log[value];
}

template <const unsigned int bits, const unsigned int generator, typename valuetype>
inline valuetype Galois<bits,generator,valuetype>::ALog(void) const
{
  return table.antilog[value];
}

#ifdef LONGMULTIPLY
template <class g> 
inline GaloisLongMultiplyTable<g>::GaloisLongMultiplyTable(void)
{
  G *table = tables;

  for (unsigned int i=0; i<Bytes; i++)
  {
    for (unsigned int j=i; j<Bytes; j++)
    {
      for (unsigned int ii=0; ii<256; ii++)
      {
        for (unsigned int jj=0; jj<256; jj++)
        {
          *table++ = G(ii << (8*i)) * G(jj << (8*j));
        }
      }
    }
  }
}
#endif

typedef Galois<8,0x11D,u8> Galois8;
typedef Galois<16,0x1100B,u16> Galois16;

} // end namespace Par2

#endif // __GALOIS_H__
