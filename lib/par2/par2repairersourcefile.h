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

#ifndef __PAR2REPAIRERSOURCEFILE_H__
#define __PAR2REPAIRERSOURCEFILE_H__

namespace Par2
{

enum MatchType
{
  eNoMatch = 0,
  ePartialMatch,
  eFullMatch
};

// The Par2RepairerSourceFile object is used during verification and repair
// to record details about a particular source file and the data blocks
// for that file.

class Par2RepairerSourceFile
{
public:
  // Construct the object and set the description and verification packets
  Par2RepairerSourceFile(DescriptionPacket  *descriptionpacket,
                         VerificationPacket *verificationpacket);
  ~Par2RepairerSourceFile(void);

  // Get/Set the description packet
  DescriptionPacket* GetDescriptionPacket(void) const {return descriptionpacket;}
  void SetDescriptionPacket(DescriptionPacket *descriptionpacket);

  // Get/Set the verification packet
  VerificationPacket* GetVerificationPacket(void) const {return verificationpacket;}
  void SetVerificationPacket(VerificationPacket *verificationpacket);

  // Record the details as to which data blocks belong to this source
  // file and set the length of each allocated block correctly.
  void SetBlocks(u32 _blocknumber,
                 u32 _blockcount,
                 vector<DataBlock>::iterator _sourceblocks, 
                 vector<DataBlock>::iterator _targetblocks,
                 u64 blocksize);

  // Determine the block count from the file size and block size.
  void SetBlockCount(u64 blocksize);

  // Set/Get which DiskFile will contain the final repaired version of the file
  void SetTargetFile(DiskFile *diskfile);
  DiskFile* GetTargetFile(void) const;

  // Set/Get whether or not the target file actually exists
  void SetTargetExists(bool exists);
  bool GetTargetExists(void) const;

  // Set/Get which DiskFile contains a full undamaged version of the source file
  void SetCompleteFile(DiskFile *diskfile);
  DiskFile* GetCompleteFile(void) const;

  // Compute/Get the filename for the final repaired version of the file
  void ComputeTargetFileName(string path);
  string TargetFileName(void) const;

  // Get the number of blocks that the file uses
  u32 BlockCount(void) const {return blockcount;}

  // Get the relative block number of the first block in the file
  u32 FirstBlockNumber(void) const {return firstblocknumber;}

  // Get the first source DataBlock for the file
  vector<DataBlock>::iterator SourceBlocks(void) const {return sourceblocks;}

  // Get the first target DataBlock for the file
  vector<DataBlock>::iterator TargetBlocks(void) const {return targetblocks;}

protected:
  DescriptionPacket           *descriptionpacket;   // The file description packet
  VerificationPacket          *verificationpacket;  // The file verification packet

  u32                          blockcount;          // The number of DataBlocks in the file
  u32                          firstblocknumber;    // The block number of the first DataBlock

  vector<DataBlock>::iterator  sourceblocks;        // The first source DataBlock
  vector<DataBlock>::iterator  targetblocks;        // The first target DataBlock

  bool                         targetexists;        // Whether the target file exists
  DiskFile                    *targetfile;          // The final version of the file
  DiskFile                    *completefile;        // A complete version of the file

  string                       targetfilename;      // The filename of the target file
};

} // end namespace Par2

#endif // __PAR2REPAIRERSOURCEFILE_H__
