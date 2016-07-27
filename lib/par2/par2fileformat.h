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

#ifndef __PAR2FILEFORMAT_H__
#define __PAR2FILEFORMAT_H__

namespace Par2
{

// This file defines the format of a PAR2 file.

// PAR2 files consist of one or more "packets" that contain information
// that is required to be able to verify and repair damaged data files.

// All packets start with a short "header" which contains information
// used to describe what sort of data is stored in the rest of the packet
// and also to allow that data to be verified.

// This file details the format for the following packet types described
// in the PAR 2.0 specification:

//  Main Packet                        struct MAINPACKET
//  File Description Packet            struct FILEDESCRIPTIONPACKET
//  Input File Slice Checksum Packet   struct FILEVERIFICATIONPACKET
//  Recovery Slice Packet              struct RECOVERYBLOCKPACKET
//  Creator Packet                     struct CREATORPACKET


#ifdef WIN32
#pragma pack(push, 1)
#define PACKED
#else
#define PACKED __attribute__ ((packed))
#endif

#ifdef _MSC_VER
#pragma warning(disable:4200)
#endif

// All numeric fields in the file format are in LITTLE ENDIAN format.

// The types leu32 and leu64 are defined in letype.h

// Two simple types used in the packet header.
struct MAGIC      {u8 magic[8];} PACKED;
struct PACKETTYPE {u8 type[16];} PACKED;

// Every packet starts with a packet header.
struct PACKET_HEADER
{
  // Header
  MAGIC            magic;  // = {'P', 'A', 'R', '2', '\0', 'P', 'K', 'T'}
  leu64            length; // Length of entire packet including header
  MD5Hash          hash;   // Hash of entire packet excepting the first 3 fields
  MD5Hash          setid;  // Normally computed as the Hash of body of "Main Packet"
  PACKETTYPE       type;   // Used to specify the meaning of the rest of the packet
} PACKED;

// The file verification packet is used to determine whether or not any
// parts of a damaged file are useable.
// It contains a FileId used to pair it with a corresponding file description
// packet, followed by an array of hash and crc values. The number of entries in
// the array can be determined from the packet_length.
struct FILEVERIFICATIONENTRY
{
  MD5Hash        hash;
  leu32          crc;
} PACKED;
struct FILEVERIFICATIONPACKET
{
  PACKET_HEADER         header;
  // Body
  MD5Hash               fileid;     // MD5hash of file_hash_16k, file_length, file_name
  FILEVERIFICATIONENTRY entries[];
} PACKED;

// The file description packet is used to record the name of the file,
// its size, and the Hash of both the whole file and the first 16k of
// the file.
// If the name of the file is an exact multiple of 4 characters in length
// then it may not have a NULL termination. If the name of the file is not
// an exact multiple of 4, then it will be padded with 0 bytes at the
// end to make it up to a multiple of 4.
struct FILEDESCRIPTIONPACKET
{
  PACKET_HEADER    header;
  // Body
  MD5Hash          fileid;    // MD5hash of [hash16k, length, name]
  MD5Hash          hashfull;  // MD5 Hash of the whole file
  MD5Hash          hash16k;   // MD5 Hash of the first 16k of the file
  leu64            length;    // Length of the file
  u8               name[];    // Name of the file, padded with 1 to 3 zero bytes to reach 
                              // a multiple of 4 bytes.
                              // Actual length can be determined from overall packet
                              // length and then working backwards to find the first non
                              // zero character.

  //u8* name(void) {return (u8*)&this[1];}
  //const u8* name(void) const {return (const u8*)&this[1];}
} PACKED;

// The main packet is used to tie together the other packets in a recovery file.
// It specifies the block size used to virtually slice the source files, a count
// of the number of source files, and an array of Hash values used to specify
// in what order the source files are processed.
// Each entry in the fileid array corresponds with the fileid value
// in a file description packet and a file verification packet.
// The fileid array may contain more entries than the count of the number
// of recoverable files. The extra entries correspond to files that were not
// used during the creation of the recovery files and which may not therefore
// be repaired if they are found to be damaged.
struct MAINPACKET
{
  PACKET_HEADER    header;
  // Body
  leu64            blocksize;
  leu32            recoverablefilecount;
  MD5Hash          fileid[0];
  //MD5Hash* fileid(void) {return (MD5Hash*)&this[1];}
  //const MD5Hash* fileid(void) const {return (const MD5Hash*)&this[1];}
} PACKED;

// The creator packet is used to identify which program created a particular
// recovery file. It is not required for verification or recovery of damaged
// files.
struct CREATORPACKET
{
  PACKET_HEADER    header;
  // Body
  u8               client[];
  //u8* client(void) {return (u8*)&this[1];}
} PACKED;

// The recovery block packet contains a single block of recovery data along
// with the exponent value used during the computation of that block.
struct RECOVERYBLOCKPACKET
{
  PACKET_HEADER    header;
  // Body
  leu32            exponent;
//  unsigned long    data[];
//  unsigned long* data(void) {return (unsigned long*)&this[1];}
} PACKED;

#ifdef _MSC_VER
#pragma warning(default:4200)
#endif

#ifdef WIN32
#pragma pack(pop)
#endif
#undef PACKED


// Operators for comparing the MAGIC and PACKETTYPE values

inline bool operator == (const MAGIC &left, const MAGIC &right)
{
  return (0==memcmp(&left, &right, sizeof(left)));
}

inline bool operator != (const MAGIC &left, const MAGIC &right)
{
  return !operator==(left, right);
}

inline bool operator == (const PACKETTYPE &left, const PACKETTYPE &right)
{
  return (0==memcmp(&left, &right, sizeof(left)));
}

inline bool operator != (const PACKETTYPE &left, const PACKETTYPE &right)
{
  return !operator==(left, right);
}

extern MAGIC packet_magic;

extern PACKETTYPE fileverificationpacket_type;
extern PACKETTYPE filedescriptionpacket_type;
extern PACKETTYPE mainpacket_type;
extern PACKETTYPE recoveryblockpacket_type;
extern PACKETTYPE creatorpacket_type;

} // end namespace Par2

#endif //__PAR2FILEFORMAT_H__
