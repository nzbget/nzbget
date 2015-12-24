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

#ifndef __VERIFICATIONHASHTABLE_H__
#define __VERIFICATIONHASHTABLE_H__

namespace Par2
{

class Par2RepairerSourceFile;
class VerificationHashTable;

// The VerificationHashEntry objects form the nodes of a binary trees stored
// in a VerificationHashTable object.

// There is one VerificationHashEntry object for each data block in the original
// source files.

class VerificationHashEntry
{
public:
  VerificationHashEntry(Par2RepairerSourceFile *_sourcefile,
                        DataBlock *_datablock,
                        bool _firstblock,
                        const FILEVERIFICATIONENTRY *_verificationentry)
  {
    sourcefile = _sourcefile;
    datablock = _datablock;
    firstblock = _firstblock;

    crc = _verificationentry->crc;
    hash = _verificationentry->hash;

    left = right = same = next = 0;
  }

  ~VerificationHashEntry(void)
  {
    delete left;
    delete right;
    delete same;
  }

  // Insert the current object is a child of the specified parent
  void Insert(VerificationHashEntry **parent);

  // Search (starting at the specified parent) for an object with a matching crc
  static const VerificationHashEntry* Search(const VerificationHashEntry *entry, u32 crc);

  // Search (starting at the specified parent) for an object with a matching hash
  static const VerificationHashEntry* Search(const VerificationHashEntry *entry, const MD5Hash &hash);

  // Comparison operators for searching
  bool operator <(const VerificationHashEntry &r) const 
  {
    return (crc < r.crc) || ((crc == r.crc) && (hash < r.hash));
  }
  bool operator >(const VerificationHashEntry &r) const 
  {
    return (crc > r.crc) || ((crc == r.crc) && (hash > r.hash));
  }
  bool operator ==(const VerificationHashEntry &r) const 
  {
    return (crc == r.crc) && (hash == r.hash);
  }
  bool operator <=(const VerificationHashEntry &r) const {return !operator>(r);}
  bool operator >=(const VerificationHashEntry &r) const {return !operator<(r);}
  bool operator !=(const VerificationHashEntry &r) const {return !operator==(r);}

  // Data
  Par2RepairerSourceFile* SourceFile(void) const {return sourcefile;}
  const DataBlock* GetDataBlock(void) const {return datablock;}
  bool FirstBlock(void) const {return firstblock;}
  
  // Set/Check the associated datablock
  void SetBlock(DiskFile *diskfile, u64 offset) const;
  bool IsSet(void) const;

  u32 Checksum(void) const {return crc;}
  const MD5Hash& Hash(void) const {return hash;}

  VerificationHashEntry* Same(void) const {return same;}
  VerificationHashEntry* Next(void) const {return next;}
  void Next(VerificationHashEntry *_next) {next = _next;}

protected:
  // Data
  Par2RepairerSourceFile       *sourcefile;
  DataBlock                    *datablock;
  bool                          firstblock;

  u32                           crc;
  MD5Hash                       hash;

protected:
  // Binary tree
  VerificationHashEntry *left;
  VerificationHashEntry *right;

  // Linked list of entries with the same crc and hash
  VerificationHashEntry *same;

  // Linked list of entries in sequence for same file
  VerificationHashEntry *next;
};

inline void VerificationHashEntry::SetBlock(DiskFile *diskfile, u64 offset) const
{
  datablock->SetLocation(diskfile, offset);
}

inline bool VerificationHashEntry::IsSet(void) const
{
  return datablock->IsSet();
}

// Insert a new entry in the tree
inline void VerificationHashEntry::Insert(VerificationHashEntry **parent)
{
  while (*parent)
  {
    if (**parent < *this)
    {
      parent = &(*parent)->right;
    }
    else if (**parent > *this)
    {
      parent = &(*parent)->left;
    }
    else
    {
      break;
    }
  }

  while (*parent)
  {
    parent = &(*parent)->same;
  }

  *parent = this;
}

// Search the tree for an entry with the correct crc
inline const VerificationHashEntry* VerificationHashEntry::Search(const VerificationHashEntry *entry, u32 crc)
{
  while (entry)
  {
    if (entry->crc < crc)
    {
      entry = entry->right;
    }
    else if (entry->crc > crc)
    {
      entry = entry->left;
    }
    else
    {
      break;
    }
  }

  return entry;
}

// Search the tree for an entry with the correct hash
inline const VerificationHashEntry* VerificationHashEntry::Search(const VerificationHashEntry *entry, const MD5Hash &hash)
{
  u32 crc = entry->crc;

  while (entry)
  {
    if ((entry->crc < crc) || ((entry->crc == crc) && (entry->hash < hash)))
    {
      entry = entry->right;
    }
    else if ((entry->crc > crc) || ((entry->crc == crc) && (entry->hash > hash)))
    {
      entry = entry->left;
    }
    else
    {
      break;
    }
  }

  return entry;
}

// The VerificationHashTable object contains all of the VerificationHashEntry objects
// and is used to find matches for blocks of data in a target file that is being
// scanned.

// It is initialised by loading data from all available verification packets for the
// source files.

class VerificationHashTable
{
public:
  VerificationHashTable(void);
  ~VerificationHashTable(void);

  void SetLimit(u32 limit);

  // Load the data from the verification packet
  void Load(Par2RepairerSourceFile *sourcefile, u64 blocksize);

  // Try to find a match.
  //   nextentry   - The entry which we expect to find next. This is used
  //                 when a sequence of matches are found.
  //   sourcefile  - Which source file we would prefer to find a match for
  //                 if there are more than one possible match (with the
  //                 same crc and hash).
  //   checksummer - Provides the crc and hash values being tested.
  //   duplicate   - Set on return if the match would have been valid except
  //                 for the fact that the block has already been found.
  const VerificationHashEntry* FindMatch(const VerificationHashEntry *nextentry,
                                         const Par2RepairerSourceFile *sourcefile,
                                         FileCheckSummer &checksummer,
                                         bool &duplicate) const;

  // Look up based on the block crc
  const VerificationHashEntry* Lookup(u32 crc) const;

  // Continue lookup based on the block hash
  const VerificationHashEntry* Lookup(const VerificationHashEntry *entry,
                                      const MD5Hash &hash);

protected:
  VerificationHashEntry **hashtable;
  unsigned int hashmask;
};

// Search for an entry with the specified crc
inline const VerificationHashEntry* VerificationHashTable::Lookup(u32 crc) const
{
  if (hashmask)
  {
    return VerificationHashEntry::Search(hashtable[crc & hashmask], crc);
  }

  return 0;
}

// Search for an entry with the specified hash
inline const VerificationHashEntry* VerificationHashTable::Lookup(const VerificationHashEntry *entry,
                                                                  const MD5Hash &hash)
{
  return VerificationHashEntry::Search(entry, hash);
}

inline const VerificationHashEntry* VerificationHashTable::FindMatch(const VerificationHashEntry *suggestedentry,
                                                                     const Par2RepairerSourceFile *sourcefile,
                                                                     FileCheckSummer &checksummer,
                                                                     bool &duplicate) const
{
  duplicate = false;

  // Get the current checksum from the checksummer
  u32 crc = checksummer.Checksum();

  MD5Hash hash;
  bool havehash = false;

  // Do we know what the next entry should be
  if (0 != suggestedentry)
  {
    // Is the suggested match supposed to be the last one in the file
    if (suggestedentry->Next() == 0)
    {
      // How long should the last block be
      u64 length = suggestedentry->GetDataBlock()->GetLength();

      // Get a short checksum from the checksummer
      u32 checksum = checksummer.ShortChecksum(length);

      // Is the checksum correct
      if (checksum == suggestedentry->Checksum())
      {
        // Get a short hash from the checksummer
        hash = checksummer.ShortHash(length);

        // If the hash matches as well, then return it
        if (hash == suggestedentry->Hash())
        {
          return suggestedentry;
        }
      }
    }
    // If the suggested entry has not already been found, compare the checksum
    else if (!suggestedentry->IsSet() && suggestedentry->Checksum() == crc)
    {
      // Get the hash value from the checksummer
      havehash = true;
      hash = checksummer.Hash();

      // If the hash value matches, then return it.
      if (hash == suggestedentry->Hash())
      {
        return suggestedentry;
      }
    }
  }

  // Look for other possible matches for the checksum
  const VerificationHashEntry *nextentry = VerificationHashEntry::Search(hashtable[crc & hashmask], crc);
  if (0 == nextentry)
    return 0;

  // If we don't have the hash yet, get it
  if (!havehash)
  {
    hash = checksummer.Hash();
  }

  // Look for an entry with a matching hash
  nextentry = VerificationHashEntry::Search(nextentry, hash);
  if (0 == nextentry)
    return 0;

  // Is there one match with the same checksum and hash, or many
  if (nextentry->Same() == 0)
  {
    // If the match is for a block that is part of a target file
    // for which we already have a complete match, then don't
    // return it.
    if (nextentry->SourceFile()->GetCompleteFile() != 0)
    {
      duplicate = true;
      return 0;
    }

    // If we are close to the end of the file and the block
    // length is wrong, don't return it because it is an
    // invalid match
    if (checksummer.ShortBlock() && checksummer.BlockLength() != nextentry->GetDataBlock()->GetLength())
    {
      return 0;
    }

    // If the match was at the start of the file and it is the first
    // block for a target file, then return it.
    if (nextentry->FirstBlock() && checksummer.Offset() == 0)
    {
      return nextentry;
    }

    // Was this match actually the one which had originally been suggested
    // but which has presumably already been found
    if (nextentry == suggestedentry)
    {
      // Was the existing match in the same file as the new match
      if (nextentry->IsSet() && 
          nextentry->GetDataBlock()->GetDiskFile() == checksummer.GetDiskFile())
      {
        // Yes. Don't return it
        duplicate = true;
        return 0;
      }
      else
      {
        // No, it was in a different file. Return it.
        // This ensures that we can find a perfect match for a target
        // file even if some of the blocks had already been found
        // in a different file.
        return nextentry;
      }
    }
    else
    {
      // return it only if it has not already been used
      if (nextentry->IsSet())
      {
        duplicate = true;
        return 0;
      }

      return nextentry;
    }
  }

  // Do we prefer to match entries for a particular source file
  if (0 != sourcefile)
  {
    const VerificationHashEntry *currententry = nextentry;
    nextentry = 0;

    // We don't want entries for the wrong source file, ones that
    // have already been matched, or ones that are the wrong length
    while (currententry && (currententry->SourceFile() != sourcefile || 
                            currententry->IsSet() ||
                            (checksummer.ShortBlock() && checksummer.BlockLength() != (currententry->GetDataBlock()->GetLength()))
                           )
          )
    {
      // If we found an unused entry (which was presumably for the wrong 
      // source file) remember it (providing it is the correct length).
      if (0 == nextentry && !(currententry->IsSet() || 
                             (checksummer.ShortBlock() && checksummer.BlockLength() != (currententry->GetDataBlock()->GetLength()))
                             )
         )
      {
        nextentry = currententry;
      }

      currententry = currententry->Same();
    }

    // If we found an unused entry for the source file we want, return it
    if (0 != currententry)
      return currententry;
  }

  // Check for an unused entry which is the correct length
  while (nextentry && (nextentry->IsSet() ||
                       (checksummer.ShortBlock() && checksummer.BlockLength() != nextentry->GetDataBlock()->GetLength())
                      )
        )
  {
    nextentry = nextentry->Same();
  }

  // Return what we have found
  if (nextentry == 0)
  {
    duplicate = true;
  }

  return nextentry;
}

} // end namespace Par2

#endif // __VERIFICATIONHASHTABLE_H__
