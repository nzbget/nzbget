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

namespace Par2 {

Par2RepairerSourceFile::Par2RepairerSourceFile(DescriptionPacket *_descriptionpacket,
                                               VerificationPacket *_verificationpacket)
{
  firstblocknumber = 0;

  descriptionpacket = _descriptionpacket;
  verificationpacket = _verificationpacket;

//  verificationhashtable = 0;

  targetexists = false;
  targetfile = 0;
  completefile = 0;
}

Par2RepairerSourceFile::~Par2RepairerSourceFile(void)
{
  delete descriptionpacket;
  delete verificationpacket;

//  delete verificationhashtable;
}


void Par2RepairerSourceFile::SetDescriptionPacket(DescriptionPacket *_descriptionpacket)
{
  descriptionpacket = _descriptionpacket;
}

void Par2RepairerSourceFile::SetVerificationPacket(VerificationPacket *_verificationpacket)
{
  verificationpacket = _verificationpacket;
}

void Par2RepairerSourceFile::ComputeTargetFileName(string path)
{
  // Get a version of the filename compatible with the OS
  string filename = DiskFile::TranslateFilename(descriptionpacket->FileName());

  // Strip the path from the filename
  string::size_type where;
  if (string::npos != (where = filename.find_last_of('\\'))
      || string::npos != (where = filename.find_last_of('/'))
#ifdef WIN32
      || string::npos != (where = filename.find_last_of(':'))
#endif
     )
  {
    filename = filename.substr(where+1);
  }

  targetfilename = path + filename;
}

string Par2RepairerSourceFile::TargetFileName(void) const
{
  return targetfilename;
}

void Par2RepairerSourceFile::SetTargetFile(DiskFile *diskfile)
{
  targetfile = diskfile;
}

DiskFile* Par2RepairerSourceFile::GetTargetFile(void) const
{
  return targetfile;
}

void Par2RepairerSourceFile::SetTargetExists(bool exists)
{
  targetexists = exists;
}

bool Par2RepairerSourceFile::GetTargetExists(void) const
{
  return targetexists;
}

void Par2RepairerSourceFile::SetCompleteFile(DiskFile *diskfile)
{
  completefile = diskfile;
}

DiskFile* Par2RepairerSourceFile::GetCompleteFile(void) const
{
  return completefile;
}

// Remember which source and target blocks will be used by this file
// and set their lengths appropriately
void Par2RepairerSourceFile::SetBlocks(u32 _blocknumber,
                                       u32 _blockcount,
                                       vector<DataBlock>::iterator _sourceblocks, 
                                       vector<DataBlock>::iterator _targetblocks,
                                       u64 blocksize)
{
  firstblocknumber = _blocknumber;
  blockcount = _blockcount;
  sourceblocks = _sourceblocks;
  targetblocks = _targetblocks;

  if (blockcount > 0)
  {
    u64 filesize = descriptionpacket->FileSize();

    vector<DataBlock>::iterator sb = sourceblocks;
    for (u32 blocknumber=0; blocknumber<blockcount; ++blocknumber, ++sb)
    {
      DataBlock &datablock = *sb;

      u64 blocklength = min(blocksize, filesize-(u64)blocknumber*blocksize);

      datablock.SetLength(blocklength);
    }
  }
}

// Determine the block count from the file size and block size.
void Par2RepairerSourceFile::SetBlockCount(u64 blocksize)
{
  if (descriptionpacket)
  {
    blockcount = (u32)((descriptionpacket->FileSize() + blocksize-1) / blocksize);
  }
  else
  {
    blockcount = 0;
  }
}

} // end namespace Par2
