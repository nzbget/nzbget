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

VerificationHashTable::VerificationHashTable(void)
{
  hashmask = 0;
  hashtable = 0;
}

VerificationHashTable::~VerificationHashTable(void)
{
  // Destroy the hash table
  if (hashtable)
  {
    for (unsigned int entry=0; entry<=hashmask; entry++)
    {
      delete hashtable[entry];
    }
  }

  delete [] hashtable;
}

// Allocate the hash table with a reasonable size
void VerificationHashTable::SetLimit(u32 limit)
{
  // Pick a good size for the hash table
  hashmask = 256;
  while (hashmask < limit && hashmask < 65536)
  {
    hashmask <<= 1;
  }

  // Allocate and clear the hash table
  hashtable = new VerificationHashEntry*[hashmask];
  memset(hashtable, 0, hashmask * sizeof(hashtable[0]));

  hashmask--;
}

// Load data from a verification packet
void VerificationHashTable::Load(Par2RepairerSourceFile *sourcefile, u64 blocksize)
{
  VerificationHashEntry *preventry = 0;

  // Get information from the sourcefile
  VerificationPacket *verificationpacket = sourcefile->GetVerificationPacket();
  u32 blockcount                         = verificationpacket->BlockCount();

  // Iterate throught the data blocks for the source file and the verification
  // entries in the verification packet.
  vector<DataBlock>::iterator sourceblocks       = sourcefile->SourceBlocks();
  const FILEVERIFICATIONENTRY *verificationentry = verificationpacket->VerificationEntry(0);
  u32 blocknumber                                = 0;

  while (blocknumber<blockcount)
  {
    DataBlock &datablock = *sourceblocks;

    // Create a new VerificationHashEntry with the details for the current
    // data block and verification entry.
    VerificationHashEntry *entry = new VerificationHashEntry(sourcefile, 
                                                             &datablock, 
                                                             blocknumber == 0,
                                                             verificationentry);

    // Insert the entry in the hash table
    entry->Insert(&hashtable[entry->Checksum() & hashmask]);

    // Make the previous entry point forwards to this one
    if (preventry)
    {
      preventry->Next(entry);
    }
    preventry = entry;

    ++blocknumber;
    ++sourceblocks;
    ++verificationentry;
  }
}

} // end namespace Par2
