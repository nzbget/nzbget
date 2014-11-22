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

#include "par2cmdline.h"

#ifdef _MSC_VER
#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif
#endif

Par2CreatorSourceFile::Par2CreatorSourceFile(void)
{
  descriptionpacket = 0;
  verificationpacket = 0;
  diskfile = 0;
  blockcount = 0;
  //diskfilename;
  //parfilename;
  contextfull = 0;
}

Par2CreatorSourceFile::~Par2CreatorSourceFile(void)
{
  delete descriptionpacket;
  delete verificationpacket;
  delete diskfile;
  delete contextfull;
}

// Open the source file, compute the MD5 Hash of the whole file and the first
// 16k of the file, and then compute the FileId and store the results
// in a file description packet and a file verification packet.

bool Par2CreatorSourceFile::Open(CommandLine::NoiseLevel noiselevel, const CommandLine::ExtraFile &extrafile, u64 blocksize, bool deferhashcomputation)
{
  // Get the filename and filesize
  diskfilename = extrafile.FileName();
  filesize = extrafile.FileSize();

  // Work out how many blocks the file will be sliced into
  blockcount = (u32)((filesize + blocksize-1) / blocksize);
  
  // Determine what filename to record in the PAR2 files
  string::size_type where;
  if (string::npos != (where = diskfilename.find_last_of('\\')) ||
      string::npos != (where = diskfilename.find_last_of('/')))
  {
    parfilename = diskfilename.substr(where+1);
  }
  else
  {
    parfilename = diskfilename;
  }

  // Create the Description and Verification packets
  descriptionpacket = new DescriptionPacket;
  descriptionpacket->Create(parfilename, filesize);

  verificationpacket = new VerificationPacket;
  verificationpacket->Create(blockcount);

  // Create the diskfile object
  diskfile  = new DiskFile;

  // Open the source file
  if (!diskfile->Open(diskfilename, filesize))
    return false;

  // Do we want to defer the computation of the full file hash, and 
  // the block crc and hashes. This is only permitted if there
  // is sufficient memory available to create all recovery blocks
  // in one pass of the source files (i.e. chunksize == blocksize)
  if (deferhashcomputation)
  {
    // Initialise a buffer to read the first 16k of the source file
    size_t buffersize = 16 * 1024;
    if (buffersize > filesize)
      buffersize = (size_t)filesize;
    char *buffer = new char[buffersize];

    // Read the data from the file
    if (!diskfile->Read(0, buffer, buffersize))
    {
      diskfile->Close();
      delete [] buffer;
      return false;
    }

    // Compute the hash of the data read from the file
    MD5Context context;
    context.Update(buffer, buffersize);
    delete [] buffer;
    MD5Hash hash;
    context.Final(hash);

    // Store the hash in the descriptionpacket and compute the file id
    descriptionpacket->Hash16k(hash);

    // Compute the fileid and store it in the verification packet.
    descriptionpacket->ComputeFileId();
    verificationpacket->FileId(descriptionpacket->FileId());

    // Allocate an MD5 context for computing the file hash
    // during the recovery data generation phase
    contextfull = new MD5Context;
  }
  else
  {
    // Initialise a buffer to read the source file
    size_t buffersize = 1024*1024;
    if (buffersize > min(blocksize,filesize))
      buffersize = (size_t)min(blocksize,filesize);
    char *buffer = new char[buffersize];

    // Get ready to start reading source file to compute the hashes and crcs
    u64 offset = 0;
    u32 blocknumber = 0;
    u64 need = blocksize;

    MD5Context filecontext;
    MD5Context blockcontext;
    u32        blockcrc = 0;

    // Whilst we have not reached the end of the file
    while (offset < filesize)
    {
      // Work out how much we can read
      size_t want = (size_t)min(filesize-offset, (u64)buffersize);

      // Read some data from the file into the buffer
      if (!diskfile->Read(offset, buffer, want))
      {
        diskfile->Close();
        delete [] buffer;
        return false;
      }

      // If the new data passes the 16k boundary, compute the 16k hash for the file
      if (offset < 16384 && offset + want >= 16384)
      {
        filecontext.Update(buffer, (size_t)(16384-offset));

        MD5Context temp = filecontext;
        MD5Hash hash;
        temp.Final(hash);

        // Store the 16k hash in the file description packet
        descriptionpacket->Hash16k(hash);

        if (offset + want > 16384)
        {
          filecontext.Update(&buffer[16384-offset], (size_t)(offset+want)-16384);
        }
      }
      else
      {
        filecontext.Update(buffer, want);
      }

      // Get ready to update block hashes and crcs
      u32 used = 0;

      // Whilst we have not used all of the data we just read
      while (used < want)
      {
        // How much of it can we use for the current block
        u32 use = (u32)min(need, (u64)(want-used));

        blockcrc = ~0 ^ CRCUpdateBlock(~0 ^ blockcrc, use, &buffer[used]);
        blockcontext.Update(&buffer[used], use);

        used += use;
        need -= use;

        // Have we finished the current block
        if (need == 0)
        {
          MD5Hash blockhash;
          blockcontext.Final(blockhash);

          // Store the block hash and block crc in the file verification packet.
          verificationpacket->SetBlockHashAndCRC(blocknumber, blockhash, blockcrc);

          blocknumber++;

          // More blocks
          if (blocknumber < blockcount)
          {
            need = blocksize;

            blockcontext.Reset();
            blockcrc = 0;
          }
        }
      }

      if (noiselevel > CommandLine::nlQuiet)
      {
        // Display progress
        u32 oldfraction = (u32)(1000 * offset / filesize);
        offset += want;
        u32 newfraction = (u32)(1000 * offset / filesize);
        if (oldfraction != newfraction)
        {
          cout << newfraction/10 << '.' << newfraction%10 << "%\r" << flush;
        }
      }
    }

    // Did we finish the last block
    if (need > 0)
    {
      blockcrc = ~0 ^ CRCUpdateBlock(~0 ^ blockcrc, (size_t)need);
      blockcontext.Update((size_t)need);

      MD5Hash blockhash;
      blockcontext.Final(blockhash);

      // Store the block hash and block crc in the file verification packet.
      verificationpacket->SetBlockHashAndCRC(blocknumber, blockhash, blockcrc);

      blocknumber++;

      need = 0;
    }

    // Finish computing the file hash.
    MD5Hash filehash;
    filecontext.Final(filehash);

    // Store the file hash in the file description packet.
    descriptionpacket->HashFull(filehash);

    // Did we compute the 16k hash.
    if (offset < 16384)
    {
      // Store the 16k hash in the file description packet.
      descriptionpacket->Hash16k(filehash);
    }

    delete [] buffer;

    // Compute the fileid and store it in the verification packet.
    descriptionpacket->ComputeFileId();
    verificationpacket->FileId(descriptionpacket->FileId());
  }

  return true;
}

void Par2CreatorSourceFile::Close(void)
{
  diskfile->Close();
}


void Par2CreatorSourceFile::RecordCriticalPackets(list<CriticalPacket*> &criticalpackets)
{
  // Add the file description packet and file verification packet to
  // the critical packet list.
  criticalpackets.push_back(descriptionpacket);
  criticalpackets.push_back(verificationpacket);
}

bool Par2CreatorSourceFile::CompareLess(const Par2CreatorSourceFile* const &left, const Par2CreatorSourceFile* const &right)
{
  // Sort source files based on fileid
  return left->descriptionpacket->FileId() < right->descriptionpacket->FileId();
}

const MD5Hash& Par2CreatorSourceFile::FileId(void) const
{
  // Get the file id hash
  return descriptionpacket->FileId();
}

void Par2CreatorSourceFile::InitialiseSourceBlocks(vector<DataBlock>::iterator &sourceblock, u64 blocksize)
{
  for (u32 blocknum=0; blocknum<blockcount; blocknum++)
  {
    // Configure each source block to an appropriate offset and length within the source file.
    sourceblock->SetLocation(diskfile,                                       // file
                             blocknum * blocksize);                          // offset
    sourceblock->SetLength(min(blocksize, filesize - (u64)blocknum * blocksize)); // length
    sourceblock++;
  }
}

void Par2CreatorSourceFile::UpdateHashes(u32 blocknumber, const void *buffer, size_t length)
{
  // Compute the crc and hash of the data
  u32 blockcrc = ~0 ^ CRCUpdateBlock(~0, length, buffer);
  MD5Context blockcontext;
  blockcontext.Update(buffer, length);
  MD5Hash blockhash;
  blockcontext.Final(blockhash);

  // Store the results in the verification packet
  verificationpacket->SetBlockHashAndCRC(blocknumber, blockhash, blockcrc);


  // Update the full file hash, but don't go beyond the end of the file
  if (length > filesize - blocknumber * length)
  {
    length = (size_t)(filesize - blocknumber * (u64)length);
  }

  assert(contextfull != 0);

  contextfull->Update(buffer, length);
}

void Par2CreatorSourceFile::FinishHashes(void)
{
  assert(contextfull != 0);

  // Finish computation of the full file hash
  MD5Hash hash;
  contextfull->Final(hash);

  // Store it in the description packet
  descriptionpacket->HashFull(hash);
}
