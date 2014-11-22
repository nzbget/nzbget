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

#ifndef __PAR2CREATORSOURCEFILE_H__
#define __PAR2CREATORSOURCEFILE_H__

class DescriptionPacket;
class VerificationPacket;
class DiskFile;

// The Par2CreatorSourceFile contains the file verification and file description
// packet for one source file.

class Par2CreatorSourceFile
{
private:
  // Don't permit copying or assignment
  Par2CreatorSourceFile(const Par2CreatorSourceFile &other);
  Par2CreatorSourceFile& operator=(const Par2CreatorSourceFile &other);

public:
  Par2CreatorSourceFile(void);
  ~Par2CreatorSourceFile(void);

  // Open the source file and compute the Hashes and CRCs.
  bool Open(CommandLine::NoiseLevel noiselevel, const CommandLine::ExtraFile &extrafile, u64 blocksize, bool deferhashcomputation);
  void Close(void);

  // Recover the file description and file verification packets
  // in the critical packet list.
  void RecordCriticalPackets(list<CriticalPacket*> &criticalpackets);

  // Get the file id
  const MD5Hash& FileId(void) const;

  // Sort source files based on the file id hash
  static bool CompareLess(const Par2CreatorSourceFile* const &left, const Par2CreatorSourceFile* const &right);

  // Allocate the appropriate number of source blocks to the source file
  void InitialiseSourceBlocks(vector<DataBlock>::iterator &sourceblock, u64 blocksize);

  // Update the file hash and the block crc and hashes
  void UpdateHashes(u32 blocknumber, const void *buffer, size_t length);

  // Finish computation of the file hash
  void FinishHashes(void);

  // How many blocks does this source file use
  u32 BlockCount(void) const {return blockcount;}

protected:
  DescriptionPacket  *descriptionpacket;  // The file description packet.
  VerificationPacket *verificationpacket; // The file verification packet.
  DiskFile           *diskfile;           // The source file

  u64    filesize;      // The size of the source file.
  string diskfilename;  // The filename of the source file on disk.
  string parfilename;   // The filename that will be recorded in the file description packet.

  u32    blockcount;    // How many blocks the file will be divided into.

  MD5Context *contextfull; // MD5 context used to calculate the hash of the whole file
};

#endif // __PAR2CREATORSOURCEFILE_H__
