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

Par2Repairer::Par2Repairer(std::ostream& cout, std::ostream& cerr):
  cout(cout), cerr(cerr), rs(cout, cerr)
{
  firstpacket = true;
  mainpacket = 0;
  creatorpacket = 0;

  blocksize = 0;
  sourceblockcount = 0;

  blocksallocated = false;

  availableblockcount = 0;
  missingblockcount = 0;

  completefilecount = 0;
  renamedfilecount = 0;
  damagedfilecount = 0;
  missingfilecount = 0;

  inputbuffer = 0;
  outputbuffer = 0;

  noiselevel = CommandLine::nlNormal;
  headers = new ParHeaders;
  alreadyloaded = false;

  cancelled = false;
}

Par2Repairer::~Par2Repairer(void)
{
  delete [] (u8*)inputbuffer;
  delete [] (u8*)outputbuffer;

  map<u32,RecoveryPacket*>::iterator rp = recoverypacketmap.begin();
  while (rp != recoverypacketmap.end())
  {
    delete (*rp).second;

    ++rp;
  }

  map<MD5Hash,Par2RepairerSourceFile*>::iterator sf = sourcefilemap.begin();
  while (sf != sourcefilemap.end())
  {
    Par2RepairerSourceFile *sourcefile = (*sf).second;
    delete sourcefile;

    ++sf;
  }

  delete mainpacket;
  delete creatorpacket;
  delete headers;
}


Result Par2Repairer::PreProcess(const CommandLine &commandline)
{
  //sig_filename.emit("coucou cmoa");
  // What noiselevel are we using
  noiselevel = commandline.GetNoiseLevel();

  // Get filesnames from the command line
  string par2filename = commandline.GetParFilename();
  const list<CommandLine::ExtraFile> &extrafiles = commandline.GetExtraFiles();

  // Determine the searchpath from the location of the main PAR2 file
  string name;
  DiskFile::SplitFilename(par2filename, searchpath, name);

  // Load packets from the main PAR2 file
  if (!LoadPacketsFromFile(searchpath + name))
    return eLogicError;

  // Load packets from other PAR2 files with names based on the original PAR2 file
  if (!LoadPacketsFromOtherFiles(par2filename))
    return eLogicError;

  // Load packets from any other PAR2 files whose names are given on the command line
  if (!LoadPacketsFromExtraFiles(extrafiles))
    return eLogicError;

  if (noiselevel > CommandLine::nlQuiet)
    cout << endl;

  // Check that the packets are consistent and discard any that are not
  if (!CheckPacketConsistency())
    return eInsufficientCriticalData;

  // Use the information in the main packet to get the source files
  // into the correct order and determine their filenames
  if (!CreateSourceFileList())
    return eLogicError;

  // Determine the total number of DataBlocks for the recoverable source files
  // The allocate the DataBlocks and assign them to each source file
  if (!AllocateSourceBlocks())
    return eLogicError;

  headers->setid = setid.print();
  headers->block_size = blocksize;
  headers->chunk_size = chunksize;
  headers->data_blocks = sourceblockcount;
  headers->data_size = totalsize;
  headers->recoverable_files = mainpacket->RecoverableFileCount();
  headers->other_files = 
    mainpacket->TotalFileCount() - mainpacket->RecoverableFileCount();
  sig_headers(headers);
  /*
  cout << "Values:" << endl <<
    "setid: " << setid << endl <<
    "blocksize: " << blocksize << endl <<
    "chunksize: " << chunksize << endl <<
    "sourceblockcount: " << sourceblockcount << endl <<
    "availableblockcount: " << availableblockcount << endl <<
    "missingblockcount: " << missingblockcount << endl <<
    "completefilecount: " << completefilecount << endl <<
    "renamedfilecount: " << renamedfilecount << endl <<
    "damagedfilecount: " << damagedfilecount << endl <<
    "missingfilecount: " << missingfilecount << endl <<
    "progress: " << progress << endl <<
    "totaldata: " << totaldata << endl <<
    "totalsize: " << totalsize << endl <<
    "RecoverableFileCount: " << mainpacket->RecoverableFileCount() << endl <<
    "TotalFileCount: " << mainpacket->TotalFileCount() << endl;
  */

  return eSuccess;
}


Result Par2Repairer::Process(const CommandLine &commandline, bool dorepair)
{
  //sig_filename.emit("coucou cmoa");
  // What noiselevel are we using
  noiselevel = commandline.GetNoiseLevel();

  // Get filesnames from the command line
  string par2filename = commandline.GetParFilename();
  const list<CommandLine::ExtraFile> &extrafiles = commandline.GetExtraFiles();

  // Determine the searchpath from the location of the main PAR2 file
  string name;
  DiskFile::SplitFilename(par2filename, searchpath, name);

  // Load packets from the main PAR2 file
  /*  if (!LoadPacketsFromFile(searchpath + name))
    return eLogicError;

  // Load packets from other PAR2 files with names based on the original PAR2 file
  if (!LoadPacketsFromOtherFiles(par2filename))
    return eLogicError;

  // Load packets from any other PAR2 files whose names are given on the command line
  if (!LoadPacketsFromExtraFiles(extrafiles))
    return eLogicError;

  if (noiselevel > CommandLine::nlQuiet)
    cout << endl;

  // Check that the packets are consistent and discard any that are not
  if (!CheckPacketConsistency())
    return eInsufficientCriticalData;

  // Use the information in the main packet to get the source files
  // into the correct order and determine their filenames
  if (!CreateSourceFileList())
    return eLogicError;

  // Determine the total number of DataBlocks for the recoverable source files
  // The allocate the DataBlocks and assign them to each source file
  if (!AllocateSourceBlocks())
    return eLogicError;
  */
  //   ---------- /Header -------------

  // Create a verification hash table for all files for which we have not
  // found a complete version of the file and for which we have
  // a verification packet
  if (!alreadyloaded) {
    if (!PrepareVerificationHashTable())
      return eLogicError;
    
    // Compute the table for the sliding CRC computation
    if (!ComputeWindowTable())
      return eLogicError;
    
    if (noiselevel > CommandLine::nlQuiet)
      cout << endl << "Verifying source files:" << endl << endl;
    
    // Attempt to verify all of the source files
    if (!VerifySourceFiles())
      return eFileIOError;

    if (completefilecount<mainpacket->RecoverableFileCount())
      {
	if (noiselevel > CommandLine::nlQuiet)
	  cout << endl << "Scanning extra files:" << endl << endl;
	
	// Scan any extra files specified on the command line
	if (!VerifyExtraFiles(extrafiles))
	  return eLogicError;
      }
    
    // Find out how much data we have found
    UpdateVerificationResults();
    alreadyloaded = true;
  }
  if (noiselevel > CommandLine::nlSilent)
    cout << endl;
  
  // Check the verification results and report the results
  if (!CheckVerificationResults())
    return eRepairNotPossible;
  
  // Are any of the files incomplete
  if (completefilecount<mainpacket->RecoverableFileCount())
    {
      // Do we want to carry out a repair
      if (dorepair)
	{
	  if (noiselevel > CommandLine::nlSilent)
	    cout << endl;
	  
	  // Rename any damaged or missnamed target files.
	  if (!RenameTargetFiles())
	    return eFileIOError;
	  
	  // Are we still missing any files
	  if (completefilecount<mainpacket->RecoverableFileCount())
	    {
	      // Work out which files are being repaired, create them, and allocate
	      // target DataBlocks to them, and remember them for later verification.
	      if (!CreateTargetFiles())
		return eFileIOError;
	      
	      // Work out which data blocks are available, which need to be copied
	      // directly to the output, and which need to be recreated, and compute
	      // the appropriate Reed Solomon matrix.
	      if (!ComputeRSmatrix())
		{
		  // Delete all of the partly reconstructed files
		  DeleteIncompleteTargetFiles();
		  return eFileIOError;
		}

	      if (noiselevel > CommandLine::nlSilent)
		cout << endl;
	      
	      // Allocate memory buffers for reading and writing data to disk.
	      if (!AllocateBuffers(commandline.GetMemoryLimit()))
		{
		  // Delete all of the partly reconstructed files
		  DeleteIncompleteTargetFiles();
		  return eMemoryError;
		}
	      
	      // Set the total amount of data to be processed.
	      progress = 0;
	      totaldata = blocksize * sourceblockcount * (missingblockcount > 0 ? missingblockcount : 1);
	      
	      BeginRepair();

	      // Start at an offset of 0 within a block.
	      u64 blockoffset = 0;
	      while (blockoffset < blocksize) // Continue until the end of the block.
		{
		  // Work out how much data to process this time.
		  size_t blocklength = (size_t)min((u64)chunksize, blocksize-blockoffset);
		  
		  // Read source data, process it through the RS matrix and write it to disk.
		  
		  if (!ProcessData(blockoffset, blocklength))
		    {
		      // Delete all of the partly reconstructed files
		      DeleteIncompleteTargetFiles();
		      EndRepair();
		      return eFileIOError;
		    }
		  
		  // Advance to the need offset within each block
		  blockoffset += blocklength;
		}
	      
	      EndRepair();

	      if (noiselevel > CommandLine::nlSilent)
		cout << endl << "Verifying repaired files:" << endl << endl;
	      
	      // Verify that all of the reconstructed target files are now correct
	      if (!VerifyTargetFiles())
		{
		  // Delete all of the partly reconstructed files
		  DeleteIncompleteTargetFiles();
		  return eFileIOError;
		}
	    }
	  
	  // Are all of the target files now complete?
	  if (completefilecount<mainpacket->RecoverableFileCount())
	    {
	      cerr << "Repair Failed." << endl;
	      return eRepairFailed;
	    }
	  else
	    {
	      if (noiselevel > CommandLine::nlSilent)
		cout << endl << "Repair complete." << endl;
	    }
	}
      else
	{
	  return eRepairPossible;
	}
    }
  
  return eSuccess;
}

// Load the packets from the specified file
bool Par2Repairer::LoadPacketsFromFile(string filename)
{
  // Skip the file if it has already been processed
  if (diskFileMap.Find(filename) != 0)
  {
    return true;
  }

  DiskFile *diskfile = new DiskFile(cerr);

  // Open the file
  if (!diskfile->Open(filename))
  {
    // If we could not open the file, ignore the error and 
    // proceed to the next file
    delete diskfile;
    return true;
  }

  if (noiselevel > CommandLine::nlSilent)
  {
    string path;
    string name;
    DiskFile::SplitFilename(filename, path, name);
    cout << "Loading \"" << name << "\"." << endl;
    sig_filename(name);
  }

  // How many useable packets have we found
  u32 packets = 0;

  // How many recovery packets were there
  u32 recoverypackets = 0;

  // How big is the file
  u64 filesize = diskfile->FileSize();
  if (filesize > 0)
  {
    // Allocate a buffer to read data into
    // The buffer should be large enough to hold a whole 
    // critical packet (i.e. file verification, file description, main,
    // and creator), but not necessarily a whole recovery packet.
    size_t buffersize = (size_t)min((u64)1048576, filesize);
    u8 *buffer = new u8[buffersize];

    // Progress indicator
    u64 progress = 0;

    // Start at the beginning of the file
    u64 offset = 0;

    // Continue as long as there is at least enough for the packet header
    while (offset + sizeof(PACKET_HEADER) <= filesize)
    {
      if (noiselevel > CommandLine::nlQuiet)
      {
        // Update a progress indicator
        u32 oldfraction = (u32)(1000 * progress / filesize);
        u32 newfraction = (u32)(1000 * offset / filesize);
        if (oldfraction != newfraction)
        {
          cout << "Loading: " << newfraction/10 << '.' << newfraction%10 << "%\r" << flush;
          progress = offset;
	sig_progress(newfraction);

          if (cancelled)
          {
            break;
          }
        }
      }

      // Attempt to read the next packet header
      PACKET_HEADER header;
      if (!diskfile->Read(offset, &header, sizeof(header)))
        break;

      // Does this look like it might be a packet
      if (packet_magic != header.magic)
      {
        offset++;

        // Is there still enough for at least a whole packet header
        while (offset + sizeof(PACKET_HEADER) <= filesize)
        {
          // How much can we read into the buffer
          size_t want = (size_t)min((u64)buffersize, filesize-offset);

          // Fill the buffer
          if (!diskfile->Read(offset, buffer, want))
          {
            offset = filesize;
            break;
          }

          // Scan the buffer for the magic value
          u8 *current = buffer;
          u8 *limit = &buffer[want-sizeof(PACKET_HEADER)];
          while (current <= limit && packet_magic != ((PACKET_HEADER*)current)->magic)
          {
            current++;
          }

          // What file offset did we reach
          offset += current-buffer;

          // Did we find the magic
          if (current <= limit)
          {
            memcpy(&header, current, sizeof(header));
            break;
          }
        }

        // Did we reach the end of the file
        if (offset + sizeof(PACKET_HEADER) > filesize)
        {
          break;
        }
      }

      // We have found the magic

      // Check the packet length
      if (sizeof(PACKET_HEADER) > header.length || // packet length is too small
          0 != (header.length & 3) ||              // packet length is not a multiple of 4
          filesize < offset + header.length)       // packet would extend beyond the end of the file
      {
        offset++;
        continue;
      }

      // Compute the MD5 Hash of the packet
      MD5Context context;
      context.Update(&header.setid, sizeof(header)-offsetof(PACKET_HEADER, setid));

      // How much more do I need to read to get the whole packet
      u64 current = offset+sizeof(PACKET_HEADER);
      u64 limit = offset+header.length;
      while (current < limit)
      {
        size_t want = (size_t)min((u64)buffersize, limit-current);

        if (!diskfile->Read(current, buffer, want))
          break;

        context.Update(buffer, want);

        current += want;
      }

      // Did the whole packet get processed
      if (current<limit)
      {
        offset++;
        continue;
      }

      // Check the calculated packet hash against the value in the header
      MD5Hash hash;
      context.Final(hash);
      if (hash != header.hash)
      {
        offset++;
        continue;
      }

      // If this is the first packet that we have found then record the setid
      if (firstpacket)
      {
        setid = header.setid;
        firstpacket = false;
      }

      // Is the packet from the correct set
      if (setid == header.setid)
      {
        // Is it a packet type that we are interested in
        if (recoveryblockpacket_type == header.type)
        {
          if (LoadRecoveryPacket(diskfile, offset, header))
          {
            recoverypackets++;
            packets++;
          }
        }
        else if (fileverificationpacket_type == header.type)
        {
          if (LoadVerificationPacket(diskfile, offset, header))
          {
            packets++;
          }
        }
        else if (filedescriptionpacket_type == header.type)
        {
          if (LoadDescriptionPacket(diskfile, offset, header))
          {
            packets++;
          }
        }
        else if (mainpacket_type == header.type)
        {
          if (LoadMainPacket(diskfile, offset, header))
          {
            packets++;
          }
        }
        else if (creatorpacket_type == header.type)
        {
          if (LoadCreatorPacket(diskfile, offset, header))
          {
            packets++;
          }
        }
      }

      // Advance to the next packet
      offset += header.length;
    }

    delete [] buffer;
  }

  // We have finished with the file for now
  diskfile->Close();

  // Did we actually find any interesting packets
  if (packets > 0)
  {
    if (noiselevel > CommandLine::nlQuiet)
    {
      cout << "Loaded " << packets << " new packets";
      if (recoverypackets > 0) cout << " including " << recoverypackets << " recovery blocks";
      cout << endl;
    }

    // Remember that the file was processed
    bool success = diskFileMap.Insert(diskfile);
    assert(success); (void)success;
  }
  else
  {
    if (noiselevel > CommandLine::nlQuiet)
      cout << "No new packets found" << endl;
    delete diskfile;
  }
  
  if (cancelled)
  {
    return false;
  }

  return true;
}

// Finish loading a recovery packet
bool Par2Repairer::LoadRecoveryPacket(DiskFile *diskfile, u64 offset, PACKET_HEADER &header)
{
  RecoveryPacket *packet = new RecoveryPacket;

  // Load the packet from disk
  if (!packet->Load(diskfile, offset, header))
  {
    delete packet;
    return false;
  }

  // What is the exponent value of this recovery packet
  u32 exponent = packet->Exponent();

  // Try to insert the new packet into the recovery packet map
  pair<map<u32,RecoveryPacket*>::const_iterator, bool> location = recoverypacketmap.insert(pair<u32,RecoveryPacket*>(exponent, packet));

  // Did the insert fail
  if (!location.second)
  {
    // The packet must be a duplicate of one we already have
    delete packet;
    return false;
  }

  return true;
}

// Finish loading a file description packet
bool Par2Repairer::LoadDescriptionPacket(DiskFile *diskfile, u64 offset, PACKET_HEADER &header)
{
  DescriptionPacket *packet = new DescriptionPacket;

  // Load the packet from disk
  if (!packet->Load(diskfile, offset, header))
  {
    delete packet;
    return false;
  }

  // What is the fileid
  const MD5Hash &fileid = packet->FileId();

  // Look up the fileid in the source file map for an existing source file entry
  map<MD5Hash, Par2RepairerSourceFile*>::iterator sfmi = sourcefilemap.find(fileid);
  Par2RepairerSourceFile *sourcefile = (sfmi == sourcefilemap.end()) ? 0 :sfmi->second;

  // Was there an existing source file
  if (sourcefile)
  {
    // Does the source file already have a description packet
    if (sourcefile->GetDescriptionPacket())
    {
      // Yes. We don't need another copy
      delete packet;
      return false;
    }
    else
    {
      // No. Store the packet in the source file
      sourcefile->SetDescriptionPacket(packet);
      return true;
    }
  }
  else
  {
    // Create a new source file for the packet
    sourcefile = new Par2RepairerSourceFile(packet, NULL);

    // Record the source file in the source file map
    sourcefilemap.insert(pair<MD5Hash, Par2RepairerSourceFile*>(fileid, sourcefile));

    return true;
  }
}

// Finish loading a file verification packet
bool Par2Repairer::LoadVerificationPacket(DiskFile *diskfile, u64 offset, PACKET_HEADER &header)
{
  VerificationPacket *packet = new VerificationPacket;

  // Load the packet from disk
  if (!packet->Load(diskfile, offset, header))
  {
    delete packet;
    return false;
  }

  // What is the fileid
  const MD5Hash &fileid = packet->FileId();

  // Look up the fileid in the source file map for an existing source file entry
  map<MD5Hash, Par2RepairerSourceFile*>::iterator sfmi = sourcefilemap.find(fileid);
  Par2RepairerSourceFile *sourcefile = (sfmi == sourcefilemap.end()) ? 0 :sfmi->second;

  // Was there an existing source file
  if (sourcefile)
  {
    // Does the source file already have a verification packet
    if (sourcefile->GetVerificationPacket())
    {
      // Yes. We don't need another copy.
      delete packet;
      return false;
    }
    else
    {
      // No. Store the packet in the source file
      sourcefile->SetVerificationPacket(packet);

      return true;
    }
  }
  else
  {
    // Create a new source file for the packet
    sourcefile = new Par2RepairerSourceFile(NULL, packet);

    // Record the source file in the source file map
    sourcefilemap.insert(pair<MD5Hash, Par2RepairerSourceFile*>(fileid, sourcefile));

    return true;
  }
}

// Finish loading the main packet
bool Par2Repairer::LoadMainPacket(DiskFile *diskfile, u64 offset, PACKET_HEADER &header)
{
  // Do we already have a main packet
  if (0 != mainpacket)
    return false;

  MainPacket *packet = new MainPacket;

  // Load the packet from disk;
  if (!packet->Load(diskfile, offset, header))
  {
    delete packet;
    return false;
  }

  mainpacket = packet;

  return true;
}

// Finish loading the creator packet
bool Par2Repairer::LoadCreatorPacket(DiskFile *diskfile, u64 offset, PACKET_HEADER &header)
{
  // Do we already have a creator packet
  if (0 != creatorpacket)
    return false;

  CreatorPacket *packet = new CreatorPacket;

  // Load the packet from disk;
  if (!packet->Load(diskfile, offset, header))
  {
    delete packet;
    return false;
  }

  creatorpacket = packet;

  return true;
}

// Load packets from other PAR2 files with names based on the original PAR2 file
bool Par2Repairer::LoadPacketsFromOtherFiles(string filename)
{
  // Split the original PAR2 filename into path and name parts
  string path;
  string name;
  DiskFile::SplitFilename(filename, path, name);

  string::size_type where;

  // Trim ".par2" off of the end original name

  // Look for the last "." in the filename
  while (string::npos != (where = name.find_last_of('.')))
  {
    // Trim what follows the last .
    string tail = name.substr(where+1);
    name = name.substr(0,where);

    // Was what followed the last "." "par2"
    if (0 == stricmp(tail.c_str(), "par2"))
      break;
  }

  // If what is left ends in ".volNNN-NNN" or ".volNNN+NNN" strip that as well

  // Is there another "."
  if (string::npos != (where = name.find_last_of('.')))
  {
    // What follows the "."
    string tail = name.substr(where+1);

    // Scan what follows the last "." to see of it matches vol123-456 or vol123+456
    int n = 0;
    string::const_iterator p;
    for (p=tail.begin(); p!=tail.end(); ++p)
    {
      char ch = *p;

      if (0 == n)
      {
        if (tolower(ch) == 'v') { n++; } else { break; }
      }
      else if (1 == n)
      {
        if (tolower(ch) == 'o') { n++; } else { break; }
      }
      else if (2 == n)
      {
        if (tolower(ch) == 'l') { n++; } else { break; }
      }
      else if (3 == n)
      {
        if (isdigit(ch)) {} else if (ch == '-' || ch == '+') { n++; } else { break; }
      }
      else if (4 == n)
      {
        if (isdigit(ch)) {} else { break; }
      }
    }

    // If we matched then retain only what preceeds the "."
    if (p == tail.end())
    {
      name = name.substr(0,where);
    }
  }

  // Find files called "*.par2" or "name.*.par2"

  {
    string wildcard = name.empty() ? "*.par2" : name + ".*.par2";
    list<string> *files = DiskFile::FindFiles(path, wildcard);

    // Load packets from each file that was found
    for (list<string>::const_iterator s=files->begin(); s!=files->end(); ++s)
    {
      LoadPacketsFromFile(*s);
      if (cancelled)
      {
        break;
      }
    }

    delete files;
    if (cancelled)
    {
      return false;
    }
  }

  {
    string wildcard = name.empty() ? "*.PAR2" : name + ".*.PAR2";
    list<string> *files = DiskFile::FindFiles(path, wildcard);

    // Load packets from each file that was found
    for (list<string>::const_iterator s=files->begin(); s!=files->end(); ++s)
    {
      LoadPacketsFromFile(*s);
      if (cancelled)
      {
        break;
      }
    }

    delete files;
    if (cancelled)
    {
      return false;
    }
  }

  return true;
}

// Load packets from any other PAR2 files whose names are given on the command line
bool Par2Repairer::LoadPacketsFromExtraFiles(const list<CommandLine::ExtraFile> &extrafiles)
{
  for (ExtraFileIterator i=extrafiles.begin(); i!=extrafiles.end(); i++)
  {
    string filename = i->FileName();

    // If the filename contains ".par2" anywhere
    if (string::npos != filename.find(".par2") ||
        string::npos != filename.find(".PAR2"))
    {
      LoadPacketsFromFile(filename);
      if (cancelled)
      {
        break;
      }
    }
  }

  if (cancelled)
  {
    return false;
  }

  return true;
}

// Check that the packets are consistent and discard any that are not
bool Par2Repairer::CheckPacketConsistency(void)
{
  // Do we have a main packet
  if (0 == mainpacket)
  {
    // If we don't have a main packet, then there is nothing more that we can do.
    // We cannot verify or repair any files.

    cerr << "Main packet not found." << endl;
    return false;
  }

  // Remember the block size from the main packet
  blocksize = mainpacket->BlockSize();

  // Check that the recovery blocks have the correct amount of data
  // and discard any that don't
  {
    map<u32,RecoveryPacket*>::iterator rp = recoverypacketmap.begin();
    while (rp != recoverypacketmap.end())
    {
      if (rp->second->BlockSize() == blocksize)
      {
        ++rp;
      }
      else
      {
        cerr << "Incorrect sized recovery block for exponent " << rp->second->Exponent() << " discarded" << endl;

        delete rp->second;
        map<u32,RecoveryPacket*>::iterator x = rp++;
        recoverypacketmap.erase(x);
      }
    }
  }

  // Check for source files that have no description packet or where the
  // verification packet has the wrong number of entries and discard them.
  {
    map<MD5Hash, Par2RepairerSourceFile*>::iterator sf = sourcefilemap.begin();
    while (sf != sourcefilemap.end())
    {
      // Do we have a description packet
      DescriptionPacket *descriptionpacket = sf->second->GetDescriptionPacket();
      if (descriptionpacket == 0)
      {
        // No description packet

        // Discard the source file
        delete sf->second;
        map<MD5Hash, Par2RepairerSourceFile*>::iterator x = sf++;
        sourcefilemap.erase(x);

        continue;
      }

      // Compute and store the block count from the filesize and blocksize
      sf->second->SetBlockCount(blocksize);

      // Do we have a verification packet
      VerificationPacket *verificationpacket = sf->second->GetVerificationPacket();
      if (verificationpacket == 0)
      {
        // No verification packet

        // That is ok, but we won't be able to use block verification.

        // Proceed to the next file.
        ++sf;

        continue;
      }

      // Work out the block count for the file from the file size
      // and compare that with the verification packet
      u64 filesize = descriptionpacket->FileSize();
      u32 blockcount = verificationpacket->BlockCount();

      if ((filesize + blocksize-1) / blocksize != (u64)blockcount)
      {
        // The block counts are different!

        cerr << "Incorrectly sized verification packet for \"" << descriptionpacket->FileName() << "\" discarded" << endl;

        // Discard the source file

        delete sf->second;
        map<MD5Hash, Par2RepairerSourceFile*>::iterator x = sf++;
        sourcefilemap.erase(x);

        continue;
      }

      // Everything is ok.

      // Proceed to the next file
      ++sf;
    }
  }

  if (noiselevel > CommandLine::nlQuiet)
  {
    cout << "There are " 
         << mainpacket->RecoverableFileCount()
         << " recoverable files and "
         << mainpacket->TotalFileCount() - mainpacket->RecoverableFileCount()
         << " other files." 
         << endl;

    cout << "The block size used was "
         << blocksize
         << " bytes."
         << endl;
  }

  return true;
}

// Use the information in the main packet to get the source files
// into the correct order and determine their filenames
bool Par2Repairer::CreateSourceFileList(void)
{
  // For each FileId entry in the main packet
  for (u32 filenumber=0; filenumber<mainpacket->TotalFileCount(); filenumber++)
  {
    const MD5Hash &fileid = mainpacket->FileId(filenumber);

    // Look up the fileid in the source file map
    map<MD5Hash, Par2RepairerSourceFile*>::iterator sfmi = sourcefilemap.find(fileid);
    Par2RepairerSourceFile *sourcefile = (sfmi == sourcefilemap.end()) ? 0 :sfmi->second;

    if (sourcefile)
    {
      sourcefile->ComputeTargetFileName(searchpath);
    }

    sourcefiles.push_back(sourcefile);
  }

  return true;
}

// Determine the total number of DataBlocks for the recoverable source files
// The allocate the DataBlocks and assign them to each source file
bool Par2Repairer::AllocateSourceBlocks(void)
{
  sourceblockcount = 0;

  u32 filenumber = 0;
  vector<Par2RepairerSourceFile*>::iterator sf = sourcefiles.begin();

  // For each recoverable source file
  while (filenumber < mainpacket->RecoverableFileCount() && sf != sourcefiles.end())
  {
    // Do we have a source file
    Par2RepairerSourceFile *sourcefile = *sf;
    if (sourcefile)
    {
      sourceblockcount += sourcefile->BlockCount();
    }
    else
    {
      // No details for this source file so we don't know what the
      // total number of source blocks is
//      sourceblockcount = 0;
//      break;
    }

    ++sf;
    ++filenumber;
  }

  // Did we determine the total number of source blocks
  if (sourceblockcount > 0)
  {
    // Yes. 
    
    // Allocate all of the Source and Target DataBlocks (which will be used
    // to read and write data to disk).

    sourceblocks.resize(sourceblockcount);
    targetblocks.resize(sourceblockcount);

    // Which DataBlocks will be allocated first
    vector<DataBlock>::iterator sourceblock = sourceblocks.begin();
    vector<DataBlock>::iterator targetblock = targetblocks.begin();

    u32 blocknumber = 0;
    totalsize = 0;

    filenumber = 0;
    sf = sourcefiles.begin();

    while (filenumber < mainpacket->RecoverableFileCount() && sf != sourcefiles.end())
    {
      Par2RepairerSourceFile *sourcefile = *sf;

      if (sourcefile)
      {
        totalsize += sourcefile->GetDescriptionPacket()->FileSize();
        u32 blockcount = sourcefile->BlockCount();

        // Allocate the source and target DataBlocks to the sourcefile
        sourcefile->SetBlocks(blocknumber, blockcount, sourceblock, targetblock, blocksize);

        blocknumber++;

        sourceblock += blockcount;
        targetblock += blockcount;
      }

      ++sf;
      ++filenumber;
    }

    blocksallocated = true;

    if (noiselevel > CommandLine::nlQuiet)
    {
      cout << "There are a total of "
           << sourceblockcount
           << " data blocks."
           << endl;

      cout << "The total size of the data files is "
           << totalsize
           << " bytes."
           << endl;
    }
  }

  return true;
}

// Create a verification hash table for all files for which we have not
// found a complete version of the file and for which we have
// a verification packet
bool Par2Repairer::PrepareVerificationHashTable(void)
{
  // Choose a size for the hash table
  verificationhashtable.SetLimit(sourceblockcount);

  // Will any files be block verifiable
  blockverifiable = false;

  // For each source file
  vector<Par2RepairerSourceFile*>::iterator sf = sourcefiles.begin();
  while (sf != sourcefiles.end())
  {
    // Get the source file
    Par2RepairerSourceFile *sourcefile = *sf;

    if (sourcefile)
    {
      // Do we have a verification packet
      if (0 != sourcefile->GetVerificationPacket())
      {
        // Yes. Load the verification entries into the hash table
        verificationhashtable.Load(sourcefile, blocksize);

        blockverifiable = true;
      }
      else
      {
        // No. We can only check the whole file
        unverifiablesourcefiles.push_back(sourcefile);
      }
    }

    ++sf;
  }

  return true;
}

// Compute the table for the sliding CRC computation
bool Par2Repairer::ComputeWindowTable(void)
{
  if (blockverifiable)
  {
    GenerateWindowTable(blocksize, windowtable);
    windowmask = ComputeWindowMask(blocksize);
  }

  return true;
}

static bool SortSourceFilesByFileName(Par2RepairerSourceFile *low,
                                      Par2RepairerSourceFile *high)
{
  return low->TargetFileName() < high->TargetFileName();
}

// Attempt to verify all of the source files
bool Par2Repairer::VerifySourceFiles(void)
{
  bool finalresult = true;

  // Created a sorted list of the source files and verify them in that
  // order rather than the order they are in the main packet.
  vector<Par2RepairerSourceFile*> sortedfiles;

  u32 filenumber = 0;
  vector<Par2RepairerSourceFile*>::iterator sf = sourcefiles.begin();
  while (sf != sourcefiles.end())
  {
    // Do we have a source file
    Par2RepairerSourceFile *sourcefile = *sf;
    if (sourcefile)
    {
      sortedfiles.push_back(sourcefile);
    }
    else
    {
      // Was this one of the recoverable files
      if (filenumber < mainpacket->RecoverableFileCount())
      {
        cerr << "No details available for recoverable file number " << filenumber+1 << "." << endl << "Recovery will not be possible." << endl;

        // Set error but let verification of other files continue
        finalresult = false;
      }
      else
      {
        cerr << "No details available for non-recoverable file number " << filenumber - mainpacket->RecoverableFileCount() + 1 << endl;
      }
    }

    ++sf;
  }

  sort(sortedfiles.begin(), sortedfiles.end(), SortSourceFilesByFileName);

  // Start verifying the files
  sf = sortedfiles.begin();
  while (sf != sortedfiles.end())
  {
    if (cancelled)
    {
      return false;
    }

    // Do we have a source file
    Par2RepairerSourceFile *sourcefile = *sf;

    // What filename does the file use
    string filename = sourcefile->TargetFileName();

    // Check to see if we have already used this file
    if (diskFileMap.Find(filename) != 0)
    {
      // The file has already been used!

      cerr << "Source file " << filenumber+1 << " is a duplicate." << endl;

      return false;
    }

    DiskFile *diskfile = new DiskFile(cerr);

    // Does the target file exist
    if (diskfile->Open(filename))
    {
      // Yes. Record that fact.
      sourcefile->SetTargetExists(true);

      // Remember that the DiskFile is the target file
      sourcefile->SetTargetFile(diskfile);

      // Remember that we have processed this file
      bool success = diskFileMap.Insert(diskfile);
      assert(success); (void)success;
      // Do the actual verification
      if (!VerifyDataFile(diskfile, sourcefile))
        finalresult = false;

      // We have finished with the file for now
      diskfile->Close();

      // Find out how much data we have found
      UpdateVerificationResults();
    }
    else
    {
      // The file does not exist.
      delete diskfile;

      if (noiselevel > CommandLine::nlSilent)
      {
        string path;
        string name;
        DiskFile::SplitFilename(filename, path, name);

        cout << "Target: \"" << name << "\" - missing." << endl;
	sig_done(name, 0, sourcefile && sourcefile->GetVerificationPacket() ? sourcefile->GetVerificationPacket()->BlockCount() : 0);
      }
    }

    ++sf;
  }

  return finalresult;
}

// Scan any extra files specified on the command line
bool Par2Repairer::VerifyExtraFiles(const list<CommandLine::ExtraFile> &extrafiles)
{
  for (ExtraFileIterator i=extrafiles.begin(); 
       i!=extrafiles.end() && completefilecount<mainpacket->RecoverableFileCount(); 
       ++i)
  {
    string filename = i->FileName();

    // If the filename does not include ".par2" we are interested in it.
    if (string::npos == filename.find(".par2") &&
        string::npos == filename.find(".PAR2"))
    {
      filename = DiskFile::GetCanonicalPathname(filename);

      // Has this file already been dealt with
      if (diskFileMap.Find(filename) == 0)
      {
        DiskFile *diskfile = new DiskFile(cerr);

        // Does the file exist
        if (!diskfile->Open(filename))
        {
          delete diskfile;
          continue;
        }

        // Remember that we have processed this file
        bool success = diskFileMap.Insert(diskfile);
        assert(success); (void)success;

        // Do the actual verification
        VerifyDataFile(diskfile, 0);
        // Ignore errors

        // We have finished with the file for now
        diskfile->Close();

        // Find out how much data we have found
        UpdateVerificationResults();
      }
    }
  }

  return true;
}

// Attempt to match the data in the DiskFile with the source file
bool Par2Repairer::VerifyDataFile(DiskFile *diskfile, Par2RepairerSourceFile *sourcefile)
{
  MatchType matchtype; // What type of match was made
  MD5Hash hashfull;    // The MD5 Hash of the whole file
  MD5Hash hash16k;     // The MD5 Hash of the files 16k of the file

  // Are there any files that can be verified at the block level
  if (blockverifiable)
  {
    u32 count;

    // Scan the file at the block level.

    if (!ScanDataFile(diskfile,   // [in]      The file to scan
                      sourcefile, // [in/out]  Modified in the match is for another source file
                      matchtype,  // [out]
                      hashfull,   // [out]
                      hash16k,    // [out]
                      count))     // [out]
      return false;

    switch (matchtype)
    {
    case eNoMatch:
      // No data was found at all.

      // Continue to next test.
      break;
    case ePartialMatch:
      {
        // We found some data.

        // Return them.
        return true;
      }
      break;
    case eFullMatch:
      {
        // We found a perfect match.

        sourcefile->SetCompleteFile(diskfile);

        // Return the match
        return true;
      }
      break;
    }
  }

  // We did not find a match for any blocks of data within the file, but if 
  // there are any files for which we did not have a verification packet
  // we can try a simple match of the hash for the whole file.

  // Are there any files that cannot be verified at the block level
  if (unverifiablesourcefiles.size() > 0)
  {
    // Would we have already computed the file hashes
    if (!blockverifiable)
    {
      u64 filesize = diskfile->FileSize();

      size_t buffersize = 1024*1024;
      if (buffersize > min(blocksize, filesize))
        buffersize = (size_t)min(blocksize, filesize);

      char *buffer = new char[buffersize];

      u64 offset = 0;

      MD5Context context;

      while (offset < filesize)
      {
        size_t want = (size_t)min((u64)buffersize, filesize-offset);

        if (!diskfile->Read(offset, buffer, want))
        {
          delete [] buffer;
          return false;
        }

        // Will the newly read data reach the 16k boundary
        if (offset < 16384 && offset + want >= 16384)
        {
          context.Update(buffer, (size_t)(16384-offset));

          // Compute the 16k hash
          MD5Context temp = context;
          temp.Final(hash16k);

          // Is there more data
          if (offset + want > 16384)
          {
            context.Update(&buffer[16384-offset], (size_t)(offset+want)-16384);
          }
        }
        else
        {
          context.Update(buffer, want);
        }

        offset += want;
      }

      // Compute the file hash
      MD5Hash hashfull;
      context.Final(hashfull);

      // If we did not have 16k of data, then the 16k hash
      // is the same as the full hash
      if (filesize < 16384)
      {
        hash16k = hashfull;
      }
    }

    list<Par2RepairerSourceFile*>::iterator sf = unverifiablesourcefiles.begin();

    // Compare the hash values of each source file for a match
    while (sf != unverifiablesourcefiles.end())
    {
      sourcefile = *sf;

      // Does the file match
      if (sourcefile->GetCompleteFile() == 0 &&
          diskfile->FileSize() == sourcefile->GetDescriptionPacket()->FileSize() &&
          hash16k == sourcefile->GetDescriptionPacket()->Hash16k() &&
          hashfull == sourcefile->GetDescriptionPacket()->HashFull())
      {
        if (noiselevel > CommandLine::nlSilent)
          cout << diskfile->FileName() << " is a perfect match for " << sourcefile->GetDescriptionPacket()->FileName() << endl;

        // Record that we have a perfect match for this source file
        sourcefile->SetCompleteFile(diskfile);

        if (blocksallocated)
        {
          // Allocate all of the DataBlocks for the source file to the DiskFile

          u64 offset = 0;
          u64 filesize = sourcefile->GetDescriptionPacket()->FileSize();

          vector<DataBlock>::iterator sb = sourcefile->SourceBlocks();

          while (offset < filesize)
          {
            DataBlock &datablock = *sb;

            datablock.SetLocation(diskfile, offset);
            datablock.SetLength(min(blocksize, filesize-offset));

            offset += blocksize;
            ++sb;
          }
        }

        // Return the match
        return true;
      }

      ++sf;
    }
  }

  return true;
}

// Perform a sliding window scan of the DiskFile looking for blocks of data that 
// might belong to any of the source files (for which a verification packet was
// available). If a block of data might be from more than one source file, prefer
// the one specified by the "sourcefile" parameter. If the first data block
// found is for a different source file then "sourcefile" is changed accordingly.
bool Par2Repairer::ScanDataFile(DiskFile                *diskfile,    // [in]
                                Par2RepairerSourceFile* &sourcefile,  // [in/out]
                                MatchType               &matchtype,   // [out]
                                MD5Hash                 &hashfull,    // [out]
                                MD5Hash                 &hash16k,     // [out]
                                u32                     &count)       // [out]
{
  // Remember which file we wanted to match
  Par2RepairerSourceFile *originalsourcefile = sourcefile;

  matchtype = eNoMatch;

  // Is the file empty
  if (diskfile->FileSize() == 0)
  {
    // If the file is empty, then just return
    return true;
  }

  string path;
  string name;
  DiskFile::SplitFilename(diskfile->FileName(), path, name);

  sig_filename(name);

  string shortname;
  if (name.size() > 56)
  {
    shortname = name.substr(0, 28) + "..." + name.substr(name.size()-28);
  }
  else
  {
    shortname = name;
  }

  // Create the checksummer for the file and start reading from it
  FileCheckSummer filechecksummer(diskfile, blocksize, windowtable, windowmask);
  if (!filechecksummer.Start())
    return false;

  // Assume we will make a perfect match for the file
  matchtype = eFullMatch;

  // How many matches have we had
  count = 0;

  // How many blocks have already been found
  u32 duplicatecount = 0;

  // Have we found data blocks in this file that belong to more than one target file
  bool multipletargets = false;

  // Which block do we expect to find first
  const VerificationHashEntry *nextentry = 0;

  u64 progress = 0;

  // Whilst we have not reached the end of the file
  while (filechecksummer.Offset() < diskfile->FileSize())
  {
    if (noiselevel > CommandLine::nlQuiet)
    {
      // Update a progress indicator
      u32 oldfraction = (u32)(1000 * progress / diskfile->FileSize());
      u32 newfraction = (u32)(1000 * (progress = filechecksummer.Offset()) / diskfile->FileSize());
      if (oldfraction != newfraction)
      {
        cout << "Scanning: \"" << shortname << "\": " << newfraction/10 << '.' << newfraction%10 << "%\r" << flush;
	sig_progress(newfraction);

        if (cancelled)
        {
          break;
        }
      }
    }

    // If we fail to find a match, it might be because it was a duplicate of a block
    // that we have already found.
    bool duplicate;

    // Look for a match
    const VerificationHashEntry *currententry = verificationhashtable.FindMatch(nextentry, sourcefile, filechecksummer, duplicate);

    // Did we find a match
    if (currententry != 0)
    {
      // Is this the first match
      if (count == 0)
      {
        // Which source file was it
        sourcefile = currententry->SourceFile();

        // If the first match found was not actually the first block
        // for the source file, or it was not at the start of the
        // data file: then this is a partial match.
        if (!currententry->FirstBlock() || filechecksummer.Offset() != 0)
        {
          matchtype = ePartialMatch;
        }
      }
      else
      {
        // If the match found is not the one which was expected
        // then this is a partial match

        if (currententry != nextentry)
        {
          matchtype = ePartialMatch;
        }

        // Is the match from a different source file
        if (sourcefile != currententry->SourceFile())
        {
          multipletargets = true;
        }
      }

      if (blocksallocated)
      {
        // Record the match
        currententry->SetBlock(diskfile, filechecksummer.Offset());
      }

      // Update the number of matches found
      count++;

      // What entry do we expect next
      nextentry = currententry->Next();

      // Advance to the next block
      if (!filechecksummer.Jump(currententry->GetDataBlock()->GetLength()))
        return false;
    }
    else
    {
      // This cannot be a perfect match
      matchtype = ePartialMatch;

      // Was this a duplicate match
      if (duplicate)
      {
        duplicatecount++;

        // What entry would we expect next
        nextentry = 0;

        // Advance one whole block
        if (!filechecksummer.Jump(blocksize))
          return false;
      }
      else
      {
        // What entry do we expect next
        nextentry = 0;

        // Advance 1 byte
        if (!filechecksummer.Step())
          return false;
      }
    }
  }

  if (cancelled)
  {
    return false;
  }

  // Get the Full and 16k hash values of the file
  filechecksummer.GetFileHashes(hashfull, hash16k);

  // Did we make any matches at all
  if (count > 0)
  {
    // If this still might be a perfect match, check the
    // hashes, file size, and number of blocks to confirm.
    if (matchtype            != eFullMatch || 
        count                != sourcefile->GetVerificationPacket()->BlockCount() ||
        diskfile->FileSize() != sourcefile->GetDescriptionPacket()->FileSize() ||
        hashfull             != sourcefile->GetDescriptionPacket()->HashFull() ||
        hash16k              != sourcefile->GetDescriptionPacket()->Hash16k())
    {
      matchtype = ePartialMatch;

      if (noiselevel > CommandLine::nlSilent)
      {
        // Did we find data from multiple target files
        if (multipletargets)
        {
          // Were we scanning the target file or an extra file
          if (originalsourcefile != 0)
          {
            cout << "Target: \"" 
                 << name 
                 << "\" - damaged, found " 
                 << count 
                 << " data blocks from several target files." 
                 << endl;
          }
          else
          {
            cout << "File: \"" 
                 << name 
                 << "\" - found " 
                 << count 
                 << " data blocks from several target files." 
                 << endl;
          }
        }
        else
        {
          // Did we find data blocks that belong to the target file
          if (originalsourcefile == sourcefile)
          {
            cout << "Target: \"" 
                 << name 
                 << "\" - damaged. Found " 
                 << count 
                 << " of " 
                 << sourcefile->GetVerificationPacket()->BlockCount() 
                 << " data blocks." 
                 << endl;
          }
          // Were we scanning the target file or an extra file
          else if (originalsourcefile != 0)
          {
            string targetname;
            DiskFile::SplitFilename(sourcefile->TargetFileName(), path, targetname);

            cout << "Target: \"" 
                 << name 
                 << "\" - damaged. Found " 
                 << count 
                 << " of " 
                 << sourcefile->GetVerificationPacket()->BlockCount() 
                 << " data blocks from \"" 
                 << targetname 
                 << "\"."
                 << endl;
          }
          else
          {
            string targetname;
            DiskFile::SplitFilename(sourcefile->TargetFileName(), path, targetname);

            cout << "File: \"" 
                 << name 
                 << "\" - found " 
                 << count 
                 << " of " 
                 << sourcefile->GetVerificationPacket()->BlockCount() 
                 << " data blocks from \"" 
                 << targetname 
                 << "\"."
                 << endl;
          }
        }
      }
    }
    else
    {
      if (noiselevel > CommandLine::nlSilent)
      {
        // Did we match the target file
        if (originalsourcefile == sourcefile)
        {
          cout << "Target: \"" << name << "\" - found." << endl;
        }
        // Were we scanning the target file or an extra file
        else if (originalsourcefile != 0)
        {
          string targetname;
          DiskFile::SplitFilename(sourcefile->TargetFileName(), path, targetname);

          cout << "Target: \"" 
               << name 
               << "\" - is a match for \"" 
               << targetname 
               << "\"." 
               << endl;
        }
        else
        {
          string targetname;
          DiskFile::SplitFilename(sourcefile->TargetFileName(), path, targetname);

          cout << "File: \"" 
               << name 
               << "\" - is a match for \"" 
               << targetname 
               << "\"." 
               << endl;
        }
      }
    }
  }
  else
  {
    matchtype = eNoMatch;

    if (noiselevel > CommandLine::nlSilent)
    {
      // We found not data, but did the file actually contain blocks we
      // had already found in other files.
      if (duplicatecount > 0)
      {
        cout << "File: \""
             << name
             << "\" - found " 
             << duplicatecount
             << " duplicate data blocks."
             << endl;
      }
      else
      {
        cout << "File: \"" 
             << name 
             << "\" - no data found." 
             << endl;
      }
    }
  }
  sig_done(name,count, sourcefile && sourcefile->GetVerificationPacket() ? sourcefile->GetVerificationPacket()->BlockCount() : 0);
  sig_progress(1000);
  return true;
}

// Find out how much data we have found
void Par2Repairer::UpdateVerificationResults(void)
{
  availableblockcount = 0;
  missingblockcount = 0;

  completefilecount = 0;
  renamedfilecount = 0;
  damagedfilecount = 0;
  missingfilecount = 0;

  u32 filenumber = 0;
  vector<Par2RepairerSourceFile*>::iterator sf = sourcefiles.begin();

  // Check the recoverable files
  while (sf != sourcefiles.end() && filenumber < mainpacket->TotalFileCount())
  {
    Par2RepairerSourceFile *sourcefile = *sf;

    if (sourcefile)
    {
      // Was a perfect match for the file found
      if (sourcefile->GetCompleteFile() != 0)
      {
        // Is it the target file or a different one
        if (sourcefile->GetCompleteFile() == sourcefile->GetTargetFile())
        {
          completefilecount++;
        }
        else
        {
          renamedfilecount++;
        }

        availableblockcount += sourcefile->BlockCount();
      }
      else
      {
        // Count the number of blocks that have been found
        vector<DataBlock>::iterator sb = sourcefile->SourceBlocks();
        for (u32 blocknumber=0; blocknumber<sourcefile->BlockCount(); ++blocknumber, ++sb)
        {
          DataBlock &datablock = *sb;
          
          if (datablock.IsSet())
            availableblockcount++;
        }

        // Does the target file exist
        if (sourcefile->GetTargetExists())
        {
          damagedfilecount++;
        }
        else
        {
          missingfilecount++;
        }
      }
    }
    else
    {
      missingfilecount++;
    }

    ++filenumber;
    ++sf;
  }

  missingblockcount = sourceblockcount - availableblockcount;
}

// Check the verification results and report the results 
bool Par2Repairer::CheckVerificationResults(void)
{
  // Is repair needed
  if (completefilecount < mainpacket->RecoverableFileCount() ||
      renamedfilecount > 0 ||
      damagedfilecount > 0 ||
      missingfilecount > 0)
  {
    if (noiselevel > CommandLine::nlSilent)
      cout << "Repair is required." << endl;
    if (noiselevel > CommandLine::nlQuiet)
    {
      if (renamedfilecount > 0) cout << renamedfilecount << " file(s) have the wrong name." << endl;
      if (missingfilecount > 0) cout << missingfilecount << " file(s) are missing." << endl;
      if (damagedfilecount > 0) cout << damagedfilecount << " file(s) exist but are damaged." << endl;
      if (completefilecount > 0) cout << completefilecount << " file(s) are ok." << endl;

      cout << "You have " << availableblockcount 
           << " out of " << sourceblockcount 
           << " data blocks available." << endl;
      if (recoverypacketmap.size() > 0)
        cout << "You have " << (u32)recoverypacketmap.size() 
             << " recovery blocks available." << endl;
    }

    // Is repair possible
    if (recoverypacketmap.size() >= missingblockcount)
    {
      if (noiselevel > CommandLine::nlSilent)
        cout << "Repair is possible." << endl;

      if (noiselevel > CommandLine::nlQuiet)
      {
        if (recoverypacketmap.size() > missingblockcount)
          cout << "You have an excess of " 
               << (u32)recoverypacketmap.size() - missingblockcount
               << " recovery blocks." << endl;

        if (missingblockcount > 0)
          cout << missingblockcount
               << " recovery blocks will be used to repair." << endl;
        else if (recoverypacketmap.size())
          cout << "None of the recovery blocks will be used for the repair." << endl;
      }

      return true;
    }
    else
    {
      if (noiselevel > CommandLine::nlSilent)
      {
        cout << "Repair is not possible." << endl;
        cout << "You need " << missingblockcount - recoverypacketmap.size()
             << " more recovery blocks to be able to repair." << endl;
      }

      return false;
    }
  }
  else
  {
    if (noiselevel > CommandLine::nlSilent)
      cout << "All files are correct, repair is not required." << endl;

    return true;
  }

  return true;
}

// Rename any damaged or missnamed target files.
bool Par2Repairer::RenameTargetFiles(void)
{
  u32 filenumber = 0;
  vector<Par2RepairerSourceFile*>::iterator sf = sourcefiles.begin();

  // Rename any damaged target files
  while (sf != sourcefiles.end() && filenumber < mainpacket->TotalFileCount())
  {
    Par2RepairerSourceFile *sourcefile = *sf;

    // If the target file exists but is not a complete version of the file
    if (sourcefile->GetTargetExists() && 
        sourcefile->GetTargetFile() != sourcefile->GetCompleteFile())
    {
      DiskFile *targetfile = sourcefile->GetTargetFile();

      // Rename it
      diskFileMap.Remove(targetfile);

      if (!targetfile->Rename())
        return false;

      bool success = diskFileMap.Insert(targetfile);
      assert(success); (void)success;

      // We no longer have a target file
      sourcefile->SetTargetExists(false);
      sourcefile->SetTargetFile(0);
    }

    ++sf;
    ++filenumber;
  }

  filenumber = 0;
  sf = sourcefiles.begin();

  // Rename any missnamed but complete versions of the files
  while (sf != sourcefiles.end() && filenumber < mainpacket->TotalFileCount())
  {
    Par2RepairerSourceFile *sourcefile = *sf;

    // If there is no targetfile and there is a complete version
    if (sourcefile->GetTargetFile() == 0 &&
        sourcefile->GetCompleteFile() != 0)
    {
      DiskFile *targetfile = sourcefile->GetCompleteFile();

      // Rename it
      diskFileMap.Remove(targetfile);

      if (!targetfile->Rename(sourcefile->TargetFileName()))
        return false;

      bool success = diskFileMap.Insert(targetfile);
      assert(success); (void)success;

      // This file is now the target file
      sourcefile->SetTargetExists(true);
      sourcefile->SetTargetFile(targetfile);

      // We have one more complete file
      completefilecount++;
    }

    ++sf;
    ++filenumber;
  }

  return true;
}

// Work out which files are being repaired, create them, and allocate
// target DataBlocks to them, and remember them for later verification.
bool Par2Repairer::CreateTargetFiles(void)
{
  u32 filenumber = 0;
  vector<Par2RepairerSourceFile*>::iterator sf = sourcefiles.begin();

  // Create any missing target files
  while (sf != sourcefiles.end() && filenumber < mainpacket->TotalFileCount())
  {
    Par2RepairerSourceFile *sourcefile = *sf;

    // If the file does not exist
    if (!sourcefile->GetTargetExists())
    {
      DiskFile *targetfile = new DiskFile(cerr);
      string filename = sourcefile->TargetFileName();
      u64 filesize = sourcefile->GetDescriptionPacket()->FileSize();

      // Create the target file
      if (!targetfile->Create(filename, filesize))
      {
        delete targetfile;
        return false;
      }

      // This file is now the target file
      sourcefile->SetTargetExists(true);
      sourcefile->SetTargetFile(targetfile);

      // Remember this file
      bool success = diskFileMap.Insert(targetfile);
      assert(success); (void)success;

      u64 offset = 0;
      vector<DataBlock>::iterator tb = sourcefile->TargetBlocks();

      // Allocate all of the target data blocks
      while (offset < filesize)
      {
        DataBlock &datablock = *tb;

        datablock.SetLocation(targetfile, offset);
        datablock.SetLength(min(blocksize, filesize-offset));

        offset += blocksize;
        ++tb;
      }

      // Add the file to the list of those that will need to be verified
      // once the repair has completed.
      verifylist.push_back(sourcefile);
    }

    ++sf;
    ++filenumber;
  }

  return true;
}

// Work out which data blocks are available, which need to be copied
// directly to the output, and which need to be recreated, and compute
// the appropriate Reed Solomon matrix.
bool Par2Repairer::ComputeRSmatrix(void)
{
  inputblocks.resize(sourceblockcount);   // The DataBlocks that will read from disk
  copyblocks.resize(availableblockcount); // Those DataBlocks which need to be copied
  outputblocks.resize(missingblockcount); // Those DataBlocks that will re recalculated

  vector<DataBlock*>::iterator inputblock  = inputblocks.begin();
  vector<DataBlock*>::iterator copyblock   = copyblocks.begin();
  vector<DataBlock*>::iterator outputblock = outputblocks.begin();

  // Build an array listing which source data blocks are present and which are missing
  vector<bool> present;
  present.resize(sourceblockcount);

  vector<DataBlock>::iterator sourceblock  = sourceblocks.begin();
  vector<DataBlock>::iterator targetblock  = targetblocks.begin();
  vector<bool>::iterator              pres = present.begin();

  // Iterate through all source blocks for all files
  while (sourceblock != sourceblocks.end())
  {
    // Was this block found
    if (sourceblock->IsSet())
    {
//      // Open the file the block was found in.
//      if (!sourceblock->Open())
//        return false;

      // Record that the block was found
      *pres = true;

      // Add the block to the list of those which will be read 
      // as input (and which might also need to be copied).
      *inputblock = &*sourceblock;
      *copyblock = &*targetblock;

      ++inputblock;
      ++copyblock;
    }
    else
    {
      // Record that the block was missing
      *pres = false;

      // Add the block to the list of those to be written
      *outputblock = &*targetblock;
      ++outputblock;
    }

    ++sourceblock;
    ++targetblock;
    ++pres;
  }

  // Set the number of source blocks and which of them are present
  if (!rs.SetInput(present))
    return false;

  // Start iterating through the available recovery packets
  map<u32,RecoveryPacket*>::iterator rp = recoverypacketmap.begin();

  // Continue to fill the remaining list of data blocks to be read
  while (inputblock != inputblocks.end())
  {
    // Get the next available recovery packet
    u32 exponent = rp->first;
    RecoveryPacket* recoverypacket = rp->second;

    // Get the DataBlock from the recovery packet
    DataBlock *recoveryblock = recoverypacket->GetDataBlock();

//    // Make sure the file is open
//    if (!recoveryblock->Open())
//      return false;

    // Add the recovery block to the list of blocks that will be read
    *inputblock = recoveryblock;

    // Record that the corresponding exponent value is the next one
    // to use in the RS matrix
    if (!rs.SetOutput(true, (u16)exponent))
      return false;

    ++inputblock;
    ++rp;
  }

  // If we need to, compute and solve the RS matrix
  if (missingblockcount == 0)
    return true;
  
  bool success = rs.Compute(noiselevel);

  return success;  
}

// Allocate memory buffers for reading and writing data to disk.
bool Par2Repairer::AllocateBuffers(size_t memorylimit)
{
  // Would single pass processing use too much memory
  if (blocksize * missingblockcount > memorylimit)
  {
    // Pick a size that is small enough
    chunksize = ~3 & (memorylimit / missingblockcount);
  }
  else
  {
    chunksize = (size_t)blocksize;
  }

  // Allocate the two buffers
  inputbuffer = new u8[(size_t)chunksize];
  outputbuffer = new u8[(size_t)chunksize * missingblockcount];

  if (inputbuffer == NULL || outputbuffer == NULL)
  {
    cerr << "Could not allocate buffer memory." << endl;
    return false;
  }
  
  return true;
}

// Read source data, process it through the RS matrix and write it to disk.
bool Par2Repairer::ProcessData(u64 blockoffset, size_t blocklength)
{
  u64 totalwritten = 0;

  // Clear the output buffer
  memset(outputbuffer, 0, (size_t)chunksize * missingblockcount);

  vector<DataBlock*>::iterator inputblock = inputblocks.begin();
  vector<DataBlock*>::iterator copyblock  = copyblocks.begin();
  u32                          inputindex = 0;

  DiskFile *lastopenfile = NULL;

  // Are there any blocks which need to be reconstructed
  if (missingblockcount > 0)
  {
    // For each input block
    while (inputblock != inputblocks.end())       
    {
      // Are we reading from a new file?
      if (lastopenfile != (*inputblock)->GetDiskFile())
      {
        // Close the last file
        if (lastopenfile != NULL)
        {
          lastopenfile->Close();
        }

        // Open the new file
        lastopenfile = (*inputblock)->GetDiskFile();
        if (!lastopenfile->Open())
        {
          return false;
        }
      }

      // Read data from the current input block
      if (!(*inputblock)->ReadData(blockoffset, blocklength, inputbuffer))
        return false;

      // Have we reached the last source data block
      if (copyblock != copyblocks.end())
      {
        // Does this block need to be copied to the target file
        if ((*copyblock)->IsSet())
        {
          size_t wrote;

          // Write the block back to disk in the new target file
          if (!(*copyblock)->WriteData(blockoffset, blocklength, inputbuffer, wrote))
            return false;

          totalwritten += wrote;
        }
        ++copyblock;
      }

      if (!RepairData(inputindex, blocklength))
      {
      // For each output block
      for (u32 outputindex=0; outputindex<missingblockcount; outputindex++)
      {
        // Select the appropriate part of the output buffer
        void *outbuf = &((u8*)outputbuffer)[chunksize * outputindex];

        // Process the data
        rs.Process(blocklength, inputindex, inputbuffer, outputindex, outbuf);

        if (noiselevel > CommandLine::nlQuiet)
        {
          // Update a progress indicator
          u32 oldfraction = (u32)(1000 * progress / totaldata);
          progress += blocklength;
          u32 newfraction = (u32)(1000 * progress / totaldata);

          if (oldfraction != newfraction)
          {
            cout << "Repairing: " << newfraction/10 << '.' << newfraction%10 << "%\r" << flush;
	    sig_progress(newfraction);

            if (cancelled)
            {
              break;
            }
          }
        }
      }
      }

      if (cancelled)
      {
        break;
      }

      ++inputblock;
      ++inputindex;
    }
  }
  else
  {
    // Reconstruction is not required, we are just copying blocks between files

    // For each block that might need to be copied
    while (copyblock != copyblocks.end())
    {
      // Does this block need to be copied
      if ((*copyblock)->IsSet())
      {
        // Are we reading from a new file?
        if (lastopenfile != (*inputblock)->GetDiskFile())
        {
          // Close the last file
          if (lastopenfile != NULL)
          {
            lastopenfile->Close();
          }

          // Open the new file
          lastopenfile = (*inputblock)->GetDiskFile();
          if (!lastopenfile->Open())
          {
            return false;
          }
        }

        // Read data from the current input block
        if (!(*inputblock)->ReadData(blockoffset, blocklength, inputbuffer))
          return false;

        size_t wrote;
        if (!(*copyblock)->WriteData(blockoffset, blocklength, inputbuffer, wrote))
          return false;
        totalwritten += wrote;
      }

      if (noiselevel > CommandLine::nlQuiet)
      {
        // Update a progress indicator
        u32 oldfraction = (u32)(1000 * progress / totaldata);
        progress += blocklength;
        u32 newfraction = (u32)(1000 * progress / totaldata);

        if (oldfraction != newfraction)
        {
          cout << "Processing: " << newfraction/10 << '.' << newfraction%10 << "%\r" << flush;
	sig_progress(newfraction);

          if (cancelled)
          {
            break;
          }
        }
      }

      if (cancelled)
      {
        break;
      }

      ++copyblock;
      ++inputblock;
    }
  }

  // Close the last file
  if (lastopenfile != NULL)
  {
    lastopenfile->Close();
  }

  if (cancelled)
  {
    return false;
  }

  if (noiselevel > CommandLine::nlQuiet)
    cout << "Writing recovered data\r";

  // For each output block that has been recomputed
  vector<DataBlock*>::iterator outputblock = outputblocks.begin();
  for (u32 outputindex=0; outputindex<missingblockcount;outputindex++)
  {
    // Select the appropriate part of the output buffer
    char *outbuf = &((char*)outputbuffer)[chunksize * outputindex];

    // Write the data to the target file
    size_t wrote;
    if (!(*outputblock)->WriteData(blockoffset, blocklength, outbuf, wrote))
      return false;
    totalwritten += wrote;

    ++outputblock;
  }

  if (noiselevel > CommandLine::nlQuiet)
    cout << "Wrote " << totalwritten << " bytes to disk" << endl;

  return true;
}

// Verify that all of the reconstructed target files are now correct
bool Par2Repairer::VerifyTargetFiles(void)
{
  bool finalresult = true;

  // Verify the target files in alphabetical order
  sort(verifylist.begin(), verifylist.end(), SortSourceFilesByFileName);

  // Iterate through each file in the verification list
  for (vector<Par2RepairerSourceFile*>::iterator sf = verifylist.begin();
       sf != verifylist.end();
       ++sf)
  {
    Par2RepairerSourceFile *sourcefile = *sf;
    DiskFile *targetfile = sourcefile->GetTargetFile();

    // Close the file
    if (targetfile->IsOpen())
      targetfile->Close();

    // Mark all data blocks for the file as unknown
    vector<DataBlock>::iterator sb = sourcefile->SourceBlocks();
    for (u32 blocknumber=0; blocknumber<sourcefile->BlockCount(); blocknumber++)
    {
      sb->ClearLocation();
      ++sb;
    }

    // Say we don't have a complete version of the file
    sourcefile->SetCompleteFile(0);

    // Re-open the target file
    if (!targetfile->Open())
    {
      finalresult = false;
      continue;
    }

    // Verify the file again
    if (!VerifyDataFile(targetfile, sourcefile))
      finalresult = false;

    // Close the file again
    targetfile->Close();

    // Find out how much data we have found
    UpdateVerificationResults();
  }

  return finalresult;
}

// Delete all of the partly reconstructed files
bool Par2Repairer::DeleteIncompleteTargetFiles(void)
{
  vector<Par2RepairerSourceFile*>::iterator sf = verifylist.begin();

  // Iterate through each file in the verification list
  while (sf != verifylist.end())
  {
    Par2RepairerSourceFile *sourcefile = *sf;
    if (sourcefile->GetTargetExists())
    {
      DiskFile *targetfile = sourcefile->GetTargetFile();

      // Close and delete the file
      if (targetfile->IsOpen())
        targetfile->Close();
      targetfile->Delete();

      // Forget the file
      diskFileMap.Remove(targetfile);
      delete targetfile;

      // There is no target file
      sourcefile->SetTargetExists(false);
      sourcefile->SetTargetFile(0);
    }

    ++sf;
  }

  return true;
}

} // end namespace Par2
