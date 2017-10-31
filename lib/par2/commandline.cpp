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
#include <iostream>
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

CommandLine::ExtraFile::ExtraFile(void)
: filename()
, filesize(0)
{
}

CommandLine::ExtraFile::ExtraFile(const CommandLine::ExtraFile &other)
: filename(other.filename)
, filesize(other.filesize)
{
}

CommandLine::ExtraFile& CommandLine::ExtraFile::operator=(const CommandLine::ExtraFile &other)
{
  filename = other.filename;
  filesize = other.filesize;

  return *this;
}

CommandLine::ExtraFile::ExtraFile(const string &name, u64 size)
: filename(name)
, filesize(size)
{
}


CommandLine::CommandLine(std::ostream& cout, std::ostream& cerr)
: operation(opNone)
, version(verUnknown)
, noiselevel(nlUnknown)
, blockcount(0)
, blocksize(0)
, firstblock(0)
, recoveryfilescheme(scUnknown)
, recoveryfilecount(0)
, recoveryblockcount(0)
, recoveryblockcountset(false)
, redundancy(0)
, redundancyset(false)
, parfilename()
, extrafiles()
, totalsourcesize(0)
, largestsourcesize(0)
, memorylimit(0)
, cout(cout)
, cerr(cerr)
{
}

void CommandLine::usage(void)
{
  std::cout <<
    "\n"
    "Usage:\n"
    "\n"
    "  par2 c(reate) [options] <par2 file> [files] : Create PAR2 files\n"
    "  par2 v(erify) [options] <par2 file> [files] : Verify files using PAR2 file\n"
    "  par2 r(epair) [options] <par2 file> [files] : Repair files using PAR2 files\n"
    "\n"
    "You may also leave out the \"c\", \"v\", and \"r\" commands by using \"parcreate\",\n"
    "\"par2verify\", or \"par2repair\" instead.\n"
    "\n"
    "Options:\n"
    "\n"
    "  -b<n>  : Set the Block-Count\n"
    "  -s<n>  : Set the Block-Size (Don't use both -b and -s)\n"
    "  -r<n>  : Level of Redundancy (%%)\n"
    "  -c<n>  : Recovery block count (Don't use both -r and -c)\n"
    "  -f<n>  : First Recovery-Block-Number\n"
    "  -u     : Uniform recovery file sizes\n"
    "  -l     : Limit size of recovery files (Don't use both -u and -l)\n"
    "  -n<n>  : Number of recovery files (Don't use both -n and -l)\n"
    "  -m<n>  : Memory (in MB) to use\n"
    "  -v [-v]: Be more verbose\n"
    "  -q [-q]: Be more quiet (-q -q gives silence)\n"
    "  --     : Treat all remaining CommandLine as filenames\n"
    "\n"
    "If you wish to create par2 files for a single source file, you may leave\n"
    "out the name of the par2 file from the command line.\n";
}

bool CommandLine::Parse(int argc, char *argv[])
{
  if (argc<1)
  {
    return false;
  }

  // Split the program name into path and filename
  string path, name;
  DiskFile::SplitFilename(argv[0], path, name);
  argc--;
  argv++;

  // Strip ".exe" from the end
  if (name.size() > 4 && 0 == stricmp(".exe", name.substr(name.length()-4).c_str()))
  {
    name = name.substr(0, name.length()-4);
  }

  // Check the resulting program name
  if (0 == stricmp("par2create", name.c_str()))
  {
    operation = opCreate;
  } 
  else if (0 == stricmp("par2verify", name.c_str()))
  {
    operation = opVerify;
  }
  else if (0 == stricmp("par2repair", name.c_str()))
  {
    operation = opRepair;
  }

  // Have we determined what operation we want?
  if (operation == opNone)
  {
    if (argc<2)
    {
      cerr << "Not enough command line arguments." << endl;
      return false;
    }

    switch (tolower(argv[0][0]))
    {
    case 'c':
      if (argv[0][1] == 0 || 0 == stricmp(argv[0], "create"))
        operation = opCreate;
      break;
    case 'v':
      if (argv[0][1] == 0 || 0 == stricmp(argv[0], "verify"))
        operation = opVerify;
      break;
    case 'r':
      if (argv[0][1] == 0 || 0 == stricmp(argv[0], "repair"))
        operation = opRepair;
      break;
    }
    if (operation == opNone)
    {
      cerr << "Invalid operation specified: " << argv[0] << endl;
      return false;
    }
    argc--;
    argv++;
  }

  bool options = true;

  while (argc>0)
  {
    if (argv[0][0])
    {
      if (options && argv[0][0] != '-')
        options = false;

      if (options)
      {
        switch (tolower(argv[0][1]))
        {
        case 'b':  // Set the block count
          {
            if (operation != opCreate)
            {
              cerr << "Cannot specify block count unless creating." << endl;
              return false;
            }
            if (blockcount > 0)
            {
              cerr << "Cannot specify block count twice." << endl;
              return false;
            }
            else if (blocksize > 0)
            {
              cerr << "Cannot specify both block count and block size." << endl;
              return false;
            }
            
            char *p = &argv[0][2];
            while (blockcount <= 3276 && *p && isdigit(*p))
            {
              blockcount = blockcount * 10 + (*p - '0');
              p++;
            }
            if (0 == blockcount || blockcount > 32768 || *p)
            {
              cerr << "Invalid block count option: " << argv[0] << endl;
              return false;
            }
          }
          break;

        case 's':  // Set the block size
          {
            if (operation != opCreate)
            {
              cerr << "Cannot specify block size unless creating." << endl;
              return false;
            }
            if (blocksize > 0)
            {
              cerr << "Cannot specify block size twice." << endl;
              return false;
            }
            else if (blockcount > 0)
            {
              cerr << "Cannot specify both block count and block size." << endl;
              return false;
            }

            char *p = &argv[0][2];
            while (blocksize <= 429496729 && *p && isdigit(*p))
            {
              blocksize = blocksize * 10 + (*p - '0');
              p++;
            }
            if (*p || blocksize == 0)
            {
              cerr << "Invalid block size option: " << argv[0] << endl;
              return false;
            }
            if (blocksize & 3)
            {
              cerr << "Block size must be a multiple of 4." << endl;
              return false;
            }
          }
          break;

        case 'r':  // Set the amount of redundancy required
          {
            if (operation != opCreate)
            {
              cerr << "Cannot specify redundancy unless creating." << endl;
              return false;
            }
            if (redundancyset)
            {
              cerr << "Cannot specify redundancy twice." << endl;
              return false;
            }
            else if (recoveryblockcountset)
            {
              cerr << "Cannot specify both redundancy and recovery block count." << endl;
              return false;
            }

            char *p = &argv[0][2];
            while (redundancy <= 10 && *p && isdigit(*p))
            {
              redundancy = redundancy * 10 + (*p - '0');
              p++;
            }
            if (redundancy > 100 || *p)
            {
              cerr << "Invalid redundancy option: " << argv[0] << endl;
              return false;
            }
            if (redundancy == 0 && recoveryfilecount > 0)
            {
              cerr << "Cannot set redundancy to 0 and file count > 0" << endl;
              return false;
            }
            redundancyset = true;
          }
          break;

        case 'c': // Set the number of recovery blocks to create
          {
            if (operation != opCreate)
            {
              cerr << "Cannot specify recovery block count unless creating." << endl;
              return false;
            }
            if (recoveryblockcountset)
            {
              cerr << "Cannot specify recovery block count twice." << endl;
              return false;
            }
            else if (redundancyset)
            {
              cerr << "Cannot specify both recovery block count and redundancy." << endl;
              return false;
            }

            char *p = &argv[0][2];
            while (recoveryblockcount <= 32768 && *p && isdigit(*p))
            {
              recoveryblockcount = recoveryblockcount * 10 + (*p - '0');
              p++;
            }
            if (recoveryblockcount > 32768 || *p)
            {
              cerr << "Invalid recoveryblockcount option: " << argv[0] << endl;
              return false;
            }
            if (recoveryblockcount == 0 && recoveryfilecount > 0)
            {
              cerr << "Cannot set recoveryblockcount to 0 and file count > 0" << endl;
              return false;
            }
            recoveryblockcountset = true;
          }
          break;

        case 'f':  // Specify the First block recovery number
          {
            if (operation != opCreate)
            {
              cerr << "Cannot specify first block number unless creating." << endl;
              return false;
            }
            if (firstblock > 0)
            {
              cerr << "Cannot specify first block twice." << endl;
              return false;
            }

            char *p = &argv[0][2];
            while (firstblock <= 3276 && *p && isdigit(*p))
            {
              firstblock = firstblock * 10 + (*p - '0');
              p++;
            }
            if (firstblock > 32768 || *p)
            {
              cerr << "Invalid first block option: " << argv[0] << endl;
              return false;
            }
          }
          break;

        case 'u':  // Specify uniformly sized recovery files
          {
            if (operation != opCreate)
            {
              cerr << "Cannot specify uniform files unless creating." << endl;
              return false;
            }
            if (argv[0][2])
            {
              cerr << "Invalid option: " << argv[0] << endl;
              return false;
            }
            if (recoveryfilescheme != scUnknown)
            {
              cerr << "Cannot specify two recovery file size schemes." << endl;
              return false;
            }

            recoveryfilescheme = scUniform;
          }
          break;

        case 'l':  // Limit the size of the recovery files
          {
            if (operation != opCreate)
            {
              cerr << "Cannot specify limit files unless creating." << endl;
              return false;
            }
            if (argv[0][2])
            {
              cerr << "Invalid option: " << argv[0] << endl;
              return false;
            }
            if (recoveryfilescheme != scUnknown)
            {
              cerr << "Cannot specify two recovery file size schemes." << endl;
              return false;
            }
            if (recoveryfilecount > 0)
            {
              cerr << "Cannot specify limited size and number of files at the same time." << endl;
              return false;
            }

            recoveryfilescheme = scLimited;
          }
          break;

        case 'n':  // Specify the number of recovery files
          {
            if (operation != opCreate)
            {
              cerr << "Cannot specify recovery file count unless creating." << endl;
              return false;
            }
            if (recoveryfilecount > 0)
            {
              cerr << "Cannot specify recovery file count twice." << endl;
              return false;
            }
            if (redundancyset && redundancy == 0)
            {
              cerr << "Cannot set file count when redundancy is set to 0." << endl;
              return false;
            }
            if (recoveryblockcountset && recoveryblockcount == 0)
            {
              cerr << "Cannot set file count when recovery block count is set to 0." << endl;
              return false;
            }
            if (recoveryfilescheme == scLimited)
            {
              cerr << "Cannot specify limited size and number of files at the same time." << endl;
              return false;
            }

            char *p = &argv[0][2];
            while (*p && isdigit(*p))
            {
              recoveryfilecount = recoveryfilecount * 10 + (*p - '0');
              p++;
            }
            if (recoveryfilecount == 0 || *p)
            {
              cerr << "Invalid recovery file count option: " << argv[0] << endl;
              return false;
            }
          }
          break;

        case 'm':  // Specify how much memory to use for output buffers
          {
            if (memorylimit > 0)
            {
              cerr << "Cannot specify memory limit twice." << endl;
              return false;
            }

            char *p = &argv[0][2];
            while (*p && isdigit(*p))
            {
              memorylimit = memorylimit * 10 + (*p - '0');
              p++;
            }
            if (memorylimit == 0 || *p)
            {
              cerr << "Invalid memory limit option: " << argv[0] << endl;
              return false;
            }
          }
          break;

        case 'v':
          {
            switch (noiselevel)
            {
            case nlUnknown:
              {
                if (argv[0][2] == 'v')
                  noiselevel = nlDebug;
                else
                  noiselevel = nlNoisy;
              }
              break;
            case nlNoisy:
            case nlDebug:
              noiselevel = nlDebug;
              break;
            default:
              cerr << "Cannot use both -v and -q." << endl;
              return false;
              break;
            }
          }
          break;

        case 'q':
          {
            switch (noiselevel)
            {
            case nlUnknown:
              {
                if (argv[0][2] == 'q')
                  noiselevel = nlSilent;
                else
                  noiselevel = nlQuiet;
              }
              break;
            case nlQuiet:
            case nlSilent:
              noiselevel = nlSilent;
              break;
            default:
              cerr << "Cannot use both -v and -q." << endl;
              return false;
              break;
            }
          }
          break;

        case '-':
          {
            argc--;
            argv++;
            options = false;
            continue;
          }
          break;
        default:
          {
            cerr << "Invalid option specified: " << argv[0] << endl;
            return false;
          }
        }
      }
      else
      {
        list<string> *filenames;

        // If the argument includes wildcard characters, 
        // search the disk for matching files
        if (strchr(argv[0], '*') || strchr(argv[0], '?'))
        {
          string path;
          string name;
          DiskFile::SplitFilename(argv[0], path, name);

          filenames = DiskFile::FindFiles(path, name);
        }
        else
        {
          //start of shell expanded * patch. -- Michael Evans
          //The shell might expaned * so, if we have our name and we're creating, then filter for files...
          if ((parfilename.length() != 0) && (operation == opCreate))
          {
#ifdef WIN32
            if (GetFileAttributes(argv[0]) & FILE_ATTRIBUTE_DIRECTORY) // != 0, but no need...
#else //Not WIN32, probably *nix
            struct stat st;
            if (!(stat(argv[0], &st) == 0 && S_ISREG(st.st_mode)))
#endif
            {
              cerr << "Skipping non-regular file: " << argv[0] << endl;
              argc--;
              argv++;
              options = false;
              continue;
            }
          }//end of shell expanded * patch. -- Michael Evans
          filenames = new list<string>;
          filenames->push_back(argv[0]);
        }

        list<string>::iterator fn = filenames->begin();
        while (fn != filenames->end())
        {
          // Convert filename from command line into a full path + filename
          string filename = DiskFile::GetCanonicalPathname(*fn);

          // If this is the first file on the command line, then it
          // is the main PAR2 file.
          if (parfilename.length() == 0)
          {
            // If we are verifying or repairing, the PAR2 file must
            // already exist
            if (operation != opCreate)
            {
              // Find the last '.' in the filename
              string::size_type where = filename.find_last_of('.');
              if (where != string::npos)
              {
                // Get what follows the last '.'
                string tail = filename.substr(where+1);

                if (0 == stricmp(tail.c_str(), "par2"))
                {
                  parfilename = filename;
                  version = verPar2;
                }
                else if (0 == stricmp(tail.c_str(), "par") ||
                         (tail.size() == 3 &&
                         tolower(tail[0]) == 'p' &&
                         isdigit(tail[1]) &&
                         isdigit(tail[2])))
                {
                  parfilename = filename;
                  version = verPar1;
                }
              }

              // If we haven't figured out which version of PAR file we
              // are using from the file extension, then presumable the
              // files filename was actually the name of a data file.
              if (version == verUnknown)
              {
                // Check for the existence of a PAR2 of PAR file.
                if (DiskFile::FileExists(filename + ".par2"))
                {
                  version = verPar2;
                  parfilename = filename + ".par2";
                }
                else if (DiskFile::FileExists(filename + ".PAR2"))
                {
                  version = verPar2;
                  parfilename = filename + ".PAR2";
                }
                else if (DiskFile::FileExists(filename + ".par"))
                {
                  version = verPar1;
                  parfilename = filename + ".par";
                }
                else if (DiskFile::FileExists(filename + ".PAR"))
                {
                  version = verPar1;
                  parfilename = filename + ".PAR";
                }
              }
              else
              {
                // Does the specified PAR or PAR2 file exist
                if (!DiskFile::FileExists(filename))
                {
                  version = verUnknown;
                }
              }

              if (version == verUnknown)
              {
                cerr << "The recovery file does not exist: " << filename << endl;
                return false;
              }
            }
            else
            {
              // We are creating a new file
              version = verPar2;
              parfilename = filename;
            }
          }
          else
          {
            // All other files must exist
            if (!DiskFile::FileExists(filename))
            {
              cerr << "The source file does not exist: " << filename << endl;
              return false;
            }

            u64 filesize = DiskFile::GetFileSize(filename);

            // Ignore all 0 byte files
            if (filesize > 0)
            {
              extrafiles.push_back(ExtraFile(filename, filesize));

              // track the total size of the source files and how
              // big the largest one is.
              totalsourcesize += filesize;
              if (largestsourcesize < filesize)
                largestsourcesize = filesize;
            }
            else
            {
              cout << "Skipping 0 byte file: " << filename << endl;
            }
          }

          ++fn;
        }
        delete filenames;
      }
    }

    argc--;
    argv++;
  }

  if (parfilename.length() == 0)
  {
    cerr << "You must specify a Recovery file." << endl;
    return false;
  }

  // Default noise level
  if (noiselevel == nlUnknown)
  {
    noiselevel = nlNormal;
  }

  // If we a creating, check the other parameters
  if (operation == opCreate)
  {
    // If no recovery file size scheme is specified then use Variable
    if (recoveryfilescheme == scUnknown)
    {
      recoveryfilescheme = scVariable;
    }

    // If neither block count not block size is specified
    if (blockcount == 0 && blocksize == 0)
    {
      // Use a block count of 2000
      blockcount = 2000;
    }

    // If we are creating, the source files must be given.
    if (extrafiles.size() == 0)
    {
      // Does the par filename include the ".par2" on the end?
      if (parfilename.length() > 5 && 0 == stricmp(parfilename.substr(parfilename.length()-5, 5).c_str(), ".par2"))
      {
        // Yes it does.
        cerr << "You must specify a list of files when creating." << endl;
        return false;
      }
      else
      {
        // No it does not.

        // In that case check to see if the file exists, and if it does
        // assume that you wish to create par2 files for it.

        u64 filesize = 0;
	if (DiskFile::FileExists(parfilename) &&
            (filesize = DiskFile::GetFileSize(parfilename)) > 0)
        {
          extrafiles.push_back(ExtraFile(parfilename, filesize));

          // track the total size of the source files and how
          // big the largest one is.
          totalsourcesize += filesize;
          if (largestsourcesize < filesize)
            largestsourcesize = filesize;
        }
        else
        {
          // The file does not exist or it is empty.

          cerr << "You must specify a list of files when creating." << endl;
          return false;
        }
      }
    }

    // Strip the ".par2" from the end of the filename of the main PAR2 file.
    if (parfilename.length() > 5 && 0 == stricmp(parfilename.substr(parfilename.length()-5, 5).c_str(), ".par2"))
    {
      parfilename = parfilename.substr(0, parfilename.length()-5);
    }

    // Assume a redundancy of 5% if neither redundancy or recoveryblockcount were set.
    if (!redundancyset && !recoveryblockcountset)
    {
      redundancy = 5;
    }
  }

  // Assume a memory limit of 16MB if not specified.
  if (memorylimit == 0)
  {
#ifdef WIN32
    u64 TotalPhysicalMemory = 0;

    HMODULE hLib = ::LoadLibraryA("kernel32.dll");
    if (NULL != hLib)
    {
      BOOL (WINAPI *pfn)(LPMEMORYSTATUSEX) = (BOOL (WINAPI*)(LPMEMORYSTATUSEX))::GetProcAddress(hLib, "GlobalMemoryStatusEx");

      if (NULL != pfn)
      {
        MEMORYSTATUSEX mse;
        mse.dwLength = sizeof(mse);
        if (pfn(&mse))
        {
          TotalPhysicalMemory = mse.ullTotalPhys;
        }
      }

      ::FreeLibrary(hLib);
    }

    if (TotalPhysicalMemory == 0)
    {
      MEMORYSTATUS ms;
      ::ZeroMemory(&ms, sizeof(ms));
      ::GlobalMemoryStatus(&ms);

      TotalPhysicalMemory = ms.dwTotalPhys;
    }

    if (TotalPhysicalMemory == 0)
    {
      // Assume 128MB
      TotalPhysicalMemory = 128 * 1048576;
    }

    // Half of total physical memory
    memorylimit = (size_t)(TotalPhysicalMemory / 1048576 / 2);
#else
    memorylimit = 16;
#endif
  }
  memorylimit *= 1048576;

  return true;
}

} // end namespace Par2
